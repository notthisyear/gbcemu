#pragma once

#include <memory>
#include <stdint.h>
#include <unordered_map>

namespace gbcemu {

// Forward declare MMU
class MMU;

class TimerController {
  public:
    void process();
    void reset_divider();
    void tima_write_occured();

    static TimerController *get(MMU *);

  private:
    static TimerController *m_instance;
    MMU *m_mmu;
    TimerController(MMU *);

    uint16_t m_div_value;
    bool m_last_output_value;
    uint8_t m_overflow_process_pending;
    uint8_t m_overflow_counter;
    bool m_set_tima_from_tma;
    bool m_set_interrupt_flag;
    bool m_div_was_reset;

    static inline std::unordered_map<uint8_t, uint8_t> s_div_bit_select = {
        // Falling edge detector, so every second flip is detected
        { 0, 9 }, // 9th bit becomes low every 1024 tick
        { 1, 3 }, // 3rd bit becomes low every 16 tick
        { 2, 5 }, // 5th bit becomes low every 64 tick
        { 3, 7 }, // 3th bit becomes low every 256 tick
    };
};
}
