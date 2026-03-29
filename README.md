# OpenMixer XR

A SteamVR overlay application that places solid-colour chroma key rectangles in your VR
world space. When combined with Virtual Desktop's (or similar software's) chroma key
passthrough feature, these rectangles become transparent cutout windows -- letting you see
your real desk, keyboard, hands, or any real-world object through precisely positioned
and sized holes in VR.

---

## What it does

Most PCVR mixed-reality setups can make a specific colour fully transparent. OpenMixer XR
fills that gap by giving you full control over *where* those coloured panels live in 3D space:

- **Add as many boxes as you want** -- each is an independent VR overlay with its own
  position, rotation, size, and chroma colour.
- **Duplicate a box** -- clone any box with one button to quickly create mirrored or
  matched setups.
- **Physical grab & place** -- enable Move Mode on a box from the dashboard, then squeeze
  your right controller grip to grab it. The box follows your hand in real time, rotating
  with your wrist, and drops where you release. Re-grip as many times as you like without
  returning to the dashboard.
- **Physical resize** -- while gripping with your right hand, also squeeze your left grip.
  Move your left hand left/right to widen or narrow the box; up/down to make it taller or
  shorter; forward/back to add depth and turn it into a **3D cuboid**. Releasing the left
  grip ends the resize; the right-hand grab continues.
- **Rectangular boxes and 3D cuboids** -- width, height, and depth are set independently;
  texture aspect ratios are computed automatically so overlays are never distorted.
- **Distance-based opacity fade** -- boxes fade out as you approach them so they don't
  obstruct close-up work.
- **Layout persistence** -- save, load, rename, and delete named layouts. Your last session
  is auto-restored on next launch.
- **SteamVR dashboard panel** -- a full ImGui UI accessible from the SteamVR overlay bar
  while in any game or app.

### Typical use case

You are using Virtual Desktop in VR with chroma key passthrough enabled (colour `#00FF80`).
You launch OpenMixer XR alongside it. Three chroma boxes appear in front of you. You open
the SteamVR dashboard, select the "Front" box, enable Move Mode, close the dashboard, and
grip the controller to drag the box over your keyboard area. You widen it with your left
hand until it covers the full width of your physical desk. Now that area is transparent in
VR -- you can see and use your real keyboard at all times.

---

## Requirements

| Requirement | Notes |
|-------------|-------|
| **Windows 10/11 64-bit** | D3D11 and DXGI required |
| **SteamVR** | Tested with SteamVR 2.x; the app registers as a SteamVR overlay |
| **A PCVR headset** | Developed and tested on **Meta Quest 3** via Air Link / Link cable |
| **Visual C++ 2022 Redistributable** | Required at runtime; bundled in the installer or download from [Microsoft](https://aka.ms/vs/17/release/vc_redist.x64.exe) |
| **Visual Studio 2022** (or 2019) | MSVC C++17 toolset -- only needed if building from source |
| **CMake 3.20+** | Build system -- only needed if building from source |
| **Git** with submodule support | Third-party dependencies use git submodules -- only needed if building from source |
| **Virtual Desktop** (or equivalent) | Any software that keys out a solid colour in VR |

---

## Installing (pre-built)

Download `OpenMixerXR-Installer.exe` from the [Releases](../../releases) page and run it.
The installer places files in `C:\Program Files\OpenMixer XR\` and registers an
uninstaller in Add/Remove Programs.

If Windows Defender prompts about an unknown publisher, click **More info -> Run anyway**.

---

## Building from source

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
2. Run `build/Release/OpenMixerXR.exe` -- no arguments needed.
3. SteamVR will recognise it as an overlay app under the name *OpenMixer XR*.
4. *(Optional)* In SteamVR Settings -> Startup/Shutdown, add OpenMixer XR to auto-start
   so it launches automatically with SteamVR.

Three default chroma boxes (Front, Left, Right) appear in standing-universe space when
the app starts. Open the SteamVR dashboard and select the OpenMixer XR tab to manage them.

For a detailed Quest + Virtual Desktop setup walkthrough, see
[`docs/quest-setup.md`](docs/quest-setup.md).

---

## Dashboard UI

Access the dashboard by pressing the SteamVR system/menu button to open the overlay bar,
then select the OpenMixer XR icon.

### Left panel -- box list

| Control | Action |
|---------|--------|
| Checkbox | Toggle a box's visibility |
| Row click | Select a box for editing in the right panel |
| `+ Add` | Add a new box snapped 1 m in front of your headset |
| `- Delete` | Remove the selected box |
| `Dup` | Duplicate the selected box (clone with a new ID, offset 10 cm on X) |
| Circle indicator | Box is in Move Mode (yellow = waiting for grip) |
| Dot indicator | Box is actively being grabbed (green) or grabbed + resized (cyan) |

### Right panel -- box properties

| Section | Controls |
|---------|----------|
| **Name** | Rename the box (free text) |
| **Position** | X / Y / Z in metres, standing universe; `[-]` `[+]` buttons for 5 cm steps |
| **Rotation** | Yaw / Pitch / Roll in degrees; 5 degree steps |
| **Size** | Width / Height / Depth in metres; 5 cm steps. Depth = 0 is flat (single quad). |
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
| *Layout name* + *Save / Load / Delete* | Manage named layouts persisted to disk |
| *KB* button | Open the SteamVR virtual keyboard to type a layout name in VR |

---

## Controller controls (Move Mode)

Move Mode is enabled per-box from the dashboard right panel.

| Gesture | Effect |
|---------|--------|
| **Right grip press** (while Move Mode on) | Grab: box latches to right controller, follows position and rotates with wrist |
| **Right grip release** | Drop: box stays at current transform. Move Mode stays on -- grip again to re-grab |
| **Left grip press** (while right grip held) | Resize latch: records current size and left-hand position as reference |
| **Move left hand left / right** (while left grip held) | Width: moving left = wider, moving right (pinch) = narrower |
| **Move left hand up / down** (while left grip held) | Height: up = taller, down = shorter |
| **Move left hand forward / back** (while left grip held) | Depth: forward = deeper (3D cuboid), back = shallower |
| **Left grip release** | End resize. Right-hand grab continues |
| **Disable Move Mode** (dashboard button) | Stop Move Mode for this box at any time |

---

## Chroma colour

The default colour is `#00FF80` (R=0, G=255, B=128), which matches Virtual Desktop's
default chroma key colour. If your passthrough software uses a different key colour,
change it per-box or globally from the dashboard.

See [`docs/quest-setup.md`](docs/quest-setup.md) for exact hex-to-float conversion
values and troubleshooting tips.

---

## Unit tests

The following tests can be built and run without SteamVR or a connected headset:

```sh
cmake --build build --config Release --target box_transform_test layout_serialization_test cuboid_transform_test
build\Release\box_transform_test.exe
build\Release\layout_serialization_test.exe
build\Release\cuboid_transform_test.exe
```

---

## Third-party dependencies

| Library | Version | License | Use |
|---------|---------|---------|-----|
| [OpenVR](https://github.com/ValveSoftware/openvr) | 2.5.1 | BSD-3-Clause | SteamVR overlay API |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.91.x | MIT | Dashboard UI |
| [GLM](https://github.com/g-truc/glm) | 0.9.9.8 | MIT | 3D math (matrices, quaternions) |
| [spdlog](https://github.com/gabime/spdlog) | 1.x | MIT | Logging |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.x | MIT | Layout persistence |

OpenVR, ImGui, spdlog, and nlohmann/json are included as git submodules under
`third_party/`. GLM is fetched automatically via CMake `FetchContent`.

---

## Project status

| Phase | Status | Deliverable |
|-------|--------|-------------|
| 1 -- Foundation | Complete | Single chroma overlay, SteamVR registration, D3D11 |
| 2 -- Core box system | Complete | N boxes, ChromaRenderer, opacity fade, reconnect, math tests |
| 3 -- Dashboard UI | Complete | ImGui panel -- snap, visibility toggle, per-box editing, recalibrate |
| 3.5 -- Grab & resize | Complete | Physical controller grab (pos + rot), two-hand resize, rectangles |
| 4 -- Layout persistence | Complete | JSON save/load, auto last_session, layout management, virtual keyboard |
| 4.5 -- 3D cuboid boxes | Complete | 6-face cuboids with depth; two-hand depth resize; JSON schema v2 |
| 5 -- Polish & release | Complete | Duplicate box, hot-plug robustness, CI, installer, full docs |

---

## Documentation

- [`docs/quest-setup.md`](docs/quest-setup.md) -- step-by-step Virtual Desktop + Quest 3 setup
- [`docs/architecture.md`](docs/architecture.md) -- module graph, thread model, matrix conventions
- [`development-plans/`](development-plans/) -- per-phase plans and implementation findings

---

## License

MIT -- see [LICENSE](LICENSE).
