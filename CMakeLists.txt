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

set(PROJECT_NAME "DemucsGUI")
set(PRODUCT_NAME "Demucs Source Separator")
set(COMPANY_NAME "cmucs")
set(BUNDLE_ID "com.cmucs.demucsgui")

# Only Standalone — this is an offline processor, not a real-time plugin
set(FORMATS Standalone)

# Force JUCE to work with sccache
set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# ============================================================
# External dependencies: cmucs core, ONNX Runtime, Eigen
# ============================================================

# Path to the cmucs project (source separation core)
set(CMUCS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../MyCppProjects/cmucs" CACHE PATH
    "Path to the cmucs project root")

# Path to ONNX Runtime GPU package
set(ONNXRUNTIME_ROOT "C:/Users/ryanb/onnxruntime-win-x64-gpu-1.24.1" CACHE PATH
    "Path to extracted ONNX Runtime GPU package")

# Build cmucs core as a static library (dsp + demucs, no CLI audio I/O)
add_library(cmucs_core STATIC
    "${CMUCS_ROOT}/src/dsp.cpp"
    "${CMUCS_ROOT}/src/demucs.cpp"
    "${CMUCS_ROOT}/src/audio_io.cpp"
)
target_include_directories(cmucs_core PUBLIC
    "${CMUCS_ROOT}/src"
    "${CMUCS_ROOT}/external/eigen-3.4.0"
    "${CMUCS_ROOT}/external"
    "${ONNXRUNTIME_ROOT}/include"
)
target_link_directories(cmucs_core PUBLIC "${ONNXRUNTIME_ROOT}/lib")
target_link_libraries(cmucs_core PUBLIC onnxruntime)
target_compile_features(cmucs_core PUBLIC cxx_std_17)

if (MSVC)
    target_compile_definitions(cmucs_core PUBLIC NOMINMAX _USE_MATH_DEFINES)
    # Suppress Eigen warnings in MSVC
    target_compile_options(cmucs_core PRIVATE /W3 /wd4554)
endif()

# ============================================================
# JUCE setup
# ============================================================

add_subdirectory(JUCE)

# Add CLAP format
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

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

clap_juce_extensions_plugin(TARGET "${PROJECT_NAME}"
    CLAP_ID "${BUNDLE_ID}"
    CLAP_FEATURES audio-effect)

# Enable fast math, C++20 and a few other target defaults
include(SharedCodeDefaults)

# Source files
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})

# Adds a BinaryData target for embedding assets into the binary
include(Assets)

# MacOS only: Cleans up folder and target organization on Xcode.
include(XcodePrettify)

# Preprocessor definitions
target_compile_definitions(SharedCode
    INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="DemucsGUI"
)

# Link JUCE modules + cmucs_core to SharedCode
target_link_libraries(SharedCode
    INTERFACE
    Assets
    cmucs_core
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
# Post-build: copy ONNX Runtime + cuDNN DLLs next to the executable
# ============================================================

# cuDNN path (auto-detect common NVIDIA install locations)
set(CUDNN_BIN_DIR "" CACHE PATH "Path to cuDNN bin directory containing DLLs")

if (WIN32 AND CUDNN_BIN_DIR STREQUAL "")
    # Auto-detect cuDNN from common NVIDIA install paths
    file(GLOB CUDNN_CANDIDATES "C:/Program Files/NVIDIA/CUDNN/v*/bin/12*/x64")
    if (CUDNN_CANDIDATES)
        list(GET CUDNN_CANDIDATES -1 CUDNN_BIN_DIR)
        message(STATUS "Auto-detected cuDNN: ${CUDNN_BIN_DIR}")
    endif()
endif()

if (WIN32)
    # Copy ONNX Runtime DLLs
    file(GLOB ORT_DLLS "${ONNXRUNTIME_ROOT}/lib/*.dll")
    foreach(DLL ${ORT_DLLS})
        add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Standalone")
    endforeach()

    # Copy cuDNN DLLs (required for CUDA execution provider)
    if (NOT CUDNN_BIN_DIR STREQUAL "")
        file(GLOB CUDNN_DLLS "${CUDNN_BIN_DIR}/*.dll")
        foreach(DLL ${CUDNN_DLLS})
            add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Standalone")
        endforeach()
        message(STATUS "cuDNN DLLs will be copied from: ${CUDNN_BIN_DIR}")
    else()
        message(WARNING "cuDNN not found. GPU acceleration will not work. Set CUDNN_BIN_DIR to enable.")
    endif()
endif()
