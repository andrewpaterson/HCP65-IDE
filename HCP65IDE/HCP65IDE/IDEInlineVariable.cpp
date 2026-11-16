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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/ConstructorCall.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEChangeSignature.h"
#include "IDEMemberQualifier.h"
#include "IDEDefinitions.h"
#include "IDEInlineVariable.h"
#include "IDEContextActions.h"


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
	M_Pointer(mpDialog);
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
// Show Context Actions: Inline is for a variable, a function or a macro.
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineVariable::AddContextActions(SIDECaretContext* psContext)
{
	if ((psContext->bSymbol && (psContext->bFunction || (psContext->uiKind == CPPSK_Variable) || ((psContext->uiKind == CPPSK_Field) && psContext->bStatic))) || psContext->bMacro)
	{
		psContext->Offer("Refactor.InlineVariable");
	}
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
//
//////////////////////////////////////////////////////////////////////////
void SIDEConstantInlining::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szKey.Init();
	szName.Init();
	szValue.Init();
	aszUseFiles.Init();
	aiUseStarts.Init();
	aiUseEnds.Init();
	aiUseWrapped.Init();
	uiCaretUse = MAX_SIZE;
	aszDeclarationFiles.Init();
	aiDeclarationStarts.Init();
	aiDeclarationEnds.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEConstantInlining::Kill(void)
{
	aiDeclarationEnds.Kill();
	aiDeclarationStarts.Kill();
	aszDeclarationFiles.Kill();
	aiUseWrapped.Kill();
	aiUseEnds.Kill();
	aiUseStarts.Kill();
	aszUseFiles.Kill();
	szValue.Kill();
	szName.Kill();
	szKey.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Inline has to work with for the constant at the caret: a variable
// of a file or of a class (not a function's own) that is declared const
// with a value that is a constant: it does nothing (no call, no ++) and
// names no variable that isn't const.  Its uses are found in every file
// of the solution that names it, each file parsed and checked as its
// project builds it.  False if there is no such variable at the caret;
// psDest->bPossible says whether it can be inlined, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::FindConstantInlining(SIDEConstantInlining* psDest)
{
	CIDECodeModel*							pcModel;
	Ptr<CIDEEditorTab>						pTab;
	CArrayTemplatePtr<CIDERefactorUnit>		apcUnits;
	CIDERefactorUnit*						pcUnit;
	CArrayIDELocations						asLocations;
	CArrayChars								aszFiles;
	CTextFile								cFile;
	SIDELocation*							psLocation;
	CCPPSyntax*								pcNode;
	CCPPSyntax*								pcUse;
	CCPPSyntax*								pcValue;
	CCPPSyntax*								pcParent;
	CCPPSyntaxDeclarator*					pcDeclarator;
	CCPPSyntaxInitDeclarator*				pcInit;
	CCPPSyntaxSimpleDeclaration*			pcDeclaration;
	CIDEEffectFinder						cEffects;
	SPreprocessedToken*						psSource;
	SCPPSyntaxRange							sRange;
	CChars									szNamed;
	CChars									szWhy;
	char*									szText;
	size									uiLength;
	size									uiPass;
	size									i;
	size									j;
	int										iLine;
	int										iColumn;
	int32									iToken;
	uint8									uiRole;
	uint8									uiKind;
	bool									bConst;

	pcModel = mpcContent->GetCodeModel();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) || !pcModel->GetSymbolAt(pTab->GetFileName(), iLine, iColumn, &psDest->szKey, &uiKind) ||
		((uiKind != CPPSK_Variable) && (uiKind != CPPSK_Field)) || (psDest->szKey.Text()[0] == '@'))
	{
		return false;
	}
	GetIDESymbolName(psDest->szKey.Text(), &psDest->szName);
	psDest->bPossible = true;

	//The files that name it, each parsed once; the declarations first, for its value.
	asLocations.Init();
	pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
	aszFiles.Init();
	apcUnits.Init();
	pcValue = NULL;
	for (uiPass = 0; psDest->bPossible && (uiPass < 2); uiPass++)
	{
		for (i = 0; psDest->bPossible && (i < asLocations.NumElements()); i++)
		{
			psLocation = asLocations.Get(i);
			if ((psLocation->uiRole == CPPIR_Reference) != (uiPass == 1))
			{
				continue;
			}
			for (j = 0; (j < aszFiles.NumElements()) && (StringInsensitiveCompare(aszFiles.GetText(j), psLocation->szFileName.Text()) != 0); j++)
			{
			}
			if (j == aszFiles.NumElements())
			{
				pcUnit = NewMalloc<CIDERefactorUnit>();
				pcUnit->Init();
				apcUnits.Add(pcUnit);
				aszFiles.Add(psLocation->szFileName.Text());
				if ((pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE) || !pcUnit->Parse(pcModel, &mpcContent->GetFindInFiles(), psLocation->szFileName.Text(), true) || !pcUnit->IsChecked())
				{
					szWhy.Init(GetIDEFileNamePart(psLocation->szFileName.Text()));
					szWhy.Append(", which names it, isn't the solution's or doesn't compile as it is");
					psDest->bPossible = false;
					psDest->szWhyNot.Set(szWhy);
					szWhy.Kill();
					break;
				}
			}
			pcUnit = apcUnits.GetPtr(j);
			cFile.Init();
			szText = NULL;
			mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength);
			iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
			pcNode = (iToken != -1) ? pcUnit->FindNodeOfTokens(iToken, iToken) : NULL;
			szWhy.Init();
			if (!pcNode || !szText)
			{
				szWhy.Set("a macro names it");
			}
			else if (uiPass == 0)
			{
				//A declaration: its own, const, and the one with a value gives the value.
				pcDeclarator = (CCPPSyntaxDeclarator*)pcUnit->FindAncestor(pcNode, CPPS_Declarator);
				while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
				{
					pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
				}
				pcInit = (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_InitDeclarator)) ? (CCPPSyntaxInitDeclarator*)pcDeclarator->mpcParent : NULL;
				pcParent = pcInit ? pcInit->mpcParent : NULL;
				pcDeclaration = (pcParent && (pcParent->GetKind() == CPPS_SimpleDeclaration)) ? (CCPPSyntaxSimpleDeclaration*)pcParent : NULL;
				bConst = false;
				for (iToken = (pcDeclaration && pcDeclaration->mpcSpecifiers) ? pcDeclaration->mpcSpecifiers->miFirstToken : 0;
					 pcDeclaration && pcDeclaration->mpcSpecifiers && (iToken >= 0) && (iToken <= pcDeclaration->mpcSpecifiers->miLastToken); iToken++)
				{
					bConst |= pcUnit->IsToken(iToken, "const") || pcUnit->IsToken(iToken, "constexpr");
				}
				if (!pcDeclaration || (pcDeclaration->macpDeclarators.NumElements() != 1) || !pcUnit->GetRange(pcDeclaration, &sRange))
				{
					szWhy.Set("it is declared with others in one declaration, or its declaration can't be read");
				}
				else if (!bConst || (pcDeclarator->NumPointers() > 0))
				{
					szWhy.Set("it isn't a const variable (and not a pointer or a reference)");
				}
				else
				{
					psDest->aszDeclarationFiles.Add(psLocation->szFileName.Text());
					psDest->aiDeclarationStarts.Add((int)FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
					psDest->aiDeclarationEnds.Add((int)FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));
					if (pcInit->mpcInitializer && pcUnit->IsExpression(pcInit->mpcInitializer) && !pcValue && pcUnit->GetRange(pcInit->mpcInitializer, &sRange))
					{
						pcValue = pcInit->mpcInitializer;
						psDest->szValue.AppendSubString(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));
						if (psDest->szValue.StartsWith("="))
						{
							psDest->szValue.RemoveFromStart(1);
						}
						psDest->szValue.StripWhitespace();

						//A constant: it does nothing, and what it names isn't a variable that can change.
						cEffects.mbEffects = false;
						cEffects.Look(pcValue);
						if (cEffects.mbEffects)
						{
							szWhy.Set("its value does something (a call, a ++): it isn't a constant");
						}
						for (iToken = pcValue->miFirstToken; szWhy.Empty() && (iToken <= pcValue->miLastToken); iToken++)
						{
							psSource = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)iToken));
							szNamed.Init();
							if (pcUnit->GetTokeniser()->Get((size)iToken)->IsIdentifier() && pcUnit->GetTokensRange(iToken, iToken, &sRange) &&
								pcModel->GetSymbolAt(psLocation->szFileName.Text(), sRange.iLine, sRange.iColumn, &szNamed, &uiKind) &&
								((uiKind == CPPSK_Variable) || (uiKind == CPPSK_Field) || (uiKind == CPPSK_Parameter)) && !mpcContent->GetMemberQualifier()->IsDeclaredWith(szNamed.Text(), "const"))
							{
								szWhy.Set("its value names ");
								szWhy.Append(psSource->szText, psSource->uiLength);
								szWhy.Append(", which isn't a constant");
							}
							szNamed.Kill();
						}
					}
				}
			}
			else
			{
				//A use: the name, with what it is written after and in whatever parentheses.
				pcUse = pcUnit->FindAncestor(pcNode, CPPS_IdExpression);
				while (pcUse && pcUse->mpcParent && (pcUse->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
				{
					pcUse = pcUse->mpcParent;
				}
				if (!pcUse || !pcUnit->GetRange(pcUse, &sRange))
				{
					szWhy.Set("it is named where no expression is, or by a macro");
				}
				else if (pcUnit->GetPass()->FindChanges(pcUse) & (CPPPC_ADDRESS | CPPPC_REFERENCE | CPPPC_ASSIGNED))
				{
					szWhy.Set("its address is taken, or it is given to a reference");
				}
				else
				{
					if ((StringInsensitiveCompare(psLocation->szFileName.Text(), pTab->GetFileName()) == 0) && (psLocation->iLine == iLine) && (iColumn >= psLocation->iColumn) &&
						(iColumn <= psLocation->iColumn + (int)psDest->szName.Length()))
					{
						psDest->uiCaretUse = psDest->aszUseFiles.NumElements();
					}
					psDest->aszUseFiles.Add(psLocation->szFileName.Text());
					psDest->aiUseStarts.Add((int)FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
					psDest->aiUseEnds.Add((int)FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));
					psDest->aiUseWrapped.Add((pcValue && IsWrapped(pcValue, pcUse)) ? 1 : 0);
				}
			}
			if (!szWhy.Empty())
			{
				psDest->bPossible = false;
				psDest->szWhyNot.Set(szWhy);
				psDest->szWhyNot.Append(" (")->Append(GetIDEFileNamePart(psLocation->szFileName.Text()))->Append(" line ")->Append((int32)(psLocation->iLine + 1))->Append(")");
			}
			szWhy.Kill();
			cFile.Kill();
		}
		if (psDest->bPossible && (uiPass == 0) && !pcValue)
		{
			psDest->bPossible = false;
			psDest->szWhyNot.Set("it isn't given a value where it is declared");
		}
	}
	for (i = 0; i < apcUnits.NumElements(); i++)
	{
		apcUnits.GetPtr(i)->Kill();
		free(apcUnits.GetPtr(i));
	}
	apcUnits.Kill();
	aszFiles.Kill();
	KillIDELocations(&asLocations);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The constant inlined: its value in place of every use and its
// declarations taken out; or, bThisOnly, in place of the use the caret is
// on, the constant staying.  One undo across the files, which are checked
// as they would be first.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::InlineConstant(SIDEConstantInlining* psInlining, bool bThisOnly, CChars* pszStatus)
{
	CIDEFileEdits	cFiles;
	CChars			szValue;
	CChars			szMessage;
	char*			szText;
	size			uiStart;
	size			uiEnd;
	size			uiUses;
	size			uiBefore;
	size			i;
	bool			bResult;

	bResult = psInlining->bPossible && (!bThisOnly || (psInlining->uiCaretUse != MAX_SIZE));
	cFiles.Init(mpcContent);
	uiUses = 0;
	for (i = 0; bResult && (i < psInlining->aszUseFiles.NumElements()); i++)
	{
		if (bThisOnly && (i != psInlining->uiCaretUse))
		{
			continue;
		}
		szText = cFiles.GetText(psInlining->aszUseFiles.GetText(i));
		bResult = (szText != NULL);
		if (bResult)
		{
			szValue.Init(psInlining->aiUseWrapped.GetValue(i) ? "(" : "");
			szValue.Append(psInlining->szValue)->Append(psInlining->aiUseWrapped.GetValue(i) ? ")" : "");
			cFiles.GetEdits(psInlining->aszUseFiles.GetText(i))->Replace((size)psInlining->aiUseStarts.GetValue(i), (size)psInlining->aiUseEnds.GetValue(i), szValue.Text());
			szValue.Kill();
			uiUses++;
		}
	}
	for (i = 0; bResult && !bThisOnly && (i < psInlining->aszDeclarationFiles.NumElements()); i++)
	{
		szText = cFiles.GetText(psInlining->aszDeclarationFiles.GetText(i));
		bResult = (szText != NULL);
		if (bResult)
		{
			uiStart = (size)psInlining->aiDeclarationStarts.GetValue(i);
			uiEnd = (size)psInlining->aiDeclarationEnds.GetValue(i);
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			cFiles.GetEdits(psInlining->aszDeclarationFiles.GetText(i))->Replace(uiStart, uiEnd, "");
		}
	}
	szMessage.Init();
	if (bResult)
	{
		uiBefore = cFiles.CountErrors(false);
		if (cFiles.CountErrors(true) > uiBefore)
		{
			bResult = false;
			szMessage.Append("Can't inline ")->Append(psInlining->szName)->Append(": the code doesn't compile with its value where it is used.");
		}
	}
	else
	{
		szMessage.Append("Can't inline ")->Append(psInlining->szName)->Append(": ")->Append(!psInlining->bPossible ? psInlining->szWhyNot.Text() : ((bThisOnly && (psInlining->uiCaretUse == MAX_SIZE)) ? "the caret isn't on a use of it" : "a file it is in couldn't be opened"))->Append('.');
	}
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Inlined ")->Append(psInlining->szName)->Append(" = ")->Append(psInlining->szValue)->Append(": ")->Append((uint32)uiUses)->Append((uiUses == 1) ? " use" : " uses");
		szMessage.Append(bThisOnly ? "; the constant stays." : ", and the constant is taken out.");
	}
	if (pszStatus)
	{
		pszStatus->Set(szMessage);
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	cFiles.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEInlineVariable::PrintConstantInlining(SIDEConstantInlining* psInlining, CChars* pszDest)
{
	CArrayChars	aszFiles;
	size		i;

	pszDest->Append("Inline Constant: ")->Append(psInlining->szName);
	if (!psInlining->bPossible)
	{
		pszDest->Append(" can't be: ")->Append(psInlining->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	aszFiles.Init();
	for (i = 0; i < psInlining->aszUseFiles.NumElements(); i++)
	{
		aszFiles.AddIfUnique(psInlining->aszUseFiles.GetText(i));
	}
	pszDest->Append(" = [")->Append(psInlining->szValue)->Append("], ")->Append((uint32)psInlining->aszUseFiles.NumElements())->Append((psInlining->aszUseFiles.NumElements() == 1) ? " use in " : " uses in ");
	pszDest->Append((uint32)aszFiles.NumElements())->Append((aszFiles.NumElements() == 1) ? " file, " : " files, ")->Append((uint32)psInlining->aszDeclarationFiles.NumElements());
	pszDest->Append((psInlining->aszDeclarationFiles.NumElements() == 1) ? " declaration" : " declarations")->Append((psInlining->uiCaretUse != MAX_SIZE) ? ", the caret on a use\n" : "\n");
	aszFiles.Kill();
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// Inline, for a constant at the caret: as Inline Function's dialog, every
// use with the constant taken out, or (from a use) that use only.
// *pbConstant is whether there is one at the caret; false if nothing
// changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineVariable::ShowInlineConstant(bool* pbConstant)
{
	SIDEConstantInlining	sInlining;
	Ptr<CComboBox>			pMode;
	CChars					szMessage;
	const char*				aszModes[2];
	bool					bResult;
	bool					bThisOnly;

	sInlining.Init();
	*pbConstant = FindConstantInlining(&sInlining);
	if (!*pbConstant)
	{
		sInlining.Kill();
		return false;
	}
	if (!sInlining.bPossible)
	{
		szMessage.Init("Can't inline ");
		szMessage.Append(sInlining.szName)->Append(": ")->Append(sInlining.szWhyNot)->Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sInlining.Kill();
		return false;
	}
	szMessage.Init("Inline ");
	szMessage.Append(sInlining.szName);
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	aszModes[0] = "All uses, and take the constant out";
	aszModes[1] = "This use only";
	pMode = mpDialog->AddComboBox("Inline:", aszModes, (sInlining.uiCaretUse != MAX_SIZE) ? 2 : 1, 0);
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(480);
	bThisOnly = bResult && (pMode->GetSelected() == 1);
	pMode = NULL;
	mpDialog = NULL;
	bResult = bResult && InlineConstant(&sInlining, bThisOnly);
	sInlining.Kill();
	return bResult;
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
	bool			bConstant;

	//A constant of a file or a class first: a function's own variable is the next thing looked for.
	bResult = ShowInlineConstant(&bConstant);
	if (bConstant)
	{
		return bResult;
	}
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
