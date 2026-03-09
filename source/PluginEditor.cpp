#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    uiImage = juce::ImageCache::getFromMemory (BinaryData::eternal_mixtape_UI_V1_1_png,
                                               BinaryData::eternal_mixtape_UI_V1_1_pngSize);

    addAndMakeVisible (inspectButton);

    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible (true);
    };

    if (uiImage.isValid())
    {
        constexpr int maxWidth  = 900;
        constexpr int maxHeight = 700;
        auto scale = juce::jmin (1.0f,
                                 (float) maxWidth  / (float) uiImage.getWidth(),
                                 (float) maxHeight / (float) uiImage.getHeight());
        setSize (juce::roundToInt (uiImage.getWidth()  * scale),
                 juce::roundToInt (uiImage.getHeight() * scale));
    }
    else
        setSize (800, 600);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (uiImage.isValid())
        g.drawImage (uiImage, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void PluginEditor::resized()
{
    inspectButton.setBounds (getWidth() - 80, getHeight() - 30, 75, 25);
}
