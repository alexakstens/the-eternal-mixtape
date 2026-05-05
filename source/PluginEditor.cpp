#include "PluginEditor.h"

//==============================================================================
// Helper: load SVG states onto a DrawableButton (clears JUCE button background)
static void applySVGImages (juce::DrawableButton& btn,
                             const void* normalData,   int normalSize,
                             const void* overData,     int overSize,
                             const void* downData,     int downSize,
                             const void* normalOnData  = nullptr, int normalOnSize  = 0,
                             const void* overOnData    = nullptr, int overOnSize    = 0)
{
    auto mk = [] (const void* d, int s) -> std::unique_ptr<juce::Drawable>
    {
        if (d && s > 0)
            return juce::Drawable::createFromImageData (d, (size_t) s);
        return {};
    };

    auto normal   = mk (normalData,   normalSize);
    auto over     = mk (overData,     overSize);
    auto down     = mk (downData,     downSize);
    auto normalOn = mk (normalOnData, normalOnSize);
    auto overOn   = mk (overOnData,   overOnSize);

    btn.setImages (normal.get(), over.get(), down.get(), nullptr,
                   normalOn.get(), overOn.get(), nullptr, nullptr);

    btn.setColour (juce::DrawableButton::backgroundColourId,   juce::Colours::transparentBlack);
    btn.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Background image — single source of truth for the UI's visual layout.
    // Aspect ratio matches the 1024x768 window (4:3); stretch-to-fit in paint().
    uiImage = juce::ImageCache::getFromMemory (BinaryData::Backgorund_png,
                                               BinaryData::Backgorund_pngSize);

    // Tape title art — band below baked-in header copy (see resized()).
    {
        auto tape = juce::ImageCache::getFromMemory (BinaryData::tape_png,
                                                     BinaryData::tape_pngSize);
        tapeHeadlineImage.setImage (tape, juce::RectanglePlacement::centred
                                          | juce::RectanglePlacement::onlyReduceInSize);
        addAndMakeVisible (tapeHeadlineImage); // behind later controls (added first)
    }

    addAndMakeVisible (inspectButton);

    inspectButton.onClick = [this] {
        if (! inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible (true);
    };

    // Runtime LED — sits in the orange digital-display slot baked into the cassette art.
    auto runtimeFont = juce::Font (juce::FontOptions ("Menlo", kRuntimeFontPt, juce::Font::bold));
    runtimeLabel.setJustificationType (juce::Justification::centred);
    runtimeLabel.setFont (runtimeFont);
    runtimeLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffa030));
    addAndMakeVisible (runtimeLabel);

    runtimeCaptionLabel.setJustificationType (juce::Justification::centred);
    runtimeCaptionLabel.setFont (runtimeFont);
    addAndMakeVisible (runtimeCaptionLabel);

    // VU meter label is no longer drawn — kept for backward compat but invisible.
    meterLabel.setText ("VU", juce::dontSendNotification);
    meterLabel.setVisible (false);

    static const char* const kTrackTitles[kNumTracks] =
        { "Track A", "Track B", "Track C", "Track D" };

    // ── Track label icons (trackA/B/C/D.png) ──
    {
        const struct { const void* data; int size; } trackIconAssets[kNumTracks] = {
            { BinaryData::trackA_png, BinaryData::trackA_pngSize },
            { BinaryData::trackB_png, BinaryData::trackB_pngSize },
            { BinaryData::trackC_png, BinaryData::trackC_pngSize },
            { BinaryData::trackD_png, BinaryData::trackD_pngSize },
        };
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto img = juce::ImageCache::getFromMemory (trackIconAssets[t].data,
                                                        trackIconAssets[t].size);
            trackLabelIcons[t].setName (kTrackTitles[t]);
            // Scale header art to fit (same contain behaviour as stem icons)
            trackLabelIcons[t].setImage (img, juce::RectanglePlacement::centred
                                              | juce::RectanglePlacement::onlyReduceInSize);
            addAndMakeVisible (trackLabelIcons[t]);
        }
    }

    // ── Per-track input waveforms (one per track, always visible) ──
    {
        const juce::uint32 trackColours[kNumTracks] = {
            0xff64b5f6, // Track A — blue
            0xffef9a9a, // Track B — red-pink
            0xffa5d6a7, // Track C — green
            0xffffcc80, // Track D — amber
        };
        for (int t = 0; t < kNumTracks; ++t)
        {
            trackInputWaveforms[t] = std::make_unique<WaveformDisplay> (formatManager,
                                                                         kTrackTitles[t]);
            trackInputWaveforms[t]->setName (kTrackTitles[t]);
            trackInputWaveforms[t]->setWaveformColour (juce::Colour (trackColours[t]));
            addAndMakeVisible (*trackInputWaveforms[t]);
        }
    }

    // ── Stem icons above each fader: vocal / bass / others / drum ──
    {
        const struct { const void* data; int size; } stemIconAssets[kNumStemsPerTrack] = {
            { BinaryData::vocal_png,  BinaryData::vocal_pngSize  },
            { BinaryData::bass_png,   BinaryData::bass_pngSize   },
            { BinaryData::others_png, BinaryData::others_pngSize },
            { BinaryData::drum_png,   BinaryData::drum_pngSize   },
        };
        static const char* const kStemIconNames[kNumStemsPerTrack] =
            { "Vocals", "Bass", "Other", "Drums" };
        for (int t = 0; t < kNumTracks; ++t)
        {
            for (int s = 0; s < kNumStemsPerTrack; ++s)
            {
                auto img = juce::ImageCache::getFromMemory (stemIconAssets[s].data,
                                                            stemIconAssets[s].size);
                stemIcons[t][s].setName (juce::String (kTrackTitles[t]) + " — "
                                         + kStemIconNames[s]);
                // Full artwork, uniformly scaled down to fit the icon slot (no clipping)
                stemIcons[t][s].setImage (
                    img, juce::RectanglePlacement::centred
                             | juce::RectanglePlacement::onlyReduceInSize);
                addAndMakeVisible (stemIcons[t][s]);
            }
        }
    }

    // ── Tracks A–D: 4 vertical stem faders each ──
    for (int i = 0; i < kNumTracks; ++i)
    {
        for (int s = 0; s < kNumStemsPerTrack; ++s)
        {
            auto& sl = trackStemSliders[i][s];
            sl.setSliderStyle (juce::Slider::LinearVertical);
            sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            sl.setRange (0.0, 2.0, 0.01);
            sl.setValue (1.0);
            sl.onValueChange = [this, i, s] {
                processorRef.setTrackStemGain (i, s, (float) trackStemSliders[i][s].getValue());
            };
            addAndMakeVisible (sl);
        }
    }

    // Splice panel — Razor button + Skip Warp toggle
    applySVGImages (spliceButton,
                    BinaryData::Razor_off_svg,   BinaryData::Razor_off_svgSize,
                    BinaryData::Razor_hover_svg, BinaryData::Razor_hover_svgSize,
                    BinaryData::Razor_click_svg, BinaryData::Razor_click_svgSize);
    addAndMakeVisible (spliceButton);
    spliceButton.onClick = [this] { startSpliceRemix(); };

    skipWarpToggle.setToggleState (false, juce::dontSendNotification);
    skipWarpToggle.setTooltip ("Bypass time-stretching — mix stems at original tempo");
    addChildComponent (skipWarpToggle); // hidden — exposed via Expertise mode later
    bpmSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 16);
    bpmSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                   juce::MathConstants<float>::pi * 2.8f, true);
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
    // Splice status + progress — not in V1.1 layout; hidden but kept wired for diagnostics.
    addChildComponent (spliceStatusLabel);
    addChildComponent (spliceProgressBar);

    // Splice output transport buttons
    applySVGImages (spliceBackBtn,
                    BinaryData::Back_off_svg,   BinaryData::Back_off_svgSize,
                    BinaryData::Back_hover_svg, BinaryData::Back_hover_svgSize,
                    BinaryData::Back_on_svg,    BinaryData::Back_on_svgSize);
    spliceBackBtn.onClick = [this] {
        processorRef.rewindSpliceOutput();
    };
    addChildComponent (spliceBackBtn); // hidden — main transport reused in V1.1 layout

    applySVGImages (splicePlayBtn,
                    BinaryData::Play_off_svg,   BinaryData::Play_off_svgSize,
                    BinaryData::Play_hover_svg, BinaryData::Play_hover_svgSize,
                    BinaryData::Play_on_svg,    BinaryData::Play_on_svgSize,
                    BinaryData::Play_on_svg,    BinaryData::Play_on_svgSize,    // normalOn (playing)
                    BinaryData::Play_hover_svg, BinaryData::Play_hover_svgSize); // overOn
    splicePlayBtn.setClickingTogglesState (true);
    splicePlayBtn.onClick = [this] {
        if (splicePlayBtn.getToggleState())
            processorRef.playSpliceOutput();
        else
            processorRef.stopSpliceOutput();
    };
    addChildComponent (splicePlayBtn);

    applySVGImages (spliceStopBtn,
                    BinaryData::Stop_off_svg,   BinaryData::Stop_off_svgSize,
                    BinaryData::Stop_hover_svg, BinaryData::Stop_hover_svgSize,
                    BinaryData::Stop_on_svg,    BinaryData::Stop_on_svgSize);
    spliceStopBtn.onClick = [this] {
        processorRef.stopSpliceOutput();
        processorRef.rewindSpliceOutput();
        splicePlayBtn.setToggleState (false, juce::dontSendNotification);
    };
    addChildComponent (spliceStopBtn);

    applySVGImages (spliceForwardBtn,
                    BinaryData::Forward_off_svg,   BinaryData::Forward_off_svgSize,
                    BinaryData::Forward_hover_svg, BinaryData::Forward_hover_svgSize,
                    BinaryData::Forward_on_svg,    BinaryData::Forward_on_svgSize);
    spliceForwardBtn.onClick = [this] {
        auto len = processorRef.getSpliceOutputLengthSeconds();
        if (len > 0.0)
            processorRef.seekSpliceOutput (len);
    };
    addChildComponent (spliceForwardBtn);

    applySVGImages (spliceLoopBtn,
                    BinaryData::Loop_off_svg,   BinaryData::Loop_off_svgSize,
                    BinaryData::Loop_hover_svg, BinaryData::Loop_hover_svgSize,
                    BinaryData::Loop_on_svg,    BinaryData::Loop_on_svgSize,
                    BinaryData::Loop_on_svg,    BinaryData::Loop_on_svgSize,    // normalOn
                    BinaryData::Loop_hover_svg, BinaryData::Loop_hover_svgSize); // overOn
    spliceLoopBtn.setClickingTogglesState (true);
    spliceLoopBtn.onClick = [this] {
        processorRef.setSpliceOutputLoop (spliceLoopBtn.getToggleState());
    };
    addChildComponent (spliceLoopBtn);

    // Main transport — DrawableButton + SVG (mirrors the splice transport state machine)
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

    // Rewind: jump to start (momentary)
    applySVGImages (mainRewindBtn,
                    BinaryData::Back_off_svg,   BinaryData::Back_off_svgSize,
                    BinaryData::Back_hover_svg, BinaryData::Back_hover_svgSize,
                    BinaryData::Back_on_svg,    BinaryData::Back_on_svgSize);
    mainRewindBtn.onClick = [this] {
        processorRef.stop();
        processorRef.setTransportPosition (0.0);
        mainPlayBtn.setToggleState (false, juce::dontSendNotification);
    };
    addAndMakeVisible (mainRewindBtn);

    // Play: toggle (off = stopped, on = playing)
    applySVGImages (mainPlayBtn,
                    BinaryData::Play_off_svg,   BinaryData::Play_off_svgSize,
                    BinaryData::Play_hover_svg, BinaryData::Play_hover_svgSize,
                    BinaryData::Play_on_svg,    BinaryData::Play_on_svgSize,
                    BinaryData::Play_on_svg,    BinaryData::Play_on_svgSize,
                    BinaryData::Play_hover_svg, BinaryData::Play_hover_svgSize);
    mainPlayBtn.setClickingTogglesState (true);
    mainPlayBtn.onClick = [this] {
        if (mainPlayBtn.getToggleState())
        {
            if (processorRef.getTransportTotalLengthSeconds() > 0.0)
                processorRef.play();
            else
                mainPlayBtn.setToggleState (false, juce::dontSendNotification);
        }
        else
        {
            processorRef.stop();
        }
    };
    addAndMakeVisible (mainPlayBtn);

    // Stop: stop + return to head (momentary)
    applySVGImages (mainStopBtn,
                    BinaryData::Stop_off_svg,   BinaryData::Stop_off_svgSize,
                    BinaryData::Stop_hover_svg, BinaryData::Stop_hover_svgSize,
                    BinaryData::Stop_on_svg,    BinaryData::Stop_on_svgSize);
    mainStopBtn.onClick = [this] {
        processorRef.stop();
        processorRef.setTransportPosition (0.0);
        mainPlayBtn.setToggleState (false, juce::dontSendNotification);
    };
    addAndMakeVisible (mainStopBtn);

    // Forward: jump to end (momentary) — fixes the previous bug where this called stop()
    applySVGImages (mainForwardBtn,
                    BinaryData::Forward_off_svg,   BinaryData::Forward_off_svgSize,
                    BinaryData::Forward_hover_svg, BinaryData::Forward_hover_svgSize,
                    BinaryData::Forward_on_svg,    BinaryData::Forward_on_svgSize);
    mainForwardBtn.onClick = [this] {
        if (processorRef.getTransportTotalLengthSeconds() > 0.0)
            processorRef.setTransportPosition (1.0);
    };
    addAndMakeVisible (mainForwardBtn);

    // Loop: toggle (off = single play, on = loop)
    applySVGImages (mainLoopBtn,
                    BinaryData::Loop_off_svg,   BinaryData::Loop_off_svgSize,
                    BinaryData::Loop_hover_svg, BinaryData::Loop_hover_svgSize,
                    BinaryData::Loop_on_svg,    BinaryData::Loop_on_svgSize,
                    BinaryData::Loop_on_svg,    BinaryData::Loop_on_svgSize,
                    BinaryData::Loop_hover_svg, BinaryData::Loop_hover_svgSize);
    mainLoopBtn.setClickingTogglesState (true);
    mainLoopBtn.onClick = [this] {
        processorRef.setLoopEnabled (mainLoopBtn.getToggleState());
    };
    addAndMakeVisible (mainLoopBtn);

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

    // Stem-separation output waveforms — only shown in Expertise mode overlay.
    drumsWaveform.setWaveformColour  (juce::Colour (0xffe57373));
    bassWaveform.setWaveformColour   (juce::Colour (0xff81c784));
    otherWaveform.setWaveformColour  (juce::Colour (0xffffb74d));
    vocalsWaveform.setWaveformColour (juce::Colour (0xff4fc3f7));
    addAndMakeVisible (drumsWaveform);
    addAndMakeVisible (bassWaveform);
    addAndMakeVisible (otherWaveform);
    addAndMakeVisible (vocalsWaveform);

    // Colour theme — sampled from Backgorund.png (warm vintage palette).
    const juce::Colour textColour   (0xffc8b896);
    const juce::Colour accentColour (0xffF07030);
    const juce::Colour panelDark    (0xff555555);

    for (int i = 0; i < kNumTracks; ++i)
    {
        for (int s = 0; s < kNumStemsPerTrack; ++s)
        {
            trackStemSliders[i][s].setColour (juce::Slider::thumbColourId, accentColour);
            trackStemSliders[i][s].setColour (juce::Slider::trackColourId, panelDark);
        }
    }
    bpmLabel.setColour          (juce::Label::textColourId, textColour);
    densityLabel.setColour      (juce::Label::textColourId, textColour);
    runtimeLabel.setColour      (juce::Label::textColourId, juce::Colour (0xffffa030));
    runtimeCaptionLabel.setColour (juce::Label::textColourId, textColour);
    meterLabel.setColour        (juce::Label::textColourId, textColour);
    spliceStatusLabel.setColour (juce::Label::textColourId, textColour);
    stemStatusLabel.setColour   (juce::Label::textColourId, textColour);

    bpmSlider.setColour     (juce::Slider::thumbColourId,             accentColour);
    bpmSlider.setColour     (juce::Slider::rotarySliderFillColourId,  accentColour);
    bpmSlider.setColour     (juce::Slider::textBoxTextColourId,       textColour);
    bpmSlider.setColour     (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    densitySlider.setColour (juce::Slider::thumbColourId,             accentColour);
    densitySlider.setColour (juce::Slider::trackColourId,             panelDark);

    autoSpliceButton.setColour (juce::TextButton::buttonColourId, accentColour);
    recButton.setColour        (juce::TextButton::buttonColourId, juce::Colour (0xffc83c2c));

    // Stem-separation panel is an "advanced" feature — hidden by default,
    // revealed when the user holds Cmd/Ctrl (Expertise Mode).
    setStemPanelVisible_ = [this] (bool v) {
        // Stem separation controls + stem output waveforms — only in Expertise mode.
        stemInputLabel.setVisible (v);
        stemModelLabel.setVisible (v);
        stemOutputLabel.setVisible (v);
        stemInputEditor.setVisible (v);
        stemModelEditor.setVisible (v);
        stemOutputEditor.setVisible (v);
        stemInputBrowse.setVisible (v);
        stemModelBrowse.setVisible (v);
        stemOutputBrowse.setVisible (v);
        stemProcessButton.setVisible (v);
        stemCancelButton.setVisible (v);
        stemCudaToggle.setVisible (v);
        stemProgressBar.setVisible (v);
        stemStatusLabel.setVisible (v);
        // Stem separation output waveforms shown in Expertise mode overlay (2×2 grid).
        drumsWaveform.setVisible (v);
        bassWaveform.setVisible (v);
        otherWaveform.setVisible (v);
        vocalsWaveform.setVisible (v);
    };
    setStemPanelVisible_ (false);

    updateUIForMode();
    startTimerHz (10);

    setSize (1024, 768);
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
    juce::ignoreUnused (processorRef.getMasterLevels()); // VU rendering removed; poll preserved

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

    // Sync play button toggle with actual transport state
    bool isPlaying = processorRef.isSpliceOutputPlaying();
    if (splicePlayBtn.getToggleState() != isPlaying)
        splicePlayBtn.setToggleState (isPlaying, juce::dontSendNotification);

    // Same for the main transport — playback may stop on its own (end-of-buffer, DAW)
    bool mainIsPlaying = processorRef.isTransportPlaying();
    if (mainPlayBtn.getToggleState() != mainIsPlaying)
        mainPlayBtn.setToggleState (mainIsPlaying, juce::dontSendNotification);

    repaint();
}

void PluginEditor::updateUIForMode()
{
    if (isExpertiseMode_)
    {
        recButton.setButtonText ("REC (choose file)");
        if (setStemPanelVisible_) setStemPanelVisible_ (true);
    }
    else
    {
        recButton.setButtonText ("REC");
        if (setStemPanelVisible_) setStemPanelVisible_ (false);
    }
    resized();
}

void PluginEditor::paint (juce::Graphics& g)
{
    if (uiImage.isValid())
    {
        g.drawImageWithin (uiImage, 0, 0, getWidth(), getHeight(),
                           juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    // No VU overlay here — the multicolor waveform strip is baked into the background art,
    // and the live level is reflected by spliceOutputWaveform.
}

void PluginEditor::resized()
{
    // Tape headline — y offset below top, 72 px side margins, 240 px tall band
    constexpr int kTapeHeadlinePad = 72;
    constexpr int kTapeHeadlineY   = 90; // clear baked-in header copy above the tape art
    constexpr int kTapeHeadlineH   = 240;
    tapeHeadlineImage.setBounds (kTapeHeadlinePad, kTapeHeadlineY,
                                 juce::jmax (0, getWidth() - 2 * kTapeHeadlinePad),
                                 kTapeHeadlineH);

    // ── Layout constants ─────────────────────────────────────────────────────────
    // Left gutter: SPLICE razor + BPM knob (x = 0..119)
    constexpr int kGutterW = 120;
    // Pinch the four-track band horizontally — columns sit closer together and the
    // whole A–D group is shifted toward the window centre.
    constexpr int kTrackBankPinch = 48;
    constexpr int kTrack0X      = kGutterW + kTrackBankPinch;
    constexpr int kTrackBankW   = (1024 - kGutterW) - 2 * kTrackBankPinch;
    constexpr int kColW         = kTrackBankW / kNumTracks;
    // Nudge track headers + stem icons + faders (not waveforms; not SPLICE/BPM vertical position).
    constexpr int kTrackControlsShiftX = 5;
    constexpr int kTrackControlsShiftY = 10; // not applied to SPLICE razor / BPM row

    // ── Y zones (top of mixer area at y=356) ──────────────────────────────────
    // Per-track input waveform windows (one per track, always visible)
    constexpr int kWaveY = 356, kWaveH = 56;
    for (int t = 0; t < kNumTracks; ++t)
    {
        if (trackInputWaveforms[t])
            trackInputWaveforms[t]->setBounds (kTrack0X + t*kColW + 1, kWaveY, kColW - 2, kWaveH);
    }

    // Runtime LED — far left, in the margin to the left of Track A’s waveform (+20 px vs art slot).
    // Two equal-height rows (time + RUNTIME), horizontally centred, share 24 pt Menlo bold.
    constexpr int kRuntimeX     = 32;
    constexpr int kRuntimeRight = kTrack0X - 8; // small gap before Track A
    constexpr int kRuntimeW     = kRuntimeRight - kRuntimeX;
    constexpr int kRuntimeCapGap = 2;
    constexpr int kRuntimeLineH = (kWaveH - kRuntimeCapGap) / 2;
    runtimeLabel.setBounds (kRuntimeX, kWaveY, kRuntimeW, kRuntimeLineH);
    runtimeCaptionLabel.setBounds (kRuntimeX, kWaveY + kRuntimeLineH + kRuntimeCapGap,
                                   kRuntimeW, kRuntimeLineH);

    // Splice output waveform — width matches the pinched track bank
    constexpr int kSpliceY = kWaveY + kWaveH + 4; // 416
    spliceOutputWaveform.setBounds (kTrack0X, kSpliceY, kTrackBankW, 34);

    // Track label icons (trackA/B/C/D.png) — below splice output
    constexpr int kIconYBase = kSpliceY + 34 + 4;
    constexpr int kIconY     = kIconYBase + kTrackControlsShiftY;
    constexpr int kIconH = 28;
    for (int t = 0; t < kNumTracks; ++t)
        trackLabelIcons[t].setBounds (kTrack0X + t*kColW + 4 + kTrackControlsShiftX, kIconY,
                                      kColW - 8, kIconH);

    // Stem icons (vocal/bass/others/drum) — one per fader, above the fader
    constexpr int kStemIconY = kIconY + kIconH + 2;
    constexpr int kStemIconH = 28; // tall enough for scaled full artwork
    constexpr int kFaderW    = 44, kFaderGap = 3;
    // Side padding inside each column pulls Track A’s left edge right and Track D’s right edge left
    constexpr int kColHPad   = 10;
    constexpr int kGroupW    = kNumStemsPerTrack * kFaderW + (kNumStemsPerTrack-1) * kFaderGap;
    for (int t = 0; t < kNumTracks; ++t)
    {
        const int gx = kTrack0X + t*kColW + kColHPad + kTrackControlsShiftX
                       + (kColW - 2*kColHPad - kGroupW) / 2;
        for (int s = 0; s < kNumStemsPerTrack; ++s)
            stemIcons[t][s].setBounds (gx + s*(kFaderW + kFaderGap), kStemIconY,
                                       kFaderW, kStemIconH);
    }

    // Stem faders — vertical, shorter than before
    constexpr int kFaderY = kStemIconY + kStemIconH + 2;
    constexpr int kFaderH = 88;
    // SPLICE razor + BPM: original row (no kTrackControlsShiftY) — only headers/icons/sliders shift
    constexpr int kFaderYAlignGutter = kIconYBase + kIconH + 2 + kStemIconH + 2;
    for (int t = 0; t < kNumTracks; ++t)
    {
        const int gx = kTrack0X + t*kColW + kColHPad + kTrackControlsShiftX
                       + (kColW - 2*kColHPad - kGroupW) / 2;
        for (int s = 0; s < kNumStemsPerTrack; ++s)
            trackStemSliders[t][s].setBounds (gx + s*(kFaderW + kFaderGap),
                                              kFaderY, kFaderW, kFaderH);
    }

    // ── Left gutter controls ──────────────────────────────────────────────────
    // SPLICE razor button — vertically aligned with the unshifted fader row (track sliders may sit lower)
    spliceButton.setBounds (48, kFaderYAlignGutter - 12, 100, 100);

    // BPM rotary — below splice button
    constexpr int kBpmY = kFaderYAlignGutter + kFaderH + 4;
    bpmLabel.setBounds  (8,  kBpmY,      104, 18);
    bpmSlider.setBounds (12, kBpmY + 18,  96, 76);

    // Density (hidden — parked off-screen)
    densityLabel.setBounds  (0, 759, 1, 1);
    densitySlider.setBounds (0, 759, 1, 1);

    // ── Bottom controls ───────────────────────────────────────────────────────
    constexpr int kBottomY = 708;

    // Bottom-left: gear + loop
    settingsButton.setBounds (8,  kBottomY, 46, 44);
    mainLoopBtn.setBounds    (58, kBottomY, 40, 40);

    // Centre transport — 4 buttons evenly spaced
    constexpr int kTW = 68, kTH = 50, kTGap = 8;
    constexpr int kTTotal = kNumTracks * kTW + (kNumTracks-1) * kTGap; // 296
    constexpr int kTX0    = (1024 - kTTotal) / 2;                      // 364
    mainRewindBtn.setBounds  (kTX0 + 0*(kTW+kTGap), kBottomY, kTW, kTH);
    mainStopBtn.setBounds    (kTX0 + 1*(kTW+kTGap), kBottomY, kTW, kTH);
    mainPlayBtn.setBounds    (kTX0 + 2*(kTW+kTGap), kBottomY, kTW, kTH);
    mainForwardBtn.setBounds (kTX0 + 3*(kTW+kTGap), kBottomY, kTW, kTH);

    // Bottom-right 2×2: AUTO SPLICE / REGENERATE / RANDOMIZE / REC
    constexpr int kRW = 120, kRH = 36, kRGap = 5;
    constexpr int kRX1 = 716, kRX2 = kRX1 + kRW + kRGap;
    constexpr int kRY1 = kBottomY, kRY2 = kRY1 + kRH + kRGap;
    autoSpliceButton.setBounds (kRX1, kRY1, kRW, kRH);
    regenerateButton.setBounds (kRX2, kRY1, kRW, kRH);
    randomizeButton.setBounds  (kRX1, kRY2, kRW, kRH);
    recButton.setBounds        (kRX2, kRY2, kRW, kRH);

    // ── Expertise mode: stem panel overlay (Cmd held) ─────────────────────────
    if (isExpertiseMode_)
    {
        auto stemR = juce::Rectangle<int> (kTrack0X, 22, kTrackBankW, 330);
        const int labelW = 60, browseW = 70, rowH = 26, gap = 4;

        auto row1 = stemR.removeFromTop (rowH);
        stemInputLabel.setBounds  (row1.removeFromLeft  (labelW));
        stemInputBrowse.setBounds (row1.removeFromRight (browseW));
        stemInputEditor.setBounds (row1);
        stemR.removeFromTop (gap);

        auto row2 = stemR.removeFromTop (rowH);
        stemModelLabel.setBounds  (row2.removeFromLeft  (labelW));
        stemModelBrowse.setBounds (row2.removeFromRight (browseW));
        stemModelEditor.setBounds (row2);
        stemR.removeFromTop (gap);

        auto row3 = stemR.removeFromTop (rowH);
        stemOutputLabel.setBounds  (row3.removeFromLeft  (labelW));
        stemOutputBrowse.setBounds (row3.removeFromRight (browseW));
        stemOutputEditor.setBounds (row3);
        stemR.removeFromTop (gap);

        auto ctlRow = stemR.removeFromTop (28);
        stemCudaToggle.setBounds    (ctlRow.removeFromLeft (80));
        ctlRow.removeFromLeft (8);
        stemProcessButton.setBounds (ctlRow.removeFromLeft (90));
        ctlRow.removeFromLeft (4);
        stemCancelButton.setBounds  (ctlRow.removeFromLeft (90));
        ctlRow.removeFromLeft (12);
        stemProgressBar.setBounds   (ctlRow.removeFromLeft (200));
        stemR.removeFromTop (gap);

        stemStatusLabel.setBounds (stemR.removeFromTop (20));
        stemR.removeFromTop (gap);

        // 2×2 stem-separation output waveform grid
        const int wH = (stemR.getHeight() - gap) / 2;
        const int wW = (stemR.getWidth()  - gap) / 2;
        auto t1 = stemR.removeFromTop (wH);
        drumsWaveform.setBounds (t1.removeFromLeft (wW));
        t1.removeFromLeft (gap);
        bassWaveform.setBounds  (t1);
        stemR.removeFromTop (gap);
        auto t2 = stemR.removeFromTop (wH);
        otherWaveform.setBounds (t2.removeFromLeft (wW));
        t2.removeFromLeft (gap);
        vocalsWaveform.setBounds (t2);
    }

    inspectButton.setBounds (getLocalBounds().removeFromBottom (22).removeFromRight (110));
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

    // sourceBPM = 0.0 → auto-detect via BeatAnalyzer in SpliceThread
    const double targetBPM = processorRef.getGlobalBPM();
    const bool   skip      = skipWarpToggle.getToggleState();

    processorRef.requestSplice (stemsDir, 0.0, targetBPM, skip,
                                (float) processorRef.getSpliceDensity());
}

void PluginEditor::loadSpliceOutputWaveform()
{
    auto stemsDir   = processorRef.getLastStemOutputDir();
    auto mixedFile  = processorRef.spliceThread.getMixedOutputFile();

    if (mixedFile.existsAsFile())
        spliceOutputWaveform.loadFile (mixedFile);

    processorRef.loadSpliceOutput (stemsDir);
    splicePlayBtn.setToggleState (false, juce::dontSendNotification);
    spliceLoaded = true;
}
