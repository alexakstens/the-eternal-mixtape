#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>

class WaveformDisplay : public juce::Component,
                        public juce::ChangeListener
{
public:
    WaveformDisplay (juce::AudioFormatManager& formatManager,
                     const juce::String& labelText = {})
        : label (labelText),
          thumbnailCache (2),
          thumbnail (512, formatManager, thumbnailCache)
    {
        thumbnail.addChangeListener (this);
    }

    ~WaveformDisplay() override
    {
        thumbnail.removeChangeListener (this);
    }

    void loadFile (const juce::File& file)
    {
        if (file.existsAsFile())
            thumbnail.setSource (new juce::FileInputSource (file));
    }

    void clear()
    {
        thumbnail.clear();
        repaint();
    }

    void setLabel (const juce::String& newLabel)
    {
        label = newLabel;
        repaint();
    }

    void setWaveformColour (juce::Colour c)
    {
        waveColour = c;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.setColour (juce::Colour (0xff1a1a2e));
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

        g.setColour (juce::Colour (0xff3a3a5e));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);

        auto drawArea = bounds.reduced (4);

        if (label.isNotEmpty())
        {
            g.setColour (juce::Colours::lightgrey);
            g.setFont (11.0f);
            g.drawText (label, drawArea.removeFromTop (14),
                        juce::Justification::centredLeft);
        }

        if (thumbnail.getTotalLength() > 0.0)
        {
            g.setColour (waveColour);
            thumbnail.drawChannels (g, drawArea, 0.0,
                                    thumbnail.getTotalLength(), 1.0f);
        }
        else
        {
            g.setColour (juce::Colours::grey.withAlpha (0.4f));
            g.setFont (11.0f);
            g.drawText ("No audio", drawArea, juce::Justification::centred);
        }
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        repaint();
    }

private:
    juce::String label;
    juce::Colour waveColour { 0xff4fc3f7 };
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
