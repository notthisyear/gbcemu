#include "TimerController.h"
#include "CPU.h"
#include "MMU.h"
#include "util/BitUtilities.h"

namespace gbcemu {

TimerController *TimerController::m_instance;

TimerController *TimerController::get(MMU *const mmu) {
    if (!m_instance)
        m_instance = new TimerController(mmu);
    return m_instance;
}

TimerController::TimerController(MMU *const mmu) : m_mmu{ mmu } {
    // DIV is only the upper byte of a 16-bit counter that increments every T-cycle from power-on.
    // The boot ROM always runs for the same number of cycles, so when it hands over at 0x0100 the
    // whole counter has a fixed value on a given model, not just its upper byte. On DMG ABC it is
    // 0xABCC. Since we skip the boot ROM, both bytes must be restored. The lower byte can't be read
    // directly, but it decides when DIV next increments and when the TIMA taps (bits 3/5/7/9) fall.
    //
    // The lower byte follows from Mooneye's boot_div-dmgABCmgb: after NOP; JP $0150 at 0x0100 and six
    // more NOPs, the read in LDH A,(DIV) (its third M-cycle) must come right after DIV goes from
    // 0xAB to 0xAC. That is 13 M-cycles = 52 T-cycles after handover, and 0xAC00 - 52 = 0xABCC.
    //
    // The CPU only accesses the bus once per M-cycle, so the tests constrain the value to 4 T-cycles:
    // 0xCB-0xCE all pass in this emulator, while 0xCA and 0xCF fail boot_div.
    m_div_value = ((m_mmu->get_io_register(MMU::IORegister::DIV) << 8) & 0xFF00) + 0xCC;
    m_last_output_value = false;
    m_overflow_state = OverflowState::Idle;
    m_overflow_state_ticks = 0U;
}

void TimerController::reset_divider() { m_div_value = 0; }

bool TimerController::tima_write_occured() {
    switch (m_overflow_state) {
    case OverflowState::Overflowed:
        // The written value is kept, and neither the reload from TMA nor the interrupt will happen
        m_overflow_state = OverflowState::Idle;
        return true;
    case OverflowState::Reloading:
        // TIMA is being loaded from TMA, so the write is ignored
        return false;
    case OverflowState::Idle:
    default:
        return true;
    }
}

void TimerController::tma_write_occured(std::uint8_t const value) {
    // While TIMA is being loaded from TMA, the new TMA value is loaded into TIMA as well
    if (m_overflow_state == OverflowState::Reloading)
        m_mmu->set_io_register(MMU::IORegister::TIMA, value);
}

void TimerController::process() {
    m_div_value++;
    m_mmu->set_io_register(MMU::IORegister::DIV, (m_div_value & 0xFF00) >> 8);

    std::uint8_t const tac_register{ m_mmu->get_io_register(MMU::IORegister::TAC) };
    bool const current_output_value{ BitUtilities::bit_is_set(tac_register, 2) &&
                                     BitUtilities::bit_is_set(m_div_value, s_div_bit_select.find(tac_register & 0x03)->second) };
    bool const falling_edge{ !current_output_value && m_last_output_value };

    m_last_output_value = current_output_value;
    bool overflow_happened_this_cycle{ false };

    if (falling_edge && m_overflow_state != OverflowState::Overflowed) {
        std::uint8_t tima_reg{ m_mmu->get_io_register(MMU::IORegister::TIMA) };
        tima_reg++;
        if (tima_reg == 0x00) {
            m_overflow_state = OverflowState::Overflowed;
            m_overflow_state_ticks = 0U;
            overflow_happened_this_cycle = true;
        }
        m_mmu->set_io_register(MMU::IORegister::TIMA, tima_reg);
    }

    if (m_overflow_state == OverflowState::Idle || overflow_happened_this_cycle)
        return;

    if (++m_overflow_state_ticks < kTicksPerOverflowState)
        return;

    m_overflow_state_ticks = 0U;

    if (m_overflow_state == OverflowState::Overflowed) {
        m_overflow_state = OverflowState::Reloading;
        m_mmu->set_io_register(MMU::IORegister::TIMA, m_mmu->get_io_register(MMU::IORegister::TMA));

        std::uint8_t if_register{ m_mmu->get_io_register(MMU::IORegister::IF) };
        BitUtilities::set_bit_in_byte(if_register, static_cast<std::uint8_t>(CPU::InterruptSource::Timer));
        m_mmu->set_io_register(MMU::IORegister::IF, if_register);
    }
    else {
        m_overflow_state = OverflowState::Idle;
    }
}
}
