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
# The targets are read from HCP65IDE.sln (codaphela_solution): the libraries its GCC-x64 rows build, each one's
# sources from its .vcxproj, so Visual Studio stays the source of truth.  HCP65IDE.vcxproj's configurations say which
# main each program has (its Reference configuration the reference screen's), and Reference's rows build WinRefLib
# and no X11Lib.  hcp65build and hcp65index are the solution's HCP65Build and HCP65Index projects.

cmake_minimum_required(VERSION 3.24)
project(HCP65IDE CXX)


# The HCP65-IDE repository is beside CommonLibraries and HCP65 (Assembler65Lib, and Libc65 for what it builds).
get_filename_component(IDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(CODAPHELA_ROOT "${IDE_ROOT}/.." ABSOLUTE)
set(COMMON_DIR "${CODAPHELA_ROOT}/CommonLibraries/Libraries")

include("${COMMON_DIR}/../CMake/Codaphela.cmake")

codaphela_options(ONLY_64 "${COMMON_DIR}" "${CODAPHELA_ROOT}/HCP65/Libraries" "${IDE_ROOT}/Libraries")

codaphela_solution("${CMAKE_CURRENT_SOURCE_DIR}/HCP65IDE.sln"
	PROGRAMS HCP65IDE HCP65Build=hcp65build HCP65Index=hcp65index
	REFERENCE HCP65IDE=HCP65IDERef)
