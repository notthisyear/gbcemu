#pragma once

#include <functional>
#include <string>

namespace gbcemu {

enum class EventType {
    None,
    WindowClosed,
    WindowResized,
    KeyPressed,
    KeyReleased,
    CharacterTyped,
    MouseMoved,
    MouseScrolled,
    MouseButtonPressed,
    MouseButtonReleased
};

class Event {

  public:
    virtual ~Event() = default;

    EventType get_event_type() const { return m_event_type; }

    std::string to_string() const { return m_event_as_string; }

  protected:
    void set_event_type(const std::string &event_as_string, EventType event_type) {
        m_event_as_string = event_as_string;
        m_event_type = event_type;
    }

  private:
    EventType m_event_type = EventType::None;
    std::string m_event_as_string = "";
};

using EventCallbackHandler = std::function<void(Event const &)>;

}
