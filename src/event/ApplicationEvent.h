#pragma once

#include "event/Event.h"
#include "util/GeneralUtilities.h"
#include <cstdint>

namespace gbcemu {

class WindowResizeEvent final : public Event {
  public:
    WindowResizeEvent(uint32_t const width, uint32_t const height) : m_width(width), m_height(height) {
        set_event_type(GeneralUtilities::formatted_string("WindowResizeEvent: %i, %i", m_width, m_height), gbcemu::EventType::WindowResized);
    }

    uint32_t get_width() const { return m_width; }
    uint32_t get_height() const { return m_height; }

  private:
    uint32_t m_width;
    uint32_t m_height;
};

class WindowCloseEvent final : public Event {
  public:
    WindowCloseEvent() { set_event_type("WindowClosedEvent", gbcemu::EventType::WindowClosed); }
};
}