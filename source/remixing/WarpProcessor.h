#pragma once

// C++ wrapper around Marcus's phase-vocoder warp engine (warp.h / warp.c).
// warp.c requires FFTW3. Enable with -DENABLE_FFTW3_WARP=ON at configure time.
//
// Algorithm: phase-locked phase vocoder with stereo-link, transient detection,
// and brick-wall anti-aliasing. Handles simultaneous pitch shifting + tempo change.

#if ENABLE_FFTW3_WARP

#include <cstdlib>
#include <vector>

extern "C"
{
#include "warp.h"
}

namespace remixing
{

class WarpProcessor
{
public:
    struct Params
    {
        float        sourceBpm   = 120.0f;
        float        targetBpm   = 120.0f;
        int          sourceKey   = 60;    // MIDI note (C4 = 60)
        int          targetKey   = 60;
        unsigned int channels    = 2;
        unsigned int sampleRate  = 44100;
    };

    // Runs the warp engine on interleaved float audio.
    // Returns a malloc'd interleaved buffer; caller must free() it.
    static float* process (float*       inputData,
                           unsigned int inputFrames,
                           const Params& p,
                           unsigned int& outFrameCount)
    {
        return warp_audio (inputData, inputFrames,
                           p.channels, p.sampleRate,
                           p.sourceBpm, p.sourceKey,
                           p.targetBpm, p.targetKey,
                           &outFrameCount);
    }
};

} // namespace remixing

#else
// Stub when FFTW3 is unavailable — keeps dependent headers compiling.
namespace remixing
{
struct WarpProcessor
{
    struct Params
    {
        float        sourceBpm  = 120.0f, targetBpm = 120.0f;
        int          sourceKey  = 60,     targetKey = 60;
        unsigned int channels   = 2,      sampleRate = 44100;
    };
};
} // namespace remixing
#endif // ENABLE_FFTW3_WARP
