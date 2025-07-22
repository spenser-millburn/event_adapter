# Examples build configuration

# Trading system example
add_executable(trading_system_example trading_system_example.cpp)
target_link_libraries(trading_system_example PRIVATE event_adapter)

# Check for nlohmann/json
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(trading_system_example PRIVATE nlohmann_json::nlohmann_json)
else()
    message(WARNING "nlohmann/json not found, trading example may not compile")
endif()

# Simple example without external dependencies
add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example PRIVATE event_adapter)

# Keyboard input example
add_executable(keyboard_example keyboard_example.cpp)
target_link_libraries(keyboard_example PRIVATE event_adapter)

# Simple keyboard example without state machine
add_executable(keyboard_simple keyboard_simple.cpp)
target_link_libraries(keyboard_simple PRIVATE event_adapter)

# Logging configuration example
add_executable(logging_config_example logging_config_example.cpp)
target_link_libraries(logging_config_example PRIVATE event_adapter)