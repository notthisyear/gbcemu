#include "Cartridge.h"
#include <exception>
#include <stdint.h>

namespace gbcemu {

Cartridge::Cartridge(uint8_t *raw_data, uint64_t size) : m_raw_data(raw_data), m_size(size) {}

void Cartridge::read_from_cartridge_switchable(uint8_t *data, uint32_t offset, uint64_t size) const {}
void Cartridge::read_from_cartridge_ram(uint8_t *data, uint32_t offset, uint64_t size) const {}
void Cartridge::write_to_cartridge_ram(uint8_t *, uint32_t, uint64_t) {}

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

std::string Cartridge::read_string_from_header(uint16_t start_offset, uint16_t end_offset) const {
    auto buffer_size = (end_offset - start_offset) + 1;
    char *buffer = new char[buffer_size];

    for (auto i = 0; i < buffer_size; i++)
        buffer[i] = m_raw_data[TitleStart + i];

    auto result = std::string(buffer);
    delete[] buffer;
    return result;
}

Cartridge::~Cartridge() { delete[] m_raw_data; }

}