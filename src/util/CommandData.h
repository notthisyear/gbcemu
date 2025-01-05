#pragma once

#include <optional>
#include <string>

namespace gbcemu {

struct CommandData {
    enum class ArgumentType {
        kHelp,
        kAttachDebugger,
        kBootRomPath,
        kCartridgePath,
        kOutputTrace,
    };

    ArgumentType type;
    std::string long_name;
    std::string help_text;
    bool is_switch{ false };
    bool is_required{ false };
    std::optional<std::string> argument_value_name{ std::nullopt };
    std::optional<char> short_name{ std::nullopt };
    std::optional<std::string> validation_regex{ std::nullopt };
};
}