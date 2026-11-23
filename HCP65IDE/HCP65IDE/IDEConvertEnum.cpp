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
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "CppCompilerLib/CPPSyntaxEnumSpecifier.h"
#include "CppCompilerLib/CPPSyntaxEnumerator.h"
#include "CppCompilerLib/CPPSyntaxes.h"
#include "CppCompilerLib/CPPSemanticPass.h"
#include "CppCompilerLib/CPP65Type.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDEDefinitions.h"
#include "IDEIncludes.h"
#include "IDEChangeSignature.h"
#include "IDEContextActions.h"
#include "IDEScript.h"
#include "IDEConvertEnum.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEEnumConversion::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szKey.Init();
	szName.Init();
	uiEnumerators = 0;
	aszEnumFiles.Init();
	aiEnumAts.Init();
	aszUseFiles.Init();
	aiUseAts.Init();
	aszCastFiles.Init();
	aiCastAts.Init();
	aiCastSpans.Init();
	aiCastAfter.Init();
	aszCastTexts.Init();
	aszReplaceFiles.Init();
	aiReplaceStarts.Init();
	aiReplaceEnds.Init();
	aszReplaceTexts.Init();
	aszCastPlaces.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEEnumConversion::Kill(void)
{
	aszCastPlaces.Kill();
	aszReplaceTexts.Kill();
	aiReplaceEnds.Kill();
	aiReplaceStarts.Kill();
	aszReplaceFiles.Kill();
	aszCastTexts.Kill();
	aiCastAfter.Kill();
	aiCastSpans.Kill();
	aiCastAts.Kill();
	aszCastFiles.Kill();
	aiUseAts.Kill();
	aszUseFiles.Kill();
	aiEnumAts.Kill();
	aszEnumFiles.Kill();
	szName.Kill();
	szKey.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mbWriteCasts = true;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	U_Bool(mbWriteCasts);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::Free(void)
{
}


bool CIDEConvertEnum::Save(CObjectWriter* pcFile) { return false; }
bool CIDEConvertEnum::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Refactor's Convert Enum to Enum Class.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.EnumToEnumClass", "Convert Enum to Enum Class...", "", Ptr<CImageCel>(), this, &CIDEConvertEnum::QueueConvert)->SetEnabledCheck(this, &CIDEConvertEnum::CanConvert);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.EnumToEnumClass"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: it is for an enum or an enumerator.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bSymbol && ((psContext->uiKind == CPPSK_Enum) || (psContext->uiKind == CPPSK_Enumerator)))
	{
		psContext->Offer("Refactor.EnumToEnumClass");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::CanConvert(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::QueueConvert(void) { mpcContent->GetWindow()->Post(this, &CIDEConvertEnum::ConvertQueued); }
void CIDEConvertEnum::ConvertQueued(void) { ShowConvert(); }


//////////////////////////////////////////////////////////////////////////
//
// Why it can't be, with the place if there is one: "(Tank.cpp line 12)".
// Returns true: something was found, though it can't be converted.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::Refuse(SIDEEnumConversion* psDest, const char* szWhyNot, const char* szFileName, int32 iLine)
{
	psDest->bPossible = false;
	if (psDest->szWhyNot.Empty())
	{
		psDest->szWhyNot.Set(szWhyNot);
		if (szFileName)
		{
			psDest->szWhyNot.Append(" (")->Append(GetIDEFileNamePart(szFileName));
			if (iLine >= 0)
			{
				psDest->szWhyNot.Append(" line ")->Append((int32)(iLine + 1));
			}
			psDest->szWhyNot.Append(")");
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether "enum" is written before a name at uiName, and where it ends.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::FindEnumKeyword(const char* szText, size uiName, size* puiKeywordEnd)
{
	size	i;

	for (i = uiName; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t') || (szText[i - 1] == '\r') || (szText[i - 1] == '\n')); i--)
	{
	}
	if ((i < 4) || (i == uiName) || (strncmp(&szText[i - 4], "enum", 4) != 0) || ((i > 4) && IsIDENameChar(szText[i - 5])))
	{
		return false;
	}
	*puiKeywordEnd = i;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the name at uiName is written after the enum's name and "::".
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::IsQualified(const char* szText, size uiName, const char* szEnumName)
{
	size	i;
	size	uiEnd;

	for (i = uiName; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t')); i--)
	{
	}
	if ((i < 2) || (szText[i - 1] != ':') || (szText[i - 2] != ':'))
	{
		return false;
	}
	for (i -= 2; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t')); i--)
	{
	}
	uiEnd = i;
	for (; (i > 0) && IsIDENameChar(szText[i - 1]); i--)
	{
	}
	return (uiEnd - i == strlen(szEnumName)) && (strncmp(&szText[i], szEnumName, uiEnd - i) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The places an enumerator is named that are given the enum's name: not
// those in the enum's own list, nor one written with it already.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::FindUses(const char* szEnumeratorKey, const char* szEnumerator, const char* szEnumFile, size uiListStart, size uiListEnd, SIDEEnumConversion* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CTextFile			cFile;
	const char*			szText;
	size				uiLength;
	size				uiAt;
	size				i;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	asLocations.Init();
	pcModel->GetSymbolLocations(szEnumeratorKey, &asLocations, &uiKind);
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole != CPPIR_Reference)
		{
			continue;
		}
		cFile.Init();
		szText = NULL;
		if (!mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength) || !szText)
		{
			Refuse(psDest, "a file that names one of its enumerators can't be read", psLocation->szFileName.Text());
			cFile.Kill();
			continue;
		}
		uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
		if ((strncmp(&szText[uiAt], szEnumerator, strlen(szEnumerator)) != 0) || IsIDENameChar(szText[uiAt + strlen(szEnumerator)]))
		{
			Refuse(psDest, "a macro names one of its enumerators", psLocation->szFileName.Text(), psLocation->iLine);
		}
		else if ((StringInsensitiveCompare(psLocation->szFileName.Text(), szEnumFile) == 0) && (uiAt >= uiListStart) && (uiAt < uiListEnd))
		{
			//In its own list: the enumerators before one are in reach there.
		}
		else if (IsQualified(szText, uiAt, psDest->szName.Text()))
		{
		}
		else if (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE)
		{
			Refuse(psDest, "a file that isn't one of the solution's names one of its enumerators", psLocation->szFileName.Text(), psLocation->iLine);
		}
		else
		{
			psDest->aszUseFiles.Add(psLocation->szFileName.Text());
			psDest->aiUseAts.Add((int)uiAt);
		}
		cFile.Kill();
	}
	KillIDELocations(&asLocations);
}


//////////////////////////////////////////////////////////////////////////
//
// What Convert Enum to Enum Class has to work with, for the enum at the
// caret or the enum of the enumerator there.  False if neither is there;
// psDest->bPossible says whether it can be converted, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::FindConversion(SIDEEnumConversion* psDest)
{
	CIDECodeModel*				pcModel;
	Ptr<CIDEEditorTab>			pTab;
	CIDERefactorUnit			cUnit;
	CArrayIDELocations			asLocations;
	SIDELocation*				psLocation;
	SIDELocation*				psDefined;
	CCPPSyntax*					pcNode;
	CCPPSyntaxEnumSpecifier*	pcSpecifier;
	CCPPSyntaxEnumerator*		pcEnumerator;
	SCPPSyntaxRange				sRange;
	CTextFile					cFile;
	CChars						szKey;
	CChars						szEnumFile;
	CChars						szEnumeratorKey;
	CChars						szEnumerator;
	const char*					szText;
	size						uiLength;
	size						uiListStart;
	size						uiListEnd;
	size						uiAt;
	size						uiEnd;
	size						i;
	int							iLine;
	int							iColumn;
	uint8						uiRole;
	uint8						uiKind;

	pcModel = mpcContent->GetCodeModel();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szKey.Init();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) || !pcModel->GetSymbolAt(pTab->GetFileName(), iLine, iColumn, &szKey, &uiKind) ||
		((uiKind != CPPSK_Enum) && (uiKind != CPPSK_Enumerator)))
	{
		szKey.Kill();
		return false;
	}
	GetIDESymbolName(szKey.Text(), &psDest->szName);
	psDest->bPossible = true;

	//Where it (or the enumerator) is defined, and the enum's syntax there.
	asLocations.Init();
	pcModel->GetSymbolLocations(szKey.Text(), &asLocations, &uiKind);
	psDefined = NULL;
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole == CPPIR_Definition) || ((psLocation->uiRole == CPPIR_Declaration) && !psDefined))
		{
			psDefined = psLocation;
			if (psLocation->uiRole == CPPIR_Definition)
			{
				break;
			}
		}
	}
	szEnumFile.Init(psDefined ? psDefined->szFileName.Text() : "");
	cUnit.Init();
	cFile.Init();
	szText = NULL;
	pcSpecifier = NULL;
	if (psDefined && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), szEnumFile.Text(), false) && mpcContent->GetFindInFiles()->GetFileText(szEnumFile.Text(), &cFile, &szText, &uiLength) && szText)
	{
		pcNode = cUnit.FindNodeAt(psDefined->iLine, psDefined->iColumn);
		pcSpecifier = pcNode ? (CCPPSyntaxEnumSpecifier*)cUnit.FindAncestor(pcNode, CPPS_EnumSpecifier) : NULL;
	}
	KillIDELocations(&asLocations);
	uiListStart = 0;
	uiListEnd = 0;
	if (!pcSpecifier || !pcSpecifier->mbBody || !cUnit.GetRange(pcSpecifier, &sRange))
	{
		Refuse(psDest, "its definition can't be read (a macro makes it, or it isn't in the solution)");
	}
	else
	{
		uiListStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
		uiListEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		if (!pcSpecifier->mpcName || !pcSpecifier->mpcName->GetLast() || !cUnit.GetTokensRange(pcSpecifier->mpcName->GetLast()->miFirstToken, pcSpecifier->mpcName->GetLast()->miFirstToken, &sRange) ||
			!pcModel->GetSymbolAt(szEnumFile.Text(), sRange.iLine, sRange.iColumn, &psDest->szKey, &uiKind) || (uiKind != CPPSK_Enum))
		{
			psDest->szName.Set("The enum");
			Refuse(psDest, "it has no name");
		}
		else
		{
			psDest->szName.Clear();
			GetIDESymbolName(psDest->szKey.Text(), &psDest->szName);
			if (pcSpecifier->mbScoped)
			{
				Refuse(psDest, "it is an enum class already");
			}
		}
	}

	//Its declarations: each "enum" becomes "enum class".
	if (psDest->bPossible)
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
		for (i = 0; i < asLocations.NumElements(); i++)
		{
			psLocation = asLocations.Get(i);
			if (psLocation->uiRole == CPPIR_Reference)
			{
				continue;
			}
			cFile.Kill();
			cFile.Init();
			szText = NULL;
			if (!mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength) || !szText || (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE))
			{
				Refuse(psDest, "it is declared in a file that isn't one of the solution's", psLocation->szFileName.Text(), psLocation->iLine);
				continue;
			}
			uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
			if (!FindEnumKeyword(szText, uiAt, &uiEnd))
			{
				Refuse(psDest, "a declaration of it can't be read (a macro makes it, or it is written with a scope)", psLocation->szFileName.Text(), psLocation->iLine);
				continue;
			}
			psDest->aszEnumFiles.Add(psLocation->szFileName.Text());
			psDest->aiEnumAts.Add((int)uiEnd);
		}
		KillIDELocations(&asLocations);
	}

	//Its enumerators' uses.
	for (i = 0; psDest->bPossible && (i < pcSpecifier->macpEnumerators.NumElements()); i++)
	{
		pcEnumerator = (CCPPSyntaxEnumerator*)pcSpecifier->macpEnumerators.GetPtr(i);
		szEnumeratorKey.Init();
		szEnumerator.Init();
		if (!cUnit.GetTokensRange(pcEnumerator->miName, pcEnumerator->miName, &sRange) || !pcModel->GetSymbolAt(szEnumFile.Text(), sRange.iLine, sRange.iColumn, &szEnumeratorKey, &uiKind))
		{
			Refuse(psDest, "one of its enumerators can't be read (a macro makes it)");
		}
		else
		{
			cUnit.AppendTokens(pcEnumerator->miName, pcEnumerator->miName, &szEnumerator);
			FindUses(szEnumeratorKey.Text(), szEnumerator.Text(), szEnumFile.Text(), uiListStart, uiListEnd, psDest);
			psDest->uiEnumerators++;
		}
		szEnumerator.Kill();
		szEnumeratorKey.Kill();
	}
	cFile.Kill();
	cUnit.Kill();
	szEnumFile.Kill();
	szKey.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Something to write at a place for a cast: before an expression uiSpan
// long that starts there, or after one that ends there.  Once.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::AddInsert(SIDEEnumConversion* psDest, const char* szFileName, size uiAt, size uiSpan, bool bAfter, const char* szText)
{
	size	i;

	for (i = 0; i < psDest->aszCastFiles.NumElements(); i++)
	{
		if (((size)psDest->aiCastAts.GetValue(i) == uiAt) && ((size)psDest->aiCastSpans.GetValue(i) == uiSpan) && ((psDest->aiCastAfter.GetValue(i) != 0) == bAfter) &&
			psDest->aszCastTexts.Get(i)->Equals(szText) && (StringInsensitiveCompare(psDest->aszCastFiles.GetText(i), szFileName) == 0))
		{
			return;
		}
	}
	psDest->aszCastFiles.Add(szFileName);
	psDest->aiCastAts.Add((int)uiAt);
	psDest->aiCastSpans.Add((int)uiSpan);
	psDest->aiCastAfter.Add(bAfter ? 1 : 0);
	psDest->aszCastTexts.Add(szText);
}


//////////////////////////////////////////////////////////////////////////
//
// A cast written before an expression of the file: "(int)" before a name,
// a call or something in brackets, "(int)(" and ")" round anything else.
// False if the expression isn't the file's own text (another file's, or a
// macro's).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::AddCast(CIDERefactorUnit* pcUnit, const char* szText, const char* szFileName, CCPPSyntax* pcNode, const char* szCast, SIDEEnumConversion* psDest)
{
	SCPPSyntaxRange	sRange;
	CChars			szBefore;
	CChars			szPlace;
	size			uiStart;
	size			uiEnd;
	bool			bWhole;

	if (!pcNode || !pcUnit->GetRange(pcNode, &sRange))
	{
		return false;
	}
	uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
	uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
	switch (pcNode->GetKind())
	{
	case CPPS_IdExpression:
	case CPPS_LiteralExpression:
	case CPPS_ParenthesisedExpression:
	case CPPS_CallExpression:
	case CPPS_MemberExpression:
	case CPPS_SubscriptExpression:
	case CPPS_ThisExpression:
		bWhole = true;
		break;
	default:
		bWhole = false;
		break;
	}
	szBefore.Init(szCast);
	if (!bWhole)
	{
		szBefore.Append('(');
		AddInsert(psDest, szFileName, uiEnd, uiEnd - uiStart, true, ")");
	}
	AddInsert(psDest, szFileName, uiStart, uiEnd - uiStart, false, szBefore.Text());
	szBefore.Kill();

	szPlace.Init(GetIDEFileNamePart(szFileName));
	szPlace.Append(" line ")->Append((int32)(sRange.iLine + 1));
	psDest->aszCastPlaces.AddIfUnique(szPlace.Text());
	szPlace.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether eOperator is one of aeCompound's uiNumCompound assignments, as
// |= is.
//
//////////////////////////////////////////////////////////////////////////
static bool IsCompoundAssignment(ECppTokens eOperator, const ECppTokens* aeCompound, size uiNumCompound)
{
	size	i;

	for (i = 0; i < uiNumCompound; i++)
	{
		if (eOperator == aeCompound[i])
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The casts one file needs: it is checked with the enum taken as an enum
// class, and each place the check records is given its cast (see the
// class's comment).  Only the file's own text: a header's are found when
// it is the file.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::FindFileCasts(const char* szFileName, SIDEEnumConversion* psDest)
{
	static const ECppTokens		aeCompound[] = { CT_AssignmentAdd, CT_AssignmentSubtract, CT_AssignmentMultiply, CT_AssignmentDivide, CT_AssignmentModulus, CT_AssignmentBitwiseAnd,
												 CT_AssignmentBitwiseOr, CT_AssignmentBitwiseXor, CT_AssignmentLeftShift, CT_AssignmentRightShift };
	CIDERefactorUnit			cUnit;
	CCPPSemanticPass*			pcPass;
	SCPPEnumCast*				psCast;
	CCPPSyntaxBinaryExpression*	pcBinary;
	CCPPSyntax*					pcNode;
	SCPPSyntaxRange				sRange;
	SCPPSyntaxRange				sLeft;
	CTextFile					cFile;
	CChars						szEnum;
	CChars						szCast;
	CChars						szOperator;
	const char*					szText;
	size						uiLength;
	size						uiStart;
	size						uiEnd;
	size						i;
	bool						bCompound;

	cFile.Init();
	cUnit.Init();
	cUnit.SetRecordedEnum(psDest->szKey.Text());
	szText = NULL;
	if (!mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength) || !szText || !cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), szFileName, true))
	{
		cUnit.Kill();
		cFile.Kill();
		return;
	}
	pcPass = cUnit.GetPass();
	szEnum.Init();
	if (!pcPass->GetRecordedEnumType() || !pcPass->AppendType(pcPass->GetRecordedEnumType(), &szEnum))
	{
		szEnum.Set(psDest->szName);
	}
	szEnum.Insert(0, "(");
	szEnum.Append(')');
	for (i = 0; i < pcPass->NumEnumCasts(); i++)
	{
		psCast = pcPass->GetEnumCast(i);
		pcNode = psCast->pcNode;
		if (!pcNode || !cUnit.GetRange(pcNode, &sRange))
		{
			continue;
		}
		pcBinary = (pcNode->GetKind() == CPPS_BinaryExpression) ? (CCPPSyntaxBinaryExpression*)pcNode : NULL;
		switch (psCast->uiKind)
		{
		case CPPEC_TO_OTHER:
			szCast.Init("(");
			if (!psCast->psTo || !pcPass->AppendType(psCast->psTo->IsReference() ? psCast->psTo->psBase->Unqualified() : psCast->psTo->Unqualified(), &szCast))
			{
				szCast.Set("(int");
			}
			szCast.Append(')');
			AddCast(&cUnit, szText, szFileName, pcNode, szCast.Text(), psDest);
			szCast.Kill();
			break;
		case CPPEC_TO_ENUM:
			//"e |= x" written out (below, as its operator) is "e = (E)((int)e | x)": its own cast to the enum, not another
			//round it.
			if (!(pcBinary && IsCompoundAssignment(pcBinary->meOperator, aeCompound, sizeof(aeCompound) / sizeof(ECppTokens)) && pcPass->IsRecordedEnum(pcPass->GetType(pcBinary->mpcLeft))))
			{
				AddCast(&cUnit, szText, szFileName, pcNode, szEnum.Text(), psDest);
			}
			break;
		case CPPEC_CONDITION:
			AddCast(&cUnit, szText, szFileName, pcNode, "(int)", psDest);
			break;
		case CPPEC_INCREMENT:
			Refuse(psDest, "its value is stepped with ++ or --, which an enum class's can't be", szFileName, sRange.iLine);
			break;
		default:
			//An operator: each operand that is the enum's value.
			bCompound = pcBinary && IsCompoundAssignment(pcBinary->meOperator, aeCompound, sizeof(aeCompound) / sizeof(ECppTokens));
			if (bCompound && pcPass->IsRecordedEnum(pcPass->GetType(pcBinary->mpcLeft)) && cUnit.GetRange(pcBinary->mpcLeft, &sLeft) &&
				cUnit.GetTokensRange(pcBinary->mpcLeft->miLastToken + 1, pcBinary->mpcLeft->miLastToken + 1, &sRange))
			{
				//"e |= x" written out: "e = (E)((int)e | x)".
				uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
				uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
				szOperator.Init();
				szOperator.AppendSubString(szText, uiStart, uiEnd - 1);
				szCast.Init("= ");
				szCast.Append(szEnum)->Append("((int)");
				szCast.AppendSubString(szText, FindIDETextOffset(szText, sLeft.iLine, sLeft.iColumn), FindIDETextOffset(szText, sLeft.iEndLine, sLeft.iEndColumn));
				szCast.Append(' ')->Append(szOperator);
				psDest->aszReplaceFiles.Add(szFileName);
				psDest->aiReplaceStarts.Add((int)uiStart);
				psDest->aiReplaceEnds.Add((int)uiEnd);
				psDest->aszReplaceTexts.Add(szCast.Text());
				szCast.Kill();
				szOperator.Kill();
				if (cUnit.GetRange(pcBinary->mpcRight, &sRange))
				{
					uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
					AddInsert(psDest, szFileName, uiEnd, uiEnd, true, ")");
				}
				if (pcPass->IsRecordedEnum(pcPass->GetType(pcBinary->mpcRight)))
				{
					AddCast(&cUnit, szText, szFileName, pcBinary->mpcRight, "(int)", psDest);
				}
				szCast.Init(GetIDEFileNamePart(szFileName));
				szCast.Append(" line ")->Append((int32)(sLeft.iLine + 1));
				psDest->aszCastPlaces.AddIfUnique(szCast.Text());
				szCast.Kill();
			}
			else if (pcBinary)
			{
				if (pcPass->IsRecordedEnum(pcPass->GetType(pcBinary->mpcLeft)))
				{
					AddCast(&cUnit, szText, szFileName, pcBinary->mpcLeft, "(int)", psDest);
				}
				if (pcPass->IsRecordedEnum(pcPass->GetType(pcBinary->mpcRight)))
				{
					AddCast(&cUnit, szText, szFileName, pcBinary->mpcRight, "(int)", psDest);
				}
			}
			else if (pcNode->GetKind() == CPPS_UnaryExpression)
			{
				AddCast(&cUnit, szText, szFileName, ((CCPPSyntaxUnaryExpression*)pcNode)->mpcOperand, "(int)", psDest);
			}
			else if (pcNode->GetKind() == CPPS_SubscriptExpression)
			{
				if (pcPass->IsRecordedEnum(pcPass->GetType(((CCPPSyntaxSubscriptExpression*)pcNode)->mpcIndex)))
				{
					AddCast(&cUnit, szText, szFileName, ((CCPPSyntaxSubscriptExpression*)pcNode)->mpcIndex, "(int)", psDest);
				}
				if (pcPass->IsRecordedEnum(pcPass->GetType(((CCPPSyntaxSubscriptExpression*)pcNode)->mpcArray)))
				{
					AddCast(&cUnit, szText, szFileName, ((CCPPSyntaxSubscriptExpression*)pcNode)->mpcArray, "(int)", psDest);
				}
			}
			break;
		}
	}
	szEnum.Kill();
	cUnit.Kill();
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The casts the enum's value needs once it is an enum class's: in its own
// file and every file of the solutions that reaches it by its #includes.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::FindCasts(SIDEEnumConversion* psDest)
{
	CIDECodeModel*		pcModel;
	CIDEIncludeGraph	cGraph;
	CArrayChars			aszSeen;
	char*				szFileName;
	size				i;
	size				j;

	if (!psDest->bPossible)
	{
		return;
	}
	pcModel = mpcContent->GetCodeModel();
	cGraph.Init(pcModel, &mpcContent->GetFindInFiles());
	aszSeen.Init();
	for (j = 0; j < psDest->aszEnumFiles.NumElements(); j++)
	{
		if (!aszSeen.Contains(psDest->aszEnumFiles.GetText(j)))
		{
			aszSeen.Add(psDest->aszEnumFiles.GetText(j));
			FindFileCasts(psDest->aszEnumFiles.GetText(j), psDest);
		}
	}
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFileName = pcModel->GetProjectFile(i);
		for (j = 0; !aszSeen.Contains(szFileName) && (j < psDest->aszEnumFiles.NumElements()); j++)
		{
			if (cGraph.Reaches(cGraph.Get(szFileName), cGraph.Get(psDest->aszEnumFiles.GetText(j)), -1, NULL))
			{
				aszSeen.Add(szFileName);
				FindFileCasts(szFileName, psDest);
			}
		}
	}
	aszSeen.Kill();
	cGraph.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// How many files a list of them names.
//
//////////////////////////////////////////////////////////////////////////
static size CountIDEEnumFiles(CArrayChars* paszFiles, CArrayChars* paszMore)
{
	CArrayChars	aszSeen;
	size		uiFiles;
	size		i;

	aszSeen.Init();
	for (i = 0; i < paszFiles->NumElements(); i++)
	{
		aszSeen.AddIfUnique(paszFiles->GetText(i));
	}
	for (i = 0; paszMore && (i < paszMore->NumElements()); i++)
	{
		aszSeen.AddIfUnique(paszMore->GetText(i));
	}
	uiFiles = aszSeen.NumElements();
	aszSeen.Kill();
	return uiFiles;
}


//////////////////////////////////////////////////////////////////////////
//
// The enum made an enum class and its enumerators' uses given its name, if
// the files compile that way.  One undo across them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::Convert(SIDEEnumConversion* psConversion, CChars* pszStatus)
{
	CIDEFileEdits	cFiles;
	CChars			szMessage;
	CChars			szPrefix;
	CChars			szAll;
	size			uiBefore;
	size			uiAfter;
	size			uiUses;
	size			uiBest;
	size			i;
	size			j;
	int				iPass;
	bool			bResult;

	szMessage.Init();
	bResult = psConversion->bPossible;
	uiUses = psConversion->aszUseFiles.NumElements();
	cFiles.Init(mpcContent);
	if (!bResult)
	{
		szMessage.Append("Can't make ")->Append(psConversion->szName)->Append(" an enum class: ")->Append(psConversion->szWhyNot)->Append('.');
	}
	szPrefix.Init(psConversion->szName);
	szPrefix.Append("::");
	for (i = 0; bResult && (i < psConversion->aszEnumFiles.NumElements()); i++)
	{
		bResult = (cFiles.GetText(psConversion->aszEnumFiles.GetText(i)) != NULL);
		if (bResult)
		{
			cFiles.GetEdits(psConversion->aszEnumFiles.GetText(i))->Insert((size)psConversion->aiEnumAts.GetValue(i), " class");
		}
	}
	//The names are written inside whatever cast is written at the same place: they are a cast's of no length.
	for (i = 0; bResult && (i < psConversion->aszUseFiles.NumElements()); i++)
	{
		AddInsert(psConversion, psConversion->aszUseFiles.GetText(i), (size)psConversion->aiUseAts.GetValue(i), 0, false, szPrefix.Text());
	}
	szPrefix.Kill();
	for (i = 0; bResult && (i < psConversion->aszCastFiles.NumElements()); i++)
	{
		if (psConversion->aiCastAts.GetValue(i) < 0)
		{
			continue;
		}
		bResult = (cFiles.GetText(psConversion->aszCastFiles.GetText(i)) != NULL);

		//Everything written at this place, in order: what closes the shorter expressions that end here first, then
		//what opens the longer ones that start here.
		szAll.Init();
		for (iPass = 0; iPass < 2; iPass++)
		{
			for (;;)
			{
				uiBest = MAX_SIZE;
				for (j = i; j < psConversion->aszCastFiles.NumElements(); j++)
				{
					if ((psConversion->aiCastAts.GetValue(j) == psConversion->aiCastAts.GetValue(i)) && ((psConversion->aiCastAfter.GetValue(j) != 0) == (iPass == 0)) && (psConversion->aiCastSpans.GetValue(j) >= 0) &&
						(StringInsensitiveCompare(psConversion->aszCastFiles.GetText(j), psConversion->aszCastFiles.GetText(i)) == 0) &&
						((uiBest == MAX_SIZE) || ((iPass == 0) ? (psConversion->aiCastSpans.GetValue(j) < psConversion->aiCastSpans.GetValue(uiBest)) : (psConversion->aiCastSpans.GetValue(j) > psConversion->aiCastSpans.GetValue(uiBest)))))
					{
						uiBest = j;
					}
				}
				if (uiBest == MAX_SIZE)
				{
					break;
				}
				szAll.Append(psConversion->aszCastTexts.GetText(uiBest));
				*psConversion->aiCastSpans.Get(uiBest) = -1;
			}
		}
		if (bResult && !szAll.Empty())
		{
			cFiles.GetEdits(psConversion->aszCastFiles.GetText(i))->Insert((size)psConversion->aiCastAts.GetValue(i), szAll.Text());
		}
		szAll.Kill();
	}
	for (i = 0; bResult && (i < psConversion->aszReplaceFiles.NumElements()); i++)
	{
		bResult = (cFiles.GetText(psConversion->aszReplaceFiles.GetText(i)) != NULL);
		if (bResult)
		{
			cFiles.GetEdits(psConversion->aszReplaceFiles.GetText(i))->Replace((size)psConversion->aiReplaceStarts.GetValue(i), (size)psConversion->aiReplaceEnds.GetValue(i), psConversion->aszReplaceTexts.GetText(i));
		}
	}
	if (!bResult && szMessage.Empty())
	{
		szMessage.Append("Can't make ")->Append(psConversion->szName)->Append(" an enum class: a file it is in couldn't be opened.");
	}
	if (bResult)
	{
		uiBefore = cFiles.CountErrors(false);
		uiAfter = cFiles.CountErrors(true);
		if (uiAfter > uiBefore)
		{
			bResult = false;
			szMessage.Append("Can't make ")->Append(psConversion->szName)->Append(" an enum class: the code doesn't compile with it one, even with the casts that could be written (");
			szMessage.Append((uint32)(uiAfter - uiBefore))->Append((uiAfter - uiBefore == 1) ? " error" : " errors")->Append(").");
		}
	}
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Made ")->Append(psConversion->szName)->Append(" an enum class: ")->Append((uint32)uiUses);
		szMessage.Append((uiUses == 1) ? " use of its enumerators given its name, in " : " uses of its enumerators given its name, in ");
		i = CountIDEEnumFiles(&psConversion->aszEnumFiles, &psConversion->aszUseFiles);
		szMessage.Append((uint32)i)->Append((i == 1) ? " file" : " files");
		if (psConversion->aszCastPlaces.NumElements() > 0)
		{
			szMessage.Append("; casts written at ")->Append((uint32)psConversion->aszCastPlaces.NumElements())->Append((psConversion->aszCastPlaces.NumElements() == 1) ? " place" : " places");
		}
		szMessage.Append('.');
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
// The command: the enum at the caret found and converted.  False, with
// pszStatus left empty, if there is none there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::ConvertAtCaret(CChars* pszStatus, bool bWriteCasts)
{
	SIDEEnumConversion	sConversion;
	CArrayChars			aszAsked;
	CChars				szLine;
	size				i;
	bool				bResult;

	sConversion.Init();
	if (!FindConversion(&sConversion))
	{
		if (!pszStatus)
		{
			mpcContent->ShowStatus("There is no enum at the caret to make an enum class.");
		}
		sConversion.Kill();
		return false;
	}
	FindCasts(&sConversion);
	if (!bWriteCasts && sConversion.bPossible && (sConversion.aszCastPlaces.NumElements() > 0))
	{
		//Casts aren't to be written: the places that would need them say why not.
		szLine.Init("its values are used as integers, and casts aren't to be written: ");
		szLine.Append(sConversion.aszCastPlaces.GetText(0));
		if (sConversion.aszCastPlaces.NumElements() > 1)
		{
			szLine.Append(" and ")->Append((uint32)(sConversion.aszCastPlaces.NumElements() - 1))->Append((sConversion.aszCastPlaces.NumElements() == 2) ? " other place" : " other places");
		}
		Refuse(&sConversion, szLine.Text());
		szLine.Kill();
	}
	bResult = true;
	if (!pszStatus && sConversion.bPossible && (sConversion.aszCastPlaces.NumElements() > 0))
	{
		//The places a cast is written, first.
		aszAsked.Init();
		for (i = 0; i < sConversion.aszCastPlaces.NumElements(); i++)
		{
			szLine.Init(sConversion.aszCastPlaces.GetText(i));
			szLine.Append(": its value is used as an integer, or an integer as its value: a cast is written.");
			aszAsked.Add(szLine.Text());
			szLine.Kill();
		}
		bResult = mpcContent->GetChangeSignature()->ShowConflicts(&aszAsked);
		aszAsked.Kill();
		if (!bResult)
		{
			mpcContent->ShowStatus("Convert Enum to Enum Class: nothing was changed.");
		}
	}
	bResult = bResult && Convert(&sConversion, pszStatus);
	sConversion.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The command: its dialog (whether casts are written), then the
// conversion.  False if nothing changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::ShowConvert(void)
{
	SIDEEnumConversion	sConversion;
	Ptr<CCheckBox>		pCasts;
	CChars				szTitle;
	bool				bResult;

	sConversion.Init();
	if (!FindConversion(&sConversion))
	{
		mpcContent->ShowStatus("There is no enum at the caret to make an enum class.");
		sConversion.Kill();
		return false;
	}
	szTitle.Init("Convert ");
	szTitle.Append(sConversion.szName)->Append(" to Enum Class");
	sConversion.Kill();
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szTitle.Text());
	szTitle.Kill();
	pCasts = mpDialog->AddCheckBox("Write casts where its values are used as integers", mbWriteCasts);
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(480);
	mpcContent->GetScript()->StopWatchingDialog();
	mbWriteCasts = pCasts->IsTicked();
	pCasts = NULL;
	mpDialog = NULL;
	return bResult && ConvertAtCaret(NULL, mbWriteCasts);
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEConvertEnum::PrintConversion(SIDEEnumConversion* psConversion, CChars* pszDest)
{
	size	uiFiles;

	pszDest->Append("Convert Enum: ")->Append(psConversion->szName);
	if (!psConversion->bPossible)
	{
		pszDest->Append(" can't be: ")->Append(psConversion->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	uiFiles = CountIDEEnumFiles(&psConversion->aszUseFiles, NULL);
	pszDest->Append(", ")->Append((uint32)psConversion->uiEnumerators)->Append((psConversion->uiEnumerators == 1) ? " enumerator, " : " enumerators, ");
	pszDest->Append((uint32)psConversion->aszEnumFiles.NumElements())->Append((psConversion->aszEnumFiles.NumElements() == 1) ? " declaration, " : " declarations, ");
	pszDest->Append((uint32)psConversion->aszUseFiles.NumElements())->Append((psConversion->aszUseFiles.NumElements() == 1) ? " use in " : " uses in ");
	pszDest->Append((uint32)uiFiles)->Append((uiFiles == 1) ? " file" : " files");
	if (psConversion->aszCastPlaces.NumElements() > 0)
	{
		pszDest->Append(", casts at ")->Append((uint32)psConversion->aszCastPlaces.NumElements())->Append((psConversion->aszCastPlaces.NumElements() == 1) ? " place" : " places");
	}
	pszDest->Append('\n');
	return pszDest->Text();
}
