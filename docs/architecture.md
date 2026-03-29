# Architecture — OpenMixer XR

This document describes the internal architecture of OpenMixer XR for contributors and
anyone extending the codebase. It is a companion to the code-level comments; read it
alongside `src/` to understand _why_ things are structured the way they are.

---

## 1. Thread model

**Single main thread throughout.** All VR events, D3D11 context calls, ImGui rendering, and
overlay updates are sequential on one thread. This decision was made in Phase 2 and confirmed
in later phases for the following reasons:

- `ID3D11DeviceContext` is not thread-safe without a deferred context (DX11 design
  constraint). Adding a second thread would require either a deferred context (complexity) or
  a separate device (extra VRAM and cross-device texture copies).
- SteamVR overlay calls are not documented as thread-safe; the OpenVR SDK is presumed
  single-threaded unless the docs explicitly state otherwise.
- At ~90 Hz with ≤10 boxes the CPU budget is well within the <2% target even on a single
  thread (Phase 5 finding).

The main loop runs at ~90 Hz (`sleep_for(11ms)`). ImGui only renders when the SteamVR
dashboard is open (`m_dashboardActive`), so the render cost is zero in the steady state.

---

## 2. Module responsibilities

```
main.cpp
│
├── D3D11Backend           — Create D3D11 device on SteamVR's DXGI adapter
│                            Exposes device + context to other modules
│
├── OverlayManager         — Owns all world-space overlay handles and ChromaRenderers
│   ├── PassthroughBox     — Pure-data struct: position, rotation, scale, chroma color,
│   │                        opacity fade params, name/id, scaleDepth (Phase 4.5)
│   ├── ChromaRenderer     — Per-face D3D11 texture + VR overlay texture submission
│   └── (static helpers)   — computeFaceWorldMatrices, facePhysWidth/Height, computeTexDims
│
├── DeviceTracker          — Per-frame HMD + controller pose query (legacy GetControllerState)
│                            Hot-plug: handles TrackedDeviceActivated/Deactivated events
│
├── GrabController         — Physical grab/resize state machine (Phase 3.5+)
│                            Reads DeviceTracker; writes PassthroughBox position/rotation/scale
│
├── DashboardUI            — SteamVR dashboard panel (Dear ImGui, DX11 backend)
│   ├── LayoutStore        — JSON layout serialization/deserialization (Phase 4)
│   └── ImGui              — Third-party Dear ImGui, compiled from third_party/imgui/
│
└── VR event pump          — PollNextEvent → DeviceTracker/OverlayManager/DashboardUI
```

---

## 3. Data flow (one frame)

```
VRSystem()->PollNextEvent()
    → tracker.handleEvent()         hot-plug state reset
    → overlayManager.handleEvent()  dashboard open/close flag
    → dashboardUI.handleSystemEvent() dashboard activated/deactivated

tracker.update()                    refresh HMD + controller poses from SteamVR

grabController.tick(tracker, overlayMgr)
    → reads controller poses/grip state
    → writes box.posX/Y/Z, rotYaw/Pitch/Roll, scaleWidth/Height/Depth

overlayManager.frame(hmdPos)
    → for each box:
        → computeFaceWorldMatrices()  produce 6 face transforms
        → depth face lifecycle        create/destroy overlays 1-5 on scaleDepth crossing MIN_DEPTH
        → SetOverlayTransformAbsolute  update VR overlay transform
        → SetOverlayWidthInMeters      update physical size
        → SetOverlayAlpha              distance-based opacity fade
        → ChromaRenderer::frame()      re-render chroma texture if size changed (lazy)

dashboardUI.pollInput()             laser pointer events → ImGui mouse IO
dashboardUI.render()                ImGui frame → D3D11 RT → SetOverlayTexture (dashboard only)

sleep_for(11ms)                     ~90 Hz cap
```

---

## 4. OpenVR overlay coordinate conventions

SteamVR matrices are **row-major 3×4** (`HmdMatrix34_t`). GLM matrices are **column-major
4×4**. The conversion is done in `MathHelpers::steamVRToGlm()` and
`MathHelpers::glmToSteamVR()`.

```
                            SteamVR           GLM
Layout:                     row-major         column-major
Size:                       3 × 4             4 × 4
Translation:                last row          last column ([3])
Access to column i:         mat.m[i][0..2]    mat[i]
```

**Visible side**: the local `+Z` axis of an overlay's transform points toward the viewer.
This is the direction the overlay is "looking at you" from. It is **not** the −Z convention
used by OpenGL cameras.

**YXZ Euler convention**: rotations are stored as `(rotYaw, rotPitch, rotRoll)` in degrees
and decoded using `glm::extractEulerAngleYXZ`. The compound quaternion is:
`q = q_yaw * q_pitch * q_roll` (Y applied first, Z applied last).

**Standing universe**: all world positions and poses are in
`vr::TrackingUniverseStanding`. The standing origin is the room-scale floor at the
centre of the play space.

---

## 5. 3D cuboid face layout (Phase 4.5)

When `PassthroughBox::scaleDepth >= OverlayManager::MIN_DEPTH (0.01 m)`, six separate OpenVR
overlay handles are created — one per face. Each face has its own `ChromaRenderer` (D3D11
texture). The face index mapping is:

| Index | Face   | Local offset from box centre | Extra rotation |
|------:|--------|------------------------------|----------------|
| 0 | Front  | `+depth/2` along local Z | none |
| 1 | Back   | `−depth/2` along local Z | 180° Y |
| 2 | Left   | `−width/2` along local X | −90° Y |
| 3 | Right  | `+width/2` along local X | +90° Y |
| 4 | Top    | `+height/2` along local Y | −90° X |
| 5 | Bottom | `−height/2` along local Y | +90° X |

Depth faces (1–5) are created or destroyed in `OverlayManager::frame()` only when `scaleDepth`
crosses `MIN_DEPTH` — never on every frame. Flat boxes (depth = 0) use only face 0, identical
to Phase 3.5 behaviour.

---

## 6. Layout persistence (Phase 4)

Layouts are stored as JSON in `%APPDATA%\OpenMixerXR\layouts\`. Each layout is one file:
`<name>.json`. The auto-save session file is `last_session.json`.

Writes use the **atomic rename** pattern: write to `<name>.json.tmp`, then
`std::filesystem::rename()` atomically replaces the destination. This prevents corrupt files
on power loss or crash.

The JSON schema version is `LayoutStore::CURRENT_VERSION` (currently 2). The deserializer
accepts all past versions with default values for new fields:

- v1 → v2: `scaleDepth` defaults to `0.0f` (flat box).

---

## 7. Adding a new persistent field to PassthroughBox

1. Add the field to `src/PassthroughBox.h` with a sensible default.
2. In `src/LayoutStore.cpp`:
   - Serialize in `serializeLayout()`.
   - Deserialize in `deserializeLayout()` with a fallback for older versions.
3. Bump `LayoutStore::CURRENT_VERSION`.
4. Add a round-trip check in `tests/layout_serialization_test.cpp`.
5. If the field affects geometry: update `OverlayManager::computeFaceWorldMatrices()` and
   `facePhysWidth/Height`.
