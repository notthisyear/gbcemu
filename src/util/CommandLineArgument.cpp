#include "CommandLineArgument.h"
#include <iostream>
#include <regex>
#include <string>

namespace gbcemu {

std::string CommandLineArgument::get_arguments_from_raw_command_line_content(char *raw_input, int argc) {
    if (argc == 1)
        return std::string();

    auto total_length = std::strlen(raw_input);
    bool found_break = false;
    int idx = 0;
    char *arguments;

    for (auto i = 0; i < total_length; i++) {
        if (found_break) {
            arguments[i - idx - 1] = raw_input[i];
            continue;
        }

        if (raw_input[i] == ' ') {
            idx = i;
            found_break = true;
            arguments = new char[total_length - idx];
        }
    }

    if (!found_break)
        return std::string();

    arguments[total_length - idx - 1] = '\0';
    auto result = std::string(arguments);
    delete[] arguments;

    return result;
}

CommandLineArgument *CommandLineArgument::get_argument(int argc, char **argv, CommandLineArgument::ArgumentType type, bool *did_match) {
    auto total_length = 0;
    for (int i = 1; i < argc; i++)
        total_length += std::strlen(argv[i]);

    char *combined_argument = new char[total_length + argc - 1];
    auto combined_str_idx = 0;
    for (int i = 1; i < argc; i++) {
        for (int j = 0; j < strlen(argv[i]); j++)
            combined_argument[combined_str_idx++] = argv[i][j];
        combined_argument[combined_str_idx++] = i == argc - 1 ? '\0' : ' ';
    }
    std::string result(combined_argument);
    delete[] combined_argument;
    return get_argument(result, type, did_match);
}

// TODO: Rework this so that only the thing after the argument named is parsed
CommandLineArgument *CommandLineArgument::get_argument(const std::string &input, CommandLineArgument::ArgumentType type, bool *did_match) {
    auto regexp = get_regexp(type);

    std::smatch sm;
    std::regex r(regexp, std::regex_constants::icase);
    *did_match = std::regex_search(input, sm, r);
    return (*did_match == true) ? new CommandLineArgument(type, sm[0]) : nullptr;
}

void CommandLineArgument::fix_path() {
    bool remove_quotes = value[0] == '"' && value[value.length() - 1] == '"';
    auto number_of_backslashes = std::count(value.begin(), value.end(), '\\');

    auto new_length = value.length() + number_of_backslashes + (remove_quotes ? -2 : 0);
    auto new_path_string = new char[new_length + 1];

    auto i = 0;
    auto j = 0;

    while (true) {
        if (i == 0 && remove_quotes) {
            j--;
        } else if (i == value.length() - 1 && remove_quotes) {
            break;
        } else if (i == value.length()) {
            break;
        } else {
            new_path_string[j] = value[i];
            if (number_of_backslashes > 0) {
                if (value[i] == '\\')
                    new_path_string[++j] = '\\';
            }
        }

        i++;
        j++;
    }

    new_path_string[j] = '\0';
    auto s = std::string(new_path_string);
    delete[] new_path_string;
    value = s;
}
}