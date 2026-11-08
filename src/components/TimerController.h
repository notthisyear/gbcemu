#pragma once

#include <cstdint>
#include <unordered_map>

namespace gbcemu {

// Forward declare MMU
class MMU;

class TimerController final {
  public:
    void process();
    void reset_divider();

    // Returns false if the write should be ignored, i.e. TIMA must keep its current value
    bool tima_write_occured();

    void tma_write_occured(std::uint8_t const);

    static TimerController *get(MMU *const);

  private:
    // After TIMA overflows, it reads 0x00 for one M-cycle (Overflowed). In the following M-cycle (Reloading),
    // TIMA is loaded from TMA and the timer interrupt is requested. During that M-cycle, writes to TIMA are
    // ignored, while writes to TMA are also written to TIMA.
    enum class OverflowState : std::uint8_t {
        Idle,
        Overflowed,
        Reloading,
    };

    static constexpr std::uint8_t kTicksPerOverflowState{ 4U };

    static TimerController *m_instance;
    MMU *m_mmu;
    TimerController(MMU *const);

    std::uint16_t m_div_value;
    bool m_last_output_value;
    OverflowState m_overflow_state;
    std::uint8_t m_overflow_state_ticks;

    static inline std::unordered_map<std::uint8_t, std::uint8_t> s_div_bit_select{
        // Falling edge detector, so every second flip is detected
        { 0, 9 }, // 9th bit becomes low every 1024 tick
        { 1, 3 }, // 3rd bit becomes low every 16 tick
        { 2, 5 }, // 5th bit becomes low every 64 tick
        { 3, 7 }, // 3th bit becomes low every 256 tick
    };
};
}
