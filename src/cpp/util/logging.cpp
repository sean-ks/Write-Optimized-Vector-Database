#include "logging.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <vector>

namespace woved::util {

static std::shared_ptr<spdlog::logger> global_logger;

void init_logging(spdlog::level::level_enum level, bool console_log, const std::string& file_path) {
    std::vector<spdlog::sink_ptr> sinks;
    
    if (console_log) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    
    if (!file_path.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, true));
    }
    
    if (sinks.empty()) {
        // Default to console if no sinks are specified
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    global_logger = std::make_shared<spdlog::logger>("woved", begin(sinks), end(sinks));
    global_logger->set_level(level);
    global_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
    
    spdlog::register_logger(global_logger);
    spdlog::set_default_logger(global_logger);
}

std::shared_ptr<spdlog::logger>& get_logger() {
    if (!global_logger) {
        // Initialize with defaults if not already done
        init_logging();
    }
    return global_logger;
}

} // namespace woved::util