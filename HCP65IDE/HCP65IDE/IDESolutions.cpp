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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/Timer.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "IDETemplateVariables.h"
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
	mpRecentMenu = NULL;
	mpExplorer = NULL;
	mpExplorerTool = NULL;
	mpExplorerMenu = NULL;
	mpFileChooser = NULL;
	mszQueuedCommand.Init();
	mcRecent.Init();
	mszRecentFile.Init();
	mszQueuedOpen.Init();
	mbAlwaysSelect = false;
	mbSingleClick = false;
	mbSelectPosted = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpRecentMenu);
	M_Pointer(mpExplorer);
	M_Pointer(mpExplorerTool);
	M_Pointer(mpExplorerMenu);
	M_Pointer(mpFileChooser);
	U_String(mszQueuedCommand);
	U_Unknown(CRecentFiles, mcRecent);
	U_String(mszRecentFile);
	U_String(mszQueuedOpen);
	U_Bool(mbAlwaysSelect);
	U_Bool(mbSingleClick);
	U_Bool(mbSelectPosted);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::Free(void)
{
	if (mpExplorer.IsNotNull() && mpExplorer->GetTree().IsNotNull())
	{
		mpExplorer->GetTree()->GetActivationEvent()->Remove(this);
	}
	mszQueuedCommand.Kill();
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
	mpcContent->GetCodeModel()->ForgetProjectFiles();
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
	ReadFileTemplates(szFileName);

	//What its user left beside it: the breakpoints, bound once its configuration's line tables are read, and the
	//bookmarks.
	mpcContent->GetUserFile()->Read();

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
		mpcContent->GetCodeModel()->ForgetProjectFiles();
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
// A solution's startup project, by its index in the solution, as Visual
// Studio's Set as Startup Project: the application of the solution's that
// is built into its program (an HCP65 build leaves the others out) and
// that Start runs.  The workspace file is written.  False if it isn't an
// application.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::SetStartupProject(size uiSolution, size uiProject)
{
	CBuildWorkspace*	pcWorkspace;

	pcWorkspace = mpcContent->GetWorkspace();
	if (uiSolution >= pcWorkspace->NumSolutions())
	{
		return false;
	}
	if (uiProject == pcWorkspace->GetStartupProject(uiSolution))
	{
		return true;
	}
	if (!pcWorkspace->SetStartupProject(uiSolution, uiProject))
	{
		return false;
	}
	WorkspaceChanged(false);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Properties... is for a project selected in Solution Explorer.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::CanShowProjectProperties(void)
{
	SSolutionExplorerNode*	psNode;

	psNode = mpExplorer.IsNotNull() ? mpExplorer->GetNode(mpExplorer->GetTree()->GetSelectedNode()) : NULL;
	return psNode && (psNode->eType == SENT_Project) && (psNode->uiSolution < mpcContent->GetWorkspace()->NumSolutions());
}


//////////////////////////////////////////////////////////////////////////
//
// The selected project's properties, kept in its .hcp65project: its
// licence, what the File Header template writes at the top of the files
// made in it (LGPL 3, AGPL 3 or None), and the mkdocs.yml of the website
// its pages are part of, for a workspace without one as an item
// (relative to the project's folder, HCP65/Website/DocsPlan.txt E3).
// False if nothing was changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::ShowProjectProperties(void)
{
	static const char*		aszLicences[] = { IDE_LICENCE_LGPL, IDE_LICENCE_AGPL, IDE_LICENCE_NONE };
	SSolutionExplorerNode*	psNode;
	CVcxSolutionProject*	pcSolutionProject;
	CVcxProject*			pcProject;
	Ptr<CDialog>			pDialog;
	Ptr<CComboBox>			pLicence;
	Ptr<CTextField>			pDocsSite;
	CChars					szFile;
	CChars					szLicence;
	CChars					szDocsSite;
	CChars					szText;
	int32					iSelected;
	size					i;
	bool					bResult;

	if (!CanShowProjectProperties())
	{
		mpcContent->ShowStatus("Select a project in Solution Explorer to see its properties.");
		return false;
	}
	psNode = mpExplorer->GetNode(mpExplorer->GetTree()->GetSelectedNode());
	pcSolutionProject = mpcContent->GetWorkspace()->GetSolution(psNode->uiSolution)->mcSolution.GetProject(psNode->uiProject);
	pcProject = pcSolutionProject ? pcSolutionProject->GetProject() : NULL;
	if (!pcProject)
	{
		mpcContent->ShowStatus("The project couldn't be read.");
		return false;
	}
	szFile.Init(pcProject->GetFileName());
	szLicence.Init();
	ReadIDEProjectLicence(szFile.Text(), &szLicence);
	szDocsSite.Init();
	ReadIDEProjectDocsSite(szFile.Text(), &szDocsSite);
	iSelected = 0;
	for (i = 0; i < 3; i++)
	{
		iSelected = szLicence.Equals(aszLicences[i]) ? (int32)i : iSelected;
	}

	szText.Init(pcProject->GetName());
	szText.Append(" Properties");
	pDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szText.Text());
	pLicence = pDialog->AddComboBox("Licence of its new files:", aszLicences, 3, iSelected);
	pDocsSite = pDialog->AddTextField("Its pages' website (mkdocs.yml):", szDocsSite.Text());
	mpcContent->GetScript()->WatchDialog(pDialog);
	bResult = pDialog->Show(420);
	mpcContent->GetScript()->StopWatchingDialog();
	iSelected = pLicence->GetSelected();
	szText.Set(pDocsSite->GetText());
	pDocsSite = NULL;
	pLicence = NULL;
	pDialog = NULL;

	bResult = bResult && (iSelected >= 0) && (!szLicence.Equals(aszLicences[iSelected]) || !szDocsSite.Equals(szText.Text()));
	if (bResult)
	{
		szDocsSite.Set(szText.Text());
		szText.Set(pcProject->GetName());
		if (WriteIDEProjectProperties(szFile.Text(), aszLicences[iSelected], szDocsSite.Text()))
		{
			szText.Append("'s new files are written with the licence ")->Append(aszLicences[iSelected]);
			if (szDocsSite.Empty())
			{
				szText.Append('.');
			}
			else
			{
				szText.Append("; its pages are part of ")->Append(szDocsSite)->Append('.');
			}
		}
		else
		{
			szText.Append("'s properties couldn't be written.");
			bResult = false;
		}
		mpcContent->ShowStatus(szText.Text());
	}
	szText.Kill();
	szDocsSite.Kill();
	szLicence.Kill();
	szFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Solution Explorer's selected node is a project that could be
// made its solution's startup project: an application that isn't it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESolutions::CanSetStartupProject(void)
{
	SSolutionExplorerNode*		psNode;
	CBuildWorkspaceSolution*	pcSolution;

	psNode = mpExplorer.IsNotNull() ? mpExplorer->GetNode(mpExplorer->GetTree()->GetSelectedNode()) : NULL;
	if ((psNode == NULL) || (psNode->eType != SENT_Project) || (psNode->uiSolution >= mpcContent->GetWorkspace()->NumSolutions()))
	{
		return false;
	}
	pcSolution = mpcContent->GetWorkspace()->GetSolution(psNode->uiSolution);
	return pcSolution->IsApplication(psNode->uiProject) && (psNode->uiProject != pcSolution->GetStartupProject());
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
	mpcContent->GetRecent()->LetGo();
	mpcContent->GetDocuments()->CloseAllFiles();
	mpcContent->GetUsages()->Clear();
	mpcContent->GetFindInFiles()->Clear();
	mpcContent->GetDuplicates()->Clear();
	mpcContent->GetBookmarks()->Clear();
	mpcContent->GetRecent()->Clear();
	mpcContent->GetHierarchy()->Clear();

	mpcContent->GetCodeModel()->Kill();
	mpcContent->GetWorkspace()->Clear();
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
	mpcContent->GetCodeModel()->ForgetProjectFiles();
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
bool CIDESolutions::IsSolutionOpen(void) const
{
	return (mpcContent->GetWorkspace()->GetStartupSolution() != NULL) && mpcContent->GetWorkspace()->GetStartupSolution()->mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's file (a .sln or .vcxproj), "" if none is open.
//
//////////////////////////////////////////////////////////////////////////
char* CIDESolutions::GetStartupFile(void) const
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




CRecentFiles* CIDESolutions::GetRecent(void) { return &mcRecent; }


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
// The file and code templates for what was opened (szFileName): the
// built-in ones, the user's Default scheme's (beside their settings,
// when they are kept) and the Workspace scheme's (beside the
// .hcp65workspace, or the solution opened on its own).
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::ReadFileTemplates(const char* szFileName)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	CChars		szDefault;
	CChars		szWorkspace;
	char*		szSettings;
	char*		szWorkspaceFile;

	szName.Init();
	szDefault.Init();
	szWorkspace.Init();
	szSettings = mpcContent->GetSettings()->GetSettingsFile();
	if (szSettings && (szSettings[0] != '\0'))
	{
		cFileUtil.SplitPath(szSettings, &szName, &szDefault);
	}
	szWorkspaceFile = mpcContent->GetWorkspace()->GetFile();
	szName.Clear();
	cFileUtil.SplitPath((szWorkspaceFile && (szWorkspaceFile[0] != '\0')) ? szWorkspaceFile : szFileName, &szName, &szWorkspace);
	if (szWorkspace.Empty())
	{
		szWorkspace.Set(".");
	}
	if (!mpcContent->GetFileTemplates()->Read(szDefault.Text(), szWorkspace.Text()))
	{
		mpcContent->ShowStatus(mpcContent->GetFileTemplates()->GetError());
	}
	szWorkspace.Kill();
	szDefault.Kill();
	szName.Kill();
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
// Startup Solution and Remove; and for a project selected, Set as Startup
// Project.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDESolutions::CreateExplorerMenu(void)
{
	mpExplorerMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpExplorerMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.SetStartupSolution"));
	mpExplorerMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.SetStartupProject"));
	mpExplorerMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.RemoveSolution"));
	mpExplorerMenu->AddSeparator();
	mpExplorerMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.RenameFile"));
	mpExplorerMenu->AddSeparator();
	mpExplorerMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.ProjectProperties"));
	mpExplorerMenu->AddSeparator();
	mpExplorerMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.AlwaysSelectOpenedFile"));
	mpExplorerMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.OpenFilesWithSingleClick"));
	return mpExplorerMenu;
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
	pCommands->Add("File.OpenSolution", "Open Project/Solution...", "", mpcContent->GetFolderIcon(), this, &CIDESolutions::QueueCommand);
	pCommands->Add("File.AddSolution", "Add Solution...", "", Ptr<CImageCel>(), this, &CIDESolutions::QueueCommand);
	pCommands->Add("File.CloseSolution", "Close Solution", "", Ptr<CImageCel>(), this, &CIDESolutions::QueueCommand);
	pCommands->Add("File.ClearRecentSolutions", "Clear Recent Solutions", "", Ptr<CImageCel>(), this, &CIDESolutions::QueueCommand);
	pCommands->Add("Explorer.SetStartupSolution", "Set as Startup Solution", "", Ptr<CImageCel>(), this, &CIDESolutions::QueueCommand);
	pCommands->Add("Explorer.SetStartupProject", "Set as Startup Project", "", Ptr<CImageCel>(), this, &CIDESolutions::QueueCommand)->SetEnabledCheck(this, &CIDESolutions::CanSetStartupProject);
	pCommands->Add("Explorer.RemoveSolution", "Remove Solution", "", Ptr<CImageCel>(), this, &CIDESolutions::QueueCommand);
	pCommands->Add("Explorer.ProjectProperties", "Properties...", "", Ptr<CImageCel>(), this, &CIDESolutions::QueueCommand)->SetEnabledCheck(this, &CIDESolutions::CanShowProjectProperties);
	pCommands->Add("View.SolutionExplorer", "Solution Explorer", "Alt+1", Ptr<CImageCel>(), this, &CIDESolutions::ShowActiveFileInExplorer);
	pCommands->Add("Explorer.AlwaysSelectOpenedFile", "Always Select Opened File", "", Ptr<CImageCel>(), this, &CIDESolutions::ToggleAlwaysSelect)->SetCheckedCheck(this, &CIDESolutions::IsAlwaysSelect);
	pCommands->Add("Explorer.OpenFilesWithSingleClick", "Open Files with Single Click", "", Ptr<CImageCel>(), this, &CIDESolutions::ToggleSingleClick)->SetCheckedCheck(this, &CIDESolutions::IsSingleClick);
}


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer's two ways, in its menu, as IntelliJ IDEA's Project
// tool window's: Always Select Opened File (the file of the editor shown
// is selected in the tree as tabs are chosen, without Alt+1) and Open
// Files with Single Click (one click on a file opens it; two, otherwise).
// Both off to begin with; kept in the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::SetExplorerOptions(bool bAlwaysSelect, bool bSingleClick)
{
	mbAlwaysSelect = bAlwaysSelect;
	mbSingleClick = bSingleClick;
	if (mpExplorer.IsNotNull())
	{
		mpExplorer->GetTree()->SetDoubleClickActivates(!mbSingleClick);
	}
	if (mbAlwaysSelect)
	{
		FileShown();
	}
}


void CIDESolutions::ToggleAlwaysSelect(void)
{
	SetExplorerOptions(!mbAlwaysSelect, mbSingleClick);
	mpcContent->GetSettings()->WriteSettings();
}


void CIDESolutions::ToggleSingleClick(void)
{
	SetExplorerOptions(mbAlwaysSelect, !mbSingleClick);
	mpcContent->GetSettings()->WriteSettings();
}


bool CIDESolutions::IsAlwaysSelect(void) { return mbAlwaysSelect; }
bool CIDESolutions::IsSingleClick(void) { return mbSingleClick; }


//////////////////////////////////////////////////////////////////////////
//
// An editor's tab was chosen (CIDEContent::DocumentSelected): with
// Always Select Opened File its file is selected in the tree at the next
// tick, once whatever chose the tab is done.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::FileShown(void)
{
	if (mbAlwaysSelect && !mbSelectPosted)
	{
		mbSelectPosted = true;
		mpcContent->GetWindow()->Post(this, &CIDESolutions::SelectActiveFile);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The active file's node is selected, its folders opened and its row in
// view; the tool window isn't brought forward and the keys stay where
// they are.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::SelectActiveFile(void)
{
	Ptr<CTreeContainer>	pTree;
	Ptr<CIDEEditorTab>	pTab;
	int32				iNode;

	mbSelectPosted = false;
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (!mbAlwaysSelect || pTab.IsNull() || mpExplorer.IsNull())
	{
		return;
	}
	pTree = mpExplorer->GetTree();
	iNode = mpExplorer->FindFilePathNode(pTab->GetFileName(), pTab->GetProject());
	if ((iNode != TREE_NO_NODE) && (iNode != pTree->GetSelectedNode()))
	{
		pTree->SelectNode(iNode);
		mpcContent->GetWindow()->Layout();
		pTree->ShowRow(pTree->FindNodeRow(iNode));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// <Explorer AlwaysSelect="true" SingleClick="true"/> in the settings,
// written only when one is on.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::ReadSettings(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;
	char*		szAlwaysSelect;
	char*		szSingleClick;

	pcTag = pcSettings->GetTag("Explorer");
	if (pcTag != NULL)
	{
		szAlwaysSelect = pcTag->GetStringAttribute("AlwaysSelect");
		szSingleClick = pcTag->GetStringAttribute("SingleClick");
		SetExplorerOptions((szAlwaysSelect != NULL) && (StringCompare(szAlwaysSelect, "true") == 0), (szSingleClick != NULL) && (StringCompare(szSingleClick, "true") == 0));
	}
}


void CIDESolutions::WriteSettings(CXMLWriter* pcWriter)
{
	if (mbAlwaysSelect || mbSingleClick)
	{
		pcWriter->Begin("Explorer");
		pcWriter->Attribute("AlwaysSelect", mbAlwaysSelect);
		pcWriter->Attribute("SingleClick", mbSingleClick);
		pcWriter->End();
	}
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


Ptr<CSolutionExplorer> CIDESolutions::GetExplorer(void) const { return mpExplorer; }
Ptr<CDockPane> CIDESolutions::GetExplorerTool(void) const { return mpExplorerTool; }
Ptr<CMenuContainer> CIDESolutions::GetExplorerMenu(void) const { return mpExplorerMenu; }


//////////////////////////////////////////////////////////////////////////
//
// File's Open Project/Solution, Add Solution, Close Solution, Recent
// Solutions (an item for each, or one saying there are none) and Clear
// Recent Solutions.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::AddFileItems(Ptr<CMenuContainer> pFile)
{
	Ptr<CCommandSet>	pCommands;

	pCommands = mpcContent->GetCommands();
	pFile->AddItem("New", NULL, mpcContent->GetNewFile()->GetFileMenu());
	pFile->AddItem(pCommands->Find("File.SaveAsTemplate"));
	pFile->AddSeparator();
	pFile->AddItem(pCommands->Find("File.OpenSolution"));
	pFile->AddItem(pCommands->Find("File.AddSolution"));
	pFile->AddItem(pCommands->Find("File.CloseSolution"));
	mpRecentMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	pFile->AddItem("Recent Solutions", NULL, mpRecentMenu);
	pFile->AddItem(pCommands->Find("File.ClearRecentSolutions"));
	ShowRecentMenu();
}


//////////////////////////////////////////////////////////////////////////
//
// A command's keys or menu item: what it does is done at the next tick,
// outside the input, as it can show a dialog.
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::QueueCommand(CCommand* pcCommand)
{
	mszQueuedCommand.Set(pcCommand->GetName());
	mpcContent->GetWindow()->Post(this, &CIDESolutions::RunQueuedCommand);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESolutions::RunQueuedCommand(void)
{
	SSolutionExplorerNode*	psNode;

	if (mszQueuedCommand.Equals("File.OpenSolution"))
	{
		ShowOpenSolutionDialog();
	}
	else if (mszQueuedCommand.Equals("File.AddSolution"))
	{
		ShowAddSolutionDialog();
	}
	else if (mszQueuedCommand.Equals("File.CloseSolution"))
	{
		CloseSelectedSolution();
	}
	else if (mszQueuedCommand.Equals("File.ClearRecentSolutions"))
	{
		ClearRecentSolutions();
	}
	else if (mszQueuedCommand.Equals("Explorer.SetStartupSolution"))
	{
		SetStartupSolution(mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode()));
	}
	else if (mszQueuedCommand.Equals("Explorer.SetStartupProject"))
	{
		psNode = mpExplorer->GetNode(mpExplorer->GetTree()->GetSelectedNode());
		if (psNode && (psNode->eType == SENT_Project))
		{
			SetStartupProject(psNode->uiSolution, psNode->uiProject);
		}
	}
	else if (mszQueuedCommand.Equals("Explorer.RemoveSolution"))
	{
		RemoveSolution(mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode()));
	}
	else if (mszQueuedCommand.Equals("Explorer.ProjectProperties"))
	{
		ShowProjectProperties();
	}
}
