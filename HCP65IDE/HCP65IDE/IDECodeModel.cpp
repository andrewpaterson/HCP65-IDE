/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/Timer.h"
#include "CppCompilerLib/CppParser.h"
#include "IDETemplateVariables.h"
#include "IDECodeModel.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDELocations(CArrayIDELocations* pasLocations)
{
	size	i;

	for (i = 0; i < pasLocations->NumElements(); i++)
	{
		pasLocations->Get(i)->szFileName.Kill();
	}
	pasLocations->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static int CompareIDELocations(const void* pvLeft, const void* pvRight)
{
	SIDELocation*	psLeft;
	SIDELocation*	psRight;
	int				iCompare;

	psLeft = (SIDELocation*)pvLeft;
	psRight = (SIDELocation*)pvRight;
	iCompare = StringCompare(psLeft->szFileName.Text(), psRight->szFileName.Text());
	if (iCompare != 0)
	{
		return iCompare;
	}
	if (psLeft->iLine != psRight->iLine)
	{
		return (psLeft->iLine < psRight->iLine) ? -1 : 1;
	}
	if (psLeft->iColumn != psRight->iColumn)
	{
		return (psLeft->iColumn < psRight->iColumn) ? -1 : 1;
	}
	return (int)psLeft->uiRole - (int)psRight->uiRole;
}


//////////////////////////////////////////////////////////////////////////
//
// Places put in the order of their files' names, and in a file of where
// they are.  The index has a symbol's places in the order its units were
// indexed, which is whichever worker thread finished first: what is shown
// to the user from them, one after another, is sorted first, so that it
// is the same each time.
//
//////////////////////////////////////////////////////////////////////////
void SortIDELocations(CArrayIDELocations* pasLocations)
{
	pasLocations->QuickSort(&CompareIDELocations);
}


//////////////////////////////////////////////////////////////////////////
//
// psSource's place given to psDest with its file's name, which psSource
// is left without: it is still to be killed, as it would have been.
//
//////////////////////////////////////////////////////////////////////////
void MoveIDELocation(SIDELocation* psDest, SIDELocation* psSource)
{
	memcpy(psDest, psSource, sizeof(SIDELocation));
	psSource->szFileName.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// The file's name in its path: what is after the last separator, of
// either kind.
//
//////////////////////////////////////////////////////////////////////////
const char* GetIDEFileNamePart(const char* szPath)
{
	const char*	szName;
	const char*	szPosition;

	szName = szPath;
	for (szPosition = szPath; *szPosition != '\0'; szPosition++)
	{
		if ((*szPosition == '/') || (*szPosition == '\\'))
		{
			szName = szPosition + 1;
		}
	}
	return szName;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's index (CWorkspaceIndex), and what the IDE adds to it:
// the projects' files, and the edits thread.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Init(CBuildWorkspace* pcWorkspace)
{
	CWorkspaceIndex::Init(pcWorkspace);
	mcProjectFiles.Init();
	mbProjectFiles = false;

	mbEdits = false;
	mapsEdited.Init();
	mbEditsBusy = false;
	miEditsDelay = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Waits for the indexing and the edits thread to finish first.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Kill(void)
{
	KillJobs();
	if (mbEdits)
	{
		mcEdits.Kill();
		mbEdits = false;
	}
	mapsEdited.Kill();
	mcProjectFiles.Kill();
	CWorkspaceIndex::Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The edits thread uses the projects and the jobs (GetIncluder) too: it is
// waited for, and with bForget what it made is forgotten, as it is of the
// projects and the index as they were.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::WaitForProjectUsers(bool bForget)
{
	WaitForEditedFiles();
	if (bForget)
	{
		KillIDEEditedFiles(&mapsEdited);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName (a full path) preprocessed and parsed as a file of the
// solution's project uiProject, and its highlights put in pacDest.  A
// header is highlighted from a unit that includes it, if one has been
// indexed.  The
// preprocessor's errors are appended to pszErrors.  False if the project
// isn't loaded; a file with errors is still highlighted as far as it
// could be parsed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::Highlight(size uiProject, const char* szFileName, const char* szContents, CArrayHighlights* pacDest, CChars* pszErrors)
{
	CParsedUnit			cUnit;
	SWorkspaceIndexJob*	psIncluder;
	CSourceFile*		pcFile;

	//A header is parsed as part of a unit that includes it, with what comes before it there.  Until a unit that
	//includes it has been indexed, or while it is being edited (szContents, the editor's text), it is parsed on its
	//own.
	cUnit.Init();
	pcFile = NULL;
	psIncluder = szContents ? NULL : FindIncluder(szFileName);
	if (psIncluder && ParseUnit(psIncluder->uiProject, psIncluder->szFileName.Text(), NULL, &cUnit, pszErrors))
	{
		pcFile = FindFileInUnit(&cUnit, szFileName);
		if (pcFile == NULL)
		{
			cUnit.Kill();
			cUnit.Init();
		}
	}
	if (pcFile == NULL)
	{
		if (!ParseUnit(uiProject, szFileName, szContents, &cUnit, pszErrors))
		{
			cUnit.Kill();
			return false;
		}
		pcFile = cUnit.mpcUnit;
	}

	HighlightFile(&cUnit, pcFile, pacDest, NULL);
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The highlights of pcFile (the unit's own file or a header it included)
// added to pacDest, and its syntax ranges to pasRanges unless it is NULL.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::HighlightFile(CParsedUnit* pcUnit, CSourceFile* pcFile, CArrayHighlights* pacDest, CArrayCPPSyntaxRanges* pasRanges)
{
	CCPPHighlighter	cHighlighter;
	size			i;

	cHighlighter.Init();
	cHighlighter.Highlight(pcFile, pcUnit->mpcUnit->GetTokens(), &pcUnit->mcTokeniser, &pcUnit->mcTree);
	for (i = 0; i < cHighlighter.NumHighlights(); i++)
	{
		pacDest->Add(cHighlighter.Get(i));
	}
	if (pasRanges)
	{
		cHighlighter.FindSyntaxRanges(pcFile, &pcUnit->mcTokeniser, &pcUnit->mcTree, pasRanges);
	}
	cHighlighter.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The file a parsed unit's tokens came from that is named szFileName,
// NULL if none of its tokens did.
//
//////////////////////////////////////////////////////////////////////////
CSourceFile* CIDECodeModel::FindFileInUnit(CParsedUnit* pcUnit, const char* szFileName)
{
	size				i;
	SPreprocessedToken*	psSource;
	CSourceFile*		pcLast;

	pcLast = NULL;
	for (i = 0; i < pcUnit->mcTokeniser.NumTokens(); i++)
	{
		psSource = pcUnit->mcTokeniser.GetSource(pcUnit->mcTokeniser.Get(i));
		if (psSource->pcFile && (psSource->pcFile != pcLast))
		{
			pcLast = psSource->pcFile;
			if (StringCompare(pcLast->FullName(), szFileName) == 0)
			{
				return pcLast;
			}
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a symbol is defined, or declared if no file indexed so far
// defines it.  NULL if it is only used.
//
//////////////////////////////////////////////////////////////////////////
static SCPPIndexLocation* GetIDESymbolPlace(CCPPIndexSymbol* pcSymbol)
{
	SCPPIndexLocation*	psLocation;

	psLocation = pcSymbol->GetDefinition();
	return psLocation ? psLocation : pcSymbol->GetDeclaration();
}


//////////////////////////////////////////////////////////////////////////
//
// Where what is named at a place (0 based) is defined, or declared if
// no file indexed so far defines it.  False if nothing indexed is named
// there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindDefinition(const char* szFileName, int iLine, int iColumn, SIDELocation* psDest)
{
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	psLocation = pcSymbol ? GetIDESymbolPlace(pcSymbol) : NULL;
	if (psLocation)
	{
		CopyLocation(psLocation, psDest);
	}
	mcIndexMutex.Unlock();
	return psLocation != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place what is named at a place is declared, defined and used,
// added to pasDest, and its key to pszKey.  False if nothing indexed is
// named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindReferences(const char* szFileName, int iLine, int iColumn, CArrayIDELocations* pasDest, CChars* pszKey)
{
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		pszKey->Append(pcSymbol->mszKey);
		CopyLocations(pcSymbol, pasDest);
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The places in szFileName that name what is named at a place in it (0
// based), as FindReferences finds them but only that file's: for an
// editor to show the usages of the symbol at its caret.  False if nothing
// indexed is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindFileReferences(const char* szFileName, int iLine, int iColumn, CArrayIDELocations* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		CopyLocationsInFile(pcSymbol, szFileName, pasDest);
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// How what is named at a place (0 based) is named there: declared,
// defined or used (ECPPIndexRole).  False if nothing indexed is named
// there.  Nothing is copied, so it can be asked whenever a command wants
// to know if it has a symbol to work on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindRoleAt(const char* szFileName, int iLine, int iColumn, uint8* puiRole)
{
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;
	size				i;
	bool				bFound;

	bFound = false;
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		for (i = 0; (i < pcSymbol->NumLocations()) && !bFound; i++)
		{
			psLocation = pcSymbol->GetLocation(i);
			if ((psLocation->iLine == iLine) && (psLocation->iColumn <= iColumn) && (iColumn < psLocation->iColumn + psLocation->iLength) &&
				(StringInsensitiveCompare(mcIndex.GetFileName(psLocation->uiFile), szFileName) == 0))
			{
				*puiRole = psLocation->uiRole;
				bFound = true;
			}
		}
	}
	mcIndexMutex.Unlock();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Call it with the index's mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::CopyLocation(SCPPIndexLocation* psLocation, SIDELocation* psDest)
{
	psDest->szFileName.Init(mcIndex.GetFileName(psLocation->uiFile));
	psDest->iLine = psLocation->iLine;
	psDest->iColumn = psLocation->iColumn;
	psDest->iLength = psLocation->iLength;
	psDest->uiRole = psLocation->uiRole;
	psDest->uiFlags = psLocation->uiFlags;
	psDest->uiUsage = psLocation->uiUsage;
	psDest->iContainer = psLocation->iContainer;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place a symbol is named added to pasDest.  Call it with the
// index's mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::CopyLocations(CCPPIndexSymbol* pcSymbol, CArrayIDELocations* pasDest)
{
	size	i;

	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		CopyLocation(pcSymbol->GetLocation(i), pasDest->Add());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol's places in szFileName, added to pasDest.  Locations are by
// the index's file number: the name is compared once for each run of them
// in a file.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::CopyLocationsInFile(CCPPIndexSymbol* pcSymbol, const char* szFileName, CArrayIDELocations* pasDest)
{
	SCPPIndexLocation*	psLocation;
	size				i;
	size				uiFile;
	bool				bInFile;

	uiFile = MAX_SIZE;
	bInFile = false;
	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if (psLocation->uiFile != uiFile)
		{
			uiFile = psLocation->uiFile;
			bInFile = StringInsensitiveCompare(mcIndex.GetFileName(uiFile), szFileName) == 0;
		}
		if (bInFile)
		{
			CopyLocation(psLocation, pasDest->Add());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The index's symbols of the member functions that override, or are
// overridden by, the virtual member function pcSymbol (not its own) added
// to paiDest: none if it isn't one or has none.  Call it with the index's
// mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindOverrides(CCPPIndexSymbol* pcSymbol, CArrayInt* paiDest)
{
	CArrayInt	aiHierarchy;
	size		i;

	aiHierarchy.Init();
	if ((pcSymbol->meKind == CPPSK_Function) && mcIndex.FindOverrides(pcSymbol, &aiHierarchy))
	{
		for (i = 0; i < aiHierarchy.NumElements(); i++)
		{
			if (mcIndex.GetSymbol((size)aiHierarchy.GetValue(i)) != pcSymbol)
			{
				paiDest->Add(aiHierarchy.GetValue(i));
			}
		}
	}
	aiHierarchy.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The first of the workspace's projects that has szFileName (a full
// path, whatever its case) as an item, MAX_SIZE if none has.  The items'
// paths are worked out once (FindProjectFiles): finding each on the disk,
// for its case, took Find Usages 95ms a usage.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindProjectOfFile(const char* szFileName)
{
	int32*	piProject;

	FindProjectFiles();
	piProject = mcProjectFiles.Get(szFileName);
	return piProject ? (size)*piProject : MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's projects' files, each once by its full path, in the
// projects' order: what Go to File searches, what Go to Class and Go to
// Symbol show symbols from, and what Rename can edit.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::NumProjectFiles(void)
{
	FindProjectFiles();
	return mcProjectFiles.NumElements();
}


const char* CIDECodeModel::GetProjectFile(size uiIndex)
{
	FindProjectFiles();
	return mcProjectFiles.GetKey(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The full paths of the files called szName (a file name, whatever its
// case) that the projects have as items, then those the index has (headers
// they include), each once, appended to paszDest.  How many there are.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindFilesNamed(const char* szName, CArrayChars* paszDest)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	CChars		szWanted;
	size		uiFound;
	size		i;
	size		j;
	bool		bHave;
	bool		bPath;
	bool		bFound;
	const char*	szPath;

	//A name with some of its path ("Sub/User.h", or all of it) is the files whose paths end with it.
	bPath = (strchr(szName, '/') != NULL) || (strchr(szName, '\\') != NULL);
	szWanted.Init(szName);
	szWanted.Replace('\\', '/');
	FindProjectFiles();
	uiFound = 0;
	mcIndexMutex.Lock();
	for (i = 0; i < mcProjectFiles.NumElements() + mcIndex.NumFiles(); i++)
	{
		szPath = (i < mcProjectFiles.NumElements()) ? mcProjectFiles.GetKey(i) : mcIndex.GetFileName(i - mcProjectFiles.NumElements());
		szFile.Init();
		if (bPath)
		{
			szFile.Set(szPath);
			szFile.Replace('\\', '/');
			bFound = szFile.EqualsIgnoreCase(szWanted.Text()) || ((szFile.Length() > szWanted.Length()) && (szFile.Text()[szFile.Length() - szWanted.Length() - 1] == '/') &&
																  (StringInsensitiveCompare(&szFile.Text()[szFile.Length() - szWanted.Length()], szWanted.Text()) == 0));
		}
		else
		{
			cFileUtil.GetFileName(&szFile, szPath);
			bFound = szFile.EqualsIgnoreCase(szName);
		}
		if (bFound)
		{
			bHave = false;
			for (j = paszDest->NumElements() - uiFound; j < paszDest->NumElements(); j++)
			{
				bHave = bHave || paszDest->Get(j)->EqualsIgnoreCase(szPath);
			}
			if (!bHave)
			{
				paszDest->Add(szPath);
				uiFound++;
			}
		}
		szFile.Kill();
	}
	mcIndexMutex.Unlock();
	szWanted.Kill();
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Every item of the workspace's projects by its full path, to the first
// project that has it, found if they haven't been since the workspace
// changed (ForgetProjectFiles).
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindProjectFiles(void)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CChars			szPath;
	int32*			piProject;
	bool			bAdded;

	if (mbProjectFiles)
	{
		return;
	}
	mbProjectFiles = true;
	mcProjectFiles.ReInit();
	for (i = 0; i < mpcWorkspace->NumProjects(); i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
		if (pcProject == NULL)
		{
			continue;
		}
		for (j = 0; j < pcProject->NumItems(); j++)
		{
			szPath.Init();
			pcProject->GetFullPath(pcProject->GetItem(j), &szPath);
			piProject = mcProjectFiles.FindOrAdd(szPath.Text(), szPath.Length(), &bAdded);
			if (bAdded && piProject)
			{
				*piProject = (int32)i;
			}
			szPath.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The projects' items are found again the next time they are wanted: the
// workspace's solutions, or a project's items, have changed.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::ForgetProjectFiles(void)
{
	mbProjectFiles = false;
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is being indexed and the projects will be made again
// (CWorkspaceIndex::Reset), and their items found again: before the
// workspace's solutions change, as the jobs use them.  StartIndexing
// indexes them again.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Reset(void)
{
	CWorkspaceIndex::Reset();
	ForgetProjectFiles();
}


//////////////////////////////////////////////////////////////////////////
//
// The website the workspace's pages are part of: an mkdocs.yml item of a
// project (CWorkspaceIndex::FindDocsSite), else the first a project's
// properties name (Properties... in Solution Explorer) that is there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindDocsSite(CChars* pszMkdocsFile)
{
	size			i;
	CVcxProject*	pcProject;
	CFileUtil		cFileUtil;
	CChars			szSite;
	bool			bFound;

	if (CWorkspaceIndex::FindDocsSite(pszMkdocsFile))
	{
		return true;
	}
	bFound = false;
	szSite.Init();
	for (i = 0; (i < mpcWorkspace->NumProjects()) && !bFound; i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
		szSite.Clear();
		if ((pcProject != NULL) && (ReadIDEProjectDocsSite(pcProject->GetFileName(), &szSite)[0] != '\0'))
		{
			pszMkdocsFile->Set(pcProject->GetDirectory());
			cFileUtil.AppendToPath(pszMkdocsFile, szSite.Text());
			cFileUtil.FixSeparators(pszMkdocsFile);
			cFileUtil.FullPath(pszMkdocsFile);
			bFound = cFileUtil.Exists(pszMkdocsFile->Text());
		}
	}
	szSite.Kill();
	if (!bFound)
	{
		pszMkdocsFile->Clear();
	}
	return bFound;
}


void CIDECodeModel::SetEditsDelay(int iMilliseconds) { miEditsDelay = iMilliseconds; }


//////////////////////////////////////////////////////////////////////////
//
// The kind of icon a symbol of eKind has, IST_Count for one Go to Symbol
// doesn't list (parameters, labels ...).
//
//////////////////////////////////////////////////////////////////////////
static EIDESymbolType GetIDESymbolType(ECPPSymbolKind eKind)
{
	switch (eKind)
	{
	case CPPSK_Class:
	case CPPSK_ClassTemplate:
		return IST_Class;
	case CPPSK_Enum:
		return IST_Enum;
	case CPPSK_Typedef:
	case CPPSK_AliasTemplate:
		return IST_Typedef;
	case CPPSK_Namespace:
		return IST_Namespace;
	case CPPSK_Function:
	case CPPSK_FunctionTemplate:
		return IST_Function;
	case CPPSK_Variable:
	case CPPSK_Field:
	case CPPSK_Enumerator:
		return IST_Variable;
	default:
		return IST_Count;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The EIDESymbolType of the icon a symbol of kind uiKind (ECPPSymbolKind)
// has, -1 for a kind without one.
//
//////////////////////////////////////////////////////////////////////////
int GetIDESymbolIconType(uint8 uiKind)
{
	EIDESymbolType	eType;

	eType = GetIDESymbolType((ECPPSymbolKind)uiKind);
	return (eType == IST_Count) ? -1 : (int)eType;
}


//////////////////////////////////////////////////////////////////////////
//
// Splits a symbol's key, "CChars::Append(char const*)", into where it is
// declared ("CChars", "" at file scope), its name ("Append") and its
// parameters ("(char const*)", "" if it isn't a function).
//
//////////////////////////////////////////////////////////////////////////
static void SplitIDESymbolKey(const char* szKey, CChars* pszScope, CChars* pszName, CChars* pszParameters)
{
	size	uiLength;
	size	uiClose;
	size	uiEnd;
	size	uiStart;
	int		iDepth;

	uiLength = strlen(szKey);
	uiEnd = uiLength;

	//The parameters are the brackets the key ends with, and what a member function has after them (" const").
	for (uiClose = uiLength; (uiClose > 0) && ((szKey[uiClose - 1] == ' ') || ((szKey[uiClose - 1] >= 'a') && (szKey[uiClose - 1] <= 'z'))); uiClose--)
	{
	}
	if ((uiClose == 0) || (szKey[uiClose - 1] != ')'))
	{
		uiClose = uiLength;
	}
	if ((uiClose > 0) && (szKey[uiClose - 1] == ')'))
	{
		iDepth = 0;
		for (uiEnd = uiClose; uiEnd > 0; uiEnd--)
		{
			if (szKey[uiEnd - 1] == ')')
			{
				iDepth++;
			}
			else if (szKey[uiEnd - 1] == '(')
			{
				iDepth--;
				if (iDepth == 0)
				{
					uiEnd--;
					break;
				}
			}
		}
	}
	pszParameters->Init();
	pszParameters->AppendSubString(szKey, uiEnd, uiLength);

	//The name is after the last "::" outside a template's angle brackets.
	iDepth = 0;
	for (uiStart = uiEnd; uiStart > 0; uiStart--)
	{
		if (szKey[uiStart - 1] == '>')
		{
			iDepth++;
		}
		else if ((szKey[uiStart - 1] == '<') && (iDepth > 0))
		{
			iDepth--;
		}
		else if ((iDepth == 0) && (uiStart >= 2) && (szKey[uiStart - 1] == ':') && (szKey[uiStart - 2] == ':'))
		{
			break;
		}
	}
	pszName->Init();
	pszName->AppendSubString(szKey, uiStart, uiEnd);
	pszScope->Init();
	if (uiStart >= 2)
	{
		pszScope->AppendSubString(szKey, 0, uiStart - 2);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Class's results (classes and enums) or, without bClasses, Go to
// Symbol's (every named thing but parameters, labels and locals): those
// whose name the pattern matches, declared in one of the projects' files.
// Each is shown as its name with its parameters,
// where it's declared and its file; its data is its symbol's index.
// aiIcons are the dialog's icons by EIDESymbolType.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindGoToSymbols(CNameMatcher* pcMatcher, bool bClasses, int32* aiIcons, CGoToItems* pcDest)
{
	size				i;
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;
	EIDESymbolType		eType;
	CChars				szScope;
	CChars				szName;
	CChars				szParameters;
	CChars				szLocation;
	SNameMatch			sMatch;
	const char*			szFile;

	FindProjectFiles();
	mcIndexMutex.Lock();
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		eType = GetIDESymbolType(pcSymbol->meKind);
		if ((eType == IST_Count) || (bClasses && (eType != IST_Class) && (eType != IST_Enum)) || pcSymbol->mszKey.Contains('@'))
		{
			continue;
		}
		psLocation = GetIDESymbolPlace(pcSymbol);
		if (psLocation == NULL)
		{
			continue;
		}
		szFile = mcIndex.GetFileName(psLocation->uiFile);
		if (!mcProjectFiles.HasKey(szFile))
		{
			continue;
		}

		SplitIDESymbolKey(pcSymbol->mszKey.Text(), &szScope, &szName, &szParameters);
		if (!szName.Empty() && pcMatcher->Match(szName.Text(), szName.Length(), pcMatcher->GetPattern(), StrLen(pcMatcher->GetPattern()), &sMatch))
		{
			szName.Append(szParameters);
			szLocation.Init(szScope);
			if (!szLocation.Empty())
			{
				szLocation.Append("  ");
			}
			szLocation.Append(GetIDEFileNamePart(szFile));
			szLocation.Append(":");
			szLocation.Append(psLocation->iLine + 1);
			pcDest->Add(szName.Text(), szLocation.Text(), aiIcons[eType], i, &sMatch);
			szLocation.Kill();
		}
		szParameters.Kill();
		szName.Kill();
		szScope.Kill();
	}
	mcIndexMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// Where symbol uiSymbol (of FindGoToSymbols) is defined, or declared.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::GetSymbolLocation(size uiSymbol, SIDELocation* psDest)
{
	SCPPIndexLocation*	psLocation;

	mcIndexMutex.Lock();
	psLocation = (uiSymbol < mcIndex.NumSymbols()) ? GetIDESymbolPlace(mcIndex.GetSymbol(uiSymbol)) : NULL;
	if (psLocation)
	{
		CopyLocation(psLocation, psDest);
	}
	mcIndexMutex.Unlock();
	return psLocation != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDERenameTarget::Init(void)
{
	szKey.Init();
	szName.Init();
	asLocations.Init();
	bRenamable = false;
	szWhyNot.Init();
	uiOverrides = 0;
	uiFamily = 0;
	szFamily.Init();
	szFamilyWhere.Init();
	apsWith.Init();
	bType = false;
	uiAssembly = IAR_None;
	szScope.Init();
	bCLinkage = false;
	bOverloaded = false;
	aszOverloads.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDERenameTarget::Kill(void)
{
	size	i;

	for (i = 0; i < apsWith.NumElements(); i++)
	{
		apsWith.GetPtr(i)->Kill();
		free(apsWith.GetPtr(i));
	}
	apsWith.Kill();
	szFamilyWhere.Kill();
	szFamily.Kill();
	aszOverloads.Kill();
	szScope.Kill();
	szWhyNot.Kill();
	KillIDELocations(&asLocations);
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A symbol's name as written: a local's is after its key's last colon
// ("@A.cpp:6:12:iCount"), anything else's is after its scope.
//
//////////////////////////////////////////////////////////////////////////
void GetIDESymbolName(const char* szKey, CChars* pszDest)
{
	CChars		szScope;
	CChars		szParameters;
	const char*	szColon;

	if (szKey[0] == '@')
	{
		szColon = strrchr(szKey, ':');
		pszDest->Init(szColon ? szColon + 1 : szKey);
		return;
	}
	SplitIDESymbolKey(szKey, &szScope, pszDest, &szParameters);
	szParameters.Kill();
	szScope.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Why Rename can't change pcSymbol's places, NULL if it can: each must be
// in one of pcFiles (the solution's, what can be edited), written as the
// name (not made by a macro) and not a call that could be to another
// overload.
//
//////////////////////////////////////////////////////////////////////////
static const char* GetIDERenameProblem(CCPPIndex* pcIndex, CCPPIndexSymbol* pcSymbol, CIDEProjectFiles* pcFiles)
{
	size				i;
	SCPPIndexLocation*	psLocation;

	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if (!pcFiles->HasKey(pcIndex->GetFileName(psLocation->uiFile)))
		{
			return "it is declared or used in a file that isn't in the solution";
		}
		if (psLocation->uiFlags & CPPIF_MACRO)
		{
			return "its name is made by a macro";
		}
		if (psLocation->uiFlags & CPPIF_AMBIGUOUS)
		{
			return "a call to it can't be told from a call to another overload";
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// If a declaration of pcSymbol is inside extern "C" (CPPIF_C_LINKAGE).
//
//////////////////////////////////////////////////////////////////////////
static bool HasIDECLinkage(CCPPIndexSymbol* pcSymbol)
{
	size	i;

	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		if (pcSymbol->GetLocation(i)->uiFlags & CPPIF_C_LINKAGE)
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A symbol's name with its scopes and without its parameters, appended:
// "CFoo::Init" for the key "CFoo::Init(int)".
//
//////////////////////////////////////////////////////////////////////////
void GetIDEScopedName(const char* szKey, CChars* pszDest)
{
	CChars	szScope;
	CChars	szName;
	CChars	szParameters;

	SplitIDESymbolKey(szKey, &szScope, &szName, &szParameters);
	if (!szScope.Empty())
	{
		pszDest->Append(szScope);
		pszDest->Append("::");
	}
	pszDest->Append(szName);
	szParameters.Kill();
	szName.Kill();
	szScope.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The scope a symbol is in, appended: "NS::CFoo" for the key
// "NS::CFoo::Init(int)", nothing for one in no scope.
//
//////////////////////////////////////////////////////////////////////////
void GetIDESymbolScope(const char* szKey, CChars* pszDest)
{
	CChars	szScope;
	CChars	szName;
	CChars	szParameters;

	SplitIDESymbolKey(szKey, &szScope, &szName, &szParameters);
	pszDest->Append(szScope);
	szParameters.Kill();
	szName.Kill();
	szScope.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// How hand written assembly's labels name pcSymbol (EIDEAssemblyRename,
// RenameFile.txt's assembly step), its scope as its key has it, and
// whether it is a function overloaded in its scope (whose labels can't be
// told from its overloads' without their parameters' types).  Call it with
// the index's mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindAssemblyRename(CCPPIndexSymbol* pcSymbol, SIDERenameTarget* psDest)
{
	const char*			szKey;
	const char*			szParameters;
	const char*			szSeparator;
	const char*			szLast;
	CCPPIndexSymbol*	pcScope;
	CCPPIndexSymbol*	pcOther;
	CChars				szPrefix;
	bool				bClassScope;
	size				i;

	szKey = pcSymbol->mszKey.Text();
	psDest->uiAssembly = IAR_None;
	if (szKey[0] == '@')
	{
		return;
	}
	szParameters = strchr(szKey, '(');
	szLast = NULL;
	for (szSeparator = strstr(szKey, "::"); szSeparator && (!szParameters || (szSeparator < szParameters)); szSeparator = strstr(szSeparator + 2, "::"))
	{
		szLast = szSeparator;
	}
	if (szLast)
	{
		psDest->szScope.Set(szKey);
		psDest->szScope.SetLength((size)(szLast - szKey));
	}
	pcScope = psDest->szScope.Empty() ? NULL : mcIndex.FindSymbol(psDest->szScope.Text());
	bClassScope = pcScope && ((pcScope->meKind == CPPSK_Class) || (pcScope->meKind == CPPSK_ClassTemplate));

	if ((pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_Enum) || (pcSymbol->meKind == CPPSK_ClassTemplate))
	{
		psDest->uiAssembly = IAR_Type;
	}
	else if (pcSymbol->meKind == CPPSK_Namespace)
	{
		psDest->uiAssembly = IAR_Namespace;
	}
	else if ((pcSymbol->meKind == CPPSK_Field) && bClassScope)
	{
		psDest->uiAssembly = IAR_Field;
	}
	else if (((pcSymbol->meKind == CPPSK_Function) || (pcSymbol->meKind == CPPSK_Variable)) && !bClassScope && HasIDECLinkage(pcSymbol))
	{
		//Declared inside extern "C": its label is its name alone, without its namespace or parameters ("_Read").
		psDest->uiAssembly = IAR_Function;
		psDest->szScope.Clear();
		psDest->bCLinkage = true;
	}
	else if ((pcSymbol->meKind == CPPSK_Function) || (pcSymbol->meKind == CPPSK_Variable))
	{
		psDest->uiAssembly = bClassScope ? IAR_Member : IAR_Function;
		if ((pcSymbol->meKind == CPPSK_Function) && szParameters)
		{
			szPrefix.Init();
			szPrefix.AppendSubString(szKey, szParameters + 1);
			for (i = 0; i < mcIndex.NumSymbols(); i++)
			{
				pcOther = mcIndex.GetSymbol(i);
				if ((pcOther->meKind == CPPSK_Function) && pcOther->mszKey.StartsWith(szPrefix.Text()))
				{
					psDest->aszOverloads.Add(pcOther->mszKey.Text());
					psDest->bOverloaded |= (pcOther != pcSymbol);
				}
			}
			if (!psDest->bOverloaded)
			{
				psDest->aszOverloads.ReInit();
			}
			szPrefix.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Rename's target at iLine, iColumn (0 based) of szFileName: the symbol
// named there and every place its name is written.  A class's
// constructors and destructors are renamed with it; a virtual member
// function's overrides are; and so is a member's family, the members of
// other classes that a template's body names with it by one name
// (CCPPIndex::FindFamily), whose overrides and families are too.  It
// can't be renamed
// (bRenamable false, szWhyNot says why) if it is an operator, a
// destructor or a compiler type, or if a place can't be changed (see
// GetIDERenameProblem: it must be in one of the projects' files).  False
// if no symbol is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindRenameTarget(const char* szFileName, int iLine, int iColumn, SIDERenameTarget* psDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcMember;
	const char*			szProblem;
	CChars				szConstructor;
	CChars				szDestructor;
	size				i;
	size				j;
	size				uiNext;
	CArrayInt			aiWith;
	CArrayInt			aiFound;
	CCPPIndexSymbol*	pcFrom;
	SCPPIndexLocation	sWhere;
	SIDERenameTarget*	psWith;
	bool				bFamily;

	FindProjectFiles();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol == NULL)
	{
		mcIndexMutex.Unlock();
		return false;
	}

	psDest->szKey.Set(pcSymbol->mszKey);
	GetIDESymbolName(pcSymbol->mszKey.Text(), &psDest->szName);
	psDest->bType = (pcSymbol->meKind == CPPSK_Namespace) || (pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_Enum) ||
					(pcSymbol->meKind == CPPSK_Typedef) || (pcSymbol->meKind == CPPSK_ClassTemplate) || (pcSymbol->meKind == CPPSK_AliasTemplate);
	FindAssemblyRename(pcSymbol, psDest);
	szProblem = NULL;
	if ((pcSymbol->meKind == CPPSK_BuiltinType) || psDest->szName.Empty())
	{
		szProblem = "it is the compiler's";
	}
	else if (psDest->szName.StartsWith("operator"))
	{
		szProblem = "it is an operator";
	}
	else if (psDest->szName.Text()[0] == '~')
	{
		szProblem = "a destructor is renamed with its class";
	}
	if (szProblem == NULL)
	{
		szProblem = GetIDERenameProblem(&mcIndex, pcSymbol, &mcProjectFiles);
	}
	CopyLocations(pcSymbol, &psDest->asLocations);

	//A class's constructors and destructor are named for it.
	if ((szProblem == NULL) && ((pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_ClassTemplate)))
	{
		szConstructor.Init(pcSymbol->mszKey);
		szConstructor.Append("::");
		szConstructor.Append(psDest->szName);
		szConstructor.Append('(');
		szDestructor.Init(pcSymbol->mszKey);
		szDestructor.Append("::~");
		szDestructor.Append(psDest->szName);
		szDestructor.Append('(');
		for (i = 0; (i < mcIndex.NumSymbols()) && (szProblem == NULL); i++)
		{
			pcMember = mcIndex.GetSymbol(i);
			if (pcMember->mszKey.StartsWith(szConstructor.Text()) || pcMember->mszKey.StartsWith(szDestructor.Text()))
			{
				szProblem = GetIDERenameProblem(&mcIndex, pcMember, &mcProjectFiles);
				CopyLocations(pcMember, &psDest->asLocations);
			}
		}
		szDestructor.Kill();
		szConstructor.Kill();
	}

	//A virtual member function is renamed with what overrides it and what it overrides.  A member a template's body
	//names is renamed with its family, the members the same name is in the template's other instances: the template is
	//written once for them all.  What is found is looked at the same way, until nothing new is.
	if (szProblem == NULL)
	{
		aiWith.Init();
		pcFrom = pcSymbol;
		uiNext = 0;
		for (;;)
		{
			for (j = 0; j < 2; j++)
			{
				bFamily = (j == 1);
				aiFound.Init();
				if (bFamily)
				{
					mcIndex.FindFamily(pcFrom, &aiFound, &sWhere);
				}
				else
				{
					FindOverrides(pcFrom, &aiFound);
				}
				for (i = 0; i < aiFound.NumElements(); i++)
				{
					pcMember = mcIndex.GetSymbol((size)aiFound.GetValue(i));
					if ((pcMember == pcSymbol) || (aiWith.Find(aiFound.GetValue(i)) != ARRAY_ELEMENT_NOT_FOUND))
					{
						continue;
					}
					aiWith.Add(aiFound.GetValue(i));
					if (!bFamily)
					{
						psDest->uiOverrides++;
						continue;
					}
					if ((psDest->uiFamily == 0) && (sWhere.iContainer >= 0))
					{
						GetIDEScopedName(mcIndex.GetSymbol((size)sWhere.iContainer)->mszKey.Text(), &psDest->szFamilyWhere);
					}
					psDest->szFamily.Append((psDest->uiFamily > 0) ? ", " : "");
					GetIDEScopedName(pcMember->mszKey.Text(), &psDest->szFamily);
					psDest->uiFamily++;
				}
				aiFound.Kill();
			}
			if (uiNext == aiWith.NumElements())
			{
				break;
			}
			pcFrom = mcIndex.GetSymbol((size)aiWith.GetValue(uiNext));
			uiNext++;
		}

		for (i = 0; i < aiWith.NumElements(); i++)
		{
			pcMember = mcIndex.GetSymbol((size)aiWith.GetValue(i));
			if (szProblem == NULL)
			{
				szProblem = GetIDERenameProblem(&mcIndex, pcMember, &mcProjectFiles);
			}
			CopyLocations(pcMember, &psDest->asLocations);

			psWith = (SIDERenameTarget*)malloc(sizeof(SIDERenameTarget));
			psWith->Init();
			psWith->szKey.Set(pcMember->mszKey);
			GetIDESymbolName(pcMember->mszKey.Text(), &psWith->szName);
			FindAssemblyRename(pcMember, psWith);
			psDest->apsWith.Add(psWith);
		}
		aiWith.Kill();
	}
	mcIndexMutex.Unlock();

	psDest->bRenamable = (szProblem == NULL);
	if (szProblem)
	{
		psDest->szWhyNot.Set(szProblem);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The places of the member functions that override, or are overridden
// by, the virtual member function szKey (not its own) added to pasDest.
// How many functions they are, 0 if szKey isn't a virtual function or
// has no overrides.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindOverrideLocations(const char* szKey, CArrayIDELocations* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CArrayInt			aiOverrides;
	size				i;
	size				uiOverrides;

	aiOverrides.Init();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	if (pcSymbol)
	{
		FindOverrides(pcSymbol, &aiOverrides);
		for (i = 0; i < aiOverrides.NumElements(); i++)
		{
			CopyLocations(mcIndex.GetSymbol((size)aiOverrides.GetValue(i)), pasDest);
		}
	}
	mcIndexMutex.Unlock();
	uiOverrides = aiOverrides.NumElements();
	aiOverrides.Kill();
	return uiOverrides;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFileRange::Init(const char* szFile, int32 iStartLine, int32 iStartColumn, int32 iLastLine, int32 iLastColumn)
{
	szFileName.Init(szFile);
	iLine = iStartLine;
	iColumn = iStartColumn;
	iEndLine = iLastLine;
	iEndColumn = iLastColumn;
	uiStart = 0;
	uiEnd = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFileRange::Kill(void)
{
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a place in szFile is in the range.
//
//////////////////////////////////////////////////////////////////////////
bool SIDEFileRange::Has(const char* szFile, int32 iAtLine, int32 iAtColumn)
{
	if ((iAtLine < iLine) || ((iAtLine == iLine) && (iAtColumn < iColumn)))
	{
		return false;
	}
	if ((iAtLine > iEndLine) || ((iAtLine == iEndLine) && (iAtColumn >= iEndColumn)))
	{
		return false;
	}
	return StringInsensitiveCompare(szFileName.Text(), szFile) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDEFileRanges(CArrayIDEFileRanges* pasRanges)
{
	size	i;

	for (i = 0; i < pasRanges->NumElements(); i++)
	{
		pasRanges->Get(i)->Kill();
	}
	pasRanges->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol named at a place (0 based) of szFileName: its key and its
// kind (ECPPSymbolKind).  False if none is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::GetSymbolAt(const char* szFileName, int iLine, int iColumn, CChars* pszKey, uint8* puiKind)
{
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		pszKey->Set(pcSymbol->mszKey);
		*puiKind = (uint8)pcSymbol->meKind;
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place the symbol szKey is named, added to pasDest, and its kind.
// False if the index has no such symbol.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::GetSymbolLocations(const char* szKey, CArrayIDELocations* pasDest, uint8* puiKind)
{
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	if (pcSymbol)
	{
		CopyLocations(pcSymbol, pasDest);
		*puiKind = (uint8)pcSymbol->meKind;
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The symbols with a declaration or a definition in one of the ranges,
// what is taken out with them: a class's members, an enum's enumerators,
// a function's parameters and locals.  Their keys are added to paszKeys,
// those not there already; the number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindSymbolsDeclaredIn(CArrayIDEFileRanges* pasRanges, CArrayChars* paszKeys)
{
	return FindSymbolsIn(pasRanges, paszKeys, false);
}


//////////////////////////////////////////////////////////////////////////
//
// The symbols named in one of the ranges without being declared there:
// what the text in them needs.  Their keys are added to paszKeys, those
// not there already; the number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindSymbolsUsedIn(CArrayIDEFileRanges* pasRanges, CArrayChars* paszKeys)
{
	return FindSymbolsIn(pasRanges, paszKeys, true);
}


//////////////////////////////////////////////////////////////////////////
//
// A class's base classes (all of them, however far up, or only those it
// names itself), their keys added to paszKeys nearest first; the number.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindClassBases(const char* szKey, bool bAll, CArrayChars* paszKeys)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcBase;
	size				uiFirst;
	size				uiNext;
	size				i;

	uiFirst = paszKeys->NumElements();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	uiNext = uiFirst;
	while (pcSymbol)
	{
		for (i = 0; i < pcSymbol->maiBases.NumElements(); i++)
		{
			pcBase = mcIndex.GetSymbol((size)pcSymbol->maiBases.GetValue(i));
			paszKeys->AddIfUnique(pcBase->mszKey.Text());
		}
		pcSymbol = (bAll && (uiNext < paszKeys->NumElements())) ? mcIndex.FindSymbol(paszKeys->GetText(uiNext)) : NULL;
		uiNext++;
	}
	mcIndexMutex.Unlock();
	return paszKeys->NumElements() - uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// The classes that have a class as a base (all of them, however far
// down, or only those that name it), their keys added to paszKeys; the
// number.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindClassDerived(const char* szKey, bool bAll, CArrayChars* paszKeys)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcDerived;
	size				uiFirst;
	size				uiNext;
	size				i;

	uiFirst = paszKeys->NumElements();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	uiNext = uiFirst;
	while (pcSymbol)
	{
		for (i = 0; i < pcSymbol->maiDerived.NumElements(); i++)
		{
			pcDerived = mcIndex.GetSymbol((size)pcSymbol->maiDerived.GetValue(i));
			paszKeys->AddIfUnique(pcDerived->mszKey.Text());
		}
		pcSymbol = (bAll && (uiNext < paszKeys->NumElements())) ? mcIndex.FindSymbol(paszKeys->GetText(uiNext)) : NULL;
		uiNext++;
	}
	mcIndexMutex.Unlock();
	return paszKeys->NumElements() - uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// The key and kind of the index's symbol iSymbol (a location's
// iContainer, say).  False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::GetSymbolKey(int32 iSymbol, CChars* pszKey, uint8* puiKind)
{
	CCPPIndexSymbol*	pcSymbol;
	bool				bFound;

	mcIndexMutex.Lock();
	bFound = (iSymbol >= 0) && ((size)iSymbol < mcIndex.NumSymbols());
	if (bFound)
	{
		pcSymbol = mcIndex.GetSymbol((size)iSymbol);
		pszKey->Set(pcSymbol->mszKey);
		*puiKind = (uint8)pcSymbol->meKind;
	}
	mcIndexMutex.Unlock();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The index's number for the symbol szKey (what GetSymbolLocation and a
// Go to item's data take), -1 if it hasn't it.  It is the symbol's for as
// long as the index isn't changed.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDECodeModel::FindSymbolIndex(const char* szKey)
{
	CCPPIndexSymbol*	pcSymbol;
	size				i;
	int32				iSymbol;

	iSymbol = -1;
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	for (i = 0; pcSymbol && (i < mcIndex.NumSymbols()) && (iSymbol < 0); i++)
	{
		if (mcIndex.GetSymbol(i) == pcSymbol)
		{
			iSymbol = (int32)i;
		}
	}
	mcIndexMutex.Unlock();
	return iSymbol;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution's named namespaces' keys ("Geometry::Detail"), each once.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindNamespaces(CArrayChars* paszDest)
{
	CCPPIndexSymbol*	pcSymbol;
	size				i;

	mcIndexMutex.Lock();
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		if ((pcSymbol->meKind == CPPSK_Namespace) && !pcSymbol->mszKey.Contains('@'))
		{
			paszDest->AddIfUnique(pcSymbol->mszKey.Text());
		}
	}
	mcIndexMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// The symbols declared or defined (or, bUsed, only named) in one of the
// ranges.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindSymbolsIn(CArrayIDEFileRanges* pasRanges, CArrayChars* paszKeys, bool bUsed)
{
	CHashSetString<>	cKeys;
	CArrayInt			aiRangeFiles;
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;
	SIDEFileRange*		psRange;
	size				i;
	size				j;
	size				k;
	size				uiAdded;
	int					iFile;
	bool				bIn;
	bool				bAdded;

	cKeys.Init();
	for (i = 0; i < paszKeys->NumElements(); i++)
	{
		cKeys.FindOrAddIndex(paszKeys->GetText(i));
	}

	mcIndexMutex.Lock();
	//Each range's file as the index numbers it, -1 if the index hasn't the file.
	aiRangeFiles.Init();
	for (i = 0; i < pasRanges->NumElements(); i++)
	{
		iFile = -1;
		for (j = 0; j < mcIndex.NumFiles(); j++)
		{
			if (StringInsensitiveCompare(mcIndex.GetFileName(j), pasRanges->Get(i)->szFileName.Text()) == 0)
			{
				iFile = (int)j;
				break;
			}
		}
		aiRangeFiles.Add(iFile);
	}

	uiAdded = 0;
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		bIn = false;
		for (j = 0; (j < pcSymbol->NumLocations()) && !bIn; j++)
		{
			psLocation = pcSymbol->GetLocation(j);
			if ((psLocation->uiRole == CPPIR_Reference) != bUsed)
			{
				continue;
			}
			for (k = 0; (k < pasRanges->NumElements()) && !bIn; k++)
			{
				psRange = pasRanges->Get(k);
				if ((int)psLocation->uiFile != aiRangeFiles.GetValue(k))
				{
					continue;
				}
				bIn = !((psLocation->iLine < psRange->iLine) || ((psLocation->iLine == psRange->iLine) && (psLocation->iColumn < psRange->iColumn)) ||
						(psLocation->iLine > psRange->iEndLine) || ((psLocation->iLine == psRange->iEndLine) && (psLocation->iColumn >= psRange->iEndColumn)));
			}
		}
		if (bIn)
		{
			cKeys.FindOrAddIndex(pcSymbol->mszKey.Text(), (size)pcSymbol->mszKey.Length(), &bAdded);
			if (bAdded)
			{
				paszKeys->Add(pcSymbol->mszKey.Text());
				uiAdded++;
			}
		}
	}
	aiRangeFiles.Kill();
	mcIndexMutex.Unlock();
	cKeys.Kill();
	return uiAdded;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether pcClass is pcBase or has it for a base class, however far up.
// Call it with the index's mutex held.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::IsDerivedClass(CCPPIndexSymbol* pcClass, CCPPIndexSymbol* pcBase, int iDepth)
{
	size	i;

	if (pcClass == pcBase)
	{
		return true;
	}
	if (iDepth > 32)
	{
		return false;
	}
	for (i = 0; i < pcClass->maiBases.NumElements(); i++)
	{
		if (IsDerivedClass(mcIndex.GetSymbol((size)pcClass->maiBases.GetValue(i)), pcBase, iDepth + 1))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The declarations and definitions of the member functions that override
// the virtual member function szKey in the classes derived from its
// class, added to pasDest: what is left overriding nothing if it is taken
// out.  The number of functions; 0 if it isn't virtual or nothing
// overrides it.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindOverriders(const char* szKey, CArrayIDELocations* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcClass;
	CCPPIndexSymbol*	pcOther;
	CCPPIndexSymbol*	pcOtherClass;
	CArrayInt			aiOverrides;
	CChars				szScope;
	CChars				szName;
	CChars				szParameters;
	size				i;
	size				j;
	size				uiOverriders;

	uiOverriders = 0;
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	SplitIDESymbolKey(szKey, &szScope, &szName, &szParameters);
	pcClass = (pcSymbol && !szScope.Empty()) ? mcIndex.FindSymbol(szScope.Text()) : NULL;
	szParameters.Kill();
	szName.Kill();
	szScope.Kill();
	if (pcClass)
	{
		aiOverrides.Init();
		FindOverrides(pcSymbol, &aiOverrides);
		for (i = 0; i < aiOverrides.NumElements(); i++)
		{
			pcOther = mcIndex.GetSymbol((size)aiOverrides.GetValue(i));
			SplitIDESymbolKey(pcOther->mszKey.Text(), &szScope, &szName, &szParameters);
			pcOtherClass = szScope.Empty() ? NULL : mcIndex.FindSymbol(szScope.Text());
			szParameters.Kill();
			szName.Kill();
			szScope.Kill();
			if (pcOtherClass && (pcOtherClass != pcClass) && IsDerivedClass(pcOtherClass, pcClass, 0))
			{
				uiOverriders++;
				for (j = 0; j < pcOther->NumLocations(); j++)
				{
					if (pcOther->GetLocation(j)->uiRole != CPPIR_Reference)
					{
						CopyLocation(pcOther->GetLocation(j), pasDest->Add());
					}
				}
			}
		}
		aiOverrides.Kill();
	}
	mcIndexMutex.Unlock();
	return uiOverriders;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDETargets(CArrayIDETargets* pasTargets)
{
	size	i;

	for (i = 0; i < pasTargets->NumElements(); i++)
	{
		pasTargets->Get(i)->szKey.Kill();
		pasTargets->Get(i)->sLocation.szFileName.Kill();
	}
	pasTargets->Kill();
}


void KillIDECallRelations(CArrayIDECallRelations* pasRelations)
{
	SIDECallRelation*	psRelation;
	size				i;

	for (i = 0; i < pasRelations->NumElements(); i++)
	{
		psRelation = pasRelations->Get(i);
		KillIDELocations(&psRelation->asCalls);
		psRelation->sPlace.szFileName.Kill();
		psRelation->szKey.Kill();
	}
	pasRelations->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A call (psCall) noted against the function pcFunction in pasDest: the
// function is added, with where it is, the first time.  Called with the
// index locked.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::AddCall(CCPPIndexSymbol* pcFunction, SCPPIndexLocation* psCall, CArrayIDECallRelations* pasDest)
{
	SIDECallRelation*	psRelation;
	SCPPIndexLocation*	psPlace;
	size				i;

	psRelation = NULL;
	for (i = 0; (i < pasDest->NumElements()) && (psRelation == NULL); i++)
	{
		if (pasDest->Get(i)->szKey.Equals(pcFunction->mszKey))
		{
			psRelation = pasDest->Get(i);
		}
	}
	if (psRelation == NULL)
	{
		psRelation = pasDest->Add();
		psRelation->szKey.Init(pcFunction->mszKey);
		psRelation->uiKind = (uint8)pcFunction->meKind;
		psRelation->asCalls.Init();
		psPlace = GetIDESymbolPlace(pcFunction);
		if (psPlace)
		{
			CopyLocation(psPlace, &psRelation->sPlace);
		}
		else
		{
			memset(&psRelation->sPlace, 0, sizeof(SIDELocation));
			psRelation->sPlace.szFileName.Init();
			psRelation->sPlace.iContainer = -1;
		}
	}
	CopyLocation(psCall, psRelation->asCalls.Add());
}


//////////////////////////////////////////////////////////////////////////
//
// The functions that call the function szKey, added to pasDest in the
// order the index has their calls, each with its calls of it: the
// function's places that are calls, by the function each is in.  A call
// outside any function (a variable's initialiser) has no caller.  The
// number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindCallers(const char* szKey, CArrayIDECallRelations* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcCaller;
	SCPPIndexLocation*	psLocation;
	size				uiBefore;
	size				i;

	uiBefore = pasDest->NumElements();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	for (i = 0; pcSymbol && (i < pcSymbol->NumLocations()); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if ((psLocation->uiRole == CPPIR_Reference) && (psLocation->uiUsage == CPPUT_Call) && (psLocation->iContainer >= 0) && ((size)psLocation->iContainer < mcIndex.NumSymbols()))
		{
			pcCaller = mcIndex.GetSymbol((size)psLocation->iContainer);
			if ((pcCaller->meKind == CPPSK_Function) || (pcCaller->meKind == CPPSK_FunctionTemplate))
			{
				AddCall(pcCaller, psLocation, pasDest);
			}
		}
	}
	mcIndexMutex.Unlock();
	return pasDest->NumElements() - uiBefore;
}


//////////////////////////////////////////////////////////////////////////
//
// The functions the function szKey calls, added to pasDest, each with
// its calls from it: every function's places that are calls inside szKey.
// The index keeps places by symbol, not by where they are, so every
// function's places are looked through.  The number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindCallees(const char* szKey, CArrayIDECallRelations* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcCallee;
	SCPPIndexLocation*	psLocation;
	size				uiBefore;
	size				i;
	size				j;
	int32				iFunction;

	uiBefore = pasDest->NumElements();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	iFunction = -1;
	for (i = 0; pcSymbol && (i < mcIndex.NumSymbols()) && (iFunction < 0); i++)
	{
		if (mcIndex.GetSymbol(i) == pcSymbol)
		{
			iFunction = (int32)i;
		}
	}
	for (i = 0; (iFunction >= 0) && (i < mcIndex.NumSymbols()); i++)
	{
		pcCallee = mcIndex.GetSymbol(i);
		if ((pcCallee->meKind != CPPSK_Function) && (pcCallee->meKind != CPPSK_FunctionTemplate))
		{
			continue;
		}
		for (j = 0; j < pcCallee->NumLocations(); j++)
		{
			psLocation = pcCallee->GetLocation(j);
			if ((psLocation->iContainer == iFunction) && (psLocation->uiRole == CPPIR_Reference) && (psLocation->uiUsage == CPPUT_Call))
			{
				AddCall(pcCallee, psLocation, pasDest);
			}
		}
	}
	mcIndexMutex.Unlock();
	return pasDest->NumElements() - uiBefore;
}


//////////////////////////////////////////////////////////////////////////
//
// pcSymbol added to pasDest with the place a "go to" goes to (its
// definition, or else its declaration).  False, and it isn't added, if it
// is only used.  Called with the index locked.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::AddTarget(CCPPIndexSymbol* pcSymbol, CArrayIDETargets* pasDest)
{
	SCPPIndexLocation*	psLocation;
	SIDETarget*			psTarget;

	psLocation = GetIDESymbolPlace(pcSymbol);
	if (psLocation == NULL)
	{
		return false;
	}
	psTarget = pasDest->Add();
	psTarget->szKey.Init(pcSymbol->mszKey);
	psTarget->uiKind = (uint8)pcSymbol->meKind;
	CopyLocation(psLocation, &psTarget->sLocation);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The class a member's key is scoped by (CVat of CVat::Fill(int)), NULL
// if it has no scope or its scope isn't a class the index has.  Called
// with the index locked.
//
//////////////////////////////////////////////////////////////////////////
CCPPIndexSymbol* CIDECodeModel::FindScopeClass(const char* szKey)
{
	CCPPIndexSymbol*	pcClass;
	CChars				szScope;
	CChars				szName;
	CChars				szParameters;

	SplitIDESymbolKey(szKey, &szScope, &szName, &szParameters);
	pcClass = szScope.Empty() ? NULL : mcIndex.FindSymbol(szScope.Text());
	szParameters.Kill();
	szName.Kill();
	szScope.Kill();
	if (pcClass && (pcClass->meKind != CPPSK_Class) && (pcClass->meKind != CPPSK_ClassTemplate))
	{
		pcClass = NULL;
	}
	return pcClass;
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol szKey added to pasDest as somewhere to go: its definition,
// or else its declaration.  False if the index hasn't it, or only its
// uses.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindTarget(const char* szKey, CArrayIDETargets* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	bool				bFound;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	bFound = pcSymbol && AddTarget(pcSymbol, pasDest);
	mcIndexMutex.Unlock();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// What IntelliJ's Go to Implementation goes to from szKey, added to
// pasDest; how many.  For a virtual member function, the functions that
// override it in the classes derived from its class, the nearest classes
// first.  For a class, the classes derived from it, however far down,
// nearest first.  For a function nothing overrides, its own definition
// (its implementation, seen from its declaration).
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindImplementations(const char* szKey, CArrayIDETargets* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcClass;
	CCPPIndexSymbol*	pcOther;
	CCPPIndexSymbol*	pcDerived;
	CArrayInt			aiOverrides;
	CArrayInt			aiClasses;
	size				uiFirst;
	size				uiNext;
	size				i;
	size				j;
	int					iDerived;

	uiFirst = pasDest->NumElements();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	if (pcSymbol && ((pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_ClassTemplate) || (pcSymbol->meKind == CPPSK_Function)))
	{
		//The classes under the symbol's class (or under it, if it is one), a level at a time.
		pcClass = (pcSymbol->meKind == CPPSK_Function) ? FindScopeClass(szKey) : pcSymbol;
		aiClasses.Init();
		aiOverrides.Init();
		if (pcSymbol->meKind == CPPSK_Function)
		{
			FindOverrides(pcSymbol, &aiOverrides);
		}
		uiNext = 0;
		pcDerived = pcClass;
		while (pcDerived)
		{
			for (i = 0; i < pcDerived->maiDerived.NumElements(); i++)
			{
				iDerived = pcDerived->maiDerived.GetValue(i);
				if (aiClasses.Find(iDerived) == ARRAY_ELEMENT_NOT_FOUND)
				{
					aiClasses.Add(iDerived);
				}
			}
			pcDerived = (uiNext < aiClasses.NumElements()) ? mcIndex.GetSymbol((size)aiClasses.GetValue(uiNext)) : NULL;
			uiNext++;
		}

		for (i = 0; i < aiClasses.NumElements(); i++)
		{
			pcDerived = mcIndex.GetSymbol((size)aiClasses.GetValue(i));
			if (pcSymbol->meKind != CPPSK_Function)
			{
				AddTarget(pcDerived, pasDest);
			}
			else
			{
				for (j = 0; j < aiOverrides.NumElements(); j++)
				{
					pcOther = mcIndex.GetSymbol((size)aiOverrides.GetValue(j));
					if ((pcOther != pcSymbol) && (FindScopeClass(pcOther->mszKey.Text()) == pcDerived))
					{
						AddTarget(pcOther, pasDest);
					}
				}
			}
		}
		aiOverrides.Kill();
		aiClasses.Kill();

		if ((pasDest->NumElements() == uiFirst) && (pcSymbol->meKind == CPPSK_Function) && pcSymbol->GetDefinition())
		{
			AddTarget(pcSymbol, pasDest);
		}
	}
	mcIndexMutex.Unlock();
	return pasDest->NumElements() - uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// What IntelliJ's Go to Super Method goes to from szKey, added to
// pasDest; how many.  For a member function, the functions it overrides
// in its class's base classes, the nearest classes first.  For a class,
// the base classes it names.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindSuperMethods(const char* szKey, CArrayIDETargets* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcClass;
	CCPPIndexSymbol*	pcOther;
	CCPPIndexSymbol*	pcBase;
	CArrayInt			aiOverrides;
	CArrayInt			aiClasses;
	size				uiFirst;
	size				uiNext;
	size				i;
	size				j;
	int					iBase;

	uiFirst = pasDest->NumElements();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	if (pcSymbol && ((pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_ClassTemplate)))
	{
		for (i = 0; i < pcSymbol->maiBases.NumElements(); i++)
		{
			AddTarget(mcIndex.GetSymbol((size)pcSymbol->maiBases.GetValue(i)), pasDest);
		}
	}
	else if (pcSymbol && (pcSymbol->meKind == CPPSK_Function))
	{
		//The classes over the function's class, a level at a time.
		pcClass = FindScopeClass(szKey);
		aiClasses.Init();
		aiOverrides.Init();
		FindOverrides(pcSymbol, &aiOverrides);
		uiNext = 0;
		pcBase = pcClass;
		while (pcBase)
		{
			for (i = 0; i < pcBase->maiBases.NumElements(); i++)
			{
				iBase = pcBase->maiBases.GetValue(i);
				if (aiClasses.Find(iBase) == ARRAY_ELEMENT_NOT_FOUND)
				{
					aiClasses.Add(iBase);
				}
			}
			pcBase = (uiNext < aiClasses.NumElements()) ? mcIndex.GetSymbol((size)aiClasses.GetValue(uiNext)) : NULL;
			uiNext++;
		}

		for (i = 0; i < aiClasses.NumElements(); i++)
		{
			pcBase = mcIndex.GetSymbol((size)aiClasses.GetValue(i));
			for (j = 0; j < aiOverrides.NumElements(); j++)
			{
				pcOther = mcIndex.GetSymbol((size)aiOverrides.GetValue(j));
				if ((pcOther != pcSymbol) && (FindScopeClass(pcOther->mszKey.Text()) == pcBase))
				{
					AddTarget(pcOther, pasDest);
				}
			}
		}
		aiOverrides.Kill();
		aiClasses.Kill();
	}
	mcIndexMutex.Unlock();
	return pasDest->NumElements() - uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// The functions declared beside the function szKey in szFileName: those
// of its scope (its class's members; with no scope, the functions outside
// any) that the file declares, in the order it declares them.  Each one's
// place there is added to pasDeclared and, to pasDefined, where it is
// defined outside its class (a line of -1 if it isn't: it is defined in
// the class, or nowhere).  A function that isn't a class's and is defined
// where the file declares it is defined there.  The number of them.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindSiblingFunctions(const char* szKey, const char* szFileName, CArrayIDELocations* pasDeclared, CArrayIDELocations* pasDefined)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcScope;
	SCPPIndexLocation*	psLocation;
	SCPPIndexLocation*	psDeclared;
	SCPPIndexLocation*	psDefined;
	SIDELocation		sDeclared;
	SIDELocation		sDefined;
	SIDELocation*		psOther;
	CChars				szScope;
	CChars				szName;
	CChars				szParameters;
	CChars				szOtherScope;
	size				i;
	size				j;
	size				uiFile;
	size				uiAt;
	size				uiFirst;
	bool				bSame;
	bool				bInClass;

	SplitIDESymbolKey(szKey, &szScope, &szName, &szParameters);
	szName.Kill();
	szParameters.Kill();
	uiFirst = pasDeclared->NumElements();

	mcIndexMutex.Lock();
	pcScope = szScope.Empty() ? NULL : mcIndex.FindSymbol(szScope.Text());
	bInClass = pcScope && ((pcScope->meKind == CPPSK_Class) || (pcScope->meKind == CPPSK_ClassTemplate));
	uiFile = MAX_SIZE;
	for (i = 0; i < mcIndex.NumFiles(); i++)
	{
		if (StringInsensitiveCompare(mcIndex.GetFileName(i), szFileName) == 0)
		{
			uiFile = i;
			break;
		}
	}
	for (i = 0; (uiFile != MAX_SIZE) && (i < mcIndex.NumSymbols()); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		if ((pcSymbol->meKind != CPPSK_Function) && (pcSymbol->meKind != CPPSK_FunctionTemplate))
		{
			continue;
		}
		psDeclared = NULL;
		psDefined = NULL;
		for (j = 0; j < pcSymbol->NumLocations(); j++)
		{
			psLocation = pcSymbol->GetLocation(j);
			if (psLocation->uiRole == CPPIR_Reference)
			{
				continue;
			}
			if ((psLocation->uiFile == uiFile) && !psDeclared)
			{
				psDeclared = psLocation;
			}
			else if ((psLocation->uiRole == CPPIR_Definition) && !psDefined)
			{
				psDefined = psLocation;
			}
		}
		if (!psDeclared)
		{
			continue;
		}
		if (!psDefined && !bInClass && (psDeclared->uiRole == CPPIR_Definition))
		{
			psDefined = psDeclared;
		}
		SplitIDESymbolKey(pcSymbol->mszKey.Text(), &szOtherScope, &szName, &szParameters);
		bSame = szOtherScope.Equals(szScope.Text());
		szParameters.Kill();
		szName.Kill();
		szOtherScope.Kill();
		if (!bSame)
		{
			continue;
		}

		CopyLocation(psDeclared, &sDeclared);
		if (psDefined)
		{
			CopyLocation(psDefined, &sDefined);
		}
		else
		{
			sDefined = sDeclared;
			sDefined.szFileName.Init();
			sDefined.iLine = -1;
		}
		//In the file's order.
		for (uiAt = uiFirst; uiAt < pasDeclared->NumElements(); uiAt++)
		{
			psOther = pasDeclared->Get(uiAt);
			if ((psOther->iLine > sDeclared.iLine) || ((psOther->iLine == sDeclared.iLine) && (psOther->iColumn > sDeclared.iColumn)))
			{
				break;
			}
		}
		pasDeclared->InsertAt(&sDeclared, uiAt);
		pasDefined->InsertAt(&sDefined, uiAt);
	}
	mcIndexMutex.Unlock();
	szScope.Kill();
	return pasDeclared->NumElements() - uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// How hand written assembly's labels name the symbol szKey, in psDest
// (its key, name, scope, assembly kind and overloads: what
// FindIDEAssemblyEdits is asked with).  False if there is no such symbol.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindAssemblyTarget(const char* szKey, SIDERenameTarget* psDest)
{
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	if (pcSymbol)
	{
		psDest->szKey.Set(pcSymbol->mszKey);
		GetIDESymbolName(pcSymbol->mszKey.Text(), &psDest->szName);
		FindAssemblyRename(pcSymbol, psDest);
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the index has a symbol with szKey, a rename's new name say.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::HasSymbol(const char* szKey)
{
	bool				bFound;
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	bFound = (pcSymbol != NULL) && (pcSymbol->NumLocations() > 0);
	mcIndexMutex.Unlock();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Where Find Usages puts a usage: the project of its file (the first that
// has it, "Other files" if none does) and the class it is in: the class
// it is written in, or the class of the member function it is in, or
// empty for a free function or anything outside a definition.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::GetUsageGroup(SIDELocation* psLocation, CChars* pszProject, CChars* pszClass)
{
	size				uiProject;
	CVcxProject*		pcProject;
	CCPPIndexSymbol*	pcContainer;
	CChars				szScope;
	CChars				szName;
	CChars				szParameters;

	uiProject = FindProjectOfFile(psLocation->szFileName.Text());
	pcProject = (uiProject != MAX_SIZE) ? mpcWorkspace->GetSolutionProject(uiProject)->GetProject() : NULL;
	pszProject->Init(pcProject ? pcProject->GetName() : "Other files");

	pszClass->Init();
	mcIndexMutex.Lock();
	if ((psLocation->iContainer >= 0) && ((size)psLocation->iContainer < mcIndex.NumSymbols()))
	{
		pcContainer = mcIndex.GetSymbol((size)psLocation->iContainer);
		if ((pcContainer->meKind == CPPSK_Class) || (pcContainer->meKind == CPPSK_ClassTemplate))
		{
			pszClass->Append(pcContainer->mszKey);
		}
		else
		{
			//A member function's class is its scope.
			SplitIDESymbolKey(pcContainer->mszKey.Text(), &szScope, &szName, &szParameters);
			pszClass->Append(szScope);
			szParameters.Kill();
			szName.Kill();
			szScope.Kill();
		}
	}
	mcIndexMutex.Unlock();
}


//An #include in a file's text: where its name is (between its quotes or angle brackets) and on which line.
struct SIDEIncludeDirective
{
	size	uiNameStart;
	size	uiNameEnd;
	int32	iLine;		//0 based.
	size	uiLineStart;
	bool	bSystem;
};


typedef CArrayTemplate<SIDEIncludeDirective>	CArrayIDEIncludeDirectives;


//////////////////////////////////////////////////////////////////////////
//
// Where the next comment starts on a line from uiStart (before uiEnd),
// past any string or character literal: MAX_SIZE if none.  pbBlock says
// if it is a /* comment, rather than a // one.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDELineComment(const char* szText, size uiStart, size uiEnd, bool* pbBlock)
{
	size	i;
	char	cQuote;

	for (i = uiStart; i + 1 < uiEnd; i++)
	{
		if ((szText[i] == '"') || (szText[i] == '\''))
		{
			cQuote = szText[i];
			for (i++; (i < uiEnd) && (szText[i] != cQuote); i++)
			{
				if (szText[i] == '\\')
				{
					i++;
				}
			}
		}
		else if ((szText[i] == '/') && ((szText[i + 1] == '/') || (szText[i + 1] == '*')))
		{
			*pbBlock = (szText[i + 1] == '*');
			return i;
		}
	}
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// Each #include in a file's text, as written: the text isn't
// preprocessed, so an #include an #if leaves out is one too, but one in a
// comment isn't.  "# include" and indented ones are found.
//
//////////////////////////////////////////////////////////////////////////
static void FindIDEIncludeDirectives(const char* szText, size uiLength, CArrayIDEIncludeDirectives* pasDest)
{
	size					uiLineStart;
	size					uiLineEnd;
	size					p;
	size					uiComment;
	const char*				szNewLine;
	const char*				szClose;
	char					cClose;
	bool					bInBlock;
	bool					bBlock;
	int32					iLine;
	SIDEIncludeDirective*	psDirective;

	bInBlock = false;
	iLine = 0;
	for (uiLineStart = 0; uiLineStart < uiLength; uiLineStart = uiLineEnd + 1, iLine++)
	{
		szNewLine = (const char*)memchr(&szText[uiLineStart], '\n', uiLength - uiLineStart);
		uiLineEnd = szNewLine ? (size)(szNewLine - szText) : uiLength;
		p = uiLineStart;
		if (bInBlock)
		{
			szClose = strstr(&szText[p], "*/");
			if ((szClose == NULL) || ((size)(szClose - szText) >= uiLineEnd))
			{
				continue;
			}
			p = (size)(szClose - szText) + 2;
			bInBlock = false;
		}

		while ((p < uiLineEnd) && ((szText[p] == ' ') || (szText[p] == '\t')))
		{
			p++;
		}
		if ((p < uiLineEnd) && (szText[p] == '#'))
		{
			p++;
			while ((p < uiLineEnd) && ((szText[p] == ' ') || (szText[p] == '\t')))
			{
				p++;
			}
			if ((p + 7 < uiLineEnd) && (strncmp(&szText[p], "include", 7) == 0) && ((szText[p + 7] == ' ') || (szText[p + 7] == '\t') || (szText[p + 7] == '"') || (szText[p + 7] == '<')))
			{
				p += 7;
				while ((p < uiLineEnd) && ((szText[p] == ' ') || (szText[p] == '\t')))
				{
					p++;
				}
				if ((p < uiLineEnd) && ((szText[p] == '"') || (szText[p] == '<')))
				{
					cClose = (szText[p] == '"') ? '"' : '>';
					szClose = (const char*)memchr(&szText[p + 1], cClose, uiLineEnd - (p + 1));
					if (szClose && ((size)(szClose - szText) > p + 1))
					{
						psDirective = pasDest->Add();
						psDirective->uiNameStart = p + 1;
						psDirective->uiNameEnd = (size)(szClose - szText);
						psDirective->iLine = iLine;
						psDirective->uiLineStart = uiLineStart;
						psDirective->bSystem = (cClose == '>');
						p = psDirective->uiNameEnd + 1;
					}
				}
			}
		}

		//A /* comment left open at the end of the line hides the lines after it.
		for (;;)
		{
			uiComment = FindIDELineComment(szText, p, uiLineEnd, &bBlock);
			if ((uiComment == MAX_SIZE) || !bBlock)
			{
				break;
			}
			szClose = strstr(&szText[uiComment + 2], "*/");
			if ((szClose == NULL) || ((size)(szClose - szText) >= uiLineEnd))
			{
				bInBlock = true;
				break;
			}
			p = (size)(szClose - szText) + 2;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every #include, in the projects' C and C++ files, that finds
// szFileName (a full path) as the compiler would (see FindIncludeFile),
// added to pasDest: the includer, the line, and the column and length of
// the file's name in the #include (its last part, after any directories),
// which is what renaming the file changes.  An #include an #if leaves out
// is found too; one in a comment isn't.  The file must still be where
// szFileName says: an #include is found by looking for its file.  The
// number found.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindIncludesOf(const char* szFileName, CIDEFileTexts* pcTexts, CArrayIDELocations* pasDest)
{
	CFileUtil					cFileUtil;
	CChars						szTargetName;
	CChars						szTargetDirectory;
	CChars						szName;
	CChars						szFound;
	CTextFile					cFile;
	CArrayIDEIncludeDirectives	asDirectives;
	SIDEIncludeDirective*		psDirective;
	SIDELocation*				psLocation;
	EIDEFileType				eType;
	const char*					szIncluder;
	const char*					szText;
	size						uiLength;
	size						uiProject;
	size						uiNamePart;
	size						uiFound;
	size						i;
	size						j;

	FindProjectFiles();
	szTargetName.Init();
	szTargetDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szTargetName, &szTargetDirectory);
	uiFound = 0;
	for (i = 0; i < mcProjectFiles.NumElements(); i++)
	{
		szIncluder = mcProjectFiles.GetKey(i);
		eType = CIDEIcons::GetFileType(szIncluder);
		if ((eType != IFT_CPP) && (eType != IFT_C) && (eType != IFT_Header))
		{
			continue;
		}
		cFile.Init();
		if (!pcTexts->GetFileText(szIncluder, &cFile, &szText, &uiLength))
		{
			cFile.Kill();
			continue;
		}

		asDirectives.Init();
		FindIDEIncludeDirectives(szText, uiLength, &asDirectives);
		uiProject = MAX_SIZE;
		for (j = 0; j < asDirectives.NumElements(); j++)
		{
			psDirective = asDirectives.Get(j);

			//Only an #include whose last part is the file's name can find it.
			for (uiNamePart = psDirective->uiNameEnd; (uiNamePart > psDirective->uiNameStart) && (szText[uiNamePart - 1] != '/') && (szText[uiNamePart - 1] != '\\'); uiNamePart--)
			{
			}
			if ((psDirective->uiNameEnd - uiNamePart != szTargetName.Length()) || (MemICmp(&szText[uiNamePart], szTargetName.Text(), szTargetName.Length()) != 0))
			{
				continue;
			}

			if (uiProject == MAX_SIZE)
			{
				uiProject = FindProjectOfFile(szIncluder);
			}
			szName.Init();
			szName.AppendSubString(szText, psDirective->uiNameStart, psDirective->uiNameEnd);
			szName.Replace('\\', '/');
			szFound.Init();
			if (FindIncludeFile(uiProject, szIncluder, szName.Text(), psDirective->bSystem, &szFound) && (StringInsensitiveCompare(szFound.Text(), szFileName) == 0))
			{
				psLocation = pasDest->Add();
				psLocation->szFileName.Init(szIncluder);
				psLocation->iLine = psDirective->iLine;
				psLocation->iColumn = (int32)(uiNamePart - psDirective->uiLineStart);
				psLocation->iLength = (int32)(psDirective->uiNameEnd - uiNamePart);
				psLocation->uiRole = 0;
				psLocation->uiFlags = 0;
				psLocation->uiUsage = 0;
				psLocation->iContainer = -1;
				uiFound++;
			}
			szFound.Kill();
			szName.Kill();
		}
		asDirectives.Kill();
		cFile.Kill();
	}
	szTargetDirectory.Kill();
	szTargetName.Kill();
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The file an #include names, as the compiler would find it: "" includes
// next to the file that includes them first, then each in project
// uiProject's include directories, then the compiler's.  False if it
// isn't found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindIncludeFile(size uiProject, const char* szIncluder, const char* szName, bool bSystem, CChars* pszDest)
{
	CFileUtil			cFileUtil;
	CArrayChars			aszDirectories;
	CChars				szPath;
	size				i;
	bool				bFound;

	aszDirectories.Init();
	GetIncludeDirectories(uiProject, szIncluder, bSystem, &aszDirectories);

	bFound = false;
	for (i = 0; (i < aszDirectories.NumElements()) && !bFound; i++)
	{
		szPath.Init(aszDirectories.GetText(i));
		cFileUtil.AppendToPath(&szPath, szName);
		if (cFileUtil.Exists(szPath.Text()))
		{
			//The name's separators are the #include's; a path with both kinds isn't collapsed ("Sub/../Engine.h" on Windows).
			cFileUtil.FixSeparators(&szPath);
			cFileUtil.FullPath(&szPath);
			pszDest->Set(szPath);
			bFound = true;
		}
		szPath.Kill();
	}
	aszDirectories.Kill();
	return bFound;
}


bool IsIDENameChar(char c);


//////////////////////////////////////////////////////////////////////////
//
// Where an #include is looked for, in order: "" includes next to the
// file that includes them first, then project uiProject's include
// directories, then the compiler's.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::GetIncludeDirectories(size uiProject, const char* szIncluder, bool bSystem, CArrayChars* paszDest)
{
	CFileUtil			cFileUtil;
	CChars				szDirectory;
	CChars				szFileName;
	CVcxConfiguration*	pcConfiguration;
	size				i;

	if (!bSystem && szIncluder)
	{
		szFileName.Init();
		szDirectory.Init();
		cFileUtil.SplitPath(szIncluder, &szFileName, &szDirectory);
		paszDest->Add(szDirectory.Text());
		szDirectory.Kill();
		szFileName.Kill();
	}
	if (uiProject < mpcWorkspace->NumProjects())
	{
		pcConfiguration = GetProjectConfiguration(uiProject);
		if (pcConfiguration && mpcWorkspace->GetSolutionProject(uiProject)->GetProject())
		{
			mpcWorkspace->GetSolutionProject(uiProject)->GetProject()->GetIncludeDirectories(pcConfiguration, mpcWorkspace->GetProjectSolution(uiProject)->GetDirectory(), paszDest);
		}
	}
	for (i = 0; i < maszSystemDirectories.NumElements(); i++)
	{
		paszDest->Add(maszSystemDirectories.GetText(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The project whose include directories a file's #includes are looked
// for in: its own, or (a header no project has as an item) that of the
// first unit indexed that included it.  MAX_SIZE if neither.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindIncludeProject(const char* szIncluder)
{
	CChars	szUnit;
	size	uiProject;

	uiProject = FindProjectOfFile(szIncluder);
	if (uiProject == MAX_SIZE)
	{
		szUnit.Init();
		if (!GetIncluder(szIncluder, &uiProject, &szUnit))
		{
			uiProject = MAX_SIZE;
		}
		szUnit.Kill();
	}
	return uiProject;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDEIncludes(CArrayIDEIncludes* pasIncludes)
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
//
//////////////////////////////////////////////////////////////////////////
void KillIDENeeds(CArrayIDENeeds* pasNeeds)
{
	size	i;

	for (i = 0; i < pasNeeds->NumElements(); i++)
	{
		pasNeeds->Get(i)->szDeclared.Kill();
		pasNeeds->Get(i)->szKey.Kill();
		pasNeeds->Get(i)->szUser.Kill();
	}
	pasNeeds->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The directive a line of a text starts with ("if", "include", "endif"),
// its length; 0 if the line isn't a directive's.  *puiAfter is where what
// follows the directive's name starts.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDEDirective(const char* szText, size uiLineStart, size* puiName, size* puiAfter)
{
	size	p;
	size	uiLength;

	for (p = uiLineStart; (szText[p] == ' ') || (szText[p] == '\t'); p++)
	{
	}
	if (szText[p] != '#')
	{
		return 0;
	}
	for (p++; (szText[p] == ' ') || (szText[p] == '\t'); p++)
	{
	}
	*puiName = p;
	for (uiLength = 0; (szText[p + uiLength] >= 'a') && (szText[p + uiLength] <= 'z'); uiLength++)
	{
	}
	for (p += uiLength; (szText[p] == ' ') || (szText[p] == '\t'); p++)
	{
	}
	*puiAfter = p;
	return uiLength;
}


//////////////////////////////////////////////////////////////////////////
//
// Each #include a file's text writes (as FindIncludesOf reads them: one
// an #if leaves out is one too), in the text's order, with the file it
// finds and whether it is inside an #if.  The #ifndef of an include
// guard (the text's first directive, with its #define next) isn't an #if
// here.  The number found.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindIncludesIn(const char* szFileName, const char* szText, CArrayIDEIncludes* pasDest)
{
	CArrayIDEIncludeDirectives	asDirectives;
	SIDEIncludeDirective*		psDirective;
	SIDEInclude*				psInclude;
	CChars						szName;
	const char*					szNewLine;
	size						uiProject;
	size						uiLength;
	size						uiLine;
	size						uiNext;
	size						uiName;
	size						uiAfter;
	size						uiDirective;
	size						uiDirectives;
	size						uiGuard;
	size						i;
	int							iDepth;
	int							iGuardDepth;

	uiLength = strlen(szText);
	uiProject = FindIncludeProject(szFileName);
	asDirectives.Init();
	FindIDEIncludeDirectives(szText, uiLength, &asDirectives);

	iDepth = 0;
	iGuardDepth = 0;
	uiDirectives = 0;
	uiGuard = 0;
	i = 0;
	for (uiLine = 0; (uiLine < uiLength) && (i < asDirectives.NumElements()); uiLine = uiNext)
	{
		szNewLine = strchr(&szText[uiLine], '\n');
		uiNext = szNewLine ? (size)(szNewLine - szText) + 1 : uiLength;
		psDirective = asDirectives.Get(i);
		if (psDirective->uiLineStart == uiLine)
		{
			psInclude = pasDest->Add();
			psInclude->uiLineStart = uiLine;
			psInclude->uiLineEnd = uiNext;
			psInclude->uiNameStart = psDirective->uiNameStart;
			psInclude->uiNameEnd = psDirective->uiNameEnd;
			psInclude->iLine = psDirective->iLine;
			psInclude->bSystem = psDirective->bSystem;
			psInclude->bConditional = (iDepth > iGuardDepth);
			psInclude->szFileName.Init();
			szName.Init();
			szName.AppendSubString(szText, psDirective->uiNameStart, psDirective->uiNameEnd);
			szName.Replace('\\', '/');
			FindIncludeFile(uiProject, szFileName, szName.Text(), psDirective->bSystem, &psInclude->szFileName);
			szName.Kill();
			i++;
			uiDirectives++;
			continue;
		}

		uiDirective = FindIDEDirective(szText, uiLine, &uiName, &uiAfter);
		if (uiDirective == 0)
		{
			continue;
		}
		uiDirectives++;
		if ((uiDirective >= 2) && (strncmp(&szText[uiName], "if", 2) == 0))
		{
			iDepth++;
			if ((uiDirectives == 1) && (uiDirective == 6) && (strncmp(&szText[uiName], "ifndef", 6) == 0))
			{
				uiGuard = uiAfter;
			}
		}
		else if ((uiDirective == 5) && (strncmp(&szText[uiName], "endif", 5) == 0))
		{
			iDepth--;
		}
		else if ((uiDirectives == 2) && (uiGuard != 0) && (uiDirective == 6) && (strncmp(&szText[uiName], "define", 6) == 0))
		{
			//The guard's #define: the same name as its #ifndef.
			for (uiLength = 0; IsIDENameChar(szText[uiGuard + uiLength]) && (szText[uiGuard + uiLength] == szText[uiAfter + uiLength]); uiLength++)
			{
			}
			if (!IsIDENameChar(szText[uiGuard + uiLength]) && !IsIDENameChar(szText[uiAfter + uiLength]))
			{
				iGuardDepth = 1;
			}
			uiLength = strlen(szText);
		}
	}
	asDirectives.Kill();
	return pasDest->NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// What an #include in szIncluder writes to find the file szTarget: its
// path from the first of the directories #includes are looked for in
// that it is under, the shortest there is ("Gauge.h" beside it,
// "BaseLib/Chars.h" from a project's include directory).  False if it is
// under none of them (pszDest is then its name alone).
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::MakeIncludeName(const char* szIncluder, const char* szTarget, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszDirectories;
	CChars		szName;
	CChars		szFound;
	CChars		szDirectory;
	size		uiProject;
	size		uiLength;
	size		i;
	bool		bFound;

	uiProject = FindIncludeProject(szIncluder);
	aszDirectories.Init();
	GetIncludeDirectories(uiProject, szIncluder, false, &aszDirectories);
	bFound = false;
	for (i = 0; i < aszDirectories.NumElements(); i++)
	{
		szDirectory.Init(aszDirectories.GetText(i));
		cFileUtil.FixSeparators(&szDirectory);
		cFileUtil.FullPath(&szDirectory);
		uiLength = szDirectory.Length();
		if ((uiLength > 0) && (strlen(szTarget) > uiLength + 1) && (MemICmp(szDirectory.Text(), szTarget, uiLength) == 0) && ((szTarget[uiLength] == '/') || (szTarget[uiLength] == '\\')))
		{
			szName.Init(&szTarget[uiLength + 1]);
			szName.Replace('\\', '/');
			szFound.Init();
			if ((!bFound || (szName.Length() < pszDest->Length())) && FindIncludeFile(uiProject, szIncluder, szName.Text(), false, &szFound) && (StringInsensitiveCompare(szFound.Text(), szTarget) == 0))
			{
				pszDest->Set(szName);
				bFound = true;
			}
			szFound.Kill();
			szName.Kill();
		}
		szDirectory.Kill();
	}
	aszDirectories.Kill();
	if (!bFound)
	{
		for (i = strlen(szTarget); (i > 0) && (szTarget[i - 1] != '/') && (szTarget[i - 1] != '\\'); i--)
		{
		}
		pszDest->Set(&szTarget[i]);
	}
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The file that declares a symbol, as the index numbers it: what a file
// that names the symbol must #include.  A class's, a class template's or
// an enum's is the one that defines it; anything else's is the first
// header that declares it, or the first file if no header does.  -1 if
// nothing declares it, or it is declared wherever it is named (a
// namespace, a function's own names).  Call it with the index's mutex
// held.
//
//////////////////////////////////////////////////////////////////////////
int CIDECodeModel::FindDeclaringFile(CCPPIndexSymbol* pcSymbol)
{
	SCPPIndexLocation*	psLocation;
	size				i;
	int					iFirst;

	if ((pcSymbol->mszKey.Text()[0] == '@') || (pcSymbol->meKind == CPPSK_Namespace) || (pcSymbol->meKind == CPPSK_TypeParameter) || (pcSymbol->meKind == CPPSK_Parameter) ||
		(pcSymbol->meKind == CPPSK_ValueParameter) || (pcSymbol->meKind == CPPSK_TemplateTemplateParameter) || (pcSymbol->meKind == CPPSK_Label) || (pcSymbol->meKind == CPPSK_BuiltinType))
	{
		return -1;
	}
	if ((pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_ClassTemplate) || (pcSymbol->meKind == CPPSK_Enum))
	{
		psLocation = pcSymbol->GetDefinition();
		if (psLocation)
		{
			return (int)psLocation->uiFile;
		}
	}
	iFirst = -1;
	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			continue;
		}
		if (CIDEIcons::GetFileType(mcIndex.GetFileName(psLocation->uiFile)) == IFT_Header)
		{
			return (int)psLocation->uiFile;
		}
		if (iFirst == -1)
		{
			iFirst = (int)psLocation->uiFile;
		}
	}
	return iFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// What files need of other files: each symbol that the file szUser (any
// file, if NULL) names, or defines out of the header that declares it,
// and that another file declares (one of paszDeclared, if not NULL), once
// for each file that names it.  A name in a template's declaration that
// only its instances resolve isn't a need of the template's file: the
// #include goes where the instance is (Refactorings.txt, 15).  What a
// file declares for itself it doesn't need, but a class it declares and
// doesn't define it does (bDeclaredByUser).  The number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindNeeds(const char* szUser, CArrayChars* paszDeclared, CArrayIDENeeds* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;
	SIDENeed*			psNeed;
	CArrayInt			aiUsers;
	CArrayInt			aiDeclarers;
	size				uiAdded;
	size				i;
	size				j;
	size				k;
	int					iUser;
	int					iDeclared;
	int					iFile;
	bool				bClass;
	bool				bWanted;

	uiAdded = 0;
	mcIndexMutex.Lock();
	iUser = -1;
	for (i = 0; szUser && (i < mcIndex.NumFiles()); i++)
	{
		if (StringInsensitiveCompare(mcIndex.GetFileName(i), szUser) == 0)
		{
			iUser = (int)i;
			break;
		}
	}
	if (szUser && (iUser == -1))
	{
		mcIndexMutex.Unlock();
		return 0;
	}

	aiUsers.Init();
	aiDeclarers.Init();
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		iDeclared = FindDeclaringFile(pcSymbol);
		if (iDeclared == -1)
		{
			continue;
		}
		if (paszDeclared)
		{
			bWanted = false;
			for (j = 0; (j < paszDeclared->NumElements()) && !bWanted; j++)
			{
				bWanted = StringInsensitiveCompare(mcIndex.GetFileName((size)iDeclared), paszDeclared->GetText(j)) == 0;
			}
			if (!bWanted)
			{
				continue;
			}
		}
		bClass = (pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_ClassTemplate) || (pcSymbol->meKind == CPPSK_Enum);

		//The files that name it, and those that declare it.
		aiUsers.ReInit();
		aiDeclarers.ReInit();
		for (j = 0; j < pcSymbol->NumLocations(); j++)
		{
			psLocation = pcSymbol->GetLocation(j);
			iFile = (int)psLocation->uiFile;
			if ((iFile == iDeclared) || ((iUser != -1) && (iFile != iUser)))
			{
				continue;
			}
			if (psLocation->uiRole == CPPIR_Declaration)
			{
				aiDeclarers.AddIfUnique(iFile);
			}
			else if ((psLocation->uiRole == CPPIR_Definition) ? !bClass : !(psLocation->uiFlags & CPPIF_INSTANCE))
			{
				aiUsers.AddIfUnique(iFile);
			}
		}
		for (j = 0; j < aiUsers.NumElements(); j++)
		{
			iFile = aiUsers.GetValue(j);
			bWanted = true;
			for (k = 0; k < aiDeclarers.NumElements(); k++)
			{
				if (aiDeclarers.GetValue(k) == iFile)
				{
					//It declares it for itself: enough, unless it is a class (it may need all of it).
					bWanted = bClass;
				}
			}
			if (!bWanted)
			{
				continue;
			}
			psNeed = pasDest->Add();
			psNeed->szUser.Init(mcIndex.GetFileName((size)iFile));
			psNeed->szKey.Init(pcSymbol->mszKey);
			psNeed->uiKind = (uint8)pcSymbol->meKind;
			psNeed->szDeclared.Init(mcIndex.GetFileName((size)iDeclared));
			psNeed->bDeclaredByUser = false;
			for (k = 0; k < aiDeclarers.NumElements(); k++)
			{
				psNeed->bDeclaredByUser |= (aiDeclarers.GetValue(k) == iFile);
			}
			uiAdded++;
		}
	}
	aiDeclarers.Kill();
	aiUsers.Kill();
	mcIndexMutex.Unlock();
	return uiAdded;
}


//////////////////////////////////////////////////////////////////////////
//
// The headers that declare something named szName outside any class or
// namespace (a class, a function whatever its parameters, a typedef, a
// variable), each once.  The number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindHeadersDeclaring(const char* szName, CArrayChars* paszDest)
{
	CCPPIndexSymbol*	pcSymbol;
	const char*			szKey;
	const char*			szFile;
	size				uiLength;
	size				uiAdded;
	size				i;
	size				j;
	int					iDeclared;
	bool				bHas;

	uiLength = strlen(szName);
	uiAdded = 0;
	mcIndexMutex.Lock();
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		szKey = pcSymbol->mszKey.Text();
		if ((strncmp(szKey, szName, uiLength) != 0) || ((szKey[uiLength] != '\0') && (szKey[uiLength] != '(')))
		{
			continue;
		}
		iDeclared = FindDeclaringFile(pcSymbol);
		szFile = (iDeclared != -1) ? mcIndex.GetFileName((size)iDeclared) : NULL;
		if (!szFile || (CIDEIcons::GetFileType(szFile) != IFT_Header))
		{
			continue;
		}
		bHas = false;
		for (j = 0; (j < paszDest->NumElements()) && !bHas; j++)
		{
			bHas = StringInsensitiveCompare(paszDest->GetText(j), szFile) == 0;
		}
		if (!bHas)
		{
			paszDest->Add(szFile);
			uiAdded++;
		}
	}
	mcIndexMutex.Unlock();
	return uiAdded;
}
