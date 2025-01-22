#pragma once

#include "util/GeneralUtilities.h"
#include <cstdint>
#include <string>

namespace gbcemu {
struct WindowProperties final {

  public:
    enum class WindowMode { Windowed, FullScreen };

    std::string title;
    WindowMode mode;
    bool capture_mouse;
    bool enable_vsync;
    bool use_raw_mouse_motion_if_possible;
    uint32_t width;
    uint32_t height;

    WindowProperties(std::string const &title = "gbcemu v 0.1", WindowMode const mode = WindowMode::Windowed, bool const capture_mouse = false,
                     bool const enable_vsync = true, bool const use_raw_mouse_motion_if_possible = true, uint32_t const width = 160,
                     uint32_t const height = 144)
        : title(title), mode(mode), capture_mouse(capture_mouse), enable_vsync(enable_vsync),
          use_raw_mouse_motion_if_possible(use_raw_mouse_motion_if_possible), width(width), height(height) {}

    std::string to_string() const {

        std::string const options{ GeneralUtilities::formatted_string("mouse capturing %s, V-SYNC %s, raw mouse motion %s", on_or_off(capture_mouse),
                                                                      on_or_off(enable_vsync), on_or_off(use_raw_mouse_motion_if_possible)) };
        switch (mode) {
        case WindowMode::Windowed:
            return GeneralUtilities::formatted_string("Windowed mode (%i x %i), %s", width, height, options);
        case WindowMode::FullScreen:
            return GeneralUtilities::formatted_string("Full screen mode, %s", options);
        default:
            return "";
        }
    }

  private:
    std::string on_or_off(bool const b) const { return b ? "<ON>" : "<OFF>"; }
};
}