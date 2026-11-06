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
	mcCodeModel.Init(&mcWorkspace);
	memset(&msDialogLook, 0, sizeof(msDialogLook));
	maFileIcons.Init();
	maSymbolIcons.Init();
	mpMatchFont = NULL;
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
	M_Pointer(mpExitItem);
	M_Pointer(mpViewMenu);
	M_Pointer(mpResetLayoutItem);
	M_Pointer(mpDock);
	M_Pointer(mpFolderIcon);
	M_Pointer(mpFileIcon);
	M_Embedded(maFileIcons);
	M_Embedded(maSymbolIcons);
	M_Pointer(mpMatchFont);
	M_Pointer(mpScript);
	M_Pointer(mpCloseGuard);
	M_Pointer(mpEdit);
	M_Pointer(mpNavigator);
	M_Pointer(mpRename);
	M_Pointer(mpUsages);
	M_Pointer(mpDocuments);
	M_Pointer(mpSolutions);
	M_Pointer(mpBuilding);
	M_Pointer(mpBuildConfigurations);
	M_Pointer(mpDebugging);
	M_Pointer(mpGitStatus);
	M_Pointer(mpDiffs);
	M_Pointer(mpAskClaude);
	M_Pointer(mpEditorMenu);
	U_Unknown(CBuildWorkspace, mcWorkspace);
	U_Unknown(CIDECodeModel, mcCodeModel);
	M_Pointer(mpStatusLabel);
	U_Int64(miStatusTime);
	M_Pointer(mpProgressLabel);
	U_Bool(mbKeepSettings);
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
	if (mpExitItem.IsNotNull())
	{
		mpExitItem->GetClickEvent()->Remove(this);
		mpResetLayoutItem->GetClickEvent()->Remove(this);
	}
	if (mpViewMenu.IsNotNull())
	{
		mpViewMenu->GetOpeningEvent()->Remove(this);
		mpViewMenu->GetItemClickEvent()->Remove(this);
	}
	for (i = 0; mpDocuments.IsNotNull() && (i < mpDocuments->NumTabs()); i++)
	{
		if (mpDocuments->GetTab(i).IsNotNull() && mpDocuments->GetTab(i)->GetEditor().IsNotNull())
		{
			mpDocuments->GetTab(i)->GetEditor()->GetUndoEvent()->Remove(&mpEdit);
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
		mpWindow->CancelPosted(&mpSolutions);
	}

	//A build still running is stopped (an HCP65 build is waited for) before the solution it builds goes.
	if (mpBuilding.IsNotNull())
	{
		mpBuilding->StopBuild();
	}
	mcCodeModel.Kill();
	mcWorkspace.Kill();
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
	mpDocuments = OMalloc<CIDEDocuments>(this);
	mpSolutions = OMalloc<CIDESolutions>(this);
	mpBuilding = OMalloc<CIDEBuilding>(this);
	mpBuildConfigurations = OMalloc<CIDEConfigurations>(this);
	mpDebugging = OMalloc<CIDEDebugging>(this);
	mpGitStatus = OMalloc<CIDEGitStatus>(this);
	mpDiffs = OMalloc<CIDEDiffs>(this);
	mpAskClaude = OMalloc<CIDEAskClaude>(this);

	//Solution Explorer on the left.
	sPlace.BesideWell(DS_Left, IDE_EXPLORER_WIDTH);
	mpSolutions->CreateExplorerPane(&sPlace);

	//Under the well, tabbed together: what Find Usages (and Find All References) found, what the build wrote, the
	//debugger's view and Claude.
	sPlace.BesideWell(DS_Bottom, IDE_BOTTOM_HEIGHT);
	mpUsages->CreatePane(&sPlace);

	sPlace.WithPane(IDE_USAGES_ID);
	mpBuilding->CreatePane(&sPlace);

	mpDebugging->CreatePane(&sPlace);
	mpAskClaude->CreatePane(&sPlace);
	mpDock->Show(mpUsages->GetTool());

	//The toolbar: the build configuration's configuration and platform, as Visual Studio has them, and Build.
	mpBuildConfigurations->CreateComboBoxes();
	pToolbar = OMalloc<CGridContainer>(pWindow);
	pToolbar->SetDesiredSize(-1, IDE_TOOLBAR_HEIGHT);
	pToolbar->SetAlignment(CSH_Stretch, CSV_Stretch);
	pToolbar->AddComponent(mpBuildConfigurations->GetConfigurationBox(), 0, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpBuildConfigurations->GetPlatformBox(), 1, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpBuilding->CreateBuildButton(), 2, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpDebugging->CreateStartButton(), 3, 0, 1, 1, 1, 1);

	//The branch checked out, at the toolbar's right, as IntelliJ IDEA shows it.
	pToolbar->AddComponent(mpGitStatus->CreateBranchLabel(), 4, 0, 1, 1, 0, 1);
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
	Ptr<CMenuContainer>	pWindowMenu;

	pFile = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpSolutions->AddFileItems(pFile);
	mpDocuments->AddFileItems(pFile);
	mpDiffs->AddFileItems(pFile);
	pFile->AddSeparator();
	mpExitItem = pFile->AddItem("Exit");
	mpExitItem->GetClickEvent()->Add(this, &CIDEContent::MenuItemClicked);

	mpViewMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpViewMenu->GetOpeningEvent()->Add(this, &CIDEContent::ViewMenuOpening);
	mpViewMenu->GetItemClickEvent()->Add(this, &CIDEContent::ViewItemClicked);

	pWindowMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpResetLayoutItem = pWindowMenu->AddItem("Reset Window Layout");
	mpResetLayoutItem->GetClickEvent()->Add(this, &CIDEContent::MenuItemClicked);
	mpDocuments->AddWindowItems(pWindowMenu);

	mpMenuBar = OMalloc<CMenuBar>(mpWindow, mpTheme);
	mpMenuBar->AddMenu("File", pFile);
	mpMenuBar->AddMenu("Edit", mpEdit->CreateEditMenu());
	mpMenuBar->AddMenu("Navigate", mpNavigator->CreateNavigateMenu());
	mpMenuBar->AddMenu("View", mpViewMenu);
	mpMenuBar->AddMenu("Git", mpDiffs->CreateGitMenu());
	mpMenuBar->AddMenu("Debug", mpDebugging->CreateDebugMenu());
	mpMenuBar->AddMenu("Window", pWindowMenu);
	mpMenuBar->SetAlignment(CSH_Stretch, CSV_Stretch);
	return mpMenuBar;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::MenuItemClicked(CMenuItem* pcItem)
{
	if (pcItem == &mpExitItem)
	{
		//As the window's close button: the window stops, and the application saves what it keeps as it ends.  Files
		//with changes are asked about first.
		if (mpCloseGuard->RequestExit())
		{
			mpWindow->Stop();
		}
	}
	else if (pcItem == &mpResetLayoutItem)
	{
		mpDock->ResetLayout();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// View: an item for each tool window, as the dock has them, which shows it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ViewMenuOpening(CMenuContainer* pcMenu)
{
	size			i;
	Ptr<CDockPane>	pPane;

	mpViewMenu->RemoveAllItems();
	for (i = 0; i < mpDock->NumPanes(); i++)
	{
		pPane = mpDock->GetPane(i);
		if (pPane->GetKind() == DPK_Tool)
		{
			mpViewMenu->AddItem(pPane->GetTitle())->SetTag((int32)i);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ViewItemClicked(CMenuItem* pcItem)
{
	if ((pcItem->GetTag() >= 0) && ((size)pcItem->GetTag() < mpDock->NumPanes()))
	{
		mpDock->Activate(mpDock->GetPane((size)pcItem->GetTag()));
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::CodeCommandAction(SComponentInputEvent* psEvent)
{
	if (psEvent->eCommand == CIC_Save)
	{
		mpDocuments->SaveActiveFile();
	}
	else if (psEvent->eCommand == CIC_SaveAll)
	{
		mpDocuments->SaveAllFiles();
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
		mpSolutions->ShowActiveFileInExplorer();
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
		mpBuilding->Build();
	}
	else
	{
		mpDebugging->DebugCommandAction(psEvent->eCommand);
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

	if (!mpDocuments->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	mpDocuments->UpdateEditedTabsNow();
	if (!mcCodeModel.FindDefinition(mpDocuments->GetActiveTab()->GetFileName(), iLine, iColumn, &sLocation))
	{
		ShowStatus("No definition found at the caret.");
		return false;
	}
	bResult = mpDocuments->OpenLocation(&sLocation);
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
// Each window tick: a tab whose text has been left alone for a moment
// after an edit is highlighted and indexed again, from its text, on the
// code model's edits thread.  The window ticks again when the next one
// might be due.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Tick(Ptr<CWindow> pWindow)
{
	size				i;
	Ptr<CIDEEditorTab>	pTab;
	int64				iNow;
	int64				iWait;
	int64				iSoonest;

	mpBuilding->PollBuild(pWindow);
	mpDebugging->PollDebugger(pWindow);
	mpAskClaude->PollClaude(pWindow);
	mpAskClaude->ScrollToEnd();

	//The platform's clock: the window's total time only counts the time it spent updating.
	iNow = GetDebugTickCount();
	iSoonest = mpGitStatus->PollGit(iNow);
	iWait = mpDiffs->PollDiffs(iNow);
	if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
	{
		iSoonest = iWait;
	}
	for (i = 0; i < mpDocuments->NumTabs(); i++)
	{
		pTab = mpDocuments->GetTab(i);
		if (pTab->NeedsChangeBars())
		{
			mpGitStatus->UpdateChangeBars(pTab);
		}
		pTab->ShowModified();
	}
	iWait = mpDocuments->UpdateEditedTabs(iNow);
	if ((iWait >= 0) && ((iSoonest < 0) || (iWait < iSoonest)))
	{
		iSoonest = iWait;
	}
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
	pTab = mpDocuments->FindTab(pcPane);
	pDiff = mpDiffs->FindDiffTab(pcPane);
	if (pTab.IsNull() && pDiff.IsNull())
	{
		return;
	}
	mpDocuments->SetActiveTab(pTab);
	mpDiffs->SetActiveDiff(pDiff);
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

	pTab = mpDocuments->FindTab(pcPane);
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
		mpDiffs->RemoveDiff(pDiff);
		return;
	}

	pTab = mpDocuments->FindTab(pcPane);
	if (pTab.IsNull())
	{
		return;
	}

	if (mpDocuments->GetActiveTab() == pTab)
	{
		mpDocuments->SetActiveTab(NULL);
	}
	mpGitStatus->TabClosing(pTab);
	pTab->GetCodeEditor()->GetChangeBarClickEvent()->Remove(&mpGitStatus);
	pTab->GetCodeEditor()->GetGlyphClickEvent()->Remove(&mpDebugging);
	pTab->GetEditor()->GetUndoEvent()->Remove(&mpEdit);
	mpEdit->FileClosed(pTab->GetFileName());
	if (pTab->IsModified())
	{
		mpDocuments->ReindexDiscarded(pTab);
	}
	mpDocuments->RemoveTab(pTab);
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
	pTab = mpDocuments->OpenFileTab(szFileName, mpSolutions->GetExplorer()->FindFileProject(szFileName), false);
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
	if (!mpSolutions->GetSettingsBaseFile(pszDest))
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

	if (!mbKeepSettings || !mpSolutions->IsSolutionOpen() || !GetLayoutFileName(&szFileName))
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
		if (mpDocuments->GetActiveTab().IsNull() && mpDiffs->GetActiveDiff().IsNull() && (mpDocuments->NumTabs() > 0))
		{
			mpDocuments->SetActiveTab(mpDocuments->GetTab(mpDocuments->NumTabs() - 1));
		}

		//As IntelliJ's, the open editor has the keys, its caret shown.
		if (mpDocuments->GetActiveTab().IsNotNull())
		{
			mpWindow->GetFocus()->SetFocus(mpDocuments->GetActiveTab()->GetEditor());
		}
	}
	return bResult;
}


Ptr<CDockManager> CIDEContent::GetDock(void) { return mpDock; }
Ptr<CDockPane> CIDEContent::GetExplorerTool(void) { return mpSolutions->GetExplorerTool(); }
Ptr<CDockPane> CIDEContent::GetDebugTool(void) { return mpDebugging->GetTool(); }
Ptr<CDockPane> CIDEContent::GetClaudeTool(void) { return mpAskClaude->GetTool(); }


CIDECodeModel* CIDEContent::GetCodeModel(void) { return &mcCodeModel; }
Ptr<CIDEScript> CIDEContent::GetScript(void) { return mpScript; }
CSolutionExplorer* CIDEContent::GetExplorer(void) { return &mpSolutions->GetExplorer(); }


//////////////////////////////////////////////////////////////////////////
//
// The editors' context menu, as IntelliJ's: Go to Source (Control+B, an
// #include's file or a definition), Find Usages (Alt+F7), Rename
// (Shift+F6) and Refactor, each added (and enabled as it opens) by the
// class that does it.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDEContent::CreateEditorMenu(void)
{
	mpEditorMenu = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpNavigator->AddEditorItems(mpEditorMenu);
	mpUsages->AddEditorItems(mpEditorMenu);
	mpEditorMenu->AddSeparator();
	mpRename->AddEditorItems(mpEditorMenu);
	return mpEditorMenu;
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


Ptr<CIDEGitStatus> CIDEContent::GetGitStatus(void) { return mpGitStatus; }


Ptr<CIDEDebugging> CIDEContent::GetDebugging(void) { return mpDebugging; }
Ptr<CIDEConfigurations> CIDEContent::GetBuildConfigurations(void) { return mpBuildConfigurations; }
Ptr<CIDEBuilding> CIDEContent::GetBuilding(void) { return mpBuilding; }
Ptr<CIDESolutions> CIDEContent::GetSolutions(void) { return mpSolutions; }
Ptr<CIDEDocuments> CIDEContent::GetDocuments(void) { return mpDocuments; }


//////////////////////////////////////////////////////////////////////////
//
// What the classes the content holds share.
//
//////////////////////////////////////////////////////////////////////////
CBuildWorkspace* CIDEContent::GetWorkspace(void) { return &mcWorkspace; }
bool CIDEContent::IsKeepingSettings(void) { return mbKeepSettings; }
Ptr<CWindow> CIDEContent::GetWindow(void) { return mpWindow; }
Ptr<CWindowTheme> CIDEContent::GetTheme(void) { return mpTheme; }
SDialogLook* CIDEContent::GetDialogLook(void) { return &msDialogLook; }
Ptr<CFontColours> CIDEContent::GetFontColours(void) { return mpFontColours; }
Ptr<CFontColours> CIDEContent::GetBoldColours(void) { return mpBoldColours; }
Ptr<CFontFamily> CIDEContent::GetCodeFamily(void) { return mpCodeFamily; }
Ptr<CFont> CIDEContent::GetDimFont(void) { return mpDimFont; }
Ptr<CFont> CIDEContent::GetMatchFont(void) { return mpMatchFont; }
Ptr<CImageCel> CIDEContent::GetFolderIcon(void) { return mpFolderIcon; }
Ptr<CImageCel> CIDEContent::GetFileIcon(void) { return mpFileIcon; }
size CIDEContent::NumFileTypeIcons(void) { return maFileIcons.Size(); }
Ptr<CImageCel> CIDEContent::GetFileTypeIcon(size uiType) { return maFileIcons.Get(uiType); }
Ptr<CImageCel> CIDEContent::GetSymbolIcon(size uiType) { return maSymbolIcons.Get(uiType); }
Ptr<CMenuBar> CIDEContent::GetMenuBar(void) { return mpMenuBar; }
Ptr<CMenuContainer> CIDEContent::GetEditorMenu(void) { return mpEditorMenu; }
Ptr<CIDENavigator> CIDEContent::GetNavigator(void) { return mpNavigator; }
Ptr<CIDEEdit> CIDEContent::GetEdit(void) { return mpEdit; }
Ptr<CIDEUsages> CIDEContent::GetUsages(void) { return mpUsages; }
Ptr<CIDERename> CIDEContent::GetRename(void) { return mpRename; }
Ptr<CIDECloseGuard> CIDEContent::GetCloseGuard(void) { return mpCloseGuard; }
