/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela GitLib

Codaphela GitLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela GitLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela GitLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "GitLineChanges.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitLineChanges::Init(void)
{
	masChanges.Init();
	mcDiff.Init("Changes");
	mcProducer.Init(TDW_Default, TDH_Lines);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitLineChanges::Kill(void)
{
	mcProducer.Kill();
	mcDiff.Kill();
	masChanges.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Replaces the changes with those from szBase (the last commit's text) to
// szText (the text now).  Neither needs to end with a '\0'.
//
//////////////////////////////////////////////////////////////////////////
void CGitLineChanges::Compare(const char* szBase, size uiBaseLength, const char* szText, size uiTextLength)
{
	lsize			uiBlock;
	STextDiffBlock*	psBlock;
	SGitLineChange*	psChange;

	masChanges.ReInit();
	mcDiff.GetContent1()->Set("Base", szBase, uiBaseLength);
	mcDiff.GetContent2()->Set("Text", szText, uiTextLength);
	mcProducer.Produce(&mcDiff);

	for (uiBlock = 0; uiBlock < mcDiff.NumBlocks(); uiBlock++)
	{
		psBlock = mcDiff.GetBlock(uiBlock);
		psChange = masChanges.Add();
		switch (psBlock->GetType())
		{
		case TDBT_Inserted:
			psChange->eType = GLC_Added;
			break;
		case TDBT_Deleted:
			psChange->eType = GLC_Removed;
			break;
		default:
			psChange->eType = GLC_Modified;
			break;
		}
		psChange->uiStartLine = (size)psBlock->uiStartLine2;
		psChange->uiEndLine = (size)psBlock->uiEndLine2;
		psChange->uiBaseStartLine = (size)psBlock->uiStartLine1;
		psChange->uiBaseEndLine = (size)psBlock->uiEndLine1;
	}

	//The texts aren't needed again.
	mcDiff.GetContent1()->Set("Base", "");
	mcDiff.GetContent2()->Set("Text", "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CGitLineChanges::Clear(void)
{
	masChanges.ReInit();
}


size CGitLineChanges::NumChanges(void) { return masChanges.NumElements(); }
SGitLineChange* CGitLineChanges::GetChange(size uiIndex) { return masChanges.SafeGet(uiIndex); }

