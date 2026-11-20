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
#include "BaseLib/StringHelper.h"
#include "BaseLib/PointerFunctions.h"
#include "CppCompilerLib/CPPSyntaxes.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEMemberQualifier.h"
#include "IDEContextActions.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDECaretContext::Init(void)
{
	memset(this, 0, sizeof(SIDECaretContext));
	szFileName.Init();
	szKey.Init();
	szClassKey.Init();
	aszCommands.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDECaretContext::Kill(void)
{
	aszCommands.Kill();
	szClassKey.Kill();
	szKey.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A refactoring's command is for what is at the caret.
//
//////////////////////////////////////////////////////////////////////////
void SIDECaretContext::Offer(const char* szCommand)
{
	if (aszCommands.GetIndex(szCommand) == ARRAY_ELEMENT_NOT_FOUND)
	{
		aszCommands.Add(szCommand);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContextActions::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpPopup = NULL;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContextActions::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpPopup);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEContextActions::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContextActions::Save(CObjectWriter* pcFile) { return false; }
bool CIDEContextActions::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Show Context Actions and its key.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContextActions::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.ContextActions", "Show Context Actions", "Alt+Enter", Ptr<CImageCel>(), this, &CIDEContextActions::QueueShow)->SetEnabledCheck(this, &CIDEContextActions::CanShow);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContextActions::CanShow(void)
{
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull();
}


void CIDEContextActions::QueueShow(void) { mpcContent->GetWindow()->Post(this, &CIDEContextActions::ShowQueued); }
void CIDEContextActions::ShowQueued(void) { Show(); }
Ptr<CMenuContainer> CIDEContextActions::GetPopup(void) const { return mpPopup; }


//////////////////////////////////////////////////////////////////////////
//
// The refactorings for what is at the caret, in a popup under it: each
// is its command's item, so choosing one runs it as its key would.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContextActions::Show(void)
{
	SIDECaretContext	sContext;
	Ptr<CCommand>		pCommand;
	size				i;

	sContext.Init();
	if (FindActions(&sContext) == 0)
	{
		mpcContent->ShowStatus("There is nothing to refactor at the caret.");
		sContext.Kill();
		return false;
	}
	if (mpPopup.IsNull())
	{
		mpPopup = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	}
	mpPopup->RemoveAllItems();
	for (i = 0; i < sContext.aszCommands.NumElements(); i++)
	{
		pCommand = mpcContent->GetCommands()->Find(sContext.aszCommands.GetText(i));
		if (pCommand.IsNotNull())
		{
			mpPopup->AddItem(pCommand);
		}
	}
	sContext.Kill();
	mpcContent->GetDocuments()->OpenCaretMenu(mpPopup, 0);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What is at the caret, and the commands of the refactorings that are
// for it (psDest->aszCommands): how many.
//
//////////////////////////////////////////////////////////////////////////
size CIDEContextActions::FindActions(SIDECaretContext* psDest)
{
	if (!FindContext(psDest))
	{
		return 0;
	}
	mpcContent->GetRename()->AddContextActions(psDest);
	if (psDest->bCode)
	{
		mpcContent->GetChangeSignature()->AddContextActions(psDest);
		mpcContent->GetIntroduceVariable()->AddContextActions(psDest);
		mpcContent->GetIntroduceTypedef()->AddContextActions(psDest);
		mpcContent->GetExtractMethod()->AddContextActions(psDest);
		mpcContent->GetInlineVariable()->AddContextActions(psDest);
		mpcContent->GetDefinitions()->AddContextActions(psDest);
		mpcContent->GetMemberQualifier()->AddContextActions(psDest);
		mpcContent->GetConvertMember()->AddContextActions(psDest);
		mpcContent->GetConvertPointer()->AddContextActions(psDest);
		mpcContent->GetConvertEnum()->AddContextActions(psDest);
		mpcContent->GetTemplateInterface()->AddContextActions(psDest);
		mpcContent->GetEncapsulateField()->AddContextActions(psDest);
		mpcContent->GetMoveClass()->AddContextActions(psDest);
		mpcContent->GetMoveNamespace()->AddContextActions(psDest);
		mpcContent->GetMoveMembers()->AddContextActions(psDest);
		mpcContent->GetExtractClass()->AddContextActions(psDest);
		mpcContent->GetTemplateParameter()->AddContextActions(psDest);
		mpcContent->GetConvertMacro()->AddContextActions(psDest);
		mpcContent->GetIncludes()->AddContextActions(psDest);
		mpcContent->GetSafeDelete()->AddContextActions(psDest);
	}
	return psDest->aszCommands.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// What is at the active editor's caret.  False if there is no editor.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContextActions::FindContext(SIDECaretContext* psDest)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	EIDEFileType		eType;
	const char*			szText;
	size				uiStart;
	size				uiEnd;
	size				uiLine;
	size				uiEndLine;
	size				uiAt;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	pEditor = pTab->GetEditor();
	psDest->szFileName.Set(pTab->GetFileName());
	eType = CIDEIcons::GetFileType(pTab->GetFileName());
	psDest->bCode = (eType == IFT_CPP) || (eType == IFT_C) || (eType == IFT_Header);
	psDest->bSelection = pEditor->GetSelection(&uiStart, &uiEnd);
	if (!psDest->bSelection)
	{
		uiStart = pEditor->GetCaretPosition();
		uiEnd = uiStart;
	}
	uiLine = pEditor->FindLine(uiStart);
	uiEndLine = pEditor->FindLine(uiEnd);
	psDest->iLine = (int)uiLine;
	psDest->iColumn = (int)(uiStart - pEditor->GetLineStart(uiLine));
	psDest->iEndLine = (int)uiEndLine;
	psDest->iEndColumn = (int)(uiEnd - pEditor->GetLineStart(uiEndLine));
	if (!psDest->bCode)
	{
		return true;
	}

	szText = pEditor->GetText();
	for (uiAt = pEditor->GetLineStart(uiLine); (szText[uiAt] == ' ') || (szText[uiAt] == '\t'); uiAt++)
	{
	}
	if (szText[uiAt] == '#')
	{
		for (uiAt++; (szText[uiAt] == ' ') || (szText[uiAt] == '\t'); uiAt++)
		{
		}
		psDest->bInclude = (strncmp(&szText[uiAt], "include", 7) == 0);
	}

	//A selection is of a symbol if it is its name and no more.
	for (uiAt = uiStart; (uiAt < uiEnd) && IsIDENameChar(szText[uiAt]); uiAt++)
	{
	}
	if (!psDest->bSelection || ((uiAt == uiEnd) && ((uiStart == 0) || !IsIDENameChar(szText[uiStart - 1])) && !IsIDENameChar(szText[uiEnd])))
	{
		FindSymbol(psDest);
	}
	FindSyntax(psDest, szText, uiStart);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol named at the caret, from the index: what kind it is, its
// class, and how it is declared (a function's static, virtual and
// override, and a variable's * or &, are read from the line that
// declares it).
//
//////////////////////////////////////////////////////////////////////////
void CIDEContextActions::FindSymbol(SIDECaretContext* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CArrayChars			aszClasses;
	CChars				szScope;
	CChars				szName;
	CChars				szScopeName;
	CTextFile			cFile;
	const char*			szText;
	size				uiLength;
	size				uiName;
	size				i;
	int					iLine;
	int					iColumn;
	uint8				uiKind;
	bool				bScopeClass;
	bool				bVariable;
	bool				bRead;

	pcModel = mpcContent->GetCodeModel();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &psDest->uiRole) || !pcModel->GetSymbolAt(psDest->szFileName.Text(), iLine, iColumn, &psDest->szKey, &psDest->uiKind))
	{
		return;
	}
	psDest->bSymbol = true;
	psDest->bFunction = (psDest->uiKind == CPPSK_Function) || (psDest->uiKind == CPPSK_FunctionTemplate);
	bVariable = (psDest->uiKind == CPPSK_Variable) || (psDest->uiKind == CPPSK_Parameter) || (psDest->uiKind == CPPSK_Field);

	szScope.Init();
	GetIDESymbolScope(psDest->szKey.Text(), &szScope);
	asLocations.Init();
	bScopeClass = !szScope.Empty() && pcModel->GetSymbolLocations(szScope.Text(), &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate));
	KillIDELocations(&asLocations);
	if ((psDest->uiKind == CPPSK_Class) || (psDest->uiKind == CPPSK_ClassTemplate))
	{
		psDest->bClass = true;
		psDest->szClassKey.Set(psDest->szKey);
	}
	else if (bScopeClass && (psDest->bFunction || (psDest->uiKind == CPPSK_Field)))
	{
		psDest->bMember = true;
		if (psDest->uiRole != CPPIR_Reference)
		{
			psDest->szClassKey.Set(szScope);
		}
	}
	if (!psDest->szClassKey.Empty())
	{
		aszClasses.Init();
		pcModel->FindClassBases(psDest->szClassKey.Text(), false, &aszClasses);
		psDest->uiBases = aszClasses.NumElements();
		aszClasses.Kill();
		aszClasses.Init();
		pcModel->FindClassDerived(psDest->szClassKey.Text(), false, &aszClasses);
		psDest->uiDerived = aszClasses.NumElements();
		aszClasses.Kill();
	}

	bRead = false;
	asLocations.Init();
	pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			continue;
		}
		if (psDest->bFunction)
		{
			psDest->bDefined |= (psLocation->uiRole == CPPIR_Definition);
			psDest->bHeaderDefined |= (psLocation->uiRole == CPPIR_Definition) && (CIDEIcons::GetFileType(psLocation->szFileName.Text()) == IFT_Header);
			psDest->bDeclared |= (psLocation->uiRole != CPPIR_Definition);

			//One that overrides is virtual without saying so: "Fill(SLevel* psLevel) override;".
			cFile.Init();
			if (mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength))
			{
				for (uiName = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn); (szText[uiName] != '\0') && (szText[uiName] != ';') && (szText[uiName] != '{'); uiName++)
				{
					if (!IsIDENameChar(szText[uiName - 1]) && (((strncmp(&szText[uiName], "override", 8) == 0) && !IsIDENameChar(szText[uiName + 8])) ||
															   ((strncmp(&szText[uiName], "final", 5) == 0) && !IsIDENameChar(szText[uiName + 5]))))
					{
						psDest->bVirtual = true;
					}
				}
			}
			cFile.Kill();
		}
		else if (bVariable && !bRead)
		{
			//What is written before its name: "CVat* pcVat", "SLevel& sLevel".
			bRead = true;
			cFile.Init();
			if (mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength))
			{
				uiName = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
				for (; (uiName > 0) && ((szText[uiName - 1] == ' ') || (szText[uiName - 1] == '\t')); uiName--)
				{
				}
				psDest->bPointer = (uiName > 0) && (szText[uiName - 1] == '*');
				psDest->bReference = (uiName > 0) && (szText[uiName - 1] == '&') && ((uiName < 2) || (szText[uiName - 2] != '&'));
			}
			cFile.Kill();
		}
	}
	KillIDELocations(&asLocations);

	if (psDest->bFunction)
	{
		szName.Init();
		szScopeName.Init();
		GetIDESymbolName(psDest->szKey.Text(), &szName);
		GetIDESymbolName(szScope.Text(), &szScopeName);
		psDest->bSpecial = psDest->bMember && (szName.Equals(szScopeName.Text()) || szName.StartsWith("~"));
		psDest->bStatic = psDest->bMember && mpcContent->GetMemberQualifier()->IsDeclaredWith(psDest->szKey.Text(), "static");
		psDest->bVirtual = psDest->bMember && (psDest->bVirtual || mpcContent->GetMemberQualifier()->IsDeclaredWith(psDest->szKey.Text(), "virtual"));
		psDest->bConst = (strstr(psDest->szKey.Text(), ") const") != NULL);
		psDest->bParameters = (strstr(psDest->szKey.Text(), "()") == NULL) && (strstr(psDest->szKey.Text(), "(void)") == NULL);
		szScopeName.Kill();
		szName.Kill();
	}
	szScope.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContextActions::IsStatement(CCPPSyntax* pcNode)
{
	return (pcNode->GetKind() >= CPPS_CompoundStatement) && (pcNode->GetKind() <= CPPS_SkippedStatement);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a token is a word of one of the language's own types.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEContextActions::IsTypeWord(CIDERefactorUnit* pcUnit, int32 iToken)
{
	static const char*	aszWords[] = { "int", "char", "short", "long", "unsigned", "signed", "float", "double", "bool", "wchar_t" };
	size				i;

	for (i = 0; i < sizeof(aszWords) / sizeof(char*); i++)
	{
		if (pcUnit->IsToken(iToken, aszWords[i]))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What the caret is in and what the selection is, from the file's syntax
// (parsed as its project builds it, and not checked: this is asked for at
// a key).  A macro's name is no token of the parsed file: what was made
// from it is found by where it is written.
//
//////////////////////////////////////////////////////////////////////////
void CIDEContextActions::FindSyntax(SIDECaretContext* psDest, const char* szText, size uiCaret)
{
	CIDERefactorUnit				cUnit;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcStart;
	CCPPSyntax*						pcEnd;
	CCPPSyntax*						pcWalk;
	size							uiWordStart;
	size							uiWordEnd;
	size							uiAt;
	int32							iToken;
	int32							iFirst;
	int32							iLast;
	int32							i;
	int								iWordColumn;
	bool							bQualified;
	bool							bSelected;

	cUnit.Init();
	if (!cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), psDest->szFileName.Text(), false))
	{
		cUnit.Kill();
		return;
	}
	psDest->bParsed = true;

	iToken = cUnit.FindTokenAt(psDest->iLine, psDest->iColumn);
	iFirst = iToken;
	iLast = iToken;
	bSelected = psDest->bSelection && cUnit.FindTokensIn(psDest->iLine, psDest->iColumn, psDest->iEndLine, psDest->iEndColumn, &iFirst, &iLast);
	if (psDest->bSelection && !bSelected)
	{
		cUnit.Kill();
		return;
	}
	pcNode = (iFirst != -1) ? cUnit.FindNodeOfTokens(iFirst, iFirst) : NULL;
	pcStart = pcNode;
	pcFunction = pcNode ? (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcNode, CPPS_FunctionDefinition) : NULL;
	if (pcFunction && pcFunction->mpcBody && (pcFunction->mpcBody->GetKind() == CPPS_CompoundStatement))
	{
		//A member function's: it is in its class, or its name is written with it ("CVat::Fill").
		bQualified = false;
		if (pcFunction->mpcDeclarator && pcFunction->mpcDeclarator->mpcName)
		{
			for (i = pcFunction->mpcDeclarator->mpcName->miFirstToken; i <= pcFunction->mpcDeclarator->mpcName->miLastToken; i++)
			{
				bQualified |= cUnit.IsToken(i, "::");
			}
			psDest->bUndeclaredFunction = !psDest->bSymbol && !psDest->bSelection && bQualified && (iFirst >= pcFunction->mpcDeclarator->mpcName->miFirstToken) &&
										  (iFirst <= pcFunction->mpcDeclarator->mpcName->miLastToken);
		}
		psDest->bInFunction = (iFirst > pcFunction->mpcBody->miFirstToken) && (iLast < pcFunction->mpcBody->miLastToken);
		psDest->bInMemberFunction = psDest->bInFunction && (bQualified || (cUnit.FindAncestor(pcFunction, CPPS_ClassSpecifier) != NULL));
	}

	if (bSelected)
	{
		//An expression, whole statements, or something of a type.
		pcWalk = cUnit.FindNodeOfTokens(iFirst, iLast);
		while (pcWalk && !cUnit.IsExpression(pcWalk) && pcWalk->mpcParent && (pcWalk->mpcParent->miFirstToken == iFirst) && (pcWalk->mpcParent->miLastToken == iLast))
		{
			pcWalk = pcWalk->mpcParent;
		}
		psDest->bExpression = pcWalk && cUnit.IsExpression(pcWalk) && (pcWalk->miFirstToken == iFirst) && (pcWalk->miLastToken == iLast);
		if (!psDest->bExpression)
		{
			for (pcWalk = pcStart; pcWalk && !(IsStatement(pcWalk) && (pcWalk->miFirstToken == iFirst)); pcWalk = pcWalk->mpcParent)
			{
			}
			for (pcEnd = cUnit.FindNodeOfTokens(iLast, iLast); pcEnd && !(IsStatement(pcEnd) && (pcEnd->miLastToken == iLast)); pcEnd = pcEnd->mpcParent)
			{
			}
			psDest->bStatements = pcWalk && pcEnd;
			for (pcWalk = pcStart; pcWalk && (pcWalk->GetKind() != CPPS_DeclSpecifiers) && (pcWalk->GetKind() != CPPS_TypeId) && !IsStatement(pcWalk); pcWalk = pcWalk->mpcParent)
			{
			}
			psDest->bTypeSelected = !psDest->bStatements && pcWalk && !IsStatement(pcWalk);
		}
	}
	else if (pcNode)
	{
		//A number or a string is an expression without being selected; a type's name is a type where it is used.
		psDest->bExpression = (pcNode->GetKind() == CPPS_LiteralExpression) || (pcNode->GetKind() == CPPS_StringExpression);
		for (pcWalk = pcNode; pcWalk && (pcWalk->GetKind() >= CPPS_DeclSpecifiers) && (pcWalk->GetKind() <= CPPS_NameComponent); pcWalk = pcWalk->mpcParent)
		{
			if (pcWalk->GetKind() == CPPS_NamedTypeSpecifier)
			{
				psDest->bType = psDest->bSymbol && (psDest->uiRole == CPPIR_Reference) && ((psDest->uiKind == CPPSK_Class) || (psDest->uiKind == CPPSK_Enum) || (psDest->uiKind == CPPSK_Typedef));
				break;
			}
			if (pcWalk->GetKind() == CPPS_KeywordSpecifier)
			{
				psDest->bType = IsTypeWord(&cUnit, iToken);
				break;
			}
		}
	}

	if (!psDest->bSymbol && !psDest->bSelection)
	{
		//The word the caret is on: a macro's use, or its #define's name.
		for (uiWordStart = uiCaret; (uiWordStart > 0) && IsIDENameChar(szText[uiWordStart - 1]); uiWordStart--)
		{
		}
		for (uiWordEnd = uiCaret; IsIDENameChar(szText[uiWordEnd]); uiWordEnd++)
		{
		}
		if (uiWordEnd > uiWordStart)
		{
			iWordColumn = psDest->iColumn - (int)(uiCaret - uiWordStart);
			psDest->bMacro = cUnit.FindExpansionTokens(psDest->iLine, iWordColumn, psDest->iLine, iWordColumn + (int)(uiWordEnd - uiWordStart), &iFirst, &iLast);
			if (!psDest->bMacro)
			{
				for (uiAt = uiCaret - (size)psDest->iColumn; (szText[uiAt] == ' ') || (szText[uiAt] == '\t'); uiAt++)
				{
				}
				if (szText[uiAt] == '#')
				{
					for (uiAt++; (szText[uiAt] == ' ') || (szText[uiAt] == '\t'); uiAt++)
					{
					}
					if (strncmp(&szText[uiAt], "define", 6) == 0)
					{
						for (uiAt += 6; (szText[uiAt] == ' ') || (szText[uiAt] == '\t'); uiAt++)
						{
						}
						psDest->bMacro = (uiAt == uiWordStart);
					}
				}
			}
		}
	}
	cUnit.Kill();
}
