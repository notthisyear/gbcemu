#include "components/CPU.h"
#include "components/MMU.h"
#include "components/OpcodeBuilder.h"
#include "components/Opcodes.h"
#include "util/GeneralUtilities.h"
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

typedef struct {
    uint8_t identifier;
    std::string name;
    uint8_t size;
    uint8_t *instruction_data;
    std::function<void(gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t *)> execution_test;
} ExpectedOpcode;

enum class FlagAction {
    Set,
    NotSet,
    None,
};
enum class ExtendedOperationSubType {
    Set,
    Reset,
    Test,
};
static FlagAction no_flag_action[] = { FlagAction::None, FlagAction::None, FlagAction::None, FlagAction::None };

static void print_error() {
    std::cout << "\033[1;31m[error]"
              << "\033[0m ";
}

static void print_warning() {
    std::cout << "\033[1;33m[warning]"
              << "\033[0m ";
}

static void print_opcode_passed(const std::string &opcode_name) {
    std::cout << "\033[1;32m[pass]"
              << "\033[0m '" << opcode_name << "' matches" << std::endl;
}

static void assert_equal(uint8_t expected, uint8_t actual, const std::string &prop) {
    if (expected != actual) {
        print_error();
        std::cout << "Expected '" << unsigned(expected) << "', got '" << unsigned(actual) << "' (" << prop << ")" << std::endl;
        exit(1);
    }
}

static void assert_equal(uint16_t expected, uint16_t actual, const std::string &prop) {
    if (expected != actual) {
        print_error();
        std::cout << "Expected '" << unsigned(expected) << "', got '" << unsigned(actual) << "' (" << prop << ")" << std::endl;
        exit(1);
    }
}

static void assert_equal(const std::string &expected, const std::string &actual, const std::string &prop) {
    if (expected.compare(actual) != 0) {
        print_error();
        std::cout << "Expected '" << expected << "' got '" << actual << "' (" << prop << ")" << std::endl;
        exit(1);
    }
}

static void assert_equal(bool expected, bool actual, const std::string &prop) {
    if (expected != actual) {
        print_error();
        std::cout << "Expected '" << (expected ? "true" : "false") << "' got '" << (actual ? "true" : "false") << "' (" << prop << ")" << std::endl;
        exit(1);
    }
}

static void map_instruction_data_to_memory(gbcemu::MMU *mmu, const std::vector<uint8_t> &d, uint16_t offset = 0x0000) {
    auto data = new uint8_t[d.size()];
    for (int i = 0; i < d.size(); i++)
        data[i] = d.at(i);
    (void)mmu->try_map_data_to_memory(data, offset, d.size());
    delete[] data;
}

static void ensure_correct_flag_action(bool new_value, bool previous_value, FlagAction action, const std::string &flag_name) {
    if (action == FlagAction::None)
        assert_equal(previous_value, new_value, flag_name);

    if (action == FlagAction::Set)
        assert_equal(true, new_value, flag_name);

    if (action == FlagAction::NotSet)
        assert_equal(false, new_value, flag_name);
}

static uint8_t execute_opcode_verify_pc_and_flags(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint16_t expected_pc,
                                                  FlagAction *flag_actions = no_flag_action, uint16_t pc_at_start = 0x0000) {
    opcode->reset_state();
    auto z_flag = cpu->flag_is_set(gbcemu::CPU::Flag::Z);
    auto n_flag = cpu->flag_is_set(gbcemu::CPU::Flag::N);
    auto h_flag = cpu->flag_is_set(gbcemu::CPU::Flag::H);
    auto c_flag = cpu->flag_is_set(gbcemu::CPU::Flag::C);
    cpu->set_register(gbcemu::CPU::Register::PC, pc_at_start);

    uint8_t tick_ctr = 0;
    while (!opcode->is_done()) {
        tick_ctr++;
        if (tick_ctr % 4 == 0)
            opcode->tick_execution(cpu, mmu);
    }

    ensure_correct_flag_action(cpu->flag_is_set(gbcemu::CPU::Flag::Z), z_flag, flag_actions[0], "Z");
    ensure_correct_flag_action(cpu->flag_is_set(gbcemu::CPU::Flag::N), n_flag, flag_actions[1], "N");
    ensure_correct_flag_action(cpu->flag_is_set(gbcemu::CPU::Flag::H), h_flag, flag_actions[2], "H");
    ensure_correct_flag_action(cpu->flag_is_set(gbcemu::CPU::Flag::C), c_flag, flag_actions[3], "C");
    assert_equal(expected_pc, cpu->get_16_bit_register(gbcemu::CPU::Register::PC), "PC");

    return tick_ctr;
}

static void test_relative_jumps(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data, bool has_condition = false,
                                bool execute_if_set = true, gbcemu::CPU::Flag flag = gbcemu::CPU::Flag::Z) {
    map_instruction_data_to_memory(mmu, { instruction_data[0] });
    int8_t offset;
    memcpy(&offset, instruction_data, 1);
    uint8_t tick_ctr;

    if (!has_condition) {
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1 + offset);
        assert_equal(12, tick_ctr, "cycles [no condition]");
    } else {
        cpu->set_flag(flag, !execute_if_set);
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1);
        assert_equal(8, tick_ctr, "cycles [condition not met]");

        map_instruction_data_to_memory(mmu, { instruction_data[0] });

        cpu->set_flag(flag, execute_if_set);
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1 + offset);
        assert_equal(12, tick_ctr, "cycles [condition met]");
    }
}

static void test_load_16_bit(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data, gbcemu::CPU::Register target) {
    map_instruction_data_to_memory(mmu, { instruction_data[0], instruction_data[1] });

    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 2);

    assert_equal(static_cast<uint16_t>(instruction_data[1] << 8 | instruction_data[0]), cpu->get_16_bit_register(target),
                 gbcemu::CPU::register_name.find(target)->second);
    assert_equal(12, tick_ctr, "cycles");
}

static void test_load_memory_from_acc(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register source_register,
                                      int source_change = 0) {
    uint8_t value_to_load = 0xFA;
    uint16_t address_to_write_to = 0xFF80;

    cpu->set_register(gbcemu::CPU::Register::A, value_to_load);
    cpu->set_register(source_register, address_to_write_to);

    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);

    uint8_t actual_value;
    (void)mmu->try_read_from_memory(&actual_value, address_to_write_to, 1);

    assert_equal(value_to_load, actual_value, "value");
    assert_equal(static_cast<uint16_t>(address_to_write_to + source_change), cpu->get_16_bit_register(source_register),
                 gbcemu::CPU::register_name.find(source_register)->second);
    assert_equal(8, tick_ctr, "cycles");
}

static void test_increment_or_decrement_16_bit(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register reg, bool increment) {
    uint16_t original_value = 0xA071;
    cpu->set_register(reg, original_value);

    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
    assert_equal(static_cast<uint16_t>(original_value + (increment ? 1 : -1)), cpu->get_16_bit_register(reg), "value");
    assert_equal(8, tick_ctr, "cycles");
}

static void test_increment_or_decrement_8_bit(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register reg, uint8_t start_value,
                                              std::vector<FlagAction> flags, bool increment = true) {
    uint16_t hl_value = 0xC081;
    bool indirect_from_hl = reg == gbcemu::CPU::Register::HL;

    if (indirect_from_hl) {
        bool result = mmu->try_map_data_to_memory(&start_value, hl_value, 1);
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);
    } else {
        cpu->set_register(reg, start_value);
    }

    FlagAction *flag_action = new FlagAction[4]{ flags.at(0), flags.at(1), flags.at(2), flags.at(3) };
    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0, flag_action);
    delete[] flag_action;

    uint8_t actual_value;
    if (indirect_from_hl) {
        (void)mmu->try_read_from_memory(&actual_value, hl_value, 1);
    } else {
        actual_value = cpu->get_8_bit_register(reg);
    }

    assert_equal(static_cast<uint8_t>(start_value + (increment ? 1 : -1)), actual_value, "value");
    assert_equal(indirect_from_hl ? 12 : 4, tick_ctr, "cycles");
}

static void test_load_8bit_from_immediate(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data, gbcemu::CPU::Register target) {
    map_instruction_data_to_memory(mmu, { instruction_data[0] });
    uint16_t hl_value = 0xFFF0;
    bool indirect_from_hl = target == gbcemu::CPU::Register::HL;

    if (indirect_from_hl)
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);

    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1);

    uint8_t actual;
    if (indirect_from_hl) {
        (void)mmu->try_read_from_memory(&actual, hl_value, 1);
    } else {
        actual = cpu->get_8_bit_register(target);
    }

    assert_equal(instruction_data[0], actual, "value");
    assert_equal(indirect_from_hl ? 12 : 8, tick_ctr, "cycles");
}

static void test_add_16bit(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register source, uint16_t hl_value, uint16_t value_to_add,
                           uint16_t expected_result, std::vector<FlagAction> flags) {

    cpu->set_register(source, value_to_add);
    cpu->set_register(gbcemu::CPU::Register::HL, hl_value);

    FlagAction *flag_action = new FlagAction[4]{ flags.at(0), flags.at(1), flags.at(2), flags.at(3) };
    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0, flag_action);
    delete[] flag_action;

    assert_equal(expected_result, cpu->get_16_bit_register(gbcemu::CPU::Register::HL), "value");
    assert_equal(8, tick_ctr, "cycles");
}

static void test_load_acc_indirect(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register source_register, int source_change = 0) {
    uint8_t value_to_load = 0xE2;
    uint16_t address_in_source = 0xFF85;
    cpu->set_register(source_register, address_in_source);
    (void)mmu->try_map_data_to_memory(&value_to_load, address_in_source, 1);

    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);

    assert_equal(value_to_load, cpu->get_8_bit_register(gbcemu::CPU::Register::A), "value");
    assert_equal(address_in_source + source_change, cpu->get_16_bit_register(source_register), "value");
    assert_equal(8, tick_ctr, "cycles");
}

static void test_load_register_from_register(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register source,
                                             gbcemu::CPU::Register target) {
    uint16_t hl_value = 0xC081;
    uint8_t value_to_load = static_cast<uint8_t>(std::rand() % 256);
    bool from_hl = source == gbcemu::CPU::Register::HL;
    bool to_hl = target == gbcemu::CPU::Register::HL;
    bool hl_involved = from_hl || to_hl;

    if (hl_involved) {
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);
        if (from_hl)
            (void)mmu->try_map_data_to_memory(&value_to_load, hl_value, 1);
        else if (source == gbcemu::CPU::Register::H || source == gbcemu::CPU::Register::L)
            value_to_load = cpu->get_8_bit_register(source);
        else
            cpu->set_register(source, value_to_load);
    } else {
        cpu->set_register(source, value_to_load);
    }

    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);

    uint8_t actual;
    if (hl_involved) {
        if (to_hl)
            (void)mmu->try_read_from_memory(&actual, hl_value, 1);
        else if (source == gbcemu::CPU::Register::H)
            actual = hl_value >> 8;
        else if (source == gbcemu::CPU::Register::L)
            actual = hl_value & 0x00FF;
        else
            actual = cpu->get_8_bit_register(target);
    } else {
        actual = cpu->get_8_bit_register(target);
    }

    assert_equal(value_to_load, actual, "value");
    assert_equal(hl_involved ? 8 : 4, tick_ctr, "cycles");
}

static void _test_arithmetic(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, std::vector<FlagAction> flags, uint8_t acc_value, bool include_carry,
                             bool carry_value, int expected_pc, uint8_t expected_result, uint8_t expected_cycles) {

    cpu->set_register(gbcemu::CPU::Register::A, acc_value);
    if (include_carry)
        cpu->set_flag(gbcemu::CPU::Flag::C, carry_value);

    FlagAction *flag_actions = new FlagAction[4]{ flags.at(0), flags.at(1), flags.at(2), flags.at(3) };
    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, expected_pc, flag_actions);
    delete[] flag_actions;

    assert_equal(expected_result, cpu->get_8_bit_register(gbcemu::CPU::Register::A), "value");
    assert_equal(expected_cycles, tick_ctr, "cycles");
}

static void test_add_8bit_arithmetic_from_reg(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t acc_value, uint8_t source_value,
                                              uint8_t expected_result, gbcemu::CPU::Register source, std::vector<FlagAction> flags, bool include_carry = false,
                                              bool carry_value = false) {
    if (source == gbcemu::CPU::Register::HL) {
        uint16_t hl_value = 0xC081;
        bool result = mmu->try_map_data_to_memory(&source_value, hl_value, 1);
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);
    } else {
        cpu->set_register(source, source_value);
    }

    _test_arithmetic(opcode, cpu, mmu, flags, acc_value, include_carry, carry_value, 0, expected_result, source == gbcemu::CPU::Register::HL ? 8 : 4);
}

static void test_add_8bit_arithmetic_from_immediate(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data, uint8_t acc_value,
                                                    uint8_t expected_result, std::vector<FlagAction> flags, bool include_carry = false,
                                                    bool carry_value = false) {
    map_instruction_data_to_memory(mmu, { instruction_data[0] });
    _test_arithmetic(opcode, cpu, mmu, flags, acc_value, include_carry, carry_value, 1, expected_result, 8);
}

static void test_return_instructions(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, bool has_condition = false, bool execute_if_set = true,
                                     gbcemu::CPU::Flag flag = gbcemu::CPU::Flag::Z, bool sets_interrupt = false) {

    uint16_t expected_pc = 0x00AA;
    uint16_t expected_sp = 0xFFFE;

    // Set up the stack
    auto lsb = (uint8_t)(expected_pc & 0x00FF);
    auto msb = (uint8_t)(expected_pc >> 8);
    mmu->try_map_data_to_memory(&lsb, 0xFFFC, 1);
    mmu->try_map_data_to_memory(&msb, 0xFFFD, 1);
    cpu->set_register(gbcemu::CPU::Register::SP, static_cast<uint16_t>(0xFFFC));

    uint8_t tick_ctr;
    if (!has_condition) {
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, expected_pc);
        assert_equal(16, tick_ctr, "cycles [no condition]");
        assert_equal(expected_sp, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
        if (sets_interrupt)
            assert_equal(true, cpu->interrupt_enabled(), "interrupt");
    } else {
        cpu->set_flag(flag, !execute_if_set);
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
        assert_equal(8, tick_ctr, "cycles [condition not met]");

        cpu->set_flag(flag, execute_if_set);
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, expected_pc);
        assert_equal(20, tick_ctr, "cycles [condition met]");
        assert_equal(expected_sp, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
    }
}

static void test_pop_instructions(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register target_register) {

    uint16_t value_at_stack = 0x12A0;
    uint16_t expected_sp = 0xFFFE;

    // Set up the stack
    auto lsb = (uint8_t)(value_at_stack & 0x00FF);
    auto msb = (uint8_t)(value_at_stack >> 8);
    mmu->try_map_data_to_memory(&lsb, 0xFFFC, 1);
    mmu->try_map_data_to_memory(&msb, 0xFFFD, 1);
    cpu->set_register(gbcemu::CPU::Register::SP, static_cast<uint16_t>(0xFFFC));

    uint8_t tick_ctr;
    if (target_register == gbcemu::CPU::Register::AF) {
        FlagAction *flags = new FlagAction[4]{ FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet };
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0, flags);
        delete[] flags;
    } else {
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
    }

    assert_equal(12, tick_ctr, "cycles");
    assert_equal(expected_sp, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
    assert_equal(value_at_stack, cpu->get_16_bit_register(target_register), "value");
}

static void test_jump_absolute(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data, bool has_condition,
                               bool execute_if_set = false, gbcemu::CPU::Flag flag = gbcemu::CPU::Flag::Z) {

    uint16_t expected_pc = instruction_data[1] << 8 | instruction_data[0];
    map_instruction_data_to_memory(mmu, { instruction_data[0], instruction_data[1] });
    uint8_t tick_ctr;

    if (!has_condition) {
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, expected_pc);
        assert_equal(16, tick_ctr, "cycles [no condition]");
    } else {
        cpu->set_flag(flag, !execute_if_set);
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 2);
        assert_equal(12, tick_ctr, "cycles [condition not met]");

        cpu->set_flag(flag, execute_if_set);
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, expected_pc);
        assert_equal(16, tick_ctr, "cycles [condition met]");
    }
}

static void test_call_instructions(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data, bool has_condition,
                                   bool execute_if_set = false, gbcemu::CPU::Flag flag = gbcemu::CPU::Flag::Z) {

    uint16_t initial_sp = 0xFFFE;
    cpu->set_register(gbcemu::CPU::Register::SP, static_cast<uint16_t>(initial_sp));
    uint16_t expected_sp = initial_sp - 2;
    uint16_t expected_value_at_stack = 0x0002;
    uint16_t expected_pc = instruction_data[1] << 8 | instruction_data[0];
    map_instruction_data_to_memory(mmu, { instruction_data[0], instruction_data[1] });

    uint8_t tick_ctr;
    if (!has_condition) {
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, expected_pc);

        uint8_t *stack_value = new uint8_t[2];
        (void)mmu->try_read_from_memory(stack_value, expected_sp, 2);
        uint16_t actual_value_at_stack = static_cast<uint16_t>(stack_value[1] << 8 | stack_value[0]);

        assert_equal(24, tick_ctr, "cycles [no condition]");
        assert_equal(expected_sp, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
        assert_equal(expected_value_at_stack, actual_value_at_stack, "value");
    } else {
        cpu->set_flag(flag, !execute_if_set);
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 2);
        assert_equal(12, tick_ctr, "cycles [condition not met]");

        cpu->set_flag(flag, execute_if_set);
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, expected_pc);

        uint8_t *stack_value = new uint8_t[2];
        (void)mmu->try_read_from_memory(stack_value, expected_sp, 2);
        uint16_t actual_value_at_stack = static_cast<uint16_t>(stack_value[1] << 8 | stack_value[0]);

        assert_equal(24, tick_ctr, "cycles [no condition]");
        assert_equal(expected_sp, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
        assert_equal(expected_value_at_stack, actual_value_at_stack, "value");
    }
}

static void test_push_instructions(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register source_register) {
    uint16_t initial_sp = 0xFFFE;
    uint16_t expected_value_at_stack = 0x1234;
    uint16_t expected_sp = initial_sp - 2;
    cpu->set_register(gbcemu::CPU::Register::SP, static_cast<uint16_t>(initial_sp));
    cpu->set_register(source_register, expected_value_at_stack);

    auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);

    uint8_t *stack_value = new uint8_t[2];
    (void)mmu->try_read_from_memory(stack_value, expected_sp, 2);
    uint16_t actual_value_at_stack = static_cast<uint16_t>(stack_value[1] << 8 | stack_value[0]);

    assert_equal(16, tick_ctr, "cycles");
    assert_equal(expected_sp, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
    assert_equal(expected_value_at_stack, actual_value_at_stack, "value");
}

static void test_reset_instructions(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint16_t reset_vector) {
    uint16_t initial_sp = 0xFFFE;
    cpu->set_register(gbcemu::CPU::Register::SP, static_cast<uint16_t>(initial_sp));
    uint16_t expected_sp = initial_sp - 2;
    uint16_t expected_value_at_stack = 0x0000;

    uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, reset_vector);

    uint8_t *stack_value = new uint8_t[2];
    (void)mmu->try_read_from_memory(stack_value, expected_sp, 2);
    uint16_t actual_value_at_stack = static_cast<uint16_t>(stack_value[1] << 8 | stack_value[0]);

    assert_equal(16, tick_ctr, "cycles");
    assert_equal(expected_sp, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
    assert_equal(expected_value_at_stack, actual_value_at_stack, "value");
}

static void test_set_reset_test_instructions(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, ExtendedOperationSubType type,
                                             gbcemu::CPU::Register target_register, uint8_t bit_to_test) {
    uint8_t value = static_cast<uint8_t>(std::rand() % 256);
    uint16_t hl_value = 0xC081;

    if (target_register == gbcemu::CPU::Register::HL) {
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);
        (void)mmu->try_map_data_to_memory(&value, hl_value, 1);
    } else {
        cpu->set_register(target_register, value);
    }

    uint8_t tick_ctr;
    if (type == ExtendedOperationSubType::Test) {
        FlagAction *flags = new FlagAction[4]{ ((((value >> bit_to_test) & 0x01) == 1) ? FlagAction::NotSet : FlagAction::Set), FlagAction::NotSet,
                                               FlagAction::Set, FlagAction::None };
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0, flags);
        delete[] flags;
    } else {
        tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
    }

    assert_equal(target_register == gbcemu::CPU::Register::HL ? 8 : 4, tick_ctr, "cycles");
    if (type == ExtendedOperationSubType::Set || type == ExtendedOperationSubType::Reset) {
        uint8_t actual_value;
        if (target_register == gbcemu::CPU::Register::HL)
            (void)mmu->try_read_from_memory(&actual_value, hl_value, 1);
        else
            actual_value = cpu->get_8_bit_register(target_register);
        assert_equal(type == ExtendedOperationSubType::Set ? true : false, ((actual_value >> bit_to_test) & 0x01) == 1, "value");
    }
}

static void test_swap_instruction(gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, gbcemu::CPU::Register target_register) {
    uint8_t value = static_cast<uint8_t>(std::rand() % 256);
    uint8_t lower_nibble = value & 0x0F;
    uint8_t upper_nibble = value & 0xF0;

    uint16_t hl_value = 0xC081;

    if (target_register == gbcemu::CPU::Register::HL) {
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);
        (void)mmu->try_map_data_to_memory(&value, hl_value, 1);
    } else {
        cpu->set_register(target_register, value);
    }

    FlagAction *flags = new FlagAction[4]{ (value == 0x00 ? FlagAction::Set : FlagAction::NotSet), FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet };
    auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0, flags);

    assert_equal(target_register == gbcemu::CPU::Register::HL ? 8 : 4, tick_ctr, "cycles");

    uint8_t actual_value;
    if (target_register == gbcemu::CPU::Register::HL)
        (void)mmu->try_read_from_memory(&actual_value, hl_value, 1);
    else
        actual_value = cpu->get_8_bit_register(target_register);

    assert_equal(lower_nibble << 4 | upper_nibble >> 4, actual_value, "value");
}

static ExpectedOpcode non_extended_opcodes[] = {
    // clang-format off
    // 0x00 - 0x3F
    { 0x00, "NOP", 1, nullptr, nullptr  },
    { 0x10, "STOP 0", 2, nullptr, nullptr },
    { 0x20, "JR NZ, d8", 2, new uint8_t[1] { 0xDE }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t  *instruction_data) {
        test_relative_jumps(opcode, cpu, mmu, instruction_data, true, false, gbcemu::CPU::Flag::Z);
    }},
    { 0x30, "JR NC, d8", 2, new uint8_t[1] { 0x2F }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_relative_jumps(opcode, cpu, mmu, instruction_data, true, false, gbcemu::CPU::Flag::C);
    }},

    { 0x01, "LD BC, d16", 3, new uint8_t[2] { 0xF5, 0x66 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_16_bit(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::BC);
    }},
    { 0x11, "LD DE, d16", 3, new uint8_t[2] { 0x18, 0x43 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_16_bit(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::DE);
    }},
    { 0x21, "LD HL, d16", 3, new uint8_t[2] { 0xEF, 0x92 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_16_bit(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::HL);
    }},
    { 0x31, "LD SP, d16", 3, new uint8_t[2] { 0x0E, 0x10 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_16_bit(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::SP);
    }},

    { 0x02, "LD (BC), A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_memory_from_acc(opcode, cpu, mmu, gbcemu::CPU::Register::BC);
    }},
    { 0x12, "LD (DE), A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_memory_from_acc(opcode, cpu, mmu, gbcemu::CPU::Register::DE);
    }},
    { 0x22, "LD (HL+), A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_memory_from_acc(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 1);
    }},
    { 0x32, "LD (HL-), A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_memory_from_acc(opcode, cpu, mmu, gbcemu::CPU::Register::HL, -1);
    }},

    { 0x03, "INC BC", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::BC, true);
    }} ,
    { 0x13, "INC DE", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::DE, true);
    }},
    { 0x23, "INC HL", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, true);
    }},
    { 0x33, "INC SP", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::SP, true);
    }},

    { 0x04, "INC B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::B, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::B, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::B, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
    }},
    { 0x14, "INC D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::D, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::D, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::D, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
     }},
    { 0x24, "INC H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::H, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::H, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::H, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
    }},
    { 0x34, "INC (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
     }},

    { 0x05, "DEC B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::B, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::B, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::B, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},
    { 0x15, "DEC D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::D, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::D, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::D, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},
    { 0x25, "DEC H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::H, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::H, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::H, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},
    { 0x35, "DEC (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},

    { 0x06, "LD B, d8", 2, new uint8_t[1] { 0xDE }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::B);
    }},
    { 0x16, "LD D, d8", 2, new uint8_t[1] { 0x02 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::D);
    }},
    { 0x26, "LD H, d8", 2, new uint8_t[1] { 0xAB }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::H);
    }},
    { 0x36, "LD (HL), d8", 2, new uint8_t[1] { 0x5E }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::HL);
    }},

    { 0x07, "RLCA", 1, nullptr, nullptr },
    { 0x17, "RLA", 1, nullptr, nullptr },
    { 0x27, "DAA", 1, nullptr, nullptr },
    { 0x37, "SCF", 1, nullptr, nullptr },

    { 0x08, "LD (a16), SP", 3, new uint8_t[2] { 0x10, 0xFE }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        uint16_t sp_value = 0xFFFE;
        cpu->set_register(gbcemu::CPU::Register::SP, sp_value);
        map_instruction_data_to_memory(mmu, { instruction_data[0], instruction_data[1] });
        uint8_t tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 2);

        auto actual = new uint8_t[2];
        (void)mmu->try_read_from_memory(actual, instruction_data[1] << 8 | instruction_data[0], 2);

        assert_equal(static_cast<uint8_t>(sp_value & 0x00FF), actual[0], "value [lb]");
        assert_equal(static_cast<uint8_t>(sp_value >> 8), actual[1], "value [hb]");
        assert_equal(20, tick_ctr, "cycles");
    }},
    { 0x18, "JR d8", 2, new uint8_t[1] { 0xAE }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_relative_jumps(opcode, cpu, mmu, instruction_data, false);
    }},
    { 0x28, "JR Z, d8", 2, new uint8_t[1] { 0x03 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_relative_jumps(opcode, cpu, mmu, instruction_data, true, true, gbcemu::CPU::Flag::Z);
    }},
    { 0x38, "JR C, d8", 2, new uint8_t[1] { 0x83 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_relative_jumps(opcode, cpu, mmu, instruction_data, true, true, gbcemu::CPU::Flag::C);
    }},

    { 0x09, "ADD HL, BC", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::BC, 0x8A23, 0x0605, 0x9028, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::BC, 0x8A23, 0x8A23, 0x1446, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::BC, 0x8A23, 0x0055, 0x8A78, { FlagAction::None, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    } },
    { 0x19, "ADD HL, DE", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::DE, 0x8A23, 0x0605, 0x9028, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::DE, 0x8A23, 0x8A23, 0x1446, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::DE, 0x8A23, 0x0055, 0x8A78, { FlagAction::None, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    } },
    { 0x29, "ADD HL, HL", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0x0F60, 0x0F60, 0x1EC0, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0x8A23, 0x8A23, 0x1446, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 0x0055, 0x0055, 0x00AA, { FlagAction::None, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    } },
    { 0x39, "ADD HL, SP", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::SP, 0x8A23, 0x0605, 0x9028, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::SP, 0x8A23, 0x8A23, 0x1446, { FlagAction::None, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
        test_add_16bit(opcode, cpu, mmu, gbcemu::CPU::Register::SP, 0x8A23, 0x0055, 0x8A78, { FlagAction::None, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    } },

    { 0x0A, "LD A, (BC)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_acc_indirect(opcode, cpu, mmu, gbcemu::CPU::Register::BC);
    }},
    { 0x1A, "LD A, (DE)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_acc_indirect(opcode, cpu, mmu, gbcemu::CPU::Register::DE);
    }},
    { 0x2A, "LD A, (HL+)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_acc_indirect(opcode, cpu, mmu, gbcemu::CPU::Register::HL, 1);
    }},
    { 0x3A, "LD A, (HL-)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_acc_indirect(opcode, cpu, mmu, gbcemu::CPU::Register::HL, -1);
    }},

    { 0x0B, "DEC BC", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::BC, false);
    }},
    { 0x1B, "DEC DE", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::DE, false);
    }},
    { 0x2B, "DEC HL", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::HL, false);
    }},
    { 0x3B, "DEC SP", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_16_bit(opcode, cpu, mmu, gbcemu::CPU::Register::SP, false);
    }},

    { 0x0C, "INC C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::C, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::C, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::C, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
     
    }},
    { 0x1C, "INC E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::E, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::E, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::E, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
     
    }},
    { 0x2C, "INC L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::L, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::L, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::L, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
     
    }},
    { 0x3C, "INC A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::A, 0x01, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::A, 0xFF, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::A, 0x0F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::None });
     
    }},

    { 0x0D, "DEC C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::C, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::C, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::C, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},
    { 0x1D, "DEC E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::E, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::E, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::E, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},
    { 0x2D, "DEC L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::L, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::L, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::L, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},
    { 0x3D, "DEC A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::A, 0x01, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::A, 0xFF, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::None }, false);
        test_increment_or_decrement_8_bit(opcode, cpu, mmu, gbcemu::CPU::Register::A, 0x10, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::None }, false);
    }},

    { 0x0E, "LD C, d8", 2, new uint8_t[1] { 0x01 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::C);
    }},
    { 0x1E, "LD E, d8", 2, new uint8_t[1] { 0xBB }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::E);
    }},
    { 0x2E, "LD L, d8", 2, new uint8_t[1] { 0xC4 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::L);
    }},
    { 0x3E, "LD A, d8", 2, new uint8_t[1] { 0x77 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_load_8bit_from_immediate(opcode, cpu, mmu, instruction_data, gbcemu::CPU::Register::A);
    }},

    { 0x0F, "RRCA", 1, nullptr, nullptr },
    { 0x1F, "RRA", 1, nullptr, nullptr },
    { 0x2F, "CPL", 1, nullptr, nullptr },
    { 0x3F, "CCF", 1, nullptr, nullptr },

    // 0x40 - 0x7F
    { 0x40, "LD B, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::B);
    }},
    { 0x50, "LD D, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::D);
    }},
    { 0x60, "LD H, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::H);
    }},
    { 0x70, "LD (HL), B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::HL);
    }},

    { 0x41, "LD B, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::B);
    }},
    { 0x51, "LD D, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::D);
    }},
    { 0x61, "LD H, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::H);
    }},
    { 0x71, "LD (HL), C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::HL);
    }},

    { 0x42, "LD B, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::B);
    }},
    { 0x52, "LD D, D", 1,  nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::D);
    }},
    { 0x62, "LD H, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::H);
    }},
    { 0x72, "LD (HL), D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::HL);
    }},

    { 0x43, "LD B, E", 1,  nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::B);
    }},
    { 0x53, "LD D, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::D);
    }},
    { 0x63, "LD H, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::H);
    }},
    { 0x73, "LD (HL), E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::HL);
    }},

    { 0x44, "LD B, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::B);
    }},
    { 0x54, "LD D, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::D);
    }},
    { 0x64, "LD H, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::H);
    }},
    { 0x74, "LD (HL), H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::HL);
    }},

    { 0x45, "LD B, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::B);
    }},
    { 0x55, "LD D, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::D);
    }},
    { 0x65, "LD H, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::H);
    }},
    { 0x75, "LD (HL), L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::HL);
    }},

    { 0x46, "LD B, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::HL, gbcemu::CPU::Register::B);
    }},
    { 0x56, "LD D, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::HL, gbcemu::CPU::Register::D);
    }},
    { 0x66, "LD H, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::HL, gbcemu::CPU::Register::H);
    }},
    { 0x76, "HALT", 1, nullptr, nullptr },

    { 0x47, "LD B, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::B);
    }},
    { 0x57, "LD D, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::D);
    }},
    { 0x67, "LD H, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::H);
    }},
    { 0x77, "LD (HL), A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::HL);
    }},

    { 0x48, "LD C, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::C);
    }},
    { 0x58, "LD E, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::E);
    }},
    { 0x68, "LD L, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::L);
    }},
    { 0x78, "LD A, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::B, gbcemu::CPU::Register::A);
    }},

    { 0x49, "LD C, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::C);
    }},
    { 0x59, "LD E, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::E);
    }},
    { 0x69, "LD L, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::L);
    }},
    { 0x79, "LD A, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::C, gbcemu::CPU::Register::A);
    }},

    { 0x4A, "LD C, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::C);
    }},
    { 0x5A, "LD E, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::E);
    }},
    { 0x6A, "LD L, D", 1,nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::L);
    }},
    { 0x7A, "LD A, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::D, gbcemu::CPU::Register::A);
    }},

    { 0x4B, "LD C, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::C);
    }},
    { 0x5B, "LD E, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::E);
    }},
    { 0x6B, "LD L, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::L);
    }},
    { 0x7B, "LD A, E", 1,nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::E, gbcemu::CPU::Register::A);
    }},

    { 0x4C, "LD C, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::C);
    }},
    { 0x5C, "LD E, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::E);
    }},
    { 0x6C, "LD L, H", 1,nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::L);
    }},
    { 0x7C, "LD A, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::H, gbcemu::CPU::Register::A);
    }},

    { 0x4D, "LD C, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::C);
    }},
    { 0x5D, "LD E, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::E);
    }},
    { 0x6D, "LD L, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::L);
    }},
    { 0x7D, "LD A, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::L, gbcemu::CPU::Register::A);
    }},

    { 0x4E, "LD C, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::HL, gbcemu::CPU::Register::C);
    }},
    { 0x5E, "LD E, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::HL, gbcemu::CPU::Register::E);
    }},
    { 0x6E, "LD L, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::HL, gbcemu::CPU::Register::L);
    }},
    { 0x7E, "LD A, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::HL, gbcemu::CPU::Register::A);
    }},

    { 0x4F, "LD C, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::C);
    }},
    { 0x5F, "LD E, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::E);
    }},
    { 0x6F, "LD L, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::L);
    }},
    { 0x7F, "LD A, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_load_register_from_register(opcode, cpu, mmu, gbcemu::CPU::Register::A, gbcemu::CPU::Register::A);
    }},

    // 0x80 - 0xBF
    { 0x80, "ADD A, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0x90, "SUB B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0xA0, "AND B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x3F, 0x1A, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x38, 0x18, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x00, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB0, "OR B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x2F, 0x7F, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x81, "ADD A, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0x91, "SUB C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0xA1, "AND C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x3F, 0x1A, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x38, 0x18, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x00, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB1, "OR C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x2F, 0x7F, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x82, "ADD A, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0x92, "SUB D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0xA2, "AND D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x3F, 0x1A, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x38, 0x18, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x00, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB2, "OR D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x2F, 0x7F, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x83, "ADD A, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0x93, "SUB E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0xA3, "AND E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x3F, 0x1A, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x38, 0x18, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x00, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB3, "OR E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x2F, 0x7F, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x84, "ADD A, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0x94, "SUB H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0xA4, "AND H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x3F, 0x1A, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x38, 0x18, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x00, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB4, "OR H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x2F, 0x7F, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x85, "ADD A, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0x95, "SUB L", 1,nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0xA5, "AND L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x3F, 0x1A, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x38, 0x18, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x00, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB5, "OR L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x2F, 0x7F, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x86, "ADD A, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0x96, "SUB (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0xA6, "AND (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x3F, 0x1A, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x38, 0x18, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x00, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB6, "OR (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x2F, 0x7F, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x87, "ADD A, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x05, 0x0A, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x0A, 0x14, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xCE, 0xCE, 0x9C, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x80, 0x80, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set });
    }},
    { 0x97, "SUB A", 1,nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xA7, "AND A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xB7, "OR A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x5A, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0x88, "ADC A, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0C, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x11, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC5, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0x98, "SBC A, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x3A, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);        
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x2A, 0x10, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x4F, 0xEB, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xA8, "XOR B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x0F, 0xF0, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x8A, 0x75, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xB8, "CP B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x2F, 0x3C, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::B, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x40, 0x3C, gbcemu::CPU::Register::B, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},

    { 0x89, "ADC A, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0C, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x11, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC5, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0x99, "SBC A, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x3A, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);        
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x2A, 0x10, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x4F, 0xEB, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xA9, "XOR C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x0F, 0xF0, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x8A, 0x75, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xB9, "CP C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x2F, 0x3C, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::C, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x40, 0x3C, gbcemu::CPU::Register::C, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},

    { 0x8A, "ADC A, D", 1,  nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0C, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x11, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC5, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0x9A, "SBC A, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x3A, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);        
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x2A, 0x10, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x4F, 0xEB, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xAA, "XOR D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x0F, 0xF0, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x8A, 0x75, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xBA, "CP D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x2F, 0x3C, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::D, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x40, 0x3C, gbcemu::CPU::Register::D, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},

    { 0x8B, "ADC A, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0C, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x11, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC5, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0x9B, "SBC A, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x3A, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);        
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x2A, 0x10, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x4F, 0xEB, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xAB, "XOR E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x0F, 0xF0, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x8A, 0x75, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xBB, "CP E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x2F, 0x3C, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::E, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x40, 0x3C, gbcemu::CPU::Register::E, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},

    { 0x8C, "ADC A, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0C, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x11, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC5, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0x9C, "SBC A, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x3A, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);        
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x2A, 0x10, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x4F, 0xEB, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xAC, "XOR H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x0F, 0xF0, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x8A, 0x75, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xBC, "CP H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x2F, 0x3C, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::H, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x40, 0x3C, gbcemu::CPU::Register::H, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},

    { 0x8D, "ADC A, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0C, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x11, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC5, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0x9D, "SBC A, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x3A, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);        
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x2A, 0x10, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x4F, 0xEB, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xAD, "XOR L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x0F, 0xF0, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x8A, 0x75, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xBD, "CP L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x2F, 0x3C, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::L, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x40, 0x3C, gbcemu::CPU::Register::L, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},

    { 0x8E, "ADC A, (HL)", 1,  nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0B, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x10, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC6, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x06, 0x0C, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x06, 0x11, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3A, 0xC5, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0x9E, "SBC A, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x0F, 0x2F, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x40, 0xFE, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x3A, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);        
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x2A, 0x10, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3B, 0x4F, 0xEB, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xAE, "XOR (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x0F, 0xF0, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0x8A, 0x75, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xBE, "CP (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x2F, 0x3C, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::HL, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x40, 0x3C, gbcemu::CPU::Register::HL, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
    }},

    { 0x8F, "ADC A, A", 1,  nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x05, 0x0A, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x0A, 0x14, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xCE, 0xCE, 0x9C, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x80, 0x80, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x05, 0x05, 0x0B, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x0A, 0x0A, 0x15, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xCE, 0xCE, 0x9D, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x80, 0x80, 0x01, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set }, true, true);
    }},
    { 0x9F, "SBC A, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);

        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3E, 0x3E, 0xFF, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0xFF, gbcemu::CPU::Register::A, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xAF, "XOR A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x5A, 0x5A, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x00, 0x00, 0x00, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xBF, "CP A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0xFF, 0xFF, 0xFF, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_reg(opcode, cpu, mmu, 0x3C, 0x3C, 0x3C, gbcemu::CPU::Register::A, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
    }},

    // 0xC0 - 0xFF
    { 0xC0, "RET NZ", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_return_instructions(opcode, cpu, mmu, true, false, gbcemu::CPU::Flag::Z);
    }},
    { 0xD0, "RET NC", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_return_instructions(opcode, cpu, mmu, true, false, gbcemu::CPU::Flag::C);
    }},
    { 0xE0, "LD ($FF00 + a8), A", 2, new uint8_t[1] { 0x10 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        uint8_t acc_value = 0x5B;
        uint16_t target_address = 0xFF00 + instruction_data[0];
        cpu->set_register(gbcemu::CPU::Register::A, acc_value);
        map_instruction_data_to_memory(mmu, { instruction_data[0] });

        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1);

        uint8_t actual_value;
        (void)mmu->try_read_from_memory(&actual_value, target_address, 1);
        assert_equal(12, tick_ctr, "cycles");
        assert_equal(acc_value, actual_value, "value");
    }},
    { 0xF0, "LD A, ($FF00 + a8)", 2, new uint8_t[1] { 0x11 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        uint8_t value_in_memory = 0x9F;
        uint16_t target_address = 0xFF00 + instruction_data[0];
        map_instruction_data_to_memory( mmu, { instruction_data[0] });
        (void)mmu->try_map_data_to_memory(&value_in_memory, target_address, 1);

        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1);
        assert_equal(12, tick_ctr, "cycles");
        assert_equal(value_in_memory, cpu->get_8_bit_register(gbcemu::CPU::Register::A), "value");
    }},

    { 0xC1, "POP BC", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_pop_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::BC);
    }},
    { 0xD1, "POP DE", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_pop_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::DE);
    }},
    { 0xE1, "POP HL", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_pop_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::HL);
    }},
    { 0xF1, "POP AF", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_pop_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::AF);
    }},

    { 0xC2, "JP NZ, a16", 3, new uint8_t[2] { 0x11, 0x45 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_jump_absolute(opcode, cpu, mmu, instruction_data, true, false, gbcemu::CPU::Flag::Z);
    }},
    { 0xD2, "JP NC, a16", 3, new uint8_t[2] { 0xFF, 0x08 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_jump_absolute(opcode, cpu, mmu, instruction_data, true, false, gbcemu::CPU::Flag::C);
    }},
    { 0xE2, "LD ($FF00 + C), A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        uint8_t value_in_reg_c = 0x10;
        uint8_t acc_value = 0x5B;
        uint16_t target_address = 0xFF00 + value_in_reg_c;
        cpu->set_register(gbcemu::CPU::Register::A, acc_value);
        cpu->set_register(gbcemu::CPU::Register::C, value_in_reg_c);

        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);

        uint8_t actual_value;
        (void)mmu->try_read_from_memory(&actual_value, target_address, 1);
        assert_equal(8, tick_ctr, "cycles");
        assert_equal(acc_value, actual_value, "value");
    }},
    { 0xF2, "LD A, ($FF00 + C)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        uint8_t value_in_memory = 0x9F;
        uint8_t value_in_reg_c = 0x11;
        uint16_t target_address = 0xFF00 + value_in_reg_c;
        (void)mmu->try_map_data_to_memory(&value_in_memory, target_address, 1);
        cpu->set_register(gbcemu::CPU::Register::C, value_in_reg_c);

        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
        assert_equal(8, tick_ctr, "cycles");
        assert_equal(value_in_memory, cpu->get_8_bit_register(gbcemu::CPU::Register::A), "value");
    }},

    { 0xC3, "JP a16", 3, new uint8_t[2] { 0x92, 0xA6 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_jump_absolute(opcode, cpu, mmu, instruction_data, false);
    }},
    { 0xD3, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xE3, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xF3, "DI", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t* ) {
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
        assert_equal(4, tick_ctr, "cycles");
        assert_equal(false, cpu->interrupt_enabled(), "interrupt");
    }},

    { 0xC4, "CALL NZ, a16", 3, new uint8_t[2] { 0x92, 0xA6 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_call_instructions(opcode, cpu, mmu, instruction_data, true, false, gbcemu::CPU::Flag::Z);
    }},
    { 0xD4, "CALL NC, a16", 3, new uint8_t[2] { 0x92, 0xA6 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_call_instructions(opcode, cpu, mmu, instruction_data, true, false, gbcemu::CPU::Flag::C);
    }},
    { 0xE4, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xF4, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid

    { 0xC5, "PUSH BC", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_push_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::BC);
    }},
    { 0xD5, "PUSH DE", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_push_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::DE);
    }},
    { 0xE5, "PUSH HL", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_push_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::HL);
    }},
    { 0xF5, "PUSH AF", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_push_instructions(opcode, cpu, mmu, gbcemu::CPU::Register::AF);
    }},

    { 0xC6, "ADD A, d8", 2, new uint8_t[] { 0x06 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*instruction_data)  {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x05, 0x0B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0A, 0x10, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0xFA, 0x00, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set });
    }},
    { 0xD6, "SUB d8", 2, new uint8_t[] { 0x0F }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data)  {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0F, 0x00, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x3E, 0x2F, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0A, 0xFB, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set });
    }},
    { 0xE6, "AND d8", 2, new uint8_t[] { 0x5A }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data)  {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x3F, 0x1A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x38, 0x18, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x00, 0x00, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet });
    }},
    { 0xF6, "OR d8", 2,new uint8_t[] { 0x5A }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data)  {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x5A, 0x5A, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x2F, 0x7F, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},

    { 0xC7, "RST 00H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0000);
    }},
    { 0xD7, "RST 10H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0010);
    }},
    { 0xE7, "RST 20H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0020);
    }},
    { 0xF7, "RST 30H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0030);
    }},

    { 0xC8, "RET Z", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_return_instructions(opcode, cpu, mmu, true, true, gbcemu::CPU::Flag::Z);
    }},
    { 0xD8, "RET C", 1,nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_return_instructions(opcode, cpu, mmu, true, true, gbcemu::CPU::Flag::C);
    }},
    { 0xE8, "ADD SP, d8", 2, new uint8_t[] { 0x02 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data)  {
        map_instruction_data_to_memory(mmu, { instruction_data[0] });
        cpu->set_register(gbcemu::CPU::Register::SP, static_cast<uint16_t>(0xFFEF));
        
        FlagAction* flags = new FlagAction[4] { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet };
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1, flags);
        delete[] flags;
        
        assert_equal(16, tick_ctr, "cycles");
        assert_equal(0xFFF1, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
    }},
    { 0xF8, "LD HL, SP + d8", 2, new uint8_t[] { 0xFA }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data)  {
        map_instruction_data_to_memory(mmu, { instruction_data[0] });
        int8_t offset;
        memcpy(&offset, instruction_data, 1);
        cpu->set_register(gbcemu::CPU::Register::SP, static_cast<uint16_t>(0xFFEF));
        
        uint16_t expected_result = cpu->get_16_bit_register(gbcemu::CPU::Register::SP) + offset;
        FlagAction* flags = new FlagAction[4] { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet };
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 1, flags);
        delete[] flags;
        
        assert_equal(12, tick_ctr, "cycles");
        assert_equal(expected_result, cpu->get_16_bit_register(gbcemu::CPU::Register::HL), "SP");
    }},

    { 0xC9, "RET", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_return_instructions(opcode, cpu, mmu, false);
    }},
    { 0xD9, "RETI", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_return_instructions(opcode, cpu, mmu, false, false, gbcemu::CPU::Flag::Z, true);
    }},
    { 0xE9, "JP (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {  
        uint16_t hl_value = 0xC081;
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, hl_value);

        assert_equal(4, tick_ctr, "cycles");
    }},
    { 0xF9, "LD SP, HL", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {  
        uint16_t hl_value = 0xC081;
        cpu->set_register(gbcemu::CPU::Register::HL, hl_value);
    
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
        assert_equal(8, tick_ctr, "cycles");
        assert_equal(hl_value, cpu->get_16_bit_register(gbcemu::CPU::Register::SP), "SP");
    }},

    { 0xCA, "JP Z, a16", 3, new uint8_t[2] { 0x11, 0x45 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_jump_absolute(opcode, cpu, mmu, instruction_data, true, true, gbcemu::CPU::Flag::Z);
    }},
    { 0xDA, "JP C, a16", 3, new uint8_t[2] { 0xFF, 0x08 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_jump_absolute(opcode, cpu, mmu, instruction_data, true, true, gbcemu::CPU::Flag::C);
    }},
    { 0xEA, "LD (a16), A", 3, new uint8_t[2] { 0x81, 0xC0 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        map_instruction_data_to_memory(mmu, { instruction_data[0], instruction_data[1] });
        uint8_t acc_value = 0x4B;
        cpu->set_register(gbcemu::CPU::Register::A, acc_value);

        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 2);

        uint8_t actual_value;
        (void)mmu->try_read_from_memory(&actual_value, static_cast<uint16_t>(instruction_data[1] << 8 | instruction_data[0]), 1);
        assert_equal(16, tick_ctr, "cycles");
        assert_equal(acc_value, actual_value, "value");
    }},
    { 0xFA, "LD A, (a16)", 3, new uint8_t[2] { 0x81, 0xC0 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        map_instruction_data_to_memory(mmu, { instruction_data[0], instruction_data[1] });
        uint8_t value_in_memory = 0x29;
        (void)mmu->try_map_data_to_memory(&value_in_memory, static_cast<uint16_t>(instruction_data[1] << 8 | instruction_data[0]), 1);

        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 2);

        assert_equal(16, tick_ctr, "cycles");
        assert_equal(value_in_memory, cpu->get_8_bit_register(gbcemu::CPU::Register::A), "value");
    }},

    { 0xCB, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // extended prefix, invalid as opcode
    { 0xDB, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xEB, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xFB, "EI", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t* ) {
        auto tick_ctr = execute_opcode_verify_pc_and_flags(opcode, cpu, mmu, 0);
        assert_equal(4, tick_ctr, "cycles");
        assert_equal(true, cpu->interrupt_enabled(), "interrupt");
    }},

    { 0xCC, "CALL Z, a16", 3, new uint8_t[2] { 0x92, 0xA6 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_call_instructions(opcode, cpu, mmu, instruction_data, true, true, gbcemu::CPU::Flag::Z);
    }},
    { 0xDC, "CALL C, a16", 3, new uint8_t[2] { 0x92, 0xA6 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_call_instructions(opcode, cpu, mmu, instruction_data, true, true, gbcemu::CPU::Flag::C);
    }},
    { 0xEC, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xFC, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid

    { 0xCD, "CALL a16", 3, new uint8_t[2] { 0x92, 0xA6 }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_call_instructions(opcode, cpu, mmu, instruction_data, false);
    }},
    { 0xDD, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xED, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid
    { 0xFD, "", 0, nullptr, [](gbcemu::Opcode *, gbcemu::CPU *, gbcemu::MMU *, uint8_t*) {} }, // invalid

    { 0xCE, "ADC A, d8", 2, new uint8_t[] { 0x06}, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t* instruction_data) {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x05, 0x0B, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0A, 0x10, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0xFA, 0x00, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, false);

        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x05, 0x0C, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0A, 0x11, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0xF9, 0x00, { FlagAction::Set, FlagAction::NotSet, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xDE, "SBC A, d8", 2, new uint8_t[] { 0x0F }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data)  {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0F, 0x00, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x3E, 0x2F, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, false);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0A, 0xFB, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, false);
        
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x10, 0x00, { FlagAction::Set, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x3E, 0x2E, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet }, true, true);
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0A, 0xFA, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::Set }, true, true);
    }},
    { 0xEE, "XOR d8", 2, new uint8_t[] { 0xFF }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0xFF, 0x00, { FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x0F, 0xF0, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x8A, 0x75, { FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet, FlagAction::NotSet });
    }},
    { 0xFE, "CP d8", 2, new uint8_t[] { 0x3C }, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t *instruction_data) {
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x2F, 0x2F, { FlagAction::NotSet, FlagAction::Set, FlagAction::NotSet, FlagAction::Set });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x3C, 0x3C, { FlagAction::Set, FlagAction::Set, FlagAction::NotSet, FlagAction::NotSet });
        test_add_8bit_arithmetic_from_immediate(opcode, cpu, mmu, instruction_data, 0x40, 0x40, { FlagAction::NotSet, FlagAction::Set, FlagAction::Set, FlagAction::NotSet });
    }},

    { 0xCF, "RST 08H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0008);
    }},
    { 0xDF, "RST 18H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0018);
    }},
    { 0xEF, "RST 28H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0028);
    }},
    { 0xFF, "RST 38H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  {
        test_reset_instructions(opcode, cpu, mmu, 0x0038);
    }},
};
// clang-format on

static ExpectedOpcode extended_opcodes[] = {
    // clang-format off
    // 0x00 - 0x3F
    { 0x00, "RLC B", 1, nullptr, nullptr },
    { 0x01, "RLC C", 1, nullptr, nullptr },
    { 0x02, "RLC D", 1, nullptr, nullptr },
    { 0x03, "RLC E", 1, nullptr, nullptr },
    { 0x04, "RLC H", 1, nullptr, nullptr },
    { 0x05, "RLC L", 1, nullptr, nullptr },
    { 0x06, "RLC (HL)", 1, nullptr, nullptr },
    { 0x07, "RLC A", 1, nullptr, nullptr },
    { 0x08, "RRC B", 1, nullptr, nullptr },
    { 0x09, "RRC C", 1, nullptr, nullptr },
    { 0x0A, "RRC D", 1, nullptr, nullptr },
    { 0x0B, "RRC E", 1, nullptr, nullptr },
    { 0x0C, "RRC H", 1, nullptr, nullptr },
    { 0x0D, "RRC L", 1, nullptr, nullptr },
    { 0x0E, "RRC (HL)", 1, nullptr, nullptr },
    { 0x0F, "RRC A", 1, nullptr, nullptr },

    { 0x10, "RL B", 1, nullptr, nullptr },
    { 0x11, "RL C", 1, nullptr, nullptr },
    { 0x12, "RL D", 1, nullptr, nullptr },
    { 0x13, "RL E", 1, nullptr, nullptr },
    { 0x14, "RL H", 1, nullptr, nullptr },
    { 0x15, "RL L", 1, nullptr, nullptr },
    { 0x16, "RL (HL)", 1, nullptr, nullptr },
    { 0x17, "RL A", 1, nullptr, nullptr },
    { 0x18, "RR B", 1, nullptr, nullptr },
    { 0x19, "RR C", 1, nullptr, nullptr },
    { 0x1A, "RR D", 1, nullptr, nullptr },
    { 0x1B, "RR E", 1, nullptr, nullptr },
    { 0x1C, "RR H", 1, nullptr, nullptr },
    { 0x1D, "RR L", 1, nullptr, nullptr },
    { 0x1E, "RR (HL)", 1, nullptr, nullptr },
    { 0x1F, "RR A", 1, nullptr, nullptr },

    { 0x20, "SLA B", 1, nullptr, nullptr },
    { 0x21, "SLA C", 1, nullptr, nullptr },
    { 0x22, "SLA D", 1, nullptr, nullptr },
    { 0x23, "SLA E", 1, nullptr, nullptr },
    { 0x24, "SLA H", 1, nullptr, nullptr },
    { 0x25, "SLA L", 1, nullptr, nullptr },
    { 0x26, "SLA (HL)", 1, nullptr, nullptr },
    { 0x27, "SLA A", 1, nullptr, nullptr },
    { 0x28, "SRA B", 1, nullptr, nullptr },
    { 0x29, "SRA C", 1, nullptr, nullptr },
    { 0x2A, "SRA D", 1, nullptr, nullptr },
    { 0x2B, "SRA E", 1, nullptr, nullptr },
    { 0x2C, "SRA H", 1, nullptr, nullptr },
    { 0x2D, "SRA L", 1, nullptr, nullptr },
    { 0x2E, "SRA (HL)", 1, nullptr, nullptr },
    { 0x2F, "SRA A", 1, nullptr, nullptr },

    { 0x30, "SWAP B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::B);
    }},
    { 0x31, "SWAP C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::C);
    }},
    { 0x32, "SWAP D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::D);
    }},
    { 0x33, "SWAP E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::E);
    }},
    { 0x34, "SWAP H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::H);
    }},
    { 0x35, "SWAP L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::L);
    }},
    { 0x36, "SWAP (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::HL);
    }},
    { 0x37, "SWAP A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) {
        test_swap_instruction(opcode, cpu, mmu, gbcemu::CPU::Register::A);
    }},
    { 0x38, "SRL B", 1, nullptr, nullptr },
    { 0x39, "SRL C", 1, nullptr, nullptr },
    { 0x3A, "SRL D", 1, nullptr, nullptr },
    { 0x3B, "SRL E", 1, nullptr, nullptr },
    { 0x3C, "SRL H", 1, nullptr, nullptr },
    { 0x3D, "SRL L", 1, nullptr, nullptr },
    { 0x3E, "SRL (HL)", 1, nullptr, nullptr },
    { 0x3F, "SRL A", 1, nullptr, nullptr },

    // 0x40 - 0x7F 
    { 0x40, "BIT 0, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 0);
    }},
    { 0x41, "BIT 0, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 0);
    }},
    { 0x42, "BIT 0, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 0);
    }},
    { 0x43, "BIT 0, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 0);
    }},
    { 0x44, "BIT 0, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 0);
    }},
    { 0x45, "BIT 0, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 0);
    }},
    { 0x46, "BIT 0, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 0);
    }},
    { 0x47, "BIT 0, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 0);
    }},
    { 0x48, "BIT 1, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 1);
    }},
    { 0x49, "BIT 1, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 1);
    }},
    { 0x4A, "BIT 1, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 1);
    }},
    { 0x4B, "BIT 1, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 1);
    }},
    { 0x4C, "BIT 1, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 1);
    }},
    { 0x4D, "BIT 1, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 1);
    }},
    { 0x4E, "BIT 1, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 1);
    }},
    { 0x4F, "BIT 1, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 1);
    }},

    { 0x50, "BIT 2, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 2);
    }},
    { 0x51, "BIT 2, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 2);
    }},
    { 0x52, "BIT 2, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 2);
    }},
    { 0x53, "BIT 2, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 2);
    }},
    { 0x54, "BIT 2, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 2);
    }},
    { 0x55, "BIT 2, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 2);
    }},
    { 0x56, "BIT 2, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 2);
    }},
    { 0x57, "BIT 2, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 2);
    }},
    { 0x58, "BIT 3, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 3);
    }},
    { 0x59, "BIT 3, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 3);
    }},
    { 0x5A, "BIT 3, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 3);
    }},
    { 0x5B, "BIT 3, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 3);
    }},
    { 0x5C, "BIT 3, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 3);
    }},
    { 0x5D, "BIT 3, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 3);
    }},
    { 0x5E, "BIT 3, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 3);
    }},
    { 0x5F, "BIT 3, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 3);
    }},

    { 0x60, "BIT 4, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 4);
    }},
    { 0x61, "BIT 4, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 4);
    }},
    { 0x62, "BIT 4, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 4);
    }},
    { 0x63, "BIT 4, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 4);
    }},
    { 0x64, "BIT 4, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 4);
    }},
    { 0x65, "BIT 4, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 4);
    }},
    { 0x66, "BIT 4, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 4);
    }},
    { 0x67, "BIT 4, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 4);
    }},
    { 0x68, "BIT 5, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 5);
    }},
    { 0x69, "BIT 5, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 5);
    }},
    { 0x6A, "BIT 5, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 5);
    }},
    { 0x6B, "BIT 5, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 5);
    }},
    { 0x6C, "BIT 5, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 5);
    }},
    { 0x6D, "BIT 5, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 5);
    }},
    { 0x6E, "BIT 5, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 5);
    }},
    { 0x6F, "BIT 5, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 5);
    }},

    { 0x70, "BIT 6, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 6);
    }},
    { 0x71, "BIT 6, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 6);
    }},
    { 0x72, "BIT 6, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 6);
    }},
    { 0x73, "BIT 6, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 6);
    }},
    { 0x74, "BIT 6, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 6);
    }},
    { 0x75, "BIT 6, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 6);
    }},
    { 0x76, "BIT 6, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 6);
    }},
    { 0x77, "BIT 6, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 6);
    }},
    { 0x78, "BIT 7, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::B, 7);
    }},
    { 0x79, "BIT 7, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::C, 7);
    }},
    { 0x7A, "BIT 7, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::D, 7);
    }},
    { 0x7B, "BIT 7, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::E, 7);
    }},
    { 0x7C, "BIT 7, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::H, 7);
    }},
    { 0x7D, "BIT 7, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::L, 7);
    }},
    { 0x7E, "BIT 7, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::HL, 7);
    }},
    { 0x7F, "BIT 7, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Test, gbcemu::CPU::Register::A, 7);
    }},

    // 0x80 - 0xBF 
    { 0x80, "RES 0, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 0);
    }},
    { 0x81, "RES 0, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 0);
    }},
    { 0x82, "RES 0, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 0);
    }},
    { 0x83, "RES 0, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 0);
    }},
    { 0x84, "RES 0, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 0);
    }},
    { 0x85, "RES 0, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 0);
    }},
    { 0x86, "RES 0, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 0);
    }},
    { 0x87, "RES 0, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 0);
    }},
    { 0x88, "RES 1, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 1);
    }},
    { 0x89, "RES 1, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 1);
    }},
    { 0x8A, "RES 1, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 1);
    }},
    { 0x8B, "RES 1, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 1);
    }},
    { 0x8C, "RES 1, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 1);
    }},
    { 0x8D, "RES 1, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 1);
    }},
    { 0x8E, "RES 1, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 1);
    }},
    { 0x8F, "RES 1, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 1);
    }},

    { 0x90, "RES 2, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 2);
    }},
    { 0x91, "RES 2, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 2);
    }},
    { 0x92, "RES 2, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 2);
    }},
    { 0x93, "RES 2, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 2);
    }},
    { 0x94, "RES 2, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 2);
    }},
    { 0x95, "RES 2, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 2);
    }},
    { 0x96, "RES 2, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 2);
    }},
    { 0x97, "RES 2, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 2);
    }},
    { 0x98, "RES 3, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 3);
    }},
    { 0x99, "RES 3, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 3);
    }},
    { 0x9A, "RES 3, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 3);
    }},
    { 0x9B, "RES 3, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 3);
    }},
    { 0x9C, "RES 3, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 3);
    }},
    { 0x9D, "RES 3, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 3);
    }},
    { 0x9E, "RES 3, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 3);
    }},
    { 0x9F, "RES 3, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 3);
    }},

    { 0xA0, "RES 4, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 4);
    }},
    { 0xA1, "RES 4, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 4);
    }},
    { 0xA2, "RES 4, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 4);
    }},
    { 0xA3, "RES 4, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 4);
    }},
    { 0xA4, "RES 4, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 4);
    }},
    { 0xA5, "RES 4, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 4);
    }},
    { 0xA6, "RES 4, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 4);
    }},
    { 0xA7, "RES 4, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 4);
    }},
    { 0xA8, "RES 5, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 5);
    }},
    { 0xA9, "RES 5, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 5);
    }},
    { 0xAA, "RES 5, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 5);
    }},
    { 0xAB, "RES 5, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 5);
    }},
    { 0xAC, "RES 5, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 5);
    }},
    { 0xAD, "RES 5, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 5);
    }},
    { 0xAE, "RES 5, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 5);
    }},
    { 0xAF, "RES 5, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 5);
    }},

    { 0xB0, "RES 6, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 6);
    }},
    { 0xB1, "RES 6, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 6);
    }},
    { 0xB2, "RES 6, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 6);
    }},
    { 0xB3, "RES 6, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 6);
    }},
    { 0xB4, "RES 6, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 6);
    }},
    { 0xB5, "RES 6, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 6);
    }},
    { 0xB6, "RES 6, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 6);
    }},
    { 0xB7, "RES 6, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 6);
    }},
    { 0xB8, "RES 7, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::B, 7);
    }},
    { 0xB9, "RES 7, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::C, 7);
    }},
    { 0xBA, "RES 7, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::D, 7);
    }},
    { 0xBB, "RES 7, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::E, 7);
    }},
    { 0xBC, "RES 7, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::H, 7);
    }},
    { 0xBD, "RES 7, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::L, 7);
    }},
    { 0xBE, "RES 7, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::HL, 7);
    }},
    { 0xBF, "RES 7, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Reset, gbcemu::CPU::Register::A, 7);
    }},

    // 0xC0 - 0xFF 
    { 0xC0, "SET 0, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 0);
    }},
    { 0xC1, "SET 0, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 0);
    }},
    { 0xC2, "SET 0, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 0);
    }},
    { 0xC3, "SET 0, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 0);
    }},
    { 0xC4, "SET 0, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 0);
    }},
    { 0xC5, "SET 0, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 0);
    }},
    { 0xC6, "SET 0, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 0);
    }},
    { 0xC7, "SET 0, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 0);
    }},
    { 0xC8, "SET 1, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 1);
    }},
    { 0xC9, "SET 1, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 1);
    }},
    { 0xCA, "SET 1, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 1);
    }},
    { 0xCB, "SET 1, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 1);
    }},
    { 0xCC, "SET 1, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 1);
    }},
    { 0xCD, "SET 1, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 1);
    }},
    { 0xCE, "SET 1, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 1);
    }},
    { 0xCF, "SET 1, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 1);
    }},

    { 0xD0, "SET 2, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 2);
    }},
    { 0xD1, "SET 2, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 2);
    }},
    { 0xD2, "SET 2, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 2);
    }},
    { 0xD3, "SET 2, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 2);
    }},
    { 0xD4, "SET 2, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 2);
    }},
    { 0xD5, "SET 2, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 2);
    }},
    { 0xD6, "SET 2, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 2);
    }},
    { 0xD7, "SET 2, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 2);
    }},
    { 0xD8, "SET 3, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 3);
    }},
    { 0xD9, "SET 3, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 3);
    }},
    { 0xDA, "SET 3, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 3);
    }},
    { 0xDB, "SET 3, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 3);
    }},
    { 0xDC, "SET 3, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 3);
    }},
    { 0xDD, "SET 3, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 3);
    }},
    { 0xDE, "SET 3, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 3);
    }},
    { 0xDF, "SET 3, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 3);
    }},

    { 0xE0, "SET 4, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 4);
    }},
    { 0xE1, "SET 4, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 4);
    }},
    { 0xE2, "SET 4, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 4);
    }},
    { 0xE3, "SET 4, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 4);
    }},
    { 0xE4, "SET 4, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 4);
    }},
    { 0xE5, "SET 4, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 4);
    }},
    { 0xE6, "SET 4, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 4);
    }},
    { 0xE7, "SET 4, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 4);
    }},
    { 0xE8, "SET 5, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 5);
    }},
    { 0xE9, "SET 5, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 5);
    }},
    { 0xEA, "SET 5, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 5);
    }},
    { 0xEB, "SET 5, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 5);
    }},
    { 0xEC, "SET 5, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 5);
    }},
    { 0xED, "SET 5, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 5);
    }},
    { 0xEE, "SET 5, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 5);
    }},
    { 0xEF, "SET 5, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 5);
    }},

    { 0xF0, "SET 6, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 6);
    }},
    { 0xF1, "SET 6, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 6);
    }},
    { 0xF2, "SET 6, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 6);
    }},
    { 0xF3, "SET 6, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 6);
    }},
    { 0xF4, "SET 6, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 6);
    }},
    { 0xF5, "SET 6, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 6);
    }},
    { 0xF6, "SET 6, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 6);
    }},
    { 0xF7, "SET 6, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 6);
    }},
    { 0xF8, "SET 7, B", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::B, 7);
    }},
    { 0xF9, "SET 7, C", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::C, 7);
    }},
    { 0xFA, "SET 7, D", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::D, 7);
    }},
    { 0xFB, "SET 7, E", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::E, 7);
    }},
    { 0xFC, "SET 7, H", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::H, 7);
    }},
    { 0xFD, "SET 7, L", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*) { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::L, 7);
    }},
    { 0xFE, "SET 7, (HL)", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::HL, 7);
    }},
    { 0xFF, "SET 7, A", 1, nullptr, [](gbcemu::Opcode *opcode, gbcemu::CPU *cpu, gbcemu::MMU *mmu, uint8_t*)  { 
        test_set_reset_test_instructions(opcode, cpu, mmu, ExtendedOperationSubType::Set, gbcemu::CPU::Register::A, 7);
    }},
};
// clang-format on

std::vector<std::string> data_placeholders = { "d8", "d16", "a16", "\\$FF00 + a8" };
std::string get_final_opcode_name(const ExpectedOpcode &current_opcode, int location_of_token, uint16_t data) {
    return gbcemu::GeneralUtilities::formatted_string("%s0x%04X%s", current_opcode.name.substr(0, location_of_token), data,
                                                      current_opcode.name.substr(location_of_token + 3, std::string::npos));
}

std::string get_final_opcode_name(const ExpectedOpcode &current_opcode, int location_of_token, uint8_t data) {
    return gbcemu::GeneralUtilities::formatted_string("%s0x%02X%s", current_opcode.name.substr(0, location_of_token), data,
                                                      current_opcode.name.substr(location_of_token + 2, std::string::npos));
}

void test_opcodes(ExpectedOpcode *expected_opcodes, int size, bool is_extended) {
    std::shared_ptr<gbcemu::Opcode> decoded_opcode;

    auto mmu = std::make_shared<gbcemu::MMU>(0xFFFF);
    auto ppu = std::make_shared<gbcemu::PPU>(mmu, 160, 144, 4);
    auto cpu = new gbcemu::CPU(mmu, ppu);

    for (int i = 0; i < size; i++) {
        auto current_opcode = expected_opcodes[i];

        try {
            decoded_opcode = gbcemu::decode_opcode(current_opcode.identifier, is_extended);
        } catch (std::runtime_error e) {
            if (!current_opcode.name.empty()) {
                print_error();
                std::cout << "Opcode '0x" << std::hex << std::uppercase << unsigned(current_opcode.identifier) << "' marked invalid, should be '"
                          << current_opcode.name << "'" << std::endl;
            } else {
                std::stringstream ss;
                ss << "0x" << std::hex << std::uppercase << unsigned(current_opcode.identifier) << " (invalid)";
                print_opcode_passed(ss.str());
            }
            continue;
        }

        assert_equal(current_opcode.size, decoded_opcode->size, "size");
        if (current_opcode.instruction_data == nullptr) {
            assert_equal(current_opcode.name, decoded_opcode->get_disassembled_instruction(nullptr), "name");
        } else {
            for (auto const &placeholder : data_placeholders) {
                std::regex r(placeholder);
                if (std::regex_search(current_opcode.name, r)) {
                    auto location_of_token = current_opcode.name.find(placeholder);
                    if (placeholder.compare("d16") == 0 || placeholder.compare("a16") == 0) {
                        uint16_t data = current_opcode.instruction_data[1] << 8 | current_opcode.instruction_data[0];
                        assert_equal(get_final_opcode_name(current_opcode, location_of_token, data),
                                     decoded_opcode->get_disassembled_instruction(current_opcode.instruction_data), "disassembled name");
                    } else if (placeholder.compare("d8") == 0) {
                        assert_equal(get_final_opcode_name(current_opcode, location_of_token, current_opcode.instruction_data[0]),
                                     decoded_opcode->get_disassembled_instruction(current_opcode.instruction_data), "disassembled name");
                    } else {
                        assert_equal(
                            get_final_opcode_name(current_opcode, location_of_token, static_cast<uint16_t>(0xFF00 + current_opcode.instruction_data[0])),
                            decoded_opcode->get_disassembled_instruction(current_opcode.instruction_data), "disassembled name");
                    }
                }
            }
        }

        if (current_opcode.execution_test != nullptr) {
            current_opcode.execution_test(decoded_opcode.get(), cpu, mmu.get(), current_opcode.instruction_data);
            print_opcode_passed(current_opcode.name);
        } else {
            print_warning();
            std::cout << "'" << current_opcode.name << "' passes, but does not have an execution test" << std::endl;
        }
    }
}

int main(int argc, char **argv) {

    test_opcodes(non_extended_opcodes, 256, false);
    test_opcodes(extended_opcodes, 256, true);

    return 0;
}