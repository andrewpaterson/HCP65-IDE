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
#include "BaseLib/StringHelper.h"
#include "BaseLib/FileUtil.h"
#include "Assembler65Lib/Asm65Attributes.h"
#include "BuildConfiguration.h"
#include "RunConfiguration.h"


#define RUN_CONFIGURATION_NAME			"Name"
#define RUN_CONFIGURATION_CONFIGURATION	"Configuration"
#define RUN_CONFIGURATION_TOOL			"Tool"
#define RUN_CONFIGURATION_DIRECTORY		"Directory"
#define RUN_CONFIGURATION_ARGUMENTS		"Arguments"
#define RUN_CONFIGURATION_CARD			"Card"
#define RUN_CONFIGURATION_SPACE			"BreakpointSpace"
#define RUN_CONFIGURATION_LOAD			"Load"
#define RUN_CONFIGURATION_LOAD_FILE		"File"
#define RUN_CONFIGURATION_LOAD_ADDRESS	"Address"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::Init(void)
{
	mszName.Init();
	mszConfiguration.Init();
	mszTool.Init();
	mszDirectory.Init();
	mszArguments.Init();
	mszCard.Init();
	maszLoadFiles.Init();
	mauiLoadAddresses.Init();
	mszBreakpointSpace.Init();
	mszShared.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::Init(const char* szName, const char* szConfiguration)
{
	Init();
	SetName(szName);
	SetConfiguration(szConfiguration);
}


//////////////////////////////////////////////////////////////////////////
//
// A copy of pcSource.
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::Init(CRunConfiguration* pcSource)
{
	Init(pcSource->GetName(), pcSource->GetConfiguration());
	size	i;

	SetTool(pcSource->GetTool(), pcSource->GetDirectory(), pcSource->GetArguments());
	SetCard(pcSource->GetCard());
	for (i = 0; i < pcSource->NumLoads(); i++)
	{
		AddLoad(pcSource->GetLoadFile(i), pcSource->GetLoadAddress(i));
	}
	SetBreakpointSpace(pcSource->GetBreakpointSpace());
	SetShared(pcSource->GetShared());
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::Kill(void)
{
	mszShared.Kill();
	mszBreakpointSpace.Kill();
	mauiLoadAddresses.Kill();
	maszLoadFiles.Kill();
	mszCard.Kill();
	mszArguments.Kill();
	mszDirectory.Kill();
	mszTool.Kill();
	mszConfiguration.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Only what is set is written.
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::SaveXML(CXMLWriter* pcWriter)
{
	size	i;

	pcWriter->Begin(RUN_CONFIGURATION_TAG);
	pcWriter->Attribute(RUN_CONFIGURATION_NAME, mszName.Text());
	pcWriter->Attribute(RUN_CONFIGURATION_CONFIGURATION, mszConfiguration.Text());
	if (!mszTool.Empty())
	{
		pcWriter->Attribute(RUN_CONFIGURATION_TOOL, mszTool.Text());
	}
	if (!mszDirectory.Empty())
	{
		pcWriter->Attribute(RUN_CONFIGURATION_DIRECTORY, mszDirectory.Text());
	}
	if (!mszArguments.Empty())
	{
		pcWriter->Attribute(RUN_CONFIGURATION_ARGUMENTS, mszArguments.Text());
	}
	if (!mszCard.Empty())
	{
		pcWriter->Attribute(RUN_CONFIGURATION_CARD, mszCard.Text());
	}
	if (!mszBreakpointSpace.Empty())
	{
		pcWriter->Attribute(RUN_CONFIGURATION_SPACE, mszBreakpointSpace.Text());
	}
	for (i = 0; i < maszLoadFiles.NumElements(); i++)
	{
		pcWriter->Begin(RUN_CONFIGURATION_LOAD);
		pcWriter->Attribute(RUN_CONFIGURATION_LOAD_FILE, maszLoadFiles.GetText(i));
		Asm65WriteHexAttribute(pcWriter, RUN_CONFIGURATION_LOAD_ADDRESS, mauiLoadAddresses.GetValue(i));
		pcWriter->End();
	}
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if it has no name, or a <Load> has no file or address.
//
//////////////////////////////////////////////////////////////////////////
bool CRunConfiguration::LoadXML(CMarkupTag* pcTag)
{
	char*			szName;
	char*			szConfiguration;
	char*			szTool;
	char*			szDirectory;
	char*			szArguments;
	char*			szFile;
	CMarkupTag*		pcLoad;
	STagIterator	sIterator;
	uint32			uiAddress;

	szName = pcTag->GetStringAttribute(RUN_CONFIGURATION_NAME);
	if (!szName || (szName[0] == '\0'))
	{
		return false;
	}
	szConfiguration = pcTag->GetStringAttribute(RUN_CONFIGURATION_CONFIGURATION);
	szTool = pcTag->GetStringAttribute(RUN_CONFIGURATION_TOOL);
	szDirectory = pcTag->GetStringAttribute(RUN_CONFIGURATION_DIRECTORY);
	szArguments = pcTag->GetStringAttribute(RUN_CONFIGURATION_ARGUMENTS);
	SetName(szName);
	SetConfiguration(szConfiguration);
	SetTool(szTool, szDirectory, szArguments);
	SetCard(pcTag->GetStringAttribute(RUN_CONFIGURATION_CARD));
	SetBreakpointSpace(pcTag->GetStringAttribute(RUN_CONFIGURATION_SPACE));
	ClearLoads();
	for (pcLoad = pcTag->GetTag(RUN_CONFIGURATION_LOAD, &sIterator); pcLoad; pcLoad = pcTag->GetNextTag(&sIterator))
	{
		szFile = pcLoad->GetStringAttribute(RUN_CONFIGURATION_LOAD_FILE);
		if (!szFile || (szFile[0] == '\0') || !Asm65ReadUint32Attribute(pcLoad, RUN_CONFIGURATION_LOAD_ADDRESS, &uiAddress))
		{
			return false;
		}
		AddLoad(szFile, uiAddress);
	}
	return true;
}


char* CRunConfiguration::GetName(void)			{ return mszName.Text(); }
char* CRunConfiguration::GetConfiguration(void)	{ return mszConfiguration.Text(); }
char* CRunConfiguration::GetTool(void)			{ return mszTool.Text(); }
char* CRunConfiguration::GetDirectory(void)		{ return mszDirectory.Text(); }
char* CRunConfiguration::GetArguments(void)		{ return mszArguments.Text(); }
bool CRunConfiguration::HasTool(void) const			{ return !mszTool.Empty(); }
bool CRunConfiguration::IsDefault(void)			{ return mszName.Equals(mszConfiguration.Text()) && mszTool.Empty() && mszDirectory.Empty() && mszArguments.Empty() && !HasDebugging() && !IsShared(); }  //Nothing but what a build configuration is given (CBuildWorkspace::AddDefaultRunConfigurations).
//Both as they are called now: a run configuration is named after its build configuration until it is given another.
void CRunConfiguration::SetName(const char* szName)						{ mszName.Set(""); CBuildConfiguration::CurrentName(szName, &mszName); }
void CRunConfiguration::SetConfiguration(const char* szConfiguration)	{ mszConfiguration.Set(""); CBuildConfiguration::CurrentName(szConfiguration, &mszConfiguration); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::SetTool(const char* szTool, const char* szDirectory, const char* szArguments)
{
	mszTool.Set(szTool ? szTool : "");
	mszDirectory.Set(szDirectory ? szDirectory : "");
	mszArguments.Set(szArguments ? szArguments : "");
}


//////////////////////////////////////////////////////////////////////////
//
// How an HCP65 program is debugged: its card file (NULL or "" for none),
// the files the emulator is given first, and a breakpoint space in place
// of the one worked out (NULL or "" to work it out).
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::SetCard(const char* szCard)					{ mszCard.Set(szCard ? szCard : ""); }
void CRunConfiguration::ClearLoads(void)								{ maszLoadFiles.ReInit(); mauiLoadAddresses.ReInit(); }
void CRunConfiguration::AddLoad(const char* szFile, uint32 uiAddress)	{ maszLoadFiles.Add(szFile); mauiLoadAddresses.Add(uiAddress); }
void CRunConfiguration::SetBreakpointSpace(const char* szSpace)		{ mszBreakpointSpace.Set(szSpace ? szSpace : ""); }
char* CRunConfiguration::GetCard(void)								{ return mszCard.Text(); }
size CRunConfiguration::NumLoads(void) const						{ return maszLoadFiles.NumElements(); }
char* CRunConfiguration::GetLoadFile(size uiIndex)					{ return maszLoadFiles.GetText(uiIndex); }
uint32 CRunConfiguration::GetLoadAddress(size uiIndex)				{ return mauiLoadAddresses.GetValue(uiIndex); }
char* CRunConfiguration::GetBreakpointSpace(void)					{ return mszBreakpointSpace.Text(); }
bool CRunConfiguration::HasDebugging(void) const					{ return !mszCard.Empty() || (maszLoadFiles.NumElements() > 0) || !mszBreakpointSpace.Empty(); }


//////////////////////////////////////////////////////////////////////////
//
// Shared: kept in the .run folder of szDirectory, a solution's folder (a
// full path), which its paths are relative to.  NULL or "" makes it
// private again.
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::SetShared(const char* szDirectory)	{ mszShared.Set(szDirectory ? szDirectory : ""); }
bool CRunConfiguration::IsShared(void) const				{ return !mszShared.Empty(); }
char* CRunConfiguration::GetShared(void)					{ return mszShared.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// A shared one's file: <its solution's folder>/.run/<Name>.hcp65run, the
// name's characters a file's can't be ("Release|HCP65C816") as "_".
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::GetSharedFile(CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	size		i;

	szName.Init(mszName);
	for (i = 0; i < szName.Length(); i++)
	{
		if (strchr("|\\/:*?\"<>", szName.GetChar(i)))
		{
			szName.SetChar(i, '_');
		}
	}
	szName.Append(RUN_CONFIGURATION_EXTENSION);
	pszDest->Set(mszShared);
	cFileUtil.AppendToPath(pszDest, RUN_CONFIGURATION_FOLDER);
	cFileUtil.AppendToPath(pszDest, szName.Text());
	szName.Kill();
}
