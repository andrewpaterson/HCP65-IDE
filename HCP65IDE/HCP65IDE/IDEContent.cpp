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
#include "IDEContent.h"


#define IDE_FONT_CHARACTERS		" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
#define IDE_EXPLORER_WIDTH		320
#define IDE_CODE_PIXEL_SIZE		15.0f
#define IDE_BOTTOM_HEIGHT		180
#define IDE_TOOLBAR_HEIGHT		34		//A grid in a grid has no height of its own.
#define IDE_TOOLBAR_GAP			8		//Right of the branch.


#define IDE_BUILD_POLL			50		//Milliseconds between looks at a running build.



//Go to Definition (F12), Find All References (Shift+F12), Build (Control+Shift+B) and the debugger's keys from anywhere in the window.
static EComponentInputCommand	gaeIDECodeCommands[] = { CIC_Save, CIC_SaveAll, CIC_GoToDefinition, CIC_FindReferences, CIC_Build, CIC_StartDebugging, CIC_StopDebugging, CIC_RestartDebugging,
														 CIC_BreakAll, CIC_ToggleBreakpoint, CIC_StepOver, CIC_StepInto, CIC_StepOut };


//Visual Studio's dark theme colours for C++, by ECPPHighlight.
struct SIDEHighlightColour
{
	uint8	r;
	uint8	g;
	uint8	b;
};


SIDEHighlightColour gasHighlightColours[] =
{
	{ 0x57, 0xA6, 0x4A },	//CPPH_Comment
	{ 0x6B, 0x6B, 0x6B },	//CPPH_Inactive
	{ 0x9B, 0x9B, 0x9B },	//CPPH_Directive
	{ 0xBE, 0xB7, 0xFF },	//CPPH_Macro
	{ 0x56, 0x9C, 0xD6 },	//CPPH_Keyword
	{ 0xB4, 0xB4, 0xB4 },	//CPPH_Operator
	{ 0xB5, 0xCE, 0xA8 },	//CPPH_Number
	{ 0xD6, 0x9D, 0x85 },	//CPPH_Character
	{ 0xD6, 0x9D, 0x85 },	//CPPH_String
	{ 0xC8, 0xC8, 0xC8 },	//CPPH_Namespace
	{ 0x4E, 0xC9, 0xB0 },	//CPPH_Class
	{ 0xB8, 0xD7, 0xA3 },	//CPPH_Enum
	{ 0x4E, 0xC9, 0xB0 },	//CPPH_Typedef
	{ 0xB8, 0xD7, 0xA3 },	//CPPH_TypeParameter
	{ 0x56, 0x9C, 0xD6 },	//CPPH_BuiltinType
	{ 0xDC, 0xDC, 0xAA },	//CPPH_Function
	{ 0x9C, 0xDC, 0xFE },	//CPPH_Variable
	{ 0xDA, 0xDA, 0xDA },	//CPPH_Field
	{ 0x9A, 0x9A, 0x9A },	//CPPH_Parameter
	{ 0xB8, 0xD7, 0xA3 },	//CPPH_Enumerator
	{ 0xC8, 0xC8, 0xC8 },	//CPPH_Label
	{ 0xDA, 0xDA, 0xDA },	//CPPH_Member
	{ 0xDC, 0xDC, 0xDC },	//CPPH_Unresolved
	{ 0xDC, 0xDC, 0xDC },	//CPPH_Identifier
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
ARGB32 GetHighlightColour(ECPPHighlight eKind)
{
	SIDEHighlightColour*	psColour;

	if ((size)eKind >= sizeof(gasHighlightColours) / sizeof(SIDEHighlightColour))
	{
		eKind = CPPH_Identifier;
	}
	psColour = &gasHighlightColours[eKind];
	return Set32BitColour(psColour->r, psColour->g, psColour->b);
}


//////////////////////////////////////////////////////////////////////////
//
// A white font of szFontFilename's printable ASCII characters, upright or
// turned clockwise (for text read down a tool window's auto hide tab).
//
//////////////////////////////////////////////////////////////////////////
Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder, EFontTurn eTurn)
{
	CTrueTypeFont			cTrueTypeFont;
	CTrueTypeFontFactory	cFactory;
	CImageColourRGB			cColour;
	Ptr<CFontDescription>	pDescription;
	Ptr<CImage>				pImage;
	Ptr<CFont>				pFont;

	cTrueTypeFont.Init();
	if (!cTrueTypeFont.Load((char*)szFontFilename))
	{
		cTrueTypeFont.Kill();
		gcLogger.Error2(__METHOD__, " Could not load font [", szFontFilename, "].", NULL);
		return NULL;
	}

	cColour.Init(1.0f, 1.0f, 1.0f);
	cFactory.Init(&cTrueTypeFont, fPixelSize, eTurn);
	pDescription = cFactory.Describe((char*)szName, (char*)IDE_FONT_CHARACTERS);
	pImage = cFactory.Rasterise(pDescription, &cColour);
	if (pImage.IsNotNull())
	{
		if (pImage->GetColourOrder() != eOrder)
		{
			pImage = CImageColourConverter::ConvertColour(pImage, eOrder);
		}
		pFont = OMalloc<CFont>(pDescription, pImage);
	}
	cFactory.Kill();
	cTrueTypeFont.Kill();

	if (pFont.IsNull())
	{
		gcLogger.Error2(__METHOD__, " Could not make a font from [", szFontFilename, "].", NULL);
	}
	return pFont;
}


//////////////////////////////////////////////////////////////////////////
//
// szUIFont is the font of the menus, Solution Explorer and so on, and
// szCodeFont the fixed width font files are edited in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Init(void)
{
	PreInit();

	mbSolution = false;
	mcSolution.Init();
	mcCodeModel.Init(&mcSolution);
	mcConfigurations.Init();
	muiConfiguration = MAX_SIZE;
	memset(&msDialogLook, 0, sizeof(msDialogLook));
	mszSolutionFile.Init();
	mcBuild.Init();
	mszLibc65.Init();
	mcListings.Init();
	muiListingToShow = MAX_SIZE;
	masReferences.Init();
	maTabs.Init();
	maDiffTabs.Init();
	maFileIcons.Init();
	mcDebugger.Init();
	mszStoppedFile.Init();
	muiStoppedLine = 0;
	maszDebugScript.Init();
	miDebugScriptTime = 0;
	miDebugScriptWait = 0;
	mszDefaultEmulator.Init();
	mszDefaultEmulatorDirectory.Init();
	mcGit.Init();
	mcLineChanges.Init();
	muiGitVersion = 0;
	miGitStatusTime = 0;
	muiChange = 0;
	muiQueuedChange = MAX_SIZE;
	miQueuedAction = 0;
	memset(&msChange, 0, sizeof(SGitLineChange));
	mcClaude.Init();
	muiClaudeReloaded = 0;
	muiClaudeTools = 0;
	muiNextDiff = 0;

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Class(void)
{
	M_Pointer(mpWindow);
	M_Pointer(mpWhite);
	M_Pointer(mpTurnedWhite);
	M_Pointer(mpTurnedColours);
	M_Pointer(mpFontColours);
	M_Pointer(mpTheme);
	M_Pointer(mpCodeFamily);
	M_Pointer(mpDimFont);
	U_Pointer(msDialogLook.pcTheme);
	U_Pointer(msDialogLook.pcDimFont);
	U_Data(ARGB32, msDialogLook.sBackground);
	U_Data(ARGB32, msDialogLook.sDivider);
	M_Pointer(mpMenuBar);
	M_Pointer(mpOpenItem);
	M_Pointer(mpSaveItem);
	M_Pointer(mpSaveAllItem);
	M_Pointer(mpCompareItem);
	M_Pointer(mpCompareBranchItem);
	M_Pointer(mpCompareRevisionItem);
	M_Pointer(mpCompareFileItem);
	M_Pointer(mpExplorerItem);
	M_Pointer(mpReferencesItem);
	M_Pointer(mpBuildItem);
	M_Pointer(mpDebugItem);
	M_Pointer(mpClaudeItem);
	M_Pointer(mpResetLayoutItem);
	M_Pointer(mpCloseAllItem);
	M_Pointer(mpDock);
	M_Pointer(mpExplorerTool);
	M_Pointer(mpReferencesTool);
	M_Pointer(mpBuildTool);
	M_Pointer(mpDebugTool);
	M_Pointer(mpClaudeTool);
	U_Size(muiNextDiff);
	M_Pointer(mpFileChooser);
	M_Pointer(mpFolderIcon);
	M_Pointer(mpFileIcon);
	U_String(mszSolutionFile);
	U_String(mszStoppedFile);
	U_UInt32(muiStoppedLine);
	M_Embedded(maTabs);
	M_Pointer(mpActive);
	M_Embedded(maDiffTabs);
	M_Pointer(mpActiveDiff);
	M_Embedded(maFileIcons);
	M_Pointer(mpExplorer);
	M_Pointer(mpConfigurations);
	M_Pointer(mpPlatforms);
	U_Unknown(CBuildConfigurations, mcConfigurations);
	U_Size(muiConfiguration);
	M_Pointer(mpBuildButton);
	U_Unknown(CSolutionBuild, mcBuild);
	U_String(mszLibc65);
	U_Unknown(CCPP65ListingIndex, mcListings);
	U_Size(muiListingToShow);
	M_Pointer(mpBuildPane);
	M_Pointer(mpReferences);
	U_Unknown(CArrayIDELocations, masReferences);
	U_Unknown(CVcxSolution, mcSolution);
	U_Unknown(CIDECodeModel, mcCodeModel);
	U_Bool(mbSolution);
	U_Unknown(CIDEDebugger, mcDebugger);
	M_Pointer(mpDebugView);
	M_Pointer(mpStartButton);
	M_Pointer(mpStartItem);
	M_Pointer(mpStepIntoItem);
	M_Pointer(mpStepOverItem);
	M_Pointer(mpStepOutItem);
	M_Pointer(mpBreakItem);
	M_Pointer(mpStopItem);
	M_Pointer(mpRestartItem);
	M_Pointer(mpToggleItem);
	M_Pointer(mpNewBreakpointItem);
	M_Pointer(mpDeleteBreakpointsItem);
	M_Pointer(mpAttachItem);
	M_Pointer(mpDialog);
	U_Unknown(CArrayChars, maszDebugScript);
	U_String(mszDefaultEmulator);
	U_String(mszDefaultEmulatorDirectory);
	U_Int64(miDebugScriptTime);
	U_Int64(miDebugScriptWait);
	U_Unknown(CGitRepositories, mcGit);
	U_Unknown(CGitLineChanges, mcLineChanges);
	U_Size(muiGitVersion);
	U_Int64(miGitStatusTime);
	M_Pointer(mpBranchLabel);
	M_Pointer(mpChangePopup);
	M_Pointer(mpRollbackButton);
	M_Pointer(mpShowDiffButton);
	M_Pointer(mpChangeTab);
	U_Data(SGitLineChange, msChange);
	U_Size(muiChange);
	U_Size(muiQueuedChange);
	U_Int32(miQueuedAction);
	U_Unknown(CClaudeCommand, mcClaude);
	M_Pointer(mpClaudePane);
	U_Size(muiClaudeReloaded);
	U_Size(muiClaudeTools);
}


//////////////////////////////////////////////////////////////////////////
//
// The widgets, fonts and theme are freed with the content's pointers.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Free(void)
{
	//An object's memory is freed without its destructor running so the content and its listeners take themselves off
	//the widgets still here.
	if (mpExplorer.IsNotNull() && mpExplorer->GetTree().IsNotNull())
	{
		mpExplorer->GetTree()->GetActivationEvent()->Remove(this);
	}
	if (mpReferences.IsNotNull())
	{
		mpReferences->GetActivationEvent()->Remove(this);
	}
	if (mpConfigurations.IsNotNull())
	{
		mpConfigurations->GetSelectionEvent()->Remove(this);
	}
	if (mpPlatforms.IsNotNull())
	{
		mpPlatforms->GetSelectionEvent()->Remove(this);
	}
	if (mpBuildButton.IsNotNull())
	{
		mpBuildButton->GetClickEvent()->Remove(this);
	}
	if (mpStartButton.IsNotNull())
	{
		mpStartButton->GetClickEvent()->Remove(this);
	}
	if (mpMenuBar.IsNotNull())
	{
		mpMenuBar->GetItemClickEvent()->Remove(this);
	}
	if (mpDock.IsNotNull())
	{
		mpDock->SetListener(NULL);
	}
	if (mpWindow.IsNotNull())
	{
		mpWindow->GetActivationEvent()->Remove(this);
	}
	if (mpRollbackButton.IsNotNull())
	{
		mpRollbackButton->GetClickEvent()->Remove(this);
	}
	if (mpShowDiffButton.IsNotNull())
	{
		mpShowDiffButton->GetClickEvent()->Remove(this);
	}
	if (mpClaudePane.IsNotNull() && mpClaudePane->GetSendButton().IsNotNull())
	{
		mpClaudePane->GetSendButton()->GetClickEvent()->Remove(this);
		mpClaudePane->GetStopButton()->GetClickEvent()->Remove(this);
		mpClaudePane->GetNewButton()->GetClickEvent()->Remove(this);
	}

	//A request Claude is still working on is stopped.
	mcClaude.Kill();

	//An emulator being debugged is quit (or detached from).
	mszStoppedFile.Kill();
	mcDebugger.Kill();
	maszDebugScript.Kill();
	mszDefaultEmulatorDirectory.Kill();
	mszDefaultEmulator.Kill();

	//A git status still being read is stopped.
	mcLineChanges.Kill();
	mcGit.Kill();

	//A build still running is stopped (an HCP65 build is waited for) before the solution it builds goes.
	mcBuild.Kill();
	mcListings.Kill();
	mszLibc65.Kill();
	mszSolutionFile.Kill();
	mcCodeModel.Kill();
	mcConfigurations.Kill();
	mcSolution.Kill();
	KillIDELocations(&masReferences);
}


//////////////////////////////////////////////////////////////////////////
//
// The content isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The window's widgets.  Free still frees whatever was made when this
// fails.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::Setup(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont)
{
	Ptr<CGridContainer>	pMain;
	Ptr<CScrollPane>	pReferencesPane;
	SDockPlace			sPlace;
	Ptr<CGridContainer>	pToolbar;
	Ptr<CFillContainer>	pBranchGap;
	CColourFormatHelper	cFormat;
	size				i;

	mpWindow = pWindow;

	mpWhite = GenerateIDEFont(szUIFont, "Inter 13px", 13.0f, pWindow->GetColourOrder());
	if (mpWhite.IsNull())
	{
		return false;
	}
	mpFontColours = OMalloc<CFontColours>(mpWhite);
	mpTheme = CJewelTheme::CreateDarkTheme(pWindow, mpFontColours->GetFont(JEWEL_TEXT));
	pWindow->GetColourFormat(&cFormat);

	//The dialogs share the window's theme: they have the same colour format.
	mpDimFont = mpFontColours->GetFont(JEWEL_LINE_NUMBER);
	msDialogLook.pcTheme = &mpTheme;
	msDialogLook.pcDimFont = &mpDimFont;
	msDialogLook.sBackground = JEWEL_PANEL;
	msDialogLook.sDivider = JEWEL_SEPARATOR;
	mpFolderIcon = CJewelImageGenerator::CreateFolder(JEWEL_ICON, &cFormat);
	mpFileIcon = CJewelImageGenerator::CreateFile(JEWEL_ICON, &cFormat);

	mpCodeFamily = OMalloc<CFontFamily>("JetBrains Mono", IDE_FONT_CHARACTERS, IDE_CODE_PIXEL_SIZE, pWindow->GetColourOrder());
	if (!mpCodeFamily->SetFace(FS_Regular, szCodeFont))
	{
		return gcLogger.Error2(__METHOD__, " Could not load the code font [", szCodeFont, "].", NULL);
	}

	//The docking: the open files in the document well, the tool windows around it, see CDockManager.
	mpTurnedWhite = GenerateIDEFont(szUIFont, "Inter 13px Turned", 13.0f, pWindow->GetColourOrder(), FT_Clockwise);
	if (mpTurnedWhite.IsNotNull())
	{
		mpTurnedColours = OMalloc<CFontColours>(mpTurnedWhite);
		mpDock = OMalloc<CDockManager>(pWindow, mpTheme, mpTurnedColours->GetFont(JEWEL_TEXT));
	}
	else
	{
		mpDock = OMalloc<CDockManager>(pWindow, mpTheme, Ptr<CFont>());
	}
	mpDock->SetListener(this);
	for (i = 0; i < IFT_Count; i++)
	{
		maFileIcons.Add(CIDEIcons::CreateFileIcon((EIDEFileType)i, &cFormat));
	}

	//Solution Explorer on the left.
	mpExplorer = OMalloc<CSolutionExplorer>(pWindow, mpTheme);
	mpExplorer->GetTree()->GetActivationEvent()->Add(this, &CIDEContent::ExplorerActivated);
	sPlace.BesideWell(DS_Left, IDE_EXPLORER_WIDTH);
	mpExplorerTool = mpDock->AddPane(IDE_EXPLORER_ID, "Solution Explorer", NULL, mpExplorer->GetComponent(), DPK_Tool, &sPlace);

	//Under the well, tabbed together: what Find All References found, what the build wrote, the debugger's view and
	//Claude.
	mpReferences = OMalloc<CTreeContainer>(pWindow, mpTheme);
	mpReferences->SetAlignment(CSH_Stretch, CSV_Top);
	mpReferences->GetActivationEvent()->Add(this, &CIDEContent::ReferenceActivated);
	pReferencesPane = OMalloc<CScrollPane>(pWindow, mpTheme);
	pReferencesPane->SetContent(mpReferences);
	pReferencesPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	sPlace.BesideWell(DS_Bottom, IDE_BOTTOM_HEIGHT);
	mpReferencesTool = mpDock->AddPane(IDE_REFERENCES_ID, "Find All References", NULL, pReferencesPane, DPK_Tool, &sPlace);

	mpBuildPane = OMalloc<CIDEBuildPane>(pWindow, mpTheme, mpFontColours->GetFont(JEWEL_ICON), mpCodeFamily, JEWEL_TEXT);
	sPlace.WithPane(IDE_REFERENCES_ID);
	mpBuildTool = mpDock->AddPane(IDE_BUILD_ID, "Build", NULL, mpBuildPane->GetComponent(), DPK_Tool, &sPlace);

	mpDebugView = OMalloc<CIDEDebugView>(pWindow, mpTheme, mpCodeFamily, JEWEL_TEXT);
	mpDebugTool = mpDock->AddPane(IDE_DEBUG_ID, "Debug", NULL, mpDebugView->GetComponent(), DPK_Tool, &sPlace);

	mpClaudePane = OMalloc<CIDEClaudePane>(pWindow, mpTheme, mpFontColours->GetFont(JEWEL_ICON), mpCodeFamily);
	mpClaudePane->GetSendButton()->GetClickEvent()->Add(this, &CIDEContent::ClaudeSendClicked);
	mpClaudePane->GetStopButton()->GetClickEvent()->Add(this, &CIDEContent::ClaudeStopClicked);
	mpClaudePane->GetNewButton()->GetClickEvent()->Add(this, &CIDEContent::ClaudeNewClicked);
	mpClaudeTool = mpDock->AddPane(IDE_CLAUDE_ID, "Claude", NULL, mpClaudePane->GetComponent(), DPK_Tool, &sPlace);
	mpDock->Show(mpReferencesTool);

	//The toolbar: the build configuration's configuration and platform, as Visual Studio has them, and Build.
	mpConfigurations = OMalloc<CComboBox>(pWindow, mpTheme);
	mpConfigurations->SetAlignment(CSH_Left, CSV_Center);
	mpConfigurations->GetSelectionEvent()->Add(this, &CIDEContent::ConfigurationChosen);
	mpPlatforms = OMalloc<CComboBox>(pWindow, mpTheme);
	mpPlatforms->SetAlignment(CSH_Left, CSV_Center);
	mpPlatforms->GetSelectionEvent()->Add(this, &CIDEContent::ConfigurationChosen);
	mpBuildButton = OMalloc<CButton>(pWindow, mpTheme, "Build");
	mpBuildButton->SetAlignment(CSH_Left, CSV_Center);
	mpBuildButton->GetClickEvent()->Add(this, &CIDEContent::BuildClicked);
	mpStartButton = OMalloc<CButton>(pWindow, mpTheme, "Start");
	mpStartButton->SetAlignment(CSH_Left, CSV_Center);
	mpStartButton->GetClickEvent()->Add(this, &CIDEContent::StartClicked);
	pToolbar = OMalloc<CGridContainer>(pWindow);
	pToolbar->SetDesiredSize(-1, IDE_TOOLBAR_HEIGHT);
	pToolbar->SetAlignment(CSH_Stretch, CSV_Stretch);
	pToolbar->AddComponent(mpConfigurations, 0, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpPlatforms, 1, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpBuildButton, 2, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpStartButton, 3, 0, 1, 1, 1, 1);

	//The branch checked out, at the toolbar's right, as IntelliJ IDEA shows it.
	mpBranchLabel = OMalloc<CLabel>(pWindow, mpFontColours->GetFont(JEWEL_ICON), "");
	mpBranchLabel->SetAlignment(CSH_Right, CSV_Center);
	pToolbar->AddComponent(mpBranchLabel, 4, 0, 1, 1, 0, 1);
	pBranchGap = OMalloc<CFillContainer>(pWindow);
	pBranchGap->SetDesiredSize(IDE_TOOLBAR_GAP, 1);
	pToolbar->AddComponent(pBranchGap, 5, 0, 1, 1, 0, 1);

	pMain = OMalloc<CGridContainer>(pWindow);
	pMain->AddComponent(CreateMenuBar(), 0, 0, 1, 1, 1, 0);
	pMain->AddComponent(pToolbar, 0, 1, 1, 1, 1, 0);
	pMain->AddComponent(mpDock->GetArea(), 0, 2, 1, 1, 1, 1);
	pMain->AddInputActions(gaeIDECodeCommands, this, &CIDEContent::CodeCommandAction);
	pWindow->SetContainer(pMain);
	pWindow->GetActivationEvent()->Add(this, &CIDEContent::WindowActivated);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// File, with Open Project/Solution, Save, Save All and Compare with File;
// View, showing the tool windows; Git, with Compare with HEAD, a branch
// or a revision; Debug; and Window, with Reset Window Layout and Close
// All Documents, as in Visual Studio.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuBar> CIDEContent::CreateMenuBar(void)
{
	Ptr<CMenuContainer>	pFile;
	Ptr<CMenuContainer>	pGit;
	Ptr<CMenuContainer>	pView;
	Ptr<CMenuContainer>	pWindowMenu;

	pFile = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpOpenItem = pFile->AddItem("Open Project/Solution...", mpFolderIcon);
	mpSaveItem = pFile->AddItem("Save");
	mpSaveAllItem = pFile->AddItem("Save All");
	mpCompareFileItem = pFile->AddItem("Compare with File...");

	mpMenuBar = OMalloc<CMenuBar>(mpWindow, mpTheme);
	pGit = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpCompareItem = pGit->AddItem("Compare with HEAD");
	mpCompareBranchItem = pGit->AddItem("Compare with Branch...");
	mpCompareRevisionItem = pGit->AddItem("Compare with Revision...");

	pView = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpExplorerItem = pView->AddItem("Solution Explorer");
	mpReferencesItem = pView->AddItem("Find All References");
	mpBuildItem = pView->AddItem("Build Output");
	mpDebugItem = pView->AddItem("Debug");
	mpClaudeItem = pView->AddItem("Claude");

	pWindowMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpResetLayoutItem = pWindowMenu->AddItem("Reset Window Layout");
	mpCloseAllItem = pWindowMenu->AddItem("Close All Documents");

	mpMenuBar->AddMenu("File", pFile);
	mpMenuBar->AddMenu("View", pView);
	mpMenuBar->AddMenu("Git", pGit);
	mpMenuBar->AddMenu("Debug", CreateDebugMenu());
	mpMenuBar->AddMenu("Window", pWindowMenu);
	mpMenuBar->GetItemClickEvent()->Add(this, &CIDEContent::MenuItemClicked);
	mpMenuBar->SetAlignment(CSH_Stretch, CSV_Stretch);
	return mpMenuBar;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::MenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem == &mpOpenItem)
	{
		ShowOpenSolutionDialog();
	}
	else if (pcItem == &mpSaveItem)
	{
		SaveActiveFile();
	}
	else if (pcItem == &mpSaveAllItem)
	{
		SaveAllFiles();
	}
	else if (pcItem == &mpCompareItem)
	{
		CompareWithHead();
	}
	else if (pcItem == &mpCompareBranchItem)
	{
		ShowCompareWithBranchDialog();
	}
	else if (pcItem == &mpCompareRevisionItem)
	{
		ShowCompareWithRevisionDialog();
	}
	else if (pcItem == &mpCompareFileItem)
	{
		ShowCompareWithFileDialog();
	}
	else if (pcItem == &mpExplorerItem)
	{
		mpDock->Activate(mpExplorerTool);
	}
	else if (pcItem == &mpReferencesItem)
	{
		mpDock->Activate(mpReferencesTool);
	}
	else if (pcItem == &mpBuildItem)
	{
		mpDock->Activate(mpBuildTool);
	}
	else if (pcItem == &mpDebugItem)
	{
		mpDock->Activate(mpDebugTool);
	}
	else if (pcItem == &mpClaudeItem)
	{
		mpDock->Activate(mpClaudeTool);
	}
	else if (pcItem == &mpResetLayoutItem)
	{
		mpDock->ResetLayout();
	}
	else if (pcItem == &mpCloseAllItem)
	{
		CloseAllFiles();
	}
	else
	{
		DebugMenuItemClicked(pcItem);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for a solution (.sln) or project (.vcxproj) and opens it, starting
// on the one open.  False if none is chosen or it can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowOpenSolutionDialog(void)
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
	mpFileChooser = OMalloc<CFileChooser>(mpWindow, &msDialogLook, "Open Project/Solution", false, mszSolutionFile.Text(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpFolderIcon, mpFileIcon);
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
// Reads a .sln (and its projects) and shows it in Solution Explorer.  A
// .vcxproj on its own is put in a solution of its own, as Visual Studio
// does.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenSolution(const char* szFileName)
{
	CChars	szName;
	bool	bSolution;

	szName.Init(szFileName);
	bSolution = szName.EndsWithIgnoreCase(".sln");
	szName.Kill();

	//The layout of the solution being let go is kept beside it, with its open files.
	SaveLayout();

	//A build of the solution being let go is stopped, and its listings forgotten.  Its files are closed and the
	//references found in it go.
	mcBuild.Kill();
	mcBuild.Init();
	SetListings(false);
	CloseAllFiles();
	KillIDELocations(&masReferences);
	masReferences.Init();
	mpReferences->RemoveAllNodes();
	mszSolutionFile.Set(szFileName);

	mcCodeModel.Kill();
	mcSolution.Kill();
	mcSolution.Init();

	//The repositories are found again from the new solution's files, and read at the next tick.
	mcGit.RemoveAll();
	miGitStatusTime = 0;
	if (bSolution)
	{
		mbSolution = mcSolution.Read(szFileName);
	}
	else
	{
		mbSolution = mcSolution.ReadProject(szFileName);
	}
	mcCodeModel.Init(&mcSolution);
	mcCodeModel.AddDefaultSystemDirectories();
	if (!mbSolution)
	{
		return false;
	}
	mpExplorer->ShowSolution(&mcSolution);

	//Colouring the files finds the repositories they are in; their statuses are read before the solution is shown.
	mcGit.FindRepository(szFileName);
	mpExplorer->ColourFiles(&mcGit, mpFontColours);
	ReadGitStatus();
	miGitStatusTime = GetDebugTickCount();

	//The build configurations beside the solution, or the defaults if it hasn't any.
	mcConfigurations.Kill();
	mcConfigurations.Init();
	muiConfiguration = MAX_SIZE;
	szName.Init();
	CBuildConfigurations::GetFileName(szFileName, &szName);
	if (!mcConfigurations.Read(szName.Text()))
	{
		mcConfigurations.AddDefaults(&mcSolution);
	}
	szName.Kill();

	//The breakpoints saved beside it, bound once its configuration's line tables are read.
	szName.Init();
	GetUserSettingsFileName(&szName);
	mcDebugger.ReadSourceBreakpoints(szName.Text());
	szName.Kill();

	SetComboBoxItems(mpConfigurations, false);
	SetComboBoxItems(mpPlatforms, true);
	if (mcConfigurations.NumConfigurations() > 0)
	{
		//Chosen before the combo boxes tell of it, so it isn't indexed twice.
		muiConfiguration = 0;
		mcCodeModel.SetConfiguration(mcConfigurations.GetConfiguration(0)->GetSolutionConfiguration());
		ShowConfiguration();
	}
	mcCodeModel.StartIndexing();

	//The windows where they were left, and the files that were open opened again.
	LoadLayout();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Shows szFileName as a document of its own, active: the one it is
// already in if it is open.  A file of the solution's project uiProject
// is highlighted as that project builds it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenFile(const char* szFileName, size uiProject)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = OpenFileTab(szFileName, uiProject, true);
	if (pTab.IsNull())
	{
		return false;
	}
	mpDock->Activate(pTab->GetDockPane());
	mpActive = pTab;
	mpActiveDiff = NULL;
	ShowBranch();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The editor of szFileName, read into a document pane if it isn't open.
// Without bShow the pane isn't put anywhere, for a layout being read to
// place.  NULL if the file can't be read.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::OpenFileTab(const char* szFileName, size uiProject, bool bShow)
{
	CTextFile			cFile;
	CFileUtil			cFileUtil;
	CChars				szName;
	CChars				szID;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CDockPane>		pPane;

	pTab = FindTab(szFileName);
	if (pTab.IsNotNull())
	{
		return pTab;
	}

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		gcLogger.Error2(__METHOD__, " Could not read [", szFileName, "].", NULL);
		return NULL;
	}
	pTab = OMalloc<CIDEEditorTab>(mpWindow, mpTheme, mpFontColours->GetFont(JEWEL_ICON), mpCodeFamily, JEWEL_TEXT, szFileName, uiProject, cFile.Text());
	cFile.Kill();
	maTabs.Add(pTab);
	HighlightTab(pTab);
	if (mcListings.NumListings() > 0)
	{
		pTab->SetListings(&mcListings);
	}

	szName.Init();
	cFileUtil.GetFileName(&szName, (char*)szFileName);
	szID.Init(IDE_DOCUMENT_PREFIX);
	szID.Append(szFileName);
	pPane = mpDock->AddPane(szID.Text(), szName.Text(), maFileIcons.Get(CIDEIcons::GetFileType(szFileName)), pTab->GetPane(), DPK_Document, NULL, bShow);
	szID.Kill();
	szName.Kill();
	pTab->SetDockPane(pPane);
	pTab->GetCodeEditor()->GetChangeBarClickEvent()->Add(this, &CIDEContent::ChangeBarClicked);
	pTab->GetCodeEditor()->GetGlyphClickEvent()->Add(this, &CIDEContent::GlyphClicked);
	ShowBreakpoints(pTab);
	ReadGitBase(pTab);
	UpdateChangeBars(pTab);
	pPane->SetTabFont(GetGitStateFont(mpFontColours, pTab->GetGitState()));
	return pTab;
}


//////////////////////////////////////////////////////////////////////////
//
// Closes szFileName's document.  False if it isn't open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::CloseFile(const char* szFileName)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = FindTab(szFileName);
	if (pTab.IsNull())
	{
		return false;
	}
	return mpDock->Close(pTab->GetDockPane());
}


//////////////////////////////////////////////////////////////////////////
//
// Every open file's and diff's document is closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::CloseAllFiles(void)
{
	while (maTabs.Size() > 0)
	{
		//Closing the document lets its file go (PaneClosed).
		if (!mpDock->Close(maTabs.Get(maTabs.Size() - 1)->GetDockPane()))
		{
			break;
		}
	}
	while (maDiffTabs.Size() > 0)
	{
		if (!mpDock->Close(maDiffTabs.Get(maDiffTabs.Size() - 1)->GetDockPane()))
		{
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Writes the tab's text to its file if it has changed since it was opened
// or saved, then reads its repository's status again.  False if it
// couldn't be written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SaveFile(Ptr<CIDEEditorTab> pTab)
{
	if (!pTab->IsModified())
	{
		return true;
	}
	if (!pTab->SaveFile())
	{
		return gcLogger.Error2(__METHOD__, " Could not save [", pTab->GetFileName(), "].", NULL);
	}
	GitFileSaved(pTab->GetFileName());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Saves the selected tab's file.  True if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SaveActiveFile(void)
{
	if (mpActive.IsNull())
	{
		return true;
	}
	return SaveFile(mpActive);
}


//////////////////////////////////////////////////////////////////////////
//
// Saves every open file that has changed.  False if any couldn't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SaveAllFiles(void)
{
	size	i;
	bool	bResult;

	bResult = true;
	for (i = 0; i < maTabs.Size(); i++)
	{
		if (!SaveFile(maTabs.Get(i)))
		{
			bResult = false;
		}
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Colours a tab's text by what the parser found each part of it to be:
// as its file is, or as it has been edited to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::HighlightTab(Ptr<CIDEEditorTab> pTab)
{
	CArrayHighlights	acHighlights;
	CChars				szErrors;

	//Only C and C++ are parsed; a text file or a project file is shown as it is.
	if ((pTab->GetProject() == SOLUTION_EXPLORER_NO_PROJECT) || (CIDEIcons::GetFileType(pTab->GetFileName()) == IFT_Other))
	{
		return;
	}
	acHighlights.Init();
	szErrors.Init();
	if (mcCodeModel.Highlight(pTab->GetProject(), pTab->GetFileName(), pTab->IsEdited() ? pTab->GetEditor()->GetText() : NULL, &acHighlights, &szErrors))
	{
		pTab->SetHighlights(&acHighlights);
	}
	szErrors.Kill();
	acHighlights.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file opened in Solution Explorer is shown in the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ExplorerActivated(void)
{
	CChars	szPath;

	szPath.Init();
	if (mpExplorer->GetSelectedFilePath(&szPath))
	{
		OpenFile(szPath.Text(), mpExplorer->GetNodeProject(mpExplorer->GetTree()->GetSelectedNode()));
	}
	szPath.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::CodeCommandAction(SComponentInputEvent* psEvent)
{
	if (psEvent->eCommand == CIC_Save)
	{
		SaveActiveFile();
	}
	else if (psEvent->eCommand == CIC_SaveAll)
	{
		SaveAllFiles();
	}
	else if (psEvent->eCommand == CIC_GoToDefinition)
	{
		GoToDefinition();
	}
	else if (psEvent->eCommand == CIC_FindReferences)
	{
		FindReferences();
	}
	else if (psEvent->eCommand == CIC_Build)
	{
		Build();
	}
	else
	{
		DebugCommandAction(psEvent->eCommand);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Opens where what is named at the caret is defined (or declared, if no
// file indexed so far defines it) with the caret on its name.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GoToDefinition(void)
{
	int				iLine;
	int				iColumn;
	SIDELocation	sLocation;
	bool			bResult;

	if (!GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	if (!mcCodeModel.FindDefinition(mpActive->GetFileName(), iLine, iColumn, &sLocation))
	{
		ShowReferencesStatus("No definition found at the caret.");
		return false;
	}
	bResult = OpenLocation(&sLocation);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Lists every place what is named at the caret is declared, defined and
// used: "File.cpp(12): the line", under the symbol's name.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::FindReferences(void)
{
	int		iLine;
	int		iColumn;
	CChars	szKey;
	CChars	szTitle;
	int32	iRoot;
	size	i;
	size	uiIndexed;
	size	uiToIndex;

	if (!GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}

	KillIDELocations(&masReferences);
	masReferences.Init();
	szKey.Init();
	if (!mcCodeModel.FindReferences(mpActive->GetFileName(), iLine, iColumn, &masReferences, &szKey))
	{
		szKey.Kill();
		ShowReferencesStatus("No references found at the caret.");
		return false;
	}

	mcCodeModel.GetIndexingProgress(&uiIndexed, &uiToIndex);
	szTitle.Init(szKey);
	szTitle.Append(" - ");
	szTitle.Append((uint32)masReferences.NumElements());
	szTitle.Append((masReferences.NumElements() == 1) ? " reference" : " references");
	if (uiIndexed < uiToIndex)
	{
		szTitle.Append(" (indexing, ");
		szTitle.Append((uint32)uiIndexed);
		szTitle.Append(" of ");
		szTitle.Append((uint32)uiToIndex);
		szTitle.Append(" files)");
	}
	mpReferences->RemoveAllNodes();
	iRoot = mpReferences->AddNode(TREE_NO_NODE, szTitle.Text(), NULL, 0);
	for (i = 0; i < masReferences.NumElements(); i++)
	{
		AddReference(iRoot, masReferences.Get(i), i);
	}
	mpReferences->SetExpanded(iRoot, true);
	mpDock->Show(mpReferencesTool);
	szTitle.Kill();
	szKey.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// File.cpp(12): the line, trimmed.  Declarations and definitions say so
// and a call to one of several overloads says it might be this one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::AddReference(int32 iParent, SIDELocation* psLocation, size uiIndex)
{
	CFileUtil	cFileUtil;
	CTextFile	cFile;
	CChars		szText;
	CChars		szLine;
	CChars		szName;
	char*		szPosition;
	char*		szEnd;
	int			iLine;

	szName.Init();
	cFileUtil.GetFileName(&szName, psLocation->szFileName.Text());
	szText.Init(szName);
	szText.Append('(');
	szText.Append(psLocation->iLine + 1);
	szText.Append("): ");

	cFile.Init();
	if (cFile.Read(psLocation->szFileName.Text()))
	{
		szPosition = cFile.Text();
		for (iLine = 0; (iLine < psLocation->iLine) && szPosition; iLine++)
		{
			szPosition = strchr(szPosition, '\n');
			szPosition = szPosition ? szPosition + 1 : NULL;
		}
		if (szPosition)
		{
			szEnd = strchr(szPosition, '\n');
			szLine.Init();
			szLine.AppendSubString(szPosition, szEnd ? szEnd : szPosition + strlen(szPosition));
			szLine.StripWhitespace(true);
			szLine.Replace('\t', ' ');
			szText.Append(szLine);
			szLine.Kill();
		}
	}
	cFile.Kill();

	if (psLocation->uiRole == CPPIR_Definition)
	{
		szText.Append("  [definition]");
	}
	else if (psLocation->uiRole == CPPIR_Declaration)
	{
		szText.Append("  [declaration]");
	}
	if (psLocation->uiFlags & CPPIF_AMBIGUOUS)
	{
		szText.Append("  [one of the overloads]");
	}
	mpReferences->AddNode(iParent, szText.Text(), NULL, (int64)uiIndex + 1);
	szText.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowReferencesStatus(const char* szText)
{
	mpReferences->RemoveAllNodes();
	mpReferences->AddNode(TREE_NO_NODE, szText, NULL, 0);
	mpDock->Show(mpReferencesTool);
}


//////////////////////////////////////////////////////////////////////////
//
// The Build tab is shown under the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowBuildTab(void)
{
	mpDock->Show(mpBuildTool);
}


//////////////////////////////////////////////////////////////////////////
//
// Where the HCP65 compiler's system headers (<stdint.h>) are: Libc65.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetLibc65Directory(const char* szDirectory)
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
bool CIDEContent::Build(void)
{
	CArrayChars	aszLines;
	bool		bResult;

	if (!mbSolution || (GetConfiguration() == NULL) || mcBuild.IsRunning())
	{
		return false;
	}

	mpBuildPane->Clear();
	ShowBuildTab();
	aszLines.Init();
	bResult = mcBuild.Start(&mcSolution, GetConfiguration(), NULL, mszLibc65.Text(), &aszLines);
	mpBuildPane->AppendLines(&aszLines);
	mpBuildPane->SetProgress(mcBuild.GetProgress());
	aszLines.Kill();
	if (mcBuild.IsRunning())
	{
		mpWindow->RequestTick(IDE_BUILD_POLL);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A running build's new output and progress go in the Build tab.  When an
// HCP65 build succeeds its listings are read.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::PollBuild(Ptr<CWindow> pWindow)
{
	CArrayChars	aszLines;
	bool		bRunning;

	if (!mcBuild.IsRunning())
	{
		return;
	}

	aszLines.Init();
	bRunning = mcBuild.Poll(&aszLines);
	if (!bRunning && (mcBuild.GetState() == BS_Succeeded) && GetConfiguration()->IsHCP65())
	{
		ReadListings(&aszLines);

		//The program's new line table binds the breakpoints in its source.
		ReadLineTables();
	}
	mpBuildPane->AppendLines(&aszLines);
	mpBuildPane->SetProgress(mcBuild.GetProgress());
	aszLines.Kill();
	if (bRunning)
	{
		pWindow->RequestTick(IDE_BUILD_POLL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The last build's listings, in place of those read before, given to the
// open files.  A line asked for (ShowListingAfterBuild) shows its code.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReadListings(CArrayChars* paszLines)
{
	CFileUtil	cFileUtil;
	CChars		szDirectory;
	CChars		szLine;
	CArrayChars	aszFiles;
	size		i;

	SetListings(false);
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
	szLine.Init("Read ");
	szLine.Append((uint32)mcListings.NumListings());
	szLine.Append((mcListings.NumListings() == 1) ? " listing from " : " listings from ");
	szLine.Append(szDirectory);
	szLine.Append('.');
	paszLines->Add(szLine.Text());
	szLine.Kill();
	aszFiles.Kill();
	szDirectory.Kill();

	SetListings(true);
	if ((muiListingToShow != MAX_SIZE) && mpActive.IsNotNull())
	{
		mpActive->ToggleListing(muiListingToShow);
		muiListingToShow = MAX_SIZE;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The open files are given the listings (bListings), or none and the
// listings are forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetListings(bool bListings)
{
	size	i;

	if (!bListings)
	{
		mcListings.Kill();
		mcListings.Init();
	}
	for (i = 0; i < maTabs.Size(); i++)
	{
		maTabs.Get(i)->SetListings(bListings ? &mcListings : NULL);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// When the next HCP65 build has succeeded, line uiLine (from 0) of the
// selected file shows its code, as clicking its mark does.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowListingAfterBuild(size uiLine)
{
	muiListingToShow = uiLine;
}


//////////////////////////////////////////////////////////////////////////
//
// A reference opened in the list is shown in the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReferenceActivated(void)
{
	int64	iData;

	if (mpReferences->GetSelectedNode() == TREE_NO_NODE)
	{
		return;
	}
	iData = mpReferences->GetNodeData(mpReferences->GetSelectedNode());
	if ((iData > 0) && ((size)iData <= masReferences.NumElements()))
	{
		OpenLocation(masReferences.Get((size)iData - 1));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The file the location is in, in its tab, with the caret on it.  A
// file that isn't one of the solution's projects' items is highlighted as
// part of the project of the file it was found from.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenLocation(SIDELocation* psLocation)
{
	size	uiProject;

	uiProject = mcCodeModel.FindProjectOfFile(psLocation->szFileName.Text());
	if ((uiProject == MAX_SIZE) && mpActive.IsNotNull())
	{
		uiProject = mpActive->GetProject();
	}
	if (!OpenFile(psLocation->szFileName.Text(), uiProject))
	{
		return false;
	}
	if (!SetCaretPlace(psLocation->iLine, psLocation->iColumn))
	{
		return false;
	}
	mpActive->GetEditor()->GetWindow()->GetFocus()->SetFocus(mpActive->GetEditor());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The caret's line and column (0 based, the column in bytes) in the
// selected tab.  False if no file is open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GetCaretPlace(int* piLine, int* piColumn)
{
	if (mpActive.IsNull())
	{
		return false;
	}
	return mpActive->GetCaretPlace(piLine, piColumn);
}


//////////////////////////////////////////////////////////////////////////
//
// Puts the selected tab's caret at a line and column (0 based, the
// column in bytes) and scrolls to show it.  False if no file is open or
// it hasn't that line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SetCaretPlace(int iLine, int iColumn)
{
	if (mpActive.IsNull())
	{
		return false;
	}
	return mpActive->SetCaretPlace(iLine, iColumn);
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration and platform chosen in the toolbar.  If there's no
// build configuration with both, the one changed is kept: the first with
// it is chosen.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ConfigurationChosen(void)
{
	char*					szConfiguration;
	char*					szPlatform;
	CBuildConfiguration*	pcCurrent;
	CBuildConfiguration*	pcChosen;

	szConfiguration = mpConfigurations->GetSelectedText();
	szPlatform = mpPlatforms->GetSelectedText();
	pcCurrent = GetConfiguration();
	if ((szConfiguration == NULL) || (szPlatform == NULL) || ((pcCurrent != NULL) && pcCurrent->Is(szConfiguration, szPlatform)))
	{
		return;
	}

	pcChosen = mcConfigurations.FindConfiguration(szConfiguration, szPlatform);
	if (pcChosen == NULL)
	{
		if ((pcCurrent != NULL) && pcCurrent->Is(NULL, szPlatform))
		{
			pcChosen = mcConfigurations.FindConfiguration(szConfiguration, NULL);
		}
		else
		{
			pcChosen = mcConfigurations.FindConfiguration(NULL, szPlatform);
		}
	}
	if (pcChosen != NULL)
	{
		SetConfiguration(pcChosen->GetName());
	}
	else
	{
		ShowConfiguration();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The configurations' configuration names, or platforms, in pComboBox.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetComboBoxItems(Ptr<CComboBox> pComboBox, bool bPlatforms)
{
	CArrayChars		aszNames;
	size			i;

	aszNames.Init();
	if (bPlatforms)
	{
		mcConfigurations.GetPlatformNames(&aszNames);
	}
	else
	{
		mcConfigurations.GetConfigurationNames(&aszNames);
	}
	pComboBox->RemoveAllItems();
	for (i = 0; i < aszNames.NumElements(); i++)
	{
		pComboBox->AddItem(aszNames.GetText(i));
	}
	aszNames.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen build configuration's configuration and platform selected in
// the toolbar.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowConfiguration(void)
{
	CChars	szName;

	if (GetConfiguration() == NULL)
	{
		return;
	}

	szName.Init();
	GetConfiguration()->GetConfigurationName(&szName);
	SelectComboBoxItem(mpConfigurations, szName.Text());
	szName.Clear();
	GetConfiguration()->GetPlatformName(&szName);
	SelectComboBoxItem(mpPlatforms, szName.Text());
	szName.Kill();

	//Its programs' line tables bind the source breakpoints.
	ReadLineTables();
}


//////////////////////////////////////////////////////////////////////////
//
// pComboBox's item szText selected (selecting doesn't tell the listeners).
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SelectComboBoxItem(Ptr<CComboBox> pComboBox, const char* szText)
{
	size	i;

	for (i = 0; i < pComboBox->NumItems(); i++)
	{
		if (StringCompare(pComboBox->GetItemText(i), szText) == 0)
		{
			pComboBox->Select((int32)i);
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The build configuration szConfiguration is chosen.  The code is parsed
// as its solution configuration builds it: if that changed the solution is
// indexed again and the open files are highlighted again.  False if there
// is no such build configuration.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SetConfiguration(const char* szConfiguration)
{
	size					i;
	CBuildConfiguration*	pcConfiguration;

	for (i = 0; i < mcConfigurations.NumConfigurations(); i++)
	{
		if (StringCompare(mcConfigurations.GetConfiguration(i)->GetName(), szConfiguration) == 0)
		{
			break;
		}
	}
	if (i == mcConfigurations.NumConfigurations())
	{
		return false;
	}
	muiConfiguration = i;
	ShowConfiguration();

	pcConfiguration = mcConfigurations.GetConfiguration(i);
	if (StringCompare(pcConfiguration->GetSolutionConfiguration(), mcCodeModel.GetConfiguration()) != 0)
	{
		mcCodeModel.SetConfiguration(pcConfiguration->GetSolutionConfiguration());
		mcCodeModel.StartIndexing();
		for (i = 0; i < maTabs.Size(); i++)
		{
			HighlightTab(maTabs.Get(i));
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen build configuration, NULL if none is.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CIDEContent::GetConfiguration(void)
{
	return (muiConfiguration < mcConfigurations.NumConfigurations()) ? mcConfigurations.GetConfiguration(muiConfiguration) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: a tab whose text has been left alone for a moment
// after an edit is highlighted again, from its text.  The window ticks
// again when the next one might be due.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Tick(Ptr<CWindow> pWindow)
{
	size				i;
	Ptr<CIDEEditorTab>	pTab;
	int64				iNow;
	int64				iWait;
	int64				iSoonest;

	PollBuild(pWindow);
	PollDebugger(pWindow);
	PollClaude(pWindow);
	if (mpClaudePane.IsNotNull())
	{
		mpClaudePane->ScrollToEnd();
	}

	//The platform's clock: the window's total time only counts the time it spent updating.
	iNow = GetDebugTickCount();
	iSoonest = PollGit(iNow);
	if ((muiQueuedChange != MAX_SIZE) && mpActive.IsNotNull() && (mpActive->GetEditor()->GetActualSize().x > 0))
	{
		if (ShowChange(mpActive, muiQueuedChange))
		{
			if (miQueuedAction == 1)
			{
				RollbackChange();
			}
			else if (miQueuedAction == 2)
			{
				ShowChangeDiff();
			}
		}
		muiQueuedChange = MAX_SIZE;
	}
	iWait = PollDiffs(iNow);
	if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
	{
		iSoonest = iWait;
	}
	for (i = 0; i < maTabs.Size(); i++)
	{
		pTab = maTabs.Get(i);
		if (pTab->NeedsChangeBars())
		{
			UpdateChangeBars(pTab);
		}
		pTab->ShowModified();
		if (pTab->NeedsRecolour(iNow, &iWait))
		{
			HighlightTab(pTab);
		}
		else if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
		{
			iSoonest = iWait;
		}
	}
	if (iSoonest >= 0)
	{
		pWindow->RequestTick((uint32)iSoonest);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The document shown in front changed: an editor's or a diff's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::DocumentSelected(CDockPane* pcPane)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEDiffTab>	pDiff;

	if (pcPane == NULL)
	{
		return;
	}
	pTab = FindTab(pcPane);
	pDiff = FindDiffTab(pcPane);
	if (pTab.IsNull() && pDiff.IsNull())
	{
		return;
	}
	mpActive = pTab;
	mpActiveDiff = pDiff;
	ShowBranch();
}


//////////////////////////////////////////////////////////////////////////
//
// Documents close without asking: their files are saved by the user.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::PaneClosing(CDockPane* pcPane)
{
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A document was closed: its file, or its diff, is let go.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::PaneClosed(CDockPane* pcPane)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEDiffTab>	pDiff;

	pDiff = FindDiffTab(pcPane);
	if (pDiff.IsNotNull())
	{
		if (mpActiveDiff == pDiff)
		{
			mpActiveDiff = NULL;
		}
		maDiffTabs.Remove(pDiff);
		return;
	}

	pTab = FindTab(pcPane);
	if (pTab.IsNull())
	{
		return;
	}

	if (mpActive == pTab)
	{
		mpActive = NULL;
	}
	if (mpChangeTab == pTab)
	{
		CloseChangePopup();
	}
	pTab->GetCodeEditor()->GetChangeBarClickEvent()->Remove(this);
	pTab->GetCodeEditor()->GetGlyphClickEvent()->Remove(this);
	maTabs.Remove(pTab);
}


//////////////////////////////////////////////////////////////////////////
//
// A layout read names a document that isn't open: its file is opened
// again, if it is still there.  Diffs aren't.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CDockPane> CIDEContent::CreatePane(const char* szID)
{
	CFileUtil			cFileUtil;
	const char*			szFileName;
	Ptr<CIDEEditorTab>	pTab;

	if (strncmp(szID, IDE_DOCUMENT_PREFIX, strlen(IDE_DOCUMENT_PREFIX)) != 0)
	{
		return NULL;
	}
	szFileName = &szID[strlen(IDE_DOCUMENT_PREFIX)];
	if (!cFileUtil.Exists(szFileName))
	{
		return NULL;
	}
	pTab = OpenFileTab(szFileName, mpExplorer->FindFileProject(szFileName), false);
	if (pTab.IsNull())
	{
		return NULL;
	}
	return pTab->GetDockPane();
}


//////////////////////////////////////////////////////////////////////////
//
// <solution>.layout.user beside the solution or project.  False if no
// solution is open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GetLayoutFileName(CChars* pszDest)
{
	CFileUtil	cFileUtil;

	if (mszSolutionFile.Empty())
	{
		return false;
	}
	pszDest->Init(mszSolutionFile);
	cFileUtil.RemoveExtension(pszDest);
	pszDest->Append(IDE_LAYOUT_EXTENSION);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Writes the windows' layout, with the files open, beside the solution.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SaveLayout(void)
{
	CChars	szFileName;
	bool	bResult;

	if (!mbSolution || !GetLayoutFileName(&szFileName))
	{
		return false;
	}
	bResult = mpDock->SaveLayout(szFileName.Text());
	szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the windows' layout kept beside the solution, opening the files
// it names.  False if there isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::LoadLayout(void)
{
	CChars	szFileName;
	bool	bResult;

	if (!GetLayoutFileName(&szFileName))
	{
		return false;
	}
	bResult = mpDock->LoadLayout(szFileName.Text());
	szFileName.Kill();
	if (bResult)
	{
		DocumentSelected(&mpDock->GetActiveDocument());
		if (mpActive.IsNull() && mpActiveDiff.IsNull() && (maTabs.Size() > 0))
		{
			mpActive = maTabs.Get(maTabs.Size() - 1);
		}
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab szFileName is open in, NULL if it isn't open.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::FindTab(const char* szFileName)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (StringCompare(maTabs.Get(i)->GetFileName(), szFileName) == 0)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The file shown in a document pane, NULL if it isn't one of them.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::FindTab(CDockPane* pcPane)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetDockPane() == pcPane)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


Ptr<CTextArea> CIDEContent::GetEditor(void) { return mpActive.IsNotNull() ? mpActive->GetEditor() : Ptr<CTextArea>(); }
Ptr<CScrollPane> CIDEContent::GetEditorPane(void) { return mpActive.IsNotNull() ? mpActive->GetPane() : Ptr<CScrollPane>(); }
Ptr<CIDEEditorTab> CIDEContent::GetActiveTab(void) { return mpActive; }
Ptr<CDockManager> CIDEContent::GetDock(void) { return mpDock; }
Ptr<CDockPane> CIDEContent::GetExplorerTool(void) { return mpExplorerTool; }
Ptr<CDockPane> CIDEContent::GetReferencesTool(void) { return mpReferencesTool; }
Ptr<CDockPane> CIDEContent::GetBuildTool(void) { return mpBuildTool; }
Ptr<CDockPane> CIDEContent::GetDebugTool(void) { return mpDebugTool; }
Ptr<CDockPane> CIDEContent::GetClaudeTool(void) { return mpClaudeTool; }
Ptr<CTreeContainer> CIDEContent::GetReferences(void) { return mpReferences; }
Ptr<CIDEBuildPane> CIDEContent::GetBuildPane(void) { return mpBuildPane; }
bool CIDEContent::IsBuilding(void) { return mcBuild.IsRunning(); }
CCPP65ListingIndex* CIDEContent::GetListings(void) { return &mcListings; }  //The last HCP65 build's, for showing a line's assembly.
CIDECodeModel* CIDEContent::GetCodeModel(void) { return &mcCodeModel; }
CSolutionExplorer* CIDEContent::GetExplorer(void) { return &mpExplorer; }

