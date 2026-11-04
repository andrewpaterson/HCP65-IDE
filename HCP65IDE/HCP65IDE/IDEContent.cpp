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
#include "WindowLib/ScrollPane.h"
#include "WindowLib/Label.h"
#include "WindowLib/JewelColours.h"
#include "WindowLib/JewelTheme.h"
#include "WindowLib/JewelImageGenerator.h"
#include "IDEContent.h"


#define IDE_FONT_CHARACTERS		" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
#define IDE_EXPLORER_WIDTH		320
#define IDE_CODE_PIXEL_SIZE		15.0f
#define IDE_BOTTOM_HEIGHT		180
#define IDE_TOOLBAR_HEIGHT		34		//A grid in a grid has no height of its own.


#define IDE_BUILD_POLL			50		//Milliseconds between looks at a running build.


//Go to Definition (F12), Find All References (Shift+F12) and Build (Control+Shift+B) from anywhere in the window.
static EComponentInputCommand	gaeIDECodeCommands[] = { CIC_GoToDefinition, CIC_FindReferences, CIC_Build };


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
// A white font of szFontFilename's printable ASCII characters.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder)
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
	cFactory.Init(&cTrueTypeFont, fPixelSize);
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
//
//////////////////////////////////////////////////////////////////////////
void CIDEActivationListener::Init(CIDEContent* pcContent, void (CIDEContent::*fActivated)(void))
{
	mpcContent = pcContent;
	mfActivated = fActivated;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurationListener::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConfigurationListener::SelectionChanged(CUnknown* pcSource, void* pvContext)
{
	mpcContent->ConfigurationChosen();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMenuListener::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMenuListener::MenuItemClicked(CUnknown* pcSource, void* pvContext)
{
	mpcContent->MenuItemClicked((CMenuItem*)pcSource);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildListener::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildListener::ButtonClicked(CUnknown* pcSource, void* pvContext)
{
	mpcContent->Build();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETabsListener::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETabsListener::TabSelected(CUnknown* pcSource, void* pvContext)
{
	mpcContent->TabSelected(pcSource);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETabsListener::TabRemoved(CUnknown* pcSource, void* pvContext)
{
	mpcContent->TabRemoved(pcSource);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEActivationListener::ItemActivated(CUnknown* pcSource, void* pvContext)
{
	(mpcContent->*mfActivated)();
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
	mcExplorerListener.Init(this, &CIDEContent::ExplorerActivated);
	mcReferencesListener.Init(this, &CIDEContent::ReferenceActivated);
	mcConfigurationsListener.Init(this);
	mcConfigurations.Init();
	muiConfiguration = MAX_SIZE;
	mcBuildListener.Init(this);
	mcMenuListener.Init(this);
	memset(&msDialogLook, 0, sizeof(msDialogLook));
	mszSolutionFile.Init();
	mcBuild.Init();
	mszLibc65.Init();
	mcListings.Init();
	muiListingToShow = MAX_SIZE;
	masReferences.Init();
	maTabs.Init();
	maFileIcons.Init();
	mcTabsListener.Init(this);

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
	U_Unknown(CIDEMenuListener, mcMenuListener);
	M_Pointer(mpFileChooser);
	M_Pointer(mpFolderIcon);
	M_Pointer(mpFileIcon);
	U_String(mszSolutionFile);
	M_Pointer(mpEditors);
	M_Embedded(maTabs);
	M_Pointer(mpActive);
	U_Unknown(CIDETabsListener, mcTabsListener);
	M_Embedded(maFileIcons);
	M_Pointer(mpExplorer);
	U_Unknown(CIDEActivationListener, mcExplorerListener);
	M_Pointer(mpConfigurations);
	M_Pointer(mpPlatforms);
	U_Unknown(CIDEConfigurationListener, mcConfigurationsListener);
	U_Unknown(CBuildConfigurations, mcConfigurations);
	U_Size(muiConfiguration);
	M_Pointer(mpBuildButton);
	U_Unknown(CIDEBuildListener, mcBuildListener);
	U_Unknown(CSolutionBuild, mcBuild);
	U_String(mszLibc65);
	U_Unknown(CCPP65ListingIndex, mcListings);
	U_Size(muiListingToShow);
	M_Pointer(mpBottomTabs);
	M_Pointer(mpReferencesTab);
	M_Pointer(mpBuildTab);
	M_Pointer(mpBuildPane);
	M_Pointer(mpReferences);
	U_Unknown(CIDEActivationListener, mcReferencesListener);
	U_Unknown(CArrayIDELocations, masReferences);
	U_Unknown(CVcxSolution, mcSolution);
	U_Unknown(CIDECodeModel, mcCodeModel);
	U_Bool(mbSolution);
}


//////////////////////////////////////////////////////////////////////////
//
// The widgets, fonts and theme are freed with the content's pointers.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Free(void)
{
	//An object's memory is freed without its destructor running so the listener takes itself off the tree.
	mcExplorerListener.RemoveFromListenerCalls();
	mcReferencesListener.RemoveFromListenerCalls();
	mcConfigurationsListener.RemoveFromListenerCalls();
	mcBuildListener.RemoveFromListenerCalls();
	mcMenuListener.RemoveFromListenerCalls();
	mcTabsListener.RemoveFromListenerCalls();

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
	Ptr<CGridContainer>	pExplorerPanel;
	Ptr<CLabel>			pExplorerTitle;
	Ptr<CScrollPane>	pReferencesPane;
	Ptr<CGridContainer>	pToolbar;
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

	//Solution Explorer: its title over the project's tree.
	mpExplorer = OMalloc<CSolutionExplorer>(pWindow, mpTheme);
	mpExplorer->GetTree()->AddListener(&mcExplorerListener);
	pExplorerPanel = OMalloc<CGridContainer>(pWindow);
	pExplorerPanel->SetDesiredSize(IDE_EXPLORER_WIDTH, -1);
	pExplorerPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	pExplorerTitle = OMalloc<CLabel>(pWindow, mpFontColours->GetFont(JEWEL_ICON), "Solution Explorer");
	pExplorerTitle->SetAlignment(CSH_Left, CSV_Center);
	pExplorerPanel->AddComponent(pExplorerTitle, 0, 0, 1, 1, 1, 0);
	pExplorerPanel->AddComponent(mpExplorer->GetComponent(), 0, 1, 1, 1, 1, 1);

	//The editor: a tab for each open file, each with an icon of its kind.
	mpEditors = OMalloc<CTabbedContainer>(pWindow, mpTheme);
	mpEditors->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpEditors->AddListener(&mcTabsListener);
	for (i = 0; i < IFT_Count; i++)
	{
		maFileIcons.Add(CIDEIcons::CreateFileIcon((EIDEFileType)i, &cFormat));
	}

	//Under the editor, a tab for what Find All References found and one for what the build wrote.
	mpReferences = OMalloc<CTreeContainer>(pWindow, mpTheme);
	mpReferences->SetAlignment(CSH_Stretch, CSV_Top);
	mpReferences->AddListener(&mcReferencesListener);
	pReferencesPane = OMalloc<CScrollPane>(pWindow, mpTheme);
	pReferencesPane->SetContent(mpReferences);
	pReferencesPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpBuildPane = OMalloc<CIDEBuildPane>(pWindow, mpTheme, mpFontColours->GetFont(JEWEL_ICON), mpCodeFamily, JEWEL_TEXT);
	mpBottomTabs = OMalloc<CTabbedContainer>(pWindow, mpTheme);
	mpBottomTabs->SetDesiredSize(-1, IDE_BOTTOM_HEIGHT);
	mpBottomTabs->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpReferencesTab = mpBottomTabs->AddTab("Find All References", pReferencesPane);
	mpBuildTab = mpBottomTabs->AddTab("Build", mpBuildPane->GetComponent());

	//The toolbar: the build configuration's configuration and platform, as Visual Studio has them, and Build.
	mpConfigurations = OMalloc<CComboBox>(pWindow, mpTheme);
	mpConfigurations->SetAlignment(CSH_Left, CSV_Center);
	mpConfigurations->AddListener(&mcConfigurationsListener);
	mpPlatforms = OMalloc<CComboBox>(pWindow, mpTheme);
	mpPlatforms->SetAlignment(CSH_Left, CSV_Center);
	mpPlatforms->AddListener(&mcConfigurationsListener);
	mpBuildButton = OMalloc<CButton>(pWindow, mpTheme, "Build");
	mpBuildButton->SetAlignment(CSH_Left, CSV_Center);
	mpBuildButton->AddListener(&mcBuildListener);
	pToolbar = OMalloc<CGridContainer>(pWindow);
	pToolbar->SetDesiredSize(-1, IDE_TOOLBAR_HEIGHT);
	pToolbar->SetAlignment(CSH_Stretch, CSV_Stretch);
	pToolbar->AddComponent(mpConfigurations, 0, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpPlatforms, 1, 0, 1, 1, 0, 1);
	pToolbar->AddComponent(mpBuildButton, 2, 0, 1, 1, 1, 1);

	pMain = OMalloc<CGridContainer>(pWindow);
	pMain->AddComponent(CreateMenuBar(), 0, 0, 2, 1, 1, 0);
	pMain->AddComponent(pToolbar, 0, 1, 2, 1, 1, 0);
	pMain->AddComponent(pExplorerPanel, 0, 2, 1, 2, 0, 1);
	pMain->AddComponent(mpEditors, 1, 2, 1, 1, 1, 1);
	pMain->AddComponent(mpBottomTabs, 1, 3, 1, 1, 1, 0);
	pMain->AddInputActions(gaeIDECodeCommands, sizeof(gaeIDECodeCommands) / sizeof(EComponentInputCommand), this, &CIDEContent::CodeCommandAction);
	pWindow->SetContainer(pMain);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// File, with Open Project/Solution.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuBar> CIDEContent::CreateMenuBar(void)
{
	Ptr<CMenuContainer>	pFile;

	pFile = OMalloc<CMenuContainer>(mpWindow, mpTheme);
	mpOpenItem = pFile->AddItem("Open Project/Solution...", mpFolderIcon);

	mpMenuBar = OMalloc<CMenuBar>(mpWindow, mpTheme);
	mpMenuBar->AddMenu("File", pFile);
	mpMenuBar->AddListener(&mcMenuListener);
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
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Shows szFileName in its own tab, selected: the tab it is already in if
// it is open.  A file of the solution's project uiProject is highlighted
// as that project builds it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenFile(const char* szFileName, size uiProject)
{
	CTextFile			cFile;
	CFileUtil			cFileUtil;
	CChars				szName;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTabComponent>	pTabComponent;

	pTab = FindTab(szFileName);
	if (pTab.IsNotNull())
	{
		mpEditors->Select(pTab->GetTab());
		return true;
	}

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return gcLogger.Error2(__METHOD__, " Could not read [", szFileName, "].", NULL);
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
	pTabComponent = mpEditors->AddTab(szName.Text(), pTab->GetPane(), maFileIcons.Get(CIDEIcons::GetFileType(szFileName)), true);
	szName.Kill();
	pTab->SetTab(pTabComponent);
	mpEditors->Select(pTabComponent);

	//The first tab is selected as it is added, before it is known as this file's.
	mpActive = pTab;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Closes szFileName's tab.  False if it isn't open.
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
	return mpEditors->RemoveTab(pTab->GetTab());
}


//////////////////////////////////////////////////////////////////////////
//
// Every open file's tab is closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::CloseAllFiles(void)
{
	while (maTabs.Size() > 0)
	{
		//Closing the tab lets its file go (TabRemoved).
		if (!mpEditors->RemoveTab(maTabs.Get(maTabs.Size() - 1)->GetTab()))
		{
			break;
		}
	}
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
	if (psEvent->eCommand == CIC_GoToDefinition)
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
	mpBottomTabs->Select(mpReferencesTab);
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
	mpBottomTabs->Select(mpReferencesTab);
}


//////////////////////////////////////////////////////////////////////////
//
// The Build tab is shown under the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ShowBuildTab(void)
{
	mpBottomTabs->Select(mpBuildTab);
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
	mpWindow->GetFocus()->SetFocus(mpActive->GetEditor());
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

	//The platform's clock: the window's total time only counts the time it spent updating.
	iNow = GetDebugTickCount();
	iSoonest = -1;
	for (i = 0; i < maTabs.Size(); i++)
	{
		pTab = maTabs.Get(i);
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
// The editor's selected tab changed.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::TabSelected(CUnknown* pcTab)
{
	mpActive = FindTab(pcTab);
}


//////////////////////////////////////////////////////////////////////////
//
// A tab was closed: its file is let go.  The tabbed container selects
// another after.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::TabRemoved(CUnknown* pcTab)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = FindTab(pcTab);
	if (pTab.IsNull())
	{
		return;
	}
	if (mpActive == pTab)
	{
		mpActive = NULL;
	}
	maTabs.Remove(pTab);
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
// The file of a tab on the editor's bar, NULL if it isn't one of them.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CIDEEditorTab> CIDEContent::FindTab(CUnknown* pcTab)
{
	size	i;

	for (i = 0; i < maTabs.Size(); i++)
	{
		if (&maTabs.Get(i)->GetTab() == pcTab)
		{
			return maTabs.Get(i);
		}
	}
	return NULL;
}


Ptr<CTextArea> CIDEContent::GetEditor(void) { return mpActive.IsNotNull() ? mpActive->GetEditor() : Ptr<CTextArea>(); }
Ptr<CScrollPane> CIDEContent::GetEditorPane(void) { return mpActive.IsNotNull() ? mpActive->GetPane() : Ptr<CScrollPane>(); }
Ptr<CIDEEditorTab> CIDEContent::GetActiveTab(void) { return mpActive; }
Ptr<CTabbedContainer> CIDEContent::GetEditors(void) { return mpEditors; }
Ptr<CTreeContainer> CIDEContent::GetReferences(void) { return mpReferences; }
Ptr<CIDEBuildPane> CIDEContent::GetBuildPane(void) { return mpBuildPane; }
bool CIDEContent::IsBuilding(void) { return mcBuild.IsRunning(); }
CCPP65ListingIndex* CIDEContent::GetListings(void) { return &mcListings; }  //The last HCP65 build's, for showing a line's assembly.
CIDECodeModel* CIDEContent::GetCodeModel(void) { return &mcCodeModel; }
CSolutionExplorer* CIDEContent::GetExplorer(void) { return &mpExplorer; }

