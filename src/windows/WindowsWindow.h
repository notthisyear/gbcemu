#pragma once

#define GLFW_INCLUDE_NONE

#include "GLFW/glfw3.h"
#include "common/WindowProperties.h"
#include "event/Event.h"
#include "opengl/GraphicsContext.h"
#include <cstdint>
#include <string>

namespace gbcemu {

class WindowsWindow {

  public:
    bool is_initialized = false;

    WindowsWindow(const WindowProperties &);

    float calculate_time_delta_since_last_frame();
    void update() const;
    void set_event_callback(const EventCallbackHandler &);

    ~WindowsWindow();

  private:
    struct WindowData {
      public:
        EventCallbackHandler callback;
    };

    WindowProperties m_properties = {};
    GLFWwindow *m_window = {};
    WindowData m_window_data;
    std::unique_ptr<GraphicsContext> m_context;
    float m_last_frame_time = 0.0f;

    void create_and_set_glfw_window();
    void set_window_hint(int, int);
    void set_properties_as_requested();
    void set_glfw_callbacks();
};
}