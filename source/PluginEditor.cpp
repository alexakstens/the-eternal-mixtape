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
    spliceButton.onClick = [this] { startSpliceRemix(); };
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

    // Splice output waveform
    spliceOutputWaveform.setWaveformColour (juce::Colour (0xffa78bfa));
    addAndMakeVisible (spliceOutputWaveform);
    spliceStatusLabel.setText ("Separate stems first, then SPLICE", juce::dontSendNotification);
    spliceStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (spliceStatusLabel);
    addAndMakeVisible (spliceProgressBar);

    // Transport
    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Choose folder for exported files",
                                                           processorRef.getConfigPath ("export_output_dir"));
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc) {
                                      if (fc.getResult() != juce::File{})
                                          processorRef.setConfigPath ("export_output_dir", fc.getResult());
                                  });
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
            fileChooser = std::make_unique<juce::FileChooser> ("Save recording",
                                                               processorRef.getConfigPath ("export_output_dir"),
                                                               "*.wav");
            fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                          | juce::FileBrowserComponent::canSelectFiles,
                                      [this] (const juce::FileChooser& fc) {
                                          if (fc.getResult() != juce::File{})
                                              processorRef.startRecording (fc.getResult());
                                      });
        }
        else
        {
            processorRef.startRecording();
        }
    };

    // Stem separation panel
    formatManager.registerBasicFormats();

    for (auto* e : { &stemInputEditor, &stemModelEditor, &stemOutputEditor })
        e->setReadOnly (true);

    // Model path resolution order:
    // 1. Next to the executable (distribution / installer layout)
    // 2. demucs.onnx/onnx-models/ inside the source tree (dev build, injected by CMake)
    const juce::String modelFilename = "htdemucs.onnx";
    juce::File defaultModel = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                  .getParentDirectory()
                                  .getChildFile (modelFilename);
    if (! defaultModel.existsAsFile())
        defaultModel = juce::File (DEMUCS_ONNX_MODELS_DIR).getChildFile (modelFilename);
    if (defaultModel.existsAsFile())
        stemModelEditor.setText (defaultModel.getFullPathName());

    for (auto* l : { &stemInputLabel, &stemModelLabel, &stemOutputLabel })
    {
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    }
    for (auto* e : { &stemInputEditor, &stemModelEditor, &stemOutputEditor })
        addAndMakeVisible (e);

    addAndMakeVisible (stemInputBrowse);
    addAndMakeVisible (stemModelBrowse);
    addAndMakeVisible (stemOutputBrowse);
    stemInputBrowse.onClick  = [this] { browseForStemInput(); };
    stemModelBrowse.onClick  = [this] { browseForStemModel(); };
    stemOutputBrowse.onClick = [this] { browseForStemOutput(); };

    addAndMakeVisible (stemProcessButton);
    addAndMakeVisible (stemCancelButton);
    stemCancelButton.setEnabled (false);
    stemProcessButton.onClick = [this] { startStemSeparation(); };
    stemCancelButton.onClick  = [this] { cancelStemSeparation(); };

    stemCudaToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (stemCudaToggle);
    addAndMakeVisible (stemProgressBar);

    stemStatusLabel.setText ("Ready", juce::dontSendNotification);
    stemStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (stemStatusLabel);

    drumsWaveform.setWaveformColour  (juce::Colour (0xffe57373));
    bassWaveform.setWaveformColour   (juce::Colour (0xff81c784));
    otherWaveform.setWaveformColour  (juce::Colour (0xffffb74d));
    vocalsWaveform.setWaveformColour (juce::Colour (0xff4fc3f7));
    addAndMakeVisible (drumsWaveform);
    addAndMakeVisible (bassWaveform);
    addAndMakeVisible (otherWaveform);
    addAndMakeVisible (vocalsWaveform);

    updateUIForMode();
    startTimerHz (10);

    setSize (720, 840);
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

    // Stem separation progress
    auto& thread = processorRef.separationThread;
    stemProgressValue = static_cast<double> (thread.getProgress());
    stemStatusLabel.setText (thread.getStatusMessage(), juce::dontSendNotification);
    bool isRunning = thread.isThreadRunning();
    stemProcessButton.setEnabled (! isRunning);
    stemCancelButton.setEnabled (isRunning);

    if (thread.getStatus() == SeparationThread::Status::Complete && ! stemsLoaded)
        loadStemWaveforms();

    // Splice thread polling
    auto& sThread = processorRef.spliceThread;
    spliceProgressValue = static_cast<double> (sThread.getProgress());
    spliceStatusLabel.setText (sThread.getStatusMessage(), juce::dontSendNotification);
    spliceButton.setEnabled (! sThread.isThreadRunning());

    if (sThread.getStatus() == SpliceThread::Status::Complete && ! spliceLoaded)
        loadSpliceOutputWaveform();

    repaint();
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

    // Splice output waveform
    spliceOutputWaveform.setBounds (r.removeFromTop (70).reduced (0, 2));
    r.removeFromTop (3);
    spliceProgressBar.setBounds (r.removeFromTop (16));
    r.removeFromTop (3);
    spliceStatusLabel.setBounds (r.removeFromTop (18));
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

    // Stem separation panel (below mixer)
    r.removeFromTop (10);
    const int stemLabelW = 60, stemBrowseW = 60, stemRowH = 26, stemGap = 3;

    auto stemRow = r.removeFromTop (stemRowH);
    stemInputLabel.setBounds (stemRow.removeFromLeft (stemLabelW));
    stemInputBrowse.setBounds (stemRow.removeFromRight (stemBrowseW));
    stemInputEditor.setBounds (stemRow);
    r.removeFromTop (stemGap);

    stemRow = r.removeFromTop (stemRowH);
    stemModelLabel.setBounds (stemRow.removeFromLeft (stemLabelW));
    stemModelBrowse.setBounds (stemRow.removeFromRight (stemBrowseW));
    stemModelEditor.setBounds (stemRow);
    r.removeFromTop (stemGap);

    stemRow = r.removeFromTop (stemRowH);
    stemOutputLabel.setBounds (stemRow.removeFromLeft (stemLabelW));
    stemOutputBrowse.setBounds (stemRow.removeFromRight (stemBrowseW));
    stemOutputEditor.setBounds (stemRow);
    r.removeFromTop (stemGap);

    stemRow = r.removeFromTop (28);
    stemCudaToggle.setBounds (stemRow.removeFromLeft (80));
    stemRow.removeFromLeft (10);
    stemProcessButton.setBounds (stemRow.removeFromLeft (80));
    stemRow.removeFromLeft (6);
    stemCancelButton.setBounds (stemRow.removeFromLeft (80));
    r.removeFromTop (stemGap);

    stemProgressBar.setBounds (r.removeFromTop (20));
    r.removeFromTop (stemGap);
    stemStatusLabel.setBounds (r.removeFromTop (20));
    r.removeFromTop (stemGap);

    // 2x2 stem waveform grid
    int stemH = (r.getHeight() - stemGap - 25) / 2;
    int halfW  = (r.getWidth() - stemGap) / 2;
    auto waveTop = r.removeFromTop (stemH);
    drumsWaveform.setBounds (waveTop.removeFromLeft (halfW));
    waveTop.removeFromLeft (stemGap);
    bassWaveform.setBounds (waveTop);
    r.removeFromTop (stemGap);
    auto waveBot = r.removeFromTop (stemH);
    otherWaveform.setBounds (waveBot.removeFromLeft (halfW));
    waveBot.removeFromLeft (stemGap);
    vocalsWaveform.setBounds (waveBot);

    inspectButton.setBounds (getLocalBounds().removeFromBottom (25).removeFromRight (90));
}

//==============================================================================
// Drag-and-drop
bool PluginEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" || ext == ".flac")
            return true;
    }
    return false;
}

void PluginEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" || ext == ".flac")
        {
            stemInputEditor.setText (f);
            auto file = juce::File (f);
            auto outputDir = file.getParentDirectory()
                .getChildFile (file.getFileNameWithoutExtension() + "_stems");
            stemOutputEditor.setText (outputDir.getFullPathName());
            break;
        }
    }
}

//==============================================================================
// Stem separation
void PluginEditor::startStemSeparation()
{
    auto inputPath  = stemInputEditor.getText();
    auto modelPath  = stemModelEditor.getText();
    auto outputPath = stemOutputEditor.getText();

    if (inputPath.isEmpty() || modelPath.isEmpty() || outputPath.isEmpty())
    {
        stemStatusLabel.setText ("Select input, model, and output first.", juce::dontSendNotification);
        return;
    }

    stemsLoaded = false;
    drumsWaveform.clear();
    bassWaveform.clear();
    otherWaveform.clear();
    vocalsWaveform.clear();

    processorRef.requestStemSeparation (juce::File (inputPath),
                                        juce::File (modelPath),
                                        juce::File (outputPath),
                                        stemCudaToggle.getToggleState());
}

void PluginEditor::cancelStemSeparation()
{
    processorRef.separationThread.signalThreadShouldExit();
    stemStatusLabel.setText ("Cancelling...", juce::dontSendNotification);
}

void PluginEditor::loadStemWaveforms()
{
    auto outputPath = stemOutputEditor.getText();
    if (outputPath.isEmpty())
        return;
    auto dir = juce::File (outputPath);
    if (dir.getChildFile ("drums.wav").existsAsFile())  drumsWaveform.loadFile  (dir.getChildFile ("drums.wav"));
    if (dir.getChildFile ("bass.wav").existsAsFile())   bassWaveform.loadFile   (dir.getChildFile ("bass.wav"));
    if (dir.getChildFile ("other.wav").existsAsFile())  otherWaveform.loadFile  (dir.getChildFile ("other.wav"));
    if (dir.getChildFile ("vocals.wav").existsAsFile()) vocalsWaveform.loadFile (dir.getChildFile ("vocals.wav"));
    stemsLoaded = true;
}

void PluginEditor::browseForStemInput()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select audio file", juce::File(),
                                                       "*.wav;*.mp3;*.aiff;*.flac");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                stemInputEditor.setText (file.getFullPathName());
                stemOutputEditor.setText (file.getParentDirectory()
                    .getChildFile (file.getFileNameWithoutExtension() + "_stems").getFullPathName());
            }
        });
}

void PluginEditor::browseForStemModel()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select ONNX model", juce::File(), "*.onnx");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
                stemModelEditor.setText (file.getFullPathName());
        });
}

void PluginEditor::browseForStemOutput()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select output directory", juce::File());
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir != juce::File{})
                stemOutputEditor.setText (dir.getFullPathName());
        });
}

//==============================================================================
// Splice remix
void PluginEditor::startSpliceRemix()
{
    auto stemsDir = processorRef.getLastStemOutputDir();
    if (stemsDir == juce::File{})
    {
        spliceStatusLabel.setText ("No stems available — run separation first.",
                                   juce::dontSendNotification);
        return;
    }

    spliceLoaded = false;
    spliceOutputWaveform.clear();

    // Source BPM defaults to 120 until BeatAnalyzer (aubio) is wired in.
    const double sourceBPM = 120.0;
    const double targetBPM = processorRef.getGlobalBPM();

    processorRef.requestSplice (stemsDir, sourceBPM, targetBPM);
}

void PluginEditor::loadSpliceOutputWaveform()
{
    auto outputFile = processorRef.spliceThread.getOutputFile();
    if (outputFile.existsAsFile())
        spliceOutputWaveform.loadFile (outputFile);
    spliceLoaded = true;
}
