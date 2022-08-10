#pragma once

#include "CPU.h"
#include "MMU.h"
#include "components/Opcodes.h"
#include "util/BitUtilities.h"
#include "util/GeneralUtilities.h"
#include "util/LogUtilities.h"
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

#define NOT_IMPLEMENTED(a)                                                                                                                                     \
    std::cout << "Opcode '" << a << "' is not implemented!" << std::endl;                                                                                      \
    exit(1);

#define INVALID_OPCODE(a)                                                                                                                                      \
    throw std::runtime_error(GeneralUtilities::formatted_string("Opcode '0x%02X' is not valid!", a));                                                          \
    exit(1);

namespace gbcemu {

struct Opcode {

  public:
    uint8_t size;

    void tick_execution(CPU *cpu, MMU *mmu) {
        if (m_is_done) {
            LogUtilities::log_error(std::cout, "Cannot tick execution - instruction complete");
            exit(1);
        }

        m_operations.at(m_operation_step++)(cpu, mmu);

        if (m_operation_step == m_operations.size())
            m_is_done = true;
    }

    virtual std::string get_disassembled_instruction(uint8_t *instruction_data) const = 0;

    bool is_done() const { return m_is_done; }

    void reset_state() {
        m_operation_step = 0;
        m_is_done = false;
    }

    virtual ~Opcode() = default;

  protected:
    bool m_is_done = false;
    std::vector<std::function<void(CPU *, MMU *)>> m_operations;

    Opcode(uint8_t opcode_number_of_bytes) : size(opcode_number_of_bytes) {}

  private:
    uint8_t m_operation_step = 0;
};

// 0x00 - NoOp
struct NoOperation final : public Opcode {
  public:
    NoOperation() : Opcode(1) {
        m_operations.push_back([](CPU *, MMU *) {});
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "NOP"; }
};

// 0x08 Store SP at addresses given by 16-bit immediate
struct StoreStackpointer final : public Opcode {
  public:
    StoreStackpointer() : Opcode(3) {
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::W); });

        // 16-bit operation really takes two cycles, so wait one cycle and do the entire operation at the end
        // Also, operation involving the SP tend to take one extra cycle for some reason
        m_operations.push_back([](CPU *cpu, MMU *mmu) {});
        m_operations.push_back([](CPU *cpu, MMU *mmu) {});
        m_operations.push_back([](CPU *cpu, MMU *mmu) {
            uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);
            mmu->try_map_data_to_memory((uint8_t *)&sp, cpu->get_16_bit_register(CPU::Register::WZ), 2);
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        uint16_t value = instruction_data[1] << 8 | instruction_data[0];
        return GeneralUtilities::formatted_string("LD (0x%04X), SP", value);
    }
};

// 0x10 - Stops to CPU (very low power mode, can be used to switch between normal and double CPU speed on GBC)
struct Stop final : public Opcode {
  public:
    Stop() : Opcode(2) {
        m_operations.push_back([](CPU *, MMU *) { ; });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "STOP 0"; }
};

// 0x76 - Halts to CPU (low-power mode until interrupt)
struct Halt final : public Opcode {
  public:
    static const uint8_t opcode = 0x76;

    Halt() : Opcode(1) {
        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_cpu_to_halt(); });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "HALT"; }
};

// 0x27 - Decimal adjust accumulator (changes A to BCD representation)
struct DecimalAdjustAccumulator final : public Opcode {
  public:
    DecimalAdjustAccumulator() : Opcode(1) {
        m_operations.push_back([](CPU *cpu, MMU *) {
            auto last_operation_was_addition = !cpu->flag_is_set(CPU::Flag::N);
            auto acc = cpu->get_8_bit_register(CPU::Register::A);
            if (last_operation_was_addition) {
                if (cpu->flag_is_set(CPU::Flag::C) || acc > 0x99) {
                    acc += 0x60;
                    cpu->set_flag(CPU::Flag::C, true);
                }

                if (cpu->flag_is_set(CPU::Flag::H) || ((acc & 0x0F) > 0x09))
                    acc += 0x06;

            } else {
                if (cpu->flag_is_set(CPU::Flag::C))
                    acc -= 0x60;

                if (cpu->flag_is_set(CPU::Flag::H))
                    acc -= 0x06;
            }

            cpu->set_register(CPU::Register::A, acc);
            cpu->set_flag(CPU::Flag::Z, acc == 0x00);
            cpu->set_flag(CPU::Flag::H, false);
        });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "DAA"; }
};

// 0x37 - Set carry flag
struct SetCarryFlag final : public Opcode {
  public:
    SetCarryFlag() : Opcode(1) {
        m_operations.push_back([](CPU *cpu, MMU *) {
            cpu->set_flag(CPU::Flag::N, false);
            cpu->set_flag(CPU::Flag::H, false);
            cpu->set_flag(CPU::Flag::C, true);
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "SCF"; }
};

// 0x2F - One's complement the accumulator
struct InvertAccumulator final : public Opcode {
  public:
    InvertAccumulator() : Opcode(1) {
        m_operations.push_back([](CPU *cpu, MMU *) {
            cpu->set_register(CPU::Register::A, static_cast<uint8_t>(~cpu->get_8_bit_register(CPU::Register::A)));
            cpu->set_flag(CPU::Flag::N, true);
            cpu->set_flag(CPU::Flag::H, true);
        });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "CPL"; }
};

// 0x3F - Complement carry flag
struct ComplementCarryFlag final : public Opcode {
  public:
    ComplementCarryFlag() : Opcode(1) {
        m_operations.push_back([](CPU *cpu, MMU *) {
            cpu->set_flag(CPU::Flag::N, false);
            cpu->set_flag(CPU::Flag::H, false);
            cpu->set_flag(CPU::Flag::C, !cpu->flag_is_set(CPU::Flag::C));
        });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "CCF"; }
};

// 0xF3 - Disable interrupt
struct DisableInterrupt final : public Opcode {
  public:
    DisableInterrupt() : Opcode(1) {
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->set_interrupt_enable(false); });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "DI"; }
};

// 0xF3 Enable interrupt
struct EnableInterrupt final : public Opcode {
  public:
    EnableInterrupt() : Opcode(1) {
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->set_interrupt_enable(true); });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "EI"; }
};

// 0xF9 - Load HL into SP
struct LoadSPWithHL final : public Opcode {

  public:
    LoadSPWithHL() : Opcode(1) {
        m_operations.push_back([](CPU *cpu, MMU *) {});
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->set_register(CPU::Register::SP, cpu->get_16_bit_register(CPU::Register::HL)); });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override { return "LD SP, HL"; }
};

// Call, returns and jumps common
struct ConditionalCallReturnOrJumpBase : public Opcode {
  protected:
    ConditionalCallReturnOrJumpBase(uint8_t size) : Opcode(size) {}

    bool condition_is_met(CPU *cpu) const {
        switch (m_condition) {
        case ConditionalCallReturnOrJumpBase::Condition::None:
            return true;
        case ConditionalCallReturnOrJumpBase::Condition::Zero:
            return cpu->flag_is_set(CPU::Flag::Z);
        case ConditionalCallReturnOrJumpBase::Condition::NotZero:
            return !cpu->flag_is_set(CPU::Flag::Z);
        case ConditionalCallReturnOrJumpBase::Condition::Carry:
            return cpu->flag_is_set(CPU::Flag::C);
        case ConditionalCallReturnOrJumpBase::Condition::NotCarry:
            return !cpu->flag_is_set(CPU::Flag::C);
        default:
            __builtin_unreachable();
        }
    }

    void append_call_instructions() {
        // Wait one cycle (16-bit operation)
        m_operations.push_back([](CPU *, MMU *) {});

        // Push PC to stack and decrement stack pointer
        m_operations.push_back([](CPU *cpu, MMU *mmu) {
            uint16_t pc = cpu->get_16_bit_register(CPU::Register::PC);
            uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);
            (void)mmu->try_map_data_to_memory((uint8_t *)&pc, sp - 2, 2);
            cpu->set_register(CPU::Register::SP, static_cast<uint16_t>(sp - 2));
        });

        // Update PC
        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register_from_intermediate(CPU::Register::PC); });
    }

    void append_return_instructions(bool enable_interrupts) {
        // Wait one cycle (16-bit operation)
        m_operations.push_back([](CPU *, MMU *) {});

        // Get PC from stack and increment stack pointer
        m_operations.push_back([](CPU *cpu, MMU *mmu) {
            uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);
            uint8_t *data = new uint8_t[2];
            (void)mmu->try_read_from_memory(data, sp, 2);
            cpu->set_register(CPU::Register::WZ, static_cast<uint16_t>(data[1] << 8 | data[0]));
            cpu->set_register(CPU::Register::SP, static_cast<uint16_t>(sp + 2));
        });

        // Update PC and optionally set interrupt flag
        m_operations.push_back([enable_interrupts](CPU *cpu, MMU *) {
            cpu->set_register_from_intermediate(CPU::Register::PC);
            if (enable_interrupts)
                cpu->set_interrupt_enable(true);
        });
    }

    std::string get_flag_to_test_name() const { return ConditionNameMap.find(m_condition)->second; }

    enum class Condition { None = -1, NotZero = 0, Zero = 1, NotCarry = 2, Carry = 3 };
    const std::unordered_map<ConditionalCallReturnOrJumpBase::Condition, std::string> ConditionNameMap = {
        { ConditionalCallReturnOrJumpBase::Condition::None, "" },   { ConditionalCallReturnOrJumpBase::Condition::NotZero, "NZ" },
        { ConditionalCallReturnOrJumpBase::Condition::Zero, "Z" },  { ConditionalCallReturnOrJumpBase::Condition::NotCarry, "NC" },
        { ConditionalCallReturnOrJumpBase::Condition::Carry, "C" },
    };

    ConditionalCallReturnOrJumpBase::Condition m_condition;
};

// 0xE9 - Jump to address pointed to by HL
struct JumpToAddressInHL final : public ConditionalCallReturnOrJumpBase {
  public:
    JumpToAddressInHL() : ConditionalCallReturnOrJumpBase(1) {
        m_operations.push_back([](CPU *cpu, MMU *mmu) { cpu->set_register(CPU::Register::PC, cpu->get_16_bit_register(CPU::Register::HL)); });
    }

    std::string get_disassembled_instruction(uint8_t *) const override { return "JP (HL)"; }
};

// Jump to immediate address
struct JumpToImmediate final : public ConditionalCallReturnOrJumpBase {

  public:
    JumpToImmediate(uint8_t opcode) : ConditionalCallReturnOrJumpBase(3) {

        if (opcode == UnconditionalJumpOpcode) {
            m_condition = ConditionalCallReturnOrJumpBase::Condition::None;
        } else {
            uint8_t flag_idx = (opcode >> 3) & 0x07;
            m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx);
        }

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::W); });
        m_operations.push_back([&](CPU *cpu, MMU *) { m_is_done = !condition_is_met(cpu); });
        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register_from_intermediate(CPU::Register::PC); });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        uint16_t value = instruction_data[1] << 8 | instruction_data[0];
        return m_condition == ConditionalCallReturnOrJumpBase::Condition::None
                   ? GeneralUtilities::formatted_string("JP 0x%04X", value)
                   : GeneralUtilities::formatted_string("JP %s, 0x%04X", get_flag_to_test_name(), value);
    }

  private:
    const uint8_t UnconditionalJumpOpcode = 0xC3;
};

// Relative jumps from immediate
struct RelativeJump final : public ConditionalCallReturnOrJumpBase {

  public:
    RelativeJump(uint8_t opcode) : ConditionalCallReturnOrJumpBase(2) {
        uint8_t flag_idx = (opcode >> 3) & 0x07;
        m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx - 4);

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([&](CPU *cpu, MMU *) { m_is_done = !condition_is_met(cpu); });
        m_operations.push_back([&](CPU *cpu, MMU *) {
            uint8_t v = cpu->get_8_bit_register(CPU::Register::Z);
            int8_t jump_offset;
            memcpy(&jump_offset, &v, 1);
            auto target_address = static_cast<uint16_t>(cpu->get_16_bit_register(CPU::Register::PC) + jump_offset);
            cpu->set_register(CPU::Register::PC, target_address);
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        int8_t jump_offset;
        memcpy(&jump_offset, instruction_data, 1);
        return m_condition == ConditionalCallReturnOrJumpBase::Condition::None
                   ? GeneralUtilities::formatted_string("JR 0x%02X", instruction_data[0])
                   : GeneralUtilities::formatted_string("JR %s, 0x%02X", get_flag_to_test_name(), instruction_data[0]);
    }
};

// Call instructions
struct Call final : public ConditionalCallReturnOrJumpBase {

  public:
    Call(uint8_t opcode) : ConditionalCallReturnOrJumpBase(3) {
        if (opcode == UnconditionalCallOpcode) {
            m_condition = ConditionalCallReturnOrJumpBase::Condition::None;
        } else {
            uint8_t flag_idx = (opcode >> 3) & 0x07;
            m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx);
        }

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::W); });
        m_operations.push_back([&](CPU *cpu, MMU *) { m_is_done = !condition_is_met(cpu); });
        append_call_instructions();
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        uint16_t data = instruction_data[1] << 8 | instruction_data[0];
        return m_condition == ConditionalCallReturnOrJumpBase::Condition::None
                   ? GeneralUtilities::formatted_string("CALL 0x%04X", data)
                   : GeneralUtilities::formatted_string("CALL %s, 0x%04X", get_flag_to_test_name(), data);
    }

  private:
    const uint8_t UnconditionalCallOpcode = 0xCD;
};

// Return instructions
struct ReturnFromCall final : public ConditionalCallReturnOrJumpBase {

  public:
    ReturnFromCall(uint8_t opcode) : ConditionalCallReturnOrJumpBase(1) {
        m_operations.push_back([](CPU *, MMU *) {
            // Wait one cycle due to stack interaction
        });

        if ((opcode & 0x0F) == 0x09) {
            m_condition = ConditionalCallReturnOrJumpBase::Condition::None;
            m_enable_interrupts = (opcode >> 4) == 0x0D;
        } else {
            uint8_t flag_idx = (opcode >> 3) & 0x07;
            m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx);
            m_operations.push_back([&](CPU *cpu, MMU *) { m_is_done = !condition_is_met(cpu); });
        }

        append_return_instructions(m_enable_interrupts);
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        if (m_condition == ConditionalCallReturnOrJumpBase::Condition::None)
            return m_enable_interrupts ? "RETI" : "RET";
        else
            return GeneralUtilities::formatted_string("RET %s", get_flag_to_test_name());
    }

  private:
    bool m_enable_interrupts = false;
};

// Reset instruction
struct Reset final : public ConditionalCallReturnOrJumpBase {
  public:
    Reset(uint8_t opcode) : ConditionalCallReturnOrJumpBase(1) {
        m_reset_target = opcode - 0xC7; // Note: these opcodes are spaced 0x08 apart and the reset vectors are 0x00, 0x08, 0x10, ...
        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register(CPU::Register::WZ, m_reset_target); });
        append_call_instructions();
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return GeneralUtilities::formatted_string("RST %02XH", m_reset_target);
    }

  private:
    uint16_t m_reset_target;
};

// Load 8-bit register from immediate
struct Load8bitImmediate final : public Opcode {

  public:
    Load8bitImmediate(uint8_t opcode) : Opcode(2) {

        uint8_t register_idx = (opcode >> 3) & 0x07;
        m_target = CPU::register_map[register_idx];

        m_operations.push_back([](CPU *cpu, MMU *mmu) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });

        if (m_target == CPU::Register::HL) {
            m_operations.push_back([](CPU *cpu, MMU *) {});
            m_operations.push_back([&](CPU *cpu, MMU *mmu) {
                auto value = cpu->get_8_bit_register(CPU::Register::Z);
                (void)mmu->try_map_data_to_memory(&value, cpu->get_16_bit_register(CPU::Register::HL), 1);
            });
        } else {
            m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register_from_intermediate(m_target); });
        }
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        auto target_name = CPU::register_name.find(m_target)->second;
        return GeneralUtilities::formatted_string((m_target == CPU::Register::HL) ? "LD (%s), 0x%02X" : "LD %s, 0x%02X", target_name, instruction_data[0]);
    }

  private:
    CPU::Register m_target;
};

// Load 8-bit register
struct Load8bitRegister final : public Opcode {

  public:
    Load8bitRegister(uint8_t opcode) : Opcode(1) {

        uint8_t target_register_idx = (opcode >> 3) & 0x07;
        uint8_t source_register_idx = opcode & 0x07;

        m_target = CPU::register_map[target_register_idx];
        m_source = CPU::register_map[source_register_idx];

        if (m_target == CPU::Register::HL && m_source == CPU::Register::HL)
            throw std::invalid_argument("Loading (HL) with (HL) is invalid, should be HALT instruction");

        if (m_target == CPU::Register::HL) {
            m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register(CPU::Register::Z, cpu->get_8_bit_register(m_source)); });
            m_operations.push_back([&](CPU *cpu, MMU *mmu) {
                auto data = cpu->get_8_bit_register(CPU::Register::Z);
                (void)mmu->try_map_data_to_memory(&data, cpu->get_16_bit_register(CPU::Register::HL), 1);
            });
        } else if (m_source == CPU::Register::HL) {
            m_operations.push_back([](CPU *cpu, MMU *mmu) {
                uint8_t source_value;
                (void)mmu->try_read_from_memory(&source_value, cpu->get_16_bit_register(CPU::Register::HL), 1);
                cpu->set_register(CPU::Register::Z, source_value);
            });
            m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register(m_target, cpu->get_8_bit_register(CPU::Register::Z)); });
        } else {
            m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register(m_target, cpu->get_8_bit_register(m_source)); });
        }
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        auto target_name = CPU::register_name.find(m_target)->second;
        auto source_name = CPU::register_name.find(m_source)->second;

        if (m_target == CPU::Register::HL)
            return GeneralUtilities::formatted_string("LD (%s), %s", target_name, source_name);
        else if (m_source == CPU::Register::HL)
            return GeneralUtilities::formatted_string("LD %s, (%s)", target_name, source_name);
        else
            return GeneralUtilities::formatted_string("LD %s, %s", target_name, source_name);
    }

  private:
    CPU::Register m_target;
    CPU::Register m_source;
};

// Load 16-bit register from immediate
struct Load16bitImmediate final : public Opcode {

  public:
    Load16bitImmediate(uint8_t opcode) : Opcode(3) {
        uint8_t register_idx = (opcode >> 4) & 0x03;

        m_target = CPU::wide_register_map[register_idx];

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::W); });
        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register_from_intermediate(m_target); });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        uint16_t data = instruction_data[1] << 8 | instruction_data[0];
        auto target_name = CPU::register_name.find(m_target)->second;
        return GeneralUtilities::formatted_string("LD %s, 0x%X", target_name, data);
    }

  private:
    CPU::Register m_target;
};

// Load 16-bit register indirect
struct Load16bitIndirect final : public Opcode {

  public:
    Load16bitIndirect(uint8_t opcode) : Opcode(1) {
        m_target_is_accumulator = ((opcode >> 3) & 0x01) == 1;
        uint8_t target_selector = (opcode >> 4) & 0x03;
        m_target_source = CPU::wide_register_map[target_selector];

        // Target cannot be SP, both values maps to HL but with either increment or decrement
        m_hl_offset = m_target_source == CPU::Register::HL ? 1 : m_target_source == CPU::Register::SP ? -1 : 0;
        m_target_source = m_target_source == CPU::Register::SP ? CPU::Register::HL : m_target_source;

        if (m_target_is_accumulator) {
            m_operations.push_back([&](CPU *cpu, MMU *mmu) {
                uint8_t data_to_load;
                (void)mmu->try_read_from_memory(&data_to_load, cpu->get_16_bit_register(m_target_source), 1);
                cpu->set_register(CPU::Register::Z, data_to_load);
            });
            m_operations.push_back([&](CPU *cpu, MMU *) {
                cpu->set_register_from_intermediate(CPU::Register::A);
                if (m_target_source == CPU::Register::HL)
                    cpu->set_register(CPU::Register::HL, static_cast<uint16_t>(cpu->get_16_bit_register(CPU::Register::HL) + m_hl_offset));
            });
        } else {
            m_operations.push_back([&](CPU *cpu, MMU *mmu) { cpu->set_register(CPU::Register::Z, cpu->get_8_bit_register(CPU::Register::A)); });
            m_operations.push_back([&](CPU *cpu, MMU *mmu) {
                uint8_t data_to_load = cpu->get_8_bit_register(CPU::Register::Z);
                (void)mmu->try_map_data_to_memory(&data_to_load, cpu->get_16_bit_register(m_target_source), 1);
                if (m_target_source == CPU::Register::HL)
                    cpu->set_register(CPU::Register::HL, static_cast<uint16_t>(cpu->get_16_bit_register(CPU::Register::HL) + m_hl_offset));
            });
        }
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        auto target_name = CPU::register_name.find(m_target_source)->second;
        auto hl_inc_or_dec = m_hl_offset == 0 ? "" : (m_hl_offset == 1 ? "+" : "-");
        auto accumulator_name = CPU::register_name.find(CPU::Register::A)->second;
        return m_target_is_accumulator ? GeneralUtilities::formatted_string("LD %s, (%s%s)", accumulator_name, target_name, hl_inc_or_dec)
                                       : GeneralUtilities::formatted_string("LD (%s%s), %s", target_name, hl_inc_or_dec, accumulator_name);
    }

  private:
    CPU::Register m_target_source;
    bool m_target_is_accumulator;
    int m_hl_offset = 0;
};

// Increment and decrement 16-bit or 8-bit
struct IncrementOrDecrement8Or16bit final : public Opcode {
  public:
    IncrementOrDecrement8Or16bit(uint8_t opcode) : Opcode(1) {
        m_is_16_bit = (opcode & 0x03) == 0x03;

        if (m_is_16_bit) {
            m_target = CPU::wide_register_map[(opcode >> 4) & 0x03];
            m_operation = ((opcode >> 3) & 0x01) == 0 ? IncrementOrDecrement8Or16bit::Operation::Increment : IncrementOrDecrement8Or16bit::Operation::Decrement;
            append_16_bit_instructions();
        } else {
            m_target = CPU::register_map[(opcode >> 3) & 0x07];
            m_operation = (opcode & 0x01) == 0 ? IncrementOrDecrement8Or16bit::Operation::Increment : IncrementOrDecrement8Or16bit::Operation::Decrement;
            append_8_bit_instructions();
        }
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        auto operation_name = s_operation_name_map[static_cast<int>(m_operation)];
        auto target_name = CPU::register_name.find(m_target)->second;
        return GeneralUtilities::formatted_string((m_target == CPU::Register::HL && !m_is_16_bit) ? "%s (%s)" : "%s %s", operation_name, target_name);
    }

  private:
    enum class Operation { Increment, Decrement };
    static inline std::string s_operation_name_map[] = { "INC", "DEC" };
    bool m_is_16_bit;
    CPU::Register m_target;
    IncrementOrDecrement8Or16bit::Operation m_operation;

    void append_16_bit_instructions() {
        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register(CPU::Register::WZ, cpu->get_16_bit_register(m_target)); });
        m_operations.push_back([&](CPU *cpu, MMU *) {
            uint16_t change = m_operation == IncrementOrDecrement8Or16bit::Operation::Increment ? 1 : -1;
            cpu->set_register(m_target, static_cast<uint16_t>(cpu->get_16_bit_register(CPU::Register::WZ) + change));
        });
    }

    void append_8_bit_instructions() {
        if (m_target == CPU::Register::HL) {
            m_operations.push_back([&](CPU *cpu, MMU *mmu) {
                uint8_t value;
                (void)mmu->try_read_from_memory(&value, cpu->get_16_bit_register(CPU::Register::HL), 1);
                cpu->set_register(CPU::Register::Z, value);
            });
            m_operations.push_back([](CPU *, MMU *) {});
        }

        m_operations.push_back([&](CPU *cpu, MMU *mmu) {
            uint8_t value = cpu->get_8_bit_register(m_target == CPU::Register::HL ? CPU::Register::Z : m_target);
            bool half_carry_flag = m_operation == IncrementOrDecrement8Or16bit::Operation::Increment ? cpu->half_carry_occurs_on_add(value, 1)
                                                                                                     : cpu->half_carry_occurs_on_subtract(value, 1);

            uint8_t result = m_operation == IncrementOrDecrement8Or16bit::Operation::Increment ? value + 1 : value - 1;
            if (m_target == CPU::Register::HL)
                (void)mmu->try_map_data_to_memory(&result, cpu->get_16_bit_register(CPU::Register::HL), 1);
            else
                cpu->set_register(m_target, result);

            cpu->set_flag(CPU::Flag::Z, result == 0x00);
            cpu->set_flag(CPU::Flag::N, m_operation == IncrementOrDecrement8Or16bit::Operation::Decrement);
            cpu->set_flag(CPU::Flag::H, half_carry_flag);
        });
    }
};

// Register operations common
struct RegisterOperationBase : public Opcode {

  protected:
    RegisterOperationBase(uint8_t opcode, uint8_t size) : Opcode(size) { set_operation_type(opcode); }

    std::string get_operation_name() const { return m_operations_name.find(m_operation)->second; }

    void execute_operation(CPU *cpu, uint8_t *operand) {

        uint8_t accumulator_value = cpu->get_8_bit_register(CPU::Register::A);
        uint8_t result;
        bool *flag_pattern; // Z, N, H, C

        switch (m_operation) {
        case RegisterOperationBase::Operation::AddToAccumulator:
            result = accumulator_value + *operand;
            flag_pattern = new bool[]{ result == 0x00, false, cpu->half_carry_occurs_on_add(accumulator_value, *operand),
                                       cpu->carry_occurs_on_add(accumulator_value, *operand) };
            break;

        case RegisterOperationBase::Operation::AddToAccumulatorWithCarry:
            result = accumulator_value + *operand + cpu->flag_is_set(CPU::Flag::C);
            flag_pattern = new bool[]{ result == 0x00, false, cpu->half_carry_occurs_on_add(accumulator_value, *operand, true),
                                       cpu->carry_occurs_on_add(accumulator_value, *operand, true) };
            break;

        case RegisterOperationBase::Operation::And:
            result = accumulator_value & *operand;
            flag_pattern = new bool[]{ result == 0x00, false, true, false };
            break;

        case RegisterOperationBase::Operation::Xor:
            result = accumulator_value ^ *operand;
            flag_pattern = new bool[]{ result == 0x00, false, false, false };
            break;

        case RegisterOperationBase::Operation::Or:
            result = accumulator_value | *operand;
            flag_pattern = new bool[]{ result == 0x00, false, false, false };
            break;

        case RegisterOperationBase::Operation::Compare:
            flag_pattern = new bool[]{ accumulator_value == *operand, 1, cpu->half_carry_occurs_on_subtract(accumulator_value, *operand),
                                       cpu->carry_occurs_on_subtract(accumulator_value, *operand) };
            break;

        case RegisterOperationBase::Operation::SubtractFromAccumulator:
            result = accumulator_value - *operand;
            flag_pattern = new bool[]{ result == 0x00, 1, cpu->half_carry_occurs_on_subtract(accumulator_value, *operand),
                                       cpu->carry_occurs_on_subtract(accumulator_value, *operand) };
            break;

        case RegisterOperationBase::Operation::SubtractFromAccumulatorWithCarry:
            result = accumulator_value - *operand - (cpu->flag_is_set(CPU::Flag::C) ? 1 : 0);
            flag_pattern = new bool[]{ result == 0x00, 1, cpu->half_carry_occurs_on_subtract_with_carry(accumulator_value, *operand),
                                       cpu->carry_occurs_on_subtract(accumulator_value, *operand + (cpu->flag_is_set(CPU::Flag::C) ? 1 : 0)) };
            break;
        default:
            __builtin_unreachable();
        }

        if (m_operation != RegisterOperationBase::Operation::Compare)
            cpu->set_register(CPU::Register::A, result);

        cpu->set_flag(CPU::Flag::Z, flag_pattern[0]);
        cpu->set_flag(CPU::Flag::N, flag_pattern[1]);
        cpu->set_flag(CPU::Flag::H, flag_pattern[2]);
        cpu->set_flag(CPU::Flag::C, flag_pattern[3]);

        delete[] flag_pattern;
    }

    enum class Operation {
        AddToAccumulator,
        AddToAccumulatorWithCarry,
        SubtractFromAccumulator,
        SubtractFromAccumulatorWithCarry,
        And,
        Xor,
        Or,
        Compare,
    };
    RegisterOperationBase::Operation m_operation;

  private:
    static inline RegisterOperationBase::Operation s_operations[] = {
        RegisterOperationBase::Operation::AddToAccumulator,
        RegisterOperationBase::Operation::AddToAccumulatorWithCarry,
        RegisterOperationBase::Operation::SubtractFromAccumulator,
        RegisterOperationBase::Operation::SubtractFromAccumulatorWithCarry,
        RegisterOperationBase::Operation::And,
        RegisterOperationBase::Operation::Xor,
        RegisterOperationBase::Operation::Or,
        RegisterOperationBase::Operation::Compare,
    };

    const std::unordered_map<RegisterOperationBase::Operation, std::string> m_operations_name = {
        { RegisterOperationBase::Operation::AddToAccumulator, "ADD A," },
        { RegisterOperationBase::Operation::AddToAccumulatorWithCarry, "ADC A," },
        { RegisterOperationBase::Operation::SubtractFromAccumulator, "SUB" },
        { RegisterOperationBase::Operation::SubtractFromAccumulatorWithCarry, "SBC A," },
        { RegisterOperationBase::Operation::And, "AND" },
        { RegisterOperationBase::Operation::Xor, "XOR" },
        { RegisterOperationBase::Operation::Or, "OR" },
        { RegisterOperationBase::Operation::Compare, "CP" },
    };

    void set_operation_type(uint8_t opcode) { m_operation = s_operations[(opcode >> 3) & 0x07]; }
};

// Register operations
struct RegisterOperation final : public RegisterOperationBase {

  public:
    RegisterOperation(uint8_t opcode) : RegisterOperationBase(opcode, 1) {
        uint8_t register_idx = opcode & 0x07;
        m_operand_register = CPU::register_map[register_idx];

        if (m_operand_register == CPU::Register::HL) {
            m_operations.push_back([](CPU *cpu, MMU *mmu) {
                uint8_t data;
                (void)mmu->try_read_from_memory(&data, cpu->get_16_bit_register(CPU::Register::HL), 1);
                cpu->set_register(CPU::Register::Z, data);
            });
        }

        m_operations.push_back([&](CPU *cpu, MMU *) {
            uint8_t operand_value = cpu->get_8_bit_register(m_operand_register == CPU::Register::HL ? CPU::Register::Z : m_operand_register);
            execute_operation(cpu, &operand_value);
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        auto target_name = CPU::register_name.find(m_operand_register)->second;
        return m_operand_register == CPU::Register::HL ? GeneralUtilities::formatted_string("%s (%s)", get_operation_name(), target_name)
                                                       : GeneralUtilities::formatted_string("%s %s", get_operation_name(), target_name);
    }

  private:
    CPU::Register m_operand_register;
};

// Operate on accumulator with immediate
struct AccumulatorOperation final : RegisterOperationBase {

  public:
    AccumulatorOperation(uint8_t opcode) : RegisterOperationBase(opcode, 2) {
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([&](CPU *cpu, MMU *) {
            uint8_t data = cpu->get_8_bit_register(CPU::Register::Z);
            execute_operation(cpu, &data);
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return GeneralUtilities::formatted_string("%s 0x%02X", get_operation_name(), instruction_data[0]);
    }
};

// 16-bit add
struct Add16bitRegister final : public Opcode {

  public:
    Add16bitRegister(uint8_t opcode) : Opcode(1) {
        m_target = CPU::wide_register_map[(opcode >> 4) & 0x03];

        m_operations.push_back([](CPU *, MMU *) {});
        m_operations.push_back([&](CPU *cpu, MMU *) {
            uint16_t v = cpu->get_16_bit_register(m_target);
            uint16_t hl = cpu->get_16_bit_register(CPU::Register::HL);

            cpu->set_register(CPU::Register::HL, static_cast<uint16_t>(hl + v));

            cpu->set_flag(CPU::Flag::H, cpu->half_carry_occurs_on_add(hl, v));
            cpu->set_flag(CPU::Flag::N, false);
            cpu->set_flag(CPU::Flag::C, cpu->carry_occurs_on_add(hl, v));
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return GeneralUtilities::formatted_string("ADD HL, %s", CPU::register_name.find(m_target)->second);
    }

  private:
    CPU::Register m_target;
};

// Read/Write IO-port C from/to A
struct ReadWriteIOPortCWithA final : public Opcode {

  public:
    ReadWriteIOPortCWithA(uint8_t opcode) : Opcode(1) {

        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x04)
            m_type = ReadWriteIOPortCWithA::ActionType::Write;
        else if (type_idx == 0x06)
            m_type = ReadWriteIOPortCWithA::ActionType::Read;
        else
            throw std::invalid_argument("Read/Write type identifier invalid");

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->set_register(CPU::Register::Z, cpu->get_8_bit_register(CPU::Register::C)); });
        m_operations.push_back([&](CPU *cpu, MMU *mmu) {
            cpu->set_register(CPU::Register::W, (uint8_t)0xFF);

            if (m_type == ReadWriteIOPortCWithA::ActionType::Write) {
                uint8_t reg_a = cpu->get_8_bit_register(CPU::Register::A);
                (void)mmu->try_map_data_to_memory(&reg_a, cpu->get_16_bit_register(CPU::Register::WZ), 1);
            } else {
                uint8_t value;
                (void)mmu->try_read_from_memory(&value, cpu->get_16_bit_register(CPU::Register::WZ), 1);
                cpu->set_register(CPU::Register::A, value);
            }
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return m_type == ReadWriteIOPortCWithA::ActionType::Write ? "LD ($FF00 + C), A" : "LD A, ($FF00 + C)";
    }

  private:
    enum class ActionType { Read, Write };
    ReadWriteIOPortCWithA::ActionType m_type;
};

// Read/Write IO-port n from/to A
struct ReadWriteIOPortNWithA final : public Opcode {

  public:
    ReadWriteIOPortNWithA(uint8_t opcode) : Opcode(2) {
        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x04)
            m_type = ReadWriteIOPortNWithA::ActionType::Write;
        else if (type_idx == 0x06)
            m_type = ReadWriteIOPortNWithA::ActionType::Read;
        else
            throw std::invalid_argument("Read/Write type identifier invalid");

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->set_register(CPU::Register::W, (uint8_t)0xFF); });
        m_operations.push_back([&](CPU *cpu, MMU *mmu) {
            if (m_type == ReadWriteIOPortNWithA::ActionType::Write) {
                uint8_t reg_a = cpu->get_8_bit_register(CPU::Register::A);
                (void)mmu->try_map_data_to_memory(&reg_a, cpu->get_16_bit_register(CPU::Register::WZ), 1);
            } else {
                uint8_t value;
                (void)mmu->try_read_from_memory(&value, cpu->get_16_bit_register(CPU::Register::WZ), 1);
                cpu->set_register(CPU::Register::A, value);
            }
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return m_type == ReadWriteIOPortNWithA::ActionType::Write ? GeneralUtilities::formatted_string("LD ($%X), A", 0xFF00 + instruction_data[0])
                                                                  : GeneralUtilities::formatted_string("LD A, ($%X)", 0xFF00 + instruction_data[0]);
    }

  private:
    enum class ActionType { Read, Write };
    ReadWriteIOPortNWithA::ActionType m_type;
};

// Add or subtract from stackpointer and store in HL or SP
struct SetSPOrHLToSPAndOffset final : public Opcode {

  public:
    SetSPOrHLToSPAndOffset(uint8_t opcode) : Opcode(2) {
        m_target = (opcode & 0xF0) == 0xE0 ? CPU::Register::SP : CPU::Register::HL;

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([](CPU *cpu, MMU *) {});
        m_operations.push_back([&](CPU *cpu, MMU *) {
            uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);
            uint8_t data = cpu->get_8_bit_register(CPU::Register::Z);
            int8_t offset;
            memcpy(&offset, &data, 1);
            uint16_t result = sp + offset;

            cpu->set_register(m_target, static_cast<uint16_t>(result));
            cpu->set_flag(CPU::Flag::Z, 0);
            cpu->set_flag(CPU::Flag::N, 0);
            cpu->set_flag(CPU::Flag::H, cpu->half_carry_occurs_on_add(static_cast<uint8_t>(sp & 0x00FF), data));
            cpu->set_flag(CPU::Flag::C, cpu->carry_occurs_on_add(static_cast<uint8_t>(sp & 0x00FF), data));
        });

        if (m_target == CPU::Register::SP)
            m_operations.push_back([](CPU *cpu, MMU *) {});
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return GeneralUtilities::formatted_string(m_target == CPU::Register::SP ? "ADD SP, 0x%02X" : "LD HL, SP + 0x%02X", instruction_data[0]);
    }

  private:
    CPU::Register m_target;
};

// Load from or set A indirect
struct LoadFromOrSetAIndirect final : public Opcode {

  public:
    LoadFromOrSetAIndirect(uint8_t opcode) : Opcode(3) {
        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x05)
            m_type = LoadFromOrSetAIndirect::Direction::FromAccumulator;
        else if (type_idx == 0x07)
            m_type = LoadFromOrSetAIndirect::Direction::ToAccumulator;
        else
            throw std::invalid_argument("Direction type identifier invalid");

        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::Z); });
        m_operations.push_back([](CPU *cpu, MMU *) { cpu->read_at_pc_and_store_in_intermediate(CPU::Register::W); });
        m_operations.push_back([](CPU *, MMU *) {});
        m_operations.push_back([&](CPU *cpu, MMU *mmu) {
            if (m_type == LoadFromOrSetAIndirect::Direction::FromAccumulator) {
                uint8_t reg_a = cpu->get_8_bit_register(CPU::Register::A);
                (void)mmu->try_map_data_to_memory(&reg_a, cpu->get_16_bit_register(CPU::Register::WZ), 1);
            } else {
                uint8_t value;
                (void)mmu->try_read_from_memory(&value, cpu->get_16_bit_register(CPU::Register::WZ), 1);
                cpu->set_register(CPU::Register::A, value);
            }
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        uint16_t data = instruction_data[1] << 8 | instruction_data[0];
        return m_type == LoadFromOrSetAIndirect::Direction::FromAccumulator ? GeneralUtilities::formatted_string("LD (0x%2X), A", data)
                                                                            : GeneralUtilities::formatted_string("LD A, (0x%2X)", data);
    }

  private:
    enum class Direction { FromAccumulator, ToAccumulator };
    LoadFromOrSetAIndirect::Direction m_type;
};

// 16-bit push
struct Push16bitRegister final : public Opcode {
  public:
    Push16bitRegister(uint8_t opcode) : Opcode(1) {
        uint8_t source_selector = (opcode >> 4) & 0x03;
        m_source = CPU::wide_register_map[source_selector];

        // Target cannot be SP, top value should map to AF
        m_source = m_source == CPU::Register::SP ? CPU::Register::AF : m_source;

        m_operations.push_back([](CPU *, MMU *) {});
        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register(CPU::Register::WZ, cpu->get_16_bit_register(m_source)); });
        m_operations.push_back([](CPU *, MMU *) {});
        m_operations.push_back([](CPU *cpu, MMU *mmu) {
            uint16_t src = cpu->get_16_bit_register(CPU::Register::WZ);
            uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);
            (void)mmu->try_map_data_to_memory((uint8_t *)&src, sp - 2, 2);
            cpu->set_register(CPU::Register::SP, static_cast<uint16_t>(sp - 2));
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return GeneralUtilities::formatted_string("PUSH %s", CPU::register_name.find(m_source)->second);
    }

  private:
    CPU::Register m_source;
};

// 16-bit pop
struct Pop16bitRegister final : public Opcode {
  public:
    Pop16bitRegister(uint8_t opcode) : Opcode(1) {
        uint8_t target_selector = (opcode >> 4) & 0x03;
        m_target = CPU::wide_register_map[target_selector];

        // Target cannot be SP, top value should map to AF
        m_target = m_target == CPU::Register::SP ? CPU::Register::AF : m_target;

        m_operations.push_back([](CPU *cpu, MMU *) {});
        m_operations.push_back([](CPU *cpu, MMU *mmu) {
            uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);
            uint8_t *data = new uint8_t[2];
            (void)mmu->try_read_from_memory(data, sp, 2);
            cpu->set_register(CPU::Register::WZ, static_cast<uint16_t>(data[1] << 8 | data[0]));
            cpu->set_register(CPU::Register::SP, static_cast<uint16_t>(sp + 2));
        });

        m_operations.push_back([&](CPU *cpu, MMU *) { cpu->set_register(m_target, cpu->get_16_bit_register(CPU::Register::WZ)); });
    }
    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        return GeneralUtilities::formatted_string("POP %s", CPU::register_name.find(m_target)->second);
    }

  private:
    CPU::Register m_target;
};

// Extended opcodes, rotations, shifts, swap, bit tests, set and reset
struct ExtendedOpcode : public Opcode {
  public:
    ExtendedOpcode(uint8_t opcode, bool is_accumulator_shorthand = false) : Opcode(1) {
        uint8_t extended_op_code_type_idx = (opcode >> 6) & 0x03;
        uint8_t bit_or_rotation_idx = (opcode >> 3) & 0x07;
        uint8_t target_idx = opcode & 0x07;

        m_is_accumulator_shorthand = is_accumulator_shorthand;
        m_target = CPU::register_map[target_idx];
        m_type = s_extended_opcode_type_map[extended_op_code_type_idx];

        if (m_type == ExtendedOpcode::ExtendedOpcodeType::RotationShiftOrSwap)
            m_rot_type = s_rotations_type_map[bit_or_rotation_idx];
        else
            m_bit = bit_or_rotation_idx;

        if (m_target == CPU::Register::HL) {
            m_operations.push_back([&](CPU *cpu, MMU *mmu) {
                uint8_t data;
                (void)mmu->try_read_from_memory(&data, cpu->get_16_bit_register(CPU::Register::HL), 1);
                cpu->set_register(CPU::Register::Z, data);
            });
        }

        m_operations.push_back([&](CPU *cpu, MMU *mmu) {
            uint8_t current_register_value = cpu->get_8_bit_register(m_target == CPU::Register::HL ? CPU::Register::Z : m_target);

            switch (m_type) {

            case ExtendedOpcode::ExtendedOpcodeType::RotationShiftOrSwap:
                perform_rotation_shift_or_swap(cpu, &current_register_value);

                if (m_target == CPU::Register::HL)
                    (void)mmu->try_map_data_to_memory(&current_register_value, cpu->get_16_bit_register(CPU::Register::HL), 1);
                else
                    cpu->set_register(m_target, current_register_value);

                if (m_is_accumulator_shorthand)
                    cpu->set_flag(CPU::Flag::Z, false);
                break;

            case ExtendedOpcode::ExtendedOpcodeType::Test:
                cpu->set_flag(CPU::Flag::Z, !BitUtilities::bit_is_set(current_register_value, m_bit));
                cpu->set_flag(CPU::Flag::N, false);
                cpu->set_flag(CPU::Flag::H, true);
                break;

            case ExtendedOpcode::ExtendedOpcodeType::Reset:
                BitUtilities::reset_bit_in_byte(current_register_value, m_bit);
                if (m_target == CPU::Register::HL)
                    (void)mmu->try_map_data_to_memory(&current_register_value, cpu->get_16_bit_register(CPU::Register::HL), 1);
                else
                    cpu->set_register(m_target, current_register_value);
                break;

            case ExtendedOpcode::ExtendedOpcodeType::Set:
                BitUtilities::set_bit_in_byte(current_register_value, m_bit);
                if (m_target == CPU::Register::HL)
                    (void)mmu->try_map_data_to_memory(&current_register_value, cpu->get_16_bit_register(CPU::Register::HL), 1);
                else
                    cpu->set_register(m_target, current_register_value);
                break;
            }
        });
    }

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        auto target_name = CPU::register_name.find(m_target)->second;
        if (m_type == ExtendedOpcode::ExtendedOpcodeType::RotationShiftOrSwap) {
            auto operation_name = RotationsTypeName.find(m_rot_type)->second;
            return m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s (%s)", operation_name, target_name)
                                                 : GeneralUtilities::formatted_string("%s %s", operation_name, target_name);
        } else {
            auto operation_name = ExtendedOpcodeTypeName.find(m_type)->second;
            return m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s %d, (%s)", operation_name, m_bit, target_name)
                                                 : GeneralUtilities::formatted_string("%s %d, %s", operation_name, m_bit, target_name);
        }
    }

  private:
    enum class ExtendedOpcodeType {
        RotationShiftOrSwap,
        Test,
        Reset,
        Set,
    };

    enum class RotationShiftOrSwapType {
        RotateLeft,
        RotateRight,
        RotateLeftThroughCarry,
        RotateRightThroughCarry,
        ShiftLeftArithmetic,
        ShiftRightArithmetic,
        SwapNibbles,
        ShiftRightLogic,
    };

    static inline ExtendedOpcodeType s_extended_opcode_type_map[] = {
        ExtendedOpcodeType::RotationShiftOrSwap,
        ExtendedOpcodeType::Test,
        ExtendedOpcodeType::Reset,
        ExtendedOpcodeType::Set,
    };

    const std::unordered_map<ExtendedOpcodeType, std::string> ExtendedOpcodeTypeName = {
        { ExtendedOpcodeType::RotationShiftOrSwap, "ROT" },
        { ExtendedOpcodeType::Test, "BIT" },
        { ExtendedOpcodeType::Reset, "RES" },
        { ExtendedOpcodeType::Set, "SET" },
    };

    static inline RotationShiftOrSwapType s_rotations_type_map[] = {
        RotationShiftOrSwapType::RotateLeft,
        RotationShiftOrSwapType::RotateRight,
        RotationShiftOrSwapType::RotateLeftThroughCarry,
        RotationShiftOrSwapType::RotateRightThroughCarry,
        RotationShiftOrSwapType::ShiftLeftArithmetic,
        RotationShiftOrSwapType::ShiftRightArithmetic,
        RotationShiftOrSwapType::SwapNibbles,
        RotationShiftOrSwapType::ShiftRightLogic,
    };

    const std::unordered_map<RotationShiftOrSwapType, std::string> RotationsTypeName = {
        { RotationShiftOrSwapType::RotateLeft, "RLC" },
        { RotationShiftOrSwapType::RotateRight, "RRC" },
        { RotationShiftOrSwapType::RotateLeftThroughCarry, "RL" },
        { RotationShiftOrSwapType::RotateRightThroughCarry, "RR" },
        { RotationShiftOrSwapType::ShiftLeftArithmetic, "SLA" },
        { RotationShiftOrSwapType::ShiftRightArithmetic, "SRA" },
        { RotationShiftOrSwapType::SwapNibbles, "SWAP" },
        { RotationShiftOrSwapType::ShiftRightLogic, "SRL" },
    };

    ExtendedOpcode::ExtendedOpcodeType m_type;
    ExtendedOpcode::RotationShiftOrSwapType m_rot_type;
    CPU::Register m_target;
    uint8_t m_bit;
    bool m_is_accumulator_shorthand;

    void perform_rotation_shift_or_swap(CPU *cpu, uint8_t *data) {
        uint8_t carry_flag = cpu->flag_is_set(CPU::Flag::C) ? 0x01 : 0x00;
        switch (m_rot_type) {

        // C <- [7 <- 0] <- [7]
        case RotationShiftOrSwapType::RotateLeft: {
            cpu->set_flag(CPU::Flag::C, last_bit_set(*data));
            *data = (*data << 1) | (last_bit_set(*data) ? 0x01 : 0x00);
            cpu->set_flag(CPU::Flag::Z, *data == 0);
        } break;

        // C <- [7 <- 0] <- C
        case RotationShiftOrSwapType::RotateLeftThroughCarry: {
            cpu->set_flag(CPU::Flag::C, last_bit_set(*data));
            *data = (*data << 1) | carry_flag;
            cpu->set_flag(CPU::Flag::Z, *data == 0x00);
        } break;

        // C <- [7 <- 0] <- 0
        case RotationShiftOrSwapType::ShiftLeftArithmetic:
            cpu->set_flag(CPU::Flag::C, last_bit_set(*data));
            *data <<= 1;
            cpu->set_flag(CPU::Flag::Z, *data == 0x00);
            break;

        // [0] -> [7 -> 0] -> C
        case RotationShiftOrSwapType::RotateRight: {
            cpu->set_flag(CPU::Flag::C, first_bit_set(*data));
            *data = (first_bit_set(*data) ? 0x80 : 0x00) | (*data >> 1);
            cpu->set_flag(CPU::Flag::Z, *data == 0x00);
        } break;

        // C -> [7 -> 0] -> C
        case RotationShiftOrSwapType::RotateRightThroughCarry:
            cpu->set_flag(CPU::Flag::C, first_bit_set(*data));
            *data = (carry_flag == 0x01 ? 0x80 : 0x00) | (*data >> 1);
            cpu->set_flag(CPU::Flag::Z, *data == 0x00);
            break;

        // [7] -> [7 -> 0] -> C
        case RotationShiftOrSwapType::ShiftRightArithmetic: {
            cpu->set_flag(CPU::Flag::C, first_bit_set(*data));
            *data = (last_bit_set(*data) ? 0x80 : 0x00) | (*data >> 1);
            cpu->set_flag(CPU::Flag::Z, *data == 0x00);
        } break;

        // 0 -> [7 -> 0] -> C
        case RotationShiftOrSwapType::ShiftRightLogic:
            cpu->set_flag(CPU::Flag::C, first_bit_set(*data));
            *data >>= 1;
            cpu->set_flag(CPU::Flag::Z, *data == 0x00);
            break;

        case RotationShiftOrSwapType::SwapNibbles: {
            uint8_t lower_nibble = (*data & 0x0F);
            *data = (*data >> 4) | (lower_nibble << 4);
            cpu->set_flag(CPU::Flag::C, false);
            cpu->set_flag(CPU::Flag::Z, *data == 0);
        } break;

        default:
            NOT_IMPLEMENTED(get_disassembled_instruction(nullptr));
        }

        cpu->set_flag(CPU::Flag::N, false);
        cpu->set_flag(CPU::Flag::H, false);
    }

    bool first_bit_set(const uint8_t &data) const { return BitUtilities::bit_is_set(data, 0); }
    bool last_bit_set(const uint8_t &data) const { return BitUtilities::bit_is_set(data, 7); }
};

// Rotate accumulator
struct RotateAccumulator final : ExtendedOpcode {

  public:
    RotateAccumulator(uint8_t opcode) : ExtendedOpcode(opcode, true) {}

    std::string get_disassembled_instruction(uint8_t *instruction_data) const override {
        auto instr = ExtendedOpcode::get_disassembled_instruction(instruction_data);
        auto ws = instr.find(' ');
        return instr.replace(ws, 1, "");
    }
};
}