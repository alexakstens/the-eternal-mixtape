#pragma once

// C++ wrapper around Marcus's beat tracker (beat_tracker.h / beat_tracker.c).
// beat_tracker.c requires aubio. Enable with -DENABLE_AUBIO_BEAT=ON at configure time.
//
// Produces: BPM, per-beat sample positions, and MFCC-based self-similarity matrix.

#if ENABLE_AUBIO_BEAT

#include <vector>

extern "C"
{
#include "beat_tracker.h"
}

namespace remixing
{

class BeatAnalyzer
{
public:
    struct Result
    {
        float                     bpm        = 0.0f;
        unsigned int              sampleRate = 0;
        std::vector<unsigned int> beatSamples;   // sample index of each detected beat
        // ssm: count x count self-similarity matrix (row-major), available via rawData()
        std::vector<float>        ssm;
        unsigned int              beatCount  = 0;
    };

    // Analyzes a file on disk. Returns beat grid + SSM.
    static Result analyze (const char* filePath)
    {
        BeatData data = track_beats (filePath);

        Result r;
        r.bpm        = data.bpm;
        r.sampleRate = data.sample_rate;
        r.beatCount  = data.count;
        r.beatSamples.assign (data.beat_samples, data.beat_samples + data.count);

        if (data.ssm != nullptr)
            r.ssm.assign (data.ssm, data.ssm + data.count * data.count);

        free_beat_data (&data);
        return r;
    }
};

} // namespace remixing

#else
// Stub when aubio is unavailable — keeps dependent headers compiling.
namespace remixing
{
struct BeatAnalyzer
{
    struct Result
    {
        float                     bpm        = 0.0f;
        unsigned int              sampleRate = 0;
        std::vector<unsigned int> beatSamples;
        std::vector<float>        ssm;
        unsigned int              beatCount  = 0;
    };
};
} // namespace remixing
#endif // ENABLE_AUBIO_BEAT
