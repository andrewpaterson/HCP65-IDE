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
#include "IDEChangeSignature.h"
#include "IDEDefinitions.h"
#include "IDEIncludes.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEMoveMembers.h"
#include "IDEIntroduceVariable.h"
#include "IDEIntroduceTypedef.h"
#include "IDEContextActions.h"


static const char*	gaszIDETypeKeywords[] = { "unsigned", "signed", "int", "char", "long", "short", "float", "double", "bool", "void", "const", "volatile", "struct", "class", "typename", "wchar_t" };
static const char*	gaszIDETypePunctuation[] = { "::", "<", ">", ",", "*", "&" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETypedef::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szFileName.Init();
	szType.Init();
	iPlace = IDE_TYPEDEF_FILE;
	aiStarts.Init();
	aiEnds.Init();
	uiSelected = 0;
	aiTypedefAt.Init();
	szIndent.Init();
	aszUsed.Init();
	aszTokens.Init();
	bHeader = false;
	aszOtherFiles.Init();
	aiOtherStarts.Init();
	aiOtherEnds.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETypedef::Kill(void)
{
	aiOtherEnds.Kill();
	aiOtherStarts.Kill();
	aszOtherFiles.Kill();
	aszTokens.Kill();
	aszUsed.Kill();
	szIndent.Kill();
	aiTypedefAt.Kill();
	aiEnds.Kill();
	aiStarts.Kill();
	szType.Kill();
	szFileName.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpsShown = NULL;
	mbAll = true;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpNameField);
	M_Pointer(mpError);
	M_Pointer(mpAllBox);
	M_Pointer(mpIncludersBox);
	U_Pointer(mpsShown);
	U_Bool(mbAll);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::Save(CObjectWriter* pcFile) { return false; }
bool CIDEIntroduceTypedef::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Ctrl+Alt+K, as CLion's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.IntroduceTypedef", "Introduce Typedef...", "Control+Alt+K", Ptr<CImageCel>(), this, &CIDEIntroduceTypedef::QueueIntroduce)->SetEnabledCheck(this, &CIDEIntroduceTypedef::CanIntroduce);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.IntroduceTypedef"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: Introduce Typedef is for a type selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bTypeSelected)
	{
		psContext->Offer("Refactor.IntroduceTypedef");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::CanIntroduce(void)
{
	size	uiStart;
	size	uiEnd;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->GetActiveTab()->GetEditor()->GetSelection(&uiStart, &uiEnd);
}


void CIDEIntroduceTypedef::QueueIntroduce(void) { mpcContent->GetWindow()->Post(this, &CIDEIntroduceTypedef::ShowQueuedIntroduce); }
void CIDEIntroduceTypedef::ShowQueuedIntroduce(void) { ShowIntroduce(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::Refuse(SIDETypedef* psDest, const char* szWhyNot)
{
	psDest->bPossible = false;
	psDest->szWhyNot.Set(szWhyNot);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file is a header by its name: what other files #include.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEHeaderName(const char* szFileName)
{
	const char*	szDot;

	szDot = strrchr(szFileName, '.');
	return szDot && ((StringInsensitiveCompare(szDot, ".h") == 0) || (StringInsensitiveCompare(szDot, ".hpp") == 0) || (StringInsensitiveCompare(szDot, ".inl") == 0));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a token can be part of a type as it is written: a name, a
// number (a template's argument), a type's keyword, or :: < > , * &.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::IsTypeToken(CIDERefactorUnit* pcUnit, int32 iToken)
{
	SPreprocessedToken*	psSource;
	size				i;

	psSource = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)iToken));
	if ((psSource->uiLength > 0) && IsIDENameChar(psSource->szText[0]))
	{
		return true;
	}
	for (i = 0; i < sizeof(gaszIDETypePunctuation) / sizeof(char*); i++)
	{
		if (pcUnit->IsToken(iToken, gaszIDETypePunctuation[i]))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the tokens at iAt are, one for one, the type's, and none of
// them a macro's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::IsSameTokens(CIDERefactorUnit* pcUnit, CArrayChars* paszTokens, int32 iAt)
{
	SPreprocessedToken*	psOther;
	CChars*				pszToken;
	size				i;

	if ((size)iAt + paszTokens->NumElements() > pcUnit->GetTokeniser()->NumTokens())
	{
		return false;
	}
	for (i = 0; i < paszTokens->NumElements(); i++)
	{
		pszToken = paszTokens->Get(i);
		psOther = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)iAt + i));
		if ((pszToken->Length() != psOther->uiLength) || (memcmp(pszToken->Text(), psOther->szText, psOther->uiLength) != 0) || (psOther->iExpansion != -1))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the type is written at a token of the unit's own file: its
// tokens, not a macro's, and not a part of a longer name or type.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::IsTypeAt(CIDERefactorUnit* pcUnit, CArrayChars* paszTokens, int32 iToken)
{
	SPreprocessedToken*	psSource;
	SCPPSyntaxRange		sRange;
	int32				iSpan;

	iSpan = (int32)paszTokens->NumElements() - 1;
	if ((iSpan < 0) || ((size)(iToken + iSpan) >= pcUnit->GetTokeniser()->NumTokens()))
	{
		return false;
	}
	psSource = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)iToken));
	if ((psSource->pcFile != pcUnit->GetFile()) || (psSource->iExpansion != -1) || !IsSameTokens(pcUnit, paszTokens, iToken) || !pcUnit->GetTokensRange(iToken, iToken + iSpan, &sRange))
	{
		return false;
	}
	//Not a part of a longer name or type: X in N::X, X::Y, X<...>; int in "long int"; nor a constructor's name.
	if ((iToken > 0) && (pcUnit->IsToken(iToken - 1, "::") || pcUnit->IsToken(iToken - 1, ".") || pcUnit->IsToken(iToken - 1, "->") || pcUnit->IsToken(iToken - 1, "~") ||
		 pcUnit->IsToken(iToken - 1, "unsigned") || pcUnit->IsToken(iToken - 1, "signed") || pcUnit->IsToken(iToken - 1, "long") || pcUnit->IsToken(iToken - 1, "short") ||
		 pcUnit->IsToken(iToken - 1, "class") || pcUnit->IsToken(iToken - 1, "struct")))
	{
		return false;
	}
	if (pcUnit->IsToken(iToken + iSpan + 1, "::") || (!pcUnit->IsToken(iToken + iSpan, ">") && pcUnit->IsToken(iToken + iSpan + 1, "<")) ||
		((iSpan == 0) && pcUnit->IsToken(iToken + 1, "(")) || pcUnit->IsToken(iToken + iSpan + 1, "long") || pcUnit->IsToken(iToken + iSpan + 1, "int"))
	{
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What Introduce Typedef has to work with, for the active editor's
// selection.  False if nothing is selected; psDest->bPossible says
// whether it is a type that can be named, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::FindTypedef(SIDETypedef* psDest)
{
	CIDERefactorUnit				cUnit;
	Ptr<CIDEEditorTab>				pTab;
	Ptr<CTextArea>					pEditor;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcParent;
	CCPPSyntax*						pcScope;
	CCPPSyntax*						pcMember;
	CCPPSyntaxTemplateDeclaration*	pcTemplate;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	SPreprocessedToken*				psSource;
	SCPPSyntaxRange					sRange;
	CArrayChars						aszParameters;
	CChars							szName;
	char*							szText;
	size							uiStart;
	size							uiEnd;
	size							uiLine;
	size							uiEndLine;
	size							uiAt;
	size							i;
	int32							iFirst;
	int32							iLast;
	int32							iToken;
	int32							iScopeFirst;
	int32							iScopeLast;
	int32							iDepth;
	bool							bType;
	bool							bStars;
	bool							bParameter;
	bool							bName;
	bool							bKeyword;
	uint8							uiKind;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetEditor()->GetSelection(&uiStart, &uiEnd))
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	psDest->szFileName.Set(pTab->GetFileName());
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cUnit.Init();
	if (!cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), pTab->GetFileName(), false))
	{
		cUnit.Kill();
		return Refuse(psDest, "the file couldn't be parsed (it must be one of the solution's)");
	}
	szText = pEditor->GetText();
	uiLine = pEditor->FindLine(uiStart);
	uiEndLine = pEditor->FindLine(uiEnd);
	if (!cUnit.FindTokensIn((int)uiLine, (int)(uiStart - pEditor->GetLineStart(uiLine)), (int)uiEndLine, (int)(uiEnd - pEditor->GetLineStart(uiEndLine)), &iFirst, &iLast))
	{
		cUnit.Kill();
		return Refuse(psDest, "the selection isn't a type");
	}

	//A type as it is written: names, type keywords and :: < > , then * and & only at its end; and not an expression.
	bType = true;
	bStars = false;
	bName = false;
	iDepth = 0;
	for (iToken = iFirst; bType && (iToken <= iLast); iToken++)
	{
		bType = IsTypeToken(&cUnit, iToken);
		if (cUnit.IsToken(iToken, "*") || cUnit.IsToken(iToken, "&"))
		{
			bStars = bStars || (iDepth == 0);
		}
		else if (bStars && (iDepth == 0) && !cUnit.IsToken(iToken, "const"))
		{
			bType = false;
		}
		iDepth += cUnit.IsToken(iToken, "<") ? 1 : (cUnit.IsToken(iToken, ">") ? -1 : 0);
		bType = bType && (iDepth >= 0) && !((iDepth == 0) && cUnit.IsToken(iToken, ","));
		psSource = cUnit.GetTokeniser()->GetSource(cUnit.GetTokeniser()->Get((size)iToken));
		bName = bName || ((psSource->uiLength > 0) && IsIDENameChar(psSource->szText[0]) && !((psSource->szText[0] >= '0') && (psSource->szText[0] <= '9')));

		//A name in it is a type's: a class, an enum, a typedef, a template's parameter (the index says).
		if (bType && cUnit.GetTokeniser()->Get((size)iToken)->IsIdentifier())
		{
			bKeyword = false;
			for (i = 0; i < sizeof(gaszIDETypeKeywords) / sizeof(char*); i++)
			{
				bKeyword = bKeyword || cUnit.IsToken(iToken, gaszIDETypeKeywords[i]);
			}
			szName.Init();
			bType = bKeyword || (mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), psSource->iLine, psSource->iColumn, &szName, &uiKind) &&
								 ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate) || (uiKind == CPPSK_Enum) || (uiKind == CPPSK_Typedef) || (uiKind == CPPSK_TypeParameter) ||
								  (uiKind == CPPSK_AliasTemplate) || (uiKind == CPPSK_BuiltinType) || (uiKind == CPPSK_Namespace)));
			szName.Kill();
		}
	}
	pcNode = cUnit.FindNodeOfTokens(iFirst, iLast);
	if (!bType || !bName || (iDepth != 0) || cUnit.IsToken(iFirst, "*") || cUnit.IsToken(iFirst, "&") || cUnit.IsToken(iFirst, "<") || cUnit.IsToken(iFirst, "::") ||
		cUnit.IsToken(iLast, "::") || cUnit.IsToken(iLast, "<") || !pcNode || cUnit.IsExpression(pcNode))
	{
		cUnit.Kill();
		return Refuse(psDest, "the selection isn't a type");
	}
	for (pcParent = pcNode; pcParent; pcParent = pcParent->mpcParent)
	{
		if (cUnit.IsExpression(pcParent) && (pcParent->miFirstToken == iFirst) && (pcParent->miLastToken == iLast))
		{
			cUnit.Kill();
			return Refuse(psDest, "the selection is an expression, not a type");
		}
	}
	cUnit.GetTokensRange(iFirst, iLast, &sRange);
	psDest->szType.AppendSubString(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));

	for (iToken = iFirst; iToken <= iLast; iToken++)
	{
		szName.Init();
		cUnit.AppendTokens(iToken, iToken, &szName);
		psDest->aszTokens.Add(szName.Text());
		szName.Kill();
	}

	//Does it name a template's parameter?  Then it is named where the parameter is.
	aszParameters.Init();
	for (pcParent = pcNode; pcParent; pcParent = pcParent->mpcParent)
	{
		if (pcParent->GetKind() != CPPS_TemplateDeclaration)
		{
			continue;
		}
		pcTemplate = (CCPPSyntaxTemplateDeclaration*)pcParent;
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
			if (iToken >= 0)
			{
				szName.Init();
				cUnit.AppendTokens(iToken, iToken, &szName);
				aszParameters.AddIfUnique(szName.Text());
				szName.Kill();
			}
		}
	}
	bParameter = false;
	for (iToken = iFirst; iToken <= iLast; iToken++)
	{
		szName.Init();
		cUnit.AppendTokens(iToken, iToken, &szName);
		bParameter = bParameter || aszParameters.Contains(szName.Text());
		szName.Kill();
	}
	aszParameters.Kill();

	//Where it can be seen: the file; a class template's body; a function's body.
	pcScope = NULL;
	pcFunction = (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcNode, CPPS_FunctionDefinition);
	psDest->iPlace = IDE_TYPEDEF_FILE;
	iScopeFirst = 0;
	iScopeLast = (int32)cUnit.GetTokeniser()->NumTokens() - 1;
	if (bParameter)
	{
		pcScope = cUnit.FindAncestor(pcNode, CPPS_ClassSpecifier);
		if (pcScope && (!pcFunction || (pcFunction->miFirstToken < pcScope->miFirstToken) || !pcFunction->mpcBody || (iFirst < pcFunction->mpcBody->miFirstToken)))
		{
			psDest->iPlace = IDE_TYPEDEF_CLASS;
		}
		else if (pcFunction && pcFunction->mpcBody && (pcFunction->mpcBody->GetKind() == CPPS_CompoundStatement) && (iFirst >= pcFunction->mpcBody->miFirstToken))
		{
			pcScope = pcFunction->mpcBody;
			psDest->iPlace = IDE_TYPEDEF_FUNCTION;
		}
		else
		{
			cUnit.Kill();
			return Refuse(psDest, "it names a template's parameter where a typedef can't be written: not in the template's class, nor in a function's body");
		}
		iScopeFirst = pcScope->miFirstToken;
		iScopeLast = pcScope->miLastToken;
	}

	//Every place it is written there, in this file and not by a macro, and where the typedef would go for each.
	psDest->bPossible = true;
	psDest->bHeader = (psDest->iPlace == IDE_TYPEDEF_FILE) && IsIDEHeaderName(pTab->GetFileName());
	for (iToken = iScopeFirst; iToken + (iLast - iFirst) <= iScopeLast; iToken++)
	{
		if (!IsTypeAt(&cUnit, &psDest->aszTokens, iToken) || !cUnit.GetTokensRange(iToken, iToken + (iLast - iFirst), &sRange))
		{
			continue;
		}
		pcParent = cUnit.FindNodeOfTokens(iToken, iToken + (iLast - iFirst));
		if (!pcParent)
		{
			continue;
		}

		//The typedef's place for it: before the thing in the file it is in; the member of the class; the body's start.
		pcMember = pcParent;
		if (psDest->iPlace == IDE_TYPEDEF_FUNCTION)
		{
			pcMember = NULL;
			cUnit.GetTokensRange(pcScope->miFirstToken, pcScope->miFirstToken, &sRange);
			uiAt = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
			for (; (szText[uiAt] != '\0') && (szText[uiAt] != '\n'); uiAt++)
			{
			}
			uiAt = (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
		}
		else
		{
			while (pcMember->mpcParent && (pcMember->mpcParent != pcScope) && (pcMember->mpcParent->GetKind() != CPPS_TranslationUnit) &&
				   !((psDest->iPlace == IDE_TYPEDEF_FILE) && ((pcMember->mpcParent->GetKind() == CPPS_Namespace) || (pcMember->mpcParent->GetKind() == CPPS_LinkageSpecification))))
			{
				pcMember = pcMember->mpcParent;
			}
			if (!cUnit.GetRange(pcMember, &sRange))
			{
				continue;
			}
			uiAt = FindIDECommentStart(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
		}
		cUnit.GetTokensRange(iToken, iToken + (iLast - iFirst), &sRange);
		if (iToken == iFirst)
		{
			psDest->uiSelected = psDest->aiStarts.NumElements();
		}
		psDest->aiStarts.Add((int)FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
		psDest->aiEnds.Add((int)FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));
		psDest->aiTypedefAt.Add((int)uiAt);
		iToken += iLast - iFirst;
	}
	if (psDest->aiStarts.NumElements() == 0)
	{
		Refuse(psDest, "a macro makes the selection");
	}
	else if (psDest->iPlace != IDE_TYPEDEF_FILE)
	{
		//A member's, or a statement's, indentation: one tab in from the brace of what it is in.
		cUnit.GetTokensRange(pcScope->miLastToken, pcScope->miLastToken, &sRange);
		uiAt = FindIDETextOffset(szText, sRange.iLine, 0);
		for (i = uiAt; (szText[i] == ' ') || (szText[i] == '\t'); i++)
		{
		}
		psDest->szIndent.AppendSubString(szText, uiAt, i);
		psDest->szIndent.Append('\t');
	}
	if ((psDest->iPlace == IDE_TYPEDEF_CLASS) && (psDest->aiStarts.NumElements() > 0))
	{
		//In a class template, with its other typedefs at the top of the class (question 34): after its opening brace,
		//the access that starts it, and the typedefs there are.
		for (iToken = pcScope->miFirstToken; (iToken <= pcScope->miLastToken) && !cUnit.IsToken(iToken, "{"); iToken++)
		{
		}
		if ((iToken <= pcScope->miLastToken) && cUnit.GetTokensRange(iToken, iToken, &sRange))
		{
			uiAt = FindIDENextLine(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
			for (;;)
			{
				for (i = uiAt; (szText[i] == ' ') || (szText[i] == '\t'); i++)
				{
				}
				if ((strncmp(&szText[i], "public:", 7) == 0) || (strncmp(&szText[i], "protected:", 10) == 0) || (strncmp(&szText[i], "private:", 8) == 0) ||
					((strncmp(&szText[i], "typedef", 7) == 0) && !IsIDENameChar(szText[i + 7])))
				{
					uiAt = FindIDENextLine(szText, i);
				}
				else
				{
					break;
				}
			}
			for (i = 0; i < psDest->aiTypedefAt.NumElements(); i++)
			{
				psDest->aiTypedefAt.SetValue(i, (int)uiAt);
			}
		}
	}

	//Every name the file's tokens write.
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
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Why the typedef can't have a name, NULL if it can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEIntroduceTypedef::GetNameProblem(SIDETypedef* psTypedef, const char* szName)
{
	if (!IsIDEIdentifier(szName))
	{
		return "It isn't a name.";
	}
	if (psTypedef->aszUsed.Contains(szName))
	{
		return "The file uses that name already.";
	}
	if ((psTypedef->iPlace == IDE_TYPEDEF_FILE) && mpcContent->GetCodeModel()->HasSymbol(szName))
	{
		return "Something has that name already.";
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The places the type is written in the files that #include the header
// the typedef goes in: the solution's files that reach it, each parsed as
// it is.  Nothing if the typedef isn't a header's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::FindIncluders(SIDETypedef* psTypedef)
{
	CIDECodeModel*		pcModel;
	CIDEIncludeGraph	cGraph;
	CIDERefactorUnit	cUnit;
	CArrayChars			aszSeen;
	CTextFile			cFile;
	SCPPSyntaxRange		sRange;
	char*				szFileName;
	char*				szText;
	size				uiLength;
	size				i;
	int					iHeader;
	int32				iToken;
	int32				iSpan;

	psTypedef->aszOtherFiles.ReInit();
	psTypedef->aiOtherStarts.ReInit();
	psTypedef->aiOtherEnds.ReInit();
	if (!psTypedef->bPossible || !psTypedef->bHeader || (psTypedef->aszTokens.NumElements() == 0))
	{
		return;
	}
	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cGraph.Init(pcModel, &mpcContent->GetFindInFiles());
	iHeader = cGraph.Get(psTypedef->szFileName.Text());
	iSpan = (int32)psTypedef->aszTokens.NumElements() - 1;
	aszSeen.Init();
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFileName = pcModel->GetProjectFile(i);
		if ((StringInsensitiveCompare(szFileName, psTypedef->szFileName.Text()) == 0) || aszSeen.Contains(szFileName) || !cGraph.Reaches(cGraph.Get(szFileName), iHeader, -1, NULL))
		{
			continue;
		}
		aszSeen.Add(szFileName);

		//Only a file whose text has the type's first word is worth parsing.
		cFile.Init();
		szText = NULL;
		if (mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength) && szText && strstr(szText, psTypedef->aszTokens.GetText(0)))
		{
			cUnit.Init();
			if (cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), szFileName, false))
			{
				for (iToken = 0; (size)(iToken + iSpan) < cUnit.GetTokeniser()->NumTokens(); iToken++)
				{
					if (IsTypeAt(&cUnit, &psTypedef->aszTokens, iToken) && cUnit.GetTokensRange(iToken, iToken + iSpan, &sRange) && cUnit.FindNodeOfTokens(iToken, iToken + iSpan))
					{
						psTypedef->aszOtherFiles.Add(szFileName);
						psTypedef->aiOtherStarts.Add((int)FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
						psTypedef->aiOtherEnds.Add((int)FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));
						iToken += iSpan;
					}
				}
			}
			cUnit.Kill();
		}
		cFile.Kill();
	}
	aszSeen.Kill();
	cGraph.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A place the type is written given the typedef's name.  A name can't
// run into the name after it ("char*sz"); and what tabs lined up after
// the type stays in its column.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::AddReplacement(CIDETextEdits* pcEdits, char* szText, size uiStart, size uiEnd, const char* szName)
{
	CChars	szLine;
	size	uiRunEnd;
	size	uiStars;
	size	uiLineStart;
	size	uiColumn;
	size	uiNextColumn;

	szLine.Init(szName);
	for (uiStars = uiEnd; (szText[uiStars] == '*') || (szText[uiStars] == '&'); uiStars++)
	{
	}
	for (uiRunEnd = uiStars; szText[uiRunEnd] == '\t'; uiRunEnd++)
	{
	}
	if (IsIDENameChar(szText[uiEnd]))
	{
		szLine.Append(' ');
	}
	else if ((uiRunEnd > uiStars) && (szText[uiRunEnd] != ' ') && (szText[uiRunEnd] != '\r') && (szText[uiRunEnd] != '\n') && (szText[uiRunEnd] != '\0'))
	{
		//With the * or & after it, which the tabs follow.
		szLine.AppendSubString(szText, uiEnd, uiStars);
		for (uiLineStart = uiStart; (uiLineStart > 0) && (szText[uiLineStart - 1] != '\n'); uiLineStart--)
		{
		}
		uiColumn = FindIDEVisualColumn(szText, uiLineStart, uiStart, 4) + szLine.Length();
		uiNextColumn = FindIDEVisualColumn(szText, uiLineStart, uiRunEnd, 4);
		if (uiNextColumn > uiColumn)
		{
			AppendIDETabsTo(&szLine, uiColumn, uiNextColumn, 4);
		}
		else
		{
			szLine.Append('\t');
		}
		uiEnd = uiRunEnd;
	}
	pcEdits->Replace(uiStart, uiEnd, szLine.Text());
	szLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The typedef written, and the places given its name (all of them, or
// the selected one), as one edit.  With bIncluders the places found in
// the files that #include the header are given it too (FindIncluders
// first), if those files compile with it: one undo across them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::Introduce(SIDETypedef* psTypedef, const char* szName, bool bAll, bool bIncluders, CChars* pszStatus)
{
	CIDEFileEdits	cFiles;
	CIDETextEdits*	pcEdits;
	CChars			szLine;
	CChars			szMessage;
	const char*		szProblem;
	char*			szText;
	size			uiFirst;
	size			uiLast;
	size			uiPlaces;
	size			uiFiles;
	size			i;
	bool			bResult;

	szProblem = psTypedef->bPossible ? GetNameProblem(psTypedef, szName) : "It can't be introduced.";
	cFiles.Init(mpcContent);
	szText = szProblem ? NULL : cFiles.GetText(psTypedef->szFileName.Text());
	bResult = (szText != NULL);
	szMessage.Init();
	uiPlaces = 0;
	uiFiles = 0;
	if (!bResult)
	{
		szMessage.Append("Introduce Typedef: ")->Append(szProblem ? szProblem : "The file isn't open.");
	}
	else
	{
		uiFirst = bAll ? 0 : psTypedef->uiSelected;
		uiLast = bAll ? psTypedef->aiStarts.NumElements() - 1 : psTypedef->uiSelected;

		//The typedef, before the first place replaced: two blank lines from what follows it in the file, a line of
		//its own in a class or a function.
		szLine.Init(psTypedef->szIndent);
		szLine.Append("typedef ");
		szLine.Append(psTypedef->szType);
		szLine.Append('\t');
		szLine.Append(szName);
		szLine.Append((psTypedef->iPlace == IDE_TYPEDEF_FILE) ? ";\n\n\n" : ";\n");
		pcEdits = cFiles.GetEdits(psTypedef->szFileName.Text());
		pcEdits->Insert((size)psTypedef->aiTypedefAt.GetValue(uiFirst), szLine.Text());
		szLine.Kill();
		for (i = uiFirst; i <= uiLast; i++)
		{
			AddReplacement(pcEdits, szText, (size)psTypedef->aiStarts.GetValue(i), (size)psTypedef->aiEnds.GetValue(i), szName);
			uiPlaces++;
		}

		//The files that #include it: a file's places are together.
		for (i = 0; bIncluders && bResult && (i < psTypedef->aszOtherFiles.NumElements()); i++)
		{
			szText = cFiles.GetText(psTypedef->aszOtherFiles.GetText(i));
			bResult = (szText != NULL);
			if (bResult)
			{
				AddReplacement(cFiles.GetEdits(psTypedef->aszOtherFiles.GetText(i)), szText, (size)psTypedef->aiOtherStarts.GetValue(i), (size)psTypedef->aiOtherEnds.GetValue(i), szName);
				uiPlaces++;
				if ((i == 0) || (StringInsensitiveCompare(psTypedef->aszOtherFiles.GetText(i), psTypedef->aszOtherFiles.GetText(i - 1)) != 0))
				{
					uiFiles++;
				}
			}
		}
		if (!bResult)
		{
			szMessage.Append("Can't introduce typedef ")->Append(szName)->Append(": a file that #includes ")->Append(GetIDEFileNamePart(psTypedef->szFileName.Text()))->Append(" couldn't be opened.");
		}
		else if ((uiFiles > 0) && (cFiles.CountErrors(true) > cFiles.CountErrors(false)))
		{
			bResult = false;
			szMessage.Append("Can't introduce typedef ")->Append(szName)->Append(" in the files that #include ")->Append(GetIDEFileNamePart(psTypedef->szFileName.Text()));
			szMessage.Append(": they don't compile with it (a place is before the #include, or the words are another type there).");
		}
	}
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Introduced typedef ")->Append(psTypedef->szType)->Append(' ')->Append(szName)->Append(": ")->Append((uint32)uiPlaces);
		szMessage.Append((uiPlaces == 1) ? " place replaced" : " places replaced");
		if (uiFiles > 0)
		{
			szMessage.Append(", in ")->Append((uint32)(uiFiles + 1))->Append(" files");
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
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEIntroduceTypedef::PrintTypedef(SIDETypedef* psTypedef, CChars* pszDest)
{
	pszDest->Append("Introduce Typedef: ");
	if (!psTypedef->bPossible)
	{
		pszDest->Append("can't be: ")->Append(psTypedef->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	pszDest->Append('[')->Append(psTypedef->szType)->Append("], ");
	pszDest->Append((uint32)psTypedef->aiStarts.NumElements());
	pszDest->Append((psTypedef->aiStarts.NumElements() == 1) ? " place, " : " places, ");
	pszDest->Append((psTypedef->iPlace == IDE_TYPEDEF_FILE) ? "in the file\n" : ((psTypedef->iPlace == IDE_TYPEDEF_CLASS) ? "in its class\n" : "in its function\n"));
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The files that #include the header and how many places each has, a
// line each.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIntroduceTypedef::ListIncluders(SIDETypedef* psTypedef, CArrayChars* paszDest)
{
	CChars	szLine;
	size	uiPlaces;
	size	i;

	uiPlaces = 0;
	for (i = 0; i < psTypedef->aszOtherFiles.NumElements(); i++)
	{
		uiPlaces++;
		if ((i + 1 == psTypedef->aszOtherFiles.NumElements()) || (StringInsensitiveCompare(psTypedef->aszOtherFiles.GetText(i), psTypedef->aszOtherFiles.GetText(i + 1)) != 0))
		{
			szLine.Init(GetIDEFileNamePart(psTypedef->aszOtherFiles.GetText(i)));
			szLine.Append(": ")->Append((uint32)uiPlaces)->Append((uiPlaces == 1) ? " place" : " places");
			paszDest->Add(szLine.Text());
			szLine.Kill();
			uiPlaces = 0;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What FindIncluders found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEIntroduceTypedef::PrintIncluders(SIDETypedef* psTypedef, CChars* pszDest)
{
	CArrayChars	aszLines;
	size		i;

	aszLines.Init();
	ListIncluders(psTypedef, &aszLines);
	pszDest->Append("Introduce Typedef: ")->Append((uint32)psTypedef->aiOtherStarts.NumElements())->Append((psTypedef->aiOtherStarts.NumElements() == 1) ? " place in " : " places in ");
	pszDest->Append((uint32)aszLines.NumElements())->Append((aszLines.NumElements() == 1) ? " file that #includes it\n" : " files that #include it\n");
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		pszDest->Append("  ")->Append(aszLines.GetText(i))->Append('\n');
	}
	aszLines.Kill();
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::ApproveName(void)
{
	const char*	szProblem;

	szProblem = GetNameProblem(mpsShown, mpNameField->GetText());
	mpError->SetText(szProblem ? szProblem : "");
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Introduce Typedef for the selection: its dialog (the name, and whether
// every place is replaced), then the typedef.  False if nothing changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIntroduceTypedef::ShowIntroduce(void)
{
	SIDETypedef	sTypedef;
	Ptr<CLabel>	pLabel;
	CArrayChars	aszAsked;
	CChars		szText;
	bool		bResult;
	bool		bAll;
	bool		bIncluders;

	sTypedef.Init();
	if (!FindTypedef(&sTypedef))
	{
		mpcContent->ShowStatus("Select a type to introduce a typedef for.");
		sTypedef.Kill();
		return false;
	}
	if (!sTypedef.bPossible)
	{
		szText.Init("Can't introduce a typedef: ");
		szText.Append(sTypedef.szWhyNot);
		szText.Append(".");
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
		sTypedef.Kill();
		return false;
	}

	mpsShown = &sTypedef;
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Introduce Typedef");
	mpNameField = mpDialog->AddTextField("Name:", "");
	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), sTypedef.szType.Text());
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	mpDialog->AddEditor("Type:", pLabel);
	pLabel = NULL;
	mpAllBox = NULL;
	if (sTypedef.aiStarts.NumElements() > 1)
	{
		szText.Init("Replace all ");
		szText.Append((uint32)sTypedef.aiStarts.NumElements());
		szText.Append(" places it is written");
		mpAllBox = mpDialog->AddCheckBox(szText.Text(), mbAll);
		szText.Kill();
	}
	mpIncludersBox = NULL;
	if (sTypedef.bHeader)
	{
		//Never ticked until it is asked for: it changes files that aren't in the editor.
		mpIncludersBox = mpDialog->AddCheckBox("And in the files that #include this one (they are listed first)", false);
	}
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEIntroduceTypedef::ApproveName);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(480);
	if (bResult)
	{
		bAll = false;
		if (mpAllBox.IsNotNull())
		{
			mbAll = mpAllBox->IsTicked();
			bAll = mbAll;
		}
		bIncluders = mpIncludersBox.IsNotNull() && mpIncludersBox->IsTicked();
		szText.Init(mpNameField->GetText());
		if (bIncluders)
		{
			//The other files and what each has, first.
			FindIncluders(&sTypedef);
			aszAsked.Init();
			ListIncluders(&sTypedef, &aszAsked);
			if ((aszAsked.NumElements() > 0) && !mpcContent->GetChangeSignature()->ShowConflicts(&aszAsked))
			{
				bResult = false;
				mpcContent->ShowStatus("Introduce Typedef: nothing was changed.");
			}
			aszAsked.Kill();
		}
		if (bResult)
		{
			bResult = Introduce(&sTypedef, szText.Text(), bAll, bIncluders);
		}
		szText.Kill();
	}
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpNameField = NULL;
	mpError = NULL;
	mpAllBox = NULL;
	mpIncludersBox = NULL;
	mpDialog = NULL;
	mpsShown = NULL;
	sTypedef.Kill();
	return bResult;
}
