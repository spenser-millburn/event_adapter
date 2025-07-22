#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "event_adapter::spdlog" for configuration "Release"
set_property(TARGET event_adapter::spdlog APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(event_adapter::spdlog PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib64/libspdlog.a"
  )

list(APPEND _cmake_import_check_targets event_adapter::spdlog )
list(APPEND _cmake_import_check_files_for_event_adapter::spdlog "${_IMPORT_PREFIX}/lib64/libspdlog.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
