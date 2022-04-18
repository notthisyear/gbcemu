#pragma once

#include "components/CPU.h"
#include "event/ApplicationEvent.h"
#include "event/Event.h"
#include "windows/WindowsWindow.h"
#include <memory>

namespace gbcemu {

class Application {
  public:
    Application(std::shared_ptr<CPU>, const WindowProperties &);

    void init();
    void run();
    void set_cpu_debug_mode(const bool);
    void set_cpu_breakpoint(const uint16_t);

    uint32_t register_event_callback(EventType, EventCallbackHandler);
    void handle_event(Event &);
    bool try_remove_event_callback(EventType, uint32_t);

    ~Application();

  private:
    bool m_app_should_run = false;
    bool m_cpu_should_run = true;
    const float MaxTimePerFrameSec = 0.02;

    std::shared_ptr<CPU> m_cpu;
    uint32_t m_current_event_id = 0;
    std::unordered_map<EventType, std::vector<std::pair<uint32_t, EventCallbackHandler>>> m_event_callbacks;

    std::unique_ptr<WindowsWindow> m_window;
    WindowProperties m_window_properties;

    void window_resized_event(WindowResizeEvent &);
    void window_closed_event(WindowCloseEvent &);
};
}