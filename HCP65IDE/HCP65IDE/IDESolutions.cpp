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
	mpOpenItem = NULL;
	mpAddSolutionItem = NULL;
	mpCloseSolutionItem = NULL;
	mpRecentMenu = NULL;
	mpClearRecentItem = NULL;
	mpExplorer = NULL;
	mpExplorerTool = NULL;
	mpExplorerMenu = NULL;
	mpStartupItem = NULL;
	mpRemoveSolutionItem = NULL;
	mpFileChooser = NULL;
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
	M_Pointer(mpOpenItem);
	M_Pointer(mpAddSolutionItem);
	M_Pointer(mpCloseSolutionItem);
	M_Pointer(mpRecentMenu);
	M_Pointer(mpClearRecentItem);
	M_Pointer(mpExplorer);
	M_Pointer(mpExplorerTool);
	M_Pointer(mpExplorerMenu);
	M_Pointer(mpStartupItem);
	M_Pointer(mpRemoveSolutionItem);
	M_Pointer(mpFileChooser);
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
	if (mpOpenItem.IsNotNull())
	{
		mpOpenItem->GetClickEvent()->Remove(this);
	}
	if (mpAddSolutionItem.IsNotNull())
	{
		mpAddSolutionItem->GetClickEvent()->Remove(this);
	}
	if (mpCloseSolutionItem.IsNotNull())
	{
		mpCloseSolutionItem->GetClickEvent()->Remove(this);
	}
	if (mpClearRecentItem.IsNotNull())
	{
		mpClearRecentItem->GetClickEvent()->Remove(this);
	}
	if (mpExplorer.IsNotNull() && mpExplorer->GetTree().IsNotNull())
	{
		mpExplorer->GetTree()->GetActivationEvent()->Remove(this);
	}
	if (mpExplorerMenu.IsNotNull())
	{
		mpExplorerMenu->GetItemClickEvent()->Remove(this);
	}
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
	mpFileChooser = OMalloc<CFileChooser>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Open Project/Solution", false, GetStartupFile(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpcContent->GetFolderIcon(), mpcContent->GetFileIcon());
	if (mpFileChooser->Show())
	{
		szChosen.Init(mpFileChooser->GetChosen());
		bResult = OpenSolution(szChosen.Text());
		if (!bResult)
		{
			gcLogger.Error2(__METHOD__, " Could not open [", szChosen.Text(), "].", NULL);
		}
		szChosen.Kill();
	}
	mpFileChooser = NULL;
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
	mpFileChooser = OMalloc<CFileChooser>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Add Solution", false, GetStartupFile(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpcContent->GetFolderIcon(), mpcContent->GetFileIcon());
	if (mpFileChooser->Show())
	{
		szChosen.Init(mpFileChooser->GetChosen());
		bResult = AddSolution(szChosen.Text());
		szChosen.Kill();
	}
	mpFileChooser = NULL;
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
	if (IsSolutionOpen() && !mpcContent->GetCloseGuard()->ApproveLettingGo(MAX_SIZE, szFileName))
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
	bResult = mpcContent->GetWorkspace()->Open(szFile.Text());
	if (!szFile.Equals(szFileName) && (!bResult || (mpcContent->GetWorkspace()->FindSolution(szFileName) == MAX_SIZE)))
	{
		//The workspace beside it no longer has it (it was removed): it is opened on its own.
		bResult = mpcContent->GetWorkspace()->Open(szFileName);
	}
	szFile.Kill();
	mpcContent->GetCodeModel()->Init(mpcContent->GetWorkspace());
	mpcContent->GetCodeModel()->AddDefaultSystemDirectories();
	mpExplorer->ShowWorkspace(mpcContent->GetWorkspace());
	mpcContent->GetNavigator()->ForgetGoToFiles();
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

	mpcContent->GetGitStatus()->FindRepositories();

	//The breakpoints saved beside it, bound once its configuration's line tables are read.
	szName.Init();
	mpcContent->GetDebugging()->GetUserSettingsFileName(&szName);
	mpcContent->GetDebugging()->GetDebugger()->ReadSourceBreakpoints(szName.Text());
	szName.Kill();

	//The configuration chosen last time, if a solution still has it; chosen before the combo boxes tell of it, so it
	//isn't indexed twice.
	mpcContent->GetBuildConfigurations()->SetConfigurationName(mpcContent->GetWorkspace()->GetConfiguration());
	mpcContent->GetBuildConfigurations()->SetRunConfigurationName(mpcContent->GetWorkspace()->GetRunConfigurationName());
	mpcContent->GetBuildConfigurations()->ShowConfigurations();
	mpcContent->GetCodeModel()->StartIndexing();

	//The windows where they were left, and the files that were open opened again.
	mpcContent->LoadLayout();

	//Without them no file parses, and nothing else would say why Go to Source, Find Usages and Rename find nothing.
	if (mpcContent->GetCodeModel()->IsMissingSystemDirectories())
	{
		mpcContent->ShowStatus("MinGW's headers weren't found, so code navigation won't work: install g++-mingw-w64-x86-64-win32 and mingw-w64-common.");
	}
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
	mpcContent->GetCodeModel()->Reset();
	uiSolution = mpcContent->GetWorkspace()->AddSolution(szFileName);
	if (uiSolution == MAX_SIZE)
	{
		mpcContent->GetCodeModel()->StartIndexing();
		return gcLogger.Error2(__METHOD__, " [", szFileName, "] is already open.", NULL);
	}
	if (!mpcContent->GetWorkspace()->GetSolution(uiSolution)->mbRead)
	{
		mpcContent->GetWorkspace()->RemoveSolution(uiSolution);
		mpcContent->GetCodeModel()->StartIndexing();
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
	if ((mpcContent->GetWorkspace()->NumSolutions() < 2) || (uiSolution >= mpcContent->GetWorkspace()->NumSolutions()))
	{
		return false;
	}
	if (mpcContent->GetBuilding()->IsBuilding())
	{
		return gcLogger.Error2(__METHOD__, " A solution can't be removed while a build is running.", NULL);
	}

	mpcContent->GetCodeModel()->Reset();
	mpcContent->GetWorkspace()->RemoveSolution(uiSolution);
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

	if (!mpcContent->IsKeepingSettings())
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

	uiSolution = mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode());
	if ((uiSolution == SOLUTION_EXPLORER_NO_SOLUTION) && (mpcContent->GetWorkspace()->NumSolutions() == 1))
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

	if (uiSolution >= mpcContent->GetWorkspace()->NumSolutions())
	{
		return false;
	}
	if (mpcContent->GetBuilding()->IsBuilding())
	{
		return gcLogger.Error2(__METHOD__, " A solution can't be closed while a build is running.", NULL);
	}

	//Its files with changes are asked about first; it closes afterwards.
	if (!mpcContent->GetCloseGuard()->ApproveLettingGo(uiSolution, NULL))
	{
		return true;
	}

	if (mpcContent->GetWorkspace()->NumSolutions() == 1)
	{
		LetGoOfWorkspace();
		mpcContent->GetCodeModel()->Init(mpcContent->GetWorkspace());
		mpcContent->GetCodeModel()->AddDefaultSystemDirectories();
		mpExplorer->ShowWorkspace(mpcContent->GetWorkspace());
		mpcContent->GetNavigator()->ForgetGoToFiles();
		mpcContent->GetBuildConfigurations()->ShowConfigurations();
		return true;
	}

	//Its files' documents, and diffs of them, are closed (closing a document lets its tab go, see PaneClosed).
	for (i = mpcContent->GetDocuments()->NumTabs(); i > 0; i--)
	{
		pTab = mpcContent->GetDocuments()->GetTab(i - 1);
		if (IsSolutionFile(uiSolution, pTab->GetProject()))
		{
			mpcContent->GetDock()->Close(pTab->GetDockPane());
		}
	}
	for (i = mpcContent->GetDiffs()->NumDiffs(); i > 0; i--)
	{
		pDiff = mpcContent->GetDiffs()->GetDiff(i - 1);
		if (IsSolutionFile(uiSolution, pDiff->GetSide(TDS_Left)->muiProject) || IsSolutionFile(uiSolution, pDiff->GetSide(TDS_Right)->muiProject))
		{
			mpcContent->GetDock()->Close(pDiff->GetDockPane());
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
	return (uiProject < mpcContent->GetWorkspace()->NumProjects()) && (mpcContent->GetWorkspace()->GetProject(uiProject)->uiSolution == uiSolution);
}


//////////////////////////////////////////////////////////////////////////
//
// The solution debugged (F5), and whose configuration the toolbar shows
// first.  The workspace file is written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::SetStartupSolution(size uiSolution)
{
	if (uiSolution >= mpcContent->GetWorkspace()->NumSolutions())
	{
		return false;
	}
	if (uiSolution == mpcContent->GetWorkspace()->GetStartup())
	{
		return true;
	}
	mpcContent->GetWorkspace()->SetStartup(uiSolution);
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

	mpcContent->GetBuilding()->StopBuild();
	mpcContent->GetBuilding()->SetListings(false);
	mpcContent->GetDocuments()->CloseAllFiles();
	mpcContent->GetUsages()->Clear();

	mpcContent->GetCodeModel()->Kill();
	mpcContent->GetWorkspace()->Clear();
	mpcContent->GetNavigator()->ForgetGoToFiles();
	mpcContent->GetBuildConfigurations()->SetConfigurationName("");
	mpcContent->GetBuildConfigurations()->SetRunConfigurationName("");

	//The repositories are found again from the new solutions' files, and read at the next tick.
	mpcContent->GetGitStatus()->ForgetRepositories();
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

	mpExplorer->ShowWorkspace(mpcContent->GetWorkspace());
	mpcContent->GetNavigator()->ForgetGoToFiles();
	mpcContent->GetGitStatus()->FindRepositories();
	mpcContent->GetBuildConfigurations()->ShowConfigurations();
	if (bReindex)
	{
		for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
		{
			mpcContent->GetDocuments()->GetTab(i)->SetProject(mpExplorer->FindFileProject(mpcContent->GetDocuments()->GetTab(i)->GetFileName()));
		}
		mpcContent->GetDiffs()->FindProjects();
		mpcContent->GetCodeModel()->StartIndexing();
		for (i = 0; i < mpcContent->GetDocuments()->NumTabs(); i++)
		{
			mpcContent->GetDocuments()->RequestHighlight(mpcContent->GetDocuments()->GetTab(i));
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

	mpcContent->GetWorkspace()->SetConfiguration(mpcContent->GetBuildConfigurations()->GetConfigurationName());
	mpcContent->GetWorkspace()->SetRunConfigurationName(mpcContent->GetBuildConfigurations()->GetRunConfigurationName());
	if ((mpcContent->GetWorkspace()->NumSolutions() == 0) || !mpcContent->IsKeepingSettings())
	{
		//A scripted run (checking the IDE) leaves the workspace's file alone, as it does the layout.
		return false;
	}
	szFile.Init(mpcContent->GetWorkspace()->GetFile());
	if (szFile.Empty())
	{
		szFile.Set(mpcContent->GetWorkspace()->GetSolution(0)->mszFile);
		cFileUtil.RemoveExtension(&szFile);
		szFile.Append(BUILD_WORKSPACE_EXTENSION);
	}
	bResult = mpcContent->GetWorkspace()->Write(szFile.Text());
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
	return (mpcContent->GetWorkspace()->GetStartupSolution() != NULL) && mpcContent->GetWorkspace()->GetStartupSolution()->mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's file (a .sln or .vcxproj), "" if none is open.
//
//////////////////////////////////////////////////////////////////////////
char* CIDESolutions::GetStartupFile(void)
{
	return (mpcContent->GetWorkspace()->GetStartupSolution() != NULL) ? mpcContent->GetWorkspace()->GetStartupSolution()->mszFile.Text() : (char*)"";
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
	if (mpcContent->GetWorkspace()->GetFile()[0] != '\0')
	{
		pszDest->Set(mpcContent->GetWorkspace()->GetFile());
		return true;
	}
	if (GetStartupFile()[0] != '\0')
	{
		pszDest->Set(GetStartupFile());
		return true;
	}
	return false;
}




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
	if (!mszRecentFile.Empty() && (mpcContent->IsKeepingSettings() || (getenv("HCP65_IDE_RECENT") != NULL)) && !mcRecent.Write(mszRecentFile.Text()))
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

	if (mpRecentMenu.IsNull())
	{
		return;
	}
	mpRecentMenu->RemoveAllItems();
	if (mcRecent.NumFiles() == 0)
	{
		mpRecentMenu->AddItem("(No recent solutions)")->SetEnabled(false);
		return;
	}
	szText.Init();
	for (i = 0; i < mcRecent.NumFiles(); i++)
	{
		szText.Clear();
		szText.Append((uint32)(i + 1));
		szText.Append(' ');
		szText.Append(mcRecent.GetFile(i));
		mpRecentMenu->AddItem(szText.Text())->GetClickEvent()->Add(this, &CIDESolutions::RecentItemClicked);
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A recent solution's item: it is opened at the next tick, once its menu
// is done with the click.  False if pcItem isn't one.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::RecentItemClicked(CMenuItem* pcItem)
{
	Ptr<CMenuItem>	pItem;
	size			i;

	for (i = 0; (i < mpRecentMenu->NumItems()) && (i < mcRecent.NumFiles()); i++)
	{
		pItem = mpRecentMenu->GetItem(i);
		if (pcItem == &pItem)
		{
			OpenRecentSolution(i);
			return;
		}
	}
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
	mpcContent->GetWindow()->Post(this, &CIDESolutions::OpenQueuedSolution);
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


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer, at psPlace: a file's node opens it, and a solution's
// menu (CreateExplorerMenu) sets it as the startup one or removes it.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::CreateExplorerPane(SDockPlace* psPlace)
{
	mpExplorer = OMalloc<CSolutionExplorer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpExplorer->SetBoldColours(mpcContent->GetBoldColours());
	mpExplorer->GetTree()->GetActivationEvent()->Add(this, &CIDESolutions::ExplorerActivated);
	mpExplorer->GetTree()->SetContextMenu(CreateExplorerMenu());
	mpExplorerTool = mpcContent->GetDock()->AddPane(IDE_EXPLORER_ID, "Solution Explorer", NULL, mpExplorer->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer's menu, for the solution of the node selected: Set as
// Startup Solution and Remove.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDESolutions::CreateExplorerMenu(void)
{
	mpExplorerMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpStartupItem = mpExplorerMenu->AddItem("Set as Startup Solution");
	mpRemoveSolutionItem = mpExplorerMenu->AddItem("Remove Solution");
	mpExplorerMenu->GetItemClickEvent()->Add(this, &CIDESolutions::ExplorerMenuItemClicked);
	return mpExplorerMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer's menu's items.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::ExplorerMenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem == &mpStartupItem)
	{
		SetStartupSolution(mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode()));
	}
	else if (pcItem == &mpRemoveSolutionItem)
	{
		RemoveSolution(mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode()));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A file opened in Solution Explorer is shown in the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::ExplorerActivated(void)
{
	CChars	szPath;

	szPath.Init();
	if (mpExplorer->GetSelectedFilePath(&szPath))
	{
		mpcContent->GetDocuments()->OpenFile(szPath.Text(), mpExplorer->GetNodeProject(mpExplorer->GetTree()->GetSelectedNode()));
	}
	szPath.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+1, as IntelliJ IDEA's Project tool window: Solution Explorer with
// the active file selected in it.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("View.SolutionExplorer", "Solution Explorer", "Alt+1", Ptr<CImageCel>(), this, &CIDESolutions::ShowActiveFileInExplorer);
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+1: Solution Explorer is shown with the active file selected, its
// folders opened and the row scrolled into view.  The editor keeps the
// keys, its caret where it was.  False if there is no file to show.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::ShowActiveFileInExplorer(void)
{
	Ptr<CTreeContainer>	pTree;
	int32				iNode;

	pTree = mpExplorer->GetTree();
	mpcContent->GetDock()->Activate(mpExplorerTool);
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		mpcContent->GetWindow()->GetFocus()->SetFocus(pTree);
		return false;
	}
	iNode = mpExplorer->FindFilePathNode(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), mpcContent->GetDocuments()->GetActiveTab()->GetProject());
	if (iNode != TREE_NO_NODE)
	{
		pTree->SelectNode(iNode);
		mpcContent->GetWindow()->Layout();
		pTree->ShowRow(pTree->FindNodeRow(iNode));
	}
	mpcContent->GetDock()->Activate(mpcContent->GetDocuments()->GetActiveTab()->GetDockPane());
	return iNode != TREE_NO_NODE;
}


Ptr<CSolutionExplorer> CIDESolutions::GetExplorer(void) { return mpExplorer; }
Ptr<CDockPane> CIDESolutions::GetExplorerTool(void) { return mpExplorerTool; }
Ptr<CMenuContainer> CIDESolutions::GetExplorerMenu(void) { return mpExplorerMenu; }


//////////////////////////////////////////////////////////////////////////
//
// File's Open Project/Solution, Add Solution, Close Solution, Recent
// Solutions (an item for each, or one saying there are none) and Clear
// Recent Solutions.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::AddFileItems(Ptr<CMenuContainer> pFile)
{
	mpOpenItem = pFile->AddItem("Open Project/Solution...", mpcContent->GetFolderIcon());
	mpOpenItem->GetClickEvent()->Add(this, &CIDESolutions::FileMenuItemClicked);
	mpAddSolutionItem = pFile->AddItem("Add Solution...");
	mpAddSolutionItem->GetClickEvent()->Add(this, &CIDESolutions::FileMenuItemClicked);
	mpCloseSolutionItem = pFile->AddItem("Close Solution");
	mpCloseSolutionItem->GetClickEvent()->Add(this, &CIDESolutions::FileMenuItemClicked);
	mpRecentMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	pFile->AddItem("Recent Solutions", NULL, mpRecentMenu);
	mpClearRecentItem = pFile->AddItem("Clear Recent Solutions");
	mpClearRecentItem->GetClickEvent()->Add(this, &CIDESolutions::FileMenuItemClicked);
	ShowRecentMenu();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::FileMenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem == &mpOpenItem)
	{
		ShowOpenSolutionDialog();
	}
	else if (pcItem == &mpAddSolutionItem)
	{
		ShowAddSolutionDialog();
	}
	else if (pcItem == &mpCloseSolutionItem)
	{
		CloseSelectedSolution();
	}
	else if (pcItem == &mpClearRecentItem)
	{
		ClearRecentSolutions();
	}
}
