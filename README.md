# BerryBeatz

<p align="center"><img src="docs/github-banner.png" width="400"/></p>

**BerryBeatz** is a native macOS tracker app — a modern, GPU-accelerated frontend built from scratch with Dear ImGui. Load any classic module file and you get a full pattern editor, instrument and sample editors, live MIDI preview, and a custom borderless UI that feels native on Apple Silicon.

> Early development — feedback and contributions very welcome.

---

## Why BerryBeatz?

There is no good native tracker on macOS. OpenMPT itself only runs on Windows. BerryBeatz brings a purpose-built tracker UI to macOS with its own embedded audio engine rather than a Wine wrapper or a watered-down port.

- **Plays everything** — MOD, XM, IT, S3M, MO3, OKT, MTM, 669, and every obscure variant in between
- **Edits patterns** — not just a player; you can write and modify tracker patterns live
- **Native feel** — borderless window, Retina-aware, drag & drop, native file dialogs, CoreMIDI
- **Low latency** — PortAudio callback thread, lock-free shared state, no GC pauses

---

## Features

### Playback & Transport
- Plays all major tracker formats
- Real-time stereo VU meters — master and per-channel
- Seek bar, Play / Pause / Stop / Play from Start
- Adjustable Tempo, Speed, Rows Per Beat, and Global Volume during playback
- Octave selector for live note entry

### Pattern Editor
- Full pattern grid — Note · Instrument · Volume · Effect · Parameter
- Keyboard note entry with edit mode toggle (`Space`)
- Per-channel isolated VU meters — each channel rendered independently, not a mix-down
- Per-channel waveform visualisation — live oscilloscope for every channel, including OPL/FM voice capture
- Per-channel mute and solo
- Play from Cursor, Play from Start, Play Row, Record, Loop pattern
- Follow-song mode, Undo/Redo, Insert/Delete rows, Expand/Shrink pattern
- Order list with click-to-seek

### Sidebar
- Collapsible tree: Song → Instruments / Samples / Patterns
- Click any item to jump straight to it
- Full keyboard navigation (`↑↓` move, `←→` expand/collapse, `Enter` activate)
- Resizable via drag handle

### Instrument Editor
- NNA, DCT, DNA, global volume, panning, filter (cutoff/resonance), resampling
- Interactive envelope editor — Volume, Pan, Pitch — drag nodes, add/remove points
- Note-to-sample keyboard map
- Live preview via on-screen piano, computer keyboard, or MIDI

### Sample Editor
- Loop points, tune, volume, panning, vibrato
- Waveform display
- Live preview via on-screen piano, computer keyboard, or MIDI

### MIDI
- CoreMIDI note-on/off with velocity for live instrument and sample preview
- Pitch bend
- Configurable CC mappings for Tempo, Speed, RPB, Volume, and Octave — with live-learn

### UI
- Custom borderless window with integrated close/minimise/maximise/fullscreen controls
- Smooth window drag via macOS compositor (`performWindowDragWithEvent:`)
- Edge resize, fullscreen, drag & drop, command-line file open
- Native Open / Save dialogs via `NSOpenPanel` / `NSSavePanel`
- Animated logo, Retina / HiDPI aware

---

## Requirements

| Dependency | Install |
|---|---|
| macOS 12.0+ | Apple Silicon or Intel |
| Xcode Command Line Tools | `xcode-select --install` |
| CMake 3.22+ | `brew install cmake` |
| GLFW | `brew install glfw` |
| PortAudio | `brew install portaudio` |
| libvorbis + libogg | `brew install libvorbis` |
| mpg123 | `brew install mpg123` |

---

## Building

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/pedrocatalao/berrybeatz
cd berrybeatz
```

[Dear ImGui](https://github.com/ocornut/imgui) (`external/imgui`) is included as a submodule. The audio engine is embedded directly under `thirdparty/` — no separate build step needed.

### 2. Build BerryBeatz

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
open BerryBeatz.app
```

---

## Project Structure

```
src/
  main.cpp                   — GLFW + ImGui init, render loop
  core/
    AudioEngine              — PortAudio stream, decode, seek/playback
    ModuleState.h            — lock-free shared state between audio and UI threads
    SoundModule              — CSoundFile wrapper (load, render, pattern access)
    OpenMPTInternal          — direct soundlib access: instruments, samples, OPL capture
    PatternEditor            — pattern cell read/write
    Settings                 — persistent settings
    UndoManager              — undo/redo stack
  ui/
    App                      — top-level layout: sidebar, tab bar, content area
    Toolbar                  — transport bar, knobs, VU meters, window controls
    TrackerView + PatternInput — pattern editor and keyboard input
    SidebarView              — collapsible module tree
    GeneralView              — song info tab
    InstrumentView           — instrument editor with envelope graph
    SampleView               — sample editor with waveform
    SettingsView             — settings dialog
    Knob.h                   — immediate-mode rotary knob widget
  platform/
    CocoaHelpers             — file dialogs, window drag, dock icon
    Logo                     — PNG texture loader
    MidiDevices / MidiEngine — CoreMIDI device enumeration and event loop
    WindowChrome             — borderless window, edge resize, fullscreen

external/imgui/              — Dear ImGui submodule
thirdparty/openmpt/          — embedded soundlib (soundlib, sounddsp, common, src/)
```

---

## Architecture

**Audio thread:** PortAudio callback thread runs the audio engine. `ModuleState` uses `std::atomic` throughout for playback state. VU meters and waveform snapshots use a double-buffered `VuSnapshot[2]` with an atomic flip index — the callback writes to one buffer and exposes it to the UI with no mutex or allocation. Module metadata is guarded by a single mutex written once on load.

**Audio engine:** `SoundModule` wraps `CSoundFile` from the OpenMPT soundlib directly — no public libopenmpt API. Pattern data, instrument state, OPL/FM voices, and per-channel PCM capture are all accessed through the internal soundlib interfaces, giving full control over the render path.

**Rendering:** Pure ImGui immediate-mode over OpenGL 3.2 Core Profile. No retained scene graph. The toolbar glow effect switches to additive GL blending mid-draw-list via `AddCallback`.

**Window chrome:** `GLFW_DECORATED = false`. All controls (close/min/max/fullscreen) are drawn in ImGui. Resizing is cursor-pos hit-tested in the GLFW callback. Dragging delegates to `performWindowDragWithEvent:` for smooth, compositor-driven movement with no tearing.

---

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `Space` | Toggle edit mode |
| `A–Z`, `0–9` (QWERTY) | Enter note |
| `Delete` | Clear cell |
| `↑ ↓ ← →` | Move cursor |
| `Tab` | Next channel |
| `F5` | Play from cursor |
| `F6` | Play from start |
| `F8` | Stop |
| `Escape` | Stop / exit edit mode |

---

## Contributing

BerryBeatz is at an early stage and there is a lot of ground to cover. All contributions are welcome — bug reports, fixes, new features, documentation, or just feedback on what a macOS tracker should feel like.

1. Fork the repo and create a branch
2. Make your changes
3. Open a pull request with a short description

Please open an issue before starting large features so we can coordinate.

---

## License

BerryBeatz is open source. See [LICENSE](LICENSE) for details.

The OpenMPT soundlib (`thirdparty/openmpt/`) is © OpenMPT contributors — [BSD license](https://github.com/OpenMPT/openmpt/blob/master/LICENSE).  
Dear ImGui is © Omar Cornut — [MIT license](https://github.com/ocornut/imgui/blob/master/LICENSE.txt).  
PortAudio is © Ross Bencina & Phil Burk — MIT license.
