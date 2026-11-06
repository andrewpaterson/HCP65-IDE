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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/Timer.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ObjectReader.h"
#include "StandardLib/ObjectWriter.h"
#include "SupportLib/ImageColour.h"
#include "SupportLib/ImageColourConverter.h"
#include "SupportLib/TrueTypeFont.h"
#include "SupportLib/TrueTypeFontFactory.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/Label.h"
#include "WindowLib/JewelColours.h"
#include "WindowLib/JewelTheme.h"
#include "WindowLib/JewelImageGenerator.h"
#include "IDEGit.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEConfigurations.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszConfiguration.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::Class(void)
{
	U_Pointer(mpcContent);
	U_String(mszConfiguration);
	M_Pointer(mpConfigurations);
	M_Pointer(mpPlatforms);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::Free(void)
{
	mszConfiguration.Kill();
	if (mpConfigurations.IsNotNull())
	{
		mpConfigurations->GetSelectionEvent()->Remove(this);
	}
	if (mpPlatforms.IsNotNull())
	{
		mpPlatforms->GetSelectionEvent()->Remove(this);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConfigurations::Save(CObjectWriter* pcFile) { return false; }
bool CIDEConfigurations::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The names of every solution's configurations in the toolbar.  The one
// chosen stays chosen if a solution still has it, or the startup
// solution's first is.  The code model is told but not indexed again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::ShowConfigurations(void)
{
	CArrayChars	aszNames;

	aszNames.Init();
	mpcContent->mcWorkspace.GetConfigurationNames(&aszNames);
	if (!aszNames.Contains(mszConfiguration.Text()))
	{
		mszConfiguration.Set((aszNames.NumElements() > 0) ? aszNames.GetText(0) : "");
	}
	aszNames.Kill();

	SetComboBoxItems(mpConfigurations, false);
	SetComboBoxItems(mpPlatforms, true);
	if (StringCompare(mszConfiguration.Text(), mpcContent->mcCodeModel.GetConfiguration()) != 0)
	{
		mpcContent->mcCodeModel.SetConfiguration(mszConfiguration.Text());
	}
	ShowConfiguration();
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration and platform chosen in the toolbar.  If there's no
// build configuration with both, the one changed is kept: the first with
// it is chosen.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::ConfigurationChosen(void)
{
	char*					szConfiguration;
	char*					szPlatform;
	CBuildConfiguration*	pcCurrent;
	CBuildConfiguration*	pcChosen;

	szConfiguration = mpConfigurations->GetSelectedText();
	szPlatform = mpPlatforms->GetSelectedText();
	pcCurrent = GetChosenConfiguration();
	if ((szConfiguration == NULL) || (szPlatform == NULL) || ((pcCurrent != NULL) && pcCurrent->Is(szConfiguration, szPlatform)))
	{
		return;
	}

	pcChosen = mpcContent->mcWorkspace.FindAnyConfiguration(szConfiguration, szPlatform);
	if (pcChosen == NULL)
	{
		if ((pcCurrent != NULL) && pcCurrent->Is(NULL, szPlatform))
		{
			pcChosen = mpcContent->mcWorkspace.FindAnyConfiguration(szConfiguration, NULL);
		}
		else
		{
			pcChosen = mpcContent->mcWorkspace.FindAnyConfiguration(NULL, szPlatform);
		}
	}
	if (pcChosen != NULL)
	{
		SetConfiguration(pcChosen->GetName());
	}
	else
	{
		ShowConfiguration();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every solution's configurations' configuration names, or platforms, in
// pComboBox.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::SetComboBoxItems(Ptr<CComboBox> pComboBox, bool bPlatforms)
{
	CArrayChars		aszNames;
	size			i;

	aszNames.Init();
	mpcContent->mcWorkspace.GetConfigurationNames(&aszNames, bPlatforms);
	pComboBox->RemoveAllItems();
	for (i = 0; i < aszNames.NumElements(); i++)
	{
		pComboBox->AddItem(aszNames.GetText(i));
	}
	aszNames.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen build configuration's configuration and platform selected in
// the toolbar.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::ShowConfiguration(void)
{
	CChars					szName;
	CBuildConfiguration*	pcConfiguration;

	pcConfiguration = GetChosenConfiguration();
	if (pcConfiguration == NULL)
	{
		return;
	}

	szName.Init();
	pcConfiguration->GetConfigurationName(&szName);
	SelectComboBoxItem(mpConfigurations, szName.Text());
	szName.Clear();
	pcConfiguration->GetPlatformName(&szName);
	SelectComboBoxItem(mpPlatforms, szName.Text());
	szName.Kill();

	//Its programs' line tables bind the source breakpoints.
	mpcContent->mpDebugging->ReadLineTables();
}


//////////////////////////////////////////////////////////////////////////
//
// pComboBox's item szText selected (selecting doesn't tell the listeners).
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::SelectComboBoxItem(Ptr<CComboBox> pComboBox, const char* szText)
{
	size	i;

	for (i = 0; i < pComboBox->NumItems(); i++)
	{
		if (StringCompare(pComboBox->GetItemText(i), szText) == 0)
		{
			pComboBox->Select((int32)i);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The build configuration szConfiguration is chosen: each solution's of
// that name.  The code is parsed as their solution configurations build
// it: if it changed the solutions are indexed again and the open files are
// highlighted again.  False if no solution has such a build configuration.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConfigurations::SetConfiguration(const char* szConfiguration)
{
	size	i;

	if (mpcContent->mcWorkspace.FindAnyConfiguration(szConfiguration) == NULL)
	{
		return false;
	}
	mszConfiguration.Set(szConfiguration);
	if (StringCompare(mpcContent->mcWorkspace.GetConfiguration(), szConfiguration) != 0)
	{
		//Remembered in the workspace file for next time.
		mpcContent->mpSolutions->WriteWorkspace();
	}
	ShowConfiguration();

	if (StringCompare(mszConfiguration.Text(), mpcContent->mcCodeModel.GetConfiguration()) != 0)
	{
		mpcContent->mcCodeModel.SetConfiguration(mszConfiguration.Text());
		mpcContent->mcCodeModel.StartIndexing();
		for (i = 0; i < mpcContent->mpDocuments->NumTabs(); i++)
		{
			mpcContent->mpDocuments->HighlightTab(mpcContent->mpDocuments->GetTab(i));
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's chosen build configuration: what is debugged.
// NULL if none is chosen or the startup solution hasn't it.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CIDEConfigurations::GetConfiguration(void)
{
	return mpcContent->mcWorkspace.FindConfiguration(mpcContent->mcWorkspace.GetStartup(), mszConfiguration.Text());
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen build configuration's name, "" if none is chosen.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEConfigurations::GetConfigurationName(void)
{
	return mszConfiguration.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// A configuration with the chosen name, the startup solution's if it has
// one: what the toolbar shows.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CIDEConfigurations::GetChosenConfiguration(void)
{
	return mpcContent->mcWorkspace.FindAnyConfiguration(mszConfiguration.Text());
}


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's configuration and platform combo boxes.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::CreateComboBoxes(void)
{
	mpConfigurations = OMalloc<CComboBox>(mpcContent->mpWindow, mpcContent->mpTheme);
	mpConfigurations->SetAlignment(CSH_Left, CSV_Center);
	mpConfigurations->GetSelectionEvent()->Add(this, &CIDEConfigurations::ConfigurationChosen);
	mpPlatforms = OMalloc<CComboBox>(mpcContent->mpWindow, mpcContent->mpTheme);
	mpPlatforms->SetAlignment(CSH_Left, CSV_Center);
	mpPlatforms->GetSelectionEvent()->Add(this, &CIDEConfigurations::ConfigurationChosen);
}


Ptr<CComboBox> CIDEConfigurations::GetConfigurationBox(void) { return mpConfigurations; }
Ptr<CComboBox> CIDEConfigurations::GetPlatformBox(void) { return mpPlatforms; }


//////////////////////////////////////////////////////////////////////////
//
// The build configuration chosen is called szName (a workspace's when it
// is read, empty when the solutions go), before the combo boxes show it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::SetConfigurationName(const char* szName) { mszConfiguration.Set(szName); }
bool CIDEConfigurations::HasConfiguration(void) { return !mszConfiguration.Empty(); }
