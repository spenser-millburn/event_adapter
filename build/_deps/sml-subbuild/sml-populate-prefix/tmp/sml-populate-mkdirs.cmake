# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-src")
  file(MAKE_DIRECTORY "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-src")
endif()
file(MAKE_DIRECTORY
  "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-build"
  "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-subbuild/sml-populate-prefix"
  "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-subbuild/sml-populate-prefix/tmp"
  "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-subbuild/sml-populate-prefix/src/sml-populate-stamp"
  "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-subbuild/sml-populate-prefix/src"
  "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-subbuild/sml-populate-prefix/src/sml-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-subbuild/sml-populate-prefix/src/sml-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/spensermillburn/sliink/repo/event_adapter/build/_deps/sml-subbuild/sml-populate-prefix/src/sml-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
