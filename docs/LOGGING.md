# Event Adapter Logging Guide

The Event Adapter framework now includes comprehensive logging using spdlog, providing high-performance, flexible logging capabilities throughout the system.

## Quick Start

```cpp
#include <event_adapter/logging.hpp>

// Initialize logging (typically in main())
event_adapter::Logger::initialize("my_app", spdlog::level::info);

// Use logging macros
EVENT_LOG_INFO("Application started");
EVENT_LOG_DEBUG("Debug information: value = {}", 42);
EVENT_LOG_ERROR("Error occurred: {}", error_message);
```

## Logging Levels

The framework supports standard logging levels (from most to least verbose):
- **TRACE**: Most detailed information, typically for deep debugging
- **DEBUG**: Detailed debugging information
- **INFO**: General informational messages
- **WARN**: Warning messages for potentially problematic situations
- **ERROR**: Error messages for recoverable errors
- **CRITICAL**: Critical errors that may cause system failure

## Basic Usage

### Initialization

```cpp
// Basic initialization with console output
event_adapter::Logger::initialize("app_name", spdlog::level::info);

// With file output
event_adapter::Logger::initialize("app_name", spdlog::level::debug, true, "app.log");

// Console only, no file
event_adapter::Logger::initialize("app_name", spdlog::level::info, true, "");
```

### Logging Macros

```cpp
EVENT_LOG_TRACE("Trace message");
EVENT_LOG_DEBUG("Debug: processing {} items", item_count);
EVENT_LOG_INFO("Connected to server at {}", server_url);
EVENT_LOG_WARN("Queue size {} exceeds threshold", queue_size);
EVENT_LOG_ERROR("Failed to process request: {}", error);
EVENT_LOG_CRITICAL("System failure: {}", reason);
```

### Component-Specific Logging

```cpp
// Get a logger for a specific component
auto logger = event_adapter::Logger::get("network");
logger->info("Network component initialized");

// Or use the macro
EVENT_LOG_COMPONENT("database", error, "Connection failed: {}", error_msg);
```

## Configuration

### Runtime Configuration

```cpp
// Change log level at runtime
event_adapter::Logger::set_level(spdlog::level::debug);

// Change log pattern
event_adapter::Logger::set_pattern("[%l] %v");  // Simple pattern
event_adapter::Logger::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");  // With timestamp
```

### Production Setup

For production environments, you typically want:
- Console output for warnings and above
- File output for info and above
- Structured logging format

```cpp
// Initialize with file logging
event_adapter::Logger::initialize("production_app", spdlog::level::info, true, "app.log");

// Adjust console to show only warnings
auto logger = spdlog::default_logger();
for (auto& sink : logger->sinks()) {
    if (dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(sink.get())) {
        sink->set_level(spdlog::level::warn);
    }
}
```

## Integration with Event Adapter Components

The logging is integrated throughout the Event Adapter framework:

### EventDispatcher Logging
- Logs event registration, dispatching, and processing
- Tracks queue sizes and processing threads
- Reports errors during event handling

### DataSourceAdapter Logging
- Logs adapter lifecycle (connect/disconnect)
- Tracks state changes
- Reports communication errors

### WebSocketAdapter Logging
- Logs connection status
- Tracks message sending/receiving
- Reports parsing errors and connection failures

## Performance Considerations

1. **Async Logging**: For high-throughput applications, consider using spdlog's async mode
2. **Log Levels**: Use appropriate log levels to control output volume
3. **Conditional Logging**: Trace and debug logs are compiled out in release builds when using macros

## Examples

See `examples/logging_config_example.cpp` for comprehensive examples including:
- Different log levels demonstration
- Component-specific logging
- Runtime configuration changes
- File logging setup
- Production configuration

Run the example:
```bash
./logging_config_example levels      # Show different log levels
./logging_config_example components  # Component-specific logging
./logging_config_example runtime     # Runtime configuration
./logging_config_example file        # File logging
./logging_config_example production  # Production setup
```

## Best Practices

1. **Initialize Early**: Set up logging at the start of your application
2. **Use Appropriate Levels**: Choose the right level for each message
3. **Include Context**: Add relevant information to log messages
4. **Avoid Sensitive Data**: Never log passwords, keys, or sensitive information
5. **Structure Messages**: Use consistent formatting for easier parsing
6. **Handle Errors**: Always log errors with context for debugging

## Troubleshooting

### No Output
- Check initialization was called
- Verify log level allows your messages
- Ensure Logger::shutdown() isn't called prematurely

### Performance Issues
- Reduce log level in production
- Consider async logging for high-throughput
- Avoid logging in tight loops

### File Permissions
- Ensure write permissions for log files
- Check disk space availability
- Verify file path is valid