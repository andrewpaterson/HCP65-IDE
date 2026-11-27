/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "BuildCard.h"


#define BUILD_CARD_TAG				"Card"
#define BUILD_CARD_IMAGE			"Image"
#define BUILD_CARD_PROGRAM			"Program"
#define BUILD_CARD_PROGRAM_FILE		"File"
#define BUILD_CARD_PROGRAM_PATH		"Path"
#define BUILD_CARD_PROGRAM_LINES	"Lines"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCard::Init(void)
{
	mszFile.Init();
	mszImage.Init();
	maszFiles.Init();
	maszPaths.Init();
	maszLines.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCard::Kill(void)
{
	maszLines.Kill();
	maszPaths.Kill();
	maszFiles.Kill();
	mszImage.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Replaces the card with szFile's.  False, and no programs, if it can't
// be read, has no image, or a program has no file or no path.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCard::Read(const char* szFile)
{
	CXMLFile		cFile;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szDirectory;
	CMarkupTag*		pcRoot;
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szProgram;
	char*			szPath;
	bool			bResult;

	Kill();
	Init();
	mszFile.Set(szFile);
	cFileUtil.FullPath(&mszFile);
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(mszFile.Text(), &szName, &szDirectory);

	cFile.Init();
	bResult = cFileUtil.Exists(mszFile.Text()) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc;
	pcRoot = bResult ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	bResult = (pcRoot != NULL) && pcRoot->Is(BUILD_CARD_TAG) && pcRoot->GetStringAttribute(BUILD_CARD_IMAGE) && (pcRoot->GetStringAttribute(BUILD_CARD_IMAGE)[0] != '\0');
	if (bResult)
	{
		mszImage.Set(pcRoot->GetStringAttribute(BUILD_CARD_IMAGE));
		for (pcTag = pcRoot->GetTag(BUILD_CARD_PROGRAM, &sIterator); pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
		{
			szProgram = pcTag->GetStringAttribute(BUILD_CARD_PROGRAM_FILE);
			szPath = pcTag->GetStringAttribute(BUILD_CARD_PROGRAM_PATH);
			if (!szProgram || (szProgram[0] == '\0') || !szPath || (szPath[0] == '\0'))
			{
				bResult = false;
				break;
			}
			AddProgram(szProgram, szPath, pcTag->GetStringAttribute(BUILD_CARD_PROGRAM_LINES));
		}
	}
	if (!bResult)
	{
		maszFiles.ReInit();
		maszPaths.ReInit();
		maszLines.ReInit();
	}
	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCard::Write(const char* szFile)
{
	CXMLWriter	cWriter;
	size		i;
	bool		bResult;

	cWriter.Init();
	cWriter.Begin(BUILD_CARD_TAG);
	cWriter.Attribute(BUILD_CARD_IMAGE, mszImage.Text());
	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		cWriter.Begin(BUILD_CARD_PROGRAM);
		cWriter.Attribute(BUILD_CARD_PROGRAM_FILE, maszFiles.GetText(i));
		cWriter.Attribute(BUILD_CARD_PROGRAM_PATH, maszPaths.GetText(i));
		if (maszLines.GetText(i)[0] != '\0')
		{
			cWriter.Attribute(BUILD_CARD_PROGRAM_LINES, maszLines.GetText(i));
		}
		cWriter.End();
	}
	cWriter.End();
	bResult = cWriter.Write(szFile);
	cWriter.Kill();
	if (bResult)
	{
		mszFile.Set(szFile);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCard::SetImage(const char* szImage)
{
	mszImage.Set(szImage);
}


//////////////////////////////////////////////////////////////////////////
//
// A program on the card: the file a build makes (relative to the card
// file; $(Configuration) and $(Platform) allowed), its path on the card,
// and the breakpoint space its line table is read in as it is linked
// (NULL or "" if it isn't).
//
//////////////////////////////////////////////////////////////////////////
void CBuildCard::AddProgram(const char* szFile, const char* szPath, const char* szLines)
{
	maszFiles.Add(szFile);
	maszPaths.Add(szPath);
	maszLines.Add(szLines ? szLines : "");
}


//////////////////////////////////////////////////////////////////////////
//
// szPath, relative to the card file, as a full path.
//
//////////////////////////////////////////////////////////////////////////
void CBuildCard::FullPath(const char* szPath, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;

	if (cFileUtil.IsAbsolutePath(szPath))
	{
		pszDest->Set(szPath);
	}
	else
	{
		szName.Init();
		pszDest->Set("");
		cFileUtil.SplitPath(mszFile.Text(), &szName, pszDest);
		cFileUtil.AppendToPath(pszDest, szPath);
		szName.Kill();
	}
	cFileUtil.FixSeparators(pszDest);  //The paths are Windows' (..\Boot\Boot.rom).
	cFileUtil.FullPath(pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The card's image, a full path.
//
//////////////////////////////////////////////////////////////////////////
void CBuildCard::GetImage(CChars* pszDest)
{
	FullPath(mszImage.Text(), pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// A program's file as the build configuration szConfiguration
// ("Release|HCP65C816") makes it, a full path: $(Configuration) is the
// name's first part and $(Platform) its second.
//
//////////////////////////////////////////////////////////////////////////
void CBuildCard::GetProgramFile(size uiIndex, const char* szConfiguration, CChars* pszDest)
{
	CChars		szFile;
	CChars		szPart;
	const char*	szBar;

	szFile.Init(maszFiles.GetText(uiIndex));
	szBar = strchr(szConfiguration, '|');
	szPart.Init();
	szPart.Append(szConfiguration, szBar ? (size)(szBar - szConfiguration) : strlen(szConfiguration));
	szFile.Replace("$(Configuration)", szPart.Text());
	szFile.Replace("$(Platform)", szBar ? szBar + 1 : "");
	szPart.Kill();
	FullPath(szFile.Text(), pszDest);
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The program whose file, as szConfiguration makes it, is szFile (a full
// path); ARRAY_ELEMENT_NOT_FOUND if the card has none.
//
//////////////////////////////////////////////////////////////////////////
size CBuildCard::FindProgram(const char* szFile, const char* szConfiguration)
{
	CChars	szProgram;
	size	i;

	szProgram.Init();
	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		GetProgramFile(i, szConfiguration, &szProgram);
		if (szProgram.Equals(szFile))
		{
			szProgram.Kill();
			return i;
		}
	}
	szProgram.Kill();
	return ARRAY_ELEMENT_NOT_FOUND;
}


char* CBuildCard::GetFile(void)						{ return mszFile.Text(); }
size CBuildCard::NumPrograms(void) const			{ return maszFiles.NumElements(); }
char* CBuildCard::GetProgramPath(size uiIndex)		{ return maszPaths.GetText(uiIndex); }
char* CBuildCard::GetProgramLines(size uiIndex)		{ return maszLines.GetText(uiIndex); }

