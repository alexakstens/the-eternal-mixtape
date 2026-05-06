# DemucsGUI - Setup Recap

## What Was Done

Created a JUCE standalone application for offline source separation using the pamplejuce template + cmucs core library.

### Project Structure
```
DemucsGUI/
├── CMakeLists.txt              # Pamplejuce template + cmucs_core + ONNX Runtime
├── JUCE/                       # JUCE submodule (from pamplejuce)
├── modules/                    # clap-juce-extensions, melatonin_inspector
├── source/
│   ├── PluginProcessor.h/cpp   # Minimal processor, holds SeparationThread
│   ├── PluginEditor.h/cpp      # Full GUI: file browsers, progress, CUDA toggle
│   ├── SeparationThread.h      # Background thread: model load → audio load → inference → stem export
│   └── AudioConversion.h       # JUCE AudioBuffer ↔ Eigen MatrixXf conversion
├── cmake/                      # Pamplejuce cmake helpers
└── tests/                      # Pamplejuce test scaffolding
```

### Key Architecture Decisions
- **Standalone app only** (not a real-time DAW plugin) — Demucs v4 has 7.8s latency, unsuitable for real-time
- **cmucs_core as static lib** — builds `dsp.cpp`, `demucs.cpp`, `audio_io.cpp` from the cmucs project
- **Background thread** — `SeparationThread` runs inference off the message thread with progress callbacks
- **JUCE for audio I/O** — Uses `AudioFormatManager` (supports WAV, MP3, AIFF, FLAC) instead of dr_wav/dr_mp3
- **Drag-and-drop** — Drop audio files onto the window to set input

### Dependencies
| Dependency | Source |
|---|---|
| JUCE | `JUCE/` submodule (pamplejuce) |
| cmucs_core | `../../MyCppProjects/cmucs/src/` (configurable via `CMUCS_ROOT`) |
| Eigen 3.4.0 | `cmucs/external/eigen-3.4.0/` |
| ONNX Runtime 1.24.1 GPU | `C:\Users\ryanb\onnxruntime-win-x64-gpu-1.24.1` |
| melatonin_inspector | `modules/melatonin_inspector` submodule |

## How to Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMUCS_ROOT="C:\Users\ryanb\OneDrive\Documents\MyProjects\MyCppProjects\cmucs" `
    -DONNXRUNTIME_ROOT="C:\Users\ryanb\onnxruntime-win-x64-gpu-1.24.1"
cmake --build build --config Release
```

The ONNX Runtime DLLs are automatically copied to the output directory post-build.

**For CUDA/GPU support:** Ensure cuDNN 9.x DLLs are on your system PATH or copy them to the build output directory.

## Build Fixes Applied

- **C++17 for cmucs_core**: Added `target_compile_features(cmucs_core PUBLIC cxx_std_17)` in CMakeLists.txt — cmucs sources require C++17 for `std::filesystem` in `audio_io.cpp`
- **ONNX Runtime DLL path**: Fixed post-build copy to target `Standalone/` subdirectory (JUCE places the exe there, not in the artefacts root)
- **Test plugin name**: Updated `PluginBasics.cpp` to check for "Demucs Source Separator" instead of "Pamplejuce Demo"

## What Remains (TODO)

### Nice to Have
- [ ] Sample rate resampling (currently rejects non-44100Hz files)
- [ ] Waveform display of input/output
- [ ] Stem preview/playback with solo/mute
- [ ] Batch processing (multiple files)
- [ ] Model selection dropdown (htdemucs, htdemucs_ft, htdemucs_6s)
- [ ] Progress ETA display
- [ ] Remember last used paths (save to app properties)
- [ ] Installer/packaging

### Known Issues
- ONNX Runtime CUDA cleanup crash on Windows (harmless, see cmucs SESSION_RECAP.md)
- Model path is hardcoded to user's Documents folder for auto-detection
