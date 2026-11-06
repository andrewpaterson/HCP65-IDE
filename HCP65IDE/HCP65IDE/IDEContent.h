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
#ifndef __IDE_CONTENT_H__
#define __IDE_CONTENT_H__
#include "SupportLib/FontColours.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TreeContainer.h"
#include "WindowLib/ComboBox.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildLib/BuildConfigurations.h"
#include "BuildLib/BuildWorkspace.h"
#include "BuildLib/BuildRecentFiles.h"
#include "BuildLib/SolutionBuild.h"
#include "CppParserLib/CPP65ListingIndex.h"
#include "WindowLib/Button.h"
#include "WindowLib/TabbedContainer.h"
#include "WindowLib/MenuBar.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/FileChooser.h"
#include "WindowLib/DiffView.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/DockArea.h"
#include "GitLib/GitRepositories.h"
#include "GitLib/GitLineChanges.h"
#include "ClaudeLib/ClaudeCommand.h"
#include "BaseLib/UndoLog.h"
#include "IDECodeModel.h"
#include "IDEEditorTab.h"
#include "IDEDiffTab.h"
#include "IDEBuildPane.h"
#include "IDEClaudePane.h"
#include "IDEDebugger.h"
#include "IDEDebugView.h"
#include "IDEIcons.h"
#include "WindowLib/GoToDialog.h"
#include "IDEScript.h"
#include "IDECloseGuard.h"
#include "IDEEdit.h"
#include "IDENavigator.h"
#include "IDERename.h"
#include "IDEUsages.h"
#include "IDEDocuments.h"
#include "IDESolutions.h"
#include "IDEBuilding.h"
#include "IDEConfigurations.h"
#include "IDEDebugging.h"
#include "IDEGitStatus.h"
#include "IDEDiffs.h"
#include "IDEAskClaude.h"
#include "SolutionExplorer.h"


//The tool windows' IDs in the layout file, and the prefixes of the documents'.
#define IDE_EXPLORER_ID			"SolutionExplorer"
#define IDE_BUILD_ID			"Build"
#define IDE_DEBUG_ID			"Debug"
#define IDE_CLAUDE_ID			"Claude"
#define IDE_USAGES_ID			"FindUsages"


#define IDE_DOCUMENT_PREFIX		"Document:"
#define IDE_DIFF_PREFIX			"Diff:"
#define IDE_LAYOUT_EXTENSION	".layout.user"

//The Go to dialog's tabs.
#define IDE_GO_TO_CLASS			0
#define IDE_GO_TO_FILE			1
#define IDE_GO_TO_SYMBOL		2


//The IDE's window's content: the menu bar and toolbar along the top, and under them Visual Studio's docking (see
//CDockManager): the open files and diffs are documents in the document well, and Solution Explorer, Find Usages
//(with Find All References), Build, Debug and Claude are tool windows docked around it, which can be moved, resized, tabbed together,
//floated in windows of their own, auto hidden on the window's edges or closed (and shown again from the View menu).
//The layout is kept beside the solution in <solution>.layout.user, with the files that were open, and read back when
//the solution is opened; Window, Reset Window Layout puts the tool windows back where they start.
//
//F12 goes to the definition of what is at the caret and Shift+F12 finds its references.  It is an object; keep it
//in the root and call Setup.
//
//Files are shown in the colours of their git status, as IntelliJ IDEA shows them, in Solution Explorer and on their
//tabs, and the lines changed since the last commit are marked in the editor's gutter.  The statuses of the solution's
//repositories are read when it is opened, when a file is saved (Control+S, or Control+Shift+S for all) and every few
//seconds after, and whenever the window is activated (git may have been used outside); the toolbar shows the branch.
//Clicking a change bar in the gutter shows the change's old lines, with Rollback and Show Diff.  Git, Compare with HEAD (or a branch, or any revision) shows the selected
//file's changes side by side in a tab of its own, as IntelliJ IDEA's diff viewer does, as does File, Compare with File
//against another file.  A diff follows its files: edits in their editors, and new commits.
//
//The Debug menu (and Visual Studio's keys) debug the HCP65 emulator at the instruction level, see CIDEDebugger: the
//Debug tab under the editor shows its disassembly and registers.
//
//The Claude tab under the editor sends requests, such as refactorings, to Claude Code run headless in the solution's
//folder (see CClaudeCommand), saying where the caret and selection are.  Files are saved before a request, and the
//open files Claude changes are read again as it changes them.
class CIDEContent : public CObject, public CDockManagerListener
{
friend class CIDEScript;
friend class CIDECloseGuard;
friend class CIDEEdit;
friend class CIDENavigator;
friend class CIDERename;
friend class CIDEUsages;
friend class CIDEDocuments;
friend class CIDESolutions;
friend class CIDEBuilding;
friend class CIDEConfigurations;
friend class CIDEDebugging;
friend class CIDEGitStatus;
friend class CIDEDiffs;
friend class CIDEAskClaude;
CONSTRUCTABLE(CIDEContent);
DESTRUCTABLE(CIDEContent);
protected:
	Ptr<CWindow>				mpWindow;
	Ptr<CFont>					mpWhite;
	Ptr<CFont>					mpTurnedWhite;		//Turned clockwise, for the auto hide strips on the window's sides.
	Ptr<CFontColours>			mpTurnedColours;
	Ptr<CFontColours>			mpFontColours;
	Ptr<CFont>					mpBoldWhite;		//The interface's font, bold (the startup solution's name).
	Ptr<CFontColours>			mpBoldColours;
	Ptr<CWindowTheme>			mpTheme;
	Ptr<CFontFamily>			mpCodeFamily;
	Ptr<CFont>					mpDimFont;			//A dialog's dividers' titles.
	SDialogLook					msDialogLook;
	Ptr<CMenuBar>				mpMenuBar;
	Ptr<CMenuItem>				mpOpenItem;			//File, Open Project/Solution.
	Ptr<CMenuItem>				mpAddSolutionItem;	//File, Add Solution.
	Ptr<CMenuItem>				mpCloseSolutionItem;	//File, Close Solution.
	Ptr<CMenuContainer>			mpRecentMenu;		//File, Recent Solutions: an item for each, or one saying there are none.
	Ptr<CMenuItem>				mpClearRecentItem;	//File, Clear Recent Solutions.
	Ptr<CMenuItem>				mpSaveItem;			//File, Save.
	Ptr<CMenuItem>				mpSaveAllItem;		//File, Save All.
	Ptr<CMenuItem>				mpCompareItem;		//Git, Compare with HEAD.
	Ptr<CMenuItem>				mpCompareBranchItem;	//Git, Compare with Branch.
	Ptr<CMenuItem>				mpCompareRevisionItem;	//Git, Compare with Revision.
	Ptr<CMenuItem>				mpCompareFileItem;	//File, Compare with File.
	Ptr<CMenuItem>				mpExitItem;			//File, Exit.
	Ptr<CMenuItem>				mpExplorerItem;		//View's tool windows.
	Ptr<CMenuItem>				mpUsagesItem;
	Ptr<CMenuItem>				mpBuildItem;
	Ptr<CMenuItem>				mpDebugItem;
	Ptr<CMenuItem>				mpClaudeItem;
	Ptr<CMenuItem>				mpResetLayoutItem;	//Window, Reset Window Layout.
	Ptr<CMenuItem>				mpCloseAllItem;		//Window, Close All Documents.
	Ptr<CDockManager>			mpDock;
	Ptr<CDockPane>				mpExplorerTool;		//The tool windows.
	Ptr<CFileChooser>			mpFileChooser;		//While it's shown.
	Ptr<CImageCel>				mpFolderIcon;		//The file chooser's.
	Ptr<CImageCel>				mpFileIcon;
	CArray<CIDEDiffTab>			maDiffTabs;			//A tab for each diff shown.
	Ptr<CIDEDiffTab>			mpActiveDiff;		//The selected tab's diff, NULL if it isn't one.
	CArray<CImageCel>			maFileIcons;		//By EIDEFileType.
	CArray<CImageCel>			maSymbolIcons;		//By EIDESymbolType.

	Ptr<CFont>					mpMatchFont;		//What Go to's pattern matched.

	Ptr<CMenuContainer>			mpEditorMenu;		//The editors' context menu: Find Usages, Rename and Refactor.
	Ptr<CMenuItem>				mpFindUsagesItem;
	Ptr<CMenuItem>				mpGoToSourceItem;
	Ptr<CIDEScript>				mpScript;			//How the IDE is driven to check it.

	Ptr<CIDECloseGuard>			mpCloseGuard;		//Asks to save files with changes before they close.
	Ptr<CIDEEdit>				mpEdit;				//The Edit menu and undo across files.
	Ptr<CIDENavigator>			mpNavigator;		//Go to, Go to Source and Alt+O.
	Ptr<CIDERename>				mpRename;			//Rename.
	Ptr<CIDEUsages>				mpUsages;			//Find Usages.
	Ptr<CIDEDocuments>			mpDocuments;		//The open files' tabs.
	Ptr<CIDESolutions>			mpSolutions;		//Opening, adding and removing solutions, and the recent ones.
	Ptr<CIDEBuilding>			mpBuilding;			//Build, its tab and button, and the listings.
	Ptr<CIDEConfigurations>		mpBuildConfigurations;	//The toolbar's configuration and platform.
	Ptr<CIDEDebugging>			mpDebugging;		//The Debug menu, the debugger and breakpoints.
	Ptr<CIDEGitStatus>			mpGitStatus;		//Git statuses, the branch and change bars.
	Ptr<CIDEDiffs>				mpDiffs;			//Compare with HEAD, a branch, a revision or a file.
	Ptr<CIDEAskClaude>			mpAskClaude;		//The Claude tab's requests.
	Ptr<CMenuItem>				mpRenameItem;
	Ptr<CMenuItem>				mpExtractMethodItem;
	Ptr<CSolutionExplorer>		mpExplorer;
	Ptr<CMenuContainer>			mpExplorerMenu;		//A solution's: Set as Startup Solution and Remove.
	Ptr<CMenuItem>				mpStartupItem;
	Ptr<CMenuItem>				mpRemoveSolutionItem;
	CChars						mszConfiguration;	//The chosen build configuration's name, each solution's of that name; empty if none.
	CBuildWorkspace				mcWorkspace;		//The solutions open, one the startup solution.
	CIDECodeModel				mcCodeModel;		//Uses mcWorkspace; initialised when a solution is opened.

	Ptr<CLabel>					mpStatusLabel;		//The status line along the bottom: what last went wrong, or was done,
	int64						miStatusTime;		//when it said it (it's cleared a while after), 0 if it says nothing,
	Ptr<CLabel>					mpProgressLabel;	//and on its right what is being done in the background (indexing).
	bool						mbKeepSettings;		//The layout and the open solution are saved for next time; not while being checked.



	Ptr<CDialog>				mpDialog;			//The address or port asked for, while it's shown.

public:
	void			Init(void);
	void			Class(void) override;
	void			Free(void) override;

	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	bool			Setup(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont);

	void			SetKeepSettings(bool bKeep);
	void			MenuItemClicked(CMenuItem* pcItem);
	void			ExplorerActivated(void);
	bool			GoToDefinition(void);
	bool			FindReferences(void);
	void			ShowStatus(const char* szText);
	char*			GetStatus(void);
	int64			UpdateStatusLine(int64 iNow);
	void			Tick(Ptr<CWindow> pWindow);
	bool			PaneClosing(CDockPane* pcPane) override;
	bool			IsPaneModified(CDockPane* pcPane) override;
	void			PaneClosed(CDockPane* pcPane) override;
	void			DocumentSelected(CDockPane* pcPane) override;
	Ptr<CDockPane>	CreatePane(const char* szID) override;
	bool			SaveLayout(void);
	bool			LoadLayout(void);
	bool			GetLayoutFileName(CChars* pszDest);
	Ptr<CDockManager>	GetDock(void);
	Ptr<CDockPane>	GetExplorerTool(void);
	Ptr<CDockPane>	GetDebugTool(void);
	Ptr<CDockPane>	GetClaudeTool(void);
	CIDECodeModel*	GetCodeModel(void);

	CSolutionExplorer*	GetExplorer(void);




	bool				ShowActiveFileInExplorer(void);
	Ptr<CMenuContainer>	CreateEditorMenu(void);
	Ptr<CIDEScript>		GetScript(void);
	Ptr<CIDEDocuments>	GetDocuments(void);
	Ptr<CIDESolutions>	GetSolutions(void);
	Ptr<CIDEBuilding>	GetBuilding(void);
	Ptr<CIDEConfigurations>	GetBuildConfigurations(void);
	Ptr<CIDEDebugging>	GetDebugging(void);
	Ptr<CIDEGitStatus>	GetGitStatus(void);
	Ptr<CIDEDiffs>	GetDiffs(void);
	Ptr<CIDEAskClaude>	GetAskClaude(void);
	void				EditorMenuOpening(CMenuContainer* pcMenu);

protected:
	Ptr<CMenuBar>		CreateMenuBar(void);
	Ptr<CMenuContainer>	CreateExplorerMenu(void);
	void				CodeCommandAction(SComponentInputEvent* psEvent);




};


Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder, EFontTurn eTurn = FT_Upright, float32 fWeight = 0.0f);
ARGB32     GetHighlightColour(ECPPHighlight eKind);
bool       SameIDEText(const char* szText1, const char* szText2);


#endif // __IDE_CONTENT_H__

