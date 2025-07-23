# Examples build configuration

# Trading system proper example (using event adapter correctly)
add_executable(trading_system_proper trading_system_proper.cpp)
target_link_libraries(trading_system_proper PRIVATE event_adapter)

# Check for nlohmann/json
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(trading_system_proper PRIVATE nlohmann_json::nlohmann_json)
else()
    message(WARNING "nlohmann/json not found, trading example may not compile")
endif()

# Simple example without external dependencies
add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example PRIVATE event_adapter)

