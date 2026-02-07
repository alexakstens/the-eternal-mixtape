#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), progressBar (progressValue)
{
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

    setSize (600, 340);
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

    const int rowHeight = 30;
    const int labelWidth = 80;
    const int browseWidth = 70;
    const int spacing = 5;

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

    area.removeFromTop (15);

    // CUDA toggle
    row = area.removeFromTop (rowHeight);
    cudaToggle.setBounds (row.removeFromLeft (200));

    area.removeFromTop (10);

    // Progress bar
    progressBar.setBounds (area.removeFromTop (25));

    area.removeFromTop (10);

    // Buttons row
    row = area.removeFromTop (35);
    processButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    cancelButton.setBounds (row.removeFromLeft (120));

    area.removeFromTop (10);

    // Status row
    row = area.removeFromTop (25);
    statusLabel.setBounds (row.removeFromLeft (row.getWidth() / 2));
    gpuStatusLabel.setBounds (row);

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

    bool isRunning = thread.isThreadRunning();
    processButton.setEnabled (! isRunning);
    cancelButton.setEnabled (isRunning);

    if (threadStatus == SeparationThread::Status::Complete)
    {
        stopTimer();
        statusLabel.setText ("Separation complete!", juce::dontSendNotification);
        progressValue = 1.0;
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
    startTimerHz (15); // Update UI at 15 Hz
}

void PluginEditor::cancelProcessing()
{
    processorRef.separationThread.signalThreadShouldExit();
    statusLabel.setText ("Cancelling...", juce::dontSendNotification);
}
