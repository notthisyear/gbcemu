#include "opengl/GraphicsContext.h"
#include "glad/glad.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>

namespace gbcemu {

GraphicsContext::GraphicsContext(GLFWwindow *window) : m_window(window) {}

bool GraphicsContext::init() {
    glfwMakeContextCurrent(m_window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LogUtilities::log_error(std::cout, "Failed to initialize GLAD");
        return false;
    }

    LogUtilities::log_info(std::cout, GeneralUtilities::formatted_string("Open GL Info -- Vendor: %s, Renderer: %s, Version: %s", glGetString(GL_VENDOR),
                                                                         glGetString(GL_RENDERER), glGetString(GL_VERSION)));
    return true;
}

void GraphicsContext::swap_buffers() const { glfwSwapBuffers(m_window); }
}