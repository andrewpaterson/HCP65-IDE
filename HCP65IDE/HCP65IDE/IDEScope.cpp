/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "IDEContent.h"
#include "IDEScope.h"


//The scopes' names, by EIDEScope: as the settings keep them (without the spaces) and as they are shown.
static const char*	gaszIDEScopeNames[] = { "Solution", "Startup Solution", "Project", "Current File", "Open Files", "Directory" };
static_assert(sizeof(gaszIDEScopeNames) / sizeof(const char*) == IS_Count, "Each EIDEScope needs its name in gaszIDEScopeNames.");


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEScope::Init(void)
{
	meScope = IS_Solution;
	muiProject = MAX_SIZE;
	muiSolution = MAX_SIZE;
	mszFile.Init();
	mszDirectory.Init();
	maszOpenFiles.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEScope::Kill(void)
{
	maszOpenFiles.Kill();
	mszDirectory.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEScope::Copy(CIDEScope* pcSource)
{
	size	i;

	meScope = pcSource->meScope;
	muiProject = pcSource->muiProject;
	muiSolution = pcSource->muiSolution;
	mszFile.Set(pcSource->mszFile);
	mszDirectory.Set(pcSource->mszDirectory);
	maszOpenFiles.ReInit();
	for (i = 0; i < pcSource->maszOpenFiles.NumElements(); i++)
	{
		maszOpenFiles.Add(pcSource->maszOpenFiles.GetText(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The scope eScope as the IDE is now: the active file and its project,
// the startup solution, the files that are open, and szDirectory (the one
// typed, for IS_Directory).
//
//////////////////////////////////////////////////////////////////////////
void CIDEScope::Capture(CIDEContent* pcContent, EIDEScope eScope, const char* szDirectory)
{
	Ptr<CIDEEditorTab>	pTab;
	CIDEDocuments*		pcDocuments;
	size				i;

	meScope = eScope;
	pcDocuments = &pcContent->GetDocuments();
	muiSolution = pcContent->GetWorkspace()->GetStartup();
	muiProject = MAX_SIZE;
	mszFile.Set("");
	pTab = pcDocuments->GetActiveTab();
	if (pTab.IsNotNull())
	{
		mszFile.Set(pTab->GetFileName());
		muiProject = pcContent->GetCodeModel()->FindProjectOfFile(pTab->GetFileName());
		if ((muiProject == MAX_SIZE) && (pTab->GetProject() < pcContent->GetWorkspace()->NumProjects()))
		{
			muiProject = pTab->GetProject();
		}
	}

	maszOpenFiles.ReInit();
	for (i = 0; i < pcDocuments->NumTabs(); i++)
	{
		maszOpenFiles.Add(pcDocuments->GetTab(i)->GetFileName());
	}

	mszDirectory.Set(szDirectory ? szDirectory : "");
	mszDirectory.StripWhitespace();
	while ((mszDirectory.Length() > 1) && ((mszDirectory.Text()[mszDirectory.Length() - 1] == '/') || (mszDirectory.Text()[mszDirectory.Length() - 1] == '\\')))
	{
		mszDirectory.SetLength(mszDirectory.Length() - 1);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file is in a directory or under it, whatever the case of
// their letters.  The directory has no separator at its end, unless it is
// only one.
//
//////////////////////////////////////////////////////////////////////////
static bool IsInIDEDirectory(const char* szDirectory, const char* szFileName)
{
	size	uiLength;
	size	i;

	uiLength = strlen(szDirectory);
	if ((uiLength == 0) || (strlen(szFileName) <= uiLength))
	{
		return false;
	}
	for (i = 0; i < uiLength; i++)
	{
		if (ToLower(szDirectory[i]) != ToLower(szFileName[i]))
		{
			return false;
		}
	}
	return (szFileName[uiLength] == '/') || (szFileName[uiLength] == '\\') || (uiLength == 1);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file is in the scope.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEScope::Contains(CIDEContent* pcContent, const char* szFileName)
{
	size	uiProject;
	size	i;

	switch (meScope)
	{
	case IS_Solution:
		return true;
	case IS_StartupSolution:
		uiProject = pcContent->GetCodeModel()->FindProjectOfFile(szFileName);
		return (uiProject != MAX_SIZE) && (pcContent->GetWorkspace()->GetProject(uiProject)->uiSolution == muiSolution);
	case IS_Project:
		return (muiProject != MAX_SIZE) && (pcContent->GetCodeModel()->FindProjectOfFile(szFileName) == muiProject);
	case IS_CurrentFile:
		return !mszFile.Empty() && (StringInsensitiveCompare(mszFile.Text(), szFileName) == 0);
	case IS_OpenFiles:
		for (i = 0; i < maszOpenFiles.NumElements(); i++)
		{
			if (StringInsensitiveCompare(maszOpenFiles.GetText(i), szFileName) == 0)
			{
				return true;
			}
		}
		return false;
	case IS_Directory:
		return IsInIDEDirectory(mszDirectory.Text(), szFileName);
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The scope's files, added to paszFiles.  A directory's aren't known
// until it is listed: it is added to paszDirectories, if it is there, for
// whoever wants them to list.
//
//////////////////////////////////////////////////////////////////////////
void CIDEScope::ListFiles(CIDEContent* pcContent, CArrayChars* paszFiles, CArrayChars* paszDirectories)
{
	CIDECodeModel*	pcCodeModel;
	CFileUtil		cFileUtil;
	size			i;

	pcCodeModel = pcContent->GetCodeModel();
	switch (meScope)
	{
	case IS_Solution:
	case IS_StartupSolution:
	case IS_Project:
		for (i = 0; i < pcCodeModel->NumProjectFiles(); i++)
		{
			if (Contains(pcContent, pcCodeModel->GetProjectFile(i)))
			{
				paszFiles->Add(pcCodeModel->GetProjectFile(i));
			}
		}
		break;
	case IS_CurrentFile:
		if (!mszFile.Empty())
		{
			paszFiles->Add(mszFile.Text());
		}
		break;
	case IS_OpenFiles:
		for (i = 0; i < maszOpenFiles.NumElements(); i++)
		{
			paszFiles->Add(maszOpenFiles.GetText(i));
		}
		break;
	case IS_Directory:
		if (!mszDirectory.Empty() && cFileUtil.IsDirectory(mszDirectory.Text()))
		{
			paszDirectories->Add(mszDirectory.Text());
		}
		break;
	default:
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The scope after what was looked for: "in the solution", "in BaseLib",
// "in Calculator.cpp", "in the open files", "in <directory>".
//
//////////////////////////////////////////////////////////////////////////
void CIDEScope::AppendName(CIDEContent* pcContent, CChars* pszDest)
{
	CBuildWorkspace*	pcWorkspace;

	pcWorkspace = pcContent->GetWorkspace();
	pszDest->Append("in ");
	switch (meScope)
	{
	case IS_StartupSolution:
		pszDest->Append((muiSolution < pcWorkspace->NumSolutions()) ? pcWorkspace->GetSolution(muiSolution)->mcSolution.GetName() : "no solution");
		break;
	case IS_Project:
		pszDest->Append((muiProject < pcWorkspace->NumProjects()) ? pcWorkspace->GetSolutionProject(muiProject)->GetName() : "no project");
		break;
	case IS_CurrentFile:
		pszDest->Append(mszFile.Empty() ? "no file" : GetIDEFileNamePart(mszFile.Text()));
		break;
	case IS_OpenFiles:
		pszDest->Append("the open files");
		break;
	case IS_Directory:
		pszDest->Append(mszDirectory);
		break;
	default:
		pszDest->Append("the solution");
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the scope looks in, as a line under its tab says: "Every project's
// files", "The files of BaseLib", "Calculator.cpp", "The 3 open files".
//
//////////////////////////////////////////////////////////////////////////
void CIDEScope::AppendDescription(CIDEContent* pcContent, CChars* pszDest)
{
	CBuildWorkspace*	pcWorkspace;

	pcWorkspace = pcContent->GetWorkspace();
	switch (meScope)
	{
	case IS_StartupSolution:
		if (muiSolution < pcWorkspace->NumSolutions())
		{
			pszDest->Append("The files of the startup solution, ");
			pszDest->Append(pcWorkspace->GetSolution(muiSolution)->mcSolution.GetName());
		}
		else
		{
			pszDest->Append("No solution is open");
		}
		break;
	case IS_Project:
		if (muiProject < pcWorkspace->NumProjects())
		{
			pszDest->Append("The files of ");
			pszDest->Append(pcWorkspace->GetSolutionProject(muiProject)->GetName());
		}
		else
		{
			pszDest->Append("The active file isn't in a project");
		}
		break;
	case IS_CurrentFile:
		pszDest->Append(mszFile.Empty() ? "No file is open" : mszFile.Text());
		break;
	case IS_OpenFiles:
		pszDest->Append("The ");
		pszDest->Append((uint32)maszOpenFiles.NumElements());
		pszDest->Append((maszOpenFiles.NumElements() == 1) ? " open file" : " open files");
		break;
	case IS_Directory:
		pszDest->Append(mszDirectory);
		break;
	default:
		pszDest->Append("Every project's files");
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A scope's name, as it is shown and kept, and the scope of a name: false
// if it is none's.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEScope::GetScopeName(EIDEScope eScope)
{
	return ((size)eScope < IS_Count) ? gaszIDEScopeNames[eScope] : "";
}


bool CIDEScope::FindScope(const char* szName, EIDEScope* peScope)
{
	size	i;

	for (i = 0; i < IS_Count; i++)
	{
		if (szName && (StringInsensitiveCompare(szName, gaszIDEScopeNames[i]) == 0))
		{
			*peScope = (EIDEScope)i;
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a scope is one to choose now: the startup solution's only
// while the workspace has more than one solution.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEScope::IsOffered(CIDEContent* pcContent, EIDEScope eScope)
{
	return (eScope != IS_StartupSolution) || (pcContent->GetWorkspace()->NumSolutions() > 1);
}


void CIDEScope::SetScope(EIDEScope eScope) { meScope = eScope; }  //The kind alone: what was captured is kept.
EIDEScope CIDEScope::GetScope(void) { return meScope; }
char* CIDEScope::GetDirectory(void) { return mszDirectory.Text(); }
size CIDEScope::GetProject(void) { return muiProject; }
char* CIDEScope::GetFile(void) { return mszFile.Text(); }

