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
#include "BaseLib/StringHelper.h"
#include "BaseLib/PointerFunctions.h"
#include "CppCompilerLib/CPPSyntaxes.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEChangeSignature.h"
#include "IDEConvertPointer.h"
#include "IDEContextActions.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mapcUnits.Init();
	maszPendingFiles.Init();
	maiPendingStarts.Init();
	mszProblem.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CArrayIDEPointerUnits, mapcUnits);
	U_Unknown(CArrayChars, maszPendingFiles);
	U_Unknown(CArrayInt, maiPendingStarts);
	U_Unknown(CChars, mszProblem);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::Free(void)
{
	mszProblem.Kill();
	maiPendingStarts.Kill();
	maszPendingFiles.Kill();
	KillUnits();
	mapcUnits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::Save(CObjectWriter* pcFile) { return false; }
bool CIDEConvertPointer::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ has neither: Java has no pointers.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.PointerToReference", "Convert Pointer to Reference", "", Ptr<CImageCel>(), this, &CIDEConvertPointer::QueueToReference)->SetEnabledCheck(this, &CIDEConvertPointer::CanConvert);
	pCommands->Add("Refactor.ReferenceToPointer", "Convert Reference to Pointer", "", Ptr<CImageCel>(), this, &CIDEConvertPointer::QueueToPointer)->SetEnabledCheck(this, &CIDEConvertPointer::CanConvert);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.PointerToReference"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ReferenceToPointer"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: a variable or a parameter declared a pointer can
// be made a reference, and one declared a reference a pointer.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bSymbol && ((psContext->uiKind == CPPSK_Variable) || (psContext->uiKind == CPPSK_Parameter)))
	{
		if (psContext->bPointer)
		{
			psContext->Offer("Refactor.PointerToReference");
		}
		if (psContext->bReference)
		{
			psContext->Offer("Refactor.ReferenceToPointer");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::CanConvert(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


void CIDEConvertPointer::QueueToReference(void) { mpcContent->GetWindow()->Post(this, &CIDEConvertPointer::ToReferenceQueued); }
void CIDEConvertPointer::QueueToPointer(void) { mpcContent->GetWindow()->Post(this, &CIDEConvertPointer::ToPointerQueued); }
void CIDEConvertPointer::ToReferenceQueued(void) { Convert(true); }
void CIDEConvertPointer::ToPointerQueued(void) { Convert(false); }


//////////////////////////////////////////////////////////////////////////
//
// A file parsed (not checked), once for a conversion.  NULL if it can't
// be.
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDEConvertPointer::GetUnit(const char* szFileName)
{
	CIDERefactorUnit*	pcUnit;
	size				i;

	for (i = 0; i < mapcUnits.NumElements(); i++)
	{
		pcUnit = mapcUnits.GetPtr(i);
		if (StringInsensitiveCompare(pcUnit->GetFileName(), szFileName) == 0)
		{
			return pcUnit->IsParsed() ? pcUnit : NULL;
		}
	}
	pcUnit = NewMalloc<CIDERefactorUnit>();
	pcUnit->Init();
	mapcUnits.Add(pcUnit);
	return pcUnit->Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), szFileName, false) ? pcUnit : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::KillUnits(void)
{
	size	i;

	for (i = 0; i < mapcUnits.NumElements(); i++)
	{
		mapcUnits.GetPtr(i)->Kill();
		free(mapcUnits.GetPtr(i));
	}
	mapcUnits.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Why it can't be converted (the first reason found is kept), with where
// if a place is given.  Always false.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::Refuse(const char* szWhyNot, const char* szFileName, int32 iLine)
{
	if (mszProblem.Empty())
	{
		mszProblem.Set(szWhyNot);
		if (szFileName)
		{
			mszProblem.Append(" (")->Append(GetIDEFileNamePart(szFileName))->Append(" line ")->Append((int32)(iLine + 1))->Append(")");
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a run of the unit's tokens is in its file's text.  False if they
// aren't written there (a macro made them).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::GetTokenOffsets(CIDERefactorUnit* pcUnit, const char* szText, int32 iFirstToken, int32 iLastToken, size* puiStart, size* puiEnd)
{
	SCPPSyntaxRange	sRange;

	if (!pcUnit->GetTokensRange(iFirstToken, iLastToken, &sRange))
	{
		return false;
	}
	*puiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
	*puiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::RemoveToken(CIDERefactorUnit* pcUnit, const char* szText, int32 iToken, CIDETextEdits* pcEdits)
{
	size	uiStart;
	size	uiEnd;

	if (!GetTokenOffsets(pcUnit, szText, iToken, iToken, &uiStart, &uiEnd))
	{
		return false;
	}
	pcEdits->Replace(uiStart, uiEnd, "");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The brackets of an expression in brackets taken away, if pcNode is one
// and what it has is a name alone: "(*p).x" is "p.x", not "(p).x".
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::RemoveBrackets(CIDERefactorUnit* pcUnit, const char* szText, CCPPSyntax* pcNode, CIDETextEdits* pcEdits)
{
	if (pcNode && (pcNode->GetKind() == CPPS_ParenthesisedExpression) && pcUnit->IsToken(pcNode->miFirstToken, "(") && pcUnit->IsToken(pcNode->miLastToken, ")") &&
		(pcNode->miLastToken - pcNode->miFirstToken <= 3))
	{
		RemoveToken(pcUnit, szText, pcNode->miFirstToken, pcEdits);
		RemoveToken(pcUnit, szText, pcNode->miLastToken, pcEdits);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A declarator's * made a & (or its & a *): the one next to its name.
// False, with why, if it has none, or the * is const.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::ConvertDeclarator(CIDERefactorUnit* pcUnit, const char* szFileName, CCPPSyntaxDeclarator* pcDeclarator, bool bToReference, CIDEFileEdits* pcFiles)
{
	CCPPSyntaxPointerOperator*	pcOperator;
	char*						szText;
	size						uiStart;
	size						uiEnd;
	size						i;

	pcOperator = NULL;
	for (i = 0; pcDeclarator && (i < pcDeclarator->NumPointers()); i++)
	{
		if (!pcOperator || (pcDeclarator->GetPointer(i)->miFirstToken > pcOperator->miFirstToken))
		{
			pcOperator = pcDeclarator->GetPointer(i);
		}
	}
	if (!pcOperator || (pcOperator->meKind != (bToReference ? CPPP_Pointer : CPPP_Reference)))
	{
		return Refuse(bToReference ? "it isn't a pointer" : "it isn't a reference");
	}
	if (pcOperator->macpQualifiers.NumElements() > 0)
	{
		return Refuse("its pointer is const");
	}
	szText = pcFiles->GetText(szFileName);
	if (!szText || !pcUnit->IsToken(pcOperator->miFirstToken, bToReference ? "*" : "&") || !GetTokenOffsets(pcUnit, szText, pcOperator->miFirstToken, pcOperator->miFirstToken, &uiStart, &uiEnd))
	{
		return Refuse("its declaration can't be read (a macro makes it)");
	}
	pcFiles->GetEdits(szFileName)->Replace(uiStart, uiEnd, bToReference ? "&" : "*");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A place the variable is used, changed to what it is now: to a
// reference, "p->x" is "p.x", "*p" is "p" and p alone is "&p"; to a
// pointer, "r.x" is "r->x", "&r" is "r" and r alone is "*r".  False,
// with why, if a reference (or a pointer) can't be used as it is there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::ConvertUse(const char* szFileName, int32 iLine, int32 iColumn, bool bToReference, CIDEFileEdits* pcFiles)
{
	CIDERefactorUnit*			pcUnit;
	CIDETextEdits*				pcEdits;
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcParent;
	CCPPSyntaxUnaryExpression*	pcUnary;
	CCPPSyntaxBinaryExpression*	pcBinary;
	char*						szText;
	size						uiStart;
	size						uiEnd;
	size						i;
	int32						iToken;
	int32						iOperator;
	bool						bBrackets;

	pcUnit = GetUnit(szFileName);
	szText = pcFiles->GetText(szFileName);
	iToken = (pcUnit && szText) ? pcUnit->FindTokenAt(iLine, iColumn) : -1;
	if ((iToken == -1) || !GetTokenOffsets(pcUnit, szText, iToken, iToken, &uiStart, &uiEnd))
	{
		return Refuse("a macro names it", szFileName, iLine);
	}
	pcEdits = pcFiles->GetEdits(szFileName);

	//The expression its name is, and what that is in.
	pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
	for (; pcNode && (pcNode->GetKind() != CPPS_IdExpression) && pcNode->mpcParent && (pcNode->mpcParent->miFirstToken == iToken) && (pcNode->mpcParent->miLastToken == iToken); pcNode = pcNode->mpcParent)
	{
	}
	if (!pcNode || (pcNode->GetKind() != CPPS_IdExpression) || !pcNode->mpcParent)
	{
		return Refuse("it is named in a way that can't be read", szFileName, iLine);
	}
	pcParent = pcNode->mpcParent;
	bBrackets = false;
	switch (pcParent->GetKind())
	{
	case CPPS_MemberExpression:
		if (((CCPPSyntaxMemberExpression*)pcParent)->mpcObject == pcNode)
		{
			if (((CCPPSyntaxMemberExpression*)pcParent)->mbArrow == bToReference)
			{
				if (!GetTokenOffsets(pcUnit, szText, iToken + 1, iToken + 1, &uiStart, &uiEnd))
				{
					return Refuse("a macro names it", szFileName, iLine);
				}
				pcEdits->Replace(uiStart, uiEnd, bToReference ? "." : "->");
				return true;
			}
			//A pointer with a . or a reference with a -> (its class's own): what it points to, in brackets.
			bBrackets = true;
		}
		break;
	case CPPS_UnaryExpression:
		pcUnary = (CCPPSyntaxUnaryExpression*)pcParent;
		if (!pcUnary->mbPostfix && pcUnit->IsToken(pcParent->miFirstToken, bToReference ? "*" : "&"))
		{
			RemoveToken(pcUnit, szText, pcParent->miFirstToken, pcEdits);
			RemoveBrackets(pcUnit, szText, pcParent->mpcParent, pcEdits);
			return true;
		}
		if (bToReference)
		{
			if (pcUnit->IsToken(pcParent->miFirstToken, "!"))
			{
				return Refuse("it is tested for NULL: a reference is never NULL", szFileName, iLine);
			}
			if (pcUnit->IsToken(pcParent->miFirstToken, "&"))
			{
				return Refuse("its address is taken", szFileName, iLine);
			}
			return Refuse("it is stepped: a reference always refers to one thing", szFileName, iLine);
		}
		bBrackets = pcUnary->mbPostfix;
		break;
	case CPPS_SubscriptExpression:
		if (((CCPPSyntaxSubscriptExpression*)pcParent)->mpcArray == pcNode)
		{
			if (bToReference)
			{
				return Refuse("it is used as an array", szFileName, iLine);
			}
			bBrackets = true;
		}
		break;
	case CPPS_CallExpression:
		bBrackets = (((CCPPSyntaxCallExpression*)pcParent)->mpcCallee == pcNode);
		break;
	case CPPS_BinaryExpression:
		if (bToReference)
		{
			pcBinary = (CCPPSyntaxBinaryExpression*)pcParent;
			iOperator = (pcBinary->mpcLeft == pcNode) ? iToken + 1 : iToken - 1;
			if (pcUnit->IsToken(iOperator, "=") && (pcBinary->mpcLeft == pcNode))
			{
				return Refuse("it is assigned to: a reference always refers to one thing", szFileName, iLine);
			}
			if (!pcUnit->IsToken(iOperator, "=") && !pcUnit->IsToken(iOperator, ","))
			{
				return Refuse("it is compared, tested or added to, as only a pointer can be", szFileName, iLine);
			}
		}
		break;
	case CPPS_ConditionalExpression:
		if (bToReference && (((CCPPSyntaxConditionalExpression*)pcParent)->mpcCondition == pcNode))
		{
			return Refuse("it is tested for NULL: a reference is never NULL", szFileName, iLine);
		}
		break;
	case CPPS_IfStatement:
	case CPPS_WhileStatement:
	case CPPS_DoStatement:
	case CPPS_ForStatement:
		if (bToReference)
		{
			return Refuse("it is tested for NULL: a reference is never NULL", szFileName, iLine);
		}
		break;
	case CPPS_DeleteExpression:
		if (bToReference)
		{
			return Refuse("it is deleted", szFileName, iLine);
		}
		break;
	default:
		break;
	}

	//Alone: what it points to is its address now, or what refers is what it points to.  An argument of a call of its
	//own function that was to change the other way stays as it is.
	for (i = 0; !bBrackets && (i < maiPendingStarts.NumElements()); i++)
	{
		if ((maiPendingStarts.GetValue(i) == (int)uiStart) && (StringInsensitiveCompare(maszPendingFiles.GetText(i), szFileName) == 0))
		{
			maiPendingStarts.SetValue(i, -1);
			return true;
		}
	}
	if (bBrackets)
	{
		pcEdits->Insert(uiStart, bToReference ? "(&" : "(*");
		pcEdits->Insert(uiEnd, ")");
	}
	else
	{
		pcEdits->Insert(uiStart, bToReference ? "&" : "*");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A value given to the variable (a call's argument for the parameter, a
// local variable's initialiser), changed to what it is now: to a
// reference, "&c" is "c" and anything else "*p"; to a pointer, "*p" is
// "p" and anything else "&c".  False, with why, if it is NULL.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::ConvertValue(CIDERefactorUnit* pcUnit, const char* szFileName, int32 iFirstToken, int32 iLastToken, bool bToReference, CIDEFileEdits* pcFiles)
{
	CIDETextEdits*	pcEdits;
	CCPPSyntax*		pcNode;
	SCPPSyntaxRange	sRange;
	char*			szText;
	size			uiStart;
	size			uiEnd;
	bool			bExact;

	szText = pcFiles->GetText(szFileName);
	if (!szText || !GetTokenOffsets(pcUnit, szText, iFirstToken, iLastToken, &uiStart, &uiEnd))
	{
		return Refuse("a macro makes what it is given");
	}
	pcUnit->GetTokensRange(iFirstToken, iFirstToken, &sRange);
	pcEdits = pcFiles->GetEdits(szFileName);
	pcNode = pcUnit->FindNodeOfTokens(iFirstToken, iLastToken);
	bExact = pcNode && (pcNode->miFirstToken == iFirstToken) && (pcNode->miLastToken == iLastToken);
	if (bExact && (pcNode->GetKind() == CPPS_UnaryExpression) && !((CCPPSyntaxUnaryExpression*)pcNode)->mbPostfix && pcUnit->IsToken(iFirstToken, bToReference ? "&" : "*"))
	{
		RemoveToken(pcUnit, szText, iFirstToken, pcEdits);
		RemoveBrackets(pcUnit, szText, ((CCPPSyntaxUnaryExpression*)pcNode)->mpcOperand, pcEdits);
		return true;
	}
	if (bToReference && (iFirstToken == iLastToken) && (pcUnit->IsToken(iFirstToken, "0") || pcUnit->IsToken(iFirstToken, "NULL") || pcUnit->IsToken(iFirstToken, "nullptr")))
	{
		return Refuse("it is given NULL", szFileName, sRange.iLine);
	}
	if ((iFirstToken == iLastToken) && !pcUnit->IsToken(iFirstToken, "this"))
	{
		//A name alone: it may be the parameter itself (ConvertUse).
		maszPendingFiles.Add(szFileName);
		maiPendingStarts.Add((int)uiStart);
	}
	else if (bExact && ((pcNode->GetKind() == CPPS_BinaryExpression) || (pcNode->GetKind() == CPPS_ConditionalExpression)))
	{
		pcEdits->Insert(uiStart, bToReference ? "*(" : "&(");
		pcEdits->Insert(uiEnd, ")");
	}
	else
	{
		pcEdits->Insert(uiStart, bToReference ? "*" : "&");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The arguments that are a name alone given their * or &: those that
// weren't the parameter itself.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertPointer::FlushPending(bool bToReference, CIDEFileEdits* pcFiles)
{
	size	i;

	for (i = 0; i < maiPendingStarts.NumElements(); i++)
	{
		if (maiPendingStarts.GetValue(i) != -1)
		{
			pcFiles->GetEdits(maszPendingFiles.GetText(i))->Insert((size)maiPendingStarts.GetValue(i), bToReference ? "*" : "&");
		}
	}
	maszPendingFiles.ReInit();
	maiPendingStarts.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Every use of the variable szKey changed (ConvertUse); the number.
//
//////////////////////////////////////////////////////////////////////////
size CIDEConvertPointer::ConvertUses(const char* szKey, bool bToReference, CIDEFileEdits* pcFiles)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	size				uiUses;
	size				i;
	uint8				uiKind;

	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind);
	uiUses = 0;
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole != CPPIR_Reference)
		{
			continue;
		}
		if (!ConvertUse(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, bToReference, pcFiles))
		{
			break;
		}
		uiUses++;
	}
	KillIDELocations(&asLocations);
	return uiUses;
}


//////////////////////////////////////////////////////////////////////////
//
// A parameter converted: in every declaration and definition of its
// function and of those changed with it (its overrides), its uses in
// each definition, and its argument in every call.  iLine and iColumn
// are where the parameter is declared in pcUnit's file.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::ConvertParameter(CIDERefactorUnit* pcUnit, CCPPSyntax* pcParameter, int32 iLine, int32 iColumn, bool bToReference, CIDEFileEdits* pcFiles, CChars* pszWhat, size* puiUses, size* puiCalls)
{
	SIDESignature			sSignature;
	SIDESignatureSite*		psSite;
	SIDESignatureCall*		psCall;
	SIDEWrittenParameter*	psWritten;
	SCPPSyntaxRange*		psRange;
	SCPPSyntaxRange			sRange;
	CIDERefactorUnit*		pcOther;
	CCPPSyntax*				pcNode;
	CCPPSyntaxName*			pcName;
	CChars					szArgument;
	char*					szText;
	size					uiParameter;
	size					i;
	size					j;
	int32					iFirst;
	int32					iLast;
	bool					bResult;

	for (pcNode = pcParameter->mpcParent; pcNode && (pcNode->GetKind() != CPPS_Declarator); pcNode = pcNode->mpcParent)
	{
	}
	pcName = pcNode ? ((CCPPSyntaxDeclarator*)pcNode)->GetName() : NULL;
	sSignature.Init();
	if (!pcName || !pcName->GetLast() || !pcUnit->GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sRange) ||
		!mpcContent->GetChangeSignature()->FindSignature(&sSignature, pcUnit->GetFileName(), sRange.iLine, sRange.iColumn))
	{
		sSignature.Kill();
		return Refuse("its function can't be read");
	}
	bResult = true;
	if (!sSignature.bChangeable)
	{
		bResult = Refuse(sSignature.szWhyNot.Text());
	}
	else if (sSignature.asOtherUses.NumElements() > 0)
	{
		bResult = Refuse("its function is named without being called: a pointer to the function wouldn't fit it", sSignature.asOtherUses.Get(0)->szFileName.Text(), sSignature.asOtherUses.Get(0)->iLine);
	}

	//Which of its function's parameters it is: by where it is written.
	uiParameter = MAX_SIZE;
	for (i = 0; bResult && (i < sSignature.apsSites.NumElements()) && (uiParameter == MAX_SIZE); i++)
	{
		psSite = sSignature.apsSites.GetPtr(i);
		for (j = 0; (StringInsensitiveCompare(psSite->szFileName.Text(), pcUnit->GetFileName()) == 0) && (j < psSite->asParameters.NumElements()); j++)
		{
			psRange = &psSite->asParameters.Get(j)->sAll;
			if (((iLine > psRange->iLine) || ((iLine == psRange->iLine) && (iColumn >= psRange->iColumn))) && ((iLine < psRange->iEndLine) || ((iLine == psRange->iEndLine) && (iColumn < psRange->iEndColumn))))
			{
				uiParameter = j;
				pszWhat->Append("the parameter ");
				pszWhat->AppendSubString(pcFiles->GetText(psSite->szFileName.Text()), FindIDETextOffset(pcFiles->GetText(psSite->szFileName.Text()), psSite->asParameters.Get(j)->sName.iLine, psSite->asParameters.Get(j)->sName.iColumn),
										 FindIDETextOffset(pcFiles->GetText(psSite->szFileName.Text()), psSite->asParameters.Get(j)->sName.iEndLine, psSite->asParameters.Get(j)->sName.iEndColumn));
				pszWhat->Append(" of ")->Append(sSignature.szName);
				break;
			}
		}
	}
	if (bResult && (uiParameter == MAX_SIZE))
	{
		bResult = Refuse("its function's parameters can't be read");
	}

	for (i = 0; bResult && (i < sSignature.apsSites.NumElements()); i++)
	{
		psSite = sSignature.apsSites.GetPtr(i);
		pcOther = GetUnit(psSite->szFileName.Text());
		if (!pcOther || (uiParameter >= psSite->asParameters.NumElements()))
		{
			bResult = Refuse("a declaration of its function can't be read", psSite->szFileName.Text(), psSite->iLine);
			break;
		}
		psWritten = psSite->asParameters.Get(uiParameter);
		if (bToReference && psWritten->bDefault)
		{
			bResult = Refuse("it has a default value", psSite->szFileName.Text(), psSite->iLine);
			break;
		}
		pcNode = pcOther->FindTokensIn(psWritten->sAll.iLine, psWritten->sAll.iColumn, psWritten->sAll.iEndLine, psWritten->sAll.iEndColumn, &iFirst, &iLast) ? pcOther->FindNodeOfTokens(iFirst, iLast) : NULL;
		for (; pcNode && (pcNode->GetKind() != CPPS_Parameter); pcNode = pcNode->mpcParent)
		{
		}
		if (!pcNode)
		{
			bResult = Refuse("a declaration of it can't be read (a macro makes it)", psSite->szFileName.Text(), psSite->iLine);
			break;
		}
		bResult = ConvertDeclarator(pcOther, psSite->szFileName.Text(), ((CCPPSyntaxParameter*)pcNode)->mpcDeclarator, bToReference, pcFiles);
	}

	//The calls first: an argument that is the parameter itself is known when its uses are changed.
	for (i = 0; bResult && (i < sSignature.apsCalls.NumElements()); i++)
	{
		psCall = sSignature.apsCalls.GetPtr(i);
		if (uiParameter >= psCall->asArguments.NumElements())
		{
			continue;
		}
		psRange = psCall->asArguments.Get(uiParameter);
		pcOther = GetUnit(psCall->szFileName.Text());
		szText = pcFiles->GetText(psCall->szFileName.Text());
		if (!pcOther || !szText || !pcOther->FindTokensIn(psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &iFirst, &iLast))
		{
			szArgument.Init();
			if (szText)
			{
				szArgument.AppendSubString(szText, FindIDETextOffset(szText, psRange->iLine, psRange->iColumn), FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn));
				szArgument.StripWhitespace();
			}
			bResult = Refuse((bToReference && szArgument.Equals("NULL")) ? "it is given NULL" : "a macro makes what a call gives it", psCall->szFileName.Text(), psCall->iLine);
			szArgument.Kill();
			break;
		}
		bResult = ConvertValue(pcOther, psCall->szFileName.Text(), iFirst, iLast, bToReference, pcFiles);
		(*puiCalls)++;
	}

	for (i = 0; bResult && (i < sSignature.apsSites.NumElements()); i++)
	{
		psWritten = sSignature.apsSites.GetPtr(i)->asParameters.Get(uiParameter);
		if (!psWritten->szKey.Empty())
		{
			(*puiUses) += ConvertUses(psWritten->szKey.Text(), bToReference, pcFiles);
			bResult = mszProblem.Empty();
		}
	}
	sSignature.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A local variable converted: its declaration, the value it is declared
// with and its uses.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::ConvertLocal(CIDERefactorUnit* pcUnit, CCPPSyntax* pcDeclarator, const char* szKey, bool bToReference, CIDEFileEdits* pcFiles, CChars* pszWhat, size* puiUses)
{
	CCPPSyntaxInitDeclarator*	pcInit;
	CChars						szName;
	int32						iFirst;
	int32						iLast;

	pcInit = (CCPPSyntaxInitDeclarator*)pcDeclarator;
	szName.Init();
	GetIDESymbolName(szKey, &szName);
	pszWhat->Append("the variable ")->Append(szName);
	szName.Kill();
	if (!ConvertDeclarator(pcUnit, pcUnit->GetFileName(), pcInit->mpcDeclarator, bToReference, pcFiles))
	{
		return false;
	}
	if (!pcInit->mpcInitializer)
	{
		return Refuse("it isn't given a value where it is declared: a reference must be");
	}
	iFirst = pcInit->mpcInitializer->miFirstToken;
	iLast = pcInit->mpcInitializer->miLastToken;
	if (pcUnit->IsToken(iFirst, "="))
	{
		iFirst++;
	}
	if ((pcInit->mpcInitializer->GetKind() == CPPS_InitializerList) || (pcInit->mpcInitializer->GetKind() == CPPS_ParenthesisedInitializer) || (iFirst > iLast))
	{
		return Refuse("its value is given in brackets where it is declared");
	}
	if (!ConvertValue(pcUnit, pcUnit->GetFileName(), iFirst, iLast, bToReference, pcFiles))
	{
		return false;
	}
	(*puiUses) += ConvertUses(szKey, bToReference, pcFiles);
	return mszProblem.Empty();
}


//////////////////////////////////////////////////////////////////////////
//
// The parameter or local variable at the caret made a reference (or a
// pointer), as one undo.  The files changed are checked as they would be
// first: nothing is changed if they would have more errors than they
// have.  The status says what was done, or why nothing was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertPointer::Convert(bool bToReference, CChars* pszStatus)
{
	CIDECodeModel*		pcModel;
	CIDEFileEdits		cFiles;
	CIDERefactorUnit*	pcUnit;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CCPPSyntax*			pcNode;
	CCPPSyntax*			pcParameter;
	CCPPSyntax*			pcInit;
	CChars				szKey;
	CChars				szWhat;
	CChars				szMessage;
	CChars				szDeclared;
	size				uiUses;
	size				uiCalls;
	size				uiBefore;
	size				i;
	int					iLine;
	int					iColumn;
	int32				iToken;
	uint8				uiRole;
	uint8				uiKind;
	bool				bResult;
	bool				bParameter;

	if (!CanConvert())
	{
		return false;
	}
	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	mszProblem.Clear();
	maszPendingFiles.ReInit();
	maiPendingStarts.ReInit();
	KillUnits();
	szKey.Init();
	szWhat.Init();
	szDeclared.Init();
	uiUses = 0;
	uiCalls = 0;
	bParameter = false;
	cFiles.Init(mpcContent);

	//Where it is declared: its parameter, or its declarator with its value.
	bResult = mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) &&
			  pcModel->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &szKey, &uiKind) && ((uiKind == CPPSK_Parameter) || (uiKind == CPPSK_Variable));
	if (bResult)
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(szKey.Text(), &asLocations, &uiKind);
		bResult = false;
		for (i = 0; (i < asLocations.NumElements()) && !bResult; i++)
		{
			psLocation = asLocations.Get(i);
			if (psLocation->uiRole != CPPIR_Reference)
			{
				szDeclared.Set(psLocation->szFileName);
				iLine = psLocation->iLine;
				iColumn = psLocation->iColumn;
				bResult = true;
			}
		}
		KillIDELocations(&asLocations);
	}
	if (!bResult)
	{
		Refuse("only a parameter or a local variable can be");
	}
	else
	{
		pcUnit = GetUnit(szDeclared.Text());
		iToken = pcUnit ? pcUnit->FindTokenAt(iLine, iColumn) : -1;
		pcNode = (iToken != -1) ? pcUnit->FindNodeOfTokens(iToken, iToken) : NULL;
		pcParameter = pcNode ? pcUnit->FindAncestor(pcNode, CPPS_Parameter) : NULL;
		pcInit = pcNode ? pcUnit->FindAncestor(pcNode, CPPS_InitDeclarator) : NULL;
		if (pcParameter)
		{
			bParameter = true;
			bResult = ConvertParameter(pcUnit, pcParameter, iLine, iColumn, bToReference, &cFiles, &szWhat, &uiUses, &uiCalls);
		}
		else if (pcInit && (szKey.Text()[0] == '@'))
		{
			bResult = ConvertLocal(pcUnit, pcInit, szKey.Text(), bToReference, &cFiles, &szWhat, &uiUses);
		}
		else
		{
			bResult = Refuse(pcNode ? "only a parameter or a local variable can be" : "its declaration can't be read (a macro makes it)");
		}
	}
	if (bResult)
	{
		FlushPending(bToReference, &cFiles);
		uiBefore = cFiles.CountErrors(false);
		if (cFiles.CountErrors(true) > uiBefore)
		{
			bResult = Refuse(bToReference ? "the code doesn't compile with it a reference" : "the code doesn't compile with it a pointer");
		}
	}

	szMessage.Init();
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Converted ")->Append(szWhat)->Append(bToReference ? " to a reference: " : " to a pointer: ");
		szMessage.Append((uint32)uiUses)->Append((uiUses == 1) ? " use" : " uses");
		if (bParameter)
		{
			szMessage.Append(" and ")->Append((uint32)uiCalls)->Append((uiCalls == 1) ? " call" : " calls");
		}
		szMessage.Append(" changed.");
	}
	else
	{
		if (szWhat.Empty() && !szKey.Empty())
		{
			GetIDESymbolName(szKey.Text(), &szWhat);
		}
		szMessage.Append("Can't convert ")->Append(szWhat.Empty() ? "what is at the caret" : szWhat.Text())->Append(bToReference ? " to a reference: " : " to a pointer: ")->Append(mszProblem)->Append('.');
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
	KillUnits();
	szDeclared.Kill();
	szWhat.Kill();
	szKey.Kill();
	return bResult;
}
