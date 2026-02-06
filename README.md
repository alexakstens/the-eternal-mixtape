# The Eternal Mixtape
Final project for GT Audio Software Engineering MUSI6106, Spring 2026

Authors: Alex Akstens, Ryan Baker, Matias Cevallos, Evelyne Li, Marcus Parker

The Eternal Mixtape (TEM) is a audio plugin / app that gives an easy, accessible workflow for quickly creating remixes of songs.
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

[Flowchart](https://app.diagrams.net/#G1KT8jlMNfz_zdqMqgA0Q41kSAAmD18AeE#%7B%22pageId%22%3A%22O037zaB-uulJ0Mrsv9BE%22%7D)
## Algorithmic references 
<!-- which reference do you base your algorithmic implementations on? -->

**Stem Separation**
- There are several challenges to implementing a machine learning (ML) model into a real-time audio C++ program. Some audio-specific tools like [RTNeural](https://github.com/jatinchowdhury18/RTNeural) are designed to implement neural networks for real-time inference, but lack the breadth to include support for complex models that are common in source seperation.
- While ongoing projects have aims of [real-time stem seperation](https://www.gpu.audio/), that is not within scope for TEM. 
- A popular stem separation model is [demucs](https://github.com/facebookresearch/demucs), which originally came out of Meta's audio research group.
- A [fork of this model](https://github.com/sevagh/demucs.onnx) has been designed for C++ implementation by using the [ONNX](https://onnx.ai/) runtime.


**Audio Content Analysis**
- TODO

**Tempo Warping**
-  A simple time stretch will match section lengths between tracks at the cost of repitching (which will be corrected for in the next processing block)

**Pitch Warping**
- A FFT-based pitch warping algorithm will be implemented after the time warping, TBD on real time or offline

**Track Selection**
- User Interface or basic RNG-based selection

**Audio Output Stream** 
- JUCE implementation for audio flow

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




