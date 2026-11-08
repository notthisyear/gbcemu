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

When a test fails, its result screen shows which assertions failed. A failed assertion shows the *expected* value followed by `!`, and the register dump above it shows the actual values.

### Acceptance
| Test name | Status | Comment |
| ----------- | -------- | --- |
| `add_sp_e_timing` | &#x2705; |
| `boot_div-dmg0` | &#x274c; | *Expects the DMG0 model, we emulate DMG ABC*
| `boot_div-dmgABCmgb` | &#x2705; |
| `boot_div-S` | &#x274c; | *Expects the SGB/SGB2 models, we emulate DMG ABC*
| `boot_div2-S` | &#x274c; | *Expects the SGB/SGB2 models, we emulate DMG ABC*
| `boot_hwio-dmg0` | &#x274c; | *Expects the DMG0 model, we emulate DMG ABC*
| `boot_hwio-dmgABCmgb` | &#x2705; |
| `boot_hwio-S` | &#x274c; | *Expects the SGB/SGB2 models, we emulate DMG ABC*
| `boot_regs-dmg0` | &#x274c; | *Expects the DMG0 model, we emulate DMG ABC*
| `boot_regs-dmgABC` | &#x2705; |
| `boot_regs-mgb` | &#x274c; | *Expects the MGB model, we emulate DMG ABC*
| `boot_regs-sgb` | &#x274c; | *Expects the SGB model, we emulate DMG ABC*
| `boot_regs-sgb2` | &#x274c; | *Expects the SGB2 model, we emulate DMG ABC*
| `call_cc_timing` | &#x2705; |
| `call_cc_timing2` | &#x2705; |
| `call_timing` | &#x2705; |
| `call_timing2` | &#x2705; |
| `di_timing-GS` | &#x2705; |
| `div_timing` | &#x2705; |
| `ei_sequence` | &#x2705; |
| `ei_timing` | &#x2705; |
| `halt_ime0_ei` | &#x2705; |
| `halt_ime0_nointr_timing` | &#x2705; |
| `halt_ime1_timing` | &#x2705; |
| `halt_ime1_timing2-GS` | &#x2705; |
| `if_ie_registers` | &#x2705; |
| `intr_timing` | &#x2705; |
| `jp_cc_timing` | &#x2705; |
| `jp_timing` | &#x2705; |
| `ld_hl_sp_e_timing` | &#x2705; |
| `oam_dma_restart` | &#x2705; |
| `oam_dma_start` | &#x2705; |
| `oam_dma_timing` | &#x2705; |
| `pop_timing` | &#x2705; |
| `push_timing` | &#x2705; |
| `rapid_di_ei` | &#x2705; |
| `ret_cc_timing` | &#x2705; |
| `ret_timing` | &#x2705; |
| `reti_intr_timing` | &#x2705; |
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
| `timer` / `tima_write_reloading` | &#x2705; |
| `timer` / `tma_write_reloading` | &#x2705; |
| `ppu` / `hblank_ly_scx_timing-GS` | &#x274c; | *Times out (never reports a result)*
| `ppu` / `intr_1_2_timing-GS` | &#x274c; | *Times out (never reports a result)*
| `ppu` / `intr_2_0_timing` | &#x274c; | *Times out (never reports a result)*
| `ppu` / `intr_2_mode0_timing` | &#x274c; | *Times out (never reports a result)*
| `ppu` / `intr_2_mode0_timing_sprites` | &#x274c; | *Times out (never reports a result)*
| `ppu` / `intr_2_mode3_timing` | &#x274c; | *Times out (never reports a result)*
| `ppu` / `intr_2_oam_ok_timing` | &#x274c; | *Times out (never reports a result)*
| `ppu` / `lcdon_timing-GS` | &#x274c; |
| `ppu` / `lcdon_write_timing-GS` | &#x274c; |
| `ppu` / `stat_irq_blocking` | &#x274c; |
| `ppu` / `stat_lyc_onoff` | &#x274c; |
| `ppu` / `vblank_stat_intr-GS` | &#x274c; | *Times out (never reports a result)*
| `serial` / `boot_sclk_align-dmgABCmgb` | &#x274c; | *Serial transfers not implemented yet*

We haven't looked into the PPU and serial categories yet, so the failing tests there have no failure details.