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
	Ptr<CDockPane>				mpBuildTool;
	Ptr<CDockPane>				mpDebugTool;
	Ptr<CDockPane>				mpClaudeTool;
	Ptr<CFileChooser>			mpFileChooser;		//While it's shown.
	Ptr<CImageCel>				mpFolderIcon;		//The file chooser's.
	Ptr<CImageCel>				mpFileIcon;
	CArray<CIDEEditorTab>		maTabs;
	Ptr<CIDEEditorTab>			mpActive;			//The selected tab's file, NULL if none is open.
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
	Ptr<CComboBox>				mpConfigurations;	//The build configuration's configuration; the code is parsed as its solution configuration.
	Ptr<CComboBox>				mpPlatforms;		//And its platform.
	CChars						mszConfiguration;	//The chosen build configuration's name, each solution's of that name; empty if none.
	Ptr<CButton>				mpBuildButton;
	CSolutionBuild				mcBuild;			//The build running, or the last.
	CArrayInt					maiBuildOrder;		//The solutions a build builds, in order.
	size						muiBuildStep;		//The one being built.
	CChars						mszLibc65;			//The HCP65 compiler's system headers.
	CCPP65ListingIndex			mcListings;			//The last HCP65 build's listings, given to the open files.
	size						muiListingToShow;	//The line to show the code of after the next build, MAX_SIZE for none.
	Ptr<CIDEBuildPane>			mpBuildPane;
	CBuildWorkspace				mcWorkspace;		//The solutions open, one the startup solution.
	CBuildRecentFiles			mcRecent;			//The solutions and workspaces opened last.
	CChars						mszRecentFile;		//Where they are kept, empty if they aren't.
	CChars						mszQueuedOpen;		//A recent one chosen, opened at the next tick (OpenQueuedSolution).
	CIDECodeModel				mcCodeModel;		//Uses mcWorkspace; initialised when a solution is opened.

	CGitRepositories			mcGit;				//The repositories the solution's files are in.
	int64						miGitStatusTime;	//When the statuses were last read, 0 to read them now.
	Ptr<CLabel>					mpBranchLabel;		//On the toolbar.
	Ptr<CLabel>					mpStatusLabel;		//The status line along the bottom: what last went wrong, or was done.
	Ptr<CIDEEditorTab>			mpChangeTab;		//The tab of the change shown.
	size						muiQueuedChange;	//A change's line to show once the editor is laid out, MAX_SIZE for none.
	int							miQueuedAction;		//Then 0 nothing, 1 Rollback or 2 Show Diff.
	bool						mbKeepSettings;		//The layout and the open solution are saved for next time; not while being checked.

	Ptr<CIDEClaudePane>			mpClaudePane;


	Ptr<CIDEDebugView>			mpDebugView;
	Ptr<CButton>				mpStartButton;		//Start, or Continue while stopped.
	Ptr<CDialog>				mpDialog;			//The address or port asked for, while it's shown.

public:
	void			Init(void);
	void			Class(void) override;
	void			Free(void) override;

	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	bool			Setup(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont);

	bool			OpenSolution(const char* szFileName);
	bool			OpenSolutions(CArrayChars* paszFileNames);
	bool			AddSolution(const char* szFileName);
	bool			RemoveSolution(size uiSolution);
	bool			CloseSolution(size uiSolution);
	bool			CloseSelectedSolution(void);
	bool			OpenLastSolution(void);
	void			RememberOpenSolution(void);
	void			SetKeepSettings(bool bKeep);
	bool			IsSolutionFile(size uiSolution, size uiProject);
	bool			SetStartupSolution(size uiSolution);
	bool			ShowOpenSolutionDialog(void);
	bool			ShowAddSolutionDialog(void);
	CBuildWorkspace*	GetWorkspace(void);
	void			SetRecentFile(const char* szFile);
	void			ClearRecentSolutions(void);
	bool			OpenRecentSolution(size uiIndex);
	CBuildRecentFiles*	GetRecent(void);
	void			MenuItemClicked(CMenuItem* pcItem);
	bool			OpenFile(const char* szFileName, size uiProject = SOLUTION_EXPLORER_NO_PROJECT);
	Ptr<CIDEEditorTab>	OpenFileTab(const char* szFileName, size uiProject, bool bShow);
	void			CloseAllFiles(void);
	bool			SaveFile(Ptr<CIDEEditorTab> pTab);
	bool			SaveActiveFile(void);
	bool			SaveAllFiles(void);
	void			ExplorerActivated(void);
	bool			GoToDefinition(void);
	bool			FindReferences(void);
	void			ShowStatus(const char* szText);
	char*			GetStatus(void);
	bool			SetCaretPlace(int iLine, int iColumn);
	bool			SetConfiguration(const char* szConfiguration);
	void			ConfigurationChosen(void);
	void			SetComboBoxItems(Ptr<CComboBox> pComboBox, bool bPlatforms);
	void			ShowConfiguration(void);
	void			SelectComboBoxItem(Ptr<CComboBox> pComboBox, const char* szText);
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
	Ptr<CDockPane>	GetBuildTool(void);
	Ptr<CDockPane>	GetDebugTool(void);
	Ptr<CDockPane>	GetClaudeTool(void);
	bool			CloseFile(const char* szFileName);
	CIDECodeModel*	GetCodeModel(void);
	CBuildConfiguration*	GetConfiguration(void);
	char*			GetConfigurationName(void);

	Ptr<CTextArea>		GetEditor(void);
	Ptr<CScrollPane>	GetEditorPane(void);
	Ptr<CIDEEditorTab>	GetActiveTab(void);
	CSolutionExplorer*	GetExplorer(void);
	Ptr<CIDEBuildPane>	GetBuildPane(void);
	void				ShowBuildTab(void);
	void				SetLibc65Directory(const char* szDirectory);
	bool				Build(void);
	bool				IsBuilding(void);
	void				ShowListingAfterBuild(size uiLine);
	CCPP65ListingIndex*	GetListings(void);
	void				BuildClicked(void);




	void				OpenQueuedSolution(void);
	bool				ShowActiveFileInExplorer(void);
	Ptr<CMenuContainer>	CreateEditorMenu(void);
	bool				FindSymbolAtCaret(CArrayIDELocations* pasDest, CChars* pszKey);
	void				GetFileLine(const char* szFileName, int iLine, CChars* pszDest);
	Ptr<CIDEScript>		GetScript(void);
	Ptr<CIDEDebugging>	GetDebugging(void);
	Ptr<CIDEGitStatus>	GetGitStatus(void);
	Ptr<CIDEDiffs>	GetDiffs(void);
	Ptr<CIDEAskClaude>	GetAskClaude(void);
	void				EditorMenuOpening(CMenuContainer* pcMenu);
	size				NumTabs(void);
	Ptr<CIDEEditorTab>	GetTab(size uiIndex);

protected:
	Ptr<CMenuBar>		CreateMenuBar(void);
	Ptr<CMenuContainer>	CreateExplorerMenu(void);
	void				ShowRecentMenu(void);
	void				WriteRecent(void);
	bool				RecentItemClicked(CMenuItem* pcItem);
	bool				IsSolutionOpen(void);
	char*				GetStartupFile(void);
	bool				GetSettingsBaseFile(CChars* pszDest);
	void				LetGoOfWorkspace(void);
	void				WorkspaceChanged(bool bReindex);
	bool				WriteWorkspace(void);
	void				FindRepositories(void);
	void				ShowConfigurations(void);
	CBuildConfiguration*	GetChosenConfiguration(void);
	bool				StartBuildStep(CArrayChars* paszLines);
	void				ShowBuildProgress(void);
	void				FinishBuildSteps(CArrayChars* paszLines);
	void				HighlightTab(Ptr<CIDEEditorTab> pTab);
	Ptr<CIDEEditorTab>	FindTab(const char* szFileName);
	Ptr<CIDEEditorTab>	FindTab(CDockPane* pcPane);
	void				CodeCommandAction(SComponentInputEvent* psEvent);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				OpenLocation(SIDELocation* psLocation);
	void				PollBuild(Ptr<CWindow> pWindow);
	void				ReadListings(CArrayChars* paszLines);
	void				SetListings(bool bListings);

	Ptr<CIDEEditorTab>	FindTab(CCodeEditor* pcEditor);



	Ptr<CIDEEditorTab>	FindTab(CTextArea* pcEditor);
};


Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder, EFontTurn eTurn = FT_Upright, float32 fWeight = 0.0f);
ARGB32     GetHighlightColour(ECPPHighlight eKind);
bool       SameIDEText(const char* szText1, const char* szText2);


#endif // __IDE_CONTENT_H__

