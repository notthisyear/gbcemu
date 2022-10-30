#include "CPU.h"
#include "OpcodeBuilder.h"
#include "Opcodes.h"
#include "components/MMU.h"
#include "components/TimerController.h"
#include "util/BitUtilities.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <stdexcept>
// Windows-specific (TODO: define some is_windows constant in a shared options header)
#include <windows.h>

namespace gbcemu {

CPU::CPU(std::shared_ptr<MMU> mmu, std::shared_ptr<PPU> ppu, bool output_trace) : m_mmu(mmu), m_ppu(ppu), m_output_trace(output_trace) {
    m_current_cpu_phase_tick_count = 0;
    m_current_interrupt_phase_counter = 0;

    m_next_instruction_preloaded = false;
    m_is_extended_opcode = false;
    m_interrupt_to_be_serviced = false;
    m_current_opcode = nullptr;
    m_cycles_until_interrupts_enabled = -1;

    m_interrupt_enabled = false;
    m_has_breakpoint = false;
    m_is_halted = false;
    m_halt_bug_active = false;

    m_state = CPU::State::FetchAndDecode;
    if (m_mmu->has_cartridge()) {
        set_initial_values_for_registers(m_mmu->get_boot_rom_type(),
                                         m_mmu->get_cartridge()->get_single_byte_header_field(Cartridge::HeaderField::HeaderChecksum) == 0x00);
    }

    m_is_running_boot_rom = m_mmu->get_boot_rom_type() != MMU::BootRomType::None;
    if (m_output_trace) {
        auto full_path = new char[CPU::MaxPathLength];
        auto size = GetModuleFileNameA(NULL, full_path, CPU::MaxPathLength);
        if (size < CPU::MaxPathLength) {
            auto s = GeneralUtilities::formatted_string("%s\\%s", GeneralUtilities::get_folder_name_from_full_path(full_path), CPU::TraceFileName);
            m_trace_stream = std::ofstream(s);
        } else {
            LogUtilities::log_error(std::cout,
                                    GeneralUtilities::formatted_string("Path to executable cannot be longer than %d characters", CPU::MaxPathLength));
            std::exit(1);
        }
    }
}

void CPU::tick() {
    if (m_cycles_until_interrupts_enabled > 0) {
        m_cycles_until_interrupts_enabled--;

        if (m_cycles_until_interrupts_enabled == 0) {
            m_interrupt_enabled = true;
            m_cycles_until_interrupts_enabled = -1;
        }
    }

    bool move_pc_back_and_trace_at_end = false;
    if (m_current_cpu_phase_tick_count == 0) {
        auto run_state_machine = true;
        if (m_state == CPU::State::FetchAndDecode) {
            run_state_machine = false;
            m_interrupt_to_be_serviced = check_for_interrupts();

            if (m_interrupt_to_be_serviced) {
                m_state = CPU::State::InterruptTransition;
                if (m_next_instruction_preloaded) {
                    m_next_instruction_preloaded = false;
                    m_reg_pc--;
                }
            } else {
                if (!m_is_halted) {
                    if (!m_next_instruction_preloaded) {
                        if (m_output_trace)
                            print_trace_line();
                        fetch_and_decode();
                    } else {
                        run_state_machine = true;
                    }
                    m_state = m_is_extended_opcode ? CPU::State::FetchAndDecodeExtended : CPU::State::Execute;
                }
            }
        }

        if (run_state_machine) {

            switch (m_state) {
            case CPU::State::FetchAndDecodeExtended:
                fetch_and_decode();
                m_state = CPU::State::Execute;
                break;

            case CPU::State::Execute:
                m_current_opcode->tick_execution(this, m_mmu.get());
                if (m_current_opcode->is_done()) {
                    m_state = CPU::State::FetchAndDecode;

                    if (!m_is_halted) {
                        if (m_output_trace)
                            move_pc_back_and_trace_at_end = true;

                        fetch_and_decode();
                        m_next_instruction_preloaded = true;
                    } else {
                        { m_next_instruction_preloaded = false; }
                    }
                }
                break;

            // see https://gbdev.io/pandocs/Interrupts.html
            case CPU::State::InterruptTransition:
                m_current_interrupt_phase_counter++;
                if (m_current_interrupt_phase_counter == 2) {
                    m_state = CPU::State::InterruptPushPC;
                    m_current_interrupt_phase_counter = 0;
                }
                break;

            case CPU::State::InterruptPushPC:
                m_current_interrupt_phase_counter++;
                if (m_current_interrupt_phase_counter == 2) {
                    uint16_t sp = get_16_bit_register(CPU::Register::SP);
                    (void)m_mmu->try_map_data_to_memory((uint8_t *)&m_reg_pc, sp - 2, 2);
                    set_register(CPU::Register::SP, static_cast<uint16_t>(sp - 2));
                    m_state = CPU::State::InterruptSetPC;
                    m_current_interrupt_phase_counter = 0;
                }
                break;

            case CPU::State::InterruptSetPC:
                set_register(CPU::Register::PC, s_interrupt_vector.find(m_current_interrupt)->second);
                m_state = CPU::State::FetchAndDecode;
                break;

            case CPU::State::FetchAndDecode:
                __builtin_unreachable();
            }
        }
    }

    m_mmu->tick_timer_controller();
    m_ppu->tick();

    if (move_pc_back_and_trace_at_end) {
        m_reg_pc--;
        print_trace_line();
        m_reg_pc++;
    }

    m_current_cpu_phase_tick_count = (m_current_cpu_phase_tick_count + 1) & (ExecutionTicksPerOperationStep - 1);
}

bool CPU::at_start_of_instruction() const { return m_current_cpu_phase_tick_count == 0 && m_state == CPU::State::FetchAndDecode; }

bool CPU::check_for_interrupts() {
    auto interrupt_enable = m_mmu->get_io_register(MMU::IORegister::IE);
    auto interrupt_flag = m_mmu->get_io_register(MMU::IORegister::IF);
    bool interrupt_pending = (interrupt_enable & interrupt_flag) > 0x00;
    auto IME_was_set = m_interrupt_enabled;

    if (interrupt_pending) {
        if (m_interrupt_enabled) {
            m_interrupt_enabled = false;
            auto last_interrupt = static_cast<int>(CPU::InterruptSource::Joypad);
            for (int i = 0; i < last_interrupt - 1; i++) {
                if (BitUtilities::bit_is_set(interrupt_flag, i)) {
                    BitUtilities::reset_bit_in_byte(interrupt_flag, i);
                    m_mmu->set_io_register(MMU::IORegister::IF, interrupt_flag);
                    m_current_interrupt = static_cast<CPU::InterruptSource>(i);
                    break;
                }
            }
        }

        // A pending interrupt always causes HALT to return
        // However, if IME is not set, the "halt bug" can
        // potentially be triggered.
        if (m_is_halted) {
            m_is_halted = false;
            // halt bug is active if interrupt is pending and IME was not set
            m_halt_bug_active = !IME_was_set;
        }
    }

    return IME_was_set && interrupt_pending;
}

void CPU::fetch_and_decode() {
    auto current_byte = read_at_pc();
    if (m_halt_bug_active) {
        m_reg_pc--;
        m_halt_bug_active = false;
    }

    if (m_state == State::FetchAndDecode) {
        m_is_extended_opcode = is_extended_opcode(current_byte);
        if (!m_is_extended_opcode)
            m_current_opcode = decode_opcode(current_byte, false);
    } else if (m_state == State::FetchAndDecodeExtended) {
        m_current_opcode = decode_opcode(current_byte, true);
    }
}

uint8_t CPU::read_at_pc() {
    uint8_t byte;
    (void)m_mmu->try_read_from_memory(&byte, m_reg_pc++, 1);
    return byte;
}

void CPU::read_at_pc_and_store_in_intermediate(CPU::Register reg) {
    if (reg != CPU::Register::W && reg != CPU::Register::Z)
        throw std::invalid_argument("Method can only be called with either 'W' or 'Z' register");
    set_register(reg, read_at_pc());
}

void CPU::load_register_into_intermediate(const CPU::Register reg) {
    switch (reg) {
    case CPU::Register::A:
    case CPU::Register::B:
    case CPU::Register::C:
    case CPU::Register::D:
    case CPU::Register::E:
    case CPU::Register::H:
    case CPU::Register::L:
        set_register(CPU::Register::Z, get_8_bit_register(reg));
        break;
    case CPU::Register::AF:
    case CPU::Register::BC:
    case CPU::Register::DE:
    case CPU::Register::HL:
    case CPU::Register::SP:
    case CPU::Register::PC:
        set_register(CPU::Register::WZ, get_16_bit_register(reg));
        break;
    case CPU::Register::W:
    case CPU::Register::Z:
    case CPU::Register::WZ:
        throw std::invalid_argument("Cannot load intermediate into intermediate");
    default:
        __builtin_unreachable();
    }
}

std::string CPU::disassemble_instruction_at(uint16_t current_pc, uint8_t &instruction_length) const {
    uint8_t current_instruction;
    auto pc_at_start = current_pc;
    m_mmu->try_read_from_memory(&current_instruction, current_pc, 1);
    current_pc++;

    auto is_extended = is_extended_opcode(current_instruction);
    if (is_extended) {
        m_mmu->try_read_from_memory(&current_instruction, current_pc, 1);
        current_pc++;
    }

    std::shared_ptr<Opcode> opcode = decode_opcode(current_instruction, is_extended);
    std::string disassembled_instruction;
    if (opcode->size > 1) {
        uint8_t *instruction_data = new uint8_t[opcode->size - 1];
        m_mmu->try_read_from_memory(instruction_data, current_pc, opcode->size - 1);
        disassembled_instruction = opcode->get_disassembled_instruction(instruction_data);
        current_pc += opcode->size - 1;
        delete[] instruction_data;
    } else {
        disassembled_instruction = opcode->get_disassembled_instruction(nullptr);
    }

    instruction_length = current_pc - pc_at_start;
    return disassembled_instruction;
}

void CPU::print_disassembled_instructions(std::ostream &stream, uint16_t number_of_instructions) {
    auto current_pc = m_reg_pc;
    if (m_state == CPU::State::FetchAndDecode && m_next_instruction_preloaded)
        current_pc--;

    uint8_t last_instruction_length = 0;
    for (auto i = 0; i < number_of_instructions; i++) {
        stream << "\033[1;37m" << std::left << std::setw(10) << std::setfill(' ') << GeneralUtilities::formatted_string("0x%04X", m_reg_pc);
        stream << "\033[0;m";
        stream << disassemble_instruction_at(current_pc, last_instruction_length) << std::endl;
        current_pc += last_instruction_length;
    }
}

void CPU::set_interrupt_enable(bool on_or_off) {
    if (on_or_off)
        m_cycles_until_interrupts_enabled = ExecutionTicksPerOperationStep + 1;
    else
        m_interrupt_enabled = false;
}

bool CPU::interrupt_enabled() const { return m_interrupt_enabled; }

void CPU::enable_breakpoint_at(uint16_t pc) {
    m_current_breakpoint = pc;
    m_has_breakpoint = true;
}

void CPU::set_cpu_to_halt() { m_is_halted = true; }

bool CPU::breakpoint_hit() const { return false; }

void CPU::clear_breakpoint() { m_has_breakpoint = false; }

bool CPU::half_carry_occurs_on_subtract(uint8_t v, const uint8_t value_to_subtract) const { return ((v & 0x0F) - (value_to_subtract & 0x0F)) & 0x10; }

bool CPU::half_carry_occurs_on_subtract_with_carry(uint8_t v, const uint8_t value_to_subtract) const {
    return ((v & 0x0F) - (value_to_subtract & 0x0F) - (flag_is_set(CPU::Flag::C) ? 1 : 0)) & 0x10;
}

bool CPU::half_carry_occurs_on_add(uint8_t v, const uint8_t value_to_add, const bool include_carry) const {
    return ((v & 0x0F) + (value_to_add & 0x0F) + (flag_is_set(CPU::Flag::C) & include_carry)) > 0x0F;
}

bool CPU::half_carry_occurs_on_add(uint16_t v, const uint16_t value_to_add, const bool include_carry) const {
    return ((v & 0x0FFF) + (value_to_add & 0x0FFF) + (flag_is_set(CPU::Flag::C) & include_carry)) > 0x0FFF;
}

bool CPU::carry_occurs_on_add(uint8_t v, const uint8_t value_to_add, const bool include_carry) const {
    return ((uint16_t)v + (uint16_t)(value_to_add) + (flag_is_set(CPU::Flag::C) & include_carry)) > 0xFF;
}

bool CPU::carry_occurs_on_add(uint16_t v, const uint16_t value_to_add, const bool include_carry) const {
    return ((uint32_t)v + (uint32_t)value_to_add + (flag_is_set(CPU::Flag::C) & include_carry)) > 0xFFFF;
}

bool CPU::carry_occurs_on_subtract(uint16_t v, const uint16_t value_to_subtract) const { return value_to_subtract > v; };

void CPU::set_initial_values_for_registers(const MMU::BootRomType boot_rom_type, bool header_checksum_is_zero) {
    m_reg_bc = boot_rom_type == MMU::BootRomType::DMG ? 0x0000 : 0x0013;
    m_reg_de = boot_rom_type == MMU::BootRomType::DMG ? 0x0000 : 0x00D8;
    m_reg_hl = boot_rom_type == MMU::BootRomType::DMG ? 0x0000 : 0x014D;
    m_reg_sp = boot_rom_type == MMU::BootRomType::DMG ? 0x0000 : 0xFFFE;
    m_reg_pc = boot_rom_type == MMU::BootRomType::DMG ? 0x0000 : 0x0100;
    m_reg_af = boot_rom_type == MMU::BootRomType::DMG ? 0x0000 : header_checksum_is_zero ? 0x0180 : 0x01B0;
    m_reg_wz = 0x0000;
}

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
    auto current_pc = get_16_bit_register(CPU::Register::PC);
    if (m_state == CPU::State::FetchAndDecode && m_next_instruction_preloaded)
        current_pc--;

    stream << "\033[0;36m" << register_name.find(CPU::Register::SP)->second << ": "
           << "\033[1;37m" << GeneralUtilities::formatted_string("0x%04x", get_16_bit_register(CPU::Register::SP)) << "\033[0;36m\t"
           << register_name.find(CPU::Register::PC)->second << ": "
           << "\033[1;37m" << GeneralUtilities::formatted_string("0x%04x", m_state) << "\033[0m" << std::endl;
}

void CPU::print_additional_info(std::ostream &stream) const {
    auto breakpoint_string = m_has_breakpoint ? GeneralUtilities::formatted_string("0x%04x", m_current_breakpoint) : "none";

    stream << "\033[0;32mbreakpoint: "
           << "\033[1;37m" << breakpoint_string << "\033[0;32m\tcpu state: "
           << "\033[1;37m" << s_cpu_state_name.find(m_state)->second << "\n\033[0;32mrunning boot rom: "
           << "\033[1;37m" << (m_is_running_boot_rom ? "true" : "false") << "\033[0;m" << std::endl;
}

void CPU::print_trace_line() {
    uint8_t instruction_length = 0;
    m_trace_stream << GeneralUtilities::formatted_string(
                          "A: 0x%02X F: %c%c%c%c BC: 0x%04X DE: 0x%04X HL: 0x%04X SP: 0x%04X PC: 0x%04X | ", get_8_bit_register(CPU::Register::A),
                          flag_is_set(CPU::Flag::Z) ? 'Z' : '-', flag_is_set(CPU::Flag::N) ? 'N' : '-', flag_is_set(CPU::Flag::H) ? 'H' : '-',
                          flag_is_set(CPU::Flag::C) ? 'C' : '-', get_16_bit_register(CPU::Register::BC), get_16_bit_register(CPU::Register::DE),
                          get_16_bit_register(CPU::Register::HL), get_16_bit_register(CPU::Register::SP), get_16_bit_register(CPU::Register::PC))
                   << disassemble_instruction_at(m_reg_pc, instruction_length) << std::endl;
}

CPU::~CPU() {
    if (m_output_trace)
        m_trace_stream.close();
}

const std::unordered_map<CPU::InterruptSource, uint16_t> CPU::s_interrupt_vector = {
    { CPU::InterruptSource::VBlank, 0x40 }, { CPU::InterruptSource::LCDStat, 0x48 }, { CPU::InterruptSource::Timer, 0x50 },
    { CPU::InterruptSource::Serial, 0x58 }, { CPU::InterruptSource::Joypad, 0x60 },
};
}