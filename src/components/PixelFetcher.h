#pragma once

#include "MMU.h"
#include <memory>
#include <queue>
#include <stdint.h>

namespace gbcemu {

// Forward declare PPU
class PPU;

struct Pixel {
    uint8_t color_index;
};

class PixelFetcher {
  public:
    PixelFetcher(std::shared_ptr<MMU>, PPU *);
    void start_fetcher(uint8_t, bool);
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

    const uint8_t PixelFifoFetchThreshold = 8;
    const uint16_t VRAMTileMap1Start = 0x9800;
    const uint16_t VRAMTileMap2Start = 0x9C00;
    const uint16_t VRAMTileData1Start = 0x8000;
    const uint16_t VRAMTileData2Start = 0x9000;

    std::shared_ptr<MMU> m_mmu;
    PPU *m_ppu;
    std::queue<Pixel> m_pixel_fifo;

    uint8_t m_current_tick;

    uint16_t m_tile_index;
    uint16_t m_tile_line;
    uint16_t m_tile_data_address;
    uint16_t m_tile_id_row_start_address;

    uint8_t m_current_tile_id;
    uint8_t m_first_data_byte;
    uint8_t m_second_data_byte;

    bool m_is_window;
    PixelFetcher::Mode m_mode = PixelFetcher::Mode::ReadTileId;
};
}