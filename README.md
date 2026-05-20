# BerryBeatz

A native macOS tracker / module player built from the ground up with a custom ImGui interface. BerryBeatz uses [libopenmpt](https://lib.openmpt.org/libopenmpt/) to read and play back virtually every tracker format ever made, while providing a modern, low-latency, GPU-accelerated UI that feels at home on macOS.

---

## Features

### Playback
- Plays **all major tracker formats** via libopenmpt: MOD, XM, IT, S3M, MO3, OKT, MTM, 669, and many more
- Real-time **stereo VU meters** — master output and per-channel
- **Seek bar** with click-to-seek anywhere in the song
- Transport controls: Play, Pause, Stop, Play from Start
- Adjustable **Tempo**, **Speed**, and **Rows Per Beat** during playback
- **Global volume** knob
- **Octave** selector for live note entry
- Per-order seek by clicking any entry in the order list

### Pattern Editor
- Full **pattern view** with frozen channel header row and order list
- Columns per cell: Note · Instrument · Volume · Effect Command · Effect Parameter
- Keyboard note entry in edit mode (`Space` to toggle)
- **Per-channel mute** toggle directly from the channel strip
- Per-channel stereo VU bars
- Channel plugin settings popup
- Pattern playback controls: Play Cursor, Play from Start, Play Row, Stop, Record
- Pattern editing: Insert, Undo, Expand, Shrink, Detail view, Overflow paste toggle
- Follow-song mode — pattern view tracks the current playback position in real time
- Loop-pattern mode

### Module Browser (Sidebar)
- Collapsible tree showing all **Instruments**, **Samples**, and **Patterns** in the loaded module
- Click any item to jump to the corresponding tab
- Sticky song title header that stays visible while scrolling
- Resizable via drag handle

### Tabs
| Tab | Contents |
|---|---|
| General | Title, artist, format, tracker, channel / pattern / sample / instrument counts |
| Patterns | Full pattern editor |
| Samples | Numbered sample list |
| Instruments | Numbered instrument list |
| Comments | Read-only module message / comment text |

### UI & Window Management
- Custom **borderless window** — no native title bar chrome
- Integrated window controls (close, minimise, maximise, fullscreen) styled to match the UI
- Smooth **window drag** via `performWindowDragWithEvent:` — uses the macOS compositor for tear-free repositioning
- Manual **edge resize** on right, bottom, and bottom-right corner
- Full **fullscreen** support with per-monitor detection
- **Drag & drop** — drop a module file onto the window to open it
- Command-line argument — `BerryBeatz.app/Contents/MacOS/BerryBeatz <path>` opens a file directly
- Native **Open / Save file dialogs** via `NSOpenPanel` / `NSSavePanel`
- Animated logo with additive-blended glow that pulses slowly
- Retina / HiDPI aware (native framebuffer scaling)

---

## Requirements

| Dependency | Notes |
|---|---|
| macOS 12.0+ | Apple Silicon or Intel |
| Xcode Command Line Tools | `xcode-select --install` |
| CMake 3.22+ | `brew install cmake` |
| GLFW | `brew install glfw` |
| PortAudio | `brew install portaudio` |
| libvorbis + libogg | `brew install libvorbis` |
| mpg123 | `brew install mpg123` |
| libopenmpt | Built from source (see below) |

---

## Building

### 1. Build libopenmpt

BerryBeatz links against a static `libopenmpt.a` built from the [OpenMPT source tree](https://github.com/OpenMPT/openmpt), expected as a sibling directory:

```
parent/
  openmpt/       ← OpenMPT source
  openmpt-mac/   ← this repo
```

```bash
cd ../openmpt
make CONFIG=clang DYNLINK=0 NO_OPENMPT123=1 NO_EXAMPLES=1 NO_TESTS=1
```

### 2. Build BerryBeatz

```bash
git clone https://github.com/yourname/berrybeatz openmpt-mac
cd openmpt-mac
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
open BerryBeatz.app
```

---

## Project Structure

```
src/
  main.cpp          — GLFW + ImGui init, render loop, input callbacks
  App.cpp / .h      — Top-level layout: sidebar, tab bar, content area
  Toolbar.cpp / .h  — Transport bar, knobs, VU meters, window controls
  TrackerView.cpp   — Pattern editor, order list, channel strip
  PatternEditor.cpp — Low-level cell read/write wrappers over libopenmpt
  AudioEngine.cpp   — PortAudio stream, libopenmpt decode, seek/playback logic
  ModuleState.h     — Lock-free shared state between audio and UI threads
  CocoaHelpers.mm   — macOS-specific: file dialogs, window drag, dock icon
  Logo.mm           — PNG texture loader via stb_image + OpenGL
  Knob.h            — Immediate-mode rotary knob widget

assets/
  AppIcon.icns      — App icon
  logo.png          — In-app logo (top-right, with animated glow)
  Info.plist        — macOS bundle metadata

external/
  imgui/            — Dear ImGui (GLFW + OpenGL3 backends)
```

---

## Architecture Notes

**Threading model:** The audio engine runs on a dedicated PortAudio callback thread. All shared state (`ModuleState`) uses `std::atomic` for lock-free reads from the UI thread, with two thin mutexes for VU data and module metadata. The UI never blocks the audio thread.

**Rendering:** Pure ImGui immediate-mode rendering over an OpenGL 3.2 Core Profile context. The toolbar glow uses `AddCallback` to switch to additive GL blending mid-draw-list, then restores normal blending. No retained-mode scene graph.

**Window chrome:** The window is created with `GLFW_DECORATED = false`. Close/minimise/maximise/fullscreen buttons are drawn in ImGui. Resizing is handled manually via cursor-pos hit-testing in `handleEdgeResize`. Window dragging calls `performWindowDragWithEvent:` via a local `NSEvent` monitor — this delegates to the macOS compositor and gives smooth, blink-free movement.

---

## Keyboard Shortcuts (Pattern Editor)

| Key | Action |
|---|---|
| `Space` | Toggle edit mode |
| `A–Z`, `0–9` (QWERTY) | Enter note (mapped to tracker layout) |
| `Delete` | Clear cell |
| `↑ ↓ ← →` | Move cursor |
| `Tab` | Next channel |
| `F5` | Play from cursor |
| `F6` | Play from start |
| `F8` | Stop |
| `Escape` | Stop / exit edit mode |

---

## License

BerryBeatz is open source. See [LICENSE](LICENSE) for details.

libopenmpt is © OpenMPT contributors, licensed under the BSD license.  
Dear ImGui is © Omar Cornut, licensed under the MIT license.  
PortAudio is © Ross Bencina & Phil Burk, licensed under the MIT license.
