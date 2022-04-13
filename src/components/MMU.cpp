#include "MMU.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <fstream>
#include <stdexcept>
#include <utility>

namespace gbcemu {
MMU::MMU(uint32_t memory_size, bool load_boot_rom) : m_memory_size(memory_size) {
    m_memory = new uint8_t[m_memory_size];
    memset(m_memory, (uint8_t)0x00, m_memory_size);

    if (load_boot_rom) {
        auto boot_rom = new uint8_t[BootRomNumberOfBytes];
        bool success = try_load_boot_rom_from_file(boot_rom);
        if (!success) {
            log(LogLevel::Error, GeneralUtilities::formatted_string("Could not load boot rom from '%s'", BootRomPath));
            exit(1);
        }
        (void)try_map_data_to_memory(boot_rom, 0, BootRomNumberOfBytes);
        delete[] boot_rom;
    }
}

bool MMU::try_map_data_to_memory(uint8_t *data, uint32_t offset, uint32_t size) {
    if (offset + size > m_memory_size)
        return false;

    auto region = find_memory_region(offset);

    for (auto i = 0; i < size; i++)
        m_memory[offset + i] = data[i];

    if (m_debug_is_on)
        log(LogLevel::Debug,
            GeneralUtilities::formatted_string("Mapped %d B to range 0x%x - 0x%x (%s)", size, offset, offset + size - 1, get_region_name(region)));
    return true;
}

bool MMU::try_read_from_memory(uint8_t *data, uint32_t offset, uint32_t size) {
    if (offset + size > m_memory_size)
        return false;

    auto region = find_memory_region(offset);

    for (int i = 0; i < size; i++)
        data[i] = m_memory[offset + i];

    if (m_debug_is_on) {
        log(LogLevel::Debug,
            GeneralUtilities::formatted_string("Read %d B from range 0x%x - 0x%x (%s)", size, offset, offset + size - 1, get_region_name(region)));
    }
    return true;
}

bool MMU::try_load_boot_rom_from_file(uint8_t *boot_rom) {
    std::ifstream boot_file(BootRomPath, std::ios::in | std::ios::binary);
    if (boot_file.fail())
        return false;

    boot_file.read((char *)boot_rom, BootRomNumberOfBytes);
    return true;
}

void MMU::log(LogLevel level, const std::string &message) const { LogUtilities::log(LoggerType::Mmu, level, message); }

void MMU::set_debug_mode(bool on_or_off) { m_debug_is_on = on_or_off; }

MMU::MemoryRegion MMU::find_memory_region(uint32_t address) const {
    for (auto const &entry : m_region_map) {
        if (address > entry.second.second)
            continue;
        return entry.first;
    }
    __builtin_unreachable();
}

const std::map<MMU::MemoryRegion, std::pair<uint32_t, uint32_t>> MMU::m_region_map = {
    { MMU::MemoryRegion::CartridgeFixed, MMU::make_address_pair(0x0000, 0x3FFF) },
    { MMU::MemoryRegion::CartridgeSwitchable, MMU::make_address_pair(0x4000, 0x7FFF) },
    { MMU::MemoryRegion::VRAMSwitchable, MMU::make_address_pair(0x8000, 0x9FFF) },
    { MMU::MemoryRegion::CartridgeRAM, MMU::make_address_pair(0xA000, 0xBFFF) },
    { MMU::MemoryRegion::WRAMFixed, MMU::make_address_pair(0xC000, 0xCFFF) },
    { MMU::MemoryRegion::WRAMSwitchable, MMU::make_address_pair(0xD000, 0xDFFF) },
    { MMU::MemoryRegion::Restricted1, MMU::make_address_pair(0xE000, 0xFDFF) },
    { MMU::MemoryRegion::SpriteAttributeTable, MMU::make_address_pair(0xFE00, 0xFEFF) },
    { MMU::MemoryRegion::Restricted2, MMU::make_address_pair(0xFF00, 0xFF7F) },
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
    { MMU::MemoryRegion::Restricted1, "Restricted1" },
    { MMU::MemoryRegion::SpriteAttributeTable, "SpriteAttributeTable" },
    { MMU::MemoryRegion::Restricted2, "Restricted2" },
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