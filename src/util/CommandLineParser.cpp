#include "CommandLineParser.h"
#include "CommandLineArgument.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace gbcemu {

CommandLineParser::CommandLineParser() {
    for (auto const &it : s_arguments) {
        m_argument_options.insert(std::make_pair(it.first, std::make_shared<CommandLineArgument>(it.first, std::get<0>(it.second), std::get<1>(it.second),
                                                                                                 std::get<2>(it.second), std::get<3>(it.second))));
    }
}

void CommandLineParser::parse(int argc, char **argv) {
    std::vector<ArgumentType> missing_arguments;
    for (auto const &it : m_argument_options) {
        bool found_argument = false;
        for (auto i = 1; i < argc; i++) {
            if (argv[i][0] != '-')
                continue;

            if (!it.second->is_command(argv[i]))
                continue;

            if (it.second->is_switch) {
                found_argument = true;
                break;
            } else if (i < (argc - 1) && it.second->parameter_is_valid(argv[i + 1])) {
                it.second->set_value(argv[i + 1]);
                found_argument = true;
                break;
            }
        }

        if (!found_argument)
            it.second->set_as_missing();
    }
}

bool CommandLineParser::has_argument(ArgumentType type) { return m_argument_options.find(type)->second->argument_type != ArgumentType::Missing; }

std::shared_ptr<CommandLineArgument> CommandLineParser::get_argument(ArgumentType type) { return m_argument_options.find(type)->second; }

void CommandLineParser::print_usage_string(std::ostream &stream, const std::string &program_name) {
    stream << "usage: " << program_name << " ";
    int i = 0;
    for (auto const &it : m_argument_options) {
        stream << it.second->get_regexp_in_usage();

        if (i < m_argument_options.size() - 1)
            stream << " ";
        i++;
    }
}

void CommandLineParser::print_options(std::ostream &stream) {
    stream << "options:\n";
    for (auto const &it : m_argument_options) {
        stream << std::left << std::setw(20) << it.second->get_regexp_printable() << std::right << it.second->get_help_text() << std::endl;
    }
}
}