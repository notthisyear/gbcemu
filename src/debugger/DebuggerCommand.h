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
        Break,
        Trace,
        None,
    };

    DebuggerCommand::Command command;
    DebuggerCommand(DebuggerCommand::Command const, std::string const & = "");

    static DebuggerCommand *get_debugger_cmd(std::string const &input) {
        int i = 0;
        for (auto const &cmd_info : s_command_info_map) {
            std::regex r(cmd_info.regex, std::regex_constants::icase);
            if (std::regex_search(input, r)) {
                DebuggerCommand *result;
                auto cmd = static_cast<DebuggerCommand::Command>(i);
                auto it = _s_command_cache.find(cmd);

                if (it != _s_command_cache.end()) {
                    result = it->second;
                    result->set_input(input);
                } else {
                    result = new DebuggerCommand(cmd, input);
                    _s_command_cache.insert({ cmd, result });
                }
                return result;
            }

            i++;
        }
        __builtin_unreachable();
    }

    static void print_commands(std::ostream &stream) {
        for (auto const &cmd_info : s_command_info_map) {
            if (cmd_info.include)
                stream << std::left << std::setw(35) << cmd_info.command << std::right << cmd_info.description << std::endl;
        }
    }

    std::string get_command_data() const;

    bool try_get_numeric_argument(uint16_t *, int base = 16) const;

    bool try_get_address_pair_arg(address_pair *) const;

    void print_command_help(std::ostream &) const;

  private:
    std::string m_input;
    CommandInfo m_command_info;

    void set_input(std::string const &);
    void parse_input();

    bool static try_parse_as_number(std::string const &, uint16_t *, int);
    static std::unordered_map<DebuggerCommand::Command, DebuggerCommand *> _s_command_cache;
    static inline CommandInfo s_command_info_map[] = {
        { "(^((h)|(help))$)", "[h|help]", "show available commands", false, true },
        { "((sh)|(show))", "[sh|show]", "show registers and memory, type 'show help' to see options", true, true },
        { "((dasm)|(disassemble))", "[dasm|disassemble] d", "disassemble the next d instructions", true, true },
        { "((sb)|(setbreakpoint))", "[sb|setbreakpoint] a16", "set a breakpoint at address a16", true, true },
        { "((cb)|(clearbreakpoint))", "[cb|clearbreakpoint]", "clear breakpoint", false, true },
        { "((st)|(step))", "[st|step]", "step execution one tick", false, true },
        { "(^((r)|(run))$)", "[r|run]", "let execution run until a breakpoint is hit or break is called", false, true },
        { "((br)|(break))", "[br|break]", "halt execution", false, true },
        { "((tr)|(trace))", "[tr|trace]", "generate traces, type 'trace help' to see options", true, true },
        { "\\.*", "invalid", "used as catchall", false, false },
    };
};

}