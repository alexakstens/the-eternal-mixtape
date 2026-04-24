cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)

# Modern concise way to add dependencies to your project
include (CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

set(PROJECT_NAME "TheEternalMixtape")
set(PRODUCT_NAME "The Eternal Mixtape")
set(COMPANY_NAME "GT MUSI6106")
set(BUNDLE_ID "com.gtmusi6106.theeternalmixtape")

# Only Standalone — this is an offline processor, not a real-time plugin
set(FORMATS Standalone)

# Force JUCE to work with sccache
set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# ============================================================
# External dependencies: demucs.onnx, ONNX Runtime, Eigen
# ============================================================

# Path to the demucs.onnx submodule
set(DEMUCS_ONNX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/demucs.onnx" CACHE PATH
    "Path to the demucs.onnx submodule root")

# Path to ONNX Runtime package (CPU or DirectML NuGet depending on ORT_USE_GPU).
# Set in CMakeUserPresets.json for local dev, or via -DONNXRUNTIME_ROOT=... in CI.
set(ONNXRUNTIME_ROOT "" CACHE PATH "Path to extracted ONNX Runtime package root")
if (NOT ONNXRUNTIME_ROOT)
    message(FATAL_ERROR
        "ONNXRUNTIME_ROOT is not set.\n"
        "  Local dev: copy CMakeUserPresets.json.example -> CMakeUserPresets.json and set ONNXRUNTIME_ROOT.\n"
        "  CI / command line: pass -DONNXRUNTIME_ROOT=/path/to/ort at configure time.")
endif()

# Enable GPU inference: DirectML on Windows, CoreML on macOS.
# CPU build: OFF (no GPU provider loaded, ships lightweight model).
# GPU build: ON  (unconditional GPU provider, ships full model).
option(ORT_USE_GPU "Enable GPU inference (DirectML/Windows, CoreML/macOS)" OFF)

# ORT package layouts differ:
#   Standard CPU / macOS:  include/  and  lib/
#   DirectML NuGet (Win):  build/native/include/  and  runtimes/win-x64/native/
if (WIN32 AND ORT_USE_GPU)
    set(ORT_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/build/native/include")
    set(ORT_LIB_DIR     "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native")
else()
    set(ORT_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
    set(ORT_LIB_DIR     "${ONNXRUNTIME_ROOT}/lib")
endif()

# On macOS, bake the ORT lib path into RPATH so all built executables (including
# Tests and Benchmarks) can find libonnxruntime at run time without DYLD_LIBRARY_PATH.
if (APPLE)
    set(CMAKE_BUILD_RPATH "${ORT_LIB_DIR}")
endif()

# ORT include shim: demucs.onnx uses <onnxruntime/core/session/...> paths but
# pre-built packages place headers at a flat level.
set(ORT_COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source/ort-compat")

# Build demucs.onnx inference core as a static library (no CLI, no embedded models)
file(GLOB DEMUCS_ONNX_SOURCES "${DEMUCS_ONNX_ROOT}/src/*.cpp")
add_library(demucs_onnx_core STATIC ${DEMUCS_ONNX_SOURCES})
target_include_directories(demucs_onnx_core PUBLIC
    "${DEMUCS_ONNX_ROOT}/src"
    "${DEMUCS_ONNX_ROOT}/vendor/eigen"
    "${ORT_INCLUDE_DIR}"
    "${ORT_COMPAT_DIR}"
)
target_link_directories(demucs_onnx_core PUBLIC "${ORT_LIB_DIR}")
target_link_libraries(demucs_onnx_core PUBLIC onnxruntime)
target_compile_features(demucs_onnx_core PUBLIC cxx_std_20)

if (MSVC)
    target_compile_definitions(demucs_onnx_core PUBLIC NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(demucs_onnx_core PRIVATE /W3 /wd4554 /wd4267 /wd4244)
endif()

# ============================================================
# Marcus's C remixing engines (warp.c / beat_tracker.c)
# These are compiled as separate C static libs due to C99 complex + external deps.
# ============================================================

# Phase-vocoder pitch+tempo warp engine — requires FFTW3
option(ENABLE_FFTW3_WARP "Build Marcus's warp engine (requires FFTW3)" OFF)
if (ENABLE_FFTW3_WARP)
    add_library(warp_engine STATIC source/remixing/warp.c)
    target_include_directories(warp_engine PUBLIC source/remixing)
    target_compile_features(warp_engine PUBLIC c_std_99)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3 REQUIRED fftw3f)
    target_include_directories(warp_engine PUBLIC ${FFTW3_INCLUDE_DIRS})
    target_link_directories(warp_engine PUBLIC ${FFTW3_LIBRARY_DIRS})
    target_link_libraries(warp_engine PUBLIC ${FFTW3_LIBRARIES})
    target_compile_definitions(warp_engine PUBLIC ENABLE_FFTW3_WARP=1)
    message(STATUS "Warp engine: ENABLED (FFTW3 found)")
else()
    message(STATUS "Warp engine: DISABLED (set ENABLE_FFTW3_WARP=ON to enable)")
endif()

# Beat tracker — requires aubio
option(ENABLE_AUBIO_BEAT "Build Marcus's beat tracker (requires aubio)" OFF)
if (ENABLE_AUBIO_BEAT)
    add_library(beat_engine STATIC source/remixing/beat_tracker.c)
    target_include_directories(beat_engine PUBLIC source/remixing)
    target_compile_features(beat_engine PUBLIC c_std_99)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AUBIO REQUIRED aubio)
    target_include_directories(beat_engine PUBLIC ${AUBIO_INCLUDE_DIRS})
    target_link_directories(beat_engine PUBLIC ${AUBIO_LIBRARY_DIRS})
    target_link_libraries(beat_engine PUBLIC ${AUBIO_LIBRARIES})
    target_compile_definitions(beat_engine PUBLIC ENABLE_AUBIO_BEAT=1)
    message(STATUS "Beat tracker: ENABLED (aubio found)")
else()
    message(STATUS "Beat tracker: DISABLED (set ENABLE_AUBIO_BEAT=ON to enable)")
endif()

# ============================================================
# JUCE setup
# ============================================================

add_subdirectory(JUCE)

# Each developer can override the VST3 copy directory to avoid needing admin rights.
# Copy CMakeUserPresets.json.example -> CMakeUserPresets.json and fill in your username.
# Or pass -DVST3_COPY_DIR="/your/path" at configure time.
set(VST3_COPY_DIR "" CACHE PATH "Override the VST3 plugin copy directory")
if(VST3_COPY_DIR)
    set_property(GLOBAL PROPERTY JUCE_VST3_COPY_DIR "${VST3_COPY_DIR}")
endif()

# Melatonin inspector (useful for UI development)
add_subdirectory(modules/melatonin_inspector)

juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Cmus
    PLUGIN_CODE Dmcs
    FORMATS "${FORMATS}"
    PRODUCT_NAME "${PRODUCT_NAME}"
    # Need file dialog access
    NEEDS_STORE_KIT FALSE)

# SharedCode interface library (pamplejuce pattern)
add_library(SharedCode INTERFACE)

# Enable fast math, C++20 and a few other target defaults
include(SharedCodeDefaults)

# Source files
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})

# time-stretcher (jatinchowdhury18) — HPSS phase-vocoder, uses juce::dsp::FFT.
# Sources compiled directly into SharedCode so they inherit JUCE's compilation context.
target_sources(SharedCode INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/time-stretcher/src/hpss.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/time-stretcher/src/stretch.cpp"
)

# Adds a BinaryData target for embedding assets into the binary
include(Assets)

# MacOS only: Cleans up folder and target organization on Xcode.
include(XcodePrettify)

# Expose remixing headers to all SharedCode consumers
target_include_directories(SharedCode INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/source/remixing"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/time-stretcher/src"
)

# Preprocessor definitions
target_compile_definitions(SharedCode
    INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="TheEternalMixtape"
    TIMESTRETCH_USING_JUCE=1
    DEMUCS_ONNX_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models"
    $<$<BOOL:${ORT_USE_GPU}>:ORT_USE_GPU=1>
)

# Link JUCE modules + inference/remixing engines to SharedCode
target_link_libraries(SharedCode
    INTERFACE
    Assets
    demucs_onnx_core
    $<$<BOOL:${ENABLE_FFTW3_WARP}>:warp_engine>
    $<$<BOOL:${ENABLE_AUBIO_BEAT}>:beat_engine>
    melatonin_inspector
    juce_audio_utils
    juce_audio_processors
    juce_audio_formats
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Link the JUCE plugin target to SharedCode
target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# IPP support, comment out to disable
include(PamplejuceIPP)

# Everything related to the tests target
include(Tests)

# A separate target for Benchmarks
include(Benchmarks)

# Output some config for CI
include(GitHubENV)

# ============================================================
# Post-build: copy ONNX Runtime DLLs + model into the output tree
# ============================================================

set(_STANDALONE_TARGET "${PROJECT_NAME}_Standalone")

# --- ORT DLLs (Windows only) ---
# DirectML ships with Windows 10 1903+ — no extra runtime DLLs required.
if (WIN32)
    file(GLOB ORT_DLLS "${ORT_LIB_DIR}/*.dll")
    foreach(DLL ${ORT_DLLS})
        add_custom_command(TARGET ${_STANDALONE_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" "$<TARGET_FILE_DIR:${_STANDALONE_TARGET}>")
    endforeach()

    if (ORT_USE_GPU)
        message(STATUS "GPU build: ORT DirectML DLLs will be copied from ${ORT_LIB_DIR}")
    else()
        message(STATUS "CPU build: ORT DLLs will be copied from ${ORT_LIB_DIR}")
    endif()
endif()

# --- Model file ---
# GPU builds ship htdemucs.onnx; CPU builds prefer htdemucs_cpu.onnx with fallback.
# Both files live in models/ at the root of the main repo (Git LFS tracked).
set(_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models")
if (ORT_USE_GPU)
    set(_MODEL_FILE "${_MODELS_DIR}/htdemucs.onnx")
else()
    if (EXISTS "${_MODELS_DIR}/htdemucs_cpu.onnx")
        set(_MODEL_FILE "${_MODELS_DIR}/htdemucs_cpu.onnx")
    else()
        set(_MODEL_FILE "${_MODELS_DIR}/htdemucs.onnx")
        message(STATUS "htdemucs_cpu.onnx not found — falling back to htdemucs.onnx for CPU build")
    endif()
endif()

# On macOS app bundles the executable lives in Contents/MacOS/; models go in Contents/Resources/models/.
# On Windows and Linux they live next to the binary, so models/ is a sibling of the exe.
if (APPLE)
    set(_MODEL_DEST "$<TARGET_FILE_DIR:${_STANDALONE_TARGET}>/../Resources/models")
else()
    set(_MODEL_DEST "$<TARGET_FILE_DIR:${_STANDALONE_TARGET}>/models")
endif()

if (EXISTS "${_MODEL_FILE}")
    get_filename_component(_MODEL_FILENAME "${_MODEL_FILE}" NAME)
    add_custom_command(TARGET ${_STANDALONE_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_MODEL_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_MODEL_FILE}" "${_MODEL_DEST}/${_MODEL_FILENAME}"
        COMMENT "Copying model to ${_MODEL_DEST}")
    message(STATUS "Model: ${_MODEL_FILE} → <exe>/models/")
else()
    message(WARNING "Model file not found at ${_MODEL_FILE} — ZIP distribution will be missing the model")
endif()
