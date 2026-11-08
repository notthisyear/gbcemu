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
#include <memory>
#include <stdexcept>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace gbcemu {

CPU::CPU(std::shared_ptr<MMU> const mmu, std::shared_ptr<PPU> const ppu, bool const output_trace) : m_mmu{ mmu }, m_ppu{ ppu }, m_output_trace{ output_trace } {
    m_timer_controller = TimerController::get(m_mmu.get());
    m_state = CPU::State::FetchAndDecode;
    if (m_mmu->has_cartridge()) {
        set_initial_values_for_registers(m_mmu->get_boot_rom_type(),
                                         m_mmu->get_cartridge()->get_single_byte_header_field(Cartridge::HeaderField::HeaderChecksum) == 0x00);
    }

    m_is_running_boot_rom = m_mmu->get_boot_rom_type() != MMU::BootRomType::None;
    if (m_output_trace) {
        char full_path[CPU::kMaxPathLength]{};
#if defined(_WIN32)
        std::size_t const size{ GetModuleFileNameA(NULL, full_path, CPU::kMaxPathLength) };
#else
        // readlink does not null-terminate and truncates silently, so a full buffer is treated as too long
        ssize_t const result{ readlink("/proc/self/exe", full_path, CPU::kMaxPathLength - 1) };
        std::size_t const size{ (result < 0 || result >= CPU::kMaxPathLength - 1) ? static_cast<std::size_t>(CPU::kMaxPathLength)
                                                                                  : static_cast<std::size_t>(result) };
#endif
        if (size < CPU::kMaxPathLength) {
            std::string const s{ GeneralUtilities::formatted_string("%s/%s", GeneralUtilities::get_folder_name_from_full_path(full_path),
                                                                    CPU::kTraceFileName) };
            m_trace_stream = std::ofstream(s);
        } else {
            LogUtilities::log_error(std::cout,
                                    GeneralUtilities::formatted_string("Path to executable cannot be longer than %d characters", CPU::kMaxPathLength));
            std::exit(1);
        }
    }
}

void CPU::tick() {
    m_timer_controller->process();
    bool trace_preloaded_instruction_at_end{ false };

    if (m_current_cpu_phase_tick_count == 0) {
        m_mmu->process_dma();
        auto run_state_machine{ true };
        if (m_state == CPU::State::FetchAndDecode) {
            run_state_machine = false;

            bool const handle_interrupt{ should_handle_interrupt() };

            // A pending EI takes effect after the interrupt check at the boundary following EI,
            // so the instruction after EI is always executed before an interrupt is serviced
            if (m_interrupt_enable_pending) {
                m_interrupt_enable_pending = false;
                m_interrupt_enabled = true;
            }

            if (handle_interrupt) {
                m_state = CPU::State::InterruptTransition;
                if (m_next_instruction_preloaded) {
                    m_next_instruction_preloaded = false;
                    m_reg_pc--;

                    // The preloaded instruction is not executed now, so it must not be traced
                    m_pending_trace_line.reset();
                }
            } else {
                if (!m_is_halted) {
                    if (!m_next_instruction_preloaded) {
                        if (m_output_trace) {
                            m_trace_stream << get_trace_line(m_reg_pc) << '\n';
                        }
                        fetch_and_decode();
                    } else {
                        run_state_machine = true;

                        if (m_pending_trace_line.has_value()) {
                            m_trace_stream << *m_pending_trace_line << '\n';
                            m_pending_trace_line.reset();
                        }
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

                    // The next opcode is fetched in the last M-cycle of every instruction, HALT included. When
                    // HALT returns with IME=0, the prefetched instruction is executed right away. With IME=1, it's
                    // discarded by the interrupt dispatch, which returns to it afterwards.
                    if (m_output_trace) {
                        trace_preloaded_instruction_at_end = true;
                    }
                    fetch_and_decode();
                    m_next_instruction_preloaded = true;
                }
                break;

            // see https://gbdev.io/pandocs/Interrupts.html
            case CPU::State::InterruptTransition:
                // Second wait state. The first one is the M-cycle in which the interrupt was detected.
                m_state = CPU::State::InterruptPushPC;
                break;

            case CPU::State::InterruptPushPC:
                // Upper byte of PC is pushed in the first cycle and the lower byte in the second
                m_current_interrupt_phase_counter++;
                if (m_current_interrupt_phase_counter == 1) {
                    push_byte_to_stack(static_cast<std::uint8_t>(m_reg_pc >> 8));

                    // The interrupt to service is selected after the upper byte has been pushed. If the push
                    // overwrote IE so that no pending interrupt is enabled anymore, the dispatch is cancelled.
                    m_current_interrupt = select_interrupt_to_service();
                } else {
                    push_byte_to_stack(static_cast<std::uint8_t>(m_reg_pc & 0x00FF));
                    m_state = CPU::State::InterruptSetPC;
                    m_current_interrupt_phase_counter = 0;
                }
                break;

            case CPU::State::InterruptSetPC:
                // A cancelled dispatch jumps to 0x0000 instead of an interrupt vector
                set_register(CPU::Register::PC,
                             m_current_interrupt.has_value() ? s_interrupt_vector.find(*m_current_interrupt)->second : static_cast<std::uint16_t>(0x0000));
                m_state = CPU::State::FetchAndDecode;

                // Like the last M-cycle of an instruction, the last M-cycle of the dispatch also fetches the next opcode
                if (m_output_trace) {
                    trace_preloaded_instruction_at_end = true;
                }
                fetch_and_decode();
                m_next_instruction_preloaded = true;
                break;

            case CPU::State::FetchAndDecode:
                __builtin_unreachable();
            }
        }
    }
    m_ppu->tick();

    if (trace_preloaded_instruction_at_end) {
        // PC already points past the preloaded opcode
        m_pending_trace_line = get_trace_line(static_cast<std::uint16_t>(m_reg_pc - 1));
    }

    m_current_cpu_phase_tick_count = (m_current_cpu_phase_tick_count + 1) & (kExecutionTicksPerOperationStep - 1);
    m_tick_ctr++;
}

bool CPU::at_start_of_instruction() const { return m_current_cpu_phase_tick_count == 0 && m_state == CPU::State::FetchAndDecode; }

std::uint16_t CPU::current_instruction_address() const {
    // If the next instruction has already been fetched (overlapping the previous instruction), PC points past its opcode
    return (m_state == CPU::State::FetchAndDecode && m_next_instruction_preloaded) ? static_cast<std::uint16_t>(m_reg_pc - 1) : m_reg_pc;
}

bool CPU::should_handle_interrupt() {
    std::uint8_t const interrupt_enable{ m_mmu->get_io_register(MMU::IORegister::IE) };
    std::uint8_t const interrupt_flag{ m_mmu->get_io_register(MMU::IORegister::IF) };

    bool const IME_was_set{ m_interrupt_enabled };

    // An interrupt is pending if the corresponding bit is set in both the IE and IF registers
    bool const interrupt_pending{ (interrupt_enable & interrupt_flag) > 0x00 };

    // A pending interrupt can only be serviced if IME is set. Note that which interrupt to
    // service is not decided until during the dispatch, see select_interrupt_to_service().
    bool const pending_interrupt_can_be_serviced{ IME_was_set && interrupt_pending };
    if (pending_interrupt_can_be_serviced) {
        m_interrupt_enabled = false;
    }

    // A pending interrupt always causes HALT to return, regardless of IME. If IME is not set, execution
    // simply continues after HALT. (The halt bug only occurs when HALT is executed with an interrupt
    // already pending, see set_cpu_to_halt().)
    if (interrupt_pending && m_is_halted) {
        m_is_halted = false;
    }

    return pending_interrupt_can_be_serviced;
}

std::optional<CPU::InterruptSource> CPU::select_interrupt_to_service() {
    std::uint8_t const interrupt_enable{ m_mmu->get_io_register(MMU::IORegister::IE) };
    std::uint8_t interrupt_flag{ m_mmu->get_io_register(MMU::IORegister::IF) };
    std::uint8_t const pending_interrupts{ static_cast<std::uint8_t>(interrupt_enable & interrupt_flag) };

    // Let's find the pending interrupt with the highest priority (lowest bit)
    std::size_t const last_interrupt_idx{ static_cast<std::size_t>(CPU::InterruptSource::Joypad) };
    for (std::size_t i{ 0 }; i <= last_interrupt_idx; ++i) {
        if (BitUtilities::bit_is_set(pending_interrupts, i)) {
            BitUtilities::reset_bit_in_byte(interrupt_flag, i);
            m_mmu->set_io_register(MMU::IORegister::IF, interrupt_flag);
            return static_cast<CPU::InterruptSource>(i);
        }
    }

    // No pending interrupt left, the dispatch is cancelled
    return std::nullopt;
}

void CPU::fetch_and_decode() {
    std::uint8_t const current_byte{ read_at_pc() };
    if (m_halt_bug_active) {
        m_reg_pc--;
        m_halt_bug_active = false;
    }

    if (m_state == State::FetchAndDecode) {
        m_is_extended_opcode = OpcodeBuilder::is_extended_opcode(current_byte);
        if (!m_is_extended_opcode) {
            m_current_opcode = OpcodeBuilder::decode_opcode(current_byte, false);
        }
    } else if (m_state == State::FetchAndDecodeExtended) {
        m_current_opcode = OpcodeBuilder::decode_opcode(current_byte, true);
    }
}

std::uint8_t CPU::read_at_pc() {
    std::uint8_t byte;
    (void)m_mmu->try_read_from_memory(&byte, m_reg_pc++, 1);
    return byte;
}

void CPU::read_at_pc_and_store_in_intermediate(const CPU::Register reg) {
    if (reg != CPU::Register::W && reg != CPU::Register::Z)
        throw std::invalid_argument("Method can only be called with either 'W' or 'Z' register");
    set_register(reg, read_at_pc());
}

void CPU::push_byte_to_stack(std::uint8_t const value) {
    std::uint8_t data{ value };
    (void)m_mmu->try_map_data_to_memory(&data, --m_reg_sp, 1);
}

std::uint8_t CPU::pop_byte_from_stack() {
    std::uint8_t byte;
    (void)m_mmu->try_read_from_memory(&byte, m_reg_sp++, 1);
    return byte;
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

std::string CPU::disassemble_instruction_at(std::uint16_t current_pc, std::uint8_t &instruction_length) const {
    std::uint8_t current_instruction;
    std::uint16_t const pc_at_start{ current_pc };
    m_mmu->try_read_from_memory(&current_instruction, current_pc, 1);
    current_pc++;

    auto is_extended{ OpcodeBuilder::is_extended_opcode(current_instruction) };
    if (is_extended) {
        m_mmu->try_read_from_memory(&current_instruction, current_pc, 1);
        current_pc++;
    }

    std::shared_ptr<Opcode> opcode{ OpcodeBuilder::decode_opcode(current_instruction, is_extended) };
    std::string disassembled_instruction;
    if (opcode->size > 1) {
        std::uint8_t *instruction_data{ new std::uint8_t[opcode->size - 1] };
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

void CPU::print_disassembled_instructions(std::ostream &stream, std::uint16_t const number_of_instructions) {
    std::uint16_t current_pc{ current_instruction_address() };

    std::uint8_t last_instruction_length{ 0U };
    for (std::size_t i{ 0U }; i < number_of_instructions; ++i) {
        stream << "\033[1;37m" << std::left << std::setw(10) << std::setfill(' ') << GeneralUtilities::formatted_string("0x%04X", current_pc);
        stream << "\033[0;m";
        stream << disassemble_instruction_at(current_pc, last_instruction_length) << std::endl;
        current_pc += last_instruction_length;
    }
}

void CPU::enable_interrupts_delayed() { m_interrupt_enable_pending = true; }

void CPU::enable_interrupts_immediately() { m_interrupt_enabled = true; }

void CPU::disable_interrupts() {
    m_interrupt_enabled = false;
    m_interrupt_enable_pending = false;
}

bool CPU::interrupt_enabled() const { return m_interrupt_enabled; }

void CPU::enable_breakpoint_at(std::uint16_t const pc) {
    m_current_breakpoint = pc;
    m_has_breakpoint = true;
}

void CPU::set_cpu_to_halt() {
    bool const interrupt_pending{ (m_mmu->get_io_register(MMU::IORegister::IE) & m_mmu->get_io_register(MMU::IORegister::IF)) > 0x00 };

    // If IME is not set and an interrupt is already pending, HALT doesn't halt the CPU. Instead, the halt bug
    // causes the byte after HALT to be read twice, as PC isn't incremented when the next opcode is fetched.
    if (!m_interrupt_enabled && interrupt_pending) {
        m_halt_bug_active = true;
        return;
    }

    m_is_halted = true;
}

bool CPU::breakpoint_hit() const { return m_has_breakpoint && at_start_of_instruction() && current_instruction_address() == m_current_breakpoint; }

void CPU::clear_breakpoint() { m_has_breakpoint = false; }

bool CPU::half_carry_occurs_on_subtract(std::uint8_t const v, std::uint8_t const value_to_subtract) const {
    return ((v & 0x0F) - (value_to_subtract & 0x0F)) & 0x10;
}

bool CPU::half_carry_occurs_on_subtract_with_carry(std::uint8_t const v, std::uint8_t const value_to_subtract) const {
    return ((v & 0x0F) - (value_to_subtract & 0x0F) - (flag_is_set(CPU::Flag::C) ? 1 : 0)) & 0x10;
}

bool CPU::half_carry_occurs_on_add(std::uint8_t const v, std::uint8_t const value_to_add, bool const include_carry) const {
    return ((v & 0x0F) + (value_to_add & 0x0F) + (flag_is_set(CPU::Flag::C) & include_carry)) > 0x0F;
}

bool CPU::half_carry_occurs_on_add(std::uint16_t const v, std::uint16_t const value_to_add, bool const include_carry) const {
    return ((v & 0x0FFF) + (value_to_add & 0x0FFF) + (flag_is_set(CPU::Flag::C) & include_carry)) > 0x0FFF;
}

bool CPU::carry_occurs_on_add(std::uint8_t const v, std::uint8_t const value_to_add, bool const include_carry) const {
    return ((std::uint16_t)v + (std::uint16_t)(value_to_add) + (flag_is_set(CPU::Flag::C) & include_carry)) > 0xFF;
}

bool CPU::carry_occurs_on_add(std::uint16_t const v, std::uint16_t const value_to_add, bool const include_carry) const {
    return ((std::uint32_t)v + (std::uint32_t)value_to_add + (flag_is_set(CPU::Flag::C) & include_carry)) > 0xFFFF;
}

bool CPU::carry_occurs_on_subtract(std::uint16_t const v, std::uint16_t const value_to_subtract) const { return value_to_subtract > v; };

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

void CPU::print_flag_value(std::ostream &stream, std::string const &flag_name, bool const value, bool const insert_newline) const {
    stream << "\033[0;33m" << flag_name << ": "
           << "\033[1;37m" << LogUtilities::to_tf(value) << "\033[0m";
    if (insert_newline)
        stream << std::endl;
}

void CPU::print_reg(std::ostream &stream, const CPU::Register reg, bool const insert_newline) const {
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
    auto breakpoint_string{ m_has_breakpoint ? GeneralUtilities::formatted_string("0x%04x", m_current_breakpoint) : "none" };

    stream << "\033[0;32mbreakpoint: "
           << "\033[1;37m" << breakpoint_string << "\033[0;32m\tcpu state: "
           << "\033[1;37m" << s_cpu_state_name.find(m_state)->second << "\n\033[0;32mrunning boot rom: "
           << "\033[1;37m" << (m_is_running_boot_rom ? "true" : "false") << "\033[0;m" << std::endl;
}

std::string CPU::get_trace_line(std::uint16_t const pc) const {
    std::uint8_t instruction_length{ 0U };
    return GeneralUtilities::formatted_string(
               "A: 0x%02X F: %c%c%c%c BC: 0x%04X DE: 0x%04X HL: 0x%04X SP: 0x%04X PC: 0x%04X (cy: %llu, DIV: 0x%02X TIMA: 0x%02X TMA: 0x%02X TAC: 0x%02X) | ",
               get_8_bit_register(CPU::Register::A), flag_is_set(CPU::Flag::Z) ? 'Z' : '-', flag_is_set(CPU::Flag::N) ? 'N' : '-',
               flag_is_set(CPU::Flag::H) ? 'H' : '-', flag_is_set(CPU::Flag::C) ? 'C' : '-', get_16_bit_register(CPU::Register::BC),
               get_16_bit_register(CPU::Register::DE), get_16_bit_register(CPU::Register::HL), get_16_bit_register(CPU::Register::SP), pc,
               static_cast<unsigned long long>(m_tick_ctr), m_mmu->get_io_register(MMU::IORegister::DIV), m_mmu->get_io_register(MMU::IORegister::TIMA),
               m_mmu->get_io_register(MMU::IORegister::TMA), m_mmu->get_io_register(MMU::IORegister::TAC)) +
           disassemble_instruction_at(pc, instruction_length);
}

CPU::~CPU() {
    if (m_output_trace)
        m_trace_stream.close();
}

}