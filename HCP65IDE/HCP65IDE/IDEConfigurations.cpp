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


#define IDE_EDIT_CONFIGURATIONS	"Edit Configurations..."


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszRunConfiguration.Init();
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
	U_String(mszRunConfiguration);
	U_String(mszConfiguration);
	M_Pointer(mpRunConfigurations);
	M_Pointer(mpDialog);
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
	mszRunConfiguration.Kill();
	if (mpRunConfigurations.IsNotNull())
	{
		mpRunConfigurations->GetSelectionEvent()->Remove(this);
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
// Edit Configurations.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Run.EditConfigurations", IDE_EDIT_CONFIGURATIONS, "", Ptr<CImageCel>(), this, &CIDEConfigurations::RequestEditDialog);
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's run configurations in the toolbar; it is given defaults
// if it has none.  The one chosen stays chosen if it is still there;
// otherwise the first of the chosen build configuration is, as a workspace
// saved before there were run configurations remembers only that, or the
// first.  The code model is told but not indexed again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::ShowConfigurations(void)
{
	CBuildWorkspace*	pcWorkspace;
	CRunConfiguration*	pcRun;
	size				i;

	pcWorkspace = mpcContent->GetWorkspace();
	pcWorkspace->AddDefaultRunConfigurations();
	pcRun = pcWorkspace->FindRunConfiguration(mszRunConfiguration.Text());
	if (pcRun == NULL)
	{
		pcRun = pcWorkspace->FindRunConfigurationBuilding(mszConfiguration.Text());
	}
	if (pcRun == NULL)
	{
		pcRun = pcWorkspace->GetRunConfiguration(0);
	}
	mszRunConfiguration.Set(pcRun ? pcRun->GetName() : "");
	mszConfiguration.Set((pcRun && pcWorkspace->FindAnyConfiguration(pcRun->GetConfiguration())) ? pcRun->GetConfiguration() : "");

	mpRunConfigurations->RemoveAllItems();
	for (i = 0; i < pcWorkspace->NumRunConfigurations(); i++)
	{
		mpRunConfigurations->AddItem(pcWorkspace->GetRunConfiguration(i)->GetName());
	}
	if (mpcContent->GetSolutions()->IsSolutionOpen())
	{
		mpRunConfigurations->AddItem(IDE_EDIT_CONFIGURATIONS);
	}
	if (!mszConfiguration.Empty() && (StringCompare(mszConfiguration.Text(), mpcContent->GetCodeModel()->GetConfiguration()) != 0))
	{
		mpcContent->GetCodeModel()->SetConfiguration(mszConfiguration.Text());
	}
	ShowConfiguration();
}


//////////////////////////////////////////////////////////////////////////
//
// A run configuration, or the last item, Edit Configurations, chosen in
// the toolbar.  That one isn't kept: the one before stays chosen, and the
// dialog is shown once the input has been read.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::RunConfigurationChosen(void)
{
	int32	iSelected;

	iSelected = mpRunConfigurations->GetSelected();
	if (iSelected < 0)
	{
		return;
	}
	if ((size)iSelected < mpcContent->GetWorkspace()->NumRunConfigurations())
	{
		SetRunConfiguration(mpRunConfigurations->GetItemText((size)iSelected));
	}
	else
	{
		ShowConfiguration();
		RequestEditDialog();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen run configuration selected in the toolbar, and its
// programs' line tables bind the source breakpoints.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::ShowConfiguration(void)
{
	size	i;

	for (i = 0; i < mpcContent->GetWorkspace()->NumRunConfigurations(); i++)
	{
		if (StringCompare(mpRunConfigurations->GetItemText(i), mszRunConfiguration.Text()) == 0)
		{
			mpRunConfigurations->Select((int32)i);
		}
	}
	if (!mszConfiguration.Empty())
	{
		mpcContent->GetDebugging()->ReadLineTables();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The run configuration szName is chosen, and with it its build
// configuration.  The code is parsed as that builds it: if it changed the
// solutions are indexed again and the open files are highlighted again.
// False if there's no such run configuration.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConfigurations::SetRunConfiguration(const char* szName)
{
	CRunConfiguration*	pcRun;
	size				i;

	pcRun = mpcContent->GetWorkspace()->FindRunConfiguration(szName);
	if (pcRun == NULL)
	{
		return false;
	}
	mszRunConfiguration.Set(pcRun->GetName());
	mszConfiguration.Set(mpcContent->GetWorkspace()->FindAnyConfiguration(pcRun->GetConfiguration()) ? pcRun->GetConfiguration() : "");
	if ((StringCompare(mpcContent->GetWorkspace()->GetConfiguration(), mszConfiguration.Text()) != 0) || (StringCompare(mpcContent->GetWorkspace()->GetRunConfigurationName(), mszRunConfiguration.Text()) != 0))
	{
		//Remembered in the workspace file for next time.
		mpcContent->GetSolutions()->WriteWorkspace();
	}
	ShowConfiguration();

	if (!mszConfiguration.Empty() && (StringCompare(mszConfiguration.Text(), mpcContent->GetCodeModel()->GetConfiguration()) != 0))
	{
		mpcContent->GetCodeModel()->SetConfiguration(mszConfiguration.Text());
		mpcContent->GetCodeModel()->StartIndexing();
		for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
		{
			mpcContent->GetDocuments()->RequestHighlight(mpcContent->GetDocuments()->GetTab(i));
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The first run configuration of the build configuration szConfiguration
// is chosen, unless the one chosen already builds it.  False if none does.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConfigurations::SetConfiguration(const char* szConfiguration)
{
	CRunConfiguration*	pcRun;

	pcRun = GetRunConfiguration();
	if ((pcRun == NULL) || (StringCompare(pcRun->GetConfiguration(), szConfiguration) != 0))
	{
		pcRun = mpcContent->GetWorkspace()->FindRunConfigurationBuilding(szConfiguration);
	}
	return (pcRun != NULL) && SetRunConfiguration(pcRun->GetName());
}


//////////////////////////////////////////////////////////////////////////
//
// Edit Configurations waits for the next tick: a dialog can't be shown
// from an input listener (see CWindow::ShowModal).
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::RequestEditDialog(void)
{
	mpcContent->GetWindow()->Post(this, &CIDEConfigurations::ShowEditDialog);
}


//////////////////////////////////////////////////////////////////////////
//
// What is edited replaces the workspace's run configurations if OK closes
// it, and the one selected in it is chosen.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::ShowEditDialog(void)
{
	CBuildWorkspace*	pcWorkspace;
	CChars				szSelected;

	if (!mpcContent->GetSolutions()->IsSolutionOpen())
	{
		return;
	}
	pcWorkspace = mpcContent->GetWorkspace();
	mpDialog = OMalloc<CIDERunConfigurationsDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), pcWorkspace, mszRunConfiguration.Text());
	if (getenv("HCP65_IDE_RUN_EDITS") != NULL)
	{
		mpDialog->ApplyEdits(getenv("HCP65_IDE_RUN_EDITS"));
	}
	mpcContent->GetScript()->WatchDialog(mpDialog->GetDialog());
	if (mpDialog->Show())
	{
		szSelected.Init(mpDialog->GetSelectedName());
		pcWorkspace->SetRunConfigurations(mpDialog->GetEdited());
		pcWorkspace->AddDefaultRunConfigurations();
		mszRunConfiguration.Set(szSelected.Text());
		szSelected.Kill();
		ShowConfigurations();
		SetRunConfiguration(mszRunConfiguration.Text());
		mpcContent->GetSolutions()->WriteWorkspace();
	}
	mpcContent->GetScript()->StopWatchingDialog();
	mpDialog = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's chosen build configuration: what is debugged.
// NULL if none is chosen or the startup solution hasn't it.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CIDEConfigurations::GetConfiguration(void)
{
	return mpcContent->GetWorkspace()->FindConfiguration(mpcContent->GetWorkspace()->GetStartup(), mszConfiguration.Text());
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
// NULL if none is chosen.
//
//////////////////////////////////////////////////////////////////////////
CRunConfiguration* CIDEConfigurations::GetRunConfiguration(void)
{
	return mpcContent->GetWorkspace()->FindRunConfiguration(mszRunConfiguration.Text());
}


char* CIDEConfigurations::GetRunConfigurationName(void) { return mszRunConfiguration.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's run configuration combo box.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComboBox> CIDEConfigurations::CreateComboBox(void)
{
	mpRunConfigurations = OMalloc<CComboBox>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpRunConfigurations->SetAlignment(CSH_Left, CSV_Center);
	mpRunConfigurations->SetToolTip("Run Configurations");
	mpRunConfigurations->GetSelectionEvent()->Add(this, &CIDEConfigurations::RunConfigurationChosen);
	return mpRunConfigurations;
}


Ptr<CComboBox> CIDEConfigurations::GetRunConfigurationBox(void) { return mpRunConfigurations; }


//////////////////////////////////////////////////////////////////////////
//
// What is chosen is called this (a workspace's when it is read, empty when
// the solutions go), before the combo box shows it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurations::SetConfigurationName(const char* szName) { mszConfiguration.Set(szName); }
void CIDEConfigurations::SetRunConfigurationName(const char* szName) { mszRunConfiguration.Set(szName); }
bool CIDEConfigurations::HasConfiguration(void) { return !mszConfiguration.Empty(); }
