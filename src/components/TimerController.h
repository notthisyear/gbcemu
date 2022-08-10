#pragma once

#include <memory>
#include <stdint.h>
#include <unordered_map>

namespace gbcemu {

// Forward declare MMU
class MMU;

class TimerController {
  public:
    TimerController(MMU *);
    void tick();
    void reset_divider();

  private:
    MMU *m_mmu;

    uint16_t m_div_value;
    bool m_last_value;
    bool m_last_increment_was_overflow;
    uint8_t m_current_overflow_value;

    bool m_started;
    int ctr;
    static inline std::unordered_map<uint8_t, uint8_t> s_div_bit_select = {
        // Falling edge detector, so every second flip is detected
        { 0, 9 }, // 9th bit becomes low every 1024 tick
        { 1, 3 }, // 3rd bit becomes low every 16 tick
        { 2, 5 }, // 5th bit becomes low every 64 tick
        { 3, 7 }, // 3th bit becomes low every 256 tick
    };
};
}
