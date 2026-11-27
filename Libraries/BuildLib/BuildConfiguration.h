/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __BUILD_CONFIGURATION_H__
#define __BUILD_CONFIGURATION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"
#include "Assembler65Lib/Asm65Image.h"
#include "Assembler65Lib/Asm65Executable.h"
#include "Assembler65Lib/Asm65Module.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayUint32.h"
#include "CppCompilerLib/CPPCompileOptions.h"
#include "CppCompilerLib/CPPTarget.h"


class CVcxProject;
class CVcxConfiguration;


//The names of the configurations file's tags and attributes; a project's HCP65Compile and HCP65Link settings have the
//same (BuildConfigurationProject.cpp).
#define BUILD_CONFIGURATION_TAG			"Configuration"
#define BUILD_CONFIGURATION_NAME		"Name"
#define BUILD_CONFIGURATION_TOOLCHAIN	"Toolchain"
#define BUILD_CONFIGURATION_SOLUTION	"Solution"
#define BUILD_CONFIGURATION_IMAGE		"Image"
#define BUILD_CONFIGURATION_EXECUTABLE	"Executable"
#define BUILD_CONFIGURATION_MODULE		"Module"
#define BUILD_CONFIGURATION_LOAD		"Load"
#define BUILD_CONFIGURATION_LOAD_FILE	"File"
#define BUILD_CONFIGURATION_LOAD_ADDRESS	"Address"
#define BUILD_CONFIGURATION_LINES		"Lines"
#define BUILD_CONFIGURATION_LINES_FILE	"File"
#define BUILD_CONFIGURATION_LINES_SPACE	"Space"
#define BUILD_CONFIGURATION_LINES_CARD_PATH	"CardPath"
#define BUILD_CONFIGURATION_LINES_DEFAULT_SPACE	"Kernel"
#define BUILD_CONFIGURATION_CARD		"Card"
#define BUILD_CONFIGURATION_CARD_PATH	"CardPath"
#define BUILD_CONFIGURATION_BREAKPOINT_SPACE	"BreakpointSpace"
#define BUILD_CONFIGURATION_NO_FLOATS	"NoFloats"
#define BUILD_CONFIGURATION_MATH_TABLES	"MathTables"
#define BUILD_CONFIGURATION_PREFER_SIZE	"PreferSize"
#define BUILD_CONFIGURATION_WARN_MISSING_OVERRIDE	"WarnMissingOverride"
#define BUILD_CONFIGURATION_EXCEPTIONS	"Exceptions"
#define BUILD_CONFIGURATION_KEEP_UNUSED_OBJECTS	"KeepUnusedObjects"
#define BUILD_CONFIGURATION_WARN_STRING_TO_POINTER	"WarnStringToPointer"
#define BUILD_CONFIGURATION_MULTI_WORD_CASTS	"MultiWordCasts"
#define BUILD_CONFIGURATION_C_CONVERSIONS	"CConversions"
#define BUILD_CONFIGURATION_STUB_MISSING	"StubMissing"
#define BUILD_CONFIGURATION_DEBUG	"Debug"
#define BUILD_CONFIGURATION_UNOPTIMISED	"Unoptimised"


//What a configuration is built with: the HCP65 compiler for one of its processors, or a compiler for this computer.
enum EBuildToolchain
{
	BT_W65C816,
	BT_65C64,
	BT_MSVC,
	BT_GCC,

	BT_NumToolchains
};


//What an HCP65 configuration links what it compiles into, if anything.
enum EBuildProgram
{
	BP_None,
	BP_Image,
	BP_Executable,
	BP_Module,
};


//A way the solution is built: a name to choose it by, its toolchain, and the solution configuration ("Debug|Win32")
//whose projects' files, include directories and defines it builds with.  MSVC builds that configuration as Visual
//Studio does.  The name is chosen as its configuration and platform, either side of a | ("Debug|HCP65C816").
//
//What it is run in isn't here: that is a run configuration's (CRunConfiguration).  An emulator that debugs what an
//HCP65 configuration builds must be the configuration's processor (GetProcessorName).
//
//An HCP65 configuration with an image (<Image>, CAsm65Image), an executable (<Executable>, CAsm65Executable, a
//program a kernal loads) or a module (<Module>, CAsm65Module, a relocatable driver a kernal loads) links what it
//builds into that file, in the build's Output folder; one without only compiles.  NoFloats="true" makes floating
//point an error (the kernal has none: CCPP65Builder::SetNoFloats).  MathTables="false" builds it to run without the
//maths tables in memory (CCPP65Builder::SetMathTables: the kernal, its drivers and the operating system, which loads
//them).  PreferSize="true" has the compiler prefer smaller code to faster where it has the choice
//(CCPPCompileOptions::mePrefer: a ROM's kernal).  WarnMissingOverride="false" stops the warning about a function
//that overrides a base class's virtual function and isn't declared override (CCPPCompileOptions::mbWarnMissingOverride,
//on unless so).  Exceptions="true" compiles the program with C++ exceptions (CCPPCompileOptions::mbExceptions: throw,
//try and catch are errors without it).  KeepUnusedObjects="true" links a global whose making or destroying runs code (a constructor, a
//destructor, an initialiser that isn't a constant) though nothing refers to it ([basic.stc.static] 2;
//CCPPCompileOptions::mbKeepUnusedObjects); without it such a global goes with the rest of what is unused.
//WarnStringToPointer="true" warns about a string literal, which is const, that becomes a char*
//(CCPPCompileOptions::mbWarnStringToPointer).  MultiWordCasts="true" accepts unsigned int(3), a cast written as a
//type of more than one word and brackets, which is an error without it (CCPPCompileOptions::mbMultiWordCasts).
//CConversions="true" says the sources are C: a void* becomes any object's pointer and a number an enum's value with
//no cast (CCPPCompileOptions::mbCConversions).  A function the program calls and no unit defines is an error;
//StubMissing="true" links a stub for it that stops the program, and the build succeeds.  Debug="true" builds it for a debugger: each variable is kept where a
//debugger reads it, nothing is inlined and every frame keeps its shape, the rest still optimised
//(CCPPCompileOptions::mbDebuggable).  Unoptimised="Init, Shell" names projects compiled for a debugger without the
//optimiser at all, whose every line steps as written (CCPP65Builder::SetUnoptimised).  Its BreakpointSpace is the emulator's space ("Kernel", "User") that debugging puts breakpoints in; without
//one they stop in every space.
//
//Debugging loads each <Load File Address/> into the waiting emulator first (a kernal for a program to run on), and
//its own image.  Each <Lines File Space CardPath/> is a program whose line table debugging reads without loading it, its
//source breakpoints in the breakpoint space Space (default "Kernel"): the kernal the boot loader loads from the card,
//OS.SYS, which runs where it is linked.  CardPath, if given, is the file on the card that should be it ("\OS\OS.SYS"):
//debugging says if it isn't.  A program is written onto the emulator's SD card image, Card, as CardPath ("\boot.exe") on its FAT
//volume, for the kernal to run.  Paths are relative to the configurations' file.
class CBuildConfiguration
{
protected:
	CChars			mszName;
	EBuildToolchain	meToolchain;
	CChars			mszSolutionConfiguration;
	CChars			mszBreakpointSpace;
	bool			mbNoFloats;
	bool			mbMathTables;
	bool			mbPreferSize;
	bool			mbWarnMissingOverride;
	bool			mbExceptions;
	bool			mbKeepUnusedObjects;
	bool			mbWarnStringToPointer;
	bool			mbMultiWordCasts;
	bool			mbCConversions;
	bool			mbStubMissing;
	bool			mbDebug;
	CChars			mszUnoptimised;		//The projects compiled without the optimiser, separated by commas.
	EBuildProgram	meProgram;
	CAsm65Image		mcImage;
	CAsm65Executable	mcExecutable;
	CAsm65Module	mcModule;
	CArrayChars		maszLoadFiles;
	CArrayUint32	mauiLoadAddresses;
	CArrayChars		maszLinesFiles;
	CArrayChars		maszLinesSpaces;
	CArrayChars		maszLinesCardPaths;
	CChars			mszCard;
	CChars			mszCardPath;

public:
	void			Init(const char* szName, EBuildToolchain eToolchain, const char* szSolutionConfiguration);
	void			Init(void);
	void			Kill(void);

	void			SaveXML(CXMLWriter* pcWriter);
	bool			LoadXML(CMarkupTag* pcTag);

	char*			GetName(void);
	EBuildToolchain	GetToolchain(void) const;
	char*			GetSolutionConfiguration(void);
	bool			IsHCP65(void) const;
	const char*		GetProcessorName(void) const;
	ECPPTarget		GetTarget(void) const;
	bool			IsGCC(void) const;
	CVcxConfiguration*	FindProjectConfiguration(CVcxProject* pcProject, const char* szName) const;
	bool			ReadProject(CVcxConfiguration* pcConfiguration, CChars* pszError);
	static void		ReadOwnOptions(CVcxConfiguration* pcConfiguration, CArrayChars* paszOptions);
	static bool		SetOwnOption(const char* szOption, CCPPCompileOptions* pcOptions, bool* pbUnoptimised);

	void			SetBreakpointSpace(const char* szSpace);
	char*			GetBreakpointSpace(void);
	void			ClearDebugging(void);
	void			SetNoFloats(bool bNoFloats);
	bool			IsNoFloats(void) const;
	void			SetMathTables(bool bMathTables);
	bool			IsMathTables(void) const;
	void			SetPreferSize(bool bPreferSize);
	bool			IsPreferSize(void) const;
	void			SetWarnMissingOverride(bool bWarnMissingOverride);
	bool			IsWarnMissingOverride(void) const;
	void			SetExceptions(bool bExceptions);
	bool			IsExceptions(void) const;
	void			SetKeepUnusedObjects(bool bKeepUnusedObjects);
	void			SetWarnStringToPointer(bool bWarnStringToPointer);
	void			SetMultiWordCasts(bool bMultiWordCasts);
	void			SetCConversions(bool bCConversions);
	void			SetStubMissing(bool bStubMissing);
	bool			IsKeepUnusedObjects(void) const;
	bool			IsWarnStringToPointer(void) const;
	bool			IsMultiWordCasts(void) const;
	bool			IsCConversions(void) const;
	bool			IsStubMissing(void) const;
	void			SetDebug(bool bDebug);
	bool			IsDebug(void) const;
	void			SetUnoptimised(const char* szProjects);
	char*			GetUnoptimised(void);
	void			GetUnoptimisedProjects(CArrayChars* paszDest);
	void			SetCompileOptions(CCPPCompileOptions* pcDest) const;

	bool			HasImage(void) const;
	CAsm65Image*	GetImage(void);
	CAsm65Image*	SetImage(const char* szFile, uint32 uiSize, uint8 uiFill, uint32 uiLimit, uint32 uiLoad);
	bool			HasExecutable(void) const;
	CAsm65Executable*	GetExecutable(void);
	CAsm65Executable*	SetExecutable(const char* szFile, const char* szEntry, uint32 uiLimit);
	bool			HasModule(void) const;
	CAsm65Module*	GetModule(void);
	CAsm65Module*	SetModule(const char* szFile, const char* szEntry, uint32 uiLimit);
	EBuildProgram	GetProgram(void) const;
	bool			IsLinked(void) const;
	const char*		GetProgramFile(void);
	uint32			GetProgramLimit(void);

	void			AddLoad(const char* szFile, uint32 uiAddress);
	size			NumLoads(void) const;
	char*			GetLoadFile(size uiIndex);
	uint32			GetLoadAddress(size uiIndex);
	void			AddLines(const char* szFile, const char* szSpace, const char* szCardPath);
	size			NumLines(void) const;
	char*			GetLinesFile(size uiIndex);
	char*			GetLinesSpace(size uiIndex);
	char*			GetLinesCardPath(size uiIndex);
	void			SetCard(const char* szCard, const char* szPath);
	bool			HasCard(void) const;
	char*			GetCard(void);
	char*			GetCardPath(void);
	bool			IsOnCard(void) const;
	void			GetConfigurationName(CChars* pszDest);
	void			GetPlatformName(CChars* pszDest);
	bool			Is(const char* szConfiguration, const char* szPlatform);

	static const char*	GetToolchainName(EBuildToolchain eToolchain);
	static bool			FindToolchain(const char* szName, EBuildToolchain* peToolchain);
	static const char*	GetToolchainPlatform(EBuildToolchain eToolchain);
	static bool			FindToolchainOfPlatform(const char* szPlatform, EBuildToolchain* peToolchain);
	static void			CurrentName(const char* szName, CChars* pszDest);

protected:
	void			ClearProgram(void);
	bool			ReadProjectLink(CVcxConfiguration* pcConfiguration, CChars* pszError);
};


#endif // __BUILD_CONFIGURATION_H__

