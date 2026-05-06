// Compatibility shim: demucs.onnx uses the full ORT include path
// but the pre-built Windows package places headers at a flat level.
#include <onnxruntime_cxx_api.h>
