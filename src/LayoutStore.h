#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "PassthroughBox.h"

// ── Layout data transfer object ───────────────────────────────────────────────
// Matches SRD §7.1 schema exactly.
// version, name, globalChromaColor, and boxes[] are serialized.
// Runtime-only fields on PassthroughBox (overlayHandle) are NOT serialized.
struct Layout {
    static constexpr int CURRENT_VERSION = 1;

    int         version       = CURRENT_VERSION;
    std::string name;
    float       globalChromaR = 0.000f;
    float       globalChromaG = 1.000f;
    float       globalChromaB = 0.502f;
    std::vector<PassthroughBox> boxes;
};

// ── LayoutStore ───────────────────────────────────────────────────────────────
// Owns %APPDATA%/OpenMixerXR/layouts/ and %APPDATA%/OpenMixerXR/last_session.json.
//
// Safe save: writes to <name>.json.tmp then atomically renames to <name>.json
// so a crash mid-write never corrupts the existing file (§6.2).
//
// JSON schema (SRD §7.1):
//   {
//     "version": 1,
//     "name": "My Setup",
//     "globalChromaColor": [r, g, b],
//     "boxes": [
//       { "id", "name", "posX/Y/Z", "rotYaw/Pitch/Roll",
//         "scaleWidth/Height", "chromaR/G/B",
//         "minOpacity", "maxOpacity", "fadeNearMeters", "fadeFarMeters", "visible" }
//     ]
//   }
class LayoutStore {
public:
    LayoutStore();

    // Returns %APPDATA%/OpenMixerXR/layouts/ and ensures the directory exists.
    std::filesystem::path layoutsDir() const;

    // Returns %APPDATA%/OpenMixerXR/last_session.json path.
    std::filesystem::path sessionPath() const;

    // ── File operations ───────────────────────────────────────────────────────

    // Save a layout by its name field (layout.name is used as the filename).
    // Writes atomically via a .tmp file. Returns false and sets lastError() on failure.
    bool save(const Layout& layout);

    // Load a named layout (pass the bare name, no extension).
    // Returns empty optional and sets lastError() on failure.
    std::optional<Layout> load(const std::string& name) const;

    // Returns all saved layout names sorted alphabetically (no .json extension).
    std::vector<std::string> enumerate() const;

    // Delete a named layout file. Returns false if not found or on error.
    bool deleteLayout(const std::string& name);

    // Rename a layout. Returns false if old doesn't exist or new already exists.
    bool renameLayout(const std::string& oldName, const std::string& newName);

    // ── Auto-session (FR-22/FR-23) ────────────────────────────────────────────

    bool saveLastSession(const Layout& layout);
    std::optional<Layout> loadLastSession() const;

    // ── Serialization helpers (also used by the unit test) ───────────────────
    // toJsonString / fromJsonString operate on strings, not files, so they can
    // be tested in isolation without touching the filesystem.

    static std::string            serializeLayout(const Layout& layout);
    static std::optional<Layout>  deserializeLayout(const std::string& json,
                                                      std::string& errorOut);

    // ── Error reporting ───────────────────────────────────────────────────────
    const std::string& lastError() const { return m_lastError; }
    void               clearError()      { m_lastError.clear(); }

private:
    bool writeToPath(const std::filesystem::path& path, const Layout& layout);
    std::optional<Layout> readFromPath(const std::filesystem::path& path) const;

    std::filesystem::path layoutPath(const std::string& name) const;
    std::filesystem::path appdataRoot() const;

    mutable std::string m_lastError;
};
