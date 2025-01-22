#include "Application.h"
#include "common/WindowProperties.h"
#include "components/CPU.h"
#include "debugger/Debugger.h"
#include "debugger/DebuggerCommand.cpp"
#include "opengl/Renderer.h"
#include "util/CommandLineParser.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>
#include <memory>
#include <thread>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
static bool const is_windows = true;
#else
static bool const is_windows = false;
#endif

void print_help(gbcemu::CommandLineParser const &parser) {
    std::cout << "gbcemu v 0.1\n";
    std::cout << "A GB/GBC/SGB emulator (at some point).\n\n";

    parser.print_usage_string(std::cout, "gbcemu");
    std::cout << "\n" << std::endl;
    parser.print_options(std::cout);
}

std::string windowsify_path(std::string const &path) {
    std::size_t const path_length{ path.length() };
    bool const remove_quotes{ (path[0] == '"') && (path[path_length - 1] == '"') };
    int64_t const number_of_backslashes{ std::count(path.cbegin(), path.cend(), '\\') };

    std::size_t const new_length{ path_length + number_of_backslashes + (remove_quotes ? -2 : 0) };
    char *const new_path_string = new char[new_length + 1];

    std::size_t i{ 0U };
    int32_t j{ 0 };

    while (true) {
        if (i == 0 && remove_quotes) {
            j--;
        } else if ((i == path_length - 1) && remove_quotes) {
            break;
        } else if (i == path_length) {
            break;
        } else {
            new_path_string[j] = path[i];
            if (number_of_backslashes > 0) {
                if (path[i] == '\\')
                    new_path_string[++j] = '\\';
            }
        }

        i++;
        j++;
    }

    new_path_string[j] = '\0';
    std::string const result{ std::string(new_path_string) };
    delete[] new_path_string;
    return result;
}

int main(int argc, char **argv) {

    gbcemu::CommandLineParser parser{};
    if (!parser.try_parse(argc, argv)) {
        gbcemu::LogUtilities::log_error(
            std::cout, gbcemu::GeneralUtilities::formatted_string("Invalid arguments! Could not parse '%s' as either a valid argument or valid argument value",
                                                                  argv[parser.parsing_error_argument_index()]));
        print_help(parser);
        exit(1);
    }

    if (argc == 1 || parser.has_argument(gbcemu::CommandData::ArgumentType::kHelp)) {
        print_help(parser);
        return 0;
    }

    if (!is_windows) {
        std::cout << "No other platform than Windows is currently supported. How did you even compile?";
        return 1;
    }

    if (!parser.has_argument(gbcemu::CommandData::ArgumentType::kCartridgePath)) {
        gbcemu::LogUtilities::log_error(std::cout, "Running without cartridge is currently not supported");
        exit(1);
    }

    std::string const cartridge_path{ windowsify_path(parser.get_argument_value(gbcemu::CommandData::ArgumentType::kCartridgePath)) };

    gbcemu::WindowProperties const window_properties{};

    std::shared_ptr<gbcemu::Renderer> const renderer{ std::make_shared<gbcemu::Renderer>(window_properties.width, window_properties.height) };
    std::shared_ptr<gbcemu::MMU> const mmu{ std::make_shared<gbcemu::MMU>(0xFFFF) };
    std::shared_ptr<gbcemu::PPU> const ppu{ std::make_shared<gbcemu::PPU>(mmu, gbcemu::WindowProperties().width, gbcemu::WindowProperties().height,
                                                                          gbcemu::Renderer::kBytesPerPixel) };

    gbcemu::LogUtilities::log_info(std::cout, "Emulator started!");

    if (parser.has_argument(gbcemu::CommandData::ArgumentType::kBootRomPath)) {
        std::string const boot_rom_path{ windowsify_path(parser.get_argument_value(gbcemu::CommandData::ArgumentType::kBootRomPath)) };
        if (!mmu->try_load_boot_rom(std::cout, boot_rom_path)) {
            exit(1);
        } else {
            gbcemu::LogUtilities::log_info(std::cout, "Boot ROM loaded");
        }
    }

    if (!mmu->try_load_cartridge(std::cout, cartridge_path)) {
        exit(1);
    }

    gbcemu::LogUtilities::log_info(std::cout, gbcemu::GeneralUtilities::formatted_string("Cartridge '%s' loaded", cartridge_path));

    std::shared_ptr<gbcemu::CPU> const cpu{ std::make_shared<gbcemu::CPU>(mmu, ppu, parser.has_argument(gbcemu::CommandData::ArgumentType::kOutputTrace)) };
    std::shared_ptr<gbcemu::Application> const app{ std::make_shared<gbcemu::Application>(cpu, ppu, renderer, gbcemu::WindowProperties()) };

    bool const attach_debugger{ parser.has_argument(gbcemu::CommandData::ArgumentType::kAttachDebugger) };
    gbcemu::Debugger *const dbg = { attach_debugger ? new gbcemu::Debugger(cpu, mmu, ppu, app) : nullptr };
    std::thread *const dbg_thread{ attach_debugger ? new std::thread(&gbcemu::Debugger::run, dbg, std::ref(std::cout)) : nullptr };

    app->init();
    renderer->init();
    app->run();

    if (attach_debugger) {
        dbg_thread->join();
        delete dbg_thread;
        delete dbg;
    }
}