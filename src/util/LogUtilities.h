#pragma once

#include <string>
#include <unordered_map>

namespace gbcemu {

class LogUtilities {
  public:
    static void log_info(std::ostream &, const std::string &);
    static void log_warning(std::ostream &, const std::string &);
    static void log_error(std::ostream &, const std::string &);
    static std::string to_tf(const bool b) { return b ? "true" : "false"; }
};
}