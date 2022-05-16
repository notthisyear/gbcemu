#include "Cartridge.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdint.h>

namespace gbcemu {

Cartridge::Cartridge(uint8_t *raw_data, uint64_t raw_size) : m_raw_data(raw_data), m_raw_size(raw_size) {
    m_type = static_cast<CartridgeType>(get_single_byte_header_field(Cartridge::HeaderField::CartridgeType));
    if (m_type != Cartridge::CartridgeType::NO_MBC && m_type != Cartridge::CartridgeType::MBC1) {
        LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Cartridge MBC flag is '%s' - not supported", s_mbc_names.find(m_type)->second));
        exit(1);
    }

    switch (m_type) {
    case Cartridge::CartridgeType::NO_MBC:
        m_current_bank_number = 0;
        break;
    case Cartridge::CartridgeType::MBC1:
        m_current_bank_number = 1;
        break;
    default:
        __builtin_unreachable();
    }
}

void Cartridge::read_from_cartridge_switchable(uint8_t *data, uint32_t offset, uint64_t size) const {
    switch (m_type) {
    case Cartridge::CartridgeType::NO_MBC:
        for (auto i = 0; i < size; i++)
            data[i] = m_raw_data[i + offset];
        break;
    case Cartridge::CartridgeType::MBC1:
        for (auto i = 0; i < size; i++)
            data[i] = m_raw_data[i + offset + ((m_current_bank_number - 1) * 0x4000)];
        break;
    default:
        LogUtilities::log_error(std::cout, "MBCs not yet supported");
        exit(1);
    }
}

void Cartridge::read_from_cartridge_ram(uint8_t *data, uint32_t offset, uint64_t size) const {
    LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Reading from cartridge RAM not yet supported (tried to read from 0x%04X)", offset));
    exit(1);
}

void Cartridge::write_to_cartridge_registers(uint8_t *data, uint16_t offset, uint16_t size) {
    if (offset >= 0x2000 && offset <= 0x3FFF && size == 0x01)
        m_current_bank_number = ((data[0] & 0x1F) == 0x00) ? 0x01 : data[0] & 0x1F;
}

void Cartridge::write_to_cartridge_ram(uint8_t *, uint16_t offset, uint16_t) {
    LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Writing to cartridge RAM not yet supported (tried to write to 0x%04X)", offset));
    exit(1);
}

std::string Cartridge::get_title() const { return read_string_from_header(TitleStart, TitleEnd); }
std::string Cartridge::get_manufacturer_code() const { return read_string_from_header(ManufacturerCodeStart, ManufacturerCodeEnd); }

uint8_t Cartridge::get_single_byte_header_field(const Cartridge::HeaderField field) const {
    if (field == Cartridge::HeaderField::NewLicenseeCode || field == Cartridge::HeaderField::GlobalChecksum)
        return 0x00;
    else
        return m_raw_data[static_cast<uint16_t>(field)];
}

uint16_t Cartridge::get_two_byte_header_field(const Cartridge::HeaderField field) const {
    if (field != Cartridge::HeaderField::NewLicenseeCode && field != Cartridge::HeaderField::GlobalChecksum)
        return 0x0000;
    else
        return m_raw_data[static_cast<uint16_t>(field)] << 8 | m_raw_data[static_cast<uint16_t>(field) + 1];
}

void Cartridge::print_info(std::ostream &stream) const {
    stream << std::endl;

    stream << std::left << std::setw(30) << std::setfill(' ') << "\033[0;36mTitle:";
    stream << "\033[1;37m" << get_title() << std::endl;

    stream << std::left << std::setw(30) << std::setfill(' ') << "\033[0;36mManufacturer code:";
    stream << "\033[1;37m" << get_manufacturer_code() << std::endl;

    auto rom_size = s_rom_sizes.find(get_single_byte_header_field(Cartridge::HeaderField::ROMSize))->second;
    stream << std::left << std::setw(30) << std::setfill(' ') << "\033[0;36mROM size:";
    stream << "\033[1;37m" << unsigned(rom_size) << " B (" << unsigned(rom_size >> 14) << " banks)" << std::endl;

    stream << std::left << std::setw(30) << std::setfill(' ') << "\033[0;36mRAM size:";
    stream << "\033[1;37m" << unsigned(s_ram_sizes.find(get_single_byte_header_field(Cartridge::HeaderField::RAMSize))->second) << " B" << std::endl;

    stream << std::left << std::setw(30) << std::setfill(' ') << "\033[0;36mMBC setting:";
    stream << "\033[1;37m" << s_mbc_names.find(m_type)->second << "\033[0m" << std::endl;

    if (m_type != Cartridge::CartridgeType::NO_MBC) {
        stream << std::left << std::setw(30) << std::setfill(' ') << "\033[0;36mCurrent ROM bank:";
        stream << "\033[1;37m" << unsigned(m_current_bank_number) << "\033[0m" << std::endl;
    }
}

Cartridge::~Cartridge() { delete[] m_raw_data; }

std::string Cartridge::read_string_from_header(uint16_t start_offset, uint16_t end_offset) const {
    auto buffer_size = (end_offset - start_offset) + 1;
    char *buffer = new char[buffer_size + 1];

    for (auto i = 0; i < buffer_size; i++)
        buffer[i] = m_raw_data[start_offset + i];

    buffer[buffer_size] = '\0';
    auto result = std::string(buffer);
    delete[] buffer;
    return result;
}

const std::unordered_map<Cartridge::CartridgeType, std::string> Cartridge::s_mbc_names = {
    { Cartridge::CartridgeType::NO_MBC, "ROM ONLY" },
    { Cartridge::CartridgeType::MBC1, "MBC1" },
    { Cartridge::CartridgeType::MBC1_RAM, "MBC1 + RAM" },
    { Cartridge::CartridgeType::MBC1_RAM_BATTERY, "MBC + RAM + BATTERY" },
    { Cartridge::CartridgeType::MBC2, "MBC2" },
    { Cartridge::CartridgeType::MBC2_BATTERY, "MBC2 + BATTERY" },
    { Cartridge::CartridgeType::ROM_RAM, "ROM + RAM" },
    { Cartridge::CartridgeType::ROM_RAM_BATTERY, "ROM + RAM + BATTERY" },
    { Cartridge::CartridgeType::MMM01, "MMM01" },
    { Cartridge::CartridgeType::MMM01_RAM, "MMM01 + RAM" },
    { Cartridge::CartridgeType::MMM01_RAM_BATTERY, "MMM01 + RAM + BATTERY" },
    { Cartridge::CartridgeType::MBC3_TIMER_BATTERY, "MBC3 + TIMER + BATTERY" },
    { Cartridge::CartridgeType::MBC3_TIMER_RAM_BATTERY, "MBC3 + TIMER + RAM + BATTERY" },
    { Cartridge::CartridgeType::MBC3, "MBC3" },
    { Cartridge::CartridgeType::MBC3_RAM, "MBC3 + RAM" },
    { Cartridge::CartridgeType::MBC3_RAM_BATTERY, "MBC3 + RAM + BATTERY" },
    { Cartridge::CartridgeType::MBC5, "MBC5" },
    { Cartridge::CartridgeType::MBC5_RAM, "MBC5 + RAM" },
    { Cartridge::CartridgeType::MBC5_RAM_BATTERY, "MBC5 + RAM + BATTERY" },
    { Cartridge::CartridgeType::MBC5_RUMBLE, "MBC5 + RUMBLE" },
    { Cartridge::CartridgeType::MBC5_RUMBLE_RAM, "MBC5 + RUMBLE + RAM" },
    { Cartridge::CartridgeType::MBC5_RUMBLE_RAM_BATTERY, "MBC5 + RUMBLE + RAM + BATTERY" },
    { Cartridge::CartridgeType::MBC6, "MBC6" },
    { Cartridge::CartridgeType::MBC7_SENSOR_RUMBLE_RAM_BATTERY, "MBC7 + SENSOR + RUMBLE + RAM + BATTERY" },
    { Cartridge::CartridgeType::POCKET_CAMERA, "POCKET CAMERA" },
    { Cartridge::CartridgeType::BANDAI_TAMA5, "BANDAI TAMA5" },
    { Cartridge::CartridgeType::HuC3, "HuC3" },
    { Cartridge::CartridgeType::HuC1_RAM_BATTERY, "HuC1 + RAM + BATTERY" },
};

const std::unordered_map<uint8_t, uint32_t> Cartridge::s_rom_sizes = {
    { 0x00, RomBaseSizeBytes },      { 0x01, RomBaseSizeBytes << 1 }, { 0x02, RomBaseSizeBytes << 2 },
    { 0x03, RomBaseSizeBytes << 3 }, { 0x04, RomBaseSizeBytes << 4 }, { 0x05, RomBaseSizeBytes << 5 },
    { 0x06, RomBaseSizeBytes << 6 }, { 0x07, RomBaseSizeBytes << 7 }, { 0x08, RomBaseSizeBytes << 8 },
};
const std::unordered_map<uint8_t, uint32_t> Cartridge::s_ram_sizes = {
    { 0x00, 0 }, { 0x02, 8 * 0x400 }, { 0x03, 32 * 0x400 }, { 0x04, 128 * 0x400 }, { 0x05, 64 * 0x400 },
};
}