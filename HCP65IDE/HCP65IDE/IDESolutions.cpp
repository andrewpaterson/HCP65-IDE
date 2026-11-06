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
#include "IDESolutions.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mcRecent.Init();
	mszRecentFile.Init();
	mszQueuedOpen.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CBuildRecentFiles, mcRecent);
	U_String(mszRecentFile);
	U_String(mszQueuedOpen);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::Free(void)
{
	mszQueuedOpen.Kill();
	mszRecentFile.Kill();
	mcRecent.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::Save(CObjectWriter* pcFile) { return false; }
bool CIDESolutions::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Asks for a workspace (.hcp65workspace), solution (.sln) or project
// (.vcxproj) and opens it, starting on the one open.  False if none is
// chosen or it can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::ShowOpenSolutionDialog(void)
{
	static SFileChooserType	asTypes[] =
	{
		{ "Solutions (*.sln)", "sln" },
		{ "Workspaces (*.hcp65workspace)", "hcp65workspace" },
		{ "Projects (*.vcxproj)", "vcxproj" },
		{ "All Files", NULL },
	};
	CChars	szChosen;
	bool	bResult;

	bResult = false;
	mpcContent->mpFileChooser = OMalloc<CFileChooser>(mpcContent->mpWindow, &mpcContent->msDialogLook, "Open Project/Solution", false, GetStartupFile(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpcContent->mpFolderIcon, mpcContent->mpFileIcon);
	if (mpcContent->mpFileChooser->Show())
	{
		szChosen.Init(mpcContent->mpFileChooser->GetChosen());
		bResult = OpenSolution(szChosen.Text());
		if (!bResult)
		{
			gcLogger.Error2(__METHOD__, " Could not open [", szChosen.Text(), "].", NULL);
		}
		szChosen.Kill();
	}
	mpcContent->mpFileChooser = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for a solution (.sln) or project (.vcxproj) to open with those
// open.  False if none is chosen or it can't be added.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::ShowAddSolutionDialog(void)
{
	static SFileChooserType	asTypes[] =
	{
		{ "Solutions (*.sln)", "sln" },
		{ "Projects (*.vcxproj)", "vcxproj" },
		{ "All Files", NULL },
	};
	CChars	szChosen;
	bool	bResult;

	bResult = false;
	mpcContent->mpFileChooser = OMalloc<CFileChooser>(mpcContent->mpWindow, &mpcContent->msDialogLook, "Add Solution", false, GetStartupFile(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpcContent->mpFolderIcon, mpcContent->mpFileIcon);
	if (mpcContent->mpFileChooser->Show())
	{
		szChosen.Init(mpcContent->mpFileChooser->GetChosen());
		bResult = AddSolution(szChosen.Text());
		szChosen.Kill();
	}
	mpcContent->mpFileChooser = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads a workspace (.hcp65workspace) and its solutions, or a .sln (and
// its projects), and shows them in Solution Explorer.  A .vcxproj on its
// own is put in a solution of its own, as Visual Studio does.  A solution
// with a workspace of its name beside it that has it opens the workspace:
// the solutions it was open with last time.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::OpenSolution(const char* szFileName)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	CChars		szName;
	bool		bResult;

	//The files with changes of what is open are asked about first; it opens afterwards.
	if (IsSolutionOpen() && !mpcContent->mpCloseGuard->ApproveLettingGo(MAX_SIZE, szFileName))
	{
		return true;
	}

	szFile.Init(szFileName);
	if (!szFile.EndsWithIgnoreCase(BUILD_WORKSPACE_EXTENSION))
	{
		cFileUtil.RemoveExtension(&szFile);
		szFile.Append(BUILD_WORKSPACE_EXTENSION);
		if (!cFileUtil.Exists(szFile.Text()))
		{
			szFile.Set(szFileName);
		}
	}

	//The solutions being let go of are kept as they were left, and their files closed.
	LetGoOfWorkspace();
	bResult = mpcContent->mcWorkspace.Open(szFile.Text());
	if (!szFile.Equals(szFileName) && (!bResult || (mpcContent->mcWorkspace.FindSolution(szFileName) == MAX_SIZE)))
	{
		//The workspace beside it no longer has it (it was removed): it is opened on its own.
		bResult = mpcContent->mcWorkspace.Open(szFileName);
	}
	szFile.Kill();
	mpcContent->mcCodeModel.Init(&mpcContent->mcWorkspace);
	mpcContent->mcCodeModel.AddDefaultSystemDirectories();
	mpcContent->mpExplorer->ShowWorkspace(&mpcContent->mcWorkspace);
	mpcContent->mpNavigator->ForgetGoToFiles();
	if (!bResult || !IsSolutionOpen())
	{
		//One that isn't there any more isn't recent.
		if (!cFileUtil.Exists(szFileName) && mcRecent.Remove(szFileName))
		{
			WriteRecent();
		}
		return false;
	}
	mcRecent.Add(szFileName);
	WriteRecent();

	mpcContent->mpGitStatus->FindRepositories();

	//The breakpoints saved beside it, bound once its configuration's line tables are read.
	szName.Init();
	mpcContent->mpDebugging->GetUserSettingsFileName(&szName);
	mpcContent->mpDebugging->GetDebugger()->ReadSourceBreakpoints(szName.Text());
	szName.Kill();

	//The configuration chosen last time, if a solution still has it; chosen before the combo boxes tell of it, so it
	//isn't indexed twice.
	mpcContent->mszConfiguration.Set(mpcContent->mcWorkspace.GetConfiguration());
	mpcContent->mpBuildConfigurations->ShowConfigurations();
	mpcContent->mcCodeModel.StartIndexing();

	//The windows where they were left, and the files that were open opened again.
	mpcContent->LoadLayout();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The first file opened (OpenSolution) and the rest added to it, as the
// command line names them.  False if any couldn't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::OpenSolutions(CArrayChars* paszFileNames)
{
	bool	bResult;
	size	i;

	if (paszFileNames->NumElements() == 0)
	{
		return false;
	}
	bResult = OpenSolution(paszFileNames->GetText(0));
	for (i = 1; i < paszFileNames->NumElements(); i++)
	{
		bResult &= AddSolution(paszFileNames->GetText(i));
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Another solution opened with those open (File, Add Solution), its
// projects indexed with theirs.  The workspace file is written.  With
// none open it is opened.  False if it is already open or can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::AddSolution(const char* szFileName)
{
	size	uiSolution;

	if (!IsSolutionOpen())
	{
		return OpenSolution(szFileName);
	}

	//The indexing uses the solutions, so it stops while they change.
	mpcContent->mcCodeModel.Reset();
	uiSolution = mpcContent->mcWorkspace.AddSolution(szFileName);
	if (uiSolution == MAX_SIZE)
	{
		mpcContent->mcCodeModel.StartIndexing();
		return gcLogger.Error2(__METHOD__, " [", szFileName, "] is already open.", NULL);
	}
	if (!mpcContent->mcWorkspace.GetSolution(uiSolution)->mbRead)
	{
		mpcContent->mcWorkspace.RemoveSolution(uiSolution);
		mpcContent->mcCodeModel.StartIndexing();
		return gcLogger.Error2(__METHOD__, " Could not read [", szFileName, "].", NULL);
	}
	WorkspaceChanged(true);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A solution taken out of those open (its node's Remove).  Its files stay
// open.  The last one, or one while a build is running, isn't.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::RemoveSolution(size uiSolution)
{
	if ((mpcContent->mcWorkspace.NumSolutions() < 2) || (uiSolution >= mpcContent->mcWorkspace.NumSolutions()))
	{
		return false;
	}
	if (mpcContent->mpBuilding->IsBuilding())
	{
		return gcLogger.Error2(__METHOD__, " A solution can't be removed while a build is running.", NULL);
	}

	mpcContent->mcCodeModel.Reset();
	mpcContent->mcWorkspace.RemoveSolution(uiSolution);
	WorkspaceChanged(true);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Opens the solution (or workspace) that was open when the IDE was last
// closed, as the IDE starts with none named.  False if none was, or it
// isn't there any more.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::OpenLastSolution(void)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	bool		bResult;

	szFile.Init(mcRecent.GetOpen());
	bResult = !szFile.Empty() && cFileUtil.Exists(szFile.Text()) && OpenSolution(szFile.Text());
	szFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// As the IDE closes: the workspace (or the solution) open is kept with the
// recent solutions, to be opened again next time (OpenLastSolution), or
// that none was.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::RememberOpenSolution(void)
{
	CFileUtil	cFileUtil;
	CChars		szFile;

	if (!mpcContent->mbKeepSettings)
	{
		return;
	}
	szFile.Init();
	if (IsSolutionOpen() && GetSettingsBaseFile(&szFile))
	{
		cFileUtil.FullPath(&szFile);
	}
	else
	{
		szFile.Clear();
	}
	mcRecent.SetOpen(szFile.Text());
	szFile.Kill();
	WriteRecent();
}


//////////////////////////////////////////////////////////////////////////
//
// File, Close Solution: the solution Solution Explorer's selection is in
// (the solution, a project, a filter or a file of it), or the only one if
// nothing is selected.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::CloseSelectedSolution(void)
{
	size	uiSolution;

	uiSolution = mpcContent->mpExplorer->GetNodeSolution(mpcContent->mpExplorer->GetTree()->GetSelectedNode());
	if ((uiSolution == SOLUTION_EXPLORER_NO_SOLUTION) && (mpcContent->mcWorkspace.NumSolutions() == 1))
	{
		uiSolution = 0;
	}
	return CloseSolution(uiSolution);
}


//////////////////////////////////////////////////////////////////////////
//
// A solution closed: its files' documents are closed and it is taken out
// of those open.  The last one closes everything, as Visual Studio's Close
// Solution does: the layout is kept beside it for the next time, and
// Solution Explorer is left empty.  Not while a build is running.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::CloseSolution(size uiSolution)
{
	size				i;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEDiffTab>	pDiff;

	if (uiSolution >= mpcContent->mcWorkspace.NumSolutions())
	{
		return false;
	}
	if (mpcContent->mpBuilding->IsBuilding())
	{
		return gcLogger.Error2(__METHOD__, " A solution can't be closed while a build is running.", NULL);
	}

	//Its files with changes are asked about first; it closes afterwards.
	if (!mpcContent->mpCloseGuard->ApproveLettingGo(uiSolution, NULL))
	{
		return true;
	}

	if (mpcContent->mcWorkspace.NumSolutions() == 1)
	{
		LetGoOfWorkspace();
		mpcContent->mcCodeModel.Init(&mpcContent->mcWorkspace);
		mpcContent->mcCodeModel.AddDefaultSystemDirectories();
		mpcContent->mpExplorer->ShowWorkspace(&mpcContent->mcWorkspace);
		mpcContent->mpNavigator->ForgetGoToFiles();
		mpcContent->mpBuildConfigurations->ShowConfigurations();
		return true;
	}

	//Its files' documents, and diffs of them, are closed (closing a document lets its tab go, see PaneClosed).
	for (i = mpcContent->mpDocuments->NumTabs(); i > 0; i--)
	{
		pTab = mpcContent->mpDocuments->GetTab(i - 1);
		if (IsSolutionFile(uiSolution, pTab->GetProject()))
		{
			mpcContent->mpDock->Close(pTab->GetDockPane());
		}
	}
	for (i = mpcContent->mpDiffs->NumDiffs(); i > 0; i--)
	{
		pDiff = mpcContent->mpDiffs->GetDiff(i - 1);
		if (IsSolutionFile(uiSolution, pDiff->GetSide(TDS_Left)->muiProject) || IsSolutionFile(uiSolution, pDiff->GetSide(TDS_Right)->muiProject))
		{
			mpcContent->mpDock->Close(pDiff->GetDockPane());
		}
	}
	return RemoveSolution(uiSolution);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the workspace's project uiProject (MAX_SIZE for a file in none)
// is one of solution uiSolution's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::IsSolutionFile(size uiSolution, size uiProject)
{
	return (uiProject < mpcContent->mcWorkspace.NumProjects()) && (mpcContent->mcWorkspace.GetProject(uiProject)->uiSolution == uiSolution);
}


//////////////////////////////////////////////////////////////////////////
//
// The solution debugged (F5), and whose configuration the toolbar shows
// first.  The workspace file is written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::SetStartupSolution(size uiSolution)
{
	if (uiSolution >= mpcContent->mcWorkspace.NumSolutions())
	{
		return false;
	}
	if (uiSolution == mpcContent->mcWorkspace.GetStartup())
	{
		return true;
	}
	mpcContent->mcWorkspace.SetStartup(uiSolution);
	WorkspaceChanged(false);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What is open is let go of: its layout saved, a build of it stopped, its
// files closed and the references found in it forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::LetGoOfWorkspace(void)
{
	mpcContent->SaveLayout();

	mpcContent->mpBuilding->StopBuild();
	mpcContent->mpBuilding->SetListings(false);
	mpcContent->mpDocuments->CloseAllFiles();
	mpcContent->mpUsages->Clear();

	mpcContent->mcCodeModel.Kill();
	mpcContent->mcWorkspace.Clear();
	mpcContent->mpNavigator->ForgetGoToFiles();
	mpcContent->mszConfiguration.Clear();

	//The repositories are found again from the new solutions' files, and read at the next tick.
	mpcContent->mpGitStatus->ForgetRepositories();
}


//////////////////////////////////////////////////////////////////////////
//
// After a solution is added, removed or made the startup one: Solution
// Explorer and the toolbar show it, the open files are given their
// projects again and (bReindex) the code is indexed again; and the
// workspace file is written.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::WorkspaceChanged(bool bReindex)
{
	size	i;

	mpcContent->mpExplorer->ShowWorkspace(&mpcContent->mcWorkspace);
	mpcContent->mpNavigator->ForgetGoToFiles();
	mpcContent->mpGitStatus->FindRepositories();
	mpcContent->mpBuildConfigurations->ShowConfigurations();
	if (bReindex)
	{
		for (i = 0; i < mpcContent->mpDocuments->NumTabs(); i++)
		{
			mpcContent->mpDocuments->GetTab(i)->SetProject(mpcContent->mpExplorer->FindFileProject(mpcContent->mpDocuments->GetTab(i)->GetFileName()));
		}
		mpcContent->mpDiffs->FindProjects();
		mpcContent->mcCodeModel.StartIndexing();
		for (i = 0; i < mpcContent->mpDocuments->NumTabs(); i++)
		{
			mpcContent->mpDocuments->HighlightTab(mpcContent->mpDocuments->GetTab(i));
		}
	}
	WriteWorkspace();
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace file, with the configuration chosen: where it was read
// from, or beside the first solution with its name (so its layout and
// breakpoints are still the solution's).
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::WriteWorkspace(void)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	bool		bResult;

	mpcContent->mcWorkspace.SetConfiguration(mpcContent->mszConfiguration.Text());
	if ((mpcContent->mcWorkspace.NumSolutions() == 0) || !mpcContent->mbKeepSettings)
	{
		//A scripted run (checking the IDE) leaves the workspace's file alone, as it does the layout.
		return false;
	}
	szFile.Init(mpcContent->mcWorkspace.GetFile());
	if (szFile.Empty())
	{
		szFile.Set(mpcContent->mcWorkspace.GetSolution(0)->mszFile);
		cFileUtil.RemoveExtension(&szFile);
		szFile.Append(BUILD_WORKSPACE_EXTENSION);
	}
	bResult = mpcContent->mcWorkspace.Write(szFile.Text());
	if (!bResult)
	{
		gcLogger.Error2(__METHOD__, " Could not write [", szFile.Text(), "].", NULL);
	}
	szFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The repositories each solution's files are in, their files coloured by
// their statuses, read before they are shown.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// The startup solution is there and could be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::IsSolutionOpen(void)
{
	return (mpcContent->mcWorkspace.GetStartupSolution() != NULL) && mpcContent->mcWorkspace.GetStartupSolution()->mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's file (a .sln or .vcxproj), "" if none is open.
//
//////////////////////////////////////////////////////////////////////////
char* CIDESolutions::GetStartupFile(void)
{
	return (mpcContent->mcWorkspace.GetStartupSolution() != NULL) ? mpcContent->mcWorkspace.GetStartupSolution()->mszFile.Text() : (char*)"";
}


//////////////////////////////////////////////////////////////////////////
//
// The file the per-user files (the layout, the breakpoints) are named
// after: the workspace file, or the solution if there isn't one.  False
// if nothing is open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::GetSettingsBaseFile(CChars* pszDest)
{
	if (mpcContent->mcWorkspace.GetFile()[0] != '\0')
	{
		pszDest->Set(mpcContent->mcWorkspace.GetFile());
		return true;
	}
	if (GetStartupFile()[0] != '\0')
	{
		pszDest->Set(GetStartupFile());
		return true;
	}
	return false;
}


CBuildWorkspace* CIDESolutions::GetWorkspace(void) { return &mpcContent->mcWorkspace; }


CBuildRecentFiles* CIDESolutions::GetRecent(void) { return &mcRecent; }


//////////////////////////////////////////////////////////////////////////
//
// The user's file the recent solutions are kept in, read now.  Empty,
// they aren't kept.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::SetRecentFile(const char* szFile)
{
	mszRecentFile.Set(szFile ? szFile : "");
	mcRecent.Clear();
	if (!mszRecentFile.Empty())
	{
		mcRecent.Read(mszRecentFile.Text());
	}
	ShowRecentMenu();
}


//////////////////////////////////////////////////////////////////////////
//
// File, Clear Recent Solutions.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::ClearRecentSolutions(void)
{
	mcRecent.Clear();
	WriteRecent();
}


//////////////////////////////////////////////////////////////////////////
//
// The recent solutions to the user's file, and shown in File, Recent
// Solutions.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::WriteRecent(void)
{
	//A scripted run writes the recent solutions only to a file of its own (HCP65_IDE_RECENT).
	if (!mszRecentFile.Empty() && (mpcContent->mbKeepSettings || (getenv("HCP65_IDE_RECENT") != NULL)) && !mcRecent.Write(mszRecentFile.Text()))
	{
		gcLogger.Error2(__METHOD__, " Could not write [", mszRecentFile.Text(), "].", NULL);
	}
	ShowRecentMenu();
}


//////////////////////////////////////////////////////////////////////////
//
// An item for each recent solution, the latest first, as Visual Studio
// numbers them: "1 D:\...\HCP65ModPlay.sln".  With none, one that says so.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::ShowRecentMenu(void)
{
	CChars	szText;
	size	i;

	if (mpcContent->mpRecentMenu.IsNull())
	{
		return;
	}
	mpcContent->mpRecentMenu->RemoveAllItems();
	if (mcRecent.NumFiles() == 0)
	{
		mpcContent->mpRecentMenu->AddItem("(No recent solutions)");
		return;
	}
	szText.Init();
	for (i = 0; i < mcRecent.NumFiles(); i++)
	{
		szText.Clear();
		szText.Append((uint32)(i + 1));
		szText.Append(' ');
		szText.Append(mcRecent.GetFile(i));
		mpcContent->mpRecentMenu->AddItem(szText.Text());
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A recent solution's item: it is opened at the next tick, once its menu
// is done with the click.  False if pcItem isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::RecentItemClicked(CMenuItem* pcItem)
{
	Ptr<CMenuItem>	pItem;
	size			i;

	if (mpcContent->mpRecentMenu.IsNull())
	{
		return false;
	}
	for (i = 0; (i < mpcContent->mpRecentMenu->NumItems()) && (i < mcRecent.NumFiles()); i++)
	{
		pItem = mpcContent->mpRecentMenu->GetItem(i);
		if (pcItem == &pItem)
		{
			OpenRecentSolution(i);
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The recent solution uiIndex (0 the latest) is opened at the next tick.
// False if there isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::OpenRecentSolution(size uiIndex)
{
	if (uiIndex >= mcRecent.NumFiles())
	{
		return false;
	}
	mszQueuedOpen.Set(mcRecent.GetFile(uiIndex));
	mpcContent->mpWindow->Post(this, &CIDESolutions::OpenQueuedSolution);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The recent solution chosen, opened after its menu's click.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::OpenQueuedSolution(void)
{
	CChars	szOpen;

	szOpen.Init(mszQueuedOpen);
	mszQueuedOpen.Clear();
	if (!OpenSolution(szOpen.Text()))
	{
		gcLogger.Error2(__METHOD__, " Could not open [", szOpen.Text(), "].", NULL);
	}
	szOpen.Kill();
}
