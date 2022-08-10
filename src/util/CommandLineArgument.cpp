#include "CommandLineArgument.h"

namespace gbcemu {

bool CommandLineArgument::is_command(std::string s) const {
    std::smatch sm;
    return std::regex_search(s, sm, m_argument_regex);
}

bool CommandLineArgument::parameter_is_valid(std::string s) const {
    std::smatch sm;
    return std::regex_search(s, sm, m_parameter_validation_regex);
}

void CommandLineArgument::set_value(std::string s) { value = s; }

void CommandLineArgument::set_as_missing() { argument_type = CommandLineParser::ArgumentType::Missing; }

std::string CommandLineArgument::get_help_text() const { return m_help_text; }

std::string CommandLineArgument::get_regexp_printable() const {
    auto location_of_whitespace = m_argument_regex_string.find(' ');
    auto last_idx = location_of_whitespace != std::string::npos ? location_of_whitespace : m_argument_regex_string.length();

    auto number_of_parentheses = 2 * std::count(m_argument_regex_string.begin(), m_argument_regex_string.begin() + last_idx, '(');
    auto number_of_variants = number_of_parentheses / 2;

    char *buffer = new char[(number_of_variants == 0) ? (last_idx + 1) : (last_idx - number_of_parentheses + number_of_variants)];
    auto result_idx = 0;
    for (int i = 0; i < last_idx; i++) {
        if (m_argument_regex_string[i] == '(' || m_argument_regex_string[i] == ')') {
            continue;
        } else if (m_argument_regex_string[i] == '|') {
            buffer[result_idx++] = ',';
            buffer[result_idx++] = ' ';
        } else {
            buffer[result_idx++] = m_argument_regex_string[i];
        }
    }

    buffer[result_idx] = '\0';
    std::string result(buffer);
    delete[] buffer;
    return result;
}

std::string CommandLineArgument::get_regexp_in_usage() const {
    auto location_of_split = m_argument_regex_string.find('|');
    auto location_of_whitespace = m_argument_regex_string.find(' ');

    auto last_idx = (location_of_split != std::string::npos)
                        ? location_of_split
                        : (location_of_whitespace != std::string::npos ? location_of_whitespace : m_argument_regex_string.length());

    char *buffer = new char[last_idx + 1];
    char prev_chr;
    int buffer_idx = 0;
    for (int i = 0; i < last_idx; i++) {
        if (m_argument_regex_string[i] == '(') {
            if (i > 0 && prev_chr == '(')
                continue;
            buffer[buffer_idx++] = '[';

        } else if (m_argument_regex_string[i] == ')') {
            if (i > 0 && prev_chr == '(')
                continue;
            buffer[buffer_idx++] = ']';
        } else {
            buffer[buffer_idx++] = m_argument_regex_string[i];
        }
        prev_chr = m_argument_regex_string[i];
    }

    buffer[buffer_idx] = '\0';
    std::string result(buffer);
    delete[] buffer;
    return result;
}

CommandLineArgument::CommandLineArgument(CommandLineParser::ArgumentType type, std::string argument_regex, bool is_switch_arg,
                                         std::string parameter_validation_regex_string, std::string help_text)
    : argument_type(type), m_argument_regex_string(argument_regex), is_switch(is_switch_arg), m_help_text(help_text) {

    m_argument_regex = std::regex(m_argument_regex_string, std::regex_constants::icase);
    if (!is_switch)
        m_parameter_validation_regex = std::regex(parameter_validation_regex_string, std::regex_constants::icase);
}

CommandLineArgument::CommandLineArgument(CommandLineParser::ArgumentType type) : argument_type(type) {}
}