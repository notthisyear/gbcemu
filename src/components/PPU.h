#pragma once
#include "MMU.h"
#include "PixelFetcher.h"
#include <memory>
#include <stdint.h>

namespace gbcemu {

class PPU {
  public:
    const static uint16_t DisplayWidth = 160;
    const static uint16_t DisplayHeight = 144;
    const static uint16_t BytesPerPixel = 4; // RGBA

    enum class Mode {
        HBlank,
        VBlank,
        OAMSearch,
        DataTransfer,
    };

    void tick();

    PPU(std::shared_ptr<MMU>);

    ~PPU();

  private:
    const uint32_t PixelsPerScanline = 160;
    const uint32_t ScanlinesPerFrame = 154;
    const uint32_t VBlankStartScanline = 144;
    const uint32_t DotsPerScanline = 456;

    const uint32_t DotsInVBlank = 4560;
    const uint32_t DotsInOAMSearch = 80;

    const uint16_t ScrollYRegisterOffset = 0x42;
    const uint16_t ScrollXRegisterOffset = 0x43;
    const uint16_t LCDYCoordinateRegisterOffset = 0x44;
    const uint16_t LCDYCompareRegisterOffset = 0x45;
    const uint16_t WindowYPositionRegister = 0x4A;
    const uint16_t WindowXPositionRegister = 0x4B;

    void set_mode(const PPU::Mode);

    std::shared_ptr<MMU> m_mmu;
    std::unique_ptr<PixelFetcher> m_pixel_fetcher;
    uint32_t m_current_dot_on_line;
    uint8_t m_current_scanline;
    uint8_t m_last_scanline;

    uint8_t *m_framebuffer;
    PPU::Mode m_mode;
};
}