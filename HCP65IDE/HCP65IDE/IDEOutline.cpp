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
#include <string.h>
#include "CppCompilerLib/SourceFile.h"
#include "CppCompilerLib/CppTokeniser.h"
#include "CppCompilerLib/CPPSyntaxTree.h"
#include "CppCompilerLib/CPPSyntaxTranslationUnit.h"
#include "CppCompilerLib/CPPSyntaxEnums.h"
#include "CppCompilerLib/CPPSyntaxNamespace.h"
#include "CppCompilerLib/CPPSyntaxClassSpecifier.h"
#include "CppCompilerLib/CPPSyntaxEnumSpecifier.h"
#include "CppCompilerLib/CPPSyntaxFunctionDefinition.h"
#include "CppCompilerLib/CPPSyntaxSimpleDeclaration.h"
#include "CppCompilerLib/CPPSyntaxAliasDeclaration.h"
#include "CppCompilerLib/CPPSyntaxDeclSpecifiers.h"
#include "CppCompilerLib/CPPSyntaxInitDeclarator.h"
#include "CppCompilerLib/CPPSyntaxDeclarator.h"
#include "CppCompilerLib/CPPSyntaxName.h"
#include "CppCompilerLib/CPPSyntaxNameComponent.h"
#include "IDERename.h"
#include "IDEOutline.h"


static const char*	gaszIDEOutlineKinds[IOK_NumKinds] = { "namespace", "class", "enum", "function", "field", "variable", "typedef" };


//////////////////////////////////////////////////////////////////////////
//
// The tokens from iFirstToken to iLastToken as they would be written on
// one line, appended to pszDest: a space only between two words and after
// a comma ("(int iX, int iY)").  With bDeclaration, to be read and not
// compiled, a word also has a space before it after a ')', a '*', a '&'
// or a '>', as a declaration is written ("(CShape* pcShape) const").
//
//////////////////////////////////////////////////////////////////////////
void AppendIDETokens(CCPPTokeniser* pcTokeniser, int32 iFirstToken, int32 iLastToken, CChars* pszDest, bool bDeclaration)
{
	SPreprocessedToken*	psSource;
	int32				i;
	char				cLast;
	char				cFirst;

	for (i = iFirstToken; (i >= 0) && (i <= iLastToken) && ((size)i < pcTokeniser->NumTokens()); i++)
	{
		psSource = pcTokeniser->GetSource(pcTokeniser->Get((size)i));
		if (psSource->uiLength == 0)
		{
			continue;
		}
		if (!pszDest->Empty() && (i > iFirstToken))
		{
			cLast = pszDest->Text()[pszDest->Length() - 1];
			cFirst = psSource->szText[0];
			if ((IsIDENameChar(cLast) && IsIDENameChar(cFirst)) || (cLast == ','))
			{
				pszDest->Append(' ');
			}
			else if (bDeclaration && IsIDENameChar(cFirst) && ((cLast == ')') || (cLast == '*') || (cLast == '&') || (cLast == '>')))
			{
				pszDest->Append(' ');
			}
		}
		pszDest->Append(psSource->szText, psSource->uiLength);
	}
}


const char* GetIDEOutlineKindName(EIDEOutlineKind eKind) { return gaszIDEOutlineKinds[eKind]; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutline::Init(void)
{
	masEntries.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutline::Kill(void)
{
	Clear();
	masEntries.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutline::Clear(void)
{
	size	i;

	for (i = 0; i < masEntries.NumElements(); i++)
	{
		masEntries.Get(i)->szName.Kill();
		masEntries.Get(i)->szDetail.Kill();
	}
	masEntries.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// pcSource's entries become this outline's, and pcSource is left empty.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutline::TakeFrom(CIDEOutline* pcSource)
{
	size	i;

	Clear();
	for (i = 0; i < pcSource->masEntries.NumElements(); i++)
	{
		memcpy(masEntries.Add(), pcSource->masEntries.Get(i), sizeof(SIDEOutlineEntry));
	}
	pcSource->masEntries.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// A new entry in entry iParent (-1 for none), its texts empty.
//
//////////////////////////////////////////////////////////////////////////
SIDEOutlineEntry* CIDEOutline::Add(EIDEOutlineKind eKind, int32 iParent)
{
	SIDEOutlineEntry*	psEntry;
	int32				iDepth;

	iDepth = (iParent >= 0) ? masEntries.Get((size)iParent)->iDepth + 1 : 0;
	psEntry = masEntries.Add();
	memset(psEntry, 0, sizeof(SIDEOutlineEntry));
	psEntry->eKind = eKind;
	psEntry->szName.Init();
	psEntry->szDetail.Init();
	psEntry->iParent = iParent;
	psEntry->iDepth = iDepth;
	return psEntry;
}


//Walks a unit's syntax tree making the outline of one of its files.
class CIDEOutlineVisitor : public CCPPSyntaxChildVisitor
{
public:
	CIDEOutline*		mpcOutline;
	CCPPHighlighter*	mpcHighlighter;
	CSourceFile*		mpcFile;
	CCPPTokeniser*		mpcTokeniser;
	int32				miParent;	//The entry what is visited is in, -1 at the top.

	void	Visit(CCPPSyntax* pcChild) override;
	bool	GetRange(int32 iFirstToken, int32 iLastToken, SCPPSyntaxRange* psDest);
	int32	AddEntry(EIDEOutlineKind eKind, CCPPSyntax* pcNode, int32 iFirstName, int32 iLastName, int32 iPlaceToken, bool bDefinition);
	void	AddDeclarator(EIDEOutlineKind eKind, CCPPSyntax* pcWhole, CCPPSyntaxDeclarator* pcDeclarator, bool bDefinition);
	void	VisitIn(int32 iParent, CCPPSyntax* pcNode);
};


//////////////////////////////////////////////////////////////////////////
//
// The range of the file the tokens are written in.  False if they aren't
// this file's (another file's, or a macro's).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEOutlineVisitor::GetRange(int32 iFirstToken, int32 iLastToken, SCPPSyntaxRange* psDest)
{
	return (iFirstToken >= 0) && (iLastToken >= iFirstToken) && mpcHighlighter->GetTokensRange(mpcFile, mpcTokeniser, iFirstToken, iLastToken, psDest);
}


//////////////////////////////////////////////////////////////////////////
//
// An entry for pcNode, named by the tokens iFirstName to iLastName, its
// name's place that of iPlaceToken.  Its index, or -1 if pcNode or its
// name isn't written in the file.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEOutlineVisitor::AddEntry(EIDEOutlineKind eKind, CCPPSyntax* pcNode, int32 iFirstName, int32 iLastName, int32 iPlaceToken, bool bDefinition)
{
	SIDEOutlineEntry*	psEntry;
	SCPPSyntaxRange		sRange;
	SCPPSyntaxRange		sName;

	if (!GetRange(pcNode->miFirstToken, pcNode->miLastToken, &sRange) || !GetRange(iPlaceToken, iPlaceToken, &sName))
	{
		return -1;
	}
	psEntry = mpcOutline->Add(eKind, miParent);
	psEntry->bDefinition = bDefinition;
	psEntry->sRange = sRange;
	psEntry->sName = sName;
	AppendIDETokens(mpcTokeniser, iFirstName, iLastName, &psEntry->szName);
	return (int32)mpcOutline->NumEntries() - 1;
}


//////////////////////////////////////////////////////////////////////////
//
// An entry for what pcDeclarator declares, in the declaration pcWhole:
// its name as it is written, and after it what the declarator has after
// its name (a function's parameters and qualifiers).
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutlineVisitor::AddDeclarator(EIDEOutlineKind eKind, CCPPSyntax* pcWhole, CCPPSyntaxDeclarator* pcDeclarator, bool bDefinition)
{
	CCPPSyntaxName*		pcName;
	int32				iEntry;

	pcName = pcDeclarator ? pcDeclarator->GetName() : NULL;
	if (!pcName || !pcName->GetLast())
	{
		return;
	}
	iEntry = AddEntry(eKind, pcWhole, pcName->miFirstToken, pcName->miLastToken, pcName->GetLast()->miFirstToken, bDefinition);
	if ((iEntry >= 0) && (eKind == IOK_Function) && (pcDeclarator->miLastToken > pcName->miLastToken))
	{
		AppendIDETokens(mpcTokeniser, pcName->miLastToken + 1, pcDeclarator->miLastToken, &mpcOutline->Get((size)iEntry)->szDetail, true);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// pcNode's children are visited as what entry iParent has in it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutlineVisitor::VisitIn(int32 iParent, CCPPSyntax* pcNode)
{
	int32	iWas;

	iWas = miParent;
	miParent = iParent;
	pcNode->VisitFields(this);
	miParent = iWas;
}


//////////////////////////////////////////////////////////////////////////
//
// A namespace, a class or an enum with its members, a function's
// definition and what a declaration declares are entries; what is in a
// namespace or a class is visited as theirs.  Nothing in a function's
// body is looked at.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutlineVisitor::Visit(CCPPSyntax* pcChild)
{
	CCPPSyntaxNamespace*			pcNamespace;
	CCPPSyntaxClassSpecifier*		pcClass;
	CCPPSyntaxEnumSpecifier*		pcEnum;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CCPPSyntaxSimpleDeclaration*	pcDeclaration;
	CCPPSyntaxAliasDeclaration*		pcAlias;
	CCPPSyntaxDeclarator*			pcDeclarator;
	CCPPSyntax*						pcDeclared;
	EIDEOutlineKind					eKind;
	size							i;
	int32							iEntry;
	bool							bInClass;

	switch (pcChild->GetKind())
	{
	case CPPS_CompoundStatement:
		break;

	case CPPS_Namespace:
		pcNamespace = (CCPPSyntaxNamespace*)pcChild;
		iEntry = (pcNamespace->miName >= 0) ? AddEntry(IOK_Namespace, pcChild, pcNamespace->miName, pcNamespace->miName, pcNamespace->miName, true) : -1;
		VisitIn((iEntry >= 0) ? iEntry : miParent, pcChild);
		break;

	case CPPS_ClassSpecifier:
		pcClass = (CCPPSyntaxClassSpecifier*)pcChild;
		iEntry = -1;
		if (pcClass->mpcName && pcClass->mpcName->GetLast())
		{
			iEntry = AddEntry(IOK_Class, pcChild, pcClass->mpcName->miFirstToken, pcClass->mpcName->miLastToken, pcClass->mpcName->GetLast()->miFirstToken, true);
		}
		VisitIn((iEntry >= 0) ? iEntry : miParent, pcChild);
		break;

	case CPPS_EnumSpecifier:
		pcEnum = (CCPPSyntaxEnumSpecifier*)pcChild;
		if (pcEnum->mbBody && pcEnum->mpcName && pcEnum->mpcName->GetLast())
		{
			AddEntry(IOK_Enum, pcChild, pcEnum->mpcName->miFirstToken, pcEnum->mpcName->miLastToken, pcEnum->mpcName->GetLast()->miFirstToken, true);
		}
		break;

	case CPPS_FunctionDefinition:
		pcFunction = (CCPPSyntaxFunctionDefinition*)pcChild;
		AddDeclarator(IOK_Function, pcChild, pcFunction->mpcDeclarator, true);
		break;

	case CPPS_SimpleDeclaration:
		pcDeclaration = (CCPPSyntaxSimpleDeclaration*)pcChild;

		//A class or an enum the declaration defines ("class CShape {...};") comes before what it declares.
		if (pcDeclaration->mpcSpecifiers && pcDeclaration->mpcSpecifiers->mpcType)
		{
			Visit(pcDeclaration->mpcSpecifiers->mpcType);
		}
		bInClass = (miParent >= 0) && (mpcOutline->Get((size)miParent)->eKind == IOK_Class);
		for (i = 0; i < pcDeclaration->macpDeclarators.NumElements(); i++)
		{
			pcDeclared = pcDeclaration->macpDeclarators.GetPtr(i);
			pcDeclarator = NULL;
			if (pcDeclared && (pcDeclared->GetKind() == CPPS_InitDeclarator))
			{
				pcDeclarator = ((CCPPSyntaxInitDeclarator*)pcDeclared)->mpcDeclarator;
			}
			else if (pcDeclared && (pcDeclared->GetKind() == CPPS_Declarator))
			{
				pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclared;
			}
			if (pcDeclarator == NULL)
			{
				continue;
			}
			if (pcDeclaration->mpcSpecifiers && (pcDeclaration->mpcSpecifiers->muiFlags & CPPDS_TYPEDEF))
			{
				eKind = IOK_Typedef;
			}
			else if (pcDeclarator->IsFunction())
			{
				eKind = IOK_Function;
			}
			else
			{
				eKind = bInClass ? IOK_Field : IOK_Variable;
			}
			AddDeclarator(eKind, pcChild, pcDeclarator, false);
		}
		break;

	case CPPS_AliasDeclaration:
		pcAlias = (CCPPSyntaxAliasDeclaration*)pcChild;
		if (pcAlias->miName >= 0)
		{
			AddEntry(IOK_Typedef, pcChild, pcAlias->miName, pcAlias->miName, pcAlias->miName, false);
		}
		break;

	default:
		pcChild->VisitFields(this);
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The outline of pcFile (a unit's own file, or a header it included),
// from the unit's tokens and syntax tree.  What it had is forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutline::Gather(CSourceFile* pcFile, CCPPTokeniser* pcTokeniser, CCPPSyntaxTree* pcTree)
{
	CIDEOutlineVisitor	cVisitor;
	CCPPHighlighter		cHighlighter;

	Clear();
	if (pcTree->GetRoot() == NULL)
	{
		return;
	}
	cHighlighter.Init();
	cVisitor.mpcOutline = this;
	cVisitor.mpcHighlighter = &cHighlighter;
	cVisitor.mpcFile = pcFile;
	cVisitor.mpcTokeniser = pcTokeniser;
	cVisitor.miParent = -1;
	pcTree->GetRoot()->VisitFields(&cVisitor);
	cHighlighter.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The innermost entry a place (a line and column, from 0) is in, -1 if
// it is in none.  Entries are in the order the file has them, each after
// the one it is in, so the last that has the place is the innermost.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEOutline::FindEntryAt(int iLine, int iColumn)
{
	SIDEOutlineEntry*	psEntry;
	size				i;
	int32				iFound;

	iFound = -1;
	for (i = 0; i < masEntries.NumElements(); i++)
	{
		psEntry = masEntries.Get(i);
		if ((psEntry->sRange.iLine > iLine) || ((psEntry->sRange.iLine == iLine) && (psEntry->sRange.iColumn > iColumn)))
		{
			break;
		}
		if ((psEntry->sRange.iEndLine > iLine) || ((psEntry->sRange.iEndLine == iLine) && (psEntry->sRange.iEndColumn > iColumn)))
		{
			iFound = (int32)i;
		}
	}
	return iFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The function a place is in (its definition or its declaration), -1 if
// it is in none: the entry at the place, or the nearest entry it is in
// that is a function.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEOutline::FindFunctionAt(int iLine, int iColumn)
{
	int32	iEntry;

	iEntry = FindEntryAt(iLine, iColumn);
	while ((iEntry >= 0) && (masEntries.Get((size)iEntry)->eKind != IOK_Function))
	{
		iEntry = masEntries.Get((size)iEntry)->iParent;
	}
	return iEntry;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, a line for each entry, indented by how deep it is:
// "  function Draw(void) 14 [14:1-14:27]", a definition's kind with a '*'
// after it, the name's line then all of it, from 1.
//
//////////////////////////////////////////////////////////////////////////
void CIDEOutline::Print(CChars* pszDest)
{
	SIDEOutlineEntry*	psEntry;
	size				i;
	int32				j;

	for (i = 0; i < masEntries.NumElements(); i++)
	{
		psEntry = masEntries.Get(i);
		for (j = 0; j < psEntry->iDepth; j++)
		{
			pszDest->Append("  ");
		}
		pszDest->Append(gaszIDEOutlineKinds[psEntry->eKind]);
		if (psEntry->bDefinition)
		{
			pszDest->Append('*');
		}
		pszDest->Append(' ');
		pszDest->Append(psEntry->szName);
		pszDest->Append(psEntry->szDetail);
		pszDest->Append(' ');
		pszDest->Append(psEntry->sName.iLine + 1);
		pszDest->Append(" [");
		pszDest->Append(psEntry->sRange.iLine + 1);
		pszDest->Append(':');
		pszDest->Append(psEntry->sRange.iColumn + 1);
		pszDest->Append('-');
		pszDest->Append(psEntry->sRange.iEndLine + 1);
		pszDest->Append(':');
		pszDest->Append(psEntry->sRange.iEndColumn + 1);
		pszDest->Append("]\n");
	}
}


size CIDEOutline::NumEntries(void) const { return masEntries.NumElements(); }
SIDEOutlineEntry* CIDEOutline::Get(size uiIndex) { return masEntries.Get(uiIndex); }

