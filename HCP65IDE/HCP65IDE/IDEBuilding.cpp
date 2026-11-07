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
	M_Pointer(mpBuildButton);
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
// output in the Build tab.  False if there is no solution or
// configuration, or a build is running.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuilding::Build(void)
{
	CArrayChars	aszLines;
	bool		bResult;

	if (!mpcContent->GetSolutions()->IsSolutionOpen() || !mpcContent->GetBuildConfigurations()->HasConfiguration() || mcBuild.IsRunning())
	{
		return false;
	}
	maiBuildOrder.ReInit();
	mpcContent->GetWorkspace()->GetBuildOrder(mpcContent->GetBuildConfigurations()->GetConfigurationName(), &maiBuildOrder);
	if (maiBuildOrder.NumElements() == 0)
	{
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
// Build (Control+Shift+B, as Visual Studio's).
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuilding::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Build.Build", "Build", "Control+Shift+B", Ptr<CImageCel>(), this, &CIDEBuilding::Build);
}


//////////////////////////////////////////////////////////////////////////
//
// The toolbar's Build button.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CButton> CIDEBuilding::CreateBuildButton(void)
{
	mpBuildButton = OMalloc<CButton>(mpcContent->GetWindow(), mpcContent->GetTheme(), "Build");
	mpBuildButton->SetAlignment(CSH_Left, CSV_Center);
	mpBuildButton->SetCommand(mpcContent->GetCommands()->Find("Build.Build"));
	return mpBuildButton;
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
