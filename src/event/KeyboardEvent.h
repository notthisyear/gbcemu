#pragma once

#include "common/KeyCode.h"
#include "event/Event.h"
#include "util/GeneralUtilities.h"

namespace gbcemu {

class KeyboardEvent : public Event {

  public:
    KeyCode get_key_code() const { return m_key_code; }

  protected:
    KeyboardEvent(KeyCode const key_code) : m_key_code(key_code) {}

    KeyCode m_key_code;
};

class KeyPressedEvent final : public KeyboardEvent {

  public:
    KeyPressedEvent(KeyCode const key_code, bool const is_repeated) : KeyboardEvent(key_code), m_is_repeated(is_repeated) {
        set_event_type(GeneralUtilities::formatted_string("KeyPressedEvent: %i (%s)", m_key_code, m_is_repeated ? "repeat" : "single"), EventType::KeyPressed);
    }

    bool get_is_repeated() const { return m_is_repeated; }

  private:
    bool m_is_repeated;
};

class KeyReleasedEvent final : public KeyboardEvent {
  public:
    KeyReleasedEvent(KeyCode const key_code) : KeyboardEvent(key_code) {
        set_event_type(GeneralUtilities::formatted_string("KeyReleasedEvent: %i", m_key_code), EventType::KeyReleased);
    }
};

class CharacterTypedEvent final : public KeyboardEvent {
  public:
    CharacterTypedEvent(KeyCode const key_code) : KeyboardEvent(key_code) {
        set_event_type(GeneralUtilities::formatted_string("CharacterTypedEvent: %i", m_key_code), EventType::CharacterTyped);
    }
};
}