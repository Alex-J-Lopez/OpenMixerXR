#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <openvr.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include <imgui.h>

#include "LayoutStore.h"
#include "OverlayManager.h"
#include "DeviceTracker.h"
#include "GrabController.h"

// DashboardUI — SteamVR dashboard panel driven by Dear ImGui.
//
// Architecture (single main thread, Phase 2 finding #10):
//   - Owns two VROverlayHandle_t (main + thumbnail); both destroyed before VR_Shutdown.
//   - 1280×720 D3D11 RT with D3D11_RESOURCE_MISC_SHARED (Phase 1 finding #2).
//     Submitted as TextureType_DXGISharedHandle (Phase 1 finding #1).
//   - No Win32 platform backend — io.DisplaySize/DeltaTime set manually each frame.
//   - Input: SteamVR laser pointer events → ImGui mouse IO via pollInput().
//   - Render: only when m_dashboardActive is true (SRD §6.1 — no work when closed).
//
// Call order each frame:
//   1. handleSystemEvent() for each VR system event
//   2. pollInput()          reads overlay mouse events
//   3. render()             builds + submits ImGui frame (no-op if dashboard closed)
class DashboardUI {
public:
    DashboardUI()  = default;
    ~DashboardUI() { shutdown(); }

    DashboardUI(const DashboardUI&)            = delete;
    DashboardUI& operator=(const DashboardUI&) = delete;

    // device and context must outlive this object (owned by D3D11Backend).
    // overlayMgr, tracker, grabCtrl, and layoutStore must outlive this object.
    bool init(ID3D11Device*        device,
              ID3D11DeviceContext* context,
              OverlayManager&      overlayMgr,
              DeviceTracker&       tracker,
              GrabController&      grabCtrl,
              LayoutStore&         layoutStore);
    void shutdown();

    // Feed system-wide VR events (VREvent_DashboardActivated/Deactivated).
    void handleSystemEvent(const vr::VREvent_t& event);

    // Poll overlay-specific mouse/laser events and feed them to ImGui IO.
    void pollInput();

    // Build and submit the ImGui frame. No-op if dashboard is not active.
    void render();

    // Destroy both overlay handles (call before VR_Shutdown).
    void closeOverlays();

    // Recreate both overlay handles (call after VR_Init).
    bool reopenOverlays();

    // ── Layout persistence (Phase 4) ──────────────────────────────────────────

    // Try to restore the last session from disk.
    // If successful, clears OverlayManager boxes and loads the session's boxes.
    // Returns true if a session was found and applied.
    // Call after init() and before the main loop.
    bool tryRestoreSession();

    // Build a Layout from current state and save it as last_session.json.
    // Call on VREvent_Quit before overlay teardown.
    void saveSession();

private:
    bool createDashboardHandles();
    bool createRenderTarget();
    void buildUI();
    void snapSelectedToHmd();
    void recalibrate();

    // ── Layout helpers ────────────────────────────────────────────────────────
    void applyLayout(const Layout& layout);
    void saveLayout();
    void loadLayout();
    void deleteLayout();
    void renameLayout();
    void refreshLayoutList();
    Layout buildCurrentLayout(const std::string& name) const;

    ID3D11Device*        m_device      = nullptr;
    ID3D11DeviceContext* m_context     = nullptr;
    OverlayManager*      m_overlayMgr  = nullptr;
    DeviceTracker*       m_tracker     = nullptr;
    GrabController*      m_grab        = nullptr;
    LayoutStore*         m_layoutStore = nullptr;

    // Two handles from CreateDashboardOverlay (Phase 2 finding #5).
    vr::VROverlayHandle_t m_mainHandle      = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t m_thumbnailHandle = vr::k_ulOverlayHandleInvalid;

    // 1280×720 shared render target — ImGui renders here, then submitted to OpenVR.
    Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_rtTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;

    static constexpr uint32_t RT_W = 1280;
    static constexpr uint32_t RT_H = 720;

    ImGuiContext* m_imguiCtx = nullptr;

    // UI state
    int   m_selectedBox   = 0;
    float m_globalChromaR = 0.000f;
    float m_globalChromaG = 1.000f;
    float m_globalChromaB = 0.502f;

    // Recalibrate — stores HMD position at last calibrate press (§8.4).
    glm::vec3 m_lastCalibratePos = glm::vec3(0.f);

    int  m_nextBoxId      = 10;  // monotone counter for generated box IDs
    bool m_dashboardActive = false;
    bool m_initialized     = false;

    // ── Layout panel state ───────────────────────────────────────────────────
    char                     m_layoutNameBuf[128] = {};
    int                      m_selectedLayout     = -1;
    std::vector<std::string> m_layoutList;
    std::string              m_layoutStatusMsg;   // prefix '!' = error (red), else success (green)
    bool                     m_keyboardOpen       = false;
};
