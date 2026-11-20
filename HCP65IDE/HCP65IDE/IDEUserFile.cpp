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
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEUserFile.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEUserFile::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mapcParts.Init();
	PostInit();
}


void CIDEUserFile::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CArrayIDEUserParts, mapcParts);
}


void CIDEUserFile::Free(void)
{
	mapcParts.Kill();
}


bool CIDEUserFile::Save(CObjectWriter* pcFile) { return false; }
bool CIDEUserFile::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Something that keeps a part of the file.  The parts are written in the
// order they are added.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUserFile::AddPart(CIDEUserPart* pcPart)
{
	mapcParts.Add(&pcPart);
}


//////////////////////////////////////////////////////////////////////////
//
// <workspace>.hcp65user beside the workspace file, or the solution if
// there isn't one; empty with no solution open.
//
//////////////////////////////////////////////////////////////////////////
void CIDEUserFile::GetFileName(CChars* pszDest)
{
	mpcContent->GetDebugging()->GetUserSettingsFileName(pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// A solution has opened: each part takes what it wrote from the file,
// or has nothing if there is no file or it isn't one.  False if there
// wasn't one to read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUserFile::Read(void)
{
	CXMLFile	cFile;
	CFileUtil	cFileUtil;
	CChars		szFileName;
	CChars		szName;
	CChars		szDirectory;
	CMarkupTag*	pcRoot;
	size		i;
	bool		bResult;

	szFileName.Init();
	GetFileName(&szFileName);
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName.Text(), &szName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	cFile.Init();
	bResult = !szFileName.Empty() && cFileUtil.Exists(szFileName.Text()) && cFile.Read(szName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc;
	pcRoot = bResult ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	bResult = pcRoot && (StringCompare(pcRoot->GetName(), IDE_USER_SETTINGS_TAG) == 0);
	for (i = 0; i < mapcParts.NumElements(); i++)
	{
		(*mapcParts.Get(i))->ReadUser(bResult ? pcRoot : NULL);
	}
	cFile.Kill();
	szDirectory.Kill();
	szName.Kill();
	szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A part has changed: the file is written, each part its own.  Not
// without a solution open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEUserFile::Write(void)
{
	CXMLWriter	cWriter;
	CChars		szFileName;
	size		i;
	bool		bResult;

	if (!mpcContent->GetSolutions()->IsSolutionOpen())
	{
		return false;
	}
	szFileName.Init();
	GetFileName(&szFileName);
	if (szFileName.Empty())
	{
		szFileName.Kill();
		return false;
	}
	cWriter.Init();
	cWriter.Begin(IDE_USER_SETTINGS_TAG);
	for (i = 0; i < mapcParts.NumElements(); i++)
	{
		(*mapcParts.Get(i))->WriteUser(&cWriter);
	}
	cWriter.End();
	bResult = cWriter.Write(szFileName.Text());
	cWriter.Kill();
	szFileName.Kill();
	return bResult;
}

