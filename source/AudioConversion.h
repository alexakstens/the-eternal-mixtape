#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <Eigen/Dense>

namespace AudioConversion
{
    // JUCE AudioBuffer -> Eigen MatrixXf (2, N)
    inline Eigen::MatrixXf juceToEigen (const juce::AudioBuffer<float>& buffer)
    {
        int channels = juce::jmin (buffer.getNumChannels(), 2);
        int samples = buffer.getNumSamples();

        Eigen::MatrixXf mat (2, samples);

        if (channels >= 2)
        {
            const float* left = buffer.getReadPointer (0);
            const float* right = buffer.getReadPointer (1);
            for (int i = 0; i < samples; ++i)
            {
                mat (0, i) = left[i];
                mat (1, i) = right[i];
            }
        }
        else if (channels == 1)
        {
            // Mono -> duplicate to stereo
            const float* mono = buffer.getReadPointer (0);
            for (int i = 0; i < samples; ++i)
            {
                mat (0, i) = mono[i];
                mat (1, i) = mono[i];
            }
        }
        else
        {
            mat.setZero();
        }

        return mat;
    }

    // Eigen MatrixXf (2, N) -> JUCE AudioBuffer
    inline juce::AudioBuffer<float> eigenToJuce (const Eigen::MatrixXf& mat)
    {
        int channels = static_cast<int> (mat.rows());
        int samples = static_cast<int> (mat.cols());

        juce::AudioBuffer<float> buffer (channels, samples);

        for (int ch = 0; ch < channels; ++ch)
        {
            float* dst = buffer.getWritePointer (ch);
            for (int i = 0; i < samples; ++i)
                dst[i] = mat (ch, i);
        }

        return buffer;
    }
}
