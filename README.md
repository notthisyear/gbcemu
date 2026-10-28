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
| `mem_timing` | &#x2705; |
| `mem_timing-02` | &#x274c; | *`MBC1+RAM+BATTERY` MBC not implemented yet*
| `oam_bug` | &#x274c; | *`MBC1+RAM+BATTERY` MBC not implemented yet*
| `halt_bug` | &#x274c; | *`MBC1+RAM` MBC not implemented yet*

## Mooneye test suite
Tests can be found [here](https://github.com/Gekkio/mooneye-test-suite). We do fail quite a bit of tests at the moment. A small comfort is that some emulators that work well in game also seems to fail a few of these tests. However, there are a few things to address...

### Acceptance
| Test name | Status | Comment |
| ----------- | -------- | --- |
| `add_sp_e_timing` | &#x2705; |
| `boot_div-dmgABCmgb` | &#x2705; |
| `boot_hwio-dmgABCmgb` | &#x2705; |
| `boot_regs-dmgABC` | &#x2705; |
| `call_cc_timing` | &#x2705; |
| `call_cc_timing2` | &#x2705; |
| `call_timing` | &#x2705; |
| `call_timing2` | &#x2705; |
| `di_timing-GS` | &#x274c; | *Fails in round 1.*
| `div_timing` | &#x2705; |
| `ei_sequence` | &#x274c; | *Fails on assertion on register `C`.*
| `ei_timing` | &#x2705; |
| `halt_ime0_ei` | &#x2705; |
| `halt_ime0_nointr_timing` | &#x274c; | *Fails on assertions on registers `D` and `E`.*
| `halt_ime1_timing` | &#x2705; |
| `halt_ime1_timing2-GS` | &#x274c; | *Fails in round 1.*
| `if_ie_registers` | &#x2705; |
| `intr_timing` | &#x274c; | *Fails on assertion on register `D`.*
| `jp_cc_timing` | &#x2705; |
| `jp_timing` | &#x2705; |
| `ld_hl_sp_e_timing` | &#x2705; |
| `oam_dma_restart` | &#x2705; |
| `oam_dma_start` | &#x2705; |
| `oam_dma_timing` | &#x2705; |
| `pop_timing` | &#x2705; |
| `push_timing` | &#x2705; |
| `rapid_di_ei` | &#x274c; | *Fails on assertions on registers `B` and `D`.*
| `ret_cc_timing` | &#x2705; |
| `ret_timing` | &#x2705; |
| `reti_intr_timing` | &#x274c; | *Fails on assertion on register `B`.*
| `reti_timing` | &#x2705; |
| `rst_timing` | &#x2705; |
| `bits` / `mem_oam` | &#x2705; |
| `bits` / `reg_f` | &#x2705; |
| `bits` / `unused_hwio-GS` | &#x2705; |
| `instr` / `daa` | &#x2705; |
| `interrupts` / `ie_push` | &#x2705; |
| `oam_dma` / `basic` | &#x2705; |
| `oam_dma` / `reg_read` | &#x2705; |
| `oam_dma` / `sources-GS` | &#x274c; | *`MBC5+RAM+BATTERY` MBC not implemented yet*
| `timer` / `div_write` | &#x2705; |
| `timer` / `rapid_toggle` | &#x2705; |
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

Regarding the PPU and serial categories, we haven't looked at those yet.