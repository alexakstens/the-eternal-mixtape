#pragma once
#include <vector>
#include <string>

class SongData {
public:
    SongData() = default;

    // Loads the file and runs all analysis (beats, MFCCs, SSM, novelty)
    bool analyze_song(const std::string& filename);

    // Getters for external access
    unsigned int get_count() const { return count; }
    unsigned int get_n_coeffs() const { return n_coeffs; }
    unsigned int get_channels() const { return channels; }
    unsigned int get_sample_rate() const { return sample_rate; }
    float get_bpm() const { return bpm; }
    int get_key() const { return key; }

    const std::vector<unsigned int>& get_beat_samples() const { return beat_samples; }
    const std::vector<float>& get_features() const { return features; }
    const std::vector<float>& get_ssm() const { return ssm; }
    const std::vector<float>& get_novelty() const { return novelty; }
    const std::vector<float>& get_audio_data() const { return audio_data; }

    // Setter for updating audio data (e.g. after warping)
    void set_audio_data(const std::vector<float>& new_data) { audio_data = new_data; }

private:
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

    void compute_ssm();
    void compute_novelty_curve();
};