#include "DashboardUI.h"
#include "Config.h"
#include "Logger.h"
#include "MathHelpers.h"
#include "GrabController.h"

#include <imgui_impl_dx11.h>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cstdio>

// ── UI helper ─────────────────────────────────────────────────────────────────

// Renders:  Label     [−]  <DragFloat>  [+]
//
// The [−]/[+] buttons adjust by `step` each click, making precise edits easy
// with a VR laser pointer. The DragFloat still allows freehand dragging for
// coarser/faster changes.
static bool StepFloat(const char* id, const char* label,
                       float* v, float step, float vmin, float vmax,
                       const char* fmt) {
    bool changed = false;

    const float btnW  = ImGui::GetFrameHeight();
    const float labelW = 90.0f;
    const float dragW  = std::max(
        ImGui::GetContentRegionAvail().x - labelW - (btnW + 4.f) * 2,
        60.f);

    ImGui::PushID(id);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
    ImGui::SameLine(labelW);

    if (ImGui::Button("-", ImVec2(btnW, 0.f))) {
        *v = std::max(vmin, *v - step);
        changed = true;
    }
    ImGui::SameLine(0.f, 2.f);
    ImGui::SetNextItemWidth(dragW);
    if (ImGui::DragFloat("##v", v, step * 0.2f, vmin, vmax, fmt))
        changed = true;
    ImGui::SameLine(0.f, 2.f);
    if (ImGui::Button("+", ImVec2(btnW, 0.f))) {
        *v = std::min(vmax, *v + step);
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

bool DashboardUI::init(ID3D11Device*        device,
                        ID3D11DeviceContext* context,
                        OverlayManager&      overlayMgr,
                        DeviceTracker&       tracker,
                        GrabController&      grabCtrl) {
    m_device     = device;
    m_context    = context;
    m_overlayMgr = &overlayMgr;
    m_tracker    = &tracker;
    m_grab       = &grabCtrl;

    if (!createRenderTarget())      return false;
    if (!createDashboardHandles())  return false;

    // Create and configure the ImGui context.
    // No Win32 platform backend — we feed IO manually (Phase 2 finding #10).
    m_imguiCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imguiCtx);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize        = ImVec2(static_cast<float>(RT_W), static_cast<float>(RT_H));
    io.DeltaTime          = 1.0f / 90.0f;
    io.IniFilename        = nullptr;   // disable imgui.ini writes
    io.ConfigFlags       |= ImGuiConfigFlags_NavEnableKeyboard;

    // Style — dark theme suits a VR environment.
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.0f;
    ImGui::GetStyle().FrameRounding  = 3.0f;

    // Init the DX11 renderer backend — reuses existing device/context (finding #2).
    ImGui_ImplDX11_Init(m_device, m_context);

    // Set mouse scale so OpenVR mouse coords arrive in pixel space.
    vr::HmdVector2_t scale{ static_cast<float>(RT_W), static_cast<float>(RT_H) };
    vr::VROverlay()->SetOverlayMouseScale(m_mainHandle, &scale);

    m_lastCalibratePos = m_tracker->getHmdPosition();
    m_initialized      = true;
    LOG_INFO("DashboardUI initialised");
    return true;
}

void DashboardUI::shutdown() {
    if (!m_initialized) return;

    if (m_imguiCtx) {
        ImGui::SetCurrentContext(m_imguiCtx);
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext(m_imguiCtx);
        m_imguiCtx = nullptr;
    }

    // Destroy both overlay handles — leaving either alive causes ghost art in SteamVR
    // (Phase 2 finding #5).
    if (m_thumbnailHandle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->DestroyOverlay(m_thumbnailHandle);
        m_thumbnailHandle = vr::k_ulOverlayHandleInvalid;
    }
    if (m_mainHandle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->DestroyOverlay(m_mainHandle);
        m_mainHandle = vr::k_ulOverlayHandleInvalid;
    }

    m_rtv.Reset();
    m_rtTexture.Reset();
    m_initialized = false;
    LOG_INFO("DashboardUI shutdown");
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool DashboardUI::createRenderTarget() {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width              = RT_W;
    desc.Height             = RT_H;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags          = D3D11_RESOURCE_MISC_SHARED;  // Phase 1 finding #2

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, m_rtTexture.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("DashboardUI: CreateTexture2D failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    hr = m_device->CreateRenderTargetView(m_rtTexture.Get(), nullptr, m_rtv.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("DashboardUI: CreateRenderTargetView failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    LOG_DEBUG("DashboardUI: render target created ({}x{})", RT_W, RT_H);
    return true;
}

bool DashboardUI::createDashboardHandles() {
    vr::EVROverlayError err = vr::VROverlay()->CreateDashboardOverlay(
        "openmixer.dashboard",
        "OpenMixer VR",
        &m_mainHandle,
        &m_thumbnailHandle
    );

    if (err != vr::VROverlayError_None) {
        LOG_ERROR("DashboardUI: CreateDashboardOverlay failed: {}",
            vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
        return false;
    }

    // Set dashboard panel size (meters) — aspect matches 1280×720.
    vr::VROverlay()->SetOverlayWidthInMeters(m_mainHandle, 2.5f);

    // Thumbnail: fill with the chroma key color as a simple colored icon.
    constexpr int  TW = 64, TH = 64;
    uint8_t thumbBuf[TW * TH * 4];
    const uint8_t cr = static_cast<uint8_t>(Config::CHROMA_R * 255.f);
    const uint8_t cg = static_cast<uint8_t>(Config::CHROMA_G * 255.f);
    const uint8_t cb = static_cast<uint8_t>(Config::CHROMA_B * 255.f);
    for (int i = 0; i < TW * TH; ++i) {
        thumbBuf[i * 4 + 0] = cr;
        thumbBuf[i * 4 + 1] = cg;
        thumbBuf[i * 4 + 2] = cb;
        thumbBuf[i * 4 + 3] = 255;
    }
    vr::VROverlay()->SetOverlayRaw(m_thumbnailHandle, thumbBuf, TW, TH, 4);

    LOG_INFO("DashboardUI: dashboard handles created (main={}, thumb={})",
        m_mainHandle, m_thumbnailHandle);
    return true;
}

// ── Event handling ────────────────────────────────────────────────────────────

void DashboardUI::handleSystemEvent(const vr::VREvent_t& event) {
    switch (event.eventType) {
    case vr::VREvent_DashboardActivated:
        m_dashboardActive = true;
        // Seed DeltaTime and mouse pos for first frame so ImGui doesn't show stale state.
        if (m_imguiCtx) {
            ImGui::SetCurrentContext(m_imguiCtx);
            ImGui::GetIO().DeltaTime = 1.0f / 90.0f;
        }
        LOG_DEBUG("DashboardUI: activated");
        break;
    case vr::VREvent_DashboardDeactivated:
        m_dashboardActive = false;
        LOG_DEBUG("DashboardUI: deactivated");
        break;
    default:
        break;
    }
}

void DashboardUI::pollInput() {
    if (!m_initialized || m_mainHandle == vr::k_ulOverlayHandleInvalid) return;

    ImGui::SetCurrentContext(m_imguiCtx);
    ImGuiIO& io = ImGui::GetIO();

    vr::VREvent_t event;
    while (vr::VROverlay()->PollNextOverlayEvent(m_mainHandle, &event, sizeof(event))) {
        switch (event.eventType) {
        case vr::VREvent_MouseMove:
            // SteamVR sends X left→right, Y bottom→top; ImGui wants Y top→bottom.
            io.AddMousePosEvent(
                event.data.mouse.x,
                static_cast<float>(RT_H) - event.data.mouse.y
            );
            break;
        case vr::VREvent_MouseButtonDown:
            io.AddMouseButtonEvent(0, true);
            break;
        case vr::VREvent_MouseButtonUp:
            io.AddMouseButtonEvent(0, false);
            break;
        default:
            break;
        }
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void DashboardUI::render() {
    if (!m_initialized || !m_dashboardActive) return;
    if (m_mainHandle == vr::k_ulOverlayHandleInvalid) return;

    ImGui::SetCurrentContext(m_imguiCtx);

    // Advance DeltaTime manually (no Win32 platform backend).
    ImGui::GetIO().DeltaTime = 1.0f / 90.0f;

    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    buildUI();

    ImGui::Render();

    // Bind our RT and viewport, then let ImGui render into it.
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(RT_W);
    vp.Height   = static_cast<float>(RT_H);
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);

    constexpr float bg[4] = { 0.08f, 0.08f, 0.10f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), bg);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Flush so the compositor sees fresh pixels (Phase 1 finding #4).
    m_context->Flush();

    // Submit texture to the dashboard overlay (Phase 1 finding #1).
    Microsoft::WRL::ComPtr<IDXGIResource> dxgiRes;
    if (SUCCEEDED(m_rtTexture.As(&dxgiRes))) {
        HANDLE sharedHandle = nullptr;
        dxgiRes->GetSharedHandle(&sharedHandle);
        if (sharedHandle) {
            vr::Texture_t vrTex;
            vrTex.handle      = sharedHandle;
            vrTex.eType       = vr::TextureType_DXGISharedHandle;
            vrTex.eColorSpace = vr::ColorSpace_Auto;
            vr::VROverlay()->SetOverlayTexture(m_mainHandle, &vrTex);
        }
    }
}

// ── UI construction ───────────────────────────────────────────────────────────

void DashboardUI::snapSelectedToHmd() {
    if (!m_tracker->isHmdTracked()) return;

    PassthroughBox* box = m_overlayMgr->boxAt(static_cast<std::size_t>(m_selectedBox));
    if (!box) return;

    const glm::mat4 pose    = m_tracker->getHmdPose();
    const glm::vec3 hmdPos  = glm::vec3(pose[3]);
    // Column 2 of the rotation matrix is the +Z axis; in OpenVR -Z is forward.
    const glm::vec3 forward = -glm::normalize(glm::vec3(pose[2]));
    const glm::vec3 snapPos = hmdPos + forward * 1.0f;

    box->posX     = snapPos.x;
    box->posY     = snapPos.y;
    box->posZ     = snapPos.z;
    box->rotPitch = 0.0f;
    box->rotYaw   = 0.0f;
    box->rotRoll  = 0.0f;
    LOG_INFO("DashboardUI: snapped '{}' to HMD ({:.2f}, {:.2f}, {:.2f})",
        box->id, snapPos.x, snapPos.y, snapPos.z);
}

void DashboardUI::recalibrate() {
    if (!m_tracker->isHmdTracked()) return;

    const glm::vec3 current = m_tracker->getHmdPosition();
    const glm::vec3 delta   = current - m_lastCalibratePos;

    for (std::size_t i = 0; i < m_overlayMgr->boxCount(); ++i) {
        PassthroughBox* box = m_overlayMgr->boxAt(i);
        box->posX += delta.x;
        box->posY += delta.y;
        box->posZ += delta.z;
    }

    m_lastCalibratePos = current;
    LOG_INFO("DashboardUI: recalibrated — delta ({:.3f}, {:.3f}, {:.3f})",
        delta.x, delta.y, delta.z);
}

void DashboardUI::buildUI() {
    // Full-screen window — no title bar, no resize, no move.
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(RT_W), static_cast<float>(RT_H)));
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar);

    ImGui::Text("OpenMixer VR");
    ImGui::SameLine();
    ImGui::TextDisabled("  Phase 3.5");
    ImGui::Separator();

    const float leftW  = 240.0f;
    const float rightW = ImGui::GetContentRegionAvail().x - leftW - ImGui::GetStyle().ItemSpacing.x;
    const int   nBoxes = static_cast<int>(m_overlayMgr->boxCount());

    // Reserve height for the bottom bar: checkbox row + chroma row + layout-stubs row + separator.
    const float lineH   = ImGui::GetFrameHeightWithSpacing();
    const float bottomH = lineH * 3.0f + ImGui::GetStyle().ItemSpacing.y + 4.0f;
    const float panelH  = ImGui::GetContentRegionAvail().y - bottomH;

    // ── Left: box list + Add/Delete (inside the panel) ───────────────────────
    ImGui::BeginChild("##boxlist", ImVec2(leftW, panelH), true);

    ImGui::TextDisabled("Boxes (%d)", nBoxes);
    ImGui::Separator();

    for (int i = 0; i < nBoxes; ++i) {
        PassthroughBox* box = m_overlayMgr->boxAt(static_cast<std::size_t>(i));
        if (!box) continue;

        ImGui::PushID(i);
        ImGui::Checkbox("##vis", &box->visible);   // FR-08: visibility toggle
        ImGui::SameLine();
        const bool selected   = (m_selectedBox == i);
        const bool inMoveMode = m_grab->isMoveMode() && m_grab->boxIndex() == i;
        if (inMoveMode) {
            if (m_grab->isGrabbing())
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), u8"\u25CF");  // filled circle = grabbing
            else
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"\u25CB");  // open circle = armed
            ImGui::SameLine();
        }
        if (ImGui::Selectable(box->name.empty() ? box->id.c_str() : box->name.c_str(),
                               selected, ImGuiSelectableFlags_None))
            m_selectedBox = i;
        ImGui::PopID();
    }

    // Pin Add/Delete to the bottom of the left panel.
    const float addBtnY = panelH
        - ImGui::GetFrameHeightWithSpacing()
        - ImGui::GetStyle().WindowPadding.y;
    ImGui::SetCursorPosY(addBtnY);
    ImGui::Separator();

    if (ImGui::Button("+ Add")) {
        // FR-01: new box spawned 1m in front of HMD, or at default pos if not tracked.
        PassthroughBox nb;
        char idBuf[32];
        std::snprintf(idBuf, sizeof(idBuf), "box%d", m_nextBoxId++);
        nb.id   = idBuf;
        nb.name = idBuf;
        if (m_tracker->isHmdTracked()) {
            const glm::mat4 pose    = m_tracker->getHmdPose();
            const glm::vec3 hmdPos  = glm::vec3(pose[3]);
            const glm::vec3 forward = -glm::normalize(glm::vec3(pose[2]));
            const glm::vec3 sp      = hmdPos + forward * 1.0f;
            nb.posX = sp.x; nb.posY = sp.y; nb.posZ = sp.z;
        }
        m_overlayMgr->addBox(nb);
        m_selectedBox = nBoxes;   // auto-select the new box
    }
    ImGui::SameLine();
    if (ImGui::Button("- Delete") && nBoxes > 0) {
        // FR-02: delete selected box.
        const PassthroughBox* box =
            m_overlayMgr->boxAt(static_cast<std::size_t>(m_selectedBox));
        if (box) {
            m_overlayMgr->removeBox(box->id);
            m_selectedBox = std::max(0, std::min(m_selectedBox, nBoxes - 2));
        }
    }

    ImGui::EndChild();

    // ── SameLine immediately after EndChild so ##props appears to the right ──
    ImGui::SameLine();

    // ── Right: properties ─────────────────────────────────────────────────────
    ImGui::BeginChild("##props", ImVec2(rightW, panelH), true);

    PassthroughBox* sel = m_overlayMgr->boxAt(static_cast<std::size_t>(m_selectedBox));
    if (sel) {
        // FR-32: name rename.
        char nameBuf[128] = {};
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", sel->name.c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
            sel->name = nameBuf;

        ImGui::Separator();

        // FR-03: position (5 cm per click).
        StepFloat("posx", "Pos X", &sel->posX, 0.05f, -10.f, 10.f, "%.3f m");
        StepFloat("posy", "Pos Y", &sel->posY, 0.05f, -10.f, 10.f, "%.3f m");
        StepFloat("posz", "Pos Z", &sel->posZ, 0.05f, -10.f, 10.f, "%.3f m");

        // FR-04: rotation (5° per click, YXZ convention — Phase 2 finding #6).
        StepFloat("yaw",   "Yaw",   &sel->rotYaw,   5.f, -180.f, 180.f, "%.1f deg");
        StepFloat("pitch", "Pitch", &sel->rotPitch, 5.f,  -90.f,  90.f, "%.1f deg");
        StepFloat("roll",  "Roll",  &sel->rotRoll,  5.f, -180.f, 180.f, "%.1f deg");

        // FR-05: scale (5 cm per click).
        StepFloat("width",  "Width",  &sel->scaleWidth,  0.05f, 0.05f, 5.f, "%.2f m");
        StepFloat("height", "Height", &sel->scaleHeight, 0.05f, 0.05f, 5.f, "%.2f m");

        ImGui::Separator();

        // FR-09: per-box chroma color.
        float chromaColor[3] = { sel->chromaR, sel->chromaG, sel->chromaB };
        if (ImGui::ColorEdit3("Chroma", chromaColor)) {
            sel->chromaR = chromaColor[0];
            sel->chromaG = chromaColor[1];
            sel->chromaB = chromaColor[2];
        }

        // Opacity fade parameters (5 cm / 0.05 opacity per click).
        StepFloat("fnear", "Fade near", &sel->fadeNearMeters, 0.05f, 0.f,              sel->fadeFarMeters, "%.2f m");
        StepFloat("ffar",  "Fade far",  &sel->fadeFarMeters,  0.05f, sel->fadeNearMeters, 10.f, "%.2f m");
        StepFloat("minalpha", "Min alpha", &sel->minOpacity, 0.05f, 0.f, sel->maxOpacity, "%.2f");
        StepFloat("maxalpha", "Max alpha", &sel->maxOpacity, 0.05f, sel->minOpacity, 1.f, "%.2f");

        ImGui::Separator();

        // FR-06 / FR-10: Snap to HMD.
        const bool tracked = m_tracker->isHmdTracked();
        if (!tracked) ImGui::BeginDisabled();
        if (ImGui::Button("Snap to HMD")) snapSelectedToHmd();
        if (!tracked) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("(tracking lost)");
        }

        ImGui::Separator();

        // ── Move mode (Phase 3.5) ─────────────────────────────────────────────
        // Move mode stays on until explicitly disabled, allowing repeated
        // grip-to-grab cycles without returning to the dashboard each time.
        // Each grip press: latch position + rotation delta.
        // Grip release: box drops at new transform, auto re-arms for next grip.
        const bool rightTracked  = m_tracker->isRightControllerTracked();
        const bool modeForThis   = m_grab->isMoveMode() && m_grab->boxIndex() == m_selectedBox;
        const bool modeElsewhere = m_grab->isMoveMode() && !modeForThis;

        if (modeForThis) {
            // Status indicator.
            if (m_grab->isGrabbing())
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "GRABBING");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "MOVE MODE ON  (squeeze grip to grab)");
            if (ImGui::Button("Disable Move Mode"))
                m_grab->disableMoveMode();
        } else {
            // Enable button — disabled when another box is in move mode or
            // the right controller isn't tracked.
            const bool canEnable = rightTracked && !modeElsewhere;
            if (!canEnable) ImGui::BeginDisabled();
            if (ImGui::Button("Enable Move Mode"))
                m_grab->enableMoveMode(m_selectedBox);
            if (!canEnable) ImGui::EndDisabled();

            if (!rightTracked) {
                ImGui::SameLine();
                ImGui::TextDisabled("(controller not tracked)");
            } else if (modeElsewhere) {
                ImGui::SameLine();
                ImGui::TextDisabled("(move mode active for another box)");
            } else {
                ImGui::SameLine();
                ImGui::TextDisabled("grip = grab + rotate, release = drop");
            }
        }
    } else {
        ImGui::TextDisabled("No box selected");
    }

    ImGui::EndChild();

    // ── Bottom bar (below both panels) ────────────────────────────────────────
    ImGui::Separator();

    // Row 1: visibility toggle.
    bool hideWhenDash = m_overlayMgr->getHideBoxesWhenDashboard();
    if (ImGui::Checkbox("Hide boxes when dashboard open", &hideWhenDash))
        m_overlayMgr->setHideBoxesWhenDashboard(hideWhenDash);
    ImGui::SameLine();
    ImGui::TextDisabled("(uncheck to position boxes live)");

    // Row 2: FR-35 global chroma + FR-36 recalibrate.
    ImGui::SetNextItemWidth(160.0f);
    float gc[3] = { m_globalChromaR, m_globalChromaG, m_globalChromaB };
    if (ImGui::ColorEdit3("Global chroma", gc)) {
        m_globalChromaR = gc[0]; m_globalChromaG = gc[1]; m_globalChromaB = gc[2];
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply to all")) {
        for (std::size_t i = 0; i < m_overlayMgr->boxCount(); ++i) {
            PassthroughBox* b = m_overlayMgr->boxAt(i);
            b->chromaR = m_globalChromaR;
            b->chromaG = m_globalChromaG;
            b->chromaB = m_globalChromaB;
        }
    }
    ImGui::SameLine();
    const bool tracked = m_tracker->isHmdTracked();
    if (!tracked) ImGui::BeginDisabled();
    if (ImGui::Button("Recalibrate")) recalibrate();
    if (!tracked) ImGui::EndDisabled();

    // Row 3: FR-34 stubs.
    ImGui::BeginDisabled();
    ImGui::Button("Save Layout");
    ImGui::SameLine();
    ImGui::Button("Load Layout");
    ImGui::SameLine();
    ImGui::Button("Delete Layout");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(Phase 4)");

    ImGui::End();
}

// ── Reconnect helpers ─────────────────────────────────────────────────────────

void DashboardUI::closeOverlays() {
    if (m_thumbnailHandle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->DestroyOverlay(m_thumbnailHandle);
        m_thumbnailHandle = vr::k_ulOverlayHandleInvalid;
    }
    if (m_mainHandle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->DestroyOverlay(m_mainHandle);
        m_mainHandle = vr::k_ulOverlayHandleInvalid;
    }
    m_dashboardActive = false;
    LOG_INFO("DashboardUI: overlay handles closed for reconnect");
}

bool DashboardUI::reopenOverlays() {
    if (!createDashboardHandles()) return false;

    // Re-apply mouse scale and width.
    vr::HmdVector2_t scale{ static_cast<float>(RT_W), static_cast<float>(RT_H) };
    vr::VROverlay()->SetOverlayMouseScale(m_mainHandle, &scale);
    vr::VROverlay()->SetOverlayWidthInMeters(m_mainHandle, 2.5f);

    LOG_INFO("DashboardUI: overlays reopened");
    return true;
}
