#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioConversion.h"
#include "demucs.hpp"
#if defined(ORT_USE_GPU) && JUCE_WINDOWS
 #include <dml_provider_factory.h>
#endif
#include <atomic>
#include <fstream>
#include <functional>

class SeparationThread : public juce::Thread
{
public:
    enum class Status
    {
        Idle,
        LoadingModel,
        LoadingAudio,
        Resampling,
        Processing,
        WritingStems,
        Complete,
        Error
    };

    SeparationThread()
        : juce::Thread ("DemucsProcessing")
    {
       #if defined(ORT_USE_GPU)
        gpuEnabled = true;
       #endif
    }

    ~SeparationThread() override
    {
        stopThread (10000);
    }

    void configure (const juce::File& inputFile,
                    const juce::File& modelFile,
                    const juce::File& outputDir)
    {
        this->inputFile = inputFile;
        this->modelFile = modelFile;
        this->outputDir = outputDir;
    }

    void run() override
    {
        progress.store (0.0f);
        statusMessage = "Starting...";
        status.store (Status::LoadingModel);
        startTime = juce::Time::getMillisecondCounterHiRes();
        etaMessage = "";

        try
        {
            // 1. Configure ONNX session options
            statusMessage = "Loading model...";
            demucsonnx::demucs_model model;

            Ort::SessionOptions opts;
            opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
            opts.SetExecutionMode (ExecutionMode::ORT_PARALLEL);
            opts.SetIntraOpNumThreads (0);
            opts.SetInterOpNumThreads (0);

           #if defined(ORT_USE_GPU)
            #if JUCE_WINDOWS
            // DirectML ships with Windows 10 1903+ — no extra driver installation required.
            Ort::ThrowOnError (OrtSessionOptionsAppendExecutionProvider_DML (opts, 0));
            #elif JUCE_MAC
            opts.AppendExecutionProvider_CoreML (0);
            #endif
           #endif

            // RAII: ensures model.sess is released whenever run() exits (cancel, error, or complete).
            struct SessionGuard
            {
                demucsonnx::demucs_model& m;
                ~SessionGuard() { m.sess.reset(); }
            } sessionGuard { model };

            if (threadShouldExit()) { cancelCleanup(); return; }

            // 2. Load model from file
            {
                std::ifstream f (modelFile.getFullPathName().toStdString(), std::ios::binary);
                if (! f)
                {
                    errorMessage = "Failed to open model: " + modelFile.getFileName();
                    status.store (Status::Error);
                    return;
                }
                std::vector<char> modelData ((std::istreambuf_iterator<char> (f)),
                                              std::istreambuf_iterator<char>());

                if (! demucsonnx::load_model (modelData, model, opts))
                {
                    errorMessage = "Failed to load model: " + modelFile.getFileName();
                    status.store (Status::Error);
                    return;
                }
            }

            if (threadShouldExit()) { cancelCleanup(); return; }

            // 3. Load audio file via JUCE
            status.store (Status::LoadingAudio);
            statusMessage = "Loading audio...";

            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();

            auto reader = std::unique_ptr<juce::AudioFormatReader> (
                formatManager.createReaderFor (inputFile));

            if (! reader)
            {
                errorMessage = "Failed to read audio file: " + inputFile.getFileName();
                status.store (Status::Error);
                return;
            }

            double sourceSampleRate = reader->sampleRate;
            int64_t rawLength = reader->lengthInSamples;
            int numChannels = static_cast<int> (reader->numChannels);

            if (sourceSampleRate <= 0.0)
            {
                errorMessage = "Invalid sample rate in audio file: " + inputFile.getFileName();
                status.store (Status::Error);
                return;
            }

            // Cap at 30 minutes to guard against corrupt length headers
            const int64_t maxSamples = static_cast<int64_t> (sourceSampleRate * 60.0 * 30.0);
            if (rawLength <= 0 || rawLength > maxSamples)
            {
                errorMessage = "Audio file has invalid or unsupported length: " + inputFile.getFileName();
                status.store (Status::Error);
                return;
            }

            int numSamples = static_cast<int> (rawLength);

            juce::AudioBuffer<float> audioBuffer (numChannels, numSamples);
            reader->read (&audioBuffer, 0, numSamples, 0, true, true);
            reader.reset();

            // 4. Resample if necessary
            if (static_cast<int> (sourceSampleRate) != demucsonnx::SUPPORTED_SAMPLE_RATE)
            {
                status.store (Status::Resampling);
                statusMessage = "Resampling from " + juce::String (static_cast<int> (sourceSampleRate))
                    + " Hz to 44100 Hz...";

                double ratio = sourceSampleRate / static_cast<double> (demucsonnx::SUPPORTED_SAMPLE_RATE);
                int newNumSamples = static_cast<int> (std::ceil (numSamples / ratio));

                juce::AudioBuffer<float> resampledBuffer (numChannels, newNumSamples);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    juce::LagrangeInterpolator interpolator;
                    interpolator.reset();
                    interpolator.process (ratio,
                        audioBuffer.getReadPointer (ch),
                        resampledBuffer.getWritePointer (ch),
                        newNumSamples);
                }

                audioBuffer = std::move (resampledBuffer);
                numSamples = newNumSamples;
            }

            Eigen::MatrixXf eigenAudio = AudioConversion::juceToEigen (audioBuffer);
            audioBuffer = juce::AudioBuffer<float>(); // free memory early

            if (threadShouldExit()) { cancelCleanup(); return; }

            // 5. Run separation
            status.store (Status::Processing);
            processingStartTime = juce::Time::getMillisecondCounterHiRes();

            auto result = demucsonnx::demucs_inference (model, eigenAudio,
                [this] (float p, const std::string& msg)
                {
                    progress.store (p);
                    statusMessage = juce::String (msg);
                    updateETA (p);
                });

            if (threadShouldExit() || result.size() == 0)
            {
                cancelCleanup();
                return;
            }

            // 6. Write stems
            status.store (Status::WritingStems);
            statusMessage = "Writing stems...";
            etaMessage = "";
            outputDir.createDirectory();

            const char* stemNames[] = { "drums", "bass", "other", "vocals", "guitar", "piano" };
            int nbSources = model.nb_sources;
            int outLen = static_cast<int> (eigenAudio.cols());

            juce::WavAudioFormat wavFormat;

            for (int s = 0; s < nbSources && s < 6; ++s)
            {
                if (threadShouldExit()) { cancelCleanup(); return; }

                Eigen::MatrixXf stemData (2, outLen);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < outLen; ++i)
                        stemData (ch, i) = result (s, ch, i);

                auto stemBuffer = AudioConversion::eigenToJuce (stemData);
                auto stemFile = outputDir.getChildFile (juce::String (stemNames[s]) + ".wav");

                auto outStream = stemFile.createOutputStream();
                if (! outStream)
                {
                    errorMessage = "Failed to create file: " + stemFile.getFileName();
                    status.store (Status::Error);
                    return;
                }

                std::unique_ptr<juce::AudioFormatWriter> writer (
                    wavFormat.createWriterFor (outStream.release(), 44100.0, 2, 32, {}, 0));

                if (writer)
                    writer->writeFromAudioSampleBuffer (stemBuffer, 0, stemBuffer.getNumSamples());
            }

            // sessionGuard destructor releases the ONNX session here
            auto elapsed = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
            statusMessage = "Complete! (" + juce::String (elapsed, 1) + "s total)";
            status.store (Status::Complete);
        }
        catch (const std::exception& e)
        {
            errorMessage = juce::String ("Error: ") + e.what();
            status.store (Status::Error);
        }
    }

    float getProgress() const { return progress.load(); }
    Status getStatus() const { return status.load(); }
    juce::String getStatusMessage() const { return statusMessage; }
    juce::String getErrorMessage() const { return errorMessage; }
    juce::String getETAMessage() const { return etaMessage; }
    bool isGpuEnabled() const { return gpuEnabled; }

private:
    juce::File inputFile;
    juce::File modelFile;
    juce::File outputDir;
    bool gpuEnabled = false;

    std::atomic<float> progress { 0.0f };
    std::atomic<Status> status { Status::Idle };
    juce::String statusMessage;
    juce::String errorMessage;
    juce::String etaMessage;

    double startTime = 0.0;
    double processingStartTime = 0.0;

    void cancelCleanup()
    {
        progress.store (0.0f);
        etaMessage = "";
        statusMessage = "Cancelled.";
        status.store (Status::Idle);
    }

    void updateETA (float p)
    {
        if (p < 0.05f) return;

        double elapsed = (juce::Time::getMillisecondCounterHiRes() - processingStartTime) / 1000.0;
        double totalEstimate = elapsed / static_cast<double> (p);
        double remaining = totalEstimate - elapsed;

        if (remaining < 60.0)
            etaMessage = "ETA: " + juce::String (static_cast<int> (remaining)) + "s";
        else
            etaMessage = "ETA: " + juce::String (static_cast<int> (remaining / 60.0)) + "m "
                + juce::String (static_cast<int> (std::fmod (remaining, 60.0))) + "s";
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeparationThread)
};
