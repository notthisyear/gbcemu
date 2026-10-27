#pragma once

#include "components/CPU.h"
#include "event/ApplicationEvent.h"
#include "event/Event.h"
#include "event/KeyboardEvent.h"
#include "opengl/Renderer.h"
#include "window/Window.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

namespace gbcemu {

class Application final {
  public:
    Application(std::shared_ptr<CPU> const, std::shared_ptr<PPU> const, std::shared_ptr<Renderer> const, WindowProperties const &);

    void init();
    void run();
    void set_cpu_debug_mode(bool const);

    // Thread-safe CPU controls for the debugger. The CPU is only ever ticked by the thread calling run().
    void pause_cpu();
    void resume_cpu();
    void step_cpu_instruction();
    bool cpu_is_running() const;

    std::uint32_t register_event_callback(EventType const, EventCallbackHandler const);
    void handle_event(Event const &);
    bool try_remove_event_callback(EventType const, std::uint32_t const);

    ~Application();

  private:
    // A frame is 70224 ticks (T-cycles) at 4.194304 MHz, i.e. ~16.74 ms. The emulation is paced per frame worth of
    // ticks (rather than per frame drawn by the PPU) so that pacing also works while the LCD is turned off.
    static constexpr std::uint32_t kTicksPerFrame{ 70224U };
    static constexpr std::uint64_t kTicksPerSecond{ 4194304U };
    static constexpr std::chrono::nanoseconds kFrameDuration{ kTicksPerFrame * 1'000'000'000ULL / kTicksPerSecond };

    // Don't try to catch up if the emulation has fallen further behind than this (e.g. after a debugger pause)
    static constexpr std::chrono::milliseconds kMaxFrameLag{ 100 };

    // When fast-forwarding, only draw a frame this often to avoid drawing becoming the bottleneck
    static constexpr std::chrono::milliseconds kFastForwardDrawInterval{ 16 };

    static constexpr Key kFastForwardKey{ Key::Tab };

    bool m_fast_forward{ false };
    std::uint32_t m_ticks_in_current_frame{ 0U };
    std::chrono::steady_clock::time_point m_next_frame_deadline{};
    std::chrono::steady_clock::time_point m_last_draw{};

    std::atomic<bool> m_app_should_run{ false };
    std::atomic<bool> m_cpu_should_run{ true };

    // Set by the run() thread once it has observed that the CPU should not run, i.e. it has stopped ticking the CPU
    std::atomic<bool> m_cpu_is_paused{ false };
    std::atomic<std::uint32_t> m_requested_instruction_steps{ 0U };

    std::uint32_t m_current_event_id{ 0U };

    std::shared_ptr<CPU> m_cpu;
    std::shared_ptr<PPU> m_ppu;
    std::shared_ptr<Renderer> m_renderer;
    WindowProperties m_window_properties;

    std::unique_ptr<Window> m_window{ nullptr };

    std::unordered_map<EventType, std::vector<std::pair<std::uint32_t, EventCallbackHandler>>> m_event_callbacks;

    void window_resized_event(WindowResizeEvent const &);
    void window_closed_event(WindowCloseEvent const &);
    void key_pressed_event(KeyPressedEvent const &);
    void key_released_event(KeyReleasedEvent const &);

    void pace_frame();
    void draw_frame();
};
}