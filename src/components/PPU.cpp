#include "PPU.h"
#include <cstring>
#include <iostream>

namespace gbcemu {

PPU::PPU(std::shared_ptr<MMU> mmu) : m_mmu(mmu), m_current_dot_on_line(0), m_mode(PPU::Mode::OAMSearch), m_current_scanline(0), m_last_scanline(0) {
    m_framebuffer = new uint8_t[PPU::DisplayWidth * PPU::DisplayHeight * PPU::BytesPerPixel];
    memset(m_framebuffer, 0, PPU::DisplayWidth * PPU::DisplayHeight * PPU::BytesPerPixel);
}

void PPU::tick() {

    m_current_dot_on_line++;

    switch (m_mode) {
    case PPU::Mode::OAMSearch:
        if (m_current_dot_on_line == PPU::DotsInOAMSearch)
            m_mode = PPU::Mode::DataTransfer;
        break;
    case PPU::Mode::DataTransfer:
        // Here, there is a pixel fetcher that could be emulated.
        // There are two FIFOs, background and sprites. They can hold 16 pixels each.
        // The pixel fetcher ensures that there
        // The fetcher fetches a row of 8 pixels.
        // It has five steps:
        // 1. Get tile
        // 2. Get tile data low
        // 3. Get tile data high
        // 4. Sleep
        // 5. Push
        // Steps 1-4 take two dots each, step 5 run for every dot until it is completed
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