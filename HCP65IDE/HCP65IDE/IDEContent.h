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
#include "BaseLib/RecentFiles.h"
#include "BuildLib/SolutionBuild.h"
#include "CppCompilerLib/CPP65ListingIndex.h"
#include "WindowLib/Button.h"
#include "WindowLib/Toolbar.h"
#include "WindowLib/StatusBar.h"
#include "WindowLib/TabbedContainer.h"
#include "WindowLib/MenuBar.h"
#include "WindowLib/CommandSet.h"
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
#include "IDEFileTemplates.h"
#include "IDENewFile.h"
#include "IDEMoveNamespace.h"
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
#include "IDEStructure.h"
#include "IDERename.h"
#include "IDEFileRename.h"
#include "IDESafeDelete.h"
#include "IDEIntroduceVariable.h"
#include "IDEChangeSignature.h"
#include "IDEDefinitions.h"
#include "IDEMoveMembers.h"
#include "IDEMoveClass.h"
#include "IDEInlineVariable.h"
#include "IDEIntroduceTypedef.h"
#include "IDESolutionFileChooser.h"
#include "IDEExtractMethod.h"
#include "IDEInlineFunction.h"
#include "IDEExtractClass.h"
#include "IDEEncapsulateField.h"
#include "IDEIncludes.h"
#include "IDEConvertMacro.h"
#include "IDEConvertPointer.h"
#include "IDEConvertEnum.h"
#include "IDETemplateInterface.h"
#include "IDEMemberQualifier.h"
#include "IDEConvertMember.h"
#include "IDEContextActions.h"
#include "IDETemplateParameter.h"
#include "IDEUsages.h"
#include "IDEFindInFiles.h"
#include "IDEDuplicates.h"
#include "IDEOccurrences.h"
#include "IDENavigationHistory.h"
#include "IDECaretUsages.h"
#include "IDEPasteHistory.h"
#include "IDEUserFile.h"
#include "IDEBookmarks.h"
#include "IDEMarks.h"
#include "IDEToolWindows.h"
#include "IDERecent.h"
#include "IDEQuickInfo.h"
#include "IDECompletion.h"
#include "IDEHierarchy.h"
#include "IDEIndexing.h"
#include "IDEProblems.h"
#include "IDEDocuments.h"
#include "IDESolutions.h"
#include "IDEBuilding.h"
#include "IDEConfigurations.h"
#include "IDERunning.h"
#include "IDESettings.h"
#include "IDEDebugging.h"
#include "IDEGitStatus.h"
#include "IDEDiffs.h"
#include "IDEAskClaude.h"
#include "SolutionExplorer.h"


//The tool windows' IDs in the layout file, and the prefixes of the documents'.
#define IDE_EXPLORER_ID			"SolutionExplorer"
#define IDE_BUILD_ID			"Build"
#define IDE_RUN_ID				"Run"
#define IDE_DEBUG_ID			"Debug"
#define IDE_CLAUDE_ID			"Claude"
#define IDE_STRUCTURE_ID			"Structure"
#define IDE_BOOKMARKS_ID			"Bookmarks"
#define IDE_HIERARCHY_ID			"Hierarchy"
#define IDE_USAGES_ID			"FindUsages"
#define IDE_FIND_ID				"Find"
#define IDE_DUPLICATES_ID		"Duplicates"
#define IDE_INDEXING_ID			"Indexing"
#define IDE_PROBLEMS_ID			"Problems"


#define IDE_DOCUMENT_PREFIX		"Document:"
#define IDE_DIFF_PREFIX			"Diff:"
#define IDE_LAYOUT_EXTENSION	".layout.user"

//The Go to dialog's tabs.
#define IDE_GO_TO_CLASS			0
#define IDE_GO_TO_FILE			1
#define IDE_GO_TO_SYMBOL		2
#define IDE_GO_TO_ACTION		3


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
	Ptr<CCommandSet>			mpCommands;  //What the keys, menus and toolbar run, given to the window.
	Ptr<CMenuContainer>			mpViewMenu;			//An item for each tool window, made as it opens.
	Ptr<CDockManager>			mpDock;
	Ptr<CImageCel>				mpFolderIcon;		//The file chooser's.
	Ptr<CImageCel>				mpFileIcon;
	CArray<CImageCel>			maFileIcons;		//By EIDEFileType.
	CArray<CImageCel>			maSymbolIcons;		//By EIDESymbolType.
	CArray<CImageCel>			maProblemIcons;		//An error's, then a warning's.
	CArray<CImageCel>			maToolIcons;		//By EIDEToolIcon, then each again dim for a button that can't be used.

	Ptr<CFont>					mpMatchFont;		//What Go to's pattern matched.

	Ptr<CMenuContainer>			mpEditorMenu;		//The editors' context menu: Find Usages, Rename and Refactor.
	Ptr<CIDEScript>				mpScript;			//How the IDE is driven to check it.

	Ptr<CIDECloseGuard>			mpCloseGuard;		//Asks to save files with changes before they close.
	Ptr<CIDEEdit>				mpEdit;				//The Edit menu and undo across files.
	Ptr<CIDENavigator>			mpNavigator;		//Go to, Go to Source and Alt+O.
	Ptr<CIDEStructure>			mpStructure;		//File Structure, Next and Previous Method.
	Ptr<CIDERename>				mpRename;			//Rename.
	Ptr<CIDEFileRename>			mpFileRename;		//Rename File.
	Ptr<CIDESafeDelete>			mpSafeDelete;		//Safe Delete.
	Ptr<CIDEIntroduceVariable>	mpIntroduceVariable;	//Introduce Variable.
	Ptr<CIDEChangeSignature>	mpChangeSignature;		//Change Signature.
	Ptr<CIDEDefinitions>		mpDefinitions;			//Generate Definition and Declaration, Move Definition.
	Ptr<CIDEMoveMembers>		mpMoveMembers;			//Pull Members Up, Push Members Down.
	Ptr<CIDEMoveClass>			mpMoveClass;			//Move Class to Its Own Files.
	Ptr<CIDENewFile>			mpNewFile;				//New, from the Files templates (Alt+Insert).
	Ptr<CIDEMoveNamespace>		mpMoveNamespace;		//Move to Namespace.
	Ptr<CIDEInlineVariable>		mpInlineVariable;		//Inline Variable.
	Ptr<CIDEIntroduceTypedef>	mpIntroduceTypedef;		//Introduce Typedef.
	Ptr<CIDESolutionFileChooser>	mpSolutionFileChooser;	//The dialog a refactoring chooses one of the solutions' files from.
	Ptr<CIDEExtractMethod>		mpExtractMethod;		//Extract Method.
	Ptr<CIDEInlineFunction>		mpInlineFunction;		//Inline, of a function.
	Ptr<CIDEExtractClass>		mpExtractClass;			//Extract Superclass, Interface and Class.
	Ptr<CIDEEncapsulateField>	mpEncapsulateField;		//Encapsulate Field.
	Ptr<CIDEIncludes>			mpIncludes;				//Optimise #includes, Sort #includes.
	Ptr<CIDEConvertMacro>		mpConvertMacro;			//Convert Macro, Inline Macro.
	Ptr<CIDEConvertPointer>		mpConvertPointer;		//Convert Pointer to Reference, and back.
	Ptr<CIDEConvertEnum>		mpConvertEnum;			//Convert Enum to Enum Class.
	Ptr<CIDETemplateInterface>	mpTemplateInterface;	//Write Template's Interface.
	Ptr<CIDEMemberQualifier>	mpMemberQualifier;		//Make Member Function const, static.
	Ptr<CIDEConvertMember>		mpConvertMember;		//Convert Function to Member Function, and back.
	Ptr<CIDETemplateParameter>	mpTemplateParameter;	//Extract Template Parameter.
	Ptr<CIDEContextActions>		mpContextActions;		//Show Context Actions: the refactorings for what is at the caret.
	Ptr<CIDEUsages>				mpUsages;			//Find Usages.
	Ptr<CIDEFindInFiles>		mpFindInFiles;		//Find in Files and the Find tool window.
	Ptr<CIDEFind>				mpFind;				//Find and Replace in the active editor.
	Ptr<CIDEUserFile>			mpUserFile;			//The workspace's per-user file: breakpoints, bookmarks.
	Ptr<CIDEBookmarks>			mpBookmarks;		//Bookmarks and their tool window.
	Ptr<CIDEMarks>				mpMarks;			//Changed lines, kept usages and override marks.
	Ptr<CIDERecent>				mpRecent;			//Recent files and locations, the switcher, edit locations.
	Ptr<CIDEQuickInfo>			mpQuickInfo;		//A name's declaration, Quick Definition, Parameter Info, Type Info.
	Ptr<CIDECompletion>			mpCompletion;		//Code completion: the list of what can be written at the caret.
	Ptr<CIDEHierarchy>			mpHierarchy;		//The Hierarchy tool window: type, method and call hierarchies.
	Ptr<CIDEToolWindows>		mpToolWindows;		//The tool windows' keys, Hide All and Jump to Last.
	Ptr<CIDEDuplicates>			mpDuplicates;		//Find Duplicates and the Duplicates tool window.
	Ptr<CIDEOccurrences>		mpOccurrences;		//Next and Previous Occurrence through Find Usages or Build.
	Ptr<CIDENavigationHistory>	mpNavigationHistory;	//Back and Forward.
	Ptr<CIDECaretUsages>		mpCaretUsages;			//The usages of the symbol at the caret, highlighted.
	Ptr<CIDEPasteHistory>		mpPasteHistory;			//Paste from History: what the clipboard's history chooses with.
	Ptr<CIDEIndexing>			mpIndexing;			//The Indexing tab.
	Ptr<CIDEProblems>			mpProblems;			//The Problems tab.
	Ptr<CIDEDocuments>			mpDocuments;		//The open files' tabs.
	Ptr<CIDESolutions>			mpSolutions;		//Opening, adding and removing solutions, and the recent ones.
	Ptr<CIDEBuilding>			mpBuilding;			//Build, its menu and tab, and the listings.
	Ptr<CIDEConfigurations>		mpBuildConfigurations;	//The toolbar's run configuration, and the build configuration it builds.
	Ptr<CIDERunning>			mpRunning;			//Start, running the run configuration's tool, and the Run tab.
	Ptr<CIDESettings>			mpSettings;			//File, Settings, and the user's keys.
	Ptr<CIDEDebugging>			mpDebugging;		//The Debug menu, the debugger and breakpoints.
	Ptr<CIDEGitStatus>			mpGitStatus;		//Git statuses, the branch and change bars.
	Ptr<CIDEDiffs>				mpDiffs;			//Compare with HEAD, a branch, a revision or a file.
	Ptr<CIDEAskClaude>			mpAskClaude;		//The Claude tab's requests.
	CBuildWorkspace				mcWorkspace;		//The solutions open, one the startup solution.
	CIDECodeModel				mcCodeModel;		//Uses mcWorkspace; initialised when a solution is opened.
	CIDEFileTemplates			mcFileTemplates;	//The file and code templates new files and generated code are written from.

	Ptr<CStatusBar>				mpStatusBar;		//Along the bottom: what last went wrong, or was done, for a while,
	Ptr<CLabel>					mpProgressLabel;	//and on its right what is being done in the background (indexing).
	bool						mbKeepSettings;		//The layout and the open solution are saved for next time; not while being checked.




public:
	void			Init(void);
	void			Class(void) override;
	void			Free(void) override;

	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	bool			Setup(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont);

	void				SetKeepSettings(bool bKeep);
	bool				GoToDefinition(void);
	bool				FindReferences(void);
	void				ShowStatus(const char* szText);
	const char*			GetStatus(void);
	int64				UpdateStatusLine(int64 iNow);
	void				Tick(Ptr<CWindow> pWindow);
	bool				PaneClosing(CDockPane* pcPane) override;
	bool				IsPaneModified(CDockPane* pcPane) override;
	void				DocumentMenuCreated(Ptr<CMenuContainer> pMenu) override;
	void				PaneClosed(CDockPane* pcPane) override;
	void				DocumentSelected(CDockPane* pcPane) override;
	Ptr<CDockPane>		CreatePane(const char* szID) override;
	bool				SaveLayout(void);
	bool				LoadLayout(void);
	bool				GetLayoutFileName(CChars* pszDest);
	Ptr<CDockManager>	GetDock(void) const;
	Ptr<CDockPane>		GetExplorerTool(void);
	Ptr<CDockPane>		GetDebugTool(void);
	Ptr<CDockPane>		GetClaudeTool(void);
	CIDECodeModel*		GetCodeModel(void);
	CIDEFileTemplates*	GetFileTemplates(void);

	CSolutionExplorer*	GetExplorer(void);
	CBuildWorkspace*	GetWorkspace(void);
	bool				IsKeepingSettings(void) const;

	//What the classes the content holds share: the window, its look and the icons.
	Ptr<CWindow>		GetWindow(void) const;
	Ptr<CWindowTheme>	GetTheme(void) const;
	SDialogLook*		GetDialogLook(void);
	Ptr<CIDEPasteHistory>	GetPasteHistory(void) const;
	Ptr<CFontColours>	GetFontColours(void) const;
	Ptr<CFontColours>	GetBoldColours(void) const;
	Ptr<CFontFamily>	GetCodeFamily(void) const;
	Ptr<CFont>			GetDimFont(void) const;
	Ptr<CFont>			GetMatchFont(void) const;
	Ptr<CImageCel>		GetFolderIcon(void) const;
	Ptr<CImageCel>		GetFileIcon(void) const;
	size				NumFileTypeIcons(void);
	Ptr<CImageCel>		GetFileTypeIcon(size uiType);
	Ptr<CImageCel>		GetSymbolIcon(size uiType);
	Ptr<CImageCel>		GetProblemIcon(bool bWarning);
	Ptr<CImageCel>		GetToolIcon(size uiIcon, bool bDim = false);
	Ptr<CMenuBar>		GetMenuBar(void) const;
	Ptr<CCommandSet>	GetCommands(void) const;
	Ptr<CMenuContainer>	GetEditorMenu(void) const;

	Ptr<CMenuContainer>		CreateEditorMenu(void);
	Ptr<CIDEScript>			GetScript(void) const;
	Ptr<CIDEDocuments>		GetDocuments(void) const;
	Ptr<CIDESolutions>		GetSolutions(void) const;
	Ptr<CIDEBuilding>		GetBuilding(void) const;
	Ptr<CIDEOccurrences>	GetOccurrences(void) const;
	Ptr<CIDENavigationHistory>	GetNavigationHistory(void) const;
	Ptr<CIDEConfigurations>	GetBuildConfigurations(void) const;
	Ptr<CIDERunning>		GetRunning(void) const;
	Ptr<CIDESettings>		GetSettings(void) const;
	Ptr<CStatusBar>			GetStatusBar(void) const;
	Ptr<CIDEDebugging>		GetDebugging(void) const;
	Ptr<CIDEGitStatus>		GetGitStatus(void) const;
	Ptr<CIDEDiffs>			GetDiffs(void) const;
	Ptr<CIDEAskClaude>		GetAskClaude(void) const;
	Ptr<CIDENavigator>		GetNavigator(void) const;
	Ptr<CIDEStructure>		GetStructure(void) const;
	Ptr<CIDEEdit>			GetEdit(void) const;
	Ptr<CIDEUsages>			GetUsages(void) const;
	Ptr<CIDEFindInFiles>	GetFindInFiles(void) const;
	Ptr<CIDEFind>			GetFind(void) const;
	Ptr<CIDEUserFile>		GetUserFile(void) const;
	Ptr<CIDEBookmarks>		GetBookmarks(void) const;
	Ptr<CIDEMarks>			GetMarks(void) const;
	Ptr<CIDEToolWindows>	GetToolWindows(void) const;
	Ptr<CIDERecent>			GetRecent(void) const;
	Ptr<CIDEQuickInfo>		GetQuickInfo(void) const;
	Ptr<CIDECompletion>		GetCompletion(void) const;
	Ptr<CIDEHierarchy>		GetHierarchy(void) const;
	Ptr<CIDEDuplicates>		GetDuplicates(void) const;
	Ptr<CIDEIndexing>		GetIndexing(void) const;
	Ptr<CIDEProblems>		GetProblems(void) const;
	Ptr<CIDERename>			GetRename(void) const;
	Ptr<CIDEFileRename>		GetFileRename(void) const;
	Ptr<CIDESafeDelete>		GetSafeDelete(void) const;
	Ptr<CIDEIntroduceVariable>	GetIntroduceVariable(void) const;
	Ptr<CIDEChangeSignature>	GetChangeSignature(void) const;
	Ptr<CIDEDefinitions>		GetDefinitions(void) const;
	Ptr<CIDEMoveMembers>		GetMoveMembers(void) const;
	Ptr<CIDEMoveClass>			GetMoveClass(void) const;
	Ptr<CIDENewFile>			GetNewFile(void) const;
	Ptr<CIDEMoveNamespace>		GetMoveNamespace(void) const;
	Ptr<CIDEInlineVariable>		GetInlineVariable(void) const;
	Ptr<CIDEIntroduceTypedef>	GetIntroduceTypedef(void) const;
	Ptr<CIDESolutionFileChooser>	GetSolutionFileChooser(void) const;
	Ptr<CIDEExtractMethod>		GetExtractMethod(void) const;
	Ptr<CIDEInlineFunction>		GetInlineFunction(void) const;
	Ptr<CIDEExtractClass>		GetExtractClass(void) const;
	Ptr<CIDEEncapsulateField>	GetEncapsulateField(void) const;
	Ptr<CIDEIncludes>			GetIncludes(void) const;
	Ptr<CIDEConvertMacro>		GetConvertMacro(void) const;
	Ptr<CIDEConvertPointer>		GetConvertPointer(void) const;
	Ptr<CIDEConvertEnum>		GetConvertEnum(void) const;
	Ptr<CIDETemplateInterface>	GetTemplateInterface(void) const;
	Ptr<CIDEMemberQualifier>	GetMemberQualifier(void) const;
	Ptr<CIDEConvertMember>		GetConvertMember(void) const;
	Ptr<CIDETemplateParameter>	GetTemplateParameter(void) const;
	Ptr<CIDEContextActions>	GetContextActions(void) const;
	Ptr<CIDECloseGuard>		GetCloseGuard(void) const;

protected:
	Ptr<CMenuBar>		CreateMenuBar(void);
	void				AddCommands(void);
	void				Exit(void);
	void				ResetLayout(void);
	void				ViewMenuOpening(CMenuContainer* pcMenu);
	void				ViewItemClicked(CMenuItem* pcItem);
};


Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder, EFontTurn eTurn = FT_Upright, float32 fWeight = 0.0f);
ARGB32     GetHighlightColour(ECPPHighlight eKind);
bool       SameIDEText(const char* szText1, const char* szText2);


#endif // __IDE_CONTENT_H__

