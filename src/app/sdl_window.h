#pragma once

#include "common/error.h"
#include "common/types.h"

#include <optional>
#include <string>

namespace nc::app {

struct WindowOptions {
    std::string title;
    int width = 960;
    int height = 720;
    bool hidden = false;
    bool vsync = true;
    std::optional<u64> max_frames; // Close after this many frames.
};

struct WindowRunStats {
    u64 frames = 0;
    std::string renderer_name;
};

// Opens an SDL window, runs its event loop until the user closes it (or
// max_frames is reached), and shuts SDL down again. The window shows a plain
// background: there is no emulator output to display yet. Connected game
// controllers are detected and logged, but not yet mapped to anything.
Result<WindowRunStats> run_window(const WindowOptions& options);

} // namespace nc::app
