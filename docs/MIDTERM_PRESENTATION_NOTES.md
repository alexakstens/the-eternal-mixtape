# Midterm Presentation Notes (10 min)

This document organizes our midterm content based on the course requirements and rubric.

## Slide-ready Version (Minimal, PPT-friendly)

Use this version directly on slides: short lines, few bullets, clear ownership.

### One-slide Final (Evelyne, detailed but concise)

- Implemented dual-mode interaction (Ordinary vs Expertise) with JUCE command modifier detection: Cmd on macOS / Ctrl on Windows-Linux, polled in a timer for live mode switching.
- In Ordinary mode, controls run one-shot actions; in Expertise mode, advanced behavior is exposed (e.g., `REC` opens file-save chooser and calls `startRecording(file)`, and button context changes such as `SPLICE (options)` / `REC (choose file)`).
- Implemented core editor control flow: runtime + VU updates, 4-track controls, transport actions, and splice entry with BPM/density parameter wiring.
- Unified teammate-facing features at UX level through one Processor/backend contract (UI calls Processor APIs, not module internals directly).
- Unified source/CMake directory planning and config-path based file workflow (`getConfigPath` / `setConfigPath`) to avoid hardcoded UI paths.
- Current splice status: base splice path and BPM/density wiring are in place; next step is deeper algorithm-side binding and JUCE graphical control polish (custom knobs/control-surface styling).

### Slide 1 - Goals & Scope
- Build an algorithmic mashup tool for structural remixing.
- Input: song(s) -> analysis + stem workflow -> remix output.
- Standalone + plugin workflow (JUCE-based).
- Target users: low-to-mid experience producers.
- Focus: creative control, not fully automated polished mastering.

### Slide 2 - System Modules (Who owns what)
- Ryan: project management + stem splitting direction (Demucs/ONNX).
- Alex: pitch/tempo warp methods and translation.
- Marcus: audio content analysis and structural features.
- Evelyne: UX + editor integration and interaction model.

### Slide 3 - My Progress (Evelyne)
- Implemented Ordinary vs Expertise interaction modes.
- Built UI -> Processor API contract for core controls.
- Implemented editor layout: meter/runtime, tracks, splice, transport.
- Standardized config-path workflow (no hardcoded paths).
- Added timer-driven UI updates for runtime and VU feedback.
- Unified teammate-facing features at the UX control layer through one Processor/backend interface.

### Slide 4 - Remaining Tasks (My Focus)
- Resolve and stabilize branch integration conflicts.
- Replace plain controls with graphical JUCE knobs.
- Bind controls to parameters for reliable UI/audio sync.
- Tighten UX linkage to splice, tempo, and stem control flow.

### Slide 5 - JUCE Graphical Controls (What this means)
- Knobs: JUCE rotary sliders + custom LookAndFeel drawing.
- Sliders, labels, and meter visuals aligned to the UI mockup.
- Optional advanced controls can be added later if needed.
- Goal: clearer visual feedback and faster creative workflow.

### Slide 6 - Timeline to May 5
- Apr 1-7: merge stabilization + UX behavior lock.
- Apr 8-14: graphical knobs + visual control polish.
- Apr 15-21: parameter binding + processor integration.
- Apr 22-28: full team integration + bug fixes.
- Apr 29-May 5: polish, testing, packaging, final presentation.

### Slide 7 - Key Takeaway
- Core UX workflow is already implemented and documented.
- Next milestone is graphical JUCE controls + deep UI/audio binding.
- Plan is scheduled weekly through final submission.

## 1) Goals and Scope

### Project goal
- Build an algorithmic mashup tool that takes a song (or songs), analyzes structure, and creates remix outputs with stem-level control.

### Scope
- Standalone app + plugin workflow (JUCE-based).
- Target users: low-to-mid experience producers.
- Focus: creative structural remixing, not fully automated "radio-ready" mastering.

## 2) Team Responsibilities (Module Ownership)

- **Ryan (Project Management + Stem Splitting)**
  - Framework and template direction (JUCE + Pamplejuce).
  - PR and issue coordination.
  - Demucs/ONNX stem splitting direction.

- **Alex (Pitch & Tempo Warp)**
  - Time/pitch warp strategy and translation from prototype algorithms.
  - Quality considerations (phase/aliasing behavior and constraints).

- **Marcus (Audio Content Analysis)**
  - Offline feature extraction and structure analysis pipeline.
  - Beat/tempo/chroma/self-similarity features for structural jump decisions.

- **Evelyne (UX + Editor Integration)**
  - Interaction model and UI/Processor contract design.
  - Editor behavior and mode switching (Ordinary vs Expertise).
  - UX documentation and implementation alignment.

## 3) What is Already Implemented (Evelyne focus, from docs)

Based on `docs/IMPLEMENTATION_NOTES.md` and `docs/UX_SUMMARY.md`:

- Implemented dual interaction modes:
  - **Ordinary mode** for quick one-shot actions.
  - **Expertise mode** (Cmd/Ctrl hold) for advanced behavior.
- Implemented core Editor layout and control flow:
  - Top runtime + meter updates.
  - Track controls (A-D), splice section, transport section.
- Implemented UI -> Processor method contract:
  - UI actions call processor APIs for transport, splice, mix, and actions.
- Implemented config-path workflow:
  - No hardcoded paths; use config keys via `getConfigPath/setConfigPath`.
- Implemented periodic UI refresh with timer polling:
  - runtime, mode state, and meter updates.

## 4) Remaining Tasks (with JUCE graphical controls focus)

### Immediate
- Finish conflict-free integration of `fix/ux_async` with `dev`.
- Ensure stable async file chooser and mode behaviors after merge.

### UX/GUI next steps (Evelyne)
- Upgrade plain controls to **graphical JUCE controls**:
  - Use JUCE rotary sliders ("knobs") with custom LookAndFeel rendering.
- Add parameter binding for reliable UI/audio sync:
  - Use APVTS attachments / parameter attachments where needed.
- Keep UI tightly coupled to actual remix workflow:
  - Connect graphical controls to splice density, tempo, stem balance, and related UX actions.

### System-level remaining work (team)
- Replace analysis and stem stubs with backend implementations.
- Integrate waveform/section views with analysis output.
- Testing, bug fixing, and deployment packaging.

## 5) Timeline to Final Submission (May 5)

- **Apr 1 - Apr 7**
  - Complete conflict resolution and stabilize integrated branch.
  - Lock UX interaction behavior for ordinary/expertise modes.

- **Apr 8 - Apr 14**
  - Implement custom JUCE knob styles (graphical controls).
  - Align sliders/buttons/meters with the visual UI design.

- **Apr 15 - Apr 21**
  - Bind graphical controls to processor parameters.
  - Integrate with transport/splice actions and validate UX flow.

- **Apr 22 - Apr 28**
  - Team integration week:
    - analysis + stem + UI behavior alignment
    - bug fixing and performance checks

- **Apr 29 - May 5**
  - Final polish, testing, packaging, and presentation/demo readiness.

## 6) Simple English Speaking Script (Evelyne section)

You can read this in about 60-90 seconds:

---

Hi everyone, I am Evelyne, and I focus on UX and editor integration for The Eternal Mixtape.

So far, I implemented two interaction modes. In Ordinary mode, users can run quick one-click actions. In Expertise mode, users get more advanced behavior, for example recording with a chosen output file instead of only the default path.

I implemented the core editor flow: runtime and VU display updates, four-track controls, transport actions, and splice controls with BPM and density wiring. This means the UI is already connected to real backend calls, not just visual placeholders.

I also handled integration across teammates' work by routing UI interactions through one Processor/backend contract. This keeps the control flow consistent even when features come from different modules.

For file workflows, I standardized config-path handling with `getConfigPath` and `setConfigPath`, so we avoid hardcoded paths and keep behavior consistent across machines.

To keep the UI responsive, the editor refreshes key status values continuously, such as runtime and meter feedback, so users can see updates while interacting.

For the next phase, I will focus on graphical JUCE controls: custom knob styling, better visual polish for sliders/meters, and tighter parameter binding between UI controls and backend actions.

My goal is to make remix actions like splice, tempo, and stem control feel clear, stable, and immediate for low-to-mid experience users.

---

## 7) Rubric Alignment Checklist

- **Presentation Clarity (25):**
  - Use module ownership + implemented/remaining split.
- **Student Effort (20):**
  - Show concrete implemented items and next deliverables.
- **Comprehensive Timeline (25):**
  - Keep weekly milestones through May 5.
- **Overall Team Understanding (20):**
  - Explain how UX links to analysis, stem, and warp modules.
- **Delivery (10):**
  - Keep wording simple, direct, and paced.
