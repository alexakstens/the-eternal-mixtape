#pragma once
// Legacy shim — BeatAnalyzer now delegates to SongAnalyzer.
// New code should include SongAnalyzer.h directly.
#include "SongAnalyzer.h"

namespace remixing {
    using BeatAnalyzer = SongAnalyzer;
} // namespace remixing
