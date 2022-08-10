#include "TimerController.h"
#include "CPU.h"
#include "MMU.h"
#include "util/BitUtilities.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>

namespace gbcemu {

TimerController::TimerController(MMU *mmu) : m_mmu(mmu) {
    m_last_value = false;
    m_last_increment_was_overflow = false;
    m_current_overflow_value = m_mmu->get_io_register(MMU::IORegister::TMA);
    m_div_value = (m_mmu->get_io_register(MMU::IORegister::DIV) << 8) & 0xFF00;
    m_started = false;
    ctr = 0;
}

void TimerController::reset_divider() { m_div_value = 0; }

void TimerController::tick() {
    m_div_value++;

    uint8_t ctrl_reg = m_mmu->get_io_register(MMU::IORegister::TAC);
    // if (BitUtilities::bit_is_set(m_mmu->get_io_register(MMU::IORegister::IE), static_cast<uint8_t>(CPU::InterruptSource::Timer)))
    //     std::cout << GeneralUtilities::formatted_string("div_value: 0x%04X (TAC: 0x%02X)\n", m_div_value, ctrl_reg);

    uint8_t timer_value;

    if (m_last_increment_was_overflow) {
        timer_value = m_current_overflow_value;
        m_last_increment_was_overflow = false;
        // std::cout << "here" << std::endl;
        auto if_register = m_mmu->get_io_register(MMU::IORegister::IF);
        BitUtilities::set_bit_in_byte(if_register, static_cast<uint8_t>(CPU::InterruptSource::Timer));
        m_mmu->set_io_register(MMU::IORegister::IF, if_register);
    } else {
        timer_value = m_mmu->get_io_register(MMU::IORegister::TIMA);
    }

    auto timer_enabled = BitUtilities::bit_is_set(ctrl_reg, 2);

    // if (timer_value == 0) {
    //     m_started = true;
    //     std::cout << "TIMA set to zero!\n";
    // }

    if (timer_enabled) {
        uint8_t clock_bit_select = s_div_bit_select.find(ctrl_reg & 0x03)->second;
        auto div_selected_bit_value = BitUtilities::bit_is_set(m_div_value, clock_bit_select);
        // std::cout << GeneralUtilities::formatted_string("\ttimer_value: 0x%02X (bit %i in DIV is %c)\n", timer_value, clock_bit_select,
        //                                                 div_selected_bit_value ? '1' : '0');

        if (m_last_value && !div_selected_bit_value) {
            timer_value++;
            if (timer_value == 0x00) {
                m_last_increment_was_overflow = true;
                m_current_overflow_value = m_mmu->get_io_register(MMU::IORegister::TMA);
            }
            m_mmu->set_io_register(MMU::IORegister::TIMA, timer_value);
        }
        m_last_value = div_selected_bit_value;
    }

    m_mmu->set_io_register(MMU::IORegister::DIV, (m_div_value & 0xFF00) >> 8);
}
}
