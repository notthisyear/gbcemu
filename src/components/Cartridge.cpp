#include "Cartridge.h"

namespace gbcemu {

Cartridge::Cartridge(uint8_t *raw_data, uint64_t size) : m_raw_data(raw_data), m_size(size) {
    auto title_buffer_size = (m_header_offset.TitleEnd - m_header_offset.TitleStart) + 1;
    char *title = new char[title_buffer_size];

    for (auto i = 0; i < title_buffer_size; i++)
        title[i] = raw_data[m_header_offset.TitleStart + i];

    m_title = std::string(title);
    delete[] title;
}

void Cartridge::read_from_cartridge_switchable(uint8_t *data, uint32_t offset, uint64_t size) const {}
void Cartridge::read_from_cartridge_ram(uint8_t *data, uint32_t offset, uint64_t size) const {}
void Cartridge::write_to_cartridge_ram(uint8_t *, uint32_t, uint64_t) {}

Cartridge::~Cartridge() { delete[] m_raw_data; }

}