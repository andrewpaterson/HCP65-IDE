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
#include "BaseLib/Logger.h"
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
void CSolutionExplorerListener::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSolutionExplorerListener::ItemActivated(CUnknown* pcSource, void* pvContext)
{
	mpcContent->ExplorerActivated();
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
	mcExplorerListener.Init(this);

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
	U_Unknown(CSolutionExplorerListener, mcExplorerListener);
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
	mcCodeModel.Kill();
	mcSolution.Kill();
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

	pMain = OMalloc<CGridContainer>(pWindow);
	pMain->AddComponent(pExplorerPanel, 0, 0, 1, 1, 0, 1);
	pMain->AddComponent(mpEditorPane, 1, 0, 1, 1, 1, 1);
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
	if (uiProject != SOLUTION_EXPLORER_NO_PROJECT)
	{
		HighlightFile(szFileName, cFile.Text(), uiProject);
	}
	cFile.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Colours the editor's text (szText, szFileName's contents) by what the
// parser found each part of it to be.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::HighlightFile(const char* szFileName, const char* szText, size uiProject)
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
	if (mcCodeModel.Highlight(uiProject, szFileName, &acHighlights, &szErrors))
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


Ptr<CTextArea> CIDEContent::GetEditor(void) { return mpEditor; }
Ptr<CScrollPane> CIDEContent::GetEditorPane(void) { return mpEditorPane; }
CSolutionExplorer* CIDEContent::GetExplorer(void) { return &mpExplorer; }

