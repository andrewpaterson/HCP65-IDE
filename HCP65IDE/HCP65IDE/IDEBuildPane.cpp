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
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "WindowLib/JewelColours.h"
#include "IDEBuildPane.h"


#define IDE_BUILD_PROGRESS_HEIGHT	6


//////////////////////////////////////////////////////////////////////////
//
// The output is in pOutputFamily's fixed width font, as tiles.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFontFamily> pOutputFamily, ARGB32 sOutputColour)
{
	PreInit();

	mpOutput = OMalloc<CTextArea>(pWindow, pOutputFamily, "");
	mpOutput->SetEditable(false);
	mpOutput->SetTiledText(true);
	mpOutput->SetColour(sOutputColour);
	mpOutput->SetAlignment(CSH_Left, CSV_Top);
	mpPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpPane->SetContent(mpOutput);
	mpPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpProgress = OMalloc<CProgressBar>(pWindow, JEWEL_SEPARATOR, JEWEL_BLUE, IDE_BUILD_PROGRESS_HEIGHT);
	mpProgress->SetAlignment(CSH_Stretch, CSV_Bottom);
	mpPanel = OMalloc<CGridContainer>(pWindow);
	mpPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel->AddComponent(mpPane, 0, 0, 1, 1, 1, 1);
	mpPanel->AddComponent(mpProgress, 0, 1, 1, 1, 1, 0);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Class(void)
{
	M_Pointer(mpPanel);
	M_Pointer(mpOutput);
	M_Pointer(mpProgress);
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
// No output and nothing done, for a new build.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Clear(void)
{
	mpOutput->SetText("");
	SetProgress(0.0f, JEWEL_BLUE);
}


//////////////////////////////////////////////////////////////////////////
//
// How far the build has got, 0 to 1, in the colour that says how it is
// going.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::SetProgress(float fFraction, ARGB32 sColour)
{
	mpProgress->SetFraction(fFraction);
	mpProgress->SetBarColour(sColour);
}


//////////////////////////////////////////////////////////////////////////
//
// The lines go after the output there is, together so the text is laid
// out once.  A caret at the end of the output (with nothing selected)
// follows it, scrolling the end into view; one put anywhere else stays
// where it is, so the output can be read while the build goes on.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::AppendLines(CArrayChars* paszLines)
{
	CChars	szText;
	size	i;
	size	uiStart;
	size	uiEnd;
	bool	bFollow;

	if (paszLines->NumElements() == 0)
	{
		return;
	}
	bFollow = (mpOutput->GetCaretPosition() == strlen(mpOutput->GetText())) && !mpOutput->GetSelection(&uiStart, &uiEnd);

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
	if (bFollow)
	{
		mpOutput->DocumentEnd();
	}
}


Ptr<CComponent> CIDEBuildPane::GetComponent(void) { return mpPanel; }
Ptr<CProgressBar> CIDEBuildPane::GetProgressBar(void) { return mpProgress; }
char* CIDEBuildPane::GetOutput(void) { return mpOutput->GetText(); }

