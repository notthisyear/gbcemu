#include "MMU.h"
#include "Cartridge.h"
#include "util/GeneralUtilities.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace gbcemu {
MMU::MMU(uint16_t memory_size) : m_memory_size(memory_size) {
    m_memory = new uint8_t[m_memory_size];
    memset(m_memory, (uint8_t)0x00, m_memory_size);
}

bool MMU::try_load_boot_rom(std::ostream &stream, const std::string &path) {

    uint64_t size;
    if (!get_file_size(path, &size)) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load boot ROM from '" << path << "'" << std::endl;
        return false;
    }

    m_boot_rom_size = size;
    m_boot_rom = new uint8_t[size];

    bool success = try_load_from_file(path, m_boot_rom, size);
    if (!success) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load boot ROM from '" << path << "'" << std::endl;
        return false;
    }

    return true;
}

bool MMU::try_load_cartridge(std::ostream &stream, const std::string &path) {

    uint64_t size;
    if (!get_file_size(path, &size)) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load cartridge from '" << path << "'" << std::endl;
        return false;
    }

    auto cartridge_data = new uint8_t[size];
    bool success = try_load_from_file(path, cartridge_data, size);
    if (!success) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load cartridge from '" << path << "'" << std::endl;
        return false;
    }

    m_cartridge = std::make_unique<Cartridge>(cartridge_data, size);
    auto fixed_cartridge_location = m_region_map.find(MMU::MemoryRegion::CartridgeFixed)->second;

    m_loading_cartridge = true;
    success = try_map_data_to_memory(cartridge_data, fixed_cartridge_location.first, fixed_cartridge_location.second - fixed_cartridge_location.first + 1);
    m_loading_cartridge = false;

    return success;
}

bool MMU::try_map_data_to_memory(uint8_t *data, uint16_t offset, uint16_t size) {

    auto region = find_memory_region(offset);
    auto region_endpoints = m_region_map.find(region)->second;

    if ((offset + size - 1) > region_endpoints.second)
        return false;

    bool result = true;
    switch (region) {

    // ROM
    case MMU::MemoryRegion::CartridgeFixed:
    case MMU::MemoryRegion::CartridgeSwitchable:
        if (!m_loading_cartridge)
            result = false;
        else
            write_to_memory(data, offset, size);

    case MMU::MemoryRegion::VRAMSwitchable:
        // TODO: Implement VRAM banks (only on CGB)
        write_to_memory(data, offset, size);
        break;

    case MMU::MemoryRegion::WRAMSwitchable:
        // TODO: Implement WRAM banks (only on CGB)
        write_to_memory(data, offset, size);
        break;

    case MMU::MemoryRegion::CartridgeRAM:
        m_cartridge->write_to_cartridge_ram(data, offset - region_endpoints.first, size);
        break;

    case MMU::MemoryRegion::EchoRAM:
        write_to_memory(data, offset - 0x2000, size);
        break;

    case MMU::MemoryRegion::WRAMFixed:
    case MMU::MemoryRegion::SpriteAttributeTable:
    case MMU::MemoryRegion::IORegisters:
    case MMU::MemoryRegion::HRAM:
    case MMU::MemoryRegion::IERegister:
        write_to_memory(data, offset, size);
        break;

    case MMU::MemoryRegion::Restricted:
        // TODO: Emulate correct behavior
        result = false;
        break;

    default:
        __builtin_unreachable();
        break;
    }

    return result;
}

bool MMU::try_read_from_memory(uint8_t *data, uint16_t offset, uint64_t size) const {
    auto region = find_memory_region(offset);
    auto region_endpoints = m_region_map.find(region)->second;

    bool result = true;
    if (region == MMU::MemoryRegion::CartridgeFixed && m_is_in_boot_mode && is_boot_rom_range(offset, size)) {
        read_from_boot_rom(data, offset, size);
    } else {
        if ((offset + size - 1) > region_endpoints.second)
            return false;

        switch (region) {
        case MMU::MemoryRegion::CartridgeSwitchable:
            m_cartridge->read_from_cartridge_switchable(data, offset - region_endpoints.first, size);
            break;

        case MMU::MemoryRegion::CartridgeRAM:
            m_cartridge->read_from_cartridge_ram(data, offset - region_endpoints.first, size);
            break;

        case MMU::MemoryRegion::VRAMSwitchable:
            // TODO: Implement VRAM banks (only on CGB)
            read_from_memory(data, offset, size);
            break;

        case MMU::MemoryRegion::WRAMSwitchable:
            // TODO: Implement WRAM banks (only on CGB)
            read_from_memory(data, offset, size);
            break;

        case MMU::MemoryRegion::EchoRAM:
            read_from_memory(data, offset - 0x2000, size);
            break;

        case MMU::MemoryRegion::CartridgeFixed:
        case MMU::MemoryRegion::WRAMFixed:
        case MMU::MemoryRegion::SpriteAttributeTable:
        case MMU::MemoryRegion::IORegisters:
        case MMU::MemoryRegion::HRAM:
        case MMU::MemoryRegion::IERegister:
            read_from_memory(data, offset, size);
            break;

        case MMU::MemoryRegion::Restricted:
            // TODO: Emulate correct behavior
            result = false;
            break;

        default:
            __builtin_unreachable();

            break;
        }
    }
    return result;
}

void MMU::print_memory_at_location(std::ostream &stream, uint16_t start, uint16_t end) const {

    stream << std::endl;

    uint16_t address_start;
    if (start % 16 != 0) {
        address_start = start - (start % 16);
    } else {
        address_start = start;
    }

    auto number_of_bytes_to_print = (end - start) + 1;
    auto buffer = new uint8_t[number_of_bytes_to_print];

    if (!try_read_from_memory(buffer, start, number_of_bytes_to_print)) {
        stream << "\033[1;31m[error] \033[0;mCannot show memory across memory regions" << std::endl;
        return;
    }

    auto region = find_memory_region(start);
    std::cout << "from region "
              << "\033[1;32m" << get_region_name(region) << "\033[0m\n"
              << std::endl;

    auto number_of_rows = ((end - address_start) / 16) + 1;

    if (number_of_bytes_to_print == 1) {
        stream << "\033[1;37m" << std::left << std::setw(10) << std::setfill(' ') << GeneralUtilities::formatted_string("0x%04X", start);
        stream << "\033[0m" << GeneralUtilities::formatted_string("%02x", buffer[0]) << std::endl;
        return;
    }

    stream << std::left << std::setw(10) << std::setfill(' ') << " ";
    stream << "\033[1;37m";
    for (auto i = 0; i < 16; i++)
        stream << std::left << std::setw(4) << GeneralUtilities::formatted_string("%02X", i);
    stream << "\033[0m" << std::endl;

    uint16_t current_start_offset, row_end;
    auto buffer_idx = 0;
    for (auto r = 0; r < number_of_rows; r++) {
        current_start_offset = address_start + (r * 16);
        row_end = current_start_offset + 16 < (end + 1) ? current_start_offset + 16 : end + 1;
        stream << "\033[1;37m" << std::left << std::setw(10) << std::setfill(' ') << GeneralUtilities::formatted_string("0x%04X", current_start_offset);
        stream << "\033[0m";

        for (auto i = current_start_offset; i < row_end; i++) {
            stream << std::left << std::setw(4);
            if (i < start)
                stream << std::setfill(' ') << " ";
            else
                stream << GeneralUtilities::formatted_string("%02x", buffer[buffer_idx++]);
        }

        stream << std::endl;
    }
}

bool MMU::get_file_size(const std::string &path, uint64_t *size) const {
    try {
        *size = std::filesystem::file_size(path);
        return true;
    } catch (std::filesystem::filesystem_error &e) {
        return false;
    }
}

bool MMU::try_load_from_file(const std::string &path, uint8_t *buffer, const uint64_t size) const {
    std::ifstream boot_file(path, std::ios::in | std::ios::binary);
    if (boot_file.fail())
        return false;

    boot_file.read((char *)buffer, size);
    return true;
}

bool MMU::is_boot_rom_range(uint16_t offset, uint64_t size) const {
    // CGB boot ROM is split into two, 0x0000 - 0x00FF and 0x0200 - 0x08FF
    if (m_boot_rom_size == 0x100)
        return (offset + size) <= 0x100;

    auto end = offset + size;
    return end < 0xFF || (offset > 0x01FF && end < 0x0900);
}

void MMU::read_from_boot_rom(uint8_t *data, uint16_t offset, uint64_t size) const {

    if (m_boot_rom_size > 0xFF && offset > 0x01FF)
        offset -= 0x0100;

    for (auto i = 0; i < size; i++)
        data[i] = m_boot_rom[offset + i];
}

void MMU::set_in_boot_mode(bool is_in_boot_mode) { m_is_in_boot_mode = is_in_boot_mode; }

MMU::MemoryRegion MMU::find_memory_region(uint16_t address) const {
    for (auto const &entry : m_region_map) {
        if (address > entry.second.second)
            continue;
        return entry.first;
    }
    __builtin_unreachable();
}

const std::map<MMU::MemoryRegion, std::pair<uint16_t, uint16_t>> MMU::m_region_map = {
    { MMU::MemoryRegion::CartridgeFixed, MMU::make_address_pair(0x0000, 0x3FFF) },
    { MMU::MemoryRegion::CartridgeSwitchable, MMU::make_address_pair(0x4000, 0x7FFF) },
    { MMU::MemoryRegion::VRAMSwitchable, MMU::make_address_pair(0x8000, 0x9FFF) },
    { MMU::MemoryRegion::CartridgeRAM, MMU::make_address_pair(0xA000, 0xBFFF) },
    { MMU::MemoryRegion::WRAMFixed, MMU::make_address_pair(0xC000, 0xCFFF) },
    { MMU::MemoryRegion::WRAMSwitchable, MMU::make_address_pair(0xD000, 0xDFFF) },
    { MMU::MemoryRegion::EchoRAM, MMU::make_address_pair(0xE000, 0xFDFF) },
    { MMU::MemoryRegion::SpriteAttributeTable, MMU::make_address_pair(0xFE00, 0xFEFF) },
    { MMU::MemoryRegion::Restricted, MMU::make_address_pair(0xFF00, 0xFF7F) },
    { MMU::MemoryRegion::IORegisters, MMU::make_address_pair(0x0000, 0x3FFF) },
    { MMU::MemoryRegion::HRAM, MMU::make_address_pair(0xFF80, 0xFFFE) },
    { MMU::MemoryRegion::IERegister, MMU::make_address_pair(0xFFFF, 0xFFFF) },
};

const std::map<MMU::IORegister, std::pair<uint8_t, uint8_t>> MMU::m_io_register_map = {
    { MMU::IORegister::Joypad, MMU::make_offset_and_size_pair(0x00, 0x01) },
    { MMU::IORegister::SerialTranfer, MMU::make_offset_and_size_pair(0x01, 0x02) },
    { MMU::IORegister::TimerAndDivider, MMU::make_offset_and_size_pair(0x04, 0x04) },
    { MMU::IORegister::Sound, MMU::make_offset_and_size_pair(0x10, 0x17) },
    { MMU::IORegister::WavePattern, MMU::make_offset_and_size_pair(0x30, 0x10) },
    { MMU::IORegister::LCDControl, MMU::make_offset_and_size_pair(0x40, 0x0C) },
    { MMU::IORegister::VRAMBankSelect, MMU::make_offset_and_size_pair(0x4F, 0x01) },
    { MMU::IORegister::DisableBootRom, MMU::make_offset_and_size_pair(0x50, 0x01) },
    { MMU::IORegister::VRAMDMA, MMU::make_offset_and_size_pair(0x51, 0x05) },
    { MMU::IORegister::BgObjPalettes, MMU::make_offset_and_size_pair(0x68, 0x02) },
    { MMU::IORegister::WRAMBankSelect, MMU::make_offset_and_size_pair(0x70, 0x01) },
};

const std::unordered_map<MMU::MemoryRegion, std::string> MMU::m_region_names = {
    { MMU::MemoryRegion::CartridgeFixed, "CartridgeFixed" },
    { MMU::MemoryRegion::CartridgeSwitchable, "CartridgeSwitchable" },
    { MMU::MemoryRegion::VRAMSwitchable, "VRAMSwitchable" },
    { MMU::MemoryRegion::CartridgeRAM, "CartridgeRAM" },
    { MMU::MemoryRegion::WRAMFixed, "WRAMFixed" },
    { MMU::MemoryRegion::WRAMSwitchable, "WRAMSwitchable" },
    { MMU::MemoryRegion::EchoRAM, "EchoRAM" },
    { MMU::MemoryRegion::SpriteAttributeTable, "SpriteAttributeTable" },
    { MMU::MemoryRegion::Restricted, "Restricted" },
    { MMU::MemoryRegion::IORegisters, "IORegisters" },
    { MMU::MemoryRegion::HRAM, "HRAM" },
    { MMU::MemoryRegion::IERegister, "IERegister" },
};

const std::unordered_map<MMU::IORegister, std::string> MMU::m_io_register_names = {
    { MMU::IORegister::Joypad, "Joypad" },
    { MMU::IORegister::SerialTranfer, "SerialTranfer" },
    { MMU::IORegister::TimerAndDivider, "TimerAndDivider" },
    { MMU::IORegister::Sound, "Sound" },
    { MMU::IORegister::WavePattern, "WavePattern" },
    { MMU::IORegister::LCDControl, "LCDControl" },
    { MMU::IORegister::VRAMBankSelect, "VRAMBankSelect" },
    { MMU::IORegister::DisableBootRom, "DisableBootRom" },
    { MMU::IORegister::VRAMDMA, "VRAMDMA" },
    { MMU::IORegister::BgObjPalettes, "BgObjPalettes" },
    { MMU::IORegister::WRAMBankSelect, "WRAMBankSelect" },
};

std::string MMU::get_region_name(MMU::MemoryRegion region) const { return MMU::m_region_names.find(region)->second; }
std::string MMU::get_io_register_name(MMU::IORegister reg) const { return MMU::m_io_register_names.find(reg)->second; }

MMU::~MMU() { delete[] m_memory; }
}