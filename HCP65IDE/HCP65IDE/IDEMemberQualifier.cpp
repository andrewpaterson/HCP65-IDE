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
#include "IDEDefinitions.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEMemberQualifier.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEQualifierChange::Init(void)
{
	bStatic = false;
	bPossible = false;
	szWhyNot.Init();
	sSignature.Init();
	szClassKey.Init();
	aszConflicts.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEQualifierChange::Kill(void)
{
	aszConflicts.Kill();
	szClassKey.Kill();
	sSignature.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMemberQualifier::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMemberQualifier::Class(void)
{
	U_Pointer(mpcContent);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMemberQualifier::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMemberQualifier::Save(CObjectWriter* pcFile) { return false; }
bool CIDEMemberQualifier::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ has Make Static (no key); const is C++'s own.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMemberQualifier::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.MakeConst", "Make Member Function const", "", Ptr<CImageCel>(), this, &CIDEMemberQualifier::QueueMakeConst)->SetEnabledCheck(this, &CIDEMemberQualifier::CanChange);
	pCommands->Add("Refactor.MakeStatic", "Make Member Function static", "", Ptr<CImageCel>(), this, &CIDEMemberQualifier::QueueMakeStatic)->SetEnabledCheck(this, &CIDEMemberQualifier::CanChange);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMemberQualifier::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MakeConst"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MakeStatic"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMemberQualifier::CanChange(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


void CIDEMemberQualifier::QueueMakeConst(void) { mpcContent->GetWindow()->Post(this, &CIDEMemberQualifier::MakeConstQueued); }
void CIDEMemberQualifier::QueueMakeStatic(void) { mpcContent->GetWindow()->Post(this, &CIDEMemberQualifier::MakeStaticQueued); }
void CIDEMemberQualifier::MakeConstQueued(void) { Show(false); }
void CIDEMemberQualifier::MakeStaticQueued(void) { Show(true); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMemberQualifier::Refuse(SIDEQualifierChange* psDest, const char* szWhyNot, const char* szFileName, int32 iLine)
{
	if (psDest->bPossible || psDest->szWhyNot.Empty())
	{
		psDest->bPossible = false;
		psDest->szWhyNot.Set(szWhyNot);
		if (szFileName)
		{
			psDest->szWhyNot.Append(" (")->Append(GetIDEFileNamePart(szFileName))->Append(" line ")->Append((int32)(iLine + 1))->Append(")");
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a declaration of the symbol szKey has szWord ("static",
// "virtual") written before its name on its line.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMemberQualifier::IsDeclaredWith(const char* szKey, const char* szWord)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CTextFile			cFile;
	const char*			szFound;
	char*				szText;
	size				uiLength;
	size				uiWord;
	size				uiName;
	size				uiLine;
	size				i;
	uint8				uiKind;
	bool				bHas;

	uiWord = strlen(szWord);
	bHas = false;
	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind);
	for (i = 0; (i < asLocations.NumElements()) && !bHas; i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			continue;
		}
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			uiName = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
			for (uiLine = uiName; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
			{
			}
			for (szFound = strstr(&szText[uiLine], szWord); szFound && ((size)(szFound - szText) + uiWord <= uiName) && !bHas; szFound = strstr(szFound + 1, szWord))
			{
				bHas = ((szFound == &szText[uiLine]) || !IsIDENameChar(szFound[-1])) && !IsIDENameChar(szFound[uiWord]);
			}
		}
		cFile.Kill();
	}
	KillIDELocations(&asLocations);
	return bHas;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMemberQualifier::AddConflict(SIDEQualifierChange* psDest, const char* szWhat, const char* szKey, const char* szAfter, const char* szFileName, int32 iLine)
{
	CChars	szConflict;
	CChars	szName;

	szName.Init();
	GetIDESymbolName(szKey, &szName);
	szConflict.Init(szWhat);
	szConflict.Append(szName)->Append(szAfter)->Append(", in ")->Append(GetIDEFileNamePart(szFileName))->Append(" at line ")->Append((int32)(iLine + 1))->Append('.');
	psDest->aszConflicts.AddIfUnique(szConflict.Text());
	szConflict.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What a definition's body does with its object: each field and member
// function of its class (or of a base class) that it names without an
// object, or through this.  For Make const, what it does to them that a
// const function can't is a conflict (the kept check's FindChanges, for a
// field and whatever of it is named: msMark.iAt, maiUnits[2]); for Make
// static, any of them that isn't static refuses it, and so does this.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMemberQualifier::FindBodyUses(SIDEQualifierChange* psDest, SIDESignatureSite* psSite)
{
	CIDECodeModel*			pcModel;
	CIDERefactorUnit		cUnit;
	CArrayIDEFileRanges		asRanges;
	CArrayIDELocations		asLocations;
	CArrayChars				aszKeys;
	CArrayChars				aszClasses;
	SIDELocation*			psLocation;
	CCPPSyntax*				pcNode;
	CCPPSyntax*				pcTarget;
	CChars					szScope;
	CChars					szName;
	const char*				szKey;
	size					i;
	size					j;
	int32					iToken;
	int32					iFirst;
	int32					iLast;
	uint8					uiKind;
	uint8					uiChanges;
	bool					bMember;
	bool					bFunction;

	pcModel = mpcContent->GetCodeModel();
	cUnit.Init();
	if (!cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psSite->szFileName.Text(), true))
	{
		cUnit.Kill();
		return;
	}
	aszClasses.Init();
	aszClasses.Add(psDest->szClassKey.Text());
	pcModel->FindClassBases(psDest->szClassKey.Text(), true, &aszClasses);
	asRanges.Init();
	asRanges.Add()->Init(psSite->szFileName.Text(), psSite->sBody.iLine, psSite->sBody.iColumn, psSite->sBody.iEndLine, psSite->sBody.iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsUsedIn(&asRanges, &aszKeys);
	for (i = 0; (i < aszKeys.NumElements()) && (psDest->bPossible || !psDest->bStatic); i++)
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
		if (!bMember || !pcModel->GetSymbolLocations(szKey, &asLocations, &uiKind) || ((uiKind != CPPSK_Field) && (uiKind != CPPSK_Function) && (uiKind != CPPSK_FunctionTemplate)) ||
			IsDeclaredWith(szKey, "static"))
		{
			KillIDELocations(&asLocations);
			continue;
		}
		bFunction = (uiKind != CPPSK_Field);
		if (bFunction && !psDest->bStatic && (strstr(szKey, ") const") != NULL))
		{
			KillIDELocations(&asLocations);
			continue;
		}
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			psLocation = asLocations.Get(j);
			if ((psLocation->uiRole != CPPIR_Reference) || !asRanges.Get(0)->Has(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn))
			{
				continue;
			}
			iToken = cUnit.FindTokenAt(psLocation->iLine, psLocation->iColumn);
			pcNode = (iToken != -1) ? cUnit.FindNodeOfTokens(iToken, iToken) : NULL;

			//Named alone (its object is this one), or as a member of something: of this, or of another object.
			for (; pcNode && (pcNode->GetKind() != CPPS_IdExpression) && (pcNode->GetKind() != CPPS_MemberExpression); pcNode = pcNode->mpcParent)
			{
			}
			pcTarget = NULL;
			if (pcNode && (pcNode->GetKind() == CPPS_IdExpression) && (pcNode->miLastToken == iToken))
			{
				pcTarget = pcNode;
			}
			else if (pcNode && (pcNode->GetKind() == CPPS_MemberExpression) && ((CCPPSyntaxMemberExpression*)pcNode)->mpcObject &&
					 (((CCPPSyntaxMemberExpression*)pcNode)->mpcObject->GetKind() == CPPS_ThisExpression) && (pcNode->miLastToken == iToken))
			{
				pcTarget = pcNode;
			}
			if (!pcTarget)
			{
				continue;
			}
			if (psDest->bStatic)
			{
				szName.Init("its body uses its object: it names ");
				szScope.Init();
				GetIDESymbolName(szKey, &szScope);
				szName.Append(szScope);
				szScope.Kill();
				Refuse(psDest, szName.Text(), psLocation->szFileName.Text(), psLocation->iLine);
				szName.Kill();
				break;
			}
			if (bFunction)
			{
				AddConflict(psDest, "It calls ", szKey, ", which isn't const", psLocation->szFileName.Text(), psLocation->iLine);
				continue;
			}
			//Whatever of the field is named: what is done to that is done to the field.
			while (pcTarget->mpcParent && (((pcTarget->mpcParent->GetKind() == CPPS_MemberExpression) && (((CCPPSyntaxMemberExpression*)pcTarget->mpcParent)->mpcObject == pcTarget) &&
					!((CCPPSyntaxMemberExpression*)pcTarget->mpcParent)->mbArrow) ||
				   ((pcTarget->mpcParent->GetKind() == CPPS_SubscriptExpression) && (((CCPPSyntaxSubscriptExpression*)pcTarget->mpcParent)->mpcArray == pcTarget))))
			{
				pcTarget = pcTarget->mpcParent;
			}
			uiChanges = cUnit.GetPass()->FindChanges(pcTarget);
			if (uiChanges & CPPPC_ASSIGNED)
			{
				AddConflict(psDest, "It assigns ", szKey, "", psLocation->szFileName.Text(), psLocation->iLine);
			}
			if (uiChanges & CPPPC_REFERENCE)
			{
				AddConflict(psDest, "It gives ", szKey, " to a reference that isn't const", psLocation->szFileName.Text(), psLocation->iLine);
			}
			if (uiChanges & CPPPC_ADDRESS)
			{
				AddConflict(psDest, "It takes the address of ", szKey, ", for a pointer that isn't const", psLocation->szFileName.Text(), psLocation->iLine);
			}
			if (uiChanges & CPPPC_MEMBER_CALL)
			{
				AddConflict(psDest, "It calls a function that isn't const on ", szKey, "", psLocation->szFileName.Text(), psLocation->iLine);
			}
		}
		KillIDELocations(&asLocations);
	}
	if (psDest->bStatic && psDest->bPossible && cUnit.FindTokensIn(psSite->sBody.iLine, psSite->sBody.iColumn, psSite->sBody.iEndLine, psSite->sBody.iEndColumn, &iFirst, &iLast))
	{
		for (iToken = iFirst; iToken <= iLast; iToken++)
		{
			if (cUnit.IsToken(iToken, "this"))
			{
				Refuse(psDest, "its body uses this", psSite->szFileName.Text(), psSite->iLine);
				break;
			}
		}
	}
	aszKeys.Kill();
	KillIDEFileRanges(&asRanges);
	aszClasses.Kill();
	cUnit.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Make const (or Make static) would change for the member function
// at the caret: every declaration, definition and call, why it can't be
// if it can't, and what its bodies do that a const function can't.
// False if no function is at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMemberQualifier::Find(bool bStatic, SIDEQualifierChange* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDESignatureSite*	psSite;
	CTextFile			cFile;
	CChars				szName;
	CChars				szClassName;
	CChars				szAfter;
	char*				szText;
	size				uiLength;
	size				i;
	uint8				uiKind;
	bool				bClass;

	pcModel = mpcContent->GetCodeModel();
	psDest->bStatic = bStatic;
	if (!mpcContent->GetChangeSignature()->FindSignature(&psDest->sSignature))
	{
		return false;
	}
	psDest->bPossible = true;
	if (!psDest->sSignature.bChangeable)
	{
		Refuse(psDest, psDest->sSignature.szWhyNot.Text());
		return true;
	}
	GetIDESymbolScope(psDest->sSignature.szKey.Text(), &psDest->szClassKey);
	asLocations.Init();
	bClass = !psDest->szClassKey.Empty() && pcModel->GetSymbolLocations(psDest->szClassKey.Text(), &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate));
	KillIDELocations(&asLocations);
	if (!bClass)
	{
		Refuse(psDest, "it isn't a member function");
		return true;
	}
	szName.Init();
	szClassName.Init();
	GetIDESymbolName(psDest->sSignature.szKey.Text(), &szName);
	GetIDESymbolName(psDest->szClassKey.Text(), &szClassName);
	if (szName.Equals(szClassName.Text()) || szName.StartsWith("~"))
	{
		Refuse(psDest, szName.StartsWith("~") ? "a destructor can't be" : "a constructor can't be");
	}
	else if (IsDeclaredWith(psDest->sSignature.szKey.Text(), "static"))
	{
		Refuse(psDest, bStatic ? "it is static already" : "it is static: it has no object to leave unchanged");
	}
	else if (bStatic && ((psDest->sSignature.uiOverrides > 0) || IsDeclaredWith(psDest->sSignature.szKey.Text(), "virtual")))
	{
		Refuse(psDest, "it is virtual: a virtual function is called on an object");
	}
	else if (bStatic && (psDest->sSignature.uiFamily > 0))
	{
		Refuse(psDest, "a template's body calls it on an object, with other classes' functions of its name");
	}
	szClassName.Kill();
	szName.Kill();

	for (i = 0; psDest->bPossible && (i < psDest->sSignature.apsSites.NumElements()); i++)
	{
		psSite = psDest->sSignature.apsSites.GetPtr(i);
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(psSite->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			szAfter.Init();
			szAfter.AppendSubString(szText, FindIDETextOffset(szText, psSite->sAfter.iLine, psSite->sAfter.iColumn), FindIDETextOffset(szText, psSite->sAfter.iEndLine, psSite->sAfter.iEndColumn));
			if (!bStatic && strstr(psSite->szQualifiers.Text(), "const"))
			{
				Refuse(psDest, "it is const already");
			}
			else if (bStatic && (strstr(szAfter.Text(), "override") || strstr(szAfter.Text(), "final")))
			{
				Refuse(psDest, "it is virtual: a virtual function is called on an object");
			}
			else if (bStatic && !psSite->bReturn)
			{
				Refuse(psDest, "a declaration of it has no return type to write static before");
			}
			szAfter.Kill();
		}
		cFile.Kill();
		if (psDest->bPossible && psSite->bBody)
		{
			FindBodyUses(psDest, psSite);
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The change made, as one undo.  Without conflicts (which the user has
// seen, and which don't compile) the files changed are checked as they
// would be first, and nothing is changed if they would have more errors
// than they have.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMemberQualifier::Change(SIDEQualifierChange* psChange, CChars* pszStatus)
{
	CIDEFileEdits				cFiles;
	CIDERefactorUnit			cUnit;
	CIDEEffectFinder			cEffects;
	SIDESignatureSite*			psSite;
	SIDESignatureCall*			psCall;
	CCPPSyntax*					pcNode;
	CCPPSyntaxMemberExpression*	pcMember;
	SCPPSyntaxRange				sObject;
	SCPPSyntaxRange				sName;
	CChars						szMessage;
	CChars						szClass;
	const char*					szConst;
	char*						szText;
	size						uiStart;
	size						uiEnd;
	size						uiLine;
	size						uiCalls;
	size						uiBefore;
	size						i;
	int32						iToken;
	bool						bResult;

	if (!psChange->bPossible)
	{
		return false;
	}
	cFiles.Init(mpcContent);
	bResult = true;
	uiCalls = 0;
	for (i = 0; bResult && (i < psChange->sSignature.apsSites.NumElements()); i++)
	{
		psSite = psChange->sSignature.apsSites.GetPtr(i);
		szText = cFiles.GetText(psSite->szFileName.Text());
		if (!szText)
		{
			bResult = false;
			break;
		}
		uiStart = FindIDETextOffset(szText, psSite->sAfter.iLine, psSite->sAfter.iColumn);
		uiEnd = FindIDETextOffset(szText, psSite->sAfter.iEndLine, psSite->sAfter.iEndColumn);
		if (!psChange->bStatic)
		{
			cFiles.GetEdits(psSite->szFileName.Text())->Insert(uiStart, " const");
			continue;
		}

		//A const after its parameters goes: a static function has no object.
		for (szConst = strstr(&szText[uiStart], "const"); szConst && ((size)(szConst - szText) + 5 <= uiEnd); szConst = strstr(szConst + 1, "const"))
		{
			if (!IsIDENameChar(szConst[-1]) && !IsIDENameChar(szConst[5]))
			{
				uiLine = (size)(szConst - szText);
				cFiles.GetEdits(psSite->szFileName.Text())->Replace(((uiLine > uiStart) && (szConst[-1] == ' ')) ? uiLine - 1 : uiLine, uiLine + 5, "");
				break;
			}
		}
		if (psSite->bInClass)
		{
			//Before its return type; in a column of its own, as virtual is, where it is written after tabs alone.
			uiStart = FindIDETextOffset(szText, psSite->sReturn.iLine, psSite->sReturn.iColumn);
			for (uiLine = uiStart; (uiLine > 0) && (szText[uiLine - 1] == '\t'); uiLine--)
			{
			}
			if ((uiStart - uiLine >= 2) && ((uiLine == 0) || (szText[uiLine - 1] == '\n')))
			{
				cFiles.GetEdits(psSite->szFileName.Text())->Replace(uiLine, uiStart, "\tstatic\t");
			}
			else
			{
				cFiles.GetEdits(psSite->szFileName.Text())->Insert(uiStart, "static ");
			}
		}
	}

	//The calls on an object name the class: "cVat.Count()" is "CVat::Count()", where naming the object does nothing.
	szClass.Init(psChange->szClassKey);
	for (i = 0; bResult && psChange->bStatic && (i < psChange->sSignature.apsCalls.NumElements()); i++)
	{
		psCall = psChange->sSignature.apsCalls.GetPtr(i);
		szText = cFiles.GetText(psCall->szFileName.Text());
		cUnit.Init();
		if (szText && cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), psCall->szFileName.Text(), false))
		{
			iToken = cUnit.FindTokenAt(psCall->iLine, psCall->iColumn);
			pcNode = (iToken != -1) ? cUnit.FindNodeOfTokens(iToken, iToken) : NULL;
			for (; pcNode && (pcNode->GetKind() != CPPS_IdExpression) && (pcNode->GetKind() != CPPS_MemberExpression); pcNode = pcNode->mpcParent)
			{
			}
			pcMember = (pcNode && (pcNode->GetKind() == CPPS_MemberExpression) && (pcNode->miLastToken == iToken)) ? (CCPPSyntaxMemberExpression*)pcNode : NULL;
			if (pcMember && pcMember->mpcObject && pcMember->mpcName)
			{
				cEffects.mbEffects = false;
				cEffects.Look(pcMember->mpcObject);
				if (!cEffects.mbEffects && cUnit.GetRange(pcMember->mpcObject, &sObject) && cUnit.GetTokensRange(pcMember->mpcName->miFirstToken, pcMember->mpcName->miFirstToken, &sName))
				{
					cFiles.GetEdits(psCall->szFileName.Text())->Replace(FindIDETextOffset(szText, sObject.iLine, sObject.iColumn), FindIDETextOffset(szText, sName.iLine, sName.iColumn),
																		 (pcMember->mpcName->miFirstToken == iToken) ? szClass.Text() : "");
					if (pcMember->mpcName->miFirstToken == iToken)
					{
						cFiles.GetEdits(psCall->szFileName.Text())->Insert(FindIDETextOffset(szText, sName.iLine, sName.iColumn), "::");
					}
					uiCalls++;
				}
			}
		}
		cUnit.Kill();
	}
	szClass.Kill();

	if (bResult && (psChange->aszConflicts.NumElements() == 0))
	{
		uiBefore = cFiles.CountErrors(false);
		if (cFiles.CountErrors(true) > uiBefore)
		{
			bResult = false;
		}
	}
	szMessage.Init();
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Made ")->Append(psChange->sSignature.szName)->Append(psChange->bStatic ? " static" : " const");
		if (psChange->sSignature.uiOverrides + psChange->sSignature.uiFamily > 0)
		{
			szMessage.Append(", and ")->Append((uint32)(psChange->sSignature.uiOverrides + psChange->sSignature.uiFamily));
			szMessage.Append((psChange->sSignature.uiOverrides + psChange->sSignature.uiFamily == 1) ? " function with it" : " functions with it");
		}
		if (psChange->bStatic)
		{
			szMessage.Append(": ")->Append((uint32)uiCalls)->Append((uiCalls == 1) ? " call names " : " calls name ")->Append(psChange->szClassKey);
		}
		szMessage.Append('.');
	}
	else
	{
		szMessage.Append("Can't make ")->Append(psChange->sSignature.szName)->Append(psChange->bStatic ? " static" : " const")->Append(": the code doesn't compile with it so.");
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
// Make const or Make static for the function at the caret: what its
// bodies do that a const function can't is shown first, as Problems
// Detected.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMemberQualifier::Show(bool bStatic)
{
	SIDEQualifierChange	sChange;
	CChars				szMessage;
	bool				bResult;

	sChange.Init();
	if (!Find(bStatic, &sChange))
	{
		mpcContent->ShowStatus("There is no member function at the caret.");
		sChange.Kill();
		return false;
	}
	if (!sChange.bPossible)
	{
		szMessage.Init("Can't make ");
		szMessage.Append(sChange.sSignature.szName)->Append(bStatic ? " static: " : " const: ")->Append(sChange.szWhyNot)->Append('.');
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sChange.Kill();
		return false;
	}
	bResult = (sChange.aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&sChange.aszConflicts);
	bResult = bResult && Change(&sChange);
	sChange.Kill();
	return bResult;
}
