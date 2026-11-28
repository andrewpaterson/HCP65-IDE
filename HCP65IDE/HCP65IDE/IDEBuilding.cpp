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
#include <stdio.h>
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
#include "VisualStudioLib/VcxProjectConfigurations.h"
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
	maCleanUpBoxes.Init();
	muiBuildStep = 0;
	mszLibc65.Init();
	muiListingToShow = MAX_SIZE;
	muiBuilds = 0;
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
	M_Pointer(mpMessages);
	M_Pointer(mpBuildTool);
	M_Pointer(mpDialog);
	M_Pointer(mpCleanUp);
	M_Embedded(maCleanUpBoxes);
	U_Size(muiBuildStep);
	U_String(mszLibc65);
	U_Size(muiListingToShow);
	U_Size(muiBuilds);
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
// The code model is told too, which indexes what an HCP65 configuration
// builds with them (CWorkspaceIndex::SetLibc65Directory).
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::SetLibc65Directory(const char* szDirectory)
{
	CFileUtil	cFileUtil;

	mszLibc65.Set(szDirectory);
	cFileUtil.FullPath(&mszLibc65);
	mpcContent->GetCodeModel()->SetLibc65Directory(mszLibc65.Text());
}


const char* CIDEBuilding::GetLibc65Directory(void) { return mszLibc65.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// Build: what isn't up to date is built (a GCC or MSVC build can tell; an
// HCP65 build compiles everything).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::Build(void)
{
	return StartBuild(false);
}


//////////////////////////////////////////////////////////////////////////
//
// Rebuild All: Build, with everything built whether or not it is up to
// date (ConfigurationsPlan.txt question 31).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::RebuildAll(void)
{
	return StartBuild(true);
}


//////////////////////////////////////////////////////////////////////////
//
// Starts building the solution in the chosen build configuration, its
// output in the Build tab.  False if there is no solution or a build is
// running, or if no solution has the build configuration, as the Build tab
// says.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::StartBuild(bool bRebuild)
{
	CArrayChars	aszLines;
	bool		bResult;

	if (!mpcContent->GetSolutions()->IsSolutionOpen() || mcBuild.IsRunning())
	{
		return false;
	}
	mcBuild.SetRebuild(bRebuild);
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
	muiBuilds++;
	SetListings(false);
	muiBuildStep = 0;
	ClearBuildTab();
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
// The Build tab emptied for a new build's output, and the message last
// stepped to with it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ClearBuildTab(void)
{
	mpBuildPane->Clear();
	mpMessages->Clear();
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
	ClearBuildTab();
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
	return mcBuild.Start(&pcSolution->mcSolution, mpcContent->GetWorkspace()->FindConfiguration(uiSolution, mpcContent->GetBuildConfigurations()->GetConfigurationName()), NULL, mszLibc65.Text(), paszLines, pcSolution->GetStartupProjectName());
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


Ptr<CIDEBuildPane> CIDEBuilding::GetBuildPane(void) const { return mpBuildPane; }


Ptr<CDockPane> CIDEBuilding::GetBuildTool(void) const { return mpBuildTool; }


//////////////////////////////////////////////////////////////////////////
//
// The Build tab, what the build writes and its progress, put at psPlace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::CreatePane(SDockPlace* psPlace)
{
	mpBuildPane = OMalloc<CIDEBuildPane>(mpcContent->GetWindow(), mpcContent->GetTheme(), mpcContent->GetCodeFamily(), mpcContent->GetTheme()->GetColour(TC_Text));
	mpBuildPane->SetMessageColours(mpcContent->GetTheme()->GetColour(TC_Failure), mpcContent->GetTheme()->GetColour(TC_Warning));
	mpMessages = OMalloc<CIDEMessageOutput>(mpcContent, mpBuildPane, "build messages");
	mpBuildTool = mpcContent->GetDock()->AddPane(IDE_BUILD_ID, "Build", NULL, mpBuildPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Build (Control+F9, as IntelliJ IDEA's Build Project: the user,
// 2026-11-20, as Control+Shift+B is IntelliJ's Go to Type Declaration) and
// Edit Build Configurations.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Build.Build", "Build", "Control+F9", Ptr<CImageCel>(), this, &CIDEBuilding::Build);
	pCommands->Add("Build.RebuildAll", "Rebuild All", "", Ptr<CImageCel>(), this, &CIDEBuilding::RebuildAll);
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
	pMenu->AddItem(mpcContent->GetCommands()->Find("Build.RebuildAll"));
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
// selected (ConfigurationsPlan.txt C9).  OK saves them; Clean Up saves
// them and then offers the configurations no solution builds.  A solution
// whose build configurations are still in a file beside it has that
// opened instead: the dialog edits what the projects say.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ShowEditDialog(void)
{
	CBuildWorkspaceSolution*	pcSolution;
	CFileUtil					cFileUtil;
	CChars						szFile;
	bool						bOK;
	bool						bCleanUp;

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
	if (cFileUtil.Exists(szFile.Text()))
	{
		mpcContent->GetDocuments()->OpenFile(szFile.Text());
		mpcContent->ShowStatus("This solution's build configurations are in the file beside it, which is open to edit.");
		szFile.Kill();
		return;
	}
	szFile.Kill();

	mpDialog = OMalloc<CIDEBuildConfigurationsDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), &pcSolution->mcSolution, pcSolution->GetStartupProjectName(), mpcContent->GetBuildConfigurations()->GetConfigurationName());
	if (getenv("HCP65_IDE_BUILD_EDITS") != NULL)
	{
		mpDialog->ApplyEdits(getenv("HCP65_IDE_BUILD_EDITS"));
	}
	mpcContent->GetScript()->WatchDialog(mpDialog->GetDialog());
	bOK = mpDialog->Show();
	mpcContent->GetScript()->StopWatchingDialog();
	bCleanUp = StringCompare(mpDialog->GetDialog()->GetClicked(), IDE_BUILD_DIALOG_CLEAN_UP) == 0;
	if (bOK || bCleanUp)
	{
		SaveEdits(&mpDialog, pcSolution);
	}
	mpDialog = NULL;
	if (bCleanUp)
	{
		ShowCleanUpDialog();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's edits go in the solution and its projects and sheets, which
// are read again.  Run configurations follow a build configuration that
// was renamed, a new one gets one so it can be chosen, and one removed
// takes its own with it.  The code is indexed again, as it is parsed as
// they say, which stops while they change.  The status line says how it
// went, what was done besides what was asked (a project given a
// configuration of its own), if another open solution still has the old
// name, as a build configuration is chosen by its name across them, and if
// run configurations are left building what was removed.  A scripted run
// leaves the files alone, as it does the layout.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::SaveEdits(CIDEBuildConfigurationsDialog* pcDialog, CBuildWorkspaceSolution* pcSolution)
{
	CBuildWorkspace*		pcWorkspace;
	CIDEBuildDialogEntry*	pcEntry;
	CArrayChars				aszOld;
	CArrayChars				aszNew;
	CArrayChars				aszAdded;
	CArrayChars				aszRemoved;
	CArrayChars				aszKept;
	CChars					szStatus;
	CChars					szNote;
	char*					szRenamed;
	bool					bResult;
	bool					bShared;
	bool					bFound;
	size					i;
	size					j;

	if (!pcDialog->IsEdited())
	{
		return;
	}
	if (!mpcContent->IsKeepingSettings())
	{
		mpcContent->ShowStatus("A scripted run doesn't save build configurations.");
		return;
	}

	//What was renamed, added and removed, by name: the solution is read again before they are needed.
	pcWorkspace = mpcContent->GetWorkspace();
	aszOld.Init();
	aszNew.Init();
	aszAdded.Init();
	aszRemoved.Init();
	aszKept.Init();
	for (i = 0; i < pcDialog->NumEntries(); i++)
	{
		pcEntry = pcDialog->GetEntry(i);
		if (pcEntry->mbNew)
		{
			aszAdded.Add(pcEntry->mszName.Text());
		}
		else if (!pcEntry->mszName.Equals(pcEntry->mszSource))
		{
			aszOld.Add(pcEntry->mszSource.Text());
			aszNew.Add(pcEntry->mszName.Text());
		}
	}
	for (i = 0; i < pcSolution->mcSolution.NumConfigurations(); i++)
	{
		bFound = false;
		for (j = 0; j < pcDialog->NumEntries(); j++)
		{
			bFound |= !pcDialog->GetEntry(j)->mbNew && pcDialog->GetEntry(j)->mszSource.Equals(pcSolution->mcSolution.GetConfiguration(i));
		}
		if (!bFound)
		{
			aszRemoved.Add(pcSolution->mcSolution.GetConfiguration(i));
		}
	}

	mpcContent->GetCodeModel()->Reset();
	szNote.Init();
	bResult = pcDialog->Write(&szNote);
	pcSolution->Reopen();
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
		for (i = 0; i < aszAdded.NumElements(); i++)
		{
			if (pcWorkspace->FindRunConfigurationBuilding(aszAdded.GetText(i)) == NULL)
			{
				pcWorkspace->AddRunConfiguration(aszAdded.GetText(i), aszAdded.GetText(i));
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
		if (!szNote.Empty())
		{
			szStatus.Append("  ")->Append(szNote);
		}
	}
	else
	{
		szStatus.Set("Could not save the build configurations: a project, a property sheet or the solution couldn't be written.  What was written before it stays.");
	}
	mpcContent->GetSolutions()->WorkspaceChanged(true);
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
	szNote.Kill();
	aszKept.Kill();
	aszRemoved.Kill();
	aszAdded.Kill();
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
// The configurations of the open solutions' projects that none of those
// solutions builds, added to paszFiles and paszNames (a project's file and
// one of its configurations' names).  A solution builds a project as the
// configuration its row names, or with no row as the one called what the
// solution configuration is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::FindUnbuiltConfigurations(CArrayChars* paszFiles, CArrayChars* paszNames)
{
	CBuildWorkspace*					pcWorkspace;
	CVcxSolution*						pcSolution;
	CVcxSolutionProject*				pcProject;
	CVcxSolutionProject*				pcOther;
	CVcxSolutionProjectConfiguration*	pcRow;
	CArrayChars							aszSeen;
	CArrayChars							aszBuilt;
	CArrayChars							aszNames;
	CChars								szName;
	size								i;
	size								j;
	size								k;
	size								l;

	pcWorkspace = mpcContent->GetWorkspace();
	aszSeen.Init();
	for (i = 0; i < pcWorkspace->NumSolutions(); i++)
	{
		for (j = 0; j < pcWorkspace->GetSolution(i)->mcSolution.NumProjects(); j++)
		{
			pcProject = pcWorkspace->GetSolution(i)->mcSolution.GetProject(j);
			if (!pcProject->IsVcxProject() || !pcProject->IsLoaded() || aszSeen.Contains(pcProject->GetProject()->GetFileName()))
			{
				continue;
			}
			aszSeen.Add(pcProject->GetProject()->GetFileName());

			aszBuilt.Init();
			for (k = 0; k < pcWorkspace->NumSolutions(); k++)
			{
				pcSolution = &pcWorkspace->GetSolution(k)->mcSolution;
				for (l = 0; l < pcSolution->NumProjects(); l++)
				{
					pcOther = pcSolution->GetProject(l);
					if (pcOther->IsLoaded() && (StringCompare(pcOther->GetProject()->GetFileName(), pcProject->GetProject()->GetFileName()) == 0))
					{
						break;
					}
				}
				for (size m = 0; (l < pcSolution->NumProjects()) && (m < pcSolution->NumConfigurations()); m++)
				{
					pcRow = pcOther->FindConfiguration(pcSolution->GetConfiguration(m));
					szName.Init(pcRow ? pcRow->GetProjectConfiguration() : pcSolution->GetConfiguration(m));
					if (szName.Contains('|'))
					{
						szName.RemoveEnd(szName.Find('|'));
					}
					aszBuilt.AddIfUnique(szName.Text());
					szName.Kill();
				}
			}

			aszNames.Init();
			GetVcxProjectConfigurationNames(pcProject->GetProject()->GetFileName(), &aszNames);
			for (k = 0; k < aszNames.NumElements(); k++)
			{
				if (!aszBuilt.Contains(aszNames.GetText(k)))
				{
					paszFiles->Add(pcProject->GetProject()->GetFileName());
					paszNames->Add(aszNames.GetText(k));
				}
			}
			aszNames.Kill();
			aszBuilt.Kill();
		}
	}
	aszSeen.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Clean Up (ConfigurationsPlan.txt question 35): the projects'
// configurations that no open solution builds, which removing and renaming
// build configurations leave behind, each a box to tick.  None is ticked:
// a solution that isn't open may build one (a library's Reference, for a
// program of another solution), and that can't be known from here.  Remove
// takes the ticked ones out of their projects, which are read again.  For
// checking, HCP65_IDE_CLEANUP=all ticks them all and each is printed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::ShowCleanUpDialog(void)
{
	CBuildWorkspace*	pcWorkspace;
	CArrayChars			aszFiles;
	CArrayChars			aszNames;
	CFileUtil			cFileUtil;
	CChars				szLabel;
	CChars				szStatus;
	size				uiRemoved;
	size				i;
	bool				bOK;

	aszFiles.Init();
	aszNames.Init();
	FindUnbuiltConfigurations(&aszFiles, &aszNames);
	if (aszFiles.NumElements() == 0)
	{
		mpcContent->ShowStatus("Nothing to clean up: every configuration of the open solutions' projects is built by one of them.");
		aszNames.Kill();
		aszFiles.Kill();
		return;
	}

	mpCleanUp = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Clean Up Configurations");
	mpCleanUp->AddDivider("Not built by any open solution; another solution may build one");
	szLabel.Init();
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		szLabel.Set("");
		cFileUtil.GetFileName(&szLabel, aszFiles.GetText(i));
		szLabel.Append(": ")->Append(aszNames.GetText(i));
		maCleanUpBoxes.Add(mpCleanUp->AddCheckBox(szLabel.Text(), (getenv("HCP65_IDE_CLEANUP") != NULL) && (strcmp(getenv("HCP65_IDE_CLEANUP"), "all") == 0)));
		if (getenv("HCP65_IDE_CLEANUP") != NULL)
		{
			printf("Clean up: %s\n", szLabel.Text());
			fflush(stdout);
		}
	}
	szLabel.Kill();
	mpCleanUp->SetOKCaption("Remove");
	mpcContent->GetScript()->WatchDialog(mpCleanUp);
	bOK = mpCleanUp->Show();
	mpcContent->GetScript()->StopWatchingDialog();

	uiRemoved = 0;
	if (bOK && !mpcContent->IsKeepingSettings())
	{
		mpcContent->ShowStatus("A scripted run doesn't save build configurations.");
	}
	else if (bOK)
	{
		mpcContent->GetCodeModel()->Reset();
		for (i = 0; i < aszFiles.NumElements(); i++)
		{
			if (maCleanUpBoxes.Get(i)->IsTicked() && RemoveVcxProjectConfiguration(aszFiles.GetText(i), aszNames.GetText(i)))
			{
				uiRemoved++;
			}
		}
		pcWorkspace = mpcContent->GetWorkspace();
		for (i = 0; (uiRemoved > 0) && (i < pcWorkspace->NumSolutions()); i++)
		{
			pcWorkspace->GetSolution(i)->Reopen();
		}
		mpcContent->GetSolutions()->WorkspaceChanged(true);
		szStatus.Init("Removed ");
		szStatus.Append((uint32)uiRemoved)->Append((uiRemoved == 1) ? " configuration from its project." : " configurations from their projects.");
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
	}
	maCleanUpBoxes.RemoveAll();
	mpCleanUp = NULL;
	aszNames.Kill();
	aszFiles.Kill();
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


//////////////////////////////////////////////////////////////////////////
//
// What Next and Previous Occurrence need of a tool window (CIDEOccurrences):
// the Build tab, and whether it has a message with a place to go to.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CDockPane> CIDEBuilding::GetSteppablePane(void) const { return mpBuildTool; }
const char* CIDEBuilding::GetSteppableName(void) { return "Build"; }
bool CIDEBuilding::HasSteps(void) { return mpMessages.IsNotNull() && mpMessages->HasMessages(); }
bool CIDEBuilding::Step(bool bNext, bool bKeepKeys) { return mpMessages->Step(bNext, bKeepKeys); }
Ptr<CIDEMessageOutput> CIDEBuilding::GetMessages(void) const { return mpMessages; }
size CIDEBuilding::NumBuilds(void) const { return muiBuilds; }


