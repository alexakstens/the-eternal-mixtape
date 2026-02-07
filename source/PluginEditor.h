#pragma once

#include "PluginProcessor.h"
#include "WaveformDisplay.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

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
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;

    // Shared format manager for waveform thumbnails
    juce::AudioFormatManager formatManager;

    // File selection
    juce::Label inputLabel { {}, "Input File:" };
    juce::Label modelLabel { {}, "Model:" };
    juce::Label outputLabel { {}, "Output Dir:" };

    juce::TextEditor inputPathEditor;
    juce::TextEditor modelPathEditor;
    juce::TextEditor outputPathEditor;

    juce::TextButton inputBrowseButton { "Browse" };
    juce::TextButton modelBrowseButton { "Browse" };
    juce::TextButton outputBrowseButton { "Browse" };
    juce::TextButton inspectButton { "Inspect UI" };

    // Controls
    juce::TextButton processButton { "Separate" };
    juce::TextButton cancelButton { "Cancel" };
    juce::ToggleButton cudaToggle { "Use CUDA (GPU)" };

    // Status
    juce::ProgressBar progressBar;
    juce::Label statusLabel;
    juce::Label gpuStatusLabel;

    double progressValue = 0.0;

    // Waveform displays
    WaveformDisplay inputWaveform { formatManager, "Input" };
    juce::Label stemsLabel { {}, "Output Stems:" };

    WaveformDisplay drumsWaveform  { formatManager, "Drums" };
    WaveformDisplay bassWaveform   { formatManager, "Bass" };
    WaveformDisplay otherWaveform  { formatManager, "Other" };
    WaveformDisplay vocalsWaveform { formatManager, "Vocals" };

    void browseForInput();
    void browseForModel();
    void browseForOutput();
    void startProcessing();
    void cancelProcessing();
    void updateUI();
    void loadInputWaveform (const juce::File& file);
    void loadStemWaveforms();

    bool stemsLoaded = false;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
