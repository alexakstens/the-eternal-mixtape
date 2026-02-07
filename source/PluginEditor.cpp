#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), progressBar (progressValue)
{
    formatManager.registerBasicFormats();

    // Labels
    for (auto* label : { &inputLabel, &modelLabel, &outputLabel })
    {
        label->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (label);
    }

    // Path editors
    for (auto* editor : { &inputPathEditor, &modelPathEditor, &outputPathEditor })
    {
        editor->setReadOnly (true);
        addAndMakeVisible (editor);
    }

    // Default model path
    auto cmucsModels = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("MyProjects/MyCppProjects/cmucs/models/htdemucs.onnx");
    if (cmucsModels.existsAsFile())
        modelPathEditor.setText (cmucsModels.getFullPathName());

    // Browse buttons
    addAndMakeVisible (inputBrowseButton);
    addAndMakeVisible (modelBrowseButton);
    addAndMakeVisible (outputBrowseButton);

    inputBrowseButton.onClick = [this] { browseForInput(); };
    modelBrowseButton.onClick = [this] { browseForModel(); };
    outputBrowseButton.onClick = [this] { browseForOutput(); };

    // Process / Cancel
    addAndMakeVisible (processButton);
    addAndMakeVisible (cancelButton);
    cancelButton.setEnabled (false);

    processButton.onClick = [this] { startProcessing(); };
    cancelButton.onClick = [this] { cancelProcessing(); };

    // CUDA toggle
    cudaToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (cudaToggle);

    // Progress bar
    addAndMakeVisible (progressBar);

    // Status labels
    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    gpuStatusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (gpuStatusLabel);

    // Waveform displays
    addAndMakeVisible (inputWaveform);

    stemsLabel.setJustificationType (juce::Justification::centredLeft);
    stemsLabel.setFont (juce::Font (13.0f));
    addAndMakeVisible (stemsLabel);

    drumsWaveform.setWaveformColour (juce::Colour (0xffe57373));
    bassWaveform.setWaveformColour (juce::Colour (0xff81c784));
    otherWaveform.setWaveformColour (juce::Colour (0xffffb74d));
    vocalsWaveform.setWaveformColour (juce::Colour (0xff4fc3f7));

    addAndMakeVisible (drumsWaveform);
    addAndMakeVisible (bassWaveform);
    addAndMakeVisible (otherWaveform);
    addAndMakeVisible (vocalsWaveform);

    // Inspector button (bottom-right, small)
    addAndMakeVisible (inspectButton);
    inspectButton.onClick = [this]
    {
        if (! inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible (true);
    };

    setSize (600, 580);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawText ("Demucs Source Separator", getLocalBounds().removeFromTop (40),
                juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (15);
    area.removeFromTop (35); // title space

    const int rowHeight = 28;
    const int labelWidth = 80;
    const int browseWidth = 70;
    const int spacing = 4;

    // Input file row
    auto row = area.removeFromTop (rowHeight);
    inputLabel.setBounds (row.removeFromLeft (labelWidth));
    row.removeFromLeft (spacing);
    inputBrowseButton.setBounds (row.removeFromRight (browseWidth));
    row.removeFromRight (spacing);
    inputPathEditor.setBounds (row);

    area.removeFromTop (spacing);

    // Model file row
    row = area.removeFromTop (rowHeight);
    modelLabel.setBounds (row.removeFromLeft (labelWidth));
    row.removeFromLeft (spacing);
    modelBrowseButton.setBounds (row.removeFromRight (browseWidth));
    row.removeFromRight (spacing);
    modelPathEditor.setBounds (row);

    area.removeFromTop (spacing);

    // Output dir row
    row = area.removeFromTop (rowHeight);
    outputLabel.setBounds (row.removeFromLeft (labelWidth));
    row.removeFromLeft (spacing);
    outputBrowseButton.setBounds (row.removeFromRight (browseWidth));
    row.removeFromRight (spacing);
    outputPathEditor.setBounds (row);

    area.removeFromTop (10);

    // CUDA toggle + buttons on the same row
    row = area.removeFromTop (30);
    cudaToggle.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (20);
    processButton.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (8);
    cancelButton.setBounds (row.removeFromLeft (100));

    area.removeFromTop (8);

    // Input waveform
    inputWaveform.setBounds (area.removeFromTop (70));

    area.removeFromTop (4);

    // Progress bar
    progressBar.setBounds (area.removeFromTop (22));

    area.removeFromTop (4);

    // Status row
    row = area.removeFromTop (22);
    statusLabel.setBounds (row.removeFromLeft (row.getWidth() / 2));
    gpuStatusLabel.setBounds (row);

    area.removeFromTop (8);

    // Stems section
    stemsLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    // 2x2 grid for stem waveforms
    int stemHeight = (area.getHeight() - spacing - 25) / 2; // leave room for inspector
    int halfWidth = (area.getWidth() - spacing) / 2;

    auto topRow = area.removeFromTop (stemHeight);
    drumsWaveform.setBounds (topRow.removeFromLeft (halfWidth));
    topRow.removeFromLeft (spacing);
    bassWaveform.setBounds (topRow);

    area.removeFromTop (spacing);

    auto bottomRow = area.removeFromTop (stemHeight);
    otherWaveform.setBounds (bottomRow.removeFromLeft (halfWidth));
    bottomRow.removeFromLeft (spacing);
    vocalsWaveform.setBounds (bottomRow);

    // Inspector button (bottom-right corner)
    inspectButton.setBounds (getLocalBounds().removeFromBottom (25).removeFromRight (80));
}

void PluginEditor::timerCallback()
{
    updateUI();
}

void PluginEditor::updateUI()
{
    auto& thread = processorRef.separationThread;
    auto threadStatus = thread.getStatus();

    progressValue = static_cast<double> (thread.getProgress());
    statusLabel.setText (thread.getStatusMessage(), juce::dontSendNotification);

    auto etaMsg = thread.getETAMessage();
    if (etaMsg.isNotEmpty())
        statusLabel.setText (thread.getStatusMessage() + "  " + etaMsg,
                             juce::dontSendNotification);

    bool isRunning = thread.isThreadRunning();
    processButton.setEnabled (! isRunning);
    cancelButton.setEnabled (isRunning);

    if (threadStatus == SeparationThread::Status::Complete)
    {
        stopTimer();
        statusLabel.setText (thread.getStatusMessage(), juce::dontSendNotification);
        progressValue = 1.0;

        if (! stemsLoaded)
            loadStemWaveforms();
    }
    else if (threadStatus == SeparationThread::Status::Error)
    {
        stopTimer();
        statusLabel.setText (thread.getErrorMessage(), juce::dontSendNotification);
    }

    if (thread.isGpuEnabled())
        gpuStatusLabel.setText ("GPU: CUDA Enabled", juce::dontSendNotification);
    else if (isRunning)
        gpuStatusLabel.setText ("GPU: CPU Only", juce::dontSendNotification);

    repaint();
}

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
            inputPathEditor.setText (f);

            // Auto-set output dir to input_file_stems/
            auto inputFile = juce::File (f);
            auto outputDir = inputFile.getParentDirectory()
                .getChildFile (inputFile.getFileNameWithoutExtension() + "_stems");
            outputPathEditor.setText (outputDir.getFullPathName());

            loadInputWaveform (inputFile);
            break;
        }
    }
}

void PluginEditor::browseForInput()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select audio file...", juce::File(),
        "*.wav;*.mp3;*.aiff;*.flac");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                inputPathEditor.setText (file.getFullPathName());

                // Auto-set output dir
                auto outputDir = file.getParentDirectory()
                    .getChildFile (file.getFileNameWithoutExtension() + "_stems");
                outputPathEditor.setText (outputDir.getFullPathName());

                loadInputWaveform (file);
            }
        });
}

void PluginEditor::browseForModel()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select ONNX model...", juce::File(), "*.onnx");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
                modelPathEditor.setText (file.getFullPathName());
        });
}

void PluginEditor::browseForOutput()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select output directory...", juce::File());

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir.isDirectory() || ! dir.exists())
                outputPathEditor.setText (dir.getFullPathName());
        });
}

void PluginEditor::startProcessing()
{
    auto inputPath = inputPathEditor.getText();
    auto modelPath = modelPathEditor.getText();
    auto outputPath = outputPathEditor.getText();

    if (inputPath.isEmpty() || modelPath.isEmpty() || outputPath.isEmpty())
    {
        statusLabel.setText ("Please select input, model, and output.", juce::dontSendNotification);
        return;
    }

    // Clear previous stem waveforms
    stemsLoaded = false;
    drumsWaveform.clear();
    bassWaveform.clear();
    otherWaveform.clear();
    vocalsWaveform.clear();

    auto& thread = processorRef.separationThread;
    thread.configure (
        juce::File (inputPath),
        juce::File (modelPath),
        juce::File (outputPath),
        cudaToggle.getToggleState()
    );

    progressValue = 0.0;
    gpuStatusLabel.setText ("", juce::dontSendNotification);
    thread.startThread();
    startTimerHz (15);
}

void PluginEditor::cancelProcessing()
{
    processorRef.separationThread.signalThreadShouldExit();
    statusLabel.setText ("Cancelling...", juce::dontSendNotification);
}

void PluginEditor::loadInputWaveform (const juce::File& file)
{
    inputWaveform.loadFile (file);
}

void PluginEditor::loadStemWaveforms()
{
    auto outputPath = outputPathEditor.getText();
    if (outputPath.isEmpty())
        return;

    auto outputDir = juce::File (outputPath);

    auto drumsFile  = outputDir.getChildFile ("drums.wav");
    auto bassFile   = outputDir.getChildFile ("bass.wav");
    auto otherFile  = outputDir.getChildFile ("other.wav");
    auto vocalsFile = outputDir.getChildFile ("vocals.wav");

    if (drumsFile.existsAsFile())  drumsWaveform.loadFile (drumsFile);
    if (bassFile.existsAsFile())   bassWaveform.loadFile (bassFile);
    if (otherFile.existsAsFile())  otherWaveform.loadFile (otherFile);
    if (vocalsFile.existsAsFile()) vocalsWaveform.loadFile (vocalsFile);

    stemsLoaded = true;
}
