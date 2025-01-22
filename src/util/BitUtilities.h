#pragma once

#include <cstdint>

namespace gbcemu {

class BitUtilities final {

  public:
    static bool bit_is_set(uint8_t const &data, uint8_t const bit_to_test) { return ((data >> bit_to_test) & 0x01) == 1; }

    static bool bit_is_set(uint16_t const &data, uint8_t const bit_to_test) { return ((data >> bit_to_test) & 0x01) == 1; }

    static void set_bit_in_byte(uint8_t &byte, uint8_t const bit_to_set) { byte |= (0x01 << bit_to_set); }

    static void set_bit_in_word(uint16_t &word, uint8_t const bit_to_set) { word |= (0x01 << bit_to_set); }

    static void reset_bit_in_byte(uint8_t &byte, uint8_t const bit_to_clear) { byte &= ~(0x01 << bit_to_clear); }

    static void reset_bit_in_word(uint16_t &word, uint8_t const bit_to_clear) { word &= ~(0x01 << bit_to_clear); }
};
}