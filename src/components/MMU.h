#pragma once

#include "util/LogUtilities.h"
#include <map>
#include <string>

namespace gbcemu {

class MMU {
  public:
    enum class MemoryRegion {
        CartridgeFixed,
        CartridgeSwitchable,
        VRAMSwitchable,
        CartridgeRAM,
        WRAMFixed,
        WRAMSwitchable,
        SpriteAttributeTable,
        IORegisters,
        HRAM,
        IERegister,
        Restricted1,
        Restricted2,
    };

    enum class IORegister {
        Joypad,
        SerialTranfer,
        TimerAndDivider,
        Sound,
        WavePattern,
        LCDControl,
        VRAMBankSelect,
        DisableBootRom,
        VRAMDMA,
        BgObjPalettes,
        WRAMBankSelect,
    };

    MMU(uint32_t memory_size, bool load_boot_rom = true);
    bool try_map_data_to_memory(uint8_t *data, uint32_t offset, uint32_t size);
    bool try_read_from_memory(uint8_t *data, uint32_t offset, uint32_t size);
    void set_debug_mode(bool on_or_off);
    ~MMU();

  private:
    const std::string BootRomPath = "C:\\programming\\gbcemu\\resources\\dmg_rom.bin";
    const uint32_t BootRomNumberOfBytes = 256;

    uint32_t m_memory_size;
    uint8_t *m_memory;

    MMU::MemoryRegion find_memory_region(uint32_t address) const;
    bool m_debug_is_on;

    bool try_load_boot_rom_from_file(uint8_t *boot_rom);
    void log(LogLevel level, const std::string &message) const;
    std::string get_region_name(MMU::MemoryRegion) const;
    std::string get_io_register_name(MMU::IORegister) const;

    static const std::map<MMU::MemoryRegion, std::pair<uint32_t, uint32_t>> m_region_map;
    static const std::map<MMU::IORegister, std::pair<uint8_t, uint8_t>> m_io_register_map;
    static const std::unordered_map<MMU::MemoryRegion, std::string> m_region_names;
    static const std::unordered_map<MMU::IORegister, std::string> m_io_register_names;

    static std::pair<uint8_t, uint8_t> make_offset_and_size_pair(uint8_t offset, uint8_t size) { return std::make_pair(offset, size); }
    static std::pair<uint32_t, uint32_t> make_address_pair(uint32_t lower, uint32_t upper) { return std::make_pair(lower, upper); }
};
}