#pragma once

#define GLFW_INCLUDE_NONE

#include "GLFW/glfw3.h"
#include "common/WindowProperties.h"
#include "event/Event.h"
#include "opengl/GraphicsContext.h"

namespace gbcemu {

class WindowsWindow final {

  public:
    bool is_initialized{ false };

    WindowsWindow(WindowProperties const &);

    void update();
    void set_event_callback(EventCallbackHandler const &);

    ~WindowsWindow();

  private:
    struct WindowData {
      public:
        EventCallbackHandler callback;
    };

    WindowProperties m_properties{};
    GLFWwindow *m_window{};
    WindowData m_window_data;
    std::unique_ptr<GraphicsContext> m_context;

    void create_and_set_glfw_window();
    void set_window_hint(int const, int const);
    void set_properties_as_requested();
    void set_glfw_callbacks();
};
}