#include "components/CPU.h"
#include "util/CommandLineArgument.h"
#include "util/DebuggerCommand.cpp"
#include "util/DebuggerCommand.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>
#include <memory>

void print_help() {
    std::cout << "gbcemu v 0.1\n";
    std::cout << "A GB/GBC/SGB emulator (at some point).\n\n";

    gbcemu::CommandLineArgument::print_usage_string(std::cout, "gbcemu");
    std::cout << "\n" << std::endl;
    gbcemu::CommandLineArgument::print_options(std::cout);
}

int main(int argc, char **argv) {

    gbcemu::LogUtilities::init();

    bool has_help;
    auto s = gbcemu::CommandLineArgument::get_debugger_cmd(argc, argv, gbcemu::CommandLineArgument::ArgumentType::Help, &has_help);
    if (has_help) {
        print_help();
        return 0;
    }

    bool has_boot_rom, has_cartridge;
    auto boot_rom_argument = gbcemu::CommandLineArgument::get_debugger_cmd(argc, argv, gbcemu::CommandLineArgument::ArgumentType::BootRomPath, &has_boot_rom);
    auto cartridge_argument =
        gbcemu::CommandLineArgument::get_debugger_cmd(argc, argv, gbcemu::CommandLineArgument::ArgumentType::CartridgePath, &has_cartridge);

    if (has_boot_rom) {
        boot_rom_argument->fix_path();
    } else {
        gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Error, "Running without boot ROM is currently not supported");
        exit(1);
    }

    if (has_cartridge) {
        cartridge_argument->fix_path();
    } else {
        gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Error, "Running without cartridge is currently not supported");
        exit(1);
    }

    auto mmu = std::make_shared<gbcemu::MMU>(0xFFFF);
    auto cpu = std::make_unique<gbcemu::CPU>(mmu);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info, "Emulator started!");

    if (!mmu->try_load_boot_rom(std::cout, boot_rom_argument->value))
        exit(1);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info, "Boot ROM loaded");

    if (!mmu->try_load_cartridge(std::cout, cartridge_argument->value))
        exit(1);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Cartridge '%s' loaded", cartridge_argument->value));

    delete boot_rom_argument;
    delete cartridge_argument;

    mmu->set_in_boot_mode(true);
    cpu->enable_breakpoint_at(0x68); // We'll hang on 0x68 due to VBLANK never occuring

    bool step_mode = true;
    std::string input, cmd;

    while (true) {

        if (step_mode) {
            std::cout << gbcemu::GeneralUtilities::formatted_string("[PC: 0x%04X]> ", cpu->get_16_bit_register(gbcemu::CPU::Register::PC));
            std::getline(std::cin, input);

            auto cmd = input.empty() ? gbcemu::DebuggerCommand::get_debugger_cmd("step") : gbcemu::DebuggerCommand::get_debugger_cmd(input);

            if (cmd->command == gbcemu::DebuggerCommand::Command::Help) {
                std::cout << "\navailable commands:\n\n";
                gbcemu::DebuggerCommand::print_commands(std::cout);
                std::cout << std::endl;
                continue;
            }

            auto cmd_data = cmd->get_command_data();
            if (cmd_data.empty() || cmd_data.compare("help") == 0) {
                cmd->print_command_help(std::cout);
                continue;
            }

            switch (cmd->command) {

            case gbcemu::DebuggerCommand::Command::Show:
                if (cmd_data.compare("cpu") == 0) {
                    cpu->print_state(std::cout);

                } else if (cmd_data.rfind("mem", 0) == 0) {
                    auto is_pair = cmd_data.find('-') != std::string::npos;
                    gbcemu::DebuggerCommand::address_pair address;
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
                        mmu->print_memory_at_location(std::cout, address.first, address.second);

                } else {
                    cmd->print_command_help(std::cout);
                }

                continue;

            case gbcemu::DebuggerCommand::Command::Disassemble:
                uint16_t number_of_instructions_to_print;
                if (cmd->try_get_numeric_argument(&number_of_instructions_to_print))
                    cpu->print_disassembled_instructions(std::cout, number_of_instructions_to_print);
                else
                    cmd->print_command_help(std::cout);
                continue;

            case gbcemu::DebuggerCommand::Command::SetBreakpoint:
                uint16_t breakpoint;
                if (cmd->try_get_numeric_argument(&breakpoint))
                    cpu->enable_breakpoint_at(breakpoint);
                else
                    cmd->print_command_help(std::cout);
                continue;

            case gbcemu::DebuggerCommand::Command::ClearBreakpoint:
                std::cout << "clear breakpoint" << std::endl;
                continue;
            case gbcemu::DebuggerCommand::Command::Step:
                cpu->tick();
                continue;
            case gbcemu::DebuggerCommand::Command::Run:
                step_mode = false;
                break;
            case gbcemu::DebuggerCommand::Command::None:
                // Do nothing
                continue;
            default:
                __builtin_unreachable();
            }
        }

        cpu->tick();

        if (cpu->breakpoint_hit()) {
            cpu->clear_breakpoint();
            step_mode = true;
        }
    }

    return 0;
}