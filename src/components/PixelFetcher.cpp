#include "PixelFetcher.h"
#include "PPU.h"
#include "util/GeneralUtilities.h"
#include <iostream>

namespace gbcemu {

PixelFetcher::PixelFetcher(std::shared_ptr<MMU> const mmu, PPU *const ppu) : m_mmu(mmu), m_ppu(ppu) {}

void PixelFetcher::start_fetcher(uint8_t const current_scanline, bool const is_window, bool const trace) {
    m_current_tick = 0;

    m_tile_line = (current_scanline + m_mmu->get_io_register(MMU::IORegister::SCY)) & 0x00FF;
    m_tile_index = m_mmu->get_io_register(MMU::IORegister::SCX) >> 3;

    bool const tile_map_area_switch{ is_window ? m_ppu->lcd_control_bit_is_set(PPU::LCDControlRegisterBit::WindowTileMapArea)
                                               : m_ppu->lcd_control_bit_is_set(PPU::LCDControlRegisterBit::BGTileMapArea) };

    m_tile_id_row_start_address = (tile_map_area_switch ? kVRAMTileMap2Start : kVRAMTileMap1Start) + ((m_tile_line >> 3) << 5);

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
        bool const use_signed_addressing_mode{ !m_ppu->lcd_control_bit_is_set(PPU::LCDControlRegisterBit::BGAndWindowTileDataArea) };
        uint16_t const tile_data_start_address{ use_signed_addressing_mode ? kVRAMTileData2Start : kVRAMTileData1Start };

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
        if (m_pixel_fifo.size() <= kPixelFifoFetchThreshold) {
            uint8_t shift{ 0 };
            while (shift < 8) {
                uint8_t const least_significant_color_bit{ static_cast<uint8_t>((m_first_data_byte >> (7 - shift)) & 0x01) };
                uint8_t const most_significant_color_bit{ static_cast<uint8_t>((m_second_data_byte >> (7 - shift)) & 0x01) };
                Pixel const pixel{ .color_index = static_cast<uint8_t>((most_significant_color_bit << 1) | least_significant_color_bit) };
                m_pixel_fifo.push(pixel);
                shift++;
            }

            m_tile_index++;
            if (m_tile_index > 0x1F) {
                m_tile_index = 0;
            }
            m_mode = PixelFetcher::Mode::ReadTileId;
        }
        break;
    }
}

Pixel PixelFetcher::pop_pixel() {
    Pixel const pixel{ m_pixel_fifo.front() };
    m_pixel_fifo.pop();
    return pixel;
}

bool PixelFetcher::can_pop_pixel() const { return m_pixel_fifo.size() > 0; }

}