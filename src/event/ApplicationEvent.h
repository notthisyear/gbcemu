#pragma once

#include "event/Event.h"
#include "util/GeneralUtilities.h"
#include <cstdint>

namespace gbcemu {

class WindowResizeEvent final : public Event {
  public:
    WindowResizeEvent(std::uint32_t const width, std::uint32_t const height) : m_width{ width }, m_height{ height } {
        set_event_type(GeneralUtilities::formatted_string("WindowResizeEvent: %i, %i", m_width, m_height), gbcemu::EventType::WindowResized);
    }

    std::uint32_t get_width() const { return m_width; }
    std::uint32_t get_height() const { return m_height; }

  private:
    std::uint32_t m_width;
    std::uint32_t m_height;
};

class WindowCloseEvent final : public Event {
  public:
    WindowCloseEvent() { set_event_type("WindowClosedEvent", gbcemu::EventType::WindowClosed); }
};
}