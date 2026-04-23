# Performance — The Eternal Mixtape

Stem separation uses Demucs v4 (HTDemucs), a neural network that processes audio in 7.8-second segments. Processing time scales linearly with audio length.

---

## Expected processing times

> **Status:** placeholder — fill in results from running `./Benchmarks [bench]` on each platform before release.
> See [Phase 5 in the release plan](../C:/Users/ryanb/.claude/plans/plan-a-release-for-kind-donut.md) for the benchmark procedure.

| Platform | Hardware | Backend | Time per minute of audio | Notes |
|---|---|---|---|---|
| Windows | NVIDIA RTX 3060 | DirectML | _TBD_ | |
| Windows | NVIDIA RTX 4080 | DirectML | _TBD_ | |
| Windows | AMD RX 6700 | DirectML | _TBD_ | |
| Windows | Intel i9-13900K (no GPU) | ONNX CPU | _TBD_ | |
| macOS | Apple M1 Pro | CoreML | _TBD_ | |
| macOS | Apple M2 Max | CoreML | _TBD_ | |
| macOS | Intel Core i7 (2020 MBP) | ONNX CPU | _TBD_ | |
| Linux | AMD Ryzen 9 5900X | ONNX CPU | _TBD_ | |

---

## How to run the benchmark

Build in Release mode, then:

```bash
# Windows / Linux
./Benchmarks "[bench]"

# macOS
./Benchmarks "[bench]"
```

The benchmark runs a 15-second synthetic clip through the full inference pipeline and prints:
```
Inference time for 15s clip: 12.3s  (49.2s per minute of audio)
```

Record the **seconds per minute of audio** value in the table above.

---

## Minimum system requirements

| Component | Minimum | Recommended |
|---|---|---|
| OS | Windows 10 1903, macOS 12, Ubuntu 22.04 | Windows 11, macOS 14, Ubuntu 22.04 |
| RAM | 4 GB | 16 GB |
| Storage | 500 MB free (for model + app) | 1 GB |
| GPU (Windows) | Any DX12-capable GPU | NVIDIA RTX or AMD RDNA series |
| GPU (macOS) | — (CoreML auto-enabled on Apple Silicon) | Apple M1 or later |

---

## Audio file constraints

- **Input sample rate:** any (resampled to 44.1 kHz automatically)
- **Maximum length:** 30 minutes (safety guard against corrupt file headers)
- **Supported formats:** WAV, MP3, AIFF, FLAC

Processing time for a 3-minute song is typically 3–15× real time depending on hardware. A 3-minute song on a mid-range GPU should complete in under 60 seconds.
