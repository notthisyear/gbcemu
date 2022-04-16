#pragma once
#include "MMU.h"
#include <stdint.h>

namespace gbcemu {

class PPU {
  public:
    enum class Mode {
        HBlank,
        VBlank,
        OAMSearch,
        DataTransfer,
    };

    void tick(uint32_t number_of_cycles);

    PPU(std::shared_ptr<MMU>);

  private:
    const uint32_t ScanlinesPerFrame = 154;
    const uint32_t DotsPerScanline = 456;
    const uint32_t DotsInVBlank = 4560;
    const uint32_t DotsInOAMSearch = 80;
    const uint32_t VBlankStartScanline = 144;

    const uint16_t ScrollYRegisterOffset = 0x42;
    const uint16_t ScrollXRegisterOffset = 0x43;
    const uint16_t LCDYCoordinateRegisterOffset = 0x44;
    const uint16_t LCDYCompareRegisterOffset = 0x45;
    const uint16_t WindowYPositionRegister = 0x4A;
    const uint16_t WindowXPositionRegister = 0x4B;

    // PPU has 1 dot / T-state
    static uint32_t m_cycles_to_dots(uint32_t cpu_cycles) { return 4 * cpu_cycles; }
    void set_mode(const PPU::Mode);

    std::shared_ptr<MMU> m_mmu;
    uint32_t m_current_dot;
    uint8_t m_current_scanline;

    PPU::Mode m_mode;
};
}