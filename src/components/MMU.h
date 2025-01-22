#pragma once

#include "Cartridge.h"
#include "TimerController.h"
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <unordered_map>

namespace gbcemu {

class MMU final {
  public:
    enum class BootRomType : uint8_t {
        None,
        DMG,
    };

    enum class MemoryRegion : uint8_t {
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

    enum class IORegister : uint8_t {
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

    MMU(std::size_t const);

    bool try_load_boot_rom(std::ostream &stream, std::string const &);

    bool try_load_cartridge(std::ostream &stream, std::string const &);

    bool try_map_data_to_memory(std::uint8_t *const data, uint16_t const offset, std::size_t const size);

    bool try_read_from_memory(std::uint8_t *const data, uint16_t const offset, std::size_t const size) const;

    void set_debug_mode(bool const);

    MMU::BootRomType get_boot_rom_type() const;

    void set_io_register(MMU::IORegister const, uint8_t const);

    std::uint8_t get_io_register(MMU::IORegister const) const;

    bool has_cartridge() const;

    Cartridge *get_cartridge() const;

    void print_memory_at_location(std::ostream &stream, uint16_t const start, uint16_t const end) const;

    ~MMU();

  private:
    enum class AccessType {
        Read,
        Write,
    };

    std::size_t m_memory_size;
    uint8_t *m_memory;
    uint8_t *m_boot_rom;

    TimerController *m_timer_controller;
    static constexpr std::uint16_t kRegisterOffsetBase{ 0xFF00 };
    static constexpr std::uint16_t kDmgBootRomSize{ 0x0100 };

    bool m_loading_cartridge;
    Cartridge *m_cartridge{ nullptr };
    BootRomType m_boot_rom_type{ BootRomType::None };
    std::size_t m_boot_rom_size{ 0U };

    void initialize_registers();

    MMU::MemoryRegion find_memory_region(uint16_t const) const;

    void read_from_memory(std::uint8_t *, uint16_t const, std::size_t const) const;

    void write_to_memory(std::uint8_t *const, uint16_t const, std::size_t const);

    bool is_boot_rom_range(uint16_t const, std::size_t const) const;
    void read_from_boot_rom(uint8_t *, uint16_t, std::size_t const) const;
    bool try_load_from_file(std::string const &, uint8_t *, std::size_t const) const;

    std::string get_region_name(const MMU::MemoryRegion) const;
    std::string get_io_register_name(const MMU::IORegister) const;
    void pre_process_io_register_access(uint8_t const, const MMU::AccessType, uint8_t *data = nullptr) const;

    bool try_get_file_size(std::string const &, std::size_t *) const;

    static std::map<MMU::MemoryRegion, std::pair<std::uint16_t, std::uint16_t>> const s_region_map;
    static std::unordered_map<MMU::MemoryRegion, std::string> const s_region_names;
    static std::unordered_map<MMU::IORegister, std::string> const s_io_register_names;

    static std::pair<std::uint8_t, std::uint8_t> make_offset_and_size_pair(std::uint8_t offset, std::uint8_t size) { return std::make_pair(offset, size); }
    static std::pair<std::uint16_t, std::uint16_t> make_address_pair(std::uint16_t lower, std::uint16_t upper) { return std::make_pair(lower, upper); }
};
}