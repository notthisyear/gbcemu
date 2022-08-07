#pragma once

#include "MMU.h"
#include "PixelFetcher.h"
#include <memory>
#include <stdint.h>

namespace gbcemu {

class PPU {
  public:
    enum class LCDControlRegisterBit {
        LCDAndPPUEnable = 7,
        WindowTileMapArea = 6,
        WindowEnable = 5,
        BGAndWindowTileDataArea = 4,
        BGTileMapArea = 3,
        OBJSize = 2,
        OBJEnable = 1,
        BGAndWindowEnableOrPriority = 0
    };

    enum class LCDStatusRegisterBit {
        LYCEqualsLYInterrupt = 6,
        ModeOAMInterrupt = 5,
        ModeVBlankInterrupt = 4,
        ModeHBLankInterrupt = 3,
        LYCEqualsLY = 2,
    };

    void tick();

    PPU(std::shared_ptr<MMU>, uint16_t, uint16_t, uint8_t);

    void request_frame_trace();
    bool cycles_per_frame_reached() const;
    void acknowledge_frame();
    uint8_t *get_framebuffer() const;
    bool get_lcd_control_bit(const PPU::LCDControlRegisterBit);
    bool get_lcd_status_bit(const PPU::LCDStatusRegisterBit);

    ~PPU();

  private:
    const uint32_t PixelsPerScanline = 160;
    const uint32_t ScanlinesPerFrame = 154;
    const uint32_t VBlankStartScanline = 144;
    const uint32_t DotsPerScanline = 456;
    const uint32_t DotsPerFrame = DotsPerScanline * ScanlinesPerFrame;

    const uint32_t DotsInVBlank = 4560;
    const uint32_t DotsInOAMSearch = 80;

    enum class Mode {
        HBlank = 0x00,
        VBlank = 0x01,
        OAMSearch = 0x02,
        DataTransfer = 0x03,
    };

    void reset_ppu_state();
    void set_lcd_control_bit(const PPU::LCDControlRegisterBit, const bool);
    void set_lcd_status_bit(const PPU::LCDStatusRegisterBit, const bool);

    void set_bit_in_ppu_register(const MMU::IORegister, const uint8_t, const bool);
    bool get_bit_in_ppu_register(const MMU::IORegister, const uint8_t);

    void write_current_mode_to_status_register();

    std::shared_ptr<MMU> m_mmu;

    uint16_t m_framebuffer_width;
    uint16_t m_framebuffer_height;
    uint16_t m_bytes_per_pixel;

    std::unique_ptr<PixelFetcher> m_pixel_fetcher;
    uint32_t m_total_frame_dots;
    uint16_t m_dots_on_current_line;
    uint8_t m_current_scanline;
    uint8_t m_last_scanline;

    uint8_t m_pixels_pushed_on_current_line;

    bool m_frame_done_flag;
    bool m_screen_enabled;
    bool m_trace_next_frame;
    bool m_tracing_frame;

    uint8_t *m_framebuffer;
    uint32_t m_framebuffer_idx;
    PPU::Mode m_mode;
    PPU::Mode m_last_mode;
};
}