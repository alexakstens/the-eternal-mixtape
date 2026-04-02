#pragma once
#include <vector>

std::vector<float> warp_audio(
    const std::vector<float>& input_data,
    unsigned int channels,
    unsigned int sample_rate,
    float source_bpm,
    int source_key,
    float target_bpm,
    int target_key
);