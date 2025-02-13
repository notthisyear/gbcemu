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

TimerController::TimerController(MMU *const mmu) : m_mmu(mmu) {
    // Note: I don't have a good justification for adding 0xC8 here, other than
    //       that it's required to pass some of the Mooneye test's. Apparantly,
    //       a DIV increase is supposed to happen after (boot sequence), seven NOP's
    //       and one JP. Initializing the lower bits of internal counter to zero puts
    //       us out of phase.
    m_div_value = ((m_mmu->get_io_register(MMU::IORegister::DIV) << 8) & 0xFF00) + 0xC8;
    m_last_output_value = false;
    m_overflow_process_pending = false;
    m_overflow_counter = 0x00;
    m_set_tima_from_tma = false;
    m_set_interrupt_flag = false;
    m_div_was_reset = false;
}

void TimerController::reset_divider() { m_div_value = 0; }

void TimerController::tima_write_occured() {
    // If a write to TIMA occures during the overflow pending cycles, we leave TIMA alone and do not set the interrupt bit
    if (m_overflow_process_pending) {
        m_set_interrupt_flag = false;
        if (m_overflow_counter < 0x03)
            m_set_tima_from_tma = false;
    }
}

void TimerController::process() {
    m_div_value++;
    m_mmu->set_io_register(MMU::IORegister::DIV, (m_div_value & 0xFF00) >> 8);

    uint8_t const tac_register{ m_mmu->get_io_register(MMU::IORegister::TAC) };
    bool const current_output_value{ BitUtilities::bit_is_set(tac_register, 2) &&
                                     BitUtilities::bit_is_set(m_div_value, s_div_bit_select.find(tac_register & 0x03)->second) };
    bool const falling_edge{ !current_output_value && m_last_output_value };

    m_last_output_value = current_output_value;
    bool overflow_happened_this_cycle{ false };

    if (falling_edge && !m_overflow_process_pending) {
        uint8_t tima_reg{ m_mmu->get_io_register(MMU::IORegister::TIMA) };
        tima_reg++;
        if (tima_reg == 0x00) {
            m_overflow_process_pending = true;
            m_overflow_counter = 0x00;
            overflow_happened_this_cycle = true;
            m_set_interrupt_flag = true;
            m_set_tima_from_tma = true;
        }
        m_mmu->set_io_register(MMU::IORegister::TIMA, tima_reg);
    }

    if (m_overflow_process_pending && !overflow_happened_this_cycle) {
        m_overflow_counter++;
        if (m_overflow_counter == 0x04) {
            m_overflow_process_pending = false;

            if (m_set_tima_from_tma)
                m_mmu->set_io_register(MMU::IORegister::TIMA, m_mmu->get_io_register(MMU::IORegister::TMA));

            if (m_set_interrupt_flag) {
                uint8_t if_register = m_mmu->get_io_register(MMU::IORegister::IF);
                BitUtilities::set_bit_in_byte(if_register, static_cast<uint8_t>(CPU::InterruptSource::Timer));
                m_mmu->set_io_register(MMU::IORegister::IF, if_register);
            }
        }
    }
}
}
