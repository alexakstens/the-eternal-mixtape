#pragma once

/**
 * C++ port of Marcus's song_analyzer (localJukebox/song_analyzer.c).
 * No aubio dependency — uses JUCE DSP for FFT and std C++17 throughout.
 *
 * Algorithms implemented (matching the original C exactly):
 *   1. Beat detection   — spectral flux onset strength + adaptive peak picker
 *   2. MFCC per beat    — Hann-windowed FFT → 40-bin mel filterbank → log → DCT-II (13 coeffs)
 *   3. SSM              — pairwise L2 distance, normalized to [0,1] similarity
 *   4. Foote novelty    — Gaussian checkerboard kernel convolved along SSM diagonal,
 *                         normalized to [0,1]  (Foote 2000)
 *
 * Usage:
 *   auto result = remixing::SongAnalyzer::analyze(monoSamples, sampleRate);
 *   float bpm           = result.bpm;
 *   auto& noveltyPeaks  = result.structureBoundaries;  // indices into beatSamples
 */

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace remixing {

class SongAnalyzer
{
public:
    struct Result
    {
        float                     bpm        = 0.0f;
        unsigned int              sampleRate = 0;
        std::vector<unsigned int> beatSamples;    // sample index of each beat
        std::vector<float>        features;       // flat [beatCount × nCoeffs], row-major
        std::vector<float>        ssm;            // flat [beatCount × beatCount] similarity
        std::vector<float>        novelty;        // [beatCount] Foote novelty, normalized [0,1]
        std::vector<unsigned int> structureBoundaries; // beat indices where novelty peaks
        unsigned int              beatCount  = 0;
        unsigned int              nCoeffs    = kNumMfcc;
    };

    // Analyze a mono audio buffer. Returns the full SongData equivalent.
    static Result analyze (const std::vector<float>& mono, int sampleRate);

    // Lighter call — BPM only (skips MFCC/SSM/novelty).
    static float  estimateBPM (const std::vector<float>& mono, int sampleRate);

private:
    // ── Constants (match song_analyzer.c defaults) ────────────────────────────
    static constexpr int   kFFTOrder  = 10;          // 1024-point FFT
    static constexpr int   kWinSize   = 1 << kFFTOrder;
    static constexpr int   kHopSize   = 512;
    static constexpr int   kNumMel    = 40;
    static constexpr int   kNumMfcc   = 13;
    static constexpr int   kNoveltyL  = 4;           // checkerboard half-size
    static constexpr float kMinHz     = 80.0f;
    static constexpr float kMaxHz     = 8000.0f;
    static constexpr float kPi        = 3.14159265358979f;

    // ── Mel helpers ───────────────────────────────────────────────────────────
    static float hzToMel (float hz)  { return 2595.0f * std::log10 (1.0f + hz / 700.0f); }
    static float melToHz (float mel) { return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f); }

    // Build triangular mel filterbank [kNumMel × (kWinSize/2+1)]
    static std::vector<std::vector<float>> buildMelFilterbank (int sr)
    {
        const int   numBins = kWinSize / 2 + 1;
        const float minMel  = hzToMel (kMinHz);
        const float maxMel  = hzToMel (std::min ((float)(sr / 2), kMaxHz));

        std::vector<float> melPts (kNumMel + 2);
        for (int i = 0; i < (int) melPts.size(); ++i)
            melPts[i] = minMel + (maxMel - minMel) * i / (kNumMel + 1);

        std::vector<int> bins (kNumMel + 2);
        for (int i = 0; i < (int) bins.size(); ++i)
            bins[i] = std::clamp ((int) std::round (melToHz (melPts[i]) * kWinSize / sr), 0, numBins - 1);

        std::vector<std::vector<float>> fb (kNumMel, std::vector<float> (numBins, 0.0f));
        for (int m = 0; m < kNumMel; ++m)
        {
            for (int k = bins[m]; k <= bins[m + 1]; ++k)
                if (bins[m + 1] > bins[m])
                    fb[m][k] = (float)(k - bins[m]) / (bins[m + 1] - bins[m]);
            for (int k = bins[m + 1]; k <= bins[m + 2]; ++k)
                if (bins[m + 2] > bins[m + 1])
                    fb[m][k] = (float)(bins[m + 2] - k) / (bins[m + 2] - bins[m + 1]);
        }
        return fb;
    }

    // Type-II DCT: N inputs → numOut coefficients
    static std::vector<float> dct (const std::vector<float>& x, int numOut)
    {
        const int N = (int) x.size();
        std::vector<float> out (numOut, 0.0f);
        for (int k = 0; k < numOut; ++k)
            for (int n = 0; n < N; ++n)
                out[k] += x[n] * std::cos (kPi * k * (n + 0.5f) / N);
        return out;
    }

    // ── Onset detection ───────────────────────────────────────────────────────
    // Half-wave rectified spectral flux, one value per hop
    static std::vector<float> spectralFlux (const std::vector<float>& audio)
    {
        juce::dsp::FFT fft (kFFTOrder);
        const int numBins = kWinSize / 2 + 1;
        const int numHops = std::max (0, (int) audio.size() - kWinSize) / kHopSize + 1;

        std::vector<float> win (kWinSize);
        for (int i = 0; i < kWinSize; ++i)
            win[i] = 0.5f * (1.0f - std::cos (2.0f * kPi * i / (kWinSize - 1)));

        std::vector<float> flux (numHops, 0.0f);
        std::vector<float> prevMag (numBins, 0.0f);
        std::vector<float> buf (2 * kWinSize, 0.0f);

        for (int hop = 0; hop < numHops; ++hop)
        {
            const int start = hop * kHopSize;
            for (int i = 0; i < kWinSize; ++i)
                buf[i] = (start + i < (int) audio.size()) ? audio[start + i] * win[i] : 0.0f;
            std::fill (buf.begin() + kWinSize, buf.end(), 0.0f);
            fft.performRealOnlyForwardTransform (buf.data());

            float f = 0.0f;
            for (int k = 0; k < numBins; ++k)
            {
                const float re  = buf[2 * k], im = buf[2 * k + 1];
                const float mag = std::sqrt (re * re + im * im);
                const float diff = mag - prevMag[k];
                if (diff > 0.0f) f += diff;   // half-wave rectify
                prevMag[k] = mag;
            }
            flux[hop] = f;
        }
        return flux;
    }

    // Adaptive-threshold peak picker — mirrors aubio_tempo behaviour
    static std::vector<int> pickPeaks (const std::vector<float>& onset, int minDistHops)
    {
        const int N = (int) onset.size();
        if (N < 3) return {};

        const int hw = std::min (20, N / 4);
        std::vector<float> thr (N);
        for (int i = 0; i < N; ++i)
        {
            int lo = std::max (0, i - hw), hi = std::min (N, i + hw + 1);
            float sum = 0.0f;
            for (int j = lo; j < hi; ++j) sum += onset[j];
            thr[i] = 0.5f * sum / (hi - lo);
        }

        std::vector<int> peaks;
        for (int i = 1; i < N - 1; ++i)
            if (onset[i] > thr[i] && onset[i] >= onset[i - 1] && onset[i] >= onset[i + 1])
                if (peaks.empty() || i - peaks.back() >= minDistHops)
                    peaks.push_back (i);
        return peaks;
    }

    // BPM from hop-indexed beats via median IOI
    static float bpmFromBeats (const std::vector<int>& beatHops, int sr)
    {
        if ((int) beatHops.size() < 2) return 120.0f;
        std::vector<float> iois;
        iois.reserve (beatHops.size() - 1);
        for (int i = 1; i < (int) beatHops.size(); ++i)
            iois.push_back ((float)(beatHops[i] - beatHops[i - 1]) * kHopSize / (float) sr);
        std::sort (iois.begin(), iois.end());
        const float med = iois[iois.size() / 2];
        return med > 0.001f ? 60.0f / med : 120.0f;
    }

    // ── MFCC ─────────────────────────────────────────────────────────────────
    // Compute mean MFCC vector for one beat segment [startSample, endSample)
    static std::vector<float> mfccForSegment (const std::vector<float>& audio,
                                               int startSample, int endSample,
                                               const std::vector<std::vector<float>>& melFB)
    {
        if (endSample <= startSample + kWinSize)
            return std::vector<float> (kNumMfcc, 0.0f);

        juce::dsp::FFT fft (kFFTOrder);
        const int numBins = kWinSize / 2 + 1;

        std::vector<float> win (kWinSize);
        for (int i = 0; i < kWinSize; ++i)
            win[i] = 0.5f * (1.0f - std::cos (2.0f * kPi * i / (kWinSize - 1)));

        std::vector<float> melAcc (kNumMel, 0.0f);
        int frameCount = 0;
        std::vector<float> buf (2 * kWinSize, 0.0f);

        for (int s = startSample; s + kWinSize <= endSample; s += kHopSize)
        {
            for (int i = 0; i < kWinSize; ++i)
                buf[i] = (s + i < (int) audio.size()) ? audio[s + i] * win[i] : 0.0f;
            std::fill (buf.begin() + kWinSize, buf.end(), 0.0f);
            fft.performRealOnlyForwardTransform (buf.data());

            for (int m = 0; m < kNumMel; ++m)
            {
                float e = 0.0f;
                for (int k = 0; k < numBins; ++k)
                {
                    const float re = buf[2 * k], im = buf[2 * k + 1];
                    e += melFB[m][k] * (re * re + im * im);
                }
                melAcc[m] += std::log (e + 1e-10f);
            }
            ++frameCount;
        }

        if (frameCount == 0) return std::vector<float> (kNumMfcc, 0.0f);
        for (auto& v : melAcc) v /= frameCount;
        return dct (melAcc, kNumMfcc);
    }

    // ── SSM ─────────────────────────────────────────────────────────────────��
    // Flat [N×N] pairwise L2 distance → normalized to similarity [0,1]
    static std::vector<float> buildSSM (const std::vector<float>& features, int N, int nCoeffs)
    {
        std::vector<float> ssm (N * N, 0.0f);
        float maxDist = 0.0f;

        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                float d = 0.0f;
                for (int k = 0; k < nCoeffs; ++k)
                {
                    float diff = features[i * nCoeffs + k] - features[j * nCoeffs + k];
                    d += diff * diff;
                }
                d = std::sqrt (d);
                ssm[i * N + j] = d;
                maxDist = std::max (maxDist, d);
            }

        for (auto& v : ssm)
            v = 1.0f - v / (maxDist + 1e-6f);
        return ssm;
    }

    // ── Foote novelty (Foote 2000) ────────────────────────────────────────────
    // Direct port of compute_novelty_curve() from song_analyzer.c
    static std::vector<float> footeNovelty (const std::vector<float>& ssm, int N)
    {
        std::vector<float> novelty (N, 0.0f);
        const int L          = kNoveltyL;
        const int kernelSize = L * 2;
        const float sigma    = (float) L / 2.0f;

        // Build Gaussian-tapered checkerboard kernel
        std::vector<float> kernel (kernelSize * kernelSize);
        for (int i = 0; i < kernelSize; ++i)
            for (int j = 0; j < kernelSize; ++j)
            {
                float sign = ((i < L) != (j < L)) ? -1.0f : 1.0f; // XOR quadrant
                float di = (float) i - ((float) L - 0.5f);
                float dj = (float) j - ((float) L - 0.5f);
                float g  = std::exp (-(di * di + dj * dj) / (2.0f * sigma * sigma));
                kernel[i * kernelSize + j] = sign * g;
            }

        // Convolve kernel along SSM main diagonal
        float maxNov = 0.0f, minNov = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            float sum = 0.0f;
            if (k >= L && k < N - L)
                for (int i = 0; i < kernelSize; ++i)
                    for (int j = 0; j < kernelSize; ++j)
                    {
                        int r = k - L + i;
                        int c = k - L + j;
                        sum += ssm[r * N + c] * kernel[i * kernelSize + j];
                    }
            novelty[k] = sum;
            maxNov = std::max (maxNov, sum);
            minNov = std::min (minNov, sum);
        }

        // Normalize to [0, 1]
        const float range = maxNov - minNov;
        if (range > 0.0f)
            for (auto& v : novelty) v = (v - minNov) / range;

        return novelty;
    }

    // Pick peaks in novelty curve → structure boundary beat indices
    static std::vector<unsigned int> noveltyPeaks (const std::vector<float>& novelty,
                                                    float threshold = 0.5f)
    {
        std::vector<unsigned int> boundaries;
        const int N = (int) novelty.size();
        for (int i = 1; i < N - 1; ++i)
            if (novelty[i] > threshold
                && novelty[i] >= novelty[i - 1]
                && novelty[i] >= novelty[i + 1])
                boundaries.push_back ((unsigned int) i);
        return boundaries;
    }
};

// ── Implementations ───────────────────────────────────────────────────────────

inline float SongAnalyzer::estimateBPM (const std::vector<float>& mono, int sampleRate)
{
    if (mono.empty()) return 120.0f;
    const int minDist = std::max (1, (int)(0.25f * sampleRate / kHopSize));
    return bpmFromBeats (pickPeaks (spectralFlux (mono), minDist), sampleRate);
}

inline SongAnalyzer::Result SongAnalyzer::analyze (const std::vector<float>& mono, int sampleRate)
{
    Result r;
    r.sampleRate = (unsigned int) sampleRate;
    if (mono.empty()) { r.bpm = 120.0f; return r; }

    // 1. Beat detection
    const int minDist = std::max (1, (int)(0.25f * sampleRate / kHopSize));
    auto beatHops     = pickPeaks (spectralFlux (mono), minDist);
    r.bpm             = bpmFromBeats (beatHops, sampleRate);

    if (beatHops.empty()) return r;

    for (int h : beatHops)
        r.beatSamples.push_back ((unsigned int)(h * kHopSize));
    r.beatCount = (unsigned int) r.beatSamples.size();

    // 2. MFCC per beat segment
    auto melFB = buildMelFilterbank (sampleRate);
    r.features.resize (r.beatCount * kNumMfcc, 0.0f);
    for (unsigned int b = 0; b < r.beatCount; ++b)
    {
        const int start = (int) r.beatSamples[b];
        const int end   = (b + 1 < r.beatCount) ? (int) r.beatSamples[b + 1] : (int) mono.size();
        auto mfcc = mfccForSegment (mono, start, end, melFB);
        for (int k = 0; k < kNumMfcc; ++k)
            r.features[b * kNumMfcc + k] = mfcc[k];
    }

    // 3. Self-similarity matrix
    r.ssm = buildSSM (r.features, (int) r.beatCount, kNumMfcc);

    // 4. Foote novelty curve + structure boundaries
    r.novelty            = footeNovelty (r.ssm, (int) r.beatCount);
    r.structureBoundaries = noveltyPeaks (r.novelty);

    return r;
}

} // namespace remixing
