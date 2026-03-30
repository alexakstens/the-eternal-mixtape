# The Eternal Mixtape
Final project for GT Audio Software Engineering MUSI6106, Spring 2026

Authors: Alex Akstens, Ryan Baker, Matias Cevallos, Evelyne Li, Marcus Parker

The Eternal Mixtape (TEM) is a audio plugin / app that gives an easy, accessible workflow for quickly creating remixes of songs.

Made using the [Pamplejuce template](https://github.com/sudara/pamplejuce)
## Motivation
<!-- problem to be solved, why is there a need for this -->
Creating a remix often requires using musical information such as key signature, tempo and chord changes. 
TEM aims to bridge the gap for anybody to quickly draft a mixtape idea.

This project is inspired by [The Eternal Jukebox](https://eternalboxmirror.xyz/jukebox_index.html) and [Mixboard](https://www.nime.org/proceedings/2023/nime2023_69.pdf). The former offers a simplistic user interface with very little interaction, splicing up songs based on audio similarity. Our application will extend on this core concept while offering the user more agency to rearrange the song in their own way. The latter offers a DAW-like interface in which the user can choose the stems of different songs to create unique mashups in a single user-friendly environment.
Our application approaches a different problem area by focusing on providing this functionality in a DAW for a more permament remix production tool that can take advantage of other DAW inclusions like stem-splitting and file management.

## Applications
<!-- use cases, target users, context, environment -->
Our application is geared towards amateur to pro-am users who are minimally familiar with music production. 
The user is able to use the application on its own, without a DAW.
For the more advanced use case, users can load the application as a VST plugin in a Digital Audio Workstation (DAW), leveraging localized audio files and built-in stem-splitting features.

The users load in a set of songs of their choice. TEM processes the songs, extracting audio information such as key, bpm, and cuts them up into different sections automatically.
## Functionality 
<!-- from user point of view and how it differentiates from similar products -->
The interaction flows in this order:
    1) the user uploads audio files of different songs into the application
    2) the application parses the songs using offline audio analysis
    3) the user is presented with a low-complexity user interface that allows them to rearrange the song as they please

## User Experience
The user experience is designed to be **intuitive, engaging, and empowering**. The core of the application is a visual, block-based timeline where users can creatively arrange their mixtape.

- **Simplified Workflow**: Users import songs, which are automatically analyzed for key, tempo, and structure, and then presented as colored blocks.
- **Drag-and-Drop Canvas**: The main interaction involves dragging these musical blocks from a library onto a remix timeline to build a new sequence.
- **Intelligent Assistance**: The application automatically handles complex tasks like tempo and pitch matching, allowing the user to focus on creativity.
- **Immediate Auditory Feedback**: Changes made on the timeline are heard in real-time, creating a fluid and responsive experience.

## Plans for implementation:
<!-- ### flow chart, processing blocks, needed components, potential need for 3rd party libs -->
**Stem Separation**
- The focus of the ML implementation, the algorithm should extract main stems of interest from the chosen song. Each stem will take information from the main audio file in the audio content analysis process. 

**Audio Content Analysis** 

- From input audio files, the offline processing will analyze each audio file and extract key and tempo and identify bar ends. 

**Tempo Warping** 

- To match tempo between songs for transitions and mashups

**Pitch Warping** 

- To match pitch between songs for transitions and mashups

**Track Selection**

- User Interface will allow the user to select tracks to mashup, sections to sequence, and levels/effects on each track.

**Audio Output Stream** 
- Summed outputs from each track, warped and repitched to a common tempo/key with optional effects mixed down to a stereo stream

[Flowchart](https://drive.google.com/file/d/1KT8jlMNfz_zdqMqgA0Q41kSAAmD18AeE/view?usp=sharing)

<img width="768" height="265" alt="Screenshot 2026-02-06 at 18 47 19" src="https://github.com/user-attachments/assets/4fe498a8-33f7-4ead-8c59-0dd0ae070bdf" />



## Algorithmic references 
<!-- which reference do you base your algorithmic implementations on? -->

**Stem Separation**
- There are several challenges to implementing a machine learning (ML) model into a real-time audio C++ program. Some audio-specific tools like [RTNeural](https://github.com/jatinchowdhury18/RTNeural) are designed to implement neural networks for real-time inference, but lack the breadth to include support for complex models that are common in source seperation.
- While ongoing projects have aims of [real-time stem seperation](https://www.gpu.audio/), that is not within scope for TEM. 
- A popular stem separation model is [demucs](https://github.com/facebookresearch/demucs), which originally came out of Meta's audio research group.
- A [fork of this model](https://github.com/sevagh/demucs.onnx) has been designed for C++ implementation by using the [ONNX](https://onnx.ai/) runtime.


**Audio Content Analysis** 
- To provide structural awareness, the analysis engine implements a multi-pass offline processing pipeline using the Aubio (https://aubio.org/) C library and custom linear algebra routines.
- **Beat Tracking & Temporal Grid:** A temporal grid is established using a spectral-flux-based onset detection and a dynamic programming beat-tracking algorithm (`aubio_tempo`). This serves as the "atomic unit" for all subsequent structural jumps.
- **Spectral Fingerprinting (MFCC):** For each detected beat, the system extracts a 13-coefficient Mel-Frequency Cepstral Coefficient (MFCC) vector. These vectors are aggregated via a mean-pooling strategy over the duration of the beat to create a stable "timbral fingerprint" for that specific musical moment.
- **Self-Similarity Matrix (SSM):** A global similarity map is constructed by calculating the pairwise Euclidean distance between all beat-level MFCC vectors. This matrix is normalized to a [0.1] similarity scale, where $1.0$ indicates an identical timbral match.
- **Structural Segmentation (Foote Novelty):** 
    - The system implements a **Foote Novelty** algorithm by convolving the SSM with a Gaussian-tapered checkerboard kernel. 
    - This identifies high-novelty "boundary" points (transitions between verse/chorus) by detecting changes in the local self-similarity texture.
- **Probabilistic Transition Mapping:** The analysis identifies "jump points"—regions where non-sequential beats exhibit similarity above a defined threshold (typically $>0.90$). These points are stored in a transition graph used by the playback engine to create seamless, "infinite" remixes. This logic will be extended to remix two songs for transitioning to the next song in a playlist.

**Tempo Warping**
-  A simple time stretch will match section lengths between tracks at the cost of repitching (which will be corrected for in the next processing block), done by resampling the original audio file. This will cause pitch warping (lower pitch when slowed, higher pitch whern sped-up), but as long as the relative change can be tracked, it can be corrected for in pitch warping. The benefit of this approach is computational speed - resampling is much faster than other tempo-warping algorithms. If pitch warping is done offline, tempo warping can be included in the same implementation by playing FFT windows for longer/shorter.

**Pitch Warping**
- A FFT-based pitch warping algorithm will be implemented after the time warping, TBD on real time or offline. This task will likely be the bulk of DSP programming tasks, but will exponentially increase the duration of unique mixes that can be made. This is a high-value, high-effort component of the system. The DAFX textbook outlines in detail a block-by-block (FFT) approach that will be used in this implementation. This approach involves reassigning FFT blocks to different pitches while preserving the time domain content of the signal. 
- Resources:
    -  https://www.fftw.org/ - C Library for highly optimized FFT
    -  https://people.ece.cornell.edu/land/courses/ece4760/FinalProjects/f2014/mjk339mm889/mjk339mm889/index.html - High-Level Outline of FFT Warping
    -  DAFX 2nd Edition - 7.4.4 Pitch Shifting
    -  https://www.isca-archive.org/interspeech_2017/lenarczyk17_interspeech.pdf - Alternate approach to pitch shifting using phase vocoder

**Track Selection**
- User Interface or basic RNG-based selection

**Audio Output Stream** 
- [JUCE's audio flow interface](https://forum.juce.com/t/audio-flow-for-dsp/12198) is a highly documented and easily implementable real-time audio stream manager for JUCE plugins. 

## Roles
### Alex - Audio DSP implementation
- Alex will focus on the audio effects needed to enhance the mixtapes (e.g. pitch shifting)
### Ryan - Audio ML implementation and project manager
- Ryan will focus on implementing an ML model to seperate track stems as well as managing the project (timeline, code rigor, testing and validation)
### Matias - User Interface
- Matias will focus on the visual design of the plugin's look and feel
### Evelyne - User Experience
- Evelyne will focus on the interactive flow of the plugin and overall UX
### Marcus - Audio Analysis implementation
- Marcus will focus on implementing the offline audio feature extraction and probabalistic composition


## User Experience & Interaction Design (Evelyne Li)

The Eternal Mixtape is designed as a **section-based structural remixing tool**.
All user interactions operate on musically meaningful units rather than raw samples, prioritizing coherence, speed, and creative control.

The system follows a fixed interaction pipeline: **import → offline analysis → structural rearrangement → playback/export**.

### Core Interaction Flow

1. **Audio Import**
   Users load a single audio track as the source material.

2. **Offline Analysis**
   The system performs analysis to extract global tempo, a beat-aligned grid, and suggested section boundaries.

3. **Section-Based Editing**
   The analyzed track is segmented into reusable sections. Users interact with these sections rather than the raw waveform.

4. **Arrangement & Playback**
   Sections can be reordered, duplicated, or removed on a timeline, with immediate playback feedback.

5. **Export**
   The rearranged structure can be rendered or exported as a new audio file.

### Interaction Modes

To balance accessibility and control, The Eternal Mixtape provides two interaction modes that share the same analysis and audio engine.

#### Standard Mode

Standard Mode is designed for fast, intuitive remixing with minimal user decisions. This mode prioritizes immediacy and musical coherence, allowing users to focus on structural experimentation.

* Section boundaries are generated automatically
* Editing is constrained to beat-aligned operations
* Crossfades are enabled by default
* Only essential controls are exposed

#### Professional Mode

Professional Mode exposes deeper control while maintaining strict interaction constraints. All edits remain non-destructive and beat-aligned. The system does not allow free-form sample editing or unrestricted parameter manipulation.

* Manual adjustment of section boundaries
* Section-level parameters such as gain, crossfade length, and repetition
* Timeline-based structural control
* Optional, constrained pitch or time manipulation

### Interaction Principles

* User interactions operate on section-level abstractions, while audio processing is performed at the sample level internally
* Sections are defined by musically meaningful boundaries, such as beat-aligned cut points and structurally coherent segments derived from analysis
* Automatic analysis provides structural suggestions rather than fixed decisions
* Immediate auditory feedback follows every structural change

### Interface Overview

```
┌───────────────────────────────────────────────┐
│                MAIN DISPLAY                   │
│        (Waveform + Sections + Timeline)       │
├───────────────┬───────────────┬───────────────┤
│  LEFT CONTROL │   TRANSPORT   │ RIGHT CONTROL │
│   (Source)    │   / GLOBAL    │  (Section)    │
└───────────────┴───────────────┴───────────────┘
```
