#include "Opcodes.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <stdint.h>
#include <unordered_map>

namespace gbcemu {

template <typename T> std::shared_ptr<Opcode> construct() { return std::make_shared<T>(); }
template <typename T> std::shared_ptr<Opcode> construct(const uint8_t data) { return std::make_shared<T>(data); }

using opcode_builder = std::function<std::shared_ptr<Opcode>(uint8_t)>;
static const std::unordered_map<uint8_t, opcode_builder> _00_opcodes = {
    { 0,
      [](uint8_t identifier) {
          uint8_t y = (identifier >> 3) & 0x07;
          switch (y) {

          case 0:
              return construct<NoOperation>();
          case 1:
              return construct<StoreStackpointer>();
          case 2:
              return construct<Stop>();
          default:
              return construct<RelativeJump>(identifier);
          }
      } },
    { 1,
      [](uint8_t identifier) {
          auto q = (identifier >> 3) & 0x01;
          if (q == 0)
              return construct<Load16bitImmediate>(identifier);
          else
              NOT_IMPLEMENTED("16-bit add");
      } },
    { 2, [](uint8_t identifier) { return construct<Load16bitIndirect>(identifier); } },
    { 4, [](uint8_t identifier) { return construct<IncrementDecrement8Bit>(identifier); } },
    { 6, [](uint8_t identifier) { return construct<Load8bitImmediate>(identifier); } },
};

static const std::unordered_map<uint8_t, opcode_builder> _11_opcodes = {
    { 0,
      [](uint8_t identifier) {
          uint8_t y = (identifier >> 3) & 0x07;
          switch (y) {
          case 0:
          case 1:
          case 2:
          case 3:
              NOT_IMPLEMENTED("Conditional returns");
          case 4:
          case 6:
              return construct<ReadWriteIOPortNWithA>(identifier);
          case 5:
              NOT_IMPLEMENTED("Add/subtract from SP");
          case 7:
              NOT_IMPLEMENTED("Set HL to SP + offset");
          default:
              exit(1);
          }
      } },
    { 2,
      [](uint8_t identifier) {
          uint8_t y = (identifier >> 3) & 0x07;
          switch (y) {

          case 0:
          case 1:
          case 2:
          case 3:
              NOT_IMPLEMENTED("Conditional jumps");
          case 4:
          case 6:
              return construct<ReadWriteIOPortCWithA>(identifier);
          case 5:
          case 7:
              NOT_IMPLEMENTED("Load/Set A indirect");
          default:
              exit(1);
          }
      } },
};

static std::unordered_map<uint8_t, std::shared_ptr<Opcode>> _opcode_cache;
static std::unordered_map<uint8_t, std::shared_ptr<Opcode>> _extended_opcode_cache;

static std::shared_ptr<Opcode> _check_opcode_cache(const uint8_t identifier, const std::unordered_map<uint8_t, std::shared_ptr<Opcode>> &cache) {
    auto result = cache.find(identifier);
    return result != cache.end() ? result->second : nullptr;
}

static bool is_extended_opcode(const uint8_t identifier) { return identifier == 0xCB; }

static std::shared_ptr<Opcode> decode_opcode(const uint8_t identifier, bool is_extended) {

    auto cached_result = is_extended ? _check_opcode_cache(identifier, _extended_opcode_cache) : _check_opcode_cache(identifier, _opcode_cache);
    if (cached_result != nullptr)
        return cached_result;

    // See http://www.z80.info/decoding.htm and https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
    uint8_t x = (identifier >> 6) & 0x03;

    if (is_extended) {
        auto opcode = construct<ExtendedOpcode>(identifier);
        if (opcode != nullptr)
            _extended_opcode_cache.insert({ identifier, opcode });
        return opcode;
    }

    uint8_t z = identifier & 0x07;
    std::shared_ptr<Opcode> result;

    switch (x) {
    case 0: {
        auto builder = _00_opcodes.find(z);
        if (builder == _00_opcodes.end())
            result = nullptr;
        else
            result = builder->second(identifier);

    } break;

    case 1:
        if (Load8bitRegister::is_halt_instruction(identifier))
            result = construct<Halt>();
        else
            result = construct<Load8bitRegister>(identifier);
        break;

    case 2:
        result = construct<RegisterOperation>(identifier);
        break;

    case 3: {
        auto builder = _11_opcodes.find(z);
        if (builder == _11_opcodes.end())
            result = nullptr;
        else
            result = builder->second(identifier);

    } break;
    default:
        result = nullptr;
        break;
    }

    if (result != nullptr)
        _opcode_cache.insert({ identifier, result });

    return result;
}

}