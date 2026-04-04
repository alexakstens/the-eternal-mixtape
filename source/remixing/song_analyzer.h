#ifndef SONG_ANALYZER_H
#define SONG_ANALYZER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    unsigned int* beat_samples;
    float* features; // e.g., MFCCs or Chroma for each beat
    float* ssm;      // Self-similarity matrix
    float* novelty;  // Foote Novelty score for each beat
    float* audio_data;
    unsigned int count;      // Number of beats
    unsigned int n_coeffs;   // Number of feature coefficients (e.g., 13 for MFCCs)
    unsigned int channels;
    unsigned int sample_rate;
    float bpm;
    int key; // Detected key of the song (e.g., MIDI note number for root)
} SongData;

SongData analyze_song(const char* filename);
void free_song_data(SongData* data);

#endif