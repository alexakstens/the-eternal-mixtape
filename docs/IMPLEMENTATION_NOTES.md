# Implementation Notes — Editor & Processor

Record of the currently implemented Editor and Processor APIs and layout. For the corresponding UX design, see `docs/UX_SUMMARY.md`.

---

## 1. PluginProcessor — Implemented API

### Config (paths only via Config; no hardcoded paths)

- `getConfigPath(key)` / `setConfigPath(key, path)`
- Default paths are set in `initDefaultConfigPaths()`, based on `userDocumentsDirectory/EternalMixtape/...`
- Keys used: `imported_audio_dir`, `stem_model_file`, `stem_output_dir`, `analysis_cache_dir`, `project_save_dir`, `export_output_dir`, `user_config_file`

### Transport

- `play()` / `stop()` / `setTransportPosition(ratio)`
- `getTransportPositionSeconds()` / `getTransportTotalLengthSeconds()`
- `setLoopEnabled(bool)` / `setLoopRegion(startSec, endSec)`

### Meters / display

- `getMasterLevels()` — updated in `processBlock` from output peak; used by the UI VU

### Tracks (0..3 = A..D)

- `getTrackSourceFile(i)` / `setTrackSource(i, file)`
- `getTrackStemFiles(i)` / `setTrackStemFile(i, stemSlot, file)` / `setTrackStemIndices(i, i1, i2)`
- `getTrackName(i)` / `setTrackName(i, name)`
- **Mix**: `getTrackGain(i)` / `setTrackGain(i, gain)`, `setTrackStemGain`, `setTrackPan`, `setTrackStemMute`

### Splice / BPM / Density

- `applySplice(trackIndex)`
- `getSpliceDensity()` / `setSpliceDensity(density)`
- `getGlobalBPM()` / `setGlobalBPM(bpm)`

### Actions

- `applyAutoSplice()` / `regenerateMix()` / `randomizeMix()`
- `startRecording()` — uses default file under Config `export_output_dir`
- `startRecording(outputFile)` — Expertise mode: user can choose file

### Stem separation (progress/error for UI polling)

- `getLastStemFiles()` / `getLastStemOutputDir()`
- `getStemProgress()` / `getStemStatusMessage()` / `getStemErrorMessage()`
- `requestStemSeparation(inputFile, modelFile, outputDir)`
- Currently stubs; to be implemented by the engine

### Analysis (stub)

- `runAnalysisAsync(file)` / `getAnalysisProgress()` / `getLastAnalysisErrorMessage()`
- Currently stubs; no real analysis logic

---

## 2. PluginEditor — Implemented layout and behaviour

### Expertise mode

- A `Timer` (≈10 Hz) polls `ModifierKeys::getCurrentModifiers().isCommandDown()` (Mac Cmd / Win·Linux Ctrl)
- Updates `isExpertiseMode_` then calls `updateUIForMode()`
- **Ordinary**: SPLICE, REC, etc. are one-shot buttons
- **Expertise**: button labels change to e.g. "SPLICE (options)", "REC (choose file)"; REC opens a save dialog and calls `startRecording(file)`

### Layout (top to bottom)

| Area        | Content                                                                 | Processor API bound to                                                                 |
|------------|-------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
| **Top**    | Left: VU bar (drawn in `paint` from `getMasterLevels()`); right: runtime "m:ss / M:SS" | `getMasterLevels`, `getTransportPositionSeconds`, `getTransportTotalLengthSeconds`       |
| **4 tracks** | TRACK A–D labels + horizontal gain faders                              | `getTrackGain` / `setTrackGain`                                                        |
| **Splice** | SPLICE button, BPM slider, DENSITY slider                               | `applySplice` (all 4 tracks), `getGlobalBPM`/`setGlobalBPM`, `getSpliceDensity`/`setSpliceDensity` |
| **Transport** | Settings, Loop toggle, << / Play / >>, AUTO SPLICE, REGENERATE, RANDOMIZE, REC | `setConfigPath` (Settings picks dir), `setLoopEnabled`, `stop`/`setTransportPosition(0)`, `play`, `stop`, `applyAutoSplice`, `regenerateMix`, `randomizeMix`, `startRecording` |

### Contract with Processor

- All interaction goes only through the Processor methods above; paths only via Config keys `getConfigPath` / `setConfigPath`
- File/directory choosers: initial directory = `processorRef.getConfigPath(key)`; after user choice call `processorRef.setConfigPath(key, path)`

### Size and refresh

- Editor default size: 720×420
- Timer is used for: Expertise modifier detection, runtime and VU value updates

---

## 3. Future extensions

- **Optional UI components** (see `docs/UX_SUMMARY.md`): can be split into `source/UI/` as `TemMeterComponent`, `TemRuntimeLabel`, `TemTrackStrip`, `TemSplicePanel`, `TemTransportBar`, `TemSettingsPanel`
- **Processor backend**: real transport playback, analysis/stem implementation, waveform and section data (e.g. `getAnalysisResult()`), etc., wired in per the Processor checklist in the internal `docs/UX_INTERFACE_DESIGN.md`
- **Progress and errors**: async tasks (analysis, stem, export) can drive the UI via existing getters (e.g. `getStemProgress`, `getStemErrorMessage`) in the Timer or an AsyncUpdater
