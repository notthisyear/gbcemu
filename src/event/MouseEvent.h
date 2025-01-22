#pragma once

#include "common/MouseCode.h"
#include "event/Event.h"
#include "util/GeneralUtilities.h"

namespace gbcemu {

class MouseMovedEvent final : public Event {
  public:
    MouseMovedEvent(float const x, float const y) : m_mouse_x(x), m_mouse_y(y) {
        set_event_type(GeneralUtilities::formatted_string("MouseMovedEvent: %.1f, %.1f", m_mouse_x, m_mouse_y), gbcemu::EventType::MouseMoved);
    }

    float get_mouse_x() const { return m_mouse_x; }
    float get_mouse_y() const { return m_mouse_y; }

  private:
    float m_mouse_x;
    float m_mouse_y;
};

class MouseScrolledEvent final : public Event {
  public:
    MouseScrolledEvent(float const x, float const y) : m_offset_x(x), m_offset_y(y) {
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
    MouseButtonEvent(MouseCode const mouse_code) : m_mouse_code(mouse_code) {}

    MouseCode m_mouse_code;
};

class MouseButtonPressedEvent final : public MouseButtonEvent {
  public:
    MouseButtonPressedEvent(MouseCode const mouse_code) : MouseButtonEvent(mouse_code) {
        set_event_type(GeneralUtilities::formatted_string("MouseButtonPressedEvent (mouse code %i)", m_mouse_code), gbcemu::EventType::MouseButtonPressed);
    }
};

class MouseButtonReleasedEvent final : public MouseButtonEvent {
  public:
    MouseButtonReleasedEvent(MouseCode const mouse_code) : MouseButtonEvent(mouse_code) {
        set_event_type(GeneralUtilities::formatted_string("MouseButtonReleasedEvent (mouse code %i)", m_mouse_code), gbcemu::EventType::MouseButtonReleased);
    }
};
}
