#pragma once

#include "spdlog/spdlog.h"
#include <string>
#include <unordered_map>

namespace gbcemu {
enum class LoggerType { Internal, Cpu, Mmu, Cartridge };

enum class LogLevel { Trace, Debug, Info, Warning, Error, Critical };

class LogUtilities {
  public:
    static void log(LoggerType type, LogLevel log_level, const std::string &log_message);
    static void init();
    static std::string to_tf(const bool b) { return b ? "true" : "false"; }

  private:
    static const std::unordered_map<LoggerType, std::string> m_logger_names;
    static std::unordered_map<LoggerType, std::shared_ptr<spdlog::logger>> &loggers();
};
}