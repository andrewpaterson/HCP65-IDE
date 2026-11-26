/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela IndexLib

Codaphela IndexLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela IndexLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela IndexLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include <ctype.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/HashSetString.h"
#include "WorkspaceIndex.h"


//////////////////////////////////////////////////////////////////////////
//
// A documentation page: a Markdown file.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::IsPageFile(const char* szFileName)
{
	CChars	szName;

	szName.Fake(szFileName);
	return szName.EndsWithIgnoreCase(".md");
}


//////////////////////////////////////////////////////////////////////////
//
// The website's symbol table (Website/.symbols/symbols.txt, which
// hcp65index makes), the list of files beside it and the folder both are
// relative to: references to what the workspace doesn't have are resolved
// with them, and references to files always are, as the website does.
// False, with none kept, if they can't be read.  Before StartIndexing.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::SetSiteSymbols(const char* szTable, const char* szFiles, const char* szRoot)
{
	CFileUtil	cFileUtil;

	mcSiteTable.Kill();
	maszSiteFiles.ReInit();
	mcSiteTable.Init(NULL);
	mbSiteSymbols = mcSiteTable.Read(szTable) && CReferenceResolver::ReadFiles(szFiles, &maszSiteFiles);
	if (!mbSiteSymbols)
	{
		mcSiteTable.Kill();
		mcSiteTable.Init(NULL);
		maszSiteFiles.ReInit();
	}
	mszSiteRoot.Set(szRoot);
	cFileUtil.FullPath(&mszSiteRoot);
	if (!mszSiteRoot.EndsWith(FILE_SEPARATOR))
	{
		mszSiteRoot.Append(FILE_SEPARATOR);
	}
	return mbSiteSymbols;
}


//////////////////////////////////////////////////////////////////////////
//
// The website whose mkdocs.yml is szMkdocsFile: its symbol table and list
// of files (made by its build in .symbols beside it), relative to the
// folder holding the repositories, two up.  False if there are none.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::SetDocsSite(const char* szMkdocsFile)
{
	CFileUtil	cFileUtil;
	CChars		szFolder;
	CChars		szName;
	CChars		szTable;
	CChars		szFiles;
	CChars		szRoot;
	bool		bSet;

	szFolder.Init();
	szName.Init();
	mszDocsSite.Set(szMkdocsFile);
	cFileUtil.FullPath(&mszDocsSite);
	cFileUtil.SplitPath(szMkdocsFile, &szName, &szFolder);
	cFileUtil.FullPath(&szFolder);
	szTable.Init(szFolder);
	cFileUtil.AppendToPath(&szTable, ".symbols");
	szFiles.Init(szTable);
	cFileUtil.AppendToPath(&szTable, "symbols.txt");
	cFileUtil.AppendToPath(&szFiles, "files.txt");
	szRoot.Init(szFolder);
	cFileUtil.AppendToPath(&szRoot, "..");
	cFileUtil.AppendToPath(&szRoot, "..");
	cFileUtil.FullPath(&szRoot);
	bSet = SetSiteSymbols(szTable.Text(), szFiles.Text(), szRoot.Text());
	szRoot.Kill();
	szFiles.Kill();
	szTable.Kill();
	szName.Kill();
	szFolder.Kill();
	return bSet;
}


//////////////////////////////////////////////////////////////////////////
//
// The website the workspace's pages are part of: an mkdocs.yml that is an
// item of one of its projects (Website.vcxproj has it).  False if none
// is.  The HCP65 IDE also asks its projects' properties.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::FindDocsSite(CChars* pszMkdocsFile)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szFolder;
	bool			bFound;

	bFound = false;
	szName.Init();
	szFolder.Init();
	for (i = 0; (i < mpcWorkspace->NumProjects()) && !bFound; i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
		for (j = 0; (pcProject != NULL) && (j < pcProject->NumItems()) && !bFound; j++)
		{
			pszMkdocsFile->Clear();
			pcProject->GetFullPath(pcProject->GetItem(j), pszMkdocsFile);
			szName.Clear();
			szFolder.Clear();
			cFileUtil.SplitPath(pszMkdocsFile->Text(), &szName, &szFolder);
			bFound = szName.EqualsIgnoreCase("mkdocs.yml");
		}
	}
	szFolder.Kill();
	szName.Kill();
	if (!bFound)
	{
		pszMkdocsFile->Clear();
	}
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's project that szFileName (a full path) is an item of,
// the first if more than one has it; MAX_SIZE if none has.  Needs no
// indexing.
//
//////////////////////////////////////////////////////////////////////////
size CWorkspaceIndex::FindItemProject(const char* szFileName)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CChars			szFile;
	CFileUtil		cFileUtil;
	size			uiProject;

	uiProject = MAX_SIZE;
	szFile.Init();
	for (i = 0; (uiProject == MAX_SIZE) && (i < mpcWorkspace->NumProjects()); i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
		for (j = 0; (pcProject != NULL) && (j < pcProject->NumItems()); j++)
		{
			szFile.Clear();
			pcProject->GetFullPath(pcProject->GetItem(j), &szFile);
			cFileUtil.FullPath(&szFile);
			if (szFile.EqualsIgnoreCase(szFileName))
			{
				uiProject = i;
				break;
			}
		}
	}
	szFile.Kill();
	return uiProject;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's Markdown items, none resolved yet (StartIndexing).
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::AddPages(void)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CVcxItem*		pcItem;
	SWorkspacePage*	psPage;
	CChars			szFile;
	CFileUtil		cFileUtil;

	szFile.Init();
	mcIndexMutex.Lock();
	KillPages();
	for (i = 0; i < mpcWorkspace->NumProjects(); i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
		for (j = 0; (pcProject != NULL) && (j < pcProject->NumItems()); j++)
		{
			pcItem = pcProject->GetItem(j);
			szFile.Clear();
			pcProject->GetFullPath(pcItem, &szFile);
			cFileUtil.FullPath(&szFile);
			if (IsPageFile(szFile.Text()) && (FindPage(szFile.Text()) == NULL))
			{
				psPage = NewMalloc<SWorkspacePage>();
				psPage->szFile.Init(szFile.Text());
				psPage->uiProject = i;
				psPage->asReferences.Init();
				mapsPages.Add(psPage);
			}
		}
	}
	mcIndexMutex.Unlock();
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// With the index's mutex.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::KillPages(void)
{
	size			i;
	SWorkspacePage*	psPage;

	for (i = 0; i < mapsPages.NumElements(); i++)
	{
		psPage = mapsPages.GetPtr(i);
		CReferenceResolver::KillPageReferences(&psPage->asReferences);
		psPage->szFile.Kill();
		SafeFree(psPage);
	}
	mapsPages.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// With the index's mutex; NULL if szFileName isn't a page of the
// workspace.
//
//////////////////////////////////////////////////////////////////////////
SWorkspacePage* CWorkspaceIndex::FindPage(const char* szFileName)
{
	size			i;
	SWorkspacePage*	psPage;

	for (i = 0; i < mapsPages.NumElements(); i++)
	{
		psPage = mapsPages.GetPtr(i);
		if (psPage->szFile.Equals(szFileName))
		{
			return psPage;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The files a reference can name when the website's list isn't there:
// the index's and the projects' items, each once, full paths with '/'
// between folders.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::GetResolvableFiles(CArrayChars* paszDest)
{
	size				i;
	size				j;
	CVcxProject*		pcProject;
	CArrayChars			aszFiles;
	CChars*				pszFile;
	CFileUtil			cFileUtil;
	CHashSetString<>	cSeen;
	bool				bAdded;

	aszFiles.Init();
	mcIndexMutex.Lock();
	for (i = 0; i < mcIndex.NumFiles(); i++)
	{
		aszFiles.Add(mcIndex.GetFileName(i));
	}
	mcIndexMutex.Unlock();
	for (i = 0; i < mpcWorkspace->NumProjects(); i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
		for (j = 0; (pcProject != NULL) && (j < pcProject->NumItems()); j++)
		{
			pszFile = aszFiles.Add("");
			pcProject->GetFullPath(pcProject->GetItem(j), pszFile);
			cFileUtil.FullPath(pszFile);
		}
	}

	cSeen.Init();
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		pszFile = aszFiles.Get(i);
		pszFile->Replace(FILE_SEPARATOR[0], '/');
		cSeen.FindOrAddIndex(pszFile->Text(), &bAdded);
		if (bAdded)
		{
			paszDest->Add(pszFile->Text());
		}
	}
	cSeen.Kill();
	aszFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Every page's references resolved from its file, once the C++ is indexed
// (on the worker thread that indexed the last unit).  The index is read
// once for them all.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::IndexPages(void)
{
	CSymbolTable		cLive;
	CArrayChars			aszFiles;
	CReferenceResolver	cLiveResolver;
	CReferenceResolver	cSiteResolver;
	CTextFile			cText;
	size				i;
	SWorkspacePage*		psPage;

	cLive.Init(NULL);
	mcIndexMutex.Lock();
	cLive.Gather(&mcIndex);
	mcIndexMutex.Unlock();
	aszFiles.Init();
	GetResolvableFiles(&aszFiles);
	cLiveResolver.Init(&cLive, &aszFiles);
	if (mbSiteSymbols)
	{
		cSiteResolver.Init(&mcSiteTable, &maszSiteFiles);
	}

	//Only StartIndexing changes the pages, and it waits for the jobs first.
	for (i = 0; i < mapsPages.NumElements(); i++)
	{
		psPage = mapsPages.GetPtr(i);
		cText.Init();
		if (cText.Read(psPage->szFile.Text()))
		{
			ResolvePage(psPage, cText.Text(), &cLiveResolver, mbSiteSymbols ? &cSiteResolver : NULL);
		}
		cText.Kill();
	}

	if (mbSiteSymbols)
	{
		cSiteResolver.Kill();
	}
	cLiveResolver.Kill();
	aszFiles.Kill();
	cLive.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// One page's references resolved again from szText (its editor's), as
// the index is now.  False if szFileName isn't a page of the workspace.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::IndexPage(const char* szFileName, const char* szText)
{
	CSymbolTable		cLive;
	CArrayChars			aszFiles;
	CReferenceResolver	cLiveResolver;
	CReferenceResolver	cSiteResolver;
	SWorkspacePage*		psPage;

	mcIndexMutex.Lock();
	psPage = FindPage(szFileName);
	mcIndexMutex.Unlock();
	if (psPage == NULL)
	{
		return false;
	}

	cLive.Init(NULL);
	mcIndexMutex.Lock();
	cLive.Gather(&mcIndex);
	mcIndexMutex.Unlock();
	aszFiles.Init();
	GetResolvableFiles(&aszFiles);
	cLiveResolver.Init(&cLive, &aszFiles);
	if (mbSiteSymbols)
	{
		cSiteResolver.Init(&mcSiteTable, &maszSiteFiles);
	}
	ResolvePage(psPage, szText, &cLiveResolver, mbSiteSymbols ? &cSiteResolver : NULL);
	if (mbSiteSymbols)
	{
		cSiteResolver.Kill();
	}
	cLiveResolver.Kill();
	aszFiles.Kill();
	cLive.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A page's references resolved: a symbol by the workspace's index (pcLive)
// first, then the website's table (pcSite, NULL if there is none); a file
// by the website's list of files when there is one, as the website does.
// Each reference to something the index has becomes its places in the
// page; the page's places before are forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::ResolvePage(SWorkspacePage* psPage, const char* szText, CReferenceResolver* pcLive, CReferenceResolver* pcSite)
{
	CArrayPageReferences	asFound;
	CCPPIndexUnit			cPlaces;
	SPageReference*			psReference;
	size					i;
	bool					bSite;

	asFound.Init();
	pcLive->ResolvePage(szText, &asFound);
	cPlaces.Init();
	for (i = 0; i < asFound.NumElements(); i++)
	{
		psReference = asFound.Get(i);
		if (psReference->eKind == PRK_Warning)
		{
			psReference->psSymbol = NULL;
			continue;
		}

		bSite = false;
		if (pcSite && (CReferenceResolver::IsFileReference(psReference->szText.Text()) || (psReference->eKind == PRK_Error)))
		{
			psReference->szMessage.Clear();
			psReference->szFile.Clear();
			pcSite->ResolveReference(psReference->szText.Text(), psReference);
			bSite = psReference->eKind == PRK_Link;
		}
		if (bSite)
		{
			//The website's files are relative to its root, with '/' between folders.
			psReference->szFile.Insert(0, mszSiteRoot.Text());
			psReference->szFile.Replace('/', FILE_SEPARATOR[0]);
		}
		else if (psReference->eKind == PRK_Link)
		{
			if (psReference->psSymbol)
			{
				AddPagePlaces(&cPlaces, psPage->szFile.Text(), psReference, pcLive);
			}
			else
			{
				psReference->szFile.Replace('/', FILE_SEPARATOR[0]);
			}
		}
		psReference->psSymbol = NULL;
	}

	mcIndexMutex.Lock();
	mcIndex.RemoveFile(psPage->szFile.Text());
	mcIndex.Merge(&cPlaces);
	CReferenceResolver::KillPageReferences(&psPage->asReferences);
	memcpy(&psPage->asReferences, &asFound, sizeof(CArrayPageReferences));
	mcIndexMutex.Unlock();
	cPlaces.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The places a reference to an indexed symbol makes: one for each name in
// it ([`CShape::Init`] is a place of CShape and one of CShape::Init) and
// one for each type its parameters name, so a rename of any of them
// changes the reference.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::AddPagePlaces(CCPPIndexUnit* pcUnit, const char* szPage, SPageReference* psReference, CReferenceResolver* pcLive)
{
	CChars				szName;
	CChars				szRest;
	CChars				szPrefix;
	SSymbolTableEntry*	psEntry;
	const char*			szText;
	size				uiStart;
	size				uiEnd;
	size				uiRest;
	size				i;

	szName.Init();
	szRest.Init();
	CReferenceResolver::SplitName(psReference->szText.Text(), &szName, &szRest);

	//The names: each "::" ends one.
	szPrefix.Init();
	uiStart = 0;
	while (uiStart < szName.Length())
	{
		uiEnd = szName.Find(uiStart, "::");
		if (uiEnd == ARRAY_ELEMENT_NOT_FOUND)
		{
			uiEnd = szName.Length();
		}
		szPrefix.Clear();
		szPrefix.AppendSubString(szName.Text(), uiEnd);
		psEntry = (uiEnd == szName.Length()) ? psReference->psSymbol : pcLive->ResolveSymbol(szPrefix.Text());
		if (psEntry && (uiEnd > uiStart))
		{
			AddPagePlace(pcUnit, psEntry, szPage, psReference->iLine, psReference->iText + (int)uiStart, (int)(uiEnd - uiStart));
		}
		uiStart = uiEnd + 2;
	}

	//The types the parameters name: a run of a name's characters and "::".
	szText = psReference->szText.Text();
	uiRest = psReference->szText.Length() - szRest.Length();
	for (i = uiRest; i < psReference->szText.Length();)
	{
		if (!(isalpha((unsigned char)szText[i]) || (szText[i] == '_')))
		{
			i++;
			continue;
		}
		for (uiEnd = i; isalnum((unsigned char)szText[uiEnd]) || (szText[uiEnd] == '_') || ((szText[uiEnd] == ':') && (szText[uiEnd + 1] == ':')) || ((szText[uiEnd] == ':') && (uiEnd > 0) && (szText[uiEnd - 1] == ':')); uiEnd++)
		{
		}
		szPrefix.Clear();
		szPrefix.AppendSubString(&szText[i], uiEnd - i);
		psEntry = pcLive->ResolveSymbol(szPrefix.Text());
		if (psEntry && ((psEntry->eKind == CPPSK_Class) || (psEntry->eKind == CPPSK_ClassTemplate) || (psEntry->eKind == CPPSK_Enum) || (psEntry->eKind == CPPSK_Typedef) || (psEntry->eKind == CPPSK_AliasTemplate)))
		{
			AddPagePlace(pcUnit, psEntry, szPage, psReference->iLine, psReference->iText + (int)i, (int)(uiEnd - i));
		}
		i = uiEnd;
	}
	szPrefix.Kill();
	szRest.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::AddPagePlace(CCPPIndexUnit* pcUnit, SSymbolTableEntry* psEntry, const char* szPage, int iLine, int iColumn, int iLength)
{
	SCPPIndexEntry*	psPlace;
	bool			bAdded;

	psPlace = pcUnit->masEntries.Add();
	psPlace->uiKey = (uint32)pcUnit->AddKey(psEntry->szKey.Text(), psEntry->eKind);
	psPlace->uiFile = (uint32)pcUnit->mcFiles.FindOrAddIndex(szPage, &bAdded);
	psPlace->iLine = iLine;
	psPlace->iColumn = iColumn;
	psPlace->iLength = iLength;
	psPlace->uiRole = CPPIR_Reference;
	psPlace->uiFlags = 0;
	psPlace->uiUsage = CPPUT_Documentation;
	psPlace->iContainer = -1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CWorkspaceIndex::NumPages(void)
{
	size	uiPages;

	mcIndexMutex.Lock();
	uiPages = mapsPages.NumElements();
	mcIndexMutex.Unlock();
	return uiPages;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::IsPage(const char* szFileName)
{
	bool	bPage;

	mcIndexMutex.Lock();
	bPage = FindPage(szFileName) != NULL;
	mcIndexMutex.Unlock();
	return bPage;
}


//////////////////////////////////////////////////////////////////////////
//
// A copy of what a page's spans were found to be, added to pasDest (to be
// killed with CReferenceResolver::KillPageReferences); how many.
//
//////////////////////////////////////////////////////////////////////////
size CWorkspaceIndex::GetPageReferences(const char* szFileName, CArrayPageReferences* pasDest)
{
	SWorkspacePage*	psPage;
	SPageReference*	psFrom;
	SPageReference*	psTo;
	size			i;
	size			uiCopied;

	uiCopied = 0;
	mcIndexMutex.Lock();
	psPage = FindPage(szFileName);
	for (i = 0; (psPage != NULL) && (i < psPage->asReferences.NumElements()); i++)
	{
		psFrom = psPage->asReferences.Get(i);
		psTo = pasDest->Add();
		memcpy(psTo, psFrom, sizeof(SPageReference));
		psTo->szText.Init(psFrom->szText.Text());
		psTo->szFile.Init(psFrom->szFile.Text());
		psTo->szMessage.Init(psFrom->szMessage.Text());
		uiCopied++;
	}
	mcIndexMutex.Unlock();
	return uiCopied;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the reference at a page's iLine and iColumn (0 based) links to:
// its file and 1 based line (0 for a file).  False if there is no linked
// reference there.  A reference to an indexed symbol is a place in the
// index too.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::FindPageReference(const char* szFileName, int iLine, int iColumn, CChars* pszFile, int* piLine)
{
	SWorkspacePage*	psPage;
	SPageReference*	psReference;
	size			i;
	bool			bFound;

	bFound = false;
	mcIndexMutex.Lock();
	psPage = FindPage(szFileName);
	for (i = 0; (psPage != NULL) && (i < psPage->asReferences.NumElements()) && !bFound; i++)
	{
		psReference = psPage->asReferences.Get(i);
		if ((psReference->eKind == PRK_Link) && (psReference->iLine == iLine) && (iColumn >= psReference->iColumn) && (iColumn < psReference->iEnd))
		{
			pszFile->Set(psReference->szFile.Text());
			*piLine = psReference->iTargetLine;
			bFound = true;
		}
	}
	mcIndexMutex.Unlock();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Each of the workspace's pages, added to paszDest.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::GetPageFiles(CArrayChars* paszDest)
{
	size	i;

	mcIndexMutex.Lock();
	for (i = 0; i < mapsPages.NumElements(); i++)
	{
		paszDest->Add(mapsPages.GetPtr(i)->szFile.Text());
	}
	mcIndexMutex.Unlock();
}


bool CWorkspaceIndex::HasSiteSymbols(void) const { return mbSiteSymbols; }
char* CWorkspaceIndex::GetDocsSite(void) { return mszDocsSite.Text(); }

