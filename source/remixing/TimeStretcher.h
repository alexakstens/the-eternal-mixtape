#pragma once

// C++ wrapper around jatinchowdhury18/time-stretcher.
// Uses juce::dsp::FFT (TIMESTRETCH_USING_JUCE=1), so no FFTW3 dependency.
//
// Algorithm: HPSS-based phase vocoder (Juillerat & Hirsbrunner, ICASSP 2017).
// Separate harmonic/percussive paths with different window sizes preserve
// transient sharpness while maintaining harmonic phase coherence.
//
// Pitch shifting is NOT handled here — use WarpProcessor for combined pitch+tempo.

#include <vector>
#include "../../modules/time-stretcher/src/stretch.h"

namespace remixing
{

class TimeStretcher
{
public:
    using Params = time_stretch::STRETCH_PARAMS;

    // Stretches a multi-channel audio signal by params.stretch_factor.
    // x[channel][sample] — returns same layout.
    static std::vector<std::vector<float>> process (const std::vector<std::vector<float>>& x,
                                                    Params& params)
    {
        return time_stretch::time_stretch (x, params);
    }

    // Build a ready-to-use Params for the most common case.
    static Params makeParams (float stretchFactor, float sampleRate = 44100.0f)
    {
        Params p;
        p.stretch_factor = stretchFactor;
        p.sample_rate    = sampleRate;
        return p;
    }
};

} // namespace remixing
