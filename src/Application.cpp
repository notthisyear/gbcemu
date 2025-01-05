#include "Application.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>

namespace gbcemu {

Application::Application(const std::shared_ptr<CPU> cpu, const std::shared_ptr<PPU> ppu, const std::shared_ptr<Renderer> renderer,
                         const WindowProperties &properties)
    : m_cpu(cpu), m_ppu(ppu), m_renderer(renderer), m_window_properties(properties) {}

void Application::init() {

    m_window = std::make_unique<WindowsWindow>(m_window_properties);

    if (!m_window->is_initialized) {
        m_app_should_run = false;
    }

    register_event_callback(EventType::WindowResized, [this](Event const &arg) -> void {
        return this->Application::window_resized_event(std::forward<WindowResizeEvent const &>(static_cast<WindowResizeEvent const &>(arg)));
    });

    register_event_callback(EventType::WindowClosed, [this](Event const &arg) -> void {
        return this->Application::window_closed_event(std::forward<WindowCloseEvent const &>(static_cast<WindowCloseEvent const &>(arg)));
    });

    m_window->set_event_callback([this](Event const &arg) -> void { return this->Application::handle_event(std::forward<decltype(arg)>(arg)); });

    m_app_should_run = true;
}

void Application::set_cpu_debug_mode(const bool on_or_off) { m_cpu_should_run = !on_or_off; }

void Application::run() {
    while (m_app_should_run) {
        if (m_cpu_should_run) {
            m_cpu->tick();
            if (m_cpu->breakpoint_hit()) {
                set_cpu_debug_mode(true);
            }
        }

        if (m_ppu->cycles_per_frame_reached()) {
            m_ppu->acknowledge_frame();
            m_renderer->update_framebuffer_and_draw(m_ppu->get_framebuffer());
            m_window->update();
        }
    }
}

uint32_t Application::register_event_callback(const EventType event_type, const EventCallbackHandler callback) {
    if (!m_event_callbacks.count(event_type))
        m_event_callbacks.emplace(event_type, std::vector<std::pair<uint32_t, EventCallbackHandler>>());

    auto &list{ m_event_callbacks.at(event_type) };
    std::uint32_t const event_id{ m_current_event_id++ };

    list.push_back(std::make_pair(event_id, callback));
    return event_id;
}

bool Application::try_remove_event_callback(const EventType event_type, const uint32_t event_id) {
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

    for (const auto &callback_pair : m_event_callbacks.at(e.get_event_type())) {
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

Application::~Application() {}
}