#pragma once

#include "Cartridge.h"
#include "TimerController.h"
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <stdint.h>
#include <string>
#include <unordered_map>

namespace gbcemu {

class MMU {
  public:
    enum class BootRomType {
        None,
        DMG,
    };

    enum class MemoryRegion {
        CartridgeFixed,
        CartridgeSwitchable,
        VRAMSwitchable,
        CartridgeRAMSwitchable,
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
        // Joypad input
        JOYP = 0x00,
        // Serial data transfer
        SB = 0x01,
        SC = 0x02,
        // Timer
        DIV = 0x04,
        TIMA = 0x05,
        TMA = 0x06,
        TAC = 0x07,
        // Sound controller
        NR10 = 0x10,
        NR11 = 0x11,
        NR12 = 0x12,
        NR13 = 0x13,
        NR14 = 0x14,
        NR21 = 0x16,
        NR22 = 0x17,
        NR23 = 0x18,
        NR24 = 0x19,
        NR30 = 0x1A,
        NR31 = 0x1B,
        NR32 = 0x1C,
        NR33 = 0x1D,
        NR34 = 0x1E,
        NR41 = 0x20,
        NR42 = 0x21,
        NR43 = 0x22,
        NR44 = 0x23,
        NR50 = 0x24,
        NR51 = 0x25,
        NR52 = 0x26,
        // PPU
        LCDC = 0x40,
        STAT = 0x41,
        SCY = 0x42,
        SCX = 0x43,
        LY = 0x44,
        LYC = 0x45,
        DMA = 0x46,
        BGP = 0x47,
        OBP0 = 0x48,
        OBP1 = 0x49,
        WY = 0x4A,
        WX = 0x4B,
        // Interrupt controller
        IF = 0x0F,
        IE = 0xFF,
        // CGB (TODO)

        // Other
        BootRomDisableOffset = 0x50,
    };

    MMU(uint16_t);

    bool try_load_boot_rom(std::ostream &stream, const std::string &);

    bool try_load_cartridge(std::ostream &stream, const std::string &);

    bool try_map_data_to_memory(uint8_t *data, uint16_t offset, uint16_t size);

    bool try_read_from_memory(uint8_t *data, uint16_t offset, uint64_t size) const;

    void set_debug_mode(bool);

    MMU::BootRomType get_boot_rom_type() const;

    void set_io_register(const MMU::IORegister, const uint8_t);

    uint8_t get_io_register(const MMU::IORegister) const;

    bool has_cartridge() const;

    Cartridge *get_cartridge() const;

    void print_memory_at_location(std::ostream &stream, uint16_t start, uint16_t end) const;

    ~MMU();

  private:
    enum class AccessType {
        Read,
        Write,
    };
    uint16_t m_memory_size;
    uint8_t *m_memory;
    uint8_t *m_boot_rom;

    TimerController *m_timer_controller;
    const uint16_t RegisterOffsetBase = 0xFF00;
    const uint16_t DmgBootRomSize = 0x0100;

    Cartridge *m_cartridge;
    MMU::MemoryRegion find_memory_region(uint16_t) const;

    bool m_loading_cartridge;
    MMU::BootRomType m_boot_rom_type;
    uint64_t m_boot_rom_size;

    void read_from_memory(uint8_t *, uint16_t, uint16_t) const;

    void write_to_memory(uint8_t *, uint16_t, uint16_t);

    bool is_boot_rom_range(uint16_t, uint64_t) const;
    void read_from_boot_rom(uint8_t *, uint16_t, uint64_t) const;
    bool try_load_from_file(const std::string &, uint8_t *, const uint64_t) const;

    std::string get_region_name(MMU::MemoryRegion) const;
    std::string get_io_register_name(MMU::IORegister) const;
    void pre_process_io_register_access(uint8_t, AccessType, uint8_t *data = nullptr) const;

    bool get_file_size(const std::string &, uint64_t *) const;

    static const std::map<MMU::MemoryRegion, std::pair<uint16_t, uint16_t>> s_region_map;
    static const std::unordered_map<MMU::MemoryRegion, std::string> s_region_names;
    static const std::unordered_map<MMU::IORegister, std::string> s_io_register_names;

    static std::pair<uint8_t, uint8_t> make_offset_and_size_pair(uint8_t offset, uint8_t size) { return std::make_pair(offset, size); }
    static std::pair<uint16_t, uint16_t> make_address_pair(uint16_t lower, uint16_t upper) { return std::make_pair(lower, upper); }
};
}