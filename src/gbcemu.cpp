#include "Application.h"
#include "common/WindowProperties.h"
#include "components/CPU.h"
#include "debugger/Debugger.h"
#include "debugger/DebuggerCommand.cpp"
#include "debugger/DebuggerCommand.h"
#include "opengl/Renderer.h"
#include "util/CommandLineArgument.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>
#include <memory>
#include <thread>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
static const bool is_windows = true;
#else
static const bool is_windows = false;
#endif

void print_help() {
    std::cout << "gbcemu v 0.1\n";
    std::cout << "A GB/GBC/SGB emulator (at some point).\n\n";

    gbcemu::CommandLineArgument::print_usage_string(std::cout, "gbcemu");
    std::cout << "\n" << std::endl;
    gbcemu::CommandLineArgument::print_options(std::cout);
}

int main(int argc, char **argv) {

    bool has_help;
    auto s = gbcemu::CommandLineArgument::get_argument(argc, argv, gbcemu::CommandLineArgument::ArgumentType::Help, &has_help);
    if (has_help) {
        print_help();
        return 0;
    }

    if (!is_windows) {
        std::cout << "No other platform than Windows is currently supported. How did you even compile?";
        return 1;
    }

    bool has_boot_rom, has_cartridge, attach_debugger, output_trace;
    auto boot_rom_argument = gbcemu::CommandLineArgument::get_argument(argc, argv, gbcemu::CommandLineArgument::ArgumentType::BootRomPath, &has_boot_rom);
    auto cartridge_argument = gbcemu::CommandLineArgument::get_argument(argc, argv, gbcemu::CommandLineArgument::ArgumentType::CartridgePath, &has_cartridge);

    (void)gbcemu::CommandLineArgument::get_argument(argc, argv, gbcemu::CommandLineArgument::ArgumentType::AttachDebugger, &attach_debugger);
    (void)gbcemu::CommandLineArgument::get_argument(argc, argv, gbcemu::CommandLineArgument::ArgumentType::OutputTrace, &output_trace);

    if (has_boot_rom)
        boot_rom_argument->fix_path();

    if (has_cartridge) {
        cartridge_argument->fix_path();
    } else {
        gbcemu::LogUtilities::log_error(std::cout, "Running without cartridge is currently not supported");
        exit(1);
    }

    auto window_properties = gbcemu::WindowProperties();

    auto renderer = std::make_shared<gbcemu::Renderer>(window_properties.width, window_properties.height);
    auto mmu = std::make_shared<gbcemu::MMU>(0xFFFF);
    auto ppu = std::make_shared<gbcemu::PPU>(mmu, gbcemu::WindowProperties().width, gbcemu::WindowProperties().height, renderer->BytesPerPixel);

    gbcemu::LogUtilities::log_info(std::cout, "Emulator started!");

    if (has_boot_rom && !mmu->try_load_boot_rom(std::cout, boot_rom_argument->value))
        exit(1);

    if (has_boot_rom)
        gbcemu::LogUtilities::log_info(std::cout, "Boot ROM loaded");

    if (!mmu->try_load_cartridge(std::cout, cartridge_argument->value))
        exit(1);

    gbcemu::LogUtilities::log_info(std::cout, gbcemu::GeneralUtilities::formatted_string("Cartridge '%s' loaded", cartridge_argument->value));

    delete boot_rom_argument;
    delete cartridge_argument;

    auto cpu = std::make_shared<gbcemu::CPU>(mmu, ppu, output_trace);
    auto app = std::make_shared<gbcemu::Application>(cpu, ppu, renderer, gbcemu::WindowProperties());

    auto dbg = attach_debugger ? new gbcemu::Debugger(cpu, mmu, ppu, app) : nullptr;
    auto dbg_thread = attach_debugger ? new std::thread(&gbcemu::Debugger::run, dbg, std::ref(std::cout)) : nullptr;

    app->init();
    renderer->init();
    app->run();

    if (attach_debugger) {
        dbg_thread->join();
        delete dbg_thread;
        delete dbg;
    }
}