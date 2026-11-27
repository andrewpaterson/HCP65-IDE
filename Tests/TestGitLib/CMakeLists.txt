# Linux build of TestGitLib (BaseLib + TestLib + StandardLib + ThreadLib + GitLib + TestGitLib).  The tests run git,
# which must be on the path.
#
# Builds 32-bit (-m32, needs g++-multilib) by default.  Add -DBASELIB_64=ON
# for a 64-bit build, in its own build directory:
#
#   cmake -S HCP65-IDE/Tests/TestGitLib -B build/TestGitLib64 -G Ninja -DBASELIB_64=ON
#
#   cmake -S HCP65-IDE/Tests/TestGitLib -B build/TestGitLib -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/TestGitLib
#   ctest --test-dir build/TestGitLib -V
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# Source lists are read from the .vcxproj files so Visual Studio stays the
# source of truth.  Building re-runs the configure step automatically when a
# .vcxproj changes.  Test directories can contain .cpp files that are test data,
# so never glob *.cpp to find sources.

cmake_minimum_required(VERSION 3.19)
project(TestGitLib CXX)


# This test is in the HCP65-IDE repository (with GitLib), which is beside CommonLibraries.
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(TESTLIB_DIR "${COMMON_DIR}/TestLib")
set(STANDARDLIB_DIR "${COMMON_DIR}/StandardLib")
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(GITLIB_DIR "${IDE_ROOT}/Libraries/GitLib")
set(TESTGITLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TestGitLib")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(TESTLIB_SOURCES "${TESTLIB_DIR}/TestLib.vcxproj")

vcxproj_sources(STANDARDLIB_SOURCES "${STANDARDLIB_DIR}/StandardLib.vcxproj")

vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(GITLIB_SOURCES "${GITLIB_DIR}/GitLib.vcxproj")

vcxproj_sources(TESTGITLIB_SOURCES "${TESTGITLIB_DIR}/TestGitLib.vcxproj")


codaphela_options("${COMMON_DIR}" "${IDE_ROOT}/Libraries")


# CMutex uses pthreads on Linux.
find_package(Threads REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options)
target_link_libraries(BaseLib PUBLIC Threads::Threads)

# Uses AVX intrinsics unconditionally; MSVC allows that without a flag, g++ does not.

add_library(TestLib STATIC ${TESTLIB_SOURCES})
target_link_libraries(TestLib PUBLIC BaseLib)

add_library(StandardLib STATIC ${STANDARDLIB_SOURCES})
target_link_libraries(StandardLib PUBLIC BaseLib)

add_library(ThreadLib STATIC ${THREADLIB_SOURCES})
target_link_libraries(ThreadLib PUBLIC BaseLib)

# GitLib runs git (ThreadLib's CProcess) and compares texts with StandardLib's diff.
add_library(GitLib STATIC ${GITLIB_SOURCES})
target_link_libraries(GitLib PUBLIC StandardLib ThreadLib BaseLib)

add_executable(TestGitLib ${TESTGITLIB_SOURCES})
target_link_libraries(TestGitLib PRIVATE TestLib GitLib StandardLib ThreadLib BaseLib)

# The tests read their input (Input/ ...) relative to the project directory,
# as Visual Studio does.
enable_testing()
add_test(NAME TestGitLib COMMAND TestGitLib WORKING_DIRECTORY "${TESTGITLIB_DIR}")
