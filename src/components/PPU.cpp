#include "PPU.h"
//#include "util/LogUtilities.h"
namespace gbcemu {

PPU::PPU(std::shared_ptr<MMU> mmu) : m_mmu(mmu) {
    m_current_dot = 0;
    m_current_scanline = 0;
}

void PPU::tick(uint32_t number_of_cycles) {
    m_current_dot += number_of_cycles;
    if (m_current_dot >= PPU::DotsPerScanline) {
        m_current_dot = 0;
        m_current_scanline++;

        if (m_current_scanline >= PPU::ScanlinesPerFrame)
            m_current_scanline = 0;
    }

    if (m_current_scanline < PPU::VBlankStartScanline) {
        switch (m_mode) {
        case PPU::Mode::VBlank:
            m_mode = PPU::Mode::OAMSearch;
            break;
        case PPU::Mode::OAMSearch:
            if (m_current_dot >= PPU::DotsInOAMSearch)
                m_mode = PPU::Mode::DataTransfer;
            break;
        case PPU::Mode::DataTransfer:
            // TODO: Implement
            break;
        default:
            break;
        }
    } else {
        if (m_mode != PPU::Mode::VBlank) {
            // LogUtilities::log(LoggerType::Internal, LogLevel::Info, "hit vblank scanline");
            set_mode(PPU::Mode::VBlank);
            // TODO: Trigger interrupt
        }
    }

    m_mmu->try_map_data_to_memory(&m_current_scanline, 0xFF00 | LCDYCoordinateRegisterOffset, 1);
}

void PPU::set_mode(const PPU::Mode mode) { m_mode = mode; }
}