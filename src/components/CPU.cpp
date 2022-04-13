#include "CPU.h"
#include "OpcodeBuilder.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <memory>

namespace gbcemu {
CPU::CPU(std::shared_ptr<MMU> mmu) : m_mmu(mmu), m_reg_af(0x0000), m_reg_bc(0x0000), m_reg_de(0x0000), m_reg_hl(0x0000), m_reg_sp(0x0000), m_reg_pc(0x0000) {}

void CPU::tick() {
    uint8_t current_instruction;
    m_mmu->try_read_from_memory(&current_instruction, m_reg_pc, 1);

    if (m_debug_is_on)
        log(LogLevel::Debug, GeneralUtilities::formatted_string("Read 0x%x at PC: 0x%x", current_instruction, m_reg_pc));
    m_reg_pc++;

    auto is_extended = is_extended_opcode(current_instruction);
    if (is_extended) {
        m_mmu->try_read_from_memory(&current_instruction, m_reg_pc, 1);
        if (m_debug_is_on)
            log(LogLevel::Debug, GeneralUtilities::formatted_string("Read 0x%x at PC: 0x%x", current_instruction, m_reg_pc));
        m_reg_pc++;
    }

    auto opcode = decode_opcode(current_instruction, is_extended);

    if (opcode == nullptr) {
        log(LogLevel::Warning, GeneralUtilities::formatted_string("Instruction '0x%x' at 0x%x not implemented", current_instruction, m_reg_pc - 1));
        return;
    }

    if (m_debug_is_on)
        log(LogLevel::Debug, GeneralUtilities::formatted_string("Instruction decoded as '%s', %d more bytes will be read", opcode->name, opcode->size - 1));

    if (opcode->size > 1) {
        uint8_t *remainder = new uint8_t[opcode->size - 1];
        m_mmu->try_read_from_memory(remainder, m_reg_pc, opcode->size - 1);
        m_reg_pc += opcode->size - 1;
        opcode->set_opcode_data(remainder);
        delete[] remainder;
    }

    if (m_show_disassembled_instruction)
        log(LogLevel::Debug, opcode->fully_disassembled_instruction());

    opcode->execute(this, m_mmu.get());

    if (m_debug_is_on) {
        auto flag_string = GeneralUtilities::formatted_string("ZF: %d, NF: %d, HF: %d, CF: %d", flag_is_set(Flag::Z) ? 1 : 0, flag_is_set(Flag::N) ? 1 : 0,
                                                              flag_is_set(Flag::H) ? 1 : 0, flag_is_set(Flag::C) ? 1 : 0);
        log(LogLevel::Debug, GeneralUtilities::formatted_string("AF: 0x%x, BC: 0x%x, DE: 0x%x, HL: 0x%x, SP: "
                                                                "0x%x, PC: 0x%x [%s]",
                                                                m_reg_af, m_reg_bc, m_reg_de, m_reg_hl, m_reg_sp, m_reg_pc, flag_string));
    }
}

void CPU::set_debug_mode(bool on_or_off) {
    m_mmu->set_debug_mode(on_or_off);
    m_debug_is_on = on_or_off;
}

void CPU::show_disassembled_instruction(bool on_or_off) { m_show_disassembled_instruction = on_or_off; }

void CPU::enable_breakpoint_at(uint16_t pc) { m_current_breakpoint = pc; }

void CPU::clear_breakpoint() { m_current_breakpoint = 0; }

bool CPU::half_carry_occurs_on_subtract(uint8_t v, const uint8_t value_to_subtract) const { return ((v & 0x0F) - (value_to_subtract & 0x0F)) < 0; }

bool CPU::half_carry_occurs_on_add(uint8_t v, const uint8_t value_to_add) const { return ((v & 0x0F) + (value_to_add & 0x0F)) > 0x0F; }

void CPU::log(LogLevel level, const std::string &message) { LogUtilities::log(LoggerType::Cpu, level, message); }

CPU::~CPU() {}
}