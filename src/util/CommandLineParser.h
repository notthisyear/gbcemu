#pragma once

#include "GeneralUtilities.h"
#include <iomanip>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>

namespace gbcemu {

struct CommandLineArgument;

class CommandLineParser {

  public:
    enum class ArgumentType {
        Missing = 0,
        Help = 1,
        AttachDebugger = 2,
        BootRomPath = 3,
        CartridgePath = 4,
        OutputTrace = 5,
    };

    CommandLineParser();

    void parse(int, char **) const;

    bool has_argument(ArgumentType) const;

    std::shared_ptr<CommandLineArgument> get_argument(ArgumentType) const;

    void print_usage_string(std::ostream &, const std::string &) const;

    void print_options(std::ostream &) const;

  private:
    std::unordered_map<CommandLineParser::ArgumentType, std::shared_ptr<CommandLineArgument>> m_argument_options;

    // Command regexp, is switch, argument validation regexp, help text
    static inline const std::unordered_map<CommandLineParser::ArgumentType, std::tuple<std::string, bool, std::string, std::string>> s_arguments = {
        { CommandLineParser::ArgumentType::Help,
          std::tuple<std::string, bool, std::string, std::string>("(-h)|(--help)", true, "", "show this help message and exit") },
        { CommandLineParser::ArgumentType::AttachDebugger,
          std::tuple<std::string, bool, std::string, std::string>("(-d)|(--dgb)", true, "", "attach the debugger at startup") },
        { CommandLineParser::ArgumentType::BootRomPath,
          std::tuple<std::string, bool, std::string, std::string>("--boot-rom", false, R"( *[\w\\:\.\-/\\(\\)\[\] \, ]+\.\w+)", "path to boot rom") },
        { CommandLineParser::ArgumentType::CartridgePath,
          std::tuple<std::string, bool, std::string, std::string>("(-c)|(--cartridge)", false, R"( *[\w\\:\.\-/\\(\\)\[\] \, ]+\.\w+)", "path to cartridge") },
        { CommandLineParser::ArgumentType::OutputTrace,
          std::tuple<std::string, bool, std::string, std::string>("(-t)|(--trace)", true, "", "output cpu trace to file for each cycle") },
    };
};
}