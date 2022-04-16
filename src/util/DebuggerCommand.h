#pragma once

#include <regex>
#include <string>

namespace gbcemu {

typedef struct CommandInfo CommandInfo;
struct CommandInfo {
    std::string regex;
    std::string name;
    std::string command;
    std::string description;
    bool has_option;
    bool include;
    std::string data;
    CommandInfo *sub_command;
};

class DebuggerCommand {

  public:
    enum class Command {
        Help,
        Show,
        SetBreakpoint,
        ClearBreakpoint,
        Step,
        Run,
        Break,
        None,
    };

    static DebuggerCommand::Command get_debugger_cmd(const std::string &input) {
        int i = 0;
        for (const auto &cmd_indo : m_command_regex) {
            std::regex r(cmd_indo.regex, std::regex_constants::icase);
            if (std::regex_search(input, r))
                return static_cast<DebuggerCommand::Command>(i);
            i++;
        }
        __builtin_unreachable();
    }

  private:
    static inline CommandInfo m_command_regex[] = {
        { "(^((h)|(help))$)", "help", "[h|help]", "show available commands", false, true },
        { "((sh)|(show))", "show", "[sh|show]", "show registers and memory, type 'show help' to see options", true, true },
        { "((sb)|(setbreakpoint))", "set breakpoint", "[sb|setbreakpoint] a16", "set a breakpoint at address a16", true, true },
        { "((cb)|(clearbreakpoint))", "clear breakpoint", "[cb|clearbreakpoint] [all|a16]", "clear either all breakpoints or at address a16", true, true },
        { "((st)|(step))", "step", "[st|step]", "step execution one tick", false, true },
        { "(^((r)|(run))$)", "run", "[r|run]", "let execution run until a breakpoint is hit", false, true },
        { "((br)|(break))", "break", "[br|break]", "immediately stop execution", false, true },
        { "\\.*", "invalid", "inv", "used as catchall", false, false },
    };
};

}