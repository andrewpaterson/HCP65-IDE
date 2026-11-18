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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/NameMatcher.h"
#include "CppCompilerLib/CPPIndex.h"
#include "CppCompilerLib/CPPSyntaxNamespace.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDEMoveMembers.h"
#include "IDEDefinitions.h"
#include "IDEChangeSignature.h"
#include "IDEContextActions.h"
#include "WindowLib/Table.h"
#include "IDEMoveNamespace.h"


#define IDE_NAMESPACE_COMPLETIONS		12
#define IDE_NAMESPACE_COLUMN_MOVE		0
#define IDE_NAMESPACE_COLUMN_NAME		1
#define IDE_NAMESPACE_VISIBLE_ROWS		6
#define IDE_NAMESPACE_COMPLETION_TAKEN	(-2147483647 - 1)


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENamespaceBlock::Init(void)
{
	szName.Init();
	uiStart = 0;
	uiOpenEnd = 0;
	uiCloseStart = 0;
	uiEnd = 0;
}


void SIDENamespaceBlock::Kill(void)
{
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENamespaceSite::Init(void)
{
	szFileName.Init();
	uiStart = 0;
	uiEnd = 0;
	asBlocks.Init();
	bQualified = false;
	uiQualifierStart = 0;
	uiQualifierEnd = 0;
}


void SIDENamespaceSite::Kill(void)
{
	size	i;

	for (i = 0; i < asBlocks.NumElements(); i++)
	{
		asBlocks.Get(i)->Kill();
	}
	asBlocks.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENamespaceEdit::Init(const char* szFileName, size uiStart, size uiEnd, const char* szText)
{
	this->szFileName.Init(szFileName);
	this->uiStart = uiStart;
	this->uiEnd = uiEnd;
	this->szText.Init(szText);
}


void SIDENamespaceEdit::Kill(void)
{
	szText.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENamespaceMove::Init(void)
{
	szKey.Init();
	szName.Init();
	szNamespace.Init();
	uiKind = 0;
	bMovable = false;
	szWhyNot.Init();
	asSites.Init();
	aszMembers.Init();
	aszTakers.Init();
	aszWith.Init();
}


void SIDENamespaceMove::Kill(void)
{
	size	i;

	aszWith.Kill();
	aszTakers.Kill();
	aszMembers.Kill();
	for (i = 0; i < asSites.NumElements(); i++)
	{
		asSites.Get(i)->Kill();
	}
	asSites.Kill();
	szWhyNot.Kill();
	szNamespace.Kill();
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mbUsings = false;
	mapcUnits.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpNamespaceField);
	M_Pointer(mpUsingBox);
	U_Bool(mbUsings);
	U_Unknown(CArrayTemplatePtr<CIDERefactorUnit>, mapcUnits);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::Free(void)
{
	if (mpcContent && mpcContent->GetWindow().IsNotNull())
	{
		mpcContent->GetWindow()->CancelPosted(this);
	}
	KillUnits();
	mapcUnits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::Save(CObjectWriter* pcFile) { return false; }
bool CIDEMoveNamespace::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Move to Namespace has no key, as IntelliJ's moves have none for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.MoveToNamespace", "Move to Namespace...", "", Ptr<CImageCel>(), this, &CIDEMoveNamespace::QueueMove)->SetEnabledCheck(this, &CIDEMoveNamespace::CanMove);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MoveToNamespace"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: what is at namespace scope (a class, a function
// that is no class's) can be moved to another namespace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bClass || (psContext->bFunction && !psContext->bMember))
	{
		psContext->Offer("Refactor.MoveToNamespace");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::CanMove(void)
{
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull();
}


void CIDEMoveNamespace::QueueMove(void)
{
	mpcContent->GetWindow()->Post(this, &CIDEMoveNamespace::ShowMove);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDEMoveNamespace::GetUnit(const char* szFileName)
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
void CIDEMoveNamespace::KillUnits(void)
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
// Whether a key is a namespace's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::IsNamespace(const char* szKey)
{
	CArrayIDELocations	asLocations;
	uint8				uiKind;
	bool				bNamespace;

	asLocations.Init();
	bNamespace = mpcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind) && (uiKind == CPPSK_Namespace);
	KillIDELocations(&asLocations);
	return bNamespace;
}


//////////////////////////////////////////////////////////////////////////
//
// The namespace a key's scope is in: its scope, or the namespace a class
// of it is in ("Geometry" of "Geometry::CPoint"); "" for the global one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::NamespaceOf(const char* szKey, CChars* pszDest)
{
	CChars	szScope;
	CChars	szOuter;

	szScope.Init();
	GetIDESymbolScope(szKey, &szScope);
	while (!szScope.Empty() && !IsNamespace(szScope.Text()))
	{
		szOuter.Init();
		GetIDESymbolScope(szScope.Text(), &szOuter);
		szScope.Set(szOuter);
		szOuter.Kill();
	}
	pszDest->Set(szScope);
	szScope.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szText from uiStart to uiEnd is only white space.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEBlank(const char* szText, size uiStart, size uiEnd)
{
	size	i;

	for (i = uiStart; i < uiEnd; i++)
	{
		if ((szText[i] != ' ') && (szText[i] != '\t') && (szText[i] != '\r') && (szText[i] != '\n'))
		{
			return false;
		}
	}
	return true;
}


static void FindIDELineColumn(const char* szText, size uiAt, int32* piLine, int32* piColumn)
{
	size	i;

	*piLine = 0;
	*piColumn = 0;
	for (i = 0; (i < uiAt) && (szText[i] != '\0'); i++)
	{
		if (szText[i] == '\n')
		{
			(*piLine)++;
			*piColumn = 0;
		}
		else
		{
			(*piColumn)++;
		}
	}
}


size CIDEMoveNamespace::FindIndent(const char* szText, size uiLine)
{
	size	i;

	for (i = uiLine; (szText[i] == ' ') || (szText[i] == '\t'); i++)
	{
	}
	return i - uiLine;
}


//////////////////////////////////////////////////////////////////////////
//
// The namespace blocks pcNode is in, outermost first; *pbUnnamed if one
// is unnamed or inline (they aren't moved out of).
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::FindBlocks(CIDERefactorUnit* pcUnit, const char* szText, CCPPSyntax* pcNode, CArrayIDENamespaceBlocks* pasDest, bool* pbUnnamed)
{
	CCPPSyntaxNamespace*	pcNamespace;
	SIDENamespaceBlock*		psBlock;
	SCPPSyntaxRange			sRange;
	SCPPSyntaxRange			sOpen;
	int32					iOpen;
	size					uiAt;

	for (; pcNode; pcNode = pcNode->mpcParent)
	{
		if (pcNode->GetKind() != CPPS_Namespace)
		{
			continue;
		}
		pcNamespace = (CCPPSyntaxNamespace*)pcNode;
		if ((pcNamespace->miName < 0) || pcNamespace->mbInline || !pcUnit->GetRange(pcNode, &sRange))
		{
			*pbUnnamed = true;
			continue;
		}
		for (iOpen = pcNamespace->miName; (iOpen <= pcNode->miLastToken) && !pcUnit->IsToken(iOpen, "{"); iOpen++)
		{
		}
		if (!pcUnit->GetTokensRange(iOpen, iOpen, &sOpen))
		{
			*pbUnnamed = true;
			continue;
		}
		psBlock = pasDest->InsertAt(0);
		psBlock->Init();
		pcUnit->AppendTokens(pcNamespace->miName, pcNamespace->miName, &psBlock->szName);
		psBlock->uiStart = FindIDELineStart(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
		psBlock->uiOpenEnd = FindIDENextLine(szText, FindIDETextOffset(szText, sOpen.iLine, sOpen.iColumn));
		uiAt = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		psBlock->uiCloseStart = FindIDELineStart(szText, (uiAt > 0) ? uiAt - 1 : 0);
		psBlock->uiEnd = FindIDENextLine(szText, (uiAt > 0) ? uiAt - 1 : 0);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The namespace a token is looked up from: its blocks', or, in a
// function written outside its namespace (int Old::F(void) { ... }), the
// namespace of what it defines.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::FindScopeAt(CIDERefactorUnit* pcUnit, const char* szText, int32 iToken, CChars* pszDest)
{
	CArrayIDENamespaceBlocks	asBlocks;
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcFunction;
	CCPPSyntaxFunctionDefinition*	pcDefinition;
	SCPPSyntaxRange				sRange;
	CChars						szKey;
	size						i;
	int32						iParenthesis;
	uint8						uiKind;
	bool						bUnnamed;

	pszDest->Clear();
	pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
	pcFunction = pcNode ? pcUnit->FindAncestor(pcNode, CPPS_FunctionDefinition) : NULL;
	if (pcFunction)
	{
		pcDefinition = (CCPPSyntaxFunctionDefinition*)pcFunction;
		szKey.Init();
		for (iParenthesis = pcDefinition->mpcDeclarator ? pcDefinition->mpcDeclarator->miFirstToken : -1; pcDefinition->mpcDeclarator && (iParenthesis <= pcDefinition->mpcDeclarator->miLastToken) && !pcUnit->IsToken(iParenthesis, "("); iParenthesis++)
		{
		}
		if (pcDefinition->mpcDeclarator && (iParenthesis > pcDefinition->mpcDeclarator->miFirstToken) && pcUnit->GetTokensRange(iParenthesis - 1, iParenthesis - 1, &sRange) &&
			mpcContent->GetCodeModel()->GetSymbolAt(pcUnit->GetFileName(), sRange.iLine, sRange.iColumn, &szKey, &uiKind))
		{
			NamespaceOf(szKey.Text(), pszDest);
			szKey.Kill();
			return;
		}
		szKey.Kill();
	}
	asBlocks.Init();
	bUnnamed = false;
	FindBlocks(pcUnit, szText, pcNode, &asBlocks, &bUnnamed);
	for (i = 0; i < asBlocks.NumElements(); i++)
	{
		pszDest->Append((i == 0) ? "" : "::")->Append(asBlocks.Get(i)->szName);
		asBlocks.Get(i)->Kill();
	}
	asBlocks.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The first token of the qualifier written before the name at iName
// (A::B::Name, ::Name), or iName if it has none.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEMoveNamespace::FindQualifier(CIDERefactorUnit* pcUnit, int32 iName)
{
	int32	iFirst;
	CChars	szToken;
	bool	bName;

	iFirst = iName;
	for (;;)
	{
		if ((iFirst < 1) || !pcUnit->IsToken(iFirst - 1, "::"))
		{
			return iFirst;
		}
		if (iFirst < 2)
		{
			return iFirst - 1;
		}
		szToken.Init();
		pcUnit->AppendTokens(iFirst - 2, iFirst - 2, &szToken);
		bName = IsIDEIdentifier(szToken.Text());
		szToken.Kill();
		if (!bName)
		{
			return iFirst - 1;
		}
		iFirst -= 2;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the file has using namespace szNamespace; at its top level
// before the token iBefore.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::HasUsingDirective(CIDERefactorUnit* pcUnit, int32 iBefore, const char* szNamespace)
{
	CArrayChars	aszOpened;
	bool		bFound;

	aszOpened.Init();
	FindUsingDirectives(pcUnit, iBefore, &aszOpened);
	bFound = aszOpened.Contains(szNamespace);
	aszOpened.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The namespaces the file's top level using-directives before the token
// iBefore open.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::FindUsingDirectives(CIDERefactorUnit* pcUnit, int32 iBefore, CArrayChars* paszDest)
{
	CChars	szWritten;
	int32	i;
	int32	j;
	int32	iDepth;

	iDepth = 0;
	for (i = 0; i < iBefore; i++)
	{
		if (pcUnit->IsToken(i, "{"))
		{
			iDepth++;
		}
		else if (pcUnit->IsToken(i, "}"))
		{
			iDepth--;
		}
		else if ((iDepth == 0) && pcUnit->IsToken(i, "using") && pcUnit->IsToken(i + 1, "namespace"))
		{
			szWritten.Init();
			for (j = i + 2; (j < iBefore) && !pcUnit->IsToken(j, ";"); j++)
			{
				pcUnit->AppendTokens(j, j, &szWritten);
			}
			if (szWritten.StartsWith("::"))
			{
				szWritten.RemoveFromStart(2);
			}
			paszDest->AddIfUnique(szWritten);
			szWritten.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What a name in szTo is written with at a use: the shortest of what the
// use's namespace (szFrom) and the namespaces its using-directives open
// need.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::AppendShortestQualifier(CIDERefactorUnit* pcUnit, int32 iName, const char* szFrom, const char* szTo, CChars* pszDest)
{
	CArrayChars	aszOpened;
	CChars		szBest;
	CChars		szTry;
	size		i;

	aszOpened.Init();
	FindUsingDirectives(pcUnit, iName, &aszOpened);
	szBest.Init();
	AppendQualifier(szFrom, szTo, &szBest);
	for (i = 0; !szBest.Empty() && (i < aszOpened.NumElements()); i++)
	{
		szTry.Init();
		if ((strncmp(szTo, aszOpened.GetText(i), aszOpened.Get(i)->Length()) == 0) && ((szTo[aszOpened.Get(i)->Length()] == ':') || (szTo[aszOpened.Get(i)->Length()] == '\0')))
		{
			AppendQualifier(aszOpened.GetText(i), szTo, &szTry);
			if (szTry.Length() < szBest.Length())
			{
				szBest.Set(szTry);
			}
		}
		szTry.Kill();
	}
	pszDest->Append(szBest);
	szBest.Kill();
	aszOpened.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What a name in szTo is written with from szFrom: nothing where szFrom
// is szTo or inside it; otherwise szTo after the namespaces they share,
// and "::".
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::AppendQualifier(const char* szFrom, const char* szTo, CChars* pszDest)
{
	CArrayChars	aszFrom;
	CArrayChars	aszTo;
	size		uiShared;
	size		i;

	aszFrom.Init();
	aszTo.Init();
	if (szFrom[0] != '\0')
	{
		aszFrom.Split(szFrom, ':');
	}
	if (szTo[0] != '\0')
	{
		aszTo.Split(szTo, ':');
	}
	for (i = aszFrom.NumElements(); i > 0; i--)
	{
		if (aszFrom.Get(i - 1)->Empty())
		{
			aszFrom.Remove(i - 1);
		}
	}
	for (i = aszTo.NumElements(); i > 0; i--)
	{
		if (aszTo.Get(i - 1)->Empty())
		{
			aszTo.Remove(i - 1);
		}
	}
	for (uiShared = 0; (uiShared < aszFrom.NumElements()) && (uiShared < aszTo.NumElements()) && aszFrom.Get(uiShared)->Equals(aszTo.Get(uiShared)); uiShared++)
	{
	}
	if (uiShared < aszTo.NumElements())
	{
		for (i = uiShared; i < aszTo.NumElements(); i++)
		{
			pszDest->Append(aszTo.GetText(i))->Append("::");
		}
	}
	aszTo.Kill();
	aszFrom.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Namespace lines of szNamespace's parts from uiFrom on, each a block of
// its own a level further in: opened (namespace A, {) or closed (}, the
// innermost first).
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::AppendBlockLines(const char* szNamespace, size uiFrom, const char* szIndent, size uiDepth, bool bOpen, CChars* pszDest)
{
	CArrayChars	aszParts;
	size		i;
	size		j;

	aszParts.Init();
	if (szNamespace[0] != '\0')
	{
		aszParts.Split(szNamespace, ':');
	}
	for (i = aszParts.NumElements(); i > 0; i--)
	{
		if (aszParts.Get(i - 1)->Empty())
		{
			aszParts.Remove(i - 1);
		}
	}
	if (bOpen)
	{
		for (i = uiFrom; i < aszParts.NumElements(); i++)
		{
			for (j = 0; j < uiDepth + (i - uiFrom); j++)
			{
				pszDest->Append(szIndent);
			}
			pszDest->Append("namespace ")->Append(aszParts.GetText(i))->Append('\n');
			for (j = 0; j < uiDepth + (i - uiFrom); j++)
			{
				pszDest->Append(szIndent);
			}
			pszDest->Append("{\n");
		}
	}
	else
	{
		for (i = aszParts.NumElements(); i > uiFrom; i--)
		{
			for (j = 0; j < uiDepth + (i - 1 - uiFrom); j++)
			{
				pszDest->Append(szIndent);
			}
			pszDest->Append("}\n");
		}
	}
	aszParts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A declaration of what moves, at a place the index has for it: its
// text's range (with the comment over it) and the namespace blocks it is
// in, or, written outside them, its qualifier.  False, with why, if it
// can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::FindSite(const char* szFileName, int32 iLine, int32 iColumn, bool bClassMember, CIDEFileEdits* pcFiles, SIDENamespaceSite* psDest, CChars* pszWhyNot)
{
	CIDERefactorUnit*	pcUnit;
	CCPPSyntax*			pcNode;
	CCPPSyntax*			pcDeclaration;
	SCPPSyntaxRange		sRange;
	const char*			szText;
	int32				iName;
	int32				iQualifier;
	bool				bUnnamed;

	pcUnit = GetUnit(szFileName);
	szText = pcUnit ? pcFiles->GetText(szFileName) : NULL;
	iName = pcUnit ? pcUnit->FindTokenAt(iLine, iColumn) : -1;
	pcNode = (iName >= 0) ? pcUnit->FindNodeOfTokens(iName, iName) : NULL;
	if (!szText || !pcNode)
	{
		pszWhyNot->Set("a declaration of it can't be read (it isn't in the solution, or a macro makes it)");
		return false;
	}

	//The whole declaration: the outermost of it that is in a namespace (or the file).
	pcDeclaration = pcNode;
	while (pcDeclaration->mpcParent && (pcDeclaration->mpcParent->GetKind() != CPPS_Namespace) && (pcDeclaration->mpcParent->GetKind() != CPPS_TranslationUnit))
	{
		pcDeclaration = pcDeclaration->mpcParent;
	}
	if (!pcUnit->GetRange(pcDeclaration, &sRange))
	{
		pszWhyNot->Set("a declaration of it can't be read");
		return false;
	}
	psDest->szFileName.Set(szFileName);
	psDest->uiStart = FindIDECommentStart(szText, FindIDELineStart(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn)));
	psDest->uiEnd = FindIDENextLine(szText, FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn));
	bUnnamed = false;
	FindBlocks(pcUnit, szText, pcDeclaration->mpcParent, &psDest->asBlocks, &bUnnamed);
	if (bUnnamed)
	{
		pszWhyNot->Set("a declaration of it is in an unnamed or inline namespace");
		return false;
	}

	//Written with its namespace: A::F, or for a member A::C::F (the class's name is its own use).
	iQualifier = FindQualifier(pcUnit, iName);
	if (!bClassMember && (iQualifier < iName))
	{
		if (!pcUnit->GetTokensRange(iQualifier, iName - 1, &sRange))
		{
			pszWhyNot->Set("a declaration of it can't be read");
			return false;
		}
		psDest->bQualified = true;
		psDest->uiQualifierStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
		psDest->uiQualifierEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What Move to Namespace has to work with, for the declaration at the
// caret.  False if there is nothing there it takes; psDest->bMovable says
// whether it can be moved, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::FindMove(SIDENamespaceMove* psDest)
{
	CIDECodeModel*			pcModel;
	CIDEFileEdits			cFiles;
	CArrayIDELocations		asMembers;
	CArrayIDEFileRanges		asRanges;
	SIDELocation*			psLocation;
	SIDENamespaceSite*		psSite;
	SIDENamespaceSite		sSite;
	SIDENamespaceSite*		psClass;
	size					uiClass;
	CChars					szScope;
	CChars					szWhyNot;
	CChars					szMemberScope;
	const char*				szClassText;
	int						iLine;
	int						iColumn;
	int32					iStartLine;
	int32					iStartColumn;
	int32					iEndLine;
	int32					iEndColumn;
	size					i;
	size					j;
	uint8					uiRole;
	uint8					uiKind;
	bool					bMember;

	pcModel = mpcContent->GetCodeModel();
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) ||
		!pcModel->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &psDest->szKey, &psDest->uiKind))
	{
		return false;
	}
	uiKind = psDest->uiKind;
	if ((uiKind != CPPSK_Class) && (uiKind != CPPSK_Enum) && (uiKind != CPPSK_Typedef) && (uiKind != CPPSK_ClassTemplate) && (uiKind != CPPSK_AliasTemplate) &&
		(uiKind != CPPSK_Function) && (uiKind != CPPSK_FunctionTemplate) && (uiKind != CPPSK_Variable) && (uiKind != CPPSK_VariableTemplate) && (uiKind != CPPSK_Namespace))
	{
		return false;
	}
	GetIDESymbolName(psDest->szKey.Text(), &psDest->szName);
	szScope.Init();
	GetIDESymbolScope(psDest->szKey.Text(), &szScope);
	if (szScope.Contains("(") || (psDest->szKey.Text()[0] == '@') || (!szScope.Empty() && !IsNamespace(szScope.Text())))
	{
		//A function's own, or a class's member: not at namespace scope.
		szScope.Kill();
		return false;
	}
	psDest->szNamespace.Set(szScope);
	szScope.Kill();
	psDest->bMovable = true;
	cFiles.Init(mpcContent);

	//Every declaration of it (a namespace's every block), and for a class its members' definitions in its namespace's
	//blocks.
	szWhyNot.Init();
	AddSites(psDest, psDest->szKey.Text(), &cFiles);
	if (psDest->bMovable && (psDest->uiKind == CPPSK_Namespace) && IsInlineNamespace(psDest, &cFiles))
	{
		psDest->bMovable = false;
		psDest->szWhyNot.Set("it is an inline namespace");
	}

	if (psDest->bMovable && ((psDest->uiKind == CPPSK_Class) || (psDest->uiKind == CPPSK_ClassTemplate)))
	{
		//The class's own site is the biggest.  It's kept by index: the sites found below are added to asSites, which
		//moves it.
		psClass = NULL;
		uiClass = 0;
		for (i = 0; i < psDest->asSites.NumElements(); i++)
		{
			psSite = psDest->asSites.Get(i);
			if (!psClass || (psSite->uiEnd - psSite->uiStart > psClass->uiEnd - psClass->uiStart))
			{
				psClass = psSite;
				uiClass = i;
			}
		}
		asRanges.Init();
		szClassText = psClass ? cFiles.GetText(psClass->szFileName.Text()) : NULL;
		if (szClassText)
		{
			FindIDELineColumn(szClassText, psClass->uiStart, &iStartLine, &iStartColumn);
			FindIDELineColumn(szClassText, psClass->uiEnd, &iEndLine, &iEndColumn);
			asRanges.Add()->Init(psClass->szFileName.Text(), iStartLine, iStartColumn, iEndLine, iEndColumn);
		}
		pcModel->FindSymbolsDeclaredIn(&asRanges, &psDest->aszMembers);
		KillIDEFileRanges(&asRanges);
		for (i = 0; psDest->bMovable && (i < psDest->aszMembers.NumElements()); i++)
		{
			szMemberScope.Init();
			GetIDESymbolScope(psDest->aszMembers.GetText(i), &szMemberScope);
			bMember = szMemberScope.Equals(psDest->szKey);
			szMemberScope.Kill();
			asMembers.Init();
			if (bMember)
			{
				pcModel->GetSymbolLocations(psDest->aszMembers.GetText(i), &asMembers, &uiKind);
			}
			for (j = 0; psDest->bMovable && (j < asMembers.NumElements()); j++)
			{
				psLocation = asMembers.Get(j);
				if (psLocation->uiRole != CPPIR_Definition)
				{
					continue;
				}
				sSite.Init();
				psClass = (psDest->asSites.NumElements() > 0) ? psDest->asSites.Get(uiClass) : NULL;
				if (FindSite(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, true, &cFiles, &sSite, &szWhyNot) && (sSite.asBlocks.NumElements() > 0) &&
					!(psClass && (StringInsensitiveCompare(sSite.szFileName.Text(), psClass->szFileName.Text()) == 0) && (sSite.uiStart >= psClass->uiStart) && (sSite.uiEnd <= psClass->uiEnd)))
				{
					//Defined outside the class in a block of its namespace: it goes with it.
					psSite = psDest->asSites.Add();
					memcpy(psSite, &sSite, sizeof(SIDENamespaceSite));
				}
				else
				{
					sSite.Kill();
				}
			}
			KillIDELocations(&asMembers);
		}

		//The old namespace's functions that take it: offered to go with it (FindConflicts warns of those left).
		FindTakers(psDest, &cFiles, &psDest->aszTakers);
	}
	szWhyNot.Kill();
	cFiles.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The sites of every declaration of szKey added to the move's.  False,
// with why (bMovable and szWhyNot), if one can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::AddSites(SIDENamespaceMove* psMove, const char* szKey, CIDEFileEdits* pcFiles)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	SIDENamespaceSite*	psSite;
	SIDENamespaceSite	sSite;
	CChars				szWhyNot;
	size				i;
	uint8				uiKind;

	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind);
	szWhyNot.Init();
	for (i = 0; psMove->bMovable && (i < asLocations.NumElements()); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			continue;
		}
		sSite.Init();
		if (FindSite(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, false, pcFiles, &sSite, &szWhyNot))
		{
			psSite = psMove->asSites.Add();
			memcpy(psSite, &sSite, sizeof(SIDENamespaceSite));
		}
		else
		{
			psMove->bMovable = false;
			psMove->szWhyNot.Set(szWhyNot);
			sSite.Kill();
		}
	}
	KillIDELocations(&asLocations);
	szWhyNot.Kill();
	return psMove->bMovable;
}


//////////////////////////////////////////////////////////////////////////
//
// Every one of the functions that take the class going with it: what the
// dialog starts with, ticked.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::TakeAllWith(SIDENamespaceMove* psMove)
{
	size	i;

	psMove->aszWith.ReInit();
	for (i = 0; i < psMove->aszTakers.NumElements(); i++)
	{
		psMove->aszWith.Add(psMove->aszTakers.GetText(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a symbol (szKey, in the scope szScope) goes with the move: it,
// one of its members, or a function that goes with it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::IsMoved(SIDENamespaceMove* psMove, const char* szKey, const char* szScope)
{
	return (strcmp(szKey, psMove->szKey.Text()) == 0) || (strcmp(szScope, psMove->szKey.Text()) == 0) ||
		   ((strncmp(szScope, psMove->szKey.Text(), psMove->szKey.Length()) == 0) && (szScope[psMove->szKey.Length()] == ':')) || psMove->aszWith.Contains(szKey);
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEMoveNamespace::PrintMove(SIDENamespaceMove* psMove, CChars* pszDest)
{
	SIDENamespaceSite*	psSite;
	size				i;

	pszDest->Append("Move to Namespace: ")->Append(psMove->szKey)->Append(" from [")->Append(psMove->szNamespace)->Append("]");
	if (!psMove->bMovable)
	{
		return pszDest->Append(" can't be: ")->Append(psMove->szWhyNot)->Append('\n')->Text();
	}
	pszDest->Append(", ")->Append((uint32)psMove->asSites.NumElements())->Append(" declarations:");
	for (i = 0; i < psMove->asSites.NumElements(); i++)
	{
		psSite = psMove->asSites.Get(i);
		pszDest->Append(' ')->Append(GetIDEFileNamePart(psSite->szFileName.Text()))->Append(psSite->bQualified ? "(qualified)" : "")->Append('[')->Append((uint32)psSite->asBlocks.NumElements())->Append(']');
	}
	for (i = 0; i < psMove->aszTakers.NumElements(); i++)
	{
		pszDest->Append((i == 0) ? ", taken by: " : ", ")->Append(psMove->aszTakers.GetText(i));
	}
	return pszDest->Append('\n')->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The unqualified names in a site's text of things of other namespaces
// that szNew can't see (what moves found them by being in the old one):
// each qualified, as edits.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::QualifyInside(SIDENamespaceMove* psMove, SIDENamespaceSite* psSite, const char* szNew, CIDEFileEdits* pcFiles, CArrayIDENamespaceEdits* pasDest)
{
	CIDERefactorUnit*	pcUnit;
	SCPPSyntaxRange		sRange;
	CChars				szToken;
	CChars				szKey;
	CChars				szScope;
	CChars				szNamespace;
	CChars				szQualifier;
	const char*			szText;
	int32				iFirst;
	int32				iLast;
	int32				iStartLine;
	int32				iStartColumn;
	int32				iEndLine;
	int32				iEndColumn;
	int32				i;
	uint8				uiKind;
	bool				bVisible;
	bool				bMoved;

	pcUnit = GetUnit(psSite->szFileName.Text());
	szText = pcFiles->GetText(psSite->szFileName.Text());
	if (!pcUnit || !szText)
	{
		return;
	}
	FindIDELineColumn(szText, psSite->uiStart, &iStartLine, &iStartColumn);
	FindIDELineColumn(szText, psSite->uiEnd, &iEndLine, &iEndColumn);
	if (!pcUnit->FindTokensIn(iStartLine, iStartColumn, iEndLine, iEndColumn, &iFirst, &iLast))
	{
		return;
	}
	for (i = iFirst; i <= iLast; i++)
	{
		if (!pcUnit->GetTokensRange(i, i, &sRange))
		{
			continue;
		}
		szToken.Init();
		pcUnit->AppendTokens(i, i, &szToken);
		if (!IsIDEIdentifier(szToken.Text()) || ((i > 0) && (pcUnit->IsToken(i - 1, "::") || pcUnit->IsToken(i - 1, ".") || pcUnit->IsToken(i - 1, "->"))))
		{
			szToken.Kill();
			continue;
		}
		szToken.Kill();
		szKey.Init();
		if (mpcContent->GetCodeModel()->GetSymbolAt(psSite->szFileName.Text(), sRange.iLine, sRange.iColumn, &szKey, &uiKind) && (uiKind != CPPSK_Parameter) && (uiKind != CPPSK_Field) && (szKey.Text()[0] != '@'))
		{
			szScope.Init();
			GetIDESymbolScope(szKey.Text(), &szScope);
			bMoved = IsMoved(psMove, szKey.Text(), szScope.Text());
			if (!bMoved && !szScope.Contains("(") && (szScope.Empty() || IsNamespace(szScope.Text())))
			{
				//At namespace scope: seen from szNew if its namespace is szNew or one szNew is in.
				szNamespace.Init(szScope);
				bVisible = szNamespace.Empty() || szNamespace.Equals(szNew) || ((strncmp(szNew, szNamespace.Text(), szNamespace.Length()) == 0) && (szNew[szNamespace.Length()] == ':'));
				if (!bVisible)
				{
					szQualifier.Init();
					AppendQualifier(szNew, szNamespace.Text(), &szQualifier);
					pasDest->Add()->Init(psSite->szFileName.Text(), FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), szQualifier.Text());
					szQualifier.Kill();
				}
				szNamespace.Kill();
			}
			szScope.Kill();
		}
		szKey.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A site in old namespace blocks written into new ones, where it is: the
// old block closed before it and opened after it (a block it is alone in
// goes; one it starts or ends goes before or after the block), the new
// namespace's opened round it.  Its text is the file's with pasDest's
// edits inside it applied, which are taken out of pasDest.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::MoveSite(SIDENamespaceSite* psSite, const char* szOld, const char* szNew, CIDEFileEdits* pcFiles, CArrayIDENamespaceEdits* pasDest)
{
	SIDENamespaceEdit*	psEdit;
	SIDENamespaceBlock*	psBlock;
	CArrayChars			aszNew;
	CArrayChars			aszLines;
	CChars				szMoved;
	CChars				szText;
	CChars				szUnit;
	CChars				szOldIndent;
	CChars				szNewIndent;
	CChars				szLine;
	const char*			szFile;
	size				uiShared;
	size				uiLevels;
	size				uiStart;
	size				uiEnd;
	size				uiDeleteStart;
	size				uiDeleteEnd;
	size				uiAnchor;
	size				uiSplit;
	size				i;
	size				j;
	size				uiIndent;
	size				uiInto;
	bool				bAnchored;
	bool				bAfter;
	bool				bSplit;

	szFile = pcFiles->GetText(psSite->szFileName.Text());
	if (!szFile)
	{
		return;
	}

	//Its text, with the edits inside it.
	szMoved.Init();
	szMoved.AppendSubString(szFile, psSite->uiStart, psSite->uiEnd);
	for (i = pasDest->NumElements(); i > 0; i--)
	{
		psEdit = pasDest->Get(i - 1);
		if ((StringInsensitiveCompare(psEdit->szFileName.Text(), psSite->szFileName.Text()) == 0) && (psEdit->uiStart >= psSite->uiStart) && (psEdit->uiEnd <= psSite->uiEnd))
		{
			szText.Init();
			szText.AppendSubString(szMoved.Text(), 0, psEdit->uiStart - psSite->uiStart);
			szText.Append(psEdit->szText);
			szText.Append(&szMoved.Text()[psEdit->uiEnd - psSite->uiStart]);
			szMoved.Set(szText);
			szText.Kill();
			psEdit->Kill();
			pasDest->RemoveAt(i - 1);
		}
	}

	//The namespaces the old blocks and the new namespace share.
	aszNew.Init();
	if (szNew[0] != '\0')
	{
		aszNew.Split(szNew, ':');
	}
	for (i = aszNew.NumElements(); i > 0; i--)
	{
		if (aszNew.Get(i - 1)->Empty())
		{
			aszNew.Remove(i - 1);
		}
	}
	uiLevels = psSite->asBlocks.NumElements();
	for (uiShared = 0; (uiShared < uiLevels) && (uiShared < aszNew.NumElements()) && psSite->asBlocks.Get(uiShared)->szName.Equals(aszNew.Get(uiShared)); uiShared++)
	{
	}

	//The indent of a block's body: the file's, or a tab.
	szUnit.Init("\t");
	szOldIndent.Init();
	if (uiLevels > 0)
	{
		psBlock = psSite->asBlocks.Get(uiLevels - 1);
		for (i = psBlock->uiOpenEnd; (i < psBlock->uiCloseStart) && IsIDEBlank(szFile, i, FindIDENextLine(szFile, i)); i = FindIDENextLine(szFile, i))
		{
		}
		if (i < psBlock->uiCloseStart)
		{
			uiIndent = FindIndent(szFile, psBlock->uiStart);
			j = FindIndent(szFile, i);
			szOldIndent.AppendSubString(szFile, i, i + j);
			szUnit.Clear();
			if (j > uiIndent)
			{
				szUnit.AppendSubString(szFile, i + uiIndent, i + j);
			}
		}
	}
	szNewIndent.Init();
	for (i = 0; i < aszNew.NumElements(); i++)
	{
		szNewIndent.Append(szUnit);
	}

	//Where it goes: out of the levels it starts, ends or is alone in, then split.
	uiDeleteStart = psSite->uiStart;
	uiDeleteEnd = psSite->uiEnd;
	uiStart = psSite->uiStart;
	uiEnd = psSite->uiEnd;
	uiAnchor = psSite->uiStart;
	bAnchored = false;
	bAfter = false;
	bSplit = false;
	uiSplit = uiShared;
	for (i = uiLevels; i > uiShared; i--)
	{
		psBlock = psSite->asBlocks.Get(i - 1);
		if (IsIDEBlank(szFile, psBlock->uiOpenEnd, uiStart) && IsIDEBlank(szFile, uiEnd, psBlock->uiCloseStart))
		{
			uiDeleteStart = bAnchored ? uiDeleteStart : psBlock->uiStart;
			uiDeleteEnd = bAnchored ? uiDeleteEnd : psBlock->uiEnd;
			uiStart = psBlock->uiStart;
			uiEnd = psBlock->uiEnd;
			uiAnchor = uiStart;
		}
		else if (IsIDEBlank(szFile, psBlock->uiOpenEnd, uiStart))
		{
			uiAnchor = psBlock->uiStart;
			uiStart = uiAnchor;
			uiEnd = uiAnchor;
			bAnchored = true;
			bAfter = false;
		}
		else if (IsIDEBlank(szFile, uiEnd, psBlock->uiCloseStart))
		{
			uiAnchor = psBlock->uiEnd;
			uiStart = uiAnchor;
			uiEnd = uiAnchor;
			bAnchored = true;
			bAfter = true;
		}
		else
		{
			bSplit = true;
			uiSplit = i;
			break;
		}
	}

	//Its lines indented for the new blocks.
	aszLines.Init();
	szMoved.Split(&aszLines, '\n');
	szText.Init();
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		szLine.Init(aszLines.GetText(i));
		if (szLine.StartsWith(szOldIndent.Text()))
		{
			szLine.RemoveFromStart(szOldIndent.Length());
		}
		if ((i + 1 < aszLines.NumElements()) || !szLine.Empty())
		{
			szText.Append(szLine.Empty() ? "" : szNewIndent.Text())->Append(szLine);
			szText.Append((i + 1 < aszLines.NumElements()) ? "\n" : "");
		}
		szLine.Kill();
	}
	aszLines.Kill();

	//Wrapped: the old levels closed and opened again round it where it splits them, the new ones round it.
	szMoved.Clear();
	if (bSplit)
	{
		for (i = uiSplit; i > uiShared; i--)
		{
			for (j = 0; j + 1 < i; j++)
			{
				szMoved.Append(szUnit);
			}
			szMoved.Append("}\n");
		}
		szMoved.Append("\n\n");
	}
	else if (bAnchored && bAfter)
	{
		szMoved.Append("\n\n");
	}
	{
		CChars	szBlockIndent;

		szBlockIndent.Init();
		for (j = 0; j < uiShared; j++)
		{
			szBlockIndent.Append(szUnit);
		}
		for (i = uiShared; i < aszNew.NumElements(); i++)
		{
			szMoved.Append(szBlockIndent)->Append("namespace ")->Append(aszNew.GetText(i))->Append('\n')->Append(szBlockIndent)->Append("{\n");
			szBlockIndent.Append(szUnit);
		}
		szMoved.Append(szText);
		for (i = aszNew.NumElements(); i > uiShared; i--)
		{
			szBlockIndent.RemoveFromEnd(szUnit.Length());
			szMoved.Append(szBlockIndent)->Append("}\n");
		}
		szBlockIndent.Kill();
	}
	if (bSplit)
	{
		szMoved.Append("\n\n");
		for (i = uiShared; i < uiSplit; i++)
		{
			for (j = 0; j < i; j++)
			{
				szMoved.Append(szUnit);
			}
			szMoved.Append("namespace ")->Append(psSite->asBlocks.Get(i)->szName)->Append('\n');
			for (j = 0; j < i; j++)
			{
				szMoved.Append(szUnit);
			}
			szMoved.Append("{\n");
		}
	}
	else if (bAnchored && !bAfter)
	{
		szMoved.Append("\n\n");
	}

	if (bSplit)
	{
		while ((uiDeleteStart > 0) && IsIDEBlank(szFile, FindIDELineStart(szFile, uiDeleteStart - 1), uiDeleteStart))
		{
			uiDeleteStart = FindIDELineStart(szFile, uiDeleteStart - 1);
		}
		while ((szFile[uiDeleteEnd] != '\0') && IsIDEBlank(szFile, uiDeleteEnd, FindIDENextLine(szFile, uiDeleteEnd)))
		{
			uiDeleteEnd = FindIDENextLine(szFile, uiDeleteEnd);
		}
	}
	if (bAnchored && bAfter)
	{
		//The block's last: the blank lines before it go, so what stays ends at the block's closing brace.
		while ((uiDeleteStart > 0) && IsIDEBlank(szFile, FindIDELineStart(szFile, uiDeleteStart - 1), uiDeleteStart))
		{
			uiDeleteStart = FindIDELineStart(szFile, uiDeleteStart - 1);
		}
	}
	else if (bAnchored)
	{
		//The block's first: the blank lines after it go, so what stays starts after the block's opening brace.
		while ((szFile[uiDeleteEnd] != '\0') && IsIDEBlank(szFile, uiDeleteEnd, FindIDENextLine(szFile, uiDeleteEnd)))
		{
			uiDeleteEnd = FindIDENextLine(szFile, uiDeleteEnd);
		}
	}
	uiInto = bSplit ? MAX_SIZE : FindNeighbourBlock(szFile, bAnchored ? uiAnchor : uiDeleteStart, bAnchored ? uiAnchor : uiDeleteEnd, &aszNew, uiShared);
	if (uiInto != MAX_SIZE)
	{
		if (uiInto < uiDeleteStart)
		{
			//A block of the new namespace is before it: it goes in at that block's end, a blank line from what is
			//there, and the blank lines between them go.
			while ((uiDeleteStart > uiInto) && IsIDEBlank(szFile, FindIDELineStart(szFile, uiDeleteStart - 1), uiDeleteStart) && (FindIDELineStart(szFile, uiDeleteStart - 1) >= FindIDENextLine(szFile, uiInto)))
			{
				uiDeleteStart = FindIDELineStart(szFile, uiDeleteStart - 1);
			}
			szText.Insert(0, "\n");
		}
		else
		{
			//A block of the new namespace is after it: it goes in at that block's start.  The blank lines after it go
			//with it only where there are some before it too, so what was on either side stays apart.
			if ((uiDeleteStart > 0) && IsIDEBlank(szFile, FindIDELineStart(szFile, uiDeleteStart - 1), uiDeleteStart))
			{
				while ((szFile[uiDeleteEnd] != '\0') && IsIDEBlank(szFile, uiDeleteEnd, FindIDENextLine(szFile, uiDeleteEnd)) && (uiDeleteEnd < uiInto))
				{
					uiDeleteEnd = FindIDENextLine(szFile, uiDeleteEnd);
				}
			}
			szText.Append('\n');
		}
		pasDest->Add()->Init(psSite->szFileName.Text(), uiDeleteStart, uiDeleteEnd, "");
		pasDest->Add()->Init(psSite->szFileName.Text(), uiInto, uiInto, szText.Text());
	}
	else if (bAnchored)
	{
		pasDest->Add()->Init(psSite->szFileName.Text(), uiDeleteStart, uiDeleteEnd, "");
		pasDest->Add()->Init(psSite->szFileName.Text(), uiAnchor, uiAnchor, szMoved.Text());
	}
	else
	{
		pasDest->Add()->Init(psSite->szFileName.Text(), uiDeleteStart, uiDeleteEnd, szMoved.Text());
	}
	szMoved.Kill();
	szText.Kill();
	szNewIndent.Kill();
	szOldIndent.Kill();
	szUnit.Kill();
	aszNew.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Problems Detected: the new namespace has the name already; for a class,
// the old namespace's functions that take it, which calls found by
// argument dependent lookup won't find from it any more.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::FindConflicts(SIDENamespaceMove* psMove, const char* szNew, CIDEFileEdits* pcFiles, CArrayChars* paszDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	CChars				szNewKey;
	CChars				szLine;
	size				i;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	szNewKey.Init(szNew);
	szNewKey.Append(szNew[0] ? "::" : "")->Append(&psMove->szKey.Text()[psMove->szNamespace.Empty() ? 0 : psMove->szNamespace.Length() + 2]);
	asLocations.Init();
	if (!szNewKey.Equals(psMove->szKey) && pcModel->GetSymbolLocations(szNewKey.Text(), &asLocations, &uiKind) && (asLocations.NumElements() > 0))
	{
		szLine.Init(szNew[0] ? szNew : "The global namespace");
		if (psMove->uiKind == CPPSK_Namespace)
		{
			szLine.Append(" has a namespace ")->Append(psMove->szName)->Append(" already: the two will be one namespace, and what they declare mustn't clash.");
		}
		else
		{
			szLine.Append(" has a ")->Append(psMove->szName)->Append(" already.");
		}
		paszDest->Add(szLine.Text());
		szLine.Kill();
	}
	KillIDELocations(&asLocations);
	szNewKey.Kill();

	for (i = 0; i < psMove->aszTakers.NumElements(); i++)
	{
		if (!psMove->aszWith.Contains(psMove->aszTakers.GetText(i)))
		{
			szLine.Init();
			GetIDEScopedName(psMove->aszTakers.GetText(i), &szLine);
			szLine.Append(" takes ")->Append(psMove->szName)->Append(" and stays in ")->Append(psMove->szNamespace)->Append(": a call that found it through ")->Append(psMove->szName)->Append("'s namespace won't.");
			paszDest->AddIfUnique(szLine);
			szLine.Kill();
		}
	}
	for (i = 0; i < psMove->aszWith.NumElements(); i++)
	{
		szNewKey.Init(szNew);
		szNewKey.Append(szNew[0] ? "::" : "")->Append(&psMove->aszWith.GetText(i)[psMove->szNamespace.Empty() ? 0 : psMove->szNamespace.Length() + 2]);
		asLocations.Init();
		if (pcModel->GetSymbolLocations(szNewKey.Text(), &asLocations, &uiKind) && (asLocations.NumElements() > 0))
		{
			szLine.Init(szNew[0] ? szNew : "The global namespace");
			szLine.Append(" has a ");
			GetIDESymbolName(psMove->aszWith.GetText(i), &szLine);
			szLine.Append(" already.");
			paszDest->AddIfUnique(szLine);
			szLine.Kill();
		}
		KillIDELocations(&asLocations);
		szNewKey.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A class's old namespace's functions that take it (a parameter of its
// type): argument dependent lookup finds them from it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::FindTakers(SIDENamespaceMove* psMove, CIDEFileEdits* pcFiles, CArrayChars* paszDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	CArrayIDEFileRanges	asRanges;
	CArrayChars			aszKeys;
	SIDELocation*		psLocation;
	CIDERefactorUnit*	pcUnit;
	CCPPSyntax*			pcNode;
	SCPPSyntaxRange		sRange;
	CChars				szScope;
	const char*			szText;
	const char*			szParameters;
	size				i;
	uint8				uiKind;

	if (((psMove->uiKind != CPPSK_Class) && (psMove->uiKind != CPPSK_ClassTemplate)) || psMove->szNamespace.Empty())
	{
		return;
	}
	pcModel = mpcContent->GetCodeModel();
	asLocations.Init();
	asRanges.Init();	pcModel->GetSymbolLocations(psMove->szNamespace.Text(), &asLocations, &uiKind);
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		pcUnit = (psLocation->uiRole != CPPIR_Reference) ? GetUnit(psLocation->szFileName.Text()) : NULL;
		pcNode = pcUnit ? pcUnit->FindNodeAt(psLocation->iLine, psLocation->iColumn) : NULL;
		pcNode = pcNode ? pcUnit->FindAncestor(pcNode, CPPS_Namespace) : NULL;
		szText = pcUnit ? pcFiles->GetText(psLocation->szFileName.Text()) : NULL;
		if (pcNode && szText && pcUnit->GetRange(pcNode, &sRange))
		{
			asRanges.Add()->Init(psLocation->szFileName.Text(), sRange.iLine, sRange.iColumn, sRange.iEndLine, sRange.iEndColumn);
		}
	}
	KillIDELocations(&asLocations);
	aszKeys.Init();
	pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	KillIDEFileRanges(&asRanges);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		szParameters = strchr(aszKeys.GetText(i), '(');
		szScope.Init();
		GetIDESymbolScope(aszKeys.GetText(i), &szScope);
		if (szParameters && szScope.Equals(psMove->szNamespace) && strstr(szParameters, psMove->szKey.Text()))
		{
			paszDest->AddIfUnique(aszKeys.GetText(i));
		}
		szScope.Kill();
	}
	aszKeys.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The declaration moved to szNamespace ("" for the global namespace), its
// uses made to name it from where they are, with a using-declaration in
// a source file that would otherwise qualify them (bUsings).  False, with
// why in pszStatus, if it can't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::Move(SIDENamespaceMove* psMove, const char* szNamespace, bool bUsings, CChars* pszStatus)
{
	CIDECodeModel*			pcModel;
	CIDEFileEdits			cFiles;
	CArrayIDENamespaceEdits	asEdits;
	CArrayChars				aszConflicts;
	CArrayChars				aszUsingFiles;
	SIDENamespaceSite*		psSite;
	SIDENamespaceEdit*		psEdit;
	CChars					szNew;
	CChars					szScope;
	CChars					szQualifier;
	CChars					szLine;
	const char*				szText;
	size					uiUses;
	size					uiLabels;
	size					uiUnsure;
	size					uiMore;
	CChars					szUsingName;
	size					uiAt;
	size					i;
	size					j;
	bool					bAfterUsing;
	bool					bResult;

	pcModel = mpcContent->GetCodeModel();
	szNew.Init(szNamespace);
	while (szNew.StartsWith("::"))
	{
		szNew.RemoveFromStart(2);
	}
	szNew.StripWhitespace();
	if (szNew.Equals(psMove->szNamespace))
	{
		pszStatus->Set("Move to Namespace: it is in that namespace already.");
		szNew.Kill();
		return false;
	}
	if ((psMove->uiKind == CPPSK_Namespace) && (szNew.Equals(psMove->szKey) || (szNew.StartsWith(psMove->szKey.Text()) && (szNew.Text()[psMove->szKey.Length()] == ':'))))
	{
		pszStatus->Set("Move to Namespace: a namespace can't go into itself.");
		szNew.Kill();
		return false;
	}
	if (!szNew.Empty() && (!IsIDEIdentifier(szNew.Text()) && !szNew.Contains("::")))
	{
		pszStatus->Set("Move to Namespace: a namespace is names separated by ::.");
		szNew.Kill();
		return false;
	}

	cFiles.Init(mpcContent);
	for (i = 0; i < psMove->aszWith.NumElements(); i++)
	{
		//What goes with it: its declarations are sites of the move too.
		if (!AddSites(psMove, psMove->aszWith.GetText(i), &cFiles))
		{
			pszStatus->Set("Move to Namespace: ");
			GetIDEScopedName(psMove->aszWith.GetText(i), pszStatus);
			pszStatus->Append(" can't go with it: ")->Append(psMove->szWhyNot)->Append('.');
			cFiles.Kill();
			szNew.Kill();
			return false;
		}
	}
	aszConflicts.Init();
	FindConflicts(psMove, szNew.Text(), &cFiles, &aszConflicts);
	bResult = (aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&aszConflicts);
	aszConflicts.Kill();
	if (!bResult)
	{
		pszStatus->Set("Move to Namespace: nothing was moved.");
		cFiles.Kill();
		szNew.Kill();
		return false;
	}

	asEdits.Init();
	aszUsingFiles.Init();

	//The uses of it and of what goes with it, each named from where it is.
	uiUses = 0;
	MoveUses(psMove, psMove->szKey.Text(), szNew.Text(), bUsings, &cFiles, &asEdits, &aszUsingFiles, &uiUses);
	for (i = 0; i < psMove->aszWith.NumElements(); i++)
	{
		MoveUses(psMove, psMove->aszWith.GetText(i), szNew.Text(), bUsings, &cFiles, &asEdits, &aszUsingFiles, &uiUses);
	}

	//A file given a using-declaration (aszUsingFiles has "name|file"): after its #includes and using lines.
	for (i = 0; i < aszUsingFiles.NumElements(); i++)
	{
		szUsingName.Init(aszUsingFiles.GetText(i));
		szUsingName.SetLength(szUsingName.Find('|'));
		szText = cFiles.GetText(&aszUsingFiles.GetText(i)[szUsingName.Length() + 1]);
		uiAt = 0;
		bAfterUsing = false;
		for (j = 0; szText && (szText[j] != '\0'); j = FindIDENextLine(szText, j))
		{
			if ((strncmp(&szText[j], "#include", 8) == 0) && !bAfterUsing)
			{
				uiAt = FindIDENextLine(szText, j);
			}
			else if (strncmp(&szText[j], "using ", 6) == 0)
			{
				uiAt = FindIDENextLine(szText, j);
				bAfterUsing = true;
			}
		}
		szLine.Init((uiAt > 0) && !bAfterUsing ? "\n\n" : "");
		szLine.Append("using ")->Append(szNew.Empty() ? "" : szNew.Text())->Append("::")->Append(szUsingName)->Append(";\n");
		asEdits.Add()->Init(&aszUsingFiles.GetText(i)[szUsingName.Length() + 1], uiAt, uiAt, szLine.Text());
		szLine.Kill();
		szUsingName.Kill();
	}

	//Its declarations: a qualified one's qualifier, the names in them qualified, then each moved.
	for (i = 0; i < psMove->asSites.NumElements(); i++)
	{
		psSite = psMove->asSites.Get(i);
		szText = cFiles.GetText(psSite->szFileName.Text());
		if (!szText)
		{
			continue;
		}
		QualifyInside(psMove, psSite, szNew.Text(), &cFiles, &asEdits);
		if (psSite->bQualified)
		{
			szScope.Init();
			szQualifier.Init();
			for (j = 0; j < psSite->asBlocks.NumElements(); j++)
			{
				szScope.Append((j == 0) ? "" : "::")->Append(psSite->asBlocks.Get(j)->szName);
			}
			AppendQualifier(szScope.Text(), szNew.Text(), &szQualifier);
			asEdits.Add()->Init(psSite->szFileName.Text(), psSite->uiQualifierStart, psSite->uiQualifierEnd, szQualifier.Text());
			szQualifier.Kill();
			szScope.Kill();
		}
	}
	JoinSites(psMove, &cFiles);
	for (i = 0; i < psMove->asSites.NumElements(); i++)
	{
		psSite = psMove->asSites.Get(i);
		if (!psSite->bQualified)
		{
			MoveSite(psSite, psMove->szNamespace.Text(), szNew.Text(), &cFiles, &asEdits);
		}
	}

	//The edits, latest first in each file; then hand written assembly's labels of it.
	for (i = 0; i < asEdits.NumElements(); i++)
	{
		psEdit = asEdits.Get(i);
		cFiles.GetEdits(psEdit->szFileName.Text())->Replace(psEdit->uiStart, psEdit->uiEnd, psEdit->szText.Text());
	}
	uiLabels = mpcContent->GetRename()->MoveAssembly(psMove->szKey.Text(), szNew.Text(), &cFiles, &uiUnsure);
	for (i = 0; i < psMove->aszWith.NumElements(); i++)
	{
		uiLabels += mpcContent->GetRename()->MoveAssembly(psMove->aszWith.GetText(i), szNew.Text(), &cFiles, &uiMore);
		uiUnsure += uiMore;
	}
	cFiles.Apply();
	for (i = 0; i < asEdits.NumElements(); i++)
	{
		asEdits.Get(i)->Kill();
	}
	asEdits.Kill();
	cFiles.Kill();
	KillUnits();

	pszStatus->Set("Moved ");
	GetIDEScopedName(psMove->szKey.Text(), pszStatus);
	for (i = 0; i < psMove->aszWith.NumElements(); i++)
	{
		pszStatus->Append((i + 1 == psMove->aszWith.NumElements()) ? " and " : ", ");
		GetIDEScopedName(psMove->aszWith.GetText(i), pszStatus);
	}
	pszStatus->Append(" to ")->Append(szNew.Empty() ? "the global namespace" : szNew.Text())->Append(": ")->Append((uint32)uiUses)->Append((uiUses == 1) ? " use" : " uses");
	pszStatus->Append(aszUsingFiles.NumElements() ? ", with using-declarations" : "");
	if (uiLabels > 0)
	{
		pszStatus->Append("; ")->Append((uint32)uiLabels)->Append((uiLabels == 1) ? " label in assembly changed" : " labels in assembly changed");
	}
	if (uiUnsure > 0)
	{
		pszStatus->Append("; ")->Append((uint32)uiUnsure)->Append((uiUnsure == 1) ? " label that may be an overload's left" : " labels that may be an overload's left");
	}
	pszStatus->Append(".");
	aszUsingFiles.Kill();
	szNew.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's field was typed in: the solution's namespaces that what is
// in it could mean, as Go to Symbol matches names, the best first.  None
// once it names one of them in full.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::NamespaceTyped(CTextField* pcField)
{
	CNameMatcher	cMatcher;
	SNameMatch		sMatch;
	CArrayChars		aszNamespaces;
	CArrayChars		aszFound;
	CArrayInt		aiScores;
	CArrayChars		aszTexts;
	CArrayChars		aszNotes;
	size			uiBest;
	size			i;

	aszTexts.Init();
	aszNotes.Init();
	if (pcField->GetText()[0] != '\0')
	{
		aszNamespaces.Init();
		mpcContent->GetCodeModel()->FindNamespaces(&aszNamespaces);
		cMatcher.Init(pcField->GetText());
		aszFound.Init();
		aiScores.Init();
		for (i = 0; i < aszNamespaces.NumElements(); i++)
		{
			if (cMatcher.Match(aszNamespaces.GetText(i), &sMatch))
			{
				aszFound.Add(aszNamespaces.GetText(i));
				aiScores.Add((int)sMatch.iScore);
			}
		}
		while (aszTexts.NumElements() < IDE_NAMESPACE_COMPLETIONS)
		{
			uiBest = MAX_SIZE;
			for (i = 0; i < aiScores.NumElements(); i++)
			{
				if ((aiScores.GetValue(i) != IDE_NAMESPACE_COMPLETION_TAKEN) && ((uiBest == MAX_SIZE) || (aiScores.GetValue(i) > aiScores.GetValue(uiBest))))
				{
					uiBest = i;
				}
			}
			if (uiBest == MAX_SIZE)
			{
				break;
			}
			*aiScores.Get(uiBest) = IDE_NAMESPACE_COMPLETION_TAKEN;
			aszTexts.Add(aszFound.GetText(uiBest));
			aszNotes.Add("namespace");
		}
		aiScores.Kill();
		aszFound.Kill();
		cMatcher.Kill();
		aszNamespaces.Kill();
		if ((aszTexts.NumElements() == 1) && aszTexts.Get(0)->Equals(pcField->GetText()))
		{
			aszTexts.ReInit();
			aszNotes.ReInit();
		}
	}
	mpNamespaceField->SetCompletions(&aszTexts, &aszNotes);
	aszNotes.Kill();
	aszTexts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Its dialog: the namespace to go to (the one it is in, to start with)
// and "Add using-declarations".
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::ShowMove(void)
{
	Ptr<CTable>			pTakers;
	size				i;
	SIDENamespaceMove	sMove;
	CChars				szTitle;
	CChars				szStatus;
	bool				bShown;

	sMove.Init();
	if (!FindMove(&sMove))
	{
		mpcContent->ShowStatus("There is nothing at the caret to move to another namespace: a class, function, variable or typedef at namespace scope.");
		KillUnits();
		sMove.Kill();
		return;
	}
	if (!sMove.bMovable)
	{
		szStatus.Init("Can't move it to another namespace: ");
		szStatus.Append(sMove.szWhyNot)->Append('.');
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
		KillUnits();
		sMove.Kill();
		return;
	}
	szTitle.Init("Move ");
	GetIDEScopedName(sMove.szKey.Text(), &szTitle);
	szTitle.Append(" to Namespace");
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szTitle.Text());
	szTitle.Kill();
	mpNamespaceField = OMalloc<CCompletionField>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, sMove.szNamespace.Text());
	mpNamespaceField->SetColumns(10);
	mpNamespaceField->SetAlignment(CSH_Stretch, CSV_Center);
	mpDialog->AddEditor("Namespace (empty for the global one):", mpNamespaceField);
	mpDialog->SetFirstEditor(mpNamespaceField);
	mpNamespaceField->SelectAll();
	mpNamespaceField->GetChangeEvent()->Add(this, &CIDEMoveNamespace::NamespaceTyped);
	mpUsingBox = mpDialog->AddCheckBox("Add using-declarations in source files instead of qualifying", mbUsings);
	pTakers = NULL;
	if (sMove.aszTakers.NumElements() > 0)
	{
		//Its old namespace's functions that take it, which calls find through it: ticked to go with it (question 63).
		mpDialog->AddDivider("Functions that take it");
		pTakers = OMalloc<CTable>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme);
		pTakers->AddColumn("Move", TCK_Tick, 0);
		pTakers->AddColumn("Function", TCK_Text, 1);
		pTakers->SetColumnEditable(IDE_NAMESPACE_COLUMN_NAME, false);
		for (i = 0; i < sMove.aszTakers.NumElements(); i++)
		{
			szTitle.Init();
			GetIDEScopedName(sMove.aszTakers.GetText(i), &szTitle);
			szTitle.Append(&strchr(sMove.aszTakers.GetText(i), '(')[0]);
			pTakers->AddRow();
			pTakers->SetCellTicked((int32)i, IDE_NAMESPACE_COLUMN_MOVE, true);
			pTakers->SetCellText((int32)i, IDE_NAMESPACE_COLUMN_NAME, szTitle.Text());
			szTitle.Kill();
		}
		pTakers->SetVisibleRows((sMove.aszTakers.NumElements() < IDE_NAMESPACE_VISIBLE_ROWS) ? (int32)sMove.aszTakers.NumElements() : IDE_NAMESPACE_VISIBLE_ROWS);
		pTakers->SetAlignment(CSH_Stretch, CSV_Top);
		pTakers->SelectCell(0, IDE_NAMESPACE_COLUMN_MOVE);
		mpDialog->AddEditor(NULL, pTakers);
	}
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bShown = mpDialog->Show(480);
	mpcContent->GetScript()->StopWatchingDialog();
	if (bShown)
	{
		mbUsings = mpUsingBox->IsTicked();
		for (i = 0; pTakers.IsNotNull() && (i < sMove.aszTakers.NumElements()); i++)
		{
			if (pTakers->IsCellTicked((int32)i, IDE_NAMESPACE_COLUMN_MOVE))
			{
				sMove.aszWith.Add(sMove.aszTakers.GetText(i));
			}
		}
		szStatus.Init();
		szTitle.Init(mpNamespaceField->GetText());
		Move(&sMove, szTitle.Text(), mbUsings, &szStatus);
		mpcContent->ShowStatus(szStatus.Text());
		szTitle.Kill();
		szStatus.Kill();
	}
	mpNamespaceField->GetChangeEvent()->Remove(this);
	pTakers = NULL;
	mpUsingBox = NULL;
	mpNamespaceField = NULL;
	mpDialog = NULL;
	KillUnits();
	sMove.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Sites of a file in the same blocks with nothing but blank lines
// between them made one, so they go into one block of the new namespace.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::JoinSites(SIDENamespaceMove* psMove, CIDEFileEdits* pcFiles)
{
	SIDENamespaceSite*	psSite;
	SIDENamespaceSite*	psOther;
	const char*			szText;
	size				i;
	size				j;
	bool				bJoined;

	bJoined = true;
	while (bJoined)
	{
		bJoined = false;
		for (i = 0; !bJoined && (i < psMove->asSites.NumElements()); i++)
		{
			psSite = psMove->asSites.Get(i);
			szText = pcFiles->GetText(psSite->szFileName.Text());
			for (j = 0; szText && !bJoined && !psSite->bQualified && (j < psMove->asSites.NumElements()); j++)
			{
				psOther = psMove->asSites.Get(j);
				if ((j != i) && !psOther->bQualified && (StringInsensitiveCompare(psSite->szFileName.Text(), psOther->szFileName.Text()) == 0) &&
					(psSite->asBlocks.NumElements() == psOther->asBlocks.NumElements()) && (psOther->uiStart >= psSite->uiEnd) && IsIDEBlank(szText, psSite->uiEnd, psOther->uiStart) &&
					((psSite->asBlocks.NumElements() == 0) || (psSite->asBlocks.Get(psSite->asBlocks.NumElements() - 1)->uiStart == psOther->asBlocks.Get(psOther->asBlocks.NumElements() - 1)->uiStart)))
				{
					psSite->uiEnd = psOther->uiEnd;
					psOther->Kill();
					psMove->asSites.RemoveAt(j);
					bJoined = true;
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where what goes from uiStart to uiEnd can go into a block of the new
// namespace instead of one of its own: after the opening line of the one
// that follows it or before the closing line of the one before it, with
// only blank lines between, when the new namespace is one level in from
// where it is (its last part, after uiShared shared ones).  MAX_SIZE if
// there is none.
//
//////////////////////////////////////////////////////////////////////////
size CIDEMoveNamespace::FindNeighbourBlock(const char* szText, size uiStart, size uiEnd, CArrayChars* paszNew, size uiShared)
{
	CChars		szOpen;
	size		uiLine;
	size		uiAt;
	size		uiBrace;
	int			iDepth;

	if (paszNew->NumElements() != uiShared + 1)
	{
		return MAX_SIZE;
	}
	szOpen.Init("namespace ");
	szOpen.Append(paszNew->GetText(uiShared));

	//The one after.
	for (uiLine = uiEnd; (szText[uiLine] != '\0') && IsIDEBlank(szText, uiLine, FindIDENextLine(szText, uiLine)); uiLine = FindIDENextLine(szText, uiLine))
	{
	}
	uiAt = uiLine + FindIndent(szText, uiLine);
	if ((strncmp(&szText[uiAt], szOpen.Text(), szOpen.Length()) == 0) && !IsIDENameChar(szText[uiAt + szOpen.Length()]))
	{
		uiLine = FindIDENextLine(szText, uiLine);
		if (szText[uiLine + FindIndent(szText, uiLine)] == '{')
		{
			szOpen.Kill();
			return FindIDENextLine(szText, uiLine);
		}
	}

	//The one before: its closing brace's line, back to its namespace line.
	uiLine = uiStart;
	while ((uiLine > 0) && IsIDEBlank(szText, FindIDELineStart(szText, uiLine - 1), uiLine))
	{
		uiLine = FindIDELineStart(szText, uiLine - 1);
	}
	if (uiLine > 0)
	{
		uiBrace = FindIDELineStart(szText, uiLine - 1);
		if (szText[uiBrace + FindIndent(szText, uiBrace)] == '}')
		{
			iDepth = 0;
			for (uiAt = uiBrace + FindIndent(szText, uiBrace); uiAt > 0; uiAt--)
			{
				iDepth += (szText[uiAt] == '}') ? 1 : ((szText[uiAt] == '{') ? -1 : 0);
				if (iDepth == 0)
				{
					break;
				}
			}
			if ((iDepth == 0) && (uiAt > 0))
			{
				uiAt = FindIDELineStart(szText, FindIDELineStart(szText, uiAt) - 1);
				uiAt += FindIndent(szText, uiAt);
				if ((strncmp(&szText[uiAt], szOpen.Text(), szOpen.Length()) == 0) && !IsIDENameChar(szText[uiAt + szOpen.Length()]))
				{
					szOpen.Kill();
					return uiBrace;
				}
			}
		}
	}
	szOpen.Kill();
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The last block of szNamespace ("A::B") in a file, by the index's
// places of the namespace's name there.  False if the file has none (or
// none that is named all the way out).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::FindBlockIn(const char* szFileName, const char* szNamespace, CIDEFileEdits* pcFiles, SIDENamespaceBlock* psDest)
{
	CArrayIDELocations			asLocations;
	CArrayIDENamespaceBlocks	asBlocks;
	SIDELocation*				psLocation;
	SIDENamespaceBlock*			psBlock;
	CIDERefactorUnit*			pcUnit;
	CCPPSyntax*					pcNode;
	CChars						szChain;
	const char*					szText;
	int32						iName;
	uint8						uiKind;
	size						i;
	size						j;
	bool						bUnnamed;
	bool						bFound;

	bFound = false;
	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(szNamespace, &asLocations, &uiKind);
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole == CPPIR_Reference) || (StringInsensitiveCompare(psLocation->szFileName.Text(), szFileName) != 0))
		{
			continue;
		}
		pcUnit = GetUnit(szFileName);
		szText = pcUnit ? pcFiles->GetText(szFileName) : NULL;
		iName = pcUnit ? pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn) : -1;
		pcNode = (iName >= 0) ? pcUnit->FindNodeOfTokens(iName, iName) : NULL;
		if (!szText || !pcNode)
		{
			continue;
		}
		asBlocks.Init();
		bUnnamed = false;
		FindBlocks(pcUnit, szText, pcNode, &asBlocks, &bUnnamed);
		szChain.Init();
		for (j = 0; j < asBlocks.NumElements(); j++)
		{
			szChain.Append((j == 0) ? "" : "::")->Append(asBlocks.Get(j)->szName);
		}
		if (!bUnnamed && szChain.Equals(szNamespace))
		{
			psBlock = asBlocks.Get(asBlocks.NumElements() - 1);
			if (!bFound || (psBlock->uiStart > psDest->uiStart))
			{
				psDest->szName.Set(psBlock->szName);
				psDest->uiStart = psBlock->uiStart;
				psDest->uiOpenEnd = psBlock->uiOpenEnd;
				psDest->uiCloseStart = psBlock->uiCloseStart;
				psDest->uiEnd = psBlock->uiEnd;
				bFound = true;
			}
		}
		szChain.Kill();
		for (j = 0; j < asBlocks.NumElements(); j++)
		{
			asBlocks.Get(j)->Kill();
		}
		asBlocks.Kill();
	}
	KillIDELocations(&asLocations);
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// What is taken out from *puiStart to *puiEnd of a site's file made to
// take the site's blocks too, innermost first, that would be left with
// nothing in them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::WidenToEmptyBlocks(SIDENamespaceSite* psSite, const char* szText, size* puiStart, size* puiEnd)
{
	SIDENamespaceBlock*	psBlock;
	size				i;

	for (i = psSite->asBlocks.NumElements(); i > 0; i--)
	{
		psBlock = psSite->asBlocks.Get(i - 1);
		if ((*puiStart < psBlock->uiOpenEnd) || (*puiEnd > psBlock->uiCloseStart) ||
			!IsIDEBlank(szText, psBlock->uiOpenEnd, *puiStart) || !IsIDEBlank(szText, *puiEnd, psBlock->uiCloseStart))
		{
			break;
		}
		*puiStart = psBlock->uiStart;
		*puiEnd = psBlock->uiEnd;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szText's lines without up to uiStrip spaces or tabs before each, and
// uiDepth tabs before each that isn't blank.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::AppendReindented(const char* szText, size uiStrip, size uiDepth, CChars* pszDest)
{
	size	uiLine;
	size	uiAt;
	size	uiEnd;
	size	i;

	for (uiLine = 0; szText[uiLine] != '\0'; uiLine = uiEnd)
	{
		for (uiEnd = uiLine; (szText[uiEnd] != '\0') && (szText[uiEnd] != '\n'); uiEnd++)
		{
		}
		if (szText[uiEnd] == '\n')
		{
			uiEnd++;
		}
		for (uiAt = uiLine; (uiAt < uiLine + uiStrip) && ((szText[uiAt] == '\t') || (szText[uiAt] == ' ')); uiAt++)
		{
		}
		if ((szText[uiAt] != '\n') && (szText[uiAt] != '\r') && (szText[uiAt] != '\0'))
		{
			for (i = 0; i < uiDepth; i++)
			{
				pszDest->Append('\t');
			}
		}
		pszDest->AppendSubString(szText, uiAt, uiEnd);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where a use of a function that isn't a call is a using-declaration of
// it (using Drawing::Plot;): from its line's start to after its ; and
// the line's end.  False if it isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::FindUsingDeclaration(SIDELocation* psUse, CIDEFileEdits* pcFiles, size* puiStart, size* puiEnd)
{
	CIDERefactorUnit	cUnit;
	SCPPSyntaxRange		sRange;
	char*				szText;
	int32				iToken;
	size				uiEnd;
	bool				bFound;

	szText = pcFiles->GetText(psUse->szFileName.Text());
	cUnit.Init();
	bFound = szText && cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), psUse->szFileName.Text(), false);
	iToken = bFound ? cUnit.FindTokenAt(psUse->iLine, psUse->iColumn) : -1;
	iToken = (iToken >= 0) ? FindQualifier(&cUnit, iToken) : -1;
	bFound = (iToken >= 1) && cUnit.IsToken(iToken - 1, "using") && !cUnit.IsToken(iToken, "namespace") && cUnit.GetTokensRange(iToken - 1, iToken - 1, &sRange);
	if (bFound)
	{
		*puiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
		uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		while ((*puiStart > 0) && (szText[*puiStart - 1] != '\n'))
		{
			(*puiStart)--;
		}
		for (*puiEnd = uiEnd; (szText[*puiEnd] != '\0') && (szText[*puiEnd] != ';'); (*puiEnd)++)
		{
		}
		for (; (szText[*puiEnd] != '\0') && (szText[*puiEnd] != '\n'); (*puiEnd)++)
		{
		}
		if (szText[*puiEnd] == '\n')
		{
			(*puiEnd)++;
		}
	}
	cUnit.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The uses of szKey (the move's, or a function's that goes with it), each
// named from where it is, as edits; a source file that would qualify one
// with bUsings is added to paszUsings ("name|file") to have a
// using-declaration instead.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveNamespace::MoveUses(SIDENamespaceMove* psMove, const char* szKey, const char* szNew, bool bUsings, CIDEFileEdits* pcFiles, CArrayIDENamespaceEdits* pasDest, CArrayChars* paszUsings, size* puiUses)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	SIDENamespaceSite*	psSite;
	CIDERefactorUnit*	pcUnit;
	SCPPSyntaxRange		sRange;
	CChars				szName;
	CChars				szScope;
	CChars				szQualifier;
	CChars				szLine;
	const char*			szText;
	size				uiAt;
	size				i;
	size				j;
	int32				iName;
	int32				iQualifier;
	uint8				uiKind;
	bool				bInside;

	szName.Init();
	pcModel = mpcContent->GetCodeModel();
	GetIDESymbolName(szKey, &szName);
	asLocations.Init();
	pcModel->GetSymbolLocations(szKey, &asLocations, &uiKind);
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole != CPPIR_Reference)
		{
			continue;
		}
		pcUnit = GetUnit(psLocation->szFileName.Text());
		szText = pcFiles->GetText(psLocation->szFileName.Text());
		iName = pcUnit ? pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn) : -1;
		if (!szText || (iName < 0))
		{
			continue;
		}
		iQualifier = FindQualifier(pcUnit, iName);
		szScope.Init();
		FindScopeAt(pcUnit, szText, iName, &szScope);
		bInside = false;
		for (j = 0; j < psMove->asSites.NumElements(); j++)
		{
			psSite = psMove->asSites.Get(j);
			uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
			bInside = bInside || (!psSite->bQualified && (StringInsensitiveCompare(psSite->szFileName.Text(), psLocation->szFileName.Text()) == 0) && (uiAt >= psSite->uiStart) && (uiAt < psSite->uiEnd));
		}
		if (bInside)
		{
			//In what moves: it is in the new namespace with it.
			szScope.Set(szNew);
		}
		szQualifier.Init();
		if ((iQualifier > 0) && pcUnit->IsToken(iQualifier - 1, "using") && !pcUnit->IsToken(iQualifier, "namespace"))
		{
			//A using-declaration of it: of the new namespace, or gone where it isn't needed.
			AppendQualifier(szScope.Text(), szNew, &szQualifier);
			if (szQualifier.Empty() && (szNew[0] != '\0'))
			{
				pcUnit->GetTokensRange(iQualifier - 1, iName, &sRange);
				uiAt = FindIDELineStart(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
				pasDest->Add()->Init(psLocation->szFileName.Text(), uiAt, FindIDENextLine(szText, uiAt), "");
			}
			else
			{
				pcUnit->GetTokensRange(iQualifier, iName, &sRange);
				szLine.Init(szQualifier.Empty() ? "::" : szQualifier.Text());
				pasDest->Add()->Init(psLocation->szFileName.Text(), FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn), szLine.Text());
				szLine.Kill();
			}
		}
		else
		{
			AppendShortestQualifier(pcUnit, iName, szScope.Text(), szNew, &szQualifier);
			if (bUsings && !szQualifier.Empty() && !bInside && (psLocation->szFileName.EndsWithIgnoreCase(".cpp") || psLocation->szFileName.EndsWithIgnoreCase(".c")))
			{
				szQualifier.Clear();
				szLine.Init(szName);
				szLine.Append('|')->Append(psLocation->szFileName);
				paszUsings->AddIfUnique(szLine);
				szLine.Kill();
			}
			pcUnit->GetTokensRange(iQualifier, iQualifier, &sRange);
			pasDest->Add()->Init(psLocation->szFileName.Text(), FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn), szQualifier.Text());
		}
		(*puiUses)++;
		szQualifier.Kill();
		szScope.Kill();
	}
	KillIDELocations(&asLocations);
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a namespace being moved is declared inline in one of its
// blocks (moving it would change what its enclosing namespace has).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveNamespace::IsInlineNamespace(SIDENamespaceMove* psMove, CIDEFileEdits* pcFiles)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CIDERefactorUnit*	pcUnit;
	CCPPSyntax*			pcNode;
	int32				iToken;
	size				i;
	uint8				uiKind;
	bool				bInline;

	bInline = false;
	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(psMove->szKey.Text(), &asLocations, &uiKind);
	for (i = 0; !bInline && (i < asLocations.NumElements()); i++)
	{
		//Each block's namespace name.
		psLocation = asLocations.Get(i);
		pcUnit = (psLocation->uiRole != CPPIR_Reference) ? GetUnit(psLocation->szFileName.Text()) : NULL;
		iToken = pcUnit ? pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn) : -1;
		pcNode = (iToken >= 0) ? pcUnit->FindAncestor(pcUnit->FindNodeOfTokens(iToken, iToken), CPPS_Namespace) : NULL;
		bInline = pcNode && ((CCPPSyntaxNamespace*)pcNode)->mbInline;
	}
	KillIDELocations(&asLocations);
	return bInline;
}
