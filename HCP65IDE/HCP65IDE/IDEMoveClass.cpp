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
#include "BaseLib/NameMatcher.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "WindowLib/GridContainer.h"
#include "IDEContent.h"
#include "IDETemplateVariables.h"
#include "IDERename.h"
#include "IDEFileRename.h"
#include "IDESafeDelete.h"
#include "IDEMoveMembers.h"
#include "IDEMoveClass.h"
#include "IDEMoveNamespace.h"
#include "IDEContextActions.h"


#define IDE_MOVE_CLASS_END_LINE		0x3fffffff


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEClassMove::Init(void)
{
	szKey.Init();
	szName.Init();
	bMovable = false;
	szWhyNot.Init();
	szHeader.Init();
	memset(&sClass, 0, sizeof(SCPPSyntaxRange));
	asOutside.Init();
	szSource.Init();
	szStem.Init();
	aszUsers.Init();
	bHeaderUses = false;
	bNeedsHeader = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEClassMove::Kill(void)
{
	aszUsers.Kill();
	szStem.Kill();
	szSource.Kill();
	KillIDEFileRanges(&asOutside);
	szHeader.Kill();
	szWhyNot.Kill();
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The start of the line after the one uiAt is on.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDENextLine(const char* szText, size uiAt)
{
	for (; (szText[uiAt] != '\0') && (szText[uiAt] != '\n'); uiAt++)
	{
	}
	return (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// What is taken out of a file, where it is the last thing in it: the
// blank lines before it go too, so the file doesn't end with them.
//
//////////////////////////////////////////////////////////////////////////
static void ExtendIDEDeletionAtEnd(const char* szText, size* puiStart, size uiEnd)
{
	size	uiStart;
	size	uiPrevious;
	size	i;
	bool	bBlank;

	for (i = uiEnd; (szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r') || (szText[i] == '\n'); i++)
	{
	}
	if (szText[i] != '\0')
	{
		return;
	}
	uiStart = *puiStart;
	while (uiStart > 0)
	{
		for (uiPrevious = uiStart - 1; (uiPrevious > 0) && (szText[uiPrevious - 1] != '\n'); uiPrevious--)
		{
		}
		bBlank = true;
		for (i = uiPrevious; i < uiStart; i++)
		{
			bBlank = bBlank && ((szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r') || (szText[i] == '\n'));
		}
		if (!bBlank)
		{
			break;
		}
		uiStart = uiPrevious;
	}
	*puiStart = uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the line at uiLine is an #include, and where the name it
// includes is (between its quotes or angle brackets).
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEIncludeLine(const char* szText, size uiLine, size* puiName, size* puiNameEnd)
{
	size	i;
	char	cClose;

	for (i = uiLine; (szText[i] == ' ') || (szText[i] == '\t'); i++)
	{
	}
	if (szText[i] != '#')
	{
		return false;
	}
	for (i++; (szText[i] == ' ') || (szText[i] == '\t'); i++)
	{
	}
	if (strncmp(&szText[i], "include", 7) != 0)
	{
		return false;
	}
	for (i += 7; (szText[i] == ' ') || (szText[i] == '\t'); i++)
	{
	}
	if ((szText[i] != '"') && (szText[i] != '<'))
	{
		return false;
	}
	cClose = (szText[i] == '"') ? '"' : '>';
	*puiName = i + 1;
	for (i++; (szText[i] != cClose) && (szText[i] != '\n') && (szText[i] != '\0'); i++)
	{
	}
	*puiNameEnd = i;
	return szText[i] == cClose;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpsMove = NULL;
	mpsFunctionMove = NULL;
	mapcUnits.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpNameField);
	M_Pointer(mpHeaderField);
	M_Pointer(mpChooseButton);
	M_Pointer(mpError);
	U_Pointer(mpsMove);
	U_Pointer(mpsFunctionMove);
	U_Unknown(CArrayTemplatePtr<CIDERefactorUnit>, mapcUnits);
	U_Unknown(CArrayChars, maszCreated);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::Free(void)
{
	KillUnits();
	mapcUnits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::Save(CObjectWriter* pcFile) { return false; }
bool CIDEMoveClass::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// F6, IntelliJ's Move.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.MoveClass", "Move...", "F6", Ptr<CImageCel>(), this, &CIDEMoveClass::QueueMove)->SetEnabledCheck(this, &CIDEMoveClass::CanMove);
	pCommands->Add("Refactor.CopyClass", "Copy Class...", "", Ptr<CImageCel>(), this, &CIDEMoveClass::QueueCopy)->SetEnabledCheck(this, &CIDEMoveClass::CanMove);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MoveClass"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.CopyClass"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: a class can be moved and copied, a function
// that is no class's moved.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bClass)
	{
		psContext->Offer("Refactor.MoveClass");
		psContext->Offer("Refactor.CopyClass");
	}
	else if (psContext->bFunction && !psContext->bMember)
	{
		psContext->Offer("Refactor.MoveClass");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::CanMove(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


void CIDEMoveClass::QueueMove(void) { mpcContent->GetWindow()->Post(this, &CIDEMoveClass::ShowQueuedMove); }
void CIDEMoveClass::ShowQueuedMove(void) { ShowMove(); }
void CIDEMoveClass::QueueCopy(void) { mpcContent->GetWindow()->Post(this, &CIDEMoveClass::ShowQueuedCopy); }
void CIDEMoveClass::ShowQueuedCopy(void) { ShowCopy(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDEMoveClass::GetUnit(const char* szFileName)
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
void CIDEMoveClass::KillUnits(void)
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
// A definition outside the class added to the move's, in its file's
// order (the header's first); one already in them (or inside one of
// them) isn't added again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddOutside(SIDEClassMove* psMove, const char* szFileName, SCPPSyntaxRange* psRange)
{
	SIDEFileRange*	psOther;
	SIDEFileRange	sRange;
	size			uiAt;
	bool			bHeader;
	bool			bOtherHeader;
	bool			bSameFile;

	if (IsMoved(psMove, szFileName, psRange->iLine))
	{
		return;
	}
	bHeader = StringInsensitiveCompare(szFileName, psMove->szHeader.Text()) == 0;
	for (uiAt = 0; uiAt < psMove->asOutside.NumElements(); uiAt++)
	{
		psOther = psMove->asOutside.Get(uiAt);
		bOtherHeader = StringInsensitiveCompare(psOther->szFileName.Text(), psMove->szHeader.Text()) == 0;
		bSameFile = StringInsensitiveCompare(psOther->szFileName.Text(), szFileName) == 0;
		if ((bHeader && !bOtherHeader) || (bSameFile && (psOther->iLine > psRange->iLine)))
		{
			break;
		}
	}
	sRange.Init(szFileName, psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn);
	psMove->asOutside.InsertAt(&sRange, uiAt);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a line of a file is in what moves: the class, or one of its
// members' definitions outside it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::IsMoved(SIDEClassMove* psMove, const char* szFileName, int32 iLine)
{
	SIDEFileRange*	psRange;
	size			i;

	if ((StringInsensitiveCompare(szFileName, psMove->szHeader.Text()) == 0) && (iLine >= psMove->sClass.iLine) && (iLine <= psMove->sClass.iEndLine))
	{
		return true;
	}
	for (i = 0; i < psMove->asOutside.NumElements(); i++)
	{
		psRange = psMove->asOutside.Get(i);
		if ((StringInsensitiveCompare(szFileName, psRange->szFileName.Text()) == 0) && (iLine >= psRange->iLine) && (iLine <= psRange->iEndLine))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What Move Class to Its Own Files has to work with, for the class the
// caret is in (on its name or one of its members).  False if there is no
// class there; psDest->bMovable says whether it can be moved, szWhyNot
// why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::FindMove(SIDEClassMove* psDest, bool bCopy)
{
	CIDECodeModel*			pcModel;
	CArrayIDELocations		asLocations;
	CArrayIDEFileRanges		asRanges;
	CArrayChars				aszKeys;
	SIDELocation*			psLocation;
	CIDERefactorUnit*		pcUnit;
	CCPPSyntax*				pcClass;
	CCPPSyntax*				pcOuter;
	CCPPSyntaxDeclarator*	pcDeclarator;
	SIDESignatureSite		sSite;
	SCPPSyntaxRange			sRange;
	CChars					szKey;
	CChars					szScope;
	CChars					szPrefix;
	CChars					szOwnName;
	const char*				szWhyNot;
	int						iLine;
	int						iColumn;
	size					uiOthers;
	size					i;
	size					j;
	uint8					uiRole;
	uint8					uiKind;
	bool					bFound;

	pcModel = mpcContent->GetCodeModel();
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szKey.Init();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) ||
		!pcModel->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &szKey, &uiKind))
	{
		szKey.Kill();
		return false;
	}
	if ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate))
	{
		psDest->szKey.Set(szKey);
	}
	else
	{
		GetIDESymbolScope(szKey.Text(), &psDest->szKey);
	}
	szKey.Kill();

	//Where it is defined: all of its definition, from its template<...> to its ;.
	asLocations.Init();
	bFound = false;
	if (!psDest->szKey.Empty() && pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
	{
		for (i = 0; !bFound && (i < asLocations.NumElements()); i++)
		{
			psLocation = asLocations.Get(i);
			if ((psLocation->uiRole != CPPIR_Definition) || (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE))
			{
				continue;
			}
			pcUnit = GetUnit(psLocation->szFileName.Text());
			pcClass = pcUnit ? pcUnit->FindAncestor(pcUnit->FindNodeAt(psLocation->iLine, psLocation->iColumn), CPPS_ClassSpecifier) : NULL;
			pcOuter = pcClass ? pcUnit->FindAncestor(pcClass, CPPS_SimpleDeclaration) : NULL;
			pcOuter = pcOuter ? pcOuter : pcClass;
			while (pcOuter && pcOuter->mpcParent && (pcOuter->mpcParent->GetKind() == CPPS_TemplateDeclaration))
			{
				pcOuter = pcOuter->mpcParent;
			}
			if (pcOuter && pcUnit->GetRange(pcOuter, &psDest->sClass))
			{
				psDest->szHeader.Set(psLocation->szFileName);
				bFound = true;
			}
		}
	}
	KillIDELocations(&asLocations);
	if (!bFound)
	{
		KillUnits();
		return false;
	}
	GetIDEScopedName(psDest->szKey.Text(), &psDest->szName);
	szOwnName.Init();
	GetIDESymbolName(psDest->szKey.Text(), &szOwnName);
	psDest->szStem.Set(&szOwnName.Text()[((szOwnName.Text()[0] == 'C') && (szOwnName.Text()[1] >= 'A') && (szOwnName.Text()[1] <= 'Z')) ? 1 : 0]);
	szOwnName.Kill();
	psDest->bMovable = true;

	//A class declared in another moves with it.
	szScope.Init();
	GetIDESymbolScope(psDest->szKey.Text(), &szScope);
	asLocations.Init();
	if (!szScope.Empty() && pcModel->GetSymbolLocations(szScope.Text(), &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
	{
		psDest->bMovable = false;
		psDest->szWhyNot.Set("it is declared in ");
		psDest->szWhyNot.Append(szScope);
	}
	KillIDELocations(&asLocations);
	szScope.Kill();

	//Its members' definitions outside it.
	szPrefix.Init(psDest->szKey);
	szPrefix.Append("::");
	asRanges.Init();
	asRanges.Add()->Init(psDest->szHeader.Text(), psDest->sClass.iLine, psDest->sClass.iColumn, psDest->sClass.iEndLine, psDest->sClass.iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	KillIDEFileRanges(&asRanges);
	for (i = 0; psDest->bMovable && (i < aszKeys.NumElements()); i++)
	{
		szScope.Init();
		GetIDESymbolScope(aszKeys.GetText(i), &szScope);
		bFound = aszKeys.Get(i)->StartsWith(szPrefix.Text()) && !szScope.Contains("(");
		szScope.Kill();
		asLocations.Init();
		if (bFound && pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind) &&
			((uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate) || (uiKind == CPPSK_Variable) || (uiKind == CPPSK_Field)))
		{
			for (j = 0; j < asLocations.NumElements(); j++)
			{
				psLocation = asLocations.Get(j);
				if ((psLocation->uiRole != CPPIR_Definition) || IsMoved(psDest, psLocation->szFileName.Text(), psLocation->iLine))
				{
					continue;
				}
				pcUnit = GetUnit(psLocation->szFileName.Text());
				bFound = false;
				if (pcUnit && ((uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate)))
				{
					sSite.Init();
					bFound = mpcContent->GetChangeSignature()->ReadSite(pcUnit, psLocation, "", &sSite, &szWhyNot);
					sRange = sSite.sAll;
					sSite.Kill();
				}
				else if (pcUnit)
				{
					pcDeclarator = (CCPPSyntaxDeclarator*)pcUnit->FindAncestor(pcUnit->FindNodeAt(psLocation->iLine, psLocation->iColumn), CPPS_Declarator);
					while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
					{
						pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
					}
					pcOuter = pcDeclarator ? pcDeclarator->mpcParent : NULL;
					if (pcOuter && (pcOuter->GetKind() == CPPS_InitDeclarator))
					{
						pcOuter = pcOuter->mpcParent;
					}
					bFound = pcOuter && pcUnit->GetRange(pcOuter, &sRange);
				}
				if (!bFound)
				{
					psDest->bMovable = false;
					psDest->szWhyNot.Set("a definition of ");
					GetIDEScopedName(aszKeys.GetText(i), &psDest->szWhyNot);
					psDest->szWhyNot.Append(" can't be read (a macro makes it)");
					break;
				}
				AddOutside(psDest, psLocation->szFileName.Text(), &sRange);
			}
		}
		KillIDELocations(&asLocations);
	}
	aszKeys.Kill();

	//One source file has them, if any has.
	for (i = 0; psDest->bMovable && (i < psDest->asOutside.NumElements()); i++)
	{
		if (StringInsensitiveCompare(psDest->asOutside.Get(i)->szFileName.Text(), psDest->szHeader.Text()) == 0)
		{
			continue;
		}
		if (psDest->szSource.Empty())
		{
			psDest->szSource.Set(psDest->asOutside.Get(i)->szFileName);
		}
		else if (StringInsensitiveCompare(psDest->asOutside.Get(i)->szFileName.Text(), psDest->szSource.Text()) != 0)
		{
			psDest->bMovable = false;
			psDest->szWhyNot.Set("its members are defined in more than one file (");
			psDest->szWhyNot.Append(GetIDEFileNamePart(psDest->szSource.Text()))->Append(", ")->Append(GetIDEFileNamePart(psDest->asOutside.Get(i)->szFileName.Text()))->Append(")");
		}
	}

	//It shares its header with something (a copy needn't).
	if (psDest->bMovable && !bCopy)
	{
		asRanges.Init();
		asRanges.Add()->Init(psDest->szHeader.Text(), 0, 0, IDE_MOVE_CLASS_END_LINE, 0);
		aszKeys.Init();
		pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
		//Another class, enum, typedef, function or variable (not what is in one: a parameter, an enumerator).
		uiOthers = 0;
		for (i = 0; i < aszKeys.NumElements(); i++)
		{
			if (aszKeys.Get(i)->Equals(psDest->szKey.Text()) || aszKeys.Get(i)->StartsWith(szPrefix.Text()) || (aszKeys.GetText(i)[0] == '@'))
			{
				continue;
			}
			szScope.Init();
			GetIDESymbolScope(aszKeys.GetText(i), &szScope);
			asLocations.Init();
			if (pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind) && !szScope.Contains("(") &&
				((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate) || (uiKind == CPPSK_Enum) || (uiKind == CPPSK_Typedef) || (uiKind == CPPSK_Function) ||
				 (uiKind == CPPSK_FunctionTemplate) || (uiKind == CPPSK_Variable)))
			{
				uiOthers++;
			}
			KillIDELocations(&asLocations);
			szScope.Kill();
		}
		aszKeys.Kill();
		KillIDEFileRanges(&asRanges);
		if (uiOthers == 0)
		{
			psDest->bMovable = false;
			psDest->szWhyNot.Set("it is the only thing ");
			psDest->szWhyNot.Append(GetIDEFileNamePart(psDest->szHeader.Text()));
			psDest->szWhyNot.Append(" declares: Rename File renames its files");
		}
	}
	szPrefix.Kill();
	if (psDest->bMovable)
	{
		FindUsers(psDest);
		if (psDest->bHeaderUses && psDest->bNeedsHeader && !bCopy)
		{
			psDest->bMovable = false;
			psDest->szWhyNot.Set("it and what stays in ");
			psDest->szWhyNot.Append(GetIDEFileNamePart(psDest->szHeader.Text()));
			psDest->szWhyNot.Append(" each name the other: their headers would each have to #include the other");
		}
	}
	KillUnits();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Who names the class or a member of it, outside what moves (the files,
// and whether what stays in its header does), and whether what moves
// names anything that stays in its header.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::FindUsers(SIDEClassMove* psMove)
{
	CIDECodeModel*		pcModel;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	CArrayChars			aszKeys;
	SIDEFileRange*		psRange;
	CChars				szPrefix;
	size				i;
	size				j;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	szPrefix.Init(psMove->szKey);
	szPrefix.Append("::");
	asRanges.Init();
	asRanges.Add()->Init(psMove->szHeader.Text(), psMove->sClass.iLine, psMove->sClass.iColumn, psMove->sClass.iEndLine, psMove->sClass.iEndColumn);
	aszKeys.Init();
	aszKeys.Add(psMove->szKey.Text());
	pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		if (!aszKeys.Get(i)->Equals(psMove->szKey.Text()) && !aszKeys.Get(i)->StartsWith(szPrefix.Text()))
		{
			continue;
		}
		asLocations.Init();
		pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind);
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			if (IsMoved(psMove, asLocations.Get(j)->szFileName.Text(), asLocations.Get(j)->iLine) || (pcModel->FindProjectOfFile(asLocations.Get(j)->szFileName.Text()) == MAX_SIZE))
			{
				continue;
			}
			if (StringInsensitiveCompare(asLocations.Get(j)->szFileName.Text(), psMove->szHeader.Text()) == 0)
			{
				psMove->bHeaderUses = true;
			}
			else
			{
				psMove->aszUsers.AddIfUnique(asLocations.Get(j)->szFileName.Text());
			}
		}
		KillIDELocations(&asLocations);
	}
	aszKeys.Kill();

	//What moves names: is any of it declared in what stays in the header?
	for (i = 0; i < psMove->asOutside.NumElements(); i++)
	{
		psRange = psMove->asOutside.Get(i);
		asRanges.Add()->Init(psRange->szFileName.Text(), psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn);
	}
	aszKeys.Init();
	pcModel->FindSymbolsUsedIn(&asRanges, &aszKeys);
	for (i = 0; !psMove->bNeedsHeader && (i < aszKeys.NumElements()); i++)
	{
		if (aszKeys.Get(i)->Equals(psMove->szKey.Text()) || aszKeys.Get(i)->StartsWith(szPrefix.Text()))
		{
			continue;
		}
		asLocations.Init();
		pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind);
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			if ((asLocations.Get(j)->uiRole != CPPIR_Reference) && (StringInsensitiveCompare(asLocations.Get(j)->szFileName.Text(), psMove->szHeader.Text()) == 0) &&
				!IsMoved(psMove, asLocations.Get(j)->szFileName.Text(), asLocations.Get(j)->iLine))
			{
				psMove->bNeedsHeader = true;
			}
		}
		KillIDELocations(&asLocations);
	}
	aszKeys.Kill();
	KillIDEFileRanges(&asRanges);
	szPrefix.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file of a stem and an extension in the directory of another file.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AppendNewPath(const char* szBeside, const char* szStem, const char* szExtension, CChars* pszDest)
{
	pszDest->AppendSubString(szBeside, 0, (size)(GetIDEFileNamePart(szBeside) - szBeside));
	pszDest->Append(szStem);
	pszDest->Append(szExtension);
}


//////////////////////////////////////////////////////////////////////////
//
// Why the class's files can't have a stem: it isn't a file's name, or a
// file they would be is there already.  False if they can.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::FindProblem(SIDEClassMove* psMove, const char* szStem, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szPath;
	size		i;
	bool		bProblem;

	bProblem = (szStem[0] == '\0');
	for (i = 0; szStem[i] != '\0'; i++)
	{
		bProblem = bProblem || !(IsIDENameChar(szStem[i]) || (szStem[i] == '-'));
	}
	if (bProblem)
	{
		pszDest->Set("the files' name is letters, digits, _ and -, without an extension");
		return true;
	}
	szPath.Init();
	AppendNewPath(psMove->szHeader.Text(), szStem, ".h", &szPath);
	bProblem = cFileUtil.Exists(szPath.Text());
	if (!bProblem && !psMove->szSource.Empty())
	{
		szPath.Clear();
		AppendNewPath(psMove->szSource.Text(), szStem, ".cpp", &szPath);
		bProblem = cFileUtil.Exists(szPath.Text());
	}
	if (bProblem)
	{
		pszDest->Set(GetIDEFileNamePart(szPath.Text()));
		pszDest->Append(" is there already");
	}
	szPath.Kill();
	return bProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a file's code starts: after the comments and blank lines it
// opens with (its licence).
//
//////////////////////////////////////////////////////////////////////////
size CIDEMoveClass::FindOpeningEnd(const char* szText)
{
	size		uiAt;
	size		i;
	const char*	szEnd;

	uiAt = 0;
	for (;;)
	{
		for (i = uiAt; (szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r') || (szText[i] == '\n') || ((uiAt == 0) && ((uint8)szText[i] >= 0xef)); i++)
		{
		}
		if ((szText[i] == '/') && (szText[i + 1] == '/'))
		{
			uiAt = FindIDENextLine(szText, i);
		}
		else if ((szText[i] == '/') && (szText[i + 1] == '*'))
		{
			szEnd = strstr(&szText[i + 2], "*/");
			if (!szEnd)
			{
				return uiAt;
			}
			uiAt = FindIDENextLine(szText, (size)(szEnd - szText));
		}
		else
		{
			//The blank lines after the last comment are the code's.
			return uiAt;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What is written at a range of a file, as it goes to another: from the
// comment over it to the end of its last line (its ; too).
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AppendBlock(const char* szText, int32 iLine, int32 iColumn, int32 iEndLine, int32 iEndColumn, CChars* pszDest)
{
	size	uiStart;
	size	uiEnd;

	uiStart = FindIDECommentStart(szText, FindIDETextOffset(szText, iLine, iColumn));
	uiEnd = FindIDENextLine(szText, FindIDETextOffset(szText, iEndLine, iEndColumn));
	pszDest->AppendSubString(szText, uiStart, uiEnd);
	if (!pszDest->EndsWith("\n"))
	{
		pszDest->Append('\n');
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A file's #include lines, as it writes them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AppendIncludes(const char* szText, CChars* pszDest)
{
	size	uiLine;
	size	uiNext;
	size	uiName;
	size	uiNameEnd;

	for (uiLine = 0; szText[uiLine] != '\0'; uiLine = uiNext)
	{
		uiNext = FindIDENextLine(szText, uiLine);
		if (IsIDEIncludeLine(szText, uiLine, &uiName, &uiNameEnd))
		{
			pszDest->AppendSubString(szText, uiLine, uiNext);
			if (!pszDest->EndsWith("\n"))
			{
				pszDest->Append('\n');
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A new header's or source file's text from a Files template (C++ Header,
// C++ Source): szIncludes its #includes (each line ended) and szBody
// what goes in it, with the file's variables (SetIDETemplateFileVariables;
// szProjectOf is a file of the project it is added to).  False, with the
// template's error (GetFileTemplates()->GetError()), if it has one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::AppendNewFileText(const char* szTemplate, const char* szPath, const char* szProjectOf, const char* szClassName, const char* szIncludes, const char* szBody, CChars* pszDest)
{
	CTemplateValues	cValues;
	bool			bResult;

	cValues.Init();
	SetIDETemplateFileVariables(mpcContent, szPath, szProjectOf, szClassName, &cValues);
	cValues.Set("INCLUDES", szIncludes);
	cValues.Set("BODY", szBody);
	bResult = mpcContent->GetFileTemplates()->Apply(ITT_Files, szTemplate, &cValues, pszDest);
	cValues.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a file gets its #include of the new header (szNewName), and the
// line: after its #include of the file at szIncluded, written as that
// one is with the new name in place of the old (true); otherwise after
// its last #include, or its guard's #define, or the comment it opens with
// (false).  The line is empty if the file #includes the new header
// already.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::FindIncludePlace(const char* szText, const char* szIncluded, const char* szNewName, size* puiAt, CChars* pszLine)
{
	const char*	szOldName;
	size		uiLine;
	size		uiNext;
	size		uiName;
	size		uiNameEnd;
	size		uiFile;
	size		uiLast;
	size		uiOpening;
	size		uiOldLength;
	bool		bFound;

	szOldName = GetIDEFileNamePart(szIncluded);
	uiOldLength = strlen(szOldName);
	uiLast = MAX_SIZE;
	bFound = false;
	for (uiLine = 0; szText[uiLine] != '\0'; uiLine = uiNext)
	{
		uiNext = FindIDENextLine(szText, uiLine);
		if (!IsIDEIncludeLine(szText, uiLine, &uiName, &uiNameEnd))
		{
			continue;
		}
		for (uiFile = uiNameEnd; (uiFile > uiName) && (szText[uiFile - 1] != '/') && (szText[uiFile - 1] != '\\'); uiFile--)
		{
		}
		if ((uiNameEnd - uiFile == strlen(szNewName)) && (MemICmp(szNewName, &szText[uiFile], strlen(szNewName)) == 0))
		{
			//It has it.
			*puiAt = uiNext;
			pszLine->Clear();
			return true;
		}
		if (!bFound && (uiOldLength > 0) && (uiNameEnd - uiFile == uiOldLength) && (MemICmp(szOldName, &szText[uiFile], uiOldLength) == 0))
		{
			bFound = true;
			*puiAt = uiNext;
			pszLine->Clear();
			pszLine->AppendSubString(szText, uiLine, uiFile);
			pszLine->Append(szNewName);
			pszLine->AppendSubString(szText, uiNameEnd, uiNext);
			if (!pszLine->EndsWith("\n"))
			{
				pszLine->Append('\n');
			}
		}
		uiLast = uiNext;
	}
	if (bFound)
	{
		return true;
	}

	pszLine->Set("#include \"");
	pszLine->Append(szNewName);
	pszLine->Append("\"\n");
	if (uiLast != MAX_SIZE)
	{
		*puiAt = uiLast;
		return false;
	}
	uiOpening = FindOpeningEnd(szText);
	for (uiLine = uiOpening; (szText[uiLine] == ' ') || (szText[uiLine] == '\t') || (szText[uiLine] == '\r') || (szText[uiLine] == '\n'); uiLine++)
	{
	}
	if (strncmp(&szText[uiLine], "#ifndef", 7) == 0)
	{
		uiNext = FindIDENextLine(szText, uiLine);
		if (strncmp(&szText[uiNext], "#define", 7) == 0)
		{
			*puiAt = FindIDENextLine(szText, uiNext);
			return false;
		}
	}
	*puiAt = uiOpening;
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A new file made, empty, and made an item of every project the file at
// szBeside is an item of, beside it, and a line of the CMakeLists.txt
// that list that one.  Its making is a step of the undo log, in the
// group of the edits that write it (BeginCreating): undo takes the file
// away again.  False if it can't be made.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::CreateFile(const char* szPath, const char* szBeside)
{
	CFileUtil			cFileUtil;
	CBuildWorkspace*	pcWorkspace;
	CVcxProject*		pcProject;
	size				i;

	if (cFileUtil.Exists(szPath) || !cFileUtil.Touch(szPath))
	{
		return false;
	}
	pcWorkspace = mpcContent->GetWorkspace();
	for (i = 0; i < pcWorkspace->NumProjects(); i++)
	{
		pcProject = pcWorkspace->GetSolutionProject(i)->GetProject();
		if (pcProject)
		{
			pcProject->AddItemBeside(szBeside, GetIDEFileNamePart(szPath));
		}
	}
	AddToListFiles(szBeside, GetIDEFileNamePart(szPath));
	mpcContent->GetEdit()->LogFileCreated((char*)szPath, (char*)szBeside);
	maszCreated.Add(szPath);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Files are about to be made and written (CreateFile, then the edits
// that fill them): all of it is one undo, from here to EndCreating,
// which is told whether they were written and saved, and then stages
// them where their directory is a git repository's (as Rename File
// stages its renames).
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::BeginCreating(void)
{
	maszCreated.ReInit();
	mpcContent->GetEdit()->BeginUndoGroup();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::EndCreating(bool bWritten)
{
	CGitRepository*	pcRepository;
	size			i;

	for (i = 0; bWritten && (i < maszCreated.NumElements()); i++)
	{
		pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(maszCreated.GetText(i));
		if (pcRepository)
		{
			pcRepository->Add(maszCreated.GetText(i));
		}
	}
	maszCreated.ReInit();
	mpcContent->GetEdit()->EndUndoGroup();
}


//////////////////////////////////////////////////////////////////////////
//
// A file a refactoring made taken away again, for the undo of its
// making: out of git's index if it was staged, off the disk, out of
// every project that has it and out of the CMakeLists.txt that list it.
// False if it couldn't be deleted, when nothing else changes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::RemoveCreatedFile(const char* szPath)
{
	CFileUtil			cFileUtil;
	CGitRepository*		pcRepository;
	CBuildWorkspace*	pcWorkspace;
	CVcxProject*		pcProject;
	size				i;

	pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(szPath);
	if (pcRepository && pcRepository->IsTracked(szPath))
	{
		pcRepository->Unstage(szPath);
	}
	if (cFileUtil.Exists(szPath) && !cFileUtil.Delete(szPath))
	{
		return false;
	}
	pcWorkspace = mpcContent->GetWorkspace();
	for (i = 0; i < pcWorkspace->NumProjects(); i++)
	{
		pcProject = pcWorkspace->GetSolutionProject(i)->GetProject();
		if (pcProject)
		{
			pcProject->RemoveItem(szPath);
		}
	}
	AddToListFiles(szPath, NULL);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A file that was taken away made again, for the redo of its making:
// with the text it had, in the projects and the CMakeLists.txt beside
// the file it was first made beside, and staged.  Nothing is logged.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::RecreateFile(const char* szPath, const char* szBeside, const char* szContent)
{
	CFileUtil			cFileUtil;
	CGitRepository*		pcRepository;
	CTextFile			cFile;
	bool				bWritten;

	if (cFileUtil.Exists(szPath))
	{
		return false;
	}
	cFile.Init();
	cFile.mcText.Append(szContent);
	bWritten = cFile.Write(szPath);
	cFile.Kill();
	if (!bWritten)
	{
		return false;
	}
	PlaceFile(szPath, szBeside);
	pcRepository = mpcContent->GetGitStatus()->GetGit()->FindRepository(szPath);
	if (pcRepository)
	{
		pcRepository->Add(szPath);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A file made in its project's place (New's: nothing beside it to copy):
// in pcProject, in szFilter, and in the CMakeLists.txt lines that name
// szListBeside (if it isn't empty), staged when EndCreating says.  What
// the undo log keeps for its redo is the place itself, "<project>\n
// <filter>\n<list beside>".  False if it is there already or can't be
// made or added.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::CreateNewFile(const char* szPath, CVcxProject* pcProject, const char* szFilter, const char* szListBeside)
{
	CFileUtil	cFileUtil;
	CChars		szPlace;

	if (!pcProject || cFileUtil.Exists(szPath) || !cFileUtil.Touch(szPath))
	{
		return false;
	}
	if (!pcProject->AddItem(szPath, szFilter))
	{
		cFileUtil.Delete(szPath);
		return false;
	}
	if (szListBeside && (szListBeside[0] != '\0'))
	{
		AddToListFiles(szListBeside, GetIDEFileNamePart(szPath));
	}
	szPlace.Init(pcProject->GetFileName());
	szPlace.Append('\n')->Append(szFilter ? szFilter : "")->Append('\n')->Append(szListBeside ? szListBeside : "");
	mpcContent->GetEdit()->LogFileCreated((char*)szPath, szPlace.Text());
	szPlace.Kill();
	maszCreated.Add(szPath);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A file made again put back where it was: beside the file it was made
// beside (szBeside, a path), in that one's projects and CMakeLists.txt;
// or, for one New made (szBeside "<project>\n<filter>\n<list beside>"),
// in that project and filter.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::PlaceFile(const char* szPath, const char* szBeside)
{
	CBuildWorkspace*	pcWorkspace;
	CVcxProject*		pcProject;
	CArrayChars			aszPlace;
	size				i;

	pcWorkspace = mpcContent->GetWorkspace();
	if (!strchr(szBeside, '\n'))
	{
		for (i = 0; i < pcWorkspace->NumProjects(); i++)
		{
			pcProject = pcWorkspace->GetSolutionProject(i)->GetProject();
			if (pcProject)
			{
				pcProject->AddItemBeside(szBeside, GetIDEFileNamePart(szPath));
			}
		}
		AddToListFiles(szBeside, GetIDEFileNamePart(szPath));
		return;
	}

	aszPlace.Init();
	aszPlace.Split(szBeside, '\n');
	for (i = 0; i < pcWorkspace->NumProjects(); i++)
	{
		pcProject = pcWorkspace->GetSolutionProject(i)->GetProject();
		if (pcProject && (aszPlace.NumElements() > 0) && (StringInsensitiveCompare(pcProject->GetFileName(), aszPlace.GetText(0)) == 0))
		{
			pcProject->AddItem(szPath, (aszPlace.NumElements() > 1) ? aszPlace.GetText(1) : NULL);
		}
	}
	if ((aszPlace.NumElements() > 2) && !aszPlace.Get(2)->Empty())
	{
		AddToListFiles(aszPlace.GetText(2), GetIDEFileNamePart(szPath));
	}
	aszPlace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Each CMakeLists.txt in a project's directory, or one or two above it,
// that has a line naming the file at szBeside gets that line again after
// it with szNewName in the name's place.  With no szNewName the lines
// that name the file are taken out: what was added for it, taken away.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddToListFiles(const char* szBeside, const char* szNewName)
{
	CBuildWorkspace*	pcWorkspace;
	CVcxProject*		pcProject;
	CFileUtil			cFileUtil;
	CArrayChars			aszLists;
	CTextFile			cFile;
	CChars				szDirectory;
	CChars				szList;
	CChars				szText;
	CChars				szLine;
	const char*			szOldName;
	const char*			szFound;
	size				uiOldLength;
	size				uiAt;
	size				uiLineStart;
	size				uiNext;
	size				i;
	size				j;
	bool				bChanged;

	szOldName = GetIDEFileNamePart(szBeside);
	uiOldLength = strlen(szOldName);
	aszLists.Init();
	pcWorkspace = mpcContent->GetWorkspace();
	for (i = 0; i < pcWorkspace->NumProjects(); i++)
	{
		pcProject = pcWorkspace->GetSolutionProject(i)->GetProject();
		if (!pcProject)
		{
			continue;
		}
		szDirectory.Init(pcProject->GetDirectory());
		for (j = 0; j < 3; j++)
		{
			szList.Init(szDirectory);
			szList.Append(FILE_SEPARATOR);
			szList.Append("CMakeLists.txt");
			if (cFileUtil.Exists(szList.Text()))
			{
				aszLists.AddIfUnique(szList.Text());
			}
			szList.Kill();
			cFileUtil.RemoveLastFromPath(&szDirectory);
		}
		szDirectory.Kill();
	}

	for (i = 0; i < aszLists.NumElements(); i++)
	{
		cFile.Init();
		if (!cFile.Read(aszLists.GetText(i)))
		{
			cFile.Kill();
			continue;
		}
		szText.Init();
		szText.AppendSubString(cFile.Text(), (size)cFile.Length());
		cFile.Kill();
		bChanged = false;
		uiAt = 0;
		for (;;)
		{
			szFound = strstr(&szText.Text()[uiAt], szOldName);
			if (!szFound)
			{
				break;
			}
			uiAt = (size)(szFound - szText.Text());
			if (((uiAt > 0) && IsIDENameChar(szText.Text()[uiAt - 1])) || IsIDENameChar(szText.Text()[uiAt + uiOldLength]))
			{
				uiAt += uiOldLength;
				continue;
			}
			for (uiLineStart = uiAt; (uiLineStart > 0) && (szText.Text()[uiLineStart - 1] != '\n'); uiLineStart--)
			{
			}
			uiNext = FindIDENextLine(szText.Text(), uiAt);
			if (!szNewName)
			{
				szText.Remove(uiLineStart, uiNext);
				uiAt = uiLineStart;
				bChanged = true;
				continue;
			}
			szLine.Init();
			szLine.AppendSubString(szText.Text(), uiLineStart, uiAt);
			szLine.Append(szNewName);
			szLine.AppendSubString(szText.Text(), uiAt + uiOldLength, uiNext);
			if (!szLine.EndsWith("\n"))
			{
				szLine.Append('\n');
			}
			szText.Insert(uiNext, szLine.Text());
			uiAt = uiNext + szLine.Length();
			szLine.Kill();
			bChanged = true;
		}
		if (bChanged)
		{
			cFile.Init();
			cFile.mcText.AppendSubString(szText.Text(), (lsize)szText.Length());
			cFile.Write(aszLists.GetText(i));
			cFile.Kill();
		}
		szText.Kill();
	}
	aszLists.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The class moved to files of a stem: see the class's comment.  False if
// the files can't be made.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::Move(SIDEClassMove* psMove, const char* szStem, CChars* pszStatus, const char* szCopyName)
{
	CIDECodeModel*		pcModel;
	CIDEFileEdits		cEdits;
	CArrayIDEFileRanges	asTaken;
	CArrayIDELocations	asLocations;
	SIDERenameTarget	sTarget;
	SIDERenameTarget*	psTarget;
	SCPPSyntaxRange		sRange;
	SIDEFileRange*		psRange;
	SIDEFileRange*		psOther;
	CChars				szOldName;
	CChars				szNewHeader;
	CChars				szNewSource;
	CChars				szHeaderText;
	CChars				szSourceText;
	CChars				szHeaderIncludes;
	CChars				szHeaderBody;
	CChars				szSourceIncludes;
	CChars				szSourceBody;
	const char*			szClassName;
	CChars				szLine;
	CChars				szMessage;
	CChars				szNewName;
	char*				szText;
	char*				szOldSource;
	char*				szUser;
	size				uiStart;
	size				uiEnd;
	size				uiAt;
	size				uiIncludes;
	size				uiOutside;
	size				i;
	size				j;
	uint8				uiKind;
	bool				bTemplates;
	bool				bMerged;
	bool				bResult;

	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cEdits.Init(mpcContent);
	szText = cEdits.GetText(psMove->szHeader.Text());
	szOldSource = psMove->szSource.Empty() ? NULL : cEdits.GetText(psMove->szSource.Text());
	szNewHeader.Init();
	szNewSource.Init();
	AppendNewPath(psMove->szHeader.Text(), szStem, ".h", &szNewHeader);
	if (!psMove->szSource.Empty())
	{
		AppendNewPath(psMove->szSource.Text(), szStem, ".cpp", &szNewSource);
	}
	szNewName.Init(GetIDEFileNamePart(szNewHeader.Text()));
	bResult = szText && (psMove->szSource.Empty() || szOldSource);

	//A copy: every place the class's name is written, as Rename finds them.
	szOldName.Init();
	GetIDESymbolName(psMove->szKey.Text(), &szOldName);
	sTarget.Init();
	psTarget = NULL;
	if (bResult && szCopyName)
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(psMove->szKey.Text(), &asLocations, &uiKind);
		for (i = 0; !psTarget && (i < asLocations.NumElements()); i++)
		{
			if ((asLocations.Get(i)->uiRole == CPPIR_Definition) && IsMoved(psMove, asLocations.Get(i)->szFileName.Text(), asLocations.Get(i)->iLine) &&
				pcModel->FindRenameTarget(asLocations.Get(i)->szFileName.Text(), asLocations.Get(i)->iLine, asLocations.Get(i)->iColumn, &sTarget))
			{
				psTarget = &sTarget;
			}
		}
		KillIDELocations(&asLocations);
		bResult = (psTarget != NULL);
	}

	bTemplates = true;
	//The new files, empty, in the projects: one undo with the edits that write them.
	BeginCreating();
	bResult = bResult && CreateFile(szNewHeader.Text(), psMove->szHeader.Text());
	bResult = bResult && (szNewSource.Empty() || CreateFile(szNewSource.Text(), psMove->szSource.Text()));
	uiIncludes = 0;
	uiOutside = 0;
	asTaken.Init();
	if (bResult)
	{
		//The new header's #includes (the old one's) and what goes in it: the class, and what it defines of the
		//class's outside it; the new source file's the same.  The C++ Header and C++ Source templates lay them out.
		szHeaderIncludes.Init();
		AppendIncludes(szText, &szHeaderIncludes);
		if (psMove->bNeedsHeader)
		{
			szHeaderIncludes.Append("#include \"")->Append(GetIDEFileNamePart(psMove->szHeader.Text()))->Append("\"\n");
		}
		szHeaderBody.Init();
		AppendMovedBlock(szText, psMove->szHeader.Text(), &psMove->sClass, psTarget, szOldName.Text(), szCopyName, &szHeaderBody);

		szSourceIncludes.Init();
		szSourceBody.Init();
		if (szOldSource)
		{
			AppendIncludes(szOldSource, &szSourceIncludes);
		}
		for (i = 0; i < psMove->asOutside.NumElements(); i++)
		{
			psRange = psMove->asOutside.Get(i);
			if (StringInsensitiveCompare(psRange->szFileName.Text(), psMove->szHeader.Text()) == 0)
			{
				szHeaderBody.Append("\n\n");
				sRange.iLine = psRange->iLine;
				sRange.iColumn = psRange->iColumn;
				sRange.iEndLine = psRange->iEndLine;
				sRange.iEndColumn = psRange->iEndColumn;
				AppendMovedBlock(szText, psRange->szFileName.Text(), &sRange, psTarget, szOldName.Text(), szCopyName, &szHeaderBody);
				uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
				uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
				ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
				asTaken.Add()->Init(psMove->szHeader.Text(), 0, 0, 0, 0);
				asTaken.Get(asTaken.NumElements() - 1)->uiStart = uiStart;
				asTaken.Get(asTaken.NumElements() - 1)->uiEnd = uiEnd;
			}
			else
			{
				szSourceBody.Append("\n\n");
				sRange.iLine = psRange->iLine;
				sRange.iColumn = psRange->iColumn;
				sRange.iEndLine = psRange->iEndLine;
				sRange.iEndColumn = psRange->iEndColumn;
				AppendMovedBlock(szOldSource, psRange->szFileName.Text(), &sRange, psTarget, szOldName.Text(), szCopyName, &szSourceBody);
				uiStart = FindIDETextOffset(szOldSource, psRange->iLine, psRange->iColumn);
				uiEnd = FindIDETextOffset(szOldSource, psRange->iEndLine, psRange->iEndColumn);
				ExtendIDEDeletion(szOldSource, &uiStart, &uiEnd, true);
				asTaken.Add()->Init(psMove->szSource.Text(), 0, 0, 0, 0);
				asTaken.Get(asTaken.NumElements() - 1)->uiStart = uiStart;
				asTaken.Get(asTaken.NumElements() - 1)->uiEnd = uiEnd;
			}
			uiOutside++;
		}
		szHeaderText.Init();
		szSourceText.Init();
		szClassName = szCopyName ? szCopyName : szOldName.Text();
		bTemplates = AppendNewFileText(IDE_TEMPLATE_CPP_HEADER, szNewHeader.Text(), psMove->szHeader.Text(), szClassName, szHeaderIncludes.Text(), szHeaderBody.Text(), &szHeaderText);
		bTemplates = bTemplates && (szNewSource.Empty() || AppendNewFileText(IDE_TEMPLATE_CPP_SOURCE, szNewSource.Text(), psMove->szSource.Text(), szClassName, szSourceIncludes.Text(), szSourceBody.Text(), &szSourceText));
		szSourceBody.Kill();
		szSourceIncludes.Kill();
		szHeaderBody.Kill();
		szHeaderIncludes.Kill();

		//The class out of the old header.
		uiStart = FindIDETextOffset(szText, psMove->sClass.iLine, psMove->sClass.iColumn);
		uiEnd = FindIDETextOffset(szText, psMove->sClass.iEndLine, psMove->sClass.iEndColumn);
		ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
		asTaken.Add()->Init(psMove->szHeader.Text(), 0, 0, 0, 0);
		asTaken.Get(asTaken.NumElements() - 1)->uiStart = uiStart;
		asTaken.Get(asTaken.NumElements() - 1)->uiEnd = uiEnd;

		//What is taken out (nothing, for a copy): pieces that touch or overlap (each takes the blank lines around
		//it) are one piece, and one a file ends with takes the blank lines before it.
		if (szCopyName)
		{
			KillIDEFileRanges(&asTaken);
			asTaken.Init();
		}
		bMerged = true;
		while (bMerged)
		{
			bMerged = false;
			for (i = 0; i < asTaken.NumElements(); i++)
			{
				psRange = asTaken.Get(i);
				for (j = 0; (j < asTaken.NumElements()) && (psRange->uiEnd > psRange->uiStart); j++)
				{
					psOther = asTaken.Get(j);
					if ((j != i) && (psOther->uiEnd > psOther->uiStart) && (psOther->uiStart >= psRange->uiStart) && (psOther->uiStart <= psRange->uiEnd) &&
						(StringInsensitiveCompare(psOther->szFileName.Text(), psRange->szFileName.Text()) == 0))
					{
						psRange->uiEnd = (psOther->uiEnd > psRange->uiEnd) ? psOther->uiEnd : psRange->uiEnd;
						psOther->uiEnd = psOther->uiStart;
						bMerged = true;
					}
				}
			}
		}
		for (i = 0; i < asTaken.NumElements(); i++)
		{
			psRange = asTaken.Get(i);
			if (psRange->uiEnd > psRange->uiStart)
			{
				szUser = cEdits.GetText(psRange->szFileName.Text());
				ExtendIDEDeletionAtEnd(szUser, &psRange->uiStart, psRange->uiEnd);
				cEdits.GetEdits(psRange->szFileName.Text())->Replace(psRange->uiStart, psRange->uiEnd, "");
			}
		}

		//The new header #included where the class is needed (nowhere yet, for a copy).
		szLine.Init();
		if (szCopyName)
		{
		}
		else if (psMove->bHeaderUses)
		{
			FindIncludePlace(szText, "", szNewName.Text(), &uiAt, &szLine);
			if (!szLine.Empty())
			{
				cEdits.GetEdits(psMove->szHeader.Text())->Insert(uiAt, szLine.Text());
				uiIncludes++;
			}
		}
		else
		{
			for (i = 0; i < psMove->aszUsers.NumElements(); i++)
			{
				szUser = cEdits.GetText(psMove->aszUsers.GetText(i));
				if (!szUser)
				{
					continue;
				}
				FindIncludePlace(szUser, psMove->szHeader.Text(), szNewName.Text(), &uiAt, &szLine);
				if (!szLine.Empty())
				{
					cEdits.GetEdits(psMove->aszUsers.GetText(i))->Insert(uiAt, szLine.Text());
					uiIncludes++;
				}
			}
		}
		szLine.Kill();

		//The new files filled, as edits, so undo empties them.
		szUser = cEdits.GetText(szNewHeader.Text());
		bResult = bTemplates && (szUser != NULL);
		if (bResult)
		{
			cEdits.GetEdits(szNewHeader.Text())->Insert(0, szHeaderText.Text());
		}
		if (bResult && !szNewSource.Empty())
		{
			szUser = cEdits.GetText(szNewSource.Text());
			bResult = (szUser != NULL);
			if (bResult)
			{
				cEdits.GetEdits(szNewSource.Text())->Insert(0, szSourceText.Text());
			}
		}
		szSourceText.Kill();
		szHeaderText.Kill();
	}
	if (!bResult)
	{
		EndCreating(false);
	}
	if (bResult)
	{
		cEdits.Apply();
		mpcContent->GetDocuments()->SaveAllFiles();
		EndCreating(true);
		mpcContent->GetCodeModel()->Reset();
		mpcContent->GetSolutions()->WorkspaceChanged(true);

		szMessage.Init(szCopyName ? "Copied " : "Moved ");
		szMessage.Append(psMove->szName);
		if (szCopyName)
		{
			szMessage.Append(" as ");
			szMessage.Append(szCopyName);
		}
		szMessage.Append(" to ");
		szMessage.Append(szNewName);
		if (!szNewSource.Empty())
		{
			szMessage.Append(" and ");
			szMessage.Append(GetIDEFileNamePart(szNewSource.Text()));
		}
		szMessage.Append(": ");
		szMessage.Append((uint32)uiOutside);
		szMessage.Append((uiOutside == 1) ? " definition outside it" : " definitions outside it");
		if (!szCopyName)
		{
			szMessage.Append(", ");
			szMessage.Append((uint32)uiIncludes);
			szMessage.Append((uiIncludes == 1) ? " #include added" : " #includes added");
		}
		szMessage.Append(".");
		if (pszStatus)
		{
			pszStatus->Set(szMessage.Text());
		}
		else
		{
			mpcContent->ShowStatus(szMessage.Text());
		}
		szMessage.Kill();
	}
	else if (!bTemplates)
	{
		szMessage.Init(szCopyName ? "Copy Class: " : "Move Class: ");
		szMessage.Append(mpcContent->GetFileTemplates()->GetError())->Append('.');
		if (pszStatus)
		{
			pszStatus->Set(szMessage.Text());
		}
		else
		{
			mpcContent->ShowStatus(szMessage.Text());
		}
		szMessage.Kill();
	}
	KillIDEFileRanges(&asTaken);
	sTarget.Kill();
	szOldName.Kill();
	szNewName.Kill();
	szNewSource.Kill();
	szNewHeader.Kill();
	cEdits.Kill();
	KillUnits();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEMoveClass::PrintMove(SIDEClassMove* psMove, CChars* pszDest)
{
	pszDest->Append("Move Class: ")->Append(psMove->szKey);
	if (!psMove->bMovable)
	{
		pszDest->Append(" can't be moved: ")->Append(psMove->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	pszDest->Append(" from ")->Append(GetIDEFileNamePart(psMove->szHeader.Text()));
	pszDest->Append(", ");
	pszDest->Append((uint32)psMove->asOutside.NumElements());
	pszDest->Append(" outside");
	if (!psMove->szSource.Empty())
	{
		pszDest->Append(" (")->Append(GetIDEFileNamePart(psMove->szSource.Text()))->Append(")");
	}
	pszDest->Append(", to ")->Append(psMove->szStem)->Append('\n');
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog says why the name written can't be the files' (FindProblem).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::ApproveMove(void)
{
	CChars	szProblem;
	bool	bProblem;

	szProblem.Init();
	bProblem = FindProblem(mpsMove, mpNameField->GetText(), &szProblem);
	if (bProblem)
	{
		if ((szProblem.Text()[0] >= 'a') && (szProblem.Text()[0] <= 'z'))
		{
			szProblem.Text()[0] -= 'a' - 'A';
		}
		szProblem.Append('.');
	}
	mpError->SetText(szProblem.Text());
	szProblem.Kill();
	return !bProblem;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFunctionMove::Init(void)
{
	szKey.Init();
	szName.Init();
	bMovable = false;
	szWhyNot.Init();
	bFunction = true;
	asDeclarations.Init();
	sDefinition.Init("", 0, 0, 0, 0);
	bDefinition = false;
	aszUsers.Init();
	szNamespace.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFunctionMove::Kill(void)
{
	szNamespace.Kill();
	aszUsers.Kill();
	sDefinition.Kill();
	KillIDEFileRanges(&asDeclarations);
	szWhyNot.Kill();
	szName.Kill();
	szKey.Kill();
}


#define IDE_MOVE_COMPLETIONS			12
#define IDE_MOVE_COMPLETION_TAKEN	(-2147483647 - 1)


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEMoveHeader(const char* szFileName)
{
	CChars	szName;
	bool	bHeader;

	szName.Init(szFileName);
	bHeader = szName.EndsWithIgnoreCase(".h") || szName.EndsWithIgnoreCase(".hpp") || szName.EndsWithIgnoreCase(".hh") || szName.EndsWithIgnoreCase(".inl");
	szName.Kill();
	return bHeader;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a line of a file is in what moves with a function: one of its
// declarations, or its definition.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::IsFunctionMoved(SIDEFunctionMove* psMove, const char* szFileName, int32 iLine)
{
	SIDEFileRange*	psRange;
	size			i;

	for (i = 0; i < psMove->asDeclarations.NumElements() + (psMove->bDefinition ? 1 : 0); i++)
	{
		psRange = (i < psMove->asDeclarations.NumElements()) ? psMove->asDeclarations.Get(i) : &psMove->sDefinition;
		if ((StringInsensitiveCompare(szFileName, psRange->szFileName.Text()) == 0) && (iLine >= psRange->iLine) && (iLine <= psRange->iEndLine))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What Move to File has to work with, for the function or variable at
// the caret that is in no class.  False if there is none there;
// psDest->bMovable says whether it can be moved, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::FindFunctionMove(SIDEFunctionMove* psDest)
{
	CIDECodeModel*			pcModel;
	CArrayIDELocations		asLocations;
	SIDELocation*			psLocation;
	CIDERefactorUnit*		pcUnit;
	CCPPSyntaxDeclarator*	pcDeclarator;
	CCPPSyntax*				pcOuter;
	SIDESignatureSite		sSite;
	SIDENamespaceSite		sNamespaceSite;
	CIDEFileEdits			cFiles;
	SCPPSyntaxRange			sRange;
	CChars					szScope;
	const char*				szWhyNot;
	int						iLine;
	int						iColumn;
	int32					iFirst;
	int32					iLast;
	size					i;
	uint8					uiRole;
	uint8					uiKind;
	bool					bRead;
	bool					bBody;

	pcModel = mpcContent->GetCodeModel();
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) ||
		!pcModel->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &psDest->szKey, &uiKind) ||
		((uiKind != CPPSK_Function) && (uiKind != CPPSK_FunctionTemplate) && (uiKind != CPPSK_Variable)))
	{
		return false;
	}
	psDest->bFunction = (uiKind != CPPSK_Variable);
	GetIDEScopedName(psDest->szKey.Text(), &psDest->szName);
	szScope.Init();
	GetIDESymbolScope(psDest->szKey.Text(), &szScope);
	if (szScope.Contains("(") || (psDest->szKey.Text()[0] == '@'))
	{
		//A function's own variable.
		szScope.Kill();
		return false;
	}
	psDest->bMovable = true;
	if (!szScope.Empty() && mpcContent->GetMoveNamespace()->IsNamespace(szScope.Text()))
	{
		//In a namespace: it goes into a block of it.
		psDest->szNamespace.Set(szScope);
	}
	else if (!szScope.Empty())
	{
		psDest->bMovable = false;
		psDest->szWhyNot.Set("it is in ");
		psDest->szWhyNot.Append(szScope);
	}
	szScope.Kill();

	asLocations.Init();
	pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
	for (i = 0; psDest->bMovable && (i < asLocations.NumElements()); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			continue;
		}
		pcUnit = (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) != MAX_SIZE) ? GetUnit(psLocation->szFileName.Text()) : NULL;
		bRead = false;
		bBody = (psLocation->uiRole == CPPIR_Definition);
		if (pcUnit && psDest->bFunction)
		{
			sSite.Init();
			bRead = mpcContent->GetChangeSignature()->ReadSite(pcUnit, psLocation, "", &sSite, &szWhyNot);
			sRange = sSite.sAll;
			bBody = sSite.bBody;
			sSite.Kill();
		}
		else if (pcUnit)
		{
			pcDeclarator = (CCPPSyntaxDeclarator*)pcUnit->FindAncestor(pcUnit->FindNodeAt(psLocation->iLine, psLocation->iColumn), CPPS_Declarator);
			while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
			{
				pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
			}
			pcOuter = pcDeclarator ? pcDeclarator->mpcParent : NULL;
			if (pcOuter && (pcOuter->GetKind() == CPPS_InitDeclarator))
			{
				pcOuter = pcOuter->mpcParent;
			}
			bRead = pcOuter && pcUnit->GetRange(pcOuter, &sRange);
			if (bRead && pcUnit->FindTokensIn(sRange.iLine, sRange.iColumn, sRange.iEndLine, sRange.iEndColumn, &iFirst, &iLast))
			{
				//A variable is defined where it is declared without extern.
				bBody = !pcUnit->IsToken(iFirst, "extern");
			}
		}
		if (!bRead)
		{
			psDest->bMovable = false;
			psDest->szWhyNot.Set("a declaration of it can't be read (it isn't in the solution, or a macro makes it)");
			break;
		}
		if (!psDest->szNamespace.Empty())
		{
			//Its namespace's blocks have to be read too, and none of them unnamed or inline.
			sNamespaceSite.Init();
			cFiles.Init(mpcContent);
			if (!mpcContent->GetMoveNamespace()->FindSite(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, false, &cFiles, &sNamespaceSite, &psDest->szWhyNot))
			{
				psDest->bMovable = false;
			}
			cFiles.Kill();
			sNamespaceSite.Kill();
			if (!psDest->bMovable)
			{
				break;
			}
		}
		if (pcUnit->FindTokensIn(sRange.iLine, sRange.iColumn, sRange.iEndLine, sRange.iEndColumn, &iFirst, &iLast) && pcUnit->IsToken(iFirst, "static"))
		{
			psDest->bMovable = false;
			psDest->szWhyNot.Set("it is static: only ");
			psDest->szWhyNot.Append(GetIDEFileNamePart(psLocation->szFileName.Text()));
			psDest->szWhyNot.Append(" can use it");
			break;
		}
		if (bBody && !psDest->bDefinition)
		{
			psDest->sDefinition.Kill();
			psDest->sDefinition.Init(psLocation->szFileName.Text(), sRange.iLine, sRange.iColumn, sRange.iEndLine, sRange.iEndColumn);
			psDest->bDefinition = true;
		}
		else if (!bBody)
		{
			psDest->asDeclarations.Add()->Init(psLocation->szFileName.Text(), sRange.iLine, sRange.iColumn, sRange.iEndLine, sRange.iEndColumn);
		}
	}
	for (i = 0; psDest->bMovable && (i < asLocations.NumElements()); i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole == CPPIR_Reference) && !IsFunctionMoved(psDest, psLocation->szFileName.Text(), psLocation->iLine) && (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) != MAX_SIZE))
		{
			psDest->aszUsers.AddIfUnique(psLocation->szFileName.Text());
		}
	}
	KillIDELocations(&asLocations);
	KillUnits();
	mpcContent->GetMoveNamespace()->KillUnits();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The header a function is to go to, named as it is typed: its path and,
// if the function is defined in a source file, the path of the header's
// source file.  True, with why, if it can't go there: no such header in
// the solution, or more than one, or it is there already, or the header
// has no source file.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::FindFunctionProblem(SIDEFunctionMove* psMove, const char* szHeaderName, CChars* pszHeader, CChars* pszSource, CChars* pszDest)
{
	CArrayChars	aszFound;
	CArrayChars	aszRelated;
	size		i;
	bool		bThere;

	aszFound.Init();
	if (!IsIDEMoveHeader(szHeaderName))
	{
		pszDest->Set("it goes to a header: a .h file of the solution");
	}
	else if (mpcContent->GetCodeModel()->FindFilesNamed(szHeaderName, &aszFound) == 0)
	{
		pszDest->Set("there is no ");
		pszDest->Append(szHeaderName);
		pszDest->Append(" in the solution");
	}
	else if (aszFound.NumElements() > 1)
	{
		pszDest->Set("there is more than one ");
		pszDest->Append(szHeaderName);
		pszDest->Append(" in the solution");
	}
	else
	{
		pszHeader->Set(aszFound.GetText(0));
		bThere = psMove->bDefinition && (StringInsensitiveCompare(psMove->sDefinition.szFileName.Text(), pszHeader->Text()) == 0);
		for (i = 0; i < psMove->asDeclarations.NumElements(); i++)
		{
			bThere = bThere || (StringInsensitiveCompare(psMove->asDeclarations.Get(i)->szFileName.Text(), pszHeader->Text()) == 0);
		}
		if (bThere)
		{
			pszDest->Set("it is in ");
			pszDest->Append(szHeaderName);
			pszDest->Append(" already");
		}
		else if (psMove->bDefinition && !IsIDEMoveHeader(psMove->sDefinition.szFileName.Text()))
		{
			aszRelated.Init();
			mpcContent->GetFileRename()->FindRelatedFiles(pszHeader->Text(), &aszRelated);
			for (i = 0; i < aszRelated.NumElements(); i++)
			{
				if ((aszRelated.Get(i)->EndsWithIgnoreCase(".cpp") || aszRelated.Get(i)->EndsWithIgnoreCase(".c")) && pszSource->Empty())
				{
					pszSource->Set(aszRelated.GetText(i));
				}
			}
			aszRelated.Kill();
			if (pszSource->Empty())
			{
				pszDest->Set(szHeaderName);
				pszDest->Append(" has no source file in the solution for its definition");
			}
			else if (StringInsensitiveCompare(pszSource->Text(), psMove->sDefinition.szFileName.Text()) == 0)
			{
				pszDest->Set("it is defined in ");
				pszDest->Append(GetIDEFileNamePart(pszSource->Text()));
				pszDest->Append(" already");
			}
		}
	}
	aszFound.Kill();
	return !pszDest->Empty();
}


//////////////////////////////////////////////////////////////////////////
//
// Text to go into a file at a place, gathered: what goes to one place is
// one insertion (the place is kept in the range's uiStart).
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddInsert(const char* szFileName, size uiAt, const char* szText, CArrayIDEFileRanges* pasInserts, CArrayChars* paszTexts)
{
	size	i;

	for (i = 0; i < pasInserts->NumElements(); i++)
	{
		if ((pasInserts->Get(i)->uiStart == uiAt) && (StringInsensitiveCompare(pasInserts->Get(i)->szFileName.Text(), szFileName) == 0))
		{
			if (!paszTexts->Get(i)->Contains(szText))
			{
				paszTexts->Get(i)->Append(szText);
			}
			return;
		}
	}
	pasInserts->Add()->Init(szFileName, 0, 0, 0, 0);
	pasInserts->Get(pasInserts->NumElements() - 1)->uiStart = uiAt;
	paszTexts->Add(szText);
}


//////////////////////////////////////////////////////////////////////////
//
// A file given an #include of a header, if it has none: beside its
// #include of szBeside (written as that one is), or after its last.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddInclude(const char* szFileName, const char* szBeside, const char* szIncluded, CIDEFileEdits* pcEdits, CArrayIDEFileRanges* pasInserts, CArrayChars* paszTexts)
{
	CChars	szLine;
	char*	szText;
	size	uiAt;

	szText = (StringInsensitiveCompare(szFileName, szIncluded) != 0) ? pcEdits->GetText(szFileName) : NULL;
	if (!szText)
	{
		return;
	}
	szLine.Init();
	FindIncludePlace(szText, szBeside, GetIDEFileNamePart(szIncluded), &uiAt, &szLine);
	if (!szLine.Empty())
	{
		AddInsert(szFileName, uiAt, szLine.Text(), pasInserts, paszTexts);
	}
	szLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The file that moved text goes to (szTarget) #includes the headers that
// declare what the text names, where it doesn't already: each thing's,
// by the header of the solution that declares it.  Nothing for what is
// declared in the files the text goes to, or moves with it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddUsedIncludes(SIDEFileRange* psRange, const char* szTarget, const char* szOtherTarget, SIDEFunctionMove* psMove, CIDEFileEdits* pcEdits, CArrayIDEFileRanges* pasInserts, CArrayChars* paszTexts)
{
	CIDECodeModel*		pcModel;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	CArrayChars			aszKeys;
	SIDELocation*		psLocation;
	CChars				szScope;
	const char*			szDeclared;
	size				i;
	size				j;
	uint8				uiKind;
	bool				bLocal;

	pcModel = mpcContent->GetCodeModel();
	asRanges.Init();
	asRanges.Add()->Init(psRange->szFileName.Text(), psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsUsedIn(&asRanges, &aszKeys);
	KillIDEFileRanges(&asRanges);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		szScope.Init();
		GetIDESymbolScope(aszKeys.GetText(i), &szScope);
		bLocal = szScope.Contains("(") || (aszKeys.GetText(i)[0] == '@') || aszKeys.Get(i)->Equals(psMove->szKey.Text());
		szScope.Kill();
		if (bLocal)
		{
			continue;
		}
		//The header that declares it: the first of the solution's; none if the text's own files do.
		asLocations.Init();
		pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind);
		szDeclared = NULL;
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			psLocation = asLocations.Get(j);
			if ((psLocation->uiRole == CPPIR_Reference) || IsFunctionMoved(psMove, psLocation->szFileName.Text(), psLocation->iLine))
			{
				continue;
			}
			if ((StringInsensitiveCompare(psLocation->szFileName.Text(), szTarget) == 0) || (szOtherTarget && (StringInsensitiveCompare(psLocation->szFileName.Text(), szOtherTarget) == 0)))
			{
				szDeclared = NULL;
				break;
			}
			if (!szDeclared && IsIDEMoveHeader(psLocation->szFileName.Text()) && (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) != MAX_SIZE))
			{
				szDeclared = psLocation->szFileName.Text();
			}
		}
		if (szDeclared)
		{
			AddInclude(szTarget, "", szDeclared, pcEdits, pasInserts, paszTexts);
		}
		KillIDELocations(&asLocations);
	}
	aszKeys.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Where a declaration goes in a header: after the last function it
// declares without defining (true), so its prototypes stay together;
// false if it has none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::FindPrototypePlace(const char* szHeader, const char* szText, size* puiAt)
{
	CArrayIDELocations	asDeclared;
	CArrayIDELocations	asDefined;
	CIDERefactorUnit*	pcUnit;
	SIDESignatureSite	sSite;
	const char*			szWhyNot;
	size				i;
	bool				bFound;

	asDeclared.Init();
	asDefined.Init();
	mpcContent->GetCodeModel()->FindSiblingFunctions("x", szHeader, &asDeclared, &asDefined);
	pcUnit = GetUnit(szHeader);
	bFound = false;
	for (i = asDeclared.NumElements(); pcUnit && !bFound && (i > 0); i--)
	{
		sSite.Init();
		if (mpcContent->GetChangeSignature()->ReadSite(pcUnit, asDeclared.Get(i - 1), "", &sSite, &szWhyNot) && !sSite.bBody && !sSite.bInClass && sSite.szOwnTemplate.Empty())
		{
			*puiAt = FindIDENextLine(szText, FindIDETextOffset(szText, sSite.sAll.iEndLine, sSite.sAll.iEndColumn));
			bFound = true;
		}
		sSite.Kill();
	}
	KillIDELocations(&asDefined);
	KillIDELocations(&asDeclared);
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Where something goes at the end of a file: before a header's closing
// #endif and the blank lines over it, or at the text's end.
//
//////////////////////////////////////////////////////////////////////////
size CIDEMoveClass::FindEndPlace(const char* szText)
{
	size	uiAt;
	size	uiLine;
	size	uiPrevious;
	size	i;
	bool	bBlank;

	uiAt = strlen(szText);
	for (uiLine = uiAt; uiLine > 0; uiLine--)
	{
		if (((uiLine == 1) || (szText[uiLine - 2] == '\n')) && (strncmp(&szText[uiLine - 1], "#endif", 6) == 0))
		{
			uiAt = uiLine - 1;
			break;
		}
	}
	while (uiAt > 0)
	{
		for (uiPrevious = uiAt - 1; (uiPrevious > 0) && (szText[uiPrevious - 1] != '\n'); uiPrevious--)
		{
		}
		bBlank = true;
		for (i = uiPrevious; i < uiAt; i++)
		{
			bBlank = bBlank && ((szText[i] == ' ') || (szText[i] == '\t') || (szText[i] == '\r') || (szText[i] == '\n'));
		}
		if (!bBlank)
		{
			break;
		}
		uiAt = uiPrevious;
	}
	return uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the line that ends just before uiAt (a line's start) is blank.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEBlankLineBefore(const char* szText, size uiAt)
{
	size	i;

	if (uiAt == 0)
	{
		return false;
	}
	for (i = uiAt - 1; i > 0; i--)
	{
		if (szText[i - 1] == '\n')
		{
			break;
		}
		if ((szText[i - 1] != ' ') && (szText[i - 1] != '\t') && (szText[i - 1] != '\r'))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What is taken out from *puiStart to *puiEnd (whole lines) made to take
// the blank lines between it and a block's braces: after it if it was
// first in its block, before it if it was last, so what stays starts
// after the opening brace and ends at the closing one.
//
//////////////////////////////////////////////////////////////////////////
static void TrimIDEBlockEdges(const char* szText, size* puiStart, size* puiEnd)
{
	size	uiAt;
	size	uiLine;

	//The first line before it that isn't blank: an opening brace's?
	uiLine = *puiStart;
	while ((uiLine > 0) && IsIDEBlankLineBefore(szText, uiLine))
	{
		for (uiLine--; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
		{
		}
	}
	if (uiLine > 0)
	{
		for (uiAt = uiLine - 1; (uiAt > 0) && ((szText[uiAt - 1] == ' ') || (szText[uiAt - 1] == '\t') || (szText[uiAt - 1] == '\r')); uiAt--)
		{
		}
		if ((uiAt > 0) && (szText[uiAt - 1] == '{'))
		{
			*puiStart = uiLine;
			while (szText[*puiEnd] != '\0')
			{
				for (uiAt = *puiEnd; (szText[uiAt] == ' ') || (szText[uiAt] == '\t') || (szText[uiAt] == '\r'); uiAt++)
				{
				}
				if (szText[uiAt] != '\n')
				{
					break;
				}
				*puiEnd = uiAt + 1;
			}
			return;
		}
	}

	//The first line after it that isn't blank: a closing brace's?
	for (uiAt = *puiEnd; (szText[uiAt] == ' ') || (szText[uiAt] == '\t') || (szText[uiAt] == '\r') || (szText[uiAt] == '\n'); uiAt++)
	{
	}
	if (szText[uiAt] == '}')
	{
		while ((*puiStart > 0) && IsIDEBlankLineBefore(szText, *puiStart))
		{
			for ((*puiStart)--; (*puiStart > 0) && (szText[*puiStart - 1] != '\n'); (*puiStart)--)
			{
			}
		}
		for (uiAt = *puiEnd; szText[uiAt] != '\0'; )
		{
			for (uiLine = uiAt; (szText[uiLine] == ' ') || (szText[uiLine] == '\t') || (szText[uiLine] == '\r'); uiLine++)
			{
			}
			if (szText[uiLine] != '\n')
			{
				break;
			}
			uiAt = uiLine + 1;
		}
		*puiEnd = uiAt;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The function or variable moved to a header (and its source file): see
// the class's comment.  One undo across the files.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::MoveFunction(SIDEFunctionMove* psMove, const char* szHeader, const char* szSource, CChars* pszStatus)
{
	CIDEFileEdits		cEdits;
	CArrayIDEFileRanges	asInserts;
	CArrayIDEFileRanges	asTaken;
	CArrayChars			aszTexts;
	SIDEFileRange*		psRange;
	SIDEFileRange*		psOther;
	CChars				szBlock;
	CChars				szMessage;
	const char*			szTarget;
	const char*			szOldHeader;
	char*				szText;
	char*				szTargetText;
	size				uiAt;
	size				uiStart;
	size				uiEnd;
	size				uiPieces;
	size				uiBefore;
	size				i;
	size				j;
	bool				bMerged;
	bool				bResult;
	bool				bInBlocks;
	SIDENamespaceSite	sNamespaceSite;
	CChars				szWhyNot;

	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szWhyNot.Init();
	cEdits.Init(mpcContent);
	asInserts.Init();
	asTaken.Init();
	aszTexts.Init();
	szOldHeader = "";
	bResult = true;
	uiPieces = psMove->asDeclarations.NumElements() + (psMove->bDefinition ? 1 : 0);
	for (i = 0; bResult && (i < uiPieces); i++)
	{
		//Its declarations to the header; its definition to the header's source file, or to the header if a
		//header had it.
		psRange = (i < psMove->asDeclarations.NumElements()) ? psMove->asDeclarations.Get(i) : &psMove->sDefinition;
		szTarget = ((i < psMove->asDeclarations.NumElements()) || IsIDEMoveHeader(psRange->szFileName.Text())) ? szHeader : szSource;
		szText = cEdits.GetText(psRange->szFileName.Text());
		szTargetText = cEdits.GetText(szTarget);
		if (!szText || !szTargetText)
		{
			bResult = false;
			break;
		}
		if ((i < psMove->asDeclarations.NumElements()) && IsIDEMoveHeader(psRange->szFileName.Text()) && (szOldHeader[0] == '\0'))
		{
			szOldHeader = psRange->szFileName.Text();
		}
		szBlock.Init();
		sNamespaceSite.Init();
		bInBlocks = !psMove->szNamespace.Empty() && mpcContent->GetMoveNamespace()->FindSite(psRange->szFileName.Text(), psRange->iLine, psRange->iColumn, false, &cEdits, &sNamespaceSite, &szWhyNot) &&
					!sNamespaceSite.bQualified && (sNamespaceSite.asBlocks.NumElements() > 0);
		if (bInBlocks)
		{
			//In its namespace's blocks: into the last block of the namespace there, or one of its own at the end.
			AppendNamespacedBlock(psMove->szNamespace.Text(), szText, psRange, szTarget, szTargetText, &cEdits, &uiAt, &szBlock);
		}
		else if ((i < psMove->asDeclarations.NumElements()) && FindPrototypePlace(szHeader, szTargetText, &uiAt))
		{
			AppendBlock(szText, psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &szBlock);
		}
		else
		{
			//Two blank lines from what is before it, and, before a header's #endif, the blank lines there stay.
			uiAt = FindEndPlace(szTargetText);
			szBlock.Append(((uiAt > 0) && (szTargetText[uiAt - 1] != '\n')) ? "\n\n\n" : "\n\n");
			AppendBlock(szText, psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &szBlock);
		}
		AddInsert(szTarget, uiAt, szBlock.Text(), &asInserts, &aszTexts);
		szBlock.Kill();
		AddUsedIncludes(psRange, szTarget, (szTarget == szSource) ? szHeader : NULL, psMove, &cEdits, &asInserts, &aszTexts);

		uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
		uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
		ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
		if (bInBlocks)
		{
			//A block it leaves empty goes with it.
			uiBefore = uiStart;
			mpcContent->GetMoveNamespace()->WidenToEmptyBlocks(&sNamespaceSite, szText, &uiStart, &uiEnd);
			if (uiStart != uiBefore)
			{
				ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			}
			TrimIDEBlockEdges(szText, &uiStart, &uiEnd);
		}
		sNamespaceSite.Kill();
		asTaken.Add()->Init(psRange->szFileName.Text(), 0, 0, 0, 0);
		asTaken.Get(asTaken.NumElements() - 1)->uiStart = uiStart;
		asTaken.Get(asTaken.NumElements() - 1)->uiEnd = uiEnd;
	}
	if (bResult)
	{
		//Its definition's file #includes the header it is declared in, and so does each file that names it.
		if (psMove->bDefinition && !IsIDEMoveHeader(psMove->sDefinition.szFileName.Text()))
		{
			AddInclude(szSource, "", szHeader, &cEdits, &asInserts, &aszTexts);
		}
		for (i = 0; i < psMove->aszUsers.NumElements(); i++)
		{
			if (!szSource || (StringInsensitiveCompare(psMove->aszUsers.GetText(i), szSource) != 0))
			{
				AddInclude(psMove->aszUsers.GetText(i), szOldHeader, szHeader, &cEdits, &asInserts, &aszTexts);
			}
		}

		bMerged = true;
		while (bMerged)
		{
			bMerged = false;
			for (i = 0; i < asTaken.NumElements(); i++)
			{
				psRange = asTaken.Get(i);
				for (j = 0; (j < asTaken.NumElements()) && (psRange->uiEnd > psRange->uiStart); j++)
				{
					psOther = asTaken.Get(j);
					if ((j != i) && (psOther->uiEnd > psOther->uiStart) && (psOther->uiStart >= psRange->uiStart) && (psOther->uiStart <= psRange->uiEnd) &&
						(StringInsensitiveCompare(psOther->szFileName.Text(), psRange->szFileName.Text()) == 0))
					{
						psRange->uiEnd = (psOther->uiEnd > psRange->uiEnd) ? psOther->uiEnd : psRange->uiEnd;
						psOther->uiEnd = psOther->uiStart;
						bMerged = true;
					}
				}
			}
		}
		for (i = 0; i < asTaken.NumElements(); i++)
		{
			psRange = asTaken.Get(i);
			if (psRange->uiEnd > psRange->uiStart)
			{
				szText = cEdits.GetText(psRange->szFileName.Text());
				ExtendIDEDeletionAtEnd(szText, &psRange->uiStart, psRange->uiEnd);
				cEdits.GetEdits(psRange->szFileName.Text())->Replace(psRange->uiStart, psRange->uiEnd, "");
			}
		}
		for (i = 0; i < asInserts.NumElements(); i++)
		{
			cEdits.GetEdits(asInserts.Get(i)->szFileName.Text())->Insert(asInserts.Get(i)->uiStart, aszTexts.GetText(i));
		}
		cEdits.Apply();

		szMessage.Init("Moved ");
		szMessage.Append(psMove->szName);
		szMessage.Append(" to ");
		szMessage.Append(GetIDEFileNamePart(szHeader));
		if (szSource && (szSource[0] != '\0'))
		{
			szMessage.Append(" and ");
			szMessage.Append(GetIDEFileNamePart(szSource));
		}
		szMessage.Append(": ");
		szMessage.Append((uint32)cEdits.NumFiles());
		szMessage.Append((cEdits.NumFiles() == 1) ? " file changed." : " files changed.");
		if (pszStatus)
		{
			pszStatus->Set(szMessage.Text());
		}
		else
		{
			mpcContent->ShowStatus(szMessage.Text());
		}
		szMessage.Kill();
	}
	aszTexts.Kill();
	KillIDEFileRanges(&asTaken);
	KillIDEFileRanges(&asInserts);
	cEdits.Kill();
	KillUnits();
	mpcContent->GetMoveNamespace()->KillUnits();
	szWhyNot.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What is at psRange of szText (a function's declaration or definition
// in szNamespace's blocks) as it goes to szTarget: into the last block of
// szNamespace there, before its closing brace, or else a block of its own
// at the end (before a header's #endif), indented as the block is.  The
// place it goes is *puiAt.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AppendNamespacedBlock(const char* szNamespace, const char* szText, SIDEFileRange* psRange, const char* szTarget, const char* szTargetText, CIDEFileEdits* pcEdits, size* puiAt, CChars* pszDest)
{
	SIDENamespaceBlock	sBlock;
	CChars				szMoved;
	size				uiLine;
	size				uiStrip;
	size				uiDepth;
	size				uiFound;
	size				uiCut;
	CChars				szOuter;
	size				i;

	szMoved.Init();
	AppendBlock(szText, psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &szMoved);
	for (uiLine = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn); (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
	{
	}
	for (uiStrip = 0; (szText[uiLine + uiStrip] == '\t') || (szText[uiLine + uiStrip] == ' '); uiStrip++)
	{
	}
	uiDepth = 1;
	for (i = 0; szNamespace[i] != '\0'; i++)
	{
		if ((szNamespace[i] == ':') && (szNamespace[i + 1] == ':'))
		{
			uiDepth++;
			i++;
		}
	}

	sBlock.Init();
	szOuter.Init(szNamespace);
	for (uiFound = uiDepth; uiFound > 0; uiFound--)
	{
		if (mpcContent->GetMoveNamespace()->FindBlockIn(szTarget, szOuter.Text(), pcEdits, &sBlock))
		{
			break;
		}
		uiCut = szOuter.FindFromEnd("::");
		if (uiCut == ARRAY_ELEMENT_NOT_FOUND)
		{
			uiFound = 1;
			continue;
		}
		szOuter.SetLength(uiCut);
	}
	szOuter.Kill();
	if (uiFound > 0)
	{
		//The block of the namespace, or of the outermost uiFound of its parts, before its closing brace; the parts
		//it doesn't have opened in it.
		*puiAt = sBlock.uiCloseStart;
		if (sBlock.uiCloseStart > sBlock.uiOpenEnd)
		{
			pszDest->Append(IsIDEMoveHeader(szTarget) ? "\n" : "\n\n");
		}
		mpcContent->GetMoveNamespace()->AppendBlockLines(szNamespace, uiFound, "\t", uiFound, true, pszDest);
		mpcContent->GetMoveNamespace()->AppendReindented(szMoved.Text(), uiStrip, uiDepth, pszDest);
		mpcContent->GetMoveNamespace()->AppendBlockLines(szNamespace, uiFound, "\t", uiFound, false, pszDest);
	}
	else
	{
		*puiAt = FindEndPlace(szTargetText);
		pszDest->Append(((*puiAt > 0) && (szTargetText[*puiAt - 1] != '\n')) ? "\n\n\n" : "\n\n");
		mpcContent->GetMoveNamespace()->AppendBlockLines(szNamespace, 0, "\t", 0, true, pszDest);
		mpcContent->GetMoveNamespace()->AppendReindented(szMoved.Text(), uiStrip, uiDepth, pszDest);
		mpcContent->GetMoveNamespace()->AppendBlockLines(szNamespace, 0, "\t", 0, false, pszDest);
	}
	sBlock.Kill();
	szMoved.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEMoveClass::PrintFunctionMove(SIDEFunctionMove* psMove, CChars* pszDest)
{
	size	i;

	pszDest->Append("Move to File: ")->Append(psMove->szKey);
	if (!psMove->bMovable)
	{
		pszDest->Append(" can't be moved: ")->Append(psMove->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	for (i = 0; i < psMove->asDeclarations.NumElements(); i++)
	{
		pszDest->Append(", declared in ")->Append(GetIDEFileNamePart(psMove->asDeclarations.Get(i)->szFileName.Text()));
	}
	if (psMove->bDefinition)
	{
		pszDest->Append(", defined in ")->Append(GetIDEFileNamePart(psMove->sDefinition.szFileName.Text()));
	}
	pszDest->Append(", ");
	pszDest->Append((uint32)psMove->aszUsers.NumElements());
	pszDest->Append((psMove->aszUsers.NumElements() == 1) ? " file names it\n" : " files name it\n");
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog says why the header written can't be where it goes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::ApproveFunctionMove(void)
{
	CChars	szProblem;
	CChars	szHeader;
	CChars	szSource;
	bool	bProblem;

	szProblem.Init();
	szHeader.Init();
	szSource.Init();
	bProblem = FindFunctionProblem(mpsFunctionMove, mpNameField->GetText(), &szHeader, &szSource, &szProblem);
	if (bProblem)
	{
		if ((szProblem.Text()[0] >= 'a') && (szProblem.Text()[0] <= 'z'))
		{
			szProblem.Text()[0] -= 'a' - 'A';
		}
		szProblem.Append('.');
	}
	mpError->SetText(szProblem.Text());
	szSource.Kill();
	szHeader.Kill();
	szProblem.Kill();
	return !bProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// A header as Move to File's field names it: its name, or its path if
// another of the solution's files has the name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AppendHeaderText(const char* szPath, CChars* pszDest)
{
	CArrayChars	aszFound;
	CChars		szText;

	aszFound.Init();
	if (mpcContent->GetCodeModel()->FindFilesNamed(GetIDEFileNamePart(szPath), &aszFound) > 1)
	{
		szText.Init(szPath);
		szText.Replace('\\', '/');
		pszDest->Append(szText);
		szText.Kill();
	}
	else
	{
		pszDest->Append(GetIDEFileNamePart(szPath));
	}
	aszFound.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Move to File's field was typed in: the solution's headers that what is
// in it could mean, as Go to File matches them, the best first, each
// with its folder.  None once it names one of them in full.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::HeaderTyped(CTextField* pcField)
{
	CIDECodeModel*	pcModel;
	CNameMatcher	cMatcher;
	SNameMatch		sMatch;
	CArrayChars		aszPaths;
	CArrayInt		aiScores;
	CArrayChars		aszTexts;
	CArrayChars		aszNotes;
	CFileUtil		cFileUtil;
	CChars			szName;
	CChars			szFolder;
	char*			szPath;
	size			uiBest;
	size			i;

	aszTexts.Init();
	aszNotes.Init();
	if (pcField->GetText()[0] != '\0')
	{
		pcModel = mpcContent->GetCodeModel();
		cMatcher.Init(pcField->GetText());
		aszPaths.Init();
		aiScores.Init();
		for (i = 0; i < pcModel->NumProjectFiles(); i++)
		{
			szPath = pcModel->GetProjectFile(i);
			if (IsIDEMoveHeader(szPath) && !aszPaths.Contains(szPath) && cMatcher.MatchPath(szPath, &sMatch))
			{
				aszPaths.Add(szPath);
				aiScores.Add((int)sMatch.iScore);
			}
		}
		while (aszTexts.NumElements() < IDE_MOVE_COMPLETIONS)
		{
			uiBest = MAX_SIZE;
			for (i = 0; i < aiScores.NumElements(); i++)
			{
				if ((aiScores.GetValue(i) != IDE_MOVE_COMPLETION_TAKEN) && ((uiBest == MAX_SIZE) || (aiScores.GetValue(i) > aiScores.GetValue(uiBest))))
				{
					uiBest = i;
				}
			}
			if (uiBest == MAX_SIZE)
			{
				break;
			}
			*aiScores.Get(uiBest) = IDE_MOVE_COMPLETION_TAKEN;
			szName.Init();
			AppendHeaderText(aszPaths.GetText(uiBest), &szName);
			aszTexts.Add(szName.Text());
			szName.Clear();
			szFolder.Init();
			cFileUtil.SplitPath(aszPaths.GetText(uiBest), &szName, &szFolder);
			aszNotes.Add(szFolder.Text());
			szFolder.Kill();
			szName.Kill();
		}
		aiScores.Kill();
		aszPaths.Kill();
		cMatcher.Kill();
		if ((aszTexts.NumElements() == 1) && aszTexts.Get(0)->EqualsIgnoreCase(pcField->GetText()))
		{
			aszTexts.ReInit();
			aszNotes.ReInit();
		}
	}
	mpHeaderField->SetCompletions(&aszTexts, &aszNotes);
	aszNotes.Kill();
	aszTexts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The button beside Move to File's field: the chooser of the solutions'
// files, on the header the field names (or the file in the editor), and
// the header chosen in it goes in the field.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::ChooseHeader(void)
{
	Ptr<CIDEEditorTab>	pTab;
	CArrayChars			aszFound;
	CChars				szSelected;
	CChars				szPath;
	CChars				szText;

	mpHeaderField->CloseCompletions();
	szSelected.Init();
	aszFound.Init();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if ((mpHeaderField->GetText()[0] != '\0') && (mpcContent->GetCodeModel()->FindFilesNamed(mpHeaderField->GetText(), &aszFound) == 1))
	{
		szSelected.Set(aszFound.GetText(0));
	}
	else if (pTab.IsNotNull())
	{
		szSelected.Set(pTab->GetFileName());
	}
	aszFound.Kill();
	szPath.Init();
	if (mpcContent->GetSolutionFileChooser()->Choose(mpDialog->GetWindow(), "Choose the Header", ".h;.hpp;.hh;.inl", szSelected.Text(), &szPath))
	{
		szText.Init();
		AppendHeaderText(szPath.Text(), &szText);
		mpHeaderField->SetText(szText.Text());
		mpHeaderField->SetCaretPosition(szText.Length());
		mpHeaderField->CloseCompletions();
		szText.Kill();
	}
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpHeaderField);
	szPath.Kill();
	szSelected.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Move to File's dialog (the header it goes to), then the move.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::ShowFunctionMove(SIDEFunctionMove* psMove)
{
	Ptr<CGridContainer>	pRow;
	CChars				szMessage;
	CChars				szName;
	CChars				szHeader;
	CChars				szSource;
	CChars				szProblem;
	bool				bResult;

	if (!psMove->bMovable)
	{
		szMessage.Init("Can't move ");
		szMessage.Append(psMove->szName);
		szMessage.Append(" to another file: ");
		szMessage.Append(psMove->szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		return false;
	}
	szMessage.Init("Move ");
	szMessage.Append(psMove->szName);
	szMessage.Append(" to File");
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	mpsFunctionMove = psMove;

	//The header's field, which completes, and the button beside it that opens the chooser.
	pRow = OMalloc<CGridContainer>(mpDialog->GetWindow());
	pRow->SetAlignment(CSH_Stretch, CSV_Center);
	mpHeaderField = OMalloc<CCompletionField>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, "");
	mpHeaderField->SetColumns(10);
	mpHeaderField->SetAlignment(CSH_Stretch, CSV_Center);
	pRow->AddComponent(mpHeaderField, 0, 0, 1, 1, 1, 0);
	mpChooseButton = OMalloc<CButton>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, "...");
	mpChooseButton->SetAlignment(CSH_Right, CSV_Center);
	pRow->AddComponent(mpChooseButton, 1, 0);
	mpDialog->AddEditor("To header:", pRow);
	mpDialog->SetFirstEditor(mpHeaderField);
	pRow = NULL;
	mpNameField = mpHeaderField;
	mpHeaderField->GetChangeEvent()->Add(this, &CIDEMoveClass::HeaderTyped);
	mpChooseButton->GetClickEvent()->Add(this, &CIDEMoveClass::ChooseHeader);

	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEMoveClass::ApproveFunctionMove);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(560);
	szName.Init(bResult ? mpNameField->GetText() : "");
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpHeaderField->GetChangeEvent()->Remove(this);
	mpChooseButton->GetClickEvent()->Remove(this);
	mpHeaderField = NULL;
	mpChooseButton = NULL;
	mpNameField = NULL;
	mpError = NULL;
	mpDialog = NULL;
	mpsFunctionMove = NULL;
	szHeader.Init();
	szSource.Init();
	szProblem.Init();
	bResult = bResult && !FindFunctionProblem(psMove, szName.Text(), &szHeader, &szSource, &szProblem) && MoveFunction(psMove, szHeader.Text(), szSource.Text());
	szProblem.Kill();
	szSource.Kill();
	szHeader.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A place added to places kept in order, once.
//
//////////////////////////////////////////////////////////////////////////
static void AddIDEMovedPlace(CArrayInt* paiPlaces, size uiPlace)
{
	size	j;

	for (j = 0; (j < paiPlaces->NumElements()) && ((size)paiPlaces->GetValue(j) < uiPlace); j++)
	{
	}
	if ((j == paiPlaces->NumElements()) || ((size)paiPlaces->GetValue(j) != uiPlace))
	{
		paiPlaces->InsertAt((int)uiPlace, j);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The places szName is a word in the comments of szText from uiStart to
// uiEnd (not in its strings or character literals: what a program says
// isn't changed), added to paiPlaces.
//
//////////////////////////////////////////////////////////////////////////
static void AddIDECommentPlaces(const char* szText, size uiStart, size uiEnd, const char* szName, CArrayInt* paiPlaces)
{
	SIDELineCounter				sLines;
	CArrayIDENameOccurrences	asFound;
	const char*					szClose;
	size						uiNameLength;
	size						i;
	size						j;
	char						cQuote;

	uiNameLength = strlen(szName);
	sLines.Init(szText);
	asFound.Init();
	for (i = uiStart; i < uiEnd; i++)
	{
		if ((szText[i] == '"') || (szText[i] == '\''))
		{
			cQuote = szText[i];
			for (i++; (i < uiEnd) && (szText[i] != cQuote) && (szText[i] != '\n'); i++)
			{
				i += ((szText[i] == '\\') && (i + 1 < uiEnd)) ? 1 : 0;
			}
		}
		else if ((szText[i] == '/') && (szText[i + 1] == '/'))
		{
			for (j = i; (j < uiEnd) && (szText[j] != '\n'); j++)
			{
			}
			FindIDEWords(szText, i, j, szName, uiNameLength, true, &sLines, &asFound);
			i = j;
		}
		else if ((szText[i] == '/') && (szText[i + 1] == '*'))
		{
			szClose = strstr(&szText[i + 2], "*/");
			j = szClose ? (size)(szClose - szText) + 2 : uiEnd;
			j = (j > uiEnd) ? uiEnd : j;
			FindIDEWords(szText, i, j, szName, uiNameLength, true, &sLines, &asFound);
			i = j - 1;
		}
	}
	for (i = 0; i < asFound.NumElements(); i++)
	{
		AddIDEMovedPlace(paiPlaces, asFound.Get(i)->uiStart);
	}
	asFound.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What is written at a range of a file as it goes to another
// (AppendBlock); for a copy (psTarget isn't NULL), with szNewName in
// place of the class's name at each place in it that Rename would change
// (psTarget's places): its own name, its constructors', its destructor's;
// and where its comments name the class (the copy's are about the new
// class).
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AppendMovedBlock(const char* szText, const char* szFileName, SCPPSyntaxRange* psRange, SIDERenameTarget* psTarget, const char* szOldName, const char* szNewName, CChars* pszDest)
{
	CArrayInt		aiPlaces;
	SIDELocation*	psLocation;
	size			uiStart;
	size			uiEnd;
	size			uiAt;
	size			uiPlace;
	size			uiOldLength;
	size			i;

	if (!psTarget)
	{
		AppendBlock(szText, psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, pszDest);
		return;
	}
	uiStart = FindIDECommentStart(szText, FindIDETextOffset(szText, psRange->iLine, psRange->iColumn));
	uiEnd = FindIDENextLine(szText, FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn));
	uiOldLength = strlen(szOldName);

	//The places in it, in order.
	aiPlaces.Init();
	for (i = 0; i < psTarget->asLocations.NumElements(); i++)
	{
		psLocation = psTarget->asLocations.Get(i);
		if (StringInsensitiveCompare(psLocation->szFileName.Text(), szFileName) != 0)
		{
			continue;
		}
		uiPlace = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
		if ((uiPlace >= uiStart) && (uiPlace + uiOldLength <= uiEnd) && (strncmp(&szText[uiPlace], szOldName, uiOldLength) == 0))
		{
			AddIDEMovedPlace(&aiPlaces, uiPlace);
		}
	}
	AddIDECommentPlaces(szText, uiStart, uiEnd, szOldName, &aiPlaces);
	uiAt = uiStart;
	for (i = 0; i < aiPlaces.NumElements(); i++)
	{
		uiPlace = (size)aiPlaces.GetValue(i);
		pszDest->AppendSubString(szText, uiAt, uiPlace);
		pszDest->Append(szNewName);
		uiAt = uiPlace + uiOldLength;
	}
	pszDest->AppendSubString(szText, uiAt, uiEnd);
	if (!pszDest->EndsWith("\n"))
	{
		pszDest->Append('\n');
	}
	aiPlaces.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Why a class can't be copied under a name: it isn't a name, it is the
// class's own, something has it already, or the files it would have are
// there.  *pszStem is the name its files get: the new name without its C.
// False if it can.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::FindCopyProblem(SIDEClassMove* psMove, const char* szNewName, CChars* pszStem, CChars* pszDest)
{
	CChars	szOwnName;
	CChars	szKey;
	size	i;
	bool	bName;

	bName = (szNewName[0] != '\0') && !((szNewName[0] >= '0') && (szNewName[0] <= '9'));
	for (i = 0; szNewName[i] != '\0'; i++)
	{
		bName = bName && IsIDENameChar(szNewName[i]);
	}
	if (!bName)
	{
		pszDest->Set("the copy's name is letters, digits and _");
		return true;
	}
	szOwnName.Init();
	GetIDESymbolName(psMove->szKey.Text(), &szOwnName);
	szKey.Init();
	szKey.AppendSubString(psMove->szKey.Text(), 0, psMove->szKey.Length() - szOwnName.Length());
	szKey.Append(szNewName);
	if (szOwnName.Equals(szNewName))
	{
		pszDest->Set("the copy has another name");
	}
	else if (mpcContent->GetCodeModel()->HasSymbol(szKey.Text()))
	{
		pszDest->Set("there is a ");
		pszDest->Append(szNewName);
		pszDest->Append(" already");
	}
	szKey.Kill();
	szOwnName.Kill();
	if (!pszDest->Empty())
	{
		return true;
	}
	pszStem->Set(&szNewName[((szNewName[0] == 'C') && (szNewName[1] >= 'A') && (szNewName[1] <= 'Z')) ? 1 : 0]);
	return FindProblem(psMove, pszStem->Text(), pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog says why the name written can't be the copy's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::ApproveCopy(void)
{
	CChars	szProblem;
	CChars	szStem;
	bool	bProblem;

	szProblem.Init();
	szStem.Init();
	bProblem = FindCopyProblem(mpsMove, mpNameField->GetText(), &szStem, &szProblem);
	if (bProblem)
	{
		if ((szProblem.Text()[0] >= 'a') && (szProblem.Text()[0] <= 'z'))
		{
			szProblem.Text()[0] -= 'a' - 'A';
		}
		szProblem.Append('.');
	}
	mpError->SetText(szProblem.Text());
	szStem.Kill();
	szProblem.Kill();
	return !bProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// Copy Class for the class at the caret: its dialog (the copy's name),
// then the copy.  False if nothing was copied.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::ShowCopy(void)
{
	SIDEClassMove	sMove;
	CChars			szMessage;
	CChars			szName;
	CChars			szStem;
	CChars			szProblem;
	bool			bResult;

	sMove.Init();
	if (!FindMove(&sMove, true))
	{
		mpcContent->ShowStatus("There is no class at the caret to copy.");
		sMove.Kill();
		return false;
	}
	if (!sMove.bMovable)
	{
		szMessage.Init("Can't copy ");
		szMessage.Append(sMove.szName);
		szMessage.Append(": ");
		szMessage.Append(sMove.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sMove.Kill();
		return false;
	}

	szMessage.Init("Copy ");
	szMessage.Append(sMove.szName);
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	mpsMove = &sMove;
	szName.Init();
	GetIDESymbolName(sMove.szKey.Text(), &szName);
	mpNameField = mpDialog->AddTextField("New name:", szName.Text());
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEMoveClass::ApproveCopy);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(560);
	szName.Set(bResult ? mpNameField->GetText() : "");
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpNameField = NULL;
	mpError = NULL;
	mpDialog = NULL;
	mpsMove = NULL;
	szStem.Init();
	szProblem.Init();
	bResult = bResult && !FindCopyProblem(&sMove, szName.Text(), &szStem, &szProblem) && Move(&sMove, szStem.Text(), NULL, szName.Text());
	szProblem.Kill();
	szStem.Kill();
	szName.Kill();
	sMove.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Move Class to Its Own Files for the class at the caret: its dialog (the
// files' name), then the move.  False if nothing was moved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::ShowMove(void)
{
	SIDEClassMove		sMove;
	SIDEFunctionMove	sFunctionMove;
	CChars				szMessage;
	CChars				szStem;
	bool				bResult;

	sMove.Init();
	if (!FindMove(&sMove))
	{
		//Not a class: a function or a variable that is in none?
		sMove.Kill();
		sFunctionMove.Init();
		bResult = FindFunctionMove(&sFunctionMove);
		if (bResult)
		{
			bResult = ShowFunctionMove(&sFunctionMove);
		}
		else
		{
			mpcContent->ShowStatus("There is no class or function at the caret to move.");
		}
		sFunctionMove.Kill();
		return bResult;
	}
	if (!sMove.bMovable)
	{
		szMessage.Init("Can't move ");
		szMessage.Append(sMove.szName);
		szMessage.Append(" to its own files: ");
		szMessage.Append(sMove.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sMove.Kill();
		return false;
	}

	szMessage.Init("Move ");
	szMessage.Append(sMove.szName);
	szMessage.Append(" to Its Own Files");
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	mpsMove = &sMove;
	mpNameField = mpDialog->AddTextField(sMove.szSource.Empty() ? "Header (.h):" : "Files (.h and .cpp):", sMove.szStem.Text());
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEMoveClass::ApproveMove);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(560);
	szStem.Init(bResult ? mpNameField->GetText() : "");
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpNameField = NULL;
	mpError = NULL;
	mpDialog = NULL;
	mpsMove = NULL;
	bResult = bResult && Move(&sMove, szStem.Text());
	szStem.Kill();
	sMove.Kill();
	return bResult;
}
