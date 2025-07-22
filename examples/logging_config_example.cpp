#include <event_adapter/logging.hpp>
#include <event_adapter/event.hpp>
#include <event_adapter/event_dispatcher.hpp>
#include <event_adapter/adapters/websocket_adapter.hpp>
#include <boost/sml.hpp>
#include <iostream>
#include <string>

// Simple event and state machine for demonstration
struct TestEvent { std::string data; };
struct TestStateMachine {
    auto operator()() const {
        using namespace boost::sml;
        return make_transition_table(
            *"idle"_s + event<TestEvent> = "processing"_s
        );
    }
};

void demonstrate_logging_levels() {
    std::cout << "\n=== Demonstrating Different Log Levels ===\n";
    
    // Test each log level
    EVENT_LOG_TRACE("This is a TRACE message - most detailed level");
    EVENT_LOG_DEBUG("This is a DEBUG message - debugging information");
    EVENT_LOG_INFO("This is an INFO message - general information");
    EVENT_LOG_WARN("This is a WARN message - warning conditions");
    EVENT_LOG_ERROR("This is an ERROR message - error conditions");
    EVENT_LOG_CRITICAL("This is a CRITICAL message - critical conditions");
}

void demonstrate_component_logging() {
    std::cout << "\n=== Demonstrating Component-Specific Logging ===\n";
    
    // Create component-specific loggers
    auto dispatcher_logger = event_adapter::Logger::get("dispatcher");
    auto adapter_logger = event_adapter::Logger::get("adapter");
    auto trading_logger = event_adapter::Logger::get("trading");
    
    // Log from different components
    dispatcher_logger->info("Message from dispatcher component");
    adapter_logger->debug("Debug message from adapter component");
    trading_logger->warn("Warning from trading component");
    
    // Using the macro
    EVENT_LOG_COMPONENT("network", info, "Network component message");
    EVENT_LOG_COMPONENT("database", error, "Database error: {}", "connection failed");
}

void demonstrate_runtime_configuration() {
    std::cout << "\n=== Demonstrating Runtime Configuration ===\n";
    
    // Change log level at runtime
    std::cout << "Setting log level to WARN...\n";
    event_adapter::Logger::set_level(spdlog::level::warn);
    
    EVENT_LOG_DEBUG("This DEBUG message won't be shown");
    EVENT_LOG_INFO("This INFO message won't be shown");
    EVENT_LOG_WARN("This WARN message will be shown");
    EVENT_LOG_ERROR("This ERROR message will be shown");
    
    // Change pattern at runtime
    std::cout << "\nChanging log pattern...\n";
    event_adapter::Logger::set_pattern("[%l] %v");
    EVENT_LOG_INFO("Message with new pattern");
    
    // Reset to default pattern
    event_adapter::Logger::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [thread %t] %v");
}

void demonstrate_file_logging() {
    std::cout << "\n=== Demonstrating File Logging ===\n";
    
    // Reinitialize with file logging
    event_adapter::Logger::initialize("file_example", spdlog::level::debug, true, "example.log");
    
    EVENT_LOG_INFO("This message goes to both console and file");
    EVENT_LOG_DEBUG("Debug information saved to file");
    
    // Simulate some operations
    for (int i = 0; i < 5; ++i) {
        EVENT_LOG_INFO("Processing item {}/{}", i + 1, 5);
    }
    
    // Force flush to file
    event_adapter::Logger::flush();
    std::cout << "Check example.log for file output\n";
}

void demonstrate_production_setup() {
    std::cout << "\n=== Production Setup Example ===\n";
    
    // Production setup: console warnings and above, file info and above
    event_adapter::Logger::initialize("production", spdlog::level::info, true, "production.log");
    
    // Set console to show only warnings and above
    auto logger = spdlog::default_logger();
    for (auto& sink : logger->sinks()) {
        if (dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(sink.get())) {
            sink->set_level(spdlog::level::warn);
        }
    }
    
    EVENT_LOG_DEBUG("Debug - only in file");
    EVENT_LOG_INFO("Info - only in file");
    EVENT_LOG_WARN("Warning - in console and file");
    EVENT_LOG_ERROR("Error - in console and file");
}

int main(int argc, char* argv[]) {
    // Initialize with default settings
    event_adapter::Logger::initialize("logging_example", spdlog::level::trace);
    
    std::cout << "Event Adapter Logging Configuration Examples\n";
    std::cout << "==========================================\n";
    
    if (argc > 1) {
        std::string option = argv[1];
        if (option == "levels") {
            demonstrate_logging_levels();
        } else if (option == "components") {
            demonstrate_component_logging();
        } else if (option == "runtime") {
            demonstrate_runtime_configuration();
        } else if (option == "file") {
            demonstrate_file_logging();
        } else if (option == "production") {
            demonstrate_production_setup();
        } else {
            std::cout << "Unknown option: " << option << "\n";
            std::cout << "Available options: levels, components, runtime, file, production\n";
        }
    } else {
        // Run all demonstrations
        demonstrate_logging_levels();
        demonstrate_component_logging();
        demonstrate_runtime_configuration();
        demonstrate_file_logging();
        demonstrate_production_setup();
    }
    
    // Cleanup
    event_adapter::Logger::shutdown();
    
    return 0;
}