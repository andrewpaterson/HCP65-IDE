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
#ifndef __VCX_SOLUTION_PROJECT_H__
#define __VCX_SOLUTION_PROJECT_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "VcxProject.h"


#define VCX_SOLUTION_FOLDER_TYPE	"{2150E333-8FDC-42A3-9474-1A3956D46DE8}"
#define VCX_CPP_PROJECT_TYPE		"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"


//The project configuration a solution configuration builds: {guid}.Debug|x64.ActiveCfg = Debug|x64 and .Build.0.
class CVcxSolutionProjectConfiguration
{
protected:
	CChars	mszSolutionConfiguration;
	CChars	mszProjectConfiguration;
	bool	mbBuild;
	bool	mbDeploy;

public:
	void	Init(const char* szSolutionConfiguration);
	void	Kill(void);

	char*	GetSolutionConfiguration(void);
	char*	GetProjectConfiguration(void);
	void	SetProjectConfiguration(const char* szProjectConfiguration);
	bool	IsBuild(void);
	void	SetBuild(bool bBuild);
	bool	IsDeploy(void);
	void	SetDeploy(bool bDeploy);
};


typedef CArrayTemplate<CVcxSolutionProjectConfiguration>	CArrayVcxSolutionProjectConfiguration;


//A Project(...) entry in a .sln: a project (a .vcxproj, read when the solution is read) or a solution folder.
class CVcxSolutionProject
{
protected:
	CChars									mszTypeGuid;
	CChars									mszName;
	CChars									mszPath;			//As the .sln has it, from the solution's directory.
	CChars									mszFullPath;		//With the case of the file on disk when it was found.
	CChars									mszGuid;
	CChars									mszParentGuid;		//The solution folder it is in (NestedProjects); empty at the top.
	CArrayChars								maszSolutionItems;	//A solution folder's files.
	CArrayChars								maszDependencies;	//GUIDs from ProjectSection(ProjectDependencies).
	CArrayVcxSolutionProjectConfiguration	masConfigurations;
	CVcxProject*							mpcProject;			//NULL for a folder, another kind of project or one that couldn't be read.

public:
	void								Init(const char* szTypeGuid, const char* szName, const char* szPath, const char* szGuid);
	void								Kill(void);

	bool								Load(const char* szSolutionDirectory);

	char*								GetTypeGuid(void);
	char*								GetName(void);
	char*								GetPath(void);
	char*								GetFullPath(void);
	char*								GetGuid(void);
	void								SetGuid(const char* szGuid);
	char*								GetParentGuid(void);
	void								SetParentGuid(const char* szParentGuid);
	bool								IsFolder(void);
	bool								IsVcxProject(void);
	bool								IsLoaded(void);
	CVcxProject*						GetProject(void);

	void								AddSolutionItem(const char* szItem);
	size								NumSolutionItems(void);
	char*								GetSolutionItem(size uiIndex);
	void								AddDependency(const char* szGuid);
	size								NumDependencies(void);
	char*								GetDependency(size uiIndex);

	CVcxSolutionProjectConfiguration*	GetOrAddConfiguration(const char* szSolutionConfiguration);
	CVcxSolutionProjectConfiguration*	FindConfiguration(const char* szSolutionConfiguration);
	size								NumConfigurations(void);
	CVcxSolutionProjectConfiguration*	GetConfiguration(size uiIndex);
};


typedef CArrayTemplatePtr<CVcxSolutionProject>	CArrayVcxSolutionProject;


#endif // __VCX_SOLUTION_PROJECT_H__

