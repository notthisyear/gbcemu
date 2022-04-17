#include "components/OpcodeBuilder.h"
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string>

typedef struct {
    uint8_t identifier;
    std::string name;
    uint8_t size;
    uint8_t cycles;
} ExpectedOpcode;

void print_error() {
    std::cout << "\033[1;31m[error]"
              << "\033[0m";
}

void print_warning() {
    std::cout << "\033[1;33m[warning]"
              << "\033[0m";
}

void print_opcode_passed(const std::string &opcode_name) {
    std::cout << "\033[1;32m[pass]"
              << "\033[0m '" << opcode_name << "' matches" << std::endl;
}

void assert_equal(uint8_t a, uint8_t b, const std::string &prop) {
    if (a != b) {
        print_error();
        std::cout << " '" << unsigned(a) << "' is not equal to '" << unsigned(b) << "' (" << prop << ")" << std::endl;
        exit(1);
    }
}

void assert_equal(const std::string &a, const std::string &b, const std::string &prop) {
    if (a.compare(b) != 0) {
        print_error();
        std::cout << " '" << a << "' is not equal to '" << b << "' (" << prop << ")" << std::endl;
        exit(1);
    }
}

static ExpectedOpcode non_extended_opcodes[] = {
    // clang-format off
    // 0x00 - 0x3F
    { 0x00, "NOP", 1, 4 },
    { 0x10, "STOP 0", 2, 4 },
    { 0x20, "JR NZ, d8", 2, 0 },
    { 0x30, "JR NC, d8", 2, 0 },

    { 0x01, "LD BC, d16", 3, 12 },
    { 0x11, "LD DE, d16", 3, 12 },
    { 0x21, "LD HL, d16", 3, 12 },
    { 0x31, "LD SP, d16", 3, 12 },

    { 0x02, "LD (BC), A", 1, 8 },
    { 0x12, "LD (DE), A", 1, 8 },
    { 0x22, "LD (HL+), A", 1, 8 },
    { 0x32, "LD (HL-), A", 1, 8 },

    { 0x03, "INC BC", 1, 8 },
    { 0x13, "INC DE", 1, 8 },
    { 0x23, "INC HL", 1, 8 },
    { 0x33, "INC SP", 1, 8 },

    { 0x04, "INC B", 1, 4 },
    { 0x14, "INC D", 1, 4 },
    { 0x24, "INC H", 1, 4 },
    { 0x34, "INC (HL)", 1, 12 },

    { 0x05, "DEC B", 1, 4 },
    { 0x15, "DEC D", 1, 4 },
    { 0x25, "DEC H", 1, 4 },
    { 0x35, "DEC (HL)", 1, 12 },

    { 0x06, "LD B, d8", 2, 8 },
    { 0x16, "LD D, d8", 2, 8 },
    { 0x26, "LD H, d8", 2, 8 },
    { 0x36, "LD (HL), d8", 2, 12 },

    { 0x07, "RLCA", 1, 4 },
    { 0x17, "RLA", 1, 4 },
    { 0x27, "DAA", 1, 4 },
    { 0x37, "SCF", 1, 4 },

    { 0x08, "LD (a16), SP", 3, 20 },
    { 0x18, "JR d8", 2, 0 },
    { 0x28, "JR Z, d8", 2, 0 },
    { 0x38, "JR C, d8", 2, 0 },

    { 0x09, "ADD HL, BC", 1, 8 },
    { 0x19, "ADD HL, DE", 1, 8 },
    { 0x29, "ADD HL, HL", 1, 8 },
    { 0x39, "ADD HL, SP", 1, 8 },

    { 0x0A, "LD A, (BC)", 1, 8 },
    { 0x1A, "LD A, (DE)", 1, 8 },
    { 0x2A, "LD A, (HL+)", 1, 8 },
    { 0x3A, "LD A, (HL-)", 1, 8 },

    { 0x0B, "DEC BC", 1, 8 },
    { 0x1B, "DEC DE", 1, 8 },
    { 0x2B, "DEC HL", 1, 8 },
    { 0x3B, "DEC SP", 1, 8 },

    { 0x0C, "INC C", 1, 4 },
    { 0x1C, "INC E", 1, 4 },
    { 0x2C, "INC L", 1, 4 },
    { 0x3C, "INC A", 1, 4 },

    { 0x0D, "DEC C", 1, 4 },
    { 0x1D, "DEC E", 1, 4 },
    { 0x2D, "DEC L", 1, 4 },
    { 0x3D, "DEC A", 1, 4 },

    { 0x0E, "LD C, d8", 2, 8 },
    { 0x1E, "LD E, d8", 2, 8 },
    { 0x2E, "LD L, d8", 2, 8 },
    { 0x3E, "LD A, d8", 2, 8 },

    { 0x0F, "RRCA", 1, 4 },
    { 0x1F, "RRA", 1, 4 },
    { 0x2F, "CPL", 1, 4 },
    { 0x3F, "CCF", 1, 4 },

    // 0x40 - 0x7F
    { 0x40, "LD B, B", 1, 4 },
    { 0x50, "LD D, B", 1, 4 },
    { 0x60, "LD H, B", 1, 4 },
    { 0x70, "LD (HL), B", 1, 8 },

    { 0x41, "LD B, C", 1, 4 },
    { 0x51, "LD D, C", 1, 4 },
    { 0x61, "LD H, C", 1, 4 },
    { 0x71, "LD (HL), C", 1, 8 },

    { 0x42, "LD B, D", 1, 4 },
    { 0x52, "LD D, D", 1, 4 },
    { 0x62, "LD H, D", 1, 4 },
    { 0x72, "LD (HL), D", 1, 8 },

    { 0x43, "LD B, E", 1, 4 },
    { 0x53, "LD D, E", 1, 4 },
    { 0x63, "LD H, E", 1, 4 },
    { 0x73, "LD (HL), E", 1, 8 },

    { 0x44, "LD B, H", 1, 4 },
    { 0x54, "LD D, H", 1, 4 },
    { 0x64, "LD H, H", 1, 4 },
    { 0x74, "LD (HL), H", 1, 8 },

    { 0x45, "LD B, L", 1, 4 },
    { 0x55, "LD D, L", 1, 4 },
    { 0x65, "LD H, L", 1, 4 },
    { 0x75, "LD (HL), L", 1, 8 },

    { 0x46, "LD B, (HL)", 1, 8 },
    { 0x56, "LD D, (HL)", 1, 8 },
    { 0x66, "LD H, (HL)", 1, 8 },
    { 0x76, "HALT", 1, 4 },

    { 0x47, "LD B, A", 1, 4 },
    { 0x57, "LD D, A", 1, 4 },
    { 0x67, "LD H, A", 1, 4 },
    { 0x77, "LD (HL), A", 1, 8 },

    { 0x48, "LD C, B", 1, 4 },
    { 0x58, "LD E, B", 1, 4 },
    { 0x68, "LD L, B", 1, 4 },
    { 0x78, "LD A, B", 1, 4 },

    { 0x49, "LD C, C", 1, 4 },
    { 0x59, "LD E, C", 1, 4 },
    { 0x69, "LD L, C", 1, 4 },
    { 0x79, "LD A, C", 1, 4 },

    { 0x4A, "LD C, D", 1, 4 },
    { 0x5A, "LD E, D", 1, 4 },
    { 0x6A, "LD L, D", 1, 4 },
    { 0x7A, "LD A, D", 1, 4 },

    { 0x4B, "LD C, E", 1, 4 },
    { 0x5B, "LD E, E", 1, 4 },
    { 0x6B, "LD L, E", 1, 4 },
    { 0x7B, "LD A, E", 1, 4 },

    { 0x4C, "LD C, H", 1, 4 },
    { 0x5C, "LD E, H", 1, 4 },
    { 0x6C, "LD L, H", 1, 4 },
    { 0x7C, "LD A, H", 1, 4 },

    { 0x4D, "LD C, L", 1, 4 },
    { 0x5D, "LD E, L", 1, 4 },
    { 0x6D, "LD L, L", 1, 4 },
    { 0x7D, "LD A, L", 1, 4 },

    { 0x4E, "LD C, (HL)", 1, 8 },
    { 0x5E, "LD E, (HL)", 1, 8 },
    { 0x6E, "LD L, (HL)", 1, 8 },
    { 0x7E, "LD A, (HL)", 1, 8 },

    { 0x4F, "LD C, A", 1, 4 },
    { 0x5F, "LD E, A", 1, 4 },
    { 0x6F, "LD L, A", 1, 4 },
    { 0x7F, "LD A, A", 1, 4 },

    // 0x80 - 0xBF
    { 0x80, "ADD A, B", 1, 4 },
    { 0x90, "SUB B", 1, 4 },
    { 0xA0, "AND B", 1, 4 },
    { 0xB0, "OR B", 1, 4 },

    { 0x81, "ADD A, C", 1, 4 },
    { 0x91, "SUB C", 1, 4 },
    { 0xA1, "AND C", 1, 4 },
    { 0xB1, "OR C", 1, 4 },

    { 0x82, "ADD A, D", 1, 4 },
    { 0x92, "SUB D", 1, 4 },
    { 0xA2, "AND D", 1, 4 },
    { 0xB2, "OR D", 1, 4 },

    { 0x83, "ADD A, E", 1, 4 },
    { 0x93, "SUB E", 1, 4 },
    { 0xA3, "AND E", 1, 4 },
    { 0xB3, "OR E", 1, 4 },

    { 0x84, "ADD A, H", 1, 4 },
    { 0x94, "SUB H", 1, 4 },
    { 0xA4, "AND H", 1, 4 },
    { 0xB4, "OR H", 1, 4 },

    { 0x85, "ADD A, L", 1, 4 },
    { 0x95, "SUB L", 1, 4 },
    { 0xA5, "AND L", 1, 4 },
    { 0xB5, "OR L", 1, 4 },

    { 0x86, "ADD A, (HL)", 1, 8 },
    { 0x96, "SUB (HL)", 1, 8 },
    { 0xA6, "AND (HL)", 1, 8 },
    { 0xB6, "OR (HL)", 1, 8 },

    { 0x87, "ADD A, A", 1, 4 },
    { 0x97, "SUB A", 1, 4 },
    { 0xA7, "AND A", 1, 4 },
    { 0xB7, "OR A", 1, 4 },

    { 0x88, "ADC A, B", 1, 4 },
    { 0x98, "SBC A, B", 1, 4 },
    { 0xA8, "XOR B", 1, 4 },
    { 0xB8, "CP B", 1, 4 },

    { 0x89, "ADC A, C", 1, 4 },
    { 0x99, "SBC A, C", 1, 4 },
    { 0xA9, "XOR C", 1, 4 },
    { 0xB9, "CP C", 1, 4 },

    { 0x8A, "ADC A, D", 1, 4 },
    { 0x9A, "SBC A, D", 1, 4 },
    { 0xAA, "XOR D", 1, 4 },
    { 0xBA, "CP D", 1, 4 },

    { 0x8B, "ADC A, E", 1, 4 },
    { 0x9B, "SBC A, E", 1, 4 },
    { 0xAB, "XOR E", 1, 4 },
    { 0xBB, "CP E", 1, 4 },

    { 0x8C, "ADC A, H", 1, 4 },
    { 0x9C, "SBC A, H", 1, 4 },
    { 0xAC, "XOR H", 1, 4 },
    { 0xBC, "CP H", 1, 4 },

    { 0x8D, "ADC A, L", 1, 4 },
    { 0x9D, "SBC A, L", 1, 4 },
    { 0xAD, "XOR L", 1, 4 },
    { 0xBD, "CP L", 1, 4 },

    { 0x8E, "ADC A, (HL)", 1, 8 },
    { 0x9E, "SBC A, (HL)", 1, 8 },
    { 0xAE, "XOR (HL)", 1, 8 },
    { 0xBE, "CP (HL)", 1, 8 },

    { 0x8F, "ADC A, A", 1, 4 },
    { 0x9F, "SBC A, A", 1, 4 },
    { 0xAF, "XOR A", 1, 4 },
    { 0xBF, "CP A", 1, 4 },

    // 0xC0 - 0xFF
    { 0xC0, "RET NZ", 1, 0 },
    { 0xD0, "RET NC", 1, 0 },
    { 0xE0, "LD ($FF00 + a8), A", 2, 12 },
    { 0xF0, "LD A, ($FF00 + a8)", 2, 12 },

    { 0xC1, "POP BC", 1, 12 },
    { 0xD1, "POP DE", 1, 12 },
    { 0xE1, "POP HL", 1, 12 },
    { 0xF1, "POP AF", 1, 12 },

    // { 0xC2, "JP NZ, a16", 3, 0 },
    // { 0xD2, "JP NC, a16", 3, 0 },
    { 0xE2, "LD ($FF00 + C), A", 1, 8 },
    { 0xF2, "LD A, ($FF00 + C)", 1, 8 },

    // { 0xC3, "JP a16", 3, 0 },
    { 0xD3, "", 0, 0 },     // invalid
    { 0xE3, "", 0, 0 },     // invalid
    { 0xF3, "DI", 1, 4 },
};

// clang-format on
int main(int argc, char **argv) {

    // Non-extended opcodes
    for (const auto &it : non_extended_opcodes) {
        auto decoded_opcode = gbcemu::decode_opcode(it.identifier, false);

        if (it.name.empty()) {
            if (decoded_opcode != nullptr) {
                print_error();
                std::cout << "Opcode '0x" << std::hex << unsigned(it.identifier) << "' decoded as '" << decoded_opcode->name << "', should be nullptr"
                          << std::endl;
            } else {
                std::stringstream ss;
                ss << "0x" << std::hex << unsigned(it.identifier) << " (invalid)";
                print_opcode_passed(ss.str());
            }
            continue;
        }

        assert_equal(decoded_opcode->identifier, it.identifier, "opcode");
        assert_equal(decoded_opcode->name, it.name, "name");
        assert_equal(decoded_opcode->size, it.size, "size");

        if (it.cycles > 0) {
            assert_equal(decoded_opcode->cycles, it.cycles, "cycles");
            print_opcode_passed(it.name);
        } else {
            print_warning();
            std::cout << "'" << it.name << "' matches, but could not check cycle count" << std::endl;
        }
    }

    return 0;
}