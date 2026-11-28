/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __VCX_SOLUTION_ROWS_H__
#define __VCX_SOLUTION_ROWS_H__
#include "BaseLib/Chars.h"


//A solution's configurations edited in its .sln, in place: the file is its text with only their lines added, taken
//away or changed, the rest byte for byte as it was (ConfigurationsPlan.txt C9-6, question 35).  A solution
//configuration is a line of GlobalSection(SolutionConfigurationPlatforms), "Release|x64 = Release|x64", and its rows
//in GlobalSection(ProjectConfigurationPlatforms), one to three for each project:
//
//  {GUID}.Release|x64.ActiveCfg = Release|x64      the project configuration it builds the project as
//  {GUID}.Release|x64.Build.0 = Release|x64        there if the project is built
//
//A new line goes where Visual Studio sorts it: among the configurations by name, and among its project's rows by
//configuration and then kind, so no line that was there moves.
//
//AddVcxSolutionConfiguration adds szNew ("Fast|GCC-x64") with a row for each row szCopied has, naming the same
//project configuration: it builds what szCopied builds.  False if the file can't be read or written, hasn't szCopied
//or has szNew already.
bool AddVcxSolutionConfiguration(const char* szFileName, const char* szNew, const char* szCopied);

//Takes a configuration's line and its rows away.  False if the file hasn't it.
bool RemoveVcxSolutionConfiguration(const char* szFileName, const char* szName);

//The configuration under another name, its rows as they were.  False if the file hasn't szOld or has szNew.
bool RenameVcxSolutionConfiguration(const char* szFileName, const char* szOld, const char* szNew);

//The project configuration a solution configuration builds a project as: its rows' values.  False if the project
//has no row for it; true with nothing written if they say so already.
bool SetVcxSolutionProjectConfiguration(const char* szFileName, const char* szGuid, const char* szSolutionConfiguration, const char* szProjectConfiguration);


#endif // __VCX_SOLUTION_ROWS_H__

