/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __VCX_SOLUTION_H__
#define __VCX_SOLUTION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "VcxSolutionProject.h"


enum EVcxSolutionState
{
	VSS_Top,
	VSS_Project,
	VSS_ProjectSection,
	VSS_Global,
	VSS_GlobalSection,
};


//A Visual Studio solution, read from its .sln: its projects (each .vcxproj read too), solution folders and the
//solution configurations with the project configuration each builds.
class CVcxSolution
{
protected:
	CChars						mszFileName;		//The .sln's full path.
	CChars						mszDirectory;		//The directory it is in; the projects' paths are from here.
	CChars						mszName;			//Its file's name without .sln, as Solution Explorer shows it.
	CChars						mszFormatVersion;
	CChars						mszVisualStudioVersion;
	CChars						mszMinimumVisualStudioVersion;
	CChars						mszSolutionGuid;
	bool						mbHideSolutionNode;
	CArrayChars					maszConfigurations;	//Debug|Win32...
	CArrayVcxSolutionProject	mapcProjects;		//In the order the .sln lists them.

	EVcxSolutionState			meState;
	CChars						mszSection;
	CVcxSolutionProject*		mpcCurrent;
	size						muiLine;

public:
	void					Init(void);
	void					Kill(void);

	bool					Read(const char* szFileName, bool bLoadProjects = true);
	bool					ReadProject(const char* szProjectFile);

	char*					GetFileName(void);
	char*					GetDirectory(void);
	char*					GetName(void);
	char*					GetFormatVersion(void);
	char*					GetVisualStudioVersion(void);
	char*					GetMinimumVisualStudioVersion(void);
	char*					GetSolutionGuid(void);
	bool					IsHideSolutionNode(void);

	size					NumConfigurations(void);
	char*					GetConfiguration(size uiIndex);

	size					NumProjects(void);
	CVcxSolutionProject*	GetProject(size uiIndex);
	CVcxSolutionProject*	FindProject(const char* szGuid);
	CVcxSolutionProject*	FindProjectByName(const char* szName);
	void					GetChildProjects(const char* szParentGuid, CArrayInt* paiProjects);
	size					NumLoadedProjects(void);

protected:
	bool					ReadText(char* szText);
	bool					ReadLine(CChars* pszLine);
	bool					ReadProjectLine(CChars* pszLine);
	bool					ReadSectionLine(const char* szKey, const char* szValue);
	bool					ReadProjectConfiguration(const char* szKey, const char* szValue);
	bool					Error(const char* szMessage);
};


bool SplitVcxSolutionLine(CChars* pszLine, CChars* pszKey, CChars* pszValue);
bool GetVcxSolutionSectionName(CChars* pszLine, const char* szStart, CChars* pszName);


#endif // __VCX_SOLUTION_H__

