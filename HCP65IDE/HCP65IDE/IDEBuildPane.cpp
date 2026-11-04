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
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEBuildPane.h"


//////////////////////////////////////////////////////////////////////////
//
// The output is in pOutputFamily's fixed width font, as tiles.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pProgressFont, Ptr<CFontFamily> pOutputFamily, ARGB32 sOutputColour)
{
	PreInit();

	mpProgress = OMalloc<CLabel>(pWindow, pProgressFont, "");
	mpProgress->SetAlignment(CSH_Left, CSV_Center);
	mpOutput = OMalloc<CTextArea>(pWindow, pOutputFamily, "");
	mpOutput->SetEditable(false);
	mpOutput->SetTiledText(true);
	mpOutput->SetColour(sOutputColour);
	mpOutput->SetAlignment(CSH_Left, CSV_Top);
	mpPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpPane->SetContent(mpOutput);
	mpPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel = OMalloc<CGridContainer>(pWindow);
	mpPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel->AddComponent(mpProgress, 0, 0, 1, 1, 1, 0);
	mpPanel->AddComponent(mpPane, 0, 1, 1, 1, 1, 1);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Class(void)
{
	M_Pointer(mpPanel);
	M_Pointer(mpProgress);
	M_Pointer(mpOutput);
	M_Pointer(mpPane);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
// The build pane isn't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildPane::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEBuildPane::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// No output and no progress, for a new build.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Clear(void)
{
	mpOutput->SetText("");
	mpProgress->SetText("");
}


//////////////////////////////////////////////////////////////////////////
//
// The lines go after the output there is, together so the text is laid
// out once, and the end of the output is scrolled into view.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::AppendLines(CArrayChars* paszLines)
{
	CChars	szText;
	size	i;

	if (paszLines->NumElements() == 0)
	{
		return;
	}

	szText.Init();
	for (i = 0; i < paszLines->NumElements(); i++)
	{
		if ((i > 0) || (mpOutput->GetText()[0] != '\0'))
		{
			szText.Append('\n');
		}
		szText.Append(paszLines->GetText(i));
	}
	mpOutput->AppendText(szText.Text(), mpOutput->GetColour());
	szText.Kill();
	mpOutput->DocumentEnd();
}


//////////////////////////////////////////////////////////////////////////
//
// How the build is going, such as "Compiling 12 of 40: Main.cpp".
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::SetProgress(const char* szProgress)
{
	mpProgress->SetText(szProgress);
}


Ptr<CComponent> CIDEBuildPane::GetComponent(void) { return mpPanel; }
char* CIDEBuildPane::GetOutput(void) { return mpOutput->GetText(); }
char* CIDEBuildPane::GetProgress(void) { return mpProgress->GetText(); }

