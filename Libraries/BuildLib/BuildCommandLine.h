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
#ifndef __BUILD_COMMAND_LINE_H__
#define __BUILD_COMMAND_LINE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BuildWorkspace.h"
#include "SolutionBuild.h"
#include "BuildPlain.h"


#define BUILD_EXIT_SUCCEEDED	0		//Every solution built.
#define BUILD_EXIT_FAILED		1		//A solution didn't build, or its program couldn't be put on its card.
#define BUILD_EXIT_USAGE		2		//The command line was wrong, or the file or configuration couldn't be found.


//hcp65build: builds a solution (.sln), a project (.vcxproj) or a workspace (.hcp65workspace) in one of its build
//configurations (its .hcp65build) without the IDE, as the IDE's Build does: every solution of a workspace in dependency
//order, each compiled and linked whole by CSolutionBuild, its output in Output\<toolchain>\<configuration> beside it.
//Optionally each program and driver the kernal runs is put on the configuration's card as the debugger does.  Given a
//solution's configurations (.hcp65build) it builds the solution they are for: the .sln of the same name beside them,
//else the .vcxproj.
//
//	hcp65build <file> [-config "Release|W65C816"] [-card] [-libc65 <directory>] [-quiet] [-show-plain <file>]
//	hcp65build -program <name> [-dir <dir>] [-D <define>]... <source>... [-lib ...]... [options]
//
//The second is a plain command line (CBuildPlain): what to compile and link given as options, with no solution.  It is
//chosen by a -program or -lib.  -show-plain writes a solution's build as one, instead of building it, and prints it as
//one PowerShell command line.  @file reads arguments from a file, one a line (blank lines and lines starting with # are
//skipped); files can read others.
//
//Options start with - or -- (or / on Windows).  Without -config the workspace's last chosen configuration is built,
//else the first HCP65 one.  Without -libc65 Libc65 is HCP65\Libraries\Libc65 in a directory above the one given
//to Run (the tool's own), or else above the file (the current directory for a plain command line).  Each line is
//written by WriteLine (to stdout unless overridden).
class CBuildCommandLine
{
protected:
	CChars				mszFile;
	CChars				mszConfiguration;
	CChars				mszLibc65;
	CChars				mszShowPlain;
	bool				mbCard;
	bool				mbQuiet;
	bool				mbHelp;
	bool				mbPlain;
	CBuildPlain			mcPlain;
	CBuildWorkspace		mcWorkspace;
	CSolutionBuild		mcBuild;

public:
			void	Init(void);
	virtual	void	Kill(void);

			bool	Parse(int iArgc, const char* const* aszArgv);
			int		Run(const char* szToolDirectory);

			char*	GetFile(void);
			char*	GetConfiguration(void);
			char*	GetLibc65(void);
			bool	IsCard(void);
			bool	IsQuiet(void);
			bool	IsHelp(void);
			bool	IsPlain(void);
			CBuildPlain*	GetPlain(void);

	static	bool	FindLibc65(const char* szDirectory, CChars* pszDest);
	static	bool	FindSolutionFile(const char* szConfigurations, CChars* pszDest);
	static	bool	IsProgressLine(const char* szLine);

protected:
	virtual	void	WriteLine(const char* szLine);
			void	WriteLines(CArrayChars* paszLines);
			void	WriteUsage(void);
			bool	ExpandArguments(const char* szArgument, CArrayChars* paszDest, size uiDepth);
			bool	ResolveLibc65(const char* szToolDirectory, const char* szFile);
			bool	ChooseConfiguration(void);
			bool	BuildSolution(size uiSolution, bool bHeading);
			bool	PutOnCard(size uiSolution, CBuildConfiguration* pcConfiguration);
			int		RunPlain(const char* szToolDirectory);
			int		ShowPlain(CArrayInt* paiOrder, const char* szToolDirectory);
			bool	IsOption(const char* szArgument, const char* szName);
};


#endif // __BUILD_COMMAND_LINE_H__

