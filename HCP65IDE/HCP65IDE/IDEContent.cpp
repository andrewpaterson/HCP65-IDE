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
#define IDE_BOLD_WEIGHT			700.0f	//Inter's bold.
#define IDE_BOTTOM_HEIGHT		180
#define IDE_TOOLBAR_HEIGHT		34		//A grid in a grid has no height of its own.
#define IDE_TOOLBAR_GAP			8		//Right of the branch, and left of the status.
#define IDE_STATUS_HEIGHT		24		//The status line along the bottom.
#define IDE_STATUS_SHOWN		10000	//How long (ms) a status message stays.
#define IDE_PROGRESS_POLL		250		//How often (ms) the indexing's progress is shown while it lasts.


#define IDE_BUILD_POLL			50		//Milliseconds between looks at a running build.



//Go to Definition (F12), Find All References (Shift+F12), Build (Control+Shift+B) and the debugger's keys from anywhere in the window.
static EComponentInputCommand	gaeIDECodeCommands[] = { CIC_Save, CIC_SaveAll, CIC_GoToDefinition, CIC_FindReferences, CIC_GoToClass, CIC_GoToFile, CIC_GoToSymbol, CIC_ShowProjectView, CIC_Rename, CIC_FindUsages, CIC_GoToSource, CIC_SwitchHeaderSource, CIC_Build, CIC_StartDebugging, CIC_StopDebugging, CIC_RestartDebugging,
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
// turned clockwise (for text read down a tool window's auto hide tab).  A
// variable font is fWeight (700 is bold), its default weight if 0.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder, EFontTurn eTurn, float32 fWeight)
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
	if ((fWeight > 0.0f) && !cTrueTypeFont.SetAxis("wght", fWeight))
	{
		cTrueTypeFont.Kill();
		gcLogger.Error2(__METHOD__, " Font [", szFontFilename, "] has no weight axis.", NULL);
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

	mcWorkspace.Init();
	mcRecent.Init();
	mszRecentFile.Init();
	mszQueuedOpen.Init();
	mcCodeModel.Init(&mcWorkspace);
	mszConfiguration.Init();
	memset(&msDialogLook, 0, sizeof(msDialogLook));
	mcBuild.Init();
	maiBuildOrder.Init();
	muiBuildStep = 0;
	mszLibc65.Init();
	mcListings.Init();
	muiListingToShow = MAX_SIZE;
	maTabs.Init();
	maDiffTabs.Init();
	maFileIcons.Init();
	maSymbolIcons.Init();
	mpMatchFont = NULL;
	mcGit.Init();
	miGitStatusTime = 0;
	muiQueuedChange = MAX_SIZE;
	miQueuedAction = 0;
	mbKeepSettings = true;
	miStatusTime = 0;

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
	M_Pointer(mpBoldWhite);
	M_Pointer(mpBoldColours);
	M_Pointer(mpTheme);
	M_Pointer(mpCodeFamily);
	M_Pointer(mpDimFont);
	U_Pointer(msDialogLook.pcTheme);
	U_Pointer(msDialogLook.pcDimFont);
	U_Data(ARGB32, msDialogLook.sBackground);
	U_Data(ARGB32, msDialogLook.sDivider);
	M_Pointer(mpMenuBar);
	M_Pointer(mpOpenItem);
	M_Pointer(mpAddSolutionItem);
	M_Pointer(mpCloseSolutionItem);
	M_Pointer(mpRecentMenu);
	M_Pointer(mpClearRecentItem);
	M_Pointer(mpSaveItem);
	M_Pointer(mpSaveAllItem);
	M_Pointer(mpCompareItem);
	M_Pointer(mpCompareBranchItem);
	M_Pointer(mpCompareRevisionItem);
	M_Pointer(mpCompareFileItem);
	M_Pointer(mpExitItem);
	M_Pointer(mpExplorerItem);
	M_Pointer(mpUsagesItem);
	M_Pointer(mpBuildItem);
	M_Pointer(mpDebugItem);
	M_Pointer(mpClaudeItem);
	M_Pointer(mpResetLayoutItem);
	M_Pointer(mpCloseAllItem);
	M_Pointer(mpDock);
	M_Pointer(mpExplorerTool);
	M_Pointer(mpBuildTool);
	M_Pointer(mpDebugTool);
	M_Pointer(mpClaudeTool);
	M_Pointer(mpFileChooser);
	M_Pointer(mpFolderIcon);
	M_Pointer(mpFileIcon);
	M_Embedded(maTabs);
	M_Pointer(mpActive);
	M_Embedded(maDiffTabs);
	M_Pointer(mpActiveDiff);
	M_Embedded(maFileIcons);
	M_Embedded(maSymbolIcons);
	M_Pointer(mpMatchFont);
	M_Pointer(mpScript);
	M_Pointer(mpCloseGuard);
	M_Pointer(mpEdit);
	M_Pointer(mpNavigator);
	M_Pointer(mpRename);
	M_Pointer(mpUsages);
	M_Pointer(mpDebugging);
	M_Pointer(mpGitStatus);
	M_Pointer(mpDiffs);
	M_Pointer(mpAskClaude);
	M_Pointer(mpEditorMenu);
	M_Pointer(mpFindUsagesItem);
	M_Pointer(mpGoToSourceItem);
	M_Pointer(mpRenameItem);
	M_Pointer(mpExtractMethodItem);
	M_Pointer(mpExplorer);
	M_Pointer(mpExplorerMenu);
	M_Pointer(mpStartupItem);
	M_Pointer(mpRemoveSolutionItem);
	M_Pointer(mpConfigurations);
	M_Pointer(mpPlatforms);
	U_String(mszConfiguration);
	M_Pointer(mpBuildButton);
	U_Unknown(CSolutionBuild, mcBuild);
	U_Unknown(CArrayInt, maiBuildOrder);
	U_Size(muiBuildStep);
	U_String(mszLibc65);
	U_Unknown(CCPP65ListingIndex, mcListings);
	U_Size(muiListingToShow);
	M_Pointer(mpBuildPane);
	U_Unknown(CBuildWorkspace, mcWorkspace);
	U_Unknown(CBuildRecentFiles, mcRecent);
	U_String(mszRecentFile);
	U_String(mszQueuedOpen);
	U_Unknown(CIDECodeModel, mcCodeModel);
	M_Pointer(mpDebugView);
	M_Pointer(mpStartButton);
	M_Pointer(mpDialog);
	U_Unknown(CGitRepositories, mcGit);
	U_Int64(miGitStatusTime);
	M_Pointer(mpBranchLabel);
	M_Pointer(mpStatusLabel);
	U_Int64(miStatusTime);
	M_Pointer(mpProgressLabel);
	M_Pointer(mpChangeTab);
	U_Size(muiQueuedChange);
	U_Int32(miQueuedAction);
	U_Bool(mbKeepSettings);
	M_Pointer(mpClaudePane);
}


//////////////////////////////////////////////////////////////////////////
//
// The widgets, fonts and theme are freed with the content's pointers.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Free(void)
{
	size	i;

	//An object's memory is freed without its destructor running so the content and its listeners take themselves off
	//the widgets still here.
	if (mpExplorer.IsNotNull() && mpExplorer->GetTree().IsNotNull())
	{
		mpExplorer->GetTree()->GetActivationEvent()->Remove(this);
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
		mpStartButton->GetClickEvent()->Remove(&mpDebugging);
	}
	if (mpMenuBar.IsNotNull())
	{
		mpMenuBar->GetItemClickEvent()->Remove(this);
	}
	if (mpExplorerMenu.IsNotNull())
	{
		mpExplorerMenu->GetItemClickEvent()->Remove(this);
	}
	if (mpEditorMenu.IsNotNull())
	{
		mpEditorMenu->GetOpeningEvent()->Remove(this);
		mpEditorMenu->GetItemClickEvent()->Remove(this);
	}
	for (i = 0; i < maTabs.Size(); i++)
	{
		if (maTabs.Get(i).IsNotNull() && maTabs.Get(i)->GetEditor().IsNotNull())
		{
			maTabs.Get(i)->GetEditor()->GetUndoEvent()->Remove(&mpEdit);
		}
	}
	if (mpDock.IsNotNull())
	{
		mpDock->SetListener(NULL);
	}
	if (mpWindow.IsNotNull())
	{
		mpWindow->GetActivationEvent()->Remove(&mpGitStatus);
		mpWindow->ClearCloseApprover();
		mpWindow->CancelPosted(this);
		mpWindow->GetFocus()->GetFocusChangeEvent()->Remove(&mpEdit);
		mpWindow->CancelPosted(&mpCloseGuard);  //The modules' posted calls too.
		mpWindow->CancelPosted(&mpNavigator);
		mpWindow->CancelPosted(&mpRename);
	}
	if (mpClaudePane.IsNotNull() && mpClaudePane->GetSendButton().IsNotNull())
	{
		mpClaudePane->GetSendButton()->GetClickEvent()->Remove(&mpAskClaude);
		mpClaudePane->GetStopButton()->GetClickEvent()->Remove(&mpAskClaude);
		mpClaudePane->GetNewButton()->GetClickEvent()->Remove(&mpAskClaude);
	}

	//A request Claude is still working on is stopped.

	//A git status still being read is stopped.
	mcGit.Kill();

	//A build still running is stopped (an HCP65 build is waited for) before the solution it builds goes.
	mcBuild.Kill();
	maiBuildOrder.Kill();
	mcListings.Kill();
	mszLibc65.Kill();
	mcCodeModel.Kill();
	mszConfiguration.Kill();
	mcWorkspace.Kill();
	mszQueuedOpen.Kill();
	mszRecentFile.Kill();
	mcRecent.Kill();
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
	Ptr<CGridContainer>	pStatusBar;
	Ptr<CFillContainer>	pStatusGap;
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
	mpBoldWhite = GenerateIDEFont(szUIFont, "Inter 13px Bold", 13.0f, pWindow->GetColourOrder(), FT_Upright, IDE_BOLD_WEIGHT);
	if (mpBoldWhite.IsNotNull())
	{
		mpBoldColours = OMalloc<CFontColours>(mpBoldWhite);
	}
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
	for (i = 0; i < IST_Count; i++)
	{
		maSymbolIcons.Add(CIDEIcons::CreateSymbolIcon((EIDESymbolType)i, &cFormat));
	}
	//Go to's matched characters are bold, if there's a bold font.
	mpMatchFont = mpBoldColours.IsNotNull() ? mpBoldColours->GetFont(JEWEL_WHITE) : mpFontColours->GetFont(JEWEL_WHITE);

	//The features split out of the content, before the panes and menus they make.
	mpScript = OMalloc<CIDEScript>(this);
	mpCloseGuard = OMalloc<CIDECloseGuard>(this);
	mpEdit = OMalloc<CIDEEdit>(this);
	mpNavigator = OMalloc<CIDENavigator>(this);
	mpRename = OMalloc<CIDERename>(this);
	mpUsages = OMalloc<CIDEUsages>(this);
	mpDebugging = OMalloc<CIDEDebugging>(this);
	mpGitStatus = OMalloc<CIDEGitStatus>(this);
	mpDiffs = OMalloc<CIDEDiffs>(this);
	mpAskClaude = OMalloc<CIDEAskClaude>(this);

	//Solution Explorer on the left.
	mpExplorer = OMalloc<CSolutionExplorer>(pWindow, mpTheme);
	mpExplorer->SetBoldColours(mpBoldColours);
	mpExplorer->GetTree()->GetActivationEvent()->Add(this, &CIDEContent::ExplorerActivated);
	mpExplorer->GetTree()->SetContextMenu(CreateExplorerMenu());
	sPlace.BesideWell(DS_Left, IDE_EXPLORER_WIDTH);
	mpExplorerTool = mpDock->AddPane(IDE_EXPLORER_ID, "Solution Explorer", NULL, mpExplorer->GetComponent(), DPK_Tool, &sPlace);

	//Under the well, tabbed together: what Find Usages (and Find All References) found, what the build wrote, the
	//debugger's view and Claude.
	sPlace.BesideWell(DS_Bottom, IDE_BOTTOM_HEIGHT);
	mpUsages->CreatePane(&sPlace);

	mpBuildPane = OMalloc<CIDEBuildPane>(pWindow, mpTheme, mpCodeFamily, JEWEL_TEXT);
	sPlace.WithPane(IDE_USAGES_ID);
	mpBuildTool = mpDock->AddPane(IDE_BUILD_ID, "Build", NULL, mpBuildPane->GetComponent(), DPK_Tool, &sPlace);

	mpDebugView = OMalloc<CIDEDebugView>(pWindow, mpTheme, mpCodeFamily, JEWEL_TEXT);
	mpDebugTool = mpDock->AddPane(IDE_DEBUG_ID, "Debug", NULL, mpDebugView->GetComponent(), DPK_Tool, &sPlace);

	mpClaudePane = OMalloc<CIDEClaudePane>(pWindow, mpTheme, mpFontColours->GetFont(JEWEL_ICON), mpCodeFamily);
	mpClaudePane->GetSendButton()->GetClickEvent()->Add(&mpAskClaude, &CIDEAskClaude::ClaudeSendClicked);
	mpClaudePane->GetStopButton()->GetClickEvent()->Add(&mpAskClaude, &CIDEAskClaude::ClaudeStopClicked);
	mpClaudePane->GetNewButton()->GetClickEvent()->Add(&mpAskClaude, &CIDEAskClaude::ClaudeNewClicked);
	mpClaudeTool = mpDock->AddPane(IDE_CLAUDE_ID, "Claude", NULL, mpClaudePane->GetComponent(), DPK_Tool, &sPlace);
	mpDock->Show(mpUsages->GetTool());

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
	mpStartButton->GetClickEvent()->Add(&mpDebugging, &CIDEDebugging::StartClicked);
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
	CreateEditorMenu();
	pMain->AddComponent(pToolbar, 0, 1, 1, 1, 1, 0);
	pMain->AddComponent(mpDock->GetArea(), 0, 2, 1, 1, 1, 1);

	//The status line along the bottom, as IntelliJ IDEA's.
	mpStatusLabel = OMalloc<CLabel>(pWindow, mpFontColours->GetFont(JEWEL_ICON), "");
	mpStatusLabel->SetAlignment(CSH_Left, CSV_Center);
	pStatusGap = OMalloc<CFillContainer>(pWindow);
	pStatusGap->SetDesiredSize(IDE_TOOLBAR_GAP, 1);
	pStatusBar = OMalloc<CGridContainer>(pWindow);
	pStatusBar->SetDesiredSize(-1, IDE_STATUS_HEIGHT);
	pStatusBar->SetAlignment(CSH_Stretch, CSV_Stretch);
	pStatusBar->AddComponent(pStatusGap, 0, 0, 1, 1, 0, 1);
	pStatusBar->AddComponent(mpStatusLabel, 1, 0, 1, 1, 1, 1);
	mpProgressLabel = OMalloc<CLabel>(pWindow, mpFontColours->GetFont(JEWEL_ICON), "");
	mpProgressLabel->SetAlignment(CSH_Right, CSV_Center);
	pStatusBar->AddComponent(mpProgressLabel, 2, 0, 1, 1, 0, 1);
	pStatusGap = OMalloc<CFillContainer>(pWindow);
	pStatusGap->SetDesiredSize(IDE_TOOLBAR_GAP, 1);
	pStatusBar->AddComponent(pStatusGap, 3, 0, 1, 1, 0, 1);
	pMain->AddComponent(pStatusBar, 0, 3, 1, 1, 1, 0);
	pMain->AddInputActions(gaeIDECodeCommands, this, &CIDEContent::CodeCommandAction);
	pWindow->SetContainer(pMain);
	pWindow->GetActivationEvent()->Add(&mpGitStatus, &CIDEGitStatus::WindowActivated);

	//The close button asks to save changes first.
	pWindow->SetCloseApprover(&mpCloseGuard, &CIDECloseGuard::RequestExit);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// File, with Open Project/Solution, Add Solution, Close Solution, Recent
// Solutions, Clear Recent Solutions, Save, Save All, Compare with File and
// Exit;
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
	mpAddSolutionItem = pFile->AddItem("Add Solution...");
	mpCloseSolutionItem = pFile->AddItem("Close Solution");
	mpRecentMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	pFile->AddItem("Recent Solutions", NULL, mpRecentMenu);
	mpClearRecentItem = pFile->AddItem("Clear Recent Solutions");
	ShowRecentMenu();
	mpSaveItem = pFile->AddItem("Save");
	mpSaveAllItem = pFile->AddItem("Save All");
	mpCompareFileItem = pFile->AddItem("Compare with File...");
	pFile->AddSeparator();
	mpExitItem = pFile->AddItem("Exit");

	mpMenuBar = OMalloc<CMenuBar>(mpWindow, mpTheme);
	pGit = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpCompareItem = pGit->AddItem("Compare with HEAD");
	mpCompareBranchItem = pGit->AddItem("Compare with Branch...");
	mpCompareRevisionItem = pGit->AddItem("Compare with Revision...");

	pView = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpExplorerItem = pView->AddItem("Solution Explorer");
	mpUsagesItem = pView->AddItem("Find Usages");
	mpBuildItem = pView->AddItem("Build Output");
	mpDebugItem = pView->AddItem("Debug");
	mpClaudeItem = pView->AddItem("Claude");

	pWindowMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpResetLayoutItem = pWindowMenu->AddItem("Reset Window Layout");
	mpCloseAllItem = pWindowMenu->AddItem("Close All Documents");

	mpMenuBar->AddMenu("File", pFile);
	mpMenuBar->AddMenu("Edit", mpEdit->CreateEditMenu());
	mpMenuBar->AddMenu("Navigate", mpNavigator->CreateNavigateMenu());
	mpMenuBar->AddMenu("View", pView);
	mpMenuBar->AddMenu("Git", pGit);
	mpMenuBar->AddMenu("Debug", mpDebugging->CreateDebugMenu());
	mpMenuBar->AddMenu("Window", pWindowMenu);
	mpMenuBar->GetItemClickEvent()->Add(this, &CIDEContent::MenuItemClicked);
	mpMenuBar->SetAlignment(CSH_Stretch, CSV_Stretch);
	return mpMenuBar;
}


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer's menu, for the solution of the node selected: Set as
// Startup Solution and Remove.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEContent::CreateExplorerMenu(void)
{
	mpExplorerMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpStartupItem = mpExplorerMenu->AddItem("Set as Startup Solution");
	mpRemoveSolutionItem = mpExplorerMenu->AddItem("Remove Solution");
	mpExplorerMenu->GetItemClickEvent()->Add(this, &CIDEContent::MenuItemClicked);
	return mpExplorerMenu;
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
	else if (RecentItemClicked(pcItem))
	{
	}
	else if (pcItem == &mpStartupItem)
	{
		SetStartupSolution(mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode()));
	}
	else if (pcItem == &mpRemoveSolutionItem)
	{
		RemoveSolution(mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode()));
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
		mpDiffs->CompareWithHead();
	}
	else if (pcItem == &mpCompareBranchItem)
	{
		mpDiffs->ShowCompareWithBranchDialog();
	}
	else if (pcItem == &mpCompareRevisionItem)
	{
		mpDiffs->ShowCompareWithRevisionDialog();
	}
	else if (pcItem == &mpCompareFileItem)
	{
		mpDiffs->ShowCompareWithFileDialog();
	}
	else if (pcItem == &mpExitItem)
	{
		//As the window's close button: the window stops, and the application saves what it keeps as it ends.  Files
		//with changes are asked about first.
		if (mpCloseGuard->RequestExit())
		{
			mpWindow->Stop();
		}
	}
	else if (pcItem == &mpExplorerItem)
	{
		mpDock->Activate(mpExplorerTool);
	}
	else if (pcItem == &mpUsagesItem)
	{
		mpDock->Activate(mpUsages->GetTool());
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
	else if (mpEdit->EditMenuItemClicked(pcItem))
	{
	}
	else if (pcItem == &mpGoToSourceItem)
	{
		mpNavigator->GoToSource();
	}
	else if (pcItem == &mpFindUsagesItem)
	{
		mpUsages->FindUsages(false);
	}
	else if (pcItem == &mpRenameItem)
	{
		mpRename->QueueRename();
	}
	else if (mpNavigator->MenuItemClicked(pcItem))
	{
	}
	else
	{
		mpDebugging->DebugMenuItemClicked(pcItem);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Asks for a workspace (.hcp65workspace), solution (.sln) or project
// (.vcxproj) and opens it, starting on the one open.  False if none is
// chosen or it can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowOpenSolutionDialog(void)
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
	mpFileChooser = OMalloc<CFileChooser>(mpWindow, &msDialogLook, "Open Project/Solution", false, GetStartupFile(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpFolderIcon, mpFileIcon);
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
bool CIDEContent::ShowAddSolutionDialog(void)
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
	mpFileChooser = OMalloc<CFileChooser>(mpWindow, &msDialogLook, "Add Solution", false, GetStartupFile(), asTypes, sizeof(asTypes) / sizeof(SFileChooserType), mpFolderIcon, mpFileIcon);
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
bool CIDEContent::OpenSolution(const char* szFileName)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	CChars		szName;
	bool		bResult;

	//The files with changes of what is open are asked about first; it opens afterwards.
	if (IsSolutionOpen() && !mpCloseGuard->ApproveLettingGo(MAX_SIZE, szFileName))
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
	bResult = mcWorkspace.Open(szFile.Text());
	if (!szFile.Equals(szFileName) && (!bResult || (mcWorkspace.FindSolution(szFileName) == MAX_SIZE)))
	{
		//The workspace beside it no longer has it (it was removed): it is opened on its own.
		bResult = mcWorkspace.Open(szFileName);
	}
	szFile.Kill();
	mcCodeModel.Init(&mcWorkspace);
	mcCodeModel.AddDefaultSystemDirectories();
	mpExplorer->ShowWorkspace(&mcWorkspace);
	mpNavigator->ForgetGoToFiles();
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

	FindRepositories();

	//The breakpoints saved beside it, bound once its configuration's line tables are read.
	szName.Init();
	mpDebugging->GetUserSettingsFileName(&szName);
	mpDebugging->mcDebugger.ReadSourceBreakpoints(szName.Text());
	szName.Kill();

	//The configuration chosen last time, if a solution still has it; chosen before the combo boxes tell of it, so it
	//isn't indexed twice.
	mszConfiguration.Set(mcWorkspace.GetConfiguration());
	ShowConfigurations();
	mcCodeModel.StartIndexing();

	//The windows where they were left, and the files that were open opened again.
	LoadLayout();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The first file opened (OpenSolution) and the rest added to it, as the
// command line names them.  False if any couldn't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenSolutions(CArrayChars* paszFileNames)
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
bool CIDEContent::AddSolution(const char* szFileName)
{
	size	uiSolution;

	if (!IsSolutionOpen())
	{
		return OpenSolution(szFileName);
	}

	//The indexing uses the solutions, so it stops while they change.
	mcCodeModel.Reset();
	uiSolution = mcWorkspace.AddSolution(szFileName);
	if (uiSolution == MAX_SIZE)
	{
		mcCodeModel.StartIndexing();
		return gcLogger.Error2(__METHOD__, " [", szFileName, "] is already open.", NULL);
	}
	if (!mcWorkspace.GetSolution(uiSolution)->mbRead)
	{
		mcWorkspace.RemoveSolution(uiSolution);
		mcCodeModel.StartIndexing();
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
bool CIDEContent::RemoveSolution(size uiSolution)
{
	if ((mcWorkspace.NumSolutions() < 2) || (uiSolution >= mcWorkspace.NumSolutions()))
	{
		return false;
	}
	if (mcBuild.IsRunning())
	{
		return gcLogger.Error2(__METHOD__, " A solution can't be removed while a build is running.", NULL);
	}

	mcCodeModel.Reset();
	mcWorkspace.RemoveSolution(uiSolution);
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
bool CIDEContent::OpenLastSolution(void)
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
void CIDEContent::RememberOpenSolution(void)
{
	CFileUtil	cFileUtil;
	CChars		szFile;

	if (!mbKeepSettings)
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
bool CIDEContent::CloseSelectedSolution(void)
{
	size	uiSolution;

	uiSolution = mpExplorer->GetNodeSolution(mpExplorer->GetTree()->GetSelectedNode());
	if ((uiSolution == SOLUTION_EXPLORER_NO_SOLUTION) && (mcWorkspace.NumSolutions() == 1))
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
bool CIDEContent::CloseSolution(size uiSolution)
{
	size				i;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CIDEDiffTab>	pDiff;

	if (uiSolution >= mcWorkspace.NumSolutions())
	{
		return false;
	}
	if (mcBuild.IsRunning())
	{
		return gcLogger.Error2(__METHOD__, " A solution can't be closed while a build is running.", NULL);
	}

	//Its files with changes are asked about first; it closes afterwards.
	if (!mpCloseGuard->ApproveLettingGo(uiSolution, NULL))
	{
		return true;
	}

	if (mcWorkspace.NumSolutions() == 1)
	{
		LetGoOfWorkspace();
		mcCodeModel.Init(&mcWorkspace);
		mcCodeModel.AddDefaultSystemDirectories();
		mpExplorer->ShowWorkspace(&mcWorkspace);
		mpNavigator->ForgetGoToFiles();
		ShowConfigurations();
		return true;
	}

	//Its files' documents, and diffs of them, are closed (closing a document lets its tab go, see PaneClosed).
	for (i = maTabs.Size(); i > 0; i--)
	{
		pTab = maTabs.Get(i - 1);
		if (IsSolutionFile(uiSolution, pTab->GetProject()))
		{
			mpDock->Close(pTab->GetDockPane());
		}
	}
	for (i = maDiffTabs.Size(); i > 0; i--)
	{
		pDiff = maDiffTabs.Get(i - 1);
		if (IsSolutionFile(uiSolution, pDiff->GetSide(TDS_Left)->muiProject) || IsSolutionFile(uiSolution, pDiff->GetSide(TDS_Right)->muiProject))
		{
			mpDock->Close(pDiff->GetDockPane());
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
bool CIDEContent::IsSolutionFile(size uiSolution, size uiProject)
{
	return (uiProject < mcWorkspace.NumProjects()) && (mcWorkspace.GetProject(uiProject)->uiSolution == uiSolution);
}


//////////////////////////////////////////////////////////////////////////
//
// The solution debugged (F5), and whose configuration the toolbar shows
// first.  The workspace file is written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SetStartupSolution(size uiSolution)
{
	if (uiSolution >= mcWorkspace.NumSolutions())
	{
		return false;
	}
	if (uiSolution == mcWorkspace.GetStartup())
	{
		return true;
	}
	mcWorkspace.SetStartup(uiSolution);
	WorkspaceChanged(false);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What is open is let go of: its layout saved, a build of it stopped, its
// files closed and the references found in it forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::LetGoOfWorkspace(void)
{
	SaveLayout();

	mcBuild.Kill();
	mcBuild.Init();
	maiBuildOrder.ReInit();
	SetListings(false);
	CloseAllFiles();
	mpUsages->Clear();

	mcCodeModel.Kill();
	mcWorkspace.Clear();
	mpNavigator->ForgetGoToFiles();
	mszConfiguration.Clear();

	//The repositories are found again from the new solutions' files, and read at the next tick.
	mcGit.RemoveAll();
	miGitStatusTime = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// After a solution is added, removed or made the startup one: Solution
// Explorer and the toolbar show it, the open files are given their
// projects again and (bReindex) the code is indexed again; and the
// workspace file is written.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::WorkspaceChanged(bool bReindex)
{
	size	i;

	mpExplorer->ShowWorkspace(&mcWorkspace);
	mpNavigator->ForgetGoToFiles();
	FindRepositories();
	ShowConfigurations();
	if (bReindex)
	{
		for (i = 0; i < maTabs.Size(); i++)
		{
			maTabs.Get(i)->SetProject(mpExplorer->FindFileProject(maTabs.Get(i)->GetFileName()));
		}
		for (i = 0; i < maDiffTabs.Size(); i++)
		{
			maDiffTabs.Get(i)->GetSide(TDS_Left)->muiProject = mpExplorer->FindFileProject(maDiffTabs.Get(i)->GetSide(TDS_Left)->mszFile.Text());
			maDiffTabs.Get(i)->GetSide(TDS_Right)->muiProject = mpExplorer->FindFileProject(maDiffTabs.Get(i)->GetSide(TDS_Right)->mszFile.Text());
		}
		mcCodeModel.StartIndexing();
		for (i = 0; i < maTabs.Size(); i++)
		{
			HighlightTab(maTabs.Get(i));
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
bool CIDEContent::WriteWorkspace(void)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	bool		bResult;

	if (mcWorkspace.NumSolutions() == 0)
	{
		return false;
	}
	mcWorkspace.SetConfiguration(mszConfiguration.Text());
	szFile.Init(mcWorkspace.GetFile());
	if (szFile.Empty())
	{
		szFile.Set(mcWorkspace.GetSolution(0)->mszFile);
		cFileUtil.RemoveExtension(&szFile);
		szFile.Append(BUILD_WORKSPACE_EXTENSION);
	}
	bResult = mcWorkspace.Write(szFile.Text());
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
void CIDEContent::FindRepositories(void)
{
	size	i;

	for (i = 0; i < mcWorkspace.NumSolutions(); i++)
	{
		mcGit.FindRepository(mcWorkspace.GetSolution(i)->mszFile.Text());
	}
	mpExplorer->ColourFiles(&mcGit, mpFontColours);
	mpGitStatus->ReadGitStatus();
	miGitStatusTime = GetDebugTickCount();
}


//////////////////////////////////////////////////////////////////////////
//
// The names of every solution's configurations in the toolbar.  The one
// chosen stays chosen if a solution still has it, or the startup
// solution's first is.  The code model is told but not indexed again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowConfigurations(void)
{
	CArrayChars	aszNames;

	aszNames.Init();
	mcWorkspace.GetConfigurationNames(&aszNames);
	if (!aszNames.Contains(mszConfiguration.Text()))
	{
		mszConfiguration.Set((aszNames.NumElements() > 0) ? aszNames.GetText(0) : "");
	}
	aszNames.Kill();

	SetComboBoxItems(mpConfigurations, false);
	SetComboBoxItems(mpPlatforms, true);
	if (StringCompare(mszConfiguration.Text(), mcCodeModel.GetConfiguration()) != 0)
	{
		mcCodeModel.SetConfiguration(mszConfiguration.Text());
	}
	ShowConfiguration();
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution is there and could be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::IsSolutionOpen(void)
{
	return (mcWorkspace.GetStartupSolution() != NULL) && mcWorkspace.GetStartupSolution()->mbRead;
}


//////////////////////////////////////////////////////////////////////////
//
// The startup solution's file (a .sln or .vcxproj), "" if none is open.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEContent::GetStartupFile(void)
{
	return (mcWorkspace.GetStartupSolution() != NULL) ? mcWorkspace.GetStartupSolution()->mszFile.Text() : (char*)"";
}


//////////////////////////////////////////////////////////////////////////
//
// The file the per-user files (the layout, the breakpoints) are named
// after: the workspace file, or the solution if there isn't one.  False
// if nothing is open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GetSettingsBaseFile(CChars* pszDest)
{
	if (mcWorkspace.GetFile()[0] != '\0')
	{
		pszDest->Set(mcWorkspace.GetFile());
		return true;
	}
	if (GetStartupFile()[0] != '\0')
	{
		pszDest->Set(GetStartupFile());
		return true;
	}
	return false;
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
	mpGitStatus->ShowBranch();
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

	//Activating the document, or clicking its tab, gives its editor the keys with its caret where it was.
	pPane->SetFocusTarget(pTab->GetEditor());

	//The right button: Rename and Refactor.
	pTab->GetEditor()->SetContextMenu(mpEditorMenu);
	pTab->GetCodeEditor()->GetChangeBarClickEvent()->Add(&mpGitStatus, &CIDEGitStatus::ChangeBarClicked);
	pTab->GetCodeEditor()->GetGlyphClickEvent()->Add(&mpDebugging, &CIDEDebugging::GlyphClicked);
	pTab->GetEditor()->GetUndoEvent()->Add(&mpEdit, &CIDEEdit::EditorUndoChanged);
	mpDebugging->ShowBreakpoints(pTab);
	mpGitStatus->ReadGitBase(pTab);
	mpGitStatus->UpdateChangeBars(pTab);
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
	size	i;

	//Closing the document lets its file go (PaneClosed); one with changes is asked about first (PaneClosing).
	for (i = maTabs.Size(); i > 0; i--)
	{
		if (i <= maTabs.Size())
		{
			mpDock->Close(maTabs.Get(i - 1)->GetDockPane());
		}
	}
	for (i = maDiffTabs.Size(); i > 0; i--)
	{
		if (i <= maDiffTabs.Size())
		{
			mpDock->Close(maDiffTabs.Get(i - 1)->GetDockPane());
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
	mpGitStatus->GitFileSaved(pTab->GetFileName());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Everything is indexed again, from the files; the open .cpp files with
// edits are indexed again from their editors once that has finished.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReindexAll(void)
{
	size	i;

	mcCodeModel.ReindexAll();
	for (i = 0; i < maTabs.Size(); i++)
	{
		if (maTabs.Get(i)->IsEdited())
		{
			maTabs.Get(i)->SetIndexStale(true);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The open files changed since they were indexed are indexed again from
// their editors' text: after a pause in the typing, or with bNow straight
// away (before Rename, Find Usages or Go to is worked out).  A header is
// preprocessed from its editor from then on, and indexed again with the
// first unit that included it; then the .cpp (and .c) files.  Not while
// everything is being indexed: they are indexed after.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReindexStaleTabs(bool bNow)
{
	size				i;
	size				iPass;
	Ptr<CIDEEditorTab>	pTab;
	EIDEFileType		eType;

	if (mcCodeModel.IsIndexing())
	{
		return;
	}
	for (iPass = 0; iPass < 2; iPass++)
	{
		for (i = 0; i < maTabs.Size(); i++)
		{
			pTab = maTabs.Get(i);
			pTab->SeeChanges(GetDebugTickCount());
			if (!pTab->IsIndexStale() || (pTab->GetProject() == MAX_SIZE) || (!bNow && pTab->IsRecolourPending()))
			{
				continue;
			}
			eType = CIDEIcons::GetFileType(pTab->GetFileName());
			if ((iPass == 0) && (eType == IFT_Header))
			{
				mcCodeModel.SetHeaderText(pTab->GetFileName(), pTab->GetEditor()->GetText());
				ReindexHeader(pTab->GetFileName());
				pTab->SetIndexStale(false);
			}
			else if ((iPass == 1) && ((eType == IFT_CPP) || (eType == IFT_C)))
			{
				mcCodeModel.ReindexUnit(pTab->GetProject(), pTab->GetFileName(), pTab->GetEditor()->GetText());
				pTab->SetIndexStale(false);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The header szFileName indexed again (after its text changed) with the
// first unit that included it, from that unit's editor if it is open.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReindexHeader(const char* szFileName)
{
	size				uiProject;
	CChars				szUnit;
	Ptr<CIDEEditorTab>	pUnitTab;

	szUnit.Init();
	if (mcCodeModel.GetIncluder(szFileName, &uiProject, &szUnit))
	{
		pUnitTab = FindTab(szUnit.Text());
		mcCodeModel.ReindexUnit(uiProject, szUnit.Text(), pUnitTab.IsNotNull() ? pUnitTab->GetEditor()->GetText() : NULL, szFileName);
	}
	szUnit.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file closed without saving its edits is indexed again as it is on
// disk: the index had its editor's text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ReindexDiscarded(Ptr<CIDEEditorTab> pTab)
{
	EIDEFileType	eType;

	if ((pTab->GetProject() == MAX_SIZE) || mcCodeModel.IsIndexing())
	{
		return;
	}
	eType = CIDEIcons::GetFileType(pTab->GetFileName());
	if (eType == IFT_Header)
	{
		mcCodeModel.SetHeaderText(pTab->GetFileName(), NULL);
		ReindexHeader(pTab->GetFileName());
	}
	else if ((eType == IFT_CPP) || (eType == IFT_C))
	{
		mcCodeModel.ReindexUnit(pTab->GetProject(), pTab->GetFileName(), NULL);
	}
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
	else if (psEvent->eCommand == CIC_GoToClass)
	{
		mpNavigator->QueueGoTo(IDE_GO_TO_CLASS);
	}
	else if (psEvent->eCommand == CIC_GoToFile)
	{
		mpNavigator->QueueGoTo(IDE_GO_TO_FILE);
	}
	else if (psEvent->eCommand == CIC_GoToSource)
	{
		mpNavigator->GoToSource();
	}
	else if (psEvent->eCommand == CIC_SwitchHeaderSource)
	{
		mpNavigator->SwitchHeaderSource();
	}
	else if (psEvent->eCommand == CIC_FindUsages)
	{
		mpUsages->FindUsages(false);
	}
	else if (psEvent->eCommand == CIC_Rename)
	{
		mpRename->QueueRename();
	}
	else if (psEvent->eCommand == CIC_ShowProjectView)
	{
		ShowActiveFileInExplorer();
	}
	else if (psEvent->eCommand == CIC_GoToSymbol)
	{
		mpNavigator->QueueGoTo(IDE_GO_TO_SYMBOL);
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
		mpDebugging->DebugCommandAction(psEvent->eCommand);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+1: Solution Explorer is shown with the active file selected, its
// folders opened and the row scrolled into view.  The editor keeps the
// keys, its caret where it was.  False if there is no file to show.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::ShowActiveFileInExplorer(void)
{
	Ptr<CTreeContainer>	pTree;
	int32				iNode;

	pTree = mpExplorer->GetTree();
	mpDock->Activate(mpExplorerTool);
	if (mpActive.IsNull())
	{
		mpWindow->GetFocus()->SetFocus(pTree);
		return false;
	}
	iNode = mpExplorer->FindFilePathNode(mpActive->GetFileName(), mpActive->GetProject());
	if (iNode != TREE_NO_NODE)
	{
		pTree->SelectNode(iNode);
		mpWindow->Layout();
		pTree->ShowRow(pTree->FindNodeRow(iNode));
	}
	mpDock->Activate(mpActive->GetDockPane());
	return iNode != TREE_NO_NODE;
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
	ReindexStaleTabs(true);
	if (!mcCodeModel.FindDefinition(mpActive->GetFileName(), iLine, iColumn, &sLocation))
	{
		ShowStatus("No definition found at the caret.");
		return false;
	}
	bResult = OpenLocation(&sLocation);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Visual Studio's Find All References (Shift+F12): Find Usages with the
// declarations and definition of what is named at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::FindReferences(void)
{
	return mpUsages->FindUsages(true);
}


//////////////////////////////////////////////////////////////////////////
//
// The status line says szText for a while (IDE_STATUS_SHOWN), or until
// something else is said.  A scripted run (checking the IDE) prints it
// too.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowStatus(const char* szText)
{
	mpStatusLabel->SetText(szText);
	miStatusTime = GetDebugTickCount();
	if (mpWindow.IsNotNull())
	{
		mpWindow->RequestTick(IDE_STATUS_SHOWN);
	}
	if (!mbKeepSettings)
	{
		printf("Status: %s\n", szText);
		fflush(stdout);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CIDEContent::GetStatus(void) { return mpStatusLabel->GetText(); }


//////////////////////////////////////////////////////////////////////////
//
// The status message is cleared once it has been shown a while, and the
// indexing's progress is shown on the right while it lasts.  How many
// milliseconds until it changes next, -1 if it won't by itself.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEContent::UpdateStatusLine(int64 iNow)
{
	int64	iWait;
	size	uiIndexed;
	size	uiToIndex;
	CChars	szProgress;

	iWait = -1;
	if (miStatusTime != 0)
	{
		if (iNow - miStatusTime >= IDE_STATUS_SHOWN)
		{
			mpStatusLabel->SetText("");
			miStatusTime = 0;
		}
		else
		{
			iWait = IDE_STATUS_SHOWN - (iNow - miStatusTime);
		}
	}

	mcCodeModel.GetIndexingProgress(&uiIndexed, &uiToIndex);
	szProgress.Init();
	if (uiIndexed < uiToIndex)
	{
		szProgress.Append("Indexing ");
		szProgress.Append((uint32)uiIndexed);
		szProgress.Append(" of ");
		szProgress.Append((uint32)uiToIndex);
		szProgress.Append(" files");
		iWait = ((iWait < 0) || (iWait > IDE_PROGRESS_POLL)) ? IDE_PROGRESS_POLL : iWait;
	}
	if (!szProgress.Equals(mpProgressLabel->GetText()))
	{
		mpProgressLabel->SetText(szProgress.Text());
	}
	szProgress.Kill();
	return iWait;
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

	if (!IsSolutionOpen() || mszConfiguration.Empty() || mcBuild.IsRunning())
	{
		return false;
	}
	maiBuildOrder.ReInit();
	mcWorkspace.GetBuildOrder(mszConfiguration.Text(), &maiBuildOrder);
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
		mpWindow->RequestTick(IDE_BUILD_POLL);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A running build's new output goes in the Build tab.  When an
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
bool CIDEContent::StartBuildStep(CArrayChars* paszLines)
{
	size						uiSolution;
	CBuildWorkspaceSolution*	pcSolution;
	CChars						szLine;

	uiSolution = (size)maiBuildOrder.GetValue(muiBuildStep);
	pcSolution = mcWorkspace.GetSolution(uiSolution);
	if (maiBuildOrder.NumElements() > 1)
	{
		szLine.Init("------ ");
		szLine.Append(pcSolution->mcSolution.GetName());
		szLine.Append(": ");
		szLine.Append(mszConfiguration);
		szLine.Append(" ------");
		paszLines->Add(szLine.Text());
		szLine.Kill();
	}
	return mcBuild.Start(&pcSolution->mcSolution, mcWorkspace.FindConfiguration(uiSolution, mszConfiguration.Text()), NULL, mszLibc65.Text(), paszLines);
}


//////////////////////////////////////////////////////////////////////////
//
// The build pane's bar: blue while the build runs, as far as it has got
// over the solutions it builds (each an equal part); then all green if
// every solution built, else all red.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowBuildProgress(void)
{
	size	uiSteps;
	float	fFraction;

	uiSteps = maiBuildOrder.NumElements();
	if (mcBuild.IsRunning() && (uiSteps > 0))
	{
		fFraction = ((float)muiBuildStep + mcBuild.GetFraction()) / (float)uiSteps;
		mpBuildPane->SetProgress(fFraction, JEWEL_BLUE);
	}
	else if (mcBuild.GetState() == BS_Succeeded)
	{
		mpBuildPane->SetProgress(1.0f, JEWEL_GREEN);
	}
	else if ((mcBuild.GetState() == BS_Failed) || (mcBuild.GetState() == BS_Cancelled))
	{
		mpBuildPane->SetProgress(1.0f, JEWEL_RED);
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
void CIDEContent::FinishBuildSteps(CArrayChars* paszLines)
{
	CBuildConfiguration*	pcConfiguration;

	while (!mcBuild.IsRunning() && (mcBuild.GetState() == BS_Succeeded) && (muiBuildStep < maiBuildOrder.NumElements()))
	{
		pcConfiguration = mcWorkspace.FindConfiguration((size)maiBuildOrder.GetValue(muiBuildStep), mszConfiguration.Text());
		if (pcConfiguration->IsHCP65())
		{
			ReadListings(paszLines);
		}
		muiBuildStep++;
		if (muiBuildStep == maiBuildOrder.NumElements())
		{
			mpDebugging->ReadLineTables();
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
void CIDEContent::ReadListings(CArrayChars* paszLines)
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
	pcCurrent = GetChosenConfiguration();
	if ((szConfiguration == NULL) || (szPlatform == NULL) || ((pcCurrent != NULL) && pcCurrent->Is(szConfiguration, szPlatform)))
	{
		return;
	}

	pcChosen = mcWorkspace.FindAnyConfiguration(szConfiguration, szPlatform);
	if (pcChosen == NULL)
	{
		if ((pcCurrent != NULL) && pcCurrent->Is(NULL, szPlatform))
		{
			pcChosen = mcWorkspace.FindAnyConfiguration(szConfiguration, NULL);
		}
		else
		{
			pcChosen = mcWorkspace.FindAnyConfiguration(NULL, szPlatform);
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
// Every solution's configurations' configuration names, or platforms, in
// pComboBox.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetComboBoxItems(Ptr<CComboBox> pComboBox, bool bPlatforms)
{
	CArrayChars		aszNames;
	size			i;

	aszNames.Init();
	mcWorkspace.GetConfigurationNames(&aszNames, bPlatforms);
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
	CChars					szName;
	CBuildConfiguration*	pcConfiguration;

	pcConfiguration = GetChosenConfiguration();
	if (pcConfiguration == NULL)
	{
		return;
	}

	szName.Init();
	pcConfiguration->GetConfigurationName(&szName);
	SelectComboBoxItem(mpConfigurations, szName.Text());
	szName.Clear();
	pcConfiguration->GetPlatformName(&szName);
	SelectComboBoxItem(mpPlatforms, szName.Text());
	szName.Kill();

	//Its programs' line tables bind the source breakpoints.
	mpDebugging->ReadLineTables();
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
// The build configuration szConfiguration is chosen: each solution's of
// that name.  The code is parsed as their solution configurations build
// it: if it changed the solutions are indexed again and the open files are
// highlighted again.  False if no solution has such a build configuration.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SetConfiguration(const char* szConfiguration)
{
	size	i;

	if (mcWorkspace.FindAnyConfiguration(szConfiguration) == NULL)
	{
		return false;
	}
	mszConfiguration.Set(szConfiguration);
	if (StringCompare(mcWorkspace.GetConfiguration(), szConfiguration) != 0)
	{
		//Remembered in the workspace file for next time.
		WriteWorkspace();
	}
	ShowConfiguration();

	if (StringCompare(mszConfiguration.Text(), mcCodeModel.GetConfiguration()) != 0)
	{
		mcCodeModel.SetConfiguration(mszConfiguration.Text());
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
// The startup solution's chosen build configuration: what is debugged.
// NULL if none is chosen or the startup solution hasn't it.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CIDEContent::GetConfiguration(void)
{
	return mcWorkspace.FindConfiguration(mcWorkspace.GetStartup(), mszConfiguration.Text());
}


//////////////////////////////////////////////////////////////////////////
//
// The chosen build configuration's name, "" if none is chosen.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEContent::GetConfigurationName(void)
{
	return mszConfiguration.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// A configuration with the chosen name, the startup solution's if it has
// one: what the toolbar shows.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CIDEContent::GetChosenConfiguration(void)
{
	return mcWorkspace.FindAnyConfiguration(mszConfiguration.Text());
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
	mpDebugging->PollDebugger(pWindow);
	mpAskClaude->PollClaude(pWindow);
	if (mpClaudePane.IsNotNull())
	{
		mpClaudePane->ScrollToEnd();
	}

	//The platform's clock: the window's total time only counts the time it spent updating.
	iNow = GetDebugTickCount();
	iSoonest = mpGitStatus->PollGit(iNow);
	if ((muiQueuedChange != MAX_SIZE) && mpActive.IsNotNull() && (mpActive->GetEditor()->GetActualSize().x > 0))
	{
		if (mpGitStatus->ShowChange(mpActive, muiQueuedChange))
		{
			if (miQueuedAction == 1)
			{
				mpGitStatus->RollbackChange();
			}
			else if (miQueuedAction == 2)
			{
				mpGitStatus->ShowChangeDiff();
			}
		}
		muiQueuedChange = MAX_SIZE;
	}
	iWait = mpDiffs->PollDiffs(iNow);
	if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
	{
		iSoonest = iWait;
	}
	for (i = 0; i < maTabs.Size(); i++)
	{
		pTab = maTabs.Get(i);
		if (pTab->NeedsChangeBars())
		{
			mpGitStatus->UpdateChangeBars(pTab);
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
	ReindexStaleTabs(false);
	iWait = UpdateStatusLine(iNow);
	if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
	{
		iSoonest = iWait;
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
	pDiff = mpDiffs->FindDiffTab(pcPane);
	if (pTab.IsNull() && pDiff.IsNull())
	{
		return;
	}
	mpActive = pTab;
	mpActiveDiff = pDiff;
	mpGitStatus->ShowBranch();
}



//////////////////////////////////////////////////////////////////////////
//
// A file's tab with changes not saved ('*'), kept by Close Unmodified.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::PaneClosing(CDockPane* pcPane)
{
	return mpCloseGuard->PaneClosing(pcPane);
}


//////////////////////////////////////////////////////////////////////////
//
// A file's tab with changes not saved ('*'), kept by Close Unmodified.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::IsPaneModified(CDockPane* pcPane)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = FindTab(pcPane);
	return pTab.IsNotNull() && pTab->IsModified();
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

	//The Edit menu lets go of text in it.
	mpEdit->PaneClosed(pcPane->GetContent());

	pDiff = mpDiffs->FindDiffTab(pcPane);
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
		mpGitStatus->CloseChangePopup();
	}
	pTab->GetCodeEditor()->GetChangeBarClickEvent()->Remove(&mpGitStatus);
	pTab->GetCodeEditor()->GetGlyphClickEvent()->Remove(&mpDebugging);
	pTab->GetEditor()->GetUndoEvent()->Remove(&mpEdit);
	mpEdit->FileClosed(pTab->GetFileName());
	if (pTab->IsModified())
	{
		ReindexDiscarded(pTab);
	}
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
// <workspace>.layout.user beside the workspace file, or the solution or
// project if there isn't one.  False if no solution is open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GetLayoutFileName(CChars* pszDest)
{
	CFileUtil	cFileUtil;

	pszDest->Init();
	if (!GetSettingsBaseFile(pszDest))
	{
		pszDest->Kill();
		return false;
	}
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

	if (!mbKeepSettings || !IsSolutionOpen() || !GetLayoutFileName(&szFileName))
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
// it names; the active one's editor has the focus.  False if there isn't
// one.
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

		//As IntelliJ's, the open editor has the keys, its caret shown.
		if (mpActive.IsNotNull())
		{
			mpWindow->GetFocus()->SetFocus(mpActive->GetEditor());
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
Ptr<CDockPane> CIDEContent::GetBuildTool(void) { return mpBuildTool; }
Ptr<CDockPane> CIDEContent::GetDebugTool(void) { return mpDebugTool; }
Ptr<CDockPane> CIDEContent::GetClaudeTool(void) { return mpClaudeTool; }
Ptr<CIDEBuildPane> CIDEContent::GetBuildPane(void) { return mpBuildPane; }
bool CIDEContent::IsBuilding(void) { return mcBuild.IsRunning(); }
CCPP65ListingIndex* CIDEContent::GetListings(void) { return &mcListings; }  //The last HCP65 build's, for showing a line's assembly.
CBuildWorkspace* CIDEContent::GetWorkspace(void) { return &mcWorkspace; }
CBuildRecentFiles* CIDEContent::GetRecent(void) { return &mcRecent; }


//////////////////////////////////////////////////////////////////////////
//
// The user's file the recent solutions are kept in, read now.  Empty,
// they aren't kept.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetRecentFile(const char* szFile)
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
void CIDEContent::ClearRecentSolutions(void)
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
void CIDEContent::WriteRecent(void)
{
	if (!mszRecentFile.Empty() && !mcRecent.Write(mszRecentFile.Text()))
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
void CIDEContent::ShowRecentMenu(void)
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
		mpRecentMenu->AddItem("(No recent solutions)");
		return;
	}
	szText.Init();
	for (i = 0; i < mcRecent.NumFiles(); i++)
	{
		szText.Clear();
		szText.Append((uint32)(i + 1));
		szText.Append(' ');
		szText.Append(mcRecent.GetFile(i));
		mpRecentMenu->AddItem(szText.Text());
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A recent solution's item: it is opened at the next tick, once its menu
// is done with the click.  False if pcItem isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::RecentItemClicked(CMenuItem* pcItem)
{
	Ptr<CMenuItem>	pItem;
	size			i;

	if (mpRecentMenu.IsNull())
	{
		return false;
	}
	for (i = 0; (i < mpRecentMenu->NumItems()) && (i < mcRecent.NumFiles()); i++)
	{
		pItem = mpRecentMenu->GetItem(i);
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
bool CIDEContent::OpenRecentSolution(size uiIndex)
{
	if (uiIndex >= mcRecent.NumFiles())
	{
		return false;
	}
	mszQueuedOpen.Set(mcRecent.GetFile(uiIndex));
	mpWindow->Post(this, &CIDEContent::OpenQueuedSolution);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The recent solution chosen, opened after its menu's click.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::OpenQueuedSolution(void)
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
CIDECodeModel* CIDEContent::GetCodeModel(void) { return &mcCodeModel; }
Ptr<CIDEScript> CIDEContent::GetScript(void) { return mpScript; }
CSolutionExplorer* CIDEContent::GetExplorer(void) { return &mpExplorer; }



//////////////////////////////////////////////////////////////////////////
//
// The symbol named at the active editor's caret (or just before it, as
// after it's typed), or where its selection starts: its key and every
// place it is named.  False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::FindSymbolAtCaret(CArrayIDELocations* pasDest, CChars* pszKey)
{
	Ptr<CTextArea>	pEditor;
	size			uiStart;
	size			uiEnd;
	size			uiLine;
	int				iColumn;
	bool			bSelected;

	if (mpActive.IsNull())
	{
		return false;
	}
	pEditor = mpActive->GetEditor();
	bSelected = pEditor->GetSelection(&uiStart, &uiEnd);
	if (!bSelected)
	{
		uiStart = pEditor->GetCaretPosition();
	}
	uiLine = pEditor->FindLine(uiStart);
	iColumn = (int)(uiStart - pEditor->GetLineStart(uiLine));
	if (mcCodeModel.FindReferences(mpActive->GetFileName(), (int)uiLine, iColumn, pasDest, pszKey))
	{
		return true;
	}
	return !bSelected && (iColumn > 0) && mcCodeModel.FindReferences(mpActive->GetFileName(), (int)uiLine, iColumn - 1, pasDest, pszKey);
}


//////////////////////////////////////////////////////////////////////////
//
// The text of a file's line (0 based), as its editor has it if it is
// open, else as it is on the disk; trimmed, tabs as spaces.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::GetFileLine(const char* szFileName, int iLine, CChars* pszDest)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CTextFile			cFile;
	char*				szPosition;
	char*				szEnd;
	int					i;

	pTab = FindTab(szFileName);
	szPosition = NULL;
	cFile.Init();
	if (pTab.IsNotNull())
	{
		pEditor = pTab->GetEditor();
		if ((size)iLine < pEditor->NumLines())
		{
			szPosition = &pEditor->GetText()[pEditor->GetLineStart((size)iLine)];
		}
	}
	else if (cFile.Read((char*)szFileName))
	{
		szPosition = cFile.Text();
		for (i = 0; (i < iLine) && szPosition; i++)
		{
			szPosition = strchr(szPosition, '\n');
			szPosition = szPosition ? szPosition + 1 : NULL;
		}
	}
	if (szPosition)
	{
		szEnd = strchr(szPosition, '\n');
		pszDest->AppendSubString(szPosition, szEnd ? szEnd : szPosition + strlen(szPosition));
		pszDest->Replace('\r', ' ');
		pszDest->Replace('\t', ' ');
		pszDest->StripWhitespace(true);
	}
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The tab whose editor pcEditor is, NULL if none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::FindTab(CTextArea* pcEditor)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetEditor() == pcEditor)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The open documents' tabs.
//
//////////////////////////////////////////////////////////////////////////
size CIDEContent::NumTabs(void) { return maTabs.Size(); }
Ptr<CIDEEditorTab> CIDEContent::GetTab(size uiIndex) { return maTabs.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The editor's context menu, as IntelliJ's: Go to Source (Control+B, an
// #include's file or a definition), Find Usages (Alt+F7), Rename
// (Shift+F6) and Refactor, Extract Method (not done yet, so always
// disabled).  What can't
// be done is disabled as it opens.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEContent::CreateEditorMenu(void)
{
	Ptr<CMenuContainer>	pRefactor;

	pRefactor = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpExtractMethodItem = pRefactor->AddItem("Extract Method...");
	mpExtractMethodItem->SetShortcut("Ctrl+Alt+M");
	mpExtractMethodItem->SetEnabled(false);

	mpEditorMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpGoToSourceItem = mpEditorMenu->AddItem("Go to Source");
	mpGoToSourceItem->SetShortcut("Ctrl+B");
	mpFindUsagesItem = mpEditorMenu->AddItem("Find Usages");
	mpFindUsagesItem->SetShortcut("Alt+F7");
	mpEditorMenu->AddSeparator();
	mpRenameItem = mpEditorMenu->AddItem("Rename...");
	mpRenameItem->SetShortcut("Shift+F6");
	mpEditorMenu->AddItem("Refactor", NULL, pRefactor);
	mpEditorMenu->GetItemClickEvent()->Add(this, &CIDEContent::MenuItemClicked);
	mpEditorMenu->GetOpeningEvent()->Add(this, &CIDEContent::EditorMenuOpening);
	return mpEditorMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// The right button has put the caret under the pointer, or kept the
// selection it was pressed in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::EditorMenuOpening(CMenuContainer* pcMenu)
{
	SIDERenameTarget	sTarget;
	CArrayIDELocations	asLocations;
	CChars				szKey;

	sTarget.Init();
	mpRenameItem->SetEnabled(mpRename->FindRenameTarget(&sTarget) && sTarget.bRenamable);
	sTarget.Kill();

	asLocations.Init();
	szKey.Init();
	mpFindUsagesItem->SetEnabled(FindSymbolAtCaret(&asLocations, &szKey));
	mpGoToSourceItem->SetEnabled(mpNavigator->CanGoToSource());
	szKey.Kill();
	KillIDELocations(&asLocations);
}


//////////////////////////////////////////////////////////////////////////
//
// Without bKeep the window layout and the open solution aren't saved for
// the next time: the IDE is being scripted to check it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::SetKeepSettings(bool bKeep) { mbKeepSettings = bKeep; }
Ptr<CIDEAskClaude> CIDEContent::GetAskClaude(void) { return mpAskClaude; }
Ptr<CIDEDiffs> CIDEContent::GetDiffs(void) { return mpDiffs; }


//////////////////////////////////////////////////////////////////////////
//
// The tab whose code editor is pcEditor, NULL if none.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::FindTab(CCodeEditor* pcEditor)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetCodeEditor() == pcEditor)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}
Ptr<CIDEGitStatus> CIDEContent::GetGitStatus(void) { return mpGitStatus; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::BuildClicked(void) { Build(); }  //The toolbar's Build button.
Ptr<CIDEDebugging> CIDEContent::GetDebugging(void) { return mpDebugging; }
