#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
    initDefaultConfigPaths();
    spliceFormatManager_.registerBasicFormats(); // used in loadSpliceOutput
    for (int i = 0; i < kNumTracks; ++i)
        trackState_[i].name = "Track " + juce::String (char ('A' + i));
}

PluginProcessor::~PluginProcessor()
{
    spliceIsPlaying_.store (false);
    separationThread.stopThread (5000);
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        juce::ignoreUnused (channelData);
        // ..do something to the data...
    }

    // Mix in splice output playback (per-stem with track gains)
    if (spliceIsPlaying_.load())
    {
        const juce::SpinLock::ScopedTryLockType tryLock (spliceLock_);
        if (tryLock.isLocked() && spliceTotalSamples_ > 0)
        {
            const int numOut = buffer.getNumSamples();
            int64_t pos      = splicePlayPos_.load();

            for (int si = 0; si < kNumTracks; ++si)
            {
                if (! spliceStems_[si].valid) continue;
                const float gain = getTrackGain (si);
                const auto& stemBuf = spliceStems_[si].audio;

                for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
                {
                    auto* out = buffer.getWritePointer (c);
                    for (int n = 0; n < numOut; ++n)
                    {
                        int64_t spos = pos + n;
                        if (spos < stemBuf.getNumSamples())
                            out[n] += gain * stemBuf.getSample (c, (int) spos);
                    }
                }
            }

            int64_t newPos = pos + numOut;
            if (newPos >= spliceTotalSamples_)
            {
                if (spliceIsLooping_.load())
                    newPos = newPos % spliceTotalSamples_;
                else
                {
                    newPos = spliceTotalSamples_;
                    spliceIsPlaying_.store (false);
                }
            }
            splicePlayPos_.store (newPos);
        }
    }

    // Update master level for UI meter (peak of output)
    if (totalNumOutputChannels > 0 && buffer.getNumSamples() > 0)
    {
        float peak = buffer.getMagnitude (0, buffer.getNumSamples());
        masterLevel_ = juce::jmin (1.0f, peak);
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// UX contract: Config
//==============================================================================
void PluginProcessor::initDefaultConfigPaths()
{
    juce::File def = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    if (! configPaths_.count ("imported_audio_dir"))
        configPaths_["imported_audio_dir"] = def.getChildFile ("EternalMixtape").getChildFile ("Imported");
    if (! configPaths_.count ("stem_model_file"))
        configPaths_["stem_model_file"] = def.getChildFile ("EternalMixtape").getChildFile ("Models").getChildFile ("stem.model");
    if (! configPaths_.count ("stem_output_dir"))
        configPaths_["stem_output_dir"] = def.getChildFile ("EternalMixtape").getChildFile ("Stems");
    if (! configPaths_.count ("analysis_cache_dir"))
        configPaths_["analysis_cache_dir"] = def.getChildFile ("EternalMixtape").getChildFile ("Analysis");
    if (! configPaths_.count ("project_save_dir"))
        configPaths_["project_save_dir"] = def.getChildFile ("EternalMixtape").getChildFile ("Projects");
    if (! configPaths_.count ("export_output_dir"))
        configPaths_["export_output_dir"] = def.getChildFile ("EternalMixtape").getChildFile ("Exports");
    if (! configPaths_.count ("user_config_file"))
        configPaths_["user_config_file"] = def.getChildFile ("EternalMixtape").getChildFile ("config.json");
}

juce::File PluginProcessor::getConfigPath (const juce::String& key) const
{
    auto it = configPaths_.find (key);
    return it != configPaths_.end() ? it->second : juce::File();
}

void PluginProcessor::setConfigPath (const juce::String& key, const juce::File& path)
{
    configPaths_[key] = path;
}

//==============================================================================
// UX contract: Transport
//==============================================================================
void PluginProcessor::play()
{
    isPlaying_ = true;
}

void PluginProcessor::stop()
{
    isPlaying_ = false;
}

void PluginProcessor::setTransportPosition (double ratio)
{
    transportPosition_ = juce::jlimit (0.0, 1.0, ratio);
}

double PluginProcessor::getTransportPositionSeconds() const
{
    return transportLengthSeconds_ * transportPosition_;
}

double PluginProcessor::getTransportTotalLengthSeconds() const
{
    return transportLengthSeconds_;
}

void PluginProcessor::setLoopEnabled (bool enabled)
{
    loopEnabled_ = enabled;
}

void PluginProcessor::setLoopRegion (double startSec, double endSec)
{
    loopStartSec_ = startSec;
    loopEndSec_ = endSec;
}

//==============================================================================
// UX contract: Meters
//==============================================================================
float PluginProcessor::getMasterLevels() const
{
    return masterLevel_;
}

//==============================================================================
// UX contract: Tracks
//==============================================================================
juce::File PluginProcessor::getTrackSourceFile (int trackIndex) const
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        return trackState_[trackIndex].sourceFile;
    return juce::File();
}

std::vector<juce::File> PluginProcessor::getTrackStemFiles (int trackIndex) const
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        return trackState_[trackIndex].stemFiles;
    return {};
}

juce::String PluginProcessor::getTrackName (int trackIndex) const
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        return trackState_[trackIndex].name;
    return {};
}

void PluginProcessor::setTrackName (int trackIndex, const juce::String& name)
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        trackState_[trackIndex].name = name;
}

void PluginProcessor::setTrackSource (int trackIndex, const juce::File& file)
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        trackState_[trackIndex].sourceFile = file;
}

void PluginProcessor::setTrackStemFile (int trackIndex, int stemSlot, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (trackIndex, kNumTracks))
        return;
    auto& stems = trackState_[trackIndex].stemFiles;
    if (stemSlot >= (int) stems.size())
        stems.resize (stemSlot + 1);
    stems[stemSlot] = file;
}

void PluginProcessor::setTrackStemIndices (int trackIndex, int stem1Index, int stem2Index)
{
    juce::ignoreUnused (trackIndex, stem1Index, stem2Index);
}

//==============================================================================
// UX contract: Mix
//==============================================================================
void PluginProcessor::setTrackGain (int trackIndex, float gain)
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        trackState_[trackIndex].gain = juce::jlimit (0.0f, 2.0f, gain);
}

float PluginProcessor::getTrackGain (int trackIndex) const
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        return trackState_[trackIndex].gain;
    return 1.0f;
}

void PluginProcessor::setTrackStemGain (int trackIndex, int stemIndex, float gain)
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks) && stemIndex >= 0 && stemIndex < 2)
        trackState_[trackIndex].stemGain[stemIndex] = juce::jlimit (0.0f, 2.0f, gain);
}

void PluginProcessor::setTrackPan (int trackIndex, float pan)
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks))
        trackState_[trackIndex].pan = juce::jlimit (-1.0f, 1.0f, pan);
}

void PluginProcessor::setTrackStemMute (int trackIndex, int stemIndex, bool muted)
{
    if (juce::isPositiveAndBelow (trackIndex, kNumTracks) && stemIndex >= 0 && stemIndex < 2)
        trackState_[trackIndex].stemMute[stemIndex] = muted;
}

//==============================================================================
// UX contract: Splice / BPM / Density
//==============================================================================
void PluginProcessor::applySplice (int trackIndex)
{
    juce::ignoreUnused (trackIndex);
}

void PluginProcessor::setSpliceDensity (float density)
{
    spliceDensity_ = juce::jlimit (0.0f, 1.0f, density);
}

float PluginProcessor::getSpliceDensity() const
{
    return spliceDensity_;
}

void PluginProcessor::setGlobalBPM (double bpm)
{
    globalBPM_ = juce::jmax (20.0, juce::jmin (300.0, bpm));
}

double PluginProcessor::getGlobalBPM() const
{
    return globalBPM_;
}

//==============================================================================
// UX contract: Actions
//==============================================================================
void PluginProcessor::requestSplice (const juce::File& stemsDir,
                                     double sourceBPM,
                                     double targetBPM,
                                     bool   skipWarp,
                                     float  density)
{
    if (spliceThread.isThreadRunning())
        return;
    spliceThread.configure (stemsDir, sourceBPM, targetBPM, skipWarp, density);
    spliceThread.startThread();
}

void PluginProcessor::applyAutoSplice()
{
}

void PluginProcessor::regenerateMix()
{
}

void PluginProcessor::randomizeMix()
{
}

void PluginProcessor::startRecording()
{
    startRecording (getConfigPath ("export_output_dir").getChildFile ("recording.wav"));
}

void PluginProcessor::startRecording (const juce::File& outputFile)
{
    juce::ignoreUnused (outputFile);
}

//==============================================================================
// UX contract: Stem
//==============================================================================
std::vector<juce::File> PluginProcessor::getLastStemFiles() const
{
    if (lastStemOutputDir_ == juce::File{})
        return {};
    auto stems = { "drums.wav", "bass.wav", "other.wav", "vocals.wav", "guitar.wav", "piano.wav" };
    std::vector<juce::File> result;
    for (auto& name : stems)
    {
        auto f = lastStemOutputDir_.getChildFile (name);
        if (f.existsAsFile())
            result.push_back (f);
    }
    return result;
}

juce::File PluginProcessor::getLastStemOutputDir() const
{
    return lastStemOutputDir_;
}

float PluginProcessor::getStemProgress() const
{
    return separationThread.getProgress();
}

juce::String PluginProcessor::getStemStatusMessage() const
{
    return separationThread.getStatusMessage();
}

juce::String PluginProcessor::getStemErrorMessage() const
{
    return separationThread.getErrorMessage();
}

void PluginProcessor::requestStemSeparation (const juce::File& inputFile,
                                             const juce::File& modelFile,
                                             const juce::File& outputDir,
                                             bool useCuda)
{
    lastStemOutputDir_ = outputDir;
    separationThread.configure (inputFile, modelFile, outputDir, useCuda);
    separationThread.startThread();
}

//==============================================================================
// UX contract: Analysis
//==============================================================================
void PluginProcessor::runAnalysisAsync (const juce::File& file)
{
    juce::ignoreUnused (file);
    analysisProgress_ = 0.0f;
}

float PluginProcessor::getAnalysisProgress() const
{
    return analysisProgress_;
}

juce::String PluginProcessor::getLastAnalysisErrorMessage() const
{
    return lastAnalysisErrorMessage_;
}

//==============================================================================
// UX contract: Splice output playback
//==============================================================================
void PluginProcessor::loadSpliceOutput (const juce::File& outputDir)
{
    spliceIsPlaying_.store (false);

    const juce::SpinLock::ScopedLockType sl (spliceLock_);

    for (int i = 0; i < kNumTracks; ++i)
        spliceStems_[i].valid = false;

    spliceTotalSamples_ = 0;
    splicePlayPos_.store (0);

    if (! outputDir.isDirectory())
        return;

    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats();

    int64_t maxLen = 0;
    for (int i = 0; i < kNumTracks; ++i)
    {
        auto f = outputDir.getChildFile ("splice_stem_" + juce::String (i) + ".wav");
        if (! f.existsAsFile()) continue;

        std::unique_ptr<juce::AudioFormatReader> reader (fmt.createReaderFor (f));
        if (! reader) continue;

        spliceSampleRate_ = (int) reader->sampleRate;
        const int numSmp  = (int) reader->lengthInSamples;
        const int numCh   = std::min (2, (int) reader->numChannels);

        spliceStems_[i].audio.setSize (2, numSmp, false, true, false);
        reader->read (&spliceStems_[i].audio, 0, numSmp, 0, true, numCh > 1);

        // If mono source, copy L→R
        if (numCh == 1)
            spliceStems_[i].audio.copyFrom (1, 0, spliceStems_[i].audio, 0, 0, numSmp);

        spliceStems_[i].valid = true;
        maxLen = std::max (maxLen, (int64_t) numSmp);
    }

    spliceTotalSamples_ = maxLen;
}

void PluginProcessor::playSpliceOutput()
{
    if (spliceTotalSamples_ > 0)
        spliceIsPlaying_.store (true);
}

void PluginProcessor::stopSpliceOutput()
{
    spliceIsPlaying_.store (false);
}

void PluginProcessor::rewindSpliceOutput()
{
    splicePlayPos_.store (0);
}

void PluginProcessor::seekSpliceOutput (double positionSeconds)
{
    int64_t pos = (int64_t) (positionSeconds * spliceSampleRate_);
    splicePlayPos_.store (juce::jlimit ((int64_t) 0, spliceTotalSamples_, pos));
}

void PluginProcessor::setSpliceOutputLoop (bool loop)
{
    spliceIsLooping_.store (loop);
}

bool PluginProcessor::isSpliceOutputPlaying() const
{
    return spliceIsPlaying_.load();
}

double PluginProcessor::getSpliceOutputPositionRatio() const
{
    if (spliceTotalSamples_ <= 0) return 0.0;
    return (double) splicePlayPos_.load() / (double) spliceTotalSamples_;
}

double PluginProcessor::getSpliceOutputLengthSeconds() const
{
    if (spliceSampleRate_ <= 0) return 0.0;
    return (double) spliceTotalSamples_ / (double) spliceSampleRate_;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
