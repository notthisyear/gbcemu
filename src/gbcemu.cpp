#include "components/CPU.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>
#include <memory>

const std::string BootRomPath = "C:\\programming\\gbcemu\\resources\\dmg_rom.bin";
const std::string CartridgePath = "C:\\programming\\gbcemu\\resources\\sml.gb";

int main(int argc, char **argv) {

    gbcemu::LogUtilities::init();
    auto mmu = std::make_shared<gbcemu::MMU>(0xFFFF);
    auto cpu = std::make_unique<gbcemu::CPU>(mmu);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Emulator started! CPU is: %s", cpu->get_cpu_name()));

    if (!mmu->try_load_boot_rom(BootRomPath))
        exit(1);

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info, "Boot ROM loaded");

    if (!mmu->try_load_cartridge(CartridgePath))
        exit(1);
    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Cartridge '%s' loaded", CartridgePath));

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