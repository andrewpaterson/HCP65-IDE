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
# Source lists are read from the .vcxproj files so Visual Studio stays the
# source of truth.  Building re-runs the configure step automatically when a
# .vcxproj changes.  Test directories can contain .cpp files that are test data,
# so never glob *.cpp to find sources.

cmake_minimum_required(VERSION 3.19)
project(TestBuildLib CXX)


# This test is in the HCP65-IDE repository (with BuildLib and VisualStudioLib), which is beside CommonLibraries.
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(TESTLIB_DIR "${COMMON_DIR}/TestLib")
set(STANDARDLIB_DIR "${COMMON_DIR}/StandardLib")
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(MICROPROCESSORLIB_DIR "${COMMON_DIR}/MicroprocessorLib")
set(CPPCOMPILERLIB_DIR "${COMMON_DIR}/CppCompilerLib")
# Assembler65Lib (which CppCompilerLib's HCP65 compiler uses) is in the HCP65 repository, beside CommonLibraries.
set(HCP65_LIBRARIES_DIR "${CODAPHELA_ROOT}/HCP65/Libraries")
set(ASSEMBLER65LIB_DIR "${HCP65_LIBRARIES_DIR}/Assembler65Lib")
set(VISUALSTUDIOLIB_DIR "${IDE_ROOT}/Libraries/VisualStudioLib")
set(BUILDLIB_DIR "${IDE_ROOT}/Libraries/BuildLib")
set(TESTBUILDLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TestBuildLib")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(TESTLIB_SOURCES "${TESTLIB_DIR}/TestLib.vcxproj")

vcxproj_sources(STANDARDLIB_SOURCES "${STANDARDLIB_DIR}/StandardLib.vcxproj")

vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(MICROPROCESSORLIB_SOURCES "${MICROPROCESSORLIB_DIR}/MicroprocessorLib.vcxproj")

vcxproj_sources(ASSEMBLER65LIB_SOURCES "${ASSEMBLER65LIB_DIR}/Assembler65Lib.vcxproj")

vcxproj_sources(CPPCOMPILERLIB_SOURCES "${CPPCOMPILERLIB_DIR}/CppCompilerLib.vcxproj")

vcxproj_sources(VISUALSTUDIOLIB_SOURCES "${VISUALSTUDIOLIB_DIR}/VisualStudioLib.vcxproj")

vcxproj_sources(BUILDLIB_SOURCES "${BUILDLIB_DIR}/BuildLib.vcxproj")

vcxproj_sources(TESTBUILDLIB_SOURCES "${TESTBUILDLIB_DIR}/TestBuildLib.vcxproj")


codaphela_options("${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}" "${IDE_ROOT}/Libraries")


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

add_library(MicroprocessorLib STATIC ${MICROPROCESSORLIB_SOURCES})
target_link_libraries(MicroprocessorLib PUBLIC StandardLib BaseLib)

add_library(Assembler65Lib STATIC ${ASSEMBLER65LIB_SOURCES})
target_link_libraries(Assembler65Lib PUBLIC MicroprocessorLib StandardLib BaseLib)

# The HCP65 compiler, which builds the W65C816 and 65C64 configurations.
add_library(CppCompilerLib STATIC ${CPPCOMPILERLIB_SOURCES})
target_link_libraries(CppCompilerLib PUBLIC Assembler65Lib MicroprocessorLib ThreadLib StandardLib BaseLib)

# VisualStudioLib only needs BaseLib (its XML parser).
add_library(VisualStudioLib STATIC ${VISUALSTUDIOLIB_SOURCES})
target_link_libraries(VisualStudioLib PUBLIC BaseLib)

# BuildLib builds Visual Studio solutions' configurations: with the HCP65 compiler, MSVC or GCC.
add_library(BuildLib STATIC ${BUILDLIB_SOURCES})
target_link_libraries(BuildLib PUBLIC VisualStudioLib CppCompilerLib ThreadLib BaseLib)

add_executable(TestBuildLib ${TESTBUILDLIB_SOURCES})
target_link_libraries(TestBuildLib PRIVATE TestLib BuildLib VisualStudioLib CppCompilerLib Assembler65Lib MicroprocessorLib ThreadLib StandardLib BaseLib)

# The tests read their input (Input/ ...) relative to the project directory,
# as Visual Studio does.
enable_testing()
add_test(NAME TestBuildLib COMMAND TestBuildLib WORKING_DIRECTORY "${TESTBUILDLIB_DIR}")
