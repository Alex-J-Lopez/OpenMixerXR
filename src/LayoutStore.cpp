#include "LayoutStore.h"
#include "Logger.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdlib>   // std::getenv

using json = nlohmann::json;

// ── Serialization helpers ─────────────────────────────────────────────────────

std::string LayoutStore::serializeLayout(const Layout& layout) {
    json j;
    j["version"]          = layout.version;
    j["name"]             = layout.name;
    j["globalChromaColor"] = { layout.globalChromaR,
                               layout.globalChromaG,
                               layout.globalChromaB };

    json boxes = json::array();
    for (const PassthroughBox& b : layout.boxes) {
        json jb;
        jb["id"]             = b.id;
        jb["name"]           = b.name;
        jb["posX"]           = b.posX;
        jb["posY"]           = b.posY;
        jb["posZ"]           = b.posZ;
        jb["rotYaw"]         = b.rotYaw;
        jb["rotPitch"]       = b.rotPitch;
        jb["rotRoll"]        = b.rotRoll;
        jb["scaleWidth"]     = b.scaleWidth;
        jb["scaleHeight"]    = b.scaleHeight;
        jb["scaleDepth"]     = b.scaleDepth;
        jb["chromaR"]        = b.chromaR;
        jb["chromaG"]        = b.chromaG;
        jb["chromaB"]        = b.chromaB;
        jb["minOpacity"]     = b.minOpacity;
        jb["maxOpacity"]     = b.maxOpacity;
        jb["fadeNearMeters"] = b.fadeNearMeters;
        jb["fadeFarMeters"]  = b.fadeFarMeters;
        jb["visible"]        = b.visible;
        boxes.push_back(std::move(jb));
    }
    j["boxes"] = std::move(boxes);

    return j.dump(2);   // 2-space indent for human readability
}

std::optional<Layout> LayoutStore::deserializeLayout(const std::string& jsonStr,
                                                       std::string& errorOut) {
    try {
        const json j = json::parse(jsonStr);

        // ── Version check ────────────────────────────────────────────────────
        if (!j.contains("version")) {
            errorOut = "missing 'version' field";
            return std::nullopt;
        }
        const int ver = j.at("version").get<int>();
        if (ver != 1 && ver != Layout::CURRENT_VERSION) {
            errorOut = "unknown version " + std::to_string(ver)
                       + " (supported: 1, " + std::to_string(Layout::CURRENT_VERSION) + ")";
            return std::nullopt;
        }

        Layout layout;
        layout.version = ver;
        layout.name    = j.value("name", "");

        if (j.contains("globalChromaColor") && j["globalChromaColor"].is_array()
                && j["globalChromaColor"].size() >= 3) {
            layout.globalChromaR = j["globalChromaColor"][0].get<float>();
            layout.globalChromaG = j["globalChromaColor"][1].get<float>();
            layout.globalChromaB = j["globalChromaColor"][2].get<float>();
        }

        if (j.contains("boxes") && j["boxes"].is_array()) {
            for (const json& jb : j["boxes"]) {
                PassthroughBox b;
                b.id             = jb.value("id",             "");
                b.name           = jb.value("name",           "");
                b.posX           = jb.value("posX",           0.0f);
                b.posY           = jb.value("posY",           1.0f);
                b.posZ           = jb.value("posZ",          -1.0f);
                b.rotYaw         = jb.value("rotYaw",         0.0f);
                b.rotPitch       = jb.value("rotPitch",       0.0f);
                b.rotRoll        = jb.value("rotRoll",        0.0f);
                b.scaleWidth     = jb.value("scaleWidth",     0.5f);
                b.scaleHeight    = jb.value("scaleHeight",    0.3f);
                b.scaleDepth     = jb.value("scaleDepth",     0.0f);  // 0 for v1 layouts (flat)
                b.chromaR        = jb.value("chromaR",        0.0f);
                b.chromaG        = jb.value("chromaG",        1.0f);
                b.chromaB        = jb.value("chromaB",        0.502f);
                b.minOpacity     = jb.value("minOpacity",     0.0f);
                b.maxOpacity     = jb.value("maxOpacity",     1.0f);
                b.fadeNearMeters = jb.value("fadeNearMeters", 0.3f);
                b.fadeFarMeters  = jb.value("fadeFarMeters",  1.2f);
                b.visible        = jb.value("visible",        true);
                // overlayHandle is runtime-only — not in JSON, default 0 is correct.
                layout.boxes.push_back(std::move(b));
            }
        }

        return layout;
    }
    catch (const nlohmann::json::exception& e) {
        errorOut = e.what();
        return std::nullopt;
    }
}

// ── Path helpers ──────────────────────────────────────────────────────────────

LayoutStore::LayoutStore() = default;

std::filesystem::path LayoutStore::appdataRoot() const {
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0] != '\0')
        return std::filesystem::path(appdata) / "OpenMixerXR";
    // Fallback: current working directory (covers headless test environments).
    return std::filesystem::current_path() / "OpenMixerXR";
}

std::filesystem::path LayoutStore::layoutsDir() const {
    const auto dir = appdataRoot() / "layouts";
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec)
            LOG_WARN("LayoutStore: could not create layouts dir '{}': {}",
                dir.string(), ec.message());
    }
    return dir;
}

std::filesystem::path LayoutStore::sessionPath() const {
    const auto root = appdataRoot();
    if (!std::filesystem::exists(root)) {
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
    }
    return root / "last_session.json";
}

std::filesystem::path LayoutStore::layoutPath(const std::string& name) const {
    return layoutsDir() / (name + ".json");
}

// ── Private file I/O ──────────────────────────────────────────────────────────

bool LayoutStore::writeToPath(const std::filesystem::path& path, const Layout& layout) {
    const auto tmpPath = std::filesystem::path(path).replace_extension(".json.tmp");
    {
        std::ofstream f(tmpPath, std::ios::out | std::ios::trunc);
        if (!f.is_open()) {
            m_lastError = "cannot open '" + tmpPath.string() + "' for writing";
            return false;
        }
        f << serializeLayout(layout);
        if (!f.good()) {
            m_lastError = "write error on '" + tmpPath.string() + "'";
            return false;
        }
    }   // close file before rename

    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        m_lastError = "rename failed: " + ec.message();
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

std::optional<Layout> LayoutStore::readFromPath(const std::filesystem::path& path) const {
    std::ifstream f(path);
    if (!f.is_open()) {
        m_lastError = "cannot open '" + path.string() + "'";
        return std::nullopt;
    }
    const std::string content{ std::istreambuf_iterator<char>(f),
                                std::istreambuf_iterator<char>() };
    std::string err;
    auto result = deserializeLayout(content, err);
    if (!result)
        m_lastError = "parse error in '" + path.string() + "': " + err;
    return result;
}

// ── Public interface ──────────────────────────────────────────────────────────

bool LayoutStore::save(const Layout& layout) {
    const auto path = layoutPath(layout.name);
    const bool ok   = writeToPath(path, layout);
    if (ok)
        LOG_INFO("LayoutStore: saved '{}' → {}", layout.name, path.string());
    else
        LOG_ERROR("LayoutStore: save failed for '{}': {}", layout.name, m_lastError);
    return ok;
}

std::optional<Layout> LayoutStore::load(const std::string& name) const {
    const auto path = layoutPath(name);
    if (!std::filesystem::exists(path)) {
        m_lastError = "layout '" + name + "' not found";
        LOG_WARN("LayoutStore: {}", m_lastError);
        return std::nullopt;
    }
    auto result = readFromPath(path);
    if (result)
        LOG_INFO("LayoutStore: loaded '{}' ({} boxes)", name, result->boxes.size());
    else
        LOG_ERROR("LayoutStore: load failed for '{}': {}", name, m_lastError);
    return result;
}

std::vector<std::string> LayoutStore::enumerate() const {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(layoutsDir(), ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool LayoutStore::deleteLayout(const std::string& name) {
    const auto path = layoutPath(name);
    std::error_code ec;
    if (!std::filesystem::remove(path, ec)) {
        m_lastError = ec ? ec.message() : "file not found";
        LOG_WARN("LayoutStore: delete '{}' failed: {}", name, m_lastError);
        return false;
    }
    LOG_INFO("LayoutStore: deleted '{}'", name);
    return true;
}

bool LayoutStore::renameLayout(const std::string& oldName, const std::string& newName) {
    const auto oldPath = layoutPath(oldName);
    const auto newPath = layoutPath(newName);

    if (!std::filesystem::exists(oldPath)) {
        m_lastError = "layout '" + oldName + "' not found";
        return false;
    }
    if (std::filesystem::exists(newPath)) {
        m_lastError = "layout '" + newName + "' already exists";
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        m_lastError = ec.message();
        LOG_ERROR("LayoutStore: rename '{}' → '{}' failed: {}", oldName, newName, m_lastError);
        return false;
    }

    // Update the 'name' field inside the JSON so it stays consistent.
    auto opt = readFromPath(newPath);
    if (opt) {
        opt->name = newName;
        writeToPath(newPath, *opt);
    }

    LOG_INFO("LayoutStore: renamed '{}' → '{}'", oldName, newName);
    return true;
}

bool LayoutStore::saveLastSession(const Layout& layout) {
    const bool ok = writeToPath(sessionPath(), layout);
    if (ok)
        LOG_INFO("LayoutStore: last session saved ({} boxes)", layout.boxes.size());
    else
        LOG_ERROR("LayoutStore: last session save failed: {}", m_lastError);
    return ok;
}

std::optional<Layout> LayoutStore::loadLastSession() const {
    const auto path = sessionPath();
    if (!std::filesystem::exists(path)) {
        LOG_DEBUG("LayoutStore: no last_session.json found");
        return std::nullopt;
    }
    auto result = readFromPath(path);
    if (result)
        LOG_INFO("LayoutStore: last session restored ({} boxes)", result->boxes.size());
    else
        LOG_WARN("LayoutStore: last session could not be read: {}", m_lastError);
    return result;
}
