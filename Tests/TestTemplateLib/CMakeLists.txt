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
# The targets are read from HCP65IDE.sln, the solution it is a project of (codaphela_solution): the libraries its
# GCC-x64 rows build, each one's sources from its .vcxproj, so Visual Studio stays the source of truth.  Building
# re-runs the configure step when the solution or a project changes.  Only the libraries the project references are
# built (REFERENCED).  Test directories can contain .cpp files that are test data, so never glob *.cpp to find
# sources.

cmake_minimum_required(VERSION 3.24)
project(TestTemplateLib CXX)


# This test is in the HCP65-IDE repository (with TemplateLib), which is beside CommonLibraries.
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options("${COMMON_DIR}" "${IDE_ROOT}/Libraries")

codaphela_solution("${IDE_ROOT}/HCP65IDE/HCP65IDE.sln" TESTS TestTemplateLib REFERENCED)
