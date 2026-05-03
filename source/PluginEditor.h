#pragma once

#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor,
                     public juce::Timer,
                     public juce::FileDragAndDropTarget
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Drag-and-drop support
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void updateUIForMode();
    void startStemSeparation();
    void cancelStemSeparation();
    void loadStemWaveforms();
    void browseForStemInput();
    void browseForStemModel();
    void browseForStemOutput();
    void startSpliceRemix();
    void loadSpliceOutputWaveform();

    PluginProcessor& processorRef;
    juce::Image uiImage;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    bool isExpertiseMode_ = false;

    // Top: meter + runtime
    juce::Label runtimeLabel;
    juce::Label meterLabel;

    // Tracks A–D: each has a label + 4 vertical stem mini-faders
    // (drums / bass / other / vocals — matching the icons baked into the background).
    static constexpr int kNumTracks = 4;
    static constexpr int kNumStemsPerTrack = 4;
    juce::Label  trackLabels[kNumTracks];
    juce::Slider trackStemSliders[kNumTracks][kNumStemsPerTrack];

    // Splice panel
    juce::DrawableButton spliceButton  { "Razor",     juce::DrawableButton::ImageFitted };
    juce::ToggleButton   skipWarpToggle { "Skip Warp" };
    juce::Slider bpmSlider;
    juce::Slider densitySlider;
    juce::Label bpmLabel { {}, "BPM" };
    juce::Label densityLabel { {}, "DENSITY" };

    // Splice output waveform (result of time-stretch remix)
    WaveformDisplay spliceOutputWaveform { formatManager, "Splice Output" };
    double spliceProgressValue = 0.0;
    juce::ProgressBar spliceProgressBar { spliceProgressValue };
    juce::Label spliceStatusLabel;
    bool spliceLoaded = false;

    // Splice output transport controls
    juce::DrawableButton spliceBackBtn    { "Back",    juce::DrawableButton::ImageFitted };
    juce::DrawableButton splicePlayBtn    { "Play",    juce::DrawableButton::ImageFitted };
    juce::DrawableButton spliceStopBtn    { "Stop",    juce::DrawableButton::ImageFitted };
    juce::DrawableButton spliceForwardBtn { "Forward", juce::DrawableButton::ImageFitted };
    juce::DrawableButton spliceLoopBtn    { "Loop",    juce::DrawableButton::ImageFitted };

    // Main transport — DrawableButton + SVG (replaces old TextButtons)
    juce::TextButton settingsButton { "Settings" };
    juce::DrawableButton mainRewindBtn  { "MainBack",    juce::DrawableButton::ImageFitted };
    juce::DrawableButton mainPlayBtn    { "MainPlay",    juce::DrawableButton::ImageFitted };
    juce::DrawableButton mainStopBtn    { "MainStop",    juce::DrawableButton::ImageFitted };
    juce::DrawableButton mainForwardBtn { "MainForward", juce::DrawableButton::ImageFitted };
    juce::DrawableButton mainLoopBtn    { "MainLoop",    juce::DrawableButton::ImageFitted };
    juce::TextButton autoSpliceButton { "AUTO SPLICE" };
    juce::TextButton regenerateButton { "REGENERATE" };
    juce::TextButton randomizeButton { "RANDOMIZE" };
    juce::TextButton recButton { "REC" };

    // Toggles visibility of the stem-separation panel; used by Expertise mode.
    std::function<void(bool)> setStemPanelVisible_;

    // Stem separation panel
    juce::AudioFormatManager formatManager;
    juce::Label stemInputLabel  { {}, "Input:" };
    juce::Label stemModelLabel  { {}, "Model:" };
    juce::Label stemOutputLabel { {}, "Output:" };
    juce::TextEditor stemInputEditor;
    juce::TextEditor stemModelEditor;
    juce::TextEditor stemOutputEditor;
    juce::TextButton stemInputBrowse  { "Browse" };
    juce::TextButton stemModelBrowse  { "Browse" };
    juce::TextButton stemOutputBrowse { "Browse" };
    juce::TextButton stemProcessButton { "Separate" };
    juce::TextButton stemCancelButton  { "Cancel" };
    juce::ToggleButton stemCudaToggle  { "CUDA" };
    double stemProgressValue = 0.0;
    juce::ProgressBar stemProgressBar { stemProgressValue };
    juce::Label stemStatusLabel;
    WaveformDisplay drumsWaveform  { formatManager, "Drums" };
    WaveformDisplay bassWaveform   { formatManager, "Bass" };
    WaveformDisplay otherWaveform  { formatManager, "Other" };
    WaveformDisplay vocalsWaveform { formatManager, "Vocals" };
    bool stemsLoaded = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
