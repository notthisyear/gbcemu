#include "windows/WindowsWindow.h"
#include "event/ApplicationEvent.h"
#include "event/KeyboardEvent.h"
#include "event/MouseEvent.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>

namespace gbcemu {

static void glfw_error_callback(int error, const char *description) {
    LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("GLFW error (%d) - %s", error, description));
}

WindowsWindow::WindowsWindow(const WindowProperties &properties) : m_properties(properties) {
    if (is_initialized) {
        LogUtilities::log_error(std::cout, "Window is already initialized");
        return;
    }

    if (!glfwInit()) {
        LogUtilities::log_error(std::cout, "GLFW initialization failed");
        return;
    }

    set_window_hint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    set_window_hint(GLFW_CONTEXT_VERSION_MINOR, 5);
    set_window_hint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwSetErrorCallback(glfw_error_callback);

    create_and_set_glfw_window();
    if (m_window == nullptr) {
        LogUtilities::log_error(std::cout, "Could not initialize window, are properties unsupported?");
        return;
    }

    m_context = std::make_unique<GraphicsContext>(m_window);
    if (!m_context->init()) {
        LogUtilities::log_error(std::cout, "Could not initialize window context");
        return;
    }

    glfwSetWindowUserPointer(m_window, &m_window_data);

    set_properties_as_requested();
    set_glfw_callbacks();

    is_initialized = true;
}

void WindowsWindow::update() {
    glfwPollEvents();
    m_context->swap_buffers();
}

void WindowsWindow::set_event_callback(const EventCallbackHandler &callback) { m_window_data.callback = callback; }

void WindowsWindow::set_window_hint(int hint_to_set, int value) { glfwWindowHint(hint_to_set, value); }

void WindowsWindow::create_and_set_glfw_window() {
    // Maybe the width and height must be changed to match the screen manually if full size?
    if (m_properties.mode == WindowProperties::WindowMode::Windowed)
        m_window = glfwCreateWindow(m_properties.width, m_properties.height, m_properties.title.c_str(), NULL, NULL);
    else if (m_properties.mode == WindowProperties::WindowMode::FullScreen)
        m_window = glfwCreateWindow(m_properties.width, m_properties.height, m_properties.title.c_str(), glfwGetPrimaryMonitor(), NULL);
    else
        m_window = nullptr;
}

void WindowsWindow::set_properties_as_requested() {

    glfwSwapInterval(m_properties.enable_vsync ? 1 : 0);
    glfwSetInputMode(m_window, GLFW_CURSOR, m_properties.capture_mouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    if (m_properties.use_raw_mouse_motion_if_possible && glfwRawMouseMotionSupported())
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void WindowsWindow::set_glfw_callbacks() {

    glfwSetWindowSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);
        WindowResizeEvent event(width, height);
        data.callback(event);
    });

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow *window) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);
        WindowCloseEvent event;
        data.callback(event);
    });

    glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        switch (action) {
        case GLFW_PRESS: {
            KeyPressedEvent event(key, false);
            data.callback(event);
            break;
        }
        case GLFW_RELEASE: {
            KeyReleasedEvent event(key);
            data.callback(event);
            break;
        }
        case GLFW_REPEAT: {
            KeyPressedEvent event(key, true);
            data.callback(event);
            break;
        }
        }
    });

    glfwSetCharCallback(m_window, [](GLFWwindow *window, unsigned int keycode) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        CharacterTypedEvent event(keycode);
        data.callback(event);
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow *window, int button, int action, int mods) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        switch (action) {
        case GLFW_PRESS: {
            MouseButtonPressedEvent event(button);
            data.callback(event);
            break;
        }
        case GLFW_RELEASE: {
            MouseButtonReleasedEvent event(button);
            data.callback(event);
            break;
        }
        }
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow *window, double x_offset, double y_offset) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);

        MouseScrolledEvent event((float)x_offset, (float)y_offset);
        data.callback(event);
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, double x_position, double y_position) {
        WindowData &data = *(WindowData *)glfwGetWindowUserPointer(window);
        MouseMovedEvent event((float)x_position, (float)y_position);
        data.callback(event);
    });
}

WindowsWindow::~WindowsWindow() {
    if (is_initialized) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

}