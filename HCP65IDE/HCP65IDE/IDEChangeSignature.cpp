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
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEIntroduceVariable.h"
#include "IDERenameAssembly.h"
#include "IDEChangeSignatureDialog.h"
#include "IDEChangeSignature.h"


#define IDE_SIGNATURE_CONFLICTS_SHOWN	12	//Problems Detected lists this many, and says how many more there are.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool IsAllTabs(const char* szText, size uiStart, size uiEnd)
{
	size	i;

	for (i = uiStart; i < uiEnd; i++)
	{
		if (szText[i] != '\t')
		{
			return false;
		}
	}
	return uiEnd > uiStart;
}


//The specifiers before a function's type that aren't of what it returns.
static const char*	gaszIDENotReturned[] = { "virtual", "static", "inline", "explicit", "friend", "extern", "constexpr", "__inline", "__forceinline" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignatureSite::Init(void)
{
	szFileName.Init();
	szKey.Init();
	iLine = 0;
	iColumn = 0;
	memset(&sParameters, 0, sizeof(SCPPSyntaxRange));
	memset(&sReturn, 0, sizeof(SCPPSyntaxRange));
	bReturn = false;
	bDefinition = false;
	bFirst = false;
	szLabel.Init();
	uiNewName = 0;
	memset(&sAll, 0, sizeof(SCPPSyntaxRange));
	memset(&sName, 0, sizeof(SCPPSyntaxRange));
	memset(&sAfter, 0, sizeof(SCPPSyntaxRange));
	memset(&sBody, 0, sizeof(SCPPSyntaxRange));
	bBody = false;
	bInClass = false;
	bPure = false;
	szOwnName.Init();
	szQualifiers.Init();
	szOwnTemplate.Init();
	szClassQualifier.Init();
	szClassTemplate.Init();
	asParameters.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignatureSite::Kill(void)
{
	size	i;

	for (i = 0; i < asParameters.NumElements(); i++)
	{
		asParameters.Get(i)->szKey.Kill();
	}
	asParameters.Kill();
	szClassTemplate.Kill();
	szClassQualifier.Kill();
	szOwnTemplate.Kill();
	szQualifiers.Kill();
	szOwnName.Kill();
	szLabel.Kill();
	szKey.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignatureCall::Init(void)
{
	szFileName.Init();
	iLine = 0;
	iColumn = 0;
	memset(&sArguments, 0, sizeof(SCPPSyntaxRange));
	asArguments.Init();
	aiEffects.Init();
	bValueUsed = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignatureCall::Kill(void)
{
	aiEffects.Kill();
	asArguments.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENewParameter::Init(int32 iOldIndex)
{
	iOld = iOldIndex;
	szType.Init();
	szName.Init();
	szDefault.Init();
	bDefaultChanged = false;
	szCallValue.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENewParameter::Kill(void)
{
	szCallValue.Kill();
	szDefault.Kill();
	szName.Kill();
	szType.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDENewParameters(CArrayIDENewParameters* pasParameters)
{
	size	i;

	for (i = 0; i < pasParameters->NumElements(); i++)
	{
		pasParameters->Get(i)->Kill();
	}
	pasParameters->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignatureChange::Init(void)
{
	asParameters.Init();
	szName.Init();
	szReturn.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignatureChange::Kill(void)
{
	szReturn.Kill();
	szName.Kill();
	KillIDENewParameters(&asParameters);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignature::Init(void)
{
	szKey.Init();
	szName.Init();
	bChangeable = false;
	szWhyNot.Init();
	uiOverrides = 0;
	uiFamily = 0;
	szFamily.Init();
	apsSites.Init();
	apsCalls.Init();
	asOtherUses.Init();
	aszOldTypes.Init();
	aszOldNames.Init();
	aszOldDefaults.Init();
	szOldReturn.Init();
	szOldName.Init();
	aszOverloads.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESignature::Kill(void)
{
	size	i;

	aszOverloads.Kill();
	szOldName.Kill();
	szOldReturn.Kill();
	aszOldDefaults.Kill();
	aszOldNames.Kill();
	aszOldTypes.Kill();
	KillIDELocations(&asOtherUses);
	for (i = 0; i < apsCalls.NumElements(); i++)
	{
		apsCalls.GetPtr(i)->Kill();
		free(apsCalls.GetPtr(i));
	}
	apsCalls.Kill();
	for (i = 0; i < apsSites.NumElements(); i++)
	{
		apsSites.GetPtr(i)->Kill();
		free(apsSites.GetPtr(i));
	}
	apsSites.Kill();
	szFamily.Kill();
	szWhyNot.Kill();
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The text of a range of szText, appended.
//
//////////////////////////////////////////////////////////////////////////
void AppendIDERangeText(const char* szText, SCPPSyntaxRange* psRange, CChars* pszDest)
{
	size	uiStart;
	size	uiEnd;

	uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
	uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
	if (uiEnd > uiStart)
	{
		pszDest->AppendSubString(szText, uiStart, uiEnd);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A parameter's type for comparing with another however each is written:
// without white space, and with a const that starts it after what it is
// of, as the index's keys have it ("const char*" and "char const *" are
// both "charconst*").
//
//////////////////////////////////////////////////////////////////////////
void NormaliseIDEParameterType(const char* szType, CChars* pszDest)
{
	CChars	szPacked;
	size	i;
	size	uiEnd;

	szPacked.Init();
	for (i = 0; szType[i] != '\0'; i++)
	{
		if ((szType[i] != ' ') && (szType[i] != '\t') && (szType[i] != '\r') && (szType[i] != '\n'))
		{
			szPacked.Append(szType[i]);
		}
	}
	if ((strncmp(szPacked.Text(), "const", 5) == 0) && (szPacked.Length() > 5) && !IsIDENameChar(szPacked.Text()[5]))
	{
		//Nothing before it to be const: "const" alone starts no type.
		pszDest->Append(szPacked);
	}
	else if (strncmp(szPacked.Text(), "const", 5) == 0)
	{
		for (uiEnd = 5; (uiEnd < szPacked.Length()) && (szPacked.Text()[uiEnd] != '*') && (szPacked.Text()[uiEnd] != '&'); uiEnd++)
		{
		}
		pszDest->AppendSubString(szPacked.Text(), 5, uiEnd);
		pszDest->Append("const");
		pszDest->Append(&szPacked.Text()[uiEnd]);
	}
	else
	{
		pszDest->Append(szPacked);
	}
	szPacked.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mapcUnits.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpSignatureDialog);
	M_Pointer(mpDialog);
	U_Unknown(CArrayIDESignatureUnits, mapcUnits);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::Free(void)
{
	KillUnits();
	mapcUnits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::Save(CObjectWriter* pcFile) { return false; }
bool CIDEChangeSignature::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Change Signature (Ctrl+F6, as IntelliJ IDEA's), while a symbol is named
// at the caret.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.ChangeSignature", "Change Signature...", "Control+F6", Ptr<CImageCel>(), this, &CIDEChangeSignature::QueueChangeSignature)->SetEnabledCheck(this, &CIDEChangeSignature::CanChangeSignature);
}


//////////////////////////////////////////////////////////////////////////
//
// The editors' context menu's Refactor gets Change Signature.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ChangeSignature"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::CanChangeSignature(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog is shown at the next tick: ShowModal can't be called from
// the input that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::QueueChangeSignature(void)
{
	mpcContent->GetWindow()->Post(this, &CIDEChangeSignature::ShowQueuedChangeSignature);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::ShowQueuedChangeSignature(void)
{
	ShowChangeSignature();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a change changes nothing: every parameter kept as it is, in
// its place, and no new name or return type.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::IsUnchanged(SIDESignature* psSignature, SIDESignatureChange* psChange)
{
	SIDENewParameter*	psNew;
	size				i;

	if (!psChange->szName.Empty() || !psChange->szReturn.Empty() || (psChange->asParameters.NumElements() != psSignature->aszOldTypes.NumElements()))
	{
		return false;
	}
	for (i = 0; i < psChange->asParameters.NumElements(); i++)
	{
		psNew = psChange->asParameters.Get(i);
		if ((psNew->iOld != (int32)i) || !psNew->szType.Empty() || !psNew->szName.Empty() || psNew->bDefaultChanged)
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// As IntelliJ's Problems Detected: what stands in the way, a line each
// (the first few, and how many more), with Continue and Cancel.  True to
// go on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::ShowConflicts(CArrayChars* paszConflicts)
{
	Ptr<CLabel>	pLabel;
	CChars		szMore;
	size		i;
	bool		bContinue;

	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Problems Detected");
	for (i = 0; (i < paszConflicts->NumElements()) && (i < IDE_SIGNATURE_CONFLICTS_SHOWN); i++)
	{
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetTheme()->GetFont(), paszConflicts->GetText(i));
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpDialog->AddEditor("", pLabel);
	}
	if (paszConflicts->NumElements() > IDE_SIGNATURE_CONFLICTS_SHOWN)
	{
		szMore.Init("... and ");
		szMore.Append((uint32)(paszConflicts->NumElements() - IDE_SIGNATURE_CONFLICTS_SHOWN));
		szMore.Append(" more.");
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), szMore.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpDialog->AddEditor("", pLabel);
		szMore.Kill();
	}
	pLabel = NULL;
	mpDialog->SetOKCaption("Continue");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bContinue = mpDialog->Show(760);
	mpDialog = NULL;
	return bContinue;
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Change Signature for the function at the caret: its dialog,
// then, if anything stands in the way of what was asked for, Problems
// Detected, and then the change.  False if nothing was changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::ShowChangeSignature(void)
{
	SIDESignature		sSignature;
	SIDESignatureChange	sChange;
	CArrayChars			aszConflicts;
	CChars				szMessage;
	size				i;
	bool				bResult;

	sSignature.Init();
	if (!FindSignature(&sSignature))
	{
		mpcContent->ShowStatus("There is no function at the caret to change the signature of.");
		sSignature.Kill();
		return false;
	}
	if (!sSignature.bChangeable)
	{
		szMessage.Init("Can't change ");
		szMessage.Append(sSignature.szName);
		szMessage.Append("'s signature: ");
		szMessage.Append(sSignature.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sSignature.Kill();
		return false;
	}

	bResult = false;
	mpSignatureDialog = OMalloc<CIDEChangeSignatureDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), this, &sSignature);
	if (getenv("HCP65_IDE_SIGNATURE_EDITS") != NULL)
	{
		mpSignatureDialog->ApplyEdits(getenv("HCP65_IDE_SIGNATURE_EDITS"));
	}
	mpcContent->GetScript()->WatchDialog(mpSignatureDialog->GetDialog());
	if (mpSignatureDialog->Show())
	{
		sChange.Init();
		mpSignatureDialog->GetChange(&sChange);
		mpSignatureDialog = NULL;
		if (IsUnchanged(&sSignature, &sChange))
		{
			mpcContent->ShowStatus("The signature is as it was: nothing was changed.");
		}
		else
		{
			aszConflicts.Init();
			FindConflicts(&sSignature, &sChange, &aszConflicts);
			for (i = 0; i < aszConflicts.NumElements(); i++)
			{
				mpcContent->GetScript()->PrintConflict(aszConflicts.GetText(i));
			}
			if ((aszConflicts.NumElements() == 0) || ShowConflicts(&aszConflicts))
			{
				bResult = Change(&sSignature, &sChange);
			}
			aszConflicts.Kill();
		}
		sChange.Kill();
	}
	mpSignatureDialog = NULL;
	sSignature.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file parsed (its unit, for a header), once for the signature being
// found.  NULL if it can't be.
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDEChangeSignature::GetUnit(const char* szFileName)
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
void CIDEChangeSignature::KillUnits(void)
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
// A declaration or definition of a function named at psLocation: where
// its parameters are, and each of them.  False, with why, if it can't be
// changed there: a macro made it, or what declares the name isn't a
// function's declarator.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::AddSite(SIDESignature* psSignature, SIDELocation* psLocation, const char* szKey, const char** pszWhyNot)
{
	CIDERefactorUnit*	pcUnit;
	SIDESignatureSite*	psSite;

	pcUnit = GetUnit(psLocation->szFileName.Text());
	if (!pcUnit)
	{
		*pszWhyNot = "a file it is declared in couldn't be parsed";
		return false;
	}
	psSite = (SIDESignatureSite*)malloc(sizeof(SIDESignatureSite));
	psSite->Init();
	if (!ReadSite(pcUnit, psLocation, szKey, psSite, pszWhyNot))
	{
		psSite->Kill();
		free(psSite);
		return false;
	}
	psSignature->apsSites.Add(psSite);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A declaration or definition of a function named at psLocation in the
// file pcUnit has parsed, read into psSite: where its parameters are and
// each of them, what it returns, and the rest of what it writes (its
// name, what follows its parameters, its body, its template<...>, and in
// its class what its name is written after outside it).  False, with
// why, if a macro made it or what declares the name isn't a function's
// declarator.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::ReadSite(CIDERefactorUnit* pcUnit, SIDELocation* psLocation, const char* szKey, SIDESignatureSite* psSite, const char** pszWhyNot)
{
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcOuter;
	CCPPSyntax*					pcParent;
	CCPPSyntaxInitDeclarator*	pcInit;
	CCPPSyntaxClassSpecifier*	pcClass;
	CCPPSyntaxTemplateDeclaration*	pcTemplate;
	CCPPSyntaxFunctionDefinition*	pcDefinition;
	CChars						szPart;
	CChars						szArguments;
	bool						bClassFound;
	CCPPSyntaxDeclarator*		pcDeclarator;
	CCPPSyntaxFunctionSuffix*	pcSuffix;
	CCPPSyntaxParameter*		pcParameter;
	CCPPSyntaxName*				pcName;
	CCPPSyntaxDeclSpecifiers*	pcSpecifiers;
	SIDEWrittenParameter*		psWritten;
	bool						bStorage;
	SCPPSyntaxRange				sOpen;
	SCPPSyntaxRange				sClose;
	SCPPSyntaxRange				sRange;
	int32						iToken;
	int32						iClose;
	int32						iDepth;
	int32						iEquals;
	size						i;
	uint8						uiKind;

	iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
	pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
	pcDeclarator = (CCPPSyntaxDeclarator*)pcUnit->FindAncestor(pcNode, CPPS_Declarator);
	while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
	{
		pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
	}
	pcSuffix = pcDeclarator ? pcDeclarator->GetFunctionSuffix() : NULL;
	if (!pcSuffix)
	{
		*pszWhyNot = "a declaration of it is made by a macro, is left out by the configuration, or isn't a function's";
		return false;
	}

	//Its parentheses: the suffix's first token, and the one that closes it.
	iClose = -1;
	iDepth = 0;
	for (iToken = pcSuffix->miFirstToken; iToken <= pcSuffix->miLastToken; iToken++)
	{
		if (pcUnit->IsToken(iToken, "("))
		{
			iDepth++;
		}
		else if (pcUnit->IsToken(iToken, ")"))
		{
			iDepth--;
			if (iDepth == 0)
			{
				iClose = iToken;
				break;
			}
		}
	}
	if (!pcUnit->IsToken(pcSuffix->miFirstToken, "(") || (iClose < 0) || !pcUnit->GetTokensRange(pcSuffix->miFirstToken, pcSuffix->miFirstToken, &sOpen) || !pcUnit->GetTokensRange(iClose, iClose, &sClose))
	{
		*pszWhyNot = "a declaration of it has its parameters made by a macro";
		return false;
	}

	psSite->szFileName.Set(psLocation->szFileName);
	psSite->szKey.Set(szKey);
	psSite->iLine = psLocation->iLine;
	psSite->iColumn = psLocation->iColumn;
	psSite->bDefinition = (psLocation->uiRole == CPPIR_Definition);
	psSite->sParameters.iLine = sOpen.iEndLine;
	psSite->sParameters.iColumn = sOpen.iEndColumn;
	psSite->sParameters.iEndLine = sClose.iLine;
	psSite->sParameters.iEndColumn = sClose.iColumn;

	//What follows its parameters: const and the like, which a definition outside its class writes too, and override
	//and final, which it doesn't.
	psSite->sAfter.iLine = sClose.iEndLine;
	psSite->sAfter.iColumn = sClose.iEndColumn;
	psSite->sAfter.iEndLine = sClose.iEndLine;
	psSite->sAfter.iEndColumn = sClose.iEndColumn;
	if ((pcSuffix->miLastToken > iClose) && pcUnit->GetTokensRange(iClose + 1, pcSuffix->miLastToken, &sRange))
	{
		psSite->sAfter.iEndLine = sRange.iEndLine;
		psSite->sAfter.iEndColumn = sRange.iEndColumn;
	}
	for (iToken = iClose + 1; iToken <= pcSuffix->miLastToken; iToken++)
	{
		if (!pcUnit->IsToken(iToken, "override") && !pcUnit->IsToken(iToken, "final"))
		{
			psSite->szQualifiers.Append(' ');
			pcUnit->AppendTokens(iToken, iToken, &psSite->szQualifiers);
		}
	}

	//All of it, its body, its own template<...>, and whether it is in its class.
	pcOuter = pcDeclarator->mpcParent;
	pcInit = NULL;
	if (pcOuter && (pcOuter->GetKind() == CPPS_InitDeclarator))
	{
		pcInit = (CCPPSyntaxInitDeclarator*)pcOuter;
		pcOuter = pcOuter->mpcParent;
	}
	psSite->bPure = pcInit && (pcInit->meInitializer == CPPI_Equals);
	if (pcOuter && (pcOuter->GetKind() == CPPS_FunctionDefinition))
	{
		pcDefinition = (CCPPSyntaxFunctionDefinition*)pcOuter;
		psSite->bBody = (pcDefinition->meBody == CPPFB_Body) && pcDefinition->mpcBody && pcUnit->GetRange(pcDefinition->mpcBody, &psSite->sBody);
	}
	pcClass = pcOuter ? (CCPPSyntaxClassSpecifier*)pcUnit->FindAncestor(pcOuter, CPPS_ClassSpecifier) : NULL;
	psSite->bInClass = (pcClass != NULL);
	while (pcOuter && pcOuter->mpcParent && (pcOuter->mpcParent->GetKind() == CPPS_TemplateDeclaration))
	{
		pcTemplate = (CCPPSyntaxTemplateDeclaration*)pcOuter->mpcParent;
		szPart.Init();
		pcUnit->AppendTokens(pcTemplate->miFirstToken, pcOuter->miFirstToken - 1, &szPart);
		szPart.Append(psSite->szOwnTemplate.Empty() ? "" : "\n");
		psSite->szOwnTemplate.Insert(0, &szPart);
		szPart.Kill();
		pcOuter = pcTemplate;
	}
	if (!pcOuter || !pcUnit->GetRange(pcOuter, &psSite->sAll))
	{
		*pszWhyNot = "a declaration of it is partly made by a macro";
		return false;
	}

	//In its class: the classes and namespaces it is in, each a class template's with its parameters' names, and
	//the template<...> of each.
	bClassFound = false;
	for (pcParent = pcClass; pcParent; pcParent = pcParent->mpcParent)
	{
		szPart.Init();
		if ((pcParent->GetKind() == CPPS_ClassSpecifier) && ((CCPPSyntaxClassSpecifier*)pcParent)->mpcName)
		{
			pcUnit->AppendTokens(((CCPPSyntaxClassSpecifier*)pcParent)->mpcName->miFirstToken, ((CCPPSyntaxClassSpecifier*)pcParent)->mpcName->miLastToken, &szPart);
			bClassFound = true;
		}
		else if ((pcParent->GetKind() == CPPS_Namespace) && (((CCPPSyntaxNamespace*)pcParent)->miName >= 0))
		{
			pcUnit->AppendTokens(((CCPPSyntaxNamespace*)pcParent)->miName, ((CCPPSyntaxNamespace*)pcParent)->miName, &szPart);
		}
		else if ((pcParent->GetKind() == CPPS_TemplateDeclaration) && bClassFound)
		{
			//The class just named is a template: its parameters' names after it, and its template<...>.
			pcTemplate = (CCPPSyntaxTemplateDeclaration*)pcParent;
			szArguments.Init("<");
			for (i = 0; i < pcTemplate->macpParameters.NumElements(); i++)
			{
				szArguments.Append((i > 0) ? ", " : "");
				pcNode = pcTemplate->macpParameters.GetPtr(i);
				if (pcNode->GetKind() == CPPS_TypeParameter)
				{
					pcUnit->AppendTokens(((CCPPSyntaxTypeParameter*)pcNode)->miName, ((CCPPSyntaxTypeParameter*)pcNode)->miName, &szArguments);
				}
				else if ((pcNode->GetKind() == CPPS_Parameter) && ((CCPPSyntaxParameter*)pcNode)->mpcDeclarator && ((CCPPSyntaxParameter*)pcNode)->mpcDeclarator->GetName())
				{
					pcName = ((CCPPSyntaxParameter*)pcNode)->mpcDeclarator->GetName();
					pcUnit->AppendTokens(pcName->miFirstToken, pcName->miLastToken, &szArguments);
				}
			}
			szArguments.Append(">");
			//After the class's name, which is the qualifier's first part so far.
			iToken = (int32)psSite->szClassQualifier.Find("::");
			psSite->szClassQualifier.Insert((size)iToken, &szArguments);
			szArguments.Kill();
			if (pcTemplate->mpcDeclaration)
			{
				szArguments.Init();
				pcUnit->AppendTokens(pcTemplate->miFirstToken, pcTemplate->mpcDeclaration->miFirstToken - 1, &szArguments);
				szArguments.Append(psSite->szClassTemplate.Empty() ? "" : "\n");
				psSite->szClassTemplate.Insert(0, &szArguments);
				szArguments.Kill();
			}
			bClassFound = false;
		}
		if (!szPart.Empty())
		{
			szPart.Append("::");
			psSite->szClassQualifier.Insert(0, &szPart);
		}
		szPart.Kill();
	}

	//What it returns: from the first of its specifiers that is of its type (not virtual, static and the like, which
	//come first) to its name.
	pcSpecifiers = NULL;
	pcNode = pcDeclarator->mpcParent;
	if (pcNode && (pcNode->GetKind() == CPPS_InitDeclarator))
	{
		pcNode = pcNode->mpcParent;
	}
	if (pcNode && (pcNode->GetKind() == CPPS_SimpleDeclaration))
	{
		pcSpecifiers = ((CCPPSyntaxSimpleDeclaration*)pcNode)->mpcSpecifiers;
	}
	else if (pcNode && (pcNode->GetKind() == CPPS_FunctionDefinition))
	{
		pcSpecifiers = ((CCPPSyntaxFunctionDefinition*)pcNode)->mpcSpecifiers;
	}
	pcName = pcDeclarator->GetName();
	if (pcName && pcName->GetLast())
	{
		pcUnit->GetRange(pcName, &psSite->sName);
		pcUnit->AppendTokens(pcName->GetLast()->miFirstToken, pcName->GetLast()->miLastToken, &psSite->szOwnName);
	}
	if (pcSpecifiers && (pcSpecifiers->miFirstToken >= 0) && pcName)
	{
		for (iToken = pcSpecifiers->miFirstToken; iToken <= pcSpecifiers->miLastToken; iToken++)
		{
			bStorage = false;
			for (i = 0; i < sizeof(gaszIDENotReturned) / sizeof(char*); i++)
			{
				bStorage |= pcUnit->IsToken(iToken, gaszIDENotReturned[i]);
			}
			if (!bStorage)
			{
				break;
			}
		}
		if ((iToken <= pcSpecifiers->miLastToken) && pcUnit->GetTokensRange(iToken, iToken, &sOpen) && pcUnit->GetRange(pcName, &sClose))
		{
			psSite->sReturn.iLine = sOpen.iLine;
			psSite->sReturn.iColumn = sOpen.iColumn;
			psSite->sReturn.iEndLine = sClose.iLine;
			psSite->sReturn.iEndColumn = sClose.iColumn;
			psSite->bReturn = true;
		}
	}

	for (i = 0; i < pcSuffix->macpParameters.NumElements(); i++)
	{
		pcParameter = (CCPPSyntaxParameter*)pcSuffix->macpParameters.GetPtr(i);
		if ((pcSuffix->macpParameters.NumElements() == 1) && (pcParameter->miFirstToken == pcParameter->miLastToken) && pcUnit->IsToken(pcParameter->miFirstToken, "void"))
		{
			//(void): no parameters.
			break;
		}
		if ((pcParameter->GetKind() != CPPS_Parameter) || !pcUnit->GetRange(pcParameter, &sRange))
		{
			*pszWhyNot = "a declaration of it has a parameter made by a macro";
			return false;
		}
		psWritten = psSite->asParameters.Add();
		psWritten->sAll = sRange;
		psWritten->szKey.Init();
		psWritten->bName = false;
		psWritten->bDefault = false;

		psWritten->sDefault = sRange;
		psWritten->sDefault.iLine = sRange.iEndLine;
		psWritten->sDefault.iColumn = sRange.iEndColumn;
		if (pcParameter->mpcDefault && pcUnit->GetRange(pcParameter->mpcDefault, &sRange))
		{
			//From its =, the token before it.
			iEquals = pcParameter->mpcDefault->miFirstToken - 1;
			psWritten->sDefault = sRange;
			if (sRange.iEndLine > psWritten->sAll.iEndLine || ((sRange.iEndLine == psWritten->sAll.iEndLine) && (sRange.iEndColumn > psWritten->sAll.iEndColumn)))
			{
				psWritten->sAll.iEndLine = sRange.iEndLine;
				psWritten->sAll.iEndColumn = sRange.iEndColumn;
			}
			if (pcUnit->IsToken(iEquals, "=") && pcUnit->GetTokensRange(iEquals, iEquals, &sRange))
			{
				psWritten->sDefault.iLine = sRange.iLine;
				psWritten->sDefault.iColumn = sRange.iColumn;
			}
			psWritten->bDefault = true;
		}

		//Its name, or where one would go: before its default value.
		psWritten->sName.iLine = psWritten->sDefault.iLine;
		psWritten->sName.iColumn = psWritten->sDefault.iColumn;
		psWritten->sName.iEndLine = psWritten->sDefault.iLine;
		psWritten->sName.iEndColumn = psWritten->sDefault.iColumn;
		pcName = pcParameter->mpcDeclarator ? pcParameter->mpcDeclarator->GetName() : NULL;
		if (pcName && pcUnit->GetRange(pcName, &sRange))
		{
			psWritten->sName = sRange;
			psWritten->bName = true;
			mpcContent->GetCodeModel()->GetSymbolAt(psLocation->szFileName.Text(), sRange.iLine, sRange.iColumn, &psWritten->szKey, &uiKind);
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A call of a function whose name is written at psLocation: where its
// arguments are, each of them, and whether each does something.  False if
// the name isn't what a call calls there (its address is taken, it is
// named for a pointer to it) or a macro makes the call.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::AddCall(SIDESignature* psSignature, SIDELocation* psLocation)
{
	CIDERefactorUnit*			pcUnit;
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcCallee;
	CCPPSyntaxName*				pcName;
	CCPPSyntaxCallExpression*	pcCall;
	SIDESignatureCall*			psCall;
	CIDEEffectFinder			cEffects;
	SCPPSyntaxRange				sOpen;
	SCPPSyntaxRange				sClose;
	SCPPSyntaxRange				sRange;
	int32						iToken;
	size						i;

	pcUnit = GetUnit(psLocation->szFileName.Text());
	if (!pcUnit)
	{
		return false;
	}
	iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
	pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
	while (pcNode && (pcNode->GetKind() != CPPS_IdExpression) && (pcNode->GetKind() != CPPS_MemberExpression))
	{
		pcNode = pcNode->mpcParent;
	}
	if (!pcNode)
	{
		return false;
	}
	pcName = (pcNode->GetKind() == CPPS_IdExpression) ? ((CCPPSyntaxIdExpression*)pcNode)->mpcName : ((CCPPSyntaxMemberExpression*)pcNode)->mpcName;
	if (!pcName || !pcName->GetLast() || (pcName->GetLast()->miName != iToken))
	{
		return false;
	}
	pcCallee = pcNode;
	while (pcCallee->mpcParent && (pcCallee->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
	{
		pcCallee = pcCallee->mpcParent;
	}
	pcCall = (CCPPSyntaxCallExpression*)pcCallee->mpcParent;
	if (!pcCall || (pcCall->GetKind() != CPPS_CallExpression) || (pcCall->mpcCallee != pcCallee))
	{
		return false;
	}
	if (!pcUnit->IsToken(pcCallee->miLastToken + 1, "(") || !pcUnit->IsToken(pcCall->miLastToken, ")") ||
		!pcUnit->GetTokensRange(pcCallee->miLastToken + 1, pcCallee->miLastToken + 1, &sOpen) || !pcUnit->GetTokensRange(pcCall->miLastToken, pcCall->miLastToken, &sClose))
	{
		return false;
	}

	psCall = (SIDESignatureCall*)malloc(sizeof(SIDESignatureCall));
	psCall->Init();
	psCall->szFileName.Set(psLocation->szFileName);
	psCall->iLine = psLocation->iLine;
	psCall->iColumn = psLocation->iColumn;
	psCall->sArguments.iLine = sOpen.iEndLine;
	psCall->sArguments.iColumn = sOpen.iEndColumn;
	psCall->sArguments.iEndLine = sClose.iLine;
	psCall->sArguments.iEndColumn = sClose.iColumn;
	for (i = 0; i < pcCall->macpArguments.NumElements(); i++)
	{
		if (!pcUnit->GetRange(pcCall->macpArguments.GetPtr(i), &sRange))
		{
			psCall->Kill();
			free(psCall);
			return false;
		}
		psCall->asArguments.Add(&sRange);
		cEffects.mbEffects = false;
		cEffects.Look(pcCall->macpArguments.GetPtr(i));
		psCall->aiEffects.Add(cEffects.mbEffects ? 1 : 0);
	}
	pcNode = pcCall;
	while (pcNode->mpcParent && (pcNode->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
	{
		pcNode = pcNode->mpcParent;
	}
	psCall->bValueUsed = !pcNode->mpcParent || (pcNode->mpcParent->GetKind() != CPPS_ExpressionStatement);
	psSignature->apsCalls.Add(psCall);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The parameters the first declaration has, as they are shown: each
// one's type (what it writes before its name and after it, an array's
// size), its name and its default value.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::FindOldParameters(SIDESignature* psSignature)
{
	SIDESignatureSite*		psSite;
	SIDEWrittenParameter*	psWritten;
	SCPPSyntaxRange			sRange;
	CTextFile				cFile;
	CChars					szPart;
	char*					szText;
	size					uiLength;
	size					i;

	psSite = psSignature->apsSites.GetPtr(0);
	GetIDESymbolName(psSignature->szKey.Text(), &psSignature->szOldName);
	cFile.Init();
	if (mpcContent->GetFindInFiles()->GetFileText(psSite->szFileName.Text(), &cFile, &szText, &uiLength))
	{
		if (psSite->bReturn)
		{
			AppendIDERangeText(szText, &psSite->sReturn, &psSignature->szOldReturn);
			psSignature->szOldReturn.StripWhitespace();
		}
		for (i = 0; i < psSite->asParameters.NumElements(); i++)
		{
			psWritten = psSite->asParameters.Get(i);
			szPart.Init();
			sRange = psWritten->sAll;
			sRange.iEndLine = psWritten->sName.iLine;
			sRange.iEndColumn = psWritten->sName.iColumn;
			AppendIDERangeText(szText, &sRange, &szPart);
			szPart.StripWhitespace();
			sRange = psWritten->sAll;
			sRange.iLine = psWritten->sName.iEndLine;
			sRange.iColumn = psWritten->sName.iEndColumn;
			sRange.iEndLine = psWritten->sDefault.iLine;
			sRange.iEndColumn = psWritten->sDefault.iColumn;
			AppendIDERangeText(szText, &sRange, &szPart);
			szPart.StripWhitespace();
			psSignature->aszOldTypes.Add(szPart.Text());
			szPart.Kill();

			szPart.Init();
			AppendIDERangeText(szText, &psWritten->sName, &szPart);
			psSignature->aszOldNames.Add(szPart.Text());
			szPart.Kill();

			szPart.Init();
			if (psWritten->bDefault)
			{
				AppendIDERangeText(szText, &psWritten->sDefault, &szPart);
				if (!szPart.Empty() && (szPart.Text()[0] == '='))
				{
					szPart.RemoveFromStart(1);
				}
				szPart.StripWhitespace();
			}
			psSignature->aszOldDefaults.Add(szPart.Text());
			szPart.Kill();
		}
	}
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Change Signature would change for the function at the caret,
// whether the caret is on a declaration, a definition or a call of it:
// its declarations and definitions, those of what is changed with it
// (CIDECodeModel::FindRenameTarget: its overrides and its family), their
// calls, and where they are named without being called.  False if no
// symbol is named at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::FindSignature(SIDESignature* psDest)
{
	SIDERenameTarget	sTarget;
	SIDELocation*		psLocation;
	SIDESignatureSite*	psSite;
	SIDESignatureCall*	psCall;
	CChars				szKey;
	const char*			szWhyNot;
	int					iLine;
	int					iColumn;
	uint8				uiRole;
	uint8				uiKind;
	size				i;
	size				j;
	bool				bFound;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) ||
		!mpcContent->GetCodeModel()->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &psDest->szKey, &uiKind))
	{
		return false;
	}
	GetIDEScopedName(psDest->szKey.Text(), &psDest->szName);
	if ((uiKind != CPPSK_Function) && (uiKind != CPPSK_FunctionTemplate))
	{
		psDest->szWhyNot.Set("it isn't a function");
		return true;
	}

	sTarget.Init();
	if (!mpcContent->GetCodeModel()->FindRenameTarget(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &sTarget))
	{
		sTarget.Kill();
		return false;
	}
	if (!sTarget.bRenamable)
	{
		psDest->szWhyNot.Set(sTarget.szWhyNot);
		sTarget.Kill();
		return true;
	}
	psDest->uiOverrides = sTarget.uiOverrides;
	psDest->uiFamily = sTarget.uiFamily;
	psDest->szFamily.Set(sTarget.szFamily);
	for (i = 0; i < sTarget.aszOverloads.NumElements(); i++)
	{
		psDest->aszOverloads.Add(sTarget.aszOverloads.GetText(i));
	}

	szWhyNot = NULL;
	szKey.Init();
	for (i = 0; (i < sTarget.asLocations.NumElements()) && !szWhyNot; i++)
	{
		psLocation = sTarget.asLocations.Get(i);
		if (psLocation->uiRole != CPPIR_Reference)
		{
			//The same declaration can be a place of two symbols only as a reference.
			szKey.Clear();
			mpcContent->GetCodeModel()->GetSymbolAt(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, &szKey, &uiKind);
			AddSite(psDest, psLocation, szKey.Text(), &szWhyNot);
			continue;
		}

		//A template's call is a place of each of its family.
		bFound = false;
		for (j = 0; (j < psDest->apsCalls.NumElements()) && !bFound; j++)
		{
			psCall = psDest->apsCalls.GetPtr(j);
			bFound = (psCall->iLine == psLocation->iLine) && (psCall->iColumn == psLocation->iColumn) && psCall->szFileName.Equals(psLocation->szFileName);
		}
		for (j = 0; (j < psDest->asOtherUses.NumElements()) && !bFound; j++)
		{
			bFound = (psDest->asOtherUses.Get(j)->iLine == psLocation->iLine) && (psDest->asOtherUses.Get(j)->iColumn == psLocation->iColumn) && psDest->asOtherUses.Get(j)->szFileName.Equals(psLocation->szFileName);
		}
		if (!bFound && !AddCall(psDest, psLocation))
		{
			MoveIDELocation(psDest->asOtherUses.Add(), psLocation);
		}
	}
	szKey.Kill();
	sTarget.Kill();
	KillUnits();

	if (!szWhyNot && (psDest->apsSites.NumElements() == 0))
	{
		szWhyNot = "it has no declaration in the solution";
	}
	if (szWhyNot)
	{
		psDest->szWhyNot.Set(szWhyNot);
		return true;
	}

	//The function's own declaration first, before its definition: what the dialog shows, and where a new default goes.
	for (i = 0; i < psDest->apsSites.NumElements(); i++)
	{
		psSite = psDest->apsSites.GetPtr(i);
		if (psSite->szKey.Equals(psDest->szKey) && !psSite->bDefinition)
		{
			break;
		}
	}
	if (i == psDest->apsSites.NumElements())
	{
		for (i = 0; i < psDest->apsSites.NumElements(); i++)
		{
			if (psDest->apsSites.GetPtr(i)->szKey.Equals(psDest->szKey))
			{
				break;
			}
		}
	}
	if ((i > 0) && (i < psDest->apsSites.NumElements()))
	{
		psSite = psDest->apsSites.GetPtr(i);
		psDest->apsSites.RemoveAt(i);
		psDest->apsSites.InsertAt(&psSite, 0);
	}
	//Where each symbol is first declared: its declaration, or its definition if it has none.
	for (i = 0; i < psDest->apsSites.NumElements(); i++)
	{
		psSite = psDest->apsSites.GetPtr(i);
		bFound = false;
		for (j = 0; j < psDest->apsSites.NumElements(); j++)
		{
			if ((j != i) && psDest->apsSites.GetPtr(j)->szKey.Equals(psSite->szKey) && (psDest->apsSites.GetPtr(j)->bFirst || (psSite->bDefinition && !psDest->apsSites.GetPtr(j)->bDefinition)))
			{
				bFound = true;
			}
		}
		psSite->bFirst = !bFound;
	}
	FindOldParameters(psDest);
	psDest->bChangeable = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// "File.cpp:12", appended.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::AppendPlace(const char* szFileName, int32 iLine, CChars* pszDest)
{
	pszDest->Append(GetIDEFileNamePart(szFileName))->Append(':')->Append(iLine + 1);
}


//////////////////////////////////////////////////////////////////////////
//
// What a signature found, for checking.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEChangeSignature::PrintSignature(SIDESignature* psSignature, CChars* pszDest)
{
	SIDESignatureSite*	psSite;
	SIDESignatureCall*	psCall;
	size				i;

	pszDest->Append("Change Signature: ")->Append(psSignature->szName)->Append(" [")->Append(psSignature->szKey)->Append("]");
	if (!psSignature->bChangeable)
	{
		pszDest->Append(" can't be changed: ")->Append(psSignature->szWhyNot)->AppendNewLine();
		return pszDest->Text();
	}
	pszDest->Append(' ')->Append(psSignature->szOldReturn)->Append(" (");
	for (i = 0; i < psSignature->aszOldTypes.NumElements(); i++)
	{
		pszDest->Append((i > 0) ? ", " : "")->Append(psSignature->aszOldTypes.GetText(i));
		if (!psSignature->aszOldNames.Get(i)->Empty())
		{
			pszDest->Append(' ')->Append(psSignature->aszOldNames.GetText(i));
		}
		if (!psSignature->aszOldDefaults.Get(i)->Empty())
		{
			pszDest->Append(" = ")->Append(psSignature->aszOldDefaults.GetText(i));
		}
	}
	pszDest->Append("): ")->Append((uint32)psSignature->apsSites.NumElements())->Append(" declarations and definitions, ");
	pszDest->Append((uint32)psSignature->uiOverrides)->Append(" overrides, ")->Append((uint32)psSignature->uiFamily)->Append(" family, ");
	pszDest->Append((uint32)psSignature->apsCalls.NumElements())->Append(" calls, ")->Append((uint32)psSignature->asOtherUses.NumElements())->Append(" other uses")->AppendNewLine();
	for (i = 0; i < psSignature->apsSites.NumElements(); i++)
	{
		psSite = psSignature->apsSites.GetPtr(i);
		pszDest->Append(psSite->bDefinition ? "  defined " : "  declared ");
		AppendPlace(psSite->szFileName.Text(), psSite->iLine, pszDest);
		pszDest->Append(" [")->Append(psSite->szKey)->Append("] ")->Append((uint32)psSite->asParameters.NumElements())->Append(" parameters")->AppendNewLine();
	}
	for (i = 0; i < psSignature->apsCalls.NumElements(); i++)
	{
		psCall = psSignature->apsCalls.GetPtr(i);
		pszDest->Append("  called ");
		AppendPlace(psCall->szFileName.Text(), psCall->iLine, pszDest);
		pszDest->Append(" with ")->Append((uint32)psCall->asArguments.NumElements())->Append(" arguments")->AppendNewLine();
	}
	for (i = 0; i < psSignature->asOtherUses.NumElements(); i++)
	{
		pszDest->Append("  named ");
		AppendPlace(psSignature->asOtherUses.Get(i)->szFileName.Text(), psSignature->asOtherUses.Get(i)->iLine, pszDest);
		pszDest->AppendNewLine();
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// A change to the signature from a written list, for scripting: items
// separated by ; each
//   - an old parameter kept, by its number from 0: "1"; kept and
//     renamed, "1=iCount"; or kept with a new name, type and default
//     value, "1|name|type|default", where an empty name or type keeps
//     it and the default value is kept unless that part is written
//     (written empty, it has none);
//   - a new parameter, "+type|name|call value|default value" (the last
//     two may be empty);
//   - the function's new name, "name=Start", or what it is to return,
//     "return=int".
// False, with why, if it isn't such a list or can't be the signature
// (FindProblem).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::ParseChange(SIDESignature* psSignature, const char* szSpec, SIDESignatureChange* psDest, CChars* pszProblem)
{
	CChars				szAll;
	CArrayChars			aszItems;
	CArrayChars			aszParts;
	SIDENewParameter*	psNew;
	CChars*				pszItem;
	const char*			szEquals;
	size				i;
	size				j;
	int					iOld;
	bool				bResult;

	szAll.Init(szSpec);
	aszItems.Init();
	szAll.Split(&aszItems, ';');
	bResult = true;
	for (i = 0; bResult && (i < aszItems.NumElements()); i++)
	{
		pszItem = aszItems.Get(i);
		pszItem->StripWhitespace();
		if (pszItem->Empty())
		{
			continue;
		}
		aszParts.Init();
		pszItem->Split(&aszParts, '|');
		for (j = 0; j < aszParts.NumElements(); j++)
		{
			aszParts.Get(j)->StripWhitespace();
		}
		if (pszItem->StartsWith("name="))
		{
			psDest->szName.Set(&pszItem->Text()[5]);
		}
		else if (pszItem->StartsWith("return="))
		{
			psDest->szReturn.Set(&pszItem->Text()[7]);
		}
		else if (pszItem->Text()[0] == '+')
		{
			psNew = psDest->asParameters.Add();
			psNew->Init(-1);
			psNew->szType.Set(&aszParts.GetText(0)[1]);
			psNew->szType.StripWhitespace();
			if (aszParts.NumElements() > 1)
			{
				psNew->szName.Set(aszParts.GetText(1));
			}
			if (aszParts.NumElements() > 2)
			{
				psNew->szCallValue.Set(aszParts.GetText(2));
			}
			if (aszParts.NumElements() > 3)
			{
				psNew->szDefault.Set(aszParts.GetText(3));
			}
		}
		else
		{
			iOld = atoi(pszItem->Text());
			szEquals = strchr(pszItem->Text(), '=');
			if ((pszItem->Text()[0] < '0') || (pszItem->Text()[0] > '9') || (iOld < 0) || ((size)iOld >= psSignature->aszOldTypes.NumElements()))
			{
				pszProblem->Set("The function has no such parameter: ");
				pszProblem->Append(pszItem);
				bResult = false;
			}
			else
			{
				psNew = psDest->asParameters.Add();
				psNew->Init(iOld);
				if (aszParts.NumElements() > 1)
				{
					psNew->szName.Set(aszParts.GetText(1));
					if (aszParts.NumElements() > 2)
					{
						psNew->szType.Set(aszParts.GetText(2));
					}
					if (aszParts.NumElements() > 3)
					{
						psNew->szDefault.Set(aszParts.GetText(3));
						psNew->bDefaultChanged = true;
					}
				}
				else if (szEquals)
				{
					psNew->szName.Set(szEquals + 1);
					psNew->szName.StripWhitespace();
				}
			}
		}
		aszParts.Kill();
	}
	aszItems.Kill();
	szAll.Kill();
	return bResult && !FindProblem(psSignature, psDest, pszProblem);
}


//////////////////////////////////////////////////////////////////////////
//
// Why a change can't be the function's signature, in pszProblem; false
// if it can.  A parameter given twice; a new one without a type or a
// name; a name that isn't one, or two parameters' the same; the
// function's name not a name; nothing to return written for a function
// that returns something.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::FindProblem(SIDESignature* psSignature, SIDESignatureChange* psChange, CChars* pszProblem)
{
	CArrayChars			aszNames;
	SIDENewParameter*	psNew;
	const char*			szName;
	const char*			szProblem;
	size				i;
	size				j;

	szProblem = NULL;
	aszNames.Init();
	for (i = 0; !szProblem && (i < psChange->asParameters.NumElements()); i++)
	{
		psNew = psChange->asParameters.Get(i);
		for (j = 0; (psNew->iOld >= 0) && (j < i); j++)
		{
			if (psChange->asParameters.Get(j)->iOld == psNew->iOld)
			{
				szProblem = "A parameter is given twice.";
			}
		}
		if ((psNew->iOld < 0) && (psNew->szType.Empty() || psNew->szName.Empty()))
		{
			szProblem = "A new parameter needs a type and a name.";
		}
		szName = (psNew->szName.Empty() && (psNew->iOld >= 0)) ? psSignature->aszOldNames.GetText((size)psNew->iOld) : psNew->szName.Text();
		if (!szProblem && !psNew->szName.Empty() && !IsIDEIdentifier(psNew->szName.Text()))
		{
			pszProblem->Set("A parameter's name isn't a name: ");
			pszProblem->Append(psNew->szName);
			aszNames.Kill();
			return true;
		}
		if (!szProblem && (szName[0] != '\0') && aszNames.Contains(szName))
		{
			pszProblem->Set("Two parameters are named ");
			pszProblem->Append(szName);
			pszProblem->Append(".");
			aszNames.Kill();
			return true;
		}
		aszNames.Add(szName);
	}
	aszNames.Kill();
	if (!szProblem && !psChange->szName.Empty() && !IsIDEIdentifier(psChange->szName.Text()))
	{
		szProblem = "The function's name isn't a name.";
	}
	if (!szProblem && !psChange->szReturn.Empty() && psSignature->szOldReturn.Empty())
	{
		szProblem = "It is a constructor or a destructor: it returns nothing to change.";
	}
	if (szProblem)
	{
		pszProblem->Set(szProblem);
	}
	return szProblem != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// What stands in the way of giving the function the new parameters, each
// as a line to show (see the class).  None if nothing does.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::FindConflicts(SIDESignature* psSignature, SIDESignatureChange* psChange, CArrayChars* paszDest)
{
	CArrayIDENewParameters*	pasNew;
	CIDECodeModel*			pcModel;
	CArrayIDELocations		asLocations;
	SIDESignatureSite*		psSite;
	SIDESignatureCall*		psCall;
	SIDEWrittenParameter*	psWritten;
	SIDENewParameter*		psNew;
	CChars					szLine;
	CChars					szNewTypes;
	CChars					szOther;
	CChars					szPacked;
	const char*				szParameters;
	size					i;
	size					j;
	size					k;
	size					uiUses;
	size					uiOld;
	uint8					uiKind;
	bool					bKept;
	bool					bDefault;
	bool					bAfterDefault;

	pcModel = mpcContent->GetCodeModel();
	pasNew = &psChange->asParameters;
	uiOld = psSignature->aszOldTypes.NumElements();

	//A parameter taken out that a body still uses, and its arguments that do something.
	for (i = 0; i < uiOld; i++)
	{
		bKept = false;
		for (j = 0; j < pasNew->NumElements(); j++)
		{
			bKept |= (pasNew->Get(j)->iOld == (int32)i);
		}
		if (bKept)
		{
			continue;
		}
		for (j = 0; j < psSignature->apsSites.NumElements(); j++)
		{
			psSite = psSignature->apsSites.GetPtr(j);
			psWritten = (i < psSite->asParameters.NumElements()) ? psSite->asParameters.Get(i) : NULL;
			if (!psSite->bDefinition || !psWritten || psWritten->szKey.Empty())
			{
				continue;
			}
			asLocations.Init();
			pcModel->GetSymbolLocations(psWritten->szKey.Text(), &asLocations, &uiKind);
			uiUses = 0;
			for (k = 0; k < asLocations.NumElements(); k++)
			{
				if (asLocations.Get(k)->uiRole == CPPIR_Reference)
				{
					if (uiUses == 0)
					{
						szLine.Init();
						GetIDEScopedName(psSite->szKey.Text(), &szLine);
						szLine.Append(" still uses the parameter taken out, ");
						GetIDESymbolName(psWritten->szKey.Text(), &szOther);
						szLine.Append(szOther);
						szOther.Kill();
						szLine.Append(", at ");
						AppendPlace(asLocations.Get(k)->szFileName.Text(), asLocations.Get(k)->iLine, &szLine);
					}
					uiUses++;
				}
			}
			if (uiUses > 0)
			{
				if (uiUses > 1)
				{
					szLine.Append(" and ")->Append((uint32)(uiUses - 1))->Append(uiUses == 2 ? " more place" : " more places");
				}
				szLine.Append(".");
				paszDest->Add(szLine.Text());
				szLine.Kill();
			}
			KillIDELocations(&asLocations);
		}
		for (j = 0; j < psSignature->apsCalls.NumElements(); j++)
		{
			psCall = psSignature->apsCalls.GetPtr(j);
			if ((i < psCall->aiEffects.NumElements()) && (psCall->aiEffects.GetValue(i) != 0))
			{
				szLine.Init("The argument taken out of the call at ");
				AppendPlace(psCall->szFileName.Text(), psCall->iLine, &szLine);
				szLine.Append(" does something (a call, an assignment or the like), which goes with it.");
				paszDest->Add(szLine.Text());
				szLine.Kill();
			}
		}
	}

	//Named but not called.
	for (i = 0; i < psSignature->asOtherUses.NumElements(); i++)
	{
		szLine.Init("It is named without being called at ");
		AppendPlace(psSignature->asOtherUses.Get(i)->szFileName.Text(), psSignature->asOtherUses.Get(i)->iLine, &szLine);
		szLine.Append(" (its address, or a pointer to it): what it is used as there isn't changed.");
		paszDest->Add(szLine.Text());
		szLine.Kill();
	}

	//A default value before a parameter without one; a new parameter the calls have no value for.
	bAfterDefault = false;
	for (j = 0; j < pasNew->NumElements(); j++)
	{
		psNew = pasNew->Get(j);
		bDefault = ((psNew->iOld >= 0) && !psNew->bDefaultChanged) ? !psSignature->aszOldDefaults.Get((size)psNew->iOld)->Empty() : !psNew->szDefault.Empty();
		if (bAfterDefault && !bDefault)
		{
			szLine.Init("A parameter with a default value is before one without: ");
			szLine.Append((psNew->iOld >= 0) ? psSignature->aszOldNames.GetText((size)psNew->iOld) : psNew->szName.Text());
			szLine.Append(" needs one, or to come first.");
			paszDest->Add(szLine.Text());
			szLine.Kill();
		}
		bAfterDefault |= bDefault;
		if ((psNew->iOld < 0) && psNew->szCallValue.Empty() && psNew->szDefault.Empty() && (psSignature->apsCalls.NumElements() > 0))
		{
			szLine.Init("No value is given for the new parameter ");
			szLine.Append(psNew->szName);
			szLine.Append(" in the ");
			szLine.Append((uint32)psSignature->apsCalls.NumElements());
			szLine.Append(psSignature->apsCalls.NumElements() == 1 ? " call there is." : " calls there are.");
			paszDest->Add(szLine.Text());
			szLine.Kill();
		}
	}

	//Another function of its name in its scope with the new parameters.
	szNewTypes.Init();
	for (j = 0; j < pasNew->NumElements(); j++)
	{
		psNew = pasNew->Get(j);
		szNewTypes.Append((j > 0) ? "," : "");
		NormaliseIDEParameterType(((psNew->iOld >= 0) && psNew->szType.Empty()) ? psSignature->aszOldTypes.GetText((size)psNew->iOld) : psNew->szType.Text(), &szNewTypes);
	}
	for (i = 0; i < psSignature->aszOverloads.NumElements(); i++)
	{
		if (psSignature->aszOverloads.Get(i)->Equals(psSignature->szKey))
		{
			continue;
		}
		szParameters = strchr(psSignature->aszOverloads.GetText(i), '(');
		if (!szParameters)
		{
			continue;
		}
		szOther.Init(szParameters + 1);
		if (szOther.Contains(")"))
		{
			szOther.SetLength(szOther.Find(")"));
		}
		szPacked.Init();
		NormaliseIDEParameterType(szOther.Text(), &szPacked);
		if (szPacked.Equals(&szNewTypes))
		{
			szLine.Init("There is another ");
			GetIDEScopedName(psSignature->aszOverloads.GetText(i), &szLine);
			szLine.Append(" with these parameters: ");
			szLine.Append(psSignature->aszOverloads.GetText(i));
			szLine.Append(".");
			paszDest->Add(szLine.Text());
			szLine.Kill();
		}
		szPacked.Kill();
		szOther.Kill();
	}
	szNewTypes.Kill();

	//To return nothing, where a call uses what it returns.
	if (psChange->szReturn.Equals("void") && !psSignature->szOldReturn.Equals("void"))
	{
		for (j = 0; j < psSignature->apsCalls.NumElements(); j++)
		{
			psCall = psSignature->apsCalls.GetPtr(j);
			if (psCall->bValueUsed)
			{
				szLine.Init("What it returns is used by the call at ");
				AppendPlace(psCall->szFileName.Text(), psCall->iLine, &szLine);
				szLine.Append(".");
				paszDest->Add(szLine.Text());
				szLine.Kill();
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A declaration's or definition's parameters as they are to be, from
// what it writes now (szText is its file's text): each kept parameter's
// own text, with its name, its type or its default value changed if they
// are to be, and each new one's type and name, with its default value
// where its symbol is first declared (bFirst): an override's declaration
// has it too, for the calls made on its class.  A changed default value
// is written where the parameter had one, or where the symbol is first
// declared if it had none.  With no parameters, "void", as the house
// style writes it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::AppendSiteParameters(SIDESignature* psSignature, SIDESignatureSite* psSite, bool bFirst, CArrayIDENewParameters* pasNew, const char* szText, CChars* pszDest)
{
	SIDENewParameter*		psNew;
	SIDEWrittenParameter*	psWritten;
	SCPPSyntaxRange			sRange;
	CChars					szPart;
	size					uiWritten;
	size					j;

	uiWritten = 0;
	for (j = 0; j < pasNew->NumElements(); j++)
	{
		psNew = pasNew->Get(j);
		if (psNew->iOld >= 0)
		{
			if ((size)psNew->iOld >= psSite->asParameters.NumElements())
			{
				continue;
			}
			psWritten = psSite->asParameters.Get((size)psNew->iOld);
			pszDest->Append((uiWritten > 0) ? ", " : "");

			//Its type and name: what it writes up to its default value, or its new type and its name.
			if (!psNew->szType.Empty())
			{
				pszDest->Append(psNew->szType);
				szPart.Init();
				if (psNew->szName.Empty())
				{
					AppendIDERangeText(szText, &psWritten->sName, &szPart);
				}
				else
				{
					szPart.Append(psNew->szName);
				}
				if (!szPart.Empty())
				{
					pszDest->Append(' ')->Append(szPart);
				}
				szPart.Kill();
			}
			else
			{
				sRange = psWritten->sAll;
				sRange.iEndLine = psWritten->sName.iLine;
				sRange.iEndColumn = psWritten->sName.iColumn;
				AppendIDERangeText(szText, &sRange, pszDest);
				if (psNew->szName.Empty() || !psWritten->bName)
				{
					AppendIDERangeText(szText, &psWritten->sName, pszDest);
				}
				else
				{
					pszDest->Append(psNew->szName);
				}
				sRange = psWritten->sAll;
				sRange.iLine = psWritten->sName.iEndLine;
				sRange.iColumn = psWritten->sName.iEndColumn;
				sRange.iEndLine = psWritten->sDefault.iLine;
				sRange.iEndColumn = psWritten->sDefault.iColumn;
				szPart.Init();
				AppendIDERangeText(szText, &sRange, &szPart);
				szPart.StripWhitespace();
				pszDest->Append(szPart);
				szPart.Kill();
			}

			//Its default value.
			if (!psNew->bDefaultChanged)
			{
				if (psWritten->bDefault)
				{
					pszDest->Append(' ');
					AppendIDERangeText(szText, &psWritten->sDefault, pszDest);
				}
			}
			else if (!psNew->szDefault.Empty() && (psWritten->bDefault || (bFirst && psSignature->aszOldDefaults.Get((size)psNew->iOld)->Empty())))
			{
				pszDest->Append(" = ")->Append(psNew->szDefault);
			}
		}
		else
		{
			pszDest->Append((uiWritten > 0) ? ", " : "");
			pszDest->Append(psNew->szType)->Append(' ')->Append(psNew->szName);
			if (bFirst && !psNew->szDefault.Empty())
			{
				pszDest->Append(" = ")->Append(psNew->szDefault);
			}
		}
		uiWritten++;
	}
	if (uiWritten == 0)
	{
		pszDest->Append("void");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The function as its first declaration would say it with a change
// made, on one line, for showing: "int CBrush::Mix(int iBlue, int iRed)".
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::AppendNewSignature(SIDESignature* psSignature, SIDESignatureChange* psChange, CChars* pszDest)
{
	SIDENewParameter*	psNew;
	const char*			szText;
	size				uiScope;
	size				j;

	szText = psChange->szReturn.Empty() ? psSignature->szOldReturn.Text() : psChange->szReturn.Text();
	if (szText[0] != '\0')
	{
		pszDest->Append(szText)->Append(' ');
	}
	if (psChange->szName.Empty())
	{
		pszDest->Append(psSignature->szName);
	}
	else
	{
		uiScope = psSignature->szName.Length() - psSignature->szOldName.Length();
		pszDest->AppendSubString(psSignature->szName.Text(), 0, uiScope);
		pszDest->Append(psChange->szName);
	}
	pszDest->Append('(');
	for (j = 0; j < psChange->asParameters.NumElements(); j++)
	{
		psNew = psChange->asParameters.Get(j);
		pszDest->Append((j > 0) ? ", " : "");
		pszDest->Append(((psNew->iOld >= 0) && psNew->szType.Empty()) ? psSignature->aszOldTypes.GetText((size)psNew->iOld) : psNew->szType.Text());
		szText = ((psNew->iOld >= 0) && psNew->szName.Empty()) ? psSignature->aszOldNames.GetText((size)psNew->iOld) : psNew->szName.Text();
		if (szText[0] != '\0')
		{
			pszDest->Append(' ')->Append(szText);
		}
		szText = ((psNew->iOld >= 0) && !psNew->bDefaultChanged) ? psSignature->aszOldDefaults.GetText((size)psNew->iOld) : psNew->szDefault.Text();
		if (szText[0] != '\0')
		{
			pszDest->Append(" = ")->Append(szText);
		}
	}
	if (psChange->asParameters.NumElements() == 0)
	{
		pszDest->Append("void");
	}
	pszDest->Append(')');
}


//////////////////////////////////////////////////////////////////////////
//
// A call's arguments as they are to be, from what it writes now: each
// kept parameter's argument, each new one's value.  An argument the call
// left to a default value, and a new parameter with no value given, are
// left out while nothing follows them; where an argument follows one,
// its default value is written (the first declaration's, or the new
// parameter's), and so it is where the parameter is to have no default
// value any more.  Arguments past the function's parameters (a variadic
// function's) stay at the end.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::AppendCallArguments(SIDESignature* psSignature, SIDESignatureCall* psCall, CArrayIDENewParameters* pasNew, const char* szText, CChars* pszDest)
{
	CArrayChars			aszArguments;
	CArrayInt			aiGiven;
	SIDENewParameter*	psNew;
	CChars				szArgument;
	size				uiOld;
	size				uiLast;
	size				j;

	aszArguments.Init();
	aiGiven.Init();
	uiOld = psSignature->aszOldTypes.NumElements();
	for (j = 0; j < pasNew->NumElements(); j++)
	{
		psNew = pasNew->Get(j);
		szArgument.Init();
		if ((psNew->iOld >= 0) && ((size)psNew->iOld < psCall->asArguments.NumElements()))
		{
			AppendIDERangeText(szText, psCall->asArguments.Get((size)psNew->iOld), &szArgument);
			aiGiven.Add(1);
		}
		else if (psNew->iOld >= 0)
		{
			//Left to its default value: it still is, unless the parameter is to have none.
			szArgument.Append(psSignature->aszOldDefaults.GetText((size)psNew->iOld));
			aiGiven.Add((psNew->bDefaultChanged && psNew->szDefault.Empty()) ? 1 : 0);
		}
		else if (!psNew->szCallValue.Empty())
		{
			szArgument.Append(psNew->szCallValue);
			aiGiven.Add(1);
		}
		else
		{
			szArgument.Append(psNew->szDefault);
			aiGiven.Add(0);
		}
		aszArguments.Add(szArgument.Text());
		szArgument.Kill();
	}
	for (j = uiOld; j < psCall->asArguments.NumElements(); j++)
	{
		szArgument.Init();
		AppendIDERangeText(szText, psCall->asArguments.Get(j), &szArgument);
		aszArguments.Add(szArgument.Text());
		aiGiven.Add(1);
		szArgument.Kill();
	}

	//Up to the last that is given.
	uiLast = 0;
	for (j = 0; j < aiGiven.NumElements(); j++)
	{
		if (aiGiven.GetValue(j) != 0)
		{
			uiLast = j + 1;
		}
	}
	for (j = 0; j < uiLast; j++)
	{
		pszDest->Append((j > 0) ? ", " : "");
		pszDest->Append(aszArguments.GetText(j));
	}
	aiGiven.Kill();
	aszArguments.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::ShowChangeStatus(CChars* pszMessage, CChars* pszStatus)
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
// Whether hand written assembly's labels are to be looked to: the
// solution has assembly, and its build configuration is for a processor
// (*peTarget), whose labels they are.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::GetAssemblyTarget(ECPPTarget* peTarget)
{
	CIDECodeModel*			pcModel;
	CBuildConfiguration*	pcConfiguration;
	size					i;
	bool					bAssembly;

	pcConfiguration = mpcContent->GetBuildConfigurations()->HasConfiguration() ? mpcContent->GetBuildConfigurations()->GetConfiguration() : NULL;
	if (!pcConfiguration || ((pcConfiguration->GetToolchain() != BT_W65C816) && (pcConfiguration->GetToolchain() != BT_65C64)))
	{
		return false;
	}
	*peTarget = (pcConfiguration->GetToolchain() == BT_W65C816) ? CPPT_W65C816 : CPPT_65C64;
	pcModel = mpcContent->GetCodeModel();
	bAssembly = false;
	for (i = 0; !bAssembly && (i < pcModel->NumProjectFiles()); i++)
	{
		bAssembly = IsIDEAssemblyFile(pcModel->GetProjectFile(i));
	}
	return bAssembly;
}


//////////////////////////////////////////////////////////////////////////
//
// The label the compiler gives the function whose declaration's or
// definition's name the caret is on, for the build configuration's
// processor.  False if the configuration isn't for one, or no function
// is declared there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::AppendCaretLabel(CChars* pszDest)
{
	CIDERefactorUnit	cUnit;
	Ptr<CIDEEditorTab>	pTab;
	ECPPTarget			eTarget;
	int					iLine;
	int					iColumn;
	uint8				uiRole;
	bool				bResult;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !GetAssemblyTarget(&eTarget) || !mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole))
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cUnit.Init();
	bResult = cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), pTab->GetFileName(), true, eTarget) && cUnit.AppendFunctionLabel(iLine, iColumn, pszDest);
	cUnit.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The label of each symbol changed, asked of the compiler where the
// symbol is first declared (a kept check of that file for eTarget): as
// it is now, into the site (bNew false), or as it is once the change is
// made, into paszNew, one for each site (empty where it has none), with
// the file parsed from its editor's text and the name found where it now
// is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEChangeSignature::FindLabels(SIDESignature* psSignature, ECPPTarget eTarget, bool bNew, CArrayChars* paszNew)
{
	CIDERefactorUnit	cUnit;
	SIDESignatureSite*	psSite;
	Ptr<CIDEEditorTab>	pTab;
	CChars				szLabel;
	size				uiLine;
	size				i;
	int					iLine;
	int					iColumn;

	if (bNew)
	{
		mpcContent->GetDocuments()->UpdateEditedTabsNow();
	}
	for (i = 0; i < psSignature->apsSites.NumElements(); i++)
	{
		psSite = psSignature->apsSites.GetPtr(i);
		szLabel.Init();
		if (psSite->bFirst)
		{
			iLine = psSite->iLine;
			iColumn = psSite->iColumn;
			pTab = mpcContent->GetDocuments()->FindTab(psSite->szFileName.Text());
			if (bNew && pTab.IsNotNull())
			{
				uiLine = pTab->GetEditor()->FindLine(psSite->uiNewName);
				iLine = (int)uiLine;
				iColumn = (int)(psSite->uiNewName - pTab->GetEditor()->GetLineStart(uiLine));
			}
			cUnit.Init();
			if (cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), psSite->szFileName.Text(), true, eTarget))
			{
				cUnit.AppendFunctionLabel(iLine, iColumn, &szLabel);
			}
			cUnit.Kill();
		}
		if (bNew)
		{
			paszNew->Add(szLabel.Text());
		}
		else
		{
			psSite->szLabel.Set(szLabel);
		}
		szLabel.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each symbol's old label changed to its new one in the solution's
// assembly, where they differ: every place it is written as a whole
// label, in each file's editor (opened, not shown, if it isn't open and
// has one).  The number of places changed.
//
//////////////////////////////////////////////////////////////////////////
size CIDEChangeSignature::ChangeLabels(SIDESignature* psSignature, CArrayChars* paszNew)
{
	CIDECodeModel*		pcModel;
	CIDETextEdits		cEdits;
	CTextFile			cFile;
	Ptr<CIDEEditorTab>	pTab;
	SIDESignatureSite*	psSite;
	char*				szFileName;
	char*				szText;
	const char*			szFound;
	const char*			szOld;
	size				uiLength;
	size				uiOldLength;
	size				uiAt;
	size				uiChanged;
	size				uiInFile;
	size				i;
	size				j;
	int					iPass;

	pcModel = mpcContent->GetCodeModel();
	uiChanged = 0;
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFileName = pcModel->GetProjectFile(i);
		if (!IsIDEAssemblyFile(szFileName))
		{
			continue;
		}

		//Looked in as it is first, so only a file that has one of the labels is opened; then its editor is changed.
		for (iPass = 0; iPass < 2; iPass++)
		{
			cFile.Init();
			cEdits.Init();
			pTab = mpcContent->GetDocuments()->FindTab(szFileName);
			if (iPass == 1)
			{
				if (pTab.IsNull())
				{
					pTab = mpcContent->GetDocuments()->OpenFileTab(szFileName, pcModel->FindProjectOfFile(szFileName), false);
				}
				szText = pTab.IsNotNull() ? pTab->GetEditor()->GetText() : NULL;
			}
			else if (!mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength))
			{
				szText = NULL;
			}
			uiInFile = 0;
			for (j = 0; szText && (j < psSignature->apsSites.NumElements()); j++)
			{
				psSite = psSignature->apsSites.GetPtr(j);
				szOld = psSite->szLabel.Text();
				uiOldLength = psSite->szLabel.Length();
				if ((uiOldLength == 0) || paszNew->Get(j)->Empty() || psSite->szLabel.Equals(paszNew->GetText(j)))
				{
					continue;
				}
				for (szFound = strstr(szText, szOld); szFound; szFound = strstr(szFound + 1, szOld))
				{
					uiAt = (size)(szFound - szText);
					if (((uiAt == 0) || !IsIDENameChar(szText[uiAt - 1])) && !IsIDENameChar(szText[uiAt + uiOldLength]))
					{
						cEdits.Replace(uiAt, uiAt + uiOldLength, paszNew->GetText(j));
						uiInFile++;
					}
				}
			}
			if ((iPass == 1) && (uiInFile > 0) && pTab.IsNotNull())
			{
				cEdits.Apply(pTab->GetEditor());
				uiChanged += uiInFile;
			}
			cEdits.Kill();
			cFile.Kill();
			if (uiInFile == 0)
			{
				break;
			}
		}
	}
	return uiChanged;
}


//////////////////////////////////////////////////////////////////////////
//
// The function given the new parameters, whatever stands in the way:
// every declaration's and definition's parameters, every call's
// arguments, and a renamed parameter's uses in each definition's body,
// each file as one edit of its editor (opened, not shown, if it isn't
// open) and all of them as one undo group.  Nothing is saved.  False if a
// file couldn't be opened; nothing is changed then.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEChangeSignature::Change(SIDESignature* psSignature, SIDESignatureChange* psChange, CChars* pszStatus)
{
	CArrayChars				aszNewLabels;
	ECPPTarget				eTarget;
	size					uiLabels;
	bool					bLabels;
	CArrayIDENewParameters*	pasNew;
	SIDERenameTarget		sTarget;
	CChars					szReturn;
	size					uiEnd;
	size					uiName;
	size					uiLine;
	bool					bNameFound;
	CIDECodeModel*			pcModel;
	CArrayChars				aszFiles;
	CArrayIDELocations		asLocations;
	CIDETextEdits			cEdits;
	Ptr<CIDEEditorTab>		pTab;
	Ptr<CTextArea>			pEditor;
	SIDESignatureSite*		psSite;
	SIDESignatureCall*		psCall;
	SIDEWrittenParameter*	psWritten;
	SIDENewParameter*		psNew;
	SIDELocation*			psLocation;
	CChars					szMessage;
	CChars					szNew;
	CChars					szOldName;
	char*					szText;
	const char*				szFileName;
	size					uiFile;
	size					uiStart;
	size					i;
	size					j;
	size					k;
	uint8					uiKind;
	bool					bOpened;

	pcModel = mpcContent->GetCodeModel();
	pasNew = &psChange->asParameters;
	uiName = 0;
	bNameFound = false;
	uiLabels = 0;

	//The labels the assembly names them by, as they are now.
	bLabels = GetAssemblyTarget(&eTarget);
	if (bLabels)
	{
		FindLabels(psSignature, eTarget, false, NULL);
	}

	//Every file's editor first: nothing is changed unless all can be.
	aszFiles.Init();
	bOpened = true;
	for (i = 0; bOpened && (i < psSignature->apsSites.NumElements() + psSignature->apsCalls.NumElements()); i++)
	{
		szFileName = (i < psSignature->apsSites.NumElements()) ? psSignature->apsSites.GetPtr(i)->szFileName.Text() : psSignature->apsCalls.GetPtr(i - psSignature->apsSites.NumElements())->szFileName.Text();
		for (j = 0; (j < aszFiles.NumElements()) && (StringInsensitiveCompare(aszFiles.GetText(j), szFileName) != 0); j++)
		{
		}
		if (j < aszFiles.NumElements())
		{
			continue;
		}
		pTab = mpcContent->GetDocuments()->FindTab(szFileName);
		if (pTab.IsNull())
		{
			pTab = mpcContent->GetDocuments()->OpenFileTab(szFileName, pcModel->FindProjectOfFile(szFileName), false);
		}
		bOpened = pTab.IsNotNull();
		if (bOpened)
		{
			aszFiles.Add(szFileName);
		}
		else
		{
			szMessage.Init("Change Signature stopped: ");
			szMessage.Append(szFileName);
			szMessage.Append(" couldn't be opened.");
			ShowChangeStatus(&szMessage, pszStatus);
			szMessage.Kill();
		}
	}

	if (bOpened)
	{
		mpcContent->GetEdit()->BeginUndoGroup();
		for (uiFile = 0; uiFile < aszFiles.NumElements(); uiFile++)
		{
			pEditor = mpcContent->GetDocuments()->FindTab(aszFiles.GetText(uiFile))->GetEditor();
			szText = pEditor->GetText();
			cEdits.Init();
			for (i = 0; i < psSignature->apsSites.NumElements(); i++)
			{
				psSite = psSignature->apsSites.GetPtr(i);
				if (StringInsensitiveCompare(psSite->szFileName.Text(), aszFiles.GetText(uiFile)) != 0)
				{
					continue;
				}
				szNew.Init();
				AppendSiteParameters(psSignature, psSite, psSite->bFirst, pasNew, szText, &szNew);
				cEdits.Replace(FindIDETextOffset(szText, psSite->sParameters.iLine, psSite->sParameters.iColumn), FindIDETextOffset(szText, psSite->sParameters.iEndLine, psSite->sParameters.iEndColumn), szNew.Text());
				szNew.Kill();

				//What it returns, the white space that was before its name kept (one space if there was none).
				if (!psChange->szReturn.Empty() && psSite->bReturn)
				{
					uiStart = FindIDETextOffset(szText, psSite->sReturn.iLine, psSite->sReturn.iColumn);
					uiEnd = FindIDETextOffset(szText, psSite->sReturn.iEndLine, psSite->sReturn.iEndColumn);
					for (k = uiEnd; (k > uiStart) && ((szText[k - 1] == ' ') || (szText[k - 1] == '\t')); k--)
					{
					}
					szReturn.Init(psChange->szReturn);
					if (k == uiEnd)
					{
						szReturn.Append(' ');
					}
					else if (IsAllTabs(szText, k, uiEnd))
					{
						//Lined up with tabs, as a class's functions are: its name stays in its column if the type allows.
						for (uiLine = uiStart; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
						{
						}
						AppendIDETabsTo(&szReturn, FindIDEVisualColumn(szText, uiLine, uiStart, 4) + psChange->szReturn.Length(), FindIDEVisualColumn(szText, uiLine, uiEnd, 4), 4);
					}
					else
					{
						szReturn.AppendSubString(szText, k, uiEnd);
					}
					cEdits.Replace(uiStart, uiEnd, szReturn.Text());
					szReturn.Kill();
				}

				//A renamed parameter, where the body uses it.
				for (j = 0; psSite->bDefinition && (j < pasNew->NumElements()); j++)
				{
					psNew = pasNew->Get(j);
					psWritten = ((psNew->iOld >= 0) && ((size)psNew->iOld < psSite->asParameters.NumElements())) ? psSite->asParameters.Get((size)psNew->iOld) : NULL;
					if (!psWritten || psNew->szName.Empty() || psWritten->szKey.Empty())
					{
						continue;
					}
					szOldName.Init();
					AppendIDERangeText(szText, &psWritten->sName, &szOldName);
					if (!szOldName.Equals(psNew->szName))
					{
						asLocations.Init();
						pcModel->GetSymbolLocations(psWritten->szKey.Text(), &asLocations, &uiKind);
						for (k = 0; k < asLocations.NumElements(); k++)
						{
							psLocation = asLocations.Get(k);
							uiStart = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
							if ((psLocation->uiRole == CPPIR_Reference) && (StringInsensitiveCompare(psLocation->szFileName.Text(), aszFiles.GetText(uiFile)) == 0) &&
								(strncmp(&szText[uiStart], szOldName.Text(), szOldName.Length()) == 0))
							{
								cEdits.Replace(uiStart, uiStart + szOldName.Length(), psNew->szName.Text());
							}
						}
						KillIDELocations(&asLocations);
					}
					szOldName.Kill();
				}
			}
			for (i = 0; i < psSignature->apsCalls.NumElements(); i++)
			{
				psCall = psSignature->apsCalls.GetPtr(i);
				if (StringInsensitiveCompare(psCall->szFileName.Text(), aszFiles.GetText(uiFile)) != 0)
				{
					continue;
				}
				szNew.Init();
				AppendCallArguments(psSignature, psCall, pasNew, szText, &szNew);
				cEdits.Replace(FindIDETextOffset(szText, psCall->sArguments.iLine, psCall->sArguments.iColumn), FindIDETextOffset(szText, psCall->sArguments.iEndLine, psCall->sArguments.iEndColumn), szNew.Text());
				szNew.Kill();
			}
			//Where each one's name is once the edits are made: for its new label, and for renaming the function.
			for (i = 0; i < psSignature->apsSites.NumElements(); i++)
			{
				psSite = psSignature->apsSites.GetPtr(i);
				if (StringInsensitiveCompare(psSite->szFileName.Text(), aszFiles.GetText(uiFile)) == 0)
				{
					psSite->uiNewName = cEdits.AdjustPosition(FindIDETextOffset(szText, psSite->iLine, psSite->iColumn));
				}
			}
			if (!psChange->szName.Empty() && (StringInsensitiveCompare(psSignature->apsSites.GetPtr(0)->szFileName.Text(), aszFiles.GetText(uiFile)) == 0))
			{
				uiName = psSignature->apsSites.GetPtr(0)->uiNewName;
				bNameFound = true;
			}
			cEdits.Apply(pEditor);
			cEdits.Kill();
		}

		//The assembly's labels, from what they were to what the compiler gives the functions now.
		if (bLabels)
		{
			aszNewLabels.Init();
			FindLabels(psSignature, eTarget, true, &aszNewLabels);
			uiLabels = ChangeLabels(psSignature, &aszNewLabels);
			aszNewLabels.Kill();
		}

		//Its name, by Rename, once the index has what was just changed: the overrides, the family and the assembly's
		//labels are renamed as Rename renames them.
		if (bNameFound)
		{
			mpcContent->GetDocuments()->UpdateEditedTabsNow();
			pEditor = mpcContent->GetDocuments()->FindTab(psSignature->apsSites.GetPtr(0)->szFileName.Text())->GetEditor();
			uiLine = pEditor->FindLine(uiName);
			sTarget.Init();
			if (pcModel->FindRenameTarget(psSignature->apsSites.GetPtr(0)->szFileName.Text(), (int)uiLine, (int)(uiName - pEditor->GetLineStart(uiLine)), &sTarget) && sTarget.bRenamable)
			{
				szMessage.Init();
				mpcContent->GetRename()->RenameSymbol(&sTarget, psChange->szName.Text(), NULL, &szMessage);
				szMessage.Kill();
			}
			sTarget.Kill();
		}
		mpcContent->GetEdit()->EndUndoGroup();

		szMessage.Init("Changed ");
		szMessage.Append(psSignature->szName);
		szMessage.Append("'s signature: ");
		szMessage.Append((uint32)psSignature->apsSites.NumElements());
		szMessage.Append(psSignature->apsSites.NumElements() == 1 ? " declaration" : " declarations and definitions");
		if (psSignature->uiOverrides > 0)
		{
			szMessage.Append(" (")->Append((uint32)psSignature->uiOverrides)->Append(psSignature->uiOverrides == 1 ? " override)" : " overrides)");
		}
		szMessage.Append(", ");
		szMessage.Append((uint32)psSignature->apsCalls.NumElements());
		szMessage.Append(psSignature->apsCalls.NumElements() == 1 ? " call in " : " calls in ");
		szMessage.Append((uint32)aszFiles.NumElements());
		szMessage.Append(aszFiles.NumElements() == 1 ? " file." : " files.");
		if (uiLabels > 0)
		{
			szMessage.Append("  ")->Append((uint32)uiLabels)->Append(uiLabels == 1 ? " label in assembly." : " labels in assembly.");
		}
		if (psSignature->uiFamily > 0)
		{
			szMessage.Append("  With ")->Append(psSignature->szFamily)->Append(", which a template calls with it by one name.");
		}
		if (bNameFound)
		{
			szMessage.Append("  It is now ")->Append(psChange->szName)->Append(".");
		}
		ShowChangeStatus(&szMessage, pszStatus);
		szMessage.Kill();
	}
	aszFiles.Kill();
	return bOpened;
}
