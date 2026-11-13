# Linux build of the HCP65 IDE (BaseLib + StandardLib + SupportLib + InputLib +
# WindowLib + X11Lib + VisualStudioLib + BuildLib + GitLib + ClaudeLib + the IDE's window sources).
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

cmake_minimum_required(VERSION 3.16)
project(HCP65IDE CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

option(BASELIB_ASAN "Build with AddressSanitizer and LeakSanitizer" OFF)

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
set(THREADLIB_DIR "${COMMON_DIR}/ThreadLib")
set(MICROPROCESSORLIB_DIR "${COMMON_DIR}/MicroprocessorLib")
set(EMULATORDEBUGLIB_DIR "${COMMON_DIR}/EmulatorDebugLib")
set(CPPCOMPILERLIB_DIR "${COMMON_DIR}/CppCompilerLib")
# Assembler65Lib (which CppCompilerLib's 65C64 generator uses) is in the HCP65 repository, beside CommonLibraries.
set(HCP65_LIBRARIES_DIR "${CODAPHELA_ROOT}/HCP65/Libraries")
set(ASSEMBLER65LIB_DIR "${HCP65_LIBRARIES_DIR}/Assembler65Lib")
set(VISUALSTUDIOLIB_DIR "${IDE_ROOT}/Libraries/VisualStudioLib")
set(BUILDLIB_DIR "${IDE_ROOT}/Libraries/BuildLib")
set(GITLIB_DIR "${IDE_ROOT}/Libraries/GitLib")
set(CLAUDELIB_DIR "${IDE_ROOT}/Libraries/ClaudeLib")
set(IDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/HCP65IDE")


# Reads <ClCompile Include="..."/> entries from a .vcxproj and resolves them
# against the files on disk case-insensitively (Windows paths are not
# case-sensitive).  Files named in EXCLUDE (paths relative to the project,
# any case) are left out.
function(vcxproj_sources OUT_VAR VCXPROJ)
	cmake_parse_arguments(ARG "" "" "EXCLUDE" ${ARGN})
	get_filename_component(PROJ_DIR "${VCXPROJ}" DIRECTORY)

	# Re-run the configure step when Visual Studio adds or removes files.
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${VCXPROJ}")

	file(GLOB_RECURSE DISK_FILES RELATIVE "${PROJ_DIR}" "${PROJ_DIR}/*.cpp" "${PROJ_DIR}/*.c")
	foreach(DISK_FILE ${DISK_FILES})
		string(TOLOWER "${DISK_FILE}" KEY)
		set("DISK_${KEY}" "${DISK_FILE}")
	endforeach()

	set(EXCLUDED "")
	foreach(EXCLUDE_FILE ${ARG_EXCLUDE})
		string(TOLOWER "${EXCLUDE_FILE}" KEY)
		list(APPEND EXCLUDED "${KEY}")
	endforeach()

	file(STRINGS "${VCXPROJ}" LINES REGEX "<ClCompile Include=\"[^\"]+\"")
	set(SOURCES "")
	foreach(LINE ${LINES})
		string(REGEX REPLACE ".*<ClCompile Include=\"([^\"]+)\".*" "\\1" SOURCE "${LINE}")
		string(REPLACE "\\" "/" SOURCE "${SOURCE}")
		string(TOLOWER "${SOURCE}" KEY)
		if(KEY IN_LIST EXCLUDED)
			continue()
		endif()
		if(NOT DEFINED "DISK_${KEY}")
			message(FATAL_ERROR "${VCXPROJ} lists ${SOURCE} but it was not found on disk")
		endif()
		list(APPEND SOURCES "${PROJ_DIR}/${DISK_${KEY}}")
	endforeach()

	set(${OUT_VAR} ${SOURCES} PARENT_SCOPE)
endfunction()


vcxproj_sources(BASELIB_SOURCES "${BASELIB_DIR}/BaseLib.vcxproj")

vcxproj_sources(STANDARDLIB_SOURCES "${STANDARDLIB_DIR}/StandardLib.vcxproj")

vcxproj_sources(SUPPORTLIB_SOURCES "${SUPPORTLIB_DIR}/SupportLib.vcxproj")

vcxproj_sources(INPUTLIB_SOURCES "${INPUTLIB_DIR}/InputLib.vcxproj")

vcxproj_sources(WINDOWLIB_SOURCES "${WINDOWLIB_DIR}/WindowLib.vcxproj")

vcxproj_sources(X11LIB_SOURCES "${X11LIB_DIR}/X11Lib.vcxproj")
vcxproj_sources(THREADLIB_SOURCES "${THREADLIB_DIR}/ThreadLib.vcxproj")

vcxproj_sources(MICROPROCESSORLIB_SOURCES "${MICROPROCESSORLIB_DIR}/MicroprocessorLib.vcxproj")
vcxproj_sources(EMULATORDEBUGLIB_SOURCES "${EMULATORDEBUGLIB_DIR}/EmulatorDebugLib.vcxproj")

vcxproj_sources(ASSEMBLER65LIB_SOURCES "${ASSEMBLER65LIB_DIR}/Assembler65Lib.vcxproj")

vcxproj_sources(CPPCOMPILERLIB_SOURCES "${CPPCOMPILERLIB_DIR}/CppCompilerLib.vcxproj")

vcxproj_sources(VISUALSTUDIOLIB_SOURCES "${VISUALSTUDIOLIB_DIR}/VisualStudioLib.vcxproj")

vcxproj_sources(BUILDLIB_SOURCES "${BUILDLIB_DIR}/BuildLib.vcxproj")

vcxproj_sources(GITLIB_SOURCES "${GITLIB_DIR}/GitLib.vcxproj")

vcxproj_sources(CLAUDELIB_SOURCES "${CLAUDELIB_DIR}/ClaudeLib.vcxproj")

# HCP65IDE.cpp (WinMain) is Windows only.
set(IDE_SOURCES
	"${IDE_DIR}/HCP65IDELinux.cpp"
	"${IDE_DIR}/IDEMain.cpp"
	"${IDE_DIR}/IDECodeModel.cpp"
	"${IDE_DIR}/IDECodeModelEdits.cpp"
	"${IDE_DIR}/IDEContent.cpp"
	"${IDE_DIR}/IDEDocuments.cpp"
	"${IDE_DIR}/IDESolutions.cpp"
	"${IDE_DIR}/IDEBuildConfigurationsDialog.cpp"
	"${IDE_DIR}/IDEBuilding.cpp"
	"${IDE_DIR}/IDEConfigurations.cpp"
	"${IDE_DIR}/IDERunConfigurationsDialog.cpp"
	"${IDE_DIR}/IDESettings.cpp"
	"${IDE_DIR}/IDESettingsDialog.cpp"
	"${IDE_DIR}/IDEKeymapPage.cpp"
	"${IDE_DIR}/IDERunning.cpp"
	"${IDE_DIR}/IDEDebugging.cpp"
	"${IDE_DIR}/IDEGitStatus.cpp"
	"${IDE_DIR}/IDEDiffs.cpp"
	"${IDE_DIR}/IDEDiffTab.cpp"
	"${IDE_DIR}/IDEAskClaude.cpp"
	"${IDE_DIR}/IDEEdit.cpp"
	"${IDE_DIR}/IDENavigator.cpp"
	"${IDE_DIR}/IDERename.cpp"
	"${IDE_DIR}/IDEUsages.cpp"
	"${IDE_DIR}/IDEScope.cpp"
	"${IDE_DIR}/IDEResultsPane.cpp"
	"${IDE_DIR}/IDEFindInFiles.cpp"
	"${IDE_DIR}/IDEIndexing.cpp"
	"${IDE_DIR}/IDENavigationHistory.cpp"
	"${IDE_DIR}/IDECaretUsages.cpp"
	"${IDE_DIR}/IDEPasteHistory.cpp"
	"${IDE_DIR}/IDEOccurrences.cpp"
	"${IDE_DIR}/IDECloseGuard.cpp"
	"${IDE_DIR}/IDEScript.cpp"
	"${IDE_DIR}/IDEGit.cpp"
	"${IDE_DIR}/IDEDebugger.cpp"
	"${IDE_DIR}/IDEDebugView.cpp"
	"${IDE_DIR}/IDEEditorTab.cpp"
	"${IDE_DIR}/IDEListingSource.cpp"
	"${IDE_DIR}/IDEBuildPane.cpp"
	"${IDE_DIR}/IDEClaudePane.cpp"
	"${IDE_DIR}/IDEIcons.cpp"
	"${IDE_DIR}/IDEWindowTick.cpp"
	"${IDE_DIR}/IDEApp.cpp"
	"${IDE_DIR}/SolutionExplorer.cpp"
)


# Matches the Visual Studio configurations, except BREAK_ON_ERROR and
# BREAK_ON_WARNING: on Linux BREAK() raises SIGTRAP, which kills the test run
# when no debugger is attached.
add_library(baselib_options INTERFACE)
target_include_directories(baselib_options INTERFACE "${COMMON_DIR}" "${HCP65_LIBRARIES_DIR}" "${IDE_ROOT}/Libraries")
target_compile_definitions(baselib_options INTERFACE
	LINUX_GNU
	$<$<CONFIG:Debug>:_DEBUG>
	$<$<CONFIG:Debug>:DEBUG>
	$<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)
# -fno-strict-aliasing: MSVC never optimises on the strict aliasing rule and the
# code relies on that (e.g. (CUnknown**)&pcTyped), so optimised builds break without it.
# -Wno-ignored-attributes: g++ drops ALIGN_64 when int64/uint64/float64 is a
# template argument.  Templates that store such a type by value use MSVC_ALIGN
# instead (see PrimitiveTypes.h), so the warning only reports the drop.
# -Wno-write-strings: string literals are passed as char* throughout; MSVC allows it.
# -Wno-multichar: chunk and file IDs are multi-character constants like 'ABCD'.
# -Wno-class-memaccess: the containers memcpy objects with constructors on purpose.
# -Wno-overloaded-virtual: subclasses overload virtuals with other argument types
# (e.g. Save(CObjectWriter*) hides CUnknown::Save(CFileWriter*)) on purpose.
# -Wno-comment: ASCII diagrams in comments end lines with a backslash.
# -Wno-unused-parameter: virtual overrides and callbacks often ignore parameters.
target_compile_options(baselib_options INTERFACE -m64 -fno-strict-aliasing -Wall -Wextra -Wno-ignored-attributes -Wno-write-strings -Wno-multichar -Wno-class-memaccess -Wno-overloaded-virtual -Wno-comment -Wno-unused-parameter)
target_link_options(baselib_options INTERFACE -m64)
if(BASELIB_ASAN)
	target_compile_options(baselib_options INTERFACE -fsanitize=address -fno-omit-frame-pointer)
	target_link_options(baselib_options INTERFACE -fsanitize=address)
endif()


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

# GitLib runs git for the files' statuses and their text in the last commit.
add_library(GitLib STATIC ${GITLIB_SOURCES})
target_link_libraries(GitLib PUBLIC StandardLib ThreadLib BaseLib)

# ClaudeLib runs Claude Code for the Claude tab's requests.
add_library(ClaudeLib STATIC ${CLAUDELIB_SOURCES})
target_link_libraries(ClaudeLib PUBLIC ThreadLib BaseLib)

add_executable(HCP65IDE ${IDE_SOURCES})
target_include_directories(HCP65IDE PRIVATE "${IDE_DIR}")
target_link_libraries(HCP65IDE PRIVATE X11Lib WindowLib InputLib BuildLib GitLib ClaudeLib VisualStudioLib CppCompilerLib Assembler65Lib EmulatorDebugLib MicroprocessorLib ThreadLib SupportLib StandardLib BaseLib)

# hcp65build, the command-line build (BuildCommandLine.h): no window, so no X11.  Build only it with
#   cmake --build build/HCP65IDE --target hcp65build
add_executable(hcp65build "${IDE_ROOT}/HCP65Build/HCP65Build.cpp")
target_link_libraries(hcp65build PRIVATE BuildLib VisualStudioLib CppCompilerLib Assembler65Lib MicroprocessorLib ThreadLib StandardLib BaseLib)
