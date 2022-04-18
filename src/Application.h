#pragma once

#include "event/ApplicationEvent.h"
#include "event/Event.h"
#include "windows/WindowsWindow.h"

namespace gbcemu {

class Application {
  public:
    Application(const WindowProperties &);

    void init();
    void run();
    uint32_t register_event_callback(EventType, EventCallbackHandler);
    void handle_event(Event &);
    bool try_remove_event_callback(EventType, uint32_t);

    ~Application();

  private:
    bool m_should_run = false;
    uint32_t m_current_event_id = 0;
    std::unordered_map<EventType, std::vector<std::pair<uint32_t, EventCallbackHandler>>> m_event_callbacks;

    std::unique_ptr<WindowsWindow> m_window;
    WindowProperties m_window_properties;

    void window_resized_event(WindowResizeEvent &);
    void window_closed_event(WindowCloseEvent &);
};
}