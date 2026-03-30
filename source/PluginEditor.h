#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void updateUIForMode();

    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    bool isExpertiseMode_ = false;

    // Top: meter + runtime
    juce::Label runtimeLabel;
    juce::Label meterLabel;

    // Tracks A–D: label + gain slider
    static constexpr int kNumTracks = 4;
    juce::Label trackLabels[kNumTracks];
    juce::Slider trackGainSliders[kNumTracks];

    // Splice panel
    juce::TextButton spliceButton { "SPLICE" };
    juce::Slider bpmSlider;
    juce::Slider densitySlider;
    juce::Label bpmLabel { {}, "BPM" };
    juce::Label densityLabel { {}, "DENSITY" };

    // Transport
    juce::TextButton settingsButton { "Settings" };
    juce::ToggleButton loopToggle { "Loop" };
    juce::TextButton rewindButton { "<<" };
    juce::TextButton playButton { "Play" };
    juce::TextButton ffButton { ">>" };
    juce::TextButton autoSpliceButton { "AUTO SPLICE" };
    juce::TextButton regenerateButton { "REGENERATE" };
    juce::TextButton randomizeButton { "RANDOMIZE" };
    juce::TextButton recButton { "REC" };

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
