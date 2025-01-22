#include "MMU.h"
#include "Cartridge.h"
#include "util/GeneralUtilities.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace gbcemu {

MMU::MMU(std::size_t const memory_size) : m_memory_size(memory_size) {

    m_memory = new uint8_t[m_memory_size];
    memset(m_memory, (uint8_t)0x00, m_memory_size);
    initialize_registers();

    m_timer_controller = TimerController::get(this);
}

bool MMU::try_load_boot_rom(std::ostream &stream, std::string const &path) {

    std::size_t size{};
    if (!try_get_file_size(path, &size)) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load boot ROM from '" << path << "'" << std::endl;
        return false;
    }

    m_boot_rom_size = size;
    m_boot_rom = new uint8_t[size];

    if (!try_load_from_file(path, m_boot_rom, size)) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load boot ROM from '" << path << "'" << std::endl;
        return false;
    }

    if (m_boot_rom_size == kDmgBootRomSize) {
        m_boot_rom_type = MMU::BootRomType::DMG;
    }

    set_io_register(gbcemu::MMU::IORegister::BootRomDisableOffset, 0x00);
    return true;
}

bool MMU::try_load_cartridge(std::ostream &stream, std::string const &path) {

    std::size_t size{};
    if (!try_get_file_size(path, &size)) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load cartridge from '" << path << "'" << std::endl;
        return false;
    }

    uint8_t *const cartridge_data = new uint8_t[size];
    bool success{ try_load_from_file(path, cartridge_data, size) };
    if (!success) {
        stream << "\033[1;31m[error] "
               << "\033[0m Could not load cartridge from '" << path << "'" << std::endl;
        return false;
    }

    m_cartridge = new Cartridge(cartridge_data, size);
    std::pair<uint16_t, uint16_t> const fixed_cartridge_location{ s_region_map.find(MMU::MemoryRegion::CartridgeFixed)->second };

    m_loading_cartridge = true;
    success = try_map_data_to_memory(cartridge_data, fixed_cartridge_location.first, fixed_cartridge_location.second - fixed_cartridge_location.first + 1);
    m_loading_cartridge = false;

    return success;
}

bool MMU::try_map_data_to_memory(uint8_t *const data, uint16_t const offset, std::size_t const size) {

    MemoryRegion const region{ find_memory_region(offset) };
    std::pair<uint16_t, uint16_t> const region_endpoints{ s_region_map.find(region)->second };

    if ((offset + size - 1) > region_endpoints.second) {
        return false;
    }

    bool result{ true };
    switch (region) {

    // ROM
    case MMU::MemoryRegion::CartridgeFixed:
    case MMU::MemoryRegion::CartridgeSwitchable:
        if (!m_loading_cartridge) {
            if (has_cartridge()) {
                m_cartridge->write_to_cartridge_registers(data, offset, size);
            } else {
                result = false;
            }
        } else {
            write_to_memory(data, offset, size);
        }
        break;

    case MMU::MemoryRegion::VRAMSwitchable:
        // TODO: Implement VRAM banks (only on CGB)
        write_to_memory(data, offset, size);
        break;

    case MMU::MemoryRegion::WRAMSwitchable:
        // TODO: Implement WRAM banks (only on CGB)
        write_to_memory(data, offset, size);
        break;

    case MMU::MemoryRegion::CartridgeRAMSwitchable:
        m_cartridge->write_to_cartridge_ram(data, offset, size);
        break;

    case MMU::MemoryRegion::EchoRAM:
        write_to_memory(data, offset - 0x2000, size);
        break;

    case MMU::MemoryRegion::IORegisters:
        if (size == 1) {
            pre_process_io_register_access(offset - 0xFF00, AccessType::Write, data);
        }
        // Intentional fall-through
    case MMU::MemoryRegion::WRAMFixed:
    case MMU::MemoryRegion::SpriteAttributeTable:
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

bool MMU::try_read_from_memory(uint8_t *data, uint16_t const offset, std::size_t const size) const {
    MemoryRegion const region{ find_memory_region(offset) };
    std::pair<uint16_t, uint16_t> const region_endpoints{ s_region_map.find(region)->second };

    bool result{ true };
    bool const boot_rom_enabled{ get_io_register(MMU::IORegister::BootRomDisableOffset) == 0x00 };
    if ((region == MMU::MemoryRegion::CartridgeFixed) && boot_rom_enabled && is_boot_rom_range(offset, size)) {
        read_from_boot_rom(data, offset, size);
    } else {
        if ((offset + size - 1) > region_endpoints.second) {
            return false;
        }

        switch (region) {

        case MMU::MemoryRegion::CartridgeFixed:
            read_from_memory(data, offset, size);
            break;

        case MMU::MemoryRegion::CartridgeSwitchable:
            m_cartridge->read_from_cartridge_switchable(data, offset, size);
            break;

        case MMU::MemoryRegion::CartridgeRAMSwitchable:
            m_cartridge->read_from_cartridge_ram(data, offset, size);
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

        case MMU::MemoryRegion::IORegisters:
            pre_process_io_register_access(offset, AccessType::Read);
            // Intentional fall-through
        case MMU::MemoryRegion::WRAMFixed:
        case MMU::MemoryRegion::SpriteAttributeTable:
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

void MMU::set_io_register(const MMU::IORegister reg, uint8_t const value) {
    uint8_t v{ value };
    write_to_memory(&v, kRegisterOffsetBase | static_cast<uint16_t>(reg), 1);
}

uint8_t MMU::get_io_register(const MMU::IORegister reg) const {
    uint8_t data{};
    read_from_memory(&data, kRegisterOffsetBase | static_cast<uint16_t>(reg), 1);
    return data;
}

void MMU::read_from_memory(uint8_t *data, uint16_t const offset, std::size_t const size) const {
    for (std::size_t i{ 0 }; i < size; ++i)
        data[i] = m_memory[offset + i];
}

void MMU::write_to_memory(uint8_t *const data, uint16_t const offset, std::size_t const size) {
    for (std::size_t i{ 0 }; i < size; ++i)
        m_memory[offset + i] = data[i];
}

bool MMU::has_cartridge() const { return m_cartridge != nullptr; }

Cartridge *MMU::get_cartridge() const { return m_cartridge; }

void MMU::print_memory_at_location(std::ostream &stream, uint16_t const start, uint16_t const end) const {

    stream << std::endl;

    uint16_t address_start{};
    if (start % 16 != 0) {
        address_start = start - (start % 16);
    } else {
        address_start = start;
    }

    std::size_t const number_of_bytes_to_print{ (end - start) + 1U };
    uint8_t *const buffer = new uint8_t[number_of_bytes_to_print];

    if (!try_read_from_memory(buffer, start, number_of_bytes_to_print)) {
        stream << "\033[1;31m[error] \033[0;mCannot show memory across memory regions" << std::endl;
        return;
    }

    MemoryRegion const region = find_memory_region(start);
    bool const boot_rom_enabled{ get_io_register(MMU::IORegister::BootRomDisableOffset) == 0x00 };
    bool const is_boot_rom{ (region == MMU::MemoryRegion::CartridgeFixed) && boot_rom_enabled && is_boot_rom_range(start, (end - start) + 1) };

    std::cout << "from region "
              << "\033[1;32m" << (is_boot_rom ? "Boot ROM" : get_region_name(region)) << "\033[0m\n"
              << std::endl;

    uint16_t const number_of_rows{ static_cast<uint16_t>(((end - address_start) / 16) + 1) };

    if (number_of_bytes_to_print == 1) {
        stream << "\033[1;37m" << std::left << std::setw(10) << std::setfill(' ') << GeneralUtilities::formatted_string("0x%04X", start);
        stream << "\033[0m" << GeneralUtilities::formatted_string("%02x", buffer[0]) << std::endl;
        return;
    }

    stream << std::left << std::setw(10) << std::setfill(' ') << " ";
    stream << "\033[1;37m";
    for (std::size_t i{ 0 }; i < 16; ++i) {
        stream << std::left << std::setw(4) << GeneralUtilities::formatted_string("%02X", i);
    }
    stream << "\033[0m" << std::endl;

    uint16_t current_start_offset{};
    uint16_t row_end{};
    size_t buffer_idx{ 0 };

    for (uint16_t r{ 0 }; r < number_of_rows; ++r) {
        current_start_offset = address_start + (r * 16);
        row_end = current_start_offset + 16 < (end + 1) ? current_start_offset + 16 : end + 1;
        stream << "\033[1;37m" << std::left << std::setw(10) << std::setfill(' ') << GeneralUtilities::formatted_string("0x%04X", current_start_offset);
        stream << "\033[0m";

        for (uint16_t i{ current_start_offset }; i < row_end; ++i) {
            stream << std::left << std::setw(4);
            if (i < start) {
                stream << std::setfill(' ') << " ";
            } else {
                stream << GeneralUtilities::formatted_string("%02x", buffer[buffer_idx++]);
            }
        }

        stream << std::endl;
    }
}

bool MMU::try_get_file_size(std::string const &path, std::size_t *size) const {
    try {
        *size = std::filesystem::file_size(path);
        return true;
    } catch (std::filesystem::filesystem_error &e) {
        return false;
    }
}

bool MMU::try_load_from_file(std::string const &path, std::uint8_t *buffer, std::size_t const size) const {
    std::ifstream boot_file(path, std::ios::in | std::ios::binary);
    if (boot_file.fail()) {
        return false;
    }
    boot_file.read((char *)buffer, size);
    return true;
}

bool MMU::is_boot_rom_range(uint16_t const offset, std::size_t const size) const {
    // CGB boot ROM is split into two, 0x0000 - 0x00FF and 0x0200 - 0x08FF
    if (m_boot_rom_type == MMU::BootRomType::DMG) {
        return (offset + size) <= kDmgBootRomSize;
    }
    std::uint64_t const end{ offset + size };
    return end < 0xFF || (offset > 0x01FF && end < 0x0900);
}

void MMU::read_from_boot_rom(uint8_t *data, uint16_t offset, std::size_t const size) const {
    // CGB boot ROM is split into two, 0x0000 - 0x00FF and 0x0200 - 0x08FF
    if (m_boot_rom_size > 0xFF && offset > 0x01FF) {
        offset -= 0x0100;
    }
    for (std::size_t i{ 0 }; i < size; ++i)
        data[i] = m_boot_rom[offset + i];
}

MMU::BootRomType MMU::get_boot_rom_type() const { return m_boot_rom_type; }

MMU::MemoryRegion MMU::find_memory_region(uint16_t const address) const {
    for (auto const &entry : s_region_map) {
        if (address > entry.second.second) {
            continue;
        }
        return entry.first;
    }
    __builtin_unreachable();
}

void MMU::initialize_registers() {
    // Unused registers in the IO-register range should return 0xFF
    memset(m_memory + kRegisterOffsetBase, (std::uint8_t)0xFF, 0x0080);

    set_io_register(MMU::IORegister::JOYP, 0xCF);
    set_io_register(MMU::IORegister::SB, 0x00);
    set_io_register(MMU::IORegister::SC, 0x7E);
    set_io_register(MMU::IORegister::DIV, 0xAB);
    set_io_register(MMU::IORegister::TIMA, 0x00);
    set_io_register(MMU::IORegister::TMA, 0x00);
    set_io_register(MMU::IORegister::TAC, 0xF8);
    set_io_register(MMU::IORegister::IF, 0xE1);

    set_io_register(MMU::IORegister::NR10, 0x80);
    set_io_register(MMU::IORegister::NR11, 0xBF);
    set_io_register(MMU::IORegister::NR12, 0xF3);
    set_io_register(MMU::IORegister::NR13, 0xFF);
    set_io_register(MMU::IORegister::NR14, 0xBF);
    set_io_register(MMU::IORegister::NR21, 0x3F);
    set_io_register(MMU::IORegister::NR22, 0x00);
    set_io_register(MMU::IORegister::NR23, 0xFF);
    set_io_register(MMU::IORegister::NR24, 0xBF);
    set_io_register(MMU::IORegister::NR30, 0x7F);
    set_io_register(MMU::IORegister::NR31, 0xFF);
    set_io_register(MMU::IORegister::NR32, 0x9F);
    set_io_register(MMU::IORegister::NR33, 0xFF);
    set_io_register(MMU::IORegister::NR34, 0xBF);
    set_io_register(MMU::IORegister::NR41, 0xFF);
    set_io_register(MMU::IORegister::NR42, 0x00);
    set_io_register(MMU::IORegister::NR43, 0x00);
    set_io_register(MMU::IORegister::NR44, 0xBF);
    set_io_register(MMU::IORegister::NR50, 0x77);
    set_io_register(MMU::IORegister::NR51, 0xF3);
    set_io_register(MMU::IORegister::NR52, 0xF1);

    set_io_register(MMU::IORegister::LCDC, 0x91);
    set_io_register(MMU::IORegister::STAT, 0x85);
    set_io_register(MMU::IORegister::SCY, 0x00);
    set_io_register(MMU::IORegister::SCX, 0x00);
    set_io_register(MMU::IORegister::LY, 0x00);
    set_io_register(MMU::IORegister::LYC, 0x00);
    set_io_register(MMU::IORegister::DMA, 0xFF);
    set_io_register(MMU::IORegister::BGP, 0xFC);
    set_io_register(MMU::IORegister::WY, 0x00);
    set_io_register(MMU::IORegister::WX, 0x00);

    set_io_register(MMU::IORegister::IE, 0x00);
}

std::map<MMU::MemoryRegion, std::pair<std::uint16_t, std::uint16_t>> const MMU::s_region_map = {
    { MMU::MemoryRegion::CartridgeFixed, MMU::make_address_pair(0x0000, 0x3FFF) },
    { MMU::MemoryRegion::CartridgeSwitchable, MMU::make_address_pair(0x4000, 0x7FFF) },
    { MMU::MemoryRegion::VRAMSwitchable, MMU::make_address_pair(0x8000, 0x9FFF) },
    { MMU::MemoryRegion::CartridgeRAMSwitchable, MMU::make_address_pair(0xA000, 0xBFFF) },
    { MMU::MemoryRegion::WRAMFixed, MMU::make_address_pair(0xC000, 0xCFFF) },
    { MMU::MemoryRegion::WRAMSwitchable, MMU::make_address_pair(0xD000, 0xDFFF) },
    { MMU::MemoryRegion::EchoRAM, MMU::make_address_pair(0xE000, 0xFDFF) },
    { MMU::MemoryRegion::SpriteAttributeTable, MMU::make_address_pair(0xFE00, 0xFEFF) },
    { MMU::MemoryRegion::Restricted, MMU::make_address_pair(0xFEA0, 0xFEFF) },
    { MMU::MemoryRegion::IORegisters, MMU::make_address_pair(0xFF00, 0xFF7F) },
    { MMU::MemoryRegion::HRAM, MMU::make_address_pair(0xFF80, 0xFFFE) },
    { MMU::MemoryRegion::IERegister, MMU::make_address_pair(0xFFFF, 0xFFFF) },
};

std::unordered_map<MMU::MemoryRegion, std::string> const MMU::s_region_names = {
    { MMU::MemoryRegion::CartridgeFixed, "CartridgeFixed" },
    { MMU::MemoryRegion::CartridgeSwitchable, "CartridgeSwitchable" },
    { MMU::MemoryRegion::VRAMSwitchable, "VRAMSwitchable" },
    { MMU::MemoryRegion::CartridgeRAMSwitchable, "CartridgeRAMSwitchable" },
    { MMU::MemoryRegion::WRAMFixed, "WRAMFixed" },
    { MMU::MemoryRegion::WRAMSwitchable, "WRAMSwitchable" },
    { MMU::MemoryRegion::EchoRAM, "EchoRAM" },
    { MMU::MemoryRegion::SpriteAttributeTable, "SpriteAttributeTable" },
    { MMU::MemoryRegion::Restricted, "Restricted" },
    { MMU::MemoryRegion::IORegisters, "IORegisters" },
    { MMU::MemoryRegion::HRAM, "HRAM" },
    { MMU::MemoryRegion::IERegister, "IERegister" },
};

std::unordered_map<MMU::IORegister, std::string> const MMU::s_io_register_names = {

    { MMU::IORegister::JOYP, "Joypad" },
    { MMU::IORegister::SB, "SerialTransferData" },
    { MMU::IORegister::SC, "SerialTransferControl" },
    { MMU::IORegister::DIV, "DividerRegister" },
    { MMU::IORegister::TIMA, "TimerCounter" },
    { MMU::IORegister::TMA, "TimerModulo" },
    { MMU::IORegister::TAC, "TimerControl" },
    { MMU::IORegister::NR10, "SoundChannel1SweepRegister" },
    { MMU::IORegister::NR11, "SoundChannel1SoundLengthAndWavePattern" },
    { MMU::IORegister::NR12, "SoundChannel1VolumeEnvelope" },
    { MMU::IORegister::NR13, "SoundChannel1LowFrequency" },
    { MMU::IORegister::NR14, "SoundChannel1HighFrequency" },
    { MMU::IORegister::NR21, "SoundChannel2SoundLengthAndWavePattern" },
    { MMU::IORegister::NR22, "SoundChannel2VolumeEnvelope" },
    { MMU::IORegister::NR23, "SoundChannel2LowFrequency" },
    { MMU::IORegister::NR24, "SoundChannel2HighFrequency" },
    { MMU::IORegister::NR30, "SoundChannel3Enable" },
    { MMU::IORegister::NR31, "SoundChannel3SoundlLength" },
    { MMU::IORegister::NR32, "SoundChannel3OutputLevel" },
    { MMU::IORegister::NR33, "SoundChannel3LowFrequency" },
    { MMU::IORegister::NR34, "SoundChannel3HighFrequency" },
    { MMU::IORegister::NR41, "SoundChannel4SoundLength" },
    { MMU::IORegister::NR42, "SoundChannel4VolumeEnvelope" },
    { MMU::IORegister::NR43, "SoundChannel4PolynomialCounter" },
    { MMU::IORegister::NR44, "SoundChannel4CounterConsecutive" },
    { MMU::IORegister::NR50, "SoundChannelControl" },
    { MMU::IORegister::NR51, "SoundSelectOutputTerminal" },
    { MMU::IORegister::NR52, "SoundEnable" },
    { MMU::IORegister::LCDC, "LCDControl" },
    { MMU::IORegister::STAT, "LCDStatus" },
    { MMU::IORegister::SCY, "LCDScrollY" },
    { MMU::IORegister::SCX, "LCDScrollX" },
    { MMU::IORegister::LY, "LCDYCoordinate" },
    { MMU::IORegister::LYC, "LCDYCompare" },
    { MMU::IORegister::DMA, "DMATransferAndStart" },
    { MMU::IORegister::BGP, "BGPaletteData" },
    { MMU::IORegister::OBP0, "OBJPalette0Data" },
    { MMU::IORegister::OBP1, "OBJPalette1Data" },
    { MMU::IORegister::WY, "WindowYPosition" },
    { MMU::IORegister::WX, "WindowXPositionMinus7" },
    { MMU::IORegister::IF, "InterruptFlags" },
    { MMU::IORegister::IE, "InterruptEnable" },
    { MMU::IORegister::BootRomDisableOffset, "BootRomDisable" },

};

std::string MMU::get_region_name(MMU::MemoryRegion const region) const { return MMU::s_region_names.find(region)->second; }

std::string MMU::get_io_register_name(MMU::IORegister const reg) const { return MMU::s_io_register_names.find(reg)->second; }

void MMU::pre_process_io_register_access(uint8_t const offset, AccessType const access_type, uint8_t *data) const {
    IORegister const io_reg = static_cast<MMU::IORegister>(offset);

    switch (io_reg) {
    case MMU::IORegister::JOYP:
        if (access_type == AccessType::Write) {
            // Bits 0-3 are readonly and bits 6-7 are unused (and unwriteable).
            *data = (*data & 0x30) | (get_io_register(MMU::IORegister::JOYP) & 0xCF);
        }
        break;

    case MMU::IORegister::SC:
        if (access_type == AccessType::Write) {
            // Bits 2-6 are unused (and unwriteable). Additionally, bit 1 can only be written to on GBC.
            // TODO: Deal with bit 1 whenever we support GBC.
            *data = (*data & 0x81) | (get_io_register(MMU::IORegister::SC) & 0x7E);
        }
        break;
    case MMU::IORegister::DIV:
        if (access_type == AccessType::Write) {
            *data = 0x00; // All writes to DIV causes it to be reset.
            m_timer_controller->reset_divider();
        }
        break;

    case MMU::IORegister::TIMA:
        if (access_type == AccessType::Write) {
            m_timer_controller->tima_write_occured();
        }
        break;

    case MMU::IORegister::TAC:
        if (access_type == AccessType::Write) {
            // Bits 3-7 are unused (and unwriteable).
            *data = (*data & 0x07) | (get_io_register(MMU::IORegister::TAC) & 0xF8);
        }
        break;

    case MMU::IORegister::IF:
        if (access_type == AccessType::Write) {
            // Bits 5-7 are unused (and unwriteable).
            *data = (*data & 0x1F) | (get_io_register(MMU::IORegister::IF) & 0xE0);
        }
        break;

    case MMU::IORegister::NR10:
        if (access_type == AccessType::Write) {
            // Bit 7 is unused (and unwriteable).
            *data = (*data & 0x7F) | (get_io_register(MMU::IORegister::NR10) & 0x80);
        }
        break;

    case MMU::IORegister::NR14:
        if (access_type == AccessType::Write) {
            // Bits 3-5 are unused (and unwriteable).
            *data = (*data & 0xC7) | (get_io_register(MMU::IORegister::NR14) & 0x38);
        }
        break;

    case MMU::IORegister::NR24:
        if (access_type == AccessType::Write) {
            // Bits 3-5 are unused (and unwriteable).
            *data = (*data & 0xC7) | (get_io_register(MMU::IORegister::NR24) & 0x38);
        }
        break;

    case MMU::IORegister::NR30:
        if (access_type == AccessType::Write) {
            // Bits 0-6 are unused (and unwriteable).
            *data = (*data & 0x80) | (get_io_register(MMU::IORegister::NR30) & 0x7F);
        }
        break;

    case MMU::IORegister::NR32:
        if (access_type == AccessType::Write) {
            // Bits 0-4 and bit 7 are unused (and unwriteable).
            *data = (*data & 0x60) | (get_io_register(MMU::IORegister::NR32) & 0x9F);
        }
        break;

    case MMU::IORegister::NR34:
        if (access_type == AccessType::Write) {
            // Bits 3-5 are unused (and unwriteable).
            *data = (*data & 0xC7) | (get_io_register(MMU::IORegister::NR34) & 0x38);
        }
        break;

    case MMU::IORegister::NR41:
        if (access_type == AccessType::Write) {
            // Bits 6-7 are unused (and unwriteable).
            *data = (*data & 0x3F) | (get_io_register(MMU::IORegister::NR41) & 0xC0);
        }
        break;

    case MMU::IORegister::NR44:
        if (access_type == AccessType::Write) {
            // Bits 0-5 are unused (and unwriteable).
            *data = (*data & 0xC0) | (get_io_register(MMU::IORegister::NR44) & 0x3F);
        }
        break;

    case MMU::IORegister::NR52:
        if (access_type == AccessType::Write) {
            // Bits 0-3 are readonly and bits 4-6 are unused (and unwriteable).
            *data = (*data & 0x80) | (get_io_register(MMU::IORegister::NR52) & 0x7F);
        }
        break;

    case MMU::IORegister::STAT:
        if (access_type == AccessType::Write) {
            // Bit 7 is unused (and unwriteable).
            *data = (*data & 0x7F) | (get_io_register(MMU::IORegister::STAT) & 0x80);
        }
        break;

    case MMU::IORegister::LY:
        if (access_type == AccessType::Write) {
            // LY is read-only.
            *data = get_io_register(MMU::IORegister::LY);
        }
        break;

    case MMU::IORegister::BootRomDisableOffset:
        if (access_type == AccessType::Write) {
            // Outside of the boot rom, this register cannot be written to.
            // TODO: Ensure that the boot rom is allowed to write to this register.
            *data = get_io_register(MMU::IORegister::BootRomDisableOffset);
        }
        break;
    case MMU::IORegister::SB:
    case MMU::IORegister::TMA:
    case MMU::IORegister::NR11:
    case MMU::IORegister::NR12:
    case MMU::IORegister::NR13:
    case MMU::IORegister::NR21:
    case MMU::IORegister::NR22:
    case MMU::IORegister::NR23:
    case MMU::IORegister::NR31:
    case MMU::IORegister::NR33:
    case MMU::IORegister::NR42:
    case MMU::IORegister::NR43:
    case MMU::IORegister::NR50:
    case MMU::IORegister::NR51:
    case MMU::IORegister::LCDC:
    case MMU::IORegister::SCY:
    case MMU::IORegister::SCX:
    case MMU::IORegister::LYC:
    case MMU::IORegister::DMA:
    case MMU::IORegister::BGP:
    case MMU::IORegister::OBP0:
    case MMU::IORegister::OBP1:
    case MMU::IORegister::WY:
    case MMU::IORegister::WX:
    case MMU::IORegister::IE:
        // For all of these accesses, no pre-processing is necessary
        break;

    default:
        if (access_type == AccessType::Write) {
            // If we are here, the program is trying to write to unused registers within the IO register span.
            // Hence, we simply overwrite whatever data the program is trying to write with the data that's
            // already there, ensuring that nothing happens.
            read_from_memory(data, kRegisterOffsetBase | static_cast<std::uint16_t>(offset), 1U);
        }
    }
}

MMU::~MMU() {
    delete[] m_memory;
    if (has_cartridge()) {
        delete m_cartridge;
    }
}
}