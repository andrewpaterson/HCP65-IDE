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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/ConstructorCall.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEChangeSignature.h"
#include "IDEInlineVariable.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEInlining::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szFileName.Init();
	szKey.Init();
	szName.Init();
	szValue.Init();
	aiUseStarts.Init();
	aiUseEnds.Init();
	aiUseWrapped.Init();
	uiDeclaration = 0;
	uiDeclarationEnd = 0;
	bAssignment = false;
	uiAssignment = 0;
	uiAssignmentEnd = 0;
	aszConflicts.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEInlining::Kill(void)
{
	aszConflicts.Kill();
	aiUseWrapped.Kill();
	aiUseEnds.Kill();
	aiUseStarts.Kill();
	szValue.Kill();
	szName.Kill();
	szKey.Kill();
	szFileName.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineVariable::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineVariable::Class(void)
{
	U_Pointer(mpcContent);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineVariable::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::Save(CObjectWriter* pcFile) { return false; }
bool CIDEInlineVariable::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Ctrl+Alt+N, as IntelliJ IDEA's: Inline, of a variable or of a function
// (CIDEInlineFunction), whichever the caret is on.
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineVariable::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.InlineVariable", "Inline...", "Control+Alt+N", Ptr<CImageCel>(), this, &CIDEInlineVariable::QueueInline)->SetEnabledCheck(this, &CIDEInlineVariable::CanInline);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineVariable::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.InlineVariable"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::CanInline(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	//A macro's name is no symbol of the index: any name may be one.
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && (mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) || mpcContent->GetConvertMacro()->CanConvert());
}


void CIDEInlineVariable::QueueInline(void) { mpcContent->GetWindow()->Post(this, &CIDEInlineVariable::ShowQueuedInline); }
void CIDEInlineVariable::ShowQueuedInline(void) { ShowInline(); }


//////////////////////////////////////////////////////////////////////////
//
// It can't be inlined: why, and at which line (from 0) if one says.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::Refuse(SIDEInlining* psDest, const char* szWhyNot, int32 iLine)
{
	psDest->bPossible = false;
	psDest->szWhyNot.Set(szWhyNot);
	if (iLine >= 0)
	{
		psDest->szWhyNot.Append(" at line ");
		psDest->szWhyNot.Append((int32)(iLine + 1));
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a value needs parentheses where it is written in place of a
// use of the variable: one made with an operator (iA + iB, a ? b : c)
// inside another operator's expression, or a member's or an element's
// object; one made with a unary operator or a cast as an object.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::IsWrapped(CCPPSyntax* pcValue, CCPPSyntax* pcUse)
{
	CCPPSyntax*	pcParent;
	ECPPSyntax	eValue;
	ECPPSyntax	eParent;
	ECppTokens	eOperator;
	bool		bObject;

	if (pcValue->GetKind() == CPPS_ParenthesisedExpression)
	{
		return false;
	}
	pcParent = pcUse->mpcParent;
	if (!pcParent)
	{
		return false;
	}
	eValue = pcValue->GetKind();
	eParent = pcParent->GetKind();
	bObject = ((eParent == CPPS_MemberExpression) && (((CCPPSyntaxMemberExpression*)pcParent)->mpcObject == pcUse)) ||
			  ((eParent == CPPS_SubscriptExpression) && (pcParent->miFirstToken == pcUse->miFirstToken)) ||
			  ((eParent == CPPS_CallExpression) && (((CCPPSyntaxCallExpression*)pcParent)->mpcCallee == pcUse));
	if ((eValue == CPPS_BinaryExpression) || (eValue == CPPS_ConditionalExpression))
	{
		if (eParent == CPPS_BinaryExpression)
		{
			//What is assigned is the whole of the right of an assignment.
			eOperator = ((CCPPSyntaxBinaryExpression*)pcParent)->meOperator;
			return !(((eOperator == CT_Assignment) || (eOperator == CT_Comma) || (eOperator == CT_AssignmentAdd) || (eOperator == CT_AssignmentSubtract) || (eOperator == CT_AssignmentMultiply) ||
					  (eOperator == CT_AssignmentDivide) || (eOperator == CT_AssignmentModulus) || (eOperator == CT_AssignmentBitwiseAnd) || (eOperator == CT_AssignmentBitwiseOr) ||
					  (eOperator == CT_AssignmentBitwiseXor) || (eOperator == CT_AssignmentLeftShift) || (eOperator == CT_AssignmentRightShift)) &&
					 (((CCPPSyntaxBinaryExpression*)pcParent)->mpcRight == pcUse));
		}
		return bObject || (eParent == CPPS_UnaryExpression) || (eParent == CPPS_CastExpression) || (eParent == CPPS_ConditionalExpression);
	}
	if ((eValue == CPPS_UnaryExpression) || (eValue == CPPS_CastExpression) || (eValue == CPPS_NewExpression))
	{
		return bObject;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What stands in the way of writing the value where the variable was
// used: it does something and would be done another number of times; or
// a variable it reads is changed after it and before the last use.
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineVariable::FindValueConflicts(CIDERefactorUnit* pcUnit, SCPPSyntaxRange* psValue, int32 iLastUseLine, size uiUses, bool bEffects, SIDEInlining* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	CArrayChars			aszKeys;
	SIDELocation*		psLocation;
	CChars				szLine;
	size				i;
	size				j;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	if (bEffects && (uiUses != 1))
	{
		szLine.Init("Its value does something (a call, an assignment), which would be done ");
		if (uiUses == 0)
		{
			szLine.Append("no more: nothing uses ");
			szLine.Append(psDest->szName);
			szLine.Append(".");
		}
		else
		{
			szLine.Append((uint32)uiUses);
			szLine.Append(" times.");
		}
		psDest->aszConflicts.Add(szLine.Text());
		szLine.Kill();
	}

	asRanges.Init();
	asRanges.Add()->Init(psDest->szFileName.Text(), psValue->iLine, psValue->iColumn, psValue->iEndLine, psValue->iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsUsedIn(&asRanges, &aszKeys);
	KillIDEFileRanges(&asRanges);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		asLocations.Init();
		if (pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind) && ((uiKind == CPPSK_Variable) || (uiKind == CPPSK_Parameter) || (uiKind == CPPSK_Field)))
		{
			for (j = 0; j < asLocations.NumElements(); j++)
			{
				psLocation = asLocations.Get(j);
				if ((psLocation->uiUsage == CPPUT_Write) && (StringInsensitiveCompare(psLocation->szFileName.Text(), psDest->szFileName.Text()) == 0) &&
					((psLocation->iLine > psValue->iEndLine) || ((psLocation->iLine == psValue->iEndLine) && (psLocation->iColumn >= psValue->iEndColumn))) && (psLocation->iLine <= iLastUseLine))
				{
					szLine.Init();
					GetIDESymbolName(aszKeys.GetText(i), &szLine);
					szLine.Append(", which its value reads, is changed at line ");
					szLine.Append((int32)(psLocation->iLine + 1));
					szLine.Append(", before ");
					szLine.Append(psDest->szName);
					szLine.Append(" is last used.");
					psDest->aszConflicts.Add(szLine.Text());
					szLine.Kill();
					break;
				}
			}
		}
		KillIDELocations(&asLocations);
	}
	aszKeys.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Inline Variable has to work with, for the variable named at the
// caret.  False if there is no variable there; psDest->bPossible says
// whether it can be inlined, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::FindInlining(SIDEInlining* psDest)
{
	CIDECodeModel*					pcModel;
	Ptr<CIDEEditorTab>				pTab;
	CIDERefactorUnit				cUnit;
	CArrayIDELocations				asLocations;
	CArraySyntax					apcUses;
	SIDELocation*					psLocation;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcUse;
	CCPPSyntax*						pcValue;
	CCPPSyntax*						pcStatement;
	CCPPSyntax*						pcBlock;
	CCPPSyntax*						pcParent;
	CCPPSyntaxDeclarator*			pcDeclarator;
	CCPPSyntaxInitDeclarator*		pcInit;
	CCPPSyntaxSimpleDeclaration*	pcDeclaration;
	CCPPSyntaxBinaryExpression*		pcAssign;
	CCPPSyntaxBinaryExpression*		pcBinary;
	CIDEEffectFinder				cEffects;
	SCPPSyntaxRange					sRange;
	SCPPSyntaxRange					sValue;
	char*							szText;
	int								iLine;
	int								iColumn;
	int32							iToken;
	int32							iLastUseLine;
	size							i;
	uint8							uiRole;
	uint8							uiKind;
	uint8							uiChanges;
	bool							bInBlock;

	pcModel = mpcContent->GetCodeModel();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) || !pcModel->GetSymbolAt(pTab->GetFileName(), iLine, iColumn, &psDest->szKey, &uiKind) || (uiKind != CPPSK_Variable))
	{
		return false;
	}
	psDest->szFileName.Set(pTab->GetFileName());
	GetIDESymbolName(psDest->szKey.Text(), &psDest->szName);
	//A function's own variable is keyed by where it is declared ("@file:line:column:name").
	if (psDest->szKey.Text()[0] != '@')
	{
		return Refuse(psDest, "only a function's own variable is inlined");
	}

	cUnit.Init();
	if (!cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), pTab->GetFileName(), true) || !cUnit.IsChecked())
	{
		cUnit.Kill();
		return Refuse(psDest, "its file doesn't compile as it is: the compiler has to say how it is used");
	}
	szText = pTab->GetEditor()->GetText();
	asLocations.Init();
	pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
	apcUses.Init();
	pcDeclaration = NULL;
	pcInit = NULL;
	pcAssign = NULL;
	pcValue = NULL;
	pcStatement = NULL;
	psDest->bPossible = true;
	iLastUseLine = -1;
	for (i = 0; psDest->bPossible && (i < asLocations.NumElements()); i++)
	{
		psLocation = asLocations.Get(i);
		if (StringInsensitiveCompare(psLocation->szFileName.Text(), pTab->GetFileName()) != 0)
		{
			continue;
		}
		iToken = cUnit.FindTokenAt(psLocation->iLine, psLocation->iColumn);
		pcNode = cUnit.FindNodeOfTokens(iToken, iToken);
		if (!pcNode)
		{
			Refuse(psDest, "a macro names it", psLocation->iLine);
			break;
		}
		if (psLocation->uiRole != CPPIR_Reference)
		{
			//Its declaration, which declares nothing else.
			pcDeclarator = (CCPPSyntaxDeclarator*)cUnit.FindAncestor(pcNode, CPPS_Declarator);
			while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
			{
				pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
			}
			pcInit = (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_InitDeclarator)) ? (CCPPSyntaxInitDeclarator*)pcDeclarator->mpcParent : NULL;
			pcParent = pcInit ? pcInit->mpcParent : (pcDeclarator ? pcDeclarator->mpcParent : NULL);
			pcDeclaration = (pcParent && (pcParent->GetKind() == CPPS_SimpleDeclaration)) ? (CCPPSyntaxSimpleDeclaration*)pcParent : NULL;
			if (!pcDeclaration)
			{
				Refuse(psDest, "its declaration isn't a variable's alone (a parameter, a for's, a condition's)");
			}
			else if (pcDeclaration->macpDeclarators.NumElements() != 1)
			{
				Refuse(psDest, "it is declared with others in one declaration");
			}
			continue;
		}

		//A use: the name, in whatever parentheses.
		pcUse = cUnit.FindAncestor(pcNode, CPPS_IdExpression);
		if (!pcUse)
		{
			Refuse(psDest, "it is named where no expression is", psLocation->iLine);
			break;
		}
		while (pcUse->mpcParent && (pcUse->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
		{
			pcUse = pcUse->mpcParent;
		}
		uiChanges = cUnit.GetPass()->FindChanges(pcUse);
		pcBinary = (pcUse->mpcParent && (pcUse->mpcParent->GetKind() == CPPS_BinaryExpression)) ? (CCPPSyntaxBinaryExpression*)pcUse->mpcParent : NULL;
		if (pcBinary && (pcBinary->meOperator == CT_Assignment) && (pcBinary->mpcLeft == pcUse) && pcBinary->mpcParent && (pcBinary->mpcParent->GetKind() == CPPS_ExpressionStatement))
		{
			//The statement that gives it its value.
			if (pcAssign)
			{
				Refuse(psDest, "it is assigned more than once: again", psLocation->iLine);
			}
			pcAssign = pcBinary;
		}
		else if (uiChanges & CPPPC_ASSIGNED)
		{
			Refuse(psDest, "it is changed (assigned in an expression, ++, +=)", psLocation->iLine);
		}
		else if (uiChanges & CPPPC_ADDRESS)
		{
			Refuse(psDest, "its address is taken", psLocation->iLine);
		}
		else if (uiChanges & CPPPC_REFERENCE)
		{
			Refuse(psDest, "it is given to a reference that can change it", psLocation->iLine);
		}
		else if (uiChanges & CPPPC_MEMBER_CALL)
		{
			Refuse(psDest, "a function that can change it is called on it", psLocation->iLine);
		}
		else
		{
			apcUses.Add(pcUse);
			iLastUseLine = (psLocation->iLine > iLastUseLine) ? psLocation->iLine : iLastUseLine;
		}
	}
	KillIDELocations(&asLocations);

	//Its one value: its declaration's, or the one assignment's.
	if (psDest->bPossible && !pcDeclaration)
	{
		Refuse(psDest, "its declaration wasn't found");
	}
	if (psDest->bPossible && pcInit && (pcInit->meInitializer != CPPI_None))
	{
		if (pcInit->meInitializer != CPPI_Equals)
		{
			Refuse(psDest, "it isn't given its value with an =");
		}
		else if (pcAssign)
		{
			Refuse(psDest, "it is assigned again after its declaration gives it a value");
		}
		pcValue = pcInit->mpcInitializer;
	}
	else if (psDest->bPossible && pcAssign)
	{
		pcValue = pcAssign->mpcRight;
		pcStatement = pcAssign->mpcParent;
	}
	else if (psDest->bPossible)
	{
		Refuse(psDest, "it is never given a value");
	}
	if (psDest->bPossible && (!pcValue || !cUnit.GetRange(pcValue, &sValue)))
	{
		Refuse(psDest, "its value can't be read (a macro makes it)");
	}

	if (psDest->bPossible)
	{
		AppendIDERangeText(szText, &sValue, &psDest->szValue);

		//Its uses are after its value, and in the block the value is given in.
		pcBlock = pcStatement ? cUnit.FindAncestor(pcStatement->mpcParent, CPPS_CompoundStatement) : NULL;
		for (i = 0; psDest->bPossible && (i < apcUses.NumElements()); i++)
		{
			pcUse = apcUses.GetPtr(i);
			if (!cUnit.GetRange(pcUse, &sRange))
			{
				Refuse(psDest, "a macro uses it");
				break;
			}
			if ((sRange.iLine < sValue.iEndLine) || ((sRange.iLine == sValue.iEndLine) && (sRange.iColumn < sValue.iEndColumn)))
			{
				Refuse(psDest, "it is used before it has its value", sRange.iLine);
				break;
			}
			bInBlock = (pcBlock == NULL);
			for (pcParent = pcUse; pcParent && !bInBlock; pcParent = pcParent->mpcParent)
			{
				bInBlock = (pcParent == pcBlock);
			}
			if (!bInBlock)
			{
				Refuse(psDest, "it is given its value in a block that this use is outside: the use", sRange.iLine);
				break;
			}
			psDest->aiUseStarts.Add((int)FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
			psDest->aiUseEnds.Add((int)FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));
			psDest->aiUseWrapped.Add(IsWrapped(pcValue, pcUse) ? 1 : 0);
		}
	}
	if (psDest->bPossible)
	{
		//What goes: its declaration's statement, and the assignment's.
		pcParent = (pcDeclaration->mpcParent && (pcDeclaration->mpcParent->GetKind() == CPPS_DeclarationStatement)) ? pcDeclaration->mpcParent : pcDeclaration;
		if (!cUnit.GetRange(pcParent, &sRange))
		{
			Refuse(psDest, "its declaration can't be read (a macro makes it)");
		}
		else
		{
			psDest->uiDeclaration = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
			psDest->uiDeclarationEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		}
		if (psDest->bPossible && pcStatement)
		{
			if (!cUnit.GetRange(pcStatement, &sRange))
			{
				Refuse(psDest, "its assignment can't be read (a macro makes it)");
			}
			else
			{
				psDest->bAssignment = true;
				psDest->uiAssignment = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
				psDest->uiAssignmentEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
			}
		}
	}
	if (psDest->bPossible)
	{
		cEffects.mbEffects = false;
		cEffects.Look(pcValue);
		FindValueConflicts(&cUnit, &sValue, iLastUseLine, apcUses.NumElements(), cEffects.mbEffects, psDest);
	}
	apcUses.Kill();
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The variable inlined, as one edit.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::Inline(SIDEInlining* psInlining, CChars* pszStatus)
{
	Ptr<CIDEEditorTab>	pTab;
	CIDETextEdits		cEdits;
	CChars				szWrapped;
	CChars				szMessage;
	char*				szText;
	size				uiStart;
	size				uiEnd;
	size				i;

	pTab = mpcContent->GetDocuments()->FindTab(psInlining->szFileName.Text());
	if (pTab.IsNull() || !psInlining->bPossible)
	{
		return false;
	}
	szText = pTab->GetEditor()->GetText();
	szWrapped.Init("(");
	szWrapped.Append(psInlining->szValue);
	szWrapped.Append(")");
	cEdits.Init();
	for (i = 0; i < psInlining->aiUseStarts.NumElements(); i++)
	{
		cEdits.Replace((size)psInlining->aiUseStarts.GetValue(i), (size)psInlining->aiUseEnds.GetValue(i), psInlining->aiUseWrapped.GetValue(i) ? szWrapped.Text() : psInlining->szValue.Text());
	}
	uiStart = psInlining->uiDeclaration;
	uiEnd = psInlining->uiDeclarationEnd;
	ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);

	//The function's only declaration: the blank line that kept its declarations from its statements goes too.
	for (i = uiStart; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t') || (szText[i - 1] == '\r') || (szText[i - 1] == '\n')); i--)
	{
	}
	if ((i > 0) && (szText[i - 1] == '{') && (uiStart > 0) && (szText[uiStart - 1] == '\n'))
	{
		for (i = uiEnd; (szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r'); i++)
		{
		}
		if (szText[i] == '\n')
		{
			uiEnd = i + 1;
		}
	}
	cEdits.Replace(uiStart, uiEnd, "");
	if (psInlining->bAssignment)
	{
		uiStart = psInlining->uiAssignment;
		uiEnd = psInlining->uiAssignmentEnd;
		ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
		cEdits.Replace(uiStart, uiEnd, "");
	}
	cEdits.Apply(pTab->GetEditor());
	cEdits.Kill();
	szWrapped.Kill();

	szMessage.Init("Inlined ");
	szMessage.Append(psInlining->szName);
	szMessage.Append(": ");
	szMessage.Append((uint32)psInlining->aiUseStarts.NumElements());
	szMessage.Append((psInlining->aiUseStarts.NumElements() == 1) ? " use." : " uses.");
	if (pszStatus)
	{
		pszStatus->Set(szMessage.Text());
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEInlineVariable::PrintInlining(SIDEInlining* psInlining, CChars* pszDest)
{
	size	i;
	size	uiWrapped;

	pszDest->Append("Inline Variable: ")->Append(psInlining->szName);
	if (!psInlining->bPossible)
	{
		pszDest->Append(" can't be inlined: ")->Append(psInlining->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	uiWrapped = 0;
	for (i = 0; i < psInlining->aiUseWrapped.NumElements(); i++)
	{
		uiWrapped += (psInlining->aiUseWrapped.GetValue(i) != 0) ? 1 : 0;
	}
	pszDest->Append(" = [")->Append(psInlining->szValue)->Append("], ");
	pszDest->Append((uint32)psInlining->aiUseStarts.NumElements());
	pszDest->Append(" uses, ");
	pszDest->Append((uint32)uiWrapped);
	pszDest->Append(psInlining->bAssignment ? " in parentheses, assigned by a statement\n" : " in parentheses, given by its declaration\n");
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Inline Variable for the variable at the caret: Problems
// Detected if anything stands in the way, then the inlining.  False if
// nothing was changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::ShowInline(void)
{
	SIDEInlining	sInlining;
	CChars			szMessage;
	bool			bResult;
	bool			bMacro;

	sInlining.Init();
	if (!FindInlining(&sInlining))
	{
		//Not a variable: a macro or a function, then (the same key inlines any of them, as IntelliJ's does).
		sInlining.Kill();
		bResult = mpcContent->GetConvertMacro()->ShowInline(&bMacro);
		return bMacro ? bResult : mpcContent->GetInlineFunction()->ShowInline();
	}
	if (!sInlining.bPossible)
	{
		szMessage.Init("Can't inline ");
		szMessage.Append(sInlining.szName);
		szMessage.Append(": ");
		szMessage.Append(sInlining.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sInlining.Kill();
		return false;
	}
	bResult = (sInlining.aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&sInlining.aszConflicts);
	bResult = bResult && Inline(&sInlining);
	sInlining.Kill();
	return bResult;
}
