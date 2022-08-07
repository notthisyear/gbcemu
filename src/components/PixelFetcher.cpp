#include "PixelFetcher.h"
#include "PPU.h"
#include "util/GeneralUtilities.h"
#include <iostream>

namespace gbcemu {

PixelFetcher::PixelFetcher(std::shared_ptr<MMU> mmu, PPU *ppu) : m_mmu(mmu), m_ppu(ppu) {}

void PixelFetcher::start_fetcher(uint8_t current_scanline, bool is_window, bool trace) {
    m_current_tick = 0;

    m_tile_line = (current_scanline + m_mmu->get_io_register(MMU::IORegister::SCY)) & 0x00FF;
    m_tile_index = m_mmu->get_io_register(MMU::IORegister::SCX) >> 3;

    auto tile_map_area_switch = is_window ? m_ppu->get_lcd_control_bit(PPU::LCDControlRegisterBit::WindowTileMapArea)
                                          : m_ppu->get_lcd_control_bit(PPU::LCDControlRegisterBit::BGTileMapArea);

    m_tile_id_row_start_address = (tile_map_area_switch ? VRAMTileMap2Start : VRAMTileMap1Start) + ((m_tile_line >> 3) << 5);

    m_pixel_fifo = {};
    m_mode = PixelFetcher::Mode::ReadTileId;
    m_output_trace = trace;

    if (m_output_trace) {
        std::cout << GeneralUtilities::formatted_string("\n\n\n\033[1;37mLine %d (SCY: %d, SCX %d)\033[0m\n", current_scanline,
                                                        m_mmu->get_io_register(MMU::IORegister::SCY), m_mmu->get_io_register(MMU::IORegister::SCX));
    }
}

void PixelFetcher::tick() {
    if ((m_current_tick & 0x01) == 0) {
        m_current_tick++;
        return;
    }

    m_current_tick = m_current_tick == 3 ? 0 : m_current_tick + 1;

    switch (m_mode) {
    case PixelFetcher::Mode::ReadTileId:
        (void)m_mmu->try_read_from_memory(&m_current_tile_id, m_tile_id_row_start_address + m_tile_index, 1);

        if (m_output_trace) {
            std::cout << GeneralUtilities::formatted_string("\033[0;36mTile ID (0x%04X): \033[1;37m0x%02X\033[0m, ", m_tile_id_row_start_address + m_tile_index,
                                                            m_current_tile_id);
        }
        m_mode = PixelFetcher::Mode::ReadTileData0;
        break;

    case PixelFetcher::Mode::ReadTileData0: {
        bool use_signed_addressing_mode = !m_ppu->get_lcd_control_bit(PPU::LCDControlRegisterBit::BGAndWindowTileDataArea);
        auto tile_data_start_address = use_signed_addressing_mode ? VRAMTileData2Start : VRAMTileData1Start;

        uint16_t tile_address;
        if (use_signed_addressing_mode) {
            int8_t offset;
            memcpy(&offset, &m_current_tile_id, 1);
            tile_address = tile_data_start_address + (static_cast<int16_t>(offset) << 4);
        } else {
            tile_address = tile_data_start_address + (m_current_tile_id << 4);
        }

        m_tile_data_address = tile_address + ((m_tile_line & 0x07) << 1);
        (void)m_mmu->try_read_from_memory(&m_first_data_byte, m_tile_data_address, 1);

        if (m_output_trace) {
            std::cout << GeneralUtilities::formatted_string("\033[0;32mlow data (0x%04X): \033[1;37m0x%02X\033[0m, ", m_tile_data_address, m_first_data_byte);
        }
        m_mode = PixelFetcher::Mode::ReadTileData1;
    }

    break;

    case PixelFetcher::Mode::ReadTileData1:
        (void)m_mmu->try_read_from_memory(&m_second_data_byte, m_tile_data_address + 1, 1);

        if (m_output_trace) {
            std::cout << GeneralUtilities::formatted_string("\033[0;32mhigh tile (0x%04X): \033[1;37m0x%02X\033[0m\n", m_tile_data_address + 1,
                                                            m_second_data_byte);
        }
        m_mode = PixelFetcher::Mode::Idle;
        break;

    case PixelFetcher::Mode::Idle:
        if (m_pixel_fifo.size() <= PixelFifoFetchThreshold) {
            for (auto i = 7; i >= 0; i--) {
                Pixel pixel;
                pixel.color_index = ((m_first_data_byte >> i) & 0x01) | (((m_second_data_byte >> i) & 0x01) << 1);
                m_pixel_fifo.push(pixel);
            }

            m_tile_index++;
            if (m_tile_index > 0x1F)
                m_tile_index = 0;

            m_mode = PixelFetcher::Mode::ReadTileId;
        }
        break;
    }
}

Pixel PixelFetcher::pop_pixel() {
    auto pixel = m_pixel_fifo.front();
    m_pixel_fifo.pop();
    return pixel;
}

bool PixelFetcher::can_pop_pixel() const { return m_pixel_fifo.size() > 0; }

}