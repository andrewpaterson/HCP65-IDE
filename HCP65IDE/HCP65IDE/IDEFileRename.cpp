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


#define IDE_RENAME_FILE_SETTINGS_TAG			"RenameFile"
#define IDE_RENAME_FILE_RELATED_ATTRIBUTE		"RenameRelated"
#define IDE_RENAME_FILE_COMMENTS_ATTRIBUTE		"SearchInComments"
#define IDE_RENAME_FILE_TEXT_ATTRIBUTE			"SearchForText"


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


static bool ContainsIDEProject(CArrayInt* paiProjects, size uiProject);


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
	mszFileName.Init();
	mbRelated = true;
	mbComments = true;
	mbText = true;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpNameField);
	M_Pointer(mpRelatedBox);
	M_Pointer(mpCommentsBox);
	M_Pointer(mpTextBox);
	M_Pointer(mpError);
	U_String(mszFileName);
	U_Bool(mbRelated);
	U_Bool(mbComments);
	U_Bool(mbText);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::Free(void)
{
	mszFileName.Kill();
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
// The file at szFrom renamed szTo (a full path in the same directory):
// on the disk, with git mv if git tracks it so the rename is staged; in
// every project that has it (each project changed added to paiProjects
// if it isn't NULL); in its tab, and in the undo log's edits.  Rename
// File, and the undo and redo of one, rename each file with it.  False
// if it couldn't be renamed on the disk, when nothing else changes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::MoveFile(const char* szFrom, const char* szTo, CArrayInt* paiProjects)
{
	CFileUtil			cFileUtil;
	CGitRepository*		pcRepository;
	CBuildWorkspace*	pcWorkspace;
	CVcxProject*		pcProject;
	Ptr<CIDEEditorTab>	pTab;
	CChars				szName;
	CChars				szDirectory;
	bool				bMoved;
	size				i;

	pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(szFrom);
	if (pcRepository && pcRepository->IsTracked(szFrom))
	{
		bMoved = pcRepository->Move(szFrom, szTo);
	}
	else
	{
		bMoved = !cFileUtil.Exists(szTo) && (rename(szFrom, szTo) == 0);
	}
	if (!bMoved)
	{
		return false;
	}

	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szTo, &szName, &szDirectory);
	pcWorkspace = mpcContent->GetWorkspace();
	for (i = 0; i < pcWorkspace->NumProjects(); i++)
	{
		pcProject = pcWorkspace->GetSolutionProject(i)->GetProject();
		if (pcProject && pcProject->RenameItem(szFrom, szName.Text()) && paiProjects && !ContainsIDEProject(paiProjects, i))
		{
			paiProjects->Add((int)i);
		}
	}
	szDirectory.Kill();
	szName.Kill();

	pTab = mpcContent->GetDocuments()->FindTab(szFrom);
	if (pTab.IsNotNull())
	{
		pTab->SetFileName(szTo);
	}
	mpcContent->GetEdit()->FileRenamed((char*)szFrom, (char*)szTo);
	return true;
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


//An edit Rename File makes: in a file (its index in the edited files), where an old name is and how long it is, which
//rename's new name replaces it, and why (EIDEFileRenameEdit).
struct SIDEFileEdit
{
	size	uiFile;
	size	uiStart;
	size	uiLength;
	size	uiRename;
	uint8	uiKind;
};


typedef CArrayTemplate<SIDEFileEdit>	CArrayIDEFileEdits;


//////////////////////////////////////////////////////////////////////////
//
// Whether the line uiOffset is on is an #include: its name is changed as
// an #include, whatever file it finds, never as a string.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEIncludeLine(const char* szText, size uiOffset)
{
	size	uiStart;

	for (uiStart = uiOffset; (uiStart > 0) && (szText[uiStart - 1] != '\n'); uiStart--)
	{
	}
	while ((szText[uiStart] == ' ') || (szText[uiStart] == '\t'))
	{
		uiStart++;
	}
	if (szText[uiStart] != '#')
	{
		return false;
	}
	uiStart++;
	while ((szText[uiStart] == ' ') || (szText[uiStart] == '\t'))
	{
		uiStart++;
	}
	return strncmp(&szText[uiStart], "include", 7) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The index of szFileName in paszFiles, added with its editor opened
// (not shown) if it isn't open.  MAX_SIZE if it can't be opened.
//
//////////////////////////////////////////////////////////////////////////
size CIDEFileRename::FindEditedFile(CArrayChars* paszFiles, const char* szFileName)
{
	size				uiFile;
	Ptr<CIDEEditorTab>	pTab;

	for (uiFile = 0; uiFile < paszFiles->NumElements(); uiFile++)
	{
		if (StringCompare(paszFiles->GetText(uiFile), szFileName) == 0)
		{
			return uiFile;
		}
	}
	pTab = mpcContent->GetDocuments()->FindTab(szFileName);
	if (pTab.IsNull())
	{
		pTab = mpcContent->GetDocuments()->OpenFileTab(szFileName, mpcContent->GetCodeModel()->FindProjectOfFile(szFileName), false);
	}
	if (pTab.IsNull())
	{
		return MAX_SIZE;
	}
	paszFiles->Add(szFileName);
	return uiFile;
}


//////////////////////////////////////////////////////////////////////////
//
// An edit of szFileName at uiStart added to pasEdits, unless it is there
// already; the file's editor must have szOldName there (any case for an
// #include, as the compiler finds one in any case on Windows).  False,
// with the file in pszChanged, if it hasn't.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::AddEdit(CArrayChars* paszFiles, CArrayIDEFileEdits* pasEdits, const char* szFileName, size uiStart, const char* szOldName, size uiRename, uint8 uiKind, CChars* pszChanged)
{
	size			uiFile;
	size			uiLength;
	size			i;
	char*			szText;
	bool			bSame;
	SIDEFileEdit*	psEdit;

	uiFile = FindEditedFile(paszFiles, szFileName);
	uiLength = strlen(szOldName);
	if (uiFile != MAX_SIZE)
	{
		szText = mpcContent->GetDocuments()->FindTab(paszFiles->GetText(uiFile))->GetEditor()->GetText();
		bSame = (uiStart + uiLength <= strlen(szText)) &&
				((uiKind == IFRE_Include) ? (MemICmp(&szText[uiStart], szOldName, uiLength) == 0) : (strncmp(&szText[uiStart], szOldName, uiLength) == 0));
	}
	else
	{
		bSame = false;
	}
	if (!bSame)
	{
		pszChanged->Set(szFileName);
		return false;
	}
	for (i = 0; i < pasEdits->NumElements(); i++)
	{
		if ((pasEdits->Get(i)->uiFile == uiFile) && (pasEdits->Get(i)->uiStart == uiStart))
		{
			return true;
		}
	}
	psEdit = pasEdits->Add();
	psEdit->uiFile = uiFile;
	psEdit->uiStart = uiStart;
	psEdit->uiLength = uiLength;
	psEdit->uiRename = uiRename;
	psEdit->uiKind = uiKind;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename File's searches: each of the solution's C and C++ files looked
// in for the old names in comments and strings (bComments, not on an
// #include's line), and the projects' text files for them as words
// (bText), the places added to pasEdits.  False, with the file in
// pszChanged, if a file's editor doesn't have the text that was searched.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::SearchFiles(CArrayChars* paszOldNames, bool bComments, bool bText, CArrayChars* paszFiles, CArrayIDEFileEdits* pasEdits, CChars* pszChanged)
{
	CIDECodeModel*				pcModel;
	size						uiProjectFile;
	char*						szFileName;
	EIDEFileType				eType;
	bool						bCpp;
	CTextFile					cFile;
	char*						szText;
	size						uiLength;
	SIDELineCounter				sLines;
	CArrayIDENameOccurrences	asFound;
	size						i;
	size						j;
	bool						bResult;
	CArrayInt					aiStarts;

	pcModel = mpcContent->GetCodeModel();
	bResult = true;
	for (uiProjectFile = 0; (uiProjectFile < pcModel->NumProjectFiles()) && bResult; uiProjectFile++)
	{
		szFileName = pcModel->GetProjectFile(uiProjectFile);
		eType = CIDEIcons::GetFileType(szFileName);
		bCpp = (eType == IFT_CPP) || (eType == IFT_C) || (eType == IFT_Header);
		if (!(bCpp && bComments) && !(bText && IsIDERenameTextFile(szFileName)))
		{
			continue;
		}
		cFile.Init();
		if (!mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength))
		{
			cFile.Kill();
			continue;
		}

		//Each old name's places, found before any is added: adding one may open the file's editor.
		asFound.Init();
		aiStarts.Init();
		for (i = 0; i < paszOldNames->NumElements(); i++)
		{
			sLines.Init(szText);
			if (bCpp)
			{
				FindIDECppOccurrences(szText, uiLength, paszOldNames->GetText(i), false, true, &sLines, &asFound);
			}
			else
			{
				FindIDEWords(szText, 0, uiLength, paszOldNames->GetText(i), paszOldNames->Get(i)->Length(), false, &sLines, &asFound);
			}
			for (j = aiStarts.NumElements(); j < asFound.NumElements(); j++)
			{
				aiStarts.Add(bCpp && IsIDEIncludeLine(szText, asFound.Get(j)->uiStart) ? -1 : (int)i);
			}
		}
		cFile.Kill();

		for (j = 0; (j < asFound.NumElements()) && bResult; j++)
		{
			if (aiStarts.GetValue(j) >= 0)
			{
				bResult = AddEdit(paszFiles, pasEdits, szFileName, asFound.Get(j)->uiStart, paszOldNames->GetText((size)aiStarts.GetValue(j)), (size)aiStarts.GetValue(j), bCpp ? IFRE_Comment : IFRE_Text, pszChanged);
			}
		}
		aiStarts.Kill();
		asFound.Kill();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Renames the files (Rename File): every #include that finds one is
// changed, and with bComments and bText its old name in comments and
// strings and in the projects' text files, in each file's editor (opened,
// not shown, if it isn't open); then each file is renamed on the disk, in
// every project that has it and in its tab; all of it one undo group.
// The files edited are saved and the workspace is shown and indexed
// again.  The #includes are found before anything changes, as they are
// found by looking for their files.  Nothing changes, and the status line
// says why, if a file can't be renamed or one to edit changed since it
// was searched.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::RenameFiles(CArrayIDEFileRenames* pasRenames, bool bComments, bool bText)
{
	CFileUtil			cFileUtil;
	CChars				szMessage;
	CChars				szChanged;
	CChars				szDirectory;
	CChars*				pszOldName;
	CArrayChars			aszOldNames;
	CArrayIDELocations	asIncludes;
	CArrayInt			aiIncludeRenames;
	CArrayChars			aszEdited;
	CArrayIDEFileEdits	asEdits;
	CArrayInt			aiProjects;
	SIDELocation*		psInclude;
	SIDEFileRename*		psRename;
	SIDEFileEdit*		psEdit;
	SIDEFileEdit*		psLatest;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	STextRange			sRange;
	size				auiKinds[IFRE_NumKinds];
	size				uiBefore;
	size				uiLatest;
	size				uiFile;
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

	aszOldNames.Init();
	for (i = 0; i < pasRenames->NumElements(); i++)
	{
		pszOldName = aszOldNames.Add();
		szDirectory.Init();
		cFileUtil.SplitPath(pasRenames->Get(i)->szOldPath.Text(), pszOldName, &szDirectory);
		szDirectory.Kill();
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

	//Every edit, each file's old name still where it was found.
	aszEdited.Init();
	asEdits.Init();
	szChanged.Init();
	bResult = true;
	for (i = 0; (i < asIncludes.NumElements()) && bResult; i++)
	{
		psInclude = asIncludes.Get(i);
		uiFile = FindEditedFile(&aszEdited, psInclude->szFileName.Text());
		bResult = (uiFile != MAX_SIZE);
		if (bResult)
		{
			pEditor = mpcContent->GetDocuments()->FindTab(aszEdited.GetText(uiFile))->GetEditor();
			bResult = ((size)psInclude->iLine < pEditor->NumLines()) &&
					  AddEdit(&aszEdited, &asEdits, psInclude->szFileName.Text(), pEditor->GetLineStart((size)psInclude->iLine) + (size)psInclude->iColumn,
							  aszOldNames.GetText((size)aiIncludeRenames.GetValue(i)), (size)aiIncludeRenames.GetValue(i), IFRE_Include, &szChanged);
		}
		if (!bResult)
		{
			szChanged.Set(psInclude->szFileName);
		}
	}
	if (bResult && (bComments || bText))
	{
		bResult = SearchFiles(&aszOldNames, bComments, bText, &aszEdited, &asEdits, &szChanged);
	}
	if (!bResult)
	{
		szMessage.Append("Rename stopped: ");
		szMessage.Append(szChanged);
		szMessage.Append(" has changed since it was searched.");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		szChanged.Kill();
		asEdits.Kill();
		aszEdited.Kill();
		aiIncludeRenames.Kill();
		KillIDEIncludes(&asIncludes);
		aszOldNames.Kill();
		return false;
	}

	//The edits, each file's from its end back so the places before stay where they are; then the files renamed, each a
	//step of the group.
	auiKinds[IFRE_Include] = 0;
	auiKinds[IFRE_Comment] = 0;
	auiKinds[IFRE_Text] = 0;
	mpcContent->GetEdit()->BeginUndoGroup();
	for (uiFile = 0; uiFile < aszEdited.NumElements(); uiFile++)
	{
		pEditor = mpcContent->GetDocuments()->FindTab(aszEdited.GetText(uiFile))->GetEditor();
		for (;;)
		{
			psLatest = NULL;
			uiLatest = 0;
			for (i = 0; i < asEdits.NumElements(); i++)
			{
				psEdit = asEdits.Get(i);
				if ((psEdit->uiFile == uiFile) && (psEdit->uiLength > 0) && ((psLatest == NULL) || (psEdit->uiStart > psLatest->uiStart)))
				{
					psLatest = psEdit;
					uiLatest = i;
				}
			}
			if (psLatest == NULL)
			{
				break;
			}
			sRange.uiStart = psLatest->uiStart;
			sRange.uiEnd = psLatest->uiStart + psLatest->uiLength;
			pEditor->ReplaceRanges(&sRange, 1, pasRenames->Get(psLatest->uiRename)->szNewName.Text());
			auiKinds[psLatest->uiKind]++;
			asEdits.Get(uiLatest)->uiLength = 0;
		}
	}

	aiProjects.Init();
	for (i = 0; (i < pasRenames->NumElements()) && bResult; i++)
	{
		psRename = pasRenames->Get(i);
		bResult = MoveFile(psRename->szOldPath.Text(), psRename->szNewPath.Text(), &aiProjects);
		if (!bResult)
		{
			szMessage.Append("Rename stopped: ");
			szMessage.Append(psRename->szOldPath);
			szMessage.Append(" couldn't be renamed on the disk.");
			break;
		}
		mpcContent->GetEdit()->LogFileRenamed(psRename->szOldPath.Text(), psRename->szNewPath.Text());
		for (j = 0; j < aszEdited.NumElements(); j++)
		{
			if (StringCompare(aszEdited.GetText(j), psRename->szOldPath.Text()) == 0)
			{
				aszEdited.Get(j)->Set(psRename->szNewPath);
			}
		}
	}
	mpcContent->GetEdit()->EndUndoGroup();

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
		szMessage.Append("Renamed ");
		szMessage.Append(aszOldNames.GetText(0));
		szMessage.Append(" to ");
		szMessage.Append(psRename->szNewName);
		if (pasRenames->NumElements() > 1)
		{
			szMessage.Append(" and ");
			szMessage.Append((int)pasRenames->NumElements() - 1);
			szMessage.Append(pasRenames->NumElements() == 2 ? " more file" : " more files");
		}
		szMessage.Append(": ");
		if (aszEdited.NumElements() == 0)
		{
			szMessage.Append("no file refers to it, ");
		}
		else
		{
			szMessage.Append((int)auiKinds[IFRE_Include]);
			szMessage.Append(auiKinds[IFRE_Include] == 1 ? " #include" : " #includes");
			if (auiKinds[IFRE_Comment] > 0)
			{
				szMessage.Append(", ");
				szMessage.Append((int)auiKinds[IFRE_Comment]);
				szMessage.Append(" in comments and strings");
			}
			if (auiKinds[IFRE_Text] > 0)
			{
				szMessage.Append(", ");
				szMessage.Append((int)auiKinds[IFRE_Text]);
				szMessage.Append(auiKinds[IFRE_Text] == 1 ? " text occurrence" : " text occurrences");
			}
			szMessage.Append(" in ");
			szMessage.Append((int)aszEdited.NumElements());
			szMessage.Append(aszEdited.NumElements() == 1 ? " file, " : " files, ");
		}
		szMessage.Append((int)aiProjects.NumElements());
		szMessage.Append(aiProjects.NumElements() == 1 ? " project." : " projects.");
	}
	mpcContent->ShowStatus(szMessage.Text());

	szMessage.Kill();
	szChanged.Kill();
	aiProjects.Kill();
	asEdits.Kill();
	aszEdited.Kill();
	aiIncludeRenames.Kill();
	KillIDEIncludes(&asIncludes);
	aszOldNames.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename File's commands: the active editor's file (Refactor, in the
// editors' context menu), Solution Explorer's selected file (its context
// menu, and Shift+F6 there through Rename), and the file of the document
// tab whose menu was opened.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.RenameFile", "Rename File...", NULL, Ptr<CImageCel>(), this, &CIDEFileRename::RenameActiveFile)->SetEnabledCheck(this, &CIDEFileRename::CanRenameActiveFile);
	pCommands->Add("Explorer.RenameFile", "Rename...", NULL, Ptr<CImageCel>(), this, &CIDEFileRename::RenameExplorerFile)->SetEnabledCheck(this, &CIDEFileRename::CanRenameExplorerFile);
	pCommands->Add("Document.RenameFile", "Rename File...", NULL, Ptr<CImageCel>(), this, &CIDEFileRename::RenameMenuFile)->SetEnabledCheck(this, &CIDEFileRename::CanRenameMenuFile);
}


//////////////////////////////////////////////////////////////////////////
//
// The documents' tab menu's Rename File (see CDockManagerListener::
// DocumentMenuCreated).
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::AddDocumentItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("Document.RenameFile"));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Rename File can rename the file: it is one of the solution's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::IsRenamable(const char* szFileName)
{
	return (szFileName != NULL) && (szFileName[0] != '\0') && (mpcContent->GetCodeModel()->FindProjectOfFile(szFileName) != MAX_SIZE);
}


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer's selected file, if it is one Rename File can rename.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::GetExplorerFile(CChars* pszDest)
{
	pszDest->Init();
	if (!mpcContent->GetSolutions()->GetExplorer()->GetSelectedFilePath(pszDest) || !IsRenamable(pszDest->Text()))
	{
		pszDest->Kill();
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The file of the document whose tab menu was opened last.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::GetMenuFile(CChars* pszDest)
{
	Ptr<CDockPane>	pPane;
	size			uiPrefix;

	pPane = mpcContent->GetDock()->GetMenuPane();
	uiPrefix = strlen(IDE_DOCUMENT_PREFIX);
	if (pPane.IsNull() || (strncmp(pPane->GetID(), IDE_DOCUMENT_PREFIX, uiPrefix) != 0) || !IsRenamable(pPane->GetID() + uiPrefix))
	{
		return false;
	}
	pszDest->Init(pPane->GetID() + uiPrefix);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::RenameActiveFile(void)
{
	if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
	{
		QueueRenameFile(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	}
}


void CIDEFileRename::RenameExplorerFile(void)
{
	CChars	szFileName;

	if (GetExplorerFile(&szFileName))
	{
		QueueRenameFile(szFileName.Text());
		szFileName.Kill();
	}
}


void CIDEFileRename::RenameMenuFile(void)
{
	CChars	szFileName;

	if (GetMenuFile(&szFileName))
	{
		QueueRenameFile(szFileName.Text());
		szFileName.Kill();
	}
}


bool CIDEFileRename::CanRenameActiveFile(void)
{
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && IsRenamable(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
}


bool CIDEFileRename::CanRenameExplorerFile(void)
{
	CChars	szFileName;

	if (!GetExplorerFile(&szFileName))
	{
		return false;
	}
	szFileName.Kill();
	return true;
}


bool CIDEFileRename::CanRenameMenuFile(void)
{
	CChars	szFileName;

	if (!GetMenuFile(&szFileName))
	{
		return false;
	}
	szFileName.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename File is shown at the next tick: ShowModal can't be called from
// the input (a key or a menu click) that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::QueueRenameFile(const char* szFileName)
{
	mszFileName.Set(szFileName);
	mpcContent->GetWindow()->Post(this, &CIDEFileRename::ShowQueuedRenameFile);
}


void CIDEFileRename::ShowQueuedRenameFile(void)
{
	CChars	szFileName;

	szFileName.Init(mszFileName);
	ShowRenameFile(szFileName.Text());
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Rename of a file: the new name asked for, its stem selected,
// with ticks for its related files (listed), and for searching comments
// and strings and the projects' text files for its old name; then the
// files renamed (see RenameFiles).  The ticks are remembered.  False if
// nothing was renamed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::ShowRenameFile(const char* szFileName)
{
	CFileUtil				cFileUtil;
	CChars					szName;
	CChars					szDirectory;
	CChars					szPrompt;
	CChars					szRelated;
	CChars					szRelatedName;
	CChars					szNewName;
	CArrayChars				aszRelated;
	CArrayIDEFileRenames	asRenames;
	size					i;
	bool					bResult;
	bool					bRelated;
	Ptr<CLabel>				pRelated;

	if (!IsRenamable(szFileName))
	{
		szPrompt.Init("Can't rename ");
		szPrompt.Append(szFileName);
		szPrompt.Append(": it isn't one of the solution's files.");
		mpcContent->ShowStatus(szPrompt.Text());
		szPrompt.Kill();
		return false;
	}
	mszFileName.Set(szFileName);
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);

	szPrompt.Init("Rename file '");
	szPrompt.Append(szName);
	szPrompt.Append("' to:");
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Rename File");
	mpNameField = mpDialog->AddTextField(szPrompt.Text(), szName.Text());
	mpNameField->Select(0, GetIDEStemLength(szName.Text()));

	aszRelated.Init();
	FindRelatedFiles(szFileName, &aszRelated);
	mpRelatedBox = NULL;
	if (aszRelated.NumElements() > 0)
	{
		szRelated.Init();
		for (i = 0; i < aszRelated.NumElements(); i++)
		{
			szRelatedName.Init();
			cFileUtil.GetFileName(&szRelatedName, aszRelated.GetText(i));
			szRelated.Append(i > 0 ? ", " : "");
			szRelated.Append(szRelatedName);
			szRelatedName.Kill();
		}
		mpRelatedBox = mpDialog->AddCheckBox("Rename related files", mbRelated);
		pRelated = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), szRelated.Text());
		pRelated->SetAlignment(CSH_Left, CSV_Center);
		mpDialog->AddEditor("", pRelated);
		pRelated = NULL;
		szRelated.Kill();
	}
	aszRelated.Kill();
	mpCommentsBox = mpDialog->AddCheckBox("Search in comments and strings", mbComments);
	mpTextBox = mpDialog->AddCheckBox("Search for text occurrences", mbText);
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEFileRename::ApproveRenameFile);
	szPrompt.Kill();

	bResult = false;
	mpcContent->GetScript()->WatchDialog(mpDialog);
	if (mpDialog->Show(560))
	{
		bRelated = mpRelatedBox.IsNotNull() ? mpRelatedBox->IsTicked() : mbRelated;
		if ((bRelated != mbRelated) || (mpCommentsBox->IsTicked() != mbComments) || (mpTextBox->IsTicked() != mbText))
		{
			mbRelated = bRelated;
			mbComments = mpCommentsBox->IsTicked();
			mbText = mpTextBox->IsTicked();
			mpcContent->GetSettings()->WriteSettings();
		}
		szNewName.Init(mpNameField->GetText());
		asRenames.Init();
		AddRenames(szFileName, szNewName.Text(), mpRelatedBox.IsNotNull() && bRelated, &asRenames);
		bResult = RenameFiles(&asRenames, mbComments, mbText);
		KillIDEFileRenames(&asRenames);
		szNewName.Kill();
	}
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpNameField = NULL;
	mpRelatedBox = NULL;
	mpCommentsBox = NULL;
	mpTextBox = NULL;
	mpError = NULL;
	mpDialog = NULL;
	szDirectory.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The new name, with the related files if they are ticked, must be one
// the files can have (see FindProblem).  The dialog says why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileRename::ApproveRenameFile(void)
{
	CArrayIDEFileRenames	asRenames;
	CChars					szProblem;
	bool					bProblem;

	asRenames.Init();
	AddRenames(mszFileName.Text(), mpNameField->GetText(), mpRelatedBox.IsNotNull() && mpRelatedBox->IsTicked(), &asRenames);
	szProblem.Init();
	bProblem = FindProblem(&asRenames, &szProblem);
	if (bProblem)
	{
		if ((szProblem.Text()[0] >= 'a') && (szProblem.Text()[0] <= 'z'))
		{
			szProblem.Text()[0] -= 'a' - 'A';
		}
		szProblem.Append('.');
	}
	mpError->SetText(szProblem.Text());
	szProblem.Kill();
	KillIDEFileRenames(&asRenames);
	return !bProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename File's searches, as the script's renamefilesearch step sets
// them.  Not written to the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::SetSearches(bool bComments, bool bText)
{
	mbComments = bComments;
	mbText = bText;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename File's part of how the IDE was left, in the settings' file (see
// CIDESettings): its ticks, <RenameFile RenameRelated="true" .../>.  One
// the file doesn't say stays as it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileRename::ReadSettings(CMarkupTag* pcSettings)
{
	static const char*	aszAttributes[] = { IDE_RENAME_FILE_RELATED_ATTRIBUTE, IDE_RENAME_FILE_COMMENTS_ATTRIBUTE, IDE_RENAME_FILE_TEXT_ATTRIBUTE };
	bool*				apbValues[] = { &mbRelated, &mbComments, &mbText };
	CMarkupTag*			pcTag;
	char*				szValue;
	size				i;

	pcTag = pcSettings->GetTag(IDE_RENAME_FILE_SETTINGS_TAG);
	if (pcTag == NULL)
	{
		return;
	}
	for (i = 0; i < 3; i++)
	{
		szValue = pcTag->GetStringAttribute(aszAttributes[i]);
		if (szValue)
		{
			*apbValues[i] = (StringCompare(szValue, "true") == 0);
		}
	}
}


void CIDEFileRename::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin(IDE_RENAME_FILE_SETTINGS_TAG);
	pcWriter->Attribute(IDE_RENAME_FILE_RELATED_ATTRIBUTE, mbRelated);
	pcWriter->Attribute(IDE_RENAME_FILE_COMMENTS_ATTRIBUTE, mbComments);
	pcWriter->Attribute(IDE_RENAME_FILE_TEXT_ATTRIBUTE, mbText);
	pcWriter->End();
}
