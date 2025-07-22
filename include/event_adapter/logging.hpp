#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace event_adapter {

class Logger {
public:
    static void initialize(const std::string& name = "event_adapter", 
                         spdlog::level::level_enum level = spdlog::level::info,
                         bool console = true,
                         const std::string& file_path = "") {
        std::vector<spdlog::sink_ptr> sinks;
        
        if (console) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(level);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [thread %t] %v");
            sinks.push_back(console_sink);
        }
        
        if (!file_path.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                file_path, 10 * 1024 * 1024, 3); // 10MB max file size, 3 files
            file_sink->set_level(level);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [thread %t] %v");
            sinks.push_back(file_sink);
        }
        
        auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        logger->set_level(level);
        logger->flush_on(spdlog::level::warn);
        
        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
    }
    
    static std::shared_ptr<spdlog::logger> get(const std::string& name = "") {
        if (name.empty()) {
            return spdlog::default_logger();
        }
        auto logger = spdlog::get(name);
        if (!logger) {
            logger = spdlog::default_logger()->clone(name);
            spdlog::register_logger(logger);
        }
        return logger;
    }
    
    static void set_level(spdlog::level::level_enum level) {
        spdlog::set_level(level);
    }
    
    static void set_pattern(const std::string& pattern) {
        spdlog::set_pattern(pattern);
    }
    
    static void flush() {
        spdlog::default_logger()->flush();
    }
    
    static void shutdown() {
        spdlog::shutdown();
    }
};

// Convenience macros for logging
#define EVENT_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define EVENT_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define EVENT_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define EVENT_LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define EVENT_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define EVENT_LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

// Component-specific logging macros
#define EVENT_LOG_COMPONENT(component, level, ...) \
    event_adapter::Logger::get(component)->level(__VA_ARGS__)

} // namespace event_adapter