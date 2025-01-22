#pragma once

#include "MMU.h"
#include "PixelFetcher.h"
#include <cstdint>
#include <memory>

namespace gbcemu {

class PPU final {
  public:
    enum class LCDControlRegisterBit : uint8_t {
        BGAndWindowEnableOrPriority = 0,
        OBJEnable = 1,
        OBJSize = 2,
        BGTileMapArea = 3,
        BGAndWindowTileDataArea = 4,
        WindowEnable = 5,
        WindowTileMapArea = 6,
        LCDAndPPUEnable = 7,
    };

    enum class LCDStatusRegisterBit : uint8_t {
        LYCEqualsLY = 2,
        ModeHBLankInterrupt = 3,
        ModeVBlankInterrupt = 4,
        ModeOAMInterrupt = 5,
        LYCEqualsLYInterrupt = 6,
    };

    void tick();

    PPU(std::shared_ptr<MMU> const, uint16_t const, uint16_t const, uint8_t const);

    void request_frame_trace();
    bool cycles_per_frame_reached() const;
    void acknowledge_frame();
    uint8_t *get_framebuffer() const;
    bool lcd_control_bit_is_set(const PPU::LCDControlRegisterBit);
    bool lcd_status_bit_is_set(const PPU::LCDStatusRegisterBit);

    ~PPU();

  private:
    static constexpr uint32_t kPixelsPerScanline{ 160 };
    static constexpr uint32_t kScanlinesPerFrame{ 154 };
    static constexpr uint32_t kVBlankStartScanline{ 144 };
    static constexpr uint32_t kDotsPerScanline{ 456 };
    static constexpr uint32_t kDotsPerFrame{ kDotsPerScanline * kScanlinesPerFrame };

    static constexpr uint32_t kDotsInVBlank{ 4560 };
    static constexpr uint32_t kDotsInOAMSearch{ 80 };

    enum class Mode : uint8_t {
        HBlank = 0x00,
        VBlank = 0x01,
        OAMSearch = 0x02,
        DataTransfer = 0x03,
    };

    void reset_ppu_state();
    void set_bit_in_ppu_register_to_value(const MMU::IORegister, uint8_t const, bool const);
    void write_current_mode_to_status_register();

    std::shared_ptr<MMU> m_mmu;
    uint8_t *m_framebuffer;
    std::unique_ptr<PixelFetcher> m_pixel_fetcher;
    PPU::Mode m_last_mode;

    uint16_t m_framebuffer_width{ 0U };
    uint16_t m_framebuffer_height{ 0U };
    uint16_t m_bytes_per_pixel{ 0U };

    uint32_t m_total_frame_dots{ 0U };
    uint16_t m_dots_on_current_line{ 0U };
    uint8_t m_current_scanline{ 0U };
    uint8_t m_last_scanline{ 0U };

    uint8_t m_pixels_pushed_on_current_line{ 0U };

    bool m_frame_done_flag{ false };
    bool m_screen_enabled{ false };
    bool m_trace_next_frame{ false };
    bool m_tracing_frame{ false };

    uint32_t m_framebuffer_idx{ 0U };
    PPU::Mode m_mode{ Mode::OAMSearch };
};
}