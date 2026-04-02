#include "warp.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <string.h>

#define WIN_SIZE 4096
#define PAD_FACTOR 4
#define PI 3.14159265358979323846f

static float m_to_f(int midi) {
    return 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
}

float* warp_audio(const float* input_data, unsigned int input_frames, unsigned int channels,
                  unsigned int sample_rate, float source_bpm, int source_key, 
                  float target_bpm, int target_key, unsigned int* out_frames_count) {

    float keyRatio = m_to_f(target_key) / m_to_f(source_key);
    float tempoRatio = target_bpm / source_bpm;

    int lx = (int)floorf((float)WIN_SIZE / keyRatio);
    int h_s = lx / 8;
    if (h_s < 1) h_s = 1;
    int h_a = (int)((float)h_s * tempoRatio);
    if (h_a < 1) h_a = 1;

    int nFFT = WIN_SIZE * PAD_FACTOR;
    int nBins = nFFT / 2 + 1;

    // --- Pre-allocating all memory to avoid malloc in the loop ---
    float *in_real = fftwf_alloc_real(nFFT);
    fftwf_complex *out_spec = fftwf_alloc_complex(nBins);
    fftwf_plan p_forward = fftwf_plan_dft_r2c_1d(nFFT, in_real, out_spec, FFTW_ESTIMATE);
    fftwf_plan p_backward = fftwf_plan_dft_c2r_1d(nFFT, out_spec, in_real, FFTW_ESTIMATE);

    float *phi0 = calloc(nBins * channels, sizeof(float));
    float *psi  = calloc(nBins * channels, sizeof(float));
    float *mag = malloc(nBins * sizeof(float));
    float *phase = malloc(nBins * sizeof(float));
    float *omega = malloc(nBins * sizeof(float));
    float *new_psi = malloc(nBins * sizeof(float));
    float *chan_phase_diff = malloc(nBins * sizeof(float));
    int *nearest_peak = malloc(nBins * sizeof(int));
    int *peaks = malloc(nBins * sizeof(int));

    unsigned int expected_out_len = (unsigned int)((float)input_frames / tempoRatio) + WIN_SIZE * 4;
    float* audio_out = calloc(expected_out_len * channels, sizeof(float));
    float* weight_out = calloc(expected_out_len, sizeof(float));

    float hann_win[WIN_SIZE];
    for (int i = 0; i < WIN_SIZE; i++) hann_win[i] = 0.5f * (1.0f - cosf(2.0f * PI * (float)i / (float)(WIN_SIZE - 1)));

    // Allocate hann_resamp dynamically since lx is calculated at runtime (VLA)
    float *hann_resamp = malloc(lx * sizeof(float));
    for (int i = 0; i < lx; i++) hann_resamp[i] = 0.5f * (1.0f - cosf(2.0f * PI * (float)i / (float)(lx - 1)));

    int pIn = 0, pOut = 0;
    float prev_energy = 0;

    printf("Warping Engine: [Ratio: %.2fx speed, %.2fx pitch]\n", 1.0f/tempoRatio, keyRatio);

    while (pIn < (int)input_frames - WIN_SIZE) {
        // Simple progress indicator
        if (pIn % (sample_rate * 5) < h_a) printf("Processing... %.1f%%\r", (float)pIn/(float)input_frames * 100.0f);

        float curr_energy = 0;
        for (int i = 0; i < WIN_SIZE; i++) {
            float s = input_data[(pIn + i) * channels]; // Check Left channel for energy
            curr_energy += s * s;
        }
        int is_transient = (curr_energy > prev_energy * 10.0f);
        prev_energy = curr_energy;

        for (int c = 0; c < (int)channels; c++) {
            memset(in_real, 0, sizeof(float) * nFFT);
            for (int i = 0; i < WIN_SIZE; i++) in_real[i] = input_data[(pIn + i) * channels + c] * hann_win[i];

            fftwf_execute(p_forward);

            for (int i = 0; i < nBins; i++) {
                mag[i] = cabsf(out_spec[i]);
                phase[i] = cargf(out_spec[i]);
            }

            if (c == 0) {
                // OPTIMIZED PEAK FINDING
                int peak_count = 0;
                for (int i = 1; i < nBins - 1; i++) {
                    if (mag[i] > mag[i-1] && mag[i] > mag[i+1]) peaks[peak_count++] = i;
                }

                // OPTIMIZED PEAK MAPPING (Single Pass)
                if (peak_count > 0) {
                    int curr_p_idx = 0;
                    for (int i = 0; i < nBins; i++) {
                        while (curr_p_idx + 1 < peak_count &&
                               abs(i - peaks[curr_p_idx + 1]) < abs(i - peaks[curr_p_idx])) {
                            curr_p_idx++;
                        }
                        nearest_peak[i] = peaks[curr_p_idx];
                    }
                } else {
                    for (int i = 0; i < nBins; i++) nearest_peak[i] = i;
                }

                float h_ratio = (float)h_s / (float)h_a;
                for (int i = 0; i < nBins; i++) {
                    float expected = 2.0f * PI * (float)h_a * (float)i / (float)nFFT;
                    float delta = (phase[i] - phi0[i]) - expected;
                    delta = fmodf(delta + PI, 2 * PI);
                    if (delta < 0) delta += 2 * PI;
                    delta -= PI;
                    omega[i] = (expected + delta) * h_ratio * keyRatio;
                }

                if (is_transient) {
                    memcpy(psi, phase, nBins * sizeof(float));
                } else {
                    for (int i = 0; i < nBins; i++) {
                        int p = nearest_peak[i];
                        new_psi[i] = psi[p] + omega[p] + (phase[i] - phase[p]);
                    }
                    memcpy(psi, new_psi, nBins * sizeof(float));
                }
                for(int i=0; i<nBins; i++) chan_phase_diff[i] = phase[i];
            } else {
                for (int i = 0; i < nBins; i++) {
                    float diff = phase[i] - chan_phase_diff[i];
                    psi[c * nBins + i] = psi[i] + diff;
                }
            }

            for (int i = 0; i < nBins; i++) phi0[c * nBins + i] = phase[i];

            // Spectral Processing
            for (int i = 0; i < nBins; i++) {
                if (keyRatio > 1.0f && i > (nFFT / (2.0f * keyRatio))) mag[i] = 0; // Anti-alias
                out_spec[i] = mag[i] * (cosf(psi[c * nBins + i]) + I * sinf(psi[c * nBins + i]));
            }

            fftwf_execute(p_backward);

            // Linear Resampling & OLA
            float resamp_scale = (float)WIN_SIZE / (float)lx;
            for (int i = 0; i < lx; i++) {
                float pos = (float)i * resamp_scale;
                int idx = (int)pos;
                float frac = pos - (float)idx;
                int idx1 = (idx + 1 < WIN_SIZE) ? idx + 1 : idx;

                float val = (in_real[idx] + frac * (in_real[idx1] - in_real[idx])) / (float)nFFT;
                val *= hann_resamp[i];

                if (pOut + i < (int)expected_out_len) {
                    audio_out[(pOut + i) * channels + c] += val;
                    if (c == 0) weight_out[pOut + i] += hann_resamp[i] * hann_resamp[i];
                }
            }
        }
        pIn += h_a;
        pOut += h_s;
    }

    printf("\nFinishing Output...\n");
    for (int i = 0; i < pOut; i++) {
        if (weight_out[i] > 1e-3f) {
            for (int c = 0; c < (int)channels; c++) audio_out[i * channels + c] /= weight_out[i];
        }
    }

    fftwf_destroy_plan(p_forward); fftwf_destroy_plan(p_backward);
    fftwf_free(in_real); fftwf_free(out_spec);
    free(mag); free(phase); free(nearest_peak); free(omega); free(peaks);
    free(phi0); free(psi); free(weight_out); free(new_psi); free(chan_phase_diff);
    free(hann_resamp);

    *out_frames_count = pOut;
    return audio_out;
}