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
#include "IDEIntroduceVariable.h"
#include "IDEDefinitions.h"
#include "IDEMoveMembers.h"
#include "IDEExtractMethod.h"
#include "IDEContextActions.h"


#define IDE_EXTRACT_BANNER	"//////////////////////////////////////////////////////////////////////////\n//\n//\n//////////////////////////////////////////////////////////////////////////\n"
#define IDE_EXTRACT_NO_TOKEN	0x7fffffff


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEExtractVariable::Init(void)
{
	szKey.Init();
	szName.Init();
	szBefore.Init();
	szAfter.Init();
	iDeclaredToken = 0;
	bObject = false;
	bArray = false;
	bDeclaredInside = false;
	bHasValue = false;
	bReadInside = false;
	bWrittenInside = false;
	bUsedInside = false;
	bUsedBefore = false;
	bUsedAfter = false;
	bAlone = false;
	uiDeclaration = 0;
	uiDeclarationEnd = 0;
	iRole = IDE_EXTRACT_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEExtractVariable::Kill(void)
{
	szAfter.Kill();
	szBefore.Kill();
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEExtraction::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szFileName.Init();
	bExpression = false;
	szType.Init();
	uiStart = 0;
	uiEnd = 0;
	szIndent.Init();
	asVariables.Init();
	bReturns = false;
	szFunctionReturn.Init();
	sFunction.Init();
	szClassKey.Init();
	szQualifier.Init();
	szTemplateArguments.Init();
	bBefore = false;
	aszUsed.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEExtraction::Kill(void)
{
	size	i;

	aszUsed.Kill();
	szTemplateArguments.Kill();
	szQualifier.Kill();
	szClassKey.Kill();
	sFunction.Kill();
	szFunctionReturn.Kill();
	for (i = 0; i < asVariables.NumElements(); i++)
	{
		asVariables.Get(i)->Kill();
	}
	asVariables.Kill();
	szIndent.Kill();
	szType.Kill();
	szFileName.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpsShown = NULL;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpNameField);
	M_Pointer(mpSignature);
	M_Pointer(mpError);
	U_Pointer(mpsShown);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::Save(CObjectWriter* pcFile) { return false; }
bool CIDEExtractMethod::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Ctrl+Alt+M, as IntelliJ IDEA's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.ExtractMethod", "Extract Method...", "Control+Alt+M", Ptr<CImageCel>(), this, &CIDEExtractMethod::QueueExtract)->SetEnabledCheck(this, &CIDEExtractMethod::CanExtract);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ExtractMethod"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: Extract Method is for an expression or whole
// statements selected in a function's body.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bSelection && psContext->bInFunction && (psContext->bExpression || psContext->bStatements))
	{
		psContext->Offer("Refactor.ExtractMethod");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::CanExtract(void)
{
	size	uiStart;
	size	uiEnd;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->GetActiveTab()->GetEditor()->GetSelection(&uiStart, &uiEnd);
}


void CIDEExtractMethod::QueueExtract(void) { mpcContent->GetWindow()->Post(this, &CIDEExtractMethod::ShowQueuedExtract); }
void CIDEExtractMethod::ShowQueuedExtract(void) { ShowExtract(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::Refuse(SIDEExtraction* psDest, const char* szWhyNot)
{
	psDest->bPossible = false;
	psDest->szWhyNot.Set(szWhyNot);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The function the selection is in, as its definition is written: what
// it returns, its class, what its name is written after, and, for a
// function template, its parameters as a call names them.  False if it
// can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::FindFunction(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, const char* szText, SIDEExtraction* psDest)
{
	CIDECodeModel*					pcModel;
	CArrayIDELocations				asLocations;
	CCPPSyntaxName*					pcName;
	CCPPSyntaxTemplateDeclaration*	pcTemplate;
	CCPPSyntax*						pcParent;
	SCPPSyntaxRange					sRange;
	SIDELocation					sLocation;
	CChars							szKey;
	CChars							szScope;
	CChars							szName;
	const char*						szWhyNot;
	size							uiTemplates;
	size							i;
	int32							iToken;
	uint8							uiKind;
	bool							bRead;

	pcModel = mpcContent->GetCodeModel();
	pcName = pcFunction->mpcDeclarator ? pcFunction->mpcDeclarator->GetName() : NULL;
	if (!pcName || !pcName->GetLast() || !pcUnit->GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sRange))
	{
		return false;
	}
	szKey.Init();
	pcModel->GetSymbolAt(psDest->szFileName.Text(), sRange.iLine, sRange.iColumn, &szKey, &uiKind);
	sLocation.szFileName.Init(psDest->szFileName);
	sLocation.iLine = sRange.iLine;
	sLocation.iColumn = sRange.iColumn;
	sLocation.iLength = sRange.iEndColumn - sRange.iColumn;
	sLocation.uiRole = CPPIR_Definition;
	sLocation.uiFlags = 0;
	sLocation.uiUsage = 0;
	sLocation.iContainer = -1;
	bRead = mpcContent->GetChangeSignature()->ReadSite(pcUnit, &sLocation, szKey.Text(), &psDest->sFunction, &szWhyNot);
	sLocation.szFileName.Kill();
	if (!bRead)
	{
		szKey.Kill();
		return false;
	}

	if (psDest->sFunction.bReturn)
	{
		AppendIDERangeText(szText, &psDest->sFunction.sReturn, &psDest->szFunctionReturn);
		psDest->szFunctionReturn.StripWhitespace();
	}
	szScope.Init();
	GetIDESymbolScope(szKey.Text(), &szScope);
	asLocations.Init();
	if (!szScope.Empty() && pcModel->GetSymbolLocations(szScope.Text(), &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
	{
		psDest->szClassKey.Set(szScope);
	}
	KillIDELocations(&asLocations);
	szScope.Kill();
	szKey.Kill();

	//What its name is written after: its name's text without its own name.
	AppendIDERangeText(szText, &psDest->sFunction.sName, &psDest->szQualifier);
	if (psDest->szQualifier.EndsWith(psDest->sFunction.szOwnName.Text()))
	{
		psDest->szQualifier.SetLength(psDest->szQualifier.Length() - psDest->sFunction.szOwnName.Length());
	}
	else
	{
		psDest->szQualifier.Clear();
	}
	psDest->bBefore = psDest->szClassKey.Empty();

	//A template<...> of its own (not its class template's, which its qualifier's <...> is for): the call names its
	//parameters.
	uiTemplates = 0;
	pcTemplate = NULL;
	for (pcParent = pcFunction->mpcParent; pcParent && (pcParent->GetKind() == CPPS_TemplateDeclaration); pcParent = pcParent->mpcParent)
	{
		pcTemplate = pcTemplate ? pcTemplate : (CCPPSyntaxTemplateDeclaration*)pcParent;
		uiTemplates++;
	}
	if (pcTemplate && (uiTemplates > (psDest->szQualifier.Contains("<") ? (size)1 : (size)0)))
	{
		psDest->szTemplateArguments.Append('<');
		for (i = 0; i < pcTemplate->macpParameters.NumElements(); i++)
		{
			iToken = -1;
			if (pcTemplate->macpParameters.GetPtr(i)->GetKind() == CPPS_TypeParameter)
			{
				iToken = ((CCPPSyntaxTypeParameter*)pcTemplate->macpParameters.GetPtr(i))->miName;
			}
			else if ((pcTemplate->macpParameters.GetPtr(i)->GetKind() == CPPS_Parameter) && ((CCPPSyntaxParameter*)pcTemplate->macpParameters.GetPtr(i))->mpcDeclarator &&
					 ((CCPPSyntaxParameter*)pcTemplate->macpParameters.GetPtr(i))->mpcDeclarator->GetName())
			{
				iToken = ((CCPPSyntaxParameter*)pcTemplate->macpParameters.GetPtr(i))->mpcDeclarator->GetName()->miFirstToken;
			}
			szName.Init();
			if (iToken >= 0)
			{
				pcUnit->AppendTokens(iToken, iToken, &szName);
			}
			psDest->szTemplateArguments.Append((i > 0) ? ", " : "");
			psDest->szTemplateArguments.Append(szName);
			szName.Kill();
		}
		psDest->szTemplateArguments.Append('>');
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The function's variables and parameters that the selection (the tokens
// iFirst to iLast, statements of pcBlock; pcBlock is NULL for an
// expression) names or declares, each with how it is used in the
// selection, before it and after it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::FindVariables(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, CCPPSyntax* pcBlock, int32 iFirst, int32 iLast, const char* szText, SIDEExtraction* psDest)
{
	CIDECodeModel*					pcModel;
	CArrayIDEFileRanges				asRanges;
	CArrayIDELocations				asLocations;
	CArrayChars						aszKeys;
	SIDEExtractVariable				sVariable;
	SIDELocation*					psLocation;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcUse;
	CCPPSyntax*						pcParent;
	CCPPSyntax*						pcLoop;
	CCPPSyntaxDeclarator*			pcDeclarator;
	CCPPSyntaxInitDeclarator*		pcInit;
	CCPPSyntaxDeclSpecifiers*		pcSpecifiers;
	CCPPSyntaxSimpleDeclaration*	pcDeclaration;
	CCPPSyntaxBinaryExpression*		pcBinary;
	CCPPSyntaxName*					pcName;
	SPreprocessedToken*				psSource;
	SCPPSyntaxRange					sRange;
	CChars							szType;
	size							uiAt;
	size							i;
	size							j;
	int32							iToken;
	int32							iFirstRead;
	int32							iFirstAssignEnd;
	int32							iLoopFirst;
	int32							iLoopLast;
	uint8							uiKind;
	uint8							uiChanges;
	bool							bPure;

	pcModel = mpcContent->GetCodeModel();

	//The outermost loop the selection is in: what is in it comes after the selection too.
	pcLoop = NULL;
	for (pcParent = pcUnit->FindNodeOfTokens(iFirst, iFirst); pcParent && (pcParent != pcFunction); pcParent = pcParent->mpcParent)
	{
		if (((pcParent->GetKind() == CPPS_WhileStatement) || (pcParent->GetKind() == CPPS_ForStatement) || (pcParent->GetKind() == CPPS_DoStatement) || (pcParent->GetKind() == CPPS_RangeForStatement)) &&
			!((pcParent->miFirstToken >= iFirst) && (pcParent->miLastToken <= iLast)))
		{
			pcLoop = pcParent;
		}
	}
	iLoopFirst = pcLoop ? pcLoop->miFirstToken : -1;
	iLoopLast = pcLoop ? pcLoop->miLastToken : -1;

	asRanges.Init();
	asRanges.Add()->Init(psDest->szFileName.Text(), psDest->sFunction.sAll.iLine, psDest->sFunction.sAll.iColumn, psDest->sFunction.sAll.iEndLine, psDest->sFunction.sAll.iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	KillIDEFileRanges(&asRanges);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		asLocations.Init();
		if ((aszKeys.GetText(i)[0] != '@') || !pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind) || ((uiKind != CPPSK_Variable) && (uiKind != CPPSK_Parameter)))
		{
			KillIDELocations(&asLocations);
			continue;
		}
		sVariable.Init();
		sVariable.szKey.Set(aszKeys.GetText(i));
		GetIDESymbolName(aszKeys.GetText(i), &sVariable.szName);
		iFirstRead = IDE_EXTRACT_NO_TOKEN;
		iFirstAssignEnd = IDE_EXTRACT_NO_TOKEN;
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			psLocation = asLocations.Get(j);
			if (StringInsensitiveCompare(psLocation->szFileName.Text(), psDest->szFileName.Text()) != 0)
			{
				continue;
			}
			iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
			pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
			if (!pcNode)
			{
				continue;
			}
			if (psLocation->uiRole != CPPIR_Reference)
			{
				//Its declaration: what is written before its name and after it, and whether it is an object.
				sVariable.iDeclaredToken = iToken;
				sVariable.bDeclaredInside = (iToken >= iFirst) && (iToken <= iLast);
				pcDeclarator = (CCPPSyntaxDeclarator*)pcUnit->FindAncestor(pcNode, CPPS_Declarator);
				while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
				{
					pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
				}
				pcName = pcDeclarator ? pcDeclarator->GetName() : NULL;
				pcParent = pcDeclarator ? pcDeclarator->mpcParent : NULL;
				pcInit = (pcParent && (pcParent->GetKind() == CPPS_InitDeclarator)) ? (CCPPSyntaxInitDeclarator*)pcParent : NULL;
				pcParent = pcInit ? pcInit->mpcParent : pcParent;
				pcSpecifiers = NULL;
				pcDeclaration = NULL;
				if (pcParent && (pcParent->GetKind() == CPPS_SimpleDeclaration))
				{
					pcDeclaration = (CCPPSyntaxSimpleDeclaration*)pcParent;
					pcSpecifiers = pcDeclaration->mpcSpecifiers;
					sVariable.bHasValue = pcInit && (pcInit->meInitializer != CPPI_None);
				}
				else if (pcParent && (pcParent->GetKind() == CPPS_Parameter))
				{
					pcSpecifiers = ((CCPPSyntaxParameter*)pcParent)->mpcSpecifiers;
					sVariable.bHasValue = true;
				}
				if (!pcName || !pcSpecifiers || (pcSpecifiers->miFirstToken < 0) || !pcUnit->GetTokensRange(pcSpecifiers->miFirstToken, pcName->miFirstToken - 1, &sRange))
				{
					continue;
				}
				AppendIDERangeText(szText, &sRange, &sVariable.szBefore);
				sVariable.szBefore.StripWhitespace();
				if ((pcDeclarator->miLastToken > pcName->miLastToken) && pcUnit->GetTokensRange(pcName->miLastToken + 1, pcDeclarator->miLastToken, &sRange))
				{
					AppendIDERangeText(szText, &sRange, &sVariable.szAfter);
					sVariable.bArray = true;
				}
				if (pcSpecifiers->mpcType && (pcDeclarator->macpPrefix.NumElements() == 0) && !sVariable.bArray)
				{
					for (iToken = pcSpecifiers->mpcType->miFirstToken; (iToken <= pcSpecifiers->mpcType->miLastToken) && (pcUnit->IsToken(iToken, "struct") || pcUnit->IsToken(iToken, "class")); iToken++)
					{
					}
					psSource = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)iToken));
					szType.Init();
					sVariable.bObject = pcModel->GetSymbolAt(psDest->szFileName.Text(), psSource->iLine, psSource->iColumn, &szType, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate));
					szType.Kill();
				}
				pcParent = (pcDeclaration && pcDeclaration->mpcParent && (pcDeclaration->mpcParent->GetKind() == CPPS_DeclarationStatement)) ? pcDeclaration->mpcParent : NULL;
				if (pcParent && (pcDeclaration->macpDeclarators.NumElements() == 1) && pcUnit->GetRange(pcParent, &sRange))
				{
					sVariable.bAlone = true;
					sVariable.uiDeclaration = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
					sVariable.uiDeclarationEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
				}
				continue;
			}

			if ((iToken < iFirst) || (iToken > iLast))
			{
				sVariable.bUsedBefore = sVariable.bUsedBefore || (iToken < iFirst);
				sVariable.bUsedAfter = sVariable.bUsedAfter || (iToken > iLast) || ((iToken >= iLoopFirst) && (iToken <= iLoopLast));
				continue;
			}

			//In the selection: read, or changed (the compiler says how), or only given a value.
			sVariable.bUsedInside = true;
			pcUse = pcUnit->FindAncestor(pcNode, CPPS_IdExpression);
			if (!pcUse)
			{
				sVariable.bWrittenInside = true;
				iFirstRead = (iToken < iFirstRead) ? iToken : iFirstRead;
				continue;
			}
			while (pcUse->mpcParent && (pcUse->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
			{
				pcUse = pcUse->mpcParent;
			}
			uiChanges = pcUnit->GetPass()->FindChanges(pcUse);
			pcBinary = (pcUse->mpcParent && (pcUse->mpcParent->GetKind() == CPPS_BinaryExpression)) ? (CCPPSyntaxBinaryExpression*)pcUse->mpcParent : NULL;
			bPure = pcBinary && (pcBinary->meOperator == CT_Assignment) && (pcBinary->mpcLeft == pcUse);
			sVariable.bWrittenInside = sVariable.bWrittenInside || bPure || (uiChanges != 0);
			if (!bPure)
			{
				iFirstRead = (iToken < iFirstRead) ? iToken : iFirstRead;
			}
			else if (pcBlock && pcBinary->mpcParent && (pcBinary->mpcParent->GetKind() == CPPS_ExpressionStatement) && (pcBinary->mpcParent->mpcParent == pcBlock))
			{
				//A statement of the selection itself gives it its value: what reads it after that statement reads that.
				iFirstAssignEnd = (pcBinary->mpcParent->miLastToken < iFirstAssignEnd) ? pcBinary->mpcParent->miLastToken : iFirstAssignEnd;
			}
		}
		KillIDELocations(&asLocations);
		//Read by the selection: unless a statement of the selection gives it a value first, or nothing before the
		//selection could have given it one (then what the selection reads is what the selection wrote).
		sVariable.bReadInside = (iFirstRead != IDE_EXTRACT_NO_TOKEN) && !((iFirstAssignEnd != IDE_EXTRACT_NO_TOKEN) && (iFirstRead > iFirstAssignEnd)) &&
								(sVariable.bHasValue || sVariable.bUsedBefore || sVariable.bDeclaredInside);
		if ((sVariable.bUsedInside || sVariable.bDeclaredInside) && !sVariable.szBefore.Empty())
		{
			for (uiAt = 0; (uiAt < psDest->asVariables.NumElements()) && (psDest->asVariables.Get(uiAt)->iDeclaredToken < sVariable.iDeclaredToken); uiAt++)
			{
			}
			psDest->asVariables.InsertAt(&sVariable, uiAt);
		}
		else
		{
			sVariable.Kill();
		}
	}
	aszKeys.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What each variable is to the new function (see the class's comment).
// It is refused if more than one thing would have to be returned and one
// of them is declared in the selection.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::ChooseRoles(SIDEExtraction* psDest)
{
	SIDEExtractVariable*	psVariable;
	size					uiOutputs;
	size					i;
	bool					bOutput;

	uiOutputs = 0;
	for (i = 0; i < psDest->asVariables.NumElements(); i++)
	{
		psVariable = psDest->asVariables.Get(i);
		uiOutputs += ((psVariable->bWrittenInside || psVariable->bDeclaredInside) && psVariable->bUsedAfter) ? 1 : 0;
	}
	for (i = 0; i < psDest->asVariables.NumElements(); i++)
	{
		psVariable = psDest->asVariables.Get(i);
		bOutput = (psVariable->bWrittenInside || psVariable->bDeclaredInside) && psVariable->bUsedAfter;
		if (bOutput && (uiOutputs == 1) && !psVariable->bObject && !psVariable->bArray && !psDest->bExpression && !psDest->bReturns)
		{
			psVariable->iRole = IDE_EXTRACT_RETURNED;
		}
		else if (bOutput && psVariable->bDeclaredInside)
		{
			psDest->bPossible = false;
			psDest->szWhyNot.Set(psVariable->szName);
			psDest->szWhyNot.Append(" is declared in the selection and used after it, and it isn't the one thing the new function could return");
			return;
		}
		else if (bOutput)
		{
			psVariable->iRole = IDE_EXTRACT_REFERENCE;
		}
		else if (psVariable->bDeclaredInside)
		{
			psVariable->iRole = IDE_EXTRACT_NONE;
		}
		else if (psVariable->bReadInside || psVariable->bArray)
		{
			psVariable->iRole = psVariable->bObject ? IDE_EXTRACT_REFERENCE : IDE_EXTRACT_VALUE;
		}
		else if (psVariable->bObject)
		{
			//An object the selection only changes: it may be read through a call the syntax can't see into.
			psVariable->iRole = (psVariable->bUsedBefore || psVariable->bUsedAfter || !psVariable->bAlone) ? IDE_EXTRACT_REFERENCE : IDE_EXTRACT_MOVED;
		}
		else
		{
			psVariable->iRole = (!psVariable->bUsedBefore && !psVariable->bUsedAfter && psVariable->bAlone) ? IDE_EXTRACT_MOVED : IDE_EXTRACT_DECLARED;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What Extract Method has to work with, for the active editor's
// selection.  False if nothing is selected; psDest->bPossible says
// whether it can be a function, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::FindExtraction(SIDEExtraction* psDest)
{
	CIDERefactorUnit				cUnit;
	Ptr<CIDEEditorTab>				pTab;
	Ptr<CTextArea>					pEditor;
	SIDEIntroduction				sIntroduction;
	CCPPSyntax*						pcStart;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcParent;
	CCPPSyntaxCompoundStatement*	pcBlock;
	CCPPSyntaxCompoundStatement*	pcCandidate;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CIDEExitFinder					cExits;
	SPreprocessedToken*				psSource;
	SCPPSyntaxRange					sRange;
	CChars							szName;
	char*							szText;
	size							uiStart;
	size							uiEnd;
	size							uiLine;
	size							uiEndLine;
	size							uiFirstStatement;
	size							uiLastStatement;
	size							i;
	size							j;
	int32							iFirst;
	int32							iLast;
	int32							iToken;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetEditor()->GetSelection(&uiStart, &uiEnd))
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	psDest->szFileName.Set(pTab->GetFileName());
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cUnit.Init();
	if (!cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), pTab->GetFileName(), true) || !cUnit.IsChecked())
	{
		cUnit.Kill();
		return Refuse(psDest, "the file doesn't compile as it is: the compiler has to say how the selection uses its variables");
	}
	szText = pEditor->GetText();
	uiLine = pEditor->FindLine(uiStart);
	uiEndLine = pEditor->FindLine(uiEnd);
	if (!cUnit.FindTokensIn((int)uiLine, (int)(uiStart - pEditor->GetLineStart(uiLine)), (int)uiEndLine, (int)(uiEnd - pEditor->GetLineStart(uiEndLine)), &iFirst, &iLast))
	{
		cUnit.Kill();
		return Refuse(psDest, "nothing of the file's is selected");
	}
	pcStart = cUnit.FindNodeOfTokens(iFirst, iFirst);
	pcFunction = pcStart ? (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcStart, CPPS_FunctionDefinition) : NULL;
	if (!pcFunction || !pcFunction->mpcBody || (pcFunction->mpcBody->GetKind() != CPPS_CompoundStatement) || (iFirst <= pcFunction->mpcBody->miFirstToken) || (iLast >= pcFunction->mpcBody->miLastToken))
	{
		cUnit.Kill();
		return Refuse(psDest, "the selection isn't in a function's body");
	}

	//An expression, or statements of one block.
	pcNode = cUnit.FindNodeOfTokens(iFirst, iLast);
	while (pcNode && !cUnit.IsExpression(pcNode) && pcNode->mpcParent && (pcNode->mpcParent->miFirstToken == iFirst) && (pcNode->mpcParent->miLastToken == iLast))
	{
		pcNode = pcNode->mpcParent;
	}
	pcBlock = NULL;
	uiFirstStatement = 0;
	uiLastStatement = 0;
	psDest->bPossible = true;
	if (pcNode && cUnit.IsExpression(pcNode) && (pcNode->miFirstToken == iFirst) && (pcNode->miLastToken == iLast))
	{
		psDest->bExpression = true;
		sIntroduction.Init();
		if (!mpcContent->GetIntroduceVariable()->FindIntroduction(&sIntroduction) || !sIntroduction.bPossible)
		{
			Refuse(psDest, sIntroduction.szWhyNot.Empty() ? "the expression's type can't be had" : sIntroduction.szWhyNot.Text());
		}
		else
		{
			psDest->szType.Set(sIntroduction.szType);
			cUnit.GetRange(pcNode, &sRange);
			psDest->uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
			psDest->uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		}
		sIntroduction.Kill();
	}
	else
	{
		for (pcParent = pcStart; pcParent && !pcBlock && (pcParent != pcFunction->mpcParent); pcParent = pcParent->mpcParent)
		{
			if (pcParent->GetKind() != CPPS_CompoundStatement)
			{
				continue;
			}
			pcCandidate = (CCPPSyntaxCompoundStatement*)pcParent;
			for (i = 0; !pcBlock && (i < pcCandidate->macpStatements.NumElements()); i++)
			{
				if (pcCandidate->macpStatements.GetPtr(i)->miFirstToken != iFirst)
				{
					continue;
				}
				for (j = i; !pcBlock && (j < pcCandidate->macpStatements.NumElements()); j++)
				{
					if (pcCandidate->macpStatements.GetPtr(j)->miLastToken == iLast)
					{
						pcBlock = pcCandidate;
						uiFirstStatement = i;
						uiLastStatement = j;
					}
				}
			}
		}
		if (!pcBlock || !cUnit.GetTokensRange(iFirst, iLast, &sRange))
		{
			Refuse(psDest, "the selection isn't an expression, or whole statements of one block");
		}
		else
		{
			psDest->uiStart = FindIDETextOffset(szText, sRange.iLine, 0);
			psDest->uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
			psDest->szIndent.AppendSubString(szText, psDest->uiStart, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));

			//How it leaves.
			cExits.muiReturns = 0;
			cExits.mbLeaves = false;
			cExits.miLoops = 0;
			cExits.miSwitches = 0;
			for (i = uiFirstStatement; i <= uiLastStatement; i++)
			{
				cExits.Look(pcBlock->macpStatements.GetPtr(i));
			}
			if (cExits.mbLeaves)
			{
				Refuse(psDest, "it leaves by a break, a continue or a goto that a function of it couldn't make (or has a label)");
			}
			else if ((cExits.muiReturns > 0) && (pcBlock->macpStatements.GetPtr(uiLastStatement)->GetKind() != CPPS_ReturnStatement))
			{
				Refuse(psDest, "it returns from the function part of the way through: a selection that returns must end with a return");
			}
			psDest->bReturns = (cExits.muiReturns > 0);
		}
	}

	if (psDest->bPossible && !FindFunction(&cUnit, pcFunction, szText, psDest))
	{
		Refuse(psDest, "the function it is in can't be read (a macro makes it)");
	}
	if (psDest->bPossible)
	{
		FindVariables(&cUnit, pcFunction, pcBlock, iFirst, iLast, szText, psDest);
		ChooseRoles(psDest);
	}
	if (psDest->bPossible)
	{
		for (iToken = 0; (size)iToken < cUnit.GetTokeniser()->NumTokens(); iToken++)
		{
			psSource = cUnit.GetTokeniser()->GetSource(cUnit.GetTokeniser()->Get((size)iToken));
			if ((psSource->pcFile == cUnit.GetFile()) && cUnit.GetTokeniser()->Get((size)iToken)->IsIdentifier())
			{
				szName.Init();
				szName.Append(psSource->szText, psSource->uiLength);
				psDest->aszUsed.AddIfUnique(szName.Text());
				szName.Kill();
			}
		}
	}
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Why the new function can't have a name, NULL if it can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEExtractMethod::GetNameProblem(SIDEExtraction* psExtraction, const char* szName)
{
	if (!IsIDEIdentifier(szName))
	{
		return "It isn't a name.";
	}
	if (psExtraction->aszUsed.Contains(szName))
	{
		return "The file uses that name already.";
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// What the new function returns: the expression's type, what the
// function it is in returns (a selection that returns), the returned
// variable's type, or void.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::AppendReturnType(SIDEExtraction* psExtraction, CChars* pszDest)
{
	size	i;

	if (psExtraction->bExpression)
	{
		pszDest->Append(psExtraction->szType);
		return;
	}
	if (psExtraction->bReturns)
	{
		pszDest->Append(psExtraction->szFunctionReturn.Empty() ? "void" : psExtraction->szFunctionReturn.Text());
		return;
	}
	for (i = 0; i < psExtraction->asVariables.NumElements(); i++)
	{
		if (psExtraction->asVariables.Get(i)->iRole == IDE_EXTRACT_RETURNED)
		{
			pszDest->Append(psExtraction->asVariables.Get(i)->szBefore);
			return;
		}
	}
	pszDest->Append("void");
}


//////////////////////////////////////////////////////////////////////////
//
// The new function's parameters as it declares them ("int iA, CChars&
// sz"; void for none), or, bNamesOnly, as a call gives them ("iA, sz").
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::AppendParameters(SIDEExtraction* psExtraction, bool bNamesOnly, CChars* pszDest)
{
	SIDEExtractVariable*	psVariable;
	size					uiParameters;
	size					i;

	uiParameters = 0;
	for (i = 0; i < psExtraction->asVariables.NumElements(); i++)
	{
		psVariable = psExtraction->asVariables.Get(i);
		if ((psVariable->iRole != IDE_EXTRACT_VALUE) && (psVariable->iRole != IDE_EXTRACT_REFERENCE) && !((psVariable->iRole == IDE_EXTRACT_RETURNED) && psVariable->bReadInside && !psVariable->bDeclaredInside))
		{
			continue;
		}
		pszDest->Append((uiParameters > 0) ? ", " : "");
		if (!bNamesOnly)
		{
			pszDest->Append(psVariable->szBefore);
			pszDest->Append(((psVariable->iRole == IDE_EXTRACT_REFERENCE) && !psVariable->bArray) ? "& " : " ");
		}
		pszDest->Append(psVariable->szName);
		if (!bNamesOnly)
		{
			pszDest->Append(psVariable->szAfter);
		}
		uiParameters++;
	}
	if ((uiParameters == 0) && !bNamesOnly)
	{
		pszDest->Append("void");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The new function as it is declared: "int Sum(int iA, int iB) const",
// with its class's name before its own if bQualified.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::AppendSignature(SIDEExtraction* psExtraction, const char* szName, bool bQualified, CChars* pszDest)
{
	AppendReturnType(psExtraction, pszDest);
	pszDest->Append(' ');
	if (bQualified)
	{
		pszDest->Append(psExtraction->szQualifier);
	}
	pszDest->Append(szName);
	pszDest->Append('(');
	AppendParameters(psExtraction, false, pszDest);
	pszDest->Append(')');
	pszDest->Append(psExtraction->sFunction.szQualifiers);
}


//////////////////////////////////////////////////////////////////////////
//
// A call of the new function: "Sum(iA, iB)".
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractMethod::AppendCall(SIDEExtraction* psExtraction, const char* szName, CChars* pszDest)
{
	pszDest->Append(szName);
	pszDest->Append(psExtraction->szTemplateArguments);
	pszDest->Append('(');
	AppendParameters(psExtraction, true, pszDest);
	pszDest->Append(')');
}


//////////////////////////////////////////////////////////////////////////
//
// The selection made a function of a name, and a call of it written
// where it was: see the class's comment.  One undo across the files.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::Extract(SIDEExtraction* psExtraction, const char* szName, CChars* pszStatus)
{
	CIDEFileEdits			cFiles;
	CIDETextEdits*			pcEdits;
	SIDEExtractVariable*	psVariable;
	CArrayChars				aszLines;
	CChars					szDefinition;
	CChars					szBody;
	CChars					szSelected;
	CChars					szCall;
	CChars					szLine;
	CChars					szMemberIndent;
	CChars					szMessage;
	const char*				szProblem;
	char*					szText;
	size					uiDeclared;
	size					uiStart;
	size					uiEnd;
	size					uiAt;
	size					uiColumn;
	size					uiNameColumn;
	size					i;
	bool					bMember;

	szProblem = psExtraction->bPossible ? GetNameProblem(psExtraction, szName) : "It can't be extracted.";
	if (szProblem)
	{
		szMessage.Init("Extract Method: ");
		szMessage.Append(szProblem);
		if (pszStatus)
		{
			pszStatus->Set(szMessage);
		}
		else
		{
			mpcContent->ShowStatus(szMessage.Text());
		}
		szMessage.Kill();
		return false;
	}
	cFiles.Init(mpcContent);
	szText = cFiles.GetText(psExtraction->szFileName.Text());
	pcEdits = cFiles.GetEdits(psExtraction->szFileName.Text());
	bMember = !psExtraction->szClassKey.Empty();

	//Its body: its own variables' declarations, the statements one tab in, and what it returns.
	szBody.Init();
	uiDeclared = 0;
	for (i = 0; i < psExtraction->asVariables.NumElements(); i++)
	{
		psVariable = psExtraction->asVariables.Get(i);
		if ((psVariable->iRole == IDE_EXTRACT_MOVED) || (psVariable->iRole == IDE_EXTRACT_DECLARED) || ((psVariable->iRole == IDE_EXTRACT_RETURNED) && !psVariable->bReadInside && !psVariable->bDeclaredInside))
		{
			szBody.Append('\t')->Append(psVariable->szBefore)->Append('\t')->Append(psVariable->szName)->Append(psVariable->szAfter)->Append(";\n");
			uiDeclared++;
		}
		if (psVariable->iRole == IDE_EXTRACT_MOVED)
		{
			uiStart = psVariable->uiDeclaration;
			uiEnd = psVariable->uiDeclarationEnd;
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			pcEdits->Replace(uiStart, uiEnd, "");
		}
	}
	if (uiDeclared > 0)
	{
		szBody.Append('\n');
	}
	szSelected.Init();
	szSelected.AppendSubString(szText, psExtraction->uiStart, psExtraction->uiEnd);
	if (psExtraction->bExpression)
	{
		szBody.Append("\treturn ")->Append(szSelected)->Append(";\n");
	}
	else
	{
		szSelected.Replace("\r", "");
		aszLines.Init();
		szSelected.Split(&aszLines, '\n');
		for (i = 0; i < aszLines.NumElements(); i++)
		{
			if (aszLines.Get(i)->StartsWith(psExtraction->szIndent.Text()))
			{
				szBody.Append('\t');
				szBody.Append(&aszLines.GetText(i)[psExtraction->szIndent.Length()]);
			}
			else
			{
				szLine.Init(aszLines.GetText(i));
				szLine.StripWhitespace();
				szBody.Append(szLine.Empty() ? "" : "\t");
				szBody.Append(szLine);
				szLine.Kill();
			}
			szBody.Append('\n');
		}
		aszLines.Kill();
		for (i = 0; i < psExtraction->asVariables.NumElements(); i++)
		{
			if (psExtraction->asVariables.Get(i)->iRole == IDE_EXTRACT_RETURNED)
			{
				szBody.Append("\treturn ")->Append(psExtraction->asVariables.Get(i)->szName)->Append(";\n");
			}
		}
	}

	//Its definition: in the class if the function it came from is defined there, otherwise outside with what that
	//one writes before its name.
	szDefinition.Init();
	szMemberIndent.Init();
	uiAt = 0;
	if (psExtraction->sFunction.bInClass)
	{
		uiAt = FindIDETextOffset(szText, psExtraction->sFunction.sAll.iLine, 0);
		for (i = uiAt; (szText[i] == ' ') || (szText[i] == '\t'); i++)
		{
		}
		szMemberIndent.AppendSubString(szText, uiAt, i);
	}
	else
	{
		szDefinition.Append(IDE_EXTRACT_BANNER);
		if (!psExtraction->sFunction.szOwnTemplate.Empty())
		{
			szDefinition.Append(psExtraction->sFunction.szOwnTemplate)->Append('\n');
		}
	}
	if (psExtraction->sFunction.bInClass)
	{
		//In the class, as its members are written there: its name in the column the function it came from has its
		//name in, and one statement on its line (CIDEDefinitions::AppendBodyInClass).
		szDefinition.Append(szMemberIndent);
		szLine.Init();
		AppendReturnType(psExtraction, &szLine);
		uiColumn = FindIDEVisualColumn(szMemberIndent.Text(), 0, szMemberIndent.Length(), 4) + szLine.Length();
		uiNameColumn = FindIDEVisualColumn(szText, uiAt, FindIDETextOffset(szText, psExtraction->sFunction.sName.iLine, psExtraction->sFunction.sName.iColumn), 4);
		if (uiNameColumn > uiColumn)
		{
			AppendIDETabsTo(&szLine, uiColumn, uiNameColumn, 4);
		}
		else
		{
			szLine.Append('\t');
		}
		szDefinition.Append(szLine);
		szLine.Kill();
		szDefinition.Append(szName);
		szDefinition.Append('(');
		AppendParameters(psExtraction, false, &szDefinition);
		szDefinition.Append(')');
		szDefinition.Append(psExtraction->sFunction.szQualifiers);
		szLine.Init("{\n");
		szLine.Append(szBody);
		szLine.Append("}");
		mpcContent->GetDefinitions()->AppendBodyInClass(szLine.Text(), szMemberIndent.Text(), &szDefinition);
		szLine.Kill();
		szDefinition.Append('\n');
	}
	else
	{
		AppendSignature(psExtraction, szName, true, &szDefinition);
		szDefinition.Append("\n{\n");
		szDefinition.Append(szBody);
		szDefinition.Append("}\n");
	}

	uiStart = FindIDETextOffset(szText, psExtraction->sFunction.sAll.iLine, psExtraction->sFunction.sAll.iColumn);
	uiEnd = FindIDETextOffset(szText, psExtraction->sFunction.sAll.iEndLine, psExtraction->sFunction.sAll.iEndColumn);
	for (; (szText[uiEnd] != '\0') && (szText[uiEnd] != '\n'); uiEnd++)
	{
	}
	uiEnd = (szText[uiEnd] == '\n') ? uiEnd + 1 : uiEnd;
	if (psExtraction->sFunction.bInClass)
	{
		pcEdits->Insert(uiEnd, szDefinition.Text());
	}
	else if (psExtraction->bBefore)
	{
		szDefinition.Append("\n\n");
		pcEdits->Insert(FindIDECommentStart(szText, uiStart), szDefinition.Text());
	}
	else
	{
		szDefinition.Insert(0, "\n\n");
		pcEdits->Insert(uiEnd, szDefinition.Text());
	}

	//Its declaration in the class, where the function it came from is defined outside it.
	if (bMember && !psExtraction->sFunction.bInClass)
	{
		szLine.Init("\t");
		AppendReturnType(psExtraction, &szLine);
		szLine.Append('\t');
		szLine.Append(szName);
		szLine.Append('(');
		AppendParameters(psExtraction, false, &szLine);
		szLine.Append(')');
		szLine.Append(psExtraction->sFunction.szQualifiers);
		szLine.Append(";\n");
		mpcContent->GetMoveMembers()->AddMember(psExtraction->szClassKey.Text(), IDE_ACCESS_PROTECTED, szLine.Text(), &cFiles, true);
		szLine.Kill();
	}

	//The call, where the selection was.
	szCall.Init();
	if (psExtraction->bExpression)
	{
		AppendCall(psExtraction, szName, &szCall);
	}
	else
	{
		szCall.Append(psExtraction->szIndent);
		for (i = 0; i < psExtraction->asVariables.NumElements(); i++)
		{
			psVariable = psExtraction->asVariables.Get(i);
			if (psVariable->iRole == IDE_EXTRACT_RETURNED)
			{
				if (psVariable->bDeclaredInside)
				{
					szCall.Append(psVariable->szBefore)->Append(' ');
				}
				szCall.Append(psVariable->szName)->Append(" = ");
			}
		}
		if (psExtraction->bReturns && !psExtraction->szFunctionReturn.Empty() && !psExtraction->szFunctionReturn.Equals("void"))
		{
			szCall.Append("return ");
		}
		AppendCall(psExtraction, szName, &szCall);
		szCall.Append(';');
		if (psExtraction->bReturns && (psExtraction->szFunctionReturn.Empty() || psExtraction->szFunctionReturn.Equals("void")))
		{
			szCall.Append('\n')->Append(psExtraction->szIndent)->Append("return;");
		}
	}
	pcEdits->Replace(psExtraction->uiStart, psExtraction->uiEnd, szCall.Text());
	cFiles.Apply();

	szMessage.Init("Extracted ");
	AppendSignature(psExtraction, szName, true, &szMessage);
	szMessage.Append(".");
	if (pszStatus)
	{
		pszStatus->Set(szMessage);
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	szCall.Kill();
	szMemberIndent.Kill();
	szDefinition.Kill();
	szSelected.Kill();
	szBody.Kill();
	cFiles.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks: the function it would be (named X),
// the call, and each variable's part.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEExtractMethod::PrintExtraction(SIDEExtraction* psExtraction, CChars* pszDest)
{
	static const char*		aszRoles[] = { "its own", "a parameter", "a reference parameter", "returned", "moved in", "declared again" };
	SIDEExtractVariable*	psVariable;
	size					i;

	pszDest->Append("Extract Method: ");
	if (!psExtraction->bPossible)
	{
		pszDest->Append("can't be: ")->Append(psExtraction->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	pszDest->Append(psExtraction->bExpression ? "an expression, [" : (psExtraction->bReturns ? "statements that return, [" : "statements, ["));
	AppendSignature(psExtraction, "X", true, pszDest);
	pszDest->Append("], called [");
	AppendCall(psExtraction, "X", pszDest);
	pszDest->Append("]\n");
	for (i = 0; i < psExtraction->asVariables.NumElements(); i++)
	{
		psVariable = psExtraction->asVariables.Get(i);
		pszDest->Append("  ")->Append(psVariable->szName)->Append(": ")->Append(aszRoles[psVariable->iRole])->Append('\n');
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::ApproveName(void)
{
	const char*	szProblem;

	szProblem = GetNameProblem(mpsShown, mpNameField->GetText());
	mpError->SetText(szProblem ? szProblem : "");
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Extract Method for the selection: its dialog (the name, and
// the function as it will be declared), then the extraction.  False if
// nothing changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractMethod::ShowExtract(void)
{
	SIDEExtraction	sExtraction;
	CChars			szText;
	bool			bResult;

	sExtraction.Init();
	if (!FindExtraction(&sExtraction))
	{
		mpcContent->ShowStatus("Select statements or an expression to extract a method from.");
		sExtraction.Kill();
		return false;
	}
	if (!sExtraction.bPossible)
	{
		szText.Init("Can't extract a method: ");
		szText.Append(sExtraction.szWhyNot);
		szText.Append(".");
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
		sExtraction.Kill();
		return false;
	}

	mpsShown = &sExtraction;
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Extract Method");
	mpNameField = mpDialog->AddTextField("Name:", "");
	szText.Init();
	AppendSignature(&sExtraction, "...", true, &szText);
	mpSignature = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), szText.Text());
	mpSignature->SetAlignment(CSH_Left, CSV_Center);
	mpDialog->AddEditor("Signature:", mpSignature);
	szText.Kill();
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEExtractMethod::ApproveName);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(640);
	szText.Init(bResult ? mpNameField->GetText() : "");
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpNameField = NULL;
	mpSignature = NULL;
	mpError = NULL;
	mpDialog = NULL;
	mpsShown = NULL;
	bResult = bResult && Extract(&sExtraction, szText.Text());
	szText.Kill();
	sExtraction.Kill();
	return bResult;
}
