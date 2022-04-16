#include "components/CPU.h"
#include "util/CommandLineArgument.h"
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

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Emulator started! CPU is: %s", cpu->get_cpu_name()));

    if (!mmu->try_load_boot_rom(boot_rom_argument->value))
        exit(1);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info, "Boot ROM loaded");

    if (!mmu->try_load_cartridge(cartridge_argument->value))
        exit(1);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Cartridge '%s' loaded", cartridge_argument->value));

    delete boot_rom_argument;
    delete cartridge_argument;

    mmu->set_in_boot_mode(true);

    cpu->enable_breakpoint_at(0x68); // We'll hang on 0x68 due to VBLANK never occuring
    cpu->set_debug_mode(false);

    bool step_mode = false;
    std::string input, cmd;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        auto cmd = input.empty() ? gbcemu::DebuggerCommand::Command::Step : gbcemu::DebuggerCommand::get_debugger_cmd(input);

        switch (cmd) {
        case gbcemu::DebuggerCommand::Command::Help:
            std::cout << "help" << std::endl;
            break;
        case gbcemu::DebuggerCommand::Command::Show:
            std::cout << "show" << std::endl;
            break;
        case gbcemu::DebuggerCommand::Command::SetBreakpoint:
            std::cout << "set breakpoint" << std::endl;
            break;
        case gbcemu::DebuggerCommand::Command::ClearBreakpoint:
            std::cout << "clear breakpoint" << std::endl;
            break;
        case gbcemu::DebuggerCommand::Command::Step:
            std::cout << "step" << std::endl;
            break;
        case gbcemu::DebuggerCommand::Command::Run:
            std::cout << "run" << std::endl;
            break;
        case gbcemu::DebuggerCommand::Command::Break:
            std::cout << "break" << std::endl;
            break;
        case gbcemu::DebuggerCommand::Command::None:
            std::cout << "none" << std::endl;
            break;
        default:
            __builtin_unreachable();
        }
        if (cpu->breakpoint_hit()) {
            cpu->clear_breakpoint();
            cpu->set_debug_mode(true);
            cpu->show_disassembled_instruction(true);
            step_mode = true;
        }
        cpu->tick();

        if (step_mode)
            std::cin.get();
    }
    return 0;
}