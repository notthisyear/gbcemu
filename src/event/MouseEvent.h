#pragma once

#include "common/MouseCode.h"
#include "event/Event.h"
#include "util/GeneralUtilities.h"

namespace gbcemu {

class MouseMovedEvent : public Event {
  public:
    MouseMovedEvent(const float x, const float y) : m_mouse_x(x), m_mouse_y(y) {
        set_event_type(GeneralUtilities::formatted_string("MouseMovedEvent: %.1f, %.1f", m_mouse_x, m_mouse_y), gbcemu::EventType::MouseMoved);
    }

    float get_mouse_x() const { return m_mouse_x; }
    float get_mouse_y() const { return m_mouse_y; }

  private:
    float m_mouse_x;
    float m_mouse_y;
};

class MouseScrolledEvent : public Event {
  public:
    MouseScrolledEvent(const float x, const float y) : m_offset_x(x), m_offset_y(y) {
        set_event_type(GeneralUtilities::formatted_string("MouseScrolledEvent: %.1f, %.1f", m_offset_x, m_offset_y), gbcemu::EventType::MouseScrolled);
    }

    float get_offset_x() const { return m_offset_x; }
    float get_offset_y() const { return m_offset_y; }

  private:
    float m_offset_x;
    float m_offset_y;
};

class MouseButtonEvent : public Event {
  public:
    MouseCode get_mouse_code() const { return m_mouse_code; }

  protected:
    MouseButtonEvent(const MouseCode mouse_code) : m_mouse_code(mouse_code) {}

    MouseCode m_mouse_code;
};

class MouseButtonPressedEvent : public MouseButtonEvent {
  public:
    MouseButtonPressedEvent(const MouseCode mouse_code) : MouseButtonEvent(mouse_code) {
        set_event_type(GeneralUtilities::formatted_string("MouseButtonPressedEvent (mouse code %i)", m_mouse_code), gbcemu::EventType::MouseButtonPressed);
    }
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
  public:
    MouseButtonReleasedEvent(const MouseCode mouse_code) : MouseButtonEvent(mouse_code) {
        set_event_type(GeneralUtilities::formatted_string("MouseButtonReleasedEvent (mouse code %i)", m_mouse_code), gbcemu::EventType::MouseButtonReleased);
    }
};
}
