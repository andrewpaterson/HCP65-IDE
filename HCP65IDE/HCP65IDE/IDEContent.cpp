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
#include "IDEContent.h"


#define IDE_FONT_CHARACTERS		" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
#define IDE_EXPLORER_WIDTH		320
#define IDE_CODE_PIXEL_SIZE		15.0f
#define IDE_REFERENCES_HEIGHT	180
#define IDE_TOOLBAR_HEIGHT		34		//A grid in a grid has no height of its own.
#define IDE_CARET_LINES_ABOVE	4		//How many lines show above a line jumped to.
#define IDE_RECOLOUR_DELAY		400		//Milliseconds after the last edit before the text is highlighted again.


//Go to Definition (F12) and Find All References (Shift+F12) from anywhere in the window.
static EComponentInputCommand	gaeIDECodeCommands[] = { CIC_GoToDefinition, CIC_FindReferences };


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
	masReferences.Init();
	mszFileName.Init();
	muiProject = SOLUTION_EXPLORER_NO_PROJECT;
	muiSeenChanges = 0;
	miChangedTime = 0;
	mbRecolour = false;
	mbEdited = false;

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
	M_Pointer(mpEditor);
	M_Pointer(mpEditorPane);
	M_Pointer(mpExplorer);
	U_Unknown(CIDEActivationListener, mcExplorerListener);
	M_Pointer(mpConfigurations);
	U_Unknown(CIDEConfigurationListener, mcConfigurationsListener);
	M_Pointer(mpReferences);
	U_Unknown(CIDEActivationListener, mcReferencesListener);
	U_Unknown(CArrayIDELocations, masReferences);
	U_String(mszFileName);
	U_Data(size, muiProject);
	U_Data(size, muiSeenChanges);
	U_Data(int64, miChangedTime);
	U_Bool(mbRecolour);
	U_Bool(mbEdited);
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
	mcCodeModel.Kill();
	mcSolution.Kill();
	KillIDELocations(&masReferences);
	mszFileName.Kill();
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
	Ptr<CGridContainer>	pReferencesPanel;
	Ptr<CScrollPane>	pReferencesPane;
	Ptr<CLabel>			pReferencesTitle;
	Ptr<CGridContainer>	pToolbar;

	mpWindow = pWindow;

	mpWhite = GenerateIDEFont(szUIFont, "Inter 13px", 13.0f, pWindow->GetColourOrder());
	if (mpWhite.IsNull())
	{
		return false;
	}
	mpFontColours = OMalloc<CFontColours>(mpWhite);
	mpTheme = CJewelTheme::CreateDarkTheme(pWindow, mpFontColours->GetFont(JEWEL_TEXT));

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

	//The editor: a fixed width font as tiles.
	mpEditor = OMalloc<CTextArea>(pWindow, mpCodeFamily, "");
	mpEditor->SetTiledText(true);
	mpEditor->SetColour(JEWEL_TEXT);
	mpEditor->SetAlignment(CSH_Left, CSV_Top);
	mpEditorPane = OMalloc<CScrollPane>(pWindow, mpTheme);
	mpEditorPane->SetContent(mpEditor);
	mpEditorPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	//Find All References: its title over what it found, under the editor.
	mpReferences = OMalloc<CTreeContainer>(pWindow, mpTheme);
	mpReferences->SetAlignment(CSH_Stretch, CSV_Top);
	mpReferences->AddListener(&mcReferencesListener);
	pReferencesPane = OMalloc<CScrollPane>(pWindow, mpTheme);
	pReferencesPane->SetContent(mpReferences);
	pReferencesPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	pReferencesPanel = OMalloc<CGridContainer>(pWindow);
	pReferencesPanel->SetDesiredSize(-1, IDE_REFERENCES_HEIGHT);
	pReferencesPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	pReferencesTitle = OMalloc<CLabel>(pWindow, mpFontColours->GetFont(JEWEL_ICON), "Find All References");
	pReferencesTitle->SetAlignment(CSH_Left, CSV_Center);
	pReferencesPanel->AddComponent(pReferencesTitle, 0, 0, 1, 1, 1, 0);
	pReferencesPanel->AddComponent(pReferencesPane, 0, 1, 1, 1, 1, 1);

	//The toolbar: the solution configuration.
	mpConfigurations = OMalloc<CComboBox>(pWindow, mpTheme);
	mpConfigurations->SetAlignment(CSH_Left, CSV_Center);
	mpConfigurations->AddListener(&mcConfigurationsListener);
	pToolbar = OMalloc<CGridContainer>(pWindow);
	pToolbar->SetDesiredSize(-1, IDE_TOOLBAR_HEIGHT);
	pToolbar->SetAlignment(CSH_Stretch, CSV_Stretch);
	pToolbar->AddComponent(mpConfigurations, 0, 0, 1, 1, 1, 1);

	pMain = OMalloc<CGridContainer>(pWindow);
	pMain->AddComponent(pToolbar, 0, 0, 2, 1, 1, 0);
	pMain->AddComponent(pExplorerPanel, 0, 1, 1, 2, 0, 1);
	pMain->AddComponent(mpEditorPane, 1, 1, 1, 1, 1, 1);
	pMain->AddComponent(pReferencesPanel, 1, 2, 1, 1, 1, 0);
	pMain->AddInputActions(gaeIDECodeCommands, sizeof(gaeIDECodeCommands) / sizeof(EComponentInputCommand), this, &CIDEContent::CodeCommandAction);
	pWindow->SetContainer(pMain);
	return true;
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
	size	i;

	szName.Init(szFileName);
	bSolution = szName.EndsWithIgnoreCase(".sln");
	szName.Kill();

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
	mpConfigurations->RemoveAllItems();
	for (i = 0; i < mcSolution.NumConfigurations(); i++)
	{
		mpConfigurations->AddItem(mcSolution.GetConfiguration(i));
	}
	if (mcSolution.NumConfigurations() > 0)
	{
		mpConfigurations->Select(0);
	}
	mcCodeModel.StartIndexing();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Shows szFileName in the editor.  A file of the solution's project
// uiProject is highlighted as that project builds it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenFile(const char* szFileName, size uiProject)
{
	CTextFile	cFile;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return gcLogger.Error2(__METHOD__, " Could not read [", szFileName, "].", NULL);
	}
	mpEditor->SetText(cFile.Text());
	mszFileName.Set(szFileName);
	muiProject = uiProject;
	mbEdited = false;
	mbRecolour = false;
	if (uiProject != SOLUTION_EXPLORER_NO_PROJECT)
	{
		HighlightFile(szFileName, cFile.Text(), uiProject, false);
	}
	muiSeenChanges = mpEditor->GetChangeCount();
	cFile.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Colours the editor's text (szText, szFileName's contents, or what it
// has been edited to if bEdited) by what the parser found each part of it
// to be.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::HighlightFile(const char* szFileName, const char* szText, size uiProject, bool bEdited)
{
	CArrayHighlights	acHighlights;
	CArrayInt			aiLineStarts;
	CChars				szErrors;
	SCPPHighlight*		psHighlight;
	size				i;
	size				uiStart;
	const char*			szPosition;
	int					iPass;

	acHighlights.Init();
	szErrors.Init();
	if (mcCodeModel.Highlight(uiProject, szFileName, bEdited ? szText : NULL, &acHighlights, &szErrors))
	{
		aiLineStarts.Init();
		aiLineStarts.Add(0);
		for (szPosition = szText; *szPosition != '\0'; szPosition++)
		{
			if (*szPosition == '\n')
			{
				aiLineStarts.Add((int)(szPosition - szText + 1));
			}
		}

		//Code skipped by conditional compilation is dimmed whatever is in it, comments too, so it goes last.
		for (iPass = 0; iPass < 2; iPass++)
		{
			for (i = 0; i < acHighlights.NumElements(); i++)
			{
				psHighlight = acHighlights.Get(i);
				if (((psHighlight->uiKind == CPPH_Inactive) == (iPass == 1)) && (psHighlight->iLine >= 0) && ((size)psHighlight->iLine < aiLineStarts.NumElements()))
				{
					uiStart = (size)aiLineStarts.GetValue((size)psHighlight->iLine) + (size)psHighlight->iColumn;
					mpEditor->AddSpan(uiStart, (size)psHighlight->iLength, GetHighlightColour((ECPPHighlight)psHighlight->uiKind));
				}
			}
		}
		aiLineStarts.Kill();
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
	if (!mcCodeModel.FindDefinition(mszFileName.Text(), iLine, iColumn, &sLocation))
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
	if (!mcCodeModel.FindReferences(mszFileName.Text(), iLine, iColumn, &masReferences, &szKey))
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
// The file the location is in, with the caret on it.  A file that isn't
// one of the solution's projects' items is highlighted as part of the
// project of the file it was found from.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenLocation(SIDELocation* psLocation)
{
	size	uiProject;

	if (!mszFileName.Equals(psLocation->szFileName.Text()))
	{
		uiProject = mcCodeModel.FindProjectOfFile(psLocation->szFileName.Text());
		if (uiProject == MAX_SIZE)
		{
			uiProject = muiProject;
		}
		if (!OpenFile(psLocation->szFileName.Text(), uiProject))
		{
			return false;
		}
	}
	if (!SetCaretPlace(psLocation->iLine, psLocation->iColumn))
	{
		return false;
	}
	mpWindow->GetFocus()->SetFocus(mpEditor);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The caret's line and column (0 based, the column in bytes) in the
// editor's text.  False if no file is open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::GetCaretPlace(int* piLine, int* piColumn)
{
	char*	szText;
	size	uiCaret;
	size	i;
	size	uiLineStart;
	int		iLine;

	if (mszFileName.Empty())
	{
		return false;
	}
	szText = mpEditor->GetText();
	uiCaret = mpEditor->GetCaretPosition();
	iLine = 0;
	uiLineStart = 0;
	for (i = 0; (i < uiCaret) && (szText[i] != '\0'); i++)
	{
		if (szText[i] == '\n')
		{
			iLine++;
			uiLineStart = i + 1;
		}
	}
	*piLine = iLine;
	*piColumn = (int)(uiCaret - uiLineStart);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Puts the caret at a line and column (0 based, the column in bytes)
// and scrolls the editor to show it.  False if the file hasn't that line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SetCaretPlace(int iLine, int iColumn)
{
	char*		szText;
	size		i;
	int			iAtLine;
	CInt32Rect	cCaret;
	int32		iView;
	int32		x;
	int32		y;

	szText = mpEditor->GetText();
	iAtLine = 0;
	for (i = 0; (iAtLine < iLine) && (szText[i] != '\0'); i++)
	{
		if (szText[i] == '\n')
		{
			iAtLine++;
		}
	}
	if (iAtLine != iLine)
	{
		return false;
	}

	//Laid out so the caret has a place: its line a few lines from the top, and the text only scrolled sideways if the
	//caret is past the right of the view (a window not shown yet has no width).
	mpWindow->Layout();
	mpEditor->SetCaretPosition(i + (size)iColumn);
	mpWindow->Layout();
	mpEditor->GetCaretBounds(&cCaret);
	iView = mpEditorPane->GetActualSize().x;
	x = ((iView <= 0) || (cCaret.GetRight() < iView)) ? 0 : cCaret.GetRight() - iView / 2;
	y = cCaret.GetTop() - IDE_CARET_LINES_ABOVE * cCaret.GetHeight();
	mpEditorPane->SetScrollPosition((x > 0) ? x : 0, (y > 0) ? y : 0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration chosen in the toolbar.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ConfigurationChosen(void)
{
	char*	szConfiguration;

	szConfiguration = mpConfigurations->GetSelectedText();
	if (szConfiguration && (StringCompare(szConfiguration, mcCodeModel.GetConfiguration()) != 0))
	{
		SetConfiguration(szConfiguration);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The code is parsed as the solution configuration builds it: the
// solution is indexed again and the open file is highlighted again.
// False if the solution hasn't the configuration.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::SetConfiguration(const char* szConfiguration)
{
	size	i;
	CChars	szText;

	for (i = 0; i < mcSolution.NumConfigurations(); i++)
	{
		if (StringCompare(mcSolution.GetConfiguration(i), szConfiguration) == 0)
		{
			break;
		}
	}
	if (i == mcSolution.NumConfigurations())
	{
		return false;
	}
	if ((int32)i != mpConfigurations->GetSelected())
	{
		mpConfigurations->Select((int32)i);
	}

	mcCodeModel.SetConfiguration(szConfiguration);
	mcCodeModel.StartIndexing();
	if (!mszFileName.Empty() && (muiProject != SOLUTION_EXPLORER_NO_PROJECT))
	{
		szText.Init(mpEditor->GetText());
		mpEditor->ClearSpans();
		HighlightFile(mszFileName.Text(), szText.Text(), muiProject, mbEdited);
		szText.Kill();
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: once the editor's text has been left alone for a
// moment after an edit it is highlighted again, from the editor's text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Tick(Ptr<CWindow> pWindow)
{
	size	uiChanges;
	CChars	szText;
	int64	iNow;

	//The platform's clock: the window's total time only counts the time it spent updating.
	iNow = GetDebugTickCount();
	uiChanges = mpEditor->GetChangeCount();
	if (uiChanges != muiSeenChanges)
	{
		muiSeenChanges = uiChanges;
		miChangedTime = iNow;
		mbRecolour = true;
		mbEdited = true;
	}
	if (!mbRecolour || mszFileName.Empty() || (muiProject == SOLUTION_EXPLORER_NO_PROJECT))
	{
		return;
	}

	if (iNow - miChangedTime < IDE_RECOLOUR_DELAY)
	{
		pWindow->RequestTick((uint32)(IDE_RECOLOUR_DELAY - (iNow - miChangedTime)));
		return;
	}
	mbRecolour = false;
	szText.Init(mpEditor->GetText());
	mpEditor->ClearSpans();
	HighlightFile(mszFileName.Text(), szText.Text(), muiProject, true);
	szText.Kill();
}


Ptr<CTextArea> CIDEContent::GetEditor(void) { return mpEditor; }
Ptr<CScrollPane> CIDEContent::GetEditorPane(void) { return mpEditorPane; }
Ptr<CTreeContainer> CIDEContent::GetReferences(void) { return mpReferences; }
CIDECodeModel* CIDEContent::GetCodeModel(void) { return &mcCodeModel; }
CSolutionExplorer* CIDEContent::GetExplorer(void) { return &mpExplorer; }

