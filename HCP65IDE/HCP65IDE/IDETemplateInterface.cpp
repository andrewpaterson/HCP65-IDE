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
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "CppCompilerLib/CPPSyntaxes.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEDefinitions.h"
#include "IDEMoveMembers.h"
#include "IDEMoveClass.h"
#include "IDEIncludes.h"
#include "IDEScript.h"
#include "IDEContextActions.h"
#include "IDETemplateInterface.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETemplateInterface::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szFileName.Init();
	szTemplate.Init();
	aszParameters.Init();
	aszMemberParameters.Init();
	aszMemberNames.Init();
	aiMemberCalled.Init();
	aszMemberDeclarations.Init();
	aiMemberFunctions.Init();
	aszMemberKeys.Init();
	uiAt = 0;
	szIndent.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETemplateInterface::Kill(void)
{
	szIndent.Kill();
	aszMemberKeys.Kill();
	aiMemberFunctions.Kill();
	aszMemberDeclarations.Kill();
	aiMemberCalled.Kill();
	aszMemberNames.Kill();
	aszMemberParameters.Kill();
	aszParameters.Kill();
	szTemplate.Kill();
	szFileName.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::Init(CIDEContent* pcContent)
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
void CIDETemplateInterface::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpKindBox);
	M_Pointer(mpDeriveBox);
	M_Pointer(mpNameField);
	M_Pointer(mpError);
	U_Pointer(mpsShown);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::Free(void)
{
}


bool CIDETemplateInterface::Save(CObjectWriter* pcFile) { return false; }
bool CIDETemplateInterface::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Refactor's Write Template's Interface.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.TemplateInterface", "Write Template's Interface...", "", Ptr<CImageCel>(), this, &CIDETemplateInterface::QueueShow)->SetEnabledCheck(this, &CIDETemplateInterface::CanWrite);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.TemplateInterface"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: it is for a template's name or its parameter.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bSymbol && ((psContext->uiKind == CPPSK_ClassTemplate) || (psContext->uiKind == CPPSK_FunctionTemplate) || (psContext->uiKind == CPPSK_TypeParameter)))
	{
		psContext->Offer("Refactor.TemplateInterface");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::CanWrite(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }
void CIDETemplateInterface::QueueShow(void) { mpcContent->GetWindow()->Post(this, &CIDETemplateInterface::ShowQueued); }
void CIDETemplateInterface::ShowQueued(void) { Show(); }


//////////////////////////////////////////////////////////////////////////
//
// Returns true: a template was found, though nothing can be written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::Refuse(SIDETemplateInterface* psDest, const char* szWhyNot)
{
	psDest->bPossible = false;
	psDest->szWhyNot.Set(szWhyNot);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The outermost template declaration a node is in, NULL if it is in none.
//
//////////////////////////////////////////////////////////////////////////
static CCPPSyntaxTemplateDeclaration* FindIDEOuterTemplate(CCPPSyntax* pcNode)
{
	CCPPSyntaxTemplateDeclaration*	pcTemplate;

	pcTemplate = NULL;
	for (; pcNode; pcNode = pcNode->mpcParent)
	{
		if (pcNode->GetKind() == CPPS_TemplateDeclaration)
		{
			pcTemplate = (CCPPSyntaxTemplateDeclaration*)pcNode;
		}
	}
	return pcTemplate;
}


//////////////////////////////////////////////////////////////////////////
//
// What a template declaration declares, read from its tokens: its type
// parameters' names (an empty name for a parameter that isn't a type), and
// its owner: a class template's name ("template<class M> class CHolder"),
// the class template a function defined outside it is a member of
// ("template<class M> void CHolder<M>::Start(void)", *pbClass true for
// both), or a function template's name.
//
//////////////////////////////////////////////////////////////////////////
static void ReadIDETemplateHead(CIDERefactorUnit* pcUnit, CCPPSyntaxTemplateDeclaration* pcTemplate, CArrayChars* paszParameters, CChars* pszOwner, bool* pbClass)
{
	CCPPSyntax*	pcParameter;
	CChars		szName;
	size		i;
	int32		iToken;
	int32		iDepth;
	int32		iName;

	for (i = 0; i < pcTemplate->macpParameters.NumElements(); i++)
	{
		pcParameter = pcTemplate->macpParameters.GetPtr(i);
		szName.Init();
		if ((pcParameter->GetKind() == CPPS_TypeParameter) && (((CCPPSyntaxTypeParameter*)pcParameter)->miName >= 0))
		{
			pcUnit->AppendTokens(((CCPPSyntaxTypeParameter*)pcParameter)->miName, ((CCPPSyntaxTypeParameter*)pcParameter)->miName, &szName);
		}
		paszParameters->Add(szName.Text());
		szName.Kill();
	}

	//Past "template<...>".
	*pbClass = false;
	iDepth = 0;
	for (iToken = pcTemplate->miFirstToken; iToken <= pcTemplate->miLastToken; iToken++)
	{
		iDepth += pcUnit->IsToken(iToken, "<") ? 1 : (pcUnit->IsToken(iToken, ">") ? -1 : 0);
		if ((iDepth == 0) && pcUnit->IsToken(iToken, ">"))
		{
			break;
		}
	}
	iToken++;
	if ((iToken <= pcTemplate->miLastToken) && (pcUnit->IsToken(iToken, "class") || pcUnit->IsToken(iToken, "struct")))
	{
		*pbClass = true;
		if (iToken + 1 <= pcTemplate->miLastToken)
		{
			pcUnit->AppendTokens(iToken + 1, iToken + 1, pszOwner);
		}
		return;
	}

	//A function: the name before its "(", or the class template before "<...>::".
	iName = -1;
	for (; (iToken <= pcTemplate->miLastToken) && !pcUnit->IsToken(iToken, "(") && !pcUnit->IsToken(iToken, "{") && !pcUnit->IsToken(iToken, ";"); iToken++)
	{
		if (pcUnit->GetTokeniser()->Get((size)iToken)->IsIdentifier())
		{
			iName = iToken;
			if (pcUnit->IsToken(iToken + 1, "<"))
			{
				iDepth = 0;
				for (i = (size)iToken + 1; (int32)i <= pcTemplate->miLastToken; i++)
				{
					iDepth += pcUnit->IsToken((int32)i, "<") ? 1 : (pcUnit->IsToken((int32)i, ">") ? -1 : 0);
					if (iDepth == 0)
					{
						break;
					}
				}
				if (pcUnit->IsToken((int32)i + 1, "::"))
				{
					*pbClass = true;
					pcUnit->AppendTokens(iToken, iToken, pszOwner);
					return;
				}
			}
		}
	}
	if (iName >= 0)
	{
		pcUnit->AppendTokens(iName, iName, pszOwner);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Which of a template declaration's type parameters a name is declared
// with there ("M* mp", "M& cItem", "const M* pc"), read from its tokens;
// MAX_SIZE if none.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDEDeclaredParameter(CIDERefactorUnit* pcUnit, CCPPSyntaxTemplateDeclaration* pcTemplate, CArrayChars* paszParameters, const char* szName)
{
	CChars	szType;
	size	i;
	size	uiFound;
	int32	iToken;
	int32	iType;

	uiFound = MAX_SIZE;
	for (iToken = pcTemplate->miFirstToken + 1; (uiFound == MAX_SIZE) && (iToken <= pcTemplate->miLastToken); iToken++)
	{
		if (!pcUnit->IsToken(iToken, szName))
		{
			continue;
		}
		for (iType = iToken - 1; (iType > pcTemplate->miFirstToken) && (pcUnit->IsToken(iType, "*") || pcUnit->IsToken(iType, "&") || pcUnit->IsToken(iType, "&&") || pcUnit->IsToken(iType, "const")); iType--)
		{
		}
		szType.Init();
		pcUnit->AppendTokens(iType, iType, &szType);
		for (i = 0; i < paszParameters->NumElements(); i++)
		{
			if (!szType.Empty() && paszParameters->Get(i)->Equals(szType))
			{
				uiFound = i;
			}
		}
		szType.Kill();
	}
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// How the member named at a place is declared by the class it is in (one
// the template is used with: the index names it there), as it would be
// written in a class without virtual, static, override or a body:
// "void	Init(void)", "int	miCount".  False if the index knows of none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::FindDeclaration(const char* szFileName, int32 iLine, int32 iColumn, CChars* pszDest, bool* pbFunction, CChars* pszKey)
{
	static const char*	aszLeading[] = { "virtual", "static", "inline" };
	static const char*	aszTrailing[] = { "override", "final", "= 0", "=0" };
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	SIDELocation*		psDeclared;
	CTextFile			cFile;
	CChars				szKey;
	char*				szText;
	size				uiLength;
	size				uiStart;
	size				uiEnd;
	size				i;
	uint8				uiKind;
	bool				bChanged;

	pcModel = mpcContent->GetCodeModel();
	szKey.Init();
	if (!pcModel->GetSymbolAt(szFileName, iLine, iColumn, &szKey, &uiKind) || ((uiKind != CPPSK_Function) && (uiKind != CPPSK_Field) && (uiKind != CPPSK_Variable)))
	{
		szKey.Kill();
		return false;
	}
	*pbFunction = (uiKind == CPPSK_Function);
	pszKey->Set(szKey);
	asLocations.Init();
	pcModel->GetSymbolLocations(szKey.Text(), &asLocations, &uiKind);
	szKey.Kill();
	psDeclared = NULL;
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole == CPPIR_Declaration) || ((psLocation->uiRole == CPPIR_Definition) && !psDeclared))
		{
			psDeclared = psLocation;
			if (psLocation->uiRole == CPPIR_Declaration)
			{
				break;
			}
		}
	}
	cFile.Init();
	szText = NULL;
	if (!psDeclared || !mpcContent->GetFindInFiles()->GetFileText(psDeclared->szFileName.Text(), &cFile, &szText, &uiLength) || !szText)
	{
		cFile.Kill();
		KillIDELocations(&asLocations);
		return false;
	}

	//Its line, from its first word to its ; or its body.
	uiStart = FindIDETextOffset(szText, psDeclared->iLine, 0);
	for (; (szText[uiStart] == ' ') || (szText[uiStart] == '\t'); uiStart++)
	{
	}
	for (uiEnd = FindIDETextOffset(szText, psDeclared->iLine, psDeclared->iColumn); (szText[uiEnd] != '\0') && (szText[uiEnd] != ';') && (szText[uiEnd] != '{'); uiEnd++)
	{
	}
	pszDest->AppendSubString(szText, uiStart, uiEnd);
	pszDest->Replace("\r", "");
	pszDest->Replace("\n", " ");
	pszDest->StripWhitespace();
	do
	{
		bChanged = false;
		for (i = 0; i < sizeof(aszLeading) / sizeof(char*); i++)
		{
			uiLength = strlen(aszLeading[i]);
			if (pszDest->StartsWith(aszLeading[i]) && ((pszDest->Text()[uiLength] == ' ') || (pszDest->Text()[uiLength] == '\t')))
			{
				pszDest->RemoveFromStart(uiLength);
				pszDest->StripWhitespace();
				bChanged = true;
			}
		}
		for (i = 0; i < sizeof(aszTrailing) / sizeof(char*); i++)
		{
			if (pszDest->EndsWith(aszTrailing[i]))
			{
				pszDest->SetLength(pszDest->Length() - strlen(aszTrailing[i]));
				pszDest->StripWhitespace();
				bChanged = true;
			}
		}
	}
	while (bChanged);
	while (pszDest->Contains("\t\t"))
	{
		pszDest->Replace("\t\t", "\t");
	}
	cFile.Kill();
	KillIDELocations(&asLocations);
	return !pszDest->Empty();
}


//////////////////////////////////////////////////////////////////////////
//
// A member the template needs of a parameter, once for its name and how
// it is used.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::AddMember(SIDETemplateInterface* psDest, const char* szParameter, const char* szName, int iCalled, const char* szFileName, int32 iLine, int32 iColumn)
{
	CChars	szDeclaration;
	CChars	szKey;
	size	i;
	bool	bFunction;

	for (i = 0; i < psDest->aszMemberNames.NumElements(); i++)
	{
		if (psDest->aszMemberNames.Get(i)->Equals(szName) && psDest->aszMemberParameters.Get(i)->Equals(szParameter) && (psDest->aiMemberCalled.GetValue(i) == iCalled))
		{
			return;
		}
	}
	szDeclaration.Init();
	szKey.Init();
	bFunction = (iCalled != 0);
	if (!FindDeclaration(szFileName, iLine, iColumn, &szDeclaration, &bFunction, &szKey))
	{
		szDeclaration.Clear();
		szKey.Clear();
	}
	psDest->aszMemberParameters.Add(szParameter);
	psDest->aszMemberNames.Add(szName);
	psDest->aiMemberCalled.Add(iCalled);
	psDest->aszMemberDeclarations.Add(szDeclaration.Text());
	psDest->aiMemberFunctions.Add(bFunction ? 1 : 0);
	psDest->aszMemberKeys.Add(szKey.Text());
	szKey.Kill();
	szDeclaration.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What the template the caret is in needs of its parameters.  False if
// the caret is in no template; psDest->bPossible says whether there is
// anything to write, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::FindInterface(SIDETemplateInterface* psDest)
{
	Ptr<CIDEEditorTab>				pTab;
	Ptr<CTextArea>					pEditor;
	CIDERefactorUnit				cUnit;
	CCPPSyntaxTemplateDeclaration*	pcTemplate;
	CCPPSyntaxTemplateDeclaration*	pcOther;
	CCPPSyntaxMemberExpression*		pcMember;
	CCPPSyntaxCallExpression*		pcCall;
	CCPPSyntax*						pcNode;
	SPreprocessedToken*				psSource;
	SCPPSyntaxRange					sRange;
	CArrayChars						aszLocal;
	CChars							szOwner;
	CChars							szType;
	CChars							szName;
	char*							szText;
	size							uiCaret;
	size							uiLine;
	size							uiParameter;
	size							i;
	int32							iToken;
	int								iCalled;
	bool							bClass;
	bool							bOtherClass;
	bool							bOurs;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cUnit.Init();
	uiCaret = pEditor->GetCaretPosition();
	uiLine = pEditor->FindLine(uiCaret);
	pcTemplate = NULL;
	if (cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), pTab->GetFileName(), true))
	{
		pcTemplate = FindIDEOuterTemplate(cUnit.FindNodeAt((int)uiLine, (int)(uiCaret - pEditor->GetLineStart(uiLine))));

		//Between two things in it (an empty line of a class): the template of what comes next, if it began before the caret.
		for (iToken = 0; !pcTemplate && ((size)iToken < cUnit.GetTokeniser()->NumTokens()); iToken++)
		{
			psSource = cUnit.GetTokeniser()->GetSource(cUnit.GetTokeniser()->Get((size)iToken));
			if ((psSource->pcFile == cUnit.GetFile()) && (psSource->iExpansion == -1) && (psSource->iLine >= (int)uiLine))
			{
				pcTemplate = FindIDEOuterTemplate(cUnit.FindNodeOfTokens(iToken, iToken));
				if (pcTemplate && (!cUnit.GetTokensRange(pcTemplate->miFirstToken, pcTemplate->miFirstToken, &sRange) || (sRange.iLine > (int)uiLine)))
				{
					pcTemplate = NULL;
				}
				break;
			}
		}
	}
	if (!pcTemplate || !cUnit.GetTokensRange(pcTemplate->miFirstToken, pcTemplate->miFirstToken, &sRange))
	{
		cUnit.Kill();
		return false;
	}
	szText = pEditor->GetText();
	psDest->szFileName.Set(pTab->GetFileName());
	psDest->uiAt = FindIDETextOffset(szText, sRange.iLine, 0);
	psDest->szIndent.AppendSubString(szText, psDest->uiAt, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
	ReadIDETemplateHead(&cUnit, pcTemplate, &psDest->aszParameters, &psDest->szTemplate, &bClass);
	psDest->bPossible = true;

	//Every member named through one of its parameters, in it and in its functions defined outside it.
	for (iToken = 1; (size)iToken < cUnit.GetTokeniser()->NumTokens(); iToken++)
	{
		psSource = cUnit.GetTokeniser()->GetSource(cUnit.GetTokeniser()->Get((size)iToken));
		if ((psSource->pcFile != cUnit.GetFile()) || (psSource->iExpansion != -1) || !cUnit.GetTokeniser()->Get((size)iToken)->IsIdentifier() ||
			!(cUnit.IsToken(iToken - 1, "->") || cUnit.IsToken(iToken - 1, ".") || cUnit.IsToken(iToken - 1, "::")))
		{
			continue;
		}
		pcNode = cUnit.FindNodeOfTokens(iToken, iToken);
		pcOther = pcNode ? FindIDEOuterTemplate(pcNode) : NULL;
		if (!pcOther || !cUnit.GetTokensRange(iToken, iToken, &sRange))
		{
			continue;
		}
		aszLocal.Init();
		szOwner.Init();
		ReadIDETemplateHead(&cUnit, pcOther, &aszLocal, &szOwner, &bOtherClass);
		bOurs = (pcOther == pcTemplate) || (bClass && bOtherClass && szOwner.Equals(psDest->szTemplate));
		szType.Init();
		iCalled = 0;
		if (bOurs && cUnit.IsToken(iToken - 1, "::"))
		{
			//M::Name.
			if (iToken >= 2)
			{
				cUnit.AppendTokens(iToken - 2, iToken - 2, &szType);
			}
			iCalled = cUnit.IsToken(iToken + 1, "(") ? 1 : 0;
		}
		else if (bOurs)
		{
			pcMember = (CCPPSyntaxMemberExpression*)cUnit.FindAncestor(pcNode, CPPS_MemberExpression);
			if (pcMember && pcMember->mpcObject && pcMember->mpcName && (pcMember->mpcName->miFirstToken <= iToken) && (pcMember->mpcName->miLastToken >= iToken) &&
				cUnit.AppendDeclaredType(pcMember->mpcObject, &szType))
			{
				szType.Replace("const ", "");
				szType.Replace("*", "");
				szType.Replace("&", "");
				szType.StripWhitespace();
				pcCall = (pcMember->mpcParent && (pcMember->mpcParent->GetKind() == CPPS_CallExpression)) ? (CCPPSyntaxCallExpression*)pcMember->mpcParent : NULL;
				if (pcCall && (pcCall->mpcCallee == pcMember))
				{
					iCalled = (int)pcCall->macpArguments.NumElements() + 1;
				}
			}
			else
			{
				szType.Clear();
			}
		}
		uiParameter = MAX_SIZE;
		for (i = 0; !szType.Empty() && (i < aszLocal.NumElements()); i++)
		{
			if (aszLocal.Get(i)->Equals(szType))
			{
				uiParameter = i;
			}
		}
		if (bOurs && (uiParameter == MAX_SIZE) && !cUnit.IsToken(iToken - 1, "::"))
		{
			//What it is named through, a name alone (or this->name): how one of the template's declarations declares it.
			pcMember = (CCPPSyntaxMemberExpression*)cUnit.FindAncestor(pcNode, CPPS_MemberExpression);
			if (pcMember && pcMember->mpcObject && pcMember->mpcName && (pcMember->mpcName->miFirstToken <= iToken) && (pcMember->mpcName->miLastToken >= iToken) &&
				((pcMember->mpcObject->miFirstToken == pcMember->mpcObject->miLastToken) ||
				 ((pcMember->mpcObject->miLastToken == pcMember->mpcObject->miFirstToken + 2) && cUnit.IsToken(pcMember->mpcObject->miFirstToken, "this"))))
			{
				szName.Init();
				cUnit.AppendTokens(pcMember->mpcObject->miLastToken, pcMember->mpcObject->miLastToken, &szName);
				uiParameter = FindIDEDeclaredParameter(&cUnit, pcOther, &aszLocal, szName.Text());
				if ((uiParameter == MAX_SIZE) && (pcOther != pcTemplate))
				{
					uiParameter = FindIDEDeclaredParameter(&cUnit, pcTemplate, &psDest->aszParameters, szName.Text());
				}
				szName.Kill();
				pcCall = (pcMember->mpcParent && (pcMember->mpcParent->GetKind() == CPPS_CallExpression)) ? (CCPPSyntaxCallExpression*)pcMember->mpcParent : NULL;
				iCalled = (pcCall && (pcCall->mpcCallee == pcMember)) ? (int)pcCall->macpArguments.NumElements() + 1 : 0;
			}
		}
		if (bOurs && (uiParameter != MAX_SIZE) && (uiParameter < psDest->aszParameters.NumElements()) && !psDest->aszParameters.Get(uiParameter)->Empty())
		{
			szName.Init();
			cUnit.AppendTokens(iToken, iToken, &szName);
			AddMember(psDest, psDest->aszParameters.GetText(uiParameter), szName.Text(), iCalled, pTab->GetFileName(), sRange.iLine, sRange.iColumn);
			szName.Kill();
		}
		szType.Kill();
		szOwner.Kill();
		aszLocal.Kill();
	}
	cUnit.Kill();
	if (psDest->aszMemberNames.NumElements() == 0)
	{
		Refuse(psDest, "its body names no member of its parameters");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A member as it is shown and written: its declaration, or its name as
// the template uses it where no class says how it is declared.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::AppendMember(SIDETemplateInterface* psInterface, size uiMember, CChars* pszDest)
{
	int	i;

	if (!psInterface->aszMemberDeclarations.Get(uiMember)->Empty())
	{
		pszDest->Append(psInterface->aszMemberDeclarations.GetText(uiMember));
		return;
	}
	pszDest->Append(psInterface->aszMemberNames.GetText(uiMember));
	if (psInterface->aiMemberCalled.GetValue(uiMember) > 0)
	{
		pszDest->Append('(');
		for (i = 1; i < psInterface->aiMemberCalled.GetValue(uiMember); i++)
		{
			pszDest->Append((i > 1) ? ", ?" : "?");
		}
		pszDest->Append(')');
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A member after szBefore ("virtual " or nothing), its name in the
// column the longest type of them all puts the names in (tabs of four).
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateInterface::AppendAligned(SIDETemplateInterface* psInterface, size uiMember, const char* szBefore, CChars* pszDest)
{
	CChars	szMember;
	size	uiTab;
	size	uiColumn;
	size	uiLength;
	size	i;

	//The column: past the longest type, at the next tab stop.
	uiColumn = 0;
	for (i = 0; i < psInterface->aszMemberNames.NumElements(); i++)
	{
		uiTab = psInterface->aszMemberDeclarations.Get(i)->Find('\t');
		if (uiTab != ARRAY_ELEMENT_NOT_FOUND)
		{
			uiLength = strlen(szBefore) + uiTab;
			uiColumn = ((uiLength / 4 + 1) * 4 > uiColumn) ? (uiLength / 4 + 1) * 4 : uiColumn;
		}
	}
	szMember.Init();
	AppendMember(psInterface, uiMember, &szMember);
	uiTab = szMember.Find('\t');
	pszDest->Append(szBefore);
	if ((uiTab == ARRAY_ELEMENT_NOT_FOUND) || psInterface->aszMemberDeclarations.Get(uiMember)->Empty())
	{
		pszDest->Append(szMember);
	}
	else
	{
		pszDest->AppendSubString(szMember.Text(), 0, uiTab);
		for (uiLength = strlen(szBefore) + uiTab; uiLength < uiColumn; uiLength = (uiLength / 4 + 1) * 4)
		{
			pszDest->Append('\t');
		}
		pszDest->Append(&szMember.Text()[uiTab + 1]);
	}
	szMember.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Why it can't be written as asked, NULL if it can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDETemplateInterface::GetProblem(SIDETemplateInterface* psInterface, int iKind, const char* szName)
{
	size	i;

	if (!psInterface->bPossible)
	{
		return "There is nothing to write.";
	}
	if (iKind == IDE_INTERFACE_COMMENT)
	{
		return NULL;
	}
	for (i = 0; i < psInterface->aszMemberNames.NumElements(); i++)
	{
		if (!psInterface->aszMemberParameters.Get(i)->Equals(psInterface->aszMemberParameters.GetText(0)))
		{
			return "A class is one parameter's: the template needs members of more than one.";
		}
		if (psInterface->aszMemberDeclarations.Get(i)->Empty())
		{
			return "A class needs each member's declaration: no class the template is used with says how one is declared.";
		}
		if (psInterface->aiMemberFunctions.GetValue(i) == 0)
		{
			return "A class can ask for functions only: the template needs a data member.";
		}
	}
	if (!IsIDEIdentifier(szName))
	{
		return "The class's name isn't a name.";
	}
	if (mpcContent->GetCodeModel()->HasSymbol(szName))
	{
		return "Something has that name already.";
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The interface written: a comment over the template, or an abstract
// class before it (and before the comment over it).
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::Write(SIDETemplateInterface* psInterface, int iKind, const char* szName, CChars* pszStatus, bool bDerive)
{
	CIDEFileEdits	cFiles;
	CChars			szBlock;
	CChars			szMessage;
	const char*		szProblem;
	char*			szText;
	size			i;
	size			j;
	bool			bResult;
	bool			bFirst;
	CArrayChars		aszClasses;
	CChars			szDerived;

	szProblem = GetProblem(psInterface, iKind, szName);
	cFiles.Init(mpcContent);
	szText = szProblem ? NULL : cFiles.GetText(psInterface->szFileName.Text());
	bResult = (szText != NULL);
	szMessage.Init();
	if (!bResult)
	{
		szMessage.Append("Write Template's Interface: ")->Append(szProblem ? szProblem : "The file couldn't be opened.");
	}
	else if (iKind == IDE_INTERFACE_COMMENT)
	{
		szBlock.Init();
		for (i = 0; i < psInterface->aszParameters.NumElements(); i++)
		{
			bFirst = true;
			for (j = 0; j < psInterface->aszMemberNames.NumElements(); j++)
			{
				if (!psInterface->aszMemberParameters.Get(j)->Equals(psInterface->aszParameters.GetText(i)))
				{
					continue;
				}
				if (bFirst)
				{
					szBlock.Append(psInterface->szIndent)->Append("//")->Append(psInterface->aszParameters.GetText(i))->Append(" must have:\n");
					bFirst = false;
				}
				szBlock.Append(psInterface->szIndent)->Append("//\t");
				AppendAligned(psInterface, j, "", &szBlock);
				szBlock.Append(";\n");
			}
		}
		cFiles.GetEdits(psInterface->szFileName.Text())->Insert(psInterface->uiAt, szBlock.Text());
		szBlock.Kill();
		szMessage.Append("Wrote what ")->Append(psInterface->szTemplate)->Append(" needs of its parameters as a comment: ");
	}
	else
	{
		szBlock.Init(psInterface->szIndent);
		szBlock.Append("class ")->Append(szName)->Append('\n')->Append(psInterface->szIndent)->Append("{\n")->Append(psInterface->szIndent)->Append("public:\n");
		for (j = 0; j < psInterface->aszMemberNames.NumElements(); j++)
		{
			szBlock.Append(psInterface->szIndent)->Append('\t');
			AppendAligned(psInterface, j, "virtual ", &szBlock);
			szBlock.Append(" =0;\n");
		}
		szBlock.Append(psInterface->szIndent)->Append("};\n\n\n");
		cFiles.GetEdits(psInterface->szFileName.Text())->Insert(FindIDECommentStart(szText, psInterface->uiAt), szBlock.Text());
		szBlock.Kill();
		szMessage.Append("Wrote what ")->Append(psInterface->szTemplate)->Append(" needs of ")->Append(psInterface->aszMemberParameters.GetText(0))->Append(" as the class ")->Append(szName)->Append(": ");
		if (bDerive)
		{
			aszClasses.Init();
			bResult = Derive(psInterface, szName, &cFiles, &aszClasses);
			if (!bResult)
			{
				szMessage.Set("Write Template's Interface: the classes it is used with can't be made to derive from it (a class's definition can't be read, or isn't the solution's).");
			}
			else if (cFiles.CountErrors(true) > cFiles.CountErrors(false))
			{
				bResult = false;
				szMessage.Set("Write Template's Interface: the code doesn't compile with the classes it is used with derived from it.");
			}
			else
			{
				szDerived.Init();
				for (i = 0; i < aszClasses.NumElements(); i++)
				{
					szDerived.Append((i == 0) ? "" : ((i + 1 == aszClasses.NumElements()) ? " and " : ", "));
					GetIDESymbolName(aszClasses.GetText(i), &szDerived);
				}
				szDerived.Append((aszClasses.NumElements() == 1) ? " derives from it" : " derive from it");
			}
			aszClasses.Kill();
		}
	}
	if (bResult)
	{
		cFiles.Apply();
		i = psInterface->aszMemberNames.NumElements();
		szMessage.Append((uint32)i)->Append((i == 1) ? " member" : " members");
		if (bDerive && (iKind == IDE_INTERFACE_CLASS))
		{
			szMessage.Append("; ")->Append(szDerived);
			szDerived.Kill();
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
// The classes the template is used with (those its members' declarations
// are in) made to derive from the interface: ": public szName" after the
// class's name, or ", public szName" after its bases; each function the
// interface has marked override in it; and the class's file given an
// #include of the template's header if it doesn't reach it.  The classes'
// keys into paszClasses.  False if a class's definition can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::Derive(SIDETemplateInterface* psInterface, const char* szName, CIDEFileEdits* pcFiles, CArrayChars* paszClasses)
{
	CIDECodeModel*			pcModel;
	CIDEIncludeGraph		cGraph;
	CArrayIDELocations		asLocations;
	SIDELocation*			psLocation;
	SIDELocation*			psDefinition;
	CArrayIDEFileRanges		asInserts;
	CArrayChars				aszInsertTexts;
	CChars					szScope;
	CChars					szBase;
	char*					szText;
	size					uiAt;
	size					uiBrace;
	size					uiColon;
	size					i;
	size					j;
	uint8					uiKind;
	bool					bResult;

	pcModel = mpcContent->GetCodeModel();
	for (i = 0; i < psInterface->aszMemberKeys.NumElements(); i++)
	{
		szScope.Init();
		GetIDESymbolScope(psInterface->aszMemberKeys.GetText(i), &szScope);
		if (!szScope.Empty())
		{
			paszClasses->AddIfUnique(szScope.Text());
		}
		szScope.Kill();
	}
	bResult = (paszClasses->NumElements() > 0);
	cGraph.Init(pcModel, &mpcContent->GetFindInFiles());
	asInserts.Init();
	aszInsertTexts.Init();
	for (i = 0; bResult && (i < paszClasses->NumElements()); i++)
	{
		//Its definition: the name, then its bases or its brace.
		asLocations.Init();
		psDefinition = NULL;
		pcModel->GetSymbolLocations(paszClasses->GetText(i), &asLocations, &uiKind);
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			psLocation = asLocations.Get(j);
			if (psLocation->uiRole == CPPIR_Definition)
			{
				psDefinition = psLocation;
			}
		}
		szText = (psDefinition && (pcModel->FindProjectOfFile(psDefinition->szFileName.Text()) != MAX_SIZE)) ? pcFiles->GetText(psDefinition->szFileName.Text()) : NULL;
		bResult = (szText != NULL);
		if (bResult)
		{
			uiAt = FindIDETextOffset(szText, psDefinition->iLine, psDefinition->iColumn) + (size)psDefinition->iLength;
			uiColon = MAX_SIZE;
			for (uiBrace = uiAt; (szText[uiBrace] != '\0') && (szText[uiBrace] != '{') && (szText[uiBrace] != ';'); uiBrace++)
			{
				uiColon = ((szText[uiBrace] == ':') && (uiColon == MAX_SIZE)) ? uiBrace : uiColon;
			}
			bResult = (szText[uiBrace] == '{');
		}
		if (bResult)
		{
			szBase.Init();
			if (uiColon != MAX_SIZE)
			{
				//After the last base, before the space and line the brace is on.
				for (; (uiBrace > uiAt) && ((szText[uiBrace - 1] == ' ') || (szText[uiBrace - 1] == '\t') || (szText[uiBrace - 1] == '\r') || (szText[uiBrace - 1] == '\n')); uiBrace--)
				{
				}
				szBase.Append(", public ")->Append(szName);
				pcFiles->GetEdits(psDefinition->szFileName.Text())->Insert(uiBrace, szBase.Text());
			}
			else
			{
				szBase.Append(" : public ")->Append(szName);
				pcFiles->GetEdits(psDefinition->szFileName.Text())->Insert(uiAt, szBase.Text());
			}
			szBase.Kill();
			if (!cGraph.Reaches(cGraph.Get(psDefinition->szFileName.Text()), cGraph.Get(psInterface->szFileName.Text()), -1, NULL))
			{
				mpcContent->GetMoveClass()->AddInclude(psDefinition->szFileName.Text(), "", psInterface->szFileName.Text(), pcFiles, &asInserts, &aszInsertTexts);
			}
		}
		KillIDELocations(&asLocations);
	}
	for (i = 0; bResult && (i < asInserts.NumElements()); i++)
	{
		pcFiles->GetEdits(asInserts.Get(i)->szFileName.Text())->Insert(asInserts.Get(i)->uiStart, aszInsertTexts.GetText(i));
	}

	//Its functions in each class, marked override.
	for (i = 0; bResult && (i < psInterface->aszMemberKeys.NumElements()); i++)
	{
		if (!psInterface->aszMemberKeys.Get(i)->Empty())
		{
			mpcContent->GetMoveMembers()->MarkOverride(psInterface->aszMemberKeys.GetText(i), pcFiles);
		}
	}
	mpcContent->GetMoveMembers()->KillUnits();
	aszInsertTexts.Kill();
	KillIDEFileRanges(&asInserts);
	cGraph.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDETemplateInterface::PrintInterface(SIDETemplateInterface* psInterface, CChars* pszDest)
{
	size	i;

	pszDest->Append("Template Interface: ")->Append(psInterface->szTemplate);
	if (!psInterface->bPossible)
	{
		pszDest->Append(" has none: ")->Append(psInterface->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	pszDest->Append(", ")->Append((uint32)psInterface->aszMemberNames.NumElements())->Append((psInterface->aszMemberNames.NumElements() == 1) ? " member\n" : " members\n");
	for (i = 0; i < psInterface->aszMemberNames.NumElements(); i++)
	{
		pszDest->Append("  ")->Append(psInterface->aszMemberParameters.GetText(i))->Append(": ");
		AppendMember(psInterface, i, pszDest);
		pszDest->Append('\n');
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::Approve(void)
{
	const char*	szProblem;

	szProblem = GetProblem(mpsShown, (int)mpKindBox->GetSelected(), mpNameField->GetText());
	mpError->SetText(szProblem ? szProblem : "");
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The command: what the template at the caret needs of its parameters is
// shown, with how to write it; then it is written.  False if nothing
// changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateInterface::Show(void)
{
	static const char*		aszKinds[] = { "A comment over the template", "An abstract class before it" };
	SIDETemplateInterface	sInterface;
	Ptr<CLabel>				pLabel;
	CChars					szText;
	size					i;
	int						iKind;
	bool					bResult;
	bool					bDerive;

	sInterface.Init();
	if (!FindInterface(&sInterface))
	{
		mpcContent->ShowStatus("The caret isn't in a template.");
		sInterface.Kill();
		return false;
	}
	if (!sInterface.bPossible)
	{
		szText.Init(sInterface.szTemplate);
		szText.Append(" has no interface to write: ")->Append(sInterface.szWhyNot)->Append('.');
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
		sInterface.Kill();
		return false;
	}

	mpsShown = &sInterface;
	szText.Init("What ");
	szText.Append(sInterface.szTemplate)->Append(" Needs of Its Parameters");
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szText.Text());
	szText.Kill();
	for (i = 0; i < sInterface.aszMemberNames.NumElements(); i++)
	{
		szText.Init();
		AppendMember(&sInterface, i, &szText);
		szText.Replace("\t", " ");
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetTheme()->GetFont(), szText.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		szText.Set(sInterface.aszMemberParameters.GetText(i));
		szText.Append(":");
		mpDialog->AddEditor(szText.Text(), pLabel);
		szText.Kill();
	}
	pLabel = NULL;
	mpKindBox = mpDialog->AddComboBox("Write as:", aszKinds, 2, 0);
	szText.Init("C");
	szText.Append((sInterface.szTemplate.Text()[0] == 'C') ? &sInterface.szTemplate.Text()[1] : sInterface.szTemplate.Text())->Append("Interface");
	mpNameField = mpDialog->AddTextField("Class's name:", szText.Text());
	szText.Kill();
	mpDeriveBox = mpDialog->AddCheckBox("Make the classes it is used with derive from it (each has a virtual table then)", false);
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Write");
	mpDialog->SetApprover(this, &CIDETemplateInterface::Approve);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(560);
	iKind = (int)mpKindBox->GetSelected();
	bDerive = mpDeriveBox->IsTicked();
	szText.Init(mpNameField->GetText());
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpKindBox = NULL;
	mpDeriveBox = NULL;
	mpNameField = NULL;
	mpError = NULL;
	mpDialog = NULL;
	mpsShown = NULL;
	bResult = bResult && Write(&sInterface, iKind, szText.Text(), NULL, bDerive && (iKind == IDE_INTERFACE_CLASS));
	szText.Kill();
	sInterface.Kill();
	return bResult;
}
