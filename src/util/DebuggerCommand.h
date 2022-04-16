#pragma once

#include <iomanip>
#include <ostream>
#include <regex>
#include <string>
#include <unordered_map>

namespace gbcemu {

class DebuggerCommand {

  public:
    typedef struct {
        std::string regex;
        std::string command;
        std::string description;
        bool has_option;
        bool include;
    } CommandInfo;

    typedef std::pair<uint16_t, uint16_t> address_pair;

    enum class Command {
        Help,
        Show,
        Disassemble,
        SetBreakpoint,
        ClearBreakpoint,
        Step,
        Run,
        None,
    };

    DebuggerCommand::Command command;
    DebuggerCommand(const DebuggerCommand::Command, const std::string & = "");

    static DebuggerCommand *get_debugger_cmd(const std::string &input) {
        int i = 0;
        for (const auto &cmd_info : m_command_info_map) {
            std::regex r(cmd_info.regex, std::regex_constants::icase);
            if (std::regex_search(input, r)) {
                DebuggerCommand *result;
                auto cmd = static_cast<DebuggerCommand::Command>(i);
                auto it = _m_command_cache.find(cmd);

                if (it != _m_command_cache.end()) {
                    result = it->second;
                    result->set_input(input);
                } else {
                    result = new DebuggerCommand(cmd, input);
                    _m_command_cache.insert({ cmd, result });
                }
                return result;
            }

            i++;
        }
        __builtin_unreachable();
    }

    static void print_commands(std::ostream &stream) {
        for (const auto &cmd_info : m_command_info_map) {
            if (cmd_info.include)
                stream << std::left << std::setw(35) << cmd_info.command << std::right << cmd_info.description << std::endl;
        }
    }

    std::string get_command_data() const;

    bool try_get_numeric_argument(uint16_t *, int base = 16) const;

    bool try_get_address_pair_arg(address_pair *) const;

    void print_command_help(std::ostream &) const;

  private:
    CommandInfo m_command_info;
    std::string m_input;

    void set_input(const std::string &);
    void parse_input();

    bool static try_parse_as_number(const std::string &, uint16_t *, int);
    static std::unordered_map<DebuggerCommand::Command, DebuggerCommand *> _m_command_cache;
    static inline CommandInfo m_command_info_map[] = {
        { "(^((h)|(help))$)", "[h|help]", "show available commands", false, true },
        { "((sh)|(show))", "[sh|show]", "show registers and memory, type 'show help' to see options", true, true },
        { "((dasm)|(disassemble))", "[dasm|disassemble d]", "disassemble the next d instructions", true, true },
        { "((sb)|(setbreakpoint))", "[sb|setbreakpoint] a16", "set a breakpoint at address a16", true, true },
        { "((cb)|(clearbreakpoint))", "[cb|clearbreakpoint] [all|a16]", "clear either all breakpoints or at address a16", true, true },
        { "((st)|(step))", "[st|step]", "step execution one tick", false, true },
        { "(^((r)|(run))$)", "[r|run]", "let execution run until a breakpoint is hit", false, true },
        { "\\.*", "invalid", "used as catchall", false, false },
    };
};

}