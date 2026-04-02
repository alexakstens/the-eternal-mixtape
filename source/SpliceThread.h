#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "remixing/SongAnalyzer.h"
#include <atomic>
#include <cmath>
#include <numeric>
#include <random>
#include <algorithm>

/**
 * Beat-aware remix engine.
 *
 * Algorithm:
 *   1. Read all available stems
 *   2. Auto-detect BPM + beat grid from the drums stem (SongAnalyzer)
 *   3. Slice all stems into per-beat chunks at the detected grid
 *   4. Optionally shuffle the chunk order (density controls shuffle intensity)
 *   5. Resize each chunk to the target beat length (linear-interp resampler)
 *      unless skipWarp is true (concatenate at original length)
 *   6. Apply short crossfades at chunk boundaries to avoid clicks
 *   7. Mix all stems and write splice_output.wav
 *
 * The result is a tempo-corrected, re-arranged mix that actually sounds different
 * from the original — not just the stems summed back together.
 */
class SpliceThread : public juce::Thread
{
public:
    enum class Status { Idle, Reading, Analyzing, Chopping, Writing, Complete, Error };

    SpliceThread() : juce::Thread ("SpliceProcessing") {}
    ~SpliceThread() override { stopThread (10000); }

    void configure (const juce::File& stemsDirectory,
                    double sourceBPM,      // 0.0 = auto-detect
                    double targetBPM,
                    bool   skipTimeWarp = false,
                    float  density      = 0.5f)
    {
        stemsDir = stemsDirectory;
        srcBPM   = sourceBPM;
        tgtBPM   = targetBPM;
        skipWarp = skipTimeWarp;
        density_ = juce::jlimit (0.0f, 1.0f, density);
    }

    juce::File getOutputFile() const
    {
        return stemsDir.getChildFile ("splice_output.wav");
    }

    //==========================================================================
    void run() override
    {
        progress.store (0.0f);
        errorMessage = {};
        status.store (Status::Reading);

        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();

        // ── 1. Read stems ────────────────────────────────────────────────────
        const juce::StringArray stemNames { "drums.wav", "bass.wav",
                                            "other.wav", "vocals.wav" };
        using StemChannels = std::vector<std::vector<float>>;
        std::vector<StemChannels> allStems;
        int sampleRate = 44100;
        int totalSamples = 0;

        for (auto& name : stemNames)
        {
            if (threadShouldExit()) return;

            auto f = stemsDir.getChildFile (name);
            if (! f.existsAsFile()) continue;

            std::unique_ptr<juce::AudioFormatReader> reader (fmtMgr.createReaderFor (f));
            if (! reader) continue;

            sampleRate   = (int) reader->sampleRate;
            int numCh    = (int) reader->numChannels;
            int numSmp   = (int) reader->lengthInSamples;
            totalSamples = std::max (totalSamples, numSmp);

            juce::AudioBuffer<float> buf (numCh, numSmp);
            reader->read (&buf, 0, numSmp, 0, true, true);

            StemChannels channels;
            for (int c = 0; c < numCh; ++c)
                channels.push_back ({ buf.getReadPointer (c),
                                      buf.getReadPointer (c) + numSmp });
            // Ensure stereo
            if (channels.size() == 1)
                channels.push_back (channels[0]);
            allStems.push_back (std::move (channels));
        }

        if (allStems.empty())
        {
            errorMessage = "No stem files found — run separation first.";
            status.store (Status::Error);
            return;
        }

        progress.store (0.15f);

        // ── 2. Detect beats from drums stem (index 0, channel 0) ─────────────
        status.store (Status::Analyzing);
        const auto& drumsChannel = allStems[0][0];

        detectedSrcBPM = (srcBPM > 0.0) ? srcBPM : 120.0;
        std::vector<unsigned int> beatSamples;

        if (srcBPM <= 0.0)
        {
            auto analysis = remixing::SongAnalyzer::analyze (drumsChannel, sampleRate);
            if (analysis.bpm > 40.0 && analysis.bpm < 300.0)
                detectedSrcBPM = (double) analysis.bpm;
            beatSamples = analysis.beatSamples;
        }

        // Fall back to evenly-spaced synthetic grid if detection missed
        if (beatSamples.size() < 2)
        {
            int beatLen = (int) (sampleRate * 60.0 / detectedSrcBPM);
            for (int s = 0; s + beatLen <= totalSamples; s += beatLen)
                beatSamples.push_back ((unsigned int) s);
        }

        const int numBeats = (int) beatSamples.size();
        progress.store (0.30f);

        // ── 3. Compute target beat length and shuffle permutation ─────────────
        status.store (Status::Chopping);
        const int targetBeatLen = (int) (sampleRate * 60.0 / tgtBPM);
        const int xfadeLen      = std::min (256, targetBeatLen / 8);

        // Build shuffle permutation — density 0 = original order, 1 = random
        std::vector<int> perm (numBeats);
        std::iota (perm.begin(), perm.end(), 0);

        if (density_ > 0.0f && numBeats > 1)
        {
            std::mt19937 rng (42); // deterministic seed for repeatability
            std::vector<int> shuffled (perm);
            std::shuffle (shuffled.begin(), shuffled.end(), rng);

            std::uniform_real_distribution<float> coin (0.0f, 1.0f);
            for (int i = 0; i < numBeats; ++i)
                if (coin (rng) < density_)
                    perm[i] = shuffled[i];
        }

        // ── 4. Chop, resize, mix ──────────────────────────────────────────────
        const int outLen   = numBeats * (skipWarp ? 0 : targetBeatLen); // rough estimate
        const int reserve  = skipWarp ? totalSamples + 4096 : numBeats * targetBeatLen + 4096;

        std::vector<std::vector<float>> mixed (2, std::vector<float> (reserve, 0.0f));
        int writePos = 0;

        for (int outBeat = 0; outBeat < numBeats; ++outBeat)
        {
            if (threadShouldExit()) return;

            const int srcBeat  = perm[outBeat];
            const int srcStart = (int) beatSamples[srcBeat];
            const int srcEnd   = (srcBeat + 1 < numBeats)
                                     ? (int) beatSamples[srcBeat + 1]
                                     : totalSamples;
            const int srcLen   = srcEnd - srcStart;
            if (srcLen <= 0) continue;

            const int chunkLen = skipWarp ? srcLen : targetBeatLen;

            // Ensure output buffer is large enough
            if (writePos + chunkLen > (int) mixed[0].size())
            {
                mixed[0].resize (writePos + chunkLen + 44100, 0.0f);
                mixed[1].resize (mixed[0].size(), 0.0f);
            }

            for (auto& stemCh : allStems)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const auto& src = stemCh[c < (int) stemCh.size() ? c : 0];

                    if (skipWarp)
                    {
                        // No resampling — copy as-is
                        for (int i = 0; i < chunkLen; ++i)
                        {
                            int si = srcStart + i;
                            mixed[c][writePos + i] += (si < (int) src.size()) ? src[si] : 0.0f;
                        }
                    }
                    else
                    {
                        // Linear-interpolation resample to targetBeatLen
                        const double ratio = (double)(srcLen - 1) / std::max (1, targetBeatLen - 1);
                        for (int i = 0; i < targetBeatLen; ++i)
                        {
                            const double pos = srcStart + i * ratio;
                            const int    lo  = (int) pos;
                            const int    hi  = std::min (lo + 1, (int) src.size() - 1);
                            const float  t   = (float)(pos - lo);
                            const float  lo_v = (lo < (int) src.size()) ? src[lo] : 0.0f;
                            const float  hi_v = (hi < (int) src.size()) ? src[hi] : 0.0f;
                            mixed[c][writePos + i] += lo_v * (1.0f - t) + hi_v * t;
                        }
                    }
                }
            }

            // Crossfade at the start of each chunk to avoid clicks
            if (outBeat > 0 && xfadeLen > 0 && writePos >= xfadeLen)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < xfadeLen; ++i)
                    {
                        float t = (float) i / xfadeLen;
                        mixed[c][writePos + i] *= t; // fade in current chunk
                    }
            }

            writePos += chunkLen;
            progress.store (0.30f + 0.55f * (float)(outBeat + 1) / numBeats);
        }

        // Trim to actual written length and normalize
        for (auto& ch : mixed)
            ch.resize (writePos);

        float peak = 0.0f;
        for (auto& ch : mixed)
            for (auto s : ch) peak = std::max (peak, std::abs (s));
        if (peak > 1e-6f)
            for (auto& ch : mixed)
                for (auto& s : ch) s /= peak;

        if (threadShouldExit()) return;

        // ── 5. Write output ───────────────────────────────────────────────────
        status.store (Status::Writing);

        auto outputFile = getOutputFile();
        outputFile.deleteFile();

        auto outStream = outputFile.createOutputStream();
        if (! outStream) { errorMessage = "Could not create output file."; status.store (Status::Error); return; }

        juce::WavAudioFormat wavFmt;
        auto writer = std::unique_ptr<juce::AudioFormatWriter> (
            wavFmt.createWriterFor (outStream.release(), sampleRate, 2, 16, {}, 0));

        if (! writer) { errorMessage = "Could not create WAV writer."; status.store (Status::Error); return; }

        const int outSamples = (int) mixed[0].size();
        juce::AudioBuffer<float> outBuf (2, outSamples);
        for (int c = 0; c < 2; ++c)
            outBuf.copyFrom (c, 0, mixed[c].data(), outSamples);

        writer->writeFromAudioSampleBuffer (outBuf, 0, outSamples);

        progress.store (1.0f);
        status.store (Status::Complete);
    }

    //==========================================================================
    float          getProgress()    const { return progress.load(); }
    Status         getStatus()      const { return status.load(); }
    juce::String   getErrorMessage() const { return errorMessage; }
    double         getDetectedBPM() const { return detectedSrcBPM; }

    juce::String getStatusMessage() const
    {
        switch (status.load())
        {
            case Status::Idle:      return "Ready";
            case Status::Reading:   return "Reading stems...";
            case Status::Analyzing: return "Detecting beats...";
            case Status::Chopping:  return "Chopping at " + juce::String (detectedSrcBPM, 1) + " BPM...";
            case Status::Writing:   return "Writing splice_output.wav...";
            case Status::Complete:  return "Complete — " + juce::String (detectedSrcBPM, 1) + " \xe2\x86\x92 "
                                           + juce::String (tgtBPM, 1) + " BPM";
            case Status::Error:     return "Error: " + errorMessage;
            default:                return {};
        }
    }

private:
    juce::File   stemsDir;
    double       srcBPM  = 0.0;
    double       tgtBPM  = 120.0;
    bool         skipWarp = false;
    float        density_ = 0.5f;
    double       detectedSrcBPM = 120.0;

    std::atomic<float>  progress { 0.0f };
    std::atomic<Status> status   { Status::Idle };
    juce::String        errorMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceThread)
};
