#include "beat_tracker.h"
#include <aubio/aubio.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

float calculate_distance(float* featA, float* featB, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float diff = featA[i] - featB[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

void compute_ssm(BeatData* data) {
    unsigned int n = data->count;
    data->ssm = malloc(sizeof(float) * n * n);
    float max_dist = 0.0f;
    for (unsigned int i = 0; i < n; i++) {
        for (unsigned int j = 0; j < n; j++) {
            float dist = calculate_distance(&data->features[i*data->n_coeffs], &data->features[j*data->n_coeffs], data->n_coeffs);
            data->ssm[i * n + j] = dist;
            if (dist > max_dist) max_dist = dist;
        }
    }
    for (unsigned int i = 0; i < n * n; i++) data->ssm[i] = 1.0f - (data->ssm[i] / (max_dist + 1e-6f));
}

BeatData track_beats(const char* filename) {
    BeatData results = {NULL, NULL, NULL, NULL, 0, 13, 0, 0, 0.0f};

    drwav_uint64 total_frames;
    results.audio_data = drwav_open_file_and_read_pcm_frames_f32(filename, &results.channels, &results.sample_rate, &total_frames, NULL);
    if (!results.audio_data) return results;

    uint_t win_s = 1024, hop_s = 512;
    aubio_tempo_t* tempo = new_aubio_tempo("default", win_s, hop_s, results.sample_rate);
    fvec_t* input = new_fvec(hop_s);
    fvec_t* output = new_fvec(2);

    unsigned int capacity = 1000;
    results.beat_samples = malloc(sizeof(unsigned int) * capacity);

    for (drwav_uint64 i = 0; i < total_frames; i += hop_s) {
        for (uint_t j = 0; j < hop_s; j++)
            input->data[j] = (i + j < total_frames) ? results.audio_data[(i + j) * results.channels] : 0;
        aubio_tempo_do(tempo, input, output);
        if (output->data[0] != 0) {
            if (results.count >= capacity) {
                capacity *= 2;
                results.beat_samples = realloc(results.beat_samples, sizeof(unsigned int) * capacity);
            }
            results.beat_samples[results.count++] = (unsigned int)aubio_tempo_get_last(tempo);
        }
    }
    results.bpm = aubio_tempo_get_bpm(tempo);

    results.features = malloc(sizeof(float) * results.count * results.n_coeffs);
    aubio_pvoc_t* pv = new_aubio_pvoc(win_s, hop_s);
    cvec_t* fftgrain = new_cvec(win_s);
    aubio_mfcc_t* mfcc = new_aubio_mfcc(win_s, 40, results.n_coeffs, results.sample_rate);
    fvec_t* mfcc_out = new_fvec(results.n_coeffs);

    for (unsigned int b = 0; b < results.count; b++) {
        uint_t start = results.beat_samples[b];
        uint_t end = (b < results.count - 1) ? results.beat_samples[b+1] : (uint_t)total_frames;
        float mean_mfcc[13] = {0};
        int frame_count = 0;
        for (uint_t s = start; s < end; s += hop_s) {
            for (uint_t j = 0; j < hop_s; j++)
                input->data[j] = (s + j < total_frames) ? results.audio_data[(s + j) * results.channels] : 0;
            aubio_pvoc_do(pv, input, fftgrain);
            aubio_mfcc_do(mfcc, fftgrain, mfcc_out);
            for (uint_t c = 0; c < results.n_coeffs; c++) mean_mfcc[c] += mfcc_out->data[c];
            frame_count++;
        }
        for (uint_t c = 0; c < results.n_coeffs; c++) results.features[b * results.n_coeffs + c] = (frame_count > 0) ? (mean_mfcc[c] / frame_count) : 0;
    }

    compute_ssm(&results);

    del_aubio_tempo(tempo); del_aubio_pvoc(pv); del_aubio_mfcc(mfcc);
    del_fvec(input); del_fvec(output); del_fvec(mfcc_out); del_cvec(fftgrain);
    return results;
}

void free_beat_data(BeatData* data) {
    if (data->beat_samples) free(data->beat_samples);
    if (data->features) free(data->features);
    if (data->ssm) free(data->ssm);
    if (data->audio_data) drwav_free(data->audio_data, NULL);
}