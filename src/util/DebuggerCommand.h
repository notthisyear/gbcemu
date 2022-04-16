#pragma once

#include <regex>
#include <string>

namespace gbcemu {

class DebuggerCommand {

  public:
    enum class Command {
        Help,
        Show,
        SetBreakpoint,
        ClearBreakpoint,
        Step,
        Run,
        None,
    };

    static DebuggerCommand::Command get_debugger_cmd(const std::string &input) {
        int i = 0;
        for (const auto &cmd_regex : m_command_regex) {
            std::regex r(cmd_regex, std::regex_constants::icase);
            if (std::regex_search(input, r))
                return static_cast<DebuggerCommand::Command>(i);
            i++;
        }
        __builtin_unreachable();
    }

  private:
    static inline std::string m_command_regex[] = {
        "(h)|(help)", "(sh)|(show)", "(sb)|(setbreakpoint)", "(cb)|(clearbreakpoint)", "(st)|(step)", "(r)|(run)", "*",
    };
};

}