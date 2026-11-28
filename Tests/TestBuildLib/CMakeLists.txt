# Linux build of TestBuildLib (BaseLib + TestLib + StandardLib + ThreadLib + MicroprocessorLib + Assembler65Lib +
# CppCompilerLib + VisualStudioLib + BuildLib + TestBuildLib).
#
# Builds 32-bit (-m32, needs g++-multilib) by default.  Add -DBASELIB_64=ON
# for a 64-bit build, in its own build directory:
#
#   cmake -S HCP65-IDE/Tests/TestBuildLib -B build/TestBuildLib64 -G Ninja -DBASELIB_64=ON
#
#   cmake -S HCP65-IDE/Tests/TestBuildLib -B build/TestBuildLib -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/TestBuildLib
#   ctest --test-dir build/TestBuildLib -V
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# The targets are read from HCP65IDE.sln, the solution it is a project of (codaphela_solution): the libraries its
# GCC-x64 rows build, each one's sources from its .vcxproj, so Visual Studio stays the source of truth.  Building
# re-runs the configure step when the solution or a project changes.  Only the libraries the project references are
# built (REFERENCED).  Test directories can contain .cpp files that are test data, so never glob *.cpp to find
# sources.

cmake_minimum_required(VERSION 3.24)
project(TestBuildLib CXX)


# This test is in the HCP65-IDE repository (with BuildLib and VisualStudioLib), which is beside CommonLibraries.
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
# Assembler65Lib (which CppCompilerLib's HCP65 compiler uses) is in the HCP65 repository, beside CommonLibraries.
set(HCP65_LIBRARIES_DIR "${CODAPHELA_ROOT}/HCP65/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options("${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}" "${IDE_ROOT}/Libraries")

codaphela_solution("${IDE_ROOT}/HCP65IDE/HCP65IDE.sln" TESTS TestBuildLib REFERENCED)
