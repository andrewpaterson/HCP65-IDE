/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __BUILD_PLAIN_H__
#define __BUILD_PLAIN_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BuildConfiguration.h"
#include "HCP65BuildDescription.h"


#define BUILD_PLAIN_CONFIGURATION	"Plain"		//A plain build is configuration Plain|<target>: Output\<target>\Plain.
#define BUILD_PLAIN_UNITS_EXTENSION	".units"	//-show-plain's list of a project's sources.


//Whether szArgument is the option szName: -name, --name or (on Windows) /name, any case.  A path on Linux starts with /
//so isn't an option.
bool	BuildIsOption(const char* szArgument, const char* szName);

//Whether szArgument looks like an option at all.
bool	BuildLooksLikeOption(const char* szArgument);

//A number as hcp65build and .hcp65build files take them: decimal, 0x hexadecimal or $ hexadecimal.  False if it isn't one.
bool	BuildParseNumber(const char* szNumber, int64* piValue);
bool	BuildParseUint32(const char* szNumber, uint32* puiValue);


//hcp65build's plain command line: what to compile and link given as options, with no solution or .hcp65build.
//
//	-target W65C816|65C64				The toolchain (W65C816 by default).
//	-I <dir>, -I+ <dir>					An include directory, without or with its sub directories.  Before the first
//										-program or -lib it is for every unit; after one, for that project's.
//	-program <name>, -lib <name>		A project, whose sources follow.  A program's units are always linked; a
//										library's only when something uses them.
//	-dir <dir>							The project's directory (the current one by default): its sources are relative
//										to it, and one outside it (..\Other\File.cpp) is a library of its own, Name.Other,
//										as a Visual Studio project's is.
//	-D <name>[=<value>]					A define for the project's units.
//	-nofloats, -no-mathtables			As a configuration's NoFloats="true" and MathTables="false".
//	-exe <file> -entry <symbol> [-limit <n>] [-symbol <name>=<value>]...
//										Link an executable, as an <Executable>.
//	-image <file> -size <n> [-fill <n>] [-limit <n>] [-load <n>] -region <start>,<end>,<offset> [-region ...]
//										Link an image, as an <Image>.
//	-o <dir>, -listings <dir>			Where the program and listings go: Output\<target>\Plain and
//										Listing\<target>\Plain by default.
//
//Without -exe or -image the units are only compiled.  A plain build adds exactly what a solution build with the same
//description adds (CHCP65Build), which -show-plain writes for a solution (Write).
class CBuildPlain
{
protected:
	CHCP65BuildDescription	mcDescription;
	EBuildToolchain			meToolchain;
	bool					mbNoFloats;
	bool					mbNoMathTables;
	CChars					mszOutput;
	CChars					mszListings;
	CChars					mszExecutable;
	CChars					mszEntry;
	CArrayChars				maszSymbols;		//"Name=Value", checked.
	CChars					mszImage;
	uint32					muiSize;
	uint32					muiFill;
	uint32					muiLoad;
	uint32					muiLimit;
	bool					mbSize;
	bool					mbFill;
	bool					mbLoad;
	bool					mbLimit;
	CArrayChars				maszRegions;		//"Start,End,Offset", checked.

public:
	void		Init(void);
	void		Kill(void);

	int			ParseOption(CArrayChars* paszArguments, size* puiIndex, CChars* pszError);
	bool		AddSource(const char* szSource, CChars* pszError);
	bool		Finish(CChars* pszError);

	bool		MakeConfiguration(CBuildConfiguration* pcConfiguration, CChars* pszError);
	void		MakeDescription(CHCP65BuildDescription* pcDest);
	char*		GetOutput(void);
	char*		GetListings(void);
	EBuildToolchain	GetToolchain(void);
	CHCP65BuildDescription*	GetDescription(void);

	static bool	Write(const char* szFile, CHCP65BuildDescription* pcDescription, CBuildConfiguration* pcConfiguration, const char* szLibc65, const char* szComment, CArrayChars* paszWritten);

protected:
	bool		NeedValue(CArrayChars* paszArguments, size* puiIndex, const char* szOption, CChars* pszError);
	static bool	ParseRegion(const char* szRegion, uint32* puiStart, uint32* puiEnd, uint32* puiOffset);
	static void	MakeFullPath(const char* szPath, CChars* pszDest);
	static void	MakeWrittenPath(const char* szPath, CChars* pszDest);
};


#endif // __BUILD_PLAIN_H__

