#include "PixelFetcher.h"

namespace gbcemu {

PixelFetcher::PixelFetcher(std::shared_ptr<MMU> mmu) : m_current_tick(0), m_mmu(mmu) {}

bool PixelFetcher::can_pop_pixel() const { return m_pixel_fifo.size() < PixelFifoFetchThreshold; }

Pixel PixelFetcher::pop_pixel() {
    auto pixel = m_pixel_fifo.front();
    m_pixel_fifo.pop();
    return pixel;
}

void PixelFetcher::tick() {
    if ((m_current_tick & 0x01) == 1) {
        m_current_tick++;
        return;
    }

    m_current_tick = m_current_tick == 2 ? 0 : m_current_tick + 1;

    switch (m_mode) {
    case PixelFetcher::Mode::ReadTileId:
        break;
    }
}
}