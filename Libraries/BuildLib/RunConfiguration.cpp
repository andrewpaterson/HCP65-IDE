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
#include "BaseLib/StringHelper.h"
#include "RunConfiguration.h"


#define RUN_CONFIGURATION_NAME			"Name"
#define RUN_CONFIGURATION_CONFIGURATION	"Configuration"
#define RUN_CONFIGURATION_TOOL			"Tool"
#define RUN_CONFIGURATION_DIRECTORY		"Directory"
#define RUN_CONFIGURATION_ARGUMENTS		"Arguments"


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
	SetTool(pcSource->GetTool(), pcSource->GetDirectory(), pcSource->GetArguments());
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CRunConfiguration::Kill(void)
{
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
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if it has no name.
//
//////////////////////////////////////////////////////////////////////////
bool CRunConfiguration::LoadXML(CMarkupTag* pcTag)
{
	char*	szName;
	char*	szConfiguration;
	char*	szTool;
	char*	szDirectory;
	char*	szArguments;

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
	return true;
}


char* CRunConfiguration::GetName(void)			{ return mszName.Text(); }
char* CRunConfiguration::GetConfiguration(void)	{ return mszConfiguration.Text(); }
char* CRunConfiguration::GetTool(void)			{ return mszTool.Text(); }
char* CRunConfiguration::GetDirectory(void)		{ return mszDirectory.Text(); }
char* CRunConfiguration::GetArguments(void)		{ return mszArguments.Text(); }
bool CRunConfiguration::HasTool(void)			{ return !mszTool.Empty(); }
void CRunConfiguration::SetName(const char* szName)						{ mszName.Set(szName ? szName : ""); }
void CRunConfiguration::SetConfiguration(const char* szConfiguration)	{ mszConfiguration.Set(szConfiguration ? szConfiguration : ""); }


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

