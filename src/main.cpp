#include <windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <string>

#include <openvr.h>

#include "Logger.h"
#include "Config.h"
#include "D3D11Backend.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string executableDir() {
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path().string();
}

static vr::HmdMatrix34_t makeTranslation(float x, float y, float z) {
    vr::HmdMatrix34_t m = {};
    m.m[0][0] = 1.0f;  m.m[0][3] = x;
    m.m[1][1] = 1.0f;  m.m[1][3] = y;
    m.m[2][2] = 1.0f;  m.m[2][3] = z;
    return m;
}

// Ask SteamVR which DXGI adapter it is using and return it.
// Critical on multi-GPU machines — the D3D device must be on the same adapter
// as the compositor or SetOverlayTexture will silently fail.
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

    LUID target;
    target.LowPart  = static_cast<DWORD>(adapterId & 0xFFFFFFFF);
    target.HighPart = static_cast<LONG> (adapterId >> 32);

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.AdapterLuid.LowPart  == target.LowPart &&
            desc.AdapterLuid.HighPart == target.HighPart) {
            // Convert wide description to narrow for logging
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

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    Logger::init();
    LOG_INFO("OpenMixer VR starting");

    // ── 1. Initialise OpenVR ──────────────────────────────────────────────────
    vr::EVRInitError vrErr = vr::VRInitError_None;
    vr::IVRSystem* vrSystem = vr::VR_Init(&vrErr, vr::VRApplication_Overlay);
    if (vrErr != vr::VRInitError_None || !vrSystem) {
        LOG_ERROR("VR_Init failed: {}",
            vr::VR_GetVRInitErrorAsEnglishDescription(vrErr));
        return 1;
    }
    LOG_INFO("OpenVR initialised");

    // ── 2. Register application manifest ──────────────────────────────────────
    const std::string manifestPath =
        (std::filesystem::path(executableDir()) / "manifest.vrmanifest").string();

    vr::EVRApplicationError appErr =
        vr::VRApplications()->AddApplicationManifest(manifestPath.c_str(), false);
    if (appErr != vr::VRApplicationError_None) {
        LOG_WARN("AddApplicationManifest: {}",
            vr::VRApplications()->GetApplicationsErrorNameFromEnum(appErr));
    } else {
        LOG_INFO("Manifest registered: {}", manifestPath);
    }

    appErr = vr::VRApplications()->IdentifyApplication(
        GetCurrentProcessId(), Config::APP_KEY);
    if (appErr != vr::VRApplicationError_None) {
        LOG_WARN("IdentifyApplication: {}",
            vr::VRApplications()->GetApplicationsErrorNameFromEnum(appErr));
    }

    // ── 3. Initialise D3D11 on the same adapter SteamVR is using ─────────────
    auto vrAdapter = findVRAdapter(vrSystem);

    D3D11Backend d3d;
    if (!d3d.init(Config::TEXTURE_WIDTH, Config::TEXTURE_HEIGHT, vrAdapter.Get())) {
        LOG_ERROR("D3D11Backend init failed — aborting");
        vr::VR_Shutdown();
        return 1;
    }

    d3d.clearChromaIfNeeded(Config::CHROMA_R, Config::CHROMA_G, Config::CHROMA_B);

    // ── 4. Create overlay ─────────────────────────────────────────────────────
    vr::VROverlayHandle_t overlayHandle = vr::k_ulOverlayHandleInvalid;
    {
        const std::string key  = std::string(Config::OVERLAY_KEY_PREFIX) + "test0";
        const std::string name = "OpenMixer Test Box";
        vr::EVROverlayError ovErr =
            vr::VROverlay()->CreateOverlay(key.c_str(), name.c_str(), &overlayHandle);
        if (ovErr != vr::VROverlayError_None) {
            LOG_ERROR("CreateOverlay failed: {}",
                vr::VROverlay()->GetOverlayErrorNameFromEnum(ovErr));
            vr::VR_Shutdown();
            return 1;
        }
    }
    LOG_INFO("Overlay created (handle {})", overlayHandle);

    constexpr float debugWidth = 2.0f;

    vr::HmdMatrix34_t transform = makeTranslation(
        Config::DEFAULT_BOX_X, Config::DEFAULT_BOX_Y, Config::DEFAULT_BOX_Z);
    vr::VROverlay()->SetOverlayTransformAbsolute(
        overlayHandle, vr::TrackingUniverseStanding, &transform);
    LOG_INFO("Transform: world-space absolute at ({}, {}, {})",
        Config::DEFAULT_BOX_X, Config::DEFAULT_BOX_Y, Config::DEFAULT_BOX_Z);
    vr::VROverlay()->SetOverlayWidthInMeters(overlayHandle, debugWidth);
    vr::VROverlay()->SetOverlayAlpha(overlayHandle, 1.0f);

    vr::Texture_t vrTex;
    vrTex.handle      = d3d.getSharedHandle();   // DXGI shared HANDLE — required for overlay targets
    vrTex.eType       = vr::TextureType_DXGISharedHandle;
    vrTex.eColorSpace = vr::ColorSpace_Auto;

    if (!vrTex.handle)
        LOG_ERROR("getSharedHandle() returned null — texture was not created with MISC_SHARED");
    else
        LOG_INFO("DXGI shared handle: {:p}", vrTex.handle);

    {
        vr::EVROverlayError texErr = vr::VROverlay()->SetOverlayTexture(overlayHandle, &vrTex);
        if (texErr != vr::VROverlayError_None)
            LOG_ERROR("SetOverlayTexture: {}",
                vr::VROverlay()->GetOverlayErrorNameFromEnum(texErr));
        else
            LOG_INFO("Texture submitted OK");
    }

    {
        vr::EVROverlayError showErr = vr::VROverlay()->ShowOverlay(overlayHandle);
        if (showErr != vr::VROverlayError_None)
            LOG_ERROR("ShowOverlay: {}",
                vr::VROverlay()->GetOverlayErrorNameFromEnum(showErr));
        else
            LOG_INFO("Overlay shown — {}m wide at ({}, {}, {})",
                debugWidth,
                Config::DEFAULT_BOX_X, Config::DEFAULT_BOX_Y, Config::DEFAULT_BOX_Z);
    }

    // ── 5. Main loop ──────────────────────────────────────────────────────────
    LOG_INFO("Entering main loop");
    bool running = true;

    while (running) {
        vr::VREvent_t event;
        while (vr::VRSystem()->PollNextEvent(&event, sizeof(event))) {
            if (event.eventType == vr::VREvent_Quit) {
                LOG_INFO("VREvent_Quit — shutting down");
                vr::VRSystem()->AcknowledgeQuit_Exiting();
                running = false;
            }
        }
        if (!running) break;

        vr::VROverlay()->SetOverlayTexture(overlayHandle, &vrTex);
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }

    // ── 6. Clean shutdown ─────────────────────────────────────────────────────
    if (overlayHandle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->HideOverlay(overlayHandle);
        vr::VROverlay()->DestroyOverlay(overlayHandle);
    }
    d3d.shutdown();
    vr::VR_Shutdown();
    LOG_INFO("Shutdown complete");
    return 0;
}
