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
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "IDEContent.h"
#include "IDERefactorUnit.h"
#include "IDEExpressionTyper.h"


//A file's text from the disk, for the edits thread, which mustn't ask the editors.
class CIDEDiskTexts : public CIDEFileTexts
{
public:
	bool GetFileText(const char* szFileName, CTextFile* pcFile, const char** pszText, size* puiLength) override
	{
		if (!pcFile->Read(szFileName))
		{
			return false;
		}
		*pszText = pcFile->Text();
		*puiLength = (size)pcFile->Length();
		return true;
	}
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETypeQuestion::Init(CIDECodeModel* pcInModel)
{
	pcModel = pcInModel;
	uiNumber = 0;
	szFileName.Init();
	szBefore.Init();
	szExpression.Init();
	szAfter.Init();
	bArrow = false;
	bHeader = false;
	szHeaderText.Init();
	szUnit.Init();
	szUnitText.Init();
	bUnitText = false;
	uiJob = 0;
	szKey.Init();
}


void SIDETypeQuestion::Kill(void)
{
	szKey.Kill();
	szUnitText.Kill();
	szUnit.Kill();
	szHeaderText.Kill();
	szAfter.Kill();
	szExpression.Kill();
	szBefore.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpressionTyper::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
	mpsWaiting = NULL;
	mpsRunning = NULL;
	mpsAnswered = NULL;
	muiAsked = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The edits thread finishes what it has of this first.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpressionTyper::Kill(void)
{
	if (mpsRunning)
	{
		mpcContent->GetCodeModel()->WaitForEditsThread();
	}
	Free(&mpsRunning);
	Free(&mpsWaiting);
	Free(&mpsAnswered);
}


void CIDEExpressionTyper::Free(SIDETypeQuestion** ppsQuestion)
{
	if (*ppsQuestion)
	{
		(*ppsQuestion)->Kill();
		free(*ppsQuestion);
		*ppsQuestion = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What is the expression from uiStart to uiEnd of pTab's text, with "->"
// after it (bArrow) or "."?  uiAfter is where the text goes on after what
// is being written after the expression.  The question's number, which
// GetAnswered is once Poll says it is answered; 0 if it can't be asked: a
// file that isn't in a project.  Any question before it is dropped.
//
//////////////////////////////////////////////////////////////////////////
size CIDEExpressionTyper::Ask(Ptr<CIDEEditorTab> pTab, size uiStart, size uiEnd, size uiAfter, bool bArrow)
{
	Ptr<CIDEEditorTab>	pUnitTab;
	SIDETypeQuestion*	psQuestion;
	const char*			szText;
	size				uiUnitProject;

	Free(&mpsWaiting);
	Free(&mpsAnswered);
	if (mpcContent->GetCodeModel()->FindProjectOfFile(pTab->GetFileName()) == MAX_SIZE)
	{
		return 0;
	}
	szText = pTab->GetEditor()->GetText();
	psQuestion = (SIDETypeQuestion*)malloc(sizeof(SIDETypeQuestion));
	psQuestion->Init(mpcContent->GetCodeModel());
	muiAsked++;
	psQuestion->uiNumber = muiAsked;
	psQuestion->szFileName.Set(pTab->GetFileName());
	psQuestion->szBefore.Append(szText, uiStart);
	psQuestion->szExpression.Append(&szText[uiStart], uiEnd - uiStart);
	psQuestion->szAfter.Append(&szText[uiAfter]);
	psQuestion->bArrow = bArrow;

	//A header is parsed in a unit that includes it: that unit's text as its editor has it, if it is open.
	if ((CIDEIcons::GetFileType(pTab->GetFileName()) == IFT_Header) && mpcContent->GetCodeModel()->GetIncluder(pTab->GetFileName(), &uiUnitProject, &psQuestion->szUnit) &&
		(StringInsensitiveCompare(psQuestion->szUnit.Text(), pTab->GetFileName()) != 0))
	{
		psQuestion->bHeader = true;
		psQuestion->szHeaderText.Set(szText);
		pUnitTab = mpcContent->GetDocuments()->FindTab(psQuestion->szUnit.Text());
		if (pUnitTab.IsNotNull())
		{
			psQuestion->szUnitText.Set(pUnitTab->GetEditor()->GetText());
			psQuestion->bUnitText = true;
		}
	}
	mpsWaiting = psQuestion;
	return muiAsked;
}


//////////////////////////////////////////////////////////////////////////
//
// Each tick: a question the edits thread has finished is the answer if
// it is the last asked, and one that is waiting is given to the edits
// thread if it is free.  True once the last question asked is answered.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExpressionTyper::Poll(void)
{
	if (mpsRunning && mpcContent->GetCodeModel()->IsEditsJobDone(mpsRunning->uiJob))
	{
		if (mpsRunning->uiNumber == muiAsked)
		{
			Free(&mpsAnswered);
			mpsAnswered = mpsRunning;
			mpsRunning = NULL;
		}
		else
		{
			Free(&mpsRunning);
		}
	}
	if (mpsWaiting && (mpsRunning == NULL))
	{
		mpsWaiting->uiJob = mpcContent->GetCodeModel()->RunEditsJob(&CIDEExpressionTyper::Job, mpsWaiting);
		if (mpsWaiting->uiJob != 0)
		{
			mpsRunning = mpsWaiting;
			mpsWaiting = NULL;
		}
	}
	return mpsAnswered != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// No answer is wanted any more: what is waiting is dropped, and what the
// edits thread has will be when it has finished.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpressionTyper::Forget(void)
{
	Free(&mpsWaiting);
	Free(&mpsAnswered);
	muiAsked++;
}


bool CIDEExpressionTyper::IsAsking(void) const { return (mpsWaiting != NULL) || ((mpsRunning != NULL) && (mpsRunning->uiNumber == muiAsked)); }
size CIDEExpressionTyper::GetAnswered(void) const { return mpsAnswered ? mpsAnswered->uiNumber : 0; }
const char* CIDEExpressionTyper::GetKey(void) const { return mpsAnswered ? mpsAnswered->szKey.Text() : ""; }


//////////////////////////////////////////////////////////////////////////
//
// The file checked with szGroup, an expression in brackets, where the
// question's expression was, and the group's type: the key of the class
// or enum it is of, and how many levels of pointer to it.  False if the
// file can't be parsed or the group has no such type.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExpressionTyper::TypeGroup(SIDETypeQuestion* psQuestion, const char* szGroup, CChars* pszKey, int32* piLevels)
{
	CIDERefactorUnit	cUnit;
	CIDEDiskTexts		cDisk;
	CIDETrialTexts		cTexts;
	CCPPSyntax*			pcNode;
	CCPPSyntax*			pcGroup;
	SCPPSyntaxRange		sRange;
	CChars				szText;
	const char*			sz;
	size				uiGroupEnd;
	size				i;
	int32				iLine;
	int32				iColumn;
	int32				iEndLine;
	int32				iEndColumn;
	bool				bResult;

	//The text as it would be with the group and nothing being written after it; a statement that ends there is ended.
	szText.Init(psQuestion->szBefore);
	szText.Append(szGroup);
	uiGroupEnd = szText.Length();
	for (sz = psQuestion->szAfter.Text(); (*sz == ' ') || (*sz == '\t') || (*sz == '\r'); sz++)
	{
	}
	if ((*sz == '\n') || (*sz == '\0') || (*sz == '}'))
	{
		szText.Append(';');
	}
	szText.Append(psQuestion->szAfter);

	//Where the group starts and ends, as the syntax tree says places: lines and columns from 0, the end exclusive.
	iLine = 0;
	iColumn = 0;
	iEndLine = 0;
	iEndColumn = 0;
	for (i = 0; i < uiGroupEnd; i++)
	{
		if (i == psQuestion->szBefore.Length())
		{
			iLine = iEndLine;
			iColumn = iEndColumn;
		}
		if (szText.Text()[i] == '\n')
		{
			iEndLine++;
			iEndColumn = 0;
		}
		else
		{
			iEndColumn++;
		}
	}

	cTexts.mpcTexts = &cDisk;
	cTexts.mszFileName = psQuestion->szFileName.Text();
	cTexts.mszText = szText.Text();
	cTexts.mszUnit = psQuestion->bHeader ? psQuestion->szUnit.Text() : NULL;
	cTexts.mszUnitText = psQuestion->bUnitText ? psQuestion->szUnitText.Text() : NULL;
	bResult = false;
	cUnit.Init();
	if (cUnit.Parse(psQuestion->pcModel, &cTexts, psQuestion->szFileName.Text(), true))
	{
		//The group is the outermost expression that starts at its bracket and ends where it does.
		pcGroup = NULL;
		for (pcNode = cUnit.FindNodeAt(iLine, iColumn); pcNode; pcNode = pcNode->mpcParent)
		{
			if (!cUnit.GetRange(pcNode, &sRange) || (sRange.iLine != iLine) || (sRange.iColumn != iColumn) || (sRange.iEndLine > iEndLine) || ((sRange.iEndLine == iEndLine) && (sRange.iEndColumn > iEndColumn)))
			{
				break;
			}
			if (cUnit.IsExpression(pcNode) && (sRange.iEndLine == iEndLine) && (sRange.iEndColumn == iEndColumn))
			{
				pcGroup = pcNode;
			}
		}
		bResult = pcGroup && cUnit.AppendTypeKey(pcGroup, pszKey, piLevels);
	}
	cUnit.Kill();
	szText.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// On the edits thread: the question answered.  The expression in
// brackets is typed; if "->" follows it and it is a class, not a pointer
// to one, a call of the class's operator-> is typed in its place.  A
// header is left with its own text in the code model again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpressionTyper::Job(void* pvData)
{
	SIDETypeQuestion*	psQuestion;
	CChars				szGroup;
	CChars				szKey;
	int32				iLevels;
	bool				bResult;

	psQuestion = (SIDETypeQuestion*)pvData;
	szGroup.Init("(");
	szGroup.Append(psQuestion->szExpression);
	szGroup.Append(')');
	szKey.Init();
	iLevels = 0;
	bResult = TypeGroup(psQuestion, szGroup.Text(), &szKey, &iLevels);
	if (bResult && psQuestion->bArrow && (iLevels == 0))
	{
		szGroup.Append(".operator->()");
		szKey.Set("");
		bResult = TypeGroup(psQuestion, szGroup.Text(), &szKey, &iLevels);
	}
	if (bResult)
	{
		psQuestion->szKey.Set(szKey);
	}
	if (psQuestion->bHeader)
	{
		psQuestion->pcModel->SetHeaderText(psQuestion->szFileName.Text(), psQuestion->szHeaderText.Text());
	}
	szKey.Kill();
	szGroup.Kill();
}
