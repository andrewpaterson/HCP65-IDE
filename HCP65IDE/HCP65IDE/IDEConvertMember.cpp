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
#include "IDEMoveMembers.h"
#include "IDEMemberQualifier.h"
#include "IDEConvertMember.h"
#include "IDEContextActions.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMember::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszProblem.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMember::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CChars, mszProblem);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMember::Free(void)
{
	mszProblem.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::Save(CObjectWriter* pcFile) { return false; }
bool CIDEConvertMember::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's nearest are Convert To Instance Method and Make Static,
// which have no keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMember::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.FunctionToMember", "Convert Function to Member Function", "", Ptr<CImageCel>(), this, &CIDEConvertMember::QueueToMember)->SetEnabledCheck(this, &CIDEConvertMember::CanConvert);
	pCommands->Add("Refactor.MemberToFunction", "Convert Member Function to Function", "", Ptr<CImageCel>(), this, &CIDEConvertMember::QueueToFunction)->SetEnabledCheck(this, &CIDEConvertMember::CanConvert);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMember::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.FunctionToMember"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MemberToFunction"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: a function with parameters that is no class's
// can be made a member function (of its first parameter's class), and a
// member function that isn't virtual a function.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMember::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bFunction && !psContext->bMember && psContext->bParameters)
	{
		psContext->Offer("Refactor.FunctionToMember");
	}
	if (psContext->bFunction && psContext->bMember && !psContext->bSpecial && !psContext->bVirtual)
	{
		psContext->Offer("Refactor.MemberToFunction");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::CanConvert(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


void CIDEConvertMember::QueueToMember(void) { mpcContent->GetWindow()->Post(this, &CIDEConvertMember::ToMemberQueued); }
void CIDEConvertMember::QueueToFunction(void) { mpcContent->GetWindow()->Post(this, &CIDEConvertMember::ToFunctionQueued); }
void CIDEConvertMember::ToMemberQueued(void) { Convert(true); }
void CIDEConvertMember::ToFunctionQueued(void) { Convert(false); }


//////////////////////////////////////////////////////////////////////////
//
// Why it can't be converted (the first reason is kept).  Always false.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::Refuse(const char* szWhyNot, const char* szFileName, int32 iLine)
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


size CIDEConvertMember::Offset(const char* szText, int32 iLine, int32 iColumn) { return FindIDETextOffset(szText, iLine, iColumn); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::TokenOffsets(CIDERefactorUnit* pcUnit, const char* szText, int32 iFirstToken, int32 iLastToken, size* puiStart, size* puiEnd)
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
// The names a definition declares for itself: its parameters and its
// locals.  A member of that name must be named through this.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMember::FindLocalNames(SIDESignatureSite* psSite, CArrayChars* paszDest)
{
	CArrayIDEFileRanges	asRanges;
	CArrayChars			aszKeys;
	CChars				szName;
	size				i;

	asRanges.Init();
	asRanges.Add()->Init(psSite->szFileName.Text(), psSite->sParameters.iLine, psSite->sParameters.iColumn, psSite->sBody.iEndLine, psSite->sBody.iEndColumn);
	aszKeys.Init();
	mpcContent->GetCodeModel()->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		szName.Init();
		GetIDESymbolName(aszKeys.GetText(i), &szName);
		paszDest->AddIfUnique(szName.Text());
		szName.Kill();
	}
	aszKeys.Kill();
	KillIDEFileRanges(&asRanges);
}


//////////////////////////////////////////////////////////////////////////
//
// A definition's body once its first parameter is its object: "p->x" is
// "x" ("this->x" where the definition has an x of its own), "*p" is
// "*this" and p alone is this; for a reference, "r.x" is "x", "&r" is
// this and r alone "*this".  False, with why, if the body changes the
// parameter.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::ToMemberBody(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, bool bPointer, CIDEFileEdits* pcFiles)
{
	CArrayIDELocations			asLocations;
	CArrayChars					aszLocals;
	SIDELocation*				psLocation;
	CIDETextEdits*				pcEdits;
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcParent;
	CCPPSyntaxMemberExpression*	pcMember;
	CChars						szMember;
	char*						szText;
	size						uiStart;
	size						uiEnd;
	size						uiMemberStart;
	size						uiMemberEnd;
	size						uiOperatorStart;
	size						uiOperatorEnd;
	size						i;
	int32						iToken;
	uint8						uiKind;
	bool						bResult;

	if (psSite->asParameters.Get(0)->szKey.Empty())
	{
		return true;
	}
	szText = pcFiles->GetText(psSite->szFileName.Text());
	pcEdits = pcFiles->GetEdits(psSite->szFileName.Text());
	aszLocals.Init();
	FindLocalNames(psSite, &aszLocals);
	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(psSite->asParameters.Get(0)->szKey.Text(), &asLocations, &uiKind);
	bResult = true;
	for (i = 0; bResult && (i < asLocations.NumElements()); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole != CPPIR_Reference)
		{
			continue;
		}
		iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
		if ((iToken == -1) || !TokenOffsets(pcUnit, szText, iToken, iToken, &uiStart, &uiEnd))
		{
			bResult = Refuse("a macro names its first parameter", psLocation->szFileName.Text(), psLocation->iLine);
			break;
		}
		pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
		for (; pcNode && (pcNode->GetKind() != CPPS_IdExpression) && pcNode->mpcParent && (pcNode->mpcParent->miFirstToken == iToken) && (pcNode->mpcParent->miLastToken == iToken); pcNode = pcNode->mpcParent)
		{
		}
		pcParent = pcNode ? pcNode->mpcParent : NULL;
		if (!pcParent || (pcNode->GetKind() != CPPS_IdExpression))
		{
			bResult = Refuse("its first parameter is named in a way that can't be read", psLocation->szFileName.Text(), psLocation->iLine);
			break;
		}
		pcMember = (pcParent->GetKind() == CPPS_MemberExpression) ? (CCPPSyntaxMemberExpression*)pcParent : NULL;
		if (pcMember && (pcMember->mpcObject == pcNode) && (pcMember->mbArrow == bPointer) && pcMember->mpcName &&
			TokenOffsets(pcUnit, szText, pcMember->mpcName->miFirstToken, pcMember->mpcName->miLastToken, &uiMemberStart, &uiMemberEnd) &&
			TokenOffsets(pcUnit, szText, iToken + 1, iToken + 1, &uiOperatorStart, &uiOperatorEnd))
		{
			//A member of it: its name alone, unless the definition has that name for something of its own.
			szMember.Init();
			szMember.AppendSubString(szText, uiMemberStart, uiMemberEnd);
			if (aszLocals.GetIndex(szMember.Text()) == ARRAY_ELEMENT_NOT_FOUND)
			{
				pcEdits->Replace(uiStart, uiMemberStart, "");
			}
			else
			{
				pcEdits->Replace(uiStart, uiEnd, "this");
				if (!bPointer)
				{
					pcEdits->Replace(uiOperatorStart, uiOperatorEnd, "->");
				}
			}
			szMember.Kill();
		}
		else if ((pcParent->GetKind() == CPPS_UnaryExpression) && !((CCPPSyntaxUnaryExpression*)pcParent)->mbPostfix && pcUnit->IsToken(pcParent->miFirstToken, bPointer ? "*" : "&"))
		{
			//"*p" is "*this"; "&r" is this.
			pcEdits->Replace(uiStart, uiEnd, "this");
			if (!bPointer && TokenOffsets(pcUnit, szText, pcParent->miFirstToken, pcParent->miFirstToken, &uiOperatorStart, &uiOperatorEnd))
			{
				pcEdits->Replace(uiOperatorStart, uiOperatorEnd, "");
			}
		}
		else if (bPointer && (((pcParent->GetKind() == CPPS_UnaryExpression) && (pcUnit->IsToken(pcParent->miFirstToken, "++") || pcUnit->IsToken(pcParent->miFirstToken, "--") ||
				 pcUnit->IsToken(pcParent->miLastToken, "++") || pcUnit->IsToken(pcParent->miLastToken, "--") || pcUnit->IsToken(pcParent->miFirstToken, "&"))) ||
				 ((pcParent->GetKind() == CPPS_BinaryExpression) && (((CCPPSyntaxBinaryExpression*)pcParent)->mpcLeft == pcNode) && pcUnit->IsToken(iToken + 1, "="))))
		{
			bResult = Refuse("its body changes its first parameter: this can't be changed", psLocation->szFileName.Text(), psLocation->iLine);
		}
		else if (bPointer)
		{
			pcEdits->Replace(uiStart, uiEnd, "this");
		}
		else
		{
			pcEdits->Replace(uiStart, uiEnd, ((pcParent->GetKind() == CPPS_SubscriptExpression) || (pcParent->GetKind() == CPPS_CallExpression) || pcMember) ? "(*this)" : "*this");
		}
	}
	KillIDELocations(&asLocations);
	aszLocals.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A call once the function is a member: its first argument is the object
// it is called on.  "F(&c, 2)" is "c.F(2)", "F(p, 2)" "p->F(2)" and
// "F(this, 2)" "F(2)"; for a reference, "F(c, 2)" is "c.F(2)" and
// "F(*p, 2)" "p->F(2)".
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::ToMemberCall(SIDESignature* psSignature, SIDESignatureCall* psCall, bool bPointer, CIDEFileEdits* pcFiles)
{
	CIDERefactorUnit	cUnit;
	CCPPSyntax*			pcNode;
	CCPPSyntax*			pcObject;
	SCPPSyntaxRange*	psRange;
	CChars				szCall;
	char*				szText;
	size				uiName;
	size				uiStart;
	size				uiEnd;
	size				uiNext;
	int32				iToken;
	int32				iFirst;
	int32				iLast;
	bool				bResult;
	bool				bArrow;
	bool				bBrackets;

	if (psCall->asArguments.NumElements() == 0)
	{
		return Refuse("a call gives it no first argument", psCall->szFileName.Text(), psCall->iLine);
	}
	szText = pcFiles->GetText(psCall->szFileName.Text());
	psRange = psCall->asArguments.Get(0);
	cUnit.Init();
	bResult = szText && cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), psCall->szFileName.Text(), false);
	iToken = bResult ? cUnit.FindTokenAt(psCall->iLine, psCall->iColumn) : -1;
	if ((iToken == -1) || cUnit.IsToken(iToken - 1, "::") || !cUnit.FindTokensIn(psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &iFirst, &iLast) ||
		!TokenOffsets(&cUnit, szText, iFirst, iLast, &uiStart, &uiEnd))
	{
		cUnit.Kill();
		return Refuse("a call of it can't be read (a macro makes it, or it names it with ::)", psCall->szFileName.Text(), psCall->iLine);
	}
	uiName = Offset(szText, psCall->iLine, psCall->iColumn);
	pcNode = cUnit.FindNodeOfTokens(iFirst, iLast);
	pcObject = pcNode;
	bArrow = bPointer;
	if (pcNode && (pcNode->miFirstToken == iFirst) && (pcNode->miLastToken == iLast) && (pcNode->GetKind() == CPPS_UnaryExpression) && !((CCPPSyntaxUnaryExpression*)pcNode)->mbPostfix &&
		cUnit.IsToken(iFirst, bPointer ? "&" : "*"))
	{
		//"&c" is called on c, "*p" through p.
		pcObject = ((CCPPSyntaxUnaryExpression*)pcNode)->mpcOperand;
		bArrow = !bPointer;
		TokenOffsets(&cUnit, szText, pcObject->miFirstToken, pcObject->miLastToken, &uiStart, &uiEnd);
	}
	szCall.Init();
	if (!(bArrow && (pcObject->miFirstToken == pcObject->miLastToken) && cUnit.IsToken(pcObject->miFirstToken, "this")))
	{
		bBrackets = pcObject && ((pcObject->GetKind() == CPPS_BinaryExpression) || (pcObject->GetKind() == CPPS_ConditionalExpression) || (pcObject->GetKind() == CPPS_CastExpression) ||
								(pcObject->GetKind() == CPPS_UnaryExpression));
		szCall.Append(bBrackets ? "(" : "");
		szCall.AppendSubString(szText, uiStart, uiEnd);
		szCall.Append(bBrackets ? ")" : "")->Append(bArrow ? "->" : ".");
	}
	szCall.Append(psSignature->szOldName)->Append('(');

	//From its name to its second argument, or to its ) if it has no other.
	uiNext = (psCall->asArguments.NumElements() > 1) ? Offset(szText, psCall->asArguments.Get(1)->iLine, psCall->asArguments.Get(1)->iColumn) : Offset(szText, psCall->sArguments.iEndLine, psCall->sArguments.iEndColumn);
	pcFiles->GetEdits(psCall->szFileName.Text())->Replace(uiName, uiNext, szCall.Text());
	szCall.Kill();
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A function made a member function of the class its first parameter
// points or refers to.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::ToMember(SIDESignature* psSignature, CIDEFileEdits* pcFiles, CChars* pszDone, size* puiCalls)
{
	CIDECodeModel*			pcModel;
	CIDERefactorUnit		cUnit;
	SIDESignatureSite*		psSite;
	SIDESignatureSite*		psDefinition;
	SIDESignatureSite*		psDeclared;
	SIDEWrittenParameter*	psFirst;
	CCPPSyntax*				pcNode;
	CCPPSyntaxParameter*	pcParameter;
	SCPPSyntaxRange			sType;
	CChars					szScope;
	CChars					szClassKey;
	CChars					szKey;
	CChars					szLine;
	CChars					szReturn;
	char*					szText;
	size					uiStart;
	size					uiEnd;
	size					uiDefinitions;
	size					i;
	int32					iFirst;
	int32					iLast;
	int32					iToken;
	uint8					uiKind;
	bool					bPointer;
	bool					bConst;
	bool					bResult;

	pcModel = mpcContent->GetCodeModel();
	szScope.Init();
	GetIDESymbolScope(psSignature->szKey.Text(), &szScope);
	bResult = szScope.Empty();
	szScope.Kill();
	if (!bResult)
	{
		return Refuse("it is in a class or a namespace already");
	}
	psDefinition = NULL;
	psDeclared = NULL;
	uiDefinitions = 0;
	for (i = 0; i < psSignature->apsSites.NumElements(); i++)
	{
		psSite = psSignature->apsSites.GetPtr(i);
		if (psSite->bBody)
		{
			psDefinition = psSite;
			uiDefinitions++;
		}
		else if (!psDeclared)
		{
			psDeclared = psSite;
		}
		if (psSite->asParameters.NumElements() == 0)
		{
			return Refuse("it has no parameter to be its object");
		}
		if (psSite->asParameters.Get(0)->bDefault)
		{
			return Refuse("its first parameter has a default value");
		}
	}
	if (uiDefinitions != 1)
	{
		return Refuse((uiDefinitions == 0) ? "it isn't defined in the solution" : "it is defined more than once");
	}
	if (!psDeclared)
	{
		psDeclared = psDefinition;
	}

	//Its first parameter: one * or & on a class of the solution.
	szText = pcFiles->GetText(psDefinition->szFileName.Text());
	psFirst = psDefinition->asParameters.Get(0);
	cUnit.Init();
	szClassKey.Init();
	szKey.Init();
	bPointer = false;
	bConst = false;
	bResult = szText && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psDefinition->szFileName.Text(), false);
	pcNode = (bResult && cUnit.FindTokensIn(psFirst->sAll.iLine, psFirst->sAll.iColumn, psFirst->sAll.iEndLine, psFirst->sAll.iEndColumn, &iFirst, &iLast)) ? cUnit.FindNodeOfTokens(iFirst, iLast) : NULL;
	pcParameter = (CCPPSyntaxParameter*)cUnit.FindAncestor(pcNode, CPPS_Parameter);
	if (!pcParameter || !pcParameter->mpcDeclarator || !pcParameter->mpcSpecifiers || (pcParameter->mpcDeclarator->NumPointers() != 1) ||
		((pcParameter->mpcDeclarator->GetPointer(0)->meKind != CPPP_Pointer) && (pcParameter->mpcDeclarator->GetPointer(0)->meKind != CPPP_Reference)))
	{
		bResult = Refuse("its first parameter isn't a pointer or a reference to a class");
	}
	else
	{
		bPointer = (pcParameter->mpcDeclarator->GetPointer(0)->meKind == CPPP_Pointer);
		for (iToken = pcParameter->mpcSpecifiers->miFirstToken; iToken <= pcParameter->mpcSpecifiers->miLastToken; iToken++)
		{
			bConst |= cUnit.IsToken(iToken, "const");
		}
		//The class its type names: the last name its type is written with.
		iToken = pcParameter->mpcSpecifiers->miLastToken;
		if (!cUnit.GetTokensRange(iToken, iToken, &sType) || !pcModel->GetSymbolAt(psDefinition->szFileName.Text(), sType.iLine, sType.iColumn, &szClassKey, &uiKind) ||
			(uiKind != CPPSK_Class))
		{
			bResult = Refuse("its first parameter isn't a pointer or a reference to a class");
		}
	}

	if (bResult)
	{
		//Its class's declaration of it: what the first declaration returns and its other parameters.
		szText = pcFiles->GetText(psDeclared->szFileName.Text());
		szReturn.Init();
		AppendIDERangeText(szText, &psDeclared->sReturn, &szReturn);
		szReturn.StripWhitespace();
		szLine.Init("\t");
		szLine.Append(szReturn)->Append('\t')->Append(psSignature->szOldName)->Append('(');
		if (psDeclared->asParameters.NumElements() > 1)
		{
			szLine.AppendSubString(szText, Offset(szText, psDeclared->asParameters.Get(1)->sAll.iLine, psDeclared->asParameters.Get(1)->sAll.iColumn), Offset(szText, psDeclared->sParameters.iEndLine, psDeclared->sParameters.iEndColumn));
			szLine.StripWhitespace();
			szLine.Insert(0, "\t");
		}
		else
		{
			szLine.Append("void");
		}
		szLine.Append(bConst ? ") const;\n" : ");\n");
		if (!mpcContent->GetMoveMembers()->AddMember(szClassKey.Text(), IDE_ACCESS_PUBLIC, szLine.Text(), pcFiles))
		{
			bResult = Refuse("its first parameter's class isn't defined in the solution");
		}
		szLine.Kill();
		szReturn.Kill();
	}

	for (i = 0; bResult && (i < psSignature->apsSites.NumElements()); i++)
	{
		psSite = psSignature->apsSites.GetPtr(i);
		szText = pcFiles->GetText(psSite->szFileName.Text());
		if (psSite != psDefinition)
		{
			//Its own declarations go.
			uiStart = Offset(szText, psSite->sAll.iLine, psSite->sAll.iColumn);
			uiEnd = Offset(szText, psSite->sAll.iEndLine, psSite->sAll.iEndColumn);
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			pcFiles->GetEdits(psSite->szFileName.Text())->Replace(uiStart, uiEnd, "");
			continue;
		}
		//Its definition: its class before its name, its first parameter out, const if what it pointed to was.
		szLine.Init();
		GetIDESymbolName(szClassKey.Text(), &szLine);
		szLine.Append("::");
		pcFiles->GetEdits(psSite->szFileName.Text())->Insert(Offset(szText, psSite->sName.iLine, psSite->sName.iColumn), szLine.Text());
		szLine.Kill();
		uiStart = Offset(szText, psSite->asParameters.Get(0)->sAll.iLine, psSite->asParameters.Get(0)->sAll.iColumn);
		if (psSite->asParameters.NumElements() > 1)
		{
			pcFiles->GetEdits(psSite->szFileName.Text())->Replace(uiStart, Offset(szText, psSite->asParameters.Get(1)->sAll.iLine, psSite->asParameters.Get(1)->sAll.iColumn), "");
		}
		else
		{
			pcFiles->GetEdits(psSite->szFileName.Text())->Replace(uiStart, Offset(szText, psSite->asParameters.Get(0)->sAll.iEndLine, psSite->asParameters.Get(0)->sAll.iEndColumn), "void");
		}
		if (bConst)
		{
			pcFiles->GetEdits(psSite->szFileName.Text())->Insert(Offset(szText, psSite->sAfter.iLine, psSite->sAfter.iColumn), " const");
		}
		//A static before it (a function of its file alone) goes: a member function isn't.
		uiStart = Offset(szText, psSite->sAll.iLine, psSite->sAll.iColumn);
		uiEnd = Offset(szText, psSite->sReturn.iLine, psSite->sReturn.iColumn);
		if ((uiEnd >= uiStart + 7) && (strncmp(&szText[uiStart], "static", 6) == 0) && !IsIDENameChar(szText[uiStart + 6]))
		{
			pcFiles->GetEdits(psSite->szFileName.Text())->Replace(uiStart, uiEnd, "");
		}
		bResult = ToMemberBody(&cUnit, psSite, bPointer, pcFiles);
	}
	for (i = 0; bResult && (i < psSignature->apsCalls.NumElements()); i++)
	{
		bResult = ToMemberCall(psSignature, psSignature->apsCalls.GetPtr(i), bPointer, pcFiles);
		(*puiCalls)++;
	}
	if (bResult)
	{
		pszDone->Append("Converted ")->Append(psSignature->szName)->Append(" to the member function ");
		GetIDESymbolName(szClassKey.Text(), &szKey);
		pszDone->Append(szKey)->Append("::")->Append(psSignature->szOldName);
	}
	szKey.Kill();
	szClassKey.Kill();
	cUnit.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A definition's body once its object is the parameter szParameter: what
// it names of its class (or of a base class) without an object is named
// through the parameter, what is static through the class, and this is
// the parameter.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::ToFunctionBody(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, const char* szClassKey, const char* szParameter, CIDEFileEdits* pcFiles)
{
	CIDECodeModel*		pcModel;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	CArrayChars			aszKeys;
	CArrayChars			aszClasses;
	SIDELocation*		psLocation;
	CIDETextEdits*		pcEdits;
	CCPPSyntax*			pcNode;
	CChars				szScope;
	CChars				szName;
	CChars				szThrough;
	const char*			szKey;
	char*				szText;
	size				uiStart;
	size				uiEnd;
	size				i;
	size				j;
	int32				iToken;
	int32				iFirst;
	int32				iLast;
	uint8				uiKind;
	bool				bMember;
	bool				bStatic;

	pcModel = mpcContent->GetCodeModel();
	szText = pcFiles->GetText(psSite->szFileName.Text());
	pcEdits = pcFiles->GetEdits(psSite->szFileName.Text());
	aszClasses.Init();
	aszClasses.Add(szClassKey);
	pcModel->FindClassBases(szClassKey, true, &aszClasses);
	asRanges.Init();
	asRanges.Add()->Init(psSite->szFileName.Text(), psSite->sBody.iLine, psSite->sBody.iColumn, psSite->sBody.iEndLine, psSite->sBody.iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsUsedIn(&asRanges, &aszKeys);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		szKey = aszKeys.GetText(i);
		szScope.Init();
		szName.Init();
		GetIDESymbolScope(szKey, &szScope);
		GetIDESymbolName(szKey, &szName);
		bMember = (aszClasses.GetIndex(szScope.Text()) != ARRAY_ELEMENT_NOT_FOUND) && !szScope.EndsWith(szName.Text());
		szName.Kill();
		szScope.Kill();
		asLocations.Init();
		if (!bMember || (strcmp(szKey, psSite->szKey.Text()) == 0) || !pcModel->GetSymbolLocations(szKey, &asLocations, &uiKind) ||
			((uiKind != CPPSK_Field) && (uiKind != CPPSK_Function) && (uiKind != CPPSK_FunctionTemplate)))
		{
			//Not a member; or the function itself, whose calls are changed as calls.
			KillIDELocations(&asLocations);
			continue;
		}
		bStatic = mpcContent->GetMemberQualifier()->IsDeclaredWith(szKey, "static");
		szThrough.Init();
		if (bStatic)
		{
			GetIDESymbolName(szClassKey, &szThrough);
			szThrough.Append("::");
		}
		else
		{
			szThrough.Append(szParameter)->Append("->");
		}
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			psLocation = asLocations.Get(j);
			if ((psLocation->uiRole != CPPIR_Reference) || !asRanges.Get(0)->Has(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn))
			{
				continue;
			}
			iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
			pcNode = (iToken != -1) ? pcUnit->FindNodeOfTokens(iToken, iToken) : NULL;
			for (; pcNode && (pcNode->GetKind() != CPPS_IdExpression) && (pcNode->GetKind() != CPPS_MemberExpression); pcNode = pcNode->mpcParent)
			{
			}
			//Named alone, and not with its class before it already.
			if (pcNode && (pcNode->GetKind() == CPPS_IdExpression) && (pcNode->miFirstToken == iToken) && (pcNode->miLastToken == iToken) && TokenOffsets(pcUnit, szText, iToken, iToken, &uiStart, &uiEnd))
			{
				pcEdits->Insert(uiStart, szThrough.Text());
			}
		}
		szThrough.Kill();
		KillIDELocations(&asLocations);
	}
	if (pcUnit->FindTokensIn(psSite->sBody.iLine, psSite->sBody.iColumn, psSite->sBody.iEndLine, psSite->sBody.iEndColumn, &iFirst, &iLast))
	{
		for (iToken = iFirst; iToken <= iLast; iToken++)
		{
			if (pcUnit->IsToken(iToken, "this") && TokenOffsets(pcUnit, szText, iToken, iToken, &uiStart, &uiEnd))
			{
				pcEdits->Replace(uiStart, uiEnd, szParameter);
			}
		}
	}
	aszKeys.Kill();
	KillIDEFileRanges(&asRanges);
	aszClasses.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A call once the member function is a function: the object it is called
// on is its first argument.  "c.F(2)" is "F(&c, 2)", "p->F(2)" "F(p, 2)",
// and a call with no object (from its class's own functions) "F(this,
// 2)"; in the function's own body, where this is its parameter now,
// "F(pcVat, 2)".
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::ToFunctionCall(SIDESignature* psSignature, SIDESignatureCall* psCall, SIDESignatureSite* psDefinition, const char* szParameter, CIDEFileEdits* pcFiles)
{
	CIDERefactorUnit			cUnit;
	CCPPSyntax*					pcNode;
	CCPPSyntaxMemberExpression*	pcMember;
	CChars						szCall;
	char*						szText;
	size						uiStart;
	size						uiEnd;
	size						uiArguments;
	int32						iToken;
	int							iKind;
	bool						bResult;
	bool						bSimple;
	bool						bOwn;

	//In its own body: this is the parameter there.
	bOwn = (StringInsensitiveCompare(psCall->szFileName.Text(), psDefinition->szFileName.Text()) == 0) &&
		   ((psCall->iLine > psDefinition->sBody.iLine) || ((psCall->iLine == psDefinition->sBody.iLine) && (psCall->iColumn >= psDefinition->sBody.iColumn))) &&
		   ((psCall->iLine < psDefinition->sBody.iEndLine) || ((psCall->iLine == psDefinition->sBody.iEndLine) && (psCall->iColumn < psDefinition->sBody.iEndColumn)));
	szText = pcFiles->GetText(psCall->szFileName.Text());
	cUnit.Init();
	bResult = szText && cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), psCall->szFileName.Text(), false);
	iToken = bResult ? cUnit.FindTokenAt(psCall->iLine, psCall->iColumn) : -1;
	pcNode = (iToken != -1) ? cUnit.FindNodeOfTokens(iToken, iToken) : NULL;
	for (; pcNode && (pcNode->GetKind() != CPPS_IdExpression) && (pcNode->GetKind() != CPPS_MemberExpression); pcNode = pcNode->mpcParent)
	{
	}
	if (!pcNode || (pcNode->miLastToken != iToken))
	{
		cUnit.Kill();
		return Refuse("a call of it can't be read (a macro makes it)", psCall->szFileName.Text(), psCall->iLine);
	}
	uiArguments = Offset(szText, psCall->sArguments.iLine, psCall->sArguments.iColumn);
	szCall.Init();
	if (pcNode->GetKind() == CPPS_IdExpression)
	{
		//No object: this one's.  Its name stays (its class before it, if it had it, goes).
		TokenOffsets(&cUnit, szText, pcNode->miFirstToken, iToken, &uiStart, &uiEnd);
		szCall.Append(psSignature->szOldName)->Append('(')->Append(bOwn ? szParameter : "this");
	}
	else
	{
		pcMember = (CCPPSyntaxMemberExpression*)pcNode;
		TokenOffsets(&cUnit, szText, pcMember->mpcObject->miFirstToken, pcMember->mpcObject->miLastToken, &uiStart, &uiEnd);
		iKind = pcMember->mpcObject->GetKind();
		bSimple = (iKind == CPPS_IdExpression) || (iKind == CPPS_MemberExpression) || (iKind == CPPS_SubscriptExpression) || (iKind == CPPS_CallExpression) || (iKind == CPPS_ThisExpression) ||
				  (iKind == CPPS_ParenthesisedExpression);
		szCall.Append(psSignature->szOldName)->Append('(');
		if (!pcMember->mbArrow)
		{
			szCall.Append(bSimple ? "&" : "&(");
		}
		szCall.AppendSubString(szText, uiStart, uiEnd);
		szCall.Append((!pcMember->mbArrow && !bSimple) ? ")" : "");
	}
	szCall.Append((psCall->asArguments.NumElements() > 0) ? ", " : "");
	pcFiles->GetEdits(psCall->szFileName.Text())->Replace(uiStart, uiArguments, szCall.Text());
	szCall.Kill();
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A member function made a function whose first parameter points to its
// class.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::ToFunction(SIDESignature* psSignature, CIDEFileEdits* pcFiles, CChars* pszDone, size* puiCalls)
{
	CIDECodeModel*		pcModel;
	CIDERefactorUnit	cUnit;
	CIDERefactorUnit	cClassUnit;
	CArrayIDELocations	asLocations;
	CArrayChars			aszLocals;
	SIDESignatureSite*	psSite;
	SIDESignatureSite*	psDefinition;
	SIDESignatureSite*	psDeclared;
	SIDELocation*		psLocation;
	CCPPSyntax*			pcNode;
	SCPPSyntaxRange		sClass;
	CChars				szClassKey;
	CChars				szClassName;
	CChars				szParameter;
	CChars				szFirst;
	CChars				szLine;
	CChars				szReturn;
	CChars				szRest;
	const char*			szConst;
	char*				szText;
	size				uiStart;
	size				uiEnd;
	size				uiAt;
	size				i;
	int32				iToken;
	uint8				uiKind;
	bool				bConst;
	bool				bResult;

	pcModel = mpcContent->GetCodeModel();
	szClassKey.Init();
	GetIDESymbolScope(psSignature->szKey.Text(), &szClassKey);
	asLocations.Init();
	bResult = !szClassKey.Empty() && pcModel->GetSymbolLocations(szClassKey.Text(), &asLocations, &uiKind) && (uiKind == CPPSK_Class);
	szClassName.Init();
	GetIDESymbolName(szClassKey.Text(), &szClassName);
	psDefinition = NULL;
	psDeclared = NULL;
	if (!bResult)
	{
		Refuse("it isn't a member function of a class (a class template's can't be)");
	}
	else if (psSignature->szOldName.Equals(szClassName.Text()) || psSignature->szOldName.StartsWith("~") || psSignature->szOldName.StartsWith("operator"))
	{
		bResult = Refuse("a constructor, a destructor or an operator can't be");
	}
	else if ((psSignature->uiOverrides > 0) || (psSignature->uiFamily > 0) || mpcContent->GetMemberQualifier()->IsDeclaredWith(psSignature->szKey.Text(), "virtual"))
	{
		bResult = Refuse("it is virtual, or a template's body calls it on an object");
	}
	else if (mpcContent->GetMemberQualifier()->IsDeclaredWith(psSignature->szKey.Text(), "static"))
	{
		bResult = Refuse("it is static: it has no object to be its parameter");
	}
	for (i = 0; bResult && (i < psSignature->apsSites.NumElements()); i++)
	{
		psSite = psSignature->apsSites.GetPtr(i);
		if (psSite->bBody && psSite->bInClass)
		{
			bResult = Refuse("it is defined in its class: Move Definition out of Class first");
		}
		else if (psSite->bBody && !psDefinition)
		{
			psDefinition = psSite;
		}
		else if (!psSite->bBody && psSite->bInClass && !psDeclared)
		{
			psDeclared = psSite;
		}
		else
		{
			bResult = Refuse("it is declared or defined more than once");
		}
	}
	if (bResult && (!psDefinition || !psDeclared))
	{
		bResult = Refuse("it isn't defined in the solution");
	}

	//Its parameter: "pcVat" for CVat, "psLevel" for SLevel; a name the definition hasn't.
	szParameter.Init();
	szFirst.Init();
	bConst = false;
	if (bResult)
	{
		szParameter.Append((szClassName.Text()[0] == 'S') ? "ps" : "pc");
		szParameter.Append(((szClassName.Length() > 1) && ((szClassName.Text()[0] == 'C') || (szClassName.Text()[0] == 'S')) && (szClassName.Text()[1] >= 'A') && (szClassName.Text()[1] <= 'Z')) ? &szClassName.Text()[1] : szClassName.Text());
		aszLocals.Init();
		FindLocalNames(psDefinition, &aszLocals);
		if (aszLocals.GetIndex(szParameter.Text()) != ARRAY_ELEMENT_NOT_FOUND)
		{
			szLine.Init("it has a ");
			szLine.Append(szParameter)->Append(" already, which its object would be named");
			bResult = Refuse(szLine.Text());
			szLine.Kill();
		}
		aszLocals.Kill();
		bConst = strstr(psDefinition->szQualifiers.Text(), "const") != NULL;
		szFirst.Append(bConst ? "const " : "")->Append(szClassName)->Append("* ")->Append(szParameter);
	}

	cUnit.Init();
	for (i = 0; bResult && (i < 2); i++)
	{
		psSite = (i == 0) ? psDeclared : psDefinition;
		szText = pcFiles->GetText(psSite->szFileName.Text());
		szRest.Init();
		szRest.AppendSubString(szText, Offset(szText, psSite->sParameters.iLine, psSite->sParameters.iColumn), Offset(szText, psSite->sParameters.iEndLine, psSite->sParameters.iEndColumn));
		szRest.StripWhitespace();
		if (szRest.Equals("void"))
		{
			szRest.Clear();
		}
		if (i == 0)
		{
			//Its class's declaration goes, and it is declared after its class.
			uiStart = Offset(szText, psSite->sAll.iLine, psSite->sAll.iColumn);
			uiEnd = Offset(szText, psSite->sAll.iEndLine, psSite->sAll.iEndColumn);
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			pcFiles->GetEdits(psSite->szFileName.Text())->Replace(uiStart, uiEnd, "");

			szReturn.Init();
			AppendIDERangeText(szText, &psSite->sReturn, &szReturn);
			szReturn.StripWhitespace();
			szLine.Init("\n\n");
			szLine.Append(szReturn)->Append('\t')->Append(psSignature->szOldName)->Append('(')->Append(szFirst)->Append(szRest.Empty() ? "" : ", ")->Append(szRest)->Append(");\n");
			szReturn.Kill();
			bResult = false;
			cClassUnit.Init();
			for (uiAt = 0; (uiAt < asLocations.NumElements()) && !bResult; uiAt++)
			{
				psLocation = asLocations.Get(uiAt);
				if ((psLocation->uiRole != CPPIR_Definition) || (StringInsensitiveCompare(psLocation->szFileName.Text(), psSite->szFileName.Text()) != 0) ||
					!cClassUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psLocation->szFileName.Text(), false))
				{
					continue;
				}
				iToken = cClassUnit.FindTokenAt(psLocation->iLine, psLocation->iColumn);
				pcNode = (iToken != -1) ? cClassUnit.FindAncestor(cClassUnit.FindNodeOfTokens(iToken, iToken), CPPS_SimpleDeclaration) : NULL;
				if (pcNode && cClassUnit.GetRange(pcNode, &sClass))
				{
					uiEnd = Offset(szText, sClass.iEndLine, sClass.iEndColumn);
					for (; (szText[uiEnd] != '\0') && (szText[uiEnd] != '\n'); uiEnd++)
					{
					}
					pcFiles->GetEdits(psSite->szFileName.Text())->Insert((szText[uiEnd] == '\n') ? uiEnd + 1 : uiEnd, szLine.Text());
					bResult = true;
				}
			}
			cClassUnit.Kill();
			szLine.Kill();
			if (!bResult)
			{
				Refuse("its class's end can't be read (a macro makes it)");
			}
		}
		else
		{
			//Its definition: its own name alone, its object first among its parameters, no const after them.
			pcFiles->GetEdits(psSite->szFileName.Text())->Replace(Offset(szText, psSite->sName.iLine, psSite->sName.iColumn), Offset(szText, psSite->sName.iEndLine, psSite->sName.iEndColumn), psSignature->szOldName.Text());
			szLine.Init(szFirst);
			szLine.Append(szRest.Empty() ? "" : ", ")->Append(szRest);
			pcFiles->GetEdits(psSite->szFileName.Text())->Replace(Offset(szText, psSite->sParameters.iLine, psSite->sParameters.iColumn), Offset(szText, psSite->sParameters.iEndLine, psSite->sParameters.iEndColumn), szLine.Text());
			szLine.Kill();
			uiStart = Offset(szText, psSite->sAfter.iLine, psSite->sAfter.iColumn);
			uiEnd = Offset(szText, psSite->sAfter.iEndLine, psSite->sAfter.iEndColumn);
			for (szConst = bConst ? strstr(&szText[uiStart], "const") : NULL; szConst && ((size)(szConst - szText) + 5 <= uiEnd); szConst = strstr(szConst + 1, "const"))
			{
				if (!IsIDENameChar(szConst[-1]) && !IsIDENameChar(szConst[5]))
				{
					uiAt = (size)(szConst - szText);
					pcFiles->GetEdits(psSite->szFileName.Text())->Replace(((uiAt > uiStart) && (szConst[-1] == ' ')) ? uiAt - 1 : uiAt, uiAt + 5, "");
					break;
				}
			}
			bResult = cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psSite->szFileName.Text(), false) && ToFunctionBody(&cUnit, psSite, szClassKey.Text(), szParameter.Text(), pcFiles);
		}
		szRest.Kill();
	}
	cUnit.Kill();
	for (i = 0; bResult && (i < psSignature->apsCalls.NumElements()); i++)
	{
		bResult = ToFunctionCall(psSignature, psSignature->apsCalls.GetPtr(i), psDefinition, szParameter.Text(), pcFiles);
		(*puiCalls)++;
	}
	if (bResult)
	{
		pszDone->Append("Converted ")->Append(psSignature->szName)->Append(" to the function ")->Append(psSignature->szOldName)->Append('(')->Append(szFirst)->Append(", ...)");
	}
	szFirst.Kill();
	szParameter.Kill();
	szClassName.Kill();
	KillIDELocations(&asLocations);
	szClassKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The function at the caret made a member function (or a member function
// a function), as one undo.  The files changed are checked as they would
// be first.  The status says what was done, or why nothing was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMember::Convert(bool bToMember, CChars* pszStatus)
{
	CIDEFileEdits		cFiles;
	SIDESignature		sSignature;
	SIDESignatureSite*	psSite;
	CArrayChars			aszOldLabels;
	CArrayChars			aszNewLabels;
	CArrayChars			aszOtherOldLabels;
	CArrayChars			aszOtherNewLabels;
	CChars				szMessage;
	CChars				szName;
	char*				szText;
	size				uiCalls;
	size				uiBefore;
	size				uiLabels;
	size				i;
	bool				bResult;
	bool				bFound;
	bool				bLabels;

	if (!CanConvert())
	{
		return false;
	}
	mszProblem.Clear();
	uiCalls = 0;
	sSignature.Init();
	cFiles.Init(mpcContent);
	szMessage.Init();
	bFound = mpcContent->GetChangeSignature()->FindSignature(&sSignature);
	bResult = bFound;
	if (!bFound)
	{
		Refuse("there is no function at the caret");
	}
	else if (!sSignature.bChangeable)
	{
		bResult = Refuse(sSignature.szWhyNot.Text());
	}
	else if (sSignature.asOtherUses.NumElements() > 0)
	{
		bResult = Refuse("it is named without being called: a pointer to it wouldn't fit it", sSignature.asOtherUses.Get(0)->szFileName.Text(), sSignature.asOtherUses.Get(0)->iLine);
	}
	if (bResult)
	{
		bResult = bToMember ? ToMember(&sSignature, &cFiles, &szMessage, &uiCalls) : ToFunction(&sSignature, &cFiles, &szMessage, &uiCalls);
	}
	if (bResult)
	{
		uiBefore = cFiles.CountErrors(false);
		if (cFiles.CountErrors(true) > uiBefore)
		{
			bResult = Refuse(bToMember ? "the code doesn't compile with it a member function" : "the code doesn't compile with it a function: it uses what only its class's functions may, or what its class names for itself");
		}
	}
	if (bResult)
	{
		//Hand written assembly names it by the compiler's label, which has its class and its parameters in it: the
		//label of its definition as it is, and as it is afterwards (where the definition's name is by then).
		aszOldLabels.Init();
		aszNewLabels.Init();
		aszOtherOldLabels.Init();
		aszOtherNewLabels.Init();
		bLabels = mpcContent->GetChangeSignature()->HasAssembly();
		if (bLabels)
		{
			mpcContent->GetChangeSignature()->FindLabels(&sSignature, CPPT_W65C816, false, &aszOldLabels, true);
			mpcContent->GetChangeSignature()->FindLabels(&sSignature, CPPT_65C64, false, &aszOtherOldLabels, true);
			for (i = 0; i < sSignature.apsSites.NumElements(); i++)
			{
				psSite = sSignature.apsSites.GetPtr(i);
				szText = psSite->bDefinition ? cFiles.GetText(psSite->szFileName.Text()) : NULL;
				if (szText)
				{
					psSite->uiNewName = cFiles.GetEdits(psSite->szFileName.Text())->AdjustPosition(FindIDETextOffset(szText, psSite->iLine, psSite->iColumn));
				}
			}
		}
		cFiles.Apply();
		uiLabels = 0;
		if (bLabels)
		{
			mpcContent->GetChangeSignature()->FindLabels(&sSignature, CPPT_W65C816, true, &aszNewLabels, true);
			mpcContent->GetChangeSignature()->FindLabels(&sSignature, CPPT_65C64, true, &aszOtherNewLabels, true);

			//Only a label that is this function's: it has its name in it.
			szName.Init("_");
			szName.Append(sSignature.szOldName);
			for (i = 0; i < aszNewLabels.NumElements(); i++)
			{
				if (!aszNewLabels.Get(i)->Contains(szName.Text()))
				{
					aszNewLabels.Get(i)->Clear();
				}
				if (!aszOtherNewLabels.Get(i)->Contains(szName.Text()))
				{
					aszOtherNewLabels.Get(i)->Clear();
				}
			}
			szName.Kill();
			uiLabels = mpcContent->GetChangeSignature()->ChangeLabels(&sSignature, &aszOldLabels, &aszNewLabels, &aszOtherOldLabels, &aszOtherNewLabels);
		}
		aszOtherNewLabels.Kill();
		aszOtherOldLabels.Kill();
		aszNewLabels.Kill();
		aszOldLabels.Kill();
		szMessage.Append(": ")->Append((uint32)uiCalls)->Append((uiCalls == 1) ? " call changed" : " calls changed");
		if (uiLabels > 0)
		{
			szMessage.Append(", ")->Append((uint32)uiLabels)->Append((uiLabels == 1) ? " label in assembly" : " labels in assembly");
		}
		szMessage.Append('.');
	}
	else
	{
		szMessage.Set("Can't convert ");
		szMessage.Append(bFound ? sSignature.szName.Text() : "what is at the caret")->Append(bToMember ? " to a member function: " : " to a function: ")->Append(mszProblem)->Append('.');
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
	sSignature.Kill();
	return bResult;
}
