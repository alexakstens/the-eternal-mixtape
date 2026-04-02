#include "warp.hpp"
#include <cmath>
#include <complex>
#include <fftw3.h>
#include <iostream>
#include <algorithm>

constexpr int WIN_SIZE = 4096;
constexpr int PAD_FACTOR = 4;
constexpr float PI = 3.14159265358979323846f;

static float m_to_f(int midi) {
    return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

std::vector<float> warp_audio(
    const std::vector<float>& input_data,
    unsigned int channels,
    unsigned int sample_rate,
    float source_bpm,
    int source_key,
    float target_bpm,
    int target_key)
{
    float keyRatio = m_to_f(target_key) / m_to_f(source_key);
    float tempoRatio = target_bpm / source_bpm;

    int lx = static_cast<int>(std::floor(static_cast<float>(WIN_SIZE) / keyRatio));
    int h_s = std::max(1, lx / 8);
    int h_a = std::max(1, static_cast<int>(static_cast<float>(h_s) * tempoRatio));

    int nFFT = WIN_SIZE * PAD_FACTOR;
    int nBins = nFFT / 2 + 1;

    std::vector<float> in_real(nFFT, 0.0f);
    std::vector<std::complex<float>> out_spec(nBins, {0.0f, 0.0f});

    // Cast the std::complex vector pointer to fftwf_complex pointer (they share the same memory layout [float real, float imag])
    fftwf_plan p_forward = fftwf_plan_dft_r2c_1d(nFFT, in_real.data(), reinterpret_cast<fftwf_complex*>(out_spec.data()), FFTW_ESTIMATE);
    fftwf_plan p_backward = fftwf_plan_dft_c2r_1d(nFFT, reinterpret_cast<fftwf_complex*>(out_spec.data()), in_real.data(), FFTW_ESTIMATE);

    std::vector<float> phi0(nBins * channels, 0.0f);
    std::vector<float> psi(nBins * channels, 0.0f);
    std::vector<float> mag(nBins, 0.0f);
    std::vector<float> phase(nBins, 0.0f);
    std::vector<float> omega(nBins, 0.0f);
    std::vector<float> new_psi(nBins, 0.0f);
    std::vector<float> chan_phase_diff(nBins, 0.0f);
    std::vector<int> nearest_peak(nBins, 0);
    std::vector<int> peaks(nBins, 0);

    unsigned int input_frames = input_data.size() / channels;
    unsigned int expected_out_len = static_cast<unsigned int>(static_cast<float>(input_frames) / tempoRatio) + WIN_SIZE * 4;

    std::vector<float> audio_out(expected_out_len * channels, 0.0f);
    std::vector<float> weight_out(expected_out_len, 0.0f);

    std::vector<float> hann_win(WIN_SIZE);
    for (int i = 0; i < WIN_SIZE; i++) {
        hann_win[i] = 0.5f * (1.0f - std::cos(2.0f * PI * static_cast<float>(i) / static_cast<float>(WIN_SIZE - 1)));
    }
    std::vector<float> hann_resamp(lx);
    for (int i = 0; i < lx; i++) {
        hann_resamp[i] = 0.5f * (1.0f - std::cos(2.0f * PI * static_cast<float>(i) / static_cast<float>(lx - 1)));
    }

    int pIn = 0, pOut = 0;
    float prev_energy = 0.0f;

    std::cout << "Warping Engine: [Ratio: " << (1.0f / tempoRatio) << "x speed, " << keyRatio << "x pitch]\n";

    while (pIn < static_cast<int>(input_frames) - WIN_SIZE) {
        if (pIn % (sample_rate * 5) < h_a) {
            std::cout << "Processing... " << (static_cast<float>(pIn) / static_cast<float>(input_frames) * 100.0f) << "%\r" << std::flush;
        }

        float curr_energy = 0.0f;
        for (int i = 0; i < WIN_SIZE; i++) {
            float s = input_data[(pIn + i) * channels];
            curr_energy += s * s;
        }
        bool is_transient = (curr_energy > prev_energy * 10.0f);
        prev_energy = curr_energy;

        for (int c = 0; c < static_cast<int>(channels); c++) {
            std::fill(in_real.begin(), in_real.end(), 0.0f);
            for (int i = 0; i < WIN_SIZE; i++) {
                in_real[i] = input_data[(pIn + i) * channels + c] * hann_win[i];
            }

            fftwf_execute(p_forward);

            for (int i = 0; i < nBins; i++) {
                mag[i] = std::abs(out_spec[i]);
                phase[i] = std::arg(out_spec[i]);
            }

            if (c == 0) {
                int peak_count = 0;
                for (int i = 1; i < nBins - 1; i++) {
                    if (mag[i] > mag[i-1] && mag[i] > mag[i+1]) {
                        peaks[peak_count++] = i;
                    }
                }

                if (peak_count > 0) {
                    int curr_p_idx = 0;
                    for (int i = 0; i < nBins; i++) {
                        while (curr_p_idx + 1 < peak_count &&
                               std::abs(i - peaks[curr_p_idx + 1]) < std::abs(i - peaks[curr_p_idx])) {
                            curr_p_idx++;
                        }
                        nearest_peak[i] = peaks[curr_p_idx];
                    }
                } else {
                    for (int i = 0; i < nBins; i++) nearest_peak[i] = i;
                }

                float h_ratio = static_cast<float>(h_s) / static_cast<float>(h_a);
                for (int i = 0; i < nBins; i++) {
                    float expected = 2.0f * PI * static_cast<float>(h_a) * static_cast<float>(i) / static_cast<float>(nFFT);
                    float delta = (phase[i] - phi0[i]) - expected;
                    delta = std::fmod(delta + PI, 2.0f * PI);
                    if (delta < 0.0f) delta += 2.0f * PI;
                    delta -= PI;
                    omega[i] = (expected + delta) * h_ratio * keyRatio;
                }

                if (is_transient) {
                    std::copy(phase.begin(), phase.end(), psi.begin());
                } else {
                    for (int i = 0; i < nBins; i++) {
                        int p = nearest_peak[i];
                        new_psi[i] = psi[p] + omega[p] + (phase[i] - phase[p]);
                    }
                    std::copy(new_psi.begin(), new_psi.end(), psi.begin());
                }
                std::copy(phase.begin(), phase.end(), chan_phase_diff.begin());
            } else {
                for (int i = 0; i < nBins; i++) {
                    float diff = phase[i] - chan_phase_diff[i];
                    psi[c * nBins + i] = psi[i] + diff;
                }
            }

            for (int i = 0; i < nBins; i++) {
                phi0[c * nBins + i] = phase[i];
            }

            for (int i = 0; i < nBins; i++) {
                if (keyRatio > 1.0f && i > (nFFT / (2.0f * keyRatio))) {
                    mag[i] = 0.0f;
                }
                out_spec[i] = std::polar(mag[i], psi[c * nBins + i]);
            }

            fftwf_execute(p_backward);

            float resamp_scale = static_cast<float>(WIN_SIZE) / static_cast<float>(lx);
            for (int i = 0; i < lx; i++) {
                float pos = static_cast<float>(i) * resamp_scale;
                int idx = static_cast<int>(pos);
                float frac = pos - static_cast<float>(idx);
                int idx1 = (idx + 1 < WIN_SIZE) ? idx + 1 : idx;

                float val = (in_real[idx] + frac * (in_real[idx1] - in_real[idx])) / static_cast<float>(nFFT);
                val *= hann_resamp[i];

                if (pOut + i < static_cast<int>(expected_out_len)) {
                    audio_out[(pOut + i) * channels + c] += val;
                    if (c == 0) weight_out[pOut + i] += hann_resamp[i] * hann_resamp[i];
                }
            }
        }
        pIn += h_a;
        pOut += h_s;
    }

    std::cout << "\nFinishing Output...\n";
    for (int i = 0; i < pOut; i++) {
        if (weight_out[i] > 1e-3f) {
            for (int c = 0; c < static_cast<int>(channels); c++) {
                audio_out[i * channels + c] /= weight_out[i];
            }
        }
    }

    fftwf_destroy_plan(p_forward);
    fftwf_destroy_plan(p_backward);

    audio_out.resize(pOut * channels); // Shrink to actual output size
    return audio_out;
}