#pragma once

#include "CommandData.h"

#include <array>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>

namespace gbcemu {

struct CommandLineArgument;

class CommandLineParser final {

  public:
    CommandLineParser();

    bool try_parse(int const, char *const *const);

    std::size_t parsing_error_argument_index() const;

    bool has_argument(CommandData::ArgumentType const) const;

    std::string get_argument_value(CommandData::ArgumentType const) const;

    void print_usage_string(std::ostream &, std::string const &) const;

    void print_options(std::ostream &) const;

  private:
    std::size_t m_parsing_error_argument_index{ 0U };
    std::unordered_map<CommandData::ArgumentType, std::shared_ptr<CommandLineArgument>> m_argument_options;

    static std::size_t constexpr kNumberOfArguments{ 5 };
    static inline std::array<CommandData, kNumberOfArguments> const kArguments = {
        CommandData{
            .type = CommandData::ArgumentType::kHelp,
            .long_name = "help",
            .help_text = "show this help message and exit",
            .is_switch = true,
            .short_name = 'h',
        },
        CommandData{
            .type = CommandData::ArgumentType::kAttachDebugger,
            .long_name = "debugger",
            .help_text = "attach the debugger at startup",
            .is_switch = true,
            .short_name = 'd',
        },
        CommandData{
            .type = CommandData::ArgumentType::kBootRomPath,
            .long_name = "boot_rom",
            .help_text = "path to boot rom",
            .argument_value_name = "path",
            .validation_regex = R"( *[\w\\:\.\-/\\(\\)\[\] \, ]+\.\w+)",
        },
        CommandData{
            .type = CommandData::ArgumentType::kCartridgePath,
            .long_name = "cartridge",
            .help_text = "path to boot cartridge",
            .is_required = true,
            .argument_value_name = "path",
            .short_name = 'c',
            .validation_regex = R"( *[\w\\:\.\-/\\(\\)\[\] \, ]+\.\w+)",
        },
        CommandData{
            .type = CommandData::ArgumentType::kOutputTrace,
            .long_name = "trace",
            .help_text = "output cpu trace to file for each cycle",
            .is_switch = true,
            .short_name = 't',
        },
    };
};
}