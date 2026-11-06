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
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "BuildRecentFiles.h"


#define BUILD_RECENT_TAG		"RecentFiles"
#define BUILD_RECENT_FILE		"File"
#define BUILD_RECENT_PATH		"Path"
#define BUILD_RECENT_OPEN		"Open"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildRecentFiles::Init(size uiMaxFiles)
{
	maszFiles.Init();
	muiMaxFiles = uiMaxFiles;
	mszOpen.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildRecentFiles::Kill(void)
{
	mszOpen.Kill();
	maszFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szFile (made a full path) is the latest: first, and not anywhere else.
// The oldest past the most kept goes.
//
//////////////////////////////////////////////////////////////////////////
void CBuildRecentFiles::Add(const char* szFile)
{
	CArrayChars	aszFiles;
	CFileUtil	cFileUtil;
	CChars		szFull;
	size		i;

	szFull.Init(szFile);
	cFileUtil.FullPath(&szFull);
	aszFiles.Init();
	aszFiles.Add(szFull.Text());
	for (i = 0; (i < maszFiles.NumElements()) && (aszFiles.NumElements() < muiMaxFiles); i++)
	{
		if (!maszFiles.Get(i)->EqualsIgnoreCase(szFull.Text()))
		{
			aszFiles.Add(maszFiles.GetText(i));
		}
	}
	maszFiles.Kill();
	maszFiles.Init();
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		maszFiles.Add(aszFiles.GetText(i));
	}
	aszFiles.Kill();
	szFull.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szFile taken off (one that can't be opened any more).  False if it
// wasn't there.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildRecentFiles::Remove(const char* szFile)
{
	size	uiIndex;

	uiIndex = Find(szFile);
	if (uiIndex == MAX_SIZE)
	{
		return false;
	}
	maszFiles.Remove(uiIndex);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The index of szFile (any path to it, whatever its case), MAX_SIZE if it
// isn't there.
//
//////////////////////////////////////////////////////////////////////////
size CBuildRecentFiles::Find(const char* szFile)
{
	CFileUtil	cFileUtil;
	CChars		szFull;
	size		i;

	szFull.Init(szFile);
	cFileUtil.FullPath(&szFull);
	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		if (maszFiles.Get(i)->EqualsIgnoreCase(szFull.Text()))
		{
			szFull.Kill();
			return i;
		}
	}
	szFull.Kill();
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The files in szFile, in place of those here: false (and none) if it
// isn't there or isn't a list of them.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildRecentFiles::Read(const char* szFile)
{
	CXMLFile		cFile;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szDirectory;
	CMarkupTag*		pcRoot;
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szPath;
	bool			bResult;

	Clear();
	mszOpen.Clear();
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFile, &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	cFile.Init();
	bResult = cFileUtil.Exists(szFile) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc;
	pcRoot = bResult ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	bResult = pcRoot && (StringCompare(pcRoot->GetName(), BUILD_RECENT_TAG) == 0);
	if (bResult)
	{
		szPath = pcRoot->GetStringAttribute(BUILD_RECENT_OPEN);
		if (szPath)
		{
			mszOpen.Set(szPath);
		}
		for (pcTag = pcRoot->GetTag(BUILD_RECENT_FILE, &sIterator); pcTag && (maszFiles.NumElements() < muiMaxFiles); pcTag = pcRoot->GetNextTag(&sIterator))
		{
			szPath = pcTag->GetStringAttribute(BUILD_RECENT_PATH);
			if (szPath && (szPath[0] != '\0') && (Find(szPath) == MAX_SIZE))
			{
				maszFiles.Add(szPath);
			}
		}
	}
	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The files to szFile, its directory made if it isn't there.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildRecentFiles::Write(const char* szFile)
{
	CXMLWriter	cWriter;
	CFileUtil	cFileUtil;
	size		i;
	bool		bResult;

	cFileUtil.TouchDir(szFile, true);
	cWriter.Init();
	cWriter.Begin(BUILD_RECENT_TAG);
	if (!mszOpen.Empty())
	{
		cWriter.Attribute(BUILD_RECENT_OPEN, mszOpen.Text());
	}
	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		cWriter.Begin(BUILD_RECENT_FILE);
		cWriter.Attribute(BUILD_RECENT_PATH, maszFiles.GetText(i));
		cWriter.End();
	}
	cWriter.End();
	bResult = cWriter.Write(szFile);
	cWriter.Kill();
	return bResult;
}


void CBuildRecentFiles::Clear(void)				{ maszFiles.ReInit(); }
size CBuildRecentFiles::NumFiles(void)			{ return maszFiles.NumElements(); }
char* CBuildRecentFiles::GetFile(size uiIndex)	{ return maszFiles.GetText(uiIndex); }
void CBuildRecentFiles::SetOpen(const char* szFile)	{ mszOpen.Set(szFile ? szFile : ""); }  //The file open when the IDE was closed, "" for none.
char* CBuildRecentFiles::GetOpen(void)			{ return mszOpen.Text(); }

