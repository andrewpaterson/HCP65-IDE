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
#ifndef __BUILD_WORKSPACE_H__
#define __BUILD_WORKSPACE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayTemplate.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildConfigurations.h"


#define BUILD_WORKSPACE_EXTENSION	".hcp65workspace"


//One solution of a workspace: the solution (or a project read as one) and its build configurations.
class CBuildWorkspaceSolution
{
public:
	CChars					mszFile;
	CVcxSolution			mcSolution;
	bool					mbRead;
	CBuildConfigurations	mcConfigurations;

	void	Init(void);
	void	Kill(void);
	bool	Open(const char* szFile);
};


//One of the workspace's projects, once: a project two solutions both have (BaseLib) is the first's.
struct SBuildWorkspaceProject
{
	size	uiSolution;
	size	uiProject;		//Its index in that solution.
};


typedef CArrayTemplate<CBuildWorkspaceSolution*>	CArrayBuildWorkspaceSolutions;
typedef CArrayTemplate<SBuildWorkspaceProject>		CArrayBuildWorkspaceProjects;


//The solutions open together (a program's and the kernal it runs on), one of them the startup solution: the one
//debugged.  The workspace file (.hcp65workspace) lists them and which is the startup one, their paths relative to
//it, and the configuration chosen last:
//
//	<Workspace Configuration="Release|W65C816">
//		<Solution File="ModPlay\HCP65ModPlay.sln" Startup="true"/>
//		<Solution File="..\HCP65Lib\Kernal\HCP65Kernal.sln"/>
//	</Workspace>
//
//Each solution has its own configurations (its .hcp65build); a configuration is chosen by name for all of them.
//Its projects are numbered once across the solutions (NumProjects), so a shared project is one project.  Built by a
//configuration name, a solution whose program another loads (a <Load> of the program's file) is built before it.
class CBuildWorkspace
{
protected:
	CChars							mszFile;		//The workspace file, empty if none has been read or written.
	CArrayBuildWorkspaceSolutions	mapcSolutions;
	size							muiStartup;		//MAX_SIZE with no solutions.
	CChars							mszConfiguration;	//The configuration chosen last, empty if none.
	CArrayBuildWorkspaceProjects	masProjects;

public:
	void					Init(void);
	void					Kill(void);
	void					Clear(void);

	bool					Open(const char* szFile);
	bool					Read(const char* szFile);
	bool					Write(const char* szFile);
	char*					GetFile(void);

	size					AddSolution(const char* szFile);
	bool					RemoveSolution(size uiSolution);
	size					FindSolution(const char* szFile);
	void					SetStartup(size uiSolution);
	size					GetStartup(void);
	void					SetConfiguration(const char* szConfiguration);
	char*					GetConfiguration(void);

	size					NumSolutions(void);
	CBuildWorkspaceSolution*	GetSolution(size uiSolution);
	CBuildWorkspaceSolution*	GetStartupSolution(void);

	size					NumProjects(void);
	SBuildWorkspaceProject*	GetProject(size uiIndex);
	CVcxSolutionProject*	GetSolutionProject(size uiIndex);
	CVcxSolution*			GetProjectSolution(size uiIndex);
	size					FindProject(size uiSolution, size uiProject);

	void					GetConfigurationNames(CArrayChars* paszDest);
	void					GetConfigurationNames(CArrayChars* paszDest, bool bPlatforms);
	CBuildConfiguration*	FindConfiguration(size uiSolution, const char* szName);
	CBuildConfiguration*	FindAnyConfiguration(const char* szName);
	CBuildConfiguration*	FindAnyConfiguration(const char* szConfiguration, const char* szPlatform);
	void					GetBuildOrder(const char* szConfiguration, CArrayInt* paiDest);
	bool					Loads(size uiSolution, size uiOther, const char* szConfiguration);
	void					GetProcessPrograms(const char* szConfiguration, CArrayInt* paiDest);
	void					GetModulePrograms(const char* szConfiguration, CArrayInt* paiDest);

protected:
	void					IndexProjects(void);
	size					GetSolutionInOrder(size uiIndex);
	void					GetCardPrograms(const char* szConfiguration, EBuildProgram eProgram, CArrayInt* paiDest);
};


void MakeRelativePath(const char* szDirectory, const char* szPath, CChars* pszDest);


#endif // __BUILD_WORKSPACE_H__

