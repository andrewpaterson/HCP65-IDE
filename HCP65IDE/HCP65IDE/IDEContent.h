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
#include "BuildLib/SolutionBuild.h"
#include "CppParserLib/CPP65ListingIndex.h"
#include "WindowLib/Button.h"
#include "WindowLib/TabbedContainer.h"
#include "WindowLib/MenuBar.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/FileChooser.h"
#include "GitLib/GitRepositories.h"
#include "GitLib/GitLineChanges.h"
#include "IDECodeModel.h"
#include "IDEEditorTab.h"
#include "IDEBuildPane.h"
#include "IDEDebugger.h"
#include "IDEDebugView.h"
#include "IDEIcons.h"
#include "SolutionExplorer.h"


//The IDE's window's content: the menu bar and toolbar along the top, Solution Explorer on the left, the editor's tabs
//of open files on the right with the references found under them.  F12 goes to the definition of what is at the caret and Shift+F12 finds its references.  It is an object; keep it
//in the root and call Setup.
//
//Files are shown in the colours of their git status, as IntelliJ IDEA shows them, in Solution Explorer and on their
//tabs, and the lines changed since the last commit are marked in the editor's gutter.  The statuses of the solution's
//repositories are read when it is opened and every few seconds after; the toolbar shows the branch.
//
//The Debug menu (and Visual Studio's keys) debug the HCP65 emulator at the instruction level, see CIDEDebugger: the
//Debug tab under the editor shows its disassembly and registers.
class CIDEContent : public CObject
{
CONSTRUCTABLE(CIDEContent);
DESTRUCTABLE(CIDEContent);
protected:
	Ptr<CWindow>				mpWindow;
	Ptr<CFont>					mpWhite;
	Ptr<CFontColours>			mpFontColours;
	Ptr<CWindowTheme>			mpTheme;
	Ptr<CFontFamily>			mpCodeFamily;
	Ptr<CFont>					mpDimFont;			//A dialog's dividers' titles.
	SDialogLook					msDialogLook;
	Ptr<CMenuBar>				mpMenuBar;
	Ptr<CMenuItem>				mpOpenItem;			//File, Open Project/Solution.
	Ptr<CFileChooser>			mpFileChooser;		//While it's shown.
	Ptr<CImageCel>				mpFolderIcon;		//The file chooser's.
	Ptr<CImageCel>				mpFileIcon;
	CChars						mszSolutionFile;	//The solution or project opened last.
	Ptr<CTabbedContainer>		mpEditors;			//A tab for each open file.
	CArray<CIDEEditorTab>		maTabs;
	Ptr<CIDEEditorTab>			mpActive;			//The selected tab's file, NULL if none is open.
	CArray<CImageCel>			maFileIcons;		//By EIDEFileType.
	Ptr<CSolutionExplorer>		mpExplorer;
	Ptr<CComboBox>				mpConfigurations;	//The build configuration's configuration; the code is parsed as its solution configuration.
	Ptr<CComboBox>				mpPlatforms;		//And its platform.
	CBuildConfigurations		mcConfigurations;	//The solution's, from beside it or the defaults.
	size						muiConfiguration;	//The chosen one, MAX_SIZE if none.
	Ptr<CButton>				mpBuildButton;
	CSolutionBuild				mcBuild;			//The build running, or the last.
	CChars						mszLibc65;			//The HCP65 compiler's system headers.
	CCPP65ListingIndex			mcListings;			//The last HCP65 build's listings, given to the open files.
	size						muiListingToShow;	//The line to show the code of after the next build, MAX_SIZE for none.
	Ptr<CTabbedContainer>		mpBottomTabs;		//Under the editor: Find All References and Build.
	Ptr<CTabComponent>			mpReferencesTab;
	Ptr<CTabComponent>			mpBuildTab;
	Ptr<CIDEBuildPane>			mpBuildPane;
	Ptr<CTreeContainer>			mpReferences;		//What Find All References found.
	CArrayIDELocations			masReferences;		//The references' places; a node's data is its index plus one.
	CVcxSolution				mcSolution;
	bool						mbSolution;
	CIDECodeModel				mcCodeModel;		//Uses mcSolution; initialised when a solution is opened.

	CGitRepositories			mcGit;				//The repositories the solution's files are in.
	CGitLineChanges				mcLineChanges;		//Worked out for one tab at a time.
	size						muiGitVersion;		//mcGit's status version when it was last shown.
	int64						miGitStatusTime;	//When the statuses were last read, 0 to read them now.
	Ptr<CLabel>					mpBranchLabel;		//On the toolbar.

	CIDEDebugger				mcDebugger;
	Ptr<CIDEDebugView>			mpDebugView;
	Ptr<CTabComponent>			mpDebugTab;
	Ptr<CButton>				mpStartButton;		//Start, or Continue while stopped.
	Ptr<CMenuItem>				mpStartItem;		//Debug's items.
	Ptr<CMenuItem>				mpStepIntoItem;
	Ptr<CMenuItem>				mpStepOverItem;
	Ptr<CMenuItem>				mpStepOutItem;
	Ptr<CMenuItem>				mpBreakItem;
	Ptr<CMenuItem>				mpStopItem;
	Ptr<CMenuItem>				mpRestartItem;
	Ptr<CMenuItem>				mpToggleItem;
	Ptr<CMenuItem>				mpNewBreakpointItem;
	Ptr<CMenuItem>				mpDeleteBreakpointsItem;
	Ptr<CMenuItem>				mpAttachItem;
	Ptr<CDialog>				mpDialog;			//The address or port asked for, while it's shown.
	CArrayChars					maszDebugScript;	//HCP65_IDE_DEBUG's commands still to do, for checking.
	CChars						mszDefaultEmulator;	//A W65C816 configuration's emulator when it doesn't say.
	CChars						mszDefaultEmulatorDirectory;
	int64						miDebugScriptTime;	//When the debugger was last seen not running, for a scripted break.
	int64						miDebugScriptWait;	//When a scripted wait started, 0 if none is.

public:
	void			Init(void);
	void			Class(void) override;
	void			Free(void) override;

	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	bool			Setup(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont);

	bool			OpenSolution(const char* szFileName);
	bool			ShowOpenSolutionDialog(void);
	void			MenuItemClicked(CMenuItem* pcItem);
	bool			OpenFile(const char* szFileName, size uiProject = SOLUTION_EXPLORER_NO_PROJECT);
	void			CloseAllFiles(void);
	void			ExplorerActivated(void);
	void			ReferenceActivated(void);
	bool			GoToDefinition(void);
	bool			FindReferences(void);
	bool			SetCaretPlace(int iLine, int iColumn);
	bool			SetConfiguration(const char* szConfiguration);
	void			ConfigurationChosen(void);
	void			SetComboBoxItems(Ptr<CComboBox> pComboBox, bool bPlatforms);
	void			ShowConfiguration(void);
	void			SelectComboBoxItem(Ptr<CComboBox> pComboBox, const char* szText);
	void			Tick(Ptr<CWindow> pWindow);
	void			TabSelected(CTabComponent* pcTab);
	void			TabRemoved(CTabComponent* pcTab);
	bool			CloseFile(const char* szFileName);
	CIDECodeModel*	GetCodeModel(void);
	CBuildConfiguration*	GetConfiguration(void);

	Ptr<CTextArea>		GetEditor(void);
	Ptr<CScrollPane>	GetEditorPane(void);
	Ptr<CIDEEditorTab>	GetActiveTab(void);
	Ptr<CTabbedContainer>	GetEditors(void);
	CSolutionExplorer*	GetExplorer(void);
	Ptr<CTreeContainer>	GetReferences(void);
	Ptr<CIDEBuildPane>	GetBuildPane(void);
	void				ShowBuildTab(void);
	void				SetLibc65Directory(const char* szDirectory);
	bool				Build(void);
	bool				IsBuilding(void);
	void				ShowListingAfterBuild(size uiLine);
	CCPP65ListingIndex*	GetListings(void);
	void				BuildClicked(void);
	void				StartClicked(void);

	void				SetDefaultEmulator(const char* szExecutable, const char* szDirectory);
	bool				StartDebugging(void);
	bool				StopDebugging(void);
	bool				RestartDebugging(void);
	bool				BreakAll(void);
	bool				StepInto(void);
	bool				StepOver(void);
	bool				StepOut(void);
	bool				ToggleBreakpoint(void);
	bool				ShowNewBreakpointDialog(void);
	bool				ShowAttachDialog(void);
	bool				AttachDebugger(int iPort);
	void				SetDebugScript(const char* szCommands);
	bool				IsDebugScriptRunning(void);
	CIDEDebugger*		GetDebugger(void);
	Ptr<CIDEDebugView>	GetDebugView(void);

	CGitRepositories*	GetGit(void);
	Ptr<CLabel>			GetBranchLabel(void);
	void				ReadGitStatus(void);

protected:
	Ptr<CMenuBar>		CreateMenuBar(void);
	Ptr<CMenuContainer>	CreateDebugMenu(void);
	bool				DebugMenuItemClicked(CMenuItem* pcItem);
	void				DebugCommandAction(EComponentInputCommand eCommand);
	void				PollDebugger(Ptr<CWindow> pWindow);
	void				UpdateDebugView(bool bKeepCaret);
	void				RunDebugScript(void);
	bool				ChooseEmulator(bool bLaunch);
	void				ShowDebugMessages(void);
	void				ResolveEmulatorPath(const char* szPath, CChars* pszDest);
	bool				ShowAddressDialog(const char* szTitle, const char* szLabel, const char* szDefault, bool bHex, uint32* puiValue);
	void				HighlightTab(Ptr<CIDEEditorTab> pTab);
	Ptr<CIDEEditorTab>	FindTab(const char* szFileName);
	Ptr<CIDEEditorTab>	FindTab(CUnknown* pcTab);
	void				CodeCommandAction(SComponentInputEvent* psEvent);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				OpenLocation(SIDELocation* psLocation);
	void				AddReference(int32 iParent, SIDELocation* psLocation, size uiIndex);
	void				ShowReferencesStatus(const char* szText);
	void				PollBuild(Ptr<CWindow> pWindow);
	void				ReadListings(CArrayChars* paszLines);
	void				SetListings(bool bListings);

	int64				PollGit(int64 iNow);
	void				ShowGitStatus(void);
	void				ShowBranch(void);
	void				ReadGitBase(Ptr<CIDEEditorTab> pTab);
	void				UpdateChangeBars(Ptr<CIDEEditorTab> pTab);
};


Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder);
ARGB32     GetHighlightColour(ECPPHighlight eKind);


#endif // __IDE_CONTENT_H__

