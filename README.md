# The Eternal Mixtape
Final project for GT Audio Software Engineering MUSI6106, Spring 2026

Authors: Alex Akstens, Ryan Baker, Matias Cevallos, Evelyne Li, Marcus Parker

The Eternal Mixtape (TEM) is a audio plugin / app that gives an easy, accessible workflow for quickly creating remixes of songs.
## Motivation
<!-- problem to be solved, why is there a need for this -->
Creating a remix often requires using musical information such as key signature, tempo and chord changes. 
TEM aims to bridge the gap for anybody to quickly draft a mixtape idea.

This project is inspired by [The Eternal Jukebox]((https://eternalboxmirror.xyz/jukebox_index.html)) and [Mixboard](https://www.nime.org/proceedings/2023/nime2023_69.pdf). The former offers a simplistic user interface with very little interaction, splicing up songs based on audio similarity. Our application will extend on this core concept while offering the user more agency to rearrange the song in their own way. The latter offers a DAW-like interface in which the user can choose the stems of different songs to create unique mashups in a single user-friendly environment.
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

## Algorithmic references 
<!-- which reference do you base your algorithmic implementations on? -->

## Roles
### Alex - Audio DSP implementation
- Alex will focus on the audio effects needed to enhance the mixtapes (e.g. pitch shifting)
### Ryan - Audio ML implementation and project manager
- Ryan will focus on implementing machine learning models to seperate track stems as well as managing the project (timeline, code rigor, testing and validation)
### Matias - User Interface
- Matias will focus on the visual design of the plugin's look and feel
### Evelyne - User Experience
- Evelyne will focus on the interactive flow of the plugin and overall UX
### Marcus - Audio Analysis implementation
- Marcus will focus on implementing the offline audio feature extraction and probabalistic composition

## (Bonus) If you can create a timeline and make use of the Github Projects functionality


