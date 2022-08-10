#pragma once

#include "CommandLineParser.h"
#include <regex>

namespace gbcemu {

struct CommandLineArgument {
  public:
    CommandLineParser::ArgumentType argument_type;
    bool is_switch;
    std::string value;

    bool is_command(std::string) const;

    bool parameter_is_valid(std::string) const;

    void set_value(std::string);

    void set_as_missing();

    std::string get_help_text() const;

    std::string get_regexp_printable() const;

    std::string get_regexp_in_usage() const;

    CommandLineArgument(CommandLineParser::ArgumentType, std::string, bool, std::string, std::string);

    CommandLineArgument(CommandLineParser::ArgumentType);

  private:
    std::string m_argument_regex_string;
    std::regex m_argument_regex;
    std::regex m_parameter_validation_regex;
    std::string m_help_text;
};
}