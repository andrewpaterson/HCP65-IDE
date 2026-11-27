# Linux build of TestClaudeLib (BaseLib + TestLib + ThreadLib + ClaudeLib + TestClaudeLib).  The tests don't run
# claude.
#
# Builds 32-bit (-m32, needs g++-multilib) by default.  Add -DBASELIB_64=ON
# for a 64-bit build, in its own build directory:
#
#   cmake -S HCP65-IDE/Tests/TestClaudeLib -B build/TestClaudeLib64 -G Ninja -DBASELIB_64=ON
#
#   cmake -S HCP65-IDE/Tests/TestClaudeLib -B build/TestClaudeLib -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/TestClaudeLib
#   ctest --test-dir build/TestClaudeLib -V
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# Source lists are read from the .vcxproj files so Visual Studio stays the
# source of truth.  Building re-runs the configure step automatically when a
# .vcxproj changes.  Test directories can contain .cpp files that are test data,
# so never glob *.cpp to find sources.

cmake_minimum_required(VERSION 3.19)
project(TestClaudeLib CXX)


# This test is in the HCP65-IDE repository (with ClaudeLib), which is beside CommonLibraries.
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(TESTLIB_DIR "${COMMON_DIR}/TestLib")
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(CLAUDELIB_DIR "${IDE_ROOT}/Libraries/ClaudeLib")
set(TESTCLAUDELIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TestClaudeLib")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(TESTLIB_SOURCES "${TESTLIB_DIR}/TestLib.vcxproj")

vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(CLAUDELIB_SOURCES "${CLAUDELIB_DIR}/ClaudeLib.vcxproj")

vcxproj_sources(TESTCLAUDELIB_SOURCES "${TESTCLAUDELIB_DIR}/TestClaudeLib.vcxproj")


codaphela_options("${COMMON_DIR}" "${IDE_ROOT}/Libraries")


# CMutex uses pthreads on Linux.
find_package(Threads REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options)
target_link_libraries(BaseLib PUBLIC Threads::Threads)

# Uses AVX intrinsics unconditionally; MSVC allows that without a flag, g++ does not.

add_library(TestLib STATIC ${TESTLIB_SOURCES})
target_link_libraries(TestLib PUBLIC BaseLib)

add_library(ThreadLib STATIC ${THREADLIB_SOURCES})
target_link_libraries(ThreadLib PUBLIC BaseLib)

# ClaudeLib runs claude with ThreadLib's CProcess.
add_library(ClaudeLib STATIC ${CLAUDELIB_SOURCES})
target_link_libraries(ClaudeLib PUBLIC ThreadLib BaseLib)

add_executable(TestClaudeLib ${TESTCLAUDELIB_SOURCES})
target_link_libraries(TestClaudeLib PRIVATE TestLib ClaudeLib ThreadLib BaseLib)

# The tests read their input (Input/ ...) relative to the project directory,
# as Visual Studio does.
enable_testing()
add_test(NAME TestClaudeLib COMMAND TestClaudeLib WORKING_DIRECTORY "${TESTCLAUDELIB_DIR}")
