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
#include "BaseLib/FileUtil.h"
#include "BaseLib/YAMLFile.h"
#include "BaseLib/TypeNames.h"
#include "DocsSite.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CDocsSite::Init(void)
{
	masSources.Init();
	maszSnippetFolders.Init();
	mbRead = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CDocsSite::Kill(void)
{
	size	i;

	for (i = 0; i < masSources.NumElements(); i++)
	{
		masSources.Get(i)->szSite.Kill();
		masSources.Get(i)->szFolder.Kill();
	}
	masSources.Kill();
	maszSnippetFolders.Kill();
	mbRead = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The website's map from its mkdocs.yml.  False if it can't be read or
// has no extra.sources.
//
//////////////////////////////////////////////////////////////////////////
bool CDocsSite::Read(const char* szMkdocsFile)
{
	CYAMLFile			cFile;
	CFileUtil			cFileUtil;
	CChars				szName;
	CChars				szWebsite;
	CChars				szRoot;
	CMarkupTag*			pcRoot;
	CMarkupTag*			pcExtra;
	CMarkupTag*			pcSources;
	CMarkupTag*			pcSource;
	STagIterator		sIter;
	SDocsSiteSource*	psSource;
	const char*			szFolder;
	const char*			szSite;

	szName.Init();
	szWebsite.Init();
	cFileUtil.SplitPath(szMkdocsFile, &szName, &szWebsite);
	cFileUtil.FullPath(&szWebsite);
	szRoot.Init(szWebsite);
	cFileUtil.AppendToPath(&szRoot, "..");
	cFileUtil.AppendToPath(&szRoot, "..");
	cFileUtil.FullPath(&szRoot);

	cFile.Init();
	pcExtra = NULL;
	pcSources = NULL;
	pcRoot = NULL;
	if (cFile.Read(szName.Text(), szWebsite.Text()) && cFile.mcMarkup.mpcDoc)
	{
		pcRoot = cFile.mcMarkup.mpcDoc->GetRootTag();
		pcExtra = pcRoot ? pcRoot->GetTag("extra") : NULL;
		pcSources = pcExtra ? pcExtra->GetTag("sources") : NULL;
	}
	for (pcSource = pcSources ? pcSources->GetTag(&sIter) : NULL; pcSource != NULL; pcSource = pcSources->GetNextTag(&sIter))
	{
		szFolder = pcSource->GetStringAttribute("folder");
		szSite = pcSource->GetStringAttribute("site");
		if (szFolder == NULL)
		{
			continue;
		}
		psSource = masSources.Add();
		psSource->szFolder.Init(szRoot);
		cFileUtil.AppendToPath(&psSource->szFolder, szFolder);
		cFileUtil.FixSeparators(&psSource->szFolder);
		cFileUtil.FullPath(&psSource->szFolder);
		while (psSource->szFolder.EndsWith(FILE_SEPARATOR[0]))
		{
			psSource->szFolder.RemoveFromEnd(1);
		}
		psSource->szSite.Init(szSite ? szSite : "");
		while (psSource->szSite.StartsWith('/'))
		{
			psSource->szSite.RemoveFromStart(1);
		}
		while (psSource->szSite.EndsWith('/'))
		{
			psSource->szSite.RemoveFromEnd(1);
		}
	}
	if (pcRoot)
	{
		ReadSnippetFolders(pcRoot, szWebsite.Text());
	}
	mbRead = masSources.NumElements() > 0;

	cFile.Kill();
	szRoot.Kill();
	szWebsite.Kill();
	szName.Kill();
	return mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// pymdownx.snippets' base_path: a folder, or a list of them, relative to
// the website's folder, under markdown_extensions.
//
//////////////////////////////////////////////////////////////////////////
void CDocsSite::ReadSnippetFolders(CMarkupTag* pcRoot, const char* szWebsite)
{
	CMarkupTag*		pcExtensions;
	CMarkupTag*		pcExtension;
	CMarkupTag*		pcSnippets;
	CMarkupTag*		pcBasePath;
	STagIterator	sIter;
	CFileUtil		cFileUtil;
	CChars*			pszFolder;
	const char*		szBasePath;
	char*			szName;
	void*			pvValue;
	uint			uiType;
	size			i;

	pcExtensions = pcRoot->GetTag("markdown_extensions");
	for (pcExtension = pcExtensions ? pcExtensions->GetTag(&sIter) : NULL; pcExtension != NULL; pcExtension = pcExtensions->GetNextTag(&sIter))
	{
		pcSnippets = pcExtension->GetTag("pymdownx.snippets");
		if (pcSnippets == NULL)
		{
			continue;
		}
		szBasePath = pcSnippets->GetStringAttribute("base_path");
		if (szBasePath)
		{
			pszFolder = maszSnippetFolders.Add(szWebsite);
			cFileUtil.AppendToPath(pszFolder, szBasePath);
			cFileUtil.FixSeparators(pszFolder);
			cFileUtil.FullPath(pszFolder);
		}
		pcBasePath = pcSnippets->GetTag("base_path");
		for (i = 0; (pcBasePath != NULL) && (i < pcBasePath->GetNumAttributes()); i++)
		{
			szName = NULL;
			pvValue = pcBasePath->GetAttribute(i, &uiType, &szName);
			if ((uiType == PT_char8Pointer) && pvValue)
			{
				pszFolder = maszSnippetFolders.Add(szWebsite);
				cFileUtil.AppendToPath(pszFolder, (const char*)pvValue);
				cFileUtil.FixSeparators(pszFolder);
				cFileUtil.FullPath(pszFolder);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where szFile lands in the website ("ide/editor.md"), false if no folder
// of the map has it.
//
//////////////////////////////////////////////////////////////////////////
bool CDocsSite::GetSitePath(const char* szFile, CChars* pszDest)
{
	CFileUtil			cFileUtil;
	CChars				szFull;
	SDocsSiteSource*	psSource;
	size				i;
	bool				bFound;

	szFull.Init(szFile);
	cFileUtil.FullPath(&szFull);
	bFound = false;
	for (i = 0; (i < masSources.NumElements()) && !bFound; i++)
	{
		psSource = masSources.Get(i);
		if (szFull.StartsWith(psSource->szFolder.Text()) && (szFull.GetChar(psSource->szFolder.Length()) == FILE_SEPARATOR[0]))
		{
			pszDest->Set(psSource->szSite);
			if (!pszDest->Empty())
			{
				pszDest->Append('/');
			}
			pszDest->Append(&szFull.Text()[psSource->szFolder.Length() + 1]);
			pszDest->Replace(FILE_SEPARATOR[0], '/');
			bFound = true;
		}
	}
	szFull.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The file that lands at szSitePath, the first folder of the map that has
// it; false if none has.
//
//////////////////////////////////////////////////////////////////////////
bool CDocsSite::GetFile(const char* szSitePath, CChars* pszDest)
{
	CFileUtil			cFileUtil;
	SDocsSiteSource*	psSource;
	size				i;
	size				uiSite;
	const char*			szInside;

	for (i = 0; i < masSources.NumElements(); i++)
	{
		psSource = masSources.Get(i);
		uiSite = psSource->szSite.Length();
		if (uiSite == 0)
		{
			szInside = szSitePath;
		}
		else if ((strncmp(szSitePath, psSource->szSite.Text(), uiSite) == 0) && (szSitePath[uiSite] == '/'))
		{
			szInside = &szSitePath[uiSite + 1];
		}
		else
		{
			continue;
		}
		pszDest->Set(psSource->szFolder);
		cFileUtil.AppendToPath(pszDest, szInside);
		cFileUtil.FixSeparators(pszDest);
		if (cFileUtil.Exists(pszDest->Text()) && !cFileUtil.IsDirectory(pszDest->Text()))
		{
			return true;
		}
	}
	pszDest->Clear();
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The file a link in szPage names: its target relative to the page's
// place in the website, or without a map to the page's folder.  False if
// it names no file (another website, an anchor in the page, a folder, a
// file that isn't there).
//
//////////////////////////////////////////////////////////////////////////
bool CDocsSite::ResolveLink(const char* szPage, const char* szTarget, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szPath;
	CChars		szSitePath;
	CChars		szName;
	CChars		szFolder;
	size		uiSlash;
	bool		bFound;

	if (!IsLocalLink(szTarget))
	{
		return false;
	}
	szPath.Init();
	GetLinkPath(szTarget, &szPath);
	bFound = false;
	szSitePath.Init();
	if (!szPath.Empty() && mbRead && GetSitePath(szPage, &szSitePath))
	{
		uiSlash = szSitePath.FindFromEnd('/');
		szSitePath.SetLength((uiSlash == ARRAY_ELEMENT_NOT_FOUND) ? 0 : uiSlash + 1);
		szSitePath.Append(szPath);
		bFound = Normalise(&szSitePath) && GetFile(szSitePath.Text(), pszDest);
	}
	else if (!szPath.Empty() && !mbRead)
	{
		szName.Init();
		szFolder.Init();
		cFileUtil.SplitPath(szPage, &szName, &szFolder);
		pszDest->Set(szFolder);
		cFileUtil.AppendToPath(pszDest, szPath.Text());
		cFileUtil.FixSeparators(pszDest);
		cFileUtil.FullPath(pszDest);
		bFound = cFileUtil.Exists(pszDest->Text()) && !cFileUtil.IsDirectory(pszDest->Text());
		szFolder.Kill();
		szName.Kill();
	}
	szSitePath.Kill();
	szPath.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The file a snippet's path names: in the first of the snippets' folders
// that has it.  False if none has (or there is no map).
//
//////////////////////////////////////////////////////////////////////////
bool CDocsSite::ResolveSnippet(const char* szPage, const char* szPath, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	size		i;

	for (i = 0; i < maszSnippetFolders.NumElements(); i++)
	{
		pszDest->Set(maszSnippetFolders.GetText(i));
		cFileUtil.AppendToPath(pszDest, szPath);
		cFileUtil.FixSeparators(pszDest);
		cFileUtil.FullPath(pszDest);
		if (cFileUtil.Exists(pszDest->Text()) && !cFileUtil.IsDirectory(pszDest->Text()))
		{
			return true;
		}
	}
	pszDest->Clear();
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A link to a file of the website: not to another website (a scheme),
// nor an anchor in its own page, nor from the website's top (/).
//
//////////////////////////////////////////////////////////////////////////
bool CDocsSite::IsLocalLink(const char* szTarget)
{
	return (szTarget[0] != '\0') && (szTarget[0] != '#') && (szTarget[0] != '/') && (strstr(szTarget, "://") == NULL) && (strncmp(szTarget, "mailto:", 7) != 0);
}


//////////////////////////////////////////////////////////////////////////
//
// A link's target without its #anchor or ?query.
//
//////////////////////////////////////////////////////////////////////////
void CDocsSite::GetLinkPath(const char* szTarget, CChars* pszDest)
{
	size	uiLength;

	uiLength = strcspn(szTarget, "#?");
	pszDest->Clear();
	pszDest->AppendSubString(szTarget, uiLength);
}


//////////////////////////////////////////////////////////////////////////
//
// A site path's "." and ".." taken out.  False if it climbs out of the
// website.
//
//////////////////////////////////////////////////////////////////////////
bool CDocsSite::Normalise(CChars* pszSitePath)
{
	CArrayChars	aszParts;
	CArrayChars	aszKept;
	size		i;
	bool		bInside;

	aszParts.Init();
	aszParts.Split(pszSitePath->Text(), '/');
	aszKept.Init();
	bInside = true;
	for (i = 0; (i < aszParts.NumElements()) && bInside; i++)
	{
		if ((aszParts.GetText(i)[0] == '\0') || (strcmp(aszParts.GetText(i), ".") == 0))
		{
			continue;
		}
		if (strcmp(aszParts.GetText(i), "..") == 0)
		{
			bInside = aszKept.NumElements() > 0;
			if (bInside)
			{
				aszKept.RemoveTail();
			}
			continue;
		}
		aszKept.Add(aszParts.GetText(i));
	}
	pszSitePath->Clear();
	for (i = 0; i < aszKept.NumElements(); i++)
	{
		if (i > 0)
		{
			pszSitePath->Append('/');
		}
		pszSitePath->Append(aszKept.GetText(i));
	}
	aszKept.Kill();
	aszParts.Kill();
	return bInside;
}


bool CDocsSite::IsRead(void) const { return mbRead; }
size CDocsSite::NumSources(void) const { return masSources.NumElements(); }

