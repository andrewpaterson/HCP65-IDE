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
#ifndef __SOLUTION_BUILD_H__
#define __SOLUTION_BUILD_H__
#include "HCP65Build.h"
#include "MSVCBuild.h"
#include "GCCBuild.h"


//Builds a solution in a build configuration, with the configuration's toolchain: the HCP65 compiler, MSVC or GCC.
//Given the solution's startup project, an HCP65 build builds only that application of the solution's (see CBuild).  The
//last build is kept (its state, progress and listings) until the next starts.  SetRebuild says whether the builds
//started after it build everything (Rebuild All) or leave what is up to date (CBuild::SetRebuild).
class CSolutionBuild
{
protected:
	CHCP65Build		mcHCP65;
	CMSVCBuild		mcMSVC;
	CGCCBuild		mcGCC;
	CBuild*			mpcBuild;	//The last build started, NULL before the first.
	bool			mbRebuild;	//The next build started builds everything (SetRebuild).

public:
	void			Init(void);
	void			Kill(void);

	bool			Start(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot, const char* szLibc65Directory, CArrayChars* paszLines, const char* szStartupProject = NULL, const char* szLayoutsFile = NULL);

	static void	AddOldFileLine(CVcxSolution* pcSolution, CArrayChars* paszLines);
	void			SetRebuild(bool bRebuild);
	bool			Poll(CArrayChars* paszLines);
	void			Cancel(void);

	EBuildState		GetState(void) const;
	bool			IsRunning(void);
	char*			GetProgress(void) const;
	float			GetFraction(void) const;
	CBuild*			GetBuild(void) const;

protected:
	void			End(void);
};


#endif // __SOLUTION_BUILD_H__

