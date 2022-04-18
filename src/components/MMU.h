#pragma once

#include "Cartridge.h"
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
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
        EchoRAM,
        Restricted,
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

    MMU(uint16_t memory_size);

    bool try_load_boot_rom(std::ostream &stream, const std::string &);
    bool try_load_cartridge(std::ostream &stream, const std::string &);

    bool try_map_data_to_memory(uint8_t *data, uint16_t offset, uint16_t size);
    bool try_read_from_memory(uint8_t *data, uint16_t offset, uint64_t size) const;
    void set_debug_mode(bool on_or_off);
    void set_in_boot_mode(bool is_in_boot_mode);

    void print_memory_at_location(std::ostream &stream, uint16_t start, uint16_t end) const;
    ~MMU();

  private:
    uint16_t m_memory_size;
    uint8_t *m_memory;
    uint8_t *m_boot_rom;

    std::unique_ptr<Cartridge> m_cartridge;
    MMU::MemoryRegion find_memory_region(uint16_t address) const;

    bool m_is_in_boot_mode;
    bool m_loading_cartridge;
    uint64_t m_boot_rom_size;

    void read_from_memory(uint8_t *data, uint16_t offset, uint16_t size) const {
        for (auto i = 0; i < size; i++)
            data[i] = m_memory[offset + i];
    }

    void write_to_memory(uint8_t *data, uint16_t offset, uint16_t size) {
        for (auto i = 0; i < size; i++)
            m_memory[offset + i] = data[i];
    }

    bool is_boot_rom_range(uint16_t offset, uint64_t size) const;
    void read_from_boot_rom(uint8_t *, uint16_t, uint64_t) const;
    bool try_load_from_file(const std::string &, uint8_t *, const uint64_t) const;

    std::string get_region_name(MMU::MemoryRegion) const;
    std::string get_io_register_name(MMU::IORegister) const;

    bool get_file_size(const std::string &, uint64_t *) const;

    static const std::map<MMU::MemoryRegion, std::pair<uint16_t, uint16_t>> s_region_map;
    static const std::map<MMU::IORegister, std::pair<uint8_t, uint8_t>> s_io_register_map;
    static const std::unordered_map<MMU::MemoryRegion, std::string> s_region_names;
    static const std::unordered_map<MMU::IORegister, std::string> s_io_register_names;

    static std::pair<uint8_t, uint8_t> make_offset_and_size_pair(uint8_t offset, uint8_t size) { return std::make_pair(offset, size); }
    static std::pair<uint16_t, uint16_t> make_address_pair(uint16_t lower, uint16_t upper) { return std::make_pair(lower, upper); }
};
}