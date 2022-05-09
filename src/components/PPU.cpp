#include "PPU.h"
#include "util/GeneralUtilities.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <stdint.h>

namespace gbcemu {

PPU::PPU(std::shared_ptr<MMU> mmu, uint16_t framebuffer_width, uint16_t framebuffer_height, uint8_t bytes_per_pixel)
    : m_mmu(mmu), m_framebuffer_width(framebuffer_width), m_framebuffer_height(framebuffer_height), m_bytes_per_pixel(bytes_per_pixel) {

    m_framebuffer = new uint8_t[m_framebuffer_width * m_framebuffer_height * m_bytes_per_pixel];
    memset(m_framebuffer, 0, m_framebuffer_width * m_framebuffer_height * m_bytes_per_pixel);
    m_pixel_fetcher = std::make_unique<PixelFetcher>(m_mmu, this);

    reset_ppu_state();
    write_current_mode_to_status_register();
}

void PPU::tick() {

    if (!m_screen_enabled) {
        if (get_lcd_control_bit(PPU::LCDControlRegisterBit::LCDAndPPUEnable)) {
            reset_ppu_state();
            write_current_mode_to_status_register();
            m_screen_enabled = true;
        } else {
            return;
        }
    } else {
        if (!get_lcd_control_bit(PPU::LCDControlRegisterBit::LCDAndPPUEnable)) {
            m_screen_enabled = false;
            return;
        }
    }

    m_total_frame_dots++;
    m_dots_on_current_line++;

    switch (m_mode) {

    case PPU::Mode::OAMSearch:
        if (m_dots_on_current_line == PPU::DotsInOAMSearch) {
            m_mode = PPU::Mode::DataTransfer;
            m_pixels_pushed_on_current_line = 0;
            m_pixel_fetcher->start_fetcher(m_current_scanline, false);
        }
        break;

    case PPU::Mode::DataTransfer:
        m_pixel_fetcher->tick();

        if (m_pixel_fetcher->can_pop_pixel()) {
            auto pixel = m_pixel_fetcher->pop_pixel();
            m_pixels_pushed_on_current_line++;

            // TODO: Proper color mapping
            uint8_t c;
            if (pixel.color_index == 0)
                c = 255;
            else if (pixel.color_index == 1)
                c = 200;
            else if (pixel.color_index == 2)
                c = 60;
            else if (pixel.color_index == 3)
                c = 0;

            m_framebuffer[m_framebuffer_idx++] = c;
            m_framebuffer[m_framebuffer_idx++] = c;
            m_framebuffer[m_framebuffer_idx++] = c;
            m_framebuffer[m_framebuffer_idx++] = 255;
        }

        if (m_pixels_pushed_on_current_line == PixelsPerScanline)
            m_mode = PPU::Mode::HBlank;

        break;

    case PPU::Mode::VBlank:
        if (m_dots_on_current_line == DotsPerScanline) {
            m_dots_on_current_line = 0;
            m_current_scanline++;
        }

        if (m_current_scanline > ScanlinesPerFrame) {
            m_mode = PPU::Mode::OAMSearch;
            m_current_scanline = 0;
            m_framebuffer_idx = 0;
        }
        break;

    case PPU::Mode::HBlank:
        if (m_dots_on_current_line == DotsPerScanline) {
            m_current_scanline++;
            m_dots_on_current_line = 0;

            if (m_current_scanline == VBlankStartScanline)
                m_mode = PPU::Mode::VBlank;
            else
                m_mode = PPU::Mode::OAMSearch;
        }

        break;

    default:
        __builtin_unreachable();
        break;
    }

    if (m_current_scanline != m_last_scanline) {
        m_mmu->set_register(MMU::MemoryRegister::LY, m_current_scanline);
        m_last_scanline = m_current_scanline;
    }

    if (m_mode != m_last_mode) {
        write_current_mode_to_status_register();
        m_last_mode = m_mode;
    }

    if (m_total_frame_dots == DotsPerFrame) {
        m_frame_done_flag = true;
        m_total_frame_dots = 0;
    }

    set_lcd_status_bit(PPU::LCDStatusRegisterBit::LYCEqualsLY, m_mmu->get_register(MMU::MemoryRegister::LY) == m_mmu->get_register(MMU::MemoryRegister::LYC));
}

bool PPU::cycles_per_frame_reached() const { return m_frame_done_flag; }

void PPU::acknowledge_frame() { m_frame_done_flag = false; }

uint8_t *PPU::get_framebuffer() const { return m_framebuffer; }

bool PPU::get_lcd_control_bit(const PPU::LCDControlRegisterBit bit_to_get) {
    return get_bit_in_ppu_register(MMU::MemoryRegister::LCDC, static_cast<uint8_t>(bit_to_get));
}

bool PPU::get_lcd_status_bit(const PPU::LCDStatusRegisterBit bit_to_get) {
    return get_bit_in_ppu_register(MMU::MemoryRegister::STAT, static_cast<uint8_t>(bit_to_get));
}

PPU::~PPU() { delete[] m_framebuffer; }

void PPU::reset_ppu_state() {
    m_framebuffer_idx = 0;
    m_total_frame_dots = 0;
    m_dots_on_current_line = 0;
    m_current_scanline = 0;
    m_last_scanline = 0;

    m_frame_done_flag = false;
    m_screen_enabled = false;
    m_mode = PPU::Mode::OAMSearch;

    m_last_mode = m_mode;
    m_mmu->set_register(MMU::MemoryRegister::LY, m_current_scanline);
}

void PPU::set_lcd_control_bit(const PPU::LCDControlRegisterBit bit_to_set, const bool on_or_off) {
    set_bit_in_ppu_register(MMU::MemoryRegister::LCDC, static_cast<uint8_t>(bit_to_set), on_or_off);
}

void PPU::set_lcd_status_bit(const PPU::LCDStatusRegisterBit bit_to_set, const bool on_or_off) {
    set_bit_in_ppu_register(MMU::MemoryRegister::STAT, static_cast<uint8_t>(bit_to_set), on_or_off);
}

void PPU::set_bit_in_ppu_register(const MMU::MemoryRegister reg, const uint8_t bit_to_set, const bool on_or_off) {
    auto current = m_mmu->get_register(reg);
    uint8_t mask = 0xFE << bit_to_set;
    uint8_t value = (on_or_off ? 0x01 : 0x00) << bit_to_set;
    m_mmu->set_register(reg, (current & mask) | value);
}

bool PPU::get_bit_in_ppu_register(const MMU::MemoryRegister reg, const uint8_t bit_to_get) {
    auto current = m_mmu->get_register(reg);
    return (current & (0x01 << bit_to_get)) > 0;
}

void PPU::write_current_mode_to_status_register() {
    auto current_stat = m_mmu->get_register(MMU::MemoryRegister::STAT);
    m_mmu->set_register(MMU::MemoryRegister::STAT, (current_stat & 0xFC) | static_cast<uint8_t>(m_mode));
}
}