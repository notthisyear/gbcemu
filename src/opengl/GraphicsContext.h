#pragma once

#define GLFW_INCLUDE_NONE

#include "GLFW/glfw3.h"

namespace gbcemu {

class GraphicsContext final {

  public:
    GraphicsContext(GLFWwindow *const);

    bool init();
    void swap_buffers() const;

  private:
    GLFWwindow *m_window;
};
}