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
 * Output files written to stemsDir/:
 *   splice_stem_0.wav  drums  (Track A)
 *   splice_stem_1.wav  bass   (Track B)
 *   splice_stem_2.wav  other  (Track C)
 *   splice_stem_3.wav  vocals (Track D)
 *   splice_output.wav  mixed preview (for waveform display)
 *
 * Per-stem files allow the Processor to apply track gains in real-time
 * without re-running the splice.  All stems are normalized by the same
 * global peak so relative levels are preserved.
 */
class SpliceThread : public juce::Thread
{
public:
    enum class Status { Idle, Reading, Analyzing, Chopping, Writing, Complete, Error };

    SpliceThread() : juce::Thread ("SpliceProcessing") {}
    ~SpliceThread() override { stopThread (10000); }

    void configure (const juce::File& stemsDirectory,
                    double       sourceBPM,
                    double       targetBPM,
                    bool         skipTimeWarp   = false,
                    float        density        = 0.5f,
                    bool         randomizeTime  = false,
                    unsigned int seed           = 42,
                    const float* stemGainsIn    = nullptr)
    {
        stemsDir       = stemsDirectory;
        srcBPM         = sourceBPM;
        tgtBPM         = targetBPM;
        skipWarp       = skipTimeWarp;
        density_       = juce::jlimit (0.0f, 1.0f, density);
        randomizeTime_ = randomizeTime;
        seed_          = seed;
        for (int i = 0; i < 4; ++i)
            stemGains_[i] = (stemGainsIn != nullptr) ? juce::jlimit (0.0f, 2.0f, stemGainsIn[i]) : 1.0f;
    }

    juce::File getMixedOutputFile() const { return stemsDir.getChildFile ("splice_output.wav"); }
    juce::File getStemOutputFile (int i)  const
    {
        return stemsDir.getChildFile ("splice_stem_" + juce::String (i) + ".wav");
    }

    // Legacy accessor kept for callers that only care about the mixed preview
    juce::File getOutputFile() const { return getMixedOutputFile(); }

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
        int sampleRate   = 44100;
        int totalSamples = 0;

        int stemIdx = 0;
        for (auto& name : stemNames)
        {
            if (threadShouldExit()) return;

            auto f = stemsDir.getChildFile (name);
            if (! f.existsAsFile()) { ++stemIdx; continue; }

            std::unique_ptr<juce::AudioFormatReader> reader (fmtMgr.createReaderFor (f));
            if (! reader) { ++stemIdx; continue; }

            sampleRate   = (int) reader->sampleRate;
            int numCh    = (int) reader->numChannels;
            int numSmp   = (int) reader->lengthInSamples;
            totalSamples = std::max (totalSamples, numSmp);

            juce::AudioBuffer<float> buf (numCh, numSmp);
            reader->read (&buf, 0, numSmp, 0, true, true);

            // Apply per-stem gain (from track faders) at the read stage so that
            // both the per-stem output files and the mixed preview bake it in.
            const float g = stemGains_[stemIdx < 4 ? stemIdx : 0];
            if (! juce::approximatelyEqual (g, 1.0f))
                buf.applyGain (g);

            StemChannels channels;
            for (int c = 0; c < numCh; ++c)
                channels.push_back ({ buf.getReadPointer (c),
                                      buf.getReadPointer (c) + numSmp });
            if ((int) channels.size() == 1)
                channels.push_back (channels[0]);   // mono → stereo
            allStems.push_back (std::move (channels));
            ++stemIdx;
        }

        if (allStems.empty())
        {
            errorMessage = "No stem files found — run separation first.";
            status.store (Status::Error);
            return;
        }

        progress.store (0.15f);

        // ── 2. Detect beats ──────────────────────────────────────────────────
        status.store (Status::Analyzing);

        detectedSrcBPM = (srcBPM > 0.0) ? srcBPM : 120.0;
        std::vector<unsigned int> beatSamples;

        if (srcBPM <= 0.0)
        {
            auto analysis = remixing::SongAnalyzer::analyze (allStems[0][0], sampleRate);
            if (analysis.bpm > 40.0 && analysis.bpm < 300.0)
                detectedSrcBPM = (double) analysis.bpm;
            beatSamples = analysis.beatSamples;
        }

        if ((int) beatSamples.size() < 2)
        {
            int beatLen = (int) (sampleRate * 60.0 / detectedSrcBPM);
            for (int s = 0; s + beatLen <= totalSamples; s += beatLen)
                beatSamples.push_back ((unsigned int) s);
        }

        const int numBeats      = (int) beatSamples.size();
        const int targetBeatLen = (int) (sampleRate * 60.0 / tgtBPM);
        const int xfadeLen      = std::min (256, std::max (1, targetBeatLen / 8));

        progress.store (0.30f);

        // Single RNG used for both beat shuffling and per-beat time warp.
        // seed_ = 42 → reproducible (AUTO SPLICE); seeded from clock → unique (REGENERATE/RANDOMIZE)
        std::mt19937 rng (seed_);

        // ── 3. Build shuffle permutation (density 0=none, 1=full random) ─────
        std::vector<int> perm (numBeats);
        std::iota (perm.begin(), perm.end(), 0);

        if (density_ > 0.0f && numBeats > 1)
        {
            std::vector<int> shuffled (perm);
            std::shuffle (shuffled.begin(), shuffled.end(), rng);

            std::uniform_real_distribution<float> coin (0.0f, 1.0f);
            for (int i = 0; i < numBeats; ++i)
                if (coin (rng) < density_)
                    perm[i] = shuffled[i];
        }

        // ── 4. Chop each stem into beat chunks and resize ────────────────────
        status.store (Status::Chopping);

        const int numStems  = (int) allStems.size();
        // When randomizing time each beat gets its own length, so reserve generously
        const int reserveN  = skipWarp
                                  ? totalSamples + 4096
                                  : randomizeTime_ ? (int)(numBeats * targetBeatLen * 1.8f) + 4096
                                                   : numBeats * targetBeatLen + 4096;

        // Per-beat tempo factor distribution: 50% – 180% of target beat length
        std::uniform_real_distribution<float> tempoVar (0.5f, 1.8f);

        // Per-stem stereo output: stemOut[stem][channel]
        std::vector<std::array<std::vector<float>, 2>> stemOut (numStems);
        for (auto& s : stemOut)
            for (auto& ch : s)
                ch.assign (reserveN, 0.0f);

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

            // Per-beat output length: fixed at targetBeatLen normally;
            // randomly varied when randomizeTime_ is true (each beat plays faster or slower).
            int outChunkLen;
            if (skipWarp)
                outChunkLen = srcLen;
            else if (randomizeTime_)
                outChunkLen = std::max (512, (int)(targetBeatLen * tempoVar (rng)));
            else
                outChunkLen = targetBeatLen;

            // Grow output buffers if needed
            if (writePos + outChunkLen > (int) stemOut[0][0].size())
                for (auto& s : stemOut)
                    for (auto& ch : s)
                        ch.resize ((size_t)(writePos + outChunkLen + 44100), 0.0f);

            for (int si = 0; si < numStems; ++si)
            {
                auto& stemCh = allStems[si];
                for (int c = 0; c < 2; ++c)
                {
                    const auto& src = stemCh[c < (int) stemCh.size() ? c : 0];

                    if (skipWarp)
                    {
                        for (int i = 0; i < outChunkLen; ++i)
                        {
                            int si2 = srcStart + i;
                            stemOut[si][c][(size_t)(writePos + i)] =
                                (si2 < (int) src.size()) ? src[(size_t) si2] : 0.0f;
                        }
                    }
                    else
                    {
                        // Interpolate: resample srcLen samples into outChunkLen samples
                        const double ratio = (double)(srcLen - 1) / std::max (1, outChunkLen - 1);
                        for (int i = 0; i < outChunkLen; ++i)
                        {
                            const double pos  = srcStart + i * ratio;
                            const int    lo   = (int) pos;
                            const int    hi   = std::min (lo + 1, (int) src.size() - 1);
                            const float  t    = (float)(pos - lo);
                            const float  lo_v = (lo < (int) src.size()) ? src[(size_t) lo] : 0.0f;
                            const float  hi_v = (hi < (int) src.size()) ? src[(size_t) hi] : 0.0f;
                            stemOut[si][c][(size_t)(writePos + i)] = lo_v * (1.0f - t) + hi_v * t;
                        }
                    }
                }
            }

            // Fade-in at chunk boundary to eliminate clicks
            if (outBeat > 0 && xfadeLen > 0 && writePos >= xfadeLen)
                for (auto& s : stemOut)
                    for (auto& ch : s)
                        for (int i = 0; i < xfadeLen; ++i)
                            ch[writePos + i] *= (float) i / xfadeLen;

            writePos += outChunkLen;
            progress.store (0.30f + 0.50f * (float)(outBeat + 1) / numBeats);
        }

        // Trim all stem outputs to writePos
        for (auto& s : stemOut)
            for (auto& ch : s)
                ch.resize (writePos);

        // ── 5. Normalize by shared global peak ───────────────────────────────
        float globalPeak = 1e-6f;
        for (auto& s : stemOut)
            for (auto& ch : s)
                for (auto v : ch)
                    globalPeak = std::max (globalPeak, std::abs (v));

        const float normScale = 1.0f / globalPeak;
        for (auto& s : stemOut)
            for (auto& ch : s)
                for (auto& v : ch)
                    v *= normScale;

        if (threadShouldExit()) return;

        // ── 6. Write per-stem WAVs + mixed preview ───────────────────────────
        status.store (Status::Writing);

        juce::WavAudioFormat wavFmt;

        // Per-stem files (one per track A-D)
        for (int si = 0; si < numStems; ++si)
        {
            auto f = getStemOutputFile (si);
            f.deleteFile();

            auto stream = f.createOutputStream();
            if (! stream) continue;

            auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                wavFmt.createWriterFor (stream.release(), sampleRate, 2, 16, {}, 0));
            if (! writer) continue;

            juce::AudioBuffer<float> buf (2, writePos);
            for (int c = 0; c < 2; ++c)
                buf.copyFrom (c, 0, stemOut[si][c].data(), writePos);
            writer->writeFromAudioSampleBuffer (buf, 0, writePos);
        }

        // Mixed preview (equal-gain sum, re-normalized)
        {
            std::vector<float> mixL (writePos, 0.0f), mixR (writePos, 0.0f);
            for (int si = 0; si < numStems; ++si)
                for (int i = 0; i < writePos; ++i)
                {
                    mixL[i] += stemOut[si][0][i];
                    mixR[i] += stemOut[si][1][i];
                }
            float mixPeak = 1e-6f;
            for (int i = 0; i < writePos; ++i)
            {
                mixPeak = std::max (mixPeak, std::abs (mixL[i]));
                mixPeak = std::max (mixPeak, std::abs (mixR[i]));
            }
            const float ms = 1.0f / mixPeak;
            for (int i = 0; i < writePos; ++i) { mixL[i] *= ms; mixR[i] *= ms; }

            auto f = getMixedOutputFile();
            f.deleteFile();
            auto stream = f.createOutputStream();
            auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                wavFmt.createWriterFor (stream.release(), sampleRate, 2, 16, {}, 0));
            if (writer)
            {
                juce::AudioBuffer<float> buf (2, writePos);
                buf.copyFrom (0, 0, mixL.data(), writePos);
                buf.copyFrom (1, 0, mixR.data(), writePos);
                writer->writeFromAudioSampleBuffer (buf, 0, writePos);
            }
        }

        progress.store (1.0f);
        status.store (Status::Complete);
    }

    //==========================================================================
    float        getProgress()     const { return progress.load(); }
    Status       getStatus()       const { return status.load(); }
    juce::String getErrorMessage() const { return errorMessage; }
    double       getDetectedBPM()  const { return detectedSrcBPM; }

    juce::String getStatusMessage() const
    {
        switch (status.load())
        {
            case Status::Idle:      return "Ready";
            case Status::Reading:   return "Reading stems...";
            case Status::Analyzing: return "Detecting beats...";
            case Status::Chopping:  return "Chopping at " + juce::String (detectedSrcBPM, 1) + " BPM...";
            case Status::Writing:   return "Writing output files...";
            case Status::Complete:  return "Complete — " + juce::String (detectedSrcBPM, 1)
                                           + " \xe2\x86\x92 " + juce::String (tgtBPM, 1) + " BPM";
            case Status::Error:     return "Error: " + errorMessage;
            default:                return {};
        }
    }

private:
    juce::File   stemsDir;
    double       srcBPM        = 0.0;
    double       tgtBPM        = 120.0;
    bool         skipWarp      = false;
    float        density_      = 0.5f;
    bool         randomizeTime_ = false;
    unsigned int seed_          = 42;
    float        stemGains_[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };
    double       detectedSrcBPM = 120.0;

    std::atomic<float>  progress { 0.0f };
    std::atomic<Status> status   { Status::Idle };
    juce::String        errorMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceThread)
};
