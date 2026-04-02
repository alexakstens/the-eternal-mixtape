#include "song_analyzer.hpp"
#include <aubio/aubio.h>
#include <cmath>
#include <iostream>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

float calculate_distance(const float* featA, const float* featB, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float diff = featA[i] - featB[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

void SongData::compute_ssm() {
    unsigned int n = count;
    ssm.resize(n * n, 0.0f);
    float max_dist = 0.0f;
    for (unsigned int i = 0; i < n; i++) {
        for (unsigned int j = 0; j < n; j++) {
            float dist = calculate_distance(&features[i * n_coeffs], &features[j * n_coeffs], n_coeffs);
            ssm[i * n + j] = dist;
            if (dist > max_dist) max_dist = dist;
        }
    }
    for (unsigned int i = 0; i < n * n; i++) {
        ssm[i] = 1.0f - (ssm[i] / (max_dist + 1e-6f));
    }
}

void SongData::compute_novelty_curve() {
    unsigned int n = count;
    novelty.assign(n, 0.0f);

    int L = 4;
    int kernel_size = L * 2;
    std::vector<float> kernel(kernel_size * kernel_size, 0.0f);
    float sigma = static_cast<float>(L) / 2.0f;

    for (int i = 0; i < kernel_size; i++) {
        for (int j = 0; j < kernel_size; j++) {
            float sign = 1.0f;
            if ((i < L && j >= L) || (i >= L && j < L)) {
                sign = -1.0f;
            }
            float center_offset = static_cast<float>(L) - 0.5f;
            float di = static_cast<float>(i) - center_offset;
            float dj = static_cast<float>(j) - center_offset;
            float gaussian = std::exp(-(di * di + dj * dj) / (2.0f * sigma * sigma));
            kernel[i * kernel_size + j] = sign * gaussian;
        }
    }

    float max_nov = 0.0f;
    float min_nov = 0.0f;

    for (unsigned int k = 0; k < n; k++) {
        float sum = 0.0f;
        if (k >= static_cast<unsigned int>(L) && k < n - L) {
            for (int i = 0; i < kernel_size; i++) {
                for (int j = 0; j < kernel_size; j++) {
                    int r = static_cast<int>(k) - L + i;
                    int c = static_cast<int>(k) - L + j;
                    sum += ssm[r * n + c] * kernel[i * kernel_size + j];
                }
            }
        }
        novelty[k] = sum;
        if (sum > max_nov) max_nov = sum;
        if (sum < min_nov) min_nov = sum;
    }

    float range = max_nov - min_nov;
    if (range > 0.0f) {
        for (unsigned int k = 0; k < n; k++) {
            novelty[k] = (novelty[k] - min_nov) / range;
        }
    }
}

bool SongData::analyze_song(const std::string& filename) {
    drwav_uint64 total_frames;
    float* pSampleData = drwav_open_file_and_read_pcm_frames_f32(filename.c_str(), &channels, &sample_rate, &total_frames, nullptr);
    if (!pSampleData) return false;

    audio_data.assign(pSampleData, pSampleData + total_frames * channels);
    drwav_free(pSampleData, nullptr);

    uint_t win_s = 1024, hop_s = 512;
    aubio_tempo_t* tempo = new_aubio_tempo("default", win_s, hop_s, sample_rate);
    fvec_t* input = new_fvec(hop_s);
    fvec_t* output = new_fvec(2);

    beat_samples.clear();

    for (drwav_uint64 i = 0; i < total_frames; i += hop_s) {
        for (uint_t j = 0; j < hop_s; j++) {
            input->data[j] = (i + j < total_frames) ? audio_data[(i + j) * channels] : 0.0f;
        }
        aubio_tempo_do(tempo, input, output);
        if (output->data[0] != 0.0f) {
            beat_samples.push_back(static_cast<unsigned int>(aubio_tempo_get_last(tempo)));
        }
    }
    bpm = aubio_tempo_get_bpm(tempo);
    count = static_cast<unsigned int>(beat_samples.size());

    features.resize(count * n_coeffs, 0.0f);
    aubio_pvoc_t* pv = new_aubio_pvoc(win_s, hop_s);
    cvec_t* fftgrain = new_cvec(win_s);
    aubio_mfcc_t* mfcc = new_aubio_mfcc(win_s, 40, n_coeffs, sample_rate);
    fvec_t* mfcc_out = new_fvec(n_coeffs);

    for (unsigned int b = 0; b < count; b++) {
        uint_t start = beat_samples[b];
        uint_t end = (b < count - 1) ? beat_samples[b + 1] : static_cast<uint_t>(total_frames);
        std::vector<float> mean_mfcc(n_coeffs, 0.0f);
        int frame_count = 0;

        for (uint_t s = start; s < end; s += hop_s) {
            for (uint_t j = 0; j < hop_s; j++) {
                input->data[j] = (s + j < total_frames) ? audio_data[(s + j) * channels] : 0.0f;
            }
            aubio_pvoc_do(pv, input, fftgrain);
            aubio_mfcc_do(mfcc, fftgrain, mfcc_out);
            for (uint_t c = 0; c < n_coeffs; c++) mean_mfcc[c] += mfcc_out->data[c];
            frame_count++;
        }

        for (uint_t c = 0; c < n_coeffs; c++) {
            features[b * n_coeffs + c] = (frame_count > 0) ? (mean_mfcc[c] / static_cast<float>(frame_count)) : 0.0f;
        }
    }

    compute_ssm();
    compute_novelty_curve();

    std::cout << "Key detection not yet implemented. Using default key for " << filename << std::endl;

    del_aubio_tempo(tempo);
    del_aubio_pvoc(pv);
    del_aubio_mfcc(mfcc);
    del_fvec(input);
    del_fvec(output);
    del_fvec(mfcc_out);
    del_cvec(fftgrain);

    return true;
}