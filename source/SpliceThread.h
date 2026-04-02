#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "remixing/TimeStretcher.h"
#include <atomic>
#include <cmath>

/**
 * Background thread that reads the separated stems from disk,
 * applies time-stretching via remixing::TimeStretcher to match
 * the target BPM, sums the stems, and writes splice_output.wav.
 *
 * Source BPM defaults to 120. When BeatAnalyzer (aubio) is enabled
 * in a future step, it will auto-detect this from the stems.
 */
class SpliceThread : public juce::Thread
{
public:
    enum class Status { Idle, Reading, Stretching, Writing, Complete, Error };

    SpliceThread() : juce::Thread ("SpliceProcessing") {}

    ~SpliceThread() override { stopThread (10000); }

    void configure (const juce::File& stemsDirectory,
                    double sourceBPM,
                    double targetBPM)
    {
        stemsDir  = stemsDirectory;
        srcBPM    = sourceBPM;
        tgtBPM    = targetBPM;
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

        // ── 1. Read all available stems ─────────────────────────────────────
        const juce::StringArray stemNames { "drums.wav", "bass.wav",
                                            "other.wav", "vocals.wav" };

        using StemChannels = std::vector<std::vector<float>>;
        std::vector<StemChannels> allStems;
        int sampleRate = 44100;

        for (auto& name : stemNames)
        {
            if (threadShouldExit()) return;

            auto f = stemsDir.getChildFile (name);
            if (! f.existsAsFile()) continue;

            std::unique_ptr<juce::AudioFormatReader> reader (fmtMgr.createReaderFor (f));
            if (! reader) continue;

            sampleRate = (int) reader->sampleRate;
            int numCh  = (int) reader->numChannels;
            int numSmp = (int) reader->lengthInSamples;

            juce::AudioBuffer<float> buf (numCh, numSmp);
            reader->read (&buf, 0, numSmp, 0, true, true);

            StemChannels channels;
            for (int c = 0; c < numCh; ++c)
                channels.push_back ({ buf.getReadPointer (c),
                                      buf.getReadPointer (c) + numSmp });
            allStems.push_back (std::move (channels));
        }

        if (allStems.empty())
        {
            errorMessage = "No stem files found — run separation first.";
            status.store (Status::Error);
            return;
        }

        progress.store (0.25f);

        // ── 2. Time-stretch each stem and mix ────────────────────────────────
        status.store (Status::Stretching);

        // stretch_factor > 1 = slower; going from srcBPM → tgtBPM:
        //   e.g. 120 BPM → 100 BPM  ⟹  stretch = 120/100 = 1.2 (lengthens)
        // TODO: replace srcBPM with BeatAnalyzer detection when aubio is enabled.
        float stretchFactor = (srcBPM > 0.0 && tgtBPM > 0.0)
                                  ? (float) (srcBPM / tgtBPM)
                                  : 1.0f;

        auto params = remixing::TimeStretcher::makeParams (stretchFactor, (float) sampleRate);

        std::vector<std::vector<float>> mixed (2);

        int stemIdx = 0;
        for (auto& stemCh : allStems)
        {
            if (threadShouldExit()) return;

            // Ensure stereo before passing to time_stretch
            if (stemCh.size() == 1)
                stemCh.push_back (stemCh[0]);

            auto stretched = remixing::TimeStretcher::process (stemCh, params);

            for (int c = 0; c < 2; ++c)
            {
                if (mixed[c].size() < stretched[c].size())
                    mixed[c].resize (stretched[c].size(), 0.0f);
                for (size_t i = 0; i < stretched[c].size(); ++i)
                    mixed[c][i] += stretched[c][i];
            }

            progress.store (0.25f + 0.5f * (float) (++stemIdx)
                                              / (float) allStems.size());
        }

        // Normalize to peak
        float peak = 0.0f;
        for (auto& ch : mixed)
            for (auto s : ch) peak = std::max (peak, std::abs (s));
        if (peak > 1e-6f)
            for (auto& ch : mixed)
                for (auto& s : ch) s /= peak;

        if (threadShouldExit()) return;

        // ── 3. Write splice_output.wav ────────────────────────────────────────
        status.store (Status::Writing);

        auto outputFile = getOutputFile();
        outputFile.deleteFile();

        auto outStream = outputFile.createOutputStream();
        if (! outStream)
        {
            errorMessage = "Could not create output file.";
            status.store (Status::Error);
            return;
        }

        juce::WavAudioFormat wavFmt;
        auto writer = std::unique_ptr<juce::AudioFormatWriter> (
            wavFmt.createWriterFor (outStream.release(), sampleRate, 2, 16, {}, 0));

        if (! writer)
        {
            errorMessage = "Could not create WAV writer.";
            status.store (Status::Error);
            return;
        }

        int outSamples = (int) mixed[0].size();
        juce::AudioBuffer<float> outBuf (2, outSamples);
        for (int c = 0; c < 2; ++c)
            outBuf.copyFrom (c, 0, mixed[c].data(), outSamples);

        writer->writeFromAudioSampleBuffer (outBuf, 0, outSamples);

        progress.store (1.0f);
        status.store (Status::Complete);
    }

    //==========================================================================
    float          getProgress()       const { return progress.load(); }
    Status         getStatus()         const { return status.load(); }
    juce::String   getErrorMessage()   const { return errorMessage; }

    juce::String getStatusMessage() const
    {
        switch (status.load())
        {
            case Status::Idle:       return "Ready";
            case Status::Reading:    return "Reading stems...";
            case Status::Stretching: return "Time-stretching...";
            case Status::Writing:    return "Writing splice_output.wav...";
            case Status::Complete:   return "Complete";
            case Status::Error:      return "Error: " + errorMessage;
            default:                 return {};
        }
    }

private:
    juce::File   stemsDir;
    double       srcBPM = 120.0;
    double       tgtBPM = 120.0;

    std::atomic<float>  progress { 0.0f };
    std::atomic<Status> status   { Status::Idle };
    juce::String        errorMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceThread)
};
