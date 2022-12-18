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
Tests can be found [here](https://github.com/Gekkio/mooneye-test-suite).

### Acceptance - Timer
| Test name | Status | Comment |
| ----------- | -------- | --- |
| `div_write` | &#x2705; |
| `rapid_toggle` | &#x274c; | *Assertion of register `C` failed (expected `D9`, was `D8`)*
| `tim00` | &#x2705; |
| `tim00_div_trigger` | &#x2705; |
| `tim01` | &#x2705; |
| `tim01_div_trigger` | &#x2705; |
| `tim10` | &#x2705; |
| `tim10_div_trigger` | &#x2705; |
| `tim11` | &#x2705; |
| `tim11_div_trigger` | &#x2705; |
| `tima_reload` | &#x2705; |
| `tima_write_reloading` | &#x274c; | *Assertion of register `C` failed (expected `FE`, was `7F`)*
| `tma_write_reloading` | &#x274c; | *Assertion of register `E` failed (expected `7F`, was `FE`)*