#pragma once

#include "MMU.h"
#include "PPU.h"
#include "components/TimerController.h"
#include "util/BitUtilities.h"
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

namespace gbcemu {

struct Opcode;

class CPU final {
  public:
    enum class Register : uint8_t {
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

    enum class Flag : uint8_t {
        Z, // Zero flag
        C, // Add/sub flag (used to convert to BCD)
        N, // Half carry flag (used to convert to BCD)
        H, // Carry flag
    };

    enum class InterruptSource : uint8_t {
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

    CPU(std::shared_ptr<MMU> const, std::shared_ptr<PPU> const, bool const output_trace = false);

    void tick();

    void print_disassembled_instructions(std::ostream &, uint16_t const);

    void clear_breakpoint();

    void enable_breakpoint_at(uint16_t const);

    void set_interrupt_enable(bool const);

    bool interrupt_enabled() const;

    uint8_t read_at_pc();

    void read_at_pc_and_store_in_intermediate(const CPU::Register);

    void load_register_into_intermediate(const CPU::Register);

    uint8_t get_8_bit_register(const CPU::Register reg) const {
        switch (reg) {
        case CPU::Register::B:
            return get_register_upper(m_reg_bc);
        case CPU::Register::C:
            return get_register_lower(m_reg_bc);
        case CPU::Register::D:
            return get_register_upper(m_reg_de);
        case CPU::Register::E:
            return get_register_lower(m_reg_de);
        case CPU::Register::H:
            return get_register_upper(m_reg_hl);
        case CPU::Register::L:
            return get_register_lower(m_reg_hl);
        case CPU::Register::A:
            return get_register_upper(m_reg_af);
        case CPU::Register::W:
            return get_register_upper(m_reg_wz);
        case CPU::Register::Z:
            return get_register_lower(m_reg_wz);
        default:
            __builtin_unreachable();
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
            __builtin_unreachable();
        }
    }

    void set_register(const CPU::Register reg, uint8_t const value) {
        switch (reg) {
        case CPU::Register::B:
            set_register_upper(m_reg_bc, value);
            break;
        case CPU::Register::C:
            set_register_lower(m_reg_bc, value);
            break;
        case CPU::Register::D:
            set_register_upper(m_reg_de, value);
            break;
        case CPU::Register::E:
            set_register_lower(m_reg_de, value);
            break;
        case CPU::Register::H:
            set_register_upper(m_reg_hl, value);
            break;
        case CPU::Register::L:
            set_register_lower(m_reg_hl, value);
            break;
        case CPU::Register::A:
            set_register_upper(m_reg_af, value);
            break;
        case CPU::Register::W:
            set_register_upper(m_reg_wz, value);
            break;
        case CPU::Register::Z:
            set_register_lower(m_reg_wz, value);
            break;
        default:
            __builtin_unreachable();
        }
    }

    void set_register(const CPU::Register reg, uint16_t const value) {
        switch (reg) {
        case CPU::Register::AF:
            set_register(m_reg_af, value & 0xFFF0); // The lower four bits should never be set
            break;
        case CPU::Register::BC:
            set_register(m_reg_bc, value);
            break;
        case CPU::Register::DE:
            set_register(m_reg_de, value);
            break;
        case CPU::Register::HL:
            set_register(m_reg_hl, value);
            break;
        case CPU::Register::SP:
            set_register(m_reg_sp, value);
            break;
        case CPU::Register::PC:
            set_register(m_reg_pc, value);
            break;
        case CPU::Register::WZ:
            set_register(m_reg_wz, value);
            break;
        default:
            __builtin_unreachable();
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

    bool flag_is_set(const CPU::Flag flag) const {
        switch (flag) {
        case CPU::Flag::Z:
            return BitUtilities::bit_is_set(m_reg_af, 7);
        case CPU::Flag::N:
            return BitUtilities::bit_is_set(m_reg_af, 6);
        case CPU::Flag::H:
            return BitUtilities::bit_is_set(m_reg_af, 5);
        case CPU::Flag::C:
            return BitUtilities::bit_is_set(m_reg_af, 4);
        default:
            __builtin_unreachable();
        }
    }

    void set_flag(const CPU::Flag flag, bool const value) {
        uint8_t bit_to_set;
        switch (flag) {
        case CPU::Flag::Z:
            bit_to_set = 7;
            break;
        case CPU::Flag::N:
            bit_to_set = 6;
            break;
        case CPU::Flag::H:
            bit_to_set = 5;
            break;
        case CPU::Flag::C:
            bit_to_set = 4;
            break;
        default:
            __builtin_unreachable();
        }

        if (value)
            BitUtilities::set_bit_in_word(m_reg_af, bit_to_set);
        else
            BitUtilities::reset_bit_in_word(m_reg_af, bit_to_set);
    }

    bool breakpoint_hit() const;

    bool half_carry_occurs_on_add(uint8_t const v, uint8_t const value_to_add, bool const include_carry = false) const;

    bool half_carry_occurs_on_add(uint16_t const v, uint16_t const value_to_add, bool const include_carry = false) const;

    bool half_carry_occurs_on_subtract(uint8_t const v, uint8_t const value_to_subtract) const;

    bool half_carry_occurs_on_subtract_with_carry(uint8_t const v, uint8_t const value_to_subtract) const;

    bool carry_occurs_on_add(uint8_t const v, uint8_t const value_to_add, bool const include_carry = false) const;

    bool carry_occurs_on_add(uint16_t const v, uint16_t const value_to_add, bool const include_carry = false) const;

    bool carry_occurs_on_subtract(uint16_t const v, uint16_t const value_to_subtract) const;

    void print_state(std::ostream &) const;

    void set_cpu_to_halt();

    bool at_start_of_instruction() const;

    ~CPU();

  private:
    void fetch_and_decode();

    bool should_handle_interrupt();

    enum class State : uint8_t {
        FetchAndDecode,
        FetchAndDecodeExtended,
        Execute,
        InterruptTransition,
        InterruptPushPC,
        InterruptSetPC,
    };

    static constexpr std::string kTraceFileName{ "trace.log" };
    static constexpr int kMaxPathLength{ 255 };
    static constexpr uint8_t kExecutionTicksPerOperationStep{ 4 };

    std::shared_ptr<MMU> m_mmu;
    std::shared_ptr<PPU> m_ppu;
    TimerController *m_timer_controller;

    bool m_output_trace;
    bool m_is_running_boot_rom;
    CPU::State m_state;

    uint64_t m_tick_ctr{ 0U };
    uint8_t m_current_cpu_phase_tick_count{ 0U };
    uint8_t m_current_interrupt_phase_counter{ 0U };
    int8_t m_cycles_until_interrupts_enabled{ 0 };

    bool m_next_instruction_preloaded{ false };
    bool m_is_extended_opcode{ false };
    bool m_is_halted{ false };
    bool m_halt_bug_active{ false };
    bool m_has_breakpoint{ false };
    bool m_interrupt_enabled{ false };

    std::shared_ptr<Opcode> m_current_opcode{ nullptr };
    std::ofstream m_trace_stream{};

    uint16_t m_current_breakpoint{ 0U };

    CPU::InterruptSource m_current_interrupt{};

    uint16_t m_reg_af{ 0U }; // Accumulator and flags
    uint16_t m_reg_bc{ 0U }; // BC (can be accessed as two 8-bit registers)
    uint16_t m_reg_de{ 0U }; // DE (can be accessed as two 8-bit registers)
    uint16_t m_reg_hl{ 0U }; // HL (can be accessed as two 8-bit registers)
    uint16_t m_reg_sp{ 0U }; // Stack pointer
    uint16_t m_reg_pc{ 0U }; // Program counter
    uint16_t m_reg_wz{ 0U }; // Internal temporary register

    std::string disassemble_instruction_at(uint16_t, uint8_t &) const;

    void set_register(uint16_t &reg, uint16_t const value) { reg = value; }

    void set_register_lower(uint16_t &reg, uint8_t const value) { reg = (reg & 0xFF00) | value; }

    void set_register_upper(uint16_t &reg, uint8_t const value) { reg = (reg & 0x00FF) | (value << 8); }

    uint8_t get_register_lower(uint16_t const &reg) const { return reg & 0x00FF; }

    uint8_t get_register_upper(uint16_t const &reg) const { return reg >> 8; }

    void set_initial_values_for_registers(const MMU::BootRomType, bool);

    void print_flag_value(std::ostream &, std::string const &, bool const, bool const) const;

    void print_reg(std::ostream &, const CPU::Register, bool const) const;

    void print_sp_and_pc(std::ostream &stream) const;

    void print_additional_info(std::ostream &stream) const;

    void print_trace_line();

    static inline std::unordered_map<CPU::State, std::string> s_cpu_state_name = {
        { CPU::State::FetchAndDecode, "FetchAndDecode" },
        { CPU::State::FetchAndDecodeExtended, "FetchAndDecodeExtended" },
        { CPU::State::Execute, "Execute" },
        { CPU::State::InterruptPushPC, "InterruptPushPC" },
        { CPU::State::InterruptSetPC, "InterruptSetPC" },
        { CPU::State::InterruptTransition, "InterruptTransition" },
    };

    static inline std::unordered_map<CPU::InterruptSource, uint16_t> const s_interrupt_vector{
        { CPU::InterruptSource::VBlank, 0x40 }, { CPU::InterruptSource::LCDStat, 0x48 }, { CPU::InterruptSource::Timer, 0x50 },
        { CPU::InterruptSource::Serial, 0x58 }, { CPU::InterruptSource::Joypad, 0x60 },
    };
};
}