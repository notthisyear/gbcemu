#include "CommandLineArgument.h"
#include "GeneralUtilities.h"

namespace gbcemu {

bool CommandLineArgument::is_switch() const { return m_command_data.is_switch; }

bool CommandLineArgument::is_command(std::string const &s) const {
    std::smatch sm{};
    std::regex command_regex{};
    if (m_command_data.short_name.has_value()) {
        command_regex = std::regex(GeneralUtilities::formatted_string("(-%c)|(--%s)", m_command_data.short_name.value(), m_command_data.long_name),
                                   std::regex_constants::icase);
    } else {
        command_regex = std::regex(GeneralUtilities::formatted_string("--%s", m_command_data.long_name), std::regex_constants::icase);
    }
    return std::regex_search(s, sm, command_regex);
}

CommandData::ArgumentType CommandLineArgument::argument_type() const { return m_argument_type; }

bool CommandLineArgument::is_found() const { return m_is_found; }

std::string CommandLineArgument::value() const { return m_value; }

bool CommandLineArgument::parameter_is_valid(std::string const &s) const {
    std::smatch sm;
    if (m_command_data.validation_regex.has_value()) {
        return std::regex_search(s, sm, std::regex(m_command_data.validation_regex.value(), std::regex_constants::icase));
    }
    return true;
}

void CommandLineArgument::set_value(std::string const s) { m_value = s; }

void CommandLineArgument::set_found() { m_is_found = true; }

std::string CommandLineArgument::get_help_text() const { return m_command_data.help_text; }

std::string CommandLineArgument::get_command_in_help() const {
    std::string const argument_value_name{ m_command_data.argument_value_name.has_value()
                                               ? GeneralUtilities::formatted_string(" <%s>", m_command_data.argument_value_name.value())
                                               : "" };
    if (m_command_data.short_name.has_value()) {
        return GeneralUtilities::formatted_string("-%c, --%s%s", m_command_data.short_name.value(), m_command_data.long_name, argument_value_name);
    } else {
        return GeneralUtilities::formatted_string("--%s%s", m_command_data.long_name, argument_value_name);
    }
}

std::string CommandLineArgument::get_command_in_usage() const {
    std::string const argument_name{ m_command_data.short_name.has_value() ? GeneralUtilities::formatted_string("-%c", m_command_data.short_name)
                                                                           : GeneralUtilities::formatted_string("--%s", m_command_data.long_name) };
    std::string const argument_value_name{ m_command_data.argument_value_name.has_value()
                                               ? GeneralUtilities::formatted_string(" <%s>", m_command_data.argument_value_name.value())
                                               : "" };
    if (m_command_data.is_required) {
        return GeneralUtilities::formatted_string("%s%s", argument_name, argument_value_name);
    } else {
        return GeneralUtilities::formatted_string("[%s%s]", argument_name, argument_value_name);
    }
}

CommandLineArgument::CommandLineArgument(CommandData const &command_data)
    : m_argument_type{ command_data.type }, m_is_found{ false }, m_command_data{ command_data } {}
}