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
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "VcxSolution.h"


//////////////////////////////////////////////////////////////////////////
//
// Splits "Key = Value" (both trimmed).  False if there is no '='.
//
//////////////////////////////////////////////////////////////////////////
bool SplitVcxSolutionLine(CChars* pszLine, CChars* pszKey, CChars* pszValue)
{
	size	uiEquals;

	uiEquals = pszLine->Find('=');
	if (uiEquals == ARRAY_ELEMENT_NOT_FOUND)
	{
		return false;
	}
	pszKey->AppendSubString(pszLine, 0, uiEquals);
	pszKey->StripWhitespace();
	pszValue->AppendSubString(pszLine, uiEquals + 1, pszLine->Length());
	pszValue->StripWhitespace();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The name in "GlobalSection(Name) = preSolution" when the line starts
// with szStart ("GlobalSection(").
//
//////////////////////////////////////////////////////////////////////////
bool GetVcxSolutionSectionName(CChars* pszLine, const char* szStart, CChars* pszName)
{
	size	uiStart;
	size	uiEnd;

	if (!pszLine->StartsWith(szStart))
	{
		return false;
	}
	uiStart = strlen(szStart);
	uiEnd = pszLine->Find(uiStart, ')');
	if (uiEnd == ARRAY_ELEMENT_NOT_FOUND)
	{
		return false;
	}
	pszName->AppendSubString(pszLine, uiStart, uiEnd);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolution::Init(void)
{
	mszFileName.Init();
	mszDirectory.Init();
	mszName.Init();
	mszFormatVersion.Init();
	mszVisualStudioVersion.Init();
	mszMinimumVisualStudioVersion.Init();
	mszSolutionGuid.Init();
	mbHideSolutionNode = false;
	maszConfigurations.Init();
	mapcProjects.Init();

	meState = VSS_Top;
	mszSection.Init();
	mpcCurrent = NULL;
	muiLine = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolution::Kill(void)
{
	size					i;
	CVcxSolutionProject*	pcProject;

	mszSection.Kill();
	for (i = 0; i < mapcProjects.NumElements(); i++)
	{
		pcProject = mapcProjects.GetPtr(i);
		SafeKill(pcProject);
	}
	mapcProjects.Kill();
	maszConfigurations.Kill();
	mszSolutionGuid.Kill();
	mszMinimumVisualStudioVersion.Kill();
	mszVisualStudioVersion.Kill();
	mszFormatVersion.Kill();
	mszName.Kill();
	mszDirectory.Kill();
	mszFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the .sln szFileName and, if bLoadProjects, each project's
// .vcxproj.  A project that can't be read is left unloaded (Visual
// Studio shows it as "(unloaded)"); only a .sln that can't be read or
// parsed is false.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::Read(const char* szFileName, bool bLoadProjects)
{
	CFileUtil	cFileUtil;
	CTextFile	cFile;
	bool		bResult;
	size		i;

	mszFileName.Set(szFileName);
	cFileUtil.FullPath(&mszFileName);
	mszDirectory.Clear();
	mszName.Clear();
	cFileUtil.SplitPath(mszFileName.Text(), &mszName, &mszDirectory);
	cFileUtil.RemoveExtension(&mszName);

	cFile.Init();
	if (!cFile.Read(mszFileName.Text()))
	{
		cFile.Kill();
		return gcLogger.Error2(__METHOD__, " Could not read solution [", szFileName, "].", NULL);
	}
	bResult = ReadText(cFile.Text());
	cFile.Kill();
	if (!bResult)
	{
		return false;
	}

	if (bLoadProjects)
	{
		for (i = 0; i < mapcProjects.NumElements(); i++)
		{
			mapcProjects.GetPtr(i)->Load(mszDirectory.Text());
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution Visual Studio makes when a .vcxproj is opened on its own:
// named after the project, in its directory, with only it in it.  False
// if the project can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::ReadProject(const char* szProjectFile)
{
	CFileUtil				cFileUtil;
	CChars					szProjectName;
	CChars					szFileName;
	CVcxSolutionProject*	pcProject;
	CVcxProject*			pcVcxProject;
	size					i;

	mszFileName.Set(szProjectFile);
	cFileUtil.FullPath(&mszFileName);
	mszDirectory.Clear();
	szFileName.Init();
	cFileUtil.SplitPath(mszFileName.Text(), &szFileName, &mszDirectory);
	szProjectName.Init(szFileName);
	cFileUtil.RemoveExtension(&szProjectName);
	mszName.Set(szProjectName);
	cFileUtil.RemoveExtension(&mszFileName);
	mszFileName.Append(".sln");

	pcProject = NewMalloc<CVcxSolutionProject>();
	pcProject->Init(VCX_CPP_PROJECT_TYPE, szProjectName.Text(), szFileName.Text(), "");
	mapcProjects.Add(pcProject);
	szProjectName.Kill();
	szFileName.Kill();

	if (!pcProject->Load(mszDirectory.Text()))
	{
		return false;
	}

	//Its configurations are the solution's, each building itself.
	pcVcxProject = pcProject->GetProject();
	pcProject->SetGuid(pcVcxProject->GetGuid());
	for (i = 0; i < pcVcxProject->NumConfigurations(); i++)
	{
		maszConfigurations.Add(pcVcxProject->GetConfiguration(i)->GetName());
		pcProject->GetOrAddConfiguration(pcVcxProject->GetConfiguration(i)->GetName())->SetProjectConfiguration(pcVcxProject->GetConfiguration(i)->GetName());
		pcProject->GetConfiguration(i)->SetBuild(true);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The .sln's lines, after its UTF-8 byte order mark when it has one.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::ReadText(char* szText)
{
	CChars		szAll;
	CArrayChars	aszLines;
	size		i;
	bool		bResult;

	if (((uint8)szText[0] == 0xEF) && ((uint8)szText[1] == 0xBB) && ((uint8)szText[2] == 0xBF))
	{
		szText += 3;
	}

	szAll.Init(szText);
	aszLines.Init();
	szAll.SplitLines(&aszLines);
	szAll.Kill();

	meState = VSS_Top;
	mpcCurrent = NULL;
	bResult = true;
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		muiLine = i + 1;
		aszLines.Get(i)->StripWhitespace(true);
		if (!ReadLine(aszLines.Get(i)))
		{
			bResult = false;
			break;
		}
	}
	aszLines.Kill();

	if (bResult && (meState != VSS_Top))
	{
		muiLine = 0;
		bResult = Error("ended inside a Project or Global.");
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::ReadLine(CChars* pszLine)
{
	CChars	szKey;
	CChars	szValue;
	bool	bResult;

	if (pszLine->Empty())
	{
		return true;
	}

	switch (meState)
	{
	case VSS_Top:
		if (pszLine->StartsWith("Project("))
		{
			return ReadProjectLine(pszLine);
		}
		else if (pszLine->Equals("Global"))
		{
			meState = VSS_Global;
			return true;
		}
		else if (pszLine->StartsWith("Microsoft Visual Studio Solution File, Format Version "))
		{
			mszFormatVersion.Set(pszLine->Text(strlen("Microsoft Visual Studio Solution File, Format Version ")));
			return true;
		}
		else if (pszLine->StartsWith("#"))
		{
			return true;
		}
		szKey.Init();
		szValue.Init();
		if (SplitVcxSolutionLine(pszLine, &szKey, &szValue))
		{
			if (szKey.Equals("VisualStudioVersion"))
			{
				mszVisualStudioVersion.Set(szValue);
			}
			else if (szKey.Equals("MinimumVisualStudioVersion"))
			{
				mszMinimumVisualStudioVersion.Set(szValue);
			}
		}
		szValue.Kill();
		szKey.Kill();
		return true;

	case VSS_Project:
		if (pszLine->Equals("EndProject"))
		{
			mpcCurrent = NULL;
			meState = VSS_Top;
			return true;
		}
		mszSection.Clear();
		if (GetVcxSolutionSectionName(pszLine, "ProjectSection(", &mszSection))
		{
			meState = VSS_ProjectSection;
			return true;
		}
		return Error("expected ProjectSection or EndProject.");

	case VSS_Global:
		if (pszLine->Equals("EndGlobal"))
		{
			meState = VSS_Top;
			return true;
		}
		mszSection.Clear();
		if (GetVcxSolutionSectionName(pszLine, "GlobalSection(", &mszSection))
		{
			meState = VSS_GlobalSection;
			return true;
		}
		return Error("expected GlobalSection or EndGlobal.");

	case VSS_ProjectSection:
	case VSS_GlobalSection:
		if (pszLine->Equals((meState == VSS_ProjectSection) ? "EndProjectSection" : "EndGlobalSection"))
		{
			meState = (meState == VSS_ProjectSection) ? VSS_Project : VSS_Global;
			return true;
		}
		szKey.Init();
		szValue.Init();
		bResult = true;
		if (SplitVcxSolutionLine(pszLine, &szKey, &szValue))
		{
			bResult = ReadSectionLine(szKey.Text(), szValue.Text());
		}
		szValue.Kill();
		szKey.Kill();
		return bResult;
	}
	return Error("is in an unknown state.");
}


//////////////////////////////////////////////////////////////////////////
//
// Project("{Type GUID}") = "Name", "Path", "{GUID}"
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::ReadProjectLine(CChars* pszLine)
{
	CArrayChars				aszQuoted;
	size					uiStart;
	size					uiEnd;
	CVcxSolutionProject*	pcProject;

	aszQuoted.Init();
	uiStart = pszLine->Find('"');
	while (uiStart != ARRAY_ELEMENT_NOT_FOUND)
	{
		uiEnd = pszLine->Find(uiStart + 1, '"');
		if (uiEnd == ARRAY_ELEMENT_NOT_FOUND)
		{
			break;
		}
		aszQuoted.Add(pszLine->Text(), uiStart + 1, uiEnd);
		uiStart = pszLine->Find(uiEnd + 1, '"');
	}

	if (aszQuoted.NumElements() != 4)
	{
		aszQuoted.Kill();
		return Error("has a Project line without a type, name, path and GUID.");
	}

	pcProject = NewMalloc<CVcxSolutionProject>();
	pcProject->Init(aszQuoted.GetText(0), aszQuoted.GetText(1), aszQuoted.GetText(2), aszQuoted.GetText(3));
	mapcProjects.Add(pcProject);
	mpcCurrent = pcProject;
	meState = VSS_Project;
	aszQuoted.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A "Key = Value" line in the current ProjectSection or GlobalSection.
// Sections Visual Studio uses for other kinds of project (or that only
// its extensions read) are skipped.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::ReadSectionLine(const char* szKey, const char* szValue)
{
	CVcxSolutionProject*	pcProject;

	if (meState == VSS_ProjectSection)
	{
		if (mszSection.Equals("SolutionItems"))
		{
			mpcCurrent->AddSolutionItem(szKey);
		}
		else if (mszSection.Equals("ProjectDependencies"))
		{
			mpcCurrent->AddDependency(szKey);
		}
		return true;
	}

	if (mszSection.Equals("SolutionConfigurationPlatforms"))
	{
		maszConfigurations.AddIfUnique((char*)szKey);
	}
	else if (mszSection.Equals("ProjectConfigurationPlatforms"))
	{
		return ReadProjectConfiguration(szKey, szValue);
	}
	else if (mszSection.Equals("NestedProjects"))
	{
		pcProject = FindProject(szKey);
		if (pcProject == NULL)
		{
			return Error("nests a project that isn't in the solution.");
		}
		pcProject->SetParentGuid(szValue);
	}
	else if (mszSection.Equals("SolutionProperties"))
	{
		if (StringCompare(szKey, "HideSolutionNode") == 0)
		{
			mbHideSolutionNode = StringInsensitiveCompare(szValue, "TRUE") == 0;
		}
	}
	else if (mszSection.Equals("ExtensibilityGlobals"))
	{
		if (StringCompare(szKey, "SolutionGuid") == 0)
		{
			mszSolutionGuid.Set(szValue);
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// {GUID}.Debug|x64.ActiveCfg = Debug|x64 says the project configuration
// the solution configuration builds; .Build.0 that it is built and
// .Deploy.0 that it is deployed.  A project the solution doesn't have is
// skipped, as Visual Studio skips it.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::ReadProjectConfiguration(const char* szKey, const char* szValue)
{
	CChars								szKeyChars;
	CChars								szGuid;
	CChars								szConfiguration;
	size								uiDot;
	CVcxSolutionProject*				pcProject;
	CVcxSolutionProjectConfiguration*	pcConfiguration;
	int									iKind;

	szKeyChars.Init(szKey);
	uiDot = szKeyChars.Find('}');
	if ((uiDot == ARRAY_ELEMENT_NOT_FOUND) || (szKeyChars.GetChar(uiDot + 1) != '.'))
	{
		szKeyChars.Kill();
		return Error("has a project configuration without a {GUID}.");
	}

	if (szKeyChars.EndsWith(".ActiveCfg"))
	{
		iKind = 0;
		szKeyChars.RemoveFromEnd(strlen(".ActiveCfg"));
	}
	else if (szKeyChars.EndsWith(".Build.0"))
	{
		iKind = 1;
		szKeyChars.RemoveFromEnd(strlen(".Build.0"));
	}
	else if (szKeyChars.EndsWith(".Deploy.0"))
	{
		iKind = 2;
		szKeyChars.RemoveFromEnd(strlen(".Deploy.0"));
	}
	else
	{
		szKeyChars.Kill();
		return true;
	}

	szGuid.Init(szKeyChars.Text(), 0, uiDot + 1);
	szConfiguration.Init(szKeyChars.Text(), uiDot + 2);
	pcProject = FindProject(szGuid.Text());
	if (pcProject != NULL)
	{
		pcConfiguration = pcProject->GetOrAddConfiguration(szConfiguration.Text());
		if (iKind == 0)
		{
			pcConfiguration->SetProjectConfiguration(szValue);
		}
		else if (iKind == 1)
		{
			pcConfiguration->SetBuild(true);
		}
		else
		{
			pcConfiguration->SetDeploy(true);
		}
	}
	szConfiguration.Kill();
	szGuid.Kill();
	szKeyChars.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolution::Error(const char* szMessage)
{
	CChars	szLine;
	bool	bResult;

	szLine.Init();
	if (muiLine != 0)
	{
		szLine.Append(" line ");
		szLine.Append((uint32)muiLine);
	}
	bResult = gcLogger.Error2(__METHOD__, " Solution [", mszFileName.Text(), "]", szLine.Text(), " ", szMessage, NULL);
	szLine.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// GUIDs aren't case sensitive: projects reference each other in lower
// case while the .sln has them in upper case.
//
//////////////////////////////////////////////////////////////////////////
CVcxSolutionProject* CVcxSolution::FindProject(const char* szGuid)
{
	size					i;
	CVcxSolutionProject*	pcProject;

	for (i = 0; i < mapcProjects.NumElements(); i++)
	{
		pcProject = mapcProjects.GetPtr(i);
		if (StringInsensitiveCompare(pcProject->GetGuid(), szGuid) == 0)
		{
			return pcProject;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CVcxSolutionProject* CVcxSolution::FindProjectByName(const char* szName)
{
	size					i;
	CVcxSolutionProject*	pcProject;

	for (i = 0; i < mapcProjects.NumElements(); i++)
	{
		pcProject = mapcProjects.GetPtr(i);
		if (StringInsensitiveCompare(pcProject->GetName(), szName) == 0)
		{
			return pcProject;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The indices of the projects and folders in the solution folder
// szParentGuid, or at the top of the solution when it is "".
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolution::GetChildProjects(const char* szParentGuid, CArrayInt* paiProjects)
{
	size					i;
	CVcxSolutionProject*	pcProject;

	for (i = 0; i < mapcProjects.NumElements(); i++)
	{
		pcProject = mapcProjects.GetPtr(i);
		if (StringInsensitiveCompare(pcProject->GetParentGuid(), szParentGuid) == 0)
		{
			paiProjects->Add((int)i);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CVcxSolution::NumLoadedProjects(void)
{
	size	i;
	size	uiLoaded;

	uiLoaded = 0;
	for (i = 0; i < mapcProjects.NumElements(); i++)
	{
		if (mapcProjects.GetPtr(i)->IsLoaded())
		{
			uiLoaded++;
		}
	}
	return uiLoaded;
}


char* CVcxSolution::GetFileName(void) { return mszFileName.Text(); }
char* CVcxSolution::GetDirectory(void) { return mszDirectory.Text(); }
char* CVcxSolution::GetName(void) { return mszName.Text(); }
char* CVcxSolution::GetFormatVersion(void) { return mszFormatVersion.Text(); }
char* CVcxSolution::GetVisualStudioVersion(void) { return mszVisualStudioVersion.Text(); }
char* CVcxSolution::GetMinimumVisualStudioVersion(void) { return mszMinimumVisualStudioVersion.Text(); }
char* CVcxSolution::GetSolutionGuid(void) { return mszSolutionGuid.Text(); }
bool CVcxSolution::IsHideSolutionNode(void) { return mbHideSolutionNode; }
size CVcxSolution::NumConfigurations(void) { return maszConfigurations.NumElements(); }
char* CVcxSolution::GetConfiguration(size uiIndex) { return maszConfigurations.GetText(uiIndex); }
size CVcxSolution::NumProjects(void) { return mapcProjects.NumElements(); }
CVcxSolutionProject* CVcxSolution::GetProject(size uiIndex) { return mapcProjects.GetPtr(uiIndex); }

