#include <fstream>

TEST_CASE ("Boot performance")
{
    BENCHMARK_ADVANCED ("Processor constructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::storage_for<PluginProcessor>> storage (size_t (meter.runs()));
        meter.measure ([&] (int i) { storage[(size_t) i].construct(); });
    };

    BENCHMARK_ADVANCED ("Processor destructor")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<Catch::Benchmark::destructable_object<PluginProcessor>> storage (size_t (meter.runs()));
        for (auto& s : storage)
            s.construct();
        meter.measure ([&] (int i) { storage[(size_t) i].destruct(); });
    };

    BENCHMARK_ADVANCED ("Editor open and close")
    (Catch::Benchmark::Chronometer meter)
    {
        PluginProcessor plugin;

        // due to complex construction logic of the editor, let's measure open/close together
        meter.measure ([&] (int /* i */) {
            auto editor = plugin.createEditorIfNeeded();
            plugin.editorBeingDeleted (editor);
            delete editor;
            return plugin.getActiveEditor();
        });
    };
}

// ============================================================
// Inference timing benchmarks
//
// Tagged [.][bench] — skipped by default in ctest.
// To run manually:
//   Windows/Linux:  ./Benchmarks [bench]
//   macOS:          ./Benchmarks [bench]
// Or via ctest:    ctest -R Benchmarks -C Release --test-dir Builds -- [bench]
//
// Requires the model file at DEMUCS_ONNX_MODELS_DIR/htdemucs.onnx.
// Results feed into docs/PERFORMANCE.md — record: platform, backend,
// seconds per minute of audio.
// ============================================================

#if defined(DEMUCS_ONNX_MODELS_DIR)

static std::vector<char> loadModelDataOrSkip()
{
    juce::File modelFile = juce::File (DEMUCS_ONNX_MODELS_DIR).getChildFile ("htdemucs.onnx");
    if (! modelFile.existsAsFile())
    {
        WARN ("Model not found — skipping inference benchmark. "
              "Run 'git lfs pull' in demucs.onnx submodule.");
        return {};
    }
    std::ifstream f (modelFile.getFullPathName().toStdString(), std::ios::binary);
    return { std::istreambuf_iterator<char> (f), std::istreambuf_iterator<char>() };
}

// Synthetic stereo silence at 44100 Hz — used for timing without needing a real audio file.
// Demucs processes audio in 7.8-second segments so make this a round number of segments.
static Eigen::MatrixXf makeSyntheticAudio (double durationSeconds)
{
    int numSamples = static_cast<int> (durationSeconds * demucsonnx::SUPPORTED_SAMPLE_RATE);
    Eigen::MatrixXf audio (2, numSamples);
    audio.setZero();
    // Add a tiny DC offset so the model doesn't trivially skip processing
    audio.array() += 1e-6f;
    return audio;
}

TEST_CASE ("Model load time", "[.][bench]")
{
    // Measures wall-clock time to read + deserialise the model file (CPU only).
    // This runs once per invocation (not a Catch2 BENCHMARK loop) to give a simple timing.
    auto modelData = loadModelDataOrSkip();
    if (modelData.empty()) return;

    auto t0 = juce::Time::getMillisecondCounterHiRes();

    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
    demucsonnx::demucs_model model;
    bool ok = demucsonnx::load_model (modelData, model, opts);

    double elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    REQUIRE (ok);
    INFO ("Model load time (CPU session): " << elapsed << "s");
    model.sess.reset();
}

TEST_CASE ("Inference timing — 15s synthetic clip (CPU)", "[.][bench]")
{
    // Times a 15-second synthetic clip through the full demucs_inference pipeline.
    // 15s = ~2 segments at 7.8s each — enough to see per-segment time without waiting too long.
    // Run on all target platforms and record results for docs/PERFORMANCE.md.
    auto modelData = loadModelDataOrSkip();
    if (modelData.empty()) return;

    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
    opts.SetExecutionMode (ExecutionMode::ORT_PARALLEL);
    opts.SetIntraOpNumThreads (0);
    opts.SetInterOpNumThreads (0);

    demucsonnx::demucs_model model;
    bool ok = demucsonnx::load_model (modelData, model, opts);
    REQUIRE (ok);

    Eigen::MatrixXf audio = makeSyntheticAudio (15.0);

    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto result = demucsonnx::demucs_inference (model, audio, [] (float, const std::string&) {});
    double elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;

    CHECK (result.size() > 0);
    double secsPerMin = elapsed / (15.0 / 60.0);
    INFO ("Inference time for 15s clip: " << elapsed << "s  (" << secsPerMin << "s per minute of audio)");
    model.sess.reset();
}

#endif // DEMUCS_ONNX_MODELS_DIR
