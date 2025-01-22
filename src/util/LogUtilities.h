#pragma once

#include <string>

namespace gbcemu {

class LogUtilities final {
  public:
    static void log_info(std::ostream &, std::string const &);
    static void log_warning(std::ostream &, std::string const &);
    static void log_error(std::ostream &, std::string const &);
    static std::string to_tf(bool const b) { return b ? "true" : "false"; }
};
}