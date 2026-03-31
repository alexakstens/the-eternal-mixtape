# UX & Editor — Summary

Short overview of the UX design and the **Editor-side code structure** for The Eternal Mixtape.  
(Detailed interface design lives in internal docs; this file is the shareable summary.)

---

## UX model

- **Ordinary mode** (default): Simple, one-shot actions. UI calls Processor’s minimal API (no or few parameters).
- **Expertise mode** (hold **Cmd** / **Ctrl**): Same controls gain extra behaviour or open parameter panels. UI calls Processor’s extended/parameterised API.
- **Paths**: No hardcoded paths. All file/directory defaults come from **Config keys** via `getConfigPath(key)` / `setConfigPath(key, path)`. Keys: `imported_audio_dir`, `stem_model_file`, `stem_output_dir`, `analysis_cache_dir`, `project_save_dir`, `export_output_dir`, `user_config_file`.

---

## UI → Processor contract (high level)

| Area | Ordinary | Expertise |
|------|----------|-----------|
| **Top** | VU + Runtime from `getMasterLevels`, `getTransportPositionSeconds`, `getTransportTotalLengthSeconds` | Optional: preview per-track/stem spectrum |
| **Tracks A–D** | Waveforms from `getTrackSourceFile` / `getTrackStemFiles`; sections from `getAnalysisResult()` | Rename track, set source/stem file; drag section boundaries → `setSectionBoundary` |
| **SPLICE / STEM 1–2 / BPM / DENSITY** | `applySplice`, `setTrackStemIndices`, `setGlobalBPM`, `setSpliceDensity` | Splice panel (points, crossfade); stem file picker; `setAutoSpliceParameters`, per-track BPM |
| **Per-track faders + icons** | `setTrackGain`, `setTrackStemGain`, `setTrackStemMute` | + `setTrackPan`, `setTrackStemEffect` |
| **Transport** | `play`, `stop`, `setTransportPosition`, `setLoopEnabled` | `setLoopRegion`, `setQuantizedPlayback`, `setPlayFromCursor` |
| **AUTO SPLICE / REGENERATE / RANDOMIZE** | `applyAutoSplice`, `regenerateMix`, `randomizeMix` | Set params then same actions; REC with `startRecording(file, options)` |
| **Settings** | Edit all Config path keys via `getConfigPath` / `setConfigPath` | Same; optional advanced options |

Async jobs (analysis, stem separation, export): progress/error via Processor getters (e.g. `getStemProgress`, `getLastErrorMessage`); UI polls or listens and updates progress/error display.

---

## Editor-side code structure (this repo)

```
source/
├── PluginEditor.h          # Main editor; KeyListener for Cmd/Ctrl → isExpertiseMode, updateUIForMode()
├── PluginEditor.cpp        # Layout, controls, callbacks → only call Processor API + Config
├── (optional) UI/         # Optional subcomponents (if split out)
│   ├── TemMeterComponent   # VU / spectrum bar
│   ├── TemRuntimeLabel     # 004:37:12 display
│   ├── TemTrackStrip       # One strip: waveform + section overlay + faders + mic/guitar/speaker
│   ├── TemSplicePanel      # SPLICE + STEM 1/2 + BPM + DENSITY (mode-dependent layout)
│   ├── TemTransportBar    # Settings, Loop, transport buttons, AUTO SPLICE, REGENERATE, RANDOMIZE, REC
│   └── TemSettingsPanel   # List of Config keys + path editor (no hardcoded paths)
└── ...
```

- **PluginEditor** owns all UI; implements `KeyListener` (or equivalent) to set `bool isExpertiseMode` and call `updateUIForMode()` so each control switches label/visibility/callback.
- **File/dir choosers**: initial path = `processor.getConfigPath(key)`; on user choice call `processor.setConfigPath(key, file)`.
- **No direct engine or path strings**: every interaction goes through `PluginProcessor` methods; paths only via Config keys.

---

## Dependencies (Editor side)

- **PluginProcessor**: provides the API in the contract above (Config, transport, analysis, stem, tracks, splice, BPM, mix, actions).
- **JUCE**: `AudioProcessorEditor`, `KeyListener`, `Timer`/`AsyncUpdater` for polling, components for layout and drawing.
- **WaveformDisplay** (or equivalent): for track/stem waveforms; fed from Processor file paths or buffer API.
- **Shared types**: e.g. `AnalysisResult`, `Section` (from TemTypes or Processor headers) for section overlay and lists.

---

## Checklist for UX implementation

1. Modifier key sets `isExpertiseMode`; `updateUIForMode()` updates each control’s appearance and behaviour.
2. Every control has two behaviours (Ordinary / Expertise) and calls the corresponding Processor API.
3. All path defaults and Settings edits use Config keys only.
4. Progress and errors for analysis/stem/export come from Processor getters; UI refreshes on a timer or listener.
5. Processor API checklist (full method list) is in the internal UX design doc; this summary gives the high-level mapping and Editor layout.
