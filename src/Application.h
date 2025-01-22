#pragma once

#include "components/CPU.h"
#include "event/ApplicationEvent.h"
#include "event/Event.h"
#include "opengl/Renderer.h"
#include "windows/WindowsWindow.h"
#include <memory>

namespace gbcemu {

class Application final {
  public:
    Application(std::shared_ptr<CPU> const, std::shared_ptr<PPU> const, std::shared_ptr<Renderer> const, WindowProperties const &);

    void init();
    void run();
    void set_cpu_debug_mode(bool const);
    void set_cpu_breakpoint(uint16_t const);

    uint32_t register_event_callback(EventType const, EventCallbackHandler const);
    void handle_event(Event const &);
    bool try_remove_event_callback(EventType const, uint32_t const);

    ~Application();

  private:
    static float constexpr kMaxTimePerFrameSec{ 0.02f };

    bool m_app_should_run{ false };
    bool m_cpu_should_run{ true };

    uint32_t m_current_event_id{ 0U };

    std::shared_ptr<CPU> m_cpu;
    std::shared_ptr<PPU> m_ppu;
    std::shared_ptr<Renderer> m_renderer;
    WindowProperties m_window_properties;

    std::unique_ptr<WindowsWindow> m_window{ nullptr };

    std::unordered_map<EventType, std::vector<std::pair<uint32_t, EventCallbackHandler>>> m_event_callbacks;

    void window_resized_event(WindowResizeEvent const &);
    void window_closed_event(WindowCloseEvent const &);
};
}