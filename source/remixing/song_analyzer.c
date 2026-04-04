#include "song_analyzer.h"
#include <aubio/aubio.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

float calculate_distance(const float* featA, const float* featB, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float diff = featA[i] - featB[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

void compute_ssm(const SongData* data) {
    unsigned int n = data->count;
    // Cast away const here because we're modifying the struct's member.
    // Ideally ssm shouldn't be computed in a const-context if it modifies the struct
    // but we can cast for now. Alternatively, remove const from SongData*.
    float* ssm = malloc(sizeof(float) * n * n);
    float max_dist = 0.0f;
    for (unsigned int i = 0; i < n; i++) {
        for (unsigned int j = 0; j < n; j++) {
            float dist = calculate_distance(&data->features[i*data->n_coeffs], &data->features[j*data->n_coeffs], data->n_coeffs);
            ssm[i * n + j] = dist;
            if (dist > max_dist) max_dist = dist;
        }
    }
    for (unsigned int i = 0; i < n * n; i++) ssm[i] = 1.0f - (ssm[i] / (max_dist + 1e-6f));
    ((SongData*)data)->ssm = ssm; // Cast away const
}

// Computes the Foote Novelty score using a Gaussian checkerboard kernel over the SSM
void compute_novelty_curve(SongData* data) {
    unsigned int n = data->count;
    data->novelty = calloc(n, sizeof(float));

    // Define the size of the checkerboard kernel (e.g., 4 beats in each direction)
    int L = 4;

    // Create the 2D kernel (L*2 x L*2)
    int kernel_size = L * 2;
    float* kernel = malloc(sizeof(float) * kernel_size * kernel_size);

    // Standard deviation for the Gaussian taper
    float sigma = (float)L / 2.0f;

    // Generate Gaussian-tapered checkerboard kernel
    for (int i = 0; i < kernel_size; i++) {
        for (int j = 0; j < kernel_size; j++) {
            // Determine sign (+1 for TL and BR quadrants, -1 for TR and BL)
            float sign = 1.0f;
            if ((i < L && j >= L) || (i >= L && j < L)) {
                sign = -1.0f;
            }

            // Calculate distance from center
            float center_offset = (float)L - 0.5f;
            float di = (float)i - center_offset;
            float dj = (float)j - center_offset;

            // Gaussian weight
            float gaussian = expf(-(di*di + dj*dj) / (2.0f * sigma * sigma));

            kernel[i * kernel_size + j] = sign * gaussian;
        }
    }

    // Convolve kernel along the main diagonal of the SSM
    float max_nov = 0.0f;
    float min_nov = 0.0f;

    for (unsigned int k = 0; k < n; k++) {
        float sum = 0.0f;

        // Only compute if we have enough space for the full kernel
        if (k >= L && k < n - L) {
            for (int i = 0; i < kernel_size; i++) {
                for (int j = 0; j < kernel_size; j++) {
                    int r = (int)k - L + i;
                    int c = (int)k - L + j;
                    sum += data->ssm[r * n + c] * kernel[i * kernel_size + j];
                }
            }
        }
        data->novelty[k] = sum;
        if (sum > max_nov) max_nov = sum;
        if (sum < min_nov) min_nov = sum;
    }

    // Normalize novelty curve to [0, 1]
    float range = max_nov - min_nov;
    if (range > 0.0f) {
        for (unsigned int k = 0; k < n; k++) {
            data->novelty[k] = (data->novelty[k] - min_nov) / range;
        }
    }

    free(kernel);
}


SongData analyze_song(const char* filename) {
    SongData results = {NULL, NULL, NULL, NULL, NULL, 0, 13, 0, 0, 0.0f, 60}; // Default key to C

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
        for (uint_t c = 0; c < results.n_coeffs; c++) results.features[b * results.n_coeffs + c] = (frame_count > 0) ? (mean_mfcc[c] / (float)frame_count) : 0;
    }

    compute_ssm(&results);
    compute_novelty_curve(&results); // Compute structural novelty

    // Placeholder for key detection
    // For now, we'll just use the default key.
    // A real implementation would use an algorithm like HPCP or a chroma-based method.
    printf("Key detection not yet implemented. Using default key for %s\n", filename);


    del_aubio_tempo(tempo); del_aubio_pvoc(pv); del_aubio_mfcc(mfcc);
    del_fvec(input); del_fvec(output); del_fvec(mfcc_out); del_cvec(fftgrain);
    return results;
}

void free_song_data(SongData* data) {
    if (data->beat_samples) free(data->beat_samples);
    if (data->features) free(data->features);
    if (data->ssm) free(data->ssm);
    if (data->novelty) free(data->novelty);
    if (data->audio_data) drwav_free(data->audio_data, NULL);
}