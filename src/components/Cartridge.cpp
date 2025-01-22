#include "Cartridge.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <iomanip>
#include <iostream>

namespace gbcemu {

Cartridge::Cartridge(uint8_t *const raw_data, std::size_t const raw_size) : m_raw_data(raw_data), m_raw_size(raw_size) {
    m_type = static_cast<CartridgeType>(get_single_byte_header_field(Cartridge::HeaderField::CartridgeType));
    switch (m_type) {
    case Cartridge::CartridgeType::NO_MBC:
        m_current_bank_number = 0;
        break;
    case Cartridge::CartridgeType::MBC1:
        m_current_bank_number = 1;
        break;
    default:
        LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Cartridge MBC flag is '%s' - not supported", s_mbc_names.find(m_type)->second));
        exit(1);
    }
}

void Cartridge::read_from_cartridge_switchable(uint8_t *const data, uint32_t const offset, std::size_t const size) const {
    switch (m_type) {
    case Cartridge::CartridgeType::NO_MBC:
        for (std::size_t i{ 0 }; i < size; ++i)
            data[i] = m_raw_data[i + offset];
        break;
    case Cartridge::CartridgeType::MBC1:
        for (std::size_t i{ 0 }; i < size; ++i)
            data[i] = m_raw_data[i + offset + ((m_current_bank_number - 1) * 0x4000)];
        break;
    default:
        LogUtilities::log_error(std::cout, "MBCs not yet supported");
        exit(1);
    }
}

void Cartridge::read_from_cartridge_ram(uint8_t *const, uint32_t const offset, std::size_t const) const {
    LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Reading from cartridge RAM not yet supported (tried to read from 0x%04X)", offset));
    exit(1);
}

void Cartridge::write_to_cartridge_registers(uint8_t *const data, uint16_t const offset, uint16_t const size) {
    if (offset >= 0x2000 && offset <= 0x3FFF && size == 0x01)
        m_current_bank_number = ((data[0] & 0x1F) == 0x00) ? 0x01 : data[0] & 0x1F;
}

void Cartridge::write_to_cartridge_ram(uint8_t *const, uint16_t const offset, std::size_t const) {
    LogUtilities::log_error(std::cout, GeneralUtilities::formatted_string("Writing to cartridge RAM not yet supported (tried to write to 0x%04X)", offset));
    exit(1);
}

std::string Cartridge::get_title() const { return read_string_from_header(TitleStart, TitleEnd); }
std::string Cartridge::get_manufacturer_code() const { return read_string_from_header(ManufacturerCodeStart, ManufacturerCodeEnd); }

uint8_t Cartridge::get_single_byte_header_field(Cartridge::HeaderField const field) const {
    if (field == Cartridge::HeaderField::NewLicenseeCode || field == Cartridge::HeaderField::GlobalChecksum) {
        return 0x00;
    } else {
        return m_raw_data[static_cast<uint16_t>(field)];
    }
}

uint16_t Cartridge::get_two_byte_header_field(Cartridge::HeaderField const field) const {
    if (field == Cartridge::HeaderField::NewLicenseeCode || field == Cartridge::HeaderField::GlobalChecksum) {
        return m_raw_data[static_cast<uint16_t>(field)] << 8 | m_raw_data[static_cast<uint16_t>(field) + 1];
    } else {
        return 0x0000;
    }
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

std::string Cartridge::read_string_from_header(uint16_t const start_offset, uint16_t const end_offset) const {
    std::size_t const buffer_size{ static_cast<std::size_t>((end_offset - start_offset) + 1U) };
    char *buffer = new char[buffer_size + 1U];

    for (std::size_t i{ 0 }; i < buffer_size; ++i)
        buffer[i] = m_raw_data[start_offset + i];

    buffer[buffer_size] = '\0';
    std::string const result{ std::string(buffer) };
    delete[] buffer;
    return result;
}
}