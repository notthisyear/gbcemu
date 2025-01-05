#pragma once

#include "CommandData.h"
#include "CommandLineParser.h"
#include <optional>
#include <regex>

namespace gbcemu {

struct CommandLineArgument {
  public:
    bool is_switch() const;

    bool is_command(std::string const &) const;

    CommandData::ArgumentType argument_type() const;

    bool is_found() const;

    std::string value() const;

    bool parameter_is_valid(std::string const &) const;

    void set_value(const std::string);

    void set_found();

    std::string get_help_text() const;

    std::string get_command_in_help() const;

    std::string get_command_in_usage() const;

    CommandLineArgument(CommandData const &);

  private:
    CommandData m_command_data;
    CommandData::ArgumentType m_argument_type;
    bool m_is_found;
    std::string m_value{ "" };
};
}