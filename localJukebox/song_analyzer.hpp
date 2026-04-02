#pragma once
#include <vector>
#include <string>

class SongData {
public:
    std::vector<unsigned int> beat_samples;
    std::vector<float> features;
    std::vector<float> ssm;
    std::vector<float> novelty;
    std::vector<float> audio_data;

    unsigned int count = 0;
    unsigned int n_coeffs = 13;
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    float bpm = 0.0f;
    int key = 60; // Default key to C

    SongData() = default;

    // Loads the file and runs all analysis (beats, MFCCs, SSM, novelty)
    bool analyze_song(const std::string& filename);

private:
    void compute_ssm();
    void compute_novelty_curve();
};