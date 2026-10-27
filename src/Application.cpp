#include "Application.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace gbcemu {

Application::Application(std::shared_ptr<CPU> const cpu, std::shared_ptr<PPU> const ppu, std::shared_ptr<Renderer> const renderer,
                         WindowProperties const &properties)
    : m_cpu(cpu), m_ppu(ppu), m_renderer(renderer), m_window_properties(properties) {}

void Application::init() {

    m_window = std::make_unique<Window>(m_window_properties);

    if (!m_window->is_initialized) {
        m_app_should_run = false;
    }

    register_event_callback(EventType::WindowResized, [this](Event const &arg) -> void {
        return this->Application::window_resized_event(std::forward<WindowResizeEvent const &>(static_cast<WindowResizeEvent const &>(arg)));
    });

    register_event_callback(EventType::WindowClosed, [this](Event const &arg) -> void {
        return this->Application::window_closed_event(std::forward<WindowCloseEvent const &>(static_cast<WindowCloseEvent const &>(arg)));
    });

    register_event_callback(EventType::KeyPressed, [this](Event const &arg) -> void {
        return this->Application::key_pressed_event(std::forward<KeyPressedEvent const &>(static_cast<KeyPressedEvent const &>(arg)));
    });

    register_event_callback(EventType::KeyReleased, [this](Event const &arg) -> void {
        return this->Application::key_released_event(std::forward<KeyReleasedEvent const &>(static_cast<KeyReleasedEvent const &>(arg)));
    });

    m_window->set_event_callback([this](Event const &arg) -> void { return this->Application::handle_event(std::forward<decltype(arg)>(arg)); });

    m_app_should_run = true;
}

void Application::set_cpu_debug_mode(bool const on_or_off) { m_cpu_should_run = !on_or_off; }

void Application::pause_cpu() {
    m_cpu_is_paused = false;
    m_cpu_should_run = false;

    // Wait until the CPU has stopped, so that it is safe to inspect its state
    while (!m_cpu_is_paused && m_app_should_run) {
        std::this_thread::yield();
    }
}

void Application::resume_cpu() { m_cpu_should_run = true; }

void Application::step_cpu_instruction() {
    m_requested_instruction_steps++;

    // Wait until the instruction has been executed
    while (m_requested_instruction_steps > 0 && m_app_should_run) {
        std::this_thread::yield();
    }
}

bool Application::cpu_is_running() const { return m_cpu_should_run; }

void Application::run() {
    m_next_frame_deadline = std::chrono::steady_clock::now();

    while (m_app_should_run) {
        if (m_cpu_should_run) {
            m_cpu->tick();
            if (m_cpu->breakpoint_hit()) {
                m_cpu_should_run = false;
                LogUtilities::log_info(std::cout, GeneralUtilities::formatted_string("Breakpoint hit at 0x%04X", m_cpu->current_instruction_address()));
            }

            if (++m_ticks_in_current_frame == kTicksPerFrame) {
                m_ticks_in_current_frame = 0;
                pace_frame();
            }
        } else {
            m_cpu_is_paused = true;

            if (m_requested_instruction_steps > 0) {
                do {
                    m_cpu->tick();
                } while (!m_cpu->at_start_of_instruction());
                m_requested_instruction_steps--;
            } else {
                // Keep the window responsive, but avoid spinning at full speed while paused
                m_window->poll_events();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        if (m_ppu->cycles_per_frame_reached()) {
            m_ppu->acknowledge_frame();
            draw_frame();
        }
    }
}

void Application::pace_frame() {
    m_window->poll_events();

    if (m_fast_forward)
        return;

    m_next_frame_deadline += kFrameDuration;
    auto const now{ std::chrono::steady_clock::now() };

    if (now > m_next_frame_deadline + kMaxFrameLag) {
        // We have fallen too far behind (e.g. after a debugger pause or fast-forwarding), start over from now
        m_next_frame_deadline = now;
    } else {
        std::this_thread::sleep_until(m_next_frame_deadline);
    }
}

void Application::draw_frame() {
    auto const now{ std::chrono::steady_clock::now() };
    if (m_fast_forward && (now - m_last_draw) < kFastForwardDrawInterval)
        return;

    m_last_draw = now;
    m_renderer->update_framebuffer_and_draw(m_ppu->get_framebuffer());
    m_window->swap_buffers();
}

uint32_t Application::register_event_callback(EventType const event_type, EventCallbackHandler const callback) {
    if (!m_event_callbacks.count(event_type)) {
        m_event_callbacks.emplace(event_type, std::vector<std::pair<uint32_t, EventCallbackHandler>>());
    }
    auto &list{ m_event_callbacks.at(event_type) };
    uint32_t const event_id{ m_current_event_id++ };

    list.push_back(std::make_pair(event_id, callback));
    return event_id;
}

bool Application::try_remove_event_callback(EventType const event_type, uint32_t const event_id) {
    if (!m_event_callbacks.count(event_type)) {
        return false;
    }

    bool element_deleted{ false };
    auto &list{ m_event_callbacks.at(event_type) };
    for (std::size_t i{ 0 }; i < list.size(); ++i) {
        if (list.at(i).first == event_id) {
            list.erase(list.begin() + i);
            element_deleted = true;
            break;
        }
    }

    return element_deleted;
}

void Application::handle_event(Event const &e) {
    // TODO: These this should perhaps be in a separate thread. As of now, its blocking, which doesn't feel good

    if (!m_event_callbacks.count(e.get_event_type())) {
        return;
    }

    for (auto const &callback_pair : m_event_callbacks.at(e.get_event_type())) {
        callback_pair.second(e);
    }
}

void Application::window_closed_event(WindowCloseEvent const &e) {
    LogUtilities::log_info(std::cout, e.to_string());
    m_app_should_run = false;
    return;
}

void Application::window_resized_event(WindowResizeEvent const &e) {
    m_renderer->set_viewport(e.get_width(), e.get_height());
    return;
}

void Application::key_pressed_event(KeyPressedEvent const &e) {
    if (e.get_key_code() == static_cast<KeyCode>(kFastForwardKey))
        m_fast_forward = true;
}

void Application::key_released_event(KeyReleasedEvent const &e) {
    if (e.get_key_code() == static_cast<KeyCode>(kFastForwardKey))
        m_fast_forward = false;
}

Application::~Application() {}
}