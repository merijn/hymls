cmake_minimum_required(VERSION 3.15.0)
project(HYMLS C CXX)

if (DEFINED ENV{TRILINOS_DIR} AND NOT DEFINED Trilinos_DIR)
  set (Trilinos_DIR $ENV{TRILINOS_DIR}/lib/cmake/Trilinos)
  message("FOO" ${Trilinos_DIR})
elseif (APPLE AND NOT DEFINED ENV{TRILINOS_DIR} AND NOT DEFINED Trilinos_DIR)
  # By default look in the homebrew install directory
  set (Trilinos_DIR /usr/local/opt/trilinos/lib/cmake/Trilinos/)
  message("BAR" ${Trilinos_DIR})
endif ()

# Some options
option(HYMLS_USE_OPENMP OFF "allow using multi-threading inside HYMLS")

option(HYMLS_USE_PHIST OFF "try to find PHIST and enable it as eigensolver with HYMLS")

if (HYMLS_USE_PHIST)
  option(HYMLS_USE_PHIST_CORRECTION_SOLVER ON "If set to OFF (the default), HYMLS provides it's own solver of the Jacobi-Davidson correction equation. If set to ON, the PHIST implementation is used.")
endif()

option(HYMLS_USE_JDQZPP OFF "try to find JDQZPP and enable it as eigensolver with HYMLS")

option(HYMLS_STORE_MATRICES "dump all matrices, maps etc. encountered (huge overhead)" OFF)
option(HYMLS_DEBUGGING "turns on verbose debugging output" OFF)
option(HYMLS_FUNCTION_TRACING "turns on very verbose output on every function entered/left" OFF)
option(HYMLS_MEMORY_PROFILING "report memory usage after calling a function, is really slow" OFF)
option(HYMLS_LONG_LONG "use long long global indices" OFF)

set(HYMLS_TEST_NPROCS 8 CACHE STRING "amount of processors to use for the integration tests")

set(HYMLS_TIMING_LEVEL 2 CACHE STRING "which function should be timed/traced (0: none, 1: major functions ... 99: all")

if (CMAKE_BUILD_TYPE MATCHES "Debug")
  message(STATUS "enabling TESTING")
  option(HYMLS_TESTING "turns on on the fly tests" ON)

  if (HYMLS_STORE_MATRICES)
    message(STATUS "enabling STORE_MATRICES output")
  endif()

  if (HYMLS_DEBUGGING)
    message(STATUS "enabling DEBUGGING output")
  endif()
  if (HYMLS_FUNCTION_TRACING)
    message(STATUS "enabling FUNCTION_TRACING output")
  endif()
else()
  option(HYMLS_TESTING "turns on on the fly tests" OFF)
endif()

# HYMLS version
execute_process(COMMAND git rev-parse --short --sq HEAD WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} OUTPUT_VARIABLE rev)
STRING(REPLACE "'" "\"" HYMLS_REVISION ${rev})

include("${PROJECT_SOURCE_DIR}/cmake/common.cmake")

# here are the HYMLS headers:
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/src)

# and here are any configured headers, e.g. HYMLS_config.h:
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/src/)

# enable "make test" and "make check"  targets
enable_testing()

add_subdirectory(src)
add_subdirectory(testSuite)
add_subdirectory(python)

####################
# uninstall target #
####################

configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake")

set(CTEST_OPTIONS --force-new-ctest-process --verbose CACHE STRING "ctest options")

# Adding custom test target "check" because it is the only way to show the full output of test errors on console
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} ${CTEST_OPTIONS})

