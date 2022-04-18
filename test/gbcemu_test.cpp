#include "components/OpcodeBuilder.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

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

    { 0xC2, "JP NZ, a16", 3, 0 },
    { 0xD2, "JP NC, a16", 3, 0 },
    { 0xE2, "LD ($FF00 + C), A", 1, 8 },
    { 0xF2, "LD A, ($FF00 + C)", 1, 8 },

    { 0xC3, "JP a16", 3, 0 },
    { 0xD3, "", 0, 0 }, // invalid
    { 0xE3, "", 0, 0 }, // invalid
    { 0xF3, "DI", 1, 4 },

    { 0xC4, "CALL NZ, a16", 3, 0 },
    { 0xD4, "CALL NC, a16", 3, 0 },
    { 0xE4, "", 0, 0 }, // invalid
    { 0xF4, "", 0, 0 }, // invalid

    { 0xC5, "PUSH BC", 1, 16 },
    { 0xD5, "PUSH DE", 1, 16 },
    { 0xE5, "PUSH HL", 1, 16 },
    { 0xF5, "PUSH AF", 1, 16 },

    { 0xC6, "ADD A, d8", 2, 8 },
    { 0xD6, "SUB d8", 2, 8 },
    { 0xE6, "AND d8", 2, 8 },
    { 0xF6, "OR d8", 2, 8 },

    { 0xC7, "RST 00H", 1, 16 },
    { 0xD7, "RST 10H", 1, 16 },
    { 0xE7, "RST 20H", 1, 16 },
    { 0xF7, "RST 30H", 1, 16 },

    { 0xC8, "RET Z", 1, 0 },
    { 0xD8, "RET C", 1, 0 },
    { 0xE8, "ADD SP, d8", 2, 16 },
    { 0xF8, "LD HL, SP + d8", 2, 12 },

    { 0xC9, "RET", 1, 0 },
    { 0xD9, "RETI", 1, 0 },
    { 0xE9, "JP (HL)", 1, 4 },
    { 0xF9, "LD SP, HL", 1, 8 },

    { 0xCA, "JP Z, a16", 3, 0 },
    { 0xDA, "JP C, a16", 3, 0 },
    { 0xEA, "LD (a16), A", 3, 16 },
    { 0xFA, "LD A, (a16)", 3, 16 },

    { 0xCB, "", 0, 0 }, // extended prefix, invalid as opcode
    { 0xDB, "", 0, 0 }, // invalid
    { 0xEB, "", 0, 0 }, // invalid
    { 0xFB, "EI", 1, 4 },

    { 0xCC, "CALL Z, a16", 3, 0 },
    { 0xDC, "CALL C, a16", 3, 0 },
    { 0xEC, "", 0, 0 }, // invalid
    { 0xFC, "", 0, 0 }, // invalid

    { 0xCD, "CALL a16", 3, 0 },
    { 0xDD, "", 0, 0 }, // invalid
    { 0xED, "", 0, 0 }, // invalid
    { 0xFD, "", 0, 0 }, // invalid

    { 0xCE, "ADC A, d8", 2, 8 },
    { 0xDE, "SBC A, d8", 2, 8 },
    { 0xEE, "XOR d8", 2, 8 },
    { 0xFE, "CP d8", 2, 8 },

    { 0xCF, "RST 08H", 1, 16 },
    { 0xDF, "RST 18H", 1, 16 },
    { 0xEF, "RST 28H", 1, 16 },
    { 0xFF, "RST 38H", 1, 16 },
};
// clang-format on

static ExpectedOpcode extended_opcodes[] = {
    // clang-format off
    // 0x00 - 0x3F
    { 0x00, "RLC B", 1, 8 },
    { 0x01, "RLC C", 1, 8 },
    { 0x02, "RLC D", 1, 8 },
    { 0x03, "RLC E", 1, 8 },
    { 0x04, "RLC H", 1, 8 },
    { 0x05, "RLC L", 1, 8 },
    { 0x06, "RLC (HL)", 1, 16 },
    { 0x07, "RLC A", 1, 8 },
    { 0x08, "RRC B", 1, 8 },
    { 0x09, "RRC C", 1, 8 },
    { 0x0A, "RRC D", 1, 8 },
    { 0x0B, "RRC E", 1, 8 },
    { 0x0C, "RRC H", 1, 8 },
    { 0x0D, "RRC L", 1, 8 },
    { 0x0E, "RRC (HL)", 1, 16 },
    { 0x0F, "RRC A", 1, 8 },

    { 0x10, "RL B", 1, 8 },
    { 0x11, "RL C", 1, 8 },
    { 0x12, "RL D", 1, 8 },
    { 0x13, "RL E", 1, 8 },
    { 0x14, "RL H", 1, 8 },
    { 0x15, "RL L", 1, 8 },
    { 0x16, "RL (HL)", 1, 16 },
    { 0x17, "RL A", 1, 8 },
    { 0x18, "RR B", 1, 8 },
    { 0x19, "RR C", 1, 8 },
    { 0x1A, "RR D", 1, 8 },
    { 0x1B, "RR E", 1, 8 },
    { 0x1C, "RR H", 1, 8 },
    { 0x1D, "RR L", 1, 8 },
    { 0x1E, "RR (HL)", 1, 16 },
    { 0x1F, "RR A", 1, 8 },

    { 0x20, "SLA B", 1, 8 },
    { 0x21, "SLA C", 1, 8 },
    { 0x22, "SLA D", 1, 8 },
    { 0x23, "SLA E", 1, 8 },
    { 0x24, "SLA H", 1, 8 },
    { 0x25, "SLA L", 1, 8 },
    { 0x26, "SLA (HL)", 1, 16 },
    { 0x27, "SLA A", 1, 8 },
    { 0x28, "SRA B", 1, 8 },
    { 0x29, "SRA C", 1, 8 },
    { 0x2A, "SRA D", 1, 8 },
    { 0x2B, "SRA E", 1, 8 },
    { 0x2C, "SRA H", 1, 8 },
    { 0x2D, "SRA L", 1, 8 },
    { 0x2E, "SRA (HL)", 1, 16 },
    { 0x2F, "SRA A", 1, 8 },

    { 0x30, "SWAP B", 1, 8 },
    { 0x31, "SWAP C", 1, 8 },
    { 0x32, "SWAP D", 1, 8 },
    { 0x33, "SWAP E", 1, 8 },
    { 0x34, "SWAP H", 1, 8 },
    { 0x35, "SWAP L", 1, 8 },
    { 0x36, "SWAP (HL)", 1, 16 },
    { 0x37, "SWAP A", 1, 8 },
    { 0x38, "SRL B", 1, 8 },
    { 0x39, "SRL C", 1, 8 },
    { 0x3A, "SRL D", 1, 8 },
    { 0x3B, "SRL E", 1, 8 },
    { 0x3C, "SRL H", 1, 8 },
    { 0x3D, "SRL L", 1, 8 },
    { 0x3E, "SRL (HL)", 1, 16 },
    { 0x3F, "SRL A", 1, 8 },

    // 0x40 - 0x7F 
    { 0x40, "BIT 0, B", 1, 8 },
    { 0x41, "BIT 0, C", 1, 8 },
    { 0x42, "BIT 0, D", 1, 8 },
    { 0x43, "BIT 0, E", 1, 8 },
    { 0x44, "BIT 0, H", 1, 8 },
    { 0x45, "BIT 0, L", 1, 8 },
    { 0x46, "BIT 0, (HL)", 1, 16 },
    { 0x47, "BIT 0, A", 1, 8 },
    { 0x48, "BIT 1, B", 1, 8 },
    { 0x49, "BIT 1, C", 1, 8 },
    { 0x4A, "BIT 1, D", 1, 8 },
    { 0x4B, "BIT 1, E", 1, 8 },
    { 0x4C, "BIT 1, H", 1, 8 },
    { 0x4D, "BIT 1, L", 1, 8 },
    { 0x4E, "BIT 1, (HL)", 1, 16 },
    { 0x4F, "BIT 1, A", 1, 8 },

    { 0x50, "BIT 2, B", 1, 8 },
    { 0x51, "BIT 2, C", 1, 8 },
    { 0x52, "BIT 2, D", 1, 8 },
    { 0x53, "BIT 2, E", 1, 8 },
    { 0x54, "BIT 2, H", 1, 8 },
    { 0x55, "BIT 2, L", 1, 8 },
    { 0x56, "BIT 2, (HL)", 1, 16 },
    { 0x57, "BIT 2, A", 1, 8 },
    { 0x58, "BIT 3, B", 1, 8 },
    { 0x59, "BIT 3, C", 1, 8 },
    { 0x5A, "BIT 3, D", 1, 8 },
    { 0x5B, "BIT 3, E", 1, 8 },
    { 0x5C, "BIT 3, H", 1, 8 },
    { 0x5D, "BIT 3, L", 1, 8 },
    { 0x5E, "BIT 3, (HL)", 1, 16 },
    { 0x5F, "BIT 3, A", 1, 8 },

    { 0x60, "BIT 4, B", 1, 8 },
    { 0x61, "BIT 4, C", 1, 8 },
    { 0x62, "BIT 4, D", 1, 8 },
    { 0x63, "BIT 4, E", 1, 8 },
    { 0x64, "BIT 4, H", 1, 8 },
    { 0x65, "BIT 4, L", 1, 8 },
    { 0x66, "BIT 4, (HL)", 1, 16 },
    { 0x67, "BIT 4, A", 1, 8 },
    { 0x68, "BIT 5, B", 1, 8 },
    { 0x69, "BIT 5, C", 1, 8 },
    { 0x6A, "BIT 5, D", 1, 8 },
    { 0x6B, "BIT 5, E", 1, 8 },
    { 0x6C, "BIT 5, H", 1, 8 },
    { 0x6D, "BIT 5, L", 1, 8 },
    { 0x6E, "BIT 5, (HL)", 1, 16 },
    { 0x6F, "BIT 5, A", 1, 8 },

    { 0x70, "BIT 6, B", 1, 8 },
    { 0x71, "BIT 6, C", 1, 8 },
    { 0x72, "BIT 6, D", 1, 8 },
    { 0x73, "BIT 6, E", 1, 8 },
    { 0x74, "BIT 6, H", 1, 8 },
    { 0x75, "BIT 6, L", 1, 8 },
    { 0x76, "BIT 6, (HL)", 1, 16 },
    { 0x77, "BIT 6, A", 1, 8 },
    { 0x78, "BIT 7, B", 1, 8 },
    { 0x79, "BIT 7, C", 1, 8 },
    { 0x7A, "BIT 7, D", 1, 8 },
    { 0x7B, "BIT 7, E", 1, 8 },
    { 0x7C, "BIT 7, H", 1, 8 },
    { 0x7D, "BIT 7, L", 1, 8 },
    { 0x7E, "BIT 7, (HL)", 1, 16 },
    { 0x7F, "BIT 7, A", 1, 8 },

    // 0x80 - 0xBF 
    { 0x80, "RES 0, B", 1, 8 },
    { 0x81, "RES 0, C", 1, 8 },
    { 0x82, "RES 0, D", 1, 8 },
    { 0x83, "RES 0, E", 1, 8 },
    { 0x84, "RES 0, H", 1, 8 },
    { 0x85, "RES 0, L", 1, 8 },
    { 0x86, "RES 0, (HL)", 1, 16 },
    { 0x87, "RES 0, A", 1, 8 },
    { 0x88, "RES 1, B", 1, 8 },
    { 0x89, "RES 1, C", 1, 8 },
    { 0x8A, "RES 1, D", 1, 8 },
    { 0x8B, "RES 1, E", 1, 8 },
    { 0x8C, "RES 1, H", 1, 8 },
    { 0x8D, "RES 1, L", 1, 8 },
    { 0x8E, "RES 1, (HL)", 1, 16 },
    { 0x8F, "RES 1, A", 1, 8 },

    { 0x90, "RES 2, B", 1, 8 },
    { 0x91, "RES 2, C", 1, 8 },
    { 0x92, "RES 2, D", 1, 8 },
    { 0x93, "RES 2, E", 1, 8 },
    { 0x94, "RES 2, H", 1, 8 },
    { 0x95, "RES 2, L", 1, 8 },
    { 0x96, "RES 2, (HL)", 1, 16 },
    { 0x97, "RES 2, A", 1, 8 },
    { 0x98, "RES 3, B", 1, 8 },
    { 0x99, "RES 3, C", 1, 8 },
    { 0x9A, "RES 3, D", 1, 8 },
    { 0x9B, "RES 3, E", 1, 8 },
    { 0x9C, "RES 3, H", 1, 8 },
    { 0x9D, "RES 3, L", 1, 8 },
    { 0x9E, "RES 3, (HL)", 1, 16 },
    { 0x9F, "RES 3, A", 1, 8 },

    { 0xA0, "RES 4, B", 1, 8 },
    { 0xA1, "RES 4, C", 1, 8 },
    { 0xA2, "RES 4, D", 1, 8 },
    { 0xA3, "RES 4, E", 1, 8 },
    { 0xA4, "RES 4, H", 1, 8 },
    { 0xA5, "RES 4, L", 1, 8 },
    { 0xA6, "RES 4, (HL)", 1, 16 },
    { 0xA7, "RES 4, A", 1, 8 },
    { 0xA8, "RES 5, B", 1, 8 },
    { 0xA9, "RES 5, C", 1, 8 },
    { 0xAA, "RES 5, D", 1, 8 },
    { 0xAB, "RES 5, E", 1, 8 },
    { 0xAC, "RES 5, H", 1, 8 },
    { 0xAD, "RES 5, L", 1, 8 },
    { 0xAE, "RES 5, (HL)", 1, 16 },
    { 0xAF, "RES 5, A", 1, 8 },

    { 0xB0, "RES 6, B", 1, 8 },
    { 0xB1, "RES 6, C", 1, 8 },
    { 0xB2, "RES 6, D", 1, 8 },
    { 0xB3, "RES 6, E", 1, 8 },
    { 0xB4, "RES 6, H", 1, 8 },
    { 0xB5, "RES 6, L", 1, 8 },
    { 0xB6, "RES 6, (HL)", 1, 16 },
    { 0xB7, "RES 6, A", 1, 8 },
    { 0xB8, "RES 7, B", 1, 8 },
    { 0xB9, "RES 7, C", 1, 8 },
    { 0xBA, "RES 7, D", 1, 8 },
    { 0xBB, "RES 7, E", 1, 8 },
    { 0xBC, "RES 7, H", 1, 8 },
    { 0xBD, "RES 7, L", 1, 8 },
    { 0xBE, "RES 7, (HL)", 1, 16 },
    { 0xBF, "RES 7, A", 1, 8 },

    // 0xC0 - 0xFF 
    { 0xC0, "SET 0, B", 1, 8 },
    { 0xC1, "SET 0, C", 1, 8 },
    { 0xC2, "SET 0, D", 1, 8 },
    { 0xC3, "SET 0, E", 1, 8 },
    { 0xC4, "SET 0, H", 1, 8 },
    { 0xC5, "SET 0, L", 1, 8 },
    { 0xC6, "SET 0, (HL)", 1, 16 },
    { 0xC7, "SET 0, A", 1, 8 },
    { 0xC8, "SET 1, B", 1, 8 },
    { 0xC9, "SET 1, C", 1, 8 },
    { 0xCA, "SET 1, D", 1, 8 },
    { 0xCB, "SET 1, E", 1, 8 },
    { 0xCC, "SET 1, H", 1, 8 },
    { 0xCD, "SET 1, L", 1, 8 },
    { 0xCE, "SET 1, (HL)", 1, 16 },
    { 0xCF, "SET 1, A", 1, 8 },

    { 0xD0, "SET 2, B", 1, 8 },
    { 0xD1, "SET 2, C", 1, 8 },
    { 0xD2, "SET 2, D", 1, 8 },
    { 0xD3, "SET 2, E", 1, 8 },
    { 0xD4, "SET 2, H", 1, 8 },
    { 0xD5, "SET 2, L", 1, 8 },
    { 0xD6, "SET 2, (HL)", 1, 16 },
    { 0xD7, "SET 2, A", 1, 8 },
    { 0xD8, "SET 3, B", 1, 8 },
    { 0xD9, "SET 3, C", 1, 8 },
    { 0xDA, "SET 3, D", 1, 8 },
    { 0xDB, "SET 3, E", 1, 8 },
    { 0xDC, "SET 3, H", 1, 8 },
    { 0xDD, "SET 3, L", 1, 8 },
    { 0xDE, "SET 3, (HL)", 1, 16 },
    { 0xDF, "SET 3, A", 1, 8 },

    { 0xE0, "SET 4, B", 1, 8 },
    { 0xE1, "SET 4, C", 1, 8 },
    { 0xE2, "SET 4, D", 1, 8 },
    { 0xE3, "SET 4, E", 1, 8 },
    { 0xE4, "SET 4, H", 1, 8 },
    { 0xE5, "SET 4, L", 1, 8 },
    { 0xE6, "SET 4, (HL)", 1, 16 },
    { 0xE7, "SET 4, A", 1, 8 },
    { 0xE8, "SET 5, B", 1, 8 },
    { 0xE9, "SET 5, C", 1, 8 },
    { 0xEA, "SET 5, D", 1, 8 },
    { 0xEB, "SET 5, E", 1, 8 },
    { 0xEC, "SET 5, H", 1, 8 },
    { 0xED, "SET 5, L", 1, 8 },
    { 0xEE, "SET 5, (HL)", 1, 16 },
    { 0xEF, "SET 5, A", 1, 8 },

    { 0xF0, "SET 6, B", 1, 8 },
    { 0xF1, "SET 6, C", 1, 8 },
    { 0xF2, "SET 6, D", 1, 8 },
    { 0xF3, "SET 6, E", 1, 8 },
    { 0xF4, "SET 6, H", 1, 8 },
    { 0xF5, "SET 6, L", 1, 8 },
    { 0xF6, "SET 6, (HL)", 1, 16 },
    { 0xF7, "SET 6, A", 1, 8 },
    { 0xF8, "SET 7, B", 1, 8 },
    { 0xF9, "SET 7, C", 1, 8 },
    { 0xFA, "SET 7, D", 1, 8 },
    { 0xFB, "SET 7, E", 1, 8 },
    { 0xFC, "SET 7, H", 1, 8 },
    { 0xFD, "SET 7, L", 1, 8 },
    { 0xFE, "SET 7, (HL)", 1, 16 },
    { 0xFF, "SET 7, A", 1, 8 },
};
// clang-format on

void test_opcodes(ExpectedOpcode *expected_opcodes, int size, bool is_extended) {
    std::shared_ptr<gbcemu::Opcode> decoded_opcode;

    for (int i = 0; i < size; i++) {
        auto current_opcode = expected_opcodes[i];

        try {
            decoded_opcode = gbcemu::decode_opcode(current_opcode.identifier, is_extended);
        } catch (std::runtime_error e) {
            if (!current_opcode.name.empty()) {
                print_error();
                std::cout << "Opcode '0x" << std::hex << unsigned(current_opcode.identifier) << "' marked invalid, should be '" << current_opcode.name << "'"
                          << std::endl;
            } else {
                std::stringstream ss;
                ss << "0x" << std::hex << unsigned(current_opcode.identifier) << " (invalid)";
                print_opcode_passed(ss.str());
            }
            continue;
        }

        assert_equal(decoded_opcode->identifier, current_opcode.identifier, "opcode");
        assert_equal(decoded_opcode->name, current_opcode.name, "name");
        assert_equal(decoded_opcode->size, current_opcode.size, "size");

        if (current_opcode.cycles > 0) {
            assert_equal(decoded_opcode->cycles, current_opcode.cycles, "cycles");
            print_opcode_passed(current_opcode.name);
        } else {
            print_warning();
            std::cout << "'" << current_opcode.name << "' matches, but could not check cycle count" << std::endl;
        }
    }
}

int main(int argc, char **argv) {

    test_opcodes(non_extended_opcodes, 256, false);
    test_opcodes(extended_opcodes, 256, true);

    return 0;
}