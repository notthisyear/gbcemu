#include "Debugger.h"
#include "DebuggerCommand.h"
#include "util/GeneralUtilities.h"
#include <iostream>
#include <memory>

namespace gbcemu {

Debugger::Debugger(std::shared_ptr<CPU> cpu, std::shared_ptr<MMU> mmu, std::shared_ptr<Application> app)
    : m_cpu(cpu), m_mmu(mmu), m_app(app), m_is_in_run_mode(false) {}

void Debugger::run(std::ostream &output_stream) {
    m_app->set_cpu_debug_mode(true);
    std::string input, cmd;

    while (true) {
        output_stream << (m_is_in_run_mode ? "[cpu running]>"
                                           : GeneralUtilities::formatted_string("[PC: 0x%04X]> ", m_cpu->get_16_bit_register(CPU::Register::PC)));
        std::getline(std::cin, input);

        if (m_is_in_run_mode && m_cpu->breakpoint_hit()) {
            m_is_in_run_mode = false;
            continue;
        }

        auto cmd = input.empty() ? DebuggerCommand::get_debugger_cmd("step") : DebuggerCommand::get_debugger_cmd(input);

        if (cmd->command == DebuggerCommand::Command::Help) {
            output_stream << "\navailable commands:\n\n";
            gbcemu::DebuggerCommand::print_commands(output_stream);
            output_stream << std::endl;
            continue;
        }

        auto cmd_data = cmd->get_command_data();
        if (cmd_data.empty() || cmd_data.compare("help") == 0) {
            cmd->print_command_help(output_stream);
            continue;
        }

        switch (cmd->command) {

        case DebuggerCommand::Command::Show:
            if (cmd_data.compare("cpu") == 0) {
                m_cpu->print_state(output_stream);

            } else if (cmd_data.rfind("mem", 0) == 0) {
                auto is_pair = cmd_data.find('-') != std::string::npos;
                DebuggerCommand::address_pair address;
                bool result;

                if (is_pair) {
                    result = cmd->try_get_address_pair_arg(&address);
                } else {
                    uint16_t addr_start;
                    result = cmd->try_get_numeric_argument(&addr_start);
                    if (result) {
                        address.first = addr_start;
                        address.second = addr_start;
                    }
                }

                if (result)
                    m_mmu->print_memory_at_location(output_stream, address.first, address.second);

            } else {
                cmd->print_command_help(output_stream);
            }
            break;

        case DebuggerCommand::Command::Disassemble:
            uint16_t number_of_instructions_to_print;
            if (cmd->try_get_numeric_argument(&number_of_instructions_to_print))
                m_cpu->print_disassembled_instructions(output_stream, number_of_instructions_to_print);
            else
                cmd->print_command_help(output_stream);
            break;

        case gbcemu::DebuggerCommand::Command::SetBreakpoint:
            uint16_t breakpoint;
            if (cmd->try_get_numeric_argument(&breakpoint))
                m_cpu->enable_breakpoint_at(breakpoint);
            else
                cmd->print_command_help(output_stream);
            break;

        case gbcemu::DebuggerCommand::Command::ClearBreakpoint:
            m_cpu->clear_breakpoint();
            break;

        case gbcemu::DebuggerCommand::Command::Step:
            if (!m_is_in_run_mode) {
                if (m_cpu->at_start_of_instruction()) {
                    do {
                        m_cpu->tick();
                    } while (!m_cpu->at_start_of_instruction());
                }
            }
            break;

        case gbcemu::DebuggerCommand::Command::Run:
            if (!m_is_in_run_mode) {
                m_is_in_run_mode = true;
                m_app->set_cpu_debug_mode(false);
            }
            break;

        case DebuggerCommand::Command::Break:
            if (m_is_in_run_mode) {
                m_app->set_cpu_debug_mode(true);
                m_is_in_run_mode = false;
            }
            break;

        case gbcemu::DebuggerCommand::Command::None:
            // Do nothing
            break;
        default:
            __builtin_unreachable();
        }
    }
}
}