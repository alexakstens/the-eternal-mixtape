# DemucsGUI

A JUCE standalone application for offline music source separation using [Demucs v4](https://github.com/facebookresearch/demucs) (HTDemucs) via [ONNX Runtime](https://github.com/microsoft/onnxruntime) with CUDA GPU acceleration. Thanks to Sevag H for their work on [demucs.onnx](https://github.com/sevagh/demucs.onnx).

Load an audio file, click **Separate**, and export individual stems: **drums**, **bass**, **other**, and **vocals**.

---

## Features

- **GPU-Accelerated Inference** — Uses ONNX Runtime with CUDA for fast processing (~10x faster than CPU)
- **Real-time Waveform Display** — Input waveform loads on file selection; stem waveforms appear after separation
- **Progress & ETA** — Live progress bar with estimated time remaining
- **Drag-and-Drop** — Drop audio files directly onto the window
- **Sample Rate Conversion** — Auto-resamples any input to 44.1kHz (JUCE LagrangeInterpolator)
- **Cancellable Processing** — Stop mid-inference with the Cancel button
- **Multiple Audio Formats** — WAV, MP3, AIFF, FLAC via JUCE AudioFormatManager

---

## Quick Start

### Prerequisites

| Tool | Version | Download |
|---|---|---|
| **Visual Studio 2022** | 17.x | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| **CMake** | 3.25+ | [cmake.org/download](https://cmake.org/download/) |
| **Git** | any recent | [git-scm.com](https://git-scm.com/) |

For GPU acceleration (recommended):

| Component | Version | Notes |
|---|---|---|
| **NVIDIA GPU** | Compute 5.0+ | Maxwell or newer (GTX 900+, RTX series) |
| **CUDA Toolkit** | 12.x | [developer.nvidia.com/cuda-downloads](https://developer.nvidia.com/cuda-downloads) |
| **cuDNN** | 9.x | [developer.nvidia.com/cudnn](https://developer.nvidia.com/cudnn) |

### Build from Source

```powershell
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/bakerbass/DemucsGUI.git
cd DemucsGUI

# 2. Download ONNX Runtime GPU (1.24.1)
$ortUrl = "https://github.com/microsoft/onnxruntime/releases/download/v1.24.1/onnxruntime-win-x64-gpu-1.24.1.zip"
Invoke-WebRequest -Uri $ortUrl -OutFile "$env:TEMP\ort-gpu.zip"
Expand-Archive "$env:TEMP\ort-gpu.zip" -DestinationPath $env:USERPROFILE -Force

# 3. Clone the cmucs core library (C++ Demucs implementation)
git clone https://github.com/bakerbass/cmucs.git ..\cmucs

# 4. Configure CMake
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMUCS_ROOT="..\cmucs" `
    -DONNXRUNTIME_ROOT="$env:USERPROFILE\onnxruntime-win-x64-gpu-1.24.1"

# 5. Build
cmake --build build --config Release
```

The executable will be at:
```
build\DemucsGUI_artefacts\Release\Standalone\Demucs Source Separator.exe
```

### Get the Model

Download the HTDemucs ONNX model:

```powershell
# Create models directory in cmucs
New-Item -Path "..\cmucs\models" -ItemType Directory -Force

# Download htdemucs.onnx (~175MB)
# NOTE: The ONNX model is not yet officially distributed by Meta.
# You'll need to export it from the PyTorch version using the conversion
# scripts in the demucs.onnx repository.
```

> **Note**: See the [Demucs ONNX export guide](https://github.com/sevagh/demucs.onnx) for conversion instructions.

The app auto-detects a model at:
```
~/Documents/MyProjects/MyCppProjects/cmucs/models/htdemucs.onnx
```

---

## Usage

1. **Launch** the application
2. **Browse** or **drag-and-drop** an audio file (WAV, MP3, AIFF, FLAC)
3. **Select model** (auto-detected if placed in default location)
4. **Choose output directory** (defaults to `<input_name>_stems/`)
5. **Toggle CUDA** on/off (GPU vs CPU inference)
6. Click **Separate** — progress bar shows ETA
7. **Cancel** anytime to abort processing
8. Output stems appear in the output directory as `drums.wav`, `bass.wav`, `other.wav`, `vocals.wav`

Stem waveforms (color-coded) appear in the UI after separation completes.

---

## Architecture

Built on the [pamplejuce](https://github.com/sudara/pamplejuce) JUCE/CMake template with custom source separation engine.

### Project Structure

```
DemucsGUI/
├── source/
│   ├── PluginEditor.h/cpp      # GUI: file browsers, waveforms, progress
│   ├── PluginProcessor.h/cpp   # Minimal processor, holds SeparationThread
│   ├── SeparationThread.h      # Background thread for inference
│   ├── AudioConversion.h       # JUCE ↔ Eigen conversion utilities
│   └── WaveformDisplay.h       # AudioThumbnail wrapper component
├── CMakeLists.txt              # Integrates cmucs_core + ONNX Runtime
├── JUCE/                       # JUCE framework (submodule)
└── modules/                    # clap-juce-extensions, melatonin_inspector
```

### Key Design Decisions

- **Standalone-only** — Demucs has 7.8s latency (unsuitable for real-time DAW plugin)
- **cmucs_core static library** — Builds `dsp.cpp`, `demucs.cpp`, `audio_io.cpp` from the [cmucs project](https://github.com/bakerbass/cmucs)
- **Background thread** — All inference runs off the message thread with progress callbacks
- **JUCE audio I/O** — Supports more formats than the CLI version (which uses dr_wav/dr_mp3)

---

## Dependencies

| Component | Version | Source |
|---|---|---|
| **JUCE** | 8.x | `JUCE/` submodule |
| **cmucs** | latest | `CMUCS_ROOT` (separate repo) |
| **Eigen** | 3.4.0 | `cmucs/external/eigen-3.4.0/` |
| **ONNX Runtime GPU** | 1.24.1 | Downloaded archive |
| **Melatonin Inspector** | latest | `modules/melatonin_inspector/` |
| **CLAP** | 1.2.0 | `modules/clap-juce-extensions/` |
| **Catch2** | 3.8.1 | CPM (test framework) |

---

## Known Issues

- **CUDA cleanup crash on Windows** — Harmless exit code -1073741819 after successful separation. Known ONNX Runtime bug ([microsoft/onnxruntime#25670](https://github.com/microsoft/onnxruntime/issues/25670)). Does not affect output.
- **Large model files** — ONNX models (~175MB) must be obtained separately (not included in repo).

---

## Roadmap

- [x] Sample rate resampling
- [x] Waveform display (input + stems)
- [x] Progress ETA
- [x] Cancellable inference
- [ ] Remember last used paths (app properties)
- [ ] Model selection dropdown (htdemucs, htdemucs_ft, htdemucs_6s)
- [ ] Stem preview/playback with solo/mute
- [ ] Batch processing (multiple files)
- [ ] Installer/packaging

---

## Credits

- **Demucs v4** by Meta Research — [github.com/facebookresearch/demucs](https://github.com/facebookresearch/demucs)
- **ONNX Runtime** by Microsoft — [onnxruntime.ai](https://onnxruntime.ai/)
- **JUCE** by Raw Material Software — [juce.com](https://juce.com/)
- **pamplejuce** template by Sudara — [github.com/sudara/pamplejuce](https://github.com/sudara/pamplejuce)
- **Eigen** — [eigen.tuxfamily.org](https://eigen.tuxfamily.org/)
- **Melatonin Inspector** by Sudara — [github.com/sudara/melatonin_inspector](https://github.com/sudara/melatonin_inspector)

---

## License

This project uses JUCE under the GPL-3.0 license. If you distribute binaries, you must either:
1. Open-source your code under GPL-3.0, or
2. Purchase a JUCE commercial license from [juce.com](https://juce.com/)

See [JUCE licensing](https://juce.com/legal/juce-8-licence/) for details.

---

## Documentation

- [docs/SETUP.md](docs/SETUP.md) — Detailed build instructions and troubleshooting
- [SETUP_RECAP.md](SETUP_RECAP.md) — Project structure and architecture notes
- [SESSION_RECAP_FEB7.md](SESSION_RECAP_FEB7.md) — Latest session development notes

---

## Contributing

Issues and pull requests welcome. For major changes, open an issue first to discuss.
