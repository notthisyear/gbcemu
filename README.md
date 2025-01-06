This is an attempt at a GB/GBC emulator. Work in progress.

# Test status
There are a number of hardware tests available that the emulator has been tested against. Below is the current test status.

## Blargg's hardware tests
Tests can be found [here](https://github.com/retrio/gb-test-roms).

| Test name | Status | Comment |
| ----------- | -------- | --- |
| `cgb_sound` | &#x274c; | *sound not implemented yet*
| `cpu_instr` | &#x2705; |
| `dmg_sound` | &#x274c; | *sound not implemented yet*
| `instr_timing` | &#x2705; |
| `interrupt_time` | &#x274c; | *`MBC1+RAM` MBC not implemented yet*
| `mem_timing` / `01-read_timing` | &#x274c; | *Instructions `F2`, `F0` and `F2` fails (all take one cycle too long)*
| `mem_timing` / `02-write_timing` | &#x274c; | *All tested instructions fails (all take one cycle too long)*
| `mem_timing` / `03-modify_timing` | &#x274c; | *All tested instructions fails (all writes takes one cycle too long)*
| `mem_timing-02` | &#x274c; | *`MBC1+RAM+BATTERY` MBC not implemented yet*
| `oam_bug` | &#x274c; | *`MBC1+RAM+BATTERY` MBC not implemented yet*

## Mooneye test suite
Tests can be found [here](https://github.com/Gekkio/mooneye-test-suite). We do fail quite a bit of tests at the moment. A small comfort is that some emulators that work well in game also seems to fail a few of these tests. However, there are a few things to address...

### Acceptance
| Test name | Status | Comment |
| ----------- | -------- | --- |
| `add_sp_e_timing` | &#x274c; | *Writing to cartridge RAM not yet supported.*
| `boot_div-dmgABCmgb` | &#x2705; |
| `boot_hwio-dmgABCmgb` | &#x2705; |
| `boot_regs-dmgABC` | &#x2705; |
| `call_cc_timing` | &#x274c; | *Fails in round 1.*
| `call_cc_timing2` | &#x274c; | *Fails assertions on register `BC` and `D`. Might be releated to OAM that we've not yet looked at.*
| `call_timing` | &#x274c; | *Fails in round 1.*
| `call_timing2` | &#x274c; | *Fails assertions on register `BC` and `D`. Might be releated to OAM that we've not yet looked at.*
| `di_timing-GS` | &#x274c; | *Fails in round 1.*
| `div_timing` | &#x2705; |
| `ei_sequence` | &#x274c; | *Crashes the emulator. Interesting.*
| `ei_timing.` | &#x274c; | *Crashes the emulator. Interesting.*
| `halt_ime0_ei` | &#x2705; |
| `halt_ime0_nointr_timing` | &#x274c; | *Fails on assertions on register `DE`.*
| `halt_ime1_timing` | &#x274c; | *Writing to cartridge RAM not yet supported.*
| `halt_ime1_timing2-GS` | &#x274c; | *Fails in round 1.*
| `if_ie_registers` | &#x274c; | *Crashes the emulator. Interesting.*
| `intr_timing` | &#x274c; | *Crashes the emulator. Interesting.*
| `jp_cc_timing` | &#x274c; | *Fails in round 1.*
| `jp_timing` | &#x274c; | *Fails in round 1.*
| `ld_hl_sp_e_timing` | &#x274c; | *Fails on assertions on register `BC`.*
| `oam_dma_restart` | &#x274c; | *Quite expected, as we haven't looked at DMA/OAM yet.*
| `oam_dma_start` | &#x274c; | *Quite expected, as we haven't looked at DMA/OAM yet.*
| `oam_dma_timing` | &#x274c; | *Quite expected, as we haven't looked at DMA/OAM yet.*
| `pop_timing` | &#x274c; | *Fails on assertions on register `E`.*
| `push_timing` | &#x274c; | *Fails on assertions on register `D`.*
| `rapid_di_ei` | &#x274c; | *Crashes the emulator. Interesting.*
| `ret_cc_timing` | &#x274c; | *Writing to cartridge RAM not yet supported.*
| `ret_timing` | &#x274c; | *Writing to cartridge RAM not yet supported.*
| `reti_intr_timing` | &#x274c; | *Writing to cartridge RAM not yet supported.*
| `reti_timing` | &#x274c; | *Writing to cartridge RAM not yet supported.*
| `rst_timing` | &#x274c; | *Quite expected, as we haven't really looked at RST yet.*
| `bits` / `mem_oam` | &#x2705; |
| `bits` / `reg_f` | &#x2705; |
| `bits` / `unused_hwio-GS` | &#x2705; |
| `instr` / `daa` | &#x2705; |
| `interrupts` / `ie_push` | &#x274c; | *Fails round 1, interrupt wasn't cancelled properly*
| `timer` / `div_write` | &#x2705; |
| `timer` / `rapid_toggle` | &#x274c; | *Assertion of register `C` failed (expected `D9`, was `D8`)*
| `timer` / `tim00` | &#x2705; |
| `timer` / `tim00_div_trigger` | &#x2705; |
| `timer` / `tim01` | &#x2705; |
| `timer` / `tim01_div_trigger` | &#x2705; |
| `timer` / `tim10` | &#x2705; |
| `timer` / `tim10_div_trigger` | &#x2705; |
| `timer` / `tim11` | &#x2705; |
| `timer` / `tim11_div_trigger` | &#x2705; |
| `timer` / `tima_reload` | &#x2705; |
| `timer` / `tima_write_reloading` | &#x274c; | *Assertion of register `C` failed (expected `FE`, was `7F`)*
| `timer` / `tma_write_reloading` | &#x274c; | *Assertion of register `E` failed (expected `7F`, was `FE`)*

Regarding the OAM DMA, PPU and serial categories, we haven't looked at those yet.