cmake_minimum_required(VERSION 3.16)
project(event_adapter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent for external dependencies
include(FetchContent)

# Fetch boost::sml
FetchContent_Declare(
    sml
    GIT_REPOSITORY https://github.com/boost-ext/sml.git
    GIT_TAG        v1.1.6
)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
)

# Don't build sml examples/tests/benchmarks
set(SML_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(SML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) 
set(SML_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Don't build spdlog examples/tests/benchmarks
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(sml spdlog)

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(CURL REQUIRED)

# Header-only library
add_library(event_adapter INTERFACE)
target_include_directories(event_adapter INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(event_adapter INTERFACE
    Threads::Threads
    Boost::boost
    spdlog::spdlog
)

# Add sml include directory
target_include_directories(event_adapter INTERFACE
    $<BUILD_INTERFACE:${sml_SOURCE_DIR}/include>
)

# Optional dependencies for adapters
option(ENABLE_WEBSOCKET_ADAPTER "Enable WebSocket adapter (requires websocketpp)" ON)
option(ENABLE_HTTP_ADAPTER "Enable HTTP adapter (requires CURL)" ON)

if(ENABLE_WEBSOCKET_ADAPTER)
    # WebSocketPP is header-only, so we just need to find it
    find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/asio_no_tls_client.hpp)
    if(WEBSOCKETPP_INCLUDE_DIR)
        target_include_directories(event_adapter INTERFACE ${WEBSOCKETPP_INCLUDE_DIR})
        target_compile_definitions(event_adapter INTERFACE EVENT_ADAPTER_HAS_WEBSOCKET)
    else()
        message(WARNING "WebSocketPP not found, WebSocket adapter will not be available")
    endif()
endif()

if(ENABLE_HTTP_ADAPTER)
    target_link_libraries(event_adapter INTERFACE ${CURL_LIBRARIES})
    target_include_directories(event_adapter INTERFACE ${CURL_INCLUDE_DIRS})
    target_compile_definitions(event_adapter INTERFACE EVENT_ADAPTER_HAS_HTTP)
endif()

# Examples
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS event_adapter spdlog
    EXPORT event_adapter-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/event_adapter
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT event_adapter-targets
    FILE event_adapter-targets.cmake
    NAMESPACE event_adapter::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/event_adapter
)

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/event_adapter-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/event_adapter-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/event_adapter
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/event_adapter-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/event_adapter-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/event_adapter-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/event_adapter
)