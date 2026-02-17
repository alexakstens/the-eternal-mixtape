#ifndef BEAT_TRACKER_H
#define BEAT_TRACKER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    unsigned int* beat_samples;
    float* features;
    float* ssm;
    float* audio_data;
    unsigned int count;
    unsigned int n_coeffs;
    unsigned int channels;
    unsigned int sample_rate;
    float bpm;
} BeatData;

BeatData track_beats(const char* filename);
void free_beat_data(BeatData* data);

#endif