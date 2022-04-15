#include "components/CPU.h"
#include "util/CommandLineParsing.cpp"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <memory>

std::string get_option_from_arguments(int argc, char **argv, std::string option_to_get, bool *option_found) {

    std::string opt;
    auto raw_opt = gbcemu::get_option(argv, argv + argc, option_to_get);
    *option_found = raw_opt != nullptr;
    if (*option_found)
        opt = std::string(raw_opt);
    return opt;
}

int main(int argc, char **argv) {

    gbcemu::LogUtilities::init();

    bool has_boot_rom, has_catridge;
    auto boot_rom_path = get_option_from_arguments(argc, argv, "--boot-rom", &has_boot_rom);
    auto cartridge_path = get_option_from_arguments(argc, argv, "--cartridge", &has_catridge);

    if (has_boot_rom)
        boot_rom_path = gbcemu::fix_path(boot_rom_path);

    if (has_catridge)
        cartridge_path = gbcemu::fix_path(cartridge_path);

    auto mmu = std::make_shared<gbcemu::MMU>(0xFFFF);
    auto cpu = std::make_unique<gbcemu::CPU>(mmu);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Emulator started! CPU is: %s", cpu->get_cpu_name()));

    if (!mmu->try_load_boot_rom(boot_rom_path))
        exit(1);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info, "Boot ROM loaded");

    if (!mmu->try_load_cartridge(cartridge_path))
        exit(1);
    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Cartridge '%s' loaded", cartridge_path));

    mmu->set_in_boot_mode(true);

    cpu->enable_breakpoint_at(0x68); // We'll hang on 0x68 due to VBLANK never occuring
    cpu->set_debug_mode(false);

    bool step_mode = false;
    while (true) {
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