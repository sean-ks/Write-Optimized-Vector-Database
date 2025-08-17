#ifndef WOVED_UTIL_LOGGING_H
#define WOVED_UTIL_LOGGING_H

#include <memory>
#include "spdlog/spdlog.h"
#include "spdlog/logger.h"

namespace woved::util {

/**
 * @brief Initializes the global logger for WOVeD.
 * * @param level The minimum log level to output.
 * @param console_log Whether to log to the console.
 * @param file_path Optional path to a log file.
 */
void init_logging(spdlog::level::level_enum level = spdlog::level::info, 
                  bool console_log = true, 
                  const std::string& file_path = "");

/**
 * @brief Retrieves the global logger instance.
 * * @return A shared pointer to the spdlog logger.
 */
std::shared_ptr<spdlog::logger>& get_logger();

} // namespace woved::util

// Convenience macros for logging
#define LOG_TRACE(...)    ::woved::util::get_logger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::woved::util::get_logger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     ::woved::util::get_logger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::woved::util::get_logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::woved::util::get_logger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::woved::util::get_logger()->critical(__VA_ARGS__)

#endif // WOVED_UTIL_LOGGING_H