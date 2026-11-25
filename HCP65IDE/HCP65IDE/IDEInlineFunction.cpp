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
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPP65Type.h"
#include "CppCompilerLib/CPPIndex.h"
#include "CppCompilerLib/CPPSyntaxMemberExpression.h"
#include "WindowLib/ComboBox.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEDefinitions.h"
#include "IDEMoveMembers.h"
#include "IDEInlineVariable.h"
#include "IDEIntroduceVariable.h"
#include "IDEInlineFunction.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEInlineCall::Init(void)
{
	szFileName.Init();
	iLine = 0;
	uiStart = 0;
	uiEnd = 0;
	aszArguments.Init();
	aiEffects.Init();
	szObject.Init();
	szThis.Init("this");
	bObjectEffects = false;
	bOutside = false;
	iContext = IDE_CALL_IN_EXPRESSION;
	uiStatement = 0;
	uiStatementEnd = 0;
	szIndent.Init();
	szAssigned.Init();
	bWrapped = false;
	uiDeclarations = 0;
	bDeclarations = false;
	aszNames.Init();
	aszInstance.Init();
	bSelected = false;
	szVariable.Init();
	aszOwnNames.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEInlineCall::Kill(void)
{
	aszOwnNames.Kill();
	szVariable.Kill();
	aszInstance.Kill();
	aszNames.Kill();
	szAssigned.Kill();
	szIndent.Kill();
	szThis.Kill();
	szObject.Kill();
	aiEffects.Kill();
	aszArguments.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFunctionInlining::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	sSignature.Init();
	szFileName.Init();
	szClassKey.Init();
	szClassName.Init();
	bExpression = false;
	uiValue = 0;
	uiValueEnd = 0;
	bValueEffects = false;
	uiStatements = 0;
	uiStatementsEnd = 0;
	szBodyIndent.Init();
	aszDeclarations.Init();
	aszDeclared.Init();
	asReplaces.Init();
	aiParameterUses.Init();
	aszTemplateParameters.Init();
	szNotPublic.Init();
	bNamesMembers = false;
	apsCalls.Init();
	bSelectedCall = false;
	aszOwnParameters.Init();
	aszOwnTypes.Init();
	szReturnType.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFunctionInlining::Kill(void)
{
	size	i;

	for (i = 0; i < apsCalls.NumElements(); i++)
	{
		apsCalls.GetPtr(i)->Kill();
		free(apsCalls.GetPtr(i));
	}
	apsCalls.Kill();
	szReturnType.Kill();
	aszOwnTypes.Kill();
	aszOwnParameters.Kill();
	szNotPublic.Kill();
	aszTemplateParameters.Kill();
	aiParameterUses.Kill();
	asReplaces.Kill();
	aszDeclared.Kill();
	aszDeclarations.Kill();
	szBodyIndent.Kill();
	szClassName.Kill();
	szClassKey.Kill();
	szFileName.Kill();
	sSignature.Kill();
	szWhyNot.Kill();
}


static const char*	gaszIDEInlineSpecifiers[] = { "static ", "inline ", "virtual ", "extern ", "static\t", "inline\t", "virtual\t", "extern\t" };


//////////////////////////////////////////////////////////////////////////
//
// Whether what is written is more than one thing: it has an operator
// outside any brackets ("iA + 1", "-iA", "a ? b : c"; not "pc->mi",
// "Get(1, 2)", "asz[i]").
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDECompoundText(const char* szText)
{
	size	i;
	int		iDepth;
	char	c;

	iDepth = 0;
	for (i = 0; szText[i] != '\0'; i++)
	{
		c = szText[i];
		if ((c == '"') || (c == '\''))
		{
			for (i++; (szText[i] != '\0') && (szText[i] != c); i++)
			{
				i += (szText[i] == '\\') ? 1 : 0;
			}
			if (szText[i] == '\0')
			{
				break;
			}
		}
		else if ((c == '(') || (c == '['))
		{
			iDepth++;
		}
		else if ((c == ')') || (c == ']'))
		{
			iDepth--;
		}
		else if ((c == '-') && (szText[i + 1] == '>'))
		{
			i++;
		}
		else if ((c == ':') && (szText[i + 1] == ':'))
		{
			i++;
		}
		else if ((iDepth == 0) && (strchr("+-*/%<>=&|^?:,!~", c) != NULL))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The statement a call inside a bigger expression can be taken out of,
// its statements written before it: one that always makes the call, and
// once.  NULL if there is none: the call is after && or ||, in an arm of
// ?:, after a comma, or in something that isn't an expression's
// statement, a return or an if's condition.
//
//////////////////////////////////////////////////////////////////////////
static CCPPSyntax* FindIDEHoistStatement(CIDERefactorUnit* pcUnit, CCPPSyntax* pcCall)
{
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcParent;
	CCPPSyntaxBinaryExpression*	pcBinary;

	pcNode = pcCall;
	for (pcParent = pcNode->mpcParent; pcParent && pcUnit->IsExpression(pcParent); pcParent = pcParent->mpcParent)
	{
		if (pcParent->GetKind() == CPPS_BinaryExpression)
		{
			pcBinary = (CCPPSyntaxBinaryExpression*)pcParent;
			if (((pcBinary->meOperator == CT_LogicalAnd) || (pcBinary->meOperator == CT_LogicalOr) || (pcBinary->meOperator == CT_Comma)) && (pcBinary->mpcRight == pcNode))
			{
				return NULL;
			}
		}
		else if ((pcParent->GetKind() == CPPS_ConditionalExpression) && (((CCPPSyntaxConditionalExpression*)pcParent)->mpcCondition != pcNode))
		{
			return NULL;
		}
		pcNode = pcParent;
	}
	if (pcParent && ((pcParent->GetKind() == CPPS_ExpressionStatement) || (pcParent->GetKind() == CPPS_ReturnStatement) ||
					 ((pcParent->GetKind() == CPPS_IfStatement) && (((CCPPSyntaxIfStatement*)pcParent)->mpcCondition == pcNode))))
	{
		return pcParent;
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A file's text as its editor has it, or the disk (pcFile keeps it).
//
//////////////////////////////////////////////////////////////////////////
static const char* GetIDEInlineText(CIDEContent* pcContent, const char* szFileName, CTextFile* pcFile)
{
	CIDEFileTexts*	pcTexts;
	const char*		szText;
	size			uiLength;

	pcTexts = &pcContent->GetFindInFiles();
	szText = NULL;
	return pcTexts->GetFileText(szFileName, pcFile, &szText, &uiLength) ? szText : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::Save(CObjectWriter* pcFile) { return false; }
bool CIDEInlineFunction::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::Refuse(SIDEFunctionInlining* psDest, const char* szWhyNot)
{
	psDest->bPossible = false;
	psDest->szWhyNot.Set(szWhyNot);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::AddReplace(SIDEFunctionInlining* psDest, size uiStart, size uiEnd, int iKind, int iIndex, bool bOperand)
{
	SIDEInlineReplace	sReplace;
	size				uiAt;

	sReplace.uiStart = uiStart;
	sReplace.uiEnd = uiEnd;
	sReplace.iKind = iKind;
	sReplace.iIndex = iIndex;
	sReplace.bOperand = bOperand;
	for (uiAt = 0; (uiAt < psDest->asReplaces.NumElements()) && (psDest->asReplaces.Get(uiAt)->uiStart <= uiStart); uiAt++)
	{
		if ((psDest->asReplaces.Get(uiAt)->uiStart == uiStart) && (psDest->asReplaces.Get(uiAt)->uiEnd == uiEnd))
		{
			return;
		}
	}
	psDest->asReplaces.InsertAt(&sReplace, uiAt);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the function's definition is written with an exception
// specification that stops what its body throws (noexcept, throw(),
// throw(A, B)): Microsoft's throw(...) stops nothing.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::IsExceptionSpecified(CCPPSyntaxFunctionDefinition* pcFunction)
{
	CCPPSyntaxFunctionSuffix*			pcSuffix;
	CCPPSyntaxExceptionSpecification*	pcSpecification;

	pcSuffix = pcFunction->mpcDeclarator ? pcFunction->mpcDeclarator->GetFunctionSuffix() : NULL;
	if (!pcSuffix || !pcSuffix->mpcExceptionSpecification || (pcSuffix->mpcExceptionSpecification->GetKind() != CPPS_ExceptionSpecification))
	{
		return false;
	}
	pcSpecification = (CCPPSyntaxExceptionSpecification*)pcSuffix->mpcExceptionSpecification;
	return !pcSpecification->mbAnything;
}


//////////////////////////////////////////////////////////////////////////
//
// The function's body, as its definition writes it: an expression it
// returns and nothing else, or its own variables' declarations, its
// statements, and what its last statement returns.  False, with why in
// psDest, if it can't be inlined.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::ReadBody(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, const char* szText, SIDEFunctionInlining* psDest, CCPPSyntax** ppcValue)
{
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CCPPSyntaxCompoundStatement*	pcBody;
	CCPPSyntaxSimpleDeclaration*	pcDeclaration;
	CCPPSyntaxDeclarator*			pcDeclarator;
	CCPPSyntax*						pcStatement;
	CCPPSyntax*						pcNode;
	CIDEExitFinder					cExits;
	CIDEEffectFinder				cEffects;
	CIDEThrowFinder					cThrows;
	SCPPSyntaxRange					sRange;
	CChars							szLine;
	size							uiStatements;
	size							uiFirst;
	size							uiLast;
	size							i;
	size							j;
	bool							bPlain;

	*ppcValue = NULL;
	pcFunction = (CCPPSyntaxFunctionDefinition*)pcUnit->FindAncestor(pcUnit->FindNodeAt(psSite->iLine, psSite->iColumn), CPPS_FunctionDefinition);
	if (pcFunction && pcFunction->mpcBody && (pcFunction->mpcBody->GetKind() == CPPS_TryStatement))
	{
		return !Refuse(psDest, "its body is a function-try-block: its handlers can't be written where it is called");
	}
	if (!pcFunction || !pcFunction->mpcBody || (pcFunction->mpcBody->GetKind() != CPPS_CompoundStatement))
	{
		return !Refuse(psDest, "its body can't be read (a macro makes it)");
	}
	if (pcUnit->IsExceptions() && IsExceptionSpecified(pcFunction))
	{
		cThrows.mbMayThrow = false;
		cThrows.Look(pcFunction->mpcBody);
		if (cThrows.mbMayThrow)
		{
			return !Refuse(psDest, "it has an exception specification (noexcept or throw()) and its body may throw: inlined, an exception would no longer stop at it");
		}
	}
	if (pcFunction->macpMemberInitializers.NumElements() > 0)
	{
		return !Refuse(psDest, "it is a constructor");
	}
	pcBody = (CCPPSyntaxCompoundStatement*)pcFunction->mpcBody;
	uiStatements = pcBody->macpStatements.NumElements();

	//What it returns, as a variable of it is declared: without static and the like, and a const that isn't a pointer's.
	if (psSite->bReturn)
	{
		szLine.Init();
		AppendIDERangeText(szText, &psSite->sReturn, &szLine);
		szLine.StripWhitespace();
		for (i = 0; i < sizeof(gaszIDEInlineSpecifiers) / sizeof(char*); i++)
		{
			if (szLine.StartsWith(gaszIDEInlineSpecifiers[i]))
			{
				szLine.RemoveFromStart(strlen(gaszIDEInlineSpecifiers[i]));
				szLine.StripWhitespace();
				i = (size)-1;
			}
		}
		if (szLine.StartsWith("const ") && !szLine.Contains("*"))
		{
			szLine.RemoveFromStart(6);
			szLine.StripWhitespace();
		}
		if (!szLine.Empty() && (szLine.Text()[szLine.Length() - 1] != '&') && !szLine.Equals("void"))
		{
			psDest->szReturnType.Set(szLine);
		}
		szLine.Kill();
	}
	cExits.muiReturns = 0;
	cExits.mbLeaves = false;
	cExits.miLoops = 0;
	cExits.miSwitches = 0;
	for (i = 0; i < uiStatements; i++)
	{
		cExits.Look(pcBody->macpStatements.GetPtr(i));
	}
	if (cExits.mbLeaves)
	{
		return !Refuse(psDest, "its body has a goto or a label");
	}
	pcStatement = (uiStatements > 0) ? pcBody->macpStatements.GetPtr(uiStatements - 1) : NULL;
	if ((cExits.muiReturns > 1) || ((cExits.muiReturns == 1) && (pcStatement->GetKind() != CPPS_ReturnStatement)))
	{
		return !Refuse(psDest, "it returns part of the way through its body: only a function that returns once, at its end, is inlined");
	}

	//What it returns.
	uiLast = uiStatements;
	if (pcStatement && (pcStatement->GetKind() == CPPS_ReturnStatement))
	{
		uiLast = uiStatements - 1;
		*ppcValue = ((CCPPSyntaxReturnStatement*)pcStatement)->mpcValue;
		if (*ppcValue)
		{
			if (!pcUnit->GetRange(*ppcValue, &sRange))
			{
				return !Refuse(psDest, "what it returns can't be read (a macro makes it)");
			}
			psDest->uiValue = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
			psDest->uiValueEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
			cEffects.mbEffects = false;
			cEffects.Look(*ppcValue);
			psDest->bValueEffects = cEffects.mbEffects;
		}
	}
	psDest->bExpression = (uiStatements == 1) && (*ppcValue != NULL);

	//Its own variables' declarations, where its body starts with them and they give no values.
	uiFirst = 0;
	for (i = 0; i < uiLast; i++)
	{
		pcStatement = pcBody->macpStatements.GetPtr(i);
		pcDeclaration = (pcStatement->GetKind() == CPPS_DeclarationStatement) ? (CCPPSyntaxSimpleDeclaration*)((CCPPSyntaxDeclarationStatement*)pcStatement)->mpcDeclaration : NULL;
		bPlain = pcDeclaration && (pcDeclaration->GetKind() == CPPS_SimpleDeclaration) && (pcDeclaration->macpDeclarators.NumElements() > 0) && pcUnit->GetRange(pcStatement, &sRange);
		for (j = 0; bPlain && (j < pcDeclaration->macpDeclarators.NumElements()); j++)
		{
			pcNode = pcDeclaration->macpDeclarators.GetPtr(j);
			bPlain = (pcNode->GetKind() == CPPS_Declarator) || ((pcNode->GetKind() == CPPS_InitDeclarator) && (((CCPPSyntaxInitDeclarator*)pcNode)->meInitializer == CPPI_None));
		}
		if (!bPlain)
		{
			break;
		}
		szLine.Init();
		AppendIDERangeText(szText, &sRange, &szLine);
		szLine.StripWhitespace();
		psDest->aszDeclarations.Add(szLine.Text());
		szLine.Kill();
		for (j = 0; j < pcDeclaration->macpDeclarators.NumElements(); j++)
		{
			pcNode = pcDeclaration->macpDeclarators.GetPtr(j);
			pcDeclarator = (pcNode->GetKind() == CPPS_InitDeclarator) ? ((CCPPSyntaxInitDeclarator*)pcNode)->mpcDeclarator : (CCPPSyntaxDeclarator*)pcNode;
			if (pcDeclarator && pcDeclarator->GetName())
			{
				szLine.Init();
				pcUnit->AppendTokens(pcDeclarator->GetName()->miFirstToken, pcDeclarator->GetName()->miLastToken, &szLine);
				psDest->aszDeclared.Add(szLine.Text());
				szLine.Kill();
			}
		}
		uiFirst = i + 1;
	}

	//Its statements between those and its return.
	if (uiFirst < uiLast)
	{
		if (!pcUnit->GetTokensRange(pcBody->macpStatements.GetPtr(uiFirst)->miFirstToken, pcBody->macpStatements.GetPtr(uiLast - 1)->miLastToken, &sRange))
		{
			return !Refuse(psDest, "its statements can't be read (a macro makes them)");
		}
		psDest->uiStatements = FindIDETextOffset(szText, sRange.iLine, 0);
		psDest->uiStatementsEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		psDest->szBodyIndent.AppendSubString(szText, psDest->uiStatements, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The places in the body that are written differently where it is
// inlined: its parameters, its class's members named alone, this, its
// class's types and constants, and its class template's parameters.  A
// parameter the body changes gets a variable of its own.
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::ReadReplaces(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, const char* szText, SIDEFunctionInlining* psDest)
{
	CIDECodeModel*				pcModel;
	CArrayIDEFileRanges			asRanges;
	CArrayIDELocations			asLocations;
	CArrayChars					aszKeys;
	CArrayChars					aszPieces;
	SIDELocation*				psLocation;
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcUse;
	CCPPSyntax*					pcParent;
	CCPPSyntaxBinaryExpression*	pcBinary;
	SPreprocessedToken*			psSource;
	SCPPSyntaxRange				sRange;
	CChars						szScope;
	CChars						szTemplate;
	CChars						szName;
	size						uiBody;
	size						uiBodyEnd;
	size						uiAt;
	size						uiBefore;
	size						uiUses;
	size						i;
	size						j;
	int32						iFirst;
	int32						iLast;
	int32						iToken;
	int							iKind;
	uint8						uiKind;
	ECPPSyntax					eParent;
	SIDEWrittenParameter*		psParameter;
	bool						bOperand;
	bool						bChanged;

	pcModel = mpcContent->GetCodeModel();
	uiBody = FindIDETextOffset(szText, psSite->sBody.iLine, psSite->sBody.iColumn);
	uiBodyEnd = FindIDETextOffset(szText, psSite->sBody.iEndLine, psSite->sBody.iEndColumn);

	//Its parameters.
	for (i = 0; psDest->bPossible && (i < psSite->asParameters.NumElements()); i++)
	{
		uiUses = 0;
		bChanged = false;
		asLocations.Init();
		if (!psSite->asParameters.Get(i)->szKey.Empty())
		{
			pcModel->GetSymbolLocations(psSite->asParameters.Get(i)->szKey.Text(), &asLocations, &uiKind);
		}
		for (j = 0; psDest->bPossible && (j < asLocations.NumElements()); j++)
		{
			psLocation = asLocations.Get(j);
			uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
			if ((psLocation->uiRole != CPPIR_Reference) || (StringInsensitiveCompare(psLocation->szFileName.Text(), psDest->szFileName.Text()) != 0) || (uiAt < uiBody) || (uiAt >= uiBodyEnd))
			{
				continue;
			}
			iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
			pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
			pcUse = pcNode ? pcUnit->FindAncestor(pcNode, CPPS_IdExpression) : NULL;
			if (!pcUse)
			{
				Refuse(psDest, "a macro names one of its parameters");
				break;
			}
			while (pcUse->mpcParent && (pcUse->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
			{
				pcUse = pcUse->mpcParent;
			}
			pcParent = pcUse->mpcParent;
			eParent = pcParent ? pcParent->GetKind() : CPPS_TranslationUnit;
			pcBinary = (eParent == CPPS_BinaryExpression) ? (CCPPSyntaxBinaryExpression*)pcParent : NULL;
			if ((pcUnit->GetPass()->FindChanges(pcUse) != 0) || (pcBinary && (pcBinary->meOperator == CT_Assignment) && (pcBinary->mpcLeft == pcUse)))
			{
				bChanged = true;
			}
			bOperand = (eParent == CPPS_UnaryExpression) || (eParent == CPPS_CastExpression) || (eParent == CPPS_ConditionalExpression) || (eParent == CPPS_MemberExpression) ||
					   (eParent == CPPS_SubscriptExpression) || (pcBinary && !(((pcBinary->meOperator == CT_Assignment) || (pcBinary->meOperator == CT_Comma)) && (pcBinary->mpcRight == pcUse)));
			if (pcUse->miFirstToken != pcNode->miFirstToken)
			{
				//In parentheses of its own already.
				bOperand = false;
			}
			AddReplace(psDest, uiAt, uiAt + (size)psLocation->iLength, IDE_INLINE_PARAMETER, (int)i, bOperand);
			uiUses++;
		}
		KillIDELocations(&asLocations);

		//One the body changes gets a variable of its own where it is inlined, if its type can be written for one; a
		//reference is the argument itself.
		szScope.Init();
		szName.Init();
		if (bChanged && psDest->bPossible)
		{
			psParameter = psSite->asParameters.Get(i);
			uiAt = FindIDETextOffset(szText, psParameter->sAll.iLine, psParameter->sAll.iColumn);
			uiBefore = FindIDETextOffset(szText, psParameter->sName.iLine, psParameter->sName.iColumn);
			if (psParameter->bName && (uiBefore > uiAt))
			{
				szScope.AppendSubString(szText, uiAt, uiBefore);
				szScope.StripWhitespace();
			}
			uiAt = FindIDETextOffset(szText, psParameter->sName.iEndLine, psParameter->sName.iEndColumn);
			uiBefore = psParameter->bDefault ? FindIDETextOffset(szText, psParameter->sDefault.iLine, psParameter->sDefault.iColumn) : FindIDETextOffset(szText, psParameter->sAll.iEndLine, psParameter->sAll.iEndColumn);
			szTemplate.Init();
			if (uiBefore > uiAt)
			{
				szTemplate.AppendSubString(szText, uiAt, uiBefore);
				szTemplate.StripWhitespace();
			}
			GetIDESymbolName(psParameter->szKey.Text(), &szName);
			if (szScope.Empty() || !szTemplate.Empty())
			{
				psDest->bPossible = false;
				psDest->szWhyNot.Set("it changes its parameter ");
				psDest->szWhyNot.Append(szName)->Append(", which a variable can't be made for (an array, or a pointer to a function)");
				szName.Clear();
			}
			else if (szScope.Text()[szScope.Length() - 1] == '&')
			{
				szName.Clear();
			}
			else
			{
				for (j = 0; j < psDest->asReplaces.NumElements(); j++)
				{
					if ((psDest->asReplaces.Get(j)->iKind == IDE_INLINE_PARAMETER) && (psDest->asReplaces.Get(j)->iIndex == (int)i))
					{
						psDest->asReplaces.Get(j)->iKind = IDE_INLINE_OWN_PARAMETER;
					}
				}
				uiUses = 1;
				psDest->bExpression = false;
			}
			szTemplate.Kill();
		}
		psDest->aszOwnParameters.Add(szName.Text());
		psDest->aszOwnTypes.Add(szScope.Text());
		szName.Kill();
		szScope.Kill();
		psDest->aiParameterUses.Add((int)uiUses);
	}
	if (!psDest->bPossible)
	{
		return;
	}

	//Its class's members, types and constants, where they are named alone.
	if (!psDest->szClassKey.Empty())
	{
		asRanges.Init();
		asRanges.Add()->Init(psDest->szFileName.Text(), psSite->sBody.iLine, psSite->sBody.iColumn, psSite->sBody.iEndLine, psSite->sBody.iEndColumn);
		aszKeys.Init();
		pcModel->FindSymbolsUsedIn(&asRanges, &aszKeys);
		KillIDEFileRanges(&asRanges);
		for (i = 0; i < aszKeys.NumElements(); i++)
		{
			szScope.Init();
			GetIDESymbolScope(aszKeys.GetText(i), &szScope);
			asLocations.Init();
			if (szScope.Equals(psDest->szClassKey.Text()) && pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind))
			{
				iKind = ((uiKind == CPPSK_Field) || (uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate) || (uiKind == CPPSK_Variable)) ? IDE_INLINE_MEMBER : IDE_INLINE_SCOPED;
				for (j = 0; j < asLocations.NumElements(); j++)
				{
					psLocation = asLocations.Get(j);
					uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
					if ((psLocation->uiRole != CPPIR_Reference) || (StringInsensitiveCompare(psLocation->szFileName.Text(), psDest->szFileName.Text()) != 0) || (uiAt < uiBody) || (uiAt >= uiBodyEnd))
					{
						continue;
					}
					for (uiBefore = uiAt; (uiBefore > 0) && ((szText[uiBefore - 1] == ' ') || (szText[uiBefore - 1] == '\t')); uiBefore--)
					{
					}
					if ((uiBefore > 0) && ((szText[uiBefore - 1] == '.') || (szText[uiBefore - 1] == '~') || ((uiBefore > 1) && (szText[uiBefore - 1] == '>') && (szText[uiBefore - 2] == '-')) ||
						((uiBefore > 1) && (szText[uiBefore - 1] == ':') && (szText[uiBefore - 2] == ':'))))
					{
						continue;
					}
					AddReplace(psDest, uiAt, uiAt, iKind, 0, false);
					if (iKind == IDE_INLINE_MEMBER)
					{
						psDest->bNamesMembers = true;
						if (psDest->szNotPublic.Empty() && (mpcContent->GetMoveMembers()->FindMemberAccess(aszKeys.GetText(i)) != IDE_ACCESS_PUBLIC))
						{
							GetIDESymbolName(aszKeys.GetText(i), &psDest->szNotPublic);
						}
					}
				}
			}
			KillIDELocations(&asLocations);
			szScope.Kill();
		}
		aszKeys.Kill();
	}

	//this, and its class template's parameters.
	szTemplate.Init(psSite->bInClass ? psSite->szClassTemplate.Text() : psSite->szOwnTemplate.Text());
	if (szTemplate.Contains("<") && !psDest->szClassKey.Empty())
	{
		szName.Init();
		szName.AppendSubString(szTemplate.Text(), szTemplate.Find('<') + 1, szTemplate.Length());
		while (!szName.Empty() && (szName.Text()[szName.Length() - 1] != '>'))
		{
			szName.SetLength(szName.Length() - 1);
		}
		szName.SetLength(szName.Empty() ? 0 : szName.Length() - 1);
		if (szName.Contains("template"))
		{
			//A member template's too: the class's are the first template<...>'s.
			szName.SetLength(szName.Find('>'));
		}
		aszPieces.Init();
		szName.Split(&aszPieces, ',');
		for (i = 0; i < aszPieces.NumElements(); i++)
		{
			//The last name in each piece ("class M", "int N").
			szScope.Init(aszPieces.GetText(i));
			if (szScope.Contains("="))
			{
				szScope.SetLength(szScope.Find('='));
			}
			szScope.StripWhitespace();
			for (j = szScope.Length(); (j > 0) && IsIDENameChar(szScope.Text()[j - 1]); j--)
			{
			}
			psDest->aszTemplateParameters.Add(&szScope.Text()[j]);
			szScope.Kill();
		}
		aszPieces.Kill();
		szName.Kill();
	}
	szTemplate.Kill();
	if (pcUnit->FindTokensIn(psSite->sBody.iLine, psSite->sBody.iColumn, psSite->sBody.iEndLine, psSite->sBody.iEndColumn, &iFirst, &iLast))
	{
		for (iToken = iFirst; iToken <= iLast; iToken++)
		{
			iKind = pcUnit->IsToken(iToken, "this") ? IDE_INLINE_THIS_WORD : -1;
			for (i = 0; (iKind < 0) && (i < psDest->aszTemplateParameters.NumElements()); i++)
			{
				if (pcUnit->IsToken(iToken, psDest->aszTemplateParameters.GetText(i)) && !pcUnit->IsToken(iToken - 1, ".") && !pcUnit->IsToken(iToken - 1, "->") && !pcUnit->IsToken(iToken - 1, "::"))
				{
					iKind = IDE_INLINE_TEMPLATE;
					break;
				}
			}
			psSource = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)iToken));
			if ((iKind >= 0) && (psSource->iExpansion == -1) && pcUnit->GetTokensRange(iToken, iToken, &sRange))
			{
				AddReplace(psDest, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn), iKind, (iKind == IDE_INLINE_TEMPLATE) ? (int)i : 0, false);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A call of the function, read where it is written: what it is called
// on, its arguments, the statement it is in and what it is to it, and
// the function it is in.  False if it can't be read (a macro makes it).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::ReadCall(SIDESignatureCall* psCall, CCPPSyntax* pcValue, SIDEFunctionInlining* psDest, int iCaretLine, const char* szCaretFile)
{
	CIDECodeModel*					pcModel;
	CIDERefactorUnit				cUnit;
	CTextFile						cFile;
	CArrayChars						aszDerived;
	SIDEInlineCall*					psInline;
	CCPPSyntax*						pcCallee;
	CCPPSyntax*						pcTop;
	CCPPSyntax*						pcParent;
	CCPPSyntax*						pcStatement;
	CCPPSyntaxCallExpression*		pcCall;
	CCPPSyntaxMemberExpression*		pcMember;
	CCPPSyntax*						pcObject;
	CCPPSyntax*						pcAddressOf;
	CCPPSyntaxBinaryExpression*		pcBinary;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CCPPSyntaxCompoundStatement*	pcBody;
	CCPPSyntaxName*					pcName;
	CIDEEffectFinder				cEffects;
	SIDEIntroduction				sIntroduction;
	SPreprocessedToken*				psSource;
	SCPPSyntaxRange					sRange;
	SCPP65Type*						psType;
	CChars							szText;
	CChars							szKey;
	CChars							szScope;
	const char*						szFile;
	size							uiAt;
	size							i;
	int32							iToken;
	int								iDepth;
	uint8							uiKind;
	bool							bChecked;
	bool							bResult;
	bool							bArrow;

	pcModel = mpcContent->GetCodeModel();
	bChecked = (psDest->aszTemplateParameters.NumElements() > 0);
	cFile.Init();
	cUnit.Init();
	szFile = GetIDEInlineText(mpcContent, psCall->szFileName.Text(), &cFile);
	bResult = szFile && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psCall->szFileName.Text(), bChecked);
	pcCall = NULL;
	pcCallee = NULL;
	if (bResult)
	{
		iToken = cUnit.FindTokenAt(psCall->iLine, psCall->iColumn);
		pcCallee = cUnit.FindNodeOfTokens(iToken, iToken);
		for (iDepth = 0; pcCallee && pcCallee->mpcParent && (pcCallee->mpcParent->GetKind() != CPPS_CallExpression) && (iDepth < 8); iDepth++)
		{
			pcCallee = pcCallee->mpcParent;
		}
		pcCall = (pcCallee && pcCallee->mpcParent && (pcCallee->mpcParent->GetKind() == CPPS_CallExpression)) ? (CCPPSyntaxCallExpression*)pcCallee->mpcParent : NULL;
		bResult = pcCall && (pcCall->mpcCallee == pcCallee) && cUnit.GetRange(pcCall, &sRange);
	}
	if (!bResult)
	{
		cUnit.Kill();
		cFile.Kill();
		return false;
	}

	psInline = NewMalloc<SIDEInlineCall>();
	psInline->Init();
	psInline->szFileName.Set(psCall->szFileName);
	psInline->iLine = psCall->iLine;
	psInline->uiStart = FindIDETextOffset(szFile, sRange.iLine, sRange.iColumn);
	psInline->uiEnd = FindIDETextOffset(szFile, sRange.iEndLine, sRange.iEndColumn);
	psInline->bSelected = (psCall->iLine == iCaretLine) && (StringInsensitiveCompare(psCall->szFileName.Text(), szCaretFile) == 0);
	for (i = 0; i < psCall->asArguments.NumElements(); i++)
	{
		szText.Init();
		AppendIDERangeText(szFile, psCall->asArguments.Get(i), &szText);
		psInline->aszArguments.Add(szText.Text());
		szText.Kill();
		psInline->aiEffects.Add(psCall->aiEffects.GetValue(i));
	}

	//What it is called on.  Through an address taken there ("(&cOther)->Kind()") it is the object itself, by ".".
	pcMember = (pcCallee->GetKind() == CPPS_MemberExpression) ? (CCPPSyntaxMemberExpression*)pcCallee : NULL;
	pcObject = pcMember ? pcMember->mpcObject : NULL;
	bArrow = pcMember && pcMember->mbArrow;
	pcAddressOf = (pcObject && bArrow) ? pcObject->WithoutParentheses() : NULL;
	if (pcAddressOf && (pcAddressOf->GetKind() == CPPS_UnaryExpression) && (((CCPPSyntaxUnaryExpression*)pcAddressOf)->meOperator == CT_BitwiseAnd) &&
		((CCPPSyntaxUnaryExpression*)pcAddressOf)->mpcOperand)
	{
		pcObject = ((CCPPSyntaxUnaryExpression*)pcAddressOf)->mpcOperand;
		bArrow = false;
	}
	if (pcObject && cUnit.GetRange(pcObject, &sRange))
	{
		szText.Init();
		AppendIDERangeText(szFile, &sRange, &szText);
		if (!bArrow && IsIDECompoundText(szText.Text()))
		{
			szText.Insert(0, "(");
			szText.Append(')');
		}
		if (!szText.Equals("this"))
		{
			psInline->szObject.Set(szText);
			psInline->szObject.Append(bArrow ? "->" : ".");
			psInline->szThis.Set(bArrow ? "" : "&");
			psInline->szThis.Append(szText);
			cEffects.mbEffects = false;
			cEffects.Look(pcMember->mpcObject);
			psInline->bObjectEffects = cEffects.mbEffects;
		}
		szText.Kill();
	}

	//What it is to the statement it is in.
	pcTop = pcCall;
	while (pcTop->mpcParent && (pcTop->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
	{
		pcTop = pcTop->mpcParent;
	}
	pcParent = pcTop->mpcParent;
	pcStatement = NULL;
	pcBinary = (pcParent && (pcParent->GetKind() == CPPS_BinaryExpression)) ? (CCPPSyntaxBinaryExpression*)pcParent : NULL;
	if (pcParent && (pcParent->GetKind() == CPPS_ExpressionStatement))
	{
		psInline->iContext = IDE_CALL_STATEMENT;
		pcStatement = pcParent;
	}
	else if (pcParent && (pcParent->GetKind() == CPPS_ReturnStatement))
	{
		psInline->iContext = IDE_CALL_RETURNED;
		pcStatement = pcParent;
	}
	else if (pcBinary && (pcBinary->meOperator == CT_Assignment) && (pcBinary->mpcRight == pcTop) && pcBinary->mpcParent && (pcBinary->mpcParent->GetKind() == CPPS_ExpressionStatement) &&
			 cUnit.GetRange(pcBinary->mpcLeft, &sRange))
	{
		psInline->iContext = IDE_CALL_ASSIGNED;
		pcStatement = pcBinary->mpcParent;
		AppendIDERangeText(szFile, &sRange, &psInline->szAssigned);
	}
	else
	{
		//Inside a bigger expression: it may be taken out of it.
		pcStatement = FindIDEHoistStatement(&cUnit, pcTop);
		if (pcStatement)
		{
			psInline->iContext = IDE_CALL_HOISTED;
		}
	}
	if (pcStatement && !(pcStatement->mpcParent && ((pcStatement->mpcParent->GetKind() == CPPS_CompoundStatement) || (pcStatement->mpcParent->GetKind() == CPPS_CaseStatement))))
	{
		//Not in braces: more statements can't be written where it is.
		pcStatement = NULL;
	}
	if (pcStatement && cUnit.GetRange(pcStatement, &sRange))
	{
		psInline->uiStatement = FindIDETextOffset(szFile, sRange.iLine, 0);
		psInline->uiStatementEnd = FindIDETextOffset(szFile, sRange.iEndLine, sRange.iEndColumn);
		psInline->szIndent.AppendSubString(szFile, psInline->uiStatement, FindIDETextOffset(szFile, sRange.iLine, sRange.iColumn));
		if (psInline->uiStatement + psInline->szIndent.Length() != FindIDETextOffset(szFile, sRange.iLine, sRange.iColumn))
		{
			psInline->iContext = IDE_CALL_IN_EXPRESSION;
		}
		for (i = 0; i < psInline->szIndent.Length(); i++)
		{
			if ((psInline->szIndent.Text()[i] != ' ') && (psInline->szIndent.Text()[i] != '\t'))
			{
				//Something is on its line before it ("if (x) Fill();"): it isn't a line of its own.
				psInline->iContext = IDE_CALL_IN_EXPRESSION;
			}
		}
	}
	else
	{
		psInline->iContext = IDE_CALL_IN_EXPRESSION;
	}
	psInline->bWrapped = pcValue && mpcContent->GetInlineVariable()->IsWrapped(pcValue, pcTop);

	//The function it is in: whether that is one of the class's, where its declarations are, and the names it writes.
	pcFunction = (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcCall, CPPS_FunctionDefinition);
	psInline->bOutside = !psDest->szClassKey.Empty();
	if (pcFunction)
	{
		pcName = pcFunction->mpcDeclarator ? pcFunction->mpcDeclarator->GetName() : NULL;
		szKey.Init();
		if (pcName && pcName->GetLast() && cUnit.GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sRange) &&
			pcModel->GetSymbolAt(psCall->szFileName.Text(), sRange.iLine, sRange.iColumn, &szKey, &uiKind))
		{
			szScope.Init();
			GetIDESymbolScope(szKey.Text(), &szScope);
			aszDerived.Init();
			pcModel->FindClassDerived(psDest->szClassKey.Text(), true, &aszDerived);
			psInline->bOutside = psInline->bOutside && !szScope.Equals(psDest->szClassKey.Text()) && !aszDerived.Contains(szScope.Text());
			aszDerived.Kill();
			szScope.Kill();
		}
		szKey.Kill();
		pcBody = (pcFunction->mpcBody && (pcFunction->mpcBody->GetKind() == CPPS_CompoundStatement)) ? (CCPPSyntaxCompoundStatement*)pcFunction->mpcBody : NULL;
		if (pcBody && cUnit.GetTokensRange(pcBody->miFirstToken, pcBody->miFirstToken, &sRange))
		{
			psInline->uiDeclarations = FindIDETextOffset(szFile, sRange.iLine, sRange.iColumn);
			for (i = 0; (i < pcBody->macpStatements.NumElements()) && (pcBody->macpStatements.GetPtr(i)->GetKind() == CPPS_DeclarationStatement) && cUnit.GetRange(pcBody->macpStatements.GetPtr(i), &sRange); i++)
			{
				psInline->uiDeclarations = FindIDETextOffset(szFile, sRange.iEndLine, sRange.iEndColumn);
				psInline->bDeclarations = true;
			}
			for (uiAt = psInline->uiDeclarations; (szFile[uiAt] != '\0') && (szFile[uiAt] != '\n'); uiAt++)
			{
			}
			psInline->uiDeclarations = (szFile[uiAt] == '\n') ? uiAt + 1 : uiAt;
		}
		for (iToken = pcFunction->miFirstToken; iToken <= pcFunction->miLastToken; iToken++)
		{
			if (cUnit.GetTokeniser()->Get((size)iToken)->IsIdentifier())
			{
				psSource = cUnit.GetTokeniser()->GetSource(cUnit.GetTokeniser()->Get((size)iToken));
				szText.Init();
				szText.Append(psSource->szText, psSource->uiLength);
				psInline->aszNames.AddIfUnique(szText.Text());
				szText.Kill();
			}
		}
	}

	//The variable what it gives goes in, where it is taken out of an expression: named as Introduce Variable names one.
	if (psInline->iContext == IDE_CALL_HOISTED)
	{
		if (!pcFunction || psDest->szReturnType.Empty())
		{
			psInline->iContext = IDE_CALL_IN_EXPRESSION;
		}
		else
		{
			sIntroduction.Init();
			sIntroduction.szType.Set(psDest->szReturnType);
			for (i = 0; i < psInline->aszNames.NumElements(); i++)
			{
				sIntroduction.aszUsed.Add(psInline->aszNames.GetText(i));
			}
			for (i = 0; i < psDest->aszDeclared.NumElements(); i++)
			{
				sIntroduction.aszUsed.Add(psDest->aszDeclared.GetText(i));
			}
			for (i = 0; i < psDest->aszOwnParameters.NumElements(); i++)
			{
				sIntroduction.aszUsed.Add(psDest->aszOwnParameters.GetText(i));
			}
			mpcContent->GetIntroduceVariable()->SuggestName(&cUnit, pcCall, &sIntroduction);
			psInline->szVariable.Set(sIntroduction.szSuggested);
			sIntroduction.Kill();
		}
	}

	//A class template's instance: its arguments, from the type of what the call is on.
	if (bChecked && cUnit.IsChecked() && pcMember && pcMember->mpcObject && psInline->bOutside)
	{
		psType = cUnit.GetPass()->GetType(pcMember->mpcObject);
		if (psType && pcMember->mbArrow && psType->IsPointer())
		{
			psType = psType->psBase;
		}
		//As its declaration writes it ("CSlot<long>"), or as the compiler has it.
		szText.Init();
		if (!cUnit.AppendDeclaredType(pcMember->mpcObject, &szText) || !szText.Contains("<"))
		{
			szText.Clear();
			if (psType)
			{
				cUnit.GetPass()->AppendType(psType->Unqualified(), &szText);
			}
		}
		if (szText.Contains("<"))
		{
			szKey.Init();
			szKey.AppendSubString(szText.Text(), szText.Find('<') + 1, szText.Length());
			while (!szKey.Empty() && (szKey.Text()[szKey.Length() - 1] != '>'))
			{
				szKey.SetLength(szKey.Length() - 1);
			}
			szKey.SetLength(szKey.Empty() ? 0 : szKey.Length() - 1);
			szScope.Init();
			iDepth = 0;
			for (i = 0; i <= szKey.Length(); i++)
			{
				if ((i == szKey.Length()) || ((szKey.Text()[i] == ',') && (iDepth == 0)))
				{
					szScope.StripWhitespace();
					psInline->aszInstance.Add(szScope.Text());
					szScope.Clear();
				}
				else
				{
					iDepth += (szKey.Text()[i] == '<') ? 1 : ((szKey.Text()[i] == '>') ? -1 : 0);
					szScope.Append(szKey.Text()[i]);
				}
			}
			szScope.Kill();
			szKey.Kill();
		}
		szText.Kill();
	}

	psDest->apsCalls.Add(psInline);
	psDest->bSelectedCall = psDest->bSelectedCall || psInline->bSelected;
	cUnit.Kill();
	cFile.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What Inline Function has to work with, for the function named at the
// caret.  False if there is no function there; psDest->bPossible says
// whether it can be inlined, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::FindInlining(SIDEFunctionInlining* psDest)
{
	CIDECodeModel*		pcModel;
	CIDERefactorUnit	cUnit;
	CTextFile			cFile;
	CTextFile			cSiteFile;
	CArrayIDELocations	asLocations;
	SIDESignature*		psSignature;
	SIDESignatureSite*	psSite;
	SIDESignatureSite*	psDefinition;
	CCPPSyntax*			pcValue;
	CChars				szScope;
	CChars				szText;
	const char*			szFile;
	const char*			szSite;
	size				uiBody;
	size				uiBodyEnd;
	size				uiAt;
	size				i;
	int					iLine;
	int					iColumn;
	uint8				uiRole;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	psSignature = &psDest->sSignature;
	if (!mpcContent->GetChangeSignature()->FindSignature(psSignature))
	{
		return false;
	}
	if (!psSignature->bChangeable)
	{
		return psSignature->szWhyNot.Equals("it isn't a function") ? false : Refuse(psDest, psSignature->szWhyNot.Text());
	}
	iLine = -1;
	iColumn = 0;
	mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);

	psDest->bPossible = true;
	psDefinition = NULL;
	cFile.Init();
	for (i = 0; psDest->bPossible && (i < psSignature->apsSites.NumElements()); i++)
	{
		psSite = psSignature->apsSites.GetPtr(i);
		if (psSite->bBody && !psDefinition && psSite->szKey.Equals(psSignature->szKey.Text()))
		{
			psDefinition = psSite;
		}
	}
	if (psSignature->uiOverrides > 0)
	{
		Refuse(psDest, "it is virtual: other functions override it, or it overrides one");
	}
	else if (!psDefinition)
	{
		Refuse(psDest, "it isn't defined in the solution");
	}
	else if (!psDefinition->bReturn || psDefinition->szOwnName.StartsWith("operator") || psDefinition->szOwnName.StartsWith("~"))
	{
		Refuse(psDest, "it is a constructor, a destructor or an operator");
	}
	szFile = psDefinition ? GetIDEInlineText(mpcContent, psDefinition->szFileName.Text(), &cFile) : NULL;
	cUnit.Init();
	if (psDest->bPossible && (!szFile || !cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psDefinition->szFileName.Text(), true) || !cUnit.IsChecked()))
	{
		Refuse(psDest, "the file it is defined in doesn't compile as it is");
	}
	if (psDest->bPossible)
	{
		psDest->szFileName.Set(psDefinition->szFileName);

		//Virtual, by what its declarations say; and a function template has no one body.
		for (i = 0; psDest->bPossible && (i < psSignature->apsSites.NumElements()); i++)
		{
			psSite = psSignature->apsSites.GetPtr(i);
			cSiteFile.Init();
			szSite = GetIDEInlineText(mpcContent, psSite->szFileName.Text(), &cSiteFile);
			if (szSite)
			{
				szText.Init();
				AppendIDERangeText(szSite, &psSite->sAll, &szText);
				uiAt = szText.Find('(');
				szText.SetLength((uiAt < szText.Length()) ? uiAt : szText.Length());
				if (szText.Contains("virtual"))
				{
					Refuse(psDest, "it is virtual");
				}
				szText.Kill();
			}
			cSiteFile.Kill();
		}
		asLocations.Init();
		if (pcModel->GetSymbolLocations(psSignature->szKey.Text(), &asLocations, &uiKind) && (uiKind == CPPSK_FunctionTemplate))
		{
			Refuse(psDest, "it is a function template");
		}
		KillIDELocations(&asLocations);
	}
	if (psDest->bPossible)
	{
		szScope.Init();
		GetIDESymbolScope(psSignature->szKey.Text(), &szScope);
		asLocations.Init();
		if (!szScope.Empty() && pcModel->GetSymbolLocations(szScope.Text(), &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
		{
			psDest->szClassKey.Set(szScope);
			psDest->szClassName.Set(szScope);
		}
		KillIDELocations(&asLocations);
		szScope.Kill();
	}
	pcValue = NULL;
	if (psDest->bPossible && ReadBody(&cUnit, psDefinition, szFile, psDest, &pcValue))
	{
		ReadReplaces(&cUnit, psDefinition, szFile, psDest);
	}
	if (psDest->bPossible)
	{
		//Its calls: one in its own body makes it recursive.
		uiBody = FindIDETextOffset(szFile, psDefinition->sBody.iLine, psDefinition->sBody.iColumn);
		uiBodyEnd = FindIDETextOffset(szFile, psDefinition->sBody.iEndLine, psDefinition->sBody.iEndColumn);
		for (i = 0; psDest->bPossible && (i < psSignature->apsCalls.NumElements()); i++)
		{
			if (StringInsensitiveCompare(psSignature->apsCalls.GetPtr(i)->szFileName.Text(), psDefinition->szFileName.Text()) == 0)
			{
				uiAt = FindIDETextOffset(szFile, psSignature->apsCalls.GetPtr(i)->iLine, psSignature->apsCalls.GetPtr(i)->iColumn);
				if ((uiAt >= uiBody) && (uiAt < uiBodyEnd))
				{
					Refuse(psDest, "it is recursive: it calls itself");
					break;
				}
			}
			if (!ReadCall(psSignature->apsCalls.GetPtr(i), pcValue, psDest, iLine, mpcContent->GetDocuments()->GetActiveTab()->GetFileName()))
			{
				Refuse(psDest, "a call of it can't be read (a macro makes it)");
			}
		}
		KeepNestedCalls(psDest);
	}
	cUnit.Kill();
	cFile.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a call is one that is inlined: every one, or the one at the
// caret; and, for a function with statements, only where the call is a
// statement, or all a statement assigns or returns.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::IsInlined(SIDEFunctionInlining* psInlining, SIDEInlineCall* psCall, int iMode)
{
	if ((iMode == IDE_INLINE_THIS) && !psCall->bSelected)
	{
		return false;
	}
	if (psInlining->bExpression)
	{
		return true;
	}
	if (psCall->iContext == IDE_CALL_HOISTED)
	{
		return (psInlining->uiValueEnd > psInlining->uiValue) && !psInlining->szReturnType.Empty() && !psCall->szVariable.Empty();
	}
	return psCall->iContext != IDE_CALL_IN_EXPRESSION;
}


//////////////////////////////////////////////////////////////////////////
//
// A call written in another's arguments stays a call where the function
// has statements: the other is written with it, and can't be if the
// call is taken out of what the other's statement was.
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::KeepNestedCalls(SIDEFunctionInlining* psDest)
{
	SIDEInlineCall*	psCall;
	SIDEInlineCall*	psOther;
	size			i;
	size			j;

	for (i = 0; !psDest->bExpression && (i < psDest->apsCalls.NumElements()); i++)
	{
		psCall = psDest->apsCalls.GetPtr(i);
		for (j = 0; (psCall->iContext == IDE_CALL_HOISTED) && (j < psDest->apsCalls.NumElements()); j++)
		{
			psOther = psDest->apsCalls.GetPtr(j);
			if ((i != j) && (psOther->uiStart <= psCall->uiStart) && (psCall->uiEnd <= psOther->uiEnd) && (StringInsensitiveCompare(psOther->szFileName.Text(), psCall->szFileName.Text()) == 0))
			{
				psCall->iContext = IDE_CALL_IN_EXPRESSION;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// "Tank.cpp line 12".
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::AppendPlace(SIDEInlineCall* psCall, CChars* pszDest)
{
	pszDest->Append(GetIDEFileNamePart(psCall->szFileName.Text()));
	pszDest->Append(" line ");
	pszDest->Append((int32)(psCall->iLine + 1));
}


//////////////////////////////////////////////////////////////////////////
//
// What stands in the way of inlining the calls a mode inlines, a line
// each.
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::FindConflicts(SIDEFunctionInlining* psInlining, int iMode, CArrayChars* paszDest)
{
	SIDEInlineCall*	psCall;
	CChars			szLine;
	size			uiStay;
	size			i;
	size			j;
	bool			bTemplate;

	bTemplate = false;
	for (i = 0; i < psInlining->asReplaces.NumElements(); i++)
	{
		bTemplate = bTemplate || (psInlining->asReplaces.Get(i)->iKind == IDE_INLINE_TEMPLATE);
	}
	uiStay = 0;
	for (i = 0; i < psInlining->apsCalls.NumElements(); i++)
	{
		psCall = psInlining->apsCalls.GetPtr(i);
		if ((iMode == IDE_INLINE_THIS) && !psCall->bSelected)
		{
			continue;
		}
		if (!IsInlined(psInlining, psCall, iMode))
		{
			szLine.Init("The call at ");
			AppendPlace(psCall, &szLine);
			szLine.Append(" is part of an expression it can't be taken out of, and ")->Append(psInlining->sSignature.szOldName)->Append(" has statements: it stays a call.");
			paszDest->Add(szLine.Text());
			szLine.Kill();
			uiStay++;
			continue;
		}
		for (j = 0; j < psInlining->aiParameterUses.NumElements(); j++)
		{
			if ((j < psCall->aiEffects.NumElements()) && (psCall->aiEffects.GetValue(j) != 0) && (psInlining->aiParameterUses.GetValue(j) != 1))
			{
				szLine.Init("Argument ");
				szLine.Append((uint32)(j + 1));
				szLine.Append(" of the call at ");
				AppendPlace(psCall, &szLine);
				szLine.Append(" does something, and would be written ");
				szLine.Append((uint32)psInlining->aiParameterUses.GetValue(j));
				szLine.Append(" times.");
				paszDest->Add(szLine.Text());
				szLine.Kill();
			}
		}
		if (psCall->bObjectEffects && psInlining->bNamesMembers)
		{
			szLine.Init("What the call at ");
			AppendPlace(psCall, &szLine);
			szLine.Append(" is made on does something, and would be written before each member the body names.");
			paszDest->Add(szLine.Text());
			szLine.Kill();
		}
		if (psCall->bOutside && !psInlining->szNotPublic.Empty())
		{
			szLine.Init("The body names ");
			szLine.Append(psInlining->szNotPublic)->Append(", which isn't public: the call at ");
			AppendPlace(psCall, &szLine);
			szLine.Append(" is outside the class.");
			paszDest->Add(szLine.Text());
			szLine.Kill();
		}
		if (psCall->bOutside && bTemplate && (psCall->aszInstance.NumElements() < psInlining->aszTemplateParameters.NumElements()))
		{
			szLine.Init("The body names its class template's parameters, and what they are at the call at ");
			AppendPlace(psCall, &szLine);
			szLine.Append(" isn't known.");
			paszDest->Add(szLine.Text());
			szLine.Kill();
		}
		for (j = 0; !psInlining->bExpression && (j < psInlining->aszDeclared.NumElements()); j++)
		{
			if (psCall->aszNames.Contains(psInlining->aszDeclared.GetText(j)))
			{
				szLine.Init("Its variable ");
				szLine.Append(psInlining->aszDeclared.GetText(j))->Append(" has a name the function at ");
				AppendPlace(psCall, &szLine);
				szLine.Append(" uses.");
				paszDest->Add(szLine.Text());
				szLine.Kill();
			}
		}
	}
	if (iMode == IDE_INLINE_ALL_AND_DELETE)
	{
		for (i = 0; i < psInlining->sSignature.asOtherUses.NumElements(); i++)
		{
			szLine.Init("It is named without being called in ");
			szLine.Append(GetIDEFileNamePart(psInlining->sSignature.asOtherUses.Get(i)->szFileName.Text()))->Append(" at line ");
			szLine.Append((int32)(psInlining->sSignature.asOtherUses.Get(i)->iLine + 1));
			szLine.Append(": it isn't taken out.");
			paszDest->Add(szLine.Text());
			szLine.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A part of the function's body (of its definition's file's text) as it
// is written at a call: each parameter the call's argument, and so on
// (see the class's comment).
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::AppendBody(SIDEFunctionInlining* psInlining, SIDEInlineCall* psCall, const char* szText, size uiStart, size uiEnd, CChars* pszDest)
{
	SIDEInlineReplace*	psReplace;
	const char*			szArgument;
	size				uiAt;
	size				uiArrow;
	size				i;

	uiAt = uiStart;
	for (i = 0; i < psInlining->asReplaces.NumElements(); i++)
	{
		psReplace = psInlining->asReplaces.Get(i);
		if ((psReplace->uiStart < uiAt) || (psReplace->uiEnd > uiEnd))
		{
			continue;
		}
		pszDest->AppendSubString(szText, uiAt, psReplace->uiStart);
		uiAt = psReplace->uiEnd;
		if (psReplace->iKind == IDE_INLINE_PARAMETER)
		{
			//The argument, or the parameter's default value where the call leaves it out.
			szArgument = ((size)psReplace->iIndex < psCall->aszArguments.NumElements()) ? psCall->aszArguments.GetText((size)psReplace->iIndex) :
						 (((size)psReplace->iIndex < psInlining->sSignature.aszOldDefaults.NumElements()) ? psInlining->sSignature.aszOldDefaults.GetText((size)psReplace->iIndex) : "");
			for (uiArrow = psReplace->uiEnd; (uiArrow < uiEnd) && ((szText[uiArrow] == ' ') || (szText[uiArrow] == '\t')); uiArrow++)
			{
			}
			if ((szArgument[0] == '&') && !IsIDECompoundText(&szArgument[1]) && (uiArrow + 1 < uiEnd) && (szText[uiArrow] == '-') && (szText[uiArrow + 1] == '>'))
			{
				//A pointer given as an object's address, used through ->: the object, by "." ("cOther.Kind()", not
				//"(&cOther)->Kind()").
				pszDest->Append(&szArgument[1])->Append('.');
				uiAt = uiArrow + 2;
			}
			else if (psReplace->bOperand && IsIDECompoundText(szArgument))
			{
				pszDest->Append('(')->Append(szArgument)->Append(')');
			}
			else
			{
				pszDest->Append(szArgument);
			}
		}
		else if (psReplace->iKind == IDE_INLINE_MEMBER)
		{
			pszDest->Append(psCall->szObject);
		}
		else if (psReplace->iKind == IDE_INLINE_THIS_WORD)
		{
			pszDest->Append(psCall->szThis);
		}
		else if (psReplace->iKind == IDE_INLINE_SCOPED)
		{
			if (psCall->bOutside)
			{
				pszDest->Append(psInlining->szClassName)->Append("::");
			}
		}
		else if (psReplace->iKind == IDE_INLINE_OWN_PARAMETER)
		{
			pszDest->Append(psCall->aszOwnNames.GetText((size)psReplace->iIndex));
		}
		else if (psCall->bOutside && ((size)psReplace->iIndex < psCall->aszInstance.NumElements()))
		{
			pszDest->Append(psCall->aszInstance.GetText((size)psReplace->iIndex));
		}
		else
		{
			pszDest->AppendSubString(szText, psReplace->uiStart, psReplace->uiEnd);
		}
	}
	pszDest->AppendSubString(szText, uiAt, uiEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// A declaration ("int	iNext;") to go with the declarations of the
// function a call is in, lined up with them: once, however many calls in
// that function ask for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEInlineFunction::AddDeclaration(CArrayIDEFileRanges* pasInserts, CArrayChars* paszTexts, SIDEInlineCall* psCall, const char* szFile, const char* szDeclaration)
{
	CChars	szLine;
	size	uiInsert;

	for (uiInsert = 0; uiInsert < pasInserts->NumElements(); uiInsert++)
	{
		if ((pasInserts->Get(uiInsert)->uiStart == psCall->uiDeclarations) && (StringInsensitiveCompare(pasInserts->Get(uiInsert)->szFileName.Text(), psCall->szFileName.Text()) == 0))
		{
			break;
		}
	}
	if (uiInsert == pasInserts->NumElements())
	{
		pasInserts->Add()->Init(psCall->szFileName.Text(), 0, 0, 0, 0);
		pasInserts->Get(uiInsert)->uiStart = psCall->uiDeclarations;
		pasInserts->Get(uiInsert)->uiEnd = psCall->bDeclarations ? 0 : 1;
		paszTexts->Add("");
	}
	szLine.Init("\t");
	szLine.Append(szDeclaration);
	szLine.Append('\n');
	if (psCall->bDeclarations)
	{
		RealignIDEDeclaration(&szLine, FindIDENameColumn(szFile, psCall->uiDeclarations));
	}
	if (!paszTexts->Get(uiInsert)->Contains(szLine.Text()))
	{
		paszTexts->Get(uiInsert)->Append(szLine);
	}
	szLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The function inlined where a mode says, and taken out if it says so
// and nothing is left that names it.  One undo across the files.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::Inline(SIDEFunctionInlining* psInlining, int iMode, CChars* pszStatus)
{
	CIDEFileEdits		cFiles;
	CArrayIDEFileRanges	asInserts;
	CArrayChars			aszInsertTexts;
	CArrayIDEFileRanges	asHoists;
	CArrayChars			aszHoistTexts;
	CArrayChars			aszHoisted;
	CArrayChars			aszLines;
	SIDEInlineCall*		psCall;
	SIDESignatureSite*	psSite;
	CChars				szText;
	CChars				szValue;
	CChars				szStatements;
	CChars				szLine;
	CChars				szMessage;
	CChars				szVariable;
	const char*			szArgument;
	const char*			szDefinition;
	const char*			szFile;
	size				uiInlined;
	size				uiStayed;
	size				uiStart;
	size				uiEnd;
	size				i;
	size				j;
	size				k;
	bool				bDeleted;
	bool				bFound;

	if (!psInlining->bPossible)
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cFiles.Init(mpcContent);
	asInserts.Init();
	aszInsertTexts.Init();
	asHoists.Init();
	aszHoistTexts.Init();
	aszHoisted.Init();
	szDefinition = cFiles.GetText(psInlining->szFileName.Text());
	uiInlined = 0;
	uiStayed = 0;
	for (i = 0; szDefinition && (i < psInlining->apsCalls.NumElements()); i++)
	{
		psCall = psInlining->apsCalls.GetPtr(i);
		if ((iMode == IDE_INLINE_THIS) && !psCall->bSelected)
		{
			continue;
		}
		if (!IsInlined(psInlining, psCall, iMode))
		{
			uiStayed++;
			continue;
		}
		szFile = cFiles.GetText(psCall->szFileName.Text());
		if (!szFile)
		{
			uiStayed++;
			continue;
		}
		//The variables its changed parameters get here: their own names, numbered where the function it goes into
		//has them.
		psCall->aszOwnNames.ReInit();
		for (j = 0; j < psInlining->aszOwnParameters.NumElements(); j++)
		{
			szLine.Init(psInlining->aszOwnParameters.GetText(j));
			for (k = 2; !szLine.Empty() && (psCall->aszNames.Contains(szLine.Text()) || psInlining->aszDeclared.Contains(szLine.Text())); k++)
			{
				szLine.Set(psInlining->aszOwnParameters.GetText(j));
				szLine.Append((uint32)k);
			}
			psCall->aszOwnNames.Add(szLine.Text());
			szLine.Kill();
		}
		szValue.Init();
		if (psInlining->uiValueEnd > psInlining->uiValue)
		{
			AppendBody(psInlining, psCall, szDefinition, psInlining->uiValue, psInlining->uiValueEnd, &szValue);
		}
		if (psInlining->bExpression)
		{
			szText.Init();
			if (psCall->bWrapped)
			{
				szText.Append('(')->Append(szValue)->Append(')');
			}
			else
			{
				szText.Append(szValue);
			}
			cFiles.GetEdits(psCall->szFileName.Text())->Replace(psCall->uiStart, psCall->uiEnd, szText.Text());
			szText.Kill();
		}
		else
		{
			//Its changed parameters' variables given the arguments (or the default values the call leaves out),
			szText.Init();
			for (j = 0; j < psInlining->aszOwnParameters.NumElements(); j++)
			{
				if (!psCall->aszOwnNames.Get(j)->Empty())
				{
					szArgument = (j < psCall->aszArguments.NumElements()) ? psCall->aszArguments.GetText(j) : ((j < psInlining->sSignature.aszOldDefaults.NumElements()) ? psInlining->sSignature.aszOldDefaults.GetText(j) : "");
					szText.Append(psCall->szIndent)->Append(psCall->aszOwnNames.GetText(j))->Append(" = ")->Append(szArgument)->Append(";\n");
				}
			}

			//Its statements at the call's statement's indentation, then what becomes of what it returned.
			szStatements.Init();
			if (psInlining->uiStatementsEnd > psInlining->uiStatements)
			{
				AppendBody(psInlining, psCall, szDefinition, psInlining->uiStatements, psInlining->uiStatementsEnd, &szStatements);
				szStatements.Replace("\r", "");
				aszLines.Init();
				szStatements.Split(&aszLines, '\n');
				for (j = 0; j < aszLines.NumElements(); j++)
				{
					szLine.Init(aszLines.GetText(j));
					if (szLine.StartsWith(psInlining->szBodyIndent.Text()))
					{
						szText.Append(psCall->szIndent);
						szText.Append(&szLine.Text()[psInlining->szBodyIndent.Length()]);
					}
					else
					{
						szLine.StripWhitespace();
						szText.Append(szLine.Empty() ? "" : psCall->szIndent.Text());
						szText.Append(szLine);
					}
					szText.Append('\n');
					szLine.Kill();
				}
				aszLines.Kill();
			}
			szStatements.Kill();
			szVariable.Init();
			if (psCall->iContext == IDE_CALL_HOISTED)
			{
				//What it returned goes in a variable no other call taken out in this function has, before the
				//statement, and the variable where the call was.
				szVariable.Set(psCall->szVariable);
				for (k = 2; ; k++)
				{
					szLine.Init(psCall->szFileName);
					szLine.Append('|')->Append((uint32)psCall->uiDeclarations)->Append('|')->Append(szVariable);
					bFound = aszHoisted.Contains(szLine.Text()) || ((k > 2) && (psCall->aszNames.Contains(szVariable.Text()) || psInlining->aszDeclared.Contains(szVariable.Text())));
					if (!bFound)
					{
						aszHoisted.Add(szLine.Text());
					}
					szLine.Kill();
					if (!bFound)
					{
						break;
					}
					szVariable.Set(psCall->szVariable);
					szVariable.Append((uint32)k);
				}
				szText.Append(psCall->szIndent)->Append(szVariable)->Append(" = ")->Append(szValue)->Append(";\n");
				bFound = false;
				for (k = 0; k < asHoists.NumElements(); k++)
				{
					if ((asHoists.Get(k)->uiStart == psCall->uiStatement) && (StringInsensitiveCompare(asHoists.Get(k)->szFileName.Text(), psCall->szFileName.Text()) == 0))
					{
						aszHoistTexts.Get(k)->Append(szText);
						bFound = true;
					}
				}
				if (!bFound)
				{
					asHoists.Add()->Init(psCall->szFileName.Text(), 0, 0, 0, 0);
					asHoists.Get(asHoists.NumElements() - 1)->uiStart = psCall->uiStatement;
					asHoists.Get(asHoists.NumElements() - 1)->uiEnd = psCall->uiStatement + psCall->szIndent.Length();
					aszHoistTexts.Add(szText.Text());
				}
				cFiles.GetEdits(psCall->szFileName.Text())->Replace(psCall->uiStart, psCall->uiEnd, szVariable.Text());
			}
			else
			{
				if (psCall->iContext == IDE_CALL_ASSIGNED)
				{
					szText.Append(psCall->szIndent)->Append(psCall->szAssigned)->Append(" = ")->Append(szValue)->Append(";\n");
				}
				else if (psCall->iContext == IDE_CALL_RETURNED)
				{
					szText.Append(psCall->szIndent)->Append(szValue.Empty() ? "return" : "return ")->Append(szValue)->Append(";\n");
				}
				else if (!szValue.Empty() && psInlining->bValueEffects)
				{
					szText.Append(psCall->szIndent)->Append(szValue)->Append(";\n");
				}
				if (szText.Empty())
				{
					//Nothing is left of it: its line goes.
					uiStart = psCall->uiStatement + psCall->szIndent.Length();
					uiEnd = psCall->uiStatementEnd;
					ExtendIDEDeletion(szFile, &uiStart, &uiEnd, true);
					cFiles.GetEdits(psCall->szFileName.Text())->Replace(uiStart, uiEnd, "");
				}
				else
				{
					szText.SetLength(szText.Length() - 1);
					cFiles.GetEdits(psCall->szFileName.Text())->Replace(psCall->uiStatement, psCall->uiStatementEnd, szText.Text());
				}
			}
			szText.Kill();

			//Its own variables' declarations, with the declarations of the function the call is in, once for each; and
			//those of its changed parameters' variables and of the one what it returned goes in.
			for (j = 0; j < psInlining->aszDeclarations.NumElements(); j++)
			{
				AddDeclaration(&asInserts, &aszInsertTexts, psCall, szFile, psInlining->aszDeclarations.GetText(j));
			}
			for (j = 0; j < psInlining->aszOwnParameters.NumElements(); j++)
			{
				if (!psCall->aszOwnNames.Get(j)->Empty())
				{
					szLine.Init(psInlining->aszOwnTypes.GetText(j));
					szLine.Append('\t')->Append(psCall->aszOwnNames.GetText(j))->Append(';');
					AddDeclaration(&asInserts, &aszInsertTexts, psCall, szFile, szLine.Text());
					szLine.Kill();
				}
			}
			if (!szVariable.Empty())
			{
				szLine.Init(psInlining->szReturnType);
				szLine.Append('\t')->Append(szVariable)->Append(';');
				AddDeclaration(&asInserts, &aszInsertTexts, psCall, szFile, szLine.Text());
				szLine.Kill();
			}
			szVariable.Kill();
		}
		szValue.Kill();
		uiInlined++;
	}
	for (i = 0; i < asHoists.NumElements(); i++)
	{
		//The statements taken out of a statement, before it: its indentation follows them.
		szFile = cFiles.GetText(asHoists.Get(i)->szFileName.Text());
		aszHoistTexts.Get(i)->AppendSubString(szFile, asHoists.Get(i)->uiStart, asHoists.Get(i)->uiEnd);
		cFiles.GetEdits(asHoists.Get(i)->szFileName.Text())->Replace(asHoists.Get(i)->uiStart, asHoists.Get(i)->uiEnd, aszHoistTexts.GetText(i));
	}
	for (i = 0; i < asInserts.NumElements(); i++)
	{
		//A blank line after them where the function had no declarations (its end says so).
		if (asInserts.Get(i)->uiEnd != 0)
		{
			aszInsertTexts.Get(i)->Append('\n');
		}
		cFiles.GetEdits(asInserts.Get(i)->szFileName.Text())->Insert(asInserts.Get(i)->uiStart, aszInsertTexts.GetText(i));
	}

	//The function, taken out: every declaration and definition of it, with the comments over them.
	bDeleted = szDefinition && (iMode == IDE_INLINE_ALL_AND_DELETE) && (uiStayed == 0) && (psInlining->sSignature.asOtherUses.NumElements() == 0);
	for (i = 0; bDeleted && (i < psInlining->sSignature.apsSites.NumElements()); i++)
	{
		psSite = psInlining->sSignature.apsSites.GetPtr(i);
		szFile = cFiles.GetText(psSite->szFileName.Text());
		if (szFile)
		{
			uiStart = FindIDETextOffset(szFile, psSite->sAll.iLine, psSite->sAll.iColumn);
			uiEnd = FindIDETextOffset(szFile, psSite->sAll.iEndLine, psSite->sAll.iEndColumn);
			ExtendIDEDeletion(szFile, &uiStart, &uiEnd, true);
			cFiles.GetEdits(psSite->szFileName.Text())->Replace(uiStart, uiEnd, "");
		}
	}
	if (szDefinition && ((uiInlined > 0) || bDeleted))
	{
		cFiles.Apply();
	}

	szMessage.Init("Inlined ");
	szMessage.Append(psInlining->sSignature.szName);
	szMessage.Append(": ");
	szMessage.Append((uint32)uiInlined);
	szMessage.Append((uiInlined == 1) ? " call" : " calls");
	if (uiStayed > 0)
	{
		szMessage.Append(", ");
		szMessage.Append((uint32)uiStayed);
		szMessage.Append(" left as they were");
	}
	szMessage.Append(bDeleted ? "; the function is taken out." : ".");
	if (pszStatus)
	{
		pszStatus->Set(szMessage);
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	aszHoisted.Kill();
	aszHoistTexts.Kill();
	KillIDEFileRanges(&asHoists);
	aszInsertTexts.Kill();
	KillIDEFileRanges(&asInserts);
	cFiles.Kill();
	return (uiInlined > 0) || bDeleted;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEInlineFunction::PrintInlining(SIDEFunctionInlining* psInlining, CChars* pszDest)
{
	static const char*	aszContexts[] = { "in an expression", "a statement", "assigned", "returned", "taken out of an expression" };
	SIDEInlineCall*		psCall;
	size				i;

	pszDest->Append("Inline Function: ")->Append(psInlining->sSignature.szName);
	if (!psInlining->bPossible)
	{
		pszDest->Append(" can't be inlined: ")->Append(psInlining->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	pszDest->Append(psInlining->bExpression ? ", an expression, " : ", statements, ");
	pszDest->Append((uint32)psInlining->apsCalls.NumElements());
	pszDest->Append((psInlining->apsCalls.NumElements() == 1) ? " call\n" : " calls\n");
	for (i = 0; i < psInlining->aszOwnParameters.NumElements(); i++)
	{
		if (!psInlining->aszOwnParameters.Get(i)->Empty())
		{
			pszDest->Append("  its parameter ")->Append(psInlining->aszOwnParameters.GetText(i))->Append(" is changed: it gets a variable\n");
		}
	}
	for (i = 0; i < psInlining->apsCalls.NumElements(); i++)
	{
		psCall = psInlining->apsCalls.GetPtr(i);
		pszDest->Append("  ");
		AppendPlace(psCall, pszDest);
		pszDest->Append(": ")->Append(aszContexts[(psCall->iContext == IDE_CALL_HOISTED) && psInlining->bExpression ? IDE_CALL_IN_EXPRESSION : psCall->iContext]);
		if (!psCall->szObject.Empty())
		{
			pszDest->Append(", on ")->Append(psCall->szObject);
		}
		pszDest->Append(psCall->bOutside ? ", outside the class" : "");
		pszDest->Append(psCall->bSelected ? ", at the caret\n" : "\n");
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Inline Method for the function at the caret: its dialog
// (which calls, and whether the function goes), Problems Detected if
// anything stands in the way, then the inlining.  False if nothing
// changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEInlineFunction::ShowInline(void)
{
	SIDEFunctionInlining	sInlining;
	CArrayChars				aszConflicts;
	Ptr<CComboBox>			pMode;
	CChars					szMessage;
	const char*				aszModes[3];
	int						iMode;
	bool					bResult;

	sInlining.Init();
	if (!FindInlining(&sInlining))
	{
		mpcContent->ShowStatus("There is no variable or function at the caret to inline.");
		sInlining.Kill();
		return false;
	}
	if (!sInlining.bPossible)
	{
		szMessage.Init("Can't inline ");
		szMessage.Append(sInlining.sSignature.szName);
		szMessage.Append(": ");
		szMessage.Append(sInlining.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sInlining.Kill();
		return false;
	}

	szMessage.Init("Inline ");
	szMessage.Append(sInlining.sSignature.szName);
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	aszModes[0] = "All calls, and take the function out";
	aszModes[1] = "All calls, and keep the function";
	aszModes[2] = "This call only";
	pMode = mpDialog->AddComboBox("Inline:", aszModes, sInlining.bSelectedCall ? 3 : 2, sInlining.bSelectedCall ? IDE_INLINE_THIS : IDE_INLINE_ALL_AND_DELETE);
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(480);
	iMode = bResult ? pMode->GetSelected() : IDE_INLINE_ALL;
	pMode = NULL;
	mpDialog = NULL;
	if (bResult)
	{
		aszConflicts.Init();
		FindConflicts(&sInlining, iMode, &aszConflicts);
		bResult = (aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&aszConflicts);
		aszConflicts.Kill();
		bResult = bResult && Inline(&sInlining, iMode);
	}
	sInlining.Kill();
	return bResult;
}
