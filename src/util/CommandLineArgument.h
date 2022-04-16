#pragma once

#include "GeneralUtilities.h"
#include <iomanip>
#include <map>
#include <memory>
#include <ostream>
#include <string>

namespace gbcemu {

class CommandLineArgument {

  public:
    enum class ArgumentType {
        Help = 0,
        BootRomPath = 1,
        CartridgePath = 2,
    };

    ArgumentType argument_type;
    std::string value;

    CommandLineArgument(ArgumentType type, std::string v) : argument_type(type) {
        if (!argument_is_switch(argument_type)) {
            auto ws = v.find(' ');
            value = v.substr(ws + 1, v.length() - ws - 1);
        }
    }

    static CommandLineArgument *get_debugger_cmd(int argc, char **argv, CommandLineArgument::ArgumentType type, bool *did_match);
    static CommandLineArgument *get_debugger_cmd(const std::string &input, CommandLineArgument::ArgumentType type, bool *did_match);

    static void print_usage_string(std::ostream &stream, const std::string &program_name) {
        stream << "usage: " << program_name << " ";
        int i = 0;
        for (auto const &it : m_argument_regexp_and_help) {
            stream << get_regexp_in_usage(it.first);

            if (i < m_argument_regexp_and_help.size() - 1)
                stream << " ";
            i++;
        }
    }

    static void print_options(std::ostream &stream) {
        stream << "options:\n";
        for (auto const &it : m_argument_regexp_and_help) {
            stream << std::left << std::setw(20) << CommandLineArgument::get_regexp_printable(it.first) << std::right << get_help_text(it.first) << std::endl;
        }
    }

    void fix_path();

    ~CommandLineArgument() {}

  private:
    static std::string get_regexp(ArgumentType type) { return m_argument_regexp_and_help.find(type)->second.first; }
    static std::string get_help_text(ArgumentType type) { return m_argument_regexp_and_help.find(type)->second.second; }
    static bool argument_is_switch(ArgumentType type) { return get_regexp(type).find(' ') == std::string::npos; }
    static std::string get_regexp_printable(const ArgumentType type) {
        auto regexp = get_regexp(type);
        auto location_of_whitespace = regexp.find(' ');
        auto last_idx = location_of_whitespace != std::string::npos ? location_of_whitespace : regexp.length();

        auto number_of_parentheses = 2 * std::count(regexp.begin(), regexp.begin() + last_idx, '(');
        auto number_of_variants = number_of_parentheses / 2;

        char *buffer = new char[last_idx - number_of_parentheses + number_of_variants];
        auto result_idx = 0;
        for (int i = 0; i < last_idx; i++) {
            if (regexp[i] == '(' || regexp[i] == ')') {
                continue;
            } else if (regexp[i] == '|') {
                buffer[result_idx++] = ',';
                buffer[result_idx++] = ' ';
            } else {
                buffer[result_idx++] = regexp[i];
            }
        }
        buffer[result_idx] = '\0';
        std::string result(buffer);
        delete[] buffer;
        return result;
    }

    static std::string get_regexp_in_usage(const ArgumentType type) {
        auto regexp = get_regexp(type);
        auto location_of_split = regexp.find('|');
        auto location_of_whitespace = regexp.find(' ');

        auto last_idx = (location_of_split != std::string::npos) ? location_of_split
                                                                 : (location_of_whitespace != std::string::npos ? location_of_whitespace : regexp.length());

        char *buffer = new char[last_idx + 1];
        char prev_chr;
        int buffer_idx = 0;
        for (int i = 0; i < last_idx; i++) {
            if (regexp[i] == '(') {
                if (i > 0 && prev_chr == '(')
                    continue;
                buffer[buffer_idx++] = '[';

            } else if (regexp[i] == ')') {
                if (i > 0 && prev_chr == '(')
                    continue;
                buffer[buffer_idx++] = ']';
            } else {
                buffer[buffer_idx++] = regexp[i];
            }
            prev_chr = regexp[i];
        }

        buffer[buffer_idx] = '\0';
        std::string result(buffer);
        delete[] buffer;
        return result;
    }

  private:
    static inline const std::map<CommandLineArgument::ArgumentType, std::pair<std::string, std::string>> m_argument_regexp_and_help = {
        { CommandLineArgument::ArgumentType::Help, std::make_pair<std::string, std::string>("(-h)|(--help)", "show this help message and exit") },
        { CommandLineArgument::ArgumentType::BootRomPath,
          std::make_pair<std::string, std::string>(R"((--boot-rom) ([\w\\:\.-/\\(\\)\[\]]+))", "path to boot rom") },
        { CommandLineArgument::ArgumentType::CartridgePath,
          std::make_pair<std::string, std::string>(R"(((-c)|(--cartridge)) ([\w\\:\.-/\\(\\)\[\]]+))", "path to cartridge") },
    };
};
}