#pragma once

#include "MMU.h"
#include "PPU.h"
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>

namespace gbcemu {

class Opcode;

class CPU {
  public:
    enum class Register {
        B,
        C,
        D,
        E,
        H,
        L,
        A,
        W, // Intermediate register
        Z, // Intermediate register
        AF,
        BC,
        DE,
        HL,
        SP,
        PC,
        WZ, // Intermediate register pair
    };

    enum class Flag {
        Z, // Zero flag
        C, // Add/sub flag (used to convert to BCD)
        N, // Half carry flag (used to convert to BCD)
        H, // Carry flag
    };

    enum class InterruptSource {
        VBlank = 0x00,
        LCDStat = 0x01,
        Timer = 0x02,
        Serial = 0x03,
        Joypad = 0x04,
    };

    static inline CPU::Register register_map[] = { CPU::Register::B, CPU::Register::C, CPU::Register::D,  CPU::Register::E,
                                                   CPU::Register::H, CPU::Register::L, CPU::Register::HL, CPU::Register::A };

    static inline CPU::Register wide_register_map[] = {
        CPU::Register::BC,
        CPU::Register::DE,
        CPU::Register::HL,
        CPU::Register::SP,
    };
    static inline std::unordered_map<CPU::Register, std::string> register_name = {
        { CPU::Register::B, "B" },   { CPU::Register::C, "C" },   { CPU::Register::D, "D" },   { CPU::Register::E, "E" },   { CPU::Register::H, "H" },
        { CPU::Register::L, "L" },   { CPU::Register::A, "A" },   { CPU::Register::AF, "AF" }, { CPU::Register::BC, "BC" }, { CPU::Register::DE, "DE" },
        { CPU::Register::HL, "HL" }, { CPU::Register::SP, "SP" }, { CPU::Register::PC, "PC" },
    };

    CPU(std::shared_ptr<MMU>, std::shared_ptr<PPU>);

    void tick();

    void print_disassembled_instructions(std::ostream &, uint16_t);

    void clear_breakpoint();

    void enable_breakpoint_at(uint16_t);

    void set_interrupt_enable(bool);

    bool interrupt_enabled() const;

    uint8_t read_at_pc();

    void read_at_pc_and_store_in_intermediate(const CPU::Register);

    void load_register_into_intermediate(const CPU::Register);

    uint8_t get_8_bit_register(const CPU::Register reg) const {
        switch (reg) {
        case CPU::Register::B:
            return get_register_upper(&m_reg_bc);
        case CPU::Register::C:
            return get_register_lower(&m_reg_bc);
        case CPU::Register::D:
            return get_register_upper(&m_reg_de);
        case CPU::Register::E:
            return get_register_lower(&m_reg_de);
        case CPU::Register::H:
            return get_register_upper(&m_reg_hl);
        case CPU::Register::L:
            return get_register_lower(&m_reg_hl);
        case CPU::Register::A:
            return get_register_upper(&m_reg_af);
        case CPU::Register::W:
            return get_register_upper(&m_reg_wz);
        case CPU::Register::Z:
            return get_register_lower(&m_reg_wz);
        default:
            exit(1);
        }
    }

    uint16_t get_16_bit_register(const CPU::Register reg) const {
        switch (reg) {
        case CPU::Register::AF:
            return m_reg_af;
        case CPU::Register::BC:
            return m_reg_bc;
        case CPU::Register::DE:
            return m_reg_de;
        case CPU::Register::HL:
            return m_reg_hl;
        case CPU::Register::PC:
            return m_reg_pc;
        case CPU::Register::SP:
            return m_reg_sp;
        case CPU::Register::WZ:
            return m_reg_wz;
        default:
            exit(1);
        }
    }

    void set_register(const CPU::Register reg, const uint8_t value) {
        switch (reg) {
        case CPU::Register::B:
            set_register_upper(&m_reg_bc, value);
            break;
        case CPU::Register::C:
            set_register_lower(&m_reg_bc, value);
            break;
        case CPU::Register::D:
            set_register_upper(&m_reg_de, value);
            break;
        case CPU::Register::E:
            set_register_lower(&m_reg_de, value);
            break;
        case CPU::Register::H:
            set_register_upper(&m_reg_hl, value);
            break;
        case CPU::Register::L:
            set_register_lower(&m_reg_hl, value);
            break;
        case CPU::Register::A:
            set_register_upper(&m_reg_af, value);
            break;
        case CPU::Register::W:
            set_register_upper(&m_reg_wz, value);
            break;
        case CPU::Register::Z:
            set_register_lower(&m_reg_wz, value);
            break;
        default:
            exit(1);
        }
    }

    void set_register(const CPU::Register reg, const uint16_t value) {
        switch (reg) {
        case CPU::Register::AF:
            set_register(&m_reg_af, value);
            break;
        case CPU::Register::BC:
            set_register(&m_reg_bc, value);
            break;
        case CPU::Register::DE:
            set_register(&m_reg_de, value);
            break;
        case CPU::Register::HL:
            set_register(&m_reg_hl, value);
            break;
        case CPU::Register::SP:
            set_register(&m_reg_sp, value);
            break;
        case CPU::Register::PC:
            set_register(&m_reg_pc, value);
            break;
        case CPU::Register::WZ:
            set_register(&m_reg_wz, value);
            break;
        default:
            exit(1);
        }
    }

    void set_register_from_intermediate(const CPU::Register target) {
        auto is_16_bit = target == CPU::Register::PC || target == CPU::Register::SP || target == CPU::Register::HL || target == CPU::Register::BC ||
                         target == CPU::Register::DE;

        if (is_16_bit)
            set_register(target, get_16_bit_register(CPU::Register::WZ));
        else
            set_register(target, get_8_bit_register(CPU::Register::Z));
    }

    bool flag_is_set(CPU::Flag flag) const {
        switch (flag) {
        case CPU::Flag::Z:
            return ((m_reg_af >> 7) & 0x01) == 1;
        case CPU::Flag::N:
            return ((m_reg_af >> 6) & 0x01) == 1;
        case CPU::Flag::H:
            return ((m_reg_af >> 5) & 0x01) == 1;
        case CPU::Flag::C:
            return ((m_reg_af >> 4) & 0x01) == 1;
        default:
            __builtin_unreachable();
        }
    }

    void set_flag(CPU::Flag flag, const bool value) {
        switch (flag) {
        case CPU::Flag::Z:
            m_reg_af = value ? (m_reg_af | 0x0080) : (m_reg_af & 0xFF7F);
            break;
        case CPU::Flag::N:
            m_reg_af = value ? (m_reg_af | 0x0040) : (m_reg_af & 0xFFBF);
            break;
        case CPU::Flag::H:
            m_reg_af = value ? (m_reg_af | 0x0020) : (m_reg_af & 0xFFDF);
            break;
        case CPU::Flag::C:
            m_reg_af = value ? (m_reg_af | 0x0010) : (m_reg_af & 0xFFEF);
            break;
        default:
            __builtin_unreachable();
        }
    }

    bool breakpoint_hit() const;

    void interleave_execute_and_decode(const bool);

    bool at_start_of_instruction() const;

    bool half_carry_occurs_on_add(uint8_t v, const uint8_t value_to_add) const;

    bool half_carry_occurs_on_add(uint16_t v, const uint16_t value_to_add) const;

    bool half_carry_occurs_on_subtract(uint8_t v, const uint8_t value_to_subtract) const;

    bool half_carry_occurs_on_subtract_with_carry(uint8_t v, const uint8_t value_to_subtract) const;

    bool carry_occurs_on_add(uint8_t v, const uint8_t value_to_add) const;

    bool carry_occurs_on_add(uint16_t v, const uint16_t value_to_add) const;

    bool carry_occurs_on_subtract(uint16_t v, const uint16_t value_to_subtract) const;

    void print_state(std::ostream &) const;

    ~CPU();

  private:
    enum class State { Idle, Wait, Execute, InterruptTransition, InterruptPushPC, InterruptSetPC };
    std::shared_ptr<MMU> m_mmu;
    std::shared_ptr<PPU> m_ppu;

    const uint8_t ExecutionTicksPerOperationStep = 4;
    bool m_is_extended_opcode;
    bool m_interleave_execute_and_decode;
    bool m_at_start_of_instruction;
    uint8_t m_current_instruction_cycle_count;
    uint32_t m_current_cycle_count;
    std::shared_ptr<Opcode> m_current_opcode;

    CPU::State m_state;

    uint16_t m_current_breakpoint;
    bool m_has_breakpoint;

    bool m_interrupt_enabled;
    CPU::InterruptSource m_current_interrupt;
    bool m_is_running_boot_rom;
    uint16_t m_reg_af; // Accumulator and flags
    uint16_t m_reg_bc; // BC (can be accessed as two 8-bit registers)
    uint16_t m_reg_de; // DE (can be accessed as two 8-bit registers)
    uint16_t m_reg_hl; // HL (can be accessed as two 8-bit registers)
    uint16_t m_reg_sp; // Stack pointer
    uint16_t m_reg_pc; // Program counter
    uint16_t m_reg_wz; // Internal temporary register

    std::string disassemble_next_instruction();

    void set_register(uint16_t *reg, const uint16_t value) { (*reg) = value; }

    void set_register_lower(uint16_t *reg, const uint8_t value) { (*reg) = ((*reg) & 0xFF00) | value; }

    void set_register_upper(uint16_t *reg, const uint8_t value) { (*reg) = ((*reg) & 0x00FF) | (value << 8); }

    uint8_t get_register_lower(const uint16_t *reg) const { return (*reg) & 0x00FF; }

    uint8_t get_register_upper(const uint16_t *reg) const { return (*reg) >> 8; }

    void set_initial_values_for_registers(const MMU::BootRomType, bool);

    void print_flag_value(std::ostream &, const std::string &, const bool, const bool) const;

    void print_reg(std::ostream &, const CPU::Register, const bool) const;

    void print_sp_and_pc(std::ostream &stream) const;

    void print_additional_info(std::ostream &stream) const;

    static const std::unordered_map<CPU::InterruptSource, uint16_t> s_interrupt_vector;
};
}