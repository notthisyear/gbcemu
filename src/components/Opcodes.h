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
    Opcode(const std::string &name, uint8_t size, uint8_t cycles, uint8_t identifier) : name(name), size(size), cycles(cycles), identifier(identifier) {}
    Opcode(uint8_t size, uint8_t cycles, uint8_t identifier) : size(size), cycles(cycles), identifier(identifier) {}
    Opcode(uint8_t size, uint8_t cycles) : size(size), cycles(cycles) {}
    Opcode(uint8_t size) : size(size) {}
};

// 0x00 - NoOp
struct NoOperation final : public Opcode {
  public:
    static const uint8_t byte_identifier = 0x00;
    NoOperation() : Opcode("NOP", 1, 1, byte_identifier) {}
};

// 0x08 Store SP at addresses given by 16-bit immediate
struct StoreStackpointer final : public Opcode {
  public:
    static const uint8_t byte_identifier = 0x08;
    StoreStackpointer() : Opcode("LD (a16), SP", 3, 5, byte_identifier) {}

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
    static const uint8_t byte_identifier = 0x10;
    Stop() : Opcode("STOP 0", 2, 2, byte_identifier) {}
};

// 0x66 - Halts to CPU (low-power mode until interrupt)
struct Halt final : public Opcode {
  public:
    static const uint8_t byte_identifier = 0x66;
    Halt() : Opcode("HALT", 1, 1, byte_identifier) {}
};

// 0xCD - Unconditional call
struct CallUnconditional final : public Opcode {
  public:
    static const uint8_t byte_identifier = 0xCD;
    CallUnconditional() : Opcode("CALL", 3, 6, byte_identifier) {}

    void set_opcode_data(uint8_t *data) override {
        m_data = data[1] << 8 | data[0];
        m_disassembled_instruction = GeneralUtilities::formatted_string("CALL 0x%X", m_data);
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint16_t pc = cpu->get_16_bit_register(CPU::Register::PC);
        uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);

        cpu->add_offset_to_sp(-2);
        (void)mmu->try_map_data_to_memory((uint8_t *)&pc, sp - 2, 2);
        cpu->set_register(CPU::Register::PC, m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

  private:
    uint16_t m_data;
    std::string m_disassembled_instruction;
};

// Relative jumps from immediate
struct RelativeJump final : public Opcode {

  public:
    RelativeJump(uint8_t opcode) : Opcode(2) {
        m_flag_idx = (opcode >> 3) & 0x07;
        auto flag_to_test_name = m_flag_to_test_name_map.find(m_flag_idx);
        if (flag_to_test_name == m_flag_to_test_name_map.end())
            throw std::invalid_argument("Opcode flag idx is not valid");

        identifier = opcode;
        m_flag_to_test_name = flag_to_test_name->second;
        name = GeneralUtilities::formatted_string("JR %s d8", m_flag_to_test_name);
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[0];
        memcpy(&m_jump_offset, &m_data, 1);
        m_disassembled_instruction = GeneralUtilities::formatted_string("JR %s 0x%X", m_flag_to_test_name, m_data);
    }

    std::string fully_disassembled_instruction() const override { return m_disassembled_instruction; }

    void execute(CPU *cpu, MMU *mmu) override {
        if (cpu->flag_is_set(CPU::Flag::Z)) {
            cycles = 2;
        } else {
            cycles = 3;
            cpu->add_offset_to_pc(m_jump_offset);
        }
    }

  private:
    uint8_t m_flag_idx;
    std::string m_flag_to_test_name;
    std::string m_disassembled_instruction;
    uint8_t m_data;
    int8_t m_jump_offset;

    const std::unordered_map<uint8_t, std::string> m_flag_to_test_name_map = {
        { 3, "" }, { 4, "NZ," }, { 5, "Z," }, { 6, "NC," }, { 7, "C," },
    };
};

// Load 8-bit register from immediate
struct Load8bitImmediate final : public Opcode {

  public:
    Load8bitImmediate(uint8_t opcode) : Opcode(2) {

        uint8_t register_idx = (opcode >> 3) & 0x07;
        auto it = CPU::register_map.find(register_idx);

        if (it == CPU::register_map.end())
            throw std::invalid_argument("Opcode target is not defined");

        m_target = it->second;
        m_target_name = CPU::register_name.find(m_target)->second;

        identifier = opcode;
        name = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("LD (%s), d8", m_target_name)
                                             : GeneralUtilities::formatted_string("LD %s, d8", m_target_name);
        cycles = m_target == CPU::Register::HL ? 3 : 2;
    }

    void set_opcode_data(uint8_t *data) override {
        m_data = data[0];
        m_disassembled_instruction = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("LD (%s), 0x%X", m_target_name, m_data)
                                                                   : GeneralUtilities::formatted_string("LD %s, 0x%X", m_target_name, m_data);
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

        auto it_target = CPU::register_map.find(target_register_idx);
        if (it_target == CPU::register_map.end())
            throw std::invalid_argument("Opcode target register is not defined");

        auto it_source = CPU::register_map.find(source_register_idx);
        if (it_source == CPU::register_map.end())
            throw std::invalid_argument("Opcode source register is not defined");

        m_target = it_target->second;
        m_source = it_source->second;

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

        cycles = (m_target == CPU::Register::HL || m_source == CPU::Register::HL) ? 2 : 1;
    }

    static bool is_halt_instruction(uint8_t opcode) { return opcode == 0x66; }

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
    Load16bitImmediate(uint8_t opcode) : Opcode(3, 3, opcode) {
        uint8_t register_idx = (opcode >> 4) & 0x03;
        auto it = CPU::wide_register_map.find(register_idx);

        if (it == CPU::wide_register_map.end())
            throw std::invalid_argument("Opcode target is not defined");

        m_target = it->second;
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
    Load16bitIndirect(uint8_t opcode) : Opcode(1, 2, opcode) {
        m_target_is_accumulator = ((opcode >> 3) & 0x01) == 1;
        uint8_t target_selector = (opcode >> 4) & 0x03;
        auto it = CPU::wide_register_map.find(target_selector);

        if (it == CPU::wide_register_map.end())
            throw std::invalid_argument("Opcode target is not defined");
        m_target_source = it->second;

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

// 8-bit increment/decrement
struct IncrementDecrement8Bit final : public Opcode {
  public:
    IncrementDecrement8Bit(uint8_t opcode) : Opcode(1) {
        uint8_t target_idx = (opcode >> 3) & 0x07;
        uint8_t type_idx = opcode & 0x07;

        if (type_idx == 0x04)
            m_type = CPU::ArithmeticOperation::Increment;
        else if (type_idx == 0x05)
            m_type = CPU::ArithmeticOperation::Decrement;
        else
            throw std::invalid_argument("Increment/Decrement type identifier invalid");

        auto it_target = CPU::register_map.find(target_idx);
        if (it_target == CPU::register_map.end())
            throw std::invalid_argument("Opcode target is not defined");

        m_target = it_target->second;

        identifier = opcode;

        auto target_name = CPU::register_name.find(m_target)->second;
        auto action_name = CPU::arithmetic_operation_name.find(m_type)->second;

        name = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s (%s)", action_name, target_name)
                                             : GeneralUtilities::formatted_string("%s %s", action_name, target_name);
        cycles = m_target == CPU::Register::HL ? 3 : 1;
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint8_t value;
        if (m_target == CPU::Register::HL)
            (void)mmu->try_read_from_memory(&value, cpu->get_16_bit_register(CPU::Register::HL), 1);
        else
            value = cpu->get_8_bit_register(m_target);

        bool half_carry_flag =
            m_type == CPU::ArithmeticOperation::Increment ? cpu->half_carry_occurs_on_add(value, 1) : cpu->half_carry_occurs_on_subtract(value, 1);

        uint8_t result = m_type == CPU::ArithmeticOperation::Increment ? value + 1 : value - 1;

        if (m_target == CPU::Register::HL)
            (void)mmu->try_map_data_to_memory(&result, cpu->get_16_bit_register(CPU::Register::HL), 1);
        else
            cpu->set_register(m_target, result);

        cpu->set_flag(CPU::Flag::Z, result == 0x00);
        cpu->set_flag(CPU::Flag::N, m_type == CPU::ArithmeticOperation::Decrement);
        cpu->set_flag(CPU::Flag::H, half_carry_flag);
    }

  private:
    enum class ActionType { Increment, Decrement };

    CPU::ArithmeticOperation m_type;
    CPU::Register m_target;
};

// Register operations
struct RegisterOperation final : public Opcode {

  public:
    RegisterOperation(uint8_t opcode) : Opcode(1) {
        uint8_t operation_type = (opcode >> 3) & 0x07;
        uint8_t register_idx = opcode & 0x07;

        auto it_op = m_register_operations_map.find(operation_type);
        if (it_op == m_register_operations_map.end())
            throw std::invalid_argument("Register operation is not defined");

        auto it_reg = CPU::register_map.find(register_idx);
        if (it_reg == CPU::register_map.end())
            throw std::invalid_argument("Opcode target is not defined");

        m_operation = it_op->second;
        m_target = it_reg->second;
        auto op_name = m_register_operations_name.find(m_operation)->second;
        auto target_name = CPU::register_name.find(m_target)->second;

        identifier = opcode;
        name = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s (%s)", op_name, target_name)
                                             : GeneralUtilities::formatted_string("%s %s", op_name, target_name);
        cycles = m_target == CPU::Register::HL ? 2 : 1;
    }

    void execute(CPU *cpu, MMU *mmu) override {

        uint8_t accumulator_value = cpu->get_8_bit_register(CPU::Register::A);
        uint8_t target_value = cpu->get_8_bit_register(m_target);

        switch (m_operation) {
        case RegisterOperation::Operation::AndWithAccumulator:
            cpu->set_register(CPU::Register::A, static_cast<uint8_t>(accumulator_value ^ target_value));
            cpu->set_flag(CPU::Flag::Z, cpu->get_8_bit_register(CPU::Register::A) == 0x00);
            cpu->set_flag(CPU::Flag::N, false);
            cpu->set_flag(CPU::Flag::H, true);
            cpu->set_flag(CPU::Flag::C, false);
            break;

        case RegisterOperation::Operation::XorWithAccumulator:
            cpu->set_register(CPU::Register::A, static_cast<uint8_t>(accumulator_value ^ target_value));
            cpu->set_flag(CPU::Flag::Z, cpu->get_8_bit_register(CPU::Register::A) == 0x00);
            cpu->set_flag(CPU::Flag::N, false);
            cpu->set_flag(CPU::Flag::H, false);
            cpu->set_flag(CPU::Flag::C, false);
            break;

        case RegisterOperation::Operation::OrWithAccumulator:
            cpu->set_register(CPU::Register::A, static_cast<uint8_t>(accumulator_value ^ target_value));
            cpu->set_flag(CPU::Flag::Z, cpu->get_8_bit_register(CPU::Register::A) == 0x00);
            cpu->set_flag(CPU::Flag::N, false);
            cpu->set_flag(CPU::Flag::H, false);
            cpu->set_flag(CPU::Flag::C, false);
            break;

        default:
            NOT_IMPLEMENTED(name);
            break;
        }
    }

  private:
    enum class Operation {
        AddToAccumulator,
        AddToAccumulatorWithCarry,
        SubtractFromAccumulator,
        SubtractFromAccumulatorWithCarry,
        AndWithAccumulator,
        XorWithAccumulator,
        OrWithAccumulator,
        CompareWithAccumulator,
    };

    static inline std::unordered_map<uint8_t, RegisterOperation::Operation> m_register_operations_map = {
        { 0, Operation::AddToAccumulator },        { 1, Operation::AddToAccumulatorWithCarry },
        { 2, Operation::SubtractFromAccumulator }, { 3, Operation::SubtractFromAccumulatorWithCarry },
        { 4, Operation::AndWithAccumulator },      { 5, Operation::XorWithAccumulator },
        { 6, Operation::OrWithAccumulator },       { 7, Operation::CompareWithAccumulator },
    };

    static inline std::unordered_map<RegisterOperation::Operation, std::string> m_register_operations_name = {
        { Operation::AddToAccumulator, "ADD A," },     { Operation::AddToAccumulatorWithCarry, "ADC A," },
        { Operation::SubtractFromAccumulator, "SUB" }, { Operation::SubtractFromAccumulatorWithCarry, "SBC A," },
        { Operation::AndWithAccumulator, "AND," },     { Operation::XorWithAccumulator, "XOR" },
        { Operation::OrWithAccumulator, "OR," },       { Operation::CompareWithAccumulator, "CP" },
    };

    RegisterOperation::Operation m_operation;
    CPU::Register m_target;
};

// Read/Write IO-port C from/to A
struct ReadWriteIOPortCWithA final : public Opcode {

  public:
    ReadWriteIOPortCWithA(uint8_t opcode) : Opcode(1, 2, opcode) {
        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x04)
            m_type = ReadWriteIOPortCWithA::ActionType::Write;
        else if (type_idx == 0x06)
            m_type = ReadWriteIOPortCWithA::ActionType::Read;
        else
            throw std::invalid_argument("Read/Write type identifier invalid");

        name = m_type == ReadWriteIOPortCWithA::ActionType::Write ? "LD ($FF00 + C), A" : "LD A, ($FF00 + C), A";
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
    ReadWriteIOPortNWithA(uint8_t opcode) : Opcode(2, 3, opcode) {
        uint8_t type_idx = (opcode >> 3) & 0x07;
        if (type_idx == 0x04)
            m_type = ReadWriteIOPortNWithA::ActionType::Write;
        else if (type_idx == 0x06)
            m_type = ReadWriteIOPortNWithA::ActionType::Read;
        else
            throw std::invalid_argument("Read/Write type identifier invalid");

        name = m_type == ReadWriteIOPortNWithA::ActionType::Write ? "LD ($FF00 + a8), A" : "LD A, ($FF00 + a8), A";
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

// 16-bit push
struct Push16bitRegister final : public Opcode {
  public:
    Push16bitRegister(uint8_t opcode) : Opcode(1, 4, opcode) {
        uint8_t source_selector = (opcode >> 4) & 0x03;
        auto it = CPU::wide_register_map.find(source_selector);

        if (it == CPU::wide_register_map.end())
            throw std::invalid_argument("Opcode source is not defined");
        m_source = it->second;

        // Target cannot be SP, top value should map to AF
        m_source = m_source == CPU::Register::HL ? CPU::Register::AF : m_source;
        name = GeneralUtilities::formatted_string("PUSH %s", CPU::register_name.find(m_source)->second);
    }

    void execute(CPU *cpu, MMU *mmu) override {
        uint16_t src = cpu->get_16_bit_register(m_source);
        uint16_t sp = cpu->get_16_bit_register(CPU::Register::SP);

        cpu->add_offset_to_sp(-2);
        (void)mmu->try_map_data_to_memory((uint8_t *)&src, sp - 2, 2);
    }

  private:
    CPU::Register m_source;
};

// 16-bit pop
struct Pop16bitRegister final : public Opcode {
  public:
    Pop16bitRegister(uint8_t opcode) : Opcode(1, 3, opcode) {
        uint8_t target_selector = (opcode >> 4) & 0x03;
        auto it = CPU::wide_register_map.find(target_selector);

        if (it == CPU::wide_register_map.end())
            throw std::invalid_argument("Opcode target is not defined");
        m_target = it->second;

        // Target cannot be SP, top value should map to AF
        m_target = m_target == CPU::Register::HL ? CPU::Register::AF : m_target;
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

        auto it_type = m_extended_opcode_type_map.find(extended_op_code_type_idx);
        if (it_type == m_extended_opcode_type_map.end())
            throw std::invalid_argument("Extended opcode type is not defined");

        auto it_target = CPU::register_map.find(target_idx);
        if (it_target == CPU::register_map.end())
            throw std::invalid_argument("Extended opcode target is not defined");

        m_type = it_type->second;
        m_target = it_target->second;

        auto target_name = CPU::register_name.find(m_target)->second;
        if (m_type == ExtendedOpcode::ExtendedOpcodeType::RotationShiftOrSwap) {

            auto it_rot = m_rotations_type_map.find(bit_or_rotation_idx);
            if (it_rot == m_rotations_type_map.end())
                throw std::invalid_argument("Opcode rotation type is not defined");

            m_rot_type = it_rot->second;
            auto operation_name = m_rotations_type_name.find(m_rot_type)->second;

            name = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s (%s)", operation_name, target_name)
                                                 : GeneralUtilities::formatted_string("%s %s", operation_name, target_name);
        } else {
            m_bit = bit_or_rotation_idx;
            auto operation_name = m_extended_opcode_type_name.find(m_type)->second;
            name = m_target == CPU::Register::HL ? GeneralUtilities::formatted_string("%s %d, (%s)", operation_name, m_bit, target_name)
                                                 : GeneralUtilities::formatted_string("%s %d, %s", operation_name, m_bit, target_name);
        }
        identifier = opcode;
        cycles = m_target == CPU::Register::HL ? 4 : 2;
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

    static inline std::unordered_map<uint8_t, ExtendedOpcodeType> m_extended_opcode_type_map = {
        { 0, ExtendedOpcodeType::RotationShiftOrSwap },
        { 1, ExtendedOpcodeType::Test },
        { 2, ExtendedOpcodeType::Reset },
        { 3, ExtendedOpcodeType::Set },
    };

    static inline std::unordered_map<ExtendedOpcodeType, std::string> m_extended_opcode_type_name = {
        { ExtendedOpcodeType::RotationShiftOrSwap, "ROT" },
        { ExtendedOpcodeType::Test, "BIT" },
        { ExtendedOpcodeType::Reset, "RES" },
        { ExtendedOpcodeType::Set, "SET" },
    };

    static inline std::unordered_map<uint8_t, RotationShiftOrSwapType> m_rotations_type_map = {
        { 0, RotationShiftOrSwapType::RotateLeft },
        { 1, RotationShiftOrSwapType::RotateRight },
        { 2, RotationShiftOrSwapType::RotateLeftThroughCarry },
        { 3, RotationShiftOrSwapType::RotateRightThroughCarry },
        { 4, RotationShiftOrSwapType::ShiftLeftArithmetic },
        { 5, RotationShiftOrSwapType::ShiftRightArithmetic },
        { 6, RotationShiftOrSwapType::SwapNibbles },
        { 7, RotationShiftOrSwapType::ShiftRightLogic },
    };

    static inline std::unordered_map<RotationShiftOrSwapType, std::string> m_rotations_type_name = {
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
    RotateAccumulator(uint8_t opcode) : Opcode(1, 1, opcode) {

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