#pragma once

#include "MMU.h"
#include <memory>
#include <queue>
#include <stdint.h>

namespace gbcemu {

struct Pixel {
    uint8_t Color;
};

class PixelFetcher {
  public:
    PixelFetcher(std::shared_ptr<MMU>);
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

    std::shared_ptr<MMU> m_mmu;
    std::queue<Pixel> m_pixel_fifo;
    uint8_t m_current_tick;
    PixelFetcher::Mode m_mode = PixelFetcher::Mode::ReadTileId;
};
}