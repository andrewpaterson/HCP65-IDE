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
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDERenameAssembly.h"
#include "IDESafeDelete.h"
#include "IDEContextActions.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESafeDeletion::Init(void)
{
	szKey.Init();
	szName.Init();
	uiKind = 0;
	aszKeys.Init();
	asRanges.Init();
	aiStatements.Init();
	asUsages.Init();
	uiOverriders = 0;
	uiAssembly = 0;
	uiComments = 0;
	uiText = 0;
	bDeletable = false;
	szWhyNot.Init();
	aszFiles.Init();
	bDeleteFiles = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDESafeDeletion::Kill(void)
{
	aszFiles.Kill();
	szWhyNot.Kill();
	KillIDELocations(&asUsages);
	aiStatements.Kill();
	KillIDEFileRanges(&asRanges);
	aszKeys.Kill();
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The start of the line after the one uiAt is on (the text's end if it
// is the last).
//
//////////////////////////////////////////////////////////////////////////
size FindIDENextLine(const char* szText, size uiAt)
{
	for (; (szText[uiAt] != '\0') && (szText[uiAt] != '\n'); uiAt++)
	{
	}
	return (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// The start of the line uiAt is on.
//
//////////////////////////////////////////////////////////////////////////
size FindIDELineStart(const char* szText, size uiAt)
{
	while ((uiAt > 0) && (szText[uiAt - 1] != '\n'))
	{
		uiAt--;
	}
	return uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a line and column (from 0) are in szText; its length if the text
// ends first.
//
//////////////////////////////////////////////////////////////////////////
size FindIDETextOffset(const char* szText, int32 iLine, int32 iColumn)
{
	size	i;
	int32	iAtLine;

	iAtLine = 0;
	for (i = 0; (szText[i] != '\0') && (iAtLine < iLine); i++)
	{
		if (szText[i] == '\n')
		{
			iAtLine++;
		}
	}
	for (; (szText[i] != '\0') && (szText[i] != '\n') && (iColumn > 0); i++, iColumn--)
	{
	}
	return i;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the line starting at uiLine is blank, or only a // comment
// (bComment); *puiNext is the start of the line after it.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEBlankOrCommentLine(const char* szText, size uiLine, bool bComment, size* puiNext)
{
	size	i;
	bool	bIs;

	for (i = uiLine; (szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r'); i++)
	{
	}
	if (bComment)
	{
		bIs = (szText[i] == '/') && (szText[i + 1] == '/');
	}
	else
	{
		bIs = (szText[i] == '\n') || (szText[i] == '\0');
	}
	for (; (szText[i] != '\0') && (szText[i] != '\n'); i++)
	{
	}
	*puiNext = (szText[i] == '\n') ? i + 1 : i;
	return bIs;
}


//////////////////////////////////////////////////////////////////////////
//
// The start of the line before the one starting at uiLine (uiLine > 0).
//
//////////////////////////////////////////////////////////////////////////
static size FindIDEPreviousLine(const char* szText, size uiLine)
{
	size	i;

	for (i = uiLine - 1; (i > 0) && (szText[i - 1] != '\n'); i--)
	{
	}
	return i;
}


//////////////////////////////////////////////////////////////////////////
//
// What is taken out of szText with a declaration written from *puiStart
// to *puiEnd, so the text is left as if it had never been there.
//
// A whole declaration (bStatement): its semicolon, if that isn't in it.
// If nothing else is on its lines but white space and a comment after
// it, the lines go, with the lines of // comments directly above it (the
// house style's banner over a function) and, if a blank line or the
// start of the text is above that, the blank lines below (the gap above
// is the gap that is left).  At the end of the text the blank lines
// above go instead.
//
// A part of a declaration (a declarator with its comma, an enumerator
// with its): the same if it has its lines to itself; otherwise the
// spaces after it, so what follows closes up.
//
//////////////////////////////////////////////////////////////////////////
void ExtendIDEDeletion(const char* szText, size* puiStart, size* puiEnd, bool bStatement)
{
	size	uiStart;
	size	uiEnd;
	size	uiLineStart;
	size	uiAfter;
	size	uiNext;
	size	uiPrevious;
	bool	bLineStart;
	bool	bLineEnd;
	bool	bGapAbove;

	uiStart = *puiStart;
	uiEnd = *puiEnd;
	if (bStatement)
	{
		for (uiAfter = uiEnd; (szText[uiAfter] == ' ') || (szText[uiAfter] == '\t'); uiAfter++)
		{
		}
		if (szText[uiAfter] == ';')
		{
			uiEnd = uiAfter + 1;
		}
	}

	for (uiLineStart = uiStart; (uiLineStart > 0) && ((szText[uiLineStart - 1] == ' ') || (szText[uiLineStart - 1] == '\t')); uiLineStart--)
	{
	}
	bLineStart = (uiLineStart == 0) || (szText[uiLineStart - 1] == '\n');
	for (uiAfter = uiEnd; (szText[uiAfter] == ' ') || (szText[uiAfter] == '\t') || (szText[uiAfter] == '\r'); uiAfter++)
	{
	}
	bLineEnd = (szText[uiAfter] == '\n') || (szText[uiAfter] == '\0') || ((szText[uiAfter] == '/') && (szText[uiAfter + 1] == '/'));

	if (!bLineStart || !bLineEnd)
	{
		//Other things are on its lines: what follows closes up.
		for (; (szText[uiEnd] == ' ') || (szText[uiEnd] == '\t'); uiEnd++)
		{
		}
		*puiStart = uiStart;
		*puiEnd = uiEnd;
		return;
	}

	uiStart = uiLineStart;
	for (uiEnd = uiAfter; (szText[uiEnd] != '\0') && (szText[uiEnd] != '\n'); uiEnd++)
	{
	}
	if (szText[uiEnd] == '\n')
	{
		uiEnd++;
	}

	//The comment lines directly above it.
	while (uiStart > 0)
	{
		uiPrevious = FindIDEPreviousLine(szText, uiStart);
		if (!IsIDEBlankOrCommentLine(szText, uiPrevious, true, &uiNext))
		{
			break;
		}
		uiStart = uiPrevious;
	}

	bGapAbove = (uiStart == 0);
	if (uiStart > 0)
	{
		uiPrevious = FindIDEPreviousLine(szText, uiStart);
		bGapAbove = IsIDEBlankOrCommentLine(szText, uiPrevious, false, &uiNext);
	}
	if (szText[uiEnd] == '\0')
	{
		//The last thing in the text: the blank lines above it go.
		while (uiStart > 0)
		{
			uiPrevious = FindIDEPreviousLine(szText, uiStart);
			if (!IsIDEBlankOrCommentLine(szText, uiPrevious, false, &uiNext))
			{
				break;
			}
			uiStart = uiPrevious;
		}
	}
	else if (bGapAbove)
	{
		while ((szText[uiEnd] != '\0') && IsIDEBlankOrCommentLine(szText, uiEnd, false, &uiNext))
		{
			uiEnd = uiNext;
		}
	}
	*puiStart = uiStart;
	*puiEnd = uiEnd;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mapcUnits.Init();
	mbComments = false;
	mbText = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpCommentsBox);
	M_Pointer(mpFilesBox);
	M_Pointer(mpTextBox);
	U_Unknown(CArrayIDERefactorUnits, mapcUnits);
	U_Bool(mbComments);
	U_Bool(mbText);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::Free(void)
{
	KillUnits();
	mapcUnits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::Save(CObjectWriter* pcFile) { return false; }
bool CIDESafeDelete::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Safe Delete (Alt+Delete, as IntelliJ IDEA's), while a symbol is named
// at the caret.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.SafeDelete", "Safe Delete...", "Alt+Delete", Ptr<CImageCel>(), this, &CIDESafeDelete::QueueSafeDelete)->SetEnabledCheck(this, &CIDESafeDelete::CanSafeDelete);
}


//////////////////////////////////////////////////////////////////////////
//
// The editors' context menu's Refactor gets Safe Delete.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.SafeDelete"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: Safe Delete is for what is declared at the caret.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::AddContextActions(SIDECaretContext* psContext)
{
	bool	bDeletable;

	bDeletable = (psContext->uiKind != CPPSK_Parameter) && (psContext->uiKind != CPPSK_TypeParameter) && (psContext->uiKind != CPPSK_ValueParameter) && (psContext->uiKind != CPPSK_TemplateTemplateParameter) && (psContext->uiKind != CPPSK_Label) &&
				 (psContext->uiKind != CPPSK_Enumerator) && (psContext->uiKind != CPPSK_BuiltinType);
	if (psContext->bSymbol && (psContext->uiRole != CPPIR_Reference) && bDeletable)
	{
		psContext->Offer("Refactor.SafeDelete");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::CanSafeDelete(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


//////////////////////////////////////////////////////////////////////////
//
// As IntelliJ's two searches: each is off until asked for.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::SetSearches(bool bComments, bool bText)
{
	mbComments = bComments;
	mbText = bText;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialogs are shown at the next tick: ShowModal can't be called from
// the input that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::QueueSafeDelete(void)
{
	mpcContent->GetWindow()->Post(this, &CIDESafeDelete::ShowQueuedSafeDelete);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::ShowQueuedSafeDelete(void)
{
	ShowSafeDelete();
}


//////////////////////////////////////////////////////////////////////////
//
// The file parsed (its unit, for a header), once for the deletion being
// found.  NULL if it can't be.
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDESafeDelete::GetUnit(const char* szFileName)
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
void CIDESafeDelete::KillUnits(void)
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
// What is taken out for the declaration or definition of a symbol named
// at psLocation, as the syntax tree has it:
//   - a function's definition, all of it;
//   - a declaration that declares only it (a function's, a variable's, a
//     field's, a typedef's), or its declarator and a comma if the
//     declaration has others ("int a, b;");
//   - a class's or an enum's definition, and a declaration of only its
//     name ("class CFoo;", which sets *pbForward);
//   - an enumerator, and a comma;
//   - an alias ("using T = ..."), a namespace's block;
// with the template<...> it is declared in, and as a statement if it is
// a local's.  *pbStatement is whether it is a whole declaration.
//
// False, with why in *pszWhyNot, if it can't be taken out: a macro made
// it or the configuration leaves it out (there is no token of it), it is
// a parameter, or it is declared with something that stays.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::FindDeclarationRange(SIDELocation* psLocation, SIDEFileRange* psDest, bool* pbStatement, bool* pbForward, const char** pszWhyNot)
{
	CIDERefactorUnit*				pcUnit;
	int32							iToken;
	int32							iFirst;
	int32							iLast;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcDelete;
	CCPPSyntaxSimpleDeclaration*	pcSimple;
	SCPPSyntaxRange					sRange;
	bool							bPart;

	*pbStatement = true;
	*pbForward = false;
	pcUnit = GetUnit(psLocation->szFileName.Text());
	if (!pcUnit)
	{
		*pszWhyNot = "a file it is declared in couldn't be parsed";
		return false;
	}
	iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
	pcNode = pcUnit->FindNodeOfTokens(iToken, iToken);
	if (!pcNode)
	{
		*pszWhyNot = "a declaration of it is made by a macro, or is left out by the configuration";
		return false;
	}

	pcDelete = NULL;
	bPart = false;
	iFirst = -1;
	iLast = -1;
	for (; pcNode && !pcDelete && !bPart; pcNode = pcNode->mpcParent)
	{
		switch (pcNode->GetKind())
		{
		case CPPS_Parameter:
		case CPPS_TypeParameter:
			*pszWhyNot = "a parameter is removed by changing its function's signature";
			return false;
		case CPPS_LabelStatement:
			*pszWhyNot = "it is a label";
			return false;
		case CPPS_Enumerator:
		case CPPS_InitDeclarator:
			pcSimple = (CCPPSyntaxSimpleDeclaration*)pcNode->mpcParent;
			if (pcNode->GetKind() == CPPS_InitDeclarator)
			{
				if (!pcSimple || (pcSimple->GetKind() != CPPS_SimpleDeclaration))
				{
					*pszWhyNot = "it is declared in a condition";
					return false;
				}
				*pbForward = pcSimple->mpcSpecifiers && (pcSimple->mpcSpecifiers->muiFlags & CPPDS_FRIEND);
				if (pcSimple->macpDeclarators.NumElements() == 1)
				{
					pcDelete = pcSimple;
					break;
				}
			}
			//One of several: it goes with the comma after it, or the one before it if it is the last.
			bPart = true;
			iFirst = pcNode->miFirstToken;
			iLast = pcNode->miLastToken;
			if (pcUnit->IsToken(iLast + 1, ","))
			{
				iLast++;
			}
			else if (pcUnit->IsToken(iFirst - 1, ","))
			{
				iFirst--;
			}
			break;
		case CPPS_FunctionDefinition:
		case CPPS_AliasDeclaration:
		case CPPS_Namespace:
			pcDelete = pcNode;
			break;
		case CPPS_ClassSpecifier:
		case CPPS_EnumSpecifier:
		case CPPS_ElaboratedTypeSpecifier:
			pcSimple = (CCPPSyntaxSimpleDeclaration*)pcUnit->FindAncestor(pcNode, CPPS_SimpleDeclaration);
			if (!pcSimple || (pcSimple->macpDeclarators.NumElements() != 0))
			{
				*pszWhyNot = "it is declared with a variable of it, which would be left";
				return false;
			}
			*pbForward = (pcNode->GetKind() == CPPS_ElaboratedTypeSpecifier);
			pcDelete = pcSimple;
			break;
		default:
			break;
		}
	}

	if (pcDelete)
	{
		while (pcDelete->mpcParent && ((pcDelete->mpcParent->GetKind() == CPPS_TemplateDeclaration) || (pcDelete->mpcParent->GetKind() == CPPS_DeclarationStatement)))
		{
			pcDelete = pcDelete->mpcParent;
		}
		iFirst = pcDelete->miFirstToken;
		iLast = pcDelete->miLastToken;
	}
	else if (!bPart)
	{
		*pszWhyNot = "what declares it isn't something Safe Delete takes out";
		return false;
	}
	if (!pcUnit->GetTokensRange(iFirst, iLast, &sRange))
	{
		*pszWhyNot = "a declaration of it is partly made by a macro";
		return false;
	}
	*pbStatement = !bPart;
	psDest->Init(psLocation->szFileName.Text(), sRange.iLine, sRange.iColumn, sRange.iEndLine, sRange.iEndColumn);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::IsInRanges(SIDESafeDeletion* psDeletion, SIDELocation* psLocation)
{
	size	i;

	for (i = 0; i < psDeletion->asRanges.NumElements(); i++)
	{
		if (psDeletion->asRanges.Get(i)->Has(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The declarations and definitions of the symbol szKey that aren't in
// what is taken out already, each added to it.  False, with why in the
// deletion, if one can't be taken out.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::AddDeclarations(SIDESafeDeletion* psDeletion, const char* szKey)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	SIDEFileRange		sRange;
	uint8				uiKind;
	size				i;
	bool				bStatement;
	bool				bForward;
	bool				bResult;
	const char*			szWhyNot;

	asLocations.Init();
	bResult = true;
	mpcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind);
	for (i = 0; bResult && (i < asLocations.NumElements()); i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole == CPPIR_Reference) || IsInRanges(psDeletion, psLocation))
		{
			continue;
		}
		if (mpcContent->GetCodeModel()->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE)
		{
			psDeletion->szWhyNot.Set("it is declared in a file that isn't in the solution");
			bResult = false;
		}
		else if (psLocation->uiFlags & CPPIF_MACRO)
		{
			psDeletion->szWhyNot.Set("its name is made by a macro");
			bResult = false;
		}
		else if (!FindDeclarationRange(psLocation, &sRange, &bStatement, &bForward, &szWhyNot))
		{
			psDeletion->szWhyNot.Set(szWhyNot);
			bResult = false;
		}
		else
		{
			*psDeletion->asRanges.Add() = sRange;
			psDeletion->aiStatements.Add(bStatement ? 1 : 0);
		}
	}
	KillIDELocations(&asLocations);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What is taken out with the deletion's symbol, until nothing more is
// found: its declarations and definitions, then those of every symbol
// declared in them (a class's members have their definitions elsewhere).
// A symbol only named in what is taken out, by a declaration of its name
// alone or as a friend ("class CBar;" in a function, "friend class CBar;"
// in a class), isn't declared there and stays.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::Plan(SIDESafeDeletion* psDeletion)
{
	CArrayChars			aszFound;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	SIDEFileRange		sRange;
	size				uiDone;
	size				i;
	size				j;
	uint8				uiKind;
	bool				bStatement;
	bool				bForward;
	bool				bOwned;
	const char*			szWhyNot;

	psDeletion->aszKeys.Add(psDeletion->szKey.Text());
	uiDone = 0;
	for (;;)
	{
		for (; uiDone < psDeletion->aszKeys.NumElements(); uiDone++)
		{
			if (!AddDeclarations(psDeletion, psDeletion->aszKeys.GetText(uiDone)))
			{
				return false;
			}
		}

		//What is declared in what is taken out goes with it.
		aszFound.Init();
		for (i = 0; i < psDeletion->aszKeys.NumElements(); i++)
		{
			aszFound.Add(psDeletion->aszKeys.GetText(i));
		}
		mpcContent->GetCodeModel()->FindSymbolsDeclaredIn(&psDeletion->asRanges, &aszFound);
		for (i = psDeletion->aszKeys.NumElements(); i < aszFound.NumElements(); i++)
		{
			asLocations.Init();
			mpcContent->GetCodeModel()->GetSymbolLocations(aszFound.GetText(i), &asLocations, &uiKind);
			bOwned = false;
			for (j = 0; (j < asLocations.NumElements()) && !bOwned; j++)
			{
				psLocation = asLocations.Get(j);
				if ((psLocation->uiRole != CPPIR_Reference) && IsInRanges(psDeletion, psLocation))
				{
					//A local's or a parameter's declaration is its only one; anything else is looked at.
					if (aszFound.GetText(i)[0] == '@')
					{
						bOwned = true;
					}
					else if (FindDeclarationRange(psLocation, &sRange, &bStatement, &bForward, &szWhyNot))
					{
						bOwned = !bForward;
						sRange.Kill();
					}
					else
					{
						bOwned = true;
					}
				}
			}
			KillIDELocations(&asLocations);
			if (bOwned)
			{
				psDeletion->aszKeys.Add(aszFound.GetText(i));
			}
		}
		aszFound.Kill();
		if (uiDone == psDeletion->aszKeys.NumElements())
		{
			return true;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What would be left naming what is taken out: the references outside
// it, the functions in derived classes that override the symbol, and its
// labels in hand written assembly.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::FindUsages(SIDESafeDeletion* psDeletion)
{
	CIDECodeModel*			pcModel;
	CArrayIDELocations		asLocations;
	SIDELocation*			psLocation;
	SIDERenameTarget		sAssembly;
	SIDEAssemblyRename		sRename;
	CArrayIDEAssemblyEdits	asEdits;
	CTextFile				cFile;
	const char*				szText;
	size					uiLength;
	uint8					uiKind;
	size					i;
	size					j;

	pcModel = mpcContent->GetCodeModel();
	for (i = 0; i < psDeletion->aszKeys.NumElements(); i++)
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(psDeletion->aszKeys.GetText(i), &asLocations, &uiKind);
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			psLocation = asLocations.Get(j);
			if (!IsInRanges(psDeletion, psLocation))
			{
				MoveIDELocation(psDeletion->asUsages.Add(), psLocation);
			}
		}
		KillIDELocations(&asLocations);
	}

	asLocations.Init();
	psDeletion->uiOverriders = pcModel->FindOverriders(psDeletion->szKey.Text(), &asLocations);
	for (j = 0; j < asLocations.NumElements(); j++)
	{
		psLocation = asLocations.Get(j);
		if (!IsInRanges(psDeletion, psLocation))
		{
			MoveIDELocation(psDeletion->asUsages.Add(), psLocation);
		}
	}
	KillIDELocations(&asLocations);

	//Its labels in the solution's assembly: what a rename of it would change there.
	sAssembly.Init();
	if (pcModel->FindAssemblyTarget(psDeletion->szKey.Text(), &sAssembly) && (sAssembly.uiAssembly != IAR_None))
	{
		sRename.Init((EIDEAssemblyRename)sAssembly.uiAssembly, sAssembly.szName.Text(), sAssembly.szName.Text(), sAssembly.szScope.Text());
		if (sAssembly.bOverloaded)
		{
			sRename.szKey.Set(sAssembly.szKey);
			for (i = 0; i < sAssembly.aszOverloads.NumElements(); i++)
			{
				sRename.aszOverloads.Add(sAssembly.aszOverloads.GetText(i));
			}
		}
		for (i = 0; i < pcModel->NumProjectFiles(); i++)
		{
			if (!IsIDEAssemblyFile(pcModel->GetProjectFile(i)))
			{
				continue;
			}
			cFile.Init();
			asEdits.Init();
			if (mpcContent->GetFindInFiles()->GetFileText(pcModel->GetProjectFile(i), &cFile, &szText, &uiLength))
			{
				FindIDEAssemblyEdits(szText, uiLength, &sRename, false, &asEdits);
			}
			for (j = 0; j < asEdits.NumElements(); j++)
			{
				if (!asEdits.Get(j)->bLength)
				{
					psDeletion->uiAssembly++;
				}
			}
			KillIDEAssemblyEdits(&asEdits);
			cFile.Kill();
		}
		psDeletion->uiAssembly += sRename.uiUnsure;
		sRename.Kill();
	}
	sAssembly.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's searches, counted: the symbol's name as a word in the
// solution's files' comments and strings (mbComments), and in their text
// files and in code the index doesn't have a name at (mbText).  What is
// in what is taken out isn't counted.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::SearchFiles(SIDESafeDeletion* psDeletion)
{
	CIDECodeModel*				pcModel;
	CArrayIDENameOccurrences	asOccurrences;
	SIDENameOccurrence*			psOccurrence;
	SIDELineCounter				sLines;
	SIDELocation				sLocation;
	CTextFile					cFile;
	CChars						szName;
	const char*					szFileName;
	const char*					szText;
	size						uiLength;
	EIDEFileType				eType;
	uint8						uiRole;
	size						i;
	size						j;

	if (!mbComments && !mbText)
	{
		return;
	}
	pcModel = mpcContent->GetCodeModel();
	szName.Init();
	GetIDESymbolName(psDeletion->szKey.Text(), &szName);
	szText = NULL;
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFileName = pcModel->GetProjectFile(i);
		eType = CIDEIcons::GetFileType(szFileName);
		cFile.Init();
		asOccurrences.Init();
		sLocation.szFileName.Init(szFileName);
		if (((eType == IFT_CPP) || (eType == IFT_C) || (eType == IFT_Header)) && mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength))
		{
			sLines.Init(szText);
			FindIDECppOccurrences(szText, uiLength, szName.Text(), mbText, mbComments, &sLines, &asOccurrences);
		}
		else if (mbText && IsIDERenameTextFile(szFileName) && mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength))
		{
			sLines.Init(szText);
			FindIDEWords(szText, 0, uiLength, szName.Text(), szName.Length(), false, &sLines, &asOccurrences);
		}
		for (j = 0; j < asOccurrences.NumElements(); j++)
		{
			psOccurrence = asOccurrences.Get(j);
			sLocation.iLine = psOccurrence->iLine;
			sLocation.iColumn = psOccurrence->iColumn;
			if (IsInRanges(psDeletion, &sLocation))
			{
				continue;
			}
			if (psOccurrence->bComment)
			{
				psDeletion->uiComments++;
			}
			else if (!pcModel->FindRoleAt(szFileName, psOccurrence->iLine, psOccurrence->iColumn, &uiRole))
			{
				//A name the index has is this symbol's, counted already, or another's.
				psDeletion->uiText++;
			}
		}
		sLocation.szFileName.Kill();
		asOccurrences.Kill();
		cFile.Kill();
	}
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Safe Delete would do for the symbol at the caret: what is taken
// out, whether it can be, and what would be left using it.  False if no
// symbol is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::FindDeletion(SIDESafeDeletion* psDest)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}

	//The index is brought up to date with the editors first.
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) ||
		!mpcContent->GetCodeModel()->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &psDest->szKey, &psDest->uiKind))
	{
		return false;
	}
	if (psDest->szKey.Text()[0] == '@')
	{
		GetIDESymbolName(psDest->szKey.Text(), &psDest->szName);
	}
	else
	{
		GetIDEScopedName(psDest->szKey.Text(), &psDest->szName);
	}

	if (psDest->uiKind == CPPSK_BuiltinType)
	{
		psDest->szWhyNot.Set("it is the compiler's");
	}
	else
	{
		psDest->bDeletable = Plan(psDest);
	}
	if (psDest->bDeletable)
	{
		FindUsages(psDest);
		SearchFiles(psDest);
	}
	KillUnits();
	FindEmptiedFiles(psDest);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What a deletion found, for checking: each range taken out as
// "File.h 3:1-9:3" (from 1, as the editor shows them, with the lines and
// comments that go with it), then the usages.
//
//////////////////////////////////////////////////////////////////////////
char* CIDESafeDelete::PrintDeletion(SIDESafeDeletion* psDeletion, CChars* pszDest)
{
	CFileUtil		cFileUtil;
	CTextFile		cFile;
	CChars			szFile;
	SIDEFileRange*	psRange;
	SIDELocation*	psLocation;
	SIDELineCounter	sLines;
	const char*		szText;
	size			uiLength;
	size			uiStart;
	size			uiEnd;
	size			i;

	pszDest->Append("Safe Delete: ")->Append(psDeletion->szName)->Append(" [")->Append(psDeletion->szKey)->Append("]");
	if (!psDeletion->bDeletable)
	{
		pszDest->Append(" can't be deleted: ")->Append(psDeletion->szWhyNot)->AppendNewLine();
		return pszDest->Text();
	}
	pszDest->Append(", ")->Append((uint32)psDeletion->aszKeys.NumElements())->Append(psDeletion->aszKeys.NumElements() == 1 ? " symbol, " : " symbols, ");
	AppendUsages(psDeletion, pszDest);
	pszDest->AppendNewLine();
	for (i = 0; i < psDeletion->asRanges.NumElements(); i++)
	{
		psRange = psDeletion->asRanges.Get(i);
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(psRange->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
			uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, psDeletion->aiStatements.GetValue(i) != 0);
			szFile.Init();
			cFileUtil.GetFileName(&szFile, psRange->szFileName.Text());
			sLines.Init(szText);
			sLines.MoveTo(uiStart);
			pszDest->Append("  takes out ")->Append(szFile)->Append(' ')->Append(sLines.iLine + 1)->Append(':')->Append((int32)(uiStart - sLines.uiLineStart) + 1);
			sLines.MoveTo(uiEnd);
			pszDest->Append('-')->Append(sLines.iLine + 1)->Append(':')->Append((int32)(uiEnd - sLines.uiLineStart) + 1)->AppendNewLine();
			szFile.Kill();
		}
		cFile.Kill();
	}
	for (i = 0; i < psDeletion->aszFiles.NumElements(); i++)
	{
		pszDest->Append("  leaves nothing in ")->Append(GetIDEFileNamePart(psDeletion->aszFiles.GetText(i)))->AppendNewLine();
	}
	for (i = 0; i < psDeletion->asUsages.NumElements(); i++)
	{
		psLocation = psDeletion->asUsages.Get(i);
		szFile.Init();
		cFileUtil.GetFileName(&szFile, psLocation->szFileName.Text());
		pszDest->Append("  used at ")->Append(szFile)->Append(' ')->Append(psLocation->iLine + 1)->Append(':')->Append(psLocation->iColumn + 1)->AppendNewLine();
		szFile.Kill();
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// "3 usages (1 function overrides it), 2 in assembly, 1 in comments and
// strings, 1 text occurrence", each part only if there are any, or
// "no usages".
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::AppendUsages(SIDESafeDeletion* psDeletion, CChars* pszDest)
{
	const char*	szSeparator;

	if (psDeletion->NumUsages() == 0)
	{
		pszDest->Append("no usages");
		return;
	}
	szSeparator = "";
	if (psDeletion->asUsages.NumElements() > 0)
	{
		pszDest->Append((uint32)psDeletion->asUsages.NumElements())->Append(psDeletion->asUsages.NumElements() == 1 ? " usage" : " usages");
		if (psDeletion->uiOverriders > 0)
		{
			pszDest->Append(" (")->Append((uint32)psDeletion->uiOverriders)->Append(psDeletion->uiOverriders == 1 ? " function overrides it)" : " functions override it)");
		}
		szSeparator = ", ";
	}
	if (psDeletion->uiAssembly > 0)
	{
		pszDest->Append(szSeparator)->Append((uint32)psDeletion->uiAssembly)->Append(" in assembly");
		szSeparator = ", ";
	}
	if (psDeletion->uiComments > 0)
	{
		pszDest->Append(szSeparator)->Append((uint32)psDeletion->uiComments)->Append(" in comments and strings");
		szSeparator = ", ";
	}
	if (psDeletion->uiText > 0)
	{
		pszDest->Append(szSeparator)->Append((uint32)psDeletion->uiText)->Append(psDeletion->uiText == 1 ? " text occurrence" : " text occurrences");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What was done, on the status line, or appended to pszStatus.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::ShowDeleteStatus(CChars* pszMessage, CChars* pszStatus)
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
// The deletion made, whatever uses it: each file's ranges taken out of
// its editor (opened, not shown, if it isn't open) as one edit, and all
// the files' as one undo group.  Nothing is saved.  False if a file
// couldn't be opened; nothing is changed then.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::Delete(SIDESafeDeletion* psDeletion, CChars* pszStatus)
{
	CArrayChars			aszFiles;
	CArrayTextRange		asTextRanges;
	STextRange			sTextRange;
	STextRange*			psTextRange;
	SIDEFileRange*		psRange;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CChars				szMessage;
	const char*			szText;
	size				uiFile;
	size				uiDeleted;
	size				i;
	size				j;
	bool				bOpened;

	//Every file's editor first: nothing is changed unless all can be.
	aszFiles.Init();
	bOpened = true;
	for (i = 0; bOpened && (i < psDeletion->asRanges.NumElements()); i++)
	{
		psRange = psDeletion->asRanges.Get(i);
		for (j = 0; (j < aszFiles.NumElements()) && (StringInsensitiveCompare(aszFiles.GetText(j), psRange->szFileName.Text()) != 0); j++)
		{
		}
		if ((j < aszFiles.NumElements()) || (psDeletion->bDeleteFiles && (psDeletion->aszFiles.GetIndex(psRange->szFileName.Text()) != ARRAY_ELEMENT_NOT_FOUND)))
		{
			continue;
		}
		pTab = mpcContent->GetDocuments()->FindTab(psRange->szFileName.Text());
		if (pTab.IsNull())
		{
			pTab = mpcContent->GetDocuments()->OpenFileTab(psRange->szFileName.Text(), mpcContent->GetCodeModel()->FindProjectOfFile(psRange->szFileName.Text()), false);
		}
		bOpened = pTab.IsNotNull();
		if (bOpened)
		{
			aszFiles.Add(psRange->szFileName.Text());
		}
		else
		{
			szMessage.Init("Safe Delete stopped: ");
			szMessage.Append(psRange->szFileName);
			szMessage.Append(" couldn't be opened.");
			ShowDeleteStatus(&szMessage, pszStatus);
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

			//The file's ranges in order, one inside or against another joined to it.
			asTextRanges.Init();
			for (i = 0; i < psDeletion->asRanges.NumElements(); i++)
			{
				psRange = psDeletion->asRanges.Get(i);
				if (StringInsensitiveCompare(psRange->szFileName.Text(), aszFiles.GetText(uiFile)) != 0)
				{
					continue;
				}
				sTextRange.uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
				sTextRange.uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
				ExtendIDEDeletion(szText, &sTextRange.uiStart, &sTextRange.uiEnd, psDeletion->aiStatements.GetValue(i) != 0);
				for (j = 0; (j < asTextRanges.NumElements()) && (asTextRanges.Get(j)->uiStart < sTextRange.uiStart); j++)
				{
				}
				asTextRanges.InsertAt(&sTextRange, j);
			}
			for (j = 1; j < asTextRanges.NumElements(); )
			{
				psTextRange = asTextRanges.Get(j - 1);
				if (asTextRanges.Get(j)->uiStart <= psTextRange->uiEnd)
				{
					if (asTextRanges.Get(j)->uiEnd > psTextRange->uiEnd)
					{
						psTextRange->uiEnd = asTextRanges.Get(j)->uiEnd;
					}
					asTextRanges.RemoveAt(j);
				}
				else
				{
					j++;
				}
			}
			pEditor->ReplaceRanges(asTextRanges.GetData(), asTextRanges.NumElements(), "");
			asTextRanges.Kill();
		}
		uiDeleted = psDeletion->bDeleteFiles ? DeleteFiles(psDeletion) : 0;
		mpcContent->GetEdit()->EndUndoGroup();

		szMessage.Init("Deleted ");
		szMessage.Append(psDeletion->szName);
		szMessage.Append(": ");
		szMessage.Append((uint32)psDeletion->asRanges.NumElements());
		szMessage.Append(psDeletion->asRanges.NumElements() == 1 ? " declaration in " : " declarations and definitions in ");
		szMessage.Append((uint32)aszFiles.NumElements());
		szMessage.Append(aszFiles.NumElements() == 1 ? " file" : " files");
		if (uiDeleted > 0)
		{
			szMessage.Clear();
			szMessage.Append("Deleted ")->Append(psDeletion->szName)->Append(" and ");
			for (i = 0; i < psDeletion->aszFiles.NumElements(); i++)
			{
				szMessage.Append((i == 0) ? "" : ((i + 1 == psDeletion->aszFiles.NumElements()) ? " and " : ", "))->Append(GetIDEFileNamePart(psDeletion->aszFiles.GetText(i)));
			}
			szMessage.Append(", which had nothing else");
		}
		if (psDeletion->aszKeys.NumElements() > 1)
		{
			szMessage.Append(", with ");
			szMessage.Append((uint32)(psDeletion->aszKeys.NumElements() - 1));
			szMessage.Append(psDeletion->aszKeys.NumElements() == 2 ? " thing declared in it" : " things declared in it");
		}
		szMessage.Append(".");
		if (psDeletion->NumUsages() > 0)
		{
			szMessage.Append("  Left using it: ");
			AppendUsages(psDeletion, &szMessage);
			szMessage.Append(".");
		}
		ShowDeleteStatus(&szMessage, pszStatus);
		szMessage.Kill();
	}
	aszFiles.Kill();
	return bOpened;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file's text has nothing in it once the ranges (in order, not
// overlapping) are taken out: only comments, an include guard (#ifndef,
// a #define of a name alone, #endif), #pragma and #includes.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEFileEmptied(const char* szText, CArrayTextRange* pasRanges)
{
	CChars	szLeft;
	char*	sz;
	size	uiAt;
	size	i;
	bool	bEmpty;
	bool	bComment;

	szLeft.Init();
	uiAt = 0;
	for (i = 0; i < pasRanges->NumElements(); i++)
	{
		szLeft.AppendSubString(szText, uiAt, pasRanges->Get(i)->uiStart);
		szLeft.Append('\n');
		uiAt = pasRanges->Get(i)->uiEnd;
	}
	szLeft.Append(&szText[uiAt]);

	bEmpty = true;
	bComment = false;
	sz = szLeft.Text();
	while (bEmpty && (*sz != '\0'))
	{
		//A line: its start, after white space.
		for (; (*sz == ' ') || (*sz == '\t') || (*sz == '\r') || (*sz == '\n'); sz++)
		{
		}
		if (bComment)
		{
			for (; (*sz != '\0') && !((sz[0] == '*') && (sz[1] == '/')); sz++)
			{
			}
			sz += (*sz != '\0') ? 2 : 0;
			bComment = false;
			continue;
		}
		if (*sz == '\0')
		{
			break;
		}
		if ((sz[0] == '/') && (sz[1] == '*'))
		{
			bComment = true;
			sz += 2;
			continue;
		}
		if ((sz[0] == '/') && (sz[1] == '/'))
		{
		}
		else if (*sz == '#')
		{
			for (sz++; (*sz == ' ') || (*sz == '\t'); sz++)
			{
			}
			if (strncmp(sz, "define", 6) == 0)
			{
				//A guard's: a name and nothing after it.
				for (sz += 6; (*sz == ' ') || (*sz == '\t'); sz++)
				{
				}
				for (; IsIDENameChar(*sz); sz++)
				{
				}
				for (; (*sz == ' ') || (*sz == '\t') || (*sz == '\r'); sz++)
				{
				}
				bEmpty = (*sz == '\n') || (*sz == '\0') || ((sz[0] == '/') && (sz[1] == '/'));
			}
			else
			{
				bEmpty = (strncmp(sz, "include", 7) == 0) || (strncmp(sz, "ifndef", 6) == 0) || (strncmp(sz, "endif", 5) == 0) || (strncmp(sz, "pragma", 6) == 0);
			}
		}
		else
		{
			bEmpty = false;
		}
		for (; (*sz != '\0') && (*sz != '\n'); sz++)
		{
		}
	}
	szLeft.Kill();
	return bEmpty;
}


//////////////////////////////////////////////////////////////////////////
//
// The files a class's deletion leaves with nothing in them
// (psDeletion->aszFiles): its own header and source file, where nothing
// else is declared or defined in them.  They are deleted with it
// (bDeleteFiles) unless who asks says not.
//
//////////////////////////////////////////////////////////////////////////
void CIDESafeDelete::FindEmptiedFiles(SIDESafeDeletion* psDeletion)
{
	CArrayChars		aszFiles;
	CArrayTextRange	asTextRanges;
	STextRange		sTextRange;
	STextRange*		psTextRange;
	SIDEFileRange*	psRange;
	CTextFile		cFile;
	const char*		szText;
	size			uiLength;
	size			uiFile;
	size			i;
	size			j;

	psDeletion->aszFiles.ReInit();
	psDeletion->bDeleteFiles = false;
	if (!psDeletion->bDeletable || ((psDeletion->uiKind != CPPSK_Class) && (psDeletion->uiKind != CPPSK_ClassTemplate)))
	{
		return;
	}
	aszFiles.Init();
	for (i = 0; i < psDeletion->asRanges.NumElements(); i++)
	{
		aszFiles.AddIfUnique(psDeletion->asRanges.Get(i)->szFileName.Text());
	}
	for (uiFile = 0; uiFile < aszFiles.NumElements(); uiFile++)
	{
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(aszFiles.GetText(uiFile), &cFile, &szText, &uiLength))
		{
			//The file's ranges in order, one inside or against another joined to it (as Delete takes them out).
			asTextRanges.Init();
			for (i = 0; i < psDeletion->asRanges.NumElements(); i++)
			{
				psRange = psDeletion->asRanges.Get(i);
				if (StringInsensitiveCompare(psRange->szFileName.Text(), aszFiles.GetText(uiFile)) != 0)
				{
					continue;
				}
				sTextRange.uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
				sTextRange.uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
				ExtendIDEDeletion(szText, &sTextRange.uiStart, &sTextRange.uiEnd, psDeletion->aiStatements.GetValue(i) != 0);
				for (j = 0; (j < asTextRanges.NumElements()) && (asTextRanges.Get(j)->uiStart < sTextRange.uiStart); j++)
				{
				}
				asTextRanges.InsertAt(&sTextRange, j);
			}
			for (j = 1; j < asTextRanges.NumElements(); )
			{
				psTextRange = asTextRanges.Get(j - 1);
				if (asTextRanges.Get(j)->uiStart <= psTextRange->uiEnd)
				{
					if (asTextRanges.Get(j)->uiEnd > psTextRange->uiEnd)
					{
						psTextRange->uiEnd = asTextRanges.Get(j)->uiEnd;
					}
					asTextRanges.RemoveAt(j);
				}
				else
				{
					j++;
				}
			}
			if (IsIDEFileEmptied(szText, &asTextRanges))
			{
				psDeletion->aszFiles.Add(aszFiles.GetText(uiFile));
			}
			asTextRanges.Kill();
		}
		cFile.Kill();
	}
	aszFiles.Kill();
	psDeletion->bDeleteFiles = psDeletion->aszFiles.NumElements() > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// A file of the projects in the directory of szFileName that isn't one
// of those going, of its extension if there is one: what the file is
// made beside again if its deletion is undone.  False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::FindFileBeside(SIDESafeDeletion* psDeletion, const char* szFileName, CChars* pszDest)
{
	CIDECodeModel*	pcModel;
	const char*		szFile;
	const char*		szExtension;
	const char*		szOther;
	size			uiDirectory;
	size			i;
	bool			bSame;

	pcModel = mpcContent->GetCodeModel();
	uiDirectory = (size)(GetIDEFileNamePart(szFileName) - szFileName);
	szExtension = strrchr(szFileName, '.');
	pszDest->Clear();
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFile = pcModel->GetProjectFile(i);
		if (((size)(GetIDEFileNamePart(szFile) - szFile) != uiDirectory) || (MemICmp(szFile, szFileName, uiDirectory) != 0) || (psDeletion->aszFiles.GetIndex(szFile) != ARRAY_ELEMENT_NOT_FOUND) ||
			(StringInsensitiveCompare(szFile, szFileName) == 0))
		{
			continue;
		}
		szOther = strrchr(szFile, '.');
		bSame = szExtension && szOther && (StringInsensitiveCompare(szExtension, szOther) == 0);
		if (pszDest->Empty() || bSame)
		{
			pszDest->Set(szFile);
			if (bSame)
			{
				break;
			}
		}
	}
	return !pszDest->Empty();
}


//////////////////////////////////////////////////////////////////////////
//
// The class's files that have nothing else are deleted outright, with
// their items in the projects and their lines in the CMakeLists.txt, and
// each #include of one of them is taken out of the file that has it.
// The deletions are steps of the undo log, in the group of the
// deletion's edits: undo makes the files again as they were.  What is
// edited is saved and the workspace indexed again, as Rename File does.
// How many were deleted.
//
//////////////////////////////////////////////////////////////////////////
size CIDESafeDelete::DeleteFiles(SIDESafeDeletion* psDeletion)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	CArrayIDEIncludes	asIncludes;
	CArrayTextRange		asTextRanges;
	CArrayChars			aszUsers;
	STextRange			sTextRange;
	Ptr<CIDEEditorTab>	pTab;
	CTextFile			cFile;
	CChars				szContent;
	CChars				szBeside;
	const char*			szFileName;
	const char*			szText;
	size				uiLength;
	size				uiDeleted;
	size				i;
	size				j;
	size				k;

	pcModel = mpcContent->GetCodeModel();
	uiDeleted = 0;
	for (i = 0; i < psDeletion->aszFiles.NumElements(); i++)
	{
		szFileName = psDeletion->aszFiles.GetText(i);

		//Its #includes, out of the files that stay.
		asLocations.Init();
		aszUsers.Init();
		pcModel->FindIncludesOf(szFileName, &mpcContent->GetFindInFiles(), &asLocations);
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			if (psDeletion->aszFiles.GetIndex(asLocations.Get(j)->szFileName.Text()) == ARRAY_ELEMENT_NOT_FOUND)
			{
				aszUsers.AddIfUnique(asLocations.Get(j)->szFileName.Text());
			}
		}
		KillIDELocations(&asLocations);
		for (j = 0; j < aszUsers.NumElements(); j++)
		{
			pTab = mpcContent->GetDocuments()->FindTab(aszUsers.GetText(j));
			if (pTab.IsNull())
			{
				pTab = mpcContent->GetDocuments()->OpenFileTab(aszUsers.GetText(j), pcModel->FindProjectOfFile(aszUsers.GetText(j)), false);
			}
			if (pTab.IsNull())
			{
				continue;
			}
			szText = pTab->GetEditor()->GetText();
			asIncludes.Init();
			asTextRanges.Init();
			pcModel->FindIncludesIn(aszUsers.GetText(j), szText, &asIncludes);
			for (k = 0; k < asIncludes.NumElements(); k++)
			{
				if (StringInsensitiveCompare(asIncludes.Get(k)->szFileName.Text(), szFileName) == 0)
				{
					sTextRange.uiStart = asIncludes.Get(k)->uiLineStart;
					sTextRange.uiEnd = asIncludes.Get(k)->uiLineEnd;
					asTextRanges.Add(&sTextRange);
				}
			}
			if (asTextRanges.NumElements() > 0)
			{
				pTab->GetEditor()->ReplaceRanges(asTextRanges.GetData(), asTextRanges.NumElements(), "");
			}
			asTextRanges.Kill();
			KillIDEIncludes(&asIncludes);
		}
		aszUsers.Kill();
		pTab = NULL;

		//The file: its text kept for an undo, its tab closed without asking, and it taken away.
		szContent.Init();
		pTab = mpcContent->GetDocuments()->FindTab(szFileName);
		if (pTab.IsNotNull())
		{
			szContent.Set(pTab->GetEditor()->GetText());
			pTab->ForgetModified();
			pTab = NULL;
			mpcContent->GetDocuments()->CloseFile(szFileName);
		}
		else
		{
			cFile.Init();
			if (mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength))
			{
				szContent.Set(szText);
			}
			cFile.Kill();
		}
		szBeside.Init();
		FindFileBeside(psDeletion, szFileName, &szBeside);
		if (mpcContent->GetMoveClass()->RemoveCreatedFile(szFileName))
		{
			mpcContent->GetEdit()->LogFileDeleted((char*)szFileName, szBeside.Text(), szContent.Text());
			uiDeleted++;
		}
		szBeside.Kill();
		szContent.Kill();
	}
	if (uiDeleted > 0)
	{
		mpcContent->GetDocuments()->SaveAllFiles();
		mpcContent->GetCodeModel()->Reset();
		mpcContent->GetSolutions()->WorkspaceChanged(true);
	}
	return uiDeleted;
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Safe Delete for the symbol at the caret.  A dialog asks for
// the deletion, with the two searches; if nothing uses the symbol it is
// deleted.  If something does, a second dialog says how much, and
// offers the usages (View Usages: Find Usages of it, nothing deleted),
// Delete Anyway, or Cancel.  False if nothing was deleted.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESafeDelete::ShowSafeDelete(void)
{
	SIDESafeDeletion	sDeletion;
	CChars				szMessage;
	CChars				szFiles;
	Ptr<CLabel>			pLabel;
	size				i;
	bool				bResult;
	bool				bDelete;
	bool				bFiles;

	sDeletion.Init();
	if (!FindDeletion(&sDeletion))
	{
		mpcContent->ShowStatus("Nothing to delete at the caret.");
		sDeletion.Kill();
		return false;
	}
	if (!sDeletion.bDeletable)
	{
		szMessage.Init("Can't delete ");
		szMessage.Append(sDeletion.szName);
		szMessage.Append(": ");
		szMessage.Append(sDeletion.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sDeletion.Kill();
		return false;
	}

	szMessage.Init("Delete '");
	szMessage.Append(sDeletion.szName);
	szMessage.Append("'?");
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Safe Delete");
	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetTheme()->GetFont(), szMessage.Text());
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	mpDialog->AddEditor("", pLabel);
	pLabel = NULL;
	mpCommentsBox = mpDialog->AddCheckBox("Search in comments and strings", mbComments);
	mpTextBox = mpDialog->AddCheckBox("Search for text occurrences", mbText);
	mpFilesBox = NULL;
	if (sDeletion.aszFiles.NumElements() > 0)
	{
		//Its files have nothing else: they go with it, unless the tick is taken off.
		szFiles.Init("Delete files (");
		for (i = 0; i < sDeletion.aszFiles.NumElements(); i++)
		{
			szFiles.Append((i > 0) ? ", " : "")->Append(GetIDEFileNamePart(sDeletion.aszFiles.GetText(i)));
		}
		szFiles.Append(")");
		mpFilesBox = mpDialog->AddCheckBox(szFiles.Text(), true);
		szFiles.Kill();
	}
	mpDialog->SetOKCaption("Refactor");
	szMessage.Kill();

	bDelete = false;
	bFiles = true;
	mpcContent->GetScript()->WatchDialog(mpDialog);
	if (mpDialog->Show(480))
	{
		bDelete = true;
		bFiles = mpFilesBox.IsNull() || mpFilesBox->IsTicked();
		if ((mbComments != mpCommentsBox->IsTicked()) || (mbText != mpTextBox->IsTicked()))
		{
			//The searches change what is found.
			mbComments = mpCommentsBox->IsTicked();
			mbText = mpTextBox->IsTicked();
			sDeletion.Kill();
			sDeletion.Init();
			bDelete = FindDeletion(&sDeletion) && sDeletion.bDeletable;
		}
	}
	mpCommentsBox = NULL;
	mpTextBox = NULL;
	mpFilesBox = NULL;
	mpDialog = NULL;
	sDeletion.bDeleteFiles = sDeletion.bDeleteFiles && bFiles;

	if (bDelete && (sDeletion.NumUsages() > 0))
	{
		//As IntelliJ's "Usages Detected".
		szMessage.Init("'");
		szMessage.Append(sDeletion.szName);
		szMessage.Append("' has ");
		AppendUsages(&sDeletion, &szMessage);
		szMessage.Append(".");
		mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Usages Detected");
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetTheme()->GetFont(), szMessage.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpDialog->AddEditor("", pLabel);
		pLabel = NULL;
		mpDialog->AddExtraButton("View Usages");
		mpDialog->SetOKCaption("Delete Anyway");
		szMessage.Kill();
		mpcContent->GetScript()->WatchDialog(mpDialog);
		bDelete = mpDialog->Show(480);
		if (!bDelete && (strcmp(mpDialog->GetClicked(), "View Usages") == 0))
		{
			mpcContent->GetUsages()->FindUsages(false);
		}
		mpDialog = NULL;
	}

	bResult = bDelete && Delete(&sDeletion);
	sDeletion.Kill();
	return bResult;
}


size SIDESafeDeletion::NumUsages(void) const { return asUsages.NumElements() + uiAssembly + uiComments + uiText; }
