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
#include "WindowLib/JewelTheme.h"
#include "WindowLib/JewelImageGenerator.h"
#include "IDEGit.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEBuilding.h"


#define IDE_BUILD_POLL			50		//Milliseconds between looks at a running build.



//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mcBuild.Init();
	maiBuildOrder.Init();
	mcListings.Init();
	muiBuildStep = 0;
	mszLibc65.Init();
	muiListingToShow = MAX_SIZE;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CSolutionBuild, mcBuild);
	U_Unknown(CArrayInt, maiBuildOrder);
	U_Unknown(CCPP65ListingIndex, mcListings);
	M_Pointer(mpBuildPane);
	M_Pointer(mpBuildTool);
	M_Pointer(mpDialog);
	U_Size(muiBuildStep);
	U_String(mszLibc65);
	U_Size(muiListingToShow);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::Free(void)
{
	mszLibc65.Kill();
	mcBuild.Kill();
	maiBuildOrder.Kill();
	mcListings.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::Save(CObjectWriter* pcFile) { return false; }
bool CIDEBuilding::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The Build tab is shown under the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ShowBuildTab(void)
{
	mpcContent->GetDock()->Show(mpBuildTool);
}


//////////////////////////////////////////////////////////////////////////
//
// Where the HCP65 compiler's system headers (<stdint.h>) are: Libc65.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::SetLibc65Directory(const char* szDirectory)
{
	CFileUtil	cFileUtil;

	mszLibc65.Set(szDirectory);
	cFileUtil.FullPath(&mszLibc65);
}


//////////////////////////////////////////////////////////////////////////
//
// Starts building the solution in the chosen build configuration, its
// output in the Build tab.  False if there is no solution or a build is
// running, or if no solution has the build configuration, as the Build tab
// says.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::Build(void)
{
	CArrayChars	aszLines;
	bool		bResult;

	if (!mpcContent->GetSolutions()->IsSolutionOpen() || mcBuild.IsRunning())
	{
		return false;
	}
	maiBuildOrder.ReInit();
	if (mpcContent->GetBuildConfigurations()->HasConfiguration())
	{
		mpcContent->GetWorkspace()->GetBuildOrder(mpcContent->GetBuildConfigurations()->GetConfigurationName(), &maiBuildOrder);
	}
	if (maiBuildOrder.NumElements() == 0)
	{
		ShowNothingToBuild();
		return false;
	}

	//The listings of every solution built are read, in place of the last build's.
	SetListings(false);
	muiBuildStep = 0;
	mpBuildPane->Clear();
	ShowBuildTab();
	aszLines.Init();
	bResult = StartBuildStep(&aszLines);
	if (bResult)
	{
		FinishBuildSteps(&aszLines);
	}
	mpBuildPane->AppendLines(&aszLines);
	ShowBuildProgress();
	aszLines.Kill();
	if (mcBuild.IsRunning())
	{
		mpcContent->GetWindow()->RequestTick(IDE_BUILD_POLL);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Build was asked for but the chosen run configuration builds nothing: the
// Build tab says why, as the Run tab does for one with no tool.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ShowNothingToBuild(void)
{
	CRunConfiguration*	pcRun;
	CArrayChars			aszLines;
	CChars				szLine;

	pcRun = mpcContent->GetBuildConfigurations()->GetRunConfiguration();
	szLine.Init();
	if (pcRun == NULL)
	{
		szLine.Append("No run configuration is chosen: add one in Edit Configurations.");
	}
	else if (pcRun->GetConfiguration()[0] == '\0')
	{
		szLine.Append(pcRun->GetName());
		szLine.Append(" has no build configuration: choose one in Edit Configurations.");
	}
	else
	{
		szLine.Append(pcRun->GetName());
		szLine.Append(" builds ");
		szLine.Append(pcRun->GetConfiguration());
		szLine.Append(", which no open solution has: choose another build configuration in Edit Configurations.");
	}
	aszLines.Init();
	aszLines.Add(szLine.Text());
	mpBuildPane->Clear();
	ShowBuildTab();
	mpBuildPane->AppendLines(&aszLines);
	aszLines.Kill();
	szLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A running build's new output goes in the Build tab.  When an
// HCP65 build succeeds its listings are read.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::PollBuild(Ptr<CWindow> pWindow)
{
	CArrayChars	aszLines;
	bool		bRunning;

	if (!mcBuild.IsRunning())
	{
		return;
	}

	aszLines.Init();
	bRunning = mcBuild.Poll(&aszLines);
	if (!bRunning)
	{
		FinishBuildSteps(&aszLines);
	}
	mpBuildPane->AppendLines(&aszLines);
	ShowBuildProgress();
	aszLines.Kill();
	if (mcBuild.IsRunning())
	{
		pWindow->RequestTick(IDE_BUILD_POLL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The build of the solution the build is up to (muiBuildStep), with a line
// saying which when more than one is built.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::StartBuildStep(CArrayChars* paszLines)
{
	size						uiSolution;
	CBuildWorkspaceSolution*	pcSolution;
	CChars						szLine;

	uiSolution = (size)maiBuildOrder.GetValue(muiBuildStep);
	pcSolution = mpcContent->GetWorkspace()->GetSolution(uiSolution);
	if (maiBuildOrder.NumElements() > 1)
	{
		szLine.Init("------ ");
		szLine.Append(pcSolution->mcSolution.GetName());
		szLine.Append(": ");
		szLine.Append(mpcContent->GetBuildConfigurations()->GetConfigurationName());
		szLine.Append(" ------");
		paszLines->Add(szLine.Text());
		szLine.Kill();
	}
	return mcBuild.Start(&pcSolution->mcSolution, mpcContent->GetWorkspace()->FindConfiguration(uiSolution, mpcContent->GetBuildConfigurations()->GetConfigurationName()), NULL, mszLibc65.Text(), paszLines);
}


//////////////////////////////////////////////////////////////////////////
//
// The build pane's bar: blue while the build runs, as far as it has got
// over the solutions it builds (each an equal part); then all green if
// every solution built, else all red.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ShowBuildProgress(void)
{
	size	uiSteps;
	float	fFraction;

	uiSteps = maiBuildOrder.NumElements();
	if (mcBuild.IsRunning() && (uiSteps > 0))
	{
		fFraction = ((float)muiBuildStep + mcBuild.GetFraction()) / (float)uiSteps;
		mpBuildPane->SetProgress(fFraction, mpcContent->GetTheme()->GetColour(TC_Accent));
	}
	else if (mcBuild.GetState() == BS_Succeeded)
	{
		mpBuildPane->SetProgress(1.0f, mpcContent->GetTheme()->GetColour(TC_Success));
	}
	else if ((mcBuild.GetState() == BS_Failed) || (mcBuild.GetState() == BS_Cancelled))
	{
		mpBuildPane->SetProgress(1.0f, mpcContent->GetTheme()->GetColour(TC_Failure));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// When a solution's build has finished: an HCP65 build that succeeded has
// its listings read, and the next solution's is started.  A build that
// failed builds no more.  When the last succeeds the programs' new line
// tables bind the breakpoints in their source.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::FinishBuildSteps(CArrayChars* paszLines)
{
	CBuildConfiguration*	pcConfiguration;

	while (!mcBuild.IsRunning() && (mcBuild.GetState() == BS_Succeeded) && (muiBuildStep < maiBuildOrder.NumElements()))
	{
		pcConfiguration = mpcContent->GetWorkspace()->FindConfiguration((size)maiBuildOrder.GetValue(muiBuildStep), mpcContent->GetBuildConfigurations()->GetConfigurationName());
		if (pcConfiguration->IsHCP65())
		{
			ReadListings(paszLines);
		}
		muiBuildStep++;
		if (muiBuildStep == maiBuildOrder.NumElements())
		{
			mpcContent->GetDebugging()->ReadLineTables();
		}
		else if (!StartBuildStep(paszLines))
		{
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The last solution build's listings, added to those of the solutions
// built before it, given to the open files.  A line asked for
// (ShowListingAfterBuild) shows its code.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ReadListings(CArrayChars* paszLines)
{
	CFileUtil	cFileUtil;
	CChars		szDirectory;
	CChars		szLine;
	CArrayChars	aszFiles;
	size		i;
	size		uiRead;

	uiRead = mcListings.NumListings();
	szDirectory.Init();
	mcBuild.GetBuild()->GetListingDirectory(&szDirectory);
	aszFiles.Init();
	cFileUtil.FindAllFiles(szDirectory.Text(), &aszFiles);
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		if (aszFiles.Get(i)->EndsWithIgnoreCase(".lst"))
		{
			mcListings.ReadListing(aszFiles.GetText(i));
		}
	}
	uiRead = mcListings.NumListings() - uiRead;
	szLine.Init("Read ");
	szLine.Append((uint32)uiRead);
	szLine.Append((uiRead == 1) ? " listing from " : " listings from ");
	szLine.Append(szDirectory);
	szLine.Append('.');
	paszLines->Add(szLine.Text());
	szLine.Kill();
	aszFiles.Kill();
	szDirectory.Kill();

	SetListings(true);
	if ((muiListingToShow != MAX_SIZE) && mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
	{
		mpcContent->GetDocuments()->GetActiveTab()->ToggleListing(muiListingToShow);
		muiListingToShow = MAX_SIZE;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The open files are given the listings (bListings), or none and the
// listings are forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::SetListings(bool bListings)
{
	size	i;

	if (!bListings)
	{
		mcListings.Kill();
		mcListings.Init();
	}
	for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
	{
		mpcContent->GetDocuments()->GetTab(i)->SetListings(bListings ? &mcListings : NULL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// When the next HCP65 build has succeeded, line uiLine (from 0) of the
// selected file shows its code, as clicking its mark does.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ShowListingAfterBuild(size uiLine)
{
	muiListingToShow = uiLine;
}


bool CIDEBuilding::IsBuilding(void) { return mcBuild.IsRunning(); }


CCPP65ListingIndex* CIDEBuilding::GetListings(void) { return &mcListings; }  //The last HCP65 build's, for showing a line's assembly.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////


Ptr<CIDEBuildPane> CIDEBuilding::GetBuildPane(void) { return mpBuildPane; }


Ptr<CDockPane> CIDEBuilding::GetBuildTool(void) { return mpBuildTool; }


//////////////////////////////////////////////////////////////////////////
//
// The Build tab, what the build writes and its progress, put at psPlace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::CreatePane(SDockPlace* psPlace)
{
	mpBuildPane = OMalloc<CIDEBuildPane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetCodeFamily(), mpcContent->GetTheme()->GetColour(TC_Text));
	mpBuildTool = mpcContent->GetDock()->AddPane(IDE_BUILD_ID, "Build", NULL, mpBuildPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Build (Control+Shift+B, as Visual Studio's) and Edit Build
// Configurations.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Build.Build", "Build", "Control+Shift+B", Ptr<CImageCel>(), this, &CIDEBuilding::Build);
	pCommands->Add("Build.EditConfigurations", "Edit Build Configurations...", "", Ptr<CImageCel>(), this, &CIDEBuilding::RequestEditDialog)->SetEnabledCheck(this, &CIDEBuilding::CanEditConfigurations);
}


//////////////////////////////////////////////////////////////////////////
//
// The Build menu.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEBuilding::CreateBuildMenu(void)
{
	Ptr<CMenuContainer>	pMenu;

	pMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	pMenu->AddItem(mpcContent->GetCommands()->Find("Build.Build"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Build.EditConfigurations"));
	return pMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// A running build uses the configurations, which saving reads again.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::CanEditConfigurations(void)
{
	return mpcContent->GetSolutions()->IsSolutionOpen() && !mcBuild.IsRunning();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog waits for the next tick: one can't be shown from an input
// listener (see CWindow::ShowModal).
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::RequestEditDialog(void)
{
	mpcContent->GetWindow()->Post(this, &CIDEBuilding::ShowEditDialog);
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's build configurations edited, the chosen one
// selected.  OK saves them, and Open File opens their file instead.  A
// scripted run leaves the file alone, as it does the layout.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ShowEditDialog(void)
{
	CBuildWorkspaceSolution*	pcSolution;
	CChars						szFile;
	bool						bOK;

	if (!CanEditConfigurations())
	{
		return;
	}
	pcSolution = mpcContent->GetWorkspace()->GetStartupSolution();
	if (pcSolution == NULL)
	{
		return;
	}
	szFile.Init();
	CBuildConfigurations::GetFileName(pcSolution->mszFile.Text(), &szFile);
	mpDialog = OMalloc<CIDEBuildConfigurationsDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), pcSolution->mcSolution.GetName(), &pcSolution->mcSolution, &pcSolution->mcConfigurations, mpcContent->GetBuildConfigurations()->GetConfigurationName());
	if (getenv("HCP65_IDE_BUILD_EDITS") != NULL)
	{
		mpDialog->ApplyEdits(getenv("HCP65_IDE_BUILD_EDITS"));
	}
	mpcContent->GetScript()->WatchDialog(mpDialog->GetDialog());
	bOK = mpDialog->Show();
	mpcContent->GetScript()->StopWatchingDialog();
	if (bOK)
	{
		SaveEdits(&mpDialog, pcSolution, szFile.Text());
	}
	else if (StringCompare(mpDialog->GetDialog()->GetClicked(), IDE_BUILD_DIALOG_OPEN_FILE) == 0)
	{
		mpcContent->GetDocuments()->OpenFile(szFile.Text());
	}
	mpDialog = NULL;
	szFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's edits go in the file and are read back.  Run configurations
// follow a build configuration that was renamed, a new one gets one so it
// can be chosen, and one removed takes its own with it.  The code is indexed
// again, as it is parsed as they say, which stops while they change.  The
// status line says how it went, if another open solution still has the old
// name, as a build configuration is chosen by its name across them, and if
// run configurations are left building what was removed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::SaveEdits(CIDEBuildConfigurationsDialog* pcDialog, CBuildWorkspaceSolution* pcSolution, const char* szFile)
{
	CBuildWorkspace*				pcWorkspace;
	CArrayBuildConfigurationEdits*	pasEdits;
	CBuildConfigurationEdit*		pcEdit;
	CArrayChars						aszOld;
	CArrayChars						aszNew;
	CArrayChars						aszRemoved;
	CArrayChars						aszKept;
	CChars							szStatus;
	char*							szRenamed;
	bool							bResult;
	bool							bShared;
	size							i;
	size							j;

	if (!mpcContent->IsKeepingSettings())
	{
		mpcContent->ShowStatus("A scripted run doesn't save build configurations.");
		return;
	}

	pcWorkspace = mpcContent->GetWorkspace();
	pasEdits = pcDialog->GetEdits();
	aszOld.Init();
	aszNew.Init();
	for (i = 0; i < pasEdits->NumElements(); i++)
	{
		pcEdit = pasEdits->Get(i);
		if (!pcEdit->mbNew && !pcEdit->mszName.Equals(pcSolution->mcConfigurations.GetConfiguration(pcEdit->muiSource)->GetName()))
		{
			aszOld.Add(pcSolution->mcConfigurations.GetConfiguration(pcEdit->muiSource)->GetName());
			aszNew.Add(pcEdit->mszName.Text());
		}
	}
	aszRemoved.Init();
	aszKept.Init();
	FindRemovedConfigurations(&pcSolution->mcConfigurations, pasEdits, &aszRemoved);

	mpcContent->GetCodeModel()->Reset();
	bResult = pcSolution->mcConfigurations.ApplyEdits(szFile, pasEdits);
	bShared = false;
	szStatus.Init();
	if (bResult)
	{
		for (i = 0; i < pcWorkspace->NumRunConfigurations(); i++)
		{
			szRenamed = FindRename(&aszOld, &aszNew, pcWorkspace->GetRunConfiguration(i)->GetConfiguration());
			if (szRenamed != NULL)
			{
				pcWorkspace->GetRunConfiguration(i)->SetConfiguration(szRenamed);
			}
		}
		szRenamed = FindRename(&aszOld, &aszNew, mpcContent->GetBuildConfigurations()->GetConfigurationName());
		if (szRenamed != NULL)
		{
			mpcContent->GetBuildConfigurations()->SetConfigurationName(szRenamed);
		}
		for (i = 0; i < aszOld.NumElements(); i++)
		{
			for (j = 0; j < pcWorkspace->NumSolutions(); j++)
			{
				bShared |= (pcWorkspace->GetSolution(j) != pcSolution) && (pcWorkspace->FindConfiguration(j, aszOld.GetText(i)) != NULL);
			}
		}
		for (i = 0; i < pasEdits->NumElements(); i++)
		{
			pcEdit = pasEdits->Get(i);
			if (pcEdit->mbNew && (pcWorkspace->FindRunConfigurationBuilding(pcEdit->mszName.Text()) == NULL))
			{
				pcWorkspace->AddRunConfiguration(pcEdit->mszName.Text(), pcEdit->mszName.Text());
			}
		}
		ForgetRemovedConfigurations(&aszRemoved, &aszKept);

		szStatus.Set("Saved");
		if (bShared)
		{
			szStatus.Append(", but another open solution still has the old name of a build configuration you renamed");
		}
		if (aszKept.NumElements() == 1)
		{
			szStatus.Append(bShared ? ", and run configuration " : ", but run configuration ");
			szStatus.Append(aszKept.GetText(0));
			szStatus.Append(" builds a build configuration you removed: choose another for it in Edit Configurations");
		}
		else if (aszKept.NumElements() > 1)
		{
			szStatus.Append(bShared ? ", and " : ", but ");
			szStatus.Append((uint32)aszKept.NumElements());
			szStatus.Append(" run configurations build build configurations you removed: choose others for them in Edit Configurations");
		}
		else if (!bShared)
		{
			szStatus.Append(" the build configurations");
		}
		szStatus.Append('.');
	}
	else
	{
		szStatus.Set("Could not save ");
		szStatus.Append(szFile);
		szStatus.Append(": it may have changed on disk.  Nothing was changed.");
	}
	mpcContent->GetSolutions()->WorkspaceChanged(true);
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
	aszKept.Kill();
	aszRemoved.Kill();
	aszNew.Kill();
	aszOld.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What the build configuration szName before the edits is called after
// them, NULL if it wasn't renamed.  Each run configuration asks once, with
// the name it had, so names swapped or passed along a chain aren't renamed
// twice.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEBuilding::FindRename(CArrayChars* paszOld, CArrayChars* paszNew, const char* szName)
{
	size	i;

	for (i = 0; i < paszOld->NumElements(); i++)
	{
		if (StringCompare(paszOld->GetText(i), szName) == 0)
		{
			return paszNew->GetText(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The names of the build configurations no edit came from: those the
// dialog removed.  A copy doesn't keep the one it was made from.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::FindRemovedConfigurations(CBuildConfigurations* pcConfigurations, CArrayBuildConfigurationEdits* pasEdits, CArrayChars* paszRemoved)
{
	CBuildConfigurationEdit*	pcEdit;
	bool						bKept;
	size						i;
	size						j;

	for (i = 0; i < pcConfigurations->NumConfigurations(); i++)
	{
		bKept = false;
		for (j = 0; j < pasEdits->NumElements(); j++)
		{
			pcEdit = pasEdits->Get(j);
			bKept |= !pcEdit->mbNew && (pcEdit->muiSource == i);
		}
		if (!bKept)
		{
			paszRemoved->Add(pcConfigurations->GetConfiguration(i)->GetName());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Once the build configurations in paszRemoved are gone, unless another
// open solution has one of that name: a run configuration with nothing but
// the name it was given goes with it.  One with anything of the user's in
// it stays, and is named in paszKept: its tool still runs, and pointing it
// at another build configuration is for the user to choose.  The toolbar
// then chooses by the run configuration's name alone, not that of what it
// built, so it doesn't move to one of those kept.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ForgetRemovedConfigurations(CArrayChars* paszRemoved, CArrayChars* paszKept)
{
	CBuildWorkspace*	pcWorkspace;
	CRunConfiguration*	pcRun;
	size				i;
	size				j;

	pcWorkspace = mpcContent->GetWorkspace();
	for (i = 0; i < paszRemoved->NumElements(); i++)
	{
		if (pcWorkspace->FindAnyConfiguration(paszRemoved->GetText(i)) != NULL)
		{
			continue;
		}
		for (j = pcWorkspace->NumRunConfigurations(); j-- > 0;)
		{
			pcRun = pcWorkspace->GetRunConfiguration(j);
			if (StringCompare(pcRun->GetConfiguration(), paszRemoved->GetText(i)) == 0)
			{
				if (pcRun->IsDefault())
				{
					pcWorkspace->RemoveRunConfiguration(j);
				}
				else
				{
					paszKept->Add(pcRun->GetName());
				}
			}
		}
		if (StringCompare(mpcContent->GetBuildConfigurations()->GetConfigurationName(), paszRemoved->GetText(i)) == 0)
		{
			mpcContent->GetBuildConfigurations()->SetConfigurationName("");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A build still running is stopped (an HCP65 build is waited for): the
// solutions it builds are going.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::StopBuild(void)
{
	mcBuild.Kill();
	mcBuild.Init();
	maiBuildOrder.ReInit();
}


