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
#ifndef __BUILD_UP_TO_DATE_H__
#define __BUILD_UP_TO_DATE_H__
#include "BaseLib/ArrayChars.h"
#include "CommandBuild.h"


//Whether a command of a build needs running (ConfigurationsPlan.txt question 31): Build runs it only if what it makes
//is older than something it is made from, or was made by a different command line; Rebuild All runs every one.
//
//What a command makes (an object, a library, a program) has its command line kept beside it in <file>.cmd.  It is up
//to date if it is there, that file says the command line it would be made with now, each file it is made from is
//there, and none of those is newer than it.  A compile's are its source and the headers the compiler says it
//included (g++ -MMD writes them to <object>.d: ReadBuildDependencies).
//
//PrepareBuildOutput, for a command that is to run, removes what it made before and writes its command line: a
//command that then fails leaves nothing that could look up to date, and whatever is made from it is seen to be out
//of date by its not being there.
bool	IsBuildOutputUpToDate(SBuildCommand* psCommand, const char* szOutput, CArrayChars* paszInputs);
void	PrepareBuildOutput(SBuildCommand* psCommand, const char* szOutput);
bool	ReadBuildDependencies(const char* szDependencyFile, CArrayChars* paszDest);
int64	GetBuildFileTime(const char* szFile);


#endif // __BUILD_UP_TO_DATE_H__

