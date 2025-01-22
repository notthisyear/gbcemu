#pragma once

#include "MMU.h"
#include <cstdint>
#include <memory>
#include <queue>

namespace gbcemu {

// Forward declare PPU
class PPU;

struct Pixel final {
    uint8_t color_index;
};

class PixelFetcher final {
  public:
    PixelFetcher(std::shared_ptr<MMU> const, PPU *const);
    void start_fetcher(uint8_t const, bool const, bool const);
    void tick();

    bool can_pop_pixel() const;
    Pixel pop_pixel();

  private:
    enum class Mode {
        ReadTileId,
        ReadTileData0,
        ReadTileData1,
        Idle,
    };

    static constexpr uint8_t kPixelFifoFetchThreshold{ 8 };
    static constexpr uint16_t kVRAMTileMap1Start{ 0x9800 };
    static constexpr uint16_t kVRAMTileMap2Start{ 0x9C00 };
    static constexpr uint16_t kVRAMTileData1Start{ 0x8000 };
    static constexpr uint16_t kVRAMTileData2Start{ 0x9000 };

    std::shared_ptr<MMU> m_mmu;
    PPU *m_ppu;
    std::queue<Pixel> m_pixel_fifo;
    uint8_t m_current_tick{ 0U };

    uint16_t m_tile_index{ 0U };
    uint16_t m_tile_line{ 0U };
    uint16_t m_tile_data_address{ 0U };
    uint16_t m_tile_id_row_start_address{ 0U };

    uint16_t m_output_trace{ 0U };
    uint8_t m_current_tile_id{ 0U };
    uint8_t m_first_data_byte{ 0U };
    uint8_t m_second_data_byte{ 0U };

    bool m_is_window{ false };
    PixelFetcher::Mode m_mode = PixelFetcher::Mode::ReadTileId;
};
}