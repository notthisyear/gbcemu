#include "LogUtilities.h"
#include <iostream>

namespace gbcemu {

void LogUtilities::log_info(std::ostream &stream, std::string const &msg) {
    stream << "\033[1;32m[info]"
           << "\033[0m " << msg << std::endl;
}
void LogUtilities::log_warning(std::ostream &stream, std::string const &msg) {
    stream << "\033[1;33m[warning]"
           << "\033[0m " << msg << std::endl;
}
void LogUtilities::log_error(std::ostream &stream, std::string const &msg) {
    stream << "\033[1;31m[error]"
           << "\033[0m " << msg << std::endl;
}
}