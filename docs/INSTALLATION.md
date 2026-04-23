# Installation — The Eternal Mixtape

## Which ZIP should I download?

| Platform | Download |
|---|---|
| Windows with an NVIDIA / AMD / Intel GPU | `TheEternalMixtape-<version>-Windows.zip` |
| Windows, CPU only | *(GPU build also runs on CPU — download the same ZIP)* |
| macOS (Apple Silicon M1/M2/M3 or Intel) | `TheEternalMixtape-<version>-macOS.zip` |
| Linux | `TheEternalMixtape-<version>-Linux.zip` |

Download from the [Releases page](../../releases).

---

## Windows

1. Download `TheEternalMixtape-<version>-Windows.zip`.
2. Right-click → **Extract All** to a folder of your choice, e.g. `C:\Apps\TheEternalMixtape\`.
3. Double-click `The Eternal Mixtape.exe` inside the extracted folder.

That's it. No installer, no admin rights required.

**GPU acceleration** is enabled automatically if your system has a DirectX 12-capable GPU (all discrete GPUs since ~2014 and many integrated GPUs). See [GPU_SETUP.md](GPU_SETUP.md) if you want to verify or troubleshoot.

### Known limitations (Windows)
- Input audio must be 44.1 kHz or will be resampled automatically.
- Maximum audio length: 30 minutes (guard against malformed file headers).

---

## macOS

1. Download `TheEternalMixtape-<version>-macOS.zip`.
2. Double-click the ZIP to expand it. Move `The Eternal Mixtape.app` to your `Applications` folder (or anywhere you prefer).
3. Double-click the app to launch.

**Apple Silicon (M1/M2/M3):** GPU acceleration via Apple's CoreML/Neural Engine is enabled automatically.

**Intel Mac:** Runs on CPU. Performance will be slower — see [PERFORMANCE.md](PERFORMANCE.md) for expected times.

> **Gatekeeper warning:** Since the app is not code-signed for v0.1, macOS may show "cannot be opened because the developer cannot be verified." To bypass:
> Right-click the app → **Open** → **Open** in the dialog. You only need to do this once.

---

## Linux

1. Download `TheEternalMixtape-<version>-Linux.zip`.
2. Extract: `unzip TheEternalMixtape-<version>-Linux.zip -d TheEternalMixtape/`
3. Make the binary executable: `chmod +x TheEternalMixtape/"The Eternal Mixtape"`
4. Run: `./TheEternalMixtape/"The Eternal Mixtape"`

GPU acceleration is not included in the Linux build. See [PERFORMANCE.md](PERFORMANCE.md) for CPU timing expectations.

---

## What's inside the ZIP

```
TheEternalMixtape/
├── The Eternal Mixtape.exe   (Windows) / .app (macOS) / binary (Linux)
├── onnxruntime*.dll          (Windows only — ORT runtime)
└── models/
    └── htdemucs.onnx         (167 MB — Demucs v4 stem separation model)
```

The model ships bundled in the ZIP. No internet connection or additional downloads are required after unzipping.

---

## Uninstalling

Delete the extracted folder. No registry entries, no system files, no `~/Library` changes.
