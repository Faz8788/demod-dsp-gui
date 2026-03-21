# DeMoDOOM

**DOOM-style framebuffer GUI for Faust DSP with multi-input abstraction, multi-resolution rendering, and professional FX chain management.**

Software-rendered at selectable resolutions (320×200 through 960×540) with nearest-neighbor upscale, CRT post-processing, PipeWire native audio, and a game-menu UX architecture designed to manage up to 12 chained effects with visual signal flow.

## Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│  main.cpp  →  Engine (core/engine)                                │
│                                                                   │
│  ┌─ InputManager ───────────────────────────────────┐             │
│  │  KeyboardDevice   (SDL keyboard + mouse)         │             │
│  │  GamepadDevice    (SDL GameController + haptic)   │             │
│  │  BCIDevice        (OpenBCI via LSL)               │             │
│  │  [YourDevice]     (implement InputDevice)         │             │
│  └──────────────────────────────────────────────────┘             │
│                                                                   │
│  ┌─ Renderer (multi-res) ───────────────────────────┐             │
│  │  Dynamic framebuffer: 320×200 → 960×540           │             │
│  │  Font (5×7 bitmap + glow)                         │             │
│  │  Widgets: HSlider, Knob, VUMeter, Scope, Toggle   │             │
│  │  Game Menu: MenuList, TabBar, FXSlotWidget,       │             │
│  │             Breadcrumb, StatusBar                  │             │
│  │  Post-FX: scanlines, bloom, vignette, barrel      │             │
│  └──────────────────────────────────────────────────┘             │
│                                                                   │
│  ┌─ AudioEngine ─────┐  ┌─ FaustBridge ─────────────┐            │
│  │  PipeWire native   │  │  JIT (.dsp), dlopen (.so) │            │
│  │  RT callback       │  │  Runtime compile (.cpp)    │            │
│  │  Scope ring buffer │  │  Param ↔ axis mapping      │            │
│  └───────────────────┘  └───────────────────────────┘            │
│                                                                   │
│  ┌─ ChiptuneSynth ──────────────────────────────────┐             │
│  │  4 voices: Square lead, Triangle bass, Pulse pad, │             │
│  │            LFSR noise drums (kick/snare/hat)       │             │
│  │  Randomized major/minor arpeggios each menu open   │             │
│  │  RT-safe: zero-alloc, xorshift32 RNG, per-sample   │             │
│  └──────────────────────────────────────────────────┘             │
│                                                                   │
│  ┌─ Screen Stack ───────────────────────────────────┐             │
│  │  MainMenu (overlay)    — DOOM title + chiptune     │             │
│  │  FXChainScreen         — Signal flow, 12 slots    │             │
│  │  ParamScreen           — Per-FX parameter editing  │             │
│  │  VizScreen             — 5 visualization modes     │             │
│  │  SettingsScreen        — Display / Audio / Post-FX │             │
│  └──────────────────────────────────────────────────┘             │
└───────────────────────────────────────────────────────────────────┘
```

## Screens

### Main Menu (`Esc`)
DOOM-style overlay menu with animated logo. Navigate to any screen, resume, or quit. Dims the current screen underneath with a slide-in animation.

### FX Chain
Visual signal flow for up to 12 effects. Each slot shows name, bypass state, and wet/dry bar in its assigned color. Reorder mode (R key) lets you drag slots to rearrange the chain. Flow arrows between slots show the signal path from INPUT to OUTPUT.

### Parameters
Per-FX parameter editing. Slider list with the active FX slot's color accent. Page Up/Down switches between FX slots. Breadcrumb trail shows the navigation path. Supports keyboard repeat, analog axis control from gamepads/BCI, randomize, and reset-to-default.

### Visualizer
Five switchable modes via Tab within the screen:

| Mode        | Description                                      |
|:------------|:-------------------------------------------------|
| SCOPE       | Time-domain waveform with trigger modes           |
| SPECTRUM    | FFT magnitude with dB grid and frequency labels   |
| WATERFALL   | Scrolling spectrogram (violet → cyan → white)     |
| LISSAJOUS   | L/R phase plot with correlation readout            |
| PHASE       | Stereo correlation meter, balance, per-channel VU  |

### Settings
Tabbed sections (Tab to switch): Display (resolution picker), Audio (PipeWire stats), Post-FX (toggle/intensity for each effect), Input (device status), About (version/license).

## Resolution Presets

| # | Resolution | Style   | Scale |
|:--|:-----------|:--------|:------|
| 0 | 320×200    | CGA     | 4×    |
| 1 | 384×240    | DOOM    | 3×    |
| 2 | 480×270    | Wide    | 3×    |
| 3 | 640×360    | HD/4    | 2×    |
| 4 | 768×480    | Crisp   | 2×    |
| 5 | 960×540    | HD/2    | 2×    |

Change at runtime via Settings → Display or the `-R` CLI flag. The renderer dynamically reallocates its framebuffer and SDL texture. All UI is resolution-independent — widgets scale and reflow to the active resolution.

## Controls

| Action          | Keyboard              | Gamepad              | BCI            |
|:----------------|:----------------------|:---------------------|:---------------|
| Navigate        | WASD / Arrows         | D-pad                | —              |
| Adjust param    | +/- or A/D            | Left stick X         | Alpha band     |
| Fine adjust     | (analog only)         | Right stick          | Beta band      |
| Select          | Enter                 | A                    | —              |
| Back / Menu     | Escape                | B                    | —              |
| Reset param     | Backspace             | X                    | —              |
| Bypass          | B                     | Y                    | —              |
| Randomize       | R                     | —                    | —              |
| Screen cycle    | Tab                   | L/R shoulder         | —              |
| Sub-tab cycle   | [ / ]                 | L/R shoulder         | —              |
| FX slot switch  | Page Up/Down          | —                    | —              |
| Freeze (Viz)    | Space                 | Start                | —              |
| Debug overlay   | F3                    | —                    | —              |
| Fullscreen      | F11                   | —                    | —              |
| Quit            | Q                     | —                    | —              |

## Chiptune Music Engine

DeMoDOOM includes a self-contained 8-bit style music synthesizer that plays on the title screen and whenever the main menu is open. Every time you open the menu, the music re-randomizes with a fresh progression, arpeggio pattern, and drum groove.

### Voices

| Voice | Waveform     | Role                                        |
|:------|:-------------|:--------------------------------------------|
| Lead  | Square wave  | Arpeggio melody, variable duty cycle         |
| Bass  | Triangle     | Root notes in half-note rhythm + fifth walk  |
| Pad   | 25% pulse    | Chord third stabs on strong beats            |
| Drums | LFSR noise   | Kick (sine+sweep), snare (noise+body), hihat |

### Randomization

Each `reset()` generates:

- **Key**: random root note (C–B), random major or minor tonality
- **Progression**: chosen from 5 major templates (I–V–vi–IV, etc.) or 5 minor templates (i–iv–VII–III, etc.)
- **Arpeggio**: 5 styles — rising triad, falling, alternating root-fifth, up-down strum, random chord tones — with ~25% rest holes
- **Drums**: 3 kick patterns × 3 snare patterns × 2 hihat densities + random ghost notes
- **Tempo**: 120–160 BPM

### DSP Design

The synth is 100% C++, zero external dependencies, matching Faust DSP idioms: phase accumulators for oscillators, one-pole exponential envelopes, Galois LFSR for noise, soft-clip via Padé tanh `x/(1+|x|)`. RT-safe: no memory allocation in `process()`, atomic volume/playing controls, xorshift32 RNG.

## Building

### NixOS (recommended)

```bash
nix develop
cmake -B build && cmake --build build -j$(nproc)
./build/demodoom
```

### System packages

```bash
sudo apt install libsdl2-dev libpipewire-0.3-dev cmake pkg-config g++
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### With Faust JIT

```bash
# Install libfaust — auto-detected by CMake via pkg-config
cmake -B build && cmake --build build -j$(nproc)
./build/demodoom my_synth.dsp
```

## Usage

```
demodoom [OPTIONS] [dsp_file]

  -r, --rate <Hz>           Sample rate (default: 48000)
  -b, --block <frames>      Block size  (default: 256)
  -R, --resolution <0-5>    Framebuffer preset (default: 1 = DOOM 384x240)
  -f, --fullscreen          Start fullscreen
  --bci                     Enable OpenBCI/LSL
  -h, --help                Help

DSP formats: .dsp (JIT), .cpp (runtime compile), .so (dlopen)
No file = demo mode with test oscillator + synthetic FX chain.
```

## Adding Input Devices

Implement the `InputDevice` interface:

```cpp
class MyDevice : public InputDevice {
    std::string name() const override;
    std::string type_tag() const override;
    bool connected() const override;
    void poll(std::vector<RawEvent>& out) override;
    std::vector<Binding> default_bindings() const override;
};
```

Register in `Engine::init()`:
```cpp
input_.add_device(std::make_unique<MyDevice>());
```

Raw events get resolved to semantic Actions through Bindings. Any number of devices can drive the same parameter simultaneously.

## Post-Processing Pipeline

All effects run on the CPU framebuffer before upload to the GPU texture:

1. **Bloom** — 3×3 additive blur on pixels above luma threshold 160
2. **Scanlines** — darken alternate rows (adjustable 0–100%)
3. **Vignette** — radial darkening with power-curve falloff
4. **Barrel distortion** — CRT curvature (optional, CPU-intensive)

All togglable via Settings or the F3 debug overlay.

## License

GPL-3.0 — DeMoD LLC
