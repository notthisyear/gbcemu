#pragma once

#include <stdint.h>

namespace gbcemu {

class BitUtilities {

  public:
    static bool bit_is_set(const uint8_t &data, const uint8_t bit_to_test) { return ((data >> bit_to_test) & 0x01) == 1; }

    static void set_bit_in_byte(uint8_t &byte, const uint8_t bit_to_set) { byte |= (0x01 << bit_to_set); }

    static void reset_bit_in_byte(uint8_t &byte, const uint8_t bit_to_clear) { byte &= ~(0x01 << bit_to_clear); }
};
}