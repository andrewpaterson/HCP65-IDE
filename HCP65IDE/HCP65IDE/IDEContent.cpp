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
bool CIDEContent::Init(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont)
{
	Ptr<CGridContainer>	pMain;
	Ptr<CGridContainer>	pExplorerPanel;
	Ptr<CLabel>			pExplorerTitle;
	Ptr<CScrollPane>	pEditorPane;

	mpWindow = pWindow;
	mbProject = false;
	mcProject.Init();

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
	mcExplorer.Init(pWindow, mpTheme);
	mcExplorerListener.Init(this);
	mcExplorer.GetTree()->AddListener(&mcExplorerListener);
	pExplorerPanel = OMalloc<CGridContainer>(pWindow);
	pExplorerPanel->SetDesiredSize(IDE_EXPLORER_WIDTH, -1);
	pExplorerPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	pExplorerTitle = OMalloc<CLabel>(pWindow, mpFontColours->GetFont(JEWEL_ICON), "Solution Explorer");
	pExplorerTitle->SetAlignment(CSH_Left, CSV_Center);
	pExplorerPanel->AddComponent(pExplorerTitle, 0, 0, 1, 1, 1, 0);
	pExplorerPanel->AddComponent(mcExplorer.GetComponent(), 0, 1, 1, 1, 1, 1);

	//The editor: a fixed width font as tiles.
	mpEditor = OMalloc<CTextArea>(pWindow, mpCodeFamily, "");
	mpEditor->SetTiledText(true);
	mpEditor->SetColour(JEWEL_TEXT);
	mpEditor->SetAlignment(CSH_Left, CSV_Top);
	pEditorPane = OMalloc<CScrollPane>(pWindow, mpTheme);
	pEditorPane->SetContent(mpEditor);
	pEditorPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	pMain = OMalloc<CGridContainer>(pWindow);
	pMain->AddComponent(pExplorerPanel, 0, 0, 1, 1, 0, 1);
	pMain->AddComponent(pEditorPane, 1, 0, 1, 1, 1, 1);
	pWindow->SetContainer(pMain);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::Kill(void)
{
	if (mcExplorer.GetTree().IsNotNull())
	{
		mcExplorer.GetTree()->RemoveListener(&mcExplorerListener);
	}
	mcExplorer.Kill();
	mcProject.Kill();
	mpEditor = NULL;
	mpCodeFamily = NULL;
	mpTheme = NULL;
	mpFontColours = NULL;
	mpWhite = NULL;
	mpWindow = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads a .vcxproj (and its .filters) and shows it in Solution Explorer.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenProject(const char* szProjectFile)
{
	mcProject.Kill();
	mcProject.Init();
	mbProject = mcProject.Read(szProjectFile);
	if (!mbProject)
	{
		return false;
	}
	mcExplorer.ShowProject(&mcProject);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Shows szFileName in the editor.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContent::OpenFile(const char* szFileName)
{
	CTextFile	cFile;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return gcLogger.Error2(__METHOD__, " Could not read [", szFileName, "].", NULL);
	}
	mpEditor->SetText(cFile.Text());
	cFile.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A file opened in Solution Explorer is shown in the editor.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContent::ExplorerActivated(void)
{
	CVcxItem*	pcItem;
	CChars		szPath;

	pcItem = mcExplorer.GetSelectedItem();
	if (pcItem == NULL)
	{
		return;
	}
	szPath.Init();
	mcProject.GetFullPath(pcItem, &szPath);
	OpenFile(szPath.Text());
	szPath.Kill();
}


Ptr<CTextArea> CIDEContent::GetEditor(void) { return mpEditor; }
CSolutionExplorer* CIDEContent::GetExplorer(void) { return &mcExplorer; }

