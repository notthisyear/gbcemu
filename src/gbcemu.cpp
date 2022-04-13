#include "components/CPU.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iostream>

int main(int argc, char **argv) {

    gbcemu::LogUtilities::init();
    auto cpu = std::make_unique<gbcemu::CPU>();

    gbcemu::LogUtilities::log(gbcemu::LoggerType::Internal, gbcemu::LogLevel::Info,
                              gbcemu::GeneralUtilities::formatted_string("Emulator started! CPU is: %s", cpu->get_cpu_name()));

    cpu->enable_breakpoint_at(0x0c);
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