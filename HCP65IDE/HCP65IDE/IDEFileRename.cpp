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
#include <stdio.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/ArrayInt.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEFileRename.h"


//////////////////////////////////////////////////////////////////////////
//
// szOld (a full path) to be called szName in its directory.
//
//////////////////////////////////////////////////////////////////////////
void SIDEFileRename::Init(const char* szOld, const char* szName)
{
	CFileUtil	cFileUtil;
	CChars		szOldName;
	CChars		szDirectory;

	szOldPath.Init(szOld);
	szNewName.Init(szName);
	szOldName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szOld, &szOldName, &szDirectory);
	szNewPath.Init(szDirectory);
	cFileUtil.AppendToPath(&szNewPath, szName);
	szDirectory.Kill();
	szOldName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFileRename::Kill(void)
{
	szNewPath.Kill();
	szNewName.Kill();
	szOldPath.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDEFileRenames(CArrayIDEFileRenames* pasRenames)
{
	size	i;

	for (i = 0; i < pasRenames->NumElements(); i++)
	{
		pasRenames->Get(i)->Kill();
	}
	pasRenames->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The length of a file name's stem: up to its first '.'.
//
//////////////////////////////////////////////////////////////////////////
static size GetIDEStemLength(const char* szName)
{
	const char*	szDot;

	szDot = strchr(szName, '.');
	return szDot ? (size)(szDot - szName) : strlen(szName);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::Class(void)
{
	U_Pointer(mpcContent);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::Save(CObjectWriter* pcFile) { return false; }
bool CIDEFileRename::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The C and C++ files of the solution's projects, in szFileName's
// directory, whose stem is its stem, or its stem and an _ part: the
// files renamed with it (Preprocessor.h's are Preprocessor.cpp,
// Preprocessor_Linux.cpp and Preprocessor_Windows.cpp, not
// PreprocessorHeaderCache.cpp).  Not szFileName.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::FindRelatedFiles(const char* szFileName, CArrayChars* paszDest)
{
	CFileUtil		cFileUtil;
	CIDECodeModel*	pcModel;
	CChars			szName;
	CChars			szDirectory;
	CChars			szOtherName;
	CChars			szOtherDirectory;
	char*			szOther;
	EIDEFileType	eType;
	size			uiStem;
	size			uiOtherStem;
	size			i;
	bool			bRelated;

	pcModel = mpcContent->GetCodeModel();
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);
	uiStem = GetIDEStemLength(szName.Text());
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szOther = pcModel->GetProjectFile(i);
		eType = CIDEIcons::GetFileType(szOther);
		if (((eType != IFT_CPP) && (eType != IFT_C) && (eType != IFT_Header)) || (StringInsensitiveCompare(szOther, szFileName) == 0))
		{
			continue;
		}
		szOtherName.Init();
		szOtherDirectory.Init();
		cFileUtil.SplitPath(szOther, &szOtherName, &szOtherDirectory);
		bRelated = false;
		if ((StringInsensitiveCompare(szOtherDirectory.Text(), szDirectory.Text()) == 0) && (szOtherName.Length() > uiStem) &&
			(MemICmp(szOtherName.Text(), szName.Text(), uiStem) == 0))
		{
			uiOtherStem = GetIDEStemLength(szOtherName.Text());
			bRelated = (uiOtherStem == uiStem) || ((uiOtherStem > uiStem + 1) && (szOtherName.Text()[uiStem] == '_'));
		}
		if (bRelated)
		{
			paszDest->Add(szOther);
		}
		szOtherDirectory.Kill();
		szOtherName.Kill();
	}
	szDirectory.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName to be called szNewName added to pasDest, and with bRelated
// its related files too (FindRelatedFiles), each given szNewName's stem
// in place of its own and keeping the rest of its name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::AddRenames(const char* szFileName, const char* szNewName, bool bRelated, CArrayIDEFileRenames* pasDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszRelated;
	CChars		szName;
	CChars		szDirectory;
	CChars		szRelatedName;
	CChars		szRelatedDirectory;
	CChars		szNewRelated;
	size		uiStem;
	size		i;

	pasDest->Add()->Init(szFileName, szNewName);
	if (!bRelated)
	{
		return;
	}
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);
	uiStem = GetIDEStemLength(szName.Text());
	aszRelated.Init();
	FindRelatedFiles(szFileName, &aszRelated);
	for (i = 0; i < aszRelated.NumElements(); i++)
	{
		szRelatedName.Init();
		szRelatedDirectory.Init();
		cFileUtil.SplitPath(aszRelated.GetText(i), &szRelatedName, &szRelatedDirectory);
		szNewRelated.Init();
		szNewRelated.AppendSubString(szNewName, GetIDEStemLength(szNewName));
		szNewRelated.Append(szRelatedName.Text(uiStem));
		pasDest->Add()->Init(aszRelated.GetText(i), szNewRelated.Text());
		szNewRelated.Kill();
		szRelatedDirectory.Kill();
		szRelatedName.Kill();
	}
	aszRelated.Kill();
	szDirectory.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Why the files can't be renamed, appended to pszDest: false if they can.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::FindProblem(CArrayIDEFileRenames* pasRenames, CChars* pszDest)
{
	CFileUtil		cFileUtil;
	CIDECodeModel*	pcModel;
	SIDEFileRename*	psRename;
	SIDEFileRename*	psOther;
	CChars			szOldName;
	CChars			szDirectory;
	size			i;
	size			j;
	bool			bProblem;

	pcModel = mpcContent->GetCodeModel();
	bProblem = false;
	for (i = 0; (i < pasRenames->NumElements()) && !bProblem; i++)
	{
		psRename = pasRenames->Get(i);
		szOldName.Init();
		szDirectory.Init();
		cFileUtil.SplitPath(psRename->szOldPath.Text(), &szOldName, &szDirectory);
		bProblem = true;
		if (psRename->szNewName.Empty() || psRename->szNewName.Contains('/') || psRename->szNewName.Contains('\\'))
		{
			pszDest->Append("a file's new name can't be empty or have directories in it");
		}
		else if (psRename->szNewName.Equals(szOldName))
		{
			pszDest->Append(szOldName);
			pszDest->Append(" is already called that");
		}
		else if (pcModel->FindProjectOfFile(psRename->szOldPath.Text()) == MAX_SIZE)
		{
			pszDest->Append(szOldName);
			pszDest->Append(" isn't in the solution");
		}
		else if (!cFileUtil.Exists(psRename->szOldPath.Text()))
		{
			pszDest->Append(szOldName);
			pszDest->Append(" isn't on the disk");
		}
		else if ((StringInsensitiveCompare(psRename->szNewPath.Text(), psRename->szOldPath.Text()) != 0) &&
				 (cFileUtil.Exists(psRename->szNewPath.Text()) || (pcModel->FindProjectOfFile(psRename->szNewPath.Text()) != MAX_SIZE)))
		{
			pszDest->Append(psRename->szNewName);
			pszDest->Append(" is already there");
		}
		else
		{
			bProblem = false;
			for (j = 0; (j < i) && !bProblem; j++)
			{
				psOther = pasRenames->Get(j);
				if ((StringInsensitiveCompare(psOther->szNewPath.Text(), psRename->szNewPath.Text()) == 0) ||
					(StringInsensitiveCompare(psOther->szOldPath.Text(), psRename->szOldPath.Text()) == 0))
				{
					pszDest->Append("two files would be called ");
					pszDest->Append(psRename->szNewName);
					bProblem = true;
				}
			}
		}
		szDirectory.Kill();
		szOldName.Kill();
	}
	return bProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// The file renamed on the disk: with git mv if git tracks it, so the
// rename is staged.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::MoveFile(SIDEFileRename* psRename)
{
	CGitRepository*	pcRepository;

	pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(psRename->szOldPath.Text());
	if (pcRepository && pcRepository->IsTracked(psRename->szOldPath.Text()))
	{
		return pcRepository->Move(psRename->szOldPath.Text(), psRename->szNewPath.Text());
	}
	return rename(psRename->szOldPath.Text(), psRename->szNewPath.Text()) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool ContainsIDEProject(CArrayInt* paiProjects, size uiProject)
{
	size	i;

	for (i = 0; i < paiProjects->NumElements(); i++)
	{
		if ((size)paiProjects->GetValue(i) == uiProject)
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void KillIDEIncludes(CArrayIDELocations* pasIncludes)
{
	size	i;

	for (i = 0; i < pasIncludes->NumElements(); i++)
	{
		pasIncludes->Get(i)->szFileName.Kill();
	}
	pasIncludes->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Renames the files (Rename File): every #include that finds one is
// changed in its file's editor (opened, not shown, if it isn't open), as
// one undo group; then each file is renamed on the disk, in every project
// that has it and in its tab; the files edited are saved and the
// workspace is shown and indexed again.  The #includes are found before
// anything changes, as they are found by looking for their files.
// Nothing changes, and the status line says why, if a file can't be
// renamed or an includer changed since it was searched.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::RenameFiles(CArrayIDEFileRenames* pasRenames)
{
	CFileUtil			cFileUtil;
	CChars				szMessage;
	CChars				szOldName;
	CChars				szDirectory;
	CArrayIDELocations	asIncludes;
	CArrayInt			aiIncludeRenames;
	CArrayChars			aszEdited;
	CArrayInt			aiProjects;
	SIDELocation*		psInclude;
	SIDEFileRename*		psRename;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CVcxProject*		pcProject;
	CBuildWorkspace*	pcWorkspace;
	STextRange			sRange;
	size				uiBefore;
	size				uiStart;
	size				i;
	size				j;
	bool				bResult;

	szMessage.Init();
	if (FindProblem(pasRenames, &szMessage))
	{
		szMessage.Insert(0, "Can't rename: ");
		szMessage.Append('.');
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		return false;
	}

	//The #includes of each file, while the files are where they are.
	asIncludes.Init();
	aiIncludeRenames.Init();
	for (i = 0; i < pasRenames->NumElements(); i++)
	{
		uiBefore = asIncludes.NumElements();
		mpcContent->GetCodeModel()->FindIncludesOf(pasRenames->Get(i)->szOldPath.Text(), &mpcContent->GetFindInFiles(), &asIncludes);
		for (j = uiBefore; j < asIncludes.NumElements(); j++)
		{
			aiIncludeRenames.Add((int)i);
		}
	}

	//Each includer's editor, the old name still where it was found.
	aszEdited.Init();
	bResult = true;
	for (i = 0; (i < asIncludes.NumElements()) && bResult; i++)
	{
		psInclude = asIncludes.Get(i);
		psRename = pasRenames->Get((size)aiIncludeRenames.GetValue(i));
		pTab = mpcContent->GetDocuments()->FindTab(psInclude->szFileName.Text());
		if (pTab.IsNull())
		{
			pTab = mpcContent->GetDocuments()->OpenFileTab(psInclude->szFileName.Text(), mpcContent->GetCodeModel()->FindProjectOfFile(psInclude->szFileName.Text()), false);
		}
		bResult = pTab.IsNotNull();
		if (bResult)
		{
			pEditor = pTab->GetEditor();
			szOldName.Init();
			szDirectory.Init();
			cFileUtil.SplitPath(psRename->szOldPath.Text(), &szOldName, &szDirectory);
			bResult = ((size)psInclude->iLine < pEditor->NumLines()) && ((size)psInclude->iLength == szOldName.Length());
			if (bResult)
			{
				uiStart = pEditor->GetLineStart((size)psInclude->iLine) + (size)psInclude->iColumn;
				bResult = (uiStart + szOldName.Length() <= strlen(pEditor->GetText())) && (MemICmp(&pEditor->GetText()[uiStart], szOldName.Text(), szOldName.Length()) == 0);
			}
			szDirectory.Kill();
			szOldName.Kill();
		}
		if (!bResult)
		{
			szMessage.Append("Rename stopped: ");
			szMessage.Append(psInclude->szFileName);
			szMessage.Append(" has changed since it was searched.");
		}
		else if (!aszEdited.Contains(psInclude->szFileName.Text()))
		{
			aszEdited.Add(psInclude->szFileName.Text());
		}
	}
	if (!bResult)
	{
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		aszEdited.Kill();
		aiIncludeRenames.Kill();
		KillIDEIncludes(&asIncludes);
		return false;
	}

	//The #includes changed, as one undo group.  An edit within a line moves no other line.
	mpcContent->GetEdit()->BeginUndoGroup();
	for (i = 0; i < asIncludes.NumElements(); i++)
	{
		psInclude = asIncludes.Get(i);
		psRename = pasRenames->Get((size)aiIncludeRenames.GetValue(i));
		pEditor = mpcContent->GetDocuments()->FindTab(psInclude->szFileName.Text())->GetEditor();
		sRange.uiStart = pEditor->GetLineStart((size)psInclude->iLine) + (size)psInclude->iColumn;
		sRange.uiEnd = sRange.uiStart + (size)psInclude->iLength;
		pEditor->ReplaceRanges(&sRange, 1, psRename->szNewName.Text());
	}
	mpcContent->GetEdit()->EndUndoGroup();

	//The files renamed: on the disk, in the projects, in their tabs and the undo log.
	pcWorkspace = mpcContent->GetWorkspace();
	aiProjects.Init();
	for (i = 0; (i < pasRenames->NumElements()) && bResult; i++)
	{
		psRename = pasRenames->Get(i);
		bResult = MoveFile(psRename);
		if (!bResult)
		{
			szMessage.Append("Rename stopped: ");
			szMessage.Append(psRename->szOldPath);
			szMessage.Append(" couldn't be renamed on the disk.");
			break;
		}
		for (j = 0; j < pcWorkspace->NumProjects(); j++)
		{
			pcProject = pcWorkspace->GetSolutionProject(j)->GetProject();
			if (pcProject && pcProject->RenameItem(psRename->szOldPath.Text(), psRename->szNewName.Text()) && !ContainsIDEProject(&aiProjects, j))
			{
				aiProjects.Add((int)j);
			}
		}
		pTab = mpcContent->GetDocuments()->FindTab(psRename->szOldPath.Text());
		if (pTab.IsNotNull())
		{
			pTab->SetFileName(psRename->szNewPath.Text());
			mpcContent->GetEdit()->FileRenamed(psRename->szOldPath.Text(), psRename->szNewPath.Text());
		}
		for (j = 0; j < aszEdited.NumElements(); j++)
		{
			if (StringCompare(aszEdited.GetText(j), psRename->szOldPath.Text()) == 0)
			{
				aszEdited.Get(j)->Set(psRename->szNewPath);
			}
		}
	}

	//The files edited are saved, as the files have moved under them.
	for (i = 0; i < aszEdited.NumElements(); i++)
	{
		pTab = mpcContent->GetDocuments()->FindTab(aszEdited.GetText(i));
		if (pTab.IsNotNull())
		{
			mpcContent->GetDocuments()->SaveFile(pTab);
		}
	}
	mpcContent->GetSolutions()->WorkspaceChanged(true);

	if (bResult)
	{
		psRename = pasRenames->Get(0);
		szOldName.Init();
		szDirectory.Init();
		cFileUtil.SplitPath(psRename->szOldPath.Text(), &szOldName, &szDirectory);
		szMessage.Append("Renamed ");
		szMessage.Append(szOldName);
		szMessage.Append(" to ");
		szMessage.Append(psRename->szNewName);
		if (pasRenames->NumElements() > 1)
		{
			szMessage.Append(" and ");
			szMessage.Append((int)pasRenames->NumElements() - 1);
			szMessage.Append(pasRenames->NumElements() == 2 ? " more file" : " more files");
		}
		szMessage.Append(": ");
		szMessage.Append((int)asIncludes.NumElements());
		szMessage.Append(asIncludes.NumElements() == 1 ? " #include in " : " #includes in ");
		szMessage.Append((int)aszEdited.NumElements());
		szMessage.Append(aszEdited.NumElements() == 1 ? " file, " : " files, ");
		szMessage.Append((int)aiProjects.NumElements());
		szMessage.Append(aiProjects.NumElements() == 1 ? " project." : " projects.");
		szDirectory.Kill();
		szOldName.Kill();
	}
	mpcContent->ShowStatus(szMessage.Text());

	szMessage.Kill();
	aiProjects.Kill();
	aszEdited.Kill();
	aiIncludeRenames.Kill();
	KillIDEIncludes(&asIncludes);
	return bResult;
}
