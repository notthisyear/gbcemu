#include "CPU.h"
#include "OpcodeBuilder.h"
#include "Opcodes.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iomanip>
#include <memory>

namespace gbcemu {

CPU::CPU(std::shared_ptr<MMU> mmu, std::shared_ptr<PPU> ppu)
    : m_mmu(mmu), m_ppu(ppu), m_reg_af(0x0000), m_reg_bc(0x0000), m_reg_de(0x0000), m_reg_hl(0x0000), m_reg_sp(0x0000), m_reg_pc(0x0000) {}

void CPU::tick() {
    auto current_cycles_before_execution = m_current_cycle_count;
    auto opcode = get_next_opcode();
    opcode->execute(this, m_mmu.get());
    m_current_cycle_count += opcode->cycles;

    m_ppu->tick(m_current_cycle_count - current_cycles_before_execution);

    if (m_current_cycle_count > CpuCyclesPerFrame)
        m_current_cycle_count -= CpuCyclesPerFrame;
}

std::shared_ptr<Opcode> CPU::get_next_opcode(bool should_disassemble) {
    uint8_t current_instruction;
    m_mmu->try_read_from_memory(&current_instruction, m_reg_pc, 1);
    m_reg_pc++;

    auto is_extended = is_extended_opcode(current_instruction);
    if (is_extended) {
        m_mmu->try_read_from_memory(&current_instruction, m_reg_pc, 1);
        m_reg_pc++;
    }

    std::shared_ptr<Opcode> opcode = decode_opcode(current_instruction, is_extended);
    if (opcode->size > 1) {
        uint8_t *remainder = new uint8_t[opcode->size - 1];
        m_mmu->try_read_from_memory(remainder, m_reg_pc, opcode->size - 1);
        m_reg_pc += opcode->size - 1;
        opcode->set_opcode_data(remainder, should_disassemble);
        delete[] remainder;
    }

    return opcode;
}

void CPU::print_disassembled_instructions(std::ostream &stream, uint16_t number_of_instructions) {
    auto current_pc = m_reg_pc;
    for (auto i = 0; i < number_of_instructions; i++) {
        stream << "\033[1;37m" << std::left << std::setw(10) << std::setfill(' ') << GeneralUtilities::formatted_string("0x%04X", m_reg_pc);
        stream << "\033[0;m";
        auto opcode = get_next_opcode(true);
        stream << opcode->fully_disassembled_instruction() << std::endl;
    }

    m_reg_pc = current_pc;
}

void CPU::set_interrupt_enable(bool on_or_off) { m_interrupt_enabled = on_or_off; }

void CPU::enable_breakpoint_at(uint16_t pc) { m_current_breakpoint = pc; }

bool CPU::breakpoint_hit() const { return m_current_breakpoint == m_reg_pc; }

void CPU::clear_breakpoint() { m_current_breakpoint = 0; }

void CPU::stop_execution() { m_execution_stop_called = true; }

void CPU::set_debug_mode(bool on_or_off) { m_is_in_debug_mode = false; }

bool CPU::half_carry_occurs_on_subtract(uint8_t v, const uint8_t value_to_subtract) const { return ((v & 0x0F) - (value_to_subtract & 0x0F)) < 0; }

bool CPU::half_carry_occurs_on_add(uint8_t v, const uint8_t value_to_add) const { return ((v & 0x0F) + (value_to_add & 0x0F)) > 0x0F; }

bool CPU::half_carry_occurs_on_add(uint16_t v, const uint16_t value_to_add) const { return ((v & 0x0FFF) + (value_to_add & 0x0FFF)) > 0x0FFF; }

bool CPU::carry_occurs_on_add(uint8_t v, const uint8_t value_to_add) const { return (uint16_t)(v + value_to_add) > 0xFF; }

bool CPU::carry_occurs_on_add(uint16_t v, const uint16_t value_to_add) const { return (uint32_t)(v + value_to_add) > 0xFFFF; }

bool CPU::carry_occurs_on_subtract(uint8_t v, const uint8_t value_to_subtract) const { return value_to_subtract > v; };

void CPU::print_state(std::ostream &stream) const {

    stream << std::endl;

    print_reg(stream, CPU::Register::AF, false);
    stream << "\t";
    print_reg(stream, CPU::Register::BC, true);
    print_reg(stream, CPU::Register::DE, false);
    stream << "\t";
    print_reg(stream, CPU::Register::HL, true);

    print_sp_and_pc(stream);

    stream << std::endl;

    print_flag_value(stream, "ZF", flag_is_set(Flag::Z), false);
    stream << "\t";
    print_flag_value(stream, "NF", flag_is_set(Flag::N), true);
    print_flag_value(stream, "HF", flag_is_set(Flag::H), false);
    stream << "\t";
    print_flag_value(stream, "CF", flag_is_set(Flag::C), true);
    print_flag_value(stream, "IE", m_interrupt_enabled, true);

    stream << std::endl;

    print_additional_info(stream);
    stream << std::endl;
}

void CPU::print_flag_value(std::ostream &stream, const std::string &flag_name, const bool value, const bool insert_newline) const {
    stream << "\033[0;33m" << flag_name << ": "
           << "\033[1;37m" << LogUtilities::to_tf(value) << "\033[0m";
    if (insert_newline)
        stream << std::endl;
}

void CPU::print_reg(std::ostream &stream, const CPU::Register reg, const bool insert_newline) const {
    stream << "\033[0;35m" << register_name.find(reg)->second << ": "
           << "\033[1;37m" << GeneralUtilities::formatted_string("0x%04x", get_16_bit_register(reg)) << "\033[0m";
    if (insert_newline)
        stream << std::endl;
}

void CPU::print_sp_and_pc(std::ostream &stream) const {
    stream << "\033[0;36m" << register_name.find(CPU::Register::SP)->second << ": "
           << "\033[1;37m" << GeneralUtilities::formatted_string("0x%04x", get_16_bit_register(CPU::Register::SP)) << "\033[0;36m\t"
           << register_name.find(CPU::Register::PC)->second << ": "
           << "\033[1;37m" << GeneralUtilities::formatted_string("0x%04x", get_16_bit_register(CPU::Register::PC)) << "\033[0m" << std::endl;
}

void CPU::print_additional_info(std::ostream &stream) const {

    auto breakpoint_string = m_current_breakpoint != 0 ? GeneralUtilities::formatted_string("0x%04x", m_current_breakpoint) : "none";
    stream << "\033[0;32mbreakpoint: "
           << "\033[1;37m" << breakpoint_string << "\033[0;32m\tcurrent_cycles: "
           << "\033[1;37m" << m_current_cycle_count << "\033[0;m" << std::endl;
}

CPU::~CPU() {}

}