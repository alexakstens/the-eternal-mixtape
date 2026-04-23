# GPU Setup — The Eternal Mixtape

GPU acceleration makes stem separation significantly faster. This page explains what's enabled by default and what (if anything) you need to do.

---

## Windows — DirectML

**Nothing to install.** DirectML is part of Windows 10 version 1903 and later.

The app will automatically use your GPU if:
- You are on Windows 10 1903+ (build 18362) or Windows 11, and
- Your GPU supports DirectX 12 (virtually all discrete GPUs since ~2014, and most integrated GPUs since 2016).

### Verifying GPU is active

The status line during separation shows the backend in use. If it says something like `Loading model...` and then inference completes faster than the expected CPU time from [PERFORMANCE.md](PERFORMANCE.md), GPU is working.

### Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Inference takes as long as CPU times | GPU not detected or not DX12-capable | Update Windows; update GPU drivers |
| App crashes on launch | DX12 not supported | Run the CPU-only build (same ZIP, GPU providers fall back internally) |
| Old GPU (pre-2014) | No DX12 support | Use the CPU build — inference still works, just slower |

### Super-user: CUDA/cuDNN path

If you want maximum throughput on an NVIDIA GPU and are comfortable building from source:
1. Install CUDA Toolkit 12.x and cuDNN 9.x from NVIDIA.
2. Download `onnxruntime-win-x64-gpu-1.24.1` from the ONNX Runtime releases.
3. Build with `-DORT_USE_GPU=ON -DONNXRUNTIME_ROOT=<path-to-ort-gpu>` and set `CUDNN_BIN_DIR` if cuDNN is in a non-standard location.

The CUDA provider uses exhaustive cuDNN algorithm search on first run, which may cause a longer warm-up on the first inference.

---

## macOS — CoreML / Apple Neural Engine

**Nothing to install.** CoreML is built into macOS.

GPU acceleration is automatic on:
- Apple Silicon (M1, M2, M3, and later) — uses the Neural Engine and GPU via CoreML
- Intel Mac — CoreML is available but the Neural Engine is absent; CPU inference is used

Apple Silicon performance is typically 3–6× faster than Intel CPU. See [PERFORMANCE.md](PERFORMANCE.md).

---

## Linux

The v0.1 Linux build uses CPU inference only. There is no GPU-accelerated Linux build at this time.

If you need GPU inference on Linux, you can build from source with the ONNX Runtime Linux GPU package and `-DORT_USE_GPU=ON`. Note: CoreML is not available on Linux; CUDA would need to be substituted in `SeparationThread.h`.
