#include "PPU.h"
#include "components/PixelFetcher.h"
#include <cstring>
#include <iostream>
#include <memory>

namespace gbcemu {

PPU::PPU(std::shared_ptr<MMU> mmu) : m_mmu(mmu), m_current_dot_on_line(0), m_mode(PPU::Mode::OAMSearch), m_current_scanline(0), m_last_scanline(0) {
    m_framebuffer = new uint8_t[PPU::DisplayWidth * PPU::DisplayHeight * PPU::BytesPerPixel];
    memset(m_framebuffer, 0, PPU::DisplayWidth * PPU::DisplayHeight * PPU::BytesPerPixel);
    m_pixel_fetcher = std::make_unique<PixelFetcher>(m_mmu);
}

void PPU::tick() {

    m_current_dot_on_line++;

    switch (m_mode) {
    case PPU::Mode::OAMSearch:
        if (m_current_dot_on_line == PPU::DotsInOAMSearch)
            m_mode = PPU::Mode::DataTransfer;
        break;
    case PPU::Mode::DataTransfer:
        if (m_pixel_fetcher->can_pop_pixel()) {
            auto pixel = m_pixel_fetcher->pop_pixel();
            // TODO: Write pixel to framebuffer
        }

        m_pixel_fetcher->tick();

        if (m_current_dot_on_line == 172)
            m_mode = PPU::Mode::HBlank;
        break;

    case PPU::Mode::VBlank:
        if (m_current_dot_on_line == DotsPerScanline) {
            m_current_dot_on_line = 0;
            m_current_scanline++;
        }

        if (m_current_scanline > ScanlinesPerFrame) {
            m_mode = PPU::Mode::OAMSearch;
            m_current_scanline = 0;
        }
        break;

    case PPU::Mode::HBlank:
        if (m_current_dot_on_line == DotsPerScanline) {
            m_current_scanline++;
            m_current_dot_on_line = 0;
            if (m_current_scanline == VBlankStartScanline)
                m_mode = PPU::Mode::VBlank;
        }
        break;
    default:
        break;
    }

    if (m_current_scanline != m_last_scanline) {
        m_mmu->try_map_data_to_memory(&m_current_scanline, 0xFF00 | LCDYCoordinateRegisterOffset, 1);
        m_last_scanline = m_current_scanline;
    }
}

PPU::~PPU() { delete[] m_framebuffer; }
void PPU::set_mode(const PPU::Mode mode) { m_mode = mode; }
}