#pragma once

#include "CPU.h"
#include "MMU.h"
#include "components/Opcodes.h"
#include "util/GeneralUtilities.h"
#include <exception>
#include <float.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stdint.h>
#include <string>

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
    uint8_t cycles;
    uint8_t identifier;
    std::string name;

    virtual std::string fully_disassembled_instruction() const { return name; }
    virtual void set_opcode_data(uint8_t *data) {}
    virtual void execute(CPU *cpu, MMU *mmu) {}

    virtual ~Opcode() {}

  protected:
    Opcode(uint8_t size, uint8_t cycles, uint8_t identifier, const std::string &name) : size(size), cycles(cycles), identifier(identifier), name(name) {}
    Opcode(uint8_t size, uint8_t cycles, uint8_t identifier) : size(size), cycles(cycles), identifier(identifier) {}
    Opcode(uint8_t size, uint8_t cycles) : size(size), cycles(cycles) {}
    Opcode(uint8_t size) : size(size) {}
    Opcode() {}
};

// 0x00 - NoOp
struct NoOperation final : public Opcode {
  public:
    NoOperation() : Opcode(1, 4, 0x00, "NOP") {}
};

// 0x08 Store SP at addresses given by 16-bit immediate
struct StoreStackpointer final : public Opcode {
  public:
    StoreStackpointer() : Opcode(3, 20, 0x08, "LD (a16), SP") {}

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[1] << 8 | data[0];
        m_disassembled_instruction = GeneralUtilities::formatted_string("LD (%s), SP", m_data);
    }

    void execute(CPU *cpu, MMU *mmu) override { exit(1); }

  private:
    uint16_t m_data;
    std::string m_disassembled_instruction;
};

// 0x10 - Stops to CPU (very low power mode, can be used to switch between normal and double CPU speed on GBC)
struct Stop final : public Opcode {
  public:
    Stop() : Opcode(2, 4, 0x10, "STOP 0") {}
    void execute(CPU *cpu, MMU *mmu) override { NOT_IMPLEMENTED("STOP"); }
};

// 0x76 - Halts to CPU (low-power mode until interrupt)
struct Halt final : public Opcode {
  public:
    static const uint8_t opcode = 0x76;
    Halt() : Opcode(1, 4, 0x76, "HALT") {}
    void execute(CPU *cpu, MMU *mmu) override { NOT_IMPLEMENTED("HALT"); }
};

// 0x27 - Decimal adjust accumulator (changes A to BCD representation)
struct DecimalAdjustAccumulator final : public Opcode {
  public:
    DecimalAdjustAccumulator() : Opcode(1, 4, 0x27, "DAA") {}
    void execute(CPU *cpu, MMU *mmu) override { NOT_IMPLEMENTED("DAA"); }
};

// 0x37 - Set carry flag
struct SetCarryFlag final : public Opcode {
  public:
    SetCarryFlag() : Opcode(1, 4, 0x37, "SCF") {}
    void execute(CPU *cpu, MMU *mmu) override {
        cpu->set_flag(CPU::Flag::N, false);
        cpu->set_flag(CPU::Flag::H, false);
        cpu->set_flag(CPU::Flag::C, true);
    }
};

// 0x2F - One's complement the accumulator
struct InvertAccumulator final : public Opcode {
  public:
    InvertAccumulator() : Opcode(1, 4, 0x2F, "CPL") {}
    void execute(CPU *cpu, MMU *mmu) override {
        cpu->set_register(CPU::Register::A, static_cast<uint8_t>(!cpu->get_8_bit_register(CPU::Register::A)));
        cpu->set_flag(CPU::Flag::N, true);
        cpu->set_flag(CPU::Flag::H, true);
    }
};

// 0x3F -Complement carry flag
struct ComplementCarryFlag final : public Opcode {
  public:
    ComplementCarryFlag() : Opcode(1, 4, 0x3F, "CCF") {}
    void execute(CPU *cpu, MMU *mmu) override {
        cpu->set_flag(CPU::Flag::N, false);
        cpu->set_flag(CPU::Flag::H, false);
        cpu->set_flag(CPU::Flag::C, !cpu->flag_is_set(CPU::Flag::C));
    }
};

// 0xF3 Disable interrupt
struct DisableInterrupt final : public Opcode {
  public:
    DisableInterrupt() : Opcode(1, 4, 0xF3, "DI") {}
    void execute(CPU *cpu, MMU *mmu) override { cpu->set_interrupt_enable(false); }
};

// 0xF3 Enable interrupt
struct EnableInterrupt final : public Opcode {
  public:
    EnableInterrupt() : Opcode(1, 4, 0xFB, "EI") {}
    void execute(CPU *cpu, MMU *mmu) override { cpu->set_interrupt_enable(true); }
};

// 0xF9 - Load HL into SP
struct LoadSPWithHL final : public Opcode {

  public:
    LoadSPWithHL() : Opcode(1, 8, 0xF9, "LD SP, HL") {}
    void execute(CPU *cpu, MMU *mmu) override { cpu->set_register(CPU::Register::SP, cpu->get_16_bit_register(CPU::Register::HL)); }
};

// Call, returns and jumps common
struct ConditionalCallReturnOrJumpBase : public Opcode {
  protected:
    ConditionalCallReturnOrJumpBase(uint8_t opcode) : Opcode() { identifier = opcode; }

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

    void execute_call(CPU *cpu, MMU *mmu, uint16_t *target_address) {
        uint16_t pc = cpu->get_16_bit_register(CPU::Register::PC);
        uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);

        cpu->add_offset_to_sp(-2);
        (void)mmu->try_map_data_to_memory((uint8_t *)&pc, sp, 2);
        cpu->set_register(CPU::Register::PC, *target_address);
    }

    void execute_return(CPU *cpu, MMU *mmu) {
        uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);

        uint8_t *data = new uint8_t[2];
        (void)mmu->try_read_from_memory(data, sp, 2);

        cpu->set_register(CPU::Register::PC, static_cast<uint16_t>(data[1] << 8 | data[0]));
        cpu->add_offset_to_sp(2);
    }

    void execute_jump(CPU *cpu, uint16_t *target_address) { cpu->set_register(CPU::Register::PC, *target_address); }

    enum class Condition { None = -1, NotZero = 0, Zero = 1, NotCarry = 2, Carry = 3 };
    const std::unordered_map<ConditionalCallReturnOrJumpBase::Condition, std::string> ConditionNameMap = {
        { ConditionalCallReturnOrJumpBase::Condition::None, "" },   { ConditionalCallReturnOrJumpBase::Condition::NotZero, "NZ" },
        { ConditionalCallReturnOrJumpBase::Condition::Zero, "Z" },  { ConditionalCallReturnOrJumpBase::Condition::NotCarry, "NC" },
        { ConditionalCallReturnOrJumpBase::Condition::Carry, "C" },
    };

    ConditionalCallReturnOrJumpBase::Condition m_condition;
};

// 0xE9 - Jump to address pointed to be HL
struct JumpToAddressInHL final : public ConditionalCallReturnOrJumpBase {
  public:
    JumpToAddressInHL() : ConditionalCallReturnOrJumpBase(0xE9) {
        size = 1;
        cycles = 4;
        name = "JP (HL)";
    }

    void execute(CPU *cpu, MMU *) override {
        uint16_t target_address = cpu->get_16_bit_register(CPU::Register::HL);
        execute_jump(cpu, &target_address);
    }
};

// Jump to immediate address
struct JumpToImmediate final : public ConditionalCallReturnOrJumpBase {

  public:
    JumpToImmediate(uint8_t opcode) : ConditionalCallReturnOrJumpBase(opcode) {
        size = 3;
        if (opcode == UnconditionalJumpOpcode) {
            m_condition = ConditionalCallReturnOrJumpBase::Condition::None;
        } else {
            uint8_t flag_idx = (opcode >> 3) & 0x07;
            m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx);
        }

        m_flag_to_test_name = ConditionNameMap.find(m_condition)->second;
        name =
            m_condition == ConditionalCallReturnOrJumpBase::Condition::None ? "JP a16" : GeneralUtilities::formatted_string("JP %s, a16", m_flag_to_test_name);
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[1] << 8 | data[0];
        m_disassembled_instruction = m_condition == ConditionalCallReturnOrJumpBase::Condition::None
                                         ? GeneralUtilities::formatted_string("JP 0x%X", m_data)
                                         : GeneralUtilities::formatted_string("JP %s, 0x%X", m_flag_to_test_name, m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override {
        if (condition_is_met(cpu)) {
            cycles = 16;
            execute_jump(cpu, &m_data);
        } else {
            cycles = 12;
        }
    }

  private:
    const uint8_t UnconditionalJumpOpcode = 0xC3;
    std::string m_flag_to_test_name;
    std::string m_disassembled_instruction;
    uint16_t m_data;
};

// Relative jumps from immediate
struct RelativeJump final : public ConditionalCallReturnOrJumpBase {

  public:
    RelativeJump(uint8_t opcode) : ConditionalCallReturnOrJumpBase(opcode) {
        size = 2;
        uint8_t flag_idx = (opcode >> 3) & 0x07;
        m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx - 4);
        m_flag_to_test_name = ConditionNameMap.find(m_condition)->second;
        name = m_condition == ConditionalCallReturnOrJumpBase::Condition::None ? "JR d8" : GeneralUtilities::formatted_string("JR %s, d8", m_flag_to_test_name);
    }

    void set_opcode_data(uint8_t *data) override {
        memcpy(&m_jump_offset, data, 1);
        m_disassembled_instruction = m_condition == ConditionalCallReturnOrJumpBase::Condition::None
                                         ? GeneralUtilities::formatted_string("JR 0x%X", data[0])
                                         : GeneralUtilities::formatted_string("JR %s, 0x%X", m_flag_to_test_name, data[0]);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override {
        if (condition_is_met(cpu)) {
            cycles = 12;
            auto target_address = static_cast<uint16_t>(cpu->get_16_bit_register(CPU::Register::PC) + m_jump_offset);
            execute_jump(cpu, &target_address);
        } else {
            cycles = 8;
        }
    }

  private:
    std::string m_flag_to_test_name;
    std::string m_disassembled_instruction;
    int8_t m_jump_offset;
};

// Call instructions
struct Call final : public ConditionalCallReturnOrJumpBase {

  public:
    Call(uint8_t opcode) : ConditionalCallReturnOrJumpBase(opcode) {
        size = 3;
        if (opcode == UnconditionalCallOpcode) {
            m_condition = ConditionalCallReturnOrJumpBase::Condition::None;
        } else {
            uint8_t flag_idx = (opcode >> 3) & 0x07;
            m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx);
        }
        m_flag_to_test_name = ConditionNameMap.find(m_condition)->second;
        name = m_condition == ConditionalCallReturnOrJumpBase::Condition::None ? "CALL a16"
                                                                               : GeneralUtilities::formatted_string("CALL %s, a16", m_flag_to_test_name);
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[1] << 8 | data[0];
        m_disassembled_instruction = m_condition == ConditionalCallReturnOrJumpBase::Condition::None
                                         ? GeneralUtilities::formatted_string("CALL 0x%X", m_data)
                                         : GeneralUtilities::formatted_string("CALL %s, 0x%X", m_flag_to_test_name, m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override {
        if (condition_is_met(cpu)) {
            cycles = 24;
            execute_call(cpu, mmu, &m_data);
        } else {
            cycles = 12;
        }
    }

  private:
    const uint8_t UnconditionalCallOpcode = 0xCD;
    uint16_t m_data;
    std::string m_flag_to_test_name;
    std::string m_disassembled_instruction;
};

// Return instructions
struct ReturnFromCall final : public ConditionalCallReturnOrJumpBase {

  public:
    ReturnFromCall(uint8_t opcode) : ConditionalCallReturnOrJumpBase(opcode) {
        size = 1;
        if ((opcode & 0x0F) == 0x09) {
            m_condition = ConditionalCallReturnOrJumpBase::Condition::None;
            m_enable_interrupts = (opcode >> 4) == 0x0D;
            name = m_enable_interrupts ? "RETI" : "RET";
        } else {
            uint8_t flag_idx = (opcode >> 3) & 0x07;
            m_condition = static_cast<ConditionalCallReturnOrJumpBase::Condition>(flag_idx);
            auto flag_to_test_name = ConditionNameMap.find(m_condition)->second;
            name = GeneralUtilities::formatted_string("RET %s", flag_to_test_name);
        }
    }

    void execute(CPU *cpu, MMU *mmu) override {
        if (condition_is_met(cpu)) {
            cycles = m_condition == ConditionalCallReturnOrJumpBase::Condition::None ? 16 : 20;
            execute_return(cpu, mmu);

            if (m_enable_interrupts)
                cpu->set_interrupt_enable(true);

        } else {
            cycles = 8;
        }
    }

  private:
    bool m_enable_interrupts = false;
};

// Reset instruction
struct Reset final : public ConditionalCallReturnOrJumpBase {
  public:
    Reset(uint8_t opcode) : ConditionalCallReturnOrJumpBase(opcode) {
        size = 1;
        cycles = 16;
        m_reset_target = opcode - 0xC7; // Note: these opcodes are spaced 0x08 apart and the reset vectors are 0x00, 0x08, 0x10, ...
        name = GeneralUtilities::formatted_string("RST %02XH", m_reset_target);
    }

    void execute(CPU *cpu, MMU *mmu) override { execute_call(cpu, mmu, &m_reset_target); }

  private:
    uint16_t m_reset_target;
};

// Load 8-bit register from immediate
struct Load8bitImmediate final : public Opcode {

  public:
    Load8bitImmediate(uint8_t opcode) : Opcode(2) {

        uint8_t register_idx = (opcode >> 3) & 0x07;
        m_target = CPU::register_map[register_idx];

        m_target_name = CPU::register_name.find(m_target)->second;

        identifier = opcode;
        name = GeneralUtilities::formatted_string((m_target == CPU::Register::HL) ? "LD (%s), d8" : "LD %s, d8", m_target_name);
        cycles = m_target == CPU::Register::HL ? 12 : 8;
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[0];
        m_disassembled_instruction =
            GeneralUtilities::formatted_string((m_target == CPU::Register::HL) ? "LD (%s), 0x%X" : "LD %s, 0x%X", m_target_name, m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override { cpu->set_register(m_target, m_data); }

  private:
    CPU::Register m_target;
    std::string m_target_name;
    std::string m_disassembled_instruction;
    uint8_t m_data;
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

        auto target_name = CPU::register_name.find(m_target)->second;
        auto source_name = CPU::register_name.find(m_source)->second;

        identifier = opcode;

        if (m_target == CPU::Register::HL)
            name = GeneralUtilities::formatted_string("LD (%s), %s", target_name, source_name);
        else if (m_source == CPU::Register::HL)
            name = GeneralUtilities::formatted_string("LD %s, (%s)", target_name, source_name);
        else
            name = GeneralUtilities::formatted_string("LD %s, %s", target_name, source_name);

        cycles = (m_target == CPU::Register::HL || m_source == CPU::Register::HL) ? 8 : 4;
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint8_t source_value;
        if (m_source == CPU::Register::HL)
            (void)mmu->try_read_from_memory(&source_value, cpu->get_16_bit_register(CPU::Register::HL), 1);
        else
            source_value = cpu->get_8_bit_register(m_source);

        if (m_target == CPU::Register::HL)
            (void)mmu->try_map_data_to_memory(&source_value, cpu->get_16_bit_register(CPU::Register::HL), 1);
        else
            cpu->set_register(m_target, source_value);
    }

  private:
    CPU::Register m_target;
    CPU::Register m_source;
};

// Load 16-bit register from immediate
struct Load16bitImmediate final : public Opcode {

  public:
    Load16bitImmediate(uint8_t opcode) : Opcode(3, 12, opcode) {
        uint8_t register_idx = (opcode >> 4) & 0x03;

        m_target = CPU::wide_register_map[register_idx];
        m_target_name = CPU::register_name.find(m_target)->second;

        name = GeneralUtilities::formatted_string("LD %s, d16", m_target_name);
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[1] << 8 | data[0];
        m_disassembled_instruction = GeneralUtilities::formatted_string("LD %s, 0x%X", m_target_name, m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }
    void execute(CPU *cpu, MMU *mmu) override { cpu->set_register(m_target, m_data); }

  private:
    CPU::Register m_target;
    std::string m_target_name;
    std::string m_disassembled_instruction;
    uint16_t m_data;
};

// Load 16-bit register indirect
struct Load16bitIndirect final : public Opcode {

  public:
    Load16bitIndirect(uint8_t opcode) : Opcode(1, 8, opcode) {
        m_target_is_accumulator = ((opcode >> 3) & 0x01) == 1;
        uint8_t target_selector = (opcode >> 4) & 0x03;
        m_target_source = CPU::wide_register_map[target_selector];

        // Target cannot be SP, both values maps to HL but with either increment or decrement
        m_hl_offset = m_target_source == CPU::Register::HL ? 1 : m_target_source == CPU::Register::SP ? -1 : 0;
        m_target_source = m_target_source == CPU::Register::SP ? CPU::Register::HL : m_target_source;

        auto target_name = CPU::register_name.find(m_target_source)->second;
        auto hl_inc_or_dec = m_hl_offset == 0 ? "" : (m_hl_offset == 1 ? "+" : "-");
        auto accumulator_name = CPU::register_name.find(CPU::Register::A)->second;

        name = m_target_is_accumulator ? GeneralUtilities::formatted_string("LD %s, (%s%s)", accumulator_name, target_name, hl_inc_or_dec)
                                       : GeneralUtilities::formatted_string("LD (%s%s), %s", target_name, hl_inc_or_dec, accumulator_name);
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint8_t data_to_load;

        if (m_target_is_accumulator) {
            (void)mmu->try_read_from_memory(&data_to_load, cpu->get_16_bit_register(m_target_source), 1);
            cpu->set_register(CPU::Register::A, data_to_load);
        } else {
            data_to_load = cpu->get_8_bit_register(CPU::Register::A);
            (void)mmu->try_map_data_to_memory(&data_to_load, cpu->get_16_bit_register(m_target_source), 1);
        }

        if (m_target_source == CPU::Register::HL) {
            cpu->set_register(CPU::Register::HL, static_cast<uint16_t>(cpu->get_16_bit_register(CPU::Register::HL) + m_hl_offset));
        }
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
        identifier = opcode;
        m_is_16_bit = (opcode & 0x03) == 0x03;

        if (m_is_16_bit) {
            m_target = CPU::wide_register_map[(opcode >> 4) & 0x03];
            m_operation = ((opcode >> 3) & 0x01) == 0 ? IncrementOrDecrement8Or16bit::Operation::Increment : IncrementOrDecrement8Or16bit::Operation::Decrement;
            cycles = 8;
        } else {
            m_target = CPU::register_map[(opcode >> 3) & 0x07];
            m_operation = (opcode & 0x01) == 0 ? IncrementOrDecrement8Or16bit::Operation::Increment : IncrementOrDecrement8Or16bit::Operation::Decrement;
            cycles = m_target == CPU::Register::HL ? 12 : 4;
        }

        auto operation_name = s_operation_name_map[static_cast<int>(m_operation)];
        auto target_name = CPU::register_name.find(m_target)->second;
        name = GeneralUtilities::formatted_string((m_target == CPU::Register::HL && !m_is_16_bit) ? "%s (%s)" : "%s %s", operation_name, target_name);
    }

    void execute(CPU *cpu, MMU *mmu) override {
        if (m_is_16_bit)
            execute_16_bit_operation(cpu);
        else
            execute_8_bit_operation(cpu, mmu);
    }

  private:
    enum class Operation { Increment, Decrement };
    static inline std::string s_operation_name_map[] = { "INC", "DEC" };
    CPU::Register m_target;
    IncrementOrDecrement8Or16bit::Operation m_operation;
    bool m_is_16_bit;

    void execute_16_bit_operation(CPU *cpu) {
        uint16_t value = cpu->get_16_bit_register(m_target);
        uint16_t change = m_operation == IncrementOrDecrement8Or16bit::Operation::Increment ? 1 : -1;
        cpu->set_register(m_target, static_cast<uint16_t>(value + change));
    }

    void execute_8_bit_operation(CPU *cpu, MMU *mmu) {
        uint8_t value;
        if (m_target == CPU::Register::HL)
            (void)mmu->try_read_from_memory(&value, cpu->get_16_bit_register(CPU::Register::HL), 1);
        else
            value = cpu->get_8_bit_register(m_target);

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
    }
};

// Register operations common
struct RegisterOperationBase : public Opcode {

  protected:
    RegisterOperationBase(uint8_t opcode, uint8_t size) : Opcode(size) {
        identifier = opcode;
        set_operation_type(opcode);
    }
    std::string get_operation_name() const { return m_operations_name.find(m_operation)->second; }

    void execute_operation(CPU *cpu, uint8_t *operand) {

        uint8_t accumulator_value = cpu->get_8_bit_register(CPU::Register::A);
        uint8_t result;
        bool *flag_pattern; // Z, N, H, C

        switch (m_operation) {
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
        default:
            NOT_IMPLEMENTED(name);
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

        auto target_name = CPU::register_name.find(m_operand_register)->second;
        name = m_operand_register == CPU::Register::HL ? GeneralUtilities::formatted_string("%s (%s)", get_operation_name(), target_name)
                                                       : GeneralUtilities::formatted_string("%s %s", get_operation_name(), target_name);
        cycles = m_operand_register == CPU::Register::HL ? 8 : 4;
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint8_t operand_value;
        if (m_operand_register == CPU::Register::HL)
            (void)mmu->try_read_from_memory(&operand_value, cpu->get_16_bit_register(CPU::Register::HL), 1);
        else
            operand_value = cpu->get_8_bit_register(m_operand_register);

        execute_operation(cpu, &operand_value);
    }

  private:
    CPU::Register m_operand_register;
};

// Operate on accumulator with immediate
struct AccumulatorOperation final : RegisterOperationBase {

  public:
    AccumulatorOperation(uint8_t opcode) : RegisterOperationBase(opcode, 2) {
        cycles = 8;
        name = GeneralUtilities::formatted_string("%s d8", get_operation_name());
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[0];
        m_disassembled_instruction = GeneralUtilities::formatted_string("%s 0x%X", get_operation_name(), m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override { execute_operation(cpu, &m_data); }

    uint8_t m_data;
    std::string m_disassembled_instruction;
};

// 16-bit add
struct Add16bitRegister final : public Opcode {

  public:
    Add16bitRegister(uint8_t opcode) : Opcode(1, 8, opcode) {
        m_target = CPU::wide_register_map[(opcode >> 4) & 0x03];
        name = GeneralUtilities::formatted_string("ADD HL, %s", CPU::register_name.find(m_target)->second);
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint16_t v = cpu->get_16_bit_register(m_target);
        uint16_t hl = cpu->get_16_bit_register(CPU::Register::HL);

        cpu->set_register(CPU::Register::HL, static_cast<uint16_t>(hl + v));

        cpu->set_flag(CPU::Flag::H, cpu->half_carry_occurs_on_add(hl, v));
        cpu->set_flag(CPU::Flag::N, false);
        cpu->set_flag(CPU::Flag::C, cpu->carry_occurs_on_add(hl, v));
    }

  private:
    CPU::Register m_target;
};

// Read/Write IO-port C from/to A
struct ReadWriteIOPortCWithA final : public Opcode {

  public:
    ReadWriteIOPortCWithA(uint8_t opcode) : Opcode(1, 8, opcode) {
        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x04)
            m_type = ReadWriteIOPortCWithA::ActionType::Write;
        else if (type_idx == 0x06)
            m_type = ReadWriteIOPortCWithA::ActionType::Read;
        else
            throw std::invalid_argument("Read/Write type identifier invalid");

        name = m_type == ReadWriteIOPortCWithA::ActionType::Write ? "LD ($FF00 + C), A" : "LD A, ($FF00 + C)";
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint32_t target_address = 0xFF00 + cpu->get_8_bit_register(CPU::Register::C);
        if (m_type == ReadWriteIOPortCWithA::ActionType::Write) {
            uint8_t reg_a = cpu->get_8_bit_register(CPU::Register::A);
            (void)mmu->try_map_data_to_memory(&reg_a, target_address, 1);
        } else {
            uint8_t value;
            (void)mmu->try_read_from_memory(&value, target_address, 1);
            cpu->set_register(CPU::Register::A, value);
        }
    }

  private:
    enum class ActionType { Read, Write };

    ReadWriteIOPortCWithA::ActionType m_type;
};

// Read/Write IO-port n from/to A
struct ReadWriteIOPortNWithA final : public Opcode {

  public:
    ReadWriteIOPortNWithA(uint8_t opcode) : Opcode(2, 12, opcode) {
        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x04)
            m_type = ReadWriteIOPortNWithA::ActionType::Write;
        else if (type_idx == 0x06)
            m_type = ReadWriteIOPortNWithA::ActionType::Read;
        else
            throw std::invalid_argument("Read/Write type identifier invalid");

        name = m_type == ReadWriteIOPortNWithA::ActionType::Write ? "LD ($FF00 + a8), A" : "LD A, ($FF00 + a8)";
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[0];
        m_disassembled_instruction = m_type == ReadWriteIOPortNWithA::ActionType::Write ? GeneralUtilities::formatted_string("LD ($%X), A", 0xFF00 + m_data)
                                                                                        : GeneralUtilities::formatted_string("LD A, ($%X)", 0xFF00 + m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override {
        uint32_t target_address = 0xFF00 + m_data;
        if (m_type == ReadWriteIOPortNWithA::ActionType::Write) {
            uint8_t reg_a = cpu->get_8_bit_register(CPU::Register::A);
            (void)mmu->try_map_data_to_memory(&reg_a, target_address, 1);
        } else {
            uint8_t value;
            (void)mmu->try_read_from_memory(&value, target_address, 1);
            cpu->set_register(CPU::Register::A, value);
        }
    }

  private:
    enum class ActionType { Read, Write };

    ReadWriteIOPortNWithA::ActionType m_type;
    std::string m_disassembled_instruction;
    uint8_t m_data;
};

// Add or subtract from stackpointer and store in HL or SP
struct SetSPOrHLToSPAndOffset final : public Opcode {

  public:
    SetSPOrHLToSPAndOffset(uint8_t opcode) : Opcode(2) {
        identifier = opcode;
        m_target = (opcode & 0xF0) == 0xE0 ? CPU::Register::SP : CPU::Register::HL;
        cycles = m_target == CPU::Register::SP ? 16 : 12;
        name = m_target == CPU::Register::SP ? "ADD SP, d8" : "LD HL, SP + d8";
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[0];
        memcpy(&m_offset, data, 1);
        m_disassembled_instruction = GeneralUtilities::formatted_string(m_target == CPU::Register::SP ? "ADD SP, %02X" : "LD HL, SP + %02X", m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override {
        uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);

        cpu->set_flag(CPU::Flag::Z, 0);
        cpu->set_flag(CPU::Flag::N, 0);
        cpu->set_flag(CPU::Flag::H, cpu->half_carry_occurs_on_add(static_cast<uint8_t>(sp & 0x00FF), m_data));
        cpu->set_flag(CPU::Flag::C, cpu->carry_occurs_on_add(static_cast<uint8_t>(sp & 0x00FF), m_data));

        cpu->set_register(m_target, static_cast<uint16_t>(sp + m_offset));
    }

  private:
    CPU::Register m_target;
    std::string m_disassembled_instruction;
    int8_t m_offset;
    uint8_t m_data;
};

// Load from or set A indirect
struct LoadFromOrSetAIndirect final : public Opcode {

  public:
    LoadFromOrSetAIndirect(uint8_t opcode) : Opcode(3, 16, opcode) {
        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x05)
            m_type = LoadFromOrSetAIndirect::Direction::FromAccumulator;
        else if (type_idx == 0x07)
            m_type = LoadFromOrSetAIndirect::Direction::ToAccumulator;
        else
            throw std::invalid_argument("Direction type identifier invalid");

        name = m_type == LoadFromOrSetAIndirect::Direction::FromAccumulator ? "LD (a16), A" : "LD A, (a16)";
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[1] << 8 | data[0];
        m_disassembled_instruction = m_type == LoadFromOrSetAIndirect::Direction::FromAccumulator ? GeneralUtilities::formatted_string("LD (0x%X), A", m_data)
                                                                                                  : GeneralUtilities::formatted_string("LD A, (0x%X)", m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override {
        if (m_type == LoadFromOrSetAIndirect::Direction::FromAccumulator) {
            uint8_t reg_a = cpu->get_8_bit_register(CPU::Register::A);
            (void)mmu->try_map_data_to_memory(&reg_a, m_data, 1);
        } else {
            uint8_t value;
            (void)mmu->try_read_from_memory(&value, m_data, 1);
            cpu->set_register(CPU::Register::A, value);
        }
    }

  private:
    enum class Direction { FromAccumulator, ToAccumulator };

    LoadFromOrSetAIndirect::Direction m_type;
    std::string m_disassembled_instruction;
    uint16_t m_data;
};

// 16-bit push
struct Push16bitRegister final : public Opcode {
  public:
    Push16bitRegister(uint8_t opcode) : Opcode(1, 16, opcode) {
        uint8_t source_selector = (opcode >> 4) & 0x03;
        m_source = CPU::wide_register_map[source_selector];

        // Target cannot be SP, top value should map to AF
        m_source = m_source == CPU::Register::SP ? CPU::Register::AF : m_source;
        name = GeneralUtilities::formatted_string("PUSH %s", CPU::register_name.find(m_source)->second);
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint16_t src = cpu->get_16_bit_register(m_source);
        uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);

        cpu->add_offset_to_sp(-2);
        (void)mmu->try_map_data_to_memory((uint8_t *)&src, sp, 2);
    }

  private:
    CPU::Register m_source;
};

// 16-bit pop
struct Pop16bitRegister final : public Opcode {
  public:
    Pop16bitRegister(uint8_t opcode) : Opcode(1, 12, opcode) {
        uint8_t target_selector = (opcode >> 4) & 0x03;
        m_target = CPU::wide_register_map[target_selector];

        // Target cannot be SP, top value should map to AF
        m_target = m_target == CPU::Register::SP ? CPU::Register::AF : m_target;
        name = GeneralUtilities::formatted_string("POP %s", CPU::register_name.find(m_target)->second);
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);

        uint8_t *data = new uint8_t[2];
        (void)mmu->try_read_from_memory(data, sp, 2);

        cpu->set_register(m_target, static_cast<uint16_t>(data[1] << 8 | data[0]));
        cpu->add_offset_to_sp(2);
    }

  private:
    CPU::Register m_target;
};

// Extended opcodes, rotations, shifts, swap, bit tests, set and reset
struct ExtendedOpcode final : public Opcode {
  public:
    ExtendedOpcode(uint8_t opcode) : Opcode(1) {

        uint8_t extended_op_code_type_idx = (opcode >> 6) & 0x03;
        uint8_t bit_or_rotation_idx = (opcode >> 3) & 0x07;
        uint8_t target_idx = opcode & 0x07;

        m_target = CPU::register_map[target_idx];
        m_type = s_extended_opcode_type_map[extended_op_code_type_idx];

        auto target_name = CPU::register_name.find(m_target)->second;
        if (m_type == ExtendedOpcode::ExtendedOpcodeType::RotationShiftOrSwap) {

            m_rot_type = s_rotations_type_map[bit_or_rotation_idx];
            auto operation_name = RotationsTypeName.find(m_rot_type)->second;

            name = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s (%s)", operation_name, target_name)
                                                 : GeneralUtilities::formatted_string("%s %s", operation_name, target_name);
        } else {
            m_bit = bit_or_rotation_idx;
            auto operation_name = ExtendedOpcodeTypeName.find(m_type)->second;
            name = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s %d, (%s)", operation_name, m_bit, target_name)
                                                 : GeneralUtilities::formatted_string("%s %d, %s", operation_name, m_bit, target_name);
        }
        identifier = opcode;
        cycles = m_target == CPU::Register::HL ? 16 : 8;
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint8_t current_register;
        if (m_target == CPU::Register::HL)
            (void)mmu->try_read_from_memory(&current_register, cpu->get_16_bit_register(m_target), 1);
        else
            current_register = cpu->get_8_bit_register(m_target);

        switch (m_type) {

        case ExtendedOpcode::ExtendedOpcodeType::RotationShiftOrSwap:
            perform_rotation_shift_or_swap(cpu, &current_register);
            cpu->set_register(m_target, current_register);
            break;

        case ExtendedOpcode::ExtendedOpcodeType::Test:
            cpu->set_flag(CPU::Flag::Z, !bit_is_set(current_register, m_bit));
            cpu->set_flag(CPU::Flag::N, false);
            cpu->set_flag(CPU::Flag::H, true);
            break;

        case ExtendedOpcode::ExtendedOpcodeType::Reset:
            current_register &= (0xFE << m_bit);
            cpu->set_register(m_target, current_register);
            break;

        case ExtendedOpcode::ExtendedOpcodeType::Set:
            current_register |= (0x01 << m_bit);
            cpu->set_register(m_target, current_register);
            break;
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
        RotateLeft,              // C <- [7 <- 0] <- [7]
        RotateRight,             // [0] -> [7 -> 0] -> C
        RotateLeftThroughCarry,  // C <- [7 <- 0] <- C
        RotateRightThroughCarry, // C -> [7 -> 0] -> C
        ShiftLeftArithmetic,     // C <- [7 <- 0] <- 0
        ShiftRightArithmetic,    // [7] -> [7 -> 0] -> C
        SwapNibbles,
        ShiftRightLogic, // 0 -> [7 -> 0] -> C
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

    void perform_rotation_shift_or_swap(CPU *cpu, uint8_t *data) {

        uint8_t carry_flag = cpu->flag_is_set(CPU::Flag::C) ? 0x01 : 0x00;
        switch (m_rot_type) {

        case RotationShiftOrSwapType::RotateLeftThroughCarry:
            cpu->set_flag(CPU::Flag::C, ((*data >> 7) & 0x01) == 0x01);
            *data = (*data << 1) + carry_flag;
            cpu->set_flag(CPU::Flag::Z, *data == 0x00);
            break;

        default:
            NOT_IMPLEMENTED(name);
        }

        cpu->set_flag(CPU::Flag::N, false);
        cpu->set_flag(CPU::Flag::H, false);
    }
    bool bit_is_set(const uint8_t &data, int bit_to_test) { return ((data >> bit_to_test) & 0x01) == 1; }
};

// Rotate accumulator
struct RotateAccumulator final : Opcode {

  public:
    RotateAccumulator(uint8_t opcode) : Opcode(1, 4, opcode) {

        // These instructions are coded identically to the extended ones targeting the
        // accumulator, so we can decode the instruction in the same way

        m_extended_opcode = std::make_unique<ExtendedOpcode>(opcode);
        name = m_extended_opcode->name;
        auto ws = name.find(' ');
        name.replace(ws, 1, "");
    }

    void execute(CPU *cpu, MMU *mmu) override {
        m_extended_opcode->execute(cpu, mmu);
        cpu->set_flag(CPU::Flag::Z, false);
    }

  private:
    std::unique_ptr<ExtendedOpcode> m_extended_opcode;
};
}