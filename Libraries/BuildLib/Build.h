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
#ifndef __BUILD_H__
#define __BUILD_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "VisualStudioLib/VcxSolution.h"
#include "VisualStudioLib/VcxProject.h"
#include "BuildConfiguration.h"


enum EBuildState
{
	BS_NotStarted,
	BS_Running,
	BS_Succeeded,
	BS_Failed,
	BS_Cancelled,
};


//A project of the solution that a configuration builds, in the configuration the solution builds it in.
struct SBuildProject
{
	CVcxProject*		pcProject;
	CVcxConfiguration*	pcConfiguration;
	bool				bLibrary;	//A static library; otherwise an application.
};


typedef CArrayTemplate<SBuildProject>	CArrayBuildProjects;


//One build of a solution in a build configuration, run a step at a time so whatever started it (a window's tick) can
//carry on: Start, then Poll until it says the build has finished, adding the lines the build wrote.  What it makes
//goes in Output\<toolchain>\<configuration> beside the solution (or in another root), and listings in Listing\ beside
//that.
//
//A solution can have several applications (projects that aren't static libraries), one of them its startup project
//(SetStartupProject, see CBuildWorkspaceSolution).  A build that links the whole solution into one program (an HCP65
//build) builds only that application, with the libraries: the others have programs of their own to be.  A build that
//links each application (GCC, MSVC) builds them all, as Visual Studio does.  With no startup project every project is
//built.
class CBuild
{
protected:
	CVcxSolution*			mpcSolution;
	CBuildConfiguration*	mpcConfiguration;
	CChars					mszRoot;
	CChars					mszOutputDirectory;		//In place of Root\Output\<toolchain>\<configuration>, if not empty.
	CChars					mszListingDirectory;	//In place of Root\Listing\<toolchain>\<configuration>, if not empty.
	CChars					mszStartupProject;		//The solution's startup project's name, empty if it hasn't been said.
	EBuildState				meState;
	CChars					mszProgress;
	size					muiDone;	//How many of the build's steps (units compiled, commands run) are done,
	size					muiSteps;	//of how many.  0 of 0 until it says.

public:
	virtual	void		Init(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot);
	virtual	void		Kill(void);

	virtual	bool		Start(CArrayChars* paszLines) =0;
	virtual	bool		Poll(CArrayChars* paszLines) =0;
	virtual	void		Cancel(void) =0;

			EBuildState	GetState(void);
			bool		IsRunning(void);
			char*		GetProgress(void);
			float		GetFraction(void);
			void		SetOutputDirectory(const char* szDirectory);
			void		SetListingDirectory(const char* szDirectory);
			void		GetOutputDirectory(CChars* pszDest);
			void		GetListingDirectory(CChars* pszDest);
			bool		GetProgramPath(CChars* pszDest);
			void		SetStartupProject(const char* szName);
			char*		GetStartupProject(void);
			void		GetProjects(CArrayBuildProjects* pasDest, bool bOneApplication = false);

	static	void		GetDirectory(const char* szRoot, CBuildConfiguration* pcConfiguration, const char* szKind, CChars* pszDest);
	static	bool		GetProgramPath(const char* szRoot, CBuildConfiguration* pcConfiguration, CChars* pszDest);
	static	bool		GetLineTablePath(const char* szRoot, CBuildConfiguration* pcConfiguration, CChars* pszDest);

protected:
			void		GetDirectory(const char* szKind, CChars* pszDest);
			void		Finish(EBuildState eState, CArrayChars* paszLines);
			void		SetProgress(const char* szProgress);
			void		SetSteps(size uiDone, size uiSteps);
};


#endif // __BUILD_H__

