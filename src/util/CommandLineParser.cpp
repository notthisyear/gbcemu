#include "CommandLineParser.h"
#include "CommandLineArgument.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace gbcemu {

CommandLineParser::CommandLineParser() {
    for (std::size_t i{ 0 }; i < kNumberOfArguments; ++i) {
        m_argument_options.insert({ kArguments[i].type, std::make_shared<CommandLineArgument>(std::move(kArguments[i])) });
    }
}

bool CommandLineParser::try_parse(int argc, char **argv) {
    std::vector<std::size_t> arguments_accounted_for{};
    bool has_errors{ false };
    for (auto const &it : m_argument_options) {
        bool found_argument{ false };
        for (std::size_t i{ 1 }; i < argc; ++i) {
            if (std::find(arguments_accounted_for.cbegin(), arguments_accounted_for.cend(), i) != arguments_accounted_for.cend()) {
                continue;
            }

            if (argv[i][0] != '-') {
                continue;
            }

            // It could be that the current argument isn't the one we're looking for
            if (!it.second->is_command(argv[i])) {
                continue;
            }

            // Either the current argument is a switch
            if (it.second->is_switch()) {
                found_argument = true;
                arguments_accounted_for.push_back(i);
                break;
            }
            // ...or, it should have some value
            else if (i < (argc - 1) && it.second->parameter_is_valid(argv[i + 1])) {
                it.second->set_value(argv[i + 1]);
                found_argument = true;
                arguments_accounted_for.push_back(i);
                arguments_accounted_for.push_back(i + 1);
                break;
            }
            // If not, then it's a known argument with an invalid value
            else {
                has_errors = true;
                m_parsing_error_argument_index = i + 1;
                break;
            }
        }

        if (found_argument) {
            it.second->set_found();
        }

        if (has_errors) {
            break;
        }
    }

    if (has_errors) {
        return false;
    }

    if (arguments_accounted_for.size() != (argc - 1)) {
        for (std::size_t i{ 1 }; i < argc; ++i) {
            if (std::find(arguments_accounted_for.cbegin(), arguments_accounted_for.cend(), i) == arguments_accounted_for.cend()) {
                m_parsing_error_argument_index = i;
                break;
            }
        }
        return false;
    }

    return true;
}

std::size_t CommandLineParser::parsing_error_argument_index() const { return m_parsing_error_argument_index; }

bool CommandLineParser::has_argument(const CommandData::ArgumentType type) const { return m_argument_options.find(type)->second->is_found(); }

std::string CommandLineParser::get_argument_value(const CommandData::ArgumentType type) const { return m_argument_options.find(type)->second->value(); }

void CommandLineParser::print_usage_string(std::ostream &stream, std::string const &program_name) const {
    stream << "usage: " << program_name << " ";
    std::size_t i{ 0 };
    std::size_t const number_of_arguments{ m_argument_options.size() };
    for (auto const &it : m_argument_options) {
        stream << it.second->get_command_in_usage();

        if (i < (number_of_arguments - 1))
            stream << " ";
        i++;
    }
}

void CommandLineParser::print_options(std::ostream &stream) const {
    stream << "options:\n";
    for (auto const &it : m_argument_options) {
        stream << std::left << std::setw(30) << it.second->get_command_in_help() << std::right << it.second->get_help_text() << std::endl;
    }
}
}