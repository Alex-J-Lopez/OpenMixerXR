# OpenMixer XR

A SteamVR overlay application that places solid-colour chroma key rectangles in your VR
world space. When combined with Virtual Desktop's (or similar software's) chroma key
passthrough feature, these rectangles become transparent cutout windows — letting you see
your real desk, keyboard, hands, or any real-world object through precisely positioned
and sized holes in VR.

---

## What it does

Most PCVR mixed-reality setups can make a specific colour fully transparent. OpenMixer XR
fills that gap by giving you full control over *where* those coloured panels live in 3D space:

- **Add as many boxes as you want** — each is an independent VR overlay with its own
  position, rotation, size, and chroma colour.
- **Physical grab & place** — enable Move Mode on a box from the dashboard, then squeeze
  your right controller grip to grab it. The box follows your hand in real time, rotating
  with your wrist, and drops where you release. Re-grip as many times as you like without
  returning to the dashboard.
- **Physical resize** — while gripping with your right hand, also squeeze your left grip.
  Move your left hand left/right to widen or narrow the box; up/down to make it taller or
  shorter. Releasing the left grip ends the resize; the right-hand grab continues.
- **Rectangular boxes** — width and height are set independently; the texture aspect ratio
  is derived automatically so the overlay is never distorted.
- **Distance-based opacity fade** — boxes fade out as you approach them so they don't
  obstruct close-up work.
- **SteamVR dashboard panel** — a full ImGui UI accessible from the SteamVR overlay bar
  while in any game or app.

### Typical use case

You are using Virtual Desktop in VR with chroma key passthrough enabled (colour `#00FF80`).
You launch OpenMixer XR alongside it. Three chroma boxes appear in front of you. You open
the SteamVR dashboard, select the "Front" box, enable Move Mode, close the dashboard, and
grip the controller to drag the box over your keyboard area. You widen it with your left
hand until it covers the full width of your physical desk. Now that area is transparent in
VR — you can see and use your real keyboard at all times.

---

## Requirements

| Requirement | Notes |
|-------------|-------|
| **Windows 10/11 64-bit** | D3D11 and DXGI required |
| **SteamVR** | Tested with SteamVR 2.x; the app registers as a SteamVR overlay |
| **A PCVR headset** | Developed and tested on **Meta Quest 3** via Air Link / Link cable |
| **Visual Studio 2022** (or 2019) | MSVC C++17 toolset |
| **CMake 3.20+** | Build system |
| **Git** with submodule support | Third-party dependencies use git submodules |
| **Virtual Desktop** (or equivalent) | Any software that keys out a solid colour in VR |

---

## Building

### 1. Clone with submodules

```sh
git clone --recurse-submodules https://github.com/your-username/OpenMixerXR.git
cd OpenMixerXR
```

If you already cloned without `--recurse-submodules`:

```sh
git submodule update --init --recursive
```

### 2. Configure with CMake

```sh
cmake -B build -G "Visual Studio 17 2022" -A x64
```

GLM is fetched automatically by CMake on first configure (requires internet access).

### 3. Build

**Visual Studio:**  
Open `build/OpenMixerXR.sln`, set the solution configuration to **Release | x64**, and
press **Build Solution** (`Ctrl+Shift+B`).

**Command line:**

```sh
cmake --build build --config Release
```

The executable and required resources are placed in `build/Release/`.

---

## Running

1. Launch **SteamVR** first.
2. Run `build/Release/OpenMixerXR.exe` — no arguments needed.
3. SteamVR will recognise it as an overlay app under the name *OpenMixer XR*.
4. *(Optional)* In SteamVR Settings → Startup/Shutdown, add OpenMixer XR to auto-start
   so it launches automatically with SteamVR.

Three default chroma boxes (Front, Left, Right) appear in standing-universe space when
the app starts. Open the SteamVR dashboard and select the OpenMixer XR tab to manage them.

---

## Dashboard UI

Access the dashboard by pressing the SteamVR system/menu button to open the overlay bar,
then select the OpenMixer XR icon.

### Left panel — box list

| Control | Action |
|---------|--------|
| Checkbox | Toggle a box's visibility |
| Row click | Select a box for editing in the right panel |
| `+ Add` | Add a new box snapped 1 m in front of your headset |
| `- Delete` | Remove the selected box |
| `○` indicator | Box is in Move Mode (yellow = waiting for grip) |
| `●` indicator | Box is actively being grabbed (green) or grabbed + resized (cyan) |

### Right panel — box properties

| Section | Controls |
|---------|----------|
| **Name** | Rename the box (free text) |
| **Position** | X / Y / Z in metres, standing universe; `[-]` `[+]` buttons for 5 cm steps |
| **Rotation** | Yaw / Pitch / Roll in degrees; 5° steps |
| **Size** | Width / Height in metres; 5 cm steps |
| **Chroma** | Per-box colour picker |
| **Fade** | Fade near / far distance; min / max opacity |
| **Snap to HMD** | Instantly move the box 1 m in front of your gaze |
| **Enable Move Mode** | Arm physical grab for this box (see controller controls below) |

### Bottom bar

| Control | Action |
|---------|--------|
| *Hide boxes when dashboard open* | When unchecked (default), boxes stay visible while you position them with the dashboard open |
| *Global chroma* + *Apply to all* | Set one colour across every box simultaneously |
| *Recalibrate* | Shift all boxes to compensate for headset drift or guardian re-centring |
| *Save / Load / Delete Layout* | *(Phase 4 — coming soon)* |

---

## Controller controls (Move Mode)

Move Mode is enabled per-box from the dashboard right panel.

| Gesture | Effect |
|---------|--------|
| **Right grip press** (while Move Mode on) | Grab: box latches to right controller, follows position and rotates with wrist |
| **Right grip release** | Drop: box stays at current transform. Move Mode stays on — grip again to re-grab |
| **Left grip press** (while right grip held) | Resize latch: records current size and left-hand position as reference |
| **Move left hand left / right** (while left grip held) | Width: moving left = wider, moving right (pinch) = narrower |
| **Move left hand up / down** (while left grip held) | Height: up = taller, down = shorter |
| **Left grip release** | End resize. Right-hand grab continues |
| **Disable Move Mode** (dashboard button) | Stop Move Mode for this box at any time |

---

## Chroma colour

The default colour is `#00FF80` (R=0, G=255, B=128), which matches Virtual Desktop's
default chroma key colour. If your passthrough software uses a different key colour,
change it per-box or globally from the dashboard.

---

## Third-party dependencies

| Library | Version | License | Use |
|---------|---------|---------|-----|
| [OpenVR](https://github.com/ValveSoftware/openvr) | 2.5.1 | BSD-3-Clause | SteamVR overlay API |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.x | MIT | Dashboard UI |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | MIT | 3D math (matrices, quaternions) |
| [spdlog](https://github.com/gabime/spdlog) | 1.x | MIT | Logging |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.x | MIT | Layout persistence *(Phase 4)* |

OpenVR, ImGui, spdlog, and nlohmann/json are included as git submodules under
`third_party/`. GLM is fetched automatically via CMake `FetchContent`.

---

## Project status

| Phase | Status | Deliverable |
|-------|--------|-------------|
| 1 — Foundation | ✅ Complete | Single chroma overlay, SteamVR registration, D3D11, actions stub |
| 2 — Core box system | ✅ Complete | N boxes, ChromaRenderer, opacity fade, reconnect, math tests |
| 3 — Dashboard UI | ✅ Complete | ImGui panel — snap, visibility toggle, per-box editing, recalibrate |
| 3.5 — Grab & resize | ✅ Complete | Physical controller grab (pos + rot), two-hand resize, rectangles |
| 4 — Layout persistence | 🔲 Planned | JSON save/load, auto last_session, layout management |
| 5 — Polish & release | 🔲 Planned | Duplicate box, hot-plug, CI, installer, full docs |

---

## Development

See [`development-plans/`](development-plans/) for the full per-phase plan documents and
[implementation findings](development-plans/README.md) captured during each phase.

The unit test for coordinate math can be built and run without SteamVR:

```sh
cmake --build build --config Release --target box_transform_test
./build/Release/box_transform_test.exe
```

---

## License

MIT — see [LICENSE](LICENSE).
