#ifndef WARP_H
#define WARP_H

#include "dr_wav.h"

// The main warping function
// Returns a pointer to the warped audio buffer
float* warp_audio(
    float* input_data, 
    unsigned int input_frames, 
    unsigned int channels,
    unsigned int sample_rate,
    float source_bpm,
    int source_key, 
    float target_bpm, 
    int target_key,
    unsigned int* out_frames_count
);

#endif