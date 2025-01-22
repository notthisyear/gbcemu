#include "Opcodes.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace gbcemu {

class OpcodeBuilder final {

  private:
    template <typename T> static std::shared_ptr<Opcode> construct() { return std::make_shared<T>(); }
    template <typename T> static std::shared_ptr<Opcode> construct(uint8_t const data) { return std::make_shared<T>(data); }

    using opcode_builder = std::function<std::shared_ptr<Opcode>(uint8_t)>;
    static inline std::unordered_map<uint8_t, opcode_builder> const k00Opcodes = {
        { 0,
          [](uint8_t identifier) {
              uint8_t const y{ static_cast<uint8_t>((identifier >> 3) & 0x07) };
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
              uint8_t const q{ static_cast<uint8_t>((identifier >> 3) & 0x01) };
              if (q == 0) {
                  return construct<Load16bitImmediate>(identifier);
              } else {
                  return construct<Add16bitRegister>(identifier);
              }
          } },
        { 2, [](uint8_t identifier) { return construct<Load16bitIndirect>(identifier); } },
        { 3, [](uint8_t identifier) { return construct<IncrementOrDecrement8Or16bit>(identifier); } },
        { 4, [](uint8_t identifier) { return construct<IncrementOrDecrement8Or16bit>(identifier); } },
        { 5, [](uint8_t identifier) { return construct<IncrementOrDecrement8Or16bit>(identifier); } },
        { 6, [](uint8_t identifier) { return construct<Load8bitImmediate>(identifier); } },
        { 7,
          [](uint8_t identifier) {
              uint8_t const y{ static_cast<uint8_t>((identifier >> 3) & 0x07) };
              switch (y) {

              case 0:
              case 1:
              case 2:
              case 3:
                  return construct<RotateAccumulator>(identifier);
              case 4:
                  return construct<DecimalAdjustAccumulator>();
              case 5:
                  return construct<InvertAccumulator>();
              case 6:
                  return construct<SetCarryFlag>();
              case 7:
                  return construct<ComplementCarryFlag>();
              default:
                  __builtin_unreachable();
              }
          } },
    };

    static inline std::unordered_map<uint8_t, opcode_builder> const k11Opcodes = {
        { 0,
          [](uint8_t identifier) {
              uint8_t const y{ static_cast<uint8_t>((identifier >> 3) & 0x07) };
              switch (y) {
              case 0:
              case 1:
              case 2:
              case 3:
                  return construct<ReturnFromCall>(identifier);
              case 4:
              case 6:
                  return construct<ReadWriteIOPortNWithA>(identifier);
              case 5:
              case 7:
                  return construct<SetSPOrHLToSPAndOffset>(identifier);
              default:
                  __builtin_unreachable();
              }
          } },
        { 1,
          [](uint8_t identifier) {
              uint8_t const y{ static_cast<uint8_t>((identifier >> 0x03) & 0x07) };
              switch (y) {
              case 0:
              case 2:
              case 4:
              case 6:
                  return construct<Pop16bitRegister>(identifier);
              case 1:
              case 3:
                  return construct<ReturnFromCall>(identifier);
              case 5:
                  return construct<JumpToAddressInHL>();
              case 7:
                  return construct<LoadSPWithHL>();
              default:
                  __builtin_unreachable();
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
                  return construct<JumpToImmediate>(identifier);
              case 4:
              case 6:
                  return construct<ReadWriteIOPortCWithA>(identifier);
              case 5:
              case 7:
                  return construct<LoadFromOrSetAIndirect>(identifier);
              default:
                  __builtin_unreachable();
              }
          } },
        { 3,
          [](uint8_t identifier) {
              uint8_t y = (identifier >> 3) & 0x07;
              switch (y) {
              case 0:
                  return construct<JumpToImmediate>(identifier);
              case 6:
                  return construct<DisableInterrupt>();
              case 7:
                  return construct<EnableInterrupt>();
              default:
                  INVALID_OPCODE(identifier);
              }
          } },
        { 4,
          [](uint8_t identifier) {
              uint8_t y = (identifier >> 3) & 0x07;
              if (y < 4)
                  return construct<Call>(identifier);

              INVALID_OPCODE(identifier)
          } },
        { 5,
          [](uint8_t identifier) {
              uint8_t y = (identifier >> 3) & 0x07;
              if (y == 1)
                  return construct<Call>(identifier);
              else if ((y & 0x01) == 0)
                  return construct<Push16bitRegister>(identifier);

              INVALID_OPCODE(identifier)
          } },
        { 6, [](uint8_t identifier) { return construct<AccumulatorOperation>(identifier); } },
        { 7, [](uint8_t identifier) { return construct<Reset>(identifier); } },

    };

    static inline std::unordered_map<uint8_t, std::shared_ptr<Opcode>> s_opcode_cache{};
    static inline std::unordered_map<uint8_t, std::shared_ptr<Opcode>> s_extended_opcode_cache{};

    static std::shared_ptr<Opcode> check_opcode_cache(uint8_t const identifier, std::unordered_map<uint8_t, std::shared_ptr<Opcode>> const &cache) {
        auto const result = cache.find(identifier);
        return result != cache.end() ? result->second : nullptr;
    }

  public:
    static bool is_extended_opcode(uint8_t const identifier) { return identifier == 0xCB; }

    static std::shared_ptr<Opcode> decode_opcode(uint8_t const identifier, bool const is_extended) {

        std::shared_ptr<Opcode> const cached_result{ is_extended ? check_opcode_cache(identifier, s_extended_opcode_cache)
                                                                 : check_opcode_cache(identifier, s_opcode_cache) };
        if (cached_result != nullptr) {
            cached_result->reset_state();
            return cached_result;
        }

        // See http://www.z80.info/decoding.htm and https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
        uint8_t const x{ static_cast<uint8_t>((identifier >> 6) & 0x03) };

        if (is_extended) {
            std::shared_ptr<Opcode> const opcode{ construct<ExtendedOpcode>(identifier) };
            s_extended_opcode_cache.insert({ identifier, opcode });
            return opcode;
        }

        uint8_t const z{ static_cast<uint8_t>(identifier & 0x07) };
        std::shared_ptr<Opcode> result{};

        switch (x) {
        case 0:
            result = k00Opcodes.find(z)->second(identifier);
            break;

        case 1:
            result = (identifier == Halt::opcode) ? construct<Halt>() : construct<Load8bitRegister>(identifier);
            break;

        case 2:
            result = construct<RegisterOperation>(identifier);
            break;

        case 3:
            result = k11Opcodes.find(z)->second(identifier);
            break;

        default:
            __builtin_unreachable();
        }

        s_opcode_cache.insert({ identifier, result });
        return result;
    }
};
}