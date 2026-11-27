# Linux build of the HCP65 IDE (BaseLib + StandardLib + SupportLib + InputLib + WindowLib + X11Lib +
# VisualStudioLib + BuildLib + IndexLib + GitLib + ClaudeLib + TemplateLib + the IDE's window sources).
# HCP65IDE shows its window on the X display; HCP65IDERef draws it on WinRefLib's reference screen, in
# memory, for the checks (Checks/IDEChecks.py) to run without a display.
#
# 64-bit only: there is no 32-bit libX11 installed.  Needs libx11-dev.
#
# From the folder the HCP65-IDE repository and CommonLibraries are in:
#
#   cmake -S HCP65-IDE/HCP65IDE -B build/HCP65IDE -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/HCP65IDE
#   cd HCP65-IDE/HCP65IDE/HCP65IDE && ../../../build/HCP65IDE/HCP65IDE [Project.vcxproj]
#
# Add -DBASELIB_ASAN=ON for AddressSanitizer and LeakSanitizer.
#
# Library source lists are read from the .vcxproj files so Visual Studio stays
# the source of truth.

cmake_minimum_required(VERSION 3.19)
project(HCP65IDE CXX)


# The IDE is in the HCP65-IDE repository (with VisualStudioLib), which is beside CommonLibraries.
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")
set(BASELIB_DIR "${COMMON_DIR}/BaseLib")
set(STANDARDLIB_DIR "${COMMON_DIR}/StandardLib")
set(SUPPORTLIB_DIR "${COMMON_DIR}/SupportLib")
set(INPUTLIB_DIR "${COMMON_DIR}/InputLib")
set(WINDOWLIB_DIR "${COMMON_DIR}/WindowLib")
set(X11LIB_DIR "${COMMON_DIR}/X11Lib")
set(WINREFLIB_DIR "${COMMON_DIR}/WinRefLib")
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(MICROPROCESSORLIB_DIR "${COMMON_DIR}/MicroprocessorLib")
set(EMULATORDEBUGLIB_DIR "${COMMON_DIR}/EmulatorDebugLib")
set(CPPCOMPILERLIB_DIR "${COMMON_DIR}/CppCompilerLib")
# Assembler65Lib (which CppCompilerLib's 65C64 generator uses) is in the HCP65 repository, beside CommonLibraries.
set(HCP65_LIBRARIES_DIR "${CODAPHELA_ROOT}/HCP65/Libraries")
set(ASSEMBLER65LIB_DIR "${HCP65_LIBRARIES_DIR}/Assembler65Lib")
set(VISUALSTUDIOLIB_DIR "${IDE_ROOT}/Libraries/VisualStudioLib")
set(BUILDLIB_DIR "${IDE_ROOT}/Libraries/BuildLib")
set(INDEXLIB_DIR "${IDE_ROOT}/Libraries/IndexLib")
set(GITLIB_DIR "${IDE_ROOT}/Libraries/GitLib")
set(CLAUDELIB_DIR "${IDE_ROOT}/Libraries/ClaudeLib")
set(TEMPLATELIB_DIR "${IDE_ROOT}/Libraries/TemplateLib")
set(IDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/HCP65IDE")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(STANDARDLIB_SOURCES "${STANDARDLIB_DIR}/StandardLib.vcxproj")

vcxproj_sources(SUPPORTLIB_SOURCES "${SUPPORTLIB_DIR}/SupportLib.vcxproj")

vcxproj_sources(INPUTLIB_SOURCES "${INPUTLIB_DIR}/InputLib.vcxproj")

vcxproj_sources(WINDOWLIB_SOURCES "${WINDOWLIB_DIR}/WindowLib.vcxproj")

vcxproj_sources(X11LIB_SOURCES "${X11LIB_DIR}/X11Lib.vcxproj")
vcxproj_sources(WINREFLIB_SOURCES "${WINREFLIB_DIR}/WinRefLib.vcxproj")
vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(MICROPROCESSORLIB_SOURCES "${MICROPROCESSORLIB_DIR}/MicroprocessorLib.vcxproj")
vcxproj_sources(EMULATORDEBUGLIB_SOURCES "${EMULATORDEBUGLIB_DIR}/EmulatorDebugLib.vcxproj")

vcxproj_sources(ASSEMBLER65LIB_SOURCES "${ASSEMBLER65LIB_DIR}/Assembler65Lib.vcxproj")

vcxproj_sources(CPPCOMPILERLIB_SOURCES "${CPPCOMPILERLIB_DIR}/CppCompilerLib.vcxproj")

vcxproj_sources(VISUALSTUDIOLIB_SOURCES "${VISUALSTUDIOLIB_DIR}/VisualStudioLib.vcxproj")

vcxproj_sources(BUILDLIB_SOURCES "${BUILDLIB_DIR}/BuildLib.vcxproj")

vcxproj_sources(INDEXLIB_SOURCES "${INDEXLIB_DIR}/IndexLib.vcxproj")

vcxproj_sources(GITLIB_SOURCES "${GITLIB_DIR}/GitLib.vcxproj")

vcxproj_sources(CLAUDELIB_SOURCES "${CLAUDELIB_DIR}/ClaudeLib.vcxproj")

vcxproj_sources(TEMPLATELIB_SOURCES "${TEMPLATELIB_DIR}/TemplateLib.vcxproj")

# Everything but a main: HCP65IDE_Windows.cpp (WinMain) is Windows only, HCP65IDE_Linux.cpp is the X11 IDE's and
# HCP65IDERef.cpp the reference screen's (which the project keeps out of a GCC-x64 build).
vcxproj_sources(IDE_SOURCES "${IDE_DIR}/HCP65IDE.vcxproj" EXCLUDE "HCP65IDE_Windows.cpp" "HCP65IDE_Linux.cpp")


codaphela_options(ONLY_64 "${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}" "${IDE_ROOT}/Libraries")


# CMutex uses pthreads on Linux.
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

add_library(BaseLib STATIC ${BASELIB_SOURCES})
target_link_libraries(BaseLib PUBLIC baselib_options)
target_link_libraries(BaseLib PUBLIC Threads::Threads)

# Uses AVX intrinsics unconditionally; MSVC allows that without a flag, g++ does not.

add_library(StandardLib STATIC ${STANDARDLIB_SOURCES})
target_link_libraries(StandardLib PUBLIC BaseLib)

add_library(SupportLib STATIC ${SUPPORTLIB_SOURCES})
target_link_libraries(SupportLib PUBLIC StandardLib BaseLib)

add_library(InputLib STATIC ${INPUTLIB_SOURCES})
target_link_libraries(InputLib PUBLIC StandardLib BaseLib)

add_library(WindowLib STATIC ${WINDOWLIB_SOURCES})
target_link_libraries(WindowLib PUBLIC InputLib SupportLib StandardLib BaseLib)

add_library(X11Lib STATIC ${X11LIB_SOURCES})
target_link_libraries(X11Lib PUBLIC WindowLib InputLib SupportLib StandardLib BaseLib X11::X11 X11::Xext)

add_library(ThreadLib STATIC ${THREADLIB_SOURCES})
target_link_libraries(ThreadLib PUBLIC BaseLib)

add_library(MicroprocessorLib STATIC ${MICROPROCESSORLIB_SOURCES})
target_link_libraries(MicroprocessorLib PUBLIC StandardLib BaseLib)

add_library(EmulatorDebugLib STATIC ${EMULATORDEBUGLIB_SOURCES})
target_link_libraries(EmulatorDebugLib PUBLIC ThreadLib MicroprocessorLib StandardLib BaseLib)

add_library(Assembler65Lib STATIC ${ASSEMBLER65LIB_SOURCES})
target_link_libraries(Assembler65Lib PUBLIC MicroprocessorLib StandardLib BaseLib)

# CppCompilerLib preprocesses, parses and highlights the solution's C++.  Its 65C64 generator brings in the rest.
add_library(CppCompilerLib STATIC ${CPPCOMPILERLIB_SOURCES})
target_link_libraries(CppCompilerLib PUBLIC Assembler65Lib MicroprocessorLib ThreadLib StandardLib BaseLib)

# VisualStudioLib reads Visual Studio's projects and solutions.  It only needs BaseLib.
add_library(VisualStudioLib STATIC ${VISUALSTUDIOLIB_SOURCES})
target_link_libraries(VisualStudioLib PUBLIC BaseLib)

# BuildLib builds the solution's configurations: with the HCP65 compiler, MSVC or GCC.
add_library(BuildLib STATIC ${BUILDLIB_SOURCES})
target_link_libraries(BuildLib PUBLIC VisualStudioLib CppCompilerLib ThreadLib BaseLib)

# IndexLib indexes a workspace's C++ as its build configuration compiles it: the IDE's code model is one, and a
# program without a window can index a workspace the same way.
add_library(IndexLib STATIC ${INDEXLIB_SOURCES})
target_link_libraries(IndexLib PUBLIC BuildLib VisualStudioLib CppCompilerLib ThreadLib BaseLib)

# GitLib runs git for the files' statuses and their text in the last commit.
add_library(GitLib STATIC ${GITLIB_SOURCES})
target_link_libraries(GitLib PUBLIC StandardLib ThreadLib BaseLib)

# ClaudeLib runs Claude Code for the Claude tab's requests.
add_library(ClaudeLib STATIC ${CLAUDELIB_SOURCES})
target_link_libraries(ClaudeLib PUBLIC ThreadLib BaseLib)

# TemplateLib applies the file and code templates.
add_library(TemplateLib STATIC ${TEMPLATELIB_SOURCES})
target_link_libraries(TemplateLib PUBLIC BaseLib)

add_library(WinRefLib STATIC ${WINREFLIB_SOURCES})
target_link_libraries(WinRefLib PUBLIC WindowLib InputLib SupportLib StandardLib BaseLib)

# The IDE, whichever window it is shown in.
add_library(HCP65IDELib STATIC ${IDE_SOURCES})
target_include_directories(HCP65IDELib PUBLIC "${IDE_DIR}")
target_link_libraries(HCP65IDELib PUBLIC WindowLib InputLib IndexLib BuildLib GitLib ClaudeLib TemplateLib VisualStudioLib CppCompilerLib Assembler65Lib EmulatorDebugLib MicroprocessorLib ThreadLib SupportLib StandardLib BaseLib)

add_executable(HCP65IDE "${IDE_DIR}/HCP65IDE_Linux.cpp")
target_link_libraries(HCP65IDE PRIVATE HCP65IDELib X11Lib)

add_executable(HCP65IDERef "${IDE_DIR}/HCP65IDERef.cpp")
target_link_libraries(HCP65IDERef PRIVATE HCP65IDELib WinRefLib)

# hcp65build, the command-line build (BuildCommandLine.h): no window, so no X11.  Build only it with
#   cmake --build build/HCP65IDE --target hcp65build
add_executable(hcp65build "${IDE_ROOT}/HCP65Build/HCP65Build.cpp")
target_link_libraries(hcp65build PRIVATE BuildLib VisualStudioLib CppCompilerLib Assembler65Lib MicroprocessorLib ThreadLib StandardLib BaseLib)

# hcp65index, the symbol table for the website's references to code (HCP65/Website/DocsPlan.txt D1): indexes
# solutions with IndexLib, no window.  Build only it with
#   cmake --build build/HCP65IDE --target hcp65index
add_executable(hcp65index "${IDE_ROOT}/HCP65Index/HCP65Index.cpp")
target_link_libraries(hcp65index PRIVATE IndexLib BuildLib VisualStudioLib CppCompilerLib Assembler65Lib MicroprocessorLib ThreadLib StandardLib BaseLib)
