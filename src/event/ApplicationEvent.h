#pragma once

#include "event/Event.h"
#include "util/GeneralUtilities.h"

namespace gbcemu {

class WindowResizeEvent : public Event {
  public:
    WindowResizeEvent(unsigned int width, unsigned int height) : m_width(width), m_height(height) {
        set_event_type(GeneralUtilities::formatted_string("WindowResizeEvent: %i, %i", m_width, m_height), gbcemu::EventType::WindowResized);
    }

    unsigned int get_width() const { return m_width; }
    unsigned int get_height() const { return m_height; }

  private:
    unsigned int m_width;
    unsigned int m_height;
};

class WindowCloseEvent : public Event {
  public:
    WindowCloseEvent() { set_event_type("WindowClosedEvent", gbcemu::EventType::WindowClosed); }
};
}