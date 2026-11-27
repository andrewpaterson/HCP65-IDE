# Linux build of TestTemplateLib (BaseLib + TestLib + TemplateLib + TestTemplateLib).
#
# Builds 32-bit (-m32, needs g++-multilib) by default.  Add -DBASELIB_64=ON
# for a 64-bit build, in its own build directory:
#
#   cmake -S HCP65-IDE/Tests/TestTemplateLib -B build/TestTemplateLib64 -G Ninja -DBASELIB_64=ON
#
#   cmake -S HCP65-IDE/Tests/TestTemplateLib -B build/TestTemplateLib -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/TestTemplateLib
#   ctest --test-dir build/TestTemplateLib -V
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# Source lists are read from the .vcxproj files so Visual Studio stays the
# source of truth.  Building re-runs the configure step automatically when a
# .vcxproj changes.  Test directories can contain .cpp files that are test data,
# so never glob *.cpp to find sources.

cmake_minimum_required(VERSION 3.19)
project(TestTemplateLib CXX)


# This test is in the HCP65-IDE repository (with TemplateLib), which is beside CommonLibraries.
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(TESTLIB_DIR "${COMMON_DIR}/TestLib")
set(TEMPLATELIB_DIR "${IDE_ROOT}/Libraries/TemplateLib")
set(TESTTEMPLATELIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TestTemplateLib")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(TESTLIB_SOURCES "${TESTLIB_DIR}/TestLib.vcxproj")

vcxproj_sources(TEMPLATELIB_SOURCES "${TEMPLATELIB_DIR}/TemplateLib.vcxproj")

vcxproj_sources(TESTTEMPLATELIB_SOURCES "${TESTTEMPLATELIB_DIR}/TestTemplateLib.vcxproj")


codaphela_options("${COMMON_DIR}" "${IDE_ROOT}/Libraries")


# CMutex uses pthreads on Linux.
find_package(Threads REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options)
target_link_libraries(BaseLib PUBLIC Threads::Threads)

# Uses AVX intrinsics unconditionally; MSVC allows that without a flag, g++ does not.

add_library(TestLib STATIC ${TESTLIB_SOURCES})
target_link_libraries(TestLib PUBLIC BaseLib)

add_library(TemplateLib STATIC ${TEMPLATELIB_SOURCES})
target_link_libraries(TemplateLib PUBLIC BaseLib)

add_executable(TestTemplateLib ${TESTTEMPLATELIB_SOURCES})
target_link_libraries(TestTemplateLib PRIVATE TestLib TemplateLib BaseLib)

# The tests read their input (Input/ ...) relative to the project directory,
# as Visual Studio does.
enable_testing()
add_test(NAME TestTemplateLib COMMAND TestTemplateLib WORKING_DIRECTORY "${TESTTEMPLATELIB_DIR}")
