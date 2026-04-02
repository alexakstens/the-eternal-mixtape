#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include <vector>
#include "SeparationThread.h"
#include "SpliceThread.h"

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // UX contract: Config (paths only; no hardcoded paths)
    //==============================================================================
    juce::File getConfigPath (const juce::String& key) const;
    void setConfigPath (const juce::String& key, const juce::File& path);

    //==============================================================================
    // UX contract: Transport
    //==============================================================================
    void play();
    void stop();
    void setTransportPosition (double ratio);
    double getTransportPositionSeconds() const;
    double getTransportTotalLengthSeconds() const;
    void setLoopEnabled (bool enabled);
    void setLoopRegion (double startSec, double endSec);

    //==============================================================================
    // UX contract: Meters / display
    //==============================================================================
    float getMasterLevels() const;

    //==============================================================================
    // UX contract: Tracks (0..3 = A..D)
    //==============================================================================
    juce::File getTrackSourceFile (int trackIndex) const;
    std::vector<juce::File> getTrackStemFiles (int trackIndex) const;
    juce::String getTrackName (int trackIndex) const;
    void setTrackName (int trackIndex, const juce::String& name);
    void setTrackSource (int trackIndex, const juce::File& file);
    void setTrackStemFile (int trackIndex, int stemSlot, const juce::File& file);
    void setTrackStemIndices (int trackIndex, int stem1Index, int stem2Index);

    //==============================================================================
    // UX contract: Mix
    //==============================================================================
    void setTrackGain (int trackIndex, float gain);
    float getTrackGain (int trackIndex) const;
    void setTrackStemGain (int trackIndex, int stemIndex, float gain);
    void setTrackPan (int trackIndex, float pan);
    void setTrackStemMute (int trackIndex, int stemIndex, bool muted);

    //==============================================================================
    // UX contract: Splice / BPM / Density
    //==============================================================================
    void applySplice (int trackIndex);
    void setSpliceDensity (float density);
    float getSpliceDensity() const;
    void setGlobalBPM (double bpm);
    double getGlobalBPM() const;

    //==============================================================================
    // UX contract: Actions
    //==============================================================================
    void applyAutoSplice();
    void regenerateMix();
    void randomizeMix();
    void startRecording();
    void startRecording (const juce::File& outputFile);

    //==============================================================================
    // UX contract: Stem separation (progress/error for UI)
    //==============================================================================
    std::vector<juce::File> getLastStemFiles() const;
    juce::File getLastStemOutputDir() const;
    float getStemProgress() const;
    juce::String getStemStatusMessage() const;
    juce::String getStemErrorMessage() const;
    void requestStemSeparation (const juce::File& inputFile,
                                const juce::File& modelFile,
                                const juce::File& outputDir,
                                bool useCuda = false);

    //==============================================================================
    // UX contract: Analysis (stub: no result yet)
    //==============================================================================
    void runAnalysisAsync (const juce::File& file);
    float getAnalysisProgress() const;
    juce::String getLastAnalysisErrorMessage() const;

    // Accessible from the editor for progress polling
    SeparationThread separationThread;
    SpliceThread     spliceThread;

    void requestSplice (const juce::File& stemsDir, double sourceBPM, double targetBPM,
                        bool skipWarp = false, float density = 0.5f);

    //==============================================================================
    // UX contract: Splice output playback
    //==============================================================================
    void loadSpliceOutput (const juce::File& file);
    void playSpliceOutput();
    void stopSpliceOutput();
    void rewindSpliceOutput();
    void seekSpliceOutput (double positionSeconds);
    void setSpliceOutputLoop (bool loop);
    bool isSpliceOutputPlaying() const;
    double getSpliceOutputPositionRatio() const;
    double getSpliceOutputLengthSeconds() const;

private:
    void initDefaultConfigPaths();

    std::map<juce::String, juce::File> configPaths_;
    double transportPosition_ = 0.0;
    double transportLengthSeconds_ = 0.0;
    bool isPlaying_ = false;
    bool loopEnabled_ = false;
    double loopStartSec_ = 0.0, loopEndSec_ = 0.0;
    float masterLevel_ = 0.0f;
    static constexpr int kNumTracks = 4;
    struct TrackState
    {
        juce::String name;
        juce::File sourceFile;
        std::vector<juce::File> stemFiles;
        float gain = 1.0f;
        float stemGain[2] = { 1.0f, 1.0f };
        float pan = 0.0f;
        bool stemMute[2] = { false, false };
    };
    TrackState trackState_[kNumTracks];
    double globalBPM_ = 120.0;
    float spliceDensity_ = 0.5f;
    juce::File lastStemOutputDir_;
    float analysisProgress_ = 0.0f;
    juce::String lastAnalysisErrorMessage_;

    // Splice output playback
    juce::SpinLock                                    spliceLock_;
    juce::AudioFormatManager                          spliceFormatManager_;
    std::unique_ptr<juce::AudioFormatReaderSource>    spliceReaderSource_;
    juce::AudioTransportSource                        spliceTransport_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
