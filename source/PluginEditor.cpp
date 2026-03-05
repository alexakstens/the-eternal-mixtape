#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (inspectButton);

    inspectButton.onClick = [this] {
        if (! inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible (true);
    };

    // Top: runtime + meter
    runtimeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (runtimeLabel);
    meterLabel.setText ("VU", juce::dontSendNotification);
    addAndMakeVisible (meterLabel);

    // Tracks
    const char* trackNames[] = { "TRACK A", "TRACK B", "TRACK C", "TRACK D" };
    for (int i = 0; i < kNumTracks; ++i)
    {
        trackLabels[i].setText (trackNames[i], juce::dontSendNotification);
        addAndMakeVisible (trackLabels[i]);
        trackGainSliders[i].setSliderStyle (juce::Slider::LinearHorizontal);
        trackGainSliders[i].setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        trackGainSliders[i].setRange (0.0, 2.0, 0.01);
        trackGainSliders[i].setValue (processorRef.getTrackGain (i));
        trackGainSliders[i].onValueChange = [this, i] {
            processorRef.setTrackGain (i, (float) trackGainSliders[i].getValue());
        };
        addAndMakeVisible (trackGainSliders[i]);
    }

    // Splice panel
    addAndMakeVisible (spliceButton);
    spliceButton.onClick = [this] {
        for (int t = 0; t < kNumTracks; ++t)
            processorRef.applySplice (t);
    };
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 50, 20);
    bpmSlider.setRange (20.0, 300.0, 1.0);
    bpmSlider.setValue (processorRef.getGlobalBPM());
    bpmSlider.onValueChange = [this] { processorRef.setGlobalBPM (bpmSlider.getValue()); };
    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (bpmLabel);
    densitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    densitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 50, 20);
    densitySlider.setRange (0.0, 1.0, 0.01);
    densitySlider.setValue (processorRef.getSpliceDensity());
    densitySlider.onValueChange = [this] { processorRef.setSpliceDensity ((float) densitySlider.getValue()); };
    addAndMakeVisible (densitySlider);
    addAndMakeVisible (densityLabel);

    // Transport
    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] {
        juce::FileChooser fc ("Choose folder for exported files",
                              processorRef.getConfigPath ("export_output_dir"),
                              "", true);
        if (fc.browseForDirectory())
        {
            processorRef.setConfigPath ("export_output_dir", fc.getResult());
        }
    };
    addAndMakeVisible (loopToggle);
    loopToggle.onClick = [this] { processorRef.setLoopEnabled (loopToggle.getToggleState()); };
    addAndMakeVisible (rewindButton);
    rewindButton.onClick = [this] {
        processorRef.stop();
        processorRef.setTransportPosition (0.0);
    };
    addAndMakeVisible (playButton);
    playButton.onClick = [this] {
        if (processorRef.getTransportTotalLengthSeconds() <= 0.0)
            return;
        processorRef.play();
    };
    addAndMakeVisible (ffButton);
    ffButton.onClick = [this] { processorRef.stop(); };
    addAndMakeVisible (autoSpliceButton);
    autoSpliceButton.onClick = [this] { processorRef.applyAutoSplice(); };
    addAndMakeVisible (regenerateButton);
    regenerateButton.onClick = [this] { processorRef.regenerateMix(); };
    addAndMakeVisible (randomizeButton);
    randomizeButton.onClick = [this] { processorRef.randomizeMix(); };
    addAndMakeVisible (recButton);
    recButton.onClick = [this] {
        if (isExpertiseMode_)
        {
            juce::FileChooser fc ("Save recording",
                                  processorRef.getConfigPath ("export_output_dir"),
                                  "*.wav", true);
            if (fc.browseForFileToSave (true))
                processorRef.startRecording (fc.getResult());
        }
        else
        {
            processorRef.startRecording();
        }
    };

    updateUIForMode();
    startTimerHz (10);

    setSize (720, 420);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::timerCallback()
{
    bool expert = juce::ModifierKeys::getCurrentModifiers().isCommandDown();
    if (expert != isExpertiseMode_)
    {
        isExpertiseMode_ = expert;
        updateUIForMode();
    }

    double pos = processorRef.getTransportPositionSeconds();
    double len = processorRef.getTransportTotalLengthSeconds();
    int pSec = (int) pos;
    int pMin = pSec / 60;
    pSec %= 60;
    int lSec = (int) len;
    int lMin = lSec / 60;
    lSec %= 60;
    runtimeLabel.setText (juce::String::formatted ("%d:%02d / %d:%02d", pMin, pSec, lMin, lSec),
                          juce::dontSendNotification);

    float level = processorRef.getMasterLevels();
    meterLabel.setText ("VU " + juce::String (juce::jlimit (0, 100, (int) (level * 100))),
                        juce::dontSendNotification);
}

void PluginEditor::updateUIForMode()
{
    if (isExpertiseMode_)
    {
        spliceButton.setButtonText ("SPLICE (options)");
        recButton.setButtonText ("REC (choose file)");
    }
    else
    {
        spliceButton.setButtonText ("SPLICE");
        recButton.setButtonText ("REC");
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    float level = processorRef.getMasterLevels();
    auto topBar = getLocalBounds().removeFromTop (28);
    auto meterRect = topBar.removeFromLeft (120).reduced (4);
    g.setColour (juce::Colours::darkgrey);
    g.fillRoundedRectangle (meterRect.toFloat(), 4.0f);
    g.setColour (juce::Colours::lime);
    g.fillRoundedRectangle (meterRect.withWidth (meterRect.getWidth() * juce::jmin (1.0f, level)).toFloat(), 4.0f);
}

void PluginEditor::resized()
{
    auto r = getLocalBounds();

    auto top = r.removeFromTop (28);
    meterLabel.setBounds (top.removeFromLeft (80).reduced (2));
    runtimeLabel.setBounds (top.removeFromRight (140).reduced (2));

    r.removeFromTop (4);

    const int trackH = 44;
    for (int i = 0; i < kNumTracks; ++i)
    {
        auto row = r.removeFromTop (trackH);
        trackLabels[i].setBounds (row.removeFromLeft (70).reduced (2));
        trackGainSliders[i].setBounds (row.reduced (2));
    }

    r.removeFromTop (6);

    auto spliceRow = r.removeFromTop (32);
    spliceButton.setBounds (spliceRow.removeFromLeft (100).reduced (2));
    bpmLabel.setBounds (spliceRow.removeFromLeft (32).reduced (2));
    bpmSlider.setBounds (spliceRow.removeFromLeft (120).reduced (2));
    densityLabel.setBounds (spliceRow.removeFromLeft (60).reduced (2));
    densitySlider.setBounds (spliceRow.removeFromLeft (140).reduced (2));

    r.removeFromTop (6);

    auto transport = r.removeFromBottom (40);
    settingsButton.setBounds (transport.removeFromLeft (70).reduced (2));
    loopToggle.setBounds (transport.removeFromLeft (50).reduced (2));
    rewindButton.setBounds (transport.removeFromLeft (36).reduced (2));
    playButton.setBounds (transport.removeFromLeft (44).reduced (2));
    ffButton.setBounds (transport.removeFromLeft (36).reduced (2));
    autoSpliceButton.setBounds (transport.removeFromLeft (100).reduced (2));
    regenerateButton.setBounds (transport.removeFromLeft (100).reduced (2));
    randomizeButton.setBounds (transport.removeFromLeft (90).reduced (2));
    recButton.setBounds (transport.removeFromLeft (50).reduced (2));

    r.removeFromBottom (8);

    inspectButton.setBounds (r.withSizeKeepingCentre (100, 32));
}
