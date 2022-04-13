#include "LogUtilities.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace gbcemu {

const std::unordered_map<LoggerType, std::string>
    LogUtilities::m_logger_names = { { LoggerType::Internal, "internal" },
                                     { LoggerType::Cpu, "cpu" },
                                     { LoggerType::Mmu, "mmu" } };

std::unordered_map<LoggerType, std::shared_ptr<spdlog::logger>> &
LogUtilities::loggers() {
    static std::unordered_map<LoggerType, std::shared_ptr<spdlog::logger>>
        loggers;
    return loggers;
}

void LogUtilities::init() { spdlog::set_level(spdlog::level::trace); }

void LogUtilities::log(LoggerType type, LogLevel log_level,
                       const std::string &log_message) {
    auto logger = loggers().find(type);
    if (logger == loggers().end()) {
        loggers().emplace(
            type, spdlog::stdout_color_mt(m_logger_names.find(type)->second));
        return log(type, log_level, log_message);
    }

    switch (log_level) {

    case LogLevel::Trace:
        logger->second->trace(log_message);
        break;
    case LogLevel::Debug:
        logger->second->debug(log_message);
        break;
    case LogLevel::Info:
        logger->second->info(log_message);
        break;
    case LogLevel::Warning:
        logger->second->warn(log_message);
        break;
    case LogLevel::Error:
        logger->second->error(log_message);
        break;
    case LogLevel::Critical:
        logger->second->critical(log_message);
        break;
    default:
        break;
    }
}
}