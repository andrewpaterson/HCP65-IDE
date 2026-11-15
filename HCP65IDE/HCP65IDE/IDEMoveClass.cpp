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
#include "IDEFileRename.h"
#include "IDESafeDelete.h"
#include "IDEMoveMembers.h"
#include "IDEMoveClass.h"


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
// A file's name as an include guard has it: its words in capitals with
// an underscore between them ("IDEMoveClass" is "IDE_MOVE_CLASS"), or,
// not bWords, only in capitals.
//
//////////////////////////////////////////////////////////////////////////
static void AppendIDEGuardName(const char* szStem, bool bWords, CChars* pszDest)
{
	size	i;
	char	c;
	bool	bUpper;
	bool	bPreviousLower;
	bool	bPreviousUpper;
	bool	bNextLower;

	for (i = 0; szStem[i] != '\0'; i++)
	{
		c = szStem[i];
		bUpper = (c >= 'A') && (c <= 'Z');
		bPreviousLower = (i > 0) && (((szStem[i - 1] >= 'a') && (szStem[i - 1] <= 'z')) || ((szStem[i - 1] >= '0') && (szStem[i - 1] <= '9')));
		bPreviousUpper = (i > 0) && (szStem[i - 1] >= 'A') && (szStem[i - 1] <= 'Z');
		bNextLower = (szStem[i + 1] >= 'a') && (szStem[i + 1] <= 'z');
		if (bWords && bUpper && (bPreviousLower || (bPreviousUpper && bNextLower)))
		{
			pszDest->Append('_');
		}
		pszDest->Append(((c >= 'a') && (c <= 'z')) ? (char)(c - 'a' + 'A') : (IsIDENameChar(c) ? c : '_'));
	}
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
	M_Pointer(mpError);
	U_Pointer(mpsMove);
	U_Unknown(CArrayTemplatePtr<CIDERefactorUnit>, mapcUnits);
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
	pCommands->Add("Refactor.MoveClass", "Move Class to Its Own Files...", "F6", Ptr<CImageCel>(), this, &CIDEMoveClass::QueueMove)->SetEnabledCheck(this, &CIDEMoveClass::CanMove);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MoveClass"));
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
bool CIDEMoveClass::FindMove(SIDEClassMove* psDest)
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

	//It shares its header with something.
	if (psDest->bMovable)
	{
		asRanges.Init();
		asRanges.Add()->Init(psDest->szHeader.Text(), 0, 0, IDE_MOVE_CLASS_END_LINE, 0);
		aszKeys.Init();
		pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
		//Another class, enum, typedef, function or variable (not what is in one: a parameter, an enumerator).
		uiOthers = 0;
		for (i = 0; i < aszKeys.NumElements(); i++)
		{
			if (aszKeys.Get(i)->Equals(psDest->szKey.Text()) || aszKeys.Get(i)->StartsWith(szPrefix.Text()))
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
		if (psDest->bHeaderUses && psDest->bNeedsHeader)
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
// The include guard of a new header, named as the header it comes from
// names its own: that one's with the new file's name where the old file's
// is ("__TANK_H__" gives "__DRUM_H__"), or, if it has none of that shape,
// "__NAME_H__".  *pbPragma if the old header has #pragma once and no
// guard, and *pbComment if its #endif says the guard's name after it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveClass::AppendGuard(const char* szText, const char* szOldPath, const char* szStem, CChars* pszGuard, bool* pbPragma, bool* pbComment)
{
	CChars		szOld;
	CChars		szOldStem;
	CChars		szOldName;
	CChars		szNewName;
	const char*	szIfndef;
	const char*	szEndif;
	size		uiAt;
	size		i;
	int			iForm;

	*pbPragma = false;
	*pbComment = true;
	szOld.Init();
	uiAt = FindOpeningEnd(szText);
	for (; (szText[uiAt] == ' ') || (szText[uiAt] == '\t') || (szText[uiAt] == '\r') || (szText[uiAt] == '\n'); uiAt++)
	{
	}
	szIfndef = (strncmp(&szText[uiAt], "#ifndef", 7) == 0) ? &szText[uiAt + 7] : NULL;
	if (szIfndef)
	{
		for (; (*szIfndef == ' ') || (*szIfndef == '\t'); szIfndef++)
		{
		}
		for (i = 0; IsIDENameChar(szIfndef[i]); i++)
		{
			szOld.Append(szIfndef[i]);
		}
		szEndif = NULL;
		for (szIfndef = strstr(szText, "#endif"); szIfndef; szIfndef = strstr(szIfndef + 1, "#endif"))
		{
			szEndif = szIfndef;
		}
		*pbComment = szEndif && (strstr(szEndif, "//") != NULL);
	}
	else if (strstr(szText, "#pragma once"))
	{
		*pbPragma = true;
		szOld.Kill();
		return;
	}

	szOldStem.Init(GetIDEFileNamePart(szOldPath));
	for (i = 0; (i < szOldStem.Length()) && (szOldStem.Text()[i] != '.'); i++)
	{
	}
	szOldStem.SetLength(i);
	for (iForm = 0; pszGuard->Empty() && (iForm < 2) && !szOld.Empty(); iForm++)
	{
		szOldName.Init();
		szNewName.Init();
		AppendIDEGuardName(szOldStem.Text(), iForm == 0, &szOldName);
		AppendIDEGuardName(szStem, iForm == 0, &szNewName);
		if (szOld.Contains(szOldName.Text()))
		{
			pszGuard->Set(szOld);
			pszGuard->Replace(szOldName.Text(), szNewName.Text());
		}
		szNewName.Kill();
		szOldName.Kill();
	}
	if (pszGuard->Empty())
	{
		pszGuard->Append("__");
		AppendIDEGuardName(szStem, true, pszGuard);
		pszGuard->Append("_H__");
	}
	szOldStem.Kill();
	szOld.Kill();
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
// that list that one.  False if it can't be made.
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
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Each CMakeLists.txt in a project's directory, or one or two above it,
// that has a line naming the file at szBeside gets that line again after
// it with szNewName in the name's place.
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
bool CIDEMoveClass::Move(SIDEClassMove* psMove, const char* szStem, CChars* pszStatus)
{
	CIDECodeModel*		pcModel;
	CIDEFileEdits		cEdits;
	CArrayIDEFileRanges	asTaken;
	SIDEFileRange*		psRange;
	SIDEFileRange*		psOther;
	CChars				szNewHeader;
	CChars				szNewSource;
	CChars				szHeaderText;
	CChars				szSourceText;
	CChars				szGuard;
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
	bool				bPragma;
	bool				bComment;
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

	//The new files, empty, in the projects.
	bResult = bResult && CreateFile(szNewHeader.Text(), psMove->szHeader.Text());
	bResult = bResult && (szNewSource.Empty() || CreateFile(szNewSource.Text(), psMove->szSource.Text()));
	uiIncludes = 0;
	uiOutside = 0;
	asTaken.Init();
	if (bResult)
	{
		//The new header: the old one's opening comment, a guard, its #includes, the class, and what it defines
		//of the class's outside it.
		szHeaderText.Init();
		szGuard.Init();
		AppendGuard(szText, psMove->szHeader.Text(), szStem, &szGuard, &bPragma, &bComment);
		szHeaderText.AppendSubString(szText, 0, FindOpeningEnd(szText));
		if (bPragma)
		{
			szHeaderText.Append("#pragma once\n");
		}
		else
		{
			szHeaderText.Append("#ifndef ")->Append(szGuard)->Append("\n#define ")->Append(szGuard)->Append('\n');
		}
		AppendIncludes(szText, &szHeaderText);
		if (psMove->bNeedsHeader)
		{
			szHeaderText.Append("#include \"")->Append(GetIDEFileNamePart(psMove->szHeader.Text()))->Append("\"\n");
		}
		szHeaderText.Append("\n\n");
		AppendBlock(szText, psMove->sClass.iLine, psMove->sClass.iColumn, psMove->sClass.iEndLine, psMove->sClass.iEndColumn, &szHeaderText);

		szSourceText.Init();
		if (szOldSource)
		{
			szSourceText.AppendSubString(szOldSource, 0, FindOpeningEnd(szOldSource));
			szSourceText.Append("#include \"")->Append(szNewName)->Append("\"\n");
			AppendIncludes(szOldSource, &szSourceText);
		}
		for (i = 0; i < psMove->asOutside.NumElements(); i++)
		{
			psRange = psMove->asOutside.Get(i);
			if (StringInsensitiveCompare(psRange->szFileName.Text(), psMove->szHeader.Text()) == 0)
			{
				szHeaderText.Append("\n\n");
				AppendBlock(szText, psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &szHeaderText);
				uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
				uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
				ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
				asTaken.Add()->Init(psMove->szHeader.Text(), 0, 0, 0, 0);
				asTaken.Get(asTaken.NumElements() - 1)->uiStart = uiStart;
				asTaken.Get(asTaken.NumElements() - 1)->uiEnd = uiEnd;
			}
			else
			{
				szSourceText.Append("\n\n");
				AppendBlock(szOldSource, psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &szSourceText);
				uiStart = FindIDETextOffset(szOldSource, psRange->iLine, psRange->iColumn);
				uiEnd = FindIDETextOffset(szOldSource, psRange->iEndLine, psRange->iEndColumn);
				ExtendIDEDeletion(szOldSource, &uiStart, &uiEnd, true);
				asTaken.Add()->Init(psMove->szSource.Text(), 0, 0, 0, 0);
				asTaken.Get(asTaken.NumElements() - 1)->uiStart = uiStart;
				asTaken.Get(asTaken.NumElements() - 1)->uiEnd = uiEnd;
			}
			uiOutside++;
		}
		if (!bPragma)
		{
			szHeaderText.Append("\n\n#endif");
			if (bComment)
			{
				szHeaderText.Append(" // ")->Append(szGuard);
			}
			szHeaderText.Append('\n');
		}
		szGuard.Kill();

		//The class out of the old header.
		uiStart = FindIDETextOffset(szText, psMove->sClass.iLine, psMove->sClass.iColumn);
		uiEnd = FindIDETextOffset(szText, psMove->sClass.iEndLine, psMove->sClass.iEndColumn);
		ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
		asTaken.Add()->Init(psMove->szHeader.Text(), 0, 0, 0, 0);
		asTaken.Get(asTaken.NumElements() - 1)->uiStart = uiStart;
		asTaken.Get(asTaken.NumElements() - 1)->uiEnd = uiEnd;

		//What is taken out: pieces that touch or overlap (each takes the blank lines around it) are one piece, and
		//one a file ends with takes the blank lines before it.
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

		//The new header #included where the class is needed.
		szLine.Init();
		if (psMove->bHeaderUses)
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
		bResult = (szUser != NULL);
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
	if (bResult)
	{
		cEdits.Apply();
		mpcContent->GetDocuments()->SaveAllFiles();
		mpcContent->GetCodeModel()->Reset();
		mpcContent->GetSolutions()->WorkspaceChanged(true);

		szMessage.Init("Moved ");
		szMessage.Append(psMove->szName);
		szMessage.Append(" to ");
		szMessage.Append(szNewName);
		if (!szNewSource.Empty())
		{
			szMessage.Append(" and ");
			szMessage.Append(GetIDEFileNamePart(szNewSource.Text()));
		}
		szMessage.Append(": ");
		szMessage.Append((uint32)uiOutside);
		szMessage.Append((uiOutside == 1) ? " definition outside it, " : " definitions outside it, ");
		szMessage.Append((uint32)uiIncludes);
		szMessage.Append((uiIncludes == 1) ? " #include added." : " #includes added.");
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
// Move Class to Its Own Files for the class at the caret: its dialog (the
// files' name), then the move.  False if nothing was moved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveClass::ShowMove(void)
{
	SIDEClassMove	sMove;
	CChars			szMessage;
	CChars			szStem;
	bool			bResult;

	sMove.Init();
	if (!FindMove(&sMove))
	{
		mpcContent->ShowStatus("There is no class at the caret to move to its own files.");
		sMove.Kill();
		return false;
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
