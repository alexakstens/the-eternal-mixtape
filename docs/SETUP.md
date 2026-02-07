# DemucsGUI — Full Setup Guide

A JUCE standalone application for offline music source separation using [Demucs v4](https://github.com/facebookresearch/demucs) (HTDemucs) via ONNX Runtime. Loads audio files, runs GPU-accelerated inference, and exports individual stems (drums, bass, other, vocals).

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Clone the Repository](#2-clone-the-repository)
3. [Download ONNX Runtime](#3-download-onnx-runtime)
4. [Obtain the Demucs ONNX Model](#4-obtain-the-demucs-onnx-model)
5. [Set Up GPU Acceleration (Optional)](#5-set-up-gpu-acceleration-optional)
6. [Clone the cmucs Core Library](#6-clone-the-cmucs-core-library)
7. [Configure and Build](#7-configure-and-build)
8. [Run the Application](#8-run-the-application)
9. [Troubleshooting](#9-troubleshooting)
10. [Project Structure](#10-project-structure)
11. [Known Issues](#11-known-issues)

---

## 1. Prerequisites

### Required

| Tool | Version | Download |
|---|---|---|
| **Visual Studio 2022** | 17.x | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| **CMake** | 3.25+ | [cmake.org/download](https://cmake.org/download/) |
| **Git** | any recent | [git-scm.com](https://git-scm.com/) |

When installing Visual Studio, select the **"Desktop development with C++"** workload.

### For GPU Acceleration (Recommended)

| Component | Version | Notes |
|---|---|---|
| **NVIDIA GPU** | Compute capability 5.0+ | Maxwell or newer (GTX 900+, RTX series) |
| **CUDA Toolkit** | 12.x | [developer.nvidia.com/cuda-downloads](https://developer.nvidia.com/cuda-downloads) |
| **cuDNN** | 9.x | [developer.nvidia.com/cudnn](https://developer.nvidia.com/cudnn) |

Without CUDA/cuDNN, the application falls back to CPU inference automatically.

---

## 2. Clone the Repository

```powershell
git clone --recurse-submodules https://github.com/YOUR_USERNAME/DemucsGUI.git
cd DemucsGUI
```

If you already cloned without `--recurse-submodules`, initialize submodules manually:

```powershell
git submodule update --init --recursive
```

This pulls four submodules:

| Submodule | Path | Purpose |
|---|---|---|
| JUCE | `JUCE/` | Audio application framework |
| cmake-includes | `cmake/` | Pamplejuce CMake helpers |
| clap-juce-extensions | `modules/clap-juce-extensions/` | CLAP plugin format support |
| melatonin_inspector | `modules/melatonin_inspector/` | UI debugging inspector |

---

## 3. Download ONNX Runtime

Download the **GPU** package of ONNX Runtime (v1.24.1 or compatible) for Windows x64.

### Option A: PowerShell (Automated)

```powershell
$ortVersion = "1.24.1"
$ortUrl = "https://github.com/microsoft/onnxruntime/releases/download/v${ortVersion}/onnxruntime-win-x64-gpu-${ortVersion}.zip"
$ortDest = "$env:USERPROFILE\onnxruntime-win-x64-gpu-${ortVersion}"

# Download and extract
Invoke-WebRequest -Uri $ortUrl -OutFile "$env:TEMP\ort-gpu.zip"
Expand-Archive "$env:TEMP\ort-gpu.zip" -DestinationPath $env:USERPROFILE -Force
Remove-Item "$env:TEMP\ort-gpu.zip"

Write-Host "ONNX Runtime extracted to: $ortDest"
```

### Option B: Manual Download

1. Go to [ONNX Runtime Releases](https://github.com/microsoft/onnxruntime/releases/tag/v1.24.1)
2. Download `onnxruntime-win-x64-gpu-1.24.1.zip`
3. Extract to a known location, e.g. `C:\Users\<you>\onnxruntime-win-x64-gpu-1.24.1`

The extracted directory should contain:

```
onnxruntime-win-x64-gpu-1.24.1/
├── include/
│   ├── onnxruntime_cxx_api.h
│   └── ...
└── lib/
    ├── onnxruntime.dll
    ├── onnxruntime.lib
    ├── onnxruntime_providers_cuda.dll
    ├── onnxruntime_providers_shared.dll
    └── onnxruntime_providers_tensorrt.dll
```

> **Note:** If you only need CPU inference, download `onnxruntime-win-x64-1.24.1.zip` (the non-GPU package) instead. The build system will still work — CUDA simply won't be available.

---

## 4. Obtain the Demucs ONNX Model

The application requires a Demucs model converted to ONNX format (~166 MB for htdemucs). The model file is **not** included in this repository due to its size.

### Option A: Copy from an Existing cmucs Build

If you already have the [cmucs](https://github.com/YOUR_USERNAME/cmucs) CLI project set up, the model is at:

```
cmucs/models/htdemucs.onnx
```

No additional download needed — the application auto-detects this path.

### Option B: Convert from PyTorch (One-Time Python Step)

This converts Meta's pretrained Demucs v4 weights to ONNX format. Requires Python 3.8+ with PyTorch.

```powershell
# 1. Create a Python environment (optional but recommended)
conda create -n demucs-convert python=3.10 -y
conda activate demucs-convert

# 2. Clone the demucs.onnx converter tool
git clone --recurse-submodules https://github.com/sevagh/demucs.onnx.git
cd demucs.onnx

# 3. Install dependencies
pip install torch torchaudio numpy onnx
pip install -e demucs-for-onnx/

# 4. Convert the model (downloads pretrained weights automatically)
#    Output: <dest_dir>/htdemucs.onnx (~166 MB)
python scripts/convert-pth-to-onnx.py <dest_dir>
```

Replace `<dest_dir>` with where you want the model saved, e.g.:

```powershell
python scripts/convert-pth-to-onnx.py C:\Users\ryanb\OneDrive\Documents\MyProjects\MyCppProjects\cmucs\models
```

#### Available Model Variants

| Model | Flag | Sources | Size | Description |
|---|---|---|---|---|
| `htdemucs` | *(default)* | 4 (drums, bass, other, vocals) | ~166 MB | Standard model, good quality/speed balance |
| `htdemucs_6s` | `--six-source` | 6 (+guitar, piano) | ~166 MB | Extended source separation |
| `htdemucs_ft_drums` | `--ft-drums` | 4 | ~166 MB | Fine-tuned for better drum isolation |
| `htdemucs_ft_bass` | `--ft-bass` | 4 | ~166 MB | Fine-tuned for better bass isolation |
| `htdemucs_ft_other` | `--ft-other` | 4 | ~166 MB | Fine-tuned for better "other" isolation |
| `htdemucs_ft_vocals` | `--ft-vocals` | 4 | ~166 MB | Fine-tuned for better vocal isolation |

Example for the 6-source model:

```powershell
python scripts/convert-pth-to-onnx.py models --six-source
```

### Where to Place the Model

The application auto-detects the model at:

```
<userDocuments>/MyProjects/MyCppProjects/cmucs/models/htdemucs.onnx
```

You can also browse to any `.onnx` file from the GUI.

---

## 5. Set Up GPU Acceleration (Optional)

GPU acceleration dramatically speeds up inference (~10x over CPU). Skip this section to use CPU-only mode.

### 5.1 Install CUDA Toolkit

1. Download [CUDA Toolkit 12.x](https://developer.nvidia.com/cuda-downloads) for Windows
2. Run the installer (Express install is fine)
3. Verify: `nvcc --version` should show CUDA 12.x

### 5.2 Install cuDNN

ONNX Runtime 1.24.1 requires **cuDNN 9.x**:

1. Download from [NVIDIA cuDNN](https://developer.nvidia.com/cudnn) (requires free NVIDIA account)
2. Install to default location: `C:\Program Files\NVIDIA\CUDNN\v9.x\`
3. **Important:** Add cuDNN DLLs to your PATH, or copy them alongside the application:

```powershell
# Option 1: Add to PATH (permanent — run as Administrator)
[Environment]::SetEnvironmentVariable("Path",
    "$env:Path;C:\Program Files\NVIDIA\CUDNN\v9.19\bin\12.9\x64",
    [EnvironmentVariableTarget]::Machine)

# Option 2: Copy to the build output (per-build)
Copy-Item "C:\Program Files\NVIDIA\CUDNN\v9.19\bin\12.9\x64\*.dll" `
    "build\DemucsGUI_artefacts\Release\Standalone\"
```

### 5.3 Laptop GPU Selection (Dual-GPU Laptops)

If your laptop has both integrated and discrete GPUs, force the app to use the NVIDIA GPU:

1. **Settings → System → Display → Graphics settings**
2. Click **Browse**, add `Demucs Source Separator.exe`
3. Set to **High performance** (discrete GPU)

---

## 6. Clone the cmucs Core Library

DemucsGUI depends on the `cmucs` core library (source separation DSP + inference code). By default, it expects cmucs to be a sibling project:

```
MyProjects/
├── MyCppProjects/
│   └── cmucs/          ← core library
└── MyPluginProjects/
    └── DemucsGUI/      ← this project
```

If you don't have cmucs yet:

```powershell
cd C:\Users\<you>\OneDrive\Documents\MyProjects\MyCppProjects
git clone https://github.com/YOUR_USERNAME/cmucs.git
```

The key files used from cmucs are:

| File | Purpose |
|---|---|
| `src/demucs.cpp` / `.hpp` | Model loading, segmented inference, weighted blending |
| `src/dsp.cpp` / `.hpp` | STFT/iSTFT implementation |
| `src/audio_io.cpp` / `.hpp` | WAV/MP3 I/O via dr_wav/dr_mp3 |
| `src/tensor.hpp` | Eigen RowMajor tensor type aliases |
| `external/eigen-3.4.0/` | Header-only linear algebra + FFT |
| `external/dr_wav.h`, `dr_mp3.h` | Single-header audio decoders |

> **Note:** You can place cmucs anywhere — just set `CMUCS_ROOT` when running CMake (see below).

---

## 7. Configure and Build

### 7.1 CMake Configure

```powershell
cd C:\Users\<you>\OneDrive\Documents\MyProjects\MyPluginProjects\DemucsGUI

cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMUCS_ROOT="C:\Users\<you>\OneDrive\Documents\MyProjects\MyCppProjects\cmucs" `
    -DONNXRUNTIME_ROOT="C:\Users\<you>\onnxruntime-win-x64-gpu-1.24.1"
```

| CMake Variable | Default | Description |
|---|---|---|
| `CMUCS_ROOT` | `../../MyCppProjects/cmucs` | Path to the cmucs project root |
| `ONNXRUNTIME_ROOT` | `C:/Users/ryanb/onnxruntime-win-x64-gpu-1.24.1` | Path to extracted ONNX Runtime package |

### 7.2 Build

```powershell
cmake --build build --config Release
```

This builds:

| Target | Output | Description |
|---|---|---|
| `DemucsGUI` | `build/DemucsGUI_artefacts/Release/Standalone/Demucs Source Separator.exe` | Main application |
| `Tests` | `build/Release/Tests.exe` | Catch2 test suite |
| `cmucs_core` | `build/Release/cmucs_core.lib` | Static library (linked into app) |

ONNX Runtime DLLs are automatically copied to the Standalone output directory by a post-build step.

### 7.3 Verify the Build

```powershell
# Check the executable and DLLs are present
Get-ChildItem "build\DemucsGUI_artefacts\Release\Standalone" |
    Select-Object Name, @{N='SizeMB';E={[math]::Round($_.Length/1MB,1)}} |
    Format-Table -AutoSize
```

Expected output:

```
Name                                SizeMB
----                                ------
Demucs Source Separator.exe            7.3
onnxruntime.dll                       13.7
onnxruntime_providers_cuda.dll       262.9
onnxruntime_providers_shared.dll       0.0
onnxruntime_providers_tensorrt.dll     0.8
```

### 7.4 Run Tests

```powershell
.\build\Release\Tests.exe
```

---

## 8. Run the Application

### Launch

```powershell
& "build\DemucsGUI_artefacts\Release\Standalone\Demucs Source Separator.exe"
```

### Usage

1. **Select input file** — Click "Browse" or drag-and-drop a WAV/MP3/AIFF/FLAC file onto the window
2. **Select model** — The app auto-detects `cmucs/models/htdemucs.onnx`; browse for a different model if needed
3. **Output directory** — Auto-set to `<input_filename>_stems/` next to the input file
4. **CUDA toggle** — Check "Use CUDA (GPU)" for GPU acceleration (unchecked = CPU only)
5. **Click "Separate"** — Progress bar shows real-time segment updates

### Input Requirements

- **Formats:** WAV, MP3, AIFF, FLAC (via JUCE AudioFormatManager)
- **Sample rate:** 44100 Hz (other rates are currently rejected)
- **Channels:** Mono or stereo (mono is automatically duplicated to stereo)

### Output

Stems are written as 32-bit float WAV files at 44100 Hz:

| Stem | File |
|---|---|
| Drums | `drums.wav` |
| Bass | `bass.wav` |
| Other (instruments) | `other.wav` |
| Vocals | `vocals.wav` |
| Guitar | `guitar.wav` (6-source model only) |
| Piano | `piano.wav` (6-source model only) |

---

## 9. Troubleshooting

### CMake can't find ONNX Runtime

```
LINK : fatal error LNK1181: cannot open input file 'onnxruntime.lib'
```

**Fix:** Ensure `ONNXRUNTIME_ROOT` points to the correct directory containing `lib/onnxruntime.lib`:

```powershell
cmake -B build -DONNXRUNTIME_ROOT="C:\path\to\onnxruntime-win-x64-gpu-1.24.1" ...
```

### Missing cmucs source files

```
CMake Error at CMakeLists.txt: Cannot find source file: .../cmucs/src/demucs.cpp
```

**Fix:** Set `CMUCS_ROOT` to the actual cmucs project path:

```powershell
cmake -B build -DCMUCS_ROOT="C:\path\to\cmucs" ...
```

### `std::filesystem` errors during build

```
error C2039: 'filesystem': is not a member of 'std'
```

**Fix:** Already resolved — `cmucs_core` now sets `cxx_std_17`. If you see this, delete the `build/` folder and reconfigure.

### CUDA not detected at runtime

The app shows "GPU: CPU Only" even though you have an NVIDIA GPU.

**Causes & fixes:**
1. **cuDNN not installed or not on PATH** — See [Section 5.2](#52-install-cudnn)
2. **Laptop using integrated GPU** — See [Section 5.3](#53-laptop-gpu-selection-dual-gpu-laptops)
3. **Using the CPU-only ONNX Runtime package** — Re-download the GPU variant

### Application crashes after successful completion

Exit code `-1073741819` (access violation) after stems are written correctly.

**This is a known harmless bug** in ONNX Runtime's CUDA cleanup on Windows. See [microsoft/onnxruntime#25670](https://github.com/microsoft/onnxruntime/issues/25670). All output files are written correctly before the crash occurs.

### "Unsupported sample rate" error

The application currently requires 44100 Hz input. To convert a file:

```powershell
ffmpeg -i input_48000.wav -ar 44100 input_44100.wav
```

---

## 10. Project Structure

```
DemucsGUI/
├── CMakeLists.txt                  # Build system: pamplejuce + cmucs_core + ONNX Runtime
├── VERSION                         # Semantic version (0.0.1)
├── JUCE/                           # JUCE framework (git submodule)
├── cmake/                          # Pamplejuce CMake helpers (git submodule)
├── modules/
│   ├── clap-juce-extensions/       # CLAP plugin format (git submodule)
│   └── melatonin_inspector/        # UI debug inspector (git submodule)
├── source/
│   ├── PluginProcessor.h/cpp       # Minimal audio processor, owns SeparationThread
│   ├── PluginEditor.h/cpp          # GUI: file browsers, progress bar, CUDA toggle, drag-and-drop
│   ├── SeparationThread.h          # Background thread: model load → audio load → inference → stem export
│   └── AudioConversion.h           # JUCE AudioBuffer<float> ↔ Eigen MatrixXf conversion
├── tests/
│   ├── Catch2Main.cpp              # Catch2 v3 entry point
│   └── PluginBasics.cpp            # Basic plugin instantiation tests
├── benchmarks/                     # Catch2 benchmark scaffolding
├── assets/images/                  # Binary data embedded in the app
├── packaging/                      # Installer resources (icon, ISS script, DMG config)
└── docs/
    └── SETUP.md                    # This file
```

### Key Architecture Decisions

- **Standalone only** — Demucs v4 processes 7.8s segments, making real-time DAW use infeasible
- **Background thread** — `SeparationThread` runs inference off the GUI thread with progress callbacks
- **cmucs_core as static lib** — Reuses the proven CLI inference pipeline (DSP + ONNX)
- **JUCE for audio I/O** — `AudioFormatManager` handles WAV/MP3/AIFF/FLAC natively
- **Drag-and-drop** — Drop audio files directly onto the window

### Dependencies

| Dependency | Version | Source | Purpose |
|---|---|---|---|
| JUCE | develop | Git submodule | Audio framework, GUI, file I/O |
| cmucs_core | — | External path (`CMUCS_ROOT`) | Demucs inference, STFT/iSTFT, audio I/O |
| Eigen | 3.4.0 | Vendored in cmucs | Tensor operations, FFT |
| ONNX Runtime | 1.24.1 GPU | External download | Neural network inference |
| Catch2 | 3.8.1 | CPM (auto-downloaded) | Testing framework |
| melatonin_inspector | main | Git submodule | UI debugging |
| clap-juce-extensions | main | Git submodule | CLAP format support |

---

## 11. Known Issues

| Issue | Impact | Status |
|---|---|---|
| CUDA cleanup crash (exit code -1073741819) | Harmless — all output written before crash | Upstream ONNX Runtime bug ([#25670](https://github.com/microsoft/onnxruntime/issues/25670)) |
| No sample rate resampling | Files must be 44100 Hz | Planned |
| Model path auto-detect is hardcoded | Only works for default project layout | Use Browse button as workaround |
| Eigen C4127 warnings during build | Cosmetic only, no functional impact | Eigen 3.4.0 limitation |

---

## Quick Reference

```powershell
# Full build from scratch
git clone --recurse-submodules https://github.com/YOUR_USERNAME/DemucsGUI.git
cd DemucsGUI

cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMUCS_ROOT="C:\path\to\cmucs" `
    -DONNXRUNTIME_ROOT="C:\path\to\onnxruntime-win-x64-gpu-1.24.1"

cmake --build build --config Release

& "build\DemucsGUI_artefacts\Release\Standalone\Demucs Source Separator.exe"
```
