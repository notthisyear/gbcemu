#include "DebuggerCommand.h"
#include <exception>
#include <iostream>

namespace gbcemu {

std::unordered_map<DebuggerCommand::Command, DebuggerCommand *> DebuggerCommand::_s_command_cache;

DebuggerCommand::DebuggerCommand(DebuggerCommand::Command const command, std::string const &input)
    : command(command), m_input(input), m_command_info(s_command_info_map[static_cast<int>(command)]) {
    parse_input();
}

void DebuggerCommand::set_input(std::string const &input) {
    m_input = input;
    parse_input();
}

void DebuggerCommand::parse_input() {
    if (!m_command_info.has_option)
        return;

    auto ws = m_input.find(' ');
    if (ws == std::string::npos)
        m_input = "";
    else
        m_input = m_input.substr(ws + 1, m_input.length() - ws - 1);
}

std::string DebuggerCommand::get_command_data() const { return m_input; }

bool DebuggerCommand::try_get_numeric_argument(uint16_t *arg, int base) const {
    auto to_parse = command == DebuggerCommand::Command::Show ? m_input.substr(4, m_input.length() - 4) : m_input; // remove "mem" in the beginning
    return try_parse_as_number(to_parse, arg, base);
}

bool DebuggerCommand::try_get_address_pair_arg(DebuggerCommand::address_pair *arg) const {
    auto to_parse = command == DebuggerCommand::Command::Show ? m_input.substr(4, m_input.length() - 4) : m_input; // remove "mem" in the beginning
    auto hypen_location = to_parse.find('-');

    return try_parse_as_number(to_parse.substr(0, hypen_location), &arg->first, 16) &&
           try_parse_as_number(to_parse.substr(hypen_location + 1, to_parse.length() - hypen_location - 1), &arg->second, 16);
}

bool DebuggerCommand::try_parse_as_number(std::string const &s, uint16_t *result, int base) {
    try {
        std::size_t pos{};
        uint32_t r;
        r = std::stoi(s, &pos, base);
        if (r > 0xFFFF)
            return false;
        *result = (uint16_t)r;
        return true;
    } catch (std::exception const &) {
        return false;
    }
}

void DebuggerCommand::print_command_help(std::ostream &stream) const {
    if (command == DebuggerCommand::Command::Show) {
        stream << "available options: ";
        stream << "[cpu|cartridge|mem a16|mem a16-a16]" << std::endl;
    } else if (command == DebuggerCommand::Command::Trace) {
        stream << "available options: ";
        stream << "[frame]" << std::endl;
    } else {
        stream << m_command_info.description << std::endl;
    }
}
}