#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioConversion.h"
#include "demucs.hpp"
#include <atomic>
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
    }

    ~SeparationThread() override
    {
        stopThread (10000);
    }

    void configure (const juce::File& inputFile,
                    const juce::File& modelFile,
                    const juce::File& outputDir,
                    bool useCuda)
    {
        this->inputFile = inputFile;
        this->modelFile = modelFile;
        this->outputDir = outputDir;
        this->useCuda = useCuda;
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
            // 1. Load ONNX model
            statusMessage = "Loading model...";
            cmucs::DemucsModel model;

            Ort::SessionOptions opts;
            opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
            opts.SetExecutionMode (ExecutionMode::ORT_PARALLEL);
            opts.SetIntraOpNumThreads (0);
            opts.SetInterOpNumThreads (0);

            if (useCuda)
            {
                OrtCUDAProviderOptions cudaOpts {};
                cudaOpts.device_id = 0;
                cudaOpts.arena_extend_strategy = 0;
                cudaOpts.gpu_mem_limit = SIZE_MAX;
                cudaOpts.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
                cudaOpts.do_copy_in_default_stream = 1;
                try
                {
                    opts.AppendExecutionProvider_CUDA (cudaOpts);
                    gpuEnabled = true;
                }
                catch (const Ort::Exception&)
                {
                    gpuEnabled = false;
                }
            }

            if (threadShouldExit()) return;

            if (! cmucs::load_model (modelFile.getFullPathName().toStdString(), model, opts))
            {
                errorMessage = "Failed to load model: " + modelFile.getFileName();
                status.store (Status::Error);
                return;
            }

            if (threadShouldExit()) return;

            // 2. Load audio file via JUCE
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
            int numSamples = static_cast<int> (reader->lengthInSamples);
            int numChannels = static_cast<int> (reader->numChannels);

            juce::AudioBuffer<float> audioBuffer (numChannels, numSamples);
            reader->read (&audioBuffer, 0, numSamples, 0, true, true);
            reader.reset();

            // 3. Resample if necessary
            if (static_cast<int> (sourceSampleRate) != cmucs::SUPPORTED_SAMPLE_RATE)
            {
                status.store (Status::Resampling);
                statusMessage = "Resampling from " + juce::String (static_cast<int> (sourceSampleRate))
                    + " Hz to 44100 Hz...";

                double ratio = sourceSampleRate / static_cast<double> (cmucs::SUPPORTED_SAMPLE_RATE);
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

            // Convert to Eigen
            Eigen::MatrixXf eigenAudio = AudioConversion::juceToEigen (audioBuffer);
            audioBuffer = juce::AudioBuffer<float>(); // free memory

            if (threadShouldExit()) return;

            // 4. Run separation
            status.store (Status::Processing);
            processingStartTime = juce::Time::getMillisecondCounterHiRes();

            auto result = cmucs::inference (model, eigenAudio,
                [this] (float p, const std::string& msg)
                {
                    progress.store (p);
                    statusMessage = juce::String (msg);
                    updateETA (p);
                });

            if (threadShouldExit()) return;

            // 5. Write stems
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
                if (threadShouldExit()) return;

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

            // 6. Explicitly release session to avoid CUDA cleanup crash
            model.session.reset();

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
    bool useCuda = true;
    bool gpuEnabled = false;

    std::atomic<float> progress { 0.0f };
    std::atomic<Status> status { Status::Idle };
    juce::String statusMessage;
    juce::String errorMessage;
    juce::String etaMessage;

    double startTime = 0.0;
    double processingStartTime = 0.0;

    void updateETA (float p)
    {
        if (p < 0.05f) return; // not enough data for a good estimate

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
