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
#include "BaseLib/TextFile.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDELocationPreview.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDELocationPreview::Init(CIDEContent* pcContent, Ptr<CCodePreview> pPreview)
{
	PreInit();
	mpcContent = pcContent;
	mpPreview = pPreview;
	muiPreviewChanges = MAX_SIZE;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDELocationPreview::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpPreview);
	U_Size(muiPreviewChanges);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDELocationPreview::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
// It is being let go of, or the IDE is closing: what it asked to be
// highlighted isn't given to it.
//
//////////////////////////////////////////////////////////////////////////
void CIDELocationPreview::Detach(void)
{
	mpcContent->GetDocuments()->ForgetPreviewAsker(this);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDELocationPreview::Save(CObjectWriter* pcFile) { return false; }
bool CIDELocationPreview::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The position in szText of a line and column (both from 0); the end of
// the line if it is shorter than the column, the end of the text if it
// has fewer lines.
//
//////////////////////////////////////////////////////////////////////////
size FindIDETextPosition(const char* szText, size uiLength, int32 iLine, int32 iColumn)
{
	const char*	szNewLine;
	size		uiPosition;
	size		uiLineEnd;

	uiPosition = 0;
	for (; iLine > 0; iLine--)
	{
		szNewLine = (const char*)memchr(&szText[uiPosition], '\n', uiLength - uiPosition);
		if (szNewLine == NULL)
		{
			return uiLength;
		}
		uiPosition = (size)(szNewLine - szText) + 1;
	}
	szNewLine = (const char*)memchr(&szText[uiPosition], '\n', uiLength - uiPosition);
	uiLineEnd = szNewLine ? (size)(szNewLine - szText) : uiLength;
	uiPosition += (iColumn > 0) ? (size)iColumn : 0;
	return (uiPosition < uiLineEnd) ? uiPosition : uiLineEnd;
}


//////////////////////////////////////////////////////////////////////////
//
// The preview shows psLocation: its file's code at its place, the text
// found coloured.  False, and it shows nothing, if the file couldn't be
// read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELocationPreview::ShowLocation(SIDELocation* psLocation)
{
	Ptr<CIDEEditorTab>	pTab;
	CTextFile			cFile;
	const char*			szText;
	size				uiLength;
	size				uiChanges;

	pTab = mpcContent->GetDocuments()->FindTab(psLocation->szFileName.Text());
	uiChanges = pTab.IsNotNull() ? pTab->GetEditor()->GetChangeCount() : MAX_SIZE;
	if (!mpPreview->IsShowing(psLocation->szFileName.Text()) || (uiChanges != muiPreviewChanges))
	{
		cFile.Init();
		szText = NULL;
		if (pTab.IsNotNull())
		{
			szText = pTab->GetEditor()->GetText();
		}
		else if (cFile.Read(psLocation->szFileName.Text()))
		{
			szText = cFile.Text();
		}
		if (szText == NULL)
		{
			cFile.Kill();
			mpPreview->Clear();
			return false;
		}
		mpPreview->SetCode(psLocation->szFileName.Text(), szText);
		muiPreviewChanges = uiChanges;
		mpcContent->GetDocuments()->HighlightPreview(this, psLocation->szFileName.Text(), szText);
		cFile.Kill();
	}

	szText = mpPreview->GetTextArea()->GetText();
	uiLength = strlen(szText);
	mpPreview->ShowPlace(FindIDETextPosition(szText, uiLength, psLocation->iLine, psLocation->iColumn), (psLocation->iLength > 0) ? (size)psLocation->iLength : 0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDELocationPreview::Clear(void)
{
	mpPreview->Clear();
}


//////////////////////////////////////////////////////////////////////////
//
// The edits thread highlighted a file for the preview: if the preview
// still shows it, it is coloured as its editor would be.
//
//////////////////////////////////////////////////////////////////////////
void CIDELocationPreview::PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights)
{
	if (mpPreview->IsShowing(szFileName))
	{
		ApplyIDEHighlights(mpPreview->GetTextArea(), pacHighlights);
	}
}


Ptr<CCodePreview> CIDELocationPreview::GetPreview(void) const { return mpPreview; }

