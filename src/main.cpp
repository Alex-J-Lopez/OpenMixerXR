// Threading model (Phase 2 decision, reconfirmed for Phase 3):
//   Single main thread only. VR events, D3D11 context calls, ImGui rendering, and overlay
//   updates are all sequential. ImGui renders only when the dashboard is active (§6.1).
//   A second thread would require a deferred D3D11 context — deferred to Phase 5 if needed.

#include <windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <string>
#include <array>

#include <openvr.h>

#include "Logger.h"
#include "Config.h"
#include "D3D11Backend.h"
#include "OverlayManager.h"
#include "DeviceTracker.h"
#include "PassthroughBox.h"
#include "DashboardUI.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string executableDir() {
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path().string();
}

// Ask SteamVR which DXGI adapter it is using.
// The D3D11 device must be on the same adapter as the compositor or
// SetOverlayTexture will silently produce a black texture (Phase 1 finding #3).
static Microsoft::WRL::ComPtr<IDXGIAdapter> findVRAdapter(vr::IVRSystem* vrSystem) {
    uint64_t adapterId = 0;
    vrSystem->GetOutputDevice(&adapterId, vr::TextureType_DirectX);
    if (adapterId == 0) {
        LOG_WARN("GetOutputDevice returned 0 — using default adapter");
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
            reinterpret_cast<void**>(factory.GetAddressOf())))) {
        LOG_WARN("CreateDXGIFactory1 failed — using default adapter");
        return nullptr;
    }

    const DWORD lo = static_cast<DWORD>(adapterId & 0xFFFFFFFF);
    const LONG  hi = static_cast<LONG> (adapterId >> 32);

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.AdapterLuid.LowPart == lo && desc.AdapterLuid.HighPart == hi) {
            char name[128] = {};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
            LOG_INFO("VR adapter: {}", name);
            return adapter;
        }
        adapter.Reset();
    }

    LOG_WARN("Could not match VR adapter LUID — using default adapter");
    return nullptr;
}

// Initialise OpenVR and register the application manifest.
// Extracted so it can be called again after a reconnect.
static bool setupVR(vr::IVRSystem*& vrSystem, const std::string& manifestPath) {
    vr::EVRInitError vrErr = vr::VRInitError_None;
    vrSystem = vr::VR_Init(&vrErr, vr::VRApplication_Overlay);
    if (vrErr != vr::VRInitError_None || !vrSystem) {
        LOG_ERROR("VR_Init failed: {}",
            vr::VR_GetVRInitErrorAsEnglishDescription(vrErr));
        return false;
    }

    vr::EVRApplicationError appErr =
        vr::VRApplications()->AddApplicationManifest(manifestPath.c_str(), false);
    if (appErr != vr::VRApplicationError_None)
        LOG_WARN("AddApplicationManifest: {}",
            vr::VRApplications()->GetApplicationsErrorNameFromEnum(appErr));

    appErr = vr::VRApplications()->IdentifyApplication(
        GetCurrentProcessId(), Config::APP_KEY);
    if (appErr != vr::VRApplicationError_None)
        LOG_WARN("IdentifyApplication: {}",
            vr::VRApplications()->GetApplicationsErrorNameFromEnum(appErr));

    return true;
}

// Build the three hardcoded validation boxes at distinct positions/orientations.
// All use the same chroma key color to match Virtual Desktop's default.
static std::array<PassthroughBox, 3> makeDefaultBoxes() {
    std::array<PassthroughBox, 3> boxes;

    // Box 0 — directly in front of the standing origin, full-size
    boxes[0].id           = "box0";
    boxes[0].name         = "Front";
    boxes[0].posX         =  0.0f;
    boxes[0].posY         =  1.0f;
    boxes[0].posZ         = -1.0f;
    boxes[0].scaleWidth   =  0.8f;

    // Box 1 — to the left, slightly rotated inward (15° yaw)
    boxes[1].id           = "box1";
    boxes[1].name         = "Left";
    boxes[1].posX         = -1.0f;
    boxes[1].posY         =  1.0f;
    boxes[1].posZ         =  0.0f;
    boxes[1].rotYaw       = 15.0f;
    boxes[1].scaleWidth   =  0.8f;

    // Box 2 — to the right, slightly rotated inward (-15° yaw)
    boxes[2].id           = "box2";
    boxes[2].name         = "Right";
    boxes[2].posX         =  1.0f;
    boxes[2].posY         =  1.0f;
    boxes[2].posZ         =  0.0f;
    boxes[2].rotYaw       = -15.0f;
    boxes[2].scaleWidth   =  0.8f;

    return boxes;
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    Logger::init();
    LOG_INFO("OpenMixer VR starting (Phase 3)");

    const std::string manifestPath =
        (std::filesystem::path(executableDir()) / "manifest.vrmanifest").string();

    // ── 1. Initialise OpenVR ──────────────────────────────────────────────────
    vr::IVRSystem* vrSystem = nullptr;
    if (!setupVR(vrSystem, manifestPath))
        return 1;
    LOG_INFO("OpenVR initialised");

    // ── 2. Initialise D3D11 on SteamVR's adapter ──────────────────────────────
    auto vrAdapter = findVRAdapter(vrSystem);

    D3D11Backend d3d;
    if (!d3d.init(Config::TEXTURE_WIDTH, Config::TEXTURE_HEIGHT, vrAdapter.Get())) {
        LOG_ERROR("D3D11Backend init failed");
        vr::VR_Shutdown();
        return 1;
    }

    // ── 3. Initialise OverlayManager and add the three boxes ──────────────────
    OverlayManager overlayManager;
    if (!overlayManager.init(d3d.getDevice(), d3d.getContext(),
                              Config::TEXTURE_WIDTH, Config::TEXTURE_HEIGHT)) {
        vr::VR_Shutdown();
        return 1;
    }
    overlayManager.reserveBoxes(64);   // prevent reallocation while UI holds pointers

    for (const auto& box : makeDefaultBoxes()) {
        if (!overlayManager.addBox(box)) {
            LOG_ERROR("Failed to add box '{}'", box.id);
            vr::VR_Shutdown();
            return 1;
        }
    }
    LOG_INFO("{} boxes registered", overlayManager.boxCount());

    // ── 4. Initialise DashboardUI ──────────────────────────────────────────────
    DeviceTracker tracker;
    DashboardUI   dashboardUI;
    if (!dashboardUI.init(d3d.getDevice(), d3d.getContext(), overlayManager, tracker)) {
        LOG_ERROR("DashboardUI init failed");
        overlayManager.shutdown();
        vr::VR_Shutdown();
        return 1;
    }

    // ── 5. Main loop ──────────────────────────────────────────────────────────
    bool running = true;

    while (running) {

        // Detect compositor loss (SRD §6.2 reconnect).
        if (!vr::VRSystem() || !vr::VROverlay()) {
            LOG_WARN("VR interfaces lost — attempting reconnect");
            overlayManager.closeOverlays();
            dashboardUI.closeOverlays();
            vr::VR_Shutdown();
            vrSystem = nullptr;

            int backoffMs = 1000;
            bool reconnected = false;
            for (int attempt = 1; attempt <= 10 && !reconnected; ++attempt) {
                LOG_INFO("Reconnect attempt {}/10 (waiting {}ms)...", attempt, backoffMs);
                std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
                backoffMs = std::min(backoffMs * 2, 30000);

                if (setupVR(vrSystem, manifestPath)) {
                    reconnected = true;
                    LOG_INFO("Reconnected to SteamVR");
                }
            }

            if (!reconnected) {
                LOG_ERROR("Reconnect failed — exiting");
                break;
            }

            if (!overlayManager.reopenOverlays()) {
                LOG_ERROR("reopenOverlays failed after reconnect — exiting");
                break;
            }
            if (!dashboardUI.reopenOverlays()) {
                LOG_ERROR("DashboardUI reopenOverlays failed after reconnect — exiting");
                break;
            }
        }

        // Poll VR events.
        vr::VREvent_t event;
        while (vr::VRSystem()->PollNextEvent(&event, sizeof(event))) {
            overlayManager.handleEvent(event);
            dashboardUI.handleSystemEvent(event);
            if (event.eventType == vr::VREvent_Quit) {
                LOG_INFO("VREvent_Quit — shutting down");
                vr::VRSystem()->AcknowledgeQuit_Exiting();
                running = false;
            }
        }
        if (!running) break;

        // Update HMD pose then run per-box frame update and dashboard render.
        tracker.update(vrSystem);
        overlayManager.frame(tracker.getHmdPosition());
        dashboardUI.pollInput();
        dashboardUI.render();

        std::this_thread::sleep_for(std::chrono::milliseconds(11));   // ~90 Hz
    }

    // ── 6. Clean shutdown ─────────────────────────────────────────────────────
    dashboardUI.shutdown();          // destroys both dashboard handles
    overlayManager.shutdown();       // destroys all world overlay handles before VR_Shutdown
    d3d.shutdown();
    vr::VR_Shutdown();
    LOG_INFO("Shutdown complete");
    return 0;
}
