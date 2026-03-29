# Quest Setup Guide — OpenMixer XR

This guide walks you through setting up OpenMixer XR on a Meta Quest 3 using Virtual Desktop.
The same chroma key approach works with any PCVR streaming software that supports chroma-key
passthrough (e.g., ALVR with passthrough enabled).

---

## Requirements

| Item | Notes |
|------|-------|
| **Meta Quest 3** | Other headsets work if they support passthrough + chroma keying in the streaming client |
| **SteamVR** (2.x) | Tested with SteamVR 2.x; must be installed and running |
| **Virtual Desktop** | Version 1.30+ recommended; [virtualdesktop.app](https://www.virtualdesktop.app/) |
| **Windows 10/11 64-bit PC** | DirectX 11 required |
| **OpenMixer XR** | Build from source or use the installer (`OpenMixerXR-Installer.exe`) |

---

## Step-by-step setup

### 1. Install and build OpenMixer XR

**Option A — Installer (recommended)**

Run `OpenMixerXR-Installer.exe`. Files are placed in `C:\Program Files\OpenMixer XR\`.
If prompted about the VC++ 2022 Redistributable, allow the installation.

**Option B — Build from source**

```powershell
git clone --recurse-submodules https://github.com/your-org/OpenMixerXR.git
cd OpenMixerXR
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built executable is at `build\Release\OpenMixerXR.exe`.

---

### 2. Register OpenMixer XR with SteamVR

Launch SteamVR, then run `OpenMixerXR.exe`. SteamVR will detect it as an overlay application.
To make it launch automatically with SteamVR:

1. In SteamVR, open **Settings → Startup/Shutdown → Manage Add-ons**.
2. Find **OpenMixer XR** and enable **Launch with SteamVR**.

---

### 3. Enable chroma key passthrough in Virtual Desktop

1. Put on your Quest 3 and connect to your PC with Virtual Desktop.
2. In the Virtual Desktop **Streamer** (PC app), go to **Settings → Video**.
3. Enable **Passthrough** (Mixed Reality mode).
4. Set the chroma key colour to `#00FF80` (hex). This matches OpenMixer XR's default.

> **Precision note**: Virtual Desktop uses 8-bit hex colour codes. The exact value
> `#00FF80` maps to `(0.0, 1.0, 0.502)` in float. OpenMixer XR's default per-box chroma
> color is pre-set to this value. If you change the colour in either application, copy
> the float value from OpenMixer XR's dashboard (shown in the "Chroma" colour picker)
> and convert to hex for Virtual Desktop.

---

### 4. Position your first box

1. With SteamVR and OpenMixer XR running, put on the headset.
2. Open the **SteamVR dashboard** (press the SteamVR button on your controller).
3. Find the **OpenMixer XR** tab (it appears as a dashboard overlay).
4. Select the **Front** box in the left panel.
5. Click **Enable Move Mode**.
6. Close the dashboard.
7. Squeeze the **right controller grip** — the Front box will attach to your hand.
8. Move it to cover your keyboard, desk, or monitor area.
9. Release the grip. The box stays in place.
10. Re-grip as many times as you like without returning to the dashboard.

---

### 5. Resize a box

1. Enable Move Mode for the box (see above).
2. Close the dashboard.
3. **Grip the right controller** (the box attaches).
4. While holding the right grip, **also grip the left controller**.
5. Move your left hand:
   - **Left / Right** — decrease / increase width (pinch gesture = narrower).
   - **Up / Down** — increase / decrease height.
   - **Forward / Back** — add / remove depth (turns the box into a 3D cuboid).
6. Release the left grip to lock the new size. The right-hand grab continues.
7. Release the right grip when done repositioning.

---

### 6. Save your layout

1. Open the SteamVR dashboard → OpenMixer XR tab.
2. In the bottom bar, type a name in the **Layout name** field (or press **KB** to open
   the SteamVR keyboard and type from within VR).
3. Press **Save**.
4. The layout is stored in `%APPDATA%\OpenMixerXR\layouts\<name>.json`.

Your last session is auto-saved on quit and auto-restored on next launch, so the boxes
are always where you left them.

---

## Troubleshooting

### Boxes are not visible

1. Make sure SteamVR is running **before** launching OpenMixerXR.exe.
2. Open the **SteamVR Overlay Viewer** (SteamVR Menu → Developer → Overlay Viewer).
   Find "openmixer.xr" entries. If their preview shows a solid green rectangle, the box
   is rendering correctly but may be positioned behind you or at the wrong depth.
3. Check that the box is marked **visible** (eye checkbox in the dashboard box list).
4. Check that "Hide boxes when dashboard open" is **unchecked** if you're trying to see
   the boxes while positioning them from the dashboard.

### The passthrough area is green, not transparent

- Confirm that Virtual Desktop's chroma key colour exactly matches the per-box Chroma
  color in OpenMixer XR. Copy the hex value from OpenMixer XR (e.g. `#00FF80`) and paste
  it into Virtual Desktop's colour picker.
- Some Virtual Desktop versions require a tolerance / threshold value >0. Try `10–20`.

### The chroma colour has slight variation / banding

Virtual Desktop matches colours within a tolerance. If the rendered chroma colour looks
slightly off (e.g. `#00FF7F` instead of `#00FF80`) it is a rounding artefact from the
`DXGI_FORMAT_R8G8B8A8_UNORM` format. Increase the Virtual Desktop chroma tolerance to
compensate, or adjust the chroma color in OpenMixer XR to match what Virtual Desktop
actually sees (use a screen colour picker on the rendered box in the Overlay Viewer).

### Controllers not tracked / Move Mode unavailable

- Move Mode requires the right controller to be tracked. The button is automatically
  disabled when tracking is lost.
- If a controller disconnects mid-grab, the box freezes in place and Move Mode exits
  automatically when the controller reconnects and the grip is no longer held.

### Boxes disappear when starting a game

Games that use exclusive fullscreen may hide all overlays. Switch the game to
**Borderless Windowed** mode, or enable the "Show overlays" option in SteamVR settings.

### Layout files are missing after reinstall

Layout files are stored in `%APPDATA%\OpenMixerXR\layouts\`. Uninstalling OpenMixer XR
does **not** delete this folder. You can back it up or move it to a new machine manually.

---

## Virtual Desktop chroma key reference values

| OpenMixer XR slider | Float value | Hex |
|--------------------|-------------|-----|
| R | 0.000 | 00 |
| G | 1.000 | FF |
| B | 0.502 | 80 |
| **Combined** | | **#00FF80** |

Use this hex code in Virtual Desktop's colour picker for a one-click match.
