# Tests configuration

# Find GTest
find_package(GTest QUIET)

if(GTest_FOUND)
    # Event tests
    add_executable(event_tests event_tests.cpp)
    target_link_libraries(event_tests PRIVATE event_adapter GTest::GTest GTest::Main)
    add_test(NAME event_tests COMMAND event_tests)
    
    # Adapter tests
    add_executable(adapter_tests adapter_tests.cpp)
    target_link_libraries(adapter_tests PRIVATE event_adapter GTest::GTest GTest::Main)
    add_test(NAME adapter_tests COMMAND adapter_tests)
else()
    message(WARNING "GTest not found, tests will not be built")
endif()