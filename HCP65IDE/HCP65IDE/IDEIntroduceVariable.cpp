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
#include "BaseLib/FileUtil.h"
#include "CppCompilerLib/CPPIndex.h"
#include "CppCompilerLib/CPP65Type.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEChangeSignature.h"
#include "IDEDefinitions.h"
#include "IDEMoveMembers.h"
#include "IDEIntroduceVariable.h"
#include "IDEContextActions.h"


#define IDE_TAB_WIDTH	4	//The editors' tabs, for lining names up.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEIntroduction::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szFileName.Init();
	szExpression.Init();
	szType.Init();
	szSuggested.Init();
	asOccurrences.Init();
	uiSelected = 0;
	uiAnchorOne = 0;
	uiAnchorAll = 0;
	bStatementOne = false;
	bStatementAll = false;
	uiDeclaration = 0;
	bDeclarations = false;
	szIndent.Init();
	asDeclared.Init();
	aszUsed.Init();
	aszWarnings.Init();
	asFileOccurrences.Init();
	bWholeFile = false;
	bInInit = false;
	szFunctionKey.Init();
	iFunctionLine = 0;
	iFunctionColumn = 0;
	szClassKey.Init();
	bConstant = false;
	bUsesLocals = false;
	bUsesMembers = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEIntroduction::Kill(void)
{
	asFileOccurrences.Kill();
	aszWarnings.Kill();
	szClassKey.Kill();
	szFunctionKey.Kill();
	aszUsed.Kill();
	asDeclared.Kill();
	szIndent.Kill();
	asOccurrences.Kill();
	szSuggested.Kill();
	szType.Kill();
	szExpression.Kill();
	szFileName.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The column uiPosition is drawn in, from 0, on the line that starts at
// uiLineStart: a tab goes to the next multiple of uiTabWidth.
//
//////////////////////////////////////////////////////////////////////////
size FindIDEVisualColumn(const char* szText, size uiLineStart, size uiPosition, size uiTabWidth)
{
	size	i;
	size	uiColumn;

	uiColumn = 0;
	for (i = uiLineStart; i < uiPosition; i++)
	{
		uiColumn = (szText[i] == '\t') ? ((uiColumn / uiTabWidth) + 1) * uiTabWidth : uiColumn + 1;
	}
	return uiColumn;
}


//////////////////////////////////////////////////////////////////////////
//
// The tabs that take what is written from uiFromColumn to uiToColumn (a
// multiple of the tab's width); one if it is there or past it already.
//
//////////////////////////////////////////////////////////////////////////
void AppendIDETabsTo(CChars* pszDest, size uiFromColumn, size uiToColumn, size uiTabWidth)
{
	size	uiTabs;

	uiTabs = (uiToColumn > uiFromColumn) ? (uiToColumn / uiTabWidth) - (uiFromColumn / uiTabWidth) : 0;
	pszDest->Append('\t', (uiTabs == 0) ? 1 : uiTabs);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	msShown.Init();
	mbAll = true;
	mbInInit = false;
	masCandidates.Init();
	miKind = IDE_INTRODUCE_VARIABLE;
	mszNameProblem.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpNameField);
	M_Pointer(mpError);
	M_Pointer(mpAllBox);
	M_Pointer(mpFileBox);
	M_Pointer(mpInitBox);
	U_Bool(mbInInit);
	U_Unknown(SIDEIntroduction, msShown);
	U_Bool(mbAll);
	M_Pointer(mpChooser);
	U_Unknown(CArrayTextRange, masCandidates);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::Free(void)
{
	mszNameProblem.Kill();
	masCandidates.Kill();
	msShown.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::Save(CObjectWriter* pcFile) { return false; }
bool CIDEIntroduceVariable::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Introduce Variable (Ctrl+Alt+V, as IntelliJ IDEA's), while there is an
// active editor: its selection, or an expression its caret is in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.IntroduceVariable", "Introduce Variable...", "Control+Alt+V", Ptr<CImageCel>(), this, &CIDEIntroduceVariable::QueueIntroduce)->SetEnabledCheck(this, &CIDEIntroduceVariable::CanIntroduce);
	pCommands->Add("Refactor.IntroduceField", "Introduce Field...", "Control+Alt+F", Ptr<CImageCel>(), this, &CIDEIntroduceVariable::QueueIntroduceField)->SetEnabledCheck(this, &CIDEIntroduceVariable::CanIntroduce);
	pCommands->Add("Refactor.IntroduceConstant", "Introduce Constant...", "Control+Alt+C", Ptr<CImageCel>(), this, &CIDEIntroduceVariable::QueueIntroduceConstant)->SetEnabledCheck(this, &CIDEIntroduceVariable::CanIntroduce);
	pCommands->Add("Refactor.IntroduceParameter", "Introduce Parameter...", "Control+Alt+P", Ptr<CImageCel>(), this, &CIDEIntroduceVariable::QueueIntroduceParameter)->SetEnabledCheck(this, &CIDEIntroduceVariable::CanIntroduce);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.IntroduceVariable"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.IntroduceField"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.IntroduceConstant"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.IntroduceParameter"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: an expression selected in a function's body (or
// a number or a string the caret is on) can be introduced; as a field in
// a member function.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bExpression && psContext->bInFunction)
	{
		psContext->Offer("Refactor.IntroduceVariable");
		if (psContext->bInMemberFunction)
		{
			psContext->Offer("Refactor.IntroduceField");
		}
		psContext->Offer("Refactor.IntroduceConstant");
		psContext->Offer("Refactor.IntroduceParameter");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::CanIntroduce(void)
{
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull();
}


Ptr<CMenuContainer> CIDEIntroduceVariable::GetChooser(void) { return mpChooser; }


//Finds what an expression names that can be changed (a variable, a member, an element), as it is written.
class CIDENamedPartFinder : public CCPPSyntaxChildVisitor
{
public:
	CIDERefactorUnit*	mpcUnit;
	CArrayChars*		mpaszParts;

	void	Look(CCPPSyntax* pcNode)
	{
		CChars	szTokens;

		if ((pcNode->GetKind() == CPPS_IdExpression) || (pcNode->GetKind() == CPPS_MemberExpression) || (pcNode->GetKind() == CPPS_SubscriptExpression))
		{
			szTokens.Init();
			mpcUnit->AppendTokens(pcNode->miFirstToken, pcNode->miLastToken, &szTokens);
			mpaszParts->AddIfUnique(szTokens.Text());
			szTokens.Kill();
		}
		pcNode->VisitFields(this);
	}

	void	Visit(CCPPSyntax* pcChild) override	{ Look(pcChild); }
};


//Finds where one of those is changed between two tokens (CIDEIntroduceVariable::FindReplaceWarnings).
class CIDEChangeBetweenFinder : public CCPPSyntaxChildVisitor
{
public:
	CIDERefactorUnit*	mpcUnit;
	CArrayChars*		mpaszParts;
	CArrayChars*		mpaszWarnings;
	int32				miAfter;
	int32				miBefore;

	void	Visit(CCPPSyntax* pcChild) override
	{
		SCPPSyntaxRange	sRange;
		CChars			szTokens;
		CChars			szWarning;

		if (mpcUnit->IsExpression(pcChild) && (pcChild->miFirstToken > miAfter) && (pcChild->miLastToken < miBefore) && (mpcUnit->GetPass()->FindChanges(pcChild) != 0) &&
			mpcUnit->GetRange(pcChild, &sRange))
		{
			szTokens.Init();
			mpcUnit->AppendTokens(pcChild->miFirstToken, pcChild->miLastToken, &szTokens);
			if (mpaszParts->GetIndex(szTokens.Text()) != ARRAY_ELEMENT_NOT_FOUND)
			{
				szWarning.Init(szTokens);
				szWarning.Append(", which the expression names, is changed at line ")->Append((int32)(sRange.iLine + 1))->Append(", between its first occurrence and its last: they may not all be the same value.");
				mpaszWarnings->AddIfUnique(szWarning.Text());
				szWarning.Kill();
			}
			szTokens.Kill();
		}
		pcChild->VisitFields(this);
	}
};


//////////////////////////////////////////////////////////////////////////
//
// What replacing every occurrence of the expression with one variable
// could change (psDest->aszWarnings), where it has more than one: the
// expression does something each time it is worked out (a call, a ++, an
// assignment), which would be done once; or something it names is
// changed between its first occurrence and its last, so they aren't all
// the same value.  A warning, shown before all are replaced: the user
// may know better (a call that only reads; a loop isn't followed round).
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::FindReplaceWarnings(CIDERefactorUnit* pcUnit, CCPPSyntax* pcBody, CCPPSyntax* pcSelected, CArraySyntax* papcNodes, SIDEIntroduction* psDest)
{
	CIDEEffectFinder		cEffects;
	CIDENamedPartFinder		cParts;
	CIDEChangeBetweenFinder	cChanges;
	CArrayChars				aszParts;

	if (papcNodes->NumElements() < 2)
	{
		return;
	}
	cEffects.mbEffects = false;
	cEffects.Look(pcSelected);
	if (cEffects.mbEffects)
	{
		psDest->aszWarnings.Add("The expression does something each time it is worked out (a call, a ++ or an assignment): one variable for every occurrence does it once.");
	}
	aszParts.Init();
	cParts.mpcUnit = pcUnit;
	cParts.mpaszParts = &aszParts;
	cParts.Look(pcSelected);
	cChanges.mpcUnit = pcUnit;
	cChanges.mpaszParts = &aszParts;
	cChanges.mpaszWarnings = &psDest->aszWarnings;
	cChanges.miAfter = papcNodes->GetPtr(0)->miLastToken;
	cChanges.miBefore = papcNodes->GetPtr(papcNodes->NumElements() - 1)->miFirstToken;
	pcBody->VisitFields(&cChanges);
	aszParts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The expressions the active editor's caret is in, from the innermost
// out to the whole of the expression it is part of: where each is in the
// text (masCandidates) and, on one line, how each is written (paszTexts).
// One that is the same text as the one inside it (a name's parentheses
// apart) is one candidate.  The number found.
//
//////////////////////////////////////////////////////////////////////////
size CIDEIntroduceVariable::FindCandidates(CArrayChars* paszTexts)
{
	CIDERefactorUnit	cUnit;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CCPPSyntax*			pcNode;
	SCPPSyntaxRange		sRange;
	STextRange			sTextRange;
	CChars				szText;
	size				uiCaret;
	size				uiLine;
	size				i;
	int32				iLastFirst;
	int32				iLastLast;
	char				c;

	masCandidates.ReInit();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return 0;
	}
	pEditor = pTab->GetEditor();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cUnit.Init();
	if (cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), pTab->GetFileName(), false))
	{
		uiCaret = pEditor->GetCaretPosition();
		uiLine = pEditor->FindLine(uiCaret);
		iLastFirst = -1;
		iLastLast = -1;
		for (pcNode = cUnit.FindNodeAt((int)uiLine, (int)(uiCaret - pEditor->GetLineStart(uiLine))); pcNode; pcNode = pcNode->mpcParent)
		{
			if ((pcNode->GetKind() >= CPPS_CompoundStatement) && (pcNode->GetKind() <= CPPS_SkippedStatement))
			{
				break;
			}
			if (!cUnit.IsExpression(pcNode) || ((pcNode->miFirstToken == iLastFirst) && (pcNode->miLastToken == iLastLast)) || !cUnit.GetRange(pcNode, &sRange))
			{
				continue;
			}
			iLastFirst = pcNode->miFirstToken;
			iLastLast = pcNode->miLastToken;
			sTextRange.uiStart = FindIDETextOffset(pEditor->GetText(), sRange.iLine, sRange.iColumn);
			sTextRange.uiEnd = FindIDETextOffset(pEditor->GetText(), sRange.iEndLine, sRange.iEndColumn);
			masCandidates.Add(&sTextRange);

			//As it is written, on one line: each run of white space is one space.
			szText.Init();
			for (i = sTextRange.uiStart; i < sTextRange.uiEnd; i++)
			{
				c = pEditor->GetText()[i];
				if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
				{
					if (!szText.Empty() && (szText.Text()[szText.Length() - 1] != ' '))
					{
						szText.Append(' ');
					}
				}
				else
				{
					szText.Append(c);
				}
			}
			paszTexts->Add(szText.Text());
			szText.Kill();
		}
	}
	cUnit.Kill();
	return masCandidates.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// A candidate chosen: it is selected in the editor, and introduced at
// the next tick (the dialog can't be shown from the click).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::ChooseCandidate(size uiCandidate)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || (uiCandidate >= masCandidates.NumElements()))
	{
		return false;
	}
	pTab->GetEditor()->Select(masCandidates.Get(uiCandidate)->uiStart, masCandidates.Get(uiCandidate)->uiEnd);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::ChooserClicked(CMenuItem* pcItem)
{
	size	i;

	for (i = 1; i < mpChooser->NumItems(); i++)
	{
		if ((&mpChooser->GetItem(i) == pcItem) && ChooseCandidate(i - 1))
		{
			PostIntroduce();
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog is shown at the next tick: ShowModal can't be called from
// the input that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::QueueIntroduce(void)
{
	miKind = IDE_INTRODUCE_VARIABLE;
	PostIntroduce();
}


void CIDEIntroduceVariable::QueueIntroduceField(void) { miKind = IDE_INTRODUCE_FIELD; PostIntroduce(); }
void CIDEIntroduceVariable::QueueIntroduceConstant(void) { miKind = IDE_INTRODUCE_CONSTANT; PostIntroduce(); }
void CIDEIntroduceVariable::QueueIntroduceParameter(void) { miKind = IDE_INTRODUCE_PARAMETER; PostIntroduce(); }


//////////////////////////////////////////////////////////////////////////
//
// The dialog is shown once the menu or popup that asked for it has gone;
// what is introduced is as it was asked (miKind).
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::PostIntroduce(void)
{
	mpcContent->GetWindow()->Post(this, &CIDEIntroduceVariable::ShowQueuedIntroduce);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::ShowQueuedIntroduce(void)
{
	ShowIntroduce();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::Refuse(SIDEIntroduction* psDest, const char* szWhyNot)
{
	psDest->bPossible = false;
	psDest->szWhyNot.Set(szWhyNot);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The type the variable is declared with, from the kept check: as the
// expression's declaration wrote it if it has one
// (CIDERefactorUnit::AppendDeclaredType), or else as the compiler worked
// it out, without a reference (the variable holds the value) and without
// const where it isn't a pointer (it is assigned to); an array's is a
// pointer to its elements.  False, with why, if it has none or it can't
// be written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::FindType(CIDERefactorUnit* pcUnit, CCPPSyntax* pcExpression, SIDEIntroduction* psDest)
{
	SCPP65Type*	psType;
	bool		bWritten;

	psType = pcUnit->GetPass()->GetType(pcExpression);
	if (!psType)
	{
		if (pcUnit->FindAncestor(pcExpression, CPPS_TemplateDeclaration))
		{
			return Refuse(psDest, "its type depends on the template's arguments");
		}
		return Refuse(psDest, "its type couldn't be worked out");
	}
	if (psType->IsReference())
	{
		psType = psType->psBase;
	}
	if (psType->IsVoid())
	{
		return Refuse(psDest, "it has no value (its type is void)");
	}
	if (psType->IsFunction())
	{
		return Refuse(psDest, "it is a function");
	}
	if (pcUnit->AppendDeclaredType(pcExpression, &psDest->szType))
	{
		return true;
	}

	if (psType->IsArray())
	{
		bWritten = pcUnit->GetPass()->AppendType(psType->psBase, &psDest->szType);
		psDest->szType.Append('*');
	}
	else
	{
		bWritten = pcUnit->GetPass()->AppendType((psType->IsPointer() || psType->IsMemberPointer()) ? psType : psType->Unqualified(), &psDest->szType);
	}
	if (!bWritten)
	{
		return Refuse(psDest, "its type has no name to write (a class or an enum without one)");
	}
	return true;
}


//Finds the expressions in a function's body written as the selected one is (CIDEIntroduceVariable::FindOccurrences).
class CIDEOccurrenceFinder : public CCPPSyntaxChildVisitor
{
public:
	CIDERefactorUnit*	mpcUnit;
	const char*			mszTokens;		//The selected expression's tokens.
	int32				miTokens;		//How many it has.
	CArraySyntax*		mpapcFound;

	void	Visit(CCPPSyntax* pcChild) override
	{
		CChars	szTokens;
		bool	bSame;

		bSame = false;
		if (mpcUnit->IsExpression(pcChild) && (pcChild->miLastToken - pcChild->miFirstToken + 1 == miTokens))
		{
			szTokens.Init();
			mpcUnit->AppendTokens(pcChild->miFirstToken, pcChild->miLastToken, &szTokens);
			bSame = szTokens.Equals(mszTokens);
			szTokens.Kill();
		}
		if (bSame)
		{
			//What is in it can't be another: it has fewer tokens.
			if ((mpapcFound->NumElements() == 0) || (mpapcFound->GetPtr(mpapcFound->NumElements() - 1)->miFirstToken != pcChild->miFirstToken))
			{
				mpapcFound->Add(pcChild);
			}
		}
		else
		{
			pcChild->VisitFields(this);
		}
	}
};


//////////////////////////////////////////////////////////////////////////
//
// Every place in the function's body the selected expression is written
// again, token for token, with it, in the order they are written: their
// nodes in papcNodes and their places in the text in the introduction.
// One that is changed where it is (assigned to, its address taken, bound
// to a reference that isn't const) isn't a value to replace and is left
// out, unless it is the selected one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::FindOccurrences(CIDERefactorUnit* pcUnit, CCPPSyntax* pcBody, CCPPSyntax* pcSelected, const char* szText, SIDEIntroduction* psDest, CArraySyntax* papcNodes)
{
	CIDEOccurrenceFinder	cFinder;
	CArraySyntax			apcFound;
	CChars					szTokens;
	CCPPSyntax*				pcNode;
	SCPPSyntaxRange			sRange;
	STextRange				sTextRange;
	size					i;

	szTokens.Init();
	pcUnit->AppendTokens(pcSelected->miFirstToken, pcSelected->miLastToken, &szTokens);
	apcFound.Init();
	cFinder.mpcUnit = pcUnit;
	cFinder.mszTokens = szTokens.Text();
	cFinder.miTokens = pcSelected->miLastToken - pcSelected->miFirstToken + 1;
	cFinder.mpapcFound = &apcFound;
	pcBody->VisitFields(&cFinder);
	szTokens.Kill();

	for (i = 0; i < apcFound.NumElements(); i++)
	{
		pcNode = apcFound.GetPtr(i);
		if ((pcNode->miFirstToken != pcSelected->miFirstToken) && (pcUnit->GetPass()->FindChanges(pcNode) != 0))
		{
			continue;
		}
		if (!pcUnit->GetRange(pcNode, &sRange))
		{
			continue;
		}
		sTextRange.uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
		sTextRange.uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		if (pcNode->miFirstToken == pcSelected->miFirstToken)
		{
			psDest->uiSelected = psDest->asOccurrences.NumElements();
		}
		psDest->asOccurrences.Add(&sTextRange);
		papcNodes->Add(pcNode);
	}
	apcFound.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The statement the variable is assigned before, for the occurrences
// from uiFirst to uiLast of papcNodes: the statement, directly in the
// innermost block that all of them are in, that the first of them is
// in.  NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
CCPPSyntax* CIDEIntroduceVariable::FindAnchor(CArraySyntax* papcNodes, size uiFirst, size uiLast)
{
	CCPPSyntax*	pcBlock;
	CCPPSyntax*	pcNode;
	CCPPSyntax*	pcStatement;
	size		i;
	bool		bInAll;

	//The innermost block the first is in that the others are in too.
	for (pcBlock = papcNodes->GetPtr(uiFirst)->mpcParent; pcBlock; pcBlock = pcBlock->mpcParent)
	{
		if (pcBlock->GetKind() != CPPS_CompoundStatement)
		{
			continue;
		}
		bInAll = true;
		for (i = uiFirst + 1; bInAll && (i <= uiLast); i++)
		{
			pcNode = papcNodes->GetPtr(i);
			bInAll = (pcNode->miFirstToken >= pcBlock->miFirstToken) && (pcNode->miLastToken <= pcBlock->miLastToken);
		}
		if (bInAll)
		{
			break;
		}
	}
	if (!pcBlock)
	{
		return NULL;
	}
	pcStatement = papcNodes->GetPtr(uiFirst);
	while (pcStatement->mpcParent != pcBlock)
	{
		pcStatement = pcStatement->mpcParent;
	}
	return pcStatement;
}


//////////////////////////////////////////////////////////////////////////
//
// The function's declarations, the statements at its body's start that
// declare its locals: where a new one's line goes (after the last of
// them, or after the body's { if there are none) and, for lining its
// name up, the white space before each name they declare and the columns
// it is in.  The body's indentation is its first statement's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::FindDeclarations(CIDERefactorUnit* pcUnit, CCPPSyntaxCompoundStatement* pcBody, const char* szText, SIDEIntroduction* psDest)
{
	CCPPSyntax*						pcStatement;
	CCPPSyntaxSimpleDeclaration*	pcSimple;
	CCPPSyntaxInitDeclarator*		pcInit;
	CCPPSyntaxName*					pcName;
	SCPPSyntaxRange					sRange;
	SIDEDeclaredName				sDeclared;
	size							uiLineStart;
	size							uiNameStart;
	size							uiEnd;
	size							i;
	size							j;

	//After the body's {, on the next line.
	psDest->uiDeclaration = 0;
	if (pcUnit->GetTokensRange(pcBody->miFirstToken, pcBody->miFirstToken, &sRange))
	{
		for (uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn); (szText[uiEnd] != '\0') && (szText[uiEnd] != '\n'); uiEnd++)
		{
		}
		psDest->uiDeclaration = (szText[uiEnd] == '\n') ? uiEnd + 1 : uiEnd;
	}

	for (i = 0; i < pcBody->macpStatements.NumElements(); i++)
	{
		pcStatement = pcBody->macpStatements.GetPtr(i);
		if (i == 0)
		{
			//The body's indentation.
			if (pcUnit->GetRange(pcStatement, &sRange))
			{
				uiLineStart = FindIDETextOffset(szText, sRange.iLine, 0);
				psDest->szIndent.AppendSubString(szText, uiLineStart, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
			}
		}
		if ((pcStatement->GetKind() != CPPS_DeclarationStatement) || !pcUnit->GetRange(pcStatement, &sRange))
		{
			break;
		}
		pcSimple = (CCPPSyntaxSimpleDeclaration*)((CCPPSyntaxDeclarationStatement*)pcStatement)->mpcDeclaration;
		if (!pcSimple || (pcSimple->GetKind() != CPPS_SimpleDeclaration))
		{
			break;
		}
		psDest->bDeclarations = true;
		for (uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn); (szText[uiEnd] != '\0') && (szText[uiEnd] != '\n'); uiEnd++)
		{
		}
		psDest->uiDeclaration = (szText[uiEnd] == '\n') ? uiEnd + 1 : uiEnd;

		//The first name it declares, if it starts its line's declaration: "CChars*		psz;" is lined up at psz.
		for (j = 0; (j < pcSimple->macpDeclarators.NumElements()) && (j < 1); j++)
		{
			pcInit = (CCPPSyntaxInitDeclarator*)pcSimple->macpDeclarators.GetPtr(j);
			pcName = (pcInit->mpcDeclarator) ? pcInit->mpcDeclarator->GetName() : NULL;
			if (!pcName || !pcUnit->GetRange(pcName, &sRange))
			{
				continue;
			}
			uiNameStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
			uiLineStart = FindIDETextOffset(szText, sRange.iLine, 0);
			sDeclared.uiSpaceEnd = uiNameStart;
			for (sDeclared.uiSpaceStart = uiNameStart; (sDeclared.uiSpaceStart > uiLineStart) && ((szText[sDeclared.uiSpaceStart - 1] == ' ') || (szText[sDeclared.uiSpaceStart - 1] == '\t')); sDeclared.uiSpaceStart--)
			{
			}
			if (sDeclared.uiSpaceStart == sDeclared.uiSpaceEnd)
			{
				continue;
			}
			sDeclared.uiSpaceColumn = FindIDEVisualColumn(szText, uiLineStart, sDeclared.uiSpaceStart, IDE_TAB_WIDTH);
			sDeclared.uiNameColumn = FindIDEVisualColumn(szText, uiLineStart, uiNameStart, IDE_TAB_WIDTH);
			psDest->asDeclared.Add(&sDeclared);
		}
	}
	if (psDest->szIndent.Empty())
	{
		psDest->szIndent.Set("\t");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A name for the variable, as the house style names locals: what the
// expression's last name says it is, without its own prefix or a verb
// (miCount, GetCount() and mcItems.NumElements() give Count, Count and
// NumElements), after the prefix its type has (i, ui, b, f, sz, pc, ps,
// c, s, e, p).  A number is added if the function uses the name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::SuggestName(CIDERefactorUnit* pcUnit, CCPPSyntax* pcExpression, SIDEIntroduction* psDest)
{
	static const char*	aszVerbs[] = { "Get", "Is", "Has", "Find", "Calculate", "Create", "Make", "Read" };
	SPreprocessedToken*	psSource;
	CChars				szBase;
	CChars				szName;
	const char*			szType;
	const char*			szPrefix;
	size				uiTypeLength;
	size				uiStart;
	size				j;
	int32				i;
	int					iNumber;
	bool				bPointer;
	bool				bUnsigned;

	//The last name written in it; a call's is its function's, not its last argument's.
	pcExpression = pcExpression->WithoutParentheses();
	if (pcExpression->GetKind() == CPPS_CallExpression)
	{
		pcExpression = ((CCPPSyntaxCallExpression*)pcExpression)->mpcCallee;
	}
	szBase.Init();
	for (i = pcExpression->miLastToken; i >= pcExpression->miFirstToken; i--)
	{
		psSource = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)i));
		if ((psSource->uiLength > 0) && (IsIDENameChar(psSource->szText[0]) && !((psSource->szText[0] >= '0') && (psSource->szText[0] <= '9'))) && pcUnit->GetTokeniser()->Get((size)i)->IsIdentifier())
		{
			szBase.Append(psSource->szText, psSource->uiLength);
			break;
		}
	}

	//Without its own prefix: an m and lower case letters before a capital, or lower case letters before a capital.
	uiStart = 0;
	while ((uiStart < szBase.Length()) && (szBase.Text()[uiStart] >= 'a') && (szBase.Text()[uiStart] <= 'z'))
	{
		uiStart++;
	}
	if ((uiStart > 0) && (uiStart < szBase.Length()) && (szBase.Text()[uiStart] >= 'A') && (szBase.Text()[uiStart] <= 'Z'))
	{
		szBase.RemoveFromStart(uiStart);
	}
	for (j = 0; j < sizeof(aszVerbs) / sizeof(char*); j++)
	{
		uiStart = strlen(aszVerbs[j]);
		if ((szBase.Length() > uiStart) && (strncmp(szBase.Text(), aszVerbs[j], uiStart) == 0) && (szBase.Text()[uiStart] >= 'A') && (szBase.Text()[uiStart] <= 'Z'))
		{
			szBase.RemoveFromStart(uiStart);
			break;
		}
	}
	if (szBase.Empty() || !((szBase.Text()[0] >= 'A') && (szBase.Text()[0] <= 'Z')))
	{
		szBase.Set("Value");
	}

	//The prefix its type has.
	szType = psDest->szType.Text();
	uiTypeLength = psDest->szType.Length();
	bPointer = (uiTypeLength > 0) && (szType[uiTypeLength - 1] == '*');
	bUnsigned = (strncmp(szType, "unsigned", 8) == 0) || (strncmp(szType, "uint", 4) == 0) || (strcmp(szType, "size") == 0);
	if (strncmp(szType, "const ", 6) == 0)
	{
		szType += 6;
	}
	if (strncmp(szType, "Ptr<", 4) == 0)
	{
		szPrefix = "p";
	}
	else if ((strncmp(szType, "char", 4) == 0) && bPointer)
	{
		szPrefix = "sz";
	}
	else if (strcmp(szType, "CChars") == 0)
	{
		szPrefix = "sz";
	}
	else if ((szType[0] == 'C') && (szType[1] >= 'A') && (szType[1] <= 'Z'))
	{
		szPrefix = bPointer ? "pc" : "c";
	}
	else if ((szType[0] == 'S') && (szType[1] >= 'A') && (szType[1] <= 'Z'))
	{
		szPrefix = bPointer ? "ps" : "s";
	}
	else if ((szType[0] == 'E') && (szType[1] >= 'A') && (szType[1] <= 'Z') && !bPointer)
	{
		szPrefix = "e";
	}
	else if (bPointer)
	{
		szPrefix = (strncmp(szType, "void", 4) == 0) ? "pv" : "p";
	}
	else if (strcmp(szType, "bool") == 0)
	{
		szPrefix = "b";
	}
	else if ((strncmp(szType, "float", 5) == 0) || (strcmp(szType, "double") == 0) || (strcmp(szType, "long double") == 0))
	{
		szPrefix = "f";
	}
	else if (strcmp(szType, "char") == 0)
	{
		szPrefix = "c";
	}
	else if (bUnsigned)
	{
		szPrefix = "ui";
	}
	else
	{
		szPrefix = "i";
	}

	szName.Init(szPrefix);
	szName.Append(szBase);
	for (iNumber = 2; psDest->aszUsed.Contains(szName.Text()); iNumber++)
	{
		szName.Set(szPrefix);
		szName.Append(szBase);
		szName.Append(iNumber);
	}
	psDest->szSuggested.Set(szName);
	szName.Kill();
	szBase.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Introduce Variable would do for the active editor's selection.
// False if there is no selection; otherwise the introduction says
// whether it can be done and why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::FindIntroduction(SIDEIntroduction* psDest)
{
	CIDERefactorUnit				cUnit;
	Ptr<CIDEEditorTab>				pTab;
	Ptr<CTextArea>					pEditor;
	CArraySyntax					apcNodes;
	CCPPSyntax*						pcExpression;
	CCPPSyntax*						pcAnchor;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	SPreprocessedToken*				psSource;
	SCPPSyntaxRange					sRange;
	CChars							szUsed;
	char*							szText;
	size							uiStart;
	size							uiEnd;
	size							uiLine;
	size							uiEndLine;
	int32							iFirst;
	int32							iLast;
	int32							i;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetEditor()->GetSelection(&uiStart, &uiEnd))
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	psDest->szFileName.Set(pTab->GetFileName());
	mpcContent->GetDocuments()->UpdateEditedTabsNow();

	cUnit.Init();
	if (!cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), pTab->GetFileName(), true))
	{
		cUnit.Kill();
		Refuse(psDest, "the file couldn't be parsed (it must be one of the solution's)");
		return true;
	}

	szText = pEditor->GetText();
	uiLine = pEditor->FindLine(uiStart);
	uiEndLine = pEditor->FindLine(uiEnd);
	pcExpression = NULL;
	if (cUnit.FindTokensIn((int)uiLine, (int)(uiStart - pEditor->GetLineStart(uiLine)), (int)uiEndLine, (int)(uiEnd - pEditor->GetLineStart(uiEndLine)), &iFirst, &iLast))
	{
		//The innermost node of just those tokens may be a part of the expression that is them (a name's name).
		pcExpression = cUnit.FindNodeOfTokens(iFirst, iLast);
		while (pcExpression && !cUnit.IsExpression(pcExpression) && pcExpression->mpcParent &&
			   (pcExpression->mpcParent->miFirstToken == iFirst) && (pcExpression->mpcParent->miLastToken == iLast))
		{
			pcExpression = pcExpression->mpcParent;
		}
	}
	pcFunction = pcExpression ? (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcExpression, CPPS_FunctionDefinition) : NULL;
	if (!pcExpression || !cUnit.IsExpression(pcExpression) || (pcExpression->miFirstToken != iFirst) || (pcExpression->miLastToken != iLast))
	{
		Refuse(psDest, "the selection isn't an expression");
	}
	else if (!pcFunction || !pcFunction->mpcBody || (pcFunction->mpcBody->GetKind() != CPPS_CompoundStatement) ||
			 (pcExpression->miFirstToken < pcFunction->mpcBody->miFirstToken) || (pcExpression->miLastToken > pcFunction->mpcBody->miLastToken))
	{
		Refuse(psDest, "the expression isn't in a function's body");
	}
	else if (FindType(&cUnit, pcExpression, psDest))
	{
		cUnit.GetRange(pcExpression, &sRange);
		psDest->szExpression.AppendSubString(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));

		apcNodes.Init();
		FindOccurrences(&cUnit, pcFunction->mpcBody, pcExpression, szText, psDest, &apcNodes);
		FindReplaceWarnings(&cUnit, pcFunction->mpcBody, pcExpression, &apcNodes, psDest);
		FindFileOccurrences(&cUnit, pcExpression, szText, psDest);
		psDest->bPossible = apcNodes.NumElements() > 0;
		if (!psDest->bPossible)
		{
			Refuse(psDest, "the expression is made by a macro");
		}
		else
		{
			pcAnchor = FindAnchor(&apcNodes, psDest->uiSelected, psDest->uiSelected);
			if (pcAnchor && cUnit.GetRange(pcAnchor, &sRange))
			{
				psDest->uiAnchorOne = FindIDETextOffset(szText, sRange.iLine, 0);
				psDest->bStatementOne = (pcAnchor->GetKind() == CPPS_ExpressionStatement) && (((CCPPSyntaxExpressionStatement*)pcAnchor)->mpcExpression == apcNodes.GetPtr(psDest->uiSelected));
			}
			else
			{
				Refuse(psDest, "the statement it is in is made by a macro");
			}
			pcAnchor = FindAnchor(&apcNodes, 0, apcNodes.NumElements() - 1);
			if (pcAnchor && cUnit.GetRange(pcAnchor, &sRange))
			{
				psDest->uiAnchorAll = FindIDETextOffset(szText, sRange.iLine, 0);
				psDest->bStatementAll = (pcAnchor->GetKind() == CPPS_ExpressionStatement) && (((CCPPSyntaxExpressionStatement*)pcAnchor)->mpcExpression == apcNodes.GetPtr(0));
			}
			else
			{
				Refuse(psDest, "a statement it is in is made by a macro");
			}
		}
		apcNodes.Kill();

		if (psDest->bPossible)
		{
			FindDeclarations(&cUnit, (CCPPSyntaxCompoundStatement*)pcFunction->mpcBody, szText, psDest);

			//Every name the function writes, its parameters' too.
			for (i = pcFunction->miFirstToken; i <= pcFunction->miLastToken; i++)
			{
				if (cUnit.GetTokeniser()->Get((size)i)->IsIdentifier())
				{
					psSource = cUnit.GetTokeniser()->GetSource(cUnit.GetTokeniser()->Get((size)i));
					szUsed.Init();
					szUsed.Append(psSource->szText, psSource->uiLength);
					psDest->aszUsed.AddIfUnique(szUsed.Text());
					szUsed.Kill();
				}
			}
			SuggestName(&cUnit, pcExpression, psDest);
			FindContext(&cUnit, pcFunction, pcExpression, psDest);
		}
	}
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Why szName can't be the variable's name, NULL if it can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEIntroduceVariable::GetNameProblem(SIDEIntroduction* psIntroduction, const char* szName, int iKind)
{
	CChars	szKey;
	bool	bTaken;

	if (!IsIDEIdentifier(szName))
	{
		return "It isn't a name.";
	}
	if (psIntroduction->aszUsed.Contains(szName))
	{
		return "The function uses that name already.";
	}
	if ((iKind == IDE_INTRODUCE_FIELD) || (iKind == IDE_INTRODUCE_CONSTANT))
	{
		//A field's name in its class, a constant's anywhere.
		szKey.Init();
		if (iKind == IDE_INTRODUCE_FIELD)
		{
			szKey.Append(psIntroduction->szClassKey);
			szKey.Append("::");
		}
		szKey.Append(szName);
		bTaken = mpcContent->GetCodeModel()->HasSymbol(szKey.Text());
		szKey.Kill();
		if (bTaken)
		{
			return (iKind == IDE_INTRODUCE_FIELD) ? "The class has that name already." : "Something has that name already.";
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Why the expression can't be introduced as a kind of thing, NULL if it
// can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEIntroduceVariable::GetKindProblem(SIDEIntroduction* psIntroduction, int iKind)
{
	if ((iKind == IDE_INTRODUCE_FIELD) && psIntroduction->szClassKey.Empty())
	{
		return "the expression isn't in a member function of a class of the solution";
	}
	if ((iKind == IDE_INTRODUCE_CONSTANT) && !psIntroduction->bConstant)
	{
		return "the expression isn't a constant: it names a variable or does something";
	}
	if ((iKind == IDE_INTRODUCE_CONSTANT) && psIntroduction->bStatementOne)
	{
		return "the expression is a statement by itself";
	}
	if ((iKind == IDE_INTRODUCE_PARAMETER) && psIntroduction->szFunctionKey.Empty())
	{
		return "the function it is in isn't one the index knows";
	}
	if ((iKind == IDE_INTRODUCE_PARAMETER) && psIntroduction->bUsesLocals)
	{
		return "the expression uses the function's own variables or parameters, which a call of it can't give";
	}
	if ((iKind == IDE_INTRODUCE_PARAMETER) && psIntroduction->bUsesMembers)
	{
		return "the expression uses its class's members, which a call of the function names another way";
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The name suggested for a kind of thing, from the variable's: a field's
// with m before it (miCount), a constant's in capitals without its type's
// letters (COUNT).
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::AppendSuggestedName(SIDEIntroduction* psIntroduction, int iKind, CChars* pszDest)
{
	const char*	szName;
	size		i;
	bool		bPreviousLower;

	szName = psIntroduction->szSuggested.Text();
	if (iKind == IDE_INTRODUCE_FIELD)
	{
		pszDest->Append('m');
		pszDest->Append(szName);
	}
	else if (iKind == IDE_INTRODUCE_CONSTANT)
	{
		//Past the lower case letters the name starts with, if a capital follows them.
		for (i = 0; (szName[i] >= 'a') && (szName[i] <= 'z'); i++)
		{
		}
		szName = ((szName[i] >= 'A') && (szName[i] <= 'Z')) ? &szName[i] : szName;
		bPreviousLower = false;
		for (i = 0; szName[i] != '\0'; i++)
		{
			if ((szName[i] >= 'A') && (szName[i] <= 'Z') && bPreviousLower)
			{
				pszDest->Append('_');
			}
			bPreviousLower = ((szName[i] >= 'a') && (szName[i] <= 'z')) || ((szName[i] >= '0') && (szName[i] <= '9'));
			pszDest->Append(((szName[i] >= 'a') && (szName[i] <= 'z')) ? (char)(szName[i] - 'a' + 'A') : szName[i]);
		}
	}
	else
	{
		pszDest->Append(szName);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every place in the file the expression is written, in any function or
// outside them all (psDest->asFileOccurrences), as FindOccurrences finds
// those of its own function: for a constant, which is the same wherever
// it is written.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::FindFileOccurrences(CIDERefactorUnit* pcUnit, CCPPSyntax* pcSelected, const char* szText, SIDEIntroduction* psDest)
{
	CIDEOccurrenceFinder	cFinder;
	CArraySyntax			apcFound;
	CChars					szTokens;
	CCPPSyntax*				pcNode;
	SCPPSyntaxRange			sRange;
	STextRange				sTextRange;
	size					i;

	if (!pcUnit->GetTree()->GetRoot())
	{
		return;
	}
	szTokens.Init();
	pcUnit->AppendTokens(pcSelected->miFirstToken, pcSelected->miLastToken, &szTokens);
	apcFound.Init();
	cFinder.mpcUnit = pcUnit;
	cFinder.mszTokens = szTokens.Text();
	cFinder.miTokens = pcSelected->miLastToken - pcSelected->miFirstToken + 1;
	cFinder.mpapcFound = &apcFound;
	pcUnit->GetTree()->GetRoot()->VisitFields(&cFinder);
	szTokens.Kill();
	for (i = 0; i < apcFound.NumElements(); i++)
	{
		pcNode = apcFound.GetPtr(i);
		if (((pcNode->miFirstToken != pcSelected->miFirstToken) && (pcUnit->GetPass()->FindChanges(pcNode) != 0)) || !pcUnit->GetRange(pcNode, &sRange))
		{
			continue;
		}
		sTextRange.uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
		sTextRange.uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		psDest->asFileOccurrences.Add(&sTextRange);
	}
	apcFound.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Where the expression is: the function it is in (its key, and where its
// name is written), that function's class, and what the expression names:
// the function's own variables and parameters, its class's members, and
// whether it is a constant (it names no variable and does nothing).
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::FindContext(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, CCPPSyntax* pcExpression, SIDEIntroduction* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	CArrayChars			aszKeys;
	CCPPSyntaxName*		pcName;
	CIDEEffectFinder	cEffects;
	SCPPSyntaxRange		sRange;
	CChars				szScope;
	int32				iToken;
	size				i;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	pcName = pcFunction->mpcDeclarator ? pcFunction->mpcDeclarator->GetName() : NULL;
	if (pcName && pcName->GetLast() && pcUnit->GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sRange) &&
		pcModel->GetSymbolAt(psDest->szFileName.Text(), sRange.iLine, sRange.iColumn, &psDest->szFunctionKey, &uiKind))
	{
		psDest->iFunctionLine = sRange.iLine;
		psDest->iFunctionColumn = sRange.iColumn;
		szScope.Init();
		GetIDESymbolScope(psDest->szFunctionKey.Text(), &szScope);
		asLocations.Init();
		if (!szScope.Empty() && pcModel->GetSymbolLocations(szScope.Text(), &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
		{
			psDest->szClassKey.Set(szScope);
		}
		KillIDELocations(&asLocations);
		szScope.Kill();
	}

	cEffects.mbEffects = false;
	cEffects.Look(pcExpression);
	psDest->bConstant = !cEffects.mbEffects;
	for (iToken = pcExpression->miFirstToken; iToken <= pcExpression->miLastToken; iToken++)
	{
		if (pcUnit->IsToken(iToken, "this"))
		{
			psDest->bUsesMembers = true;
			psDest->bConstant = false;
		}
	}
	if (pcUnit->GetRange(pcExpression, &sRange))
	{
		asRanges.Init();
		asRanges.Add()->Init(psDest->szFileName.Text(), sRange.iLine, sRange.iColumn, sRange.iEndLine, sRange.iEndColumn);
		aszKeys.Init();
		pcModel->FindSymbolsUsedIn(&asRanges, &aszKeys);
		for (i = 0; i < aszKeys.NumElements(); i++)
		{
			asLocations.Init();
			uiKind = CPPSK_Variable;
			pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind);
			KillIDELocations(&asLocations);
			if (aszKeys.GetText(i)[0] == '@')
			{
				psDest->bUsesLocals = true;
			}
			szScope.Init();
			GetIDESymbolScope(aszKeys.GetText(i), &szScope);
			if (!psDest->szClassKey.Empty() && szScope.Equals(psDest->szClassKey.Text()) && ((uiKind == CPPSK_Field) || (uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate) || (uiKind == CPPSK_Variable)))
			{
				psDest->bUsesMembers = true;
			}
			szScope.Kill();
			if ((uiKind != CPPSK_Enumerator) && (uiKind != CPPSK_Enum) && (uiKind != CPPSK_Class) && (uiKind != CPPSK_ClassTemplate) && (uiKind != CPPSK_Typedef) && (uiKind != CPPSK_BuiltinType))
			{
				psDest->bConstant = false;
			}
		}
		aszKeys.Kill();
		KillIDEFileRanges(&asRanges);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where a new #define goes in a file: after its last #define or
// #include before its code starts (true: its line goes there as it is);
// with neither, at its start, after the comment it opens with (false).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::FindDefinePlace(const char* szText, size* puiAt)
{
	size	uiLine;
	size	uiNext;
	size	uiFirstCode;
	size	i;
	bool	bFound;
	bool	bComment;

	bFound = false;
	bComment = false;
	uiFirstCode = 0;
	for (uiLine = 0; szText[uiLine] != '\0'; uiLine = uiNext)
	{
		for (uiNext = uiLine; (szText[uiNext] != '\0') && (szText[uiNext] != '\n'); uiNext++)
		{
		}
		uiNext = (szText[uiNext] == '\n') ? uiNext + 1 : uiNext;
		for (i = uiLine; (szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r') || ((uiLine == 0) && ((uint8)szText[i] >= 0xef)); i++)
		{
		}
		if (bComment)
		{
			bComment = (strstr(&szText[uiLine], "*/") == NULL) || ((size)(strstr(&szText[uiLine], "*/") - szText) >= uiNext);
			uiFirstCode = uiNext;
		}
		else if ((szText[i] == '\n') || (szText[i] == '\0') || ((szText[i] == '/') && (szText[i + 1] == '/')))
		{
			uiFirstCode = bFound ? uiFirstCode : uiNext;
		}
		else if ((szText[i] == '/') && (szText[i + 1] == '*'))
		{
			bComment = (strstr(&szText[i + 2], "*/") == NULL) || ((size)(strstr(&szText[i + 2], "*/") - szText) >= uiNext);
			uiFirstCode = bFound ? uiFirstCode : uiNext;
		}
		else if (szText[i] == '#')
		{
			for (i++; (szText[i] == ' ') || (szText[i] == '\t'); i++)
			{
			}
			if ((strncmp(&szText[i], "include", 7) == 0) || (strncmp(&szText[i], "define", 6) == 0))
			{
				*puiAt = uiNext;
				bFound = true;
			}
		}
		else
		{
			break;
		}
	}
	if (!bFound)
	{
		*puiAt = uiFirstCode;
	}
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The expression made a field, a constant or a parameter: see the
// class's comment.  One undo across the files it changes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::IntroduceElsewhere(SIDEIntroduction* psIntroduction, const char* szName, bool bAll, CChars* pszStatus, int iKind)
{
	CIDEFileEdits		cFiles;
	CIDETextEdits*		pcEdits;
	Ptr<CIDEEditorTab>	pTab;
	SIDESignature		sSignature;
	SIDESignatureChange	sChange;
	SIDENewParameter*	psNew;
	CArrayChars			aszConflicts;
	CArrayInt			aiStarts;
	CArrayInt			aiEnds;
	STextRange*			psOccurrence;
	CChars				szMessage;
	CChars				szLine;
	const char*			szText;
	const char*			szProblem;
	size				uiFirst;
	size				uiLast;
	size				uiAnchor;
	size				uiAt;
	size				uiReplaced;
	size				i;
	bool				bStatement;
	bool				bAfter;
	bool				bResult;

	pTab = mpcContent->GetDocuments()->FindTab(psIntroduction->szFileName.Text());
	szText = pTab->GetEditor()->GetText();
	uiFirst = bAll ? 0 : psIntroduction->uiSelected;
	uiLast = bAll ? psIntroduction->asOccurrences.NumElements() - 1 : psIntroduction->uiSelected;
	uiAnchor = (uiFirst == uiLast) ? psIntroduction->uiAnchorOne : psIntroduction->uiAnchorAll;
	bStatement = (uiFirst == uiLast) ? psIntroduction->bStatementOne : psIntroduction->bStatementAll;
	uiReplaced = 0;
	bResult = true;
	szProblem = NULL;

	if (iKind == IDE_INTRODUCE_PARAMETER)
	{
		//Change Signature adds the parameter and gives every call the expression; the expression becomes the
		//parameter's name in the same edit.
		sSignature.Init();
		sChange.Init();
		aiStarts.Init();
		aiEnds.Init();
		if (!mpcContent->GetChangeSignature()->FindSignature(&sSignature, psIntroduction->szFileName.Text(), psIntroduction->iFunctionLine, psIntroduction->iFunctionColumn) || !sSignature.bChangeable)
		{
			szProblem = "its function's signature can't be changed";
			bResult = false;
		}
		else
		{
			for (i = 0; i < sSignature.aszOldTypes.NumElements(); i++)
			{
				sChange.asParameters.Add()->Init((int32)i);
			}
			psNew = sChange.asParameters.Add();
			psNew->Init(-1);
			psNew->szType.Set(psIntroduction->szType);
			psNew->szName.Set(szName);
			psNew->szCallValue.Set(psIntroduction->szExpression);
			for (i = uiFirst; i <= uiLast; i++)
			{
				psOccurrence = psIntroduction->asOccurrences.Get(i);
				aiStarts.Add((int)psOccurrence->uiStart);
				aiEnds.Add((int)psOccurrence->uiEnd);
				uiReplaced++;
			}
			aszConflicts.Init();
			mpcContent->GetChangeSignature()->FindConflicts(&sSignature, &sChange, &aszConflicts);
			bResult = (aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&aszConflicts);
			aszConflicts.Kill();
			if (bResult)
			{
				mpcContent->GetChangeSignature()->SetExtraEdits(psIntroduction->szFileName.Text(), &aiStarts, &aiEnds, szName);
				szMessage.Init();
				bResult = mpcContent->GetChangeSignature()->Change(&sSignature, &sChange, &szMessage);
				szMessage.Kill();
			}
		}
		aiEnds.Kill();
		aiStarts.Kill();
		sChange.Kill();
		sSignature.Kill();
	}
	else
	{
		cFiles.Init(mpcContent);
		pcEdits = cFiles.GetEdits(psIntroduction->szFileName.Text());
		szText = cFiles.GetText(psIntroduction->szFileName.Text());
		if (iKind == IDE_INTRODUCE_FIELD)
		{
			//Its declaration in the class, and its assignment before the statement (or the statement, made it).
			szLine.Init("\t");
			szLine.Append(psIntroduction->szType);
			szLine.Append('\t');
			szLine.Append(szName);
			szLine.Append(";\n");
			if (!mpcContent->GetMoveMembers()->AddMember(psIntroduction->szClassKey.Text(), -1, szLine.Text(), &cFiles))
			{
				szProblem = "its class's definition wasn't found in the solution";
				bResult = false;
			}
			szLine.Kill();
			if (bResult && psIntroduction->bInInit)
			{
				//Assigned in the class's Init (question 31), not where it was.
				if (bStatement)
				{
					szProblem = "the expression is a statement of its own, so it is assigned where it is";
					bResult = false;
				}
				else
				{
					szLine.Init(szName);
					szLine.Append(" = ")->Append(psIntroduction->szExpression)->Append(';');
					if (AddInitAssignments(psIntroduction->szClassKey.Text(), szLine.Text(), &cFiles) == 0)
					{
						szProblem = "its class defines no Init to assign it in";
						bResult = false;
					}
					szLine.Kill();
				}
			}
			else if (bResult && !bStatement)
			{
				szLine.Init();
				for (i = uiAnchor; (szText[i] == ' ') || (szText[i] == '\t'); i++)
				{
					szLine.Append(szText[i]);
				}
				szLine.Append(szName);
				szLine.Append(" = ");
				szLine.Append(psIntroduction->szExpression);
				szLine.Append(";\n");
				pcEdits->Insert(uiAnchor, szLine.Text());
				szLine.Kill();
			}
		}
		else
		{
			//Its #define: in parentheses if it is more than one thing.
			szLine.Init();
			bAfter = FindDefinePlace(szText, &uiAt);
			if (bAfter)
			{
				//After an #include: two blank lines between them.
				for (i = uiAt - 1; (i > 0) && (szText[i - 1] != '\n'); i--)
				{
				}
				for (; (szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '#'); i++)
				{
				}
				if (strncmp(&szText[i], "include", 7) == 0)
				{
					szLine.Append("\n\n");
				}
			}
			szLine.Append("#define ");
			szLine.Append(szName);
			szLine.Append('\t');
			bStatement = false;
			for (i = 0; i < psIntroduction->szExpression.Length(); i++)
			{
				bStatement = bStatement || !(IsIDENameChar(psIntroduction->szExpression.Text()[i]) || (psIntroduction->szExpression.Text()[i] == '.'));
			}
			if (bStatement && (psIntroduction->szExpression.Text()[0] != '"') && (psIntroduction->szExpression.Text()[0] != '\'') && (psIntroduction->szExpression.Text()[0] != '('))
			{
				szLine.Append('(')->Append(psIntroduction->szExpression)->Append(')');
			}
			else
			{
				szLine.Append(psIntroduction->szExpression);
			}
			bStatement = false;
			szLine.Append("\n");
			if (!bAfter)
			{
				szLine.Append("\n\n");
			}
			pcEdits->Insert(uiAt, szLine.Text());
			szLine.Kill();
		}
		for (i = 0; bResult && (iKind == IDE_INTRODUCE_CONSTANT) && psIntroduction->bWholeFile && (i < psIntroduction->asFileOccurrences.NumElements()); i++)
		{
			//A constant, everywhere in the file it is written.
			psOccurrence = psIntroduction->asFileOccurrences.Get(i);
			pcEdits->Replace(psOccurrence->uiStart, psOccurrence->uiEnd, szName);
			uiReplaced++;
		}
		for (i = uiFirst; bResult && !((iKind == IDE_INTRODUCE_CONSTANT) && psIntroduction->bWholeFile) && (i <= uiLast); i++)
		{
			psOccurrence = psIntroduction->asOccurrences.Get(i);
			if (bStatement && (i == uiFirst))
			{
				szLine.Init(szName);
				szLine.Append(" = ");
				szLine.Append(psIntroduction->szExpression);
				pcEdits->Replace(psOccurrence->uiStart, psOccurrence->uiEnd, szLine.Text());
				szLine.Kill();
			}
			else
			{
				pcEdits->Replace(psOccurrence->uiStart, psOccurrence->uiEnd, szName);
				uiReplaced++;
			}
		}
		if (bResult)
		{
			cFiles.Apply();
		}
		cFiles.Kill();
	}

	szMessage.Init();
	if (bResult)
	{
		szMessage.Append((iKind == IDE_INTRODUCE_FIELD) ? "Introduced the field " : ((iKind == IDE_INTRODUCE_CONSTANT) ? "Introduced the constant " : "Introduced the parameter "));
		if (iKind != IDE_INTRODUCE_CONSTANT)
		{
			szMessage.Append(psIntroduction->szType);
			szMessage.Append(' ');
		}
		szMessage.Append(szName);
		szMessage.Append(" = ");
		szMessage.Append(psIntroduction->szExpression);
		szMessage.Append(": ");
		szMessage.Append((uint32)uiReplaced);
		szMessage.Append(uiReplaced == 1 ? " occurrence replaced." : " occurrences replaced.");
		ShowIntroduceStatus(&szMessage, pszStatus);
	}
	else if (szProblem)
	{
		szMessage.Append("Can't introduce it: ");
		szMessage.Append(szProblem);
		szMessage.Append(".");
		ShowIntroduceStatus(&szMessage, pszStatus);
	}
	szMessage.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What an introduction found, for checking.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEIntroduceVariable::PrintIntroduction(SIDEIntroduction* psIntroduction, CChars* pszDest)
{
	size	i;

	pszDest->Append("Introduce Variable: ");
	if (!psIntroduction->bPossible)
	{
		pszDest->Append("can't: ")->Append(psIntroduction->szWhyNot)->AppendNewLine();
		return pszDest->Text();
	}
	pszDest->Append('[')->Append(psIntroduction->szExpression)->Append("] is [")->Append(psIntroduction->szType)->Append("], named [")->Append(psIntroduction->szSuggested);
	pszDest->Append("], ")->Append((uint32)psIntroduction->asOccurrences.NumElements())->Append(psIntroduction->asOccurrences.NumElements() == 1 ? " occurrence" : " occurrences")->AppendNewLine();
	for (i = 0; i < psIntroduction->aszWarnings.NumElements(); i++)
	{
		pszDest->Append("  Replacing all: ")->Append(psIntroduction->aszWarnings.GetText(i))->AppendNewLine();
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceVariable::ShowIntroduceStatus(CChars* pszMessage, CChars* pszStatus)
{
	if (pszStatus)
	{
		pszStatus->Append(pszMessage);
	}
	else
	{
		mpcContent->ShowStatus(pszMessage->Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The variable introduced, as one edit of the file's editor: declared
// with the function's declarations (their names moved over to its column
// if its type is longer than theirs allow), assigned before the statement
// that first uses it (or the statement, if it is only the expression,
// made the assignment), and each occurrence replaced by its name: all of
// them (bAll) or the selected one.  The caret is left on its name in the
// assignment.  False if the name can't be used.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::Introduce(SIDEIntroduction* psIntroduction, const char* szName, bool bAll, CChars* pszStatus, int iKind)
{
	CIDETextEdits		cEdits;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CChars				szMessage;
	CChars				szLine;
	CChars				szTabs;
	SIDEDeclaredName*	psDeclared;
	STextRange*			psOccurrence;
	const char*			szProblem;
	size				uiNameColumn;
	size				uiTypeEnd;
	size				uiAnchor;
	size				uiFirst;
	size				uiLast;
	size				uiCaret;
	size				uiReplaced;
	size				i;
	bool				bStatement;
	bool				bUniform;

	szProblem = psIntroduction->bPossible ? GetNameProblem(psIntroduction, szName, iKind) : "It can't be introduced.";
	pTab = mpcContent->GetDocuments()->FindTab(psIntroduction->szFileName.Text());
	if (!szProblem && psIntroduction->bPossible && GetKindProblem(psIntroduction, iKind))
	{
		mszNameProblem.Set("Can't introduce it: ");
		mszNameProblem.Append(GetKindProblem(psIntroduction, iKind));
		mszNameProblem.Append(".");
		ShowIntroduceStatus(&mszNameProblem, pszStatus);
		return false;
	}
	if (szProblem || pTab.IsNull())
	{
		szMessage.Init("Introduce Variable: ");
		szMessage.Append(szProblem ? szProblem : "The file isn't open.");
		ShowIntroduceStatus(&szMessage, pszStatus);
		szMessage.Kill();
		return false;
	}
	if (iKind != IDE_INTRODUCE_VARIABLE)
	{
		return IntroduceElsewhere(psIntroduction, szName, bAll, pszStatus, iKind);
	}
	pEditor = pTab->GetEditor();

	uiFirst = bAll ? 0 : psIntroduction->uiSelected;
	uiLast = bAll ? psIntroduction->asOccurrences.NumElements() - 1 : psIntroduction->uiSelected;
	uiAnchor = (uiFirst == uiLast) ? psIntroduction->uiAnchorOne : psIntroduction->uiAnchorAll;
	bStatement = (uiFirst == uiLast) ? psIntroduction->bStatementOne : psIntroduction->bStatementAll;

	cEdits.Init();

	//The declaration: its name in the column the function's declarations have theirs in, or the next the type allows.
	uiTypeEnd = FindIDEVisualColumn(psIntroduction->szIndent.Text(), 0, psIntroduction->szIndent.Length(), IDE_TAB_WIDTH) + psIntroduction->szType.Length();
	uiNameColumn = ((uiTypeEnd / IDE_TAB_WIDTH) + 1) * IDE_TAB_WIDTH;
	bUniform = true;
	for (i = 0; i < psIntroduction->asDeclared.NumElements(); i++)
	{
		psDeclared = psIntroduction->asDeclared.Get(i);
		bUniform &= (psDeclared->uiNameColumn == psIntroduction->asDeclared.Get(0)->uiNameColumn);
	}
	if (bUniform && (psIntroduction->asDeclared.NumElements() > 0))
	{
		if (psIntroduction->asDeclared.Get(0)->uiNameColumn >= uiNameColumn)
		{
			uiNameColumn = psIntroduction->asDeclared.Get(0)->uiNameColumn;
		}
		else
		{
			//Its type is longer: the others' names move over to its column.
			for (i = 0; i < psIntroduction->asDeclared.NumElements(); i++)
			{
				psDeclared = psIntroduction->asDeclared.Get(i);
				szTabs.Init();
				AppendIDETabsTo(&szTabs, psDeclared->uiSpaceColumn, uiNameColumn, IDE_TAB_WIDTH);
				cEdits.Replace(psDeclared->uiSpaceStart, psDeclared->uiSpaceEnd, szTabs.Text());
				szTabs.Kill();
			}
		}
	}
	szLine.Init(psIntroduction->szIndent);
	szLine.Append(psIntroduction->szType);
	AppendIDETabsTo(&szLine, uiTypeEnd, uiNameColumn, IDE_TAB_WIDTH);
	szLine.Append(szName);
	szLine.Append(";\n");
	if (!psIntroduction->bDeclarations)
	{
		szLine.Append("\n");
	}
	cEdits.Insert(psIntroduction->uiDeclaration, szLine.Text());
	szLine.Kill();

	//The assignment, before the statement; or the statement itself.
	uiCaret = uiAnchor;
	if (!bStatement)
	{
		szLine.Init();
		szLine.AppendSubString(pEditor->GetText(), uiAnchor, uiAnchor);
		for (i = uiAnchor; (pEditor->GetText()[i] == ' ') || (pEditor->GetText()[i] == '\t'); i++)
		{
			szLine.Append(pEditor->GetText()[i]);
		}
		szLine.Append(szName);
		szLine.Append(" = ");
		szLine.Append(psIntroduction->szExpression);
		szLine.Append(";\n");
		cEdits.Insert(uiAnchor, szLine.Text());
		szLine.Kill();
	}

	uiReplaced = 0;
	for (i = uiFirst; i <= uiLast; i++)
	{
		psOccurrence = psIntroduction->asOccurrences.Get(i);
		if (bStatement && (i == uiFirst))
		{
			szLine.Init(szName);
			szLine.Append(" = ");
			szLine.Append(psIntroduction->szExpression);
			cEdits.Replace(psOccurrence->uiStart, psOccurrence->uiEnd, szLine.Text());
			szLine.Kill();
			uiCaret = psOccurrence->uiStart;
		}
		else
		{
			cEdits.Replace(psOccurrence->uiStart, psOccurrence->uiEnd, szName);
			uiReplaced++;
		}
	}

	//The caret on the name in the assignment: where the statement was, after what was put in before it.
	uiCaret = bStatement ? cEdits.AdjustPosition(uiCaret) : cEdits.AdjustPosition(uiAnchor) - (strlen(szName) + 3 + psIntroduction->szExpression.Length() + 2);
	cEdits.Apply(pEditor);
	cEdits.Kill();
	pEditor->SetCaretPosition(uiCaret);

	szMessage.Init("Introduced ");
	szMessage.Append(psIntroduction->szType);
	szMessage.Append(' ');
	szMessage.Append(szName);
	szMessage.Append(" = ");
	szMessage.Append(psIntroduction->szExpression);
	szMessage.Append(": ");
	if (bStatement)
	{
		szMessage.Append("the statement is its assignment");
		szMessage.Append((uiReplaced > 0) ? ", " : ".");
	}
	if (!bStatement || (uiReplaced > 0))
	{
		szMessage.Append((uint32)uiReplaced);
		szMessage.Append(uiReplaced == 1 ? " occurrence replaced." : " occurrences replaced.");
	}
	ShowIntroduceStatus(&szMessage, pszStatus);
	szMessage.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's name must be one the variable can have.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::ApproveName(void)
{
	const char*	szProblem;

	szProblem = GetNameProblem(&msShown, mpNameField->GetText(), miKind);
	mpError->SetText(szProblem ? szProblem : "");
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Introduce Variable for the active editor's selection: a
// dialog with the variable's name (one is suggested), its type, and
// whether every occurrence of the expression is replaced.  False if
// nothing was introduced.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceVariable::ShowIntroduce(int iKind)
{
	CChars		szText;
	Ptr<CLabel>	pLabel;
	bool		bResult;
	bool		bAll;

	CArrayChars	aszCandidates;
	size		uiStart;
	size		uiEnd;
	size		uiCandidates;
	size		i;

	if (iKind >= 0)
	{
		miKind = iKind;
	}

	//Nothing selected: the expressions at the caret are offered, or the only one taken.
	if (mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && !mpcContent->GetDocuments()->GetActiveTab()->GetEditor()->GetSelection(&uiStart, &uiEnd))
	{
		aszCandidates.Init();
		uiCandidates = FindCandidates(&aszCandidates);
		if (uiCandidates > 1)
		{
			if (mpChooser.IsNull())
			{
				mpChooser = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
				mpChooser->GetItemClickEvent()->Add(this, &CIDEIntroduceVariable::ChooserClicked);
			}
			mpChooser->RemoveAllItems();
			mpChooser->AddItem("Expressions")->SetEnabled(false);
			for (i = 0; i < uiCandidates; i++)
			{
				mpChooser->AddItem(aszCandidates.GetText(i));
			}
			mpcContent->GetUsages()->OpenCaretPopup(mpChooser, 1);
			aszCandidates.Kill();
			return false;
		}
		aszCandidates.Kill();
		if (uiCandidates == 1)
		{
			ChooseCandidate(0);
		}
	}

	msShown.Kill();
	msShown.Init();
	if (!FindIntroduction(&msShown))
	{
		mpcContent->ShowStatus("There is no expression at the caret to introduce a variable for.");
		return false;
	}
	if (!msShown.bPossible || GetKindProblem(&msShown, miKind))
	{
		szText.Init("Can't introduce it: ");
		szText.Append(msShown.bPossible ? GetKindProblem(&msShown, miKind) : msShown.szWhyNot.Text());
		szText.Append(".");
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
		return false;
	}

	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), (miKind == IDE_INTRODUCE_FIELD) ? "Introduce Field" :
						((miKind == IDE_INTRODUCE_CONSTANT) ? "Introduce Constant" : ((miKind == IDE_INTRODUCE_PARAMETER) ? "Introduce Parameter" : "Introduce Variable")));
	szText.Init();
	AppendSuggestedName(&msShown, miKind, &szText);
	mpNameField = mpDialog->AddTextField("Name:", szText.Text());
	szText.Kill();
	mpNameField->SelectAll();
	szText.Init(msShown.szType);
	szText.Append("  =  ");
	szText.Append(msShown.szExpression);
	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), szText.Text());
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	mpDialog->AddEditor("Type:", pLabel);
	pLabel = NULL;
	szText.Kill();
	mpAllBox = NULL;
	if (msShown.asOccurrences.NumElements() > 1)
	{
		szText.Init("Replace all ");
		szText.Append((uint32)msShown.asOccurrences.NumElements());
		szText.Append(" occurrences");
		mpAllBox = mpDialog->AddCheckBox(szText.Text(), mbAll);
		szText.Kill();
	}
	mpFileBox = NULL;
	if ((miKind == IDE_INTRODUCE_CONSTANT) && (msShown.asFileOccurrences.NumElements() > msShown.asOccurrences.NumElements()))
	{
		//Every place in the file: not ticked until it is asked for.
		szText.Init("Replace all ");
		szText.Append((uint32)msShown.asFileOccurrences.NumElements());
		szText.Append(" occurrences in the file");
		mpFileBox = mpDialog->AddCheckBox(szText.Text(), false);
		szText.Kill();
	}
	mpInitBox = NULL;
	if ((miKind == IDE_INTRODUCE_FIELD) && !msShown.bUsesLocals && (AddInitAssignments(msShown.szClassKey.Text(), "", NULL) > 0))
	{
		//The class's Init can assign it: the expression names nothing of the function's own.
		mpInitBox = mpDialog->AddCheckBox("Assign it in Init", mbInInit);
	}
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEIntroduceVariable::ApproveName);

	bResult = false;
	mpcContent->GetScript()->WatchDialog(mpDialog);
	if (mpDialog->Show(480))
	{
		bAll = false;
		if (mpAllBox.IsNotNull())
		{
			mbAll = mpAllBox->IsTicked();
			bAll = mbAll;
		}
		msShown.bWholeFile = mpFileBox.IsNotNull() && mpFileBox->IsTicked();

		//What replacing all of them could change is shown first: Cancel there introduces nothing.
		if (!bAll || (msShown.aszWarnings.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&msShown.aszWarnings))
		{
			szText.Init(mpNameField->GetText());
			if (mpInitBox.IsNotNull())
			{
				mbInInit = mpInitBox->IsTicked();
				msShown.bInInit = mbInInit;
			}
			bResult = Introduce(&msShown, szText.Text(), bAll, NULL, miKind);
			szText.Kill();
		}
	}
	mpNameField = NULL;
	mpError = NULL;
	mpAllBox = NULL;
	mpInitBox = NULL;
	mpFileBox = NULL;
	mpDialog = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Introduce Field's "Assign it in Init" (question 31): szStatement
// ("miSize = 4;") at the end of each Init the class szClassKey defines,
// before the PostInit() it ends with if it does, as an edit of pcFiles.
// With pcFiles NULL nothing is written: how many Inits there are is what
// is asked.  The number of Inits.
//
//////////////////////////////////////////////////////////////////////////
size CIDEIntroduceVariable::AddInitAssignments(const char* szClassKey, const char* szStatement, CIDEFileEdits* pcFiles)
{
	CIDECodeModel*					pcModel;
	CIDEMoveMembers*				pcMembers;
	CIDERefactorUnit				cUnit;
	CArrayIDEFileRanges				asRanges;
	CArrayIDELocations				asLocations;
	CArrayChars						aszKeys;
	SIDEMoveClass					sClass;
	SIDELocation*					psLocation;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CCPPSyntaxCompoundStatement*	pcBody;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcLast;
	SCPPSyntaxRange					sRange;
	CChars							szScope;
	CChars							szName;
	CChars							szLine;
	CChars							szLast;
	char*							szText;
	size							uiAt;
	size							uiIndent;
	size							uiInits;
	size							i;
	size							j;
	int32							iToken;
	uint8							uiKind;
	bool							bBeforeLast;

	pcModel = mpcContent->GetCodeModel();
	pcMembers = &mpcContent->GetMoveMembers();
	uiInits = 0;
	sClass.Init();
	if (!pcMembers->ReadClass(szClassKey, &sClass))
	{
		pcMembers->KillUnits();
		sClass.Kill();
		return 0;
	}
	pcMembers->KillUnits();
	asRanges.Init();
	asRanges.Add()->Init(sClass.szFileName.Text(), sClass.sRange.iLine, sClass.sRange.iColumn, sClass.sRange.iEndLine, sClass.sRange.iEndColumn);
	sClass.Kill();
	aszKeys.Init();
	pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		szScope.Init();
		szName.Init();
		GetIDESymbolScope(aszKeys.GetText(i), &szScope);
		GetIDESymbolName(aszKeys.GetText(i), &szName);
		asLocations.Init();
		if (szScope.Equals(szClassKey) && szName.Equals("Init") && pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind) && (uiKind == CPPSK_Function))
		{
			for (j = 0; j < asLocations.NumElements(); j++)
			{
				psLocation = asLocations.Get(j);
				if (psLocation->uiRole != CPPIR_Definition)
				{
					continue;
				}
				uiInits++;
				szText = pcFiles ? pcFiles->GetText(psLocation->szFileName.Text()) : NULL;
				cUnit.Init();
				if (szText && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psLocation->szFileName.Text(), false))
				{
					iToken = cUnit.FindTokenAt(psLocation->iLine, psLocation->iColumn);
					pcNode = (iToken != -1) ? cUnit.FindNodeOfTokens(iToken, iToken) : NULL;
					pcFunction = pcNode ? (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcNode, CPPS_FunctionDefinition) : NULL;
					pcBody = (pcFunction && pcFunction->mpcBody && (pcFunction->mpcBody->GetKind() == CPPS_CompoundStatement)) ? (CCPPSyntaxCompoundStatement*)pcFunction->mpcBody : NULL;

					//Before a PostInit() it ends with, in its indent; otherwise before its closing brace, a tab in from it.
					bBeforeLast = false;
					pcLast = (pcBody && (pcBody->macpStatements.NumElements() > 0)) ? pcBody->macpStatements.GetPtr(pcBody->macpStatements.NumElements() - 1) : NULL;
					if (pcLast && cUnit.GetRange(pcLast, &sRange))
					{
						szLast.Init();
						AppendIDERangeText(szText, &sRange, &szLast);
						bBeforeLast = szLast.StartsWith("PostInit(");
						szLast.Kill();
					}
					if (pcBody && (bBeforeLast || cUnit.GetTokensRange(pcBody->miLastToken, pcBody->miLastToken, &sRange)))
					{
						uiAt = FindIDETextOffset(szText, sRange.iLine, 0);
						for (uiIndent = uiAt; (szText[uiIndent] == ' ') || (szText[uiIndent] == '\t'); uiIndent++)
						{
						}
						szLine.Init();
						szLine.AppendSubString(szText, uiAt, uiIndent);
						szLine.Append(bBeforeLast ? "" : "\t")->Append(szStatement)->Append('\n');
						pcFiles->GetEdits(psLocation->szFileName.Text())->Insert(uiAt, szLine.Text());
						szLine.Kill();
					}
				}
				cUnit.Kill();
			}
		}
		KillIDELocations(&asLocations);
		szName.Kill();
		szScope.Kill();
	}
	aszKeys.Kill();
	KillIDEFileRanges(&asRanges);
	return uiInits;
}
