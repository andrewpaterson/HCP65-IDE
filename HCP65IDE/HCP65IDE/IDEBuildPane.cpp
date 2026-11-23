/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEMessageOutput.h"
#include "IDEBuildPane.h"


//A line to colour, as it is appended.
struct SIDEColourSpan
{
	size	uiStart;
	size	uiEnd;
	ARGB32	sColour;
};


typedef CArrayTemplate<SIDEColourSpan>	CArrayIDEColourSpans;


#define IDE_BUILD_PROGRESS_HEIGHT	6


//////////////////////////////////////////////////////////////////////////
//
// The output is in pOutputFamily's fixed width font, as tiles.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFontFamily> pOutputFamily, ARGB32 sOutputColour)
{
	PreInit();

	mpOutput = OMalloc<CTextArea>(pWindow, pTheme, pOutputFamily, "");
	mpOutput->SetEditable(false);
	mpOutput->SetTiledText(true);
	mpOutput->SetColour(sOutputColour);
	mpOutput->SetAlignment(CSH_Left, CSV_Top);
	mpPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpPane->SetContent(mpOutput);
	mpPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	msProgress = pTheme->GetColour(TC_Accent);
	mbMessageColours = false;
	msError = 0;
	msWarning = 0;
	mpProgress = OMalloc<CProgressBar>(pWindow, pTheme->GetColour(TC_Separator), msProgress, IDE_BUILD_PROGRESS_HEIGHT);
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
	U_UInt32(msProgress);
	U_Bool(mbMessageColours);
	U_UInt32(msError);
	U_UInt32(msWarning);
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
	SetProgress(0.0f, msProgress);
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
// The lines appended from now on that are errors (CIDEMessageOutput::
// ParseMessageLine) are in sError, and warnings in sWarning.
//
//////////////////////////////////////////////////////////////////////////
void CIDEBuildPane::SetMessageColours(ARGB32 sError, ARGB32 sWarning)
{
	mbMessageColours = true;
	msError = sError;
	msWarning = sWarning;
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
	CChars					szText;
	CChars					szFile;
	CArrayIDEColourSpans	asSpans;
	SIDEColourSpan*			psSpan;
	size					i;
	size					uiStart;
	size					uiEnd;
	size					uiOutput;
	int32					iLine;
	int32					iColumn;
	EIDEMessageKind			eKind;
	bool					bFollow;

	if (paszLines->NumElements() == 0)
	{
		return;
	}
	bFollow = (mpOutput->GetCaretPosition() == strlen(mpOutput->GetText())) && !mpOutput->GetSelection(&uiStart, &uiEnd);

	uiOutput = strlen(mpOutput->GetText());
	szText.Init();
	asSpans.Init();
	for (i = 0; i < paszLines->NumElements(); i++)
	{
		if ((i > 0) || (uiOutput > 0))
		{
			szText.Append('\n');
		}
		if (mbMessageColours && (paszLines->GetText(i)[0] != '\0'))
		{
			//An error's or a warning's line is coloured once it is there.
			szFile.Init();
			if (CIDEMessageOutput::ParseMessageLine(paszLines->GetText(i), &szFile, &iLine, &iColumn, &eKind) && (eKind != IMK_Note))
			{
				psSpan = asSpans.Add();
				psSpan->uiStart = uiOutput + szText.Length();
				psSpan->uiEnd = psSpan->uiStart + strlen(paszLines->GetText(i));
				psSpan->sColour = (eKind == IMK_Warning) ? msWarning : msError;
			}
			szFile.Kill();
		}
		szText.Append(paszLines->GetText(i));
	}
	mpOutput->AppendText(szText.Text(), mpOutput->GetColour());
	for (i = 0; i < asSpans.NumElements(); i++)
	{
		psSpan = asSpans.Get(i);
		mpOutput->AddSpan(psSpan->uiStart, psSpan->uiEnd - psSpan->uiStart, psSpan->sColour);
	}
	asSpans.Kill();
	szText.Kill();
	if (bFollow)
	{
		mpOutput->GetEditing()->DocumentEnd();
	}
}


Ptr<CComponent> CIDEBuildPane::GetComponent(void) const { return mpPanel; }
Ptr<CProgressBar> CIDEBuildPane::GetProgressBar(void) const { return mpProgress; }
const char* CIDEBuildPane::GetOutput(void) { return mpOutput->GetText(); }
Ptr<CTextArea> CIDEBuildPane::GetOutputArea(void) const { return mpOutput; }
