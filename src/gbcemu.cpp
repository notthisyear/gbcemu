#include "Application.h"
#include "common/WindowProperties.h"
#include "components/CPU.h"
#include "debugger/Debugger.h"
#include "debugger/DebuggerCommand.cpp"
#include "debugger/DebuggerCommand.h"
#include "opengl/Renderer.h"
#include "util/CommandLineArgument.h"
#include "util/CommandLineParser.h"
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

void print_help(gbcemu::CommandLineParser &parser) {
    std::cout << "gbcemu v 0.1\n";
    std::cout << "A GB/GBC/SGB emulator (at some point).\n\n";

    parser.print_usage_string(std::cout, "gbcemu");
    std::cout << "\n" << std::endl;
    parser.print_options(std::cout);
}

void fix_path(std::string &value) {
    bool remove_quotes = value[0] == '"' && value[value.length() - 1] == '"';
    auto number_of_backslashes = std::count(value.begin(), value.end(), '\\');

    auto new_length = value.length() + number_of_backslashes + (remove_quotes ? -2 : 0);
    auto new_path_string = new char[new_length + 1];

    auto i = 0;
    auto j = 0;

    while (true) {
        if (i == 0 && remove_quotes) {
            j--;
        } else if (i == value.length() - 1 && remove_quotes) {
            break;
        } else if (i == value.length()) {
            break;
        } else {
            new_path_string[j] = value[i];
            if (number_of_backslashes > 0) {
                if (value[i] == '\\')
                    new_path_string[++j] = '\\';
            }
        }

        i++;
        j++;
    }

    new_path_string[j] = '\0';
    auto s = std::string(new_path_string);
    delete[] new_path_string;
    value = s;
}

int main(int argc, char **argv) {

    gbcemu::CommandLineParser parser;
    parser.parse(argc, argv);
    if (parser.has_argument(gbcemu::CommandLineParser::ArgumentType::Help)) {
        print_help(parser);
        return 0;
    }

    if (!is_windows) {
        std::cout << "No other platform than Windows is currently supported. How did you even compile?";
        return 1;
    }

    if (!parser.has_argument(gbcemu::CommandLineParser::ArgumentType::CartridgePath)) {
        gbcemu::LogUtilities::log_error(std::cout, "Running without cartridge is currently not supported");
        exit(1);
    }

    auto cartridge_path = parser.get_argument(gbcemu::CommandLineParser::ArgumentType::CartridgePath)->value;
    fix_path(cartridge_path);

    auto window_properties = gbcemu::WindowProperties();

    auto renderer = std::make_shared<gbcemu::Renderer>(window_properties.width, window_properties.height);
    auto mmu = std::make_shared<gbcemu::MMU>(0xFFFF);
    auto ppu = std::make_shared<gbcemu::PPU>(mmu, gbcemu::WindowProperties().width, gbcemu::WindowProperties().height, renderer->BytesPerPixel);

    gbcemu::LogUtilities::log_info(std::cout, "Emulator started!");

    if (parser.has_argument(gbcemu::CommandLineParser::ArgumentType::BootRomPath)) {
        auto boot_rom_path = parser.get_argument(gbcemu::CommandLineParser::ArgumentType::BootRomPath)->value;
        fix_path(boot_rom_path);
        if (!mmu->try_load_boot_rom(std::cout, boot_rom_path))
            exit(1);
        else
            gbcemu::LogUtilities::log_info(std::cout, "Boot ROM loaded");
    }

    if (!mmu->try_load_cartridge(std::cout, cartridge_path))
        exit(1);

    gbcemu::LogUtilities::log_info(std::cout, gbcemu::GeneralUtilities::formatted_string("Cartridge '%s' loaded", cartridge_path));

    auto cpu = std::make_shared<gbcemu::CPU>(mmu, ppu, parser.has_argument(gbcemu::CommandLineParser::ArgumentType::OutputTrace));
    auto app = std::make_shared<gbcemu::Application>(cpu, ppu, renderer, gbcemu::WindowProperties());

    auto attach_debugger = parser.has_argument(gbcemu::CommandLineParser::ArgumentType::AttachDebugger);
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