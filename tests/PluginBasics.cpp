#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <SeparationThread.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <fstream>

TEST_CASE ("Plugin instance", "[instance]")
{
    PluginProcessor testPlugin;

    SECTION ("name")
    {
        CHECK_THAT (testPlugin.getName().toStdString(),
            Catch::Matchers::Equals ("The Eternal Mixtape"));
    }
}

TEST_CASE ("GPU backend flag matches build type", "[gpu]")
{
    // gpuEnabled is set at construction time from compile-time ORT_USE_GPU.
    // This catches mismatches between the CMake option and the runtime flag.
    SeparationThread thread;

   #if defined(ORT_USE_GPU)
    CHECK (thread.isGpuEnabled() == true);
   #else
    CHECK (thread.isGpuEnabled() == false);
   #endif
}

#if defined(DEMUCS_ONNX_MODELS_DIR)
TEST_CASE ("ONNX model — source-tree path", "[model]")
{
    juce::File modelFile = juce::File (DEMUCS_ONNX_MODELS_DIR).getChildFile ("htdemucs.onnx");

    if (! modelFile.existsAsFile())
    {
        WARN ("Model not found at " + modelFile.getFullPathName().toStdString()
              + " — run 'git lfs pull' in the demucs.onnx submodule or download manually");
        return;
    }

    SECTION ("file is plausibly sized")
    {
        // htdemucs.onnx is ~167 MB; any real model should be >> 10 MB
        CHECK (modelFile.getSize() > 10LL * 1024 * 1024);
    }

    SECTION ("ONNX session creation — CPU provider")
    {
        // Loads the model and creates an ONNX session using the CPU provider only.
        // Catches model format regressions without requiring a GPU.
        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

        std::ifstream f (modelFile.getFullPathName().toStdString(), std::ios::binary);
        REQUIRE (f.is_open());

        std::vector<char> modelData ((std::istreambuf_iterator<char> (f)),
                                      std::istreambuf_iterator<char>());

        demucsonnx::demucs_model model;
        bool loaded = demucsonnx::load_model (modelData, model, opts);
        CHECK (loaded == true);
        model.sess.reset(); // explicit cleanup before test teardown
    }
}
#endif

#ifdef PAMPLEJUCE_IPP
 #include <ipp.h>

TEST_CASE ("IPP version", "[ipp]")
{
    auto* v = ippsGetLibVersion();
    REQUIRE (v != nullptr);
    INFO ("IPP linked: " << v->Version);
    CHECK (v->major > 0);
}
#endif
