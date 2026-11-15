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
#include "CppCompilerLib/CPPSyntaxMemberExpression.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/ComboBox.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDEFileRename.h"
#include "IDESafeDelete.h"
#include "IDEIntroduceVariable.h"
#include "IDEMoveMembers.h"


static const char*	gaszIDEAccess[] = { "public", "protected", "private" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMoveClass::Init(void)
{
	szKey.Init();
	szFileName.Init();
	memset(&sRange, 0, sizeof(SCPPSyntaxRange));
	bTemplate = false;
	bStruct = false;
	bChosen = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMoveClass::Kill(void)
{
	szFileName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMovedMember::Init(void)
{
	szKey.Init();
	szName.Init();
	szOwnName.Init();
	bFunction = false;
	bStatic = false;
	iAccess = IDE_ACCESS_PUBLIC;
	memset(&sDeclared, 0, sizeof(SCPPSyntaxRange));
	sDeclaration.Init();
	bOutside = false;
	szOutsideFile.Init();
	memset(&sOutside, 0, sizeof(SCPPSyntaxRange));
	memset(&sOutsideName, 0, sizeof(SCPPSyntaxRange));
	sDefinition.Init();
	szWhyNot.Init();
	bChosen = false;
	bAbstract = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMovedMember::Kill(void)
{
	szWhyNot.Kill();
	sDefinition.Kill();
	szOutsideFile.Kill();
	sDeclaration.Kill();
	szOwnName.Kill();
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMembersMove::Init(void)
{
	bUp = true;
	bMovable = false;
	szWhyNot.Init();
	sClass.Init();
	asTargets.Init();
	uiTarget = 0;
	apsMembers.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMembersMove::Kill(void)
{
	size	i;

	for (i = 0; i < apsMembers.NumElements(); i++)
	{
		apsMembers.GetPtr(i)->Kill();
		free(apsMembers.GetPtr(i));
	}
	apsMembers.Kill();
	for (i = 0; i < asTargets.NumElements(); i++)
	{
		asTargets.Get(i)->Kill();
	}
	asTargets.Kill();
	sClass.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Where the comment over what is at uiAt starts: the start of the first
// of the // lines directly above the line it is on; that line's start if
// there are none.
//
//////////////////////////////////////////////////////////////////////////
size FindIDECommentStart(const char* szText, size uiAt)
{
	size	uiStart;
	size	uiPrevious;
	size	i;

	for (uiStart = uiAt; (uiStart > 0) && (szText[uiStart - 1] != '\n'); uiStart--)
	{
	}
	while (uiStart > 0)
	{
		for (uiPrevious = uiStart - 1; (uiPrevious > 0) && (szText[uiPrevious - 1] != '\n'); uiPrevious--)
		{
		}
		for (i = uiPrevious; (szText[i] == ' ') || (szText[i] == '\t'); i++)
		{
		}
		if ((szText[i] != '/') || (szText[i + 1] != '/'))
		{
			break;
		}
		uiStart = uiPrevious;
	}
	return uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
// The start of the line after the one uiAt is on (the text's end if it
// is the last).
//
//////////////////////////////////////////////////////////////////////////
static size FindIDELineEnd(const char* szText, size uiAt)
{
	for (; (szText[uiAt] != '\0') && (szText[uiAt] != '\n'); uiAt++)
	{
	}
	return (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// The column the names of a class's declarations are lined up in, by the
// line above uiAt: the column its last run of tabs ends in, where that
// is after something (a type).  0 if the line isn't such a declaration.
//
//////////////////////////////////////////////////////////////////////////
size FindIDENameColumn(const char* szText, size uiAt)
{
	size	uiLineStart;
	size	uiRunEnd;
	size	i;

	if (uiAt == 0)
	{
		return 0;
	}
	for (uiLineStart = uiAt - 1; (uiLineStart > 0) && (szText[uiLineStart - 1] != '\n'); uiLineStart--)
	{
	}
	for (i = uiLineStart; (szText[i] == ' ') || (szText[i] == '\t'); i++)
	{
	}
	if ((szText[i] == '/') || (szText[i] == '{') || (szText[i] == '}'))
	{
		return 0;
	}
	uiRunEnd = 0;
	for (; i + 1 < uiAt; i++)
	{
		if ((szText[i] == '\t') && (szText[i + 1] != '\t') && (szText[i + 1] != '\n') && (szText[i + 1] != '\r'))
		{
			uiRunEnd = i + 1;
		}
		if ((szText[i] == '(') || (szText[i] == ';') || (szText[i] == '='))
		{
			break;
		}
	}
	return (uiRunEnd > 0) ? FindIDEVisualColumn(szText, uiLineStart, uiRunEnd, 4) : 0;
}


//////////////////////////////////////////////////////////////////////////
//
// A declaration's lines (comment lines, then the declaration) with its
// name put in a column: the run of tabs before its name made to end
// there, where its name is after tabs at all and the column is far
// enough along.
//
//////////////////////////////////////////////////////////////////////////
void RealignIDEDeclaration(CChars* pszLines, size uiNameColumn)
{
	CChars		szNew;
	const char*	szText;
	size		uiLineStart;
	size		uiRunStart;
	size		uiRunEnd;
	size		uiColumn;
	size		i;

	if (uiNameColumn == 0)
	{
		return;
	}
	szText = pszLines->Text();
	uiLineStart = 0;
	for (;;)
	{
		for (i = uiLineStart; (szText[i] == ' ') || (szText[i] == '\t'); i++)
		{
		}
		if ((szText[i] != '/') || (szText[i + 1] != '/'))
		{
			break;
		}
		for (; (szText[i] != '\0') && (szText[i] != '\n'); i++)
		{
		}
		if (szText[i] == '\0')
		{
			return;
		}
		uiLineStart = i + 1;
	}
	uiRunStart = 0;
	uiRunEnd = 0;
	for (; (szText[i] != '\0') && (szText[i] != '\n') && (szText[i] != '(') && (szText[i] != ';') && (szText[i] != '='); i++)
	{
		if ((szText[i] == '\t') && (szText[i + 1] != '\t'))
		{
			uiRunEnd = i + 1;
			for (uiRunStart = i; szText[uiRunStart - 1] == '\t'; uiRunStart--)
			{
			}
		}
	}
	if (uiRunEnd == 0)
	{
		return;
	}
	uiColumn = FindIDEVisualColumn(szText, uiLineStart, uiRunStart, 4);
	szNew.Init();
	szNew.AppendSubString(szText, 0, uiRunStart);
	if (uiNameColumn > uiColumn)
	{
		AppendIDETabsTo(&szNew, uiColumn, uiNameColumn, 4);
	}
	else
	{
		szNew.Append('\t');
	}
	szNew.Append(&szText[uiRunEnd]);
	pszLines->Set(szNew.Text());
	szNew.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Text to go into a file at a place, gathered: what goes to one place (in
// a class, with one access) is one insertion, szBefore written before the
// first of it.
//
//////////////////////////////////////////////////////////////////////////
struct SIDEMoveInsert
{
	CChars	szFileName;
	size	uiAt;
	int32	iAccess;
	CChars	szText;
};


typedef CArrayTemplate<SIDEMoveInsert>	CArrayIDEMoveInserts;


static CChars* AddIDEMoveInsert(CArrayIDEMoveInserts* pasInserts, const char* szFileName, size uiAt, int32 iAccess, const char* szBefore)
{
	SIDEMoveInsert*	psInsert;
	size			i;

	for (i = 0; i < pasInserts->NumElements(); i++)
	{
		psInsert = pasInserts->Get(i);
		if ((psInsert->uiAt == uiAt) && (psInsert->iAccess == iAccess) && (StringInsensitiveCompare(psInsert->szFileName.Text(), szFileName) == 0))
		{
			return &psInsert->szText;
		}
	}
	psInsert = pasInserts->Add();
	psInsert->szFileName.Init(szFileName);
	psInsert->uiAt = uiAt;
	psInsert->iAccess = iAccess;
	psInsert->szText.Init(szBefore);
	return &psInsert->szText;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mapcUnits.Init();
	mapcChecked.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	U_Unknown(CArrayTemplatePtr<CIDERefactorUnit>, mapcUnits);
	U_Unknown(CArrayTemplatePtr<CIDERefactorUnit>, mapcChecked);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::Free(void)
{
	KillUnits();
	mapcChecked.Kill();
	mapcUnits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::Save(CObjectWriter* pcFile) { return false; }
bool CIDEMoveMembers::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ gives neither a key.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.PullMembersUp", "Pull Members Up...", "", Ptr<CImageCel>(), this, &CIDEMoveMembers::QueuePullUp)->SetEnabledCheck(this, &CIDEMoveMembers::CanMove);
	pCommands->Add("Refactor.PushMembersDown", "Push Members Down...", "", Ptr<CImageCel>(), this, &CIDEMoveMembers::QueuePushDown)->SetEnabledCheck(this, &CIDEMoveMembers::CanMove);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.PullMembersUp"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.PushMembersDown"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::CanMove(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


//////////////////////////////////////////////////////////////////////////
//
// A menu's item is clicked while its popup is open: the dialog is shown
// once the popup has gone.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::QueuePullUp(void) { mpcContent->GetWindow()->Post(this, &CIDEMoveMembers::ShowQueuedPullUp); }
void CIDEMoveMembers::QueuePushDown(void) { mpcContent->GetWindow()->Post(this, &CIDEMoveMembers::ShowQueuedPushDown); }
void CIDEMoveMembers::ShowQueuedPullUp(void) { ShowMove(true); }
void CIDEMoveMembers::ShowQueuedPushDown(void) { ShowMove(false); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDEMoveMembers::GetUnit(const char* szFileName)
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
void CIDEMoveMembers::KillUnits(void)
{
	size	i;

	for (i = 0; i < mapcUnits.NumElements(); i++)
	{
		mapcUnits.GetPtr(i)->Kill();
		free(mapcUnits.GetPtr(i));
	}
	mapcUnits.ReInit();
	for (i = 0; i < mapcChecked.NumElements(); i++)
	{
		mapcChecked.GetPtr(i)->Kill();
		free(mapcChecked.GetPtr(i));
	}
	mapcChecked.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The file parsed and checked, so its expressions have types.  NULL if
// it can't be.
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDEMoveMembers::GetCheckedUnit(const char* szFileName)
{
	CIDERefactorUnit*	pcUnit;
	size				i;

	for (i = 0; i < mapcChecked.NumElements(); i++)
	{
		pcUnit = mapcChecked.GetPtr(i);
		if (StringInsensitiveCompare(pcUnit->GetFileName(), szFileName) == 0)
		{
			return pcUnit->IsChecked() ? pcUnit : NULL;
		}
	}
	pcUnit = NewMalloc<CIDERefactorUnit>();
	pcUnit->Init();
	mapcChecked.Add(pcUnit);
	return (pcUnit->Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), szFileName, true) && pcUnit->IsChecked()) ? pcUnit : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a member is named at psLocation through an object of one of
// paszClasses: "cDrum.Seal()", "pcDrum->Seal()" where the object is a
// CDrum, by the type the compiler gives the object.  False if it is
// named some other way, or the type can't be had.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::IsUsedThrough(SIDELocation* psLocation, CArrayChars* paszClasses)
{
	CIDERefactorUnit*				pcUnit;
	CCPPSyntaxMemberExpression*		pcMember;
	SCPP65Type*						psType;
	CChars							szType;
	bool							bThrough;

	pcUnit = GetCheckedUnit(psLocation->szFileName.Text());
	pcMember = pcUnit ? (CCPPSyntaxMemberExpression*)pcUnit->FindAncestor(pcUnit->FindNodeAt(psLocation->iLine, psLocation->iColumn), CPPS_MemberExpression) : NULL;
	psType = (pcMember && pcMember->mpcObject) ? pcUnit->GetPass()->GetType(pcMember->mpcObject) : NULL;
	if (psType && pcMember->mbArrow && psType->IsPointer())
	{
		psType = psType->psBase;
	}
	if (!psType)
	{
		return false;
	}
	szType.Init();
	bThrough = pcUnit->GetPass()->AppendType(psType->Unqualified(), &szType);
	szType.Replace("&", "");
	szType.StripWhitespace();
	bThrough = bThrough && paszClasses->Contains(szType.Text());
	szType.Kill();
	return bThrough;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the class szKey is defined, in a file of the solution.  False if
// it isn't.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::ReadClass(const char* szKey, SIDEMoveClass* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CIDERefactorUnit*	pcUnit;
	CCPPSyntax*			pcClass;
	int32				iFirst;
	int32				iLast;
	size				i;
	uint8				uiKind;
	bool				bFound;

	pcModel = mpcContent->GetCodeModel();
	asLocations.Init();
	bFound = false;
	if (pcModel->GetSymbolLocations(szKey, &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
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
			if (pcClass && pcUnit->GetRange(pcClass, &psDest->sRange) &&
				pcUnit->FindTokensIn(psDest->sRange.iLine, psDest->sRange.iColumn, psDest->sRange.iEndLine, psDest->sRange.iEndColumn, &iFirst, &iLast))
			{
				psDest->szKey.Set(szKey);
				psDest->szFileName.Set(psLocation->szFileName);
				psDest->bTemplate = (uiKind == CPPSK_ClassTemplate);
				psDest->bStruct = pcUnit->IsToken(iFirst, "struct");
				bFound = true;
			}
		}
	}
	KillIDELocations(&asLocations);
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// A class's access parts, by its tokens.  The access in force at the
// token iAtToken (*piAccessAt, if asked); the token that ends the last
// part with iAccess (the next part's public, or the closing brace; -1 if
// the class has no such part); and its closing brace's token.  False if
// the class's tokens can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::ScanClass(SIDEMoveClass* psClass, int32 iAtToken, int32 iAccess, int32* piAccessAt, int32* piBoundaryToken, int32* piCloseToken)
{
	CIDERefactorUnit*	pcUnit;
	int32				iFirst;
	int32				iLast;
	int32				iOpen;
	int32				iClose;
	int32				iBoundary;
	int32				iCurrent;
	int32				iPartStart;
	int32				iDepth;
	int32				i;
	int32				j;

	pcUnit = GetUnit(psClass->szFileName.Text());
	if (!pcUnit || !pcUnit->FindTokensIn(psClass->sRange.iLine, psClass->sRange.iColumn, psClass->sRange.iEndLine, psClass->sRange.iEndColumn, &iFirst, &iLast))
	{
		return false;
	}
	for (iOpen = iFirst; (iOpen <= iLast) && !pcUnit->IsToken(iOpen, "{"); iOpen++)
	{
	}
	iCurrent = psClass->bStruct ? IDE_ACCESS_PUBLIC : IDE_ACCESS_PRIVATE;
	iPartStart = iOpen + 1;
	iBoundary = -1;
	iClose = iLast;
	iDepth = 1;
	for (i = iOpen + 1; i <= iLast; i++)
	{
		if (piAccessAt && (i == iAtToken))
		{
			*piAccessAt = iCurrent;
		}
		if (pcUnit->IsToken(i, "{"))
		{
			iDepth++;
		}
		else if (pcUnit->IsToken(i, "}"))
		{
			iDepth--;
			if (iDepth == 0)
			{
				iClose = i;
				break;
			}
		}
		else if ((iDepth == 1) && pcUnit->IsToken(i + 1, ":"))
		{
			for (j = 0; j < 3; j++)
			{
				if (pcUnit->IsToken(i, gaszIDEAccess[j]))
				{
					//A part with nothing in it (a class's private part before its first public:) isn't one.
					if ((iCurrent == iAccess) && (i > iPartStart))
					{
						iBoundary = i;
					}
					iCurrent = j;
					iPartStart = i + 2;
				}
			}
		}
	}
	if ((iCurrent == iAccess) && ((iClose > iPartStart) || (iPartStart > iOpen + 1)))
	{
		iBoundary = iClose;
	}
	if (piBoundaryToken)
	{
		*piBoundaryToken = iBoundary;
	}
	if (piCloseToken)
	{
		*piCloseToken = iClose;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a member with iAccess goes in a class: the end of the last part
// of the class with that access, above the blank lines that end it.  If
// the class has no such part, above its closing brace, and *pszBefore is
// the "public:" line to write first.  szText is the class's file's text
// and *puiAt is in it.  False if the class can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::FindInsertPlace(SIDEMoveClass* psClass, const char* szText, int32 iAccess, size* puiAt, CChars* pszBefore, bool bLastPart)
{
	CIDERefactorUnit*	pcUnit;
	SCPPSyntaxRange		sRange;
	int32				iBoundary;
	int32				iClose;
	size				uiAt;
	size				uiPrevious;
	size				i;
	bool				bBlank;

	pcUnit = GetUnit(psClass->szFileName.Text());
	if (!pcUnit || !ScanClass(psClass, -1, iAccess, NULL, &iBoundary, &iClose))
	{
		return false;
	}
	if (bLastPart && (iBoundary != iClose))
	{
		//Only the class's last part will do (a function's, after the parts its fields and public functions are in):
		//a new part is made.
		iBoundary = -1;
	}
	if (!pcUnit->GetTokensRange((iBoundary >= 0) ? iBoundary : iClose, (iBoundary >= 0) ? iBoundary : iClose, &sRange))
	{
		return false;
	}
	uiAt = FindIDETextOffset(szText, sRange.iLine, 0);
	if (iBoundary < 0)
	{
		pszBefore->Append('\n');
		pszBefore->AppendSubString(szText, uiAt, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn));
		pszBefore->Append(gaszIDEAccess[iAccess]);
		pszBefore->Append(":\n");
	}
	else
	{
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
	}
	*puiAt = uiAt;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A member's declaration (its line, "\tint\tmiCount;\n") added to a
// class: at the end of its part with iAccess (IDE_ACCESS_), or, for -1,
// where its fields go: after the last field it has, or, if it has none,
// at the end of its protected part (a struct's public).  It is lined up
// with the declaration above it, as an edit of the class's file.
// Introduce Field adds its field so.  False if the class isn't defined in
// the solution.  bLastPart: only the class's last part will do, and one
// is made if that hasn't iAccess (a function's own part, after the parts
// its fields and its public functions are in).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::AddMember(const char* szClassKey, int32 iAccess, const char* szDeclaration, CIDEFileEdits* pcEdits, bool bLastPart)
{
	CIDECodeModel*		pcModel;
	SIDEMoveClass		sClass;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	CArrayChars			aszKeys;
	CArrayChars			aszLines;
	CChars				szScope;
	CChars				szBefore;
	CChars				szLine;
	char*				szText;
	size				uiAt;
	size				i;
	size				j;
	int32				iLastLine;
	int32				iLastColumn;
	uint8				uiKind;
	bool				bField;
	bool				bResult;

	sClass.Init();
	szBefore.Init();
	bResult = ReadClass(szClassKey, &sClass);
	szText = bResult ? pcEdits->GetText(sClass.szFileName.Text()) : NULL;
	bField = false;
	if (szText && (iAccess < 0))
	{
		//The class's last field.
		pcModel = mpcContent->GetCodeModel();
		asRanges.Init();
		asRanges.Add()->Init(sClass.szFileName.Text(), sClass.sRange.iLine, sClass.sRange.iColumn, sClass.sRange.iEndLine, sClass.sRange.iEndColumn);
		aszKeys.Init();
		pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
		iLastLine = -1;
		iLastColumn = 0;
		for (i = 0; i < aszKeys.NumElements(); i++)
		{
			szScope.Init();
			GetIDESymbolScope(aszKeys.GetText(i), &szScope);
			asLocations.Init();
			if (szScope.Equals(szClassKey) && pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind) && (uiKind == CPPSK_Field))
			{
				for (j = 0; j < asLocations.NumElements(); j++)
				{
					if ((asLocations.Get(j)->uiRole != CPPIR_Reference) && (asLocations.Get(j)->iLine > iLastLine) && (asLocations.Get(j)->iLine >= sClass.sRange.iLine) &&
						(asLocations.Get(j)->iLine <= sClass.sRange.iEndLine) && (StringInsensitiveCompare(asLocations.Get(j)->szFileName.Text(), sClass.szFileName.Text()) == 0))
					{
						iLastLine = asLocations.Get(j)->iLine;
						iLastColumn = asLocations.Get(j)->iColumn;
					}
				}
			}
			KillIDELocations(&asLocations);
			szScope.Kill();
		}
		aszKeys.Kill();
		KillIDEFileRanges(&asRanges);
		if (iLastLine >= 0)
		{
			for (uiAt = FindIDETextOffset(szText, iLastLine, iLastColumn); (szText[uiAt] != '\0') && (szText[uiAt] != ';'); uiAt++)
			{
			}
			uiAt = FindIDELineEnd(szText, uiAt);
			bField = true;
		}
		iAccess = sClass.bStruct ? IDE_ACCESS_PUBLIC : IDE_ACCESS_PROTECTED;
	}
	bResult = szText && (bField || FindInsertPlace(&sClass, szText, iAccess, &uiAt, &szBefore, bLastPart));
	if (bResult)
	{
		//Each of its lines (it may be several declarations) lined up with the declaration above where it goes.
		aszLines.Init();
		szLine.Init(szDeclaration);
		szLine.Split(&aszLines, '\n');
		szLine.Kill();
		for (i = 0; i < aszLines.NumElements(); i++)
		{
			if (!aszLines.Get(i)->Empty())
			{
				szLine.Init(aszLines.GetText(i));
				szLine.Append('\n');
				RealignIDEDeclaration(&szLine, FindIDENameColumn(szText, uiAt));
				szBefore.Append(szLine);
				szLine.Kill();
			}
		}
		aszLines.Kill();
		pcEdits->GetEdits(sClass.szFileName.Text())->Insert(uiAt, szBefore.Text());
	}
	szBefore.Kill();
	sClass.Kill();
	KillUnits();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The access a class gives one of its members (IDE_ACCESS_), by where the
// class declares it; public if the class or the member isn't found (so
// nothing is said against it).
//
//////////////////////////////////////////////////////////////////////////
int32 CIDEMoveMembers::FindMemberAccess(const char* szMemberKey)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDEMoveClass		sClass;
	SIDELocation*		psLocation;
	CIDERefactorUnit*	pcUnit;
	CChars				szScope;
	size				i;
	int32				iAccess;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	iAccess = IDE_ACCESS_PUBLIC;
	szScope.Init();
	GetIDESymbolScope(szMemberKey, &szScope);
	sClass.Init();
	asLocations.Init();
	if (!szScope.Empty() && ReadClass(szScope.Text(), &sClass) && pcModel->GetSymbolLocations(szMemberKey, &asLocations, &uiKind))
	{
		pcUnit = GetUnit(sClass.szFileName.Text());
		for (i = 0; pcUnit && (i < asLocations.NumElements()); i++)
		{
			psLocation = asLocations.Get(i);
			if ((psLocation->uiRole != CPPIR_Reference) && (StringInsensitiveCompare(psLocation->szFileName.Text(), sClass.szFileName.Text()) == 0) &&
				(psLocation->iLine >= sClass.sRange.iLine) && (psLocation->iLine <= sClass.sRange.iEndLine))
			{
				ScanClass(&sClass, pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn), -1, &iAccess, NULL, NULL);
				break;
			}
		}
	}
	KillIDELocations(&asLocations);
	sClass.Kill();
	szScope.Kill();
	KillUnits();
	return iAccess;
}


//////////////////////////////////////////////////////////////////////////
//
// A member of the move's class, read from where the class declares it
// and where it is defined outside the class.  False if szKey isn't a
// function or a field of it that can be moved at all (a constructor, the
// destructor).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::ReadMember(SIDEMembersMove* psMove, const char* szKey, SIDEMovedMember* psDest)
{
	CIDECodeModel*			pcModel;
	CArrayIDELocations		asLocations;
	SIDELocation*			psLocation;
	CIDERefactorUnit*		pcUnit;
	CCPPSyntaxDeclarator*	pcDeclarator;
	CCPPSyntax*				pcOuter;
	CChars					szClassName;
	const char*				szWhyNot;
	int32					iFirst;
	int32					iLast;
	size					i;
	uint8					uiKind;
	bool					bInClass;
	bool					bDeclared;

	pcModel = mpcContent->GetCodeModel();
	asLocations.Init();
	if (!pcModel->GetSymbolLocations(szKey, &asLocations, &uiKind) || ((uiKind != CPPSK_Function) && (uiKind != CPPSK_FunctionTemplate) && (uiKind != CPPSK_Field) && (uiKind != CPPSK_Variable)))
	{
		KillIDELocations(&asLocations);
		return false;
	}
	psDest->szKey.Set(szKey);
	psDest->szName.Set(&szKey[psMove->sClass.szKey.Length() + 2]);
	GetIDESymbolName(szKey, &psDest->szOwnName);
	psDest->bFunction = (uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate);
	szClassName.Init();
	GetIDESymbolName(psMove->sClass.szKey.Text(), &szClassName);
	if (psDest->szOwnName.Equals(szClassName.Text()) || psDest->szOwnName.StartsWith("~"))
	{
		szClassName.Kill();
		KillIDELocations(&asLocations);
		return false;
	}
	szClassName.Kill();

	bDeclared = false;
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			continue;
		}
		bInClass = (StringInsensitiveCompare(psLocation->szFileName.Text(), psMove->sClass.szFileName.Text()) == 0) &&
				   (psLocation->iLine >= psMove->sClass.sRange.iLine) && (psLocation->iLine <= psMove->sClass.sRange.iEndLine);
		if ((bInClass && bDeclared) || (!bInClass && (psDest->bOutside || (psLocation->uiRole != CPPIR_Definition))))
		{
			continue;
		}
		pcUnit = GetUnit(psLocation->szFileName.Text());
		if (!pcUnit)
		{
			psDest->szWhyNot.Set("a file it is declared in couldn't be parsed");
			continue;
		}
		szWhyNot = NULL;
		if (psDest->bFunction)
		{
			if (!mpcContent->GetChangeSignature()->ReadSite(pcUnit, psLocation, szKey, bInClass ? &psDest->sDeclaration : &psDest->sDefinition, &szWhyNot))
			{
				psDest->szWhyNot.Set(szWhyNot);
				continue;
			}
			if (bInClass)
			{
				psDest->sDeclared = psDest->sDeclaration.sAll;
			}
			else
			{
				psDest->sOutside = psDest->sDefinition.sAll;
				psDest->sOutsideName = psDest->sDefinition.sName;
			}
		}
		else
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
			if (!pcOuter || !pcDeclarator->GetName() || !pcUnit->GetRange(pcOuter, bInClass ? &psDest->sDeclared : &psDest->sOutside) ||
				(!bInClass && !pcUnit->GetRange(pcDeclarator->GetName(), &psDest->sOutsideName)))
			{
				psDest->szWhyNot.Set("a declaration of it is made by a macro");
				continue;
			}
		}
		if (bInClass)
		{
			bDeclared = true;
			psDest->bStatic = pcUnit->FindTokensIn(psDest->sDeclared.iLine, psDest->sDeclared.iColumn, psDest->sDeclared.iEndLine, psDest->sDeclared.iEndColumn, &iFirst, &iLast) && pcUnit->IsToken(iFirst, "static");
			ScanClass(&psMove->sClass, pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn), -1, &psDest->iAccess, NULL, NULL);
		}
		else
		{
			psDest->bOutside = true;
			psDest->szOutsideFile.Set(psLocation->szFileName);
		}
	}
	KillIDELocations(&asLocations);
	if (!bDeclared && psDest->szWhyNot.Empty())
	{
		psDest->szWhyNot.Set("its declaration in the class wasn't found");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The class's members, in the order it declares them; the one named at
// the caret is chosen.  Those one declaration declares together
// ("int x, y;") can't be moved apart.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::ReadMembers(SIDEMembersMove* psMove, const char* szCaretKey)
{
	CIDECodeModel*			pcModel;
	CArrayIDEFileRanges		asRanges;
	CArrayChars				aszKeys;
	SIDEMovedMember*		psMember;
	SIDEMovedMember*		psOther;
	CChars					szScope;
	size					uiAt;
	size					i;
	size					j;

	pcModel = mpcContent->GetCodeModel();
	asRanges.Init();
	asRanges.Add()->Init(psMove->sClass.szFileName.Text(), psMove->sClass.sRange.iLine, psMove->sClass.sRange.iColumn, psMove->sClass.sRange.iEndLine, psMove->sClass.sRange.iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	for (i = 0; i < aszKeys.NumElements(); i++)
	{
		szScope.Init();
		GetIDESymbolScope(aszKeys.GetText(i), &szScope);
		if (szScope.Equals(psMove->sClass.szKey.Text()))
		{
			psMember = NewMalloc<SIDEMovedMember>();
			psMember->Init();
			if (ReadMember(psMove, aszKeys.GetText(i), psMember))
			{
				psMember->bChosen = psMember->szKey.Equals(szCaretKey);
				for (uiAt = 0; uiAt < psMove->apsMembers.NumElements(); uiAt++)
				{
					psOther = psMove->apsMembers.GetPtr(uiAt);
					if ((psOther->sDeclared.iLine > psMember->sDeclared.iLine) || ((psOther->sDeclared.iLine == psMember->sDeclared.iLine) && (psOther->sDeclared.iColumn > psMember->sDeclared.iColumn)))
					{
						break;
					}
				}
				psMove->apsMembers.InsertAt(&psMember, uiAt);
			}
			else
			{
				psMember->Kill();
				free(psMember);
			}
		}
		szScope.Kill();
	}
	aszKeys.Kill();
	KillIDEFileRanges(&asRanges);

	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		for (j = 0; j < psMove->apsMembers.NumElements(); j++)
		{
			psOther = psMove->apsMembers.GetPtr(j);
			if ((i != j) && psMember->szWhyNot.Empty() && (memcmp(&psMember->sDeclared, &psOther->sDeclared, sizeof(SCPPSyntaxRange)) == 0))
			{
				psMember->szWhyNot.Set("it is declared with ");
				psMember->szWhyNot.Append(psOther->szName);
				psMember->szWhyNot.Append(" in one declaration");
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What Pull Members Up (bUp) or Push Members Down has to work with, for
// the class the caret is in: on one of its members, or on its name.
// False if there is no class there; psDest->bMovable says whether there
// is anywhere for its members to go, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::FindMove(bool bUp, SIDEMembersMove* psDest)
{
	CIDECodeModel*	pcModel;
	CArrayChars		aszClasses;
	SIDEMoveClass	sTarget;
	CChars			szKey;
	CChars			szClass;
	int				iLine;
	int				iColumn;
	size			i;
	uint8			uiRole;
	uint8			uiKind;

	pcModel = mpcContent->GetCodeModel();
	psDest->bUp = bUp;
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
	szClass.Init();
	if ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate))
	{
		szClass.Set(szKey);
	}
	else
	{
		GetIDESymbolScope(szKey.Text(), &szClass);
	}
	if (szClass.Empty() || !ReadClass(szClass.Text(), &psDest->sClass))
	{
		szClass.Kill();
		szKey.Kill();
		KillUnits();
		return false;
	}
	ReadMembers(psDest, szKey.Text());

	aszClasses.Init();
	if (bUp)
	{
		pcModel->FindClassBases(szClass.Text(), true, &aszClasses);
	}
	else
	{
		pcModel->FindClassDerived(szClass.Text(), false, &aszClasses);
	}
	for (i = 0; i < aszClasses.NumElements(); i++)
	{
		sTarget.Init();
		if (ReadClass(aszClasses.GetText(i), &sTarget))
		{
			sTarget.bChosen = !bUp;
			memcpy(psDest->asTargets.Add(), &sTarget, sizeof(SIDEMoveClass));
		}
		else
		{
			sTarget.Kill();
		}
	}
	aszClasses.Kill();

	psDest->bMovable = true;
	if (psDest->asTargets.NumElements() == 0)
	{
		psDest->bMovable = false;
		psDest->szWhyNot.Set(bUp ? "it has no base class in the solution" : "no class in the solution derives from it");
	}
	else if (psDest->apsMembers.NumElements() == 0)
	{
		psDest->bMovable = false;
		psDest->szWhyNot.Set("it has no functions or fields");
	}
	szClass.Kill();
	szKey.Kill();
	KillUnits();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What is asked for, written as a script writes it:
// "CVessel|Fill(int, int);miLevel|Vent()": the class to (Push Down: the
// classes, or * for all of them), the members to move, and those of them
// to make or keep abstract.  A member is named as it is shown, or by its
// name alone if it has no overloads.  False, with why, if it names what
// isn't there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::ParseSpec(SIDEMembersMove* psMove, const char* szSpec, CChars* pszProblem)
{
	CArrayChars			aszParts;
	CArrayChars			aszNames;
	CChars				szAll;
	CChars				szName;
	SIDEMovedMember*	psMember;
	SIDEMovedMember*	psFound;
	size				uiPart;
	size				i;
	size				j;
	size				uiFound;
	bool				bResult;

	szAll.Init(szSpec);
	aszParts.Init();
	szAll.Split(&aszParts, '|');
	bResult = true;
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMove->apsMembers.GetPtr(i)->bChosen = false;
		psMove->apsMembers.GetPtr(i)->bAbstract = false;
	}
	for (uiPart = 0; bResult && (uiPart < aszParts.NumElements()) && (uiPart < 3); uiPart++)
	{
		aszNames.Init();
		aszParts.Get(uiPart)->Split(&aszNames, ';');
		if ((uiPart == 0) && !psMove->bUp)
		{
			for (j = 0; j < psMove->asTargets.NumElements(); j++)
			{
				psMove->asTargets.Get(j)->bChosen = aszParts.Get(0)->Equals("*");
			}
		}
		for (i = 0; bResult && (i < aszNames.NumElements()); i++)
		{
			if (aszNames.Get(i)->Empty() || ((uiPart == 0) && aszNames.Get(i)->Equals("*")))
			{
				continue;
			}
			uiFound = 0;
			if (uiPart == 0)
			{
				for (j = 0; j < psMove->asTargets.NumElements(); j++)
				{
					szName.Init();
					GetIDESymbolName(psMove->asTargets.Get(j)->szKey.Text(), &szName);
					if (szName.Equals(aszNames.GetText(i)) || psMove->asTargets.Get(j)->szKey.Equals(aszNames.GetText(i)))
					{
						uiFound++;
						psMove->uiTarget = j;
						psMove->asTargets.Get(j)->bChosen = true;
					}
					szName.Kill();
				}
			}
			else
			{
				psFound = NULL;
				for (j = 0; j < psMove->apsMembers.NumElements(); j++)
				{
					psMember = psMove->apsMembers.GetPtr(j);
					if (psMember->szName.Equals(aszNames.GetText(i)) || psMember->szOwnName.Equals(aszNames.GetText(i)))
					{
						uiFound++;
						psFound = psMember;
					}
				}
				if (uiFound == 1)
				{
					psFound->bChosen = true;
					psFound->bAbstract = (uiPart == 2);
				}
			}
			if (uiFound != 1)
			{
				pszProblem->Set((uiFound == 0) ? "there is no " : "there is more than one ");
				pszProblem->Append(aszNames.GetText(i));
				bResult = false;
			}
		}
		aszNames.Kill();
	}
	aszParts.Kill();
	szAll.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::IsChosen(SIDEMembersMove* psMove, const char* szKey, bool bAbstractToo)
{
	SIDEMovedMember*	psMember;
	size				i;

	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		if (psMember->bChosen && (bAbstractToo || !psMember->bAbstract) && psMember->szKey.Equals(szKey))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The key a member has as a member of another class.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::AppendMemberKeyIn(SIDEMovedMember* psMember, const char* szClassKey, CChars* pszDest)
{
	pszDest->Append(szClassKey);
	pszDest->Append("::");
	pszDest->Append(psMember->szName);
}


//////////////////////////////////////////////////////////////////////////
//
// What stands in the way of the move as it is chosen, a line each.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::FindConflicts(SIDEMembersMove* psMove, CArrayChars* paszDest)
{
	CIDECodeModel*		pcModel;
	CArrayChars			aszBetween;
	CArrayChars			aszBases;
	CArrayChars			aszUsed;
	CArrayChars			aszSaid;
	CArrayChars			aszInTargets;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	SIDEMovedMember*	psMember;
	SIDEMoveClass*		psTo;
	SIDELocation*		psLocation;
	CChars				szLine;
	CChars				szKey;
	CChars				szScope;
	CChars				szClassName;
	CChars				szToName;
	size				uiChosen;
	size				uiTargets;
	size				i;
	size				j;
	uint8				uiKind;
	bool				bInside;

	pcModel = mpcContent->GetCodeModel();
	szClassName.Init();
	GetIDEScopedName(psMove->sClass.szKey.Text(), &szClassName);
	szToName.Init();
	psTo = psMove->asTargets.Get(psMove->uiTarget);
	if (psMove->bUp)
	{
		GetIDEScopedName(psTo->szKey.Text(), &szToName);
	}

	//Up: the classes whose members the class to doesn't have: the class and what is between the two.
	aszBetween.Init();
	aszInTargets.Init();
	if (psMove->bUp)
	{
		aszBetween.Add(psMove->sClass.szKey.Text());
		aszBases.Init();
		pcModel->FindClassBases(psMove->sClass.szKey.Text(), true, &aszBases);
		for (i = 0; i < aszBases.NumElements(); i++)
		{
			aszUsed.Init();
			pcModel->FindClassBases(aszBases.GetText(i), true, &aszUsed);
			for (j = 0; j < aszUsed.NumElements(); j++)
			{
				if (aszUsed.Get(j)->Equals(psTo->szKey.Text()))
				{
					aszBetween.AddIfUnique(aszBases.GetText(i));
				}
			}
			aszUsed.Kill();
		}
		aszBases.Kill();
	}
	else
	{
		//Down: the classes that will have the members: those chosen, and what derives from them.
		uiTargets = 0;
		for (i = 0; i < psMove->asTargets.NumElements(); i++)
		{
			if (psMove->asTargets.Get(i)->bChosen)
			{
				uiTargets++;
				aszInTargets.AddIfUnique(psMove->asTargets.Get(i)->szKey.Text());
				pcModel->FindClassDerived(psMove->asTargets.Get(i)->szKey.Text(), true, &aszInTargets);
			}
		}
		if (uiTargets == 0)
		{
			paszDest->Add("No class is chosen to push the members down to.");
		}
	}

	uiChosen = 0;
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		if (!psMember->bChosen)
		{
			continue;
		}
		uiChosen++;
		if (!psMember->szWhyNot.Empty())
		{
			szLine.Init();
			szLine.Append(psMember->szName)->Append(" can't be moved: ")->Append(psMember->szWhyNot)->Append(".");
			paszDest->Add(szLine.Text());
			szLine.Kill();
			continue;
		}
		if (psMember->bAbstract)
		{
			if (!psMember->bFunction || psMember->bStatic || !psMember->sDeclaration.szOwnTemplate.Empty())
			{
				szLine.Init();
				szLine.Append(psMember->szName)->Append(" can't be abstract: only a function that isn't static or a template can be.");
				paszDest->Add(szLine.Text());
				szLine.Kill();
			}
		}

		//The class, or one of the classes, it goes to has it already.
		for (j = 0; j < psMove->asTargets.NumElements(); j++)
		{
			if (psMove->bUp ? (j != psMove->uiTarget) : (!psMove->asTargets.Get(j)->bChosen || psMember->bFunction))
			{
				continue;
			}
			szKey.Init();
			AppendMemberKeyIn(psMember, psMove->asTargets.Get(j)->szKey.Text(), &szKey);
			if (pcModel->HasSymbol(szKey.Text()))
			{
				szLine.Init();
				GetIDEScopedName(psMove->asTargets.Get(j)->szKey.Text(), &szLine);
				szLine.Append(" already has ")->Append(psMember->szName)->Append(".");
				paszDest->Add(szLine.Text());
				szLine.Kill();
			}
			szKey.Kill();
		}

		aszSaid.Init();
		if (psMove->bUp && !psMember->bAbstract)
		{
			//What it uses that the class it goes to doesn't have.
			asRanges.Init();
			asRanges.Add()->Init(psMove->sClass.szFileName.Text(), psMember->sDeclared.iLine, psMember->sDeclared.iColumn, psMember->sDeclared.iEndLine, psMember->sDeclared.iEndColumn);
			if (psMember->bOutside)
			{
				asRanges.Add()->Init(psMember->szOutsideFile.Text(), psMember->sOutside.iLine, psMember->sOutside.iColumn, psMember->sOutside.iEndLine, psMember->sOutside.iEndColumn);
			}
			aszUsed.Init();
			pcModel->FindSymbolsUsedIn(&asRanges, &aszUsed);
			for (j = 0; j < aszUsed.NumElements(); j++)
			{
				szScope.Init();
				GetIDESymbolScope(aszUsed.GetText(j), &szScope);
				if (!szScope.Empty() && aszBetween.Contains(szScope.Text()) && !IsChosen(psMove, aszUsed.GetText(j), true) && !psMember->szKey.Equals(aszUsed.GetText(j)))
				{
					szLine.Init();
					szLine.Append(psMember->szName)->Append(" uses ");
					GetIDEScopedName(aszUsed.GetText(j), &szLine);
					szLine.Append(", which ")->Append(szToName)->Append(" doesn't have.");
					paszDest->Add(szLine.Text());
					szLine.Kill();
				}
				szScope.Kill();
			}
			aszUsed.Kill();
			KillIDEFileRanges(&asRanges);
		}
		if ((psMove->bUp && !psMember->bAbstract && (psMember->iAccess == IDE_ACCESS_PRIVATE)) || (!psMove->bUp && !psMember->bAbstract))
		{
			//Who uses it and won't be able to: up, the class's others once it is the base class's private; down,
			//everything but the classes it goes to.
			asLocations.Init();
			pcModel->GetSymbolLocations(psMember->szKey.Text(), &asLocations, &uiKind);
			for (j = 0; j < asLocations.NumElements(); j++)
			{
				psLocation = asLocations.Get(j);
				if (psLocation->uiRole != CPPIR_Reference)
				{
					continue;
				}
				szKey.Init();
				szScope.Init();
				if (pcModel->GetSymbolKey(psLocation->iContainer, &szKey, &uiKind))
				{
					GetIDESymbolScope(szKey.Text(), &szScope);
				}
				szLine.Init();
				if (psMove->bUp)
				{
					if (szScope.Equals(psMove->sClass.szKey.Text()) && !IsChosen(psMove, szKey.Text(), false))
					{
						szLine.Append(psMember->szName)->Append(" will be private in ")->Append(szToName)->Append(": ");
						GetIDEScopedName(szKey.Text(), &szLine);
						szLine.Append(" uses it.");
					}
				}
				else
				{
					bInside = IsChosen(psMove, szKey.Text(), false) || aszInTargets.Contains(szScope.Text()) || aszInTargets.Contains(szKey.Text()) || IsUsedThrough(psLocation, &aszInTargets);
					if (!bInside && !szKey.Empty())
					{
						GetIDEScopedName(szKey.Text(), &szLine);
						szLine.Append(" uses ")->Append(psMember->szName)->Append(" (")->Append(GetIDEFileNamePart(psLocation->szFileName.Text()))->Append(" line ");
						szLine.Append((int32)(psLocation->iLine + 1));
						szLine.Append("), which ")->Append(szClassName)->Append(" won't have.");
					}
					else if (!bInside)
					{
						szLine.Append(psMember->szName)->Append(" is used in ")->Append(GetIDEFileNamePart(psLocation->szFileName.Text()))->Append(" at line ");
						szLine.Append((int32)(psLocation->iLine + 1));
						szLine.Append(", and ")->Append(szClassName)->Append(" won't have it.");
					}
				}
				if (!szLine.Empty() && !aszSaid.Contains(szLine.Text()))
				{
					aszSaid.Add(szLine.Text());
					paszDest->Add(szLine.Text());
				}
				szLine.Kill();
				szScope.Kill();
				szKey.Kill();
			}
			KillIDELocations(&asLocations);
		}
		aszSaid.Kill();
	}
	if (uiChosen == 0)
	{
		paszDest->Add("No member is chosen.");
	}
	aszInTargets.Kill();
	aszBetween.Kill();
	szToName.Kill();
	szClassName.Kill();
	KillUnits();
}


//////////////////////////////////////////////////////////////////////////
//
// The file a class's functions are defined in outside it: where one of
// them is, or its header's source file.  Nothing if there is neither.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::FindSourceFile(SIDEMoveClass* psClass, CChars* pszDest)
{
	CArrayIDELocations	asDeclared;
	CArrayIDELocations	asDefined;
	CArrayChars			aszRelated;
	CChars				szKey;
	size				i;

	szKey.Init(psClass->szKey);
	szKey.Append("::x");
	asDeclared.Init();
	asDefined.Init();
	mpcContent->GetCodeModel()->FindSiblingFunctions(szKey.Text(), psClass->szFileName.Text(), &asDeclared, &asDefined);
	for (i = 0; pszDest->Empty() && (i < asDefined.NumElements()); i++)
	{
		if ((asDefined.Get(i)->iLine >= 0) && (StringInsensitiveCompare(asDefined.Get(i)->szFileName.Text(), psClass->szFileName.Text()) != 0))
		{
			pszDest->Set(asDefined.Get(i)->szFileName);
		}
	}
	KillIDELocations(&asDefined);
	KillIDELocations(&asDeclared);
	szKey.Kill();
	if (pszDest->Empty())
	{
		aszRelated.Init();
		mpcContent->GetFileRename()->FindRelatedFiles(psClass->szFileName.Text(), &aszRelated);
		for (i = 0; i < aszRelated.NumElements(); i++)
		{
			if ((aszRelated.Get(i)->EndsWithIgnoreCase(".cpp") || aszRelated.Get(i)->EndsWithIgnoreCase(".c")) && pszDest->Empty())
			{
				pszDest->Set(aszRelated.GetText(i));
			}
		}
		aszRelated.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A member as it is declared in the class it goes to: its lines in its
// own class, with the comment over them.  A function defined outside,
// where either class is a class template, is written with its body
// (CIDEDefinitions::AppendBodyInClass): there it needs no template<...>
// and no class's name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::AppendDeclarationText(SIDEMembersMove* psMove, SIDEMovedMember* psMember, SIDEMoveClass* psTo, CIDEFileEdits* pcEdits, CChars* pszDest)
{
	CChars	szBody;
	CChars	szOutside;
	CChars	szIndent;
	char*	szText;
	char*	szOutsideText;
	size	uiStart;
	size	uiLineStart;
	size	uiEnd;
	size	i;

	szText = pcEdits->GetText(psMove->sClass.szFileName.Text());
	uiStart = FindIDETextOffset(szText, psMember->sDeclared.iLine, psMember->sDeclared.iColumn);
	uiEnd = FindIDELineEnd(szText, FindIDETextOffset(szText, psMember->sDeclared.iEndLine, psMember->sDeclared.iEndColumn));
	szOutsideText = (psMember->bFunction && psMember->bOutside && (psMove->sClass.bTemplate || psTo->bTemplate)) ? pcEdits->GetText(psMember->szOutsideFile.Text()) : NULL;
	if (!szOutsideText)
	{
		pszDest->AppendSubString(szText, FindIDECommentStart(szText, uiStart), uiEnd);
		if (!pszDest->EndsWith("\n"))
		{
			pszDest->Append('\n');
		}
		return;
	}

	uiLineStart = FindIDETextOffset(szText, psMember->sDeclared.iLine, 0);
	for (i = uiLineStart; (szText[i] == ' ') || (szText[i] == '\t'); i++)
	{
	}
	szIndent.Init();
	szIndent.AppendSubString(szText, uiLineStart, i);
	szBody.Init();
	szOutside.Init();
	AppendIDERangeText(szOutsideText, &psMember->sDefinition.sBody, &szBody);
	mpcContent->GetDefinitions()->AppendBodyOutside(szBody.Text(), &szOutside);
	pszDest->AppendSubString(szText, FindIDECommentStart(szText, uiStart), FindIDETextOffset(szText, psMember->sDeclaration.sAfter.iEndLine, psMember->sDeclaration.sAfter.iEndColumn));
	mpcContent->GetDefinitions()->AppendBodyInClass(szOutside.Text(), szIndent.Text(), pszDest);
	pszDest->Append('\n');
	szOutside.Kill();
	szBody.Kill();
	szIndent.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A function as a class declares it abstract: its declaration's line as
// its class writes it, to its parameters and const, virtual and "=0".
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::AppendPureText(SIDEMovedMember* psMember, const char* szText, CChars* pszDest)
{
	CChars	szStart;
	size	uiStart;
	size	uiLineStart;
	size	uiName;

	uiStart = FindIDETextOffset(szText, psMember->sDeclared.iLine, psMember->sDeclared.iColumn);
	uiLineStart = FindIDETextOffset(szText, psMember->sDeclared.iLine, 0);
	uiName = FindIDETextOffset(szText, psMember->sDeclaration.sName.iLine, psMember->sDeclaration.sName.iColumn);
	szStart.Init();
	szStart.AppendSubString(szText, uiStart, uiName);
	pszDest->AppendSubString(szText, uiLineStart, uiStart);
	pszDest->Append(szStart.Contains("virtual") ? "" : "virtual ");
	pszDest->AppendSubString(szText, uiStart, FindIDETextOffset(szText, psMember->sDeclaration.sParameters.iEndLine, psMember->sDeclaration.sParameters.iEndColumn));
	pszDest->Append(')');
	pszDest->Append(psMember->sDeclaration.szQualifiers);
	pszDest->Append(" =0;\n");
	szStart.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A member's definition outside its class as another class's: with the
// comment over it, and that class's name before its own.  szText is the
// text of the file it is in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::AppendOutsideText(SIDEMovedMember* psMember, SIDEMoveClass* psTo, const char* szText, CChars* pszDest)
{
	size	uiStart;
	size	uiEnd;
	size	uiName;
	size	uiNameEnd;
	size	uiOwn;
	size	i;
	int		iDepth;

	uiStart = FindIDETextOffset(szText, psMember->sOutside.iLine, psMember->sOutside.iColumn);
	uiEnd = FindIDETextOffset(szText, psMember->sOutside.iEndLine, psMember->sOutside.iEndColumn);
	for (i = uiEnd; (szText[i] == ' ') || (szText[i] == '\t'); i++)
	{
	}
	uiEnd = (szText[i] == ';') ? i + 1 : uiEnd;
	uiName = FindIDETextOffset(szText, psMember->sOutsideName.iLine, psMember->sOutsideName.iColumn);
	uiNameEnd = FindIDETextOffset(szText, psMember->sOutsideName.iEndLine, psMember->sOutsideName.iEndColumn);

	//Its own name: what follows the last :: that isn't in a template's angle brackets.
	uiOwn = uiName;
	iDepth = 0;
	for (i = uiName; i + 1 < uiNameEnd; i++)
	{
		iDepth += (szText[i] == '<') ? 1 : ((szText[i] == '>') ? -1 : 0);
		if ((iDepth == 0) && (szText[i] == ':') && (szText[i + 1] == ':'))
		{
			uiOwn = i + 2;
		}
		if ((i >= uiName + 8) && (strncmp(&szText[i - 8], "operator", 8) == 0))
		{
			break;
		}
	}
	pszDest->AppendSubString(szText, FindIDECommentStart(szText, uiStart), uiName);
	pszDest->Append(psTo->szKey);
	pszDest->Append("::");
	pszDest->AppendSubString(szText, uiOwn, uiEnd);
	pszDest->Append('\n');
}


//////////////////////////////////////////////////////////////////////////
//
// The move made, as one undo across the files.  False if there is
// nothing to move or somewhere it goes can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::Move(SIDEMembersMove* psMove, CChars* pszStatus)
{
	CIDECodeModel*			pcModel;
	CIDEFileEdits			cEdits;
	CArrayIDEMoveInserts	asInserts;
	SIDEMovedMember*		psMember;
	SIDEMoveClass*			psTo;
	SIDEMoveInsert*			psInsert;
	CChars*					pszInsert;
	CChars					szBefore;
	CChars					szDeclaration;
	CChars					szBlock;
	CChars					szCopies;
	CChars					szDest;
	CChars					szKey;
	CChars					szMessage;
	CChars					szLast;
	char*					szText;
	char*					szToText;
	char*					szOutsideText;
	char*					szDestText;
	size					uiStart;
	size					uiEnd;
	size					uiAt;
	size					uiMoved;
	size					uiTargets;
	size					i;
	size					j;
	bool					bInline;
	bool					bResult;

	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	cEdits.Init(mpcContent);
	asInserts.Init();
	szLast.Init();
	szText = cEdits.GetText(psMove->sClass.szFileName.Text());
	bResult = (szText != NULL);
	uiMoved = 0;
	uiTargets = 0;
	for (j = 0; j < psMove->asTargets.NumElements(); j++)
	{
		uiTargets += (psMove->bUp ? (j == psMove->uiTarget) : psMove->asTargets.Get(j)->bChosen) ? 1 : 0;
	}
	for (i = 0; bResult && (i < psMove->apsMembers.NumElements()); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		if (!psMember->bChosen || !psMember->szWhyNot.Empty())
		{
			continue;
		}
		uiMoved++;
		szLast.Set(psMember->szName);
		szOutsideText = psMember->bOutside ? cEdits.GetText(psMember->szOutsideFile.Text()) : NULL;
		szCopies.Init();

		//Into each class it goes to.
		for (j = 0; bResult && (j < psMove->asTargets.NumElements()); j++)
		{
			psTo = psMove->asTargets.Get(j);
			if (psMove->bUp ? (j != psMove->uiTarget) : !psTo->bChosen)
			{
				continue;
			}
			szKey.Init();
			AppendMemberKeyIn(psMember, psTo->szKey.Text(), &szKey);
			if (!psMove->bUp && psMember->bFunction && pcModel->HasSymbol(szKey.Text()))
			{
				//It overrides it already: its own stays.
				szKey.Kill();
				continue;
			}
			szKey.Kill();
			szToText = cEdits.GetText(psTo->szFileName.Text());
			szBefore.Init();
			if (!szToText || !FindInsertPlace(psTo, szToText, psMember->iAccess, &uiAt, &szBefore))
			{
				szBefore.Kill();
				bResult = false;
				break;
			}
			szDeclaration.Init();
			if (psMove->bUp && psMember->bAbstract)
			{
				AppendPureText(psMember, szText, &szDeclaration);
			}
			else
			{
				AppendDeclarationText(psMove, psMember, psTo, &cEdits, &szDeclaration);
			}
			RealignIDEDeclaration(&szDeclaration, FindIDENameColumn(szToText, uiAt));
			AddIDEMoveInsert(&asInserts, psTo->szFileName.Text(), uiAt, psMember->iAccess, szBefore.Text())->Append(szDeclaration);
			szDeclaration.Kill();
			szBefore.Kill();

			//Its definition outside the class, as this class's.
			bInline = psMember->bFunction && (psMove->sClass.bTemplate || psTo->bTemplate);
			if (szOutsideText && !bInline && !(psMove->bUp && psMember->bAbstract))
			{
				szDest.Init();
				FindSourceFile(psTo, &szDest);
				szBlock.Init();
				AppendOutsideText(psMember, psTo, szOutsideText, &szBlock);
				szDestText = szDest.Empty() ? NULL : cEdits.GetText(szDest.Text());
				if (!szDestText || (StringInsensitiveCompare(szDest.Text(), psMember->szOutsideFile.Text()) == 0))
				{
					//It stays in its file, where it was.
					szCopies.Append(szCopies.Empty() ? "" : "\n\n");
					szCopies.Append(szBlock);
				}
				else
				{
					pszInsert = AddIDEMoveInsert(&asInserts, szDest.Text(), strlen(szDestText), -1, ((szDestText[0] != '\0') && (szDestText[strlen(szDestText) - 1] != '\n')) ? "\n" : "");
					pszInsert->Append("\n\n");
					pszInsert->Append(szBlock);
				}
				szBlock.Kill();
				szDest.Kill();
			}
		}

		//Out of its class, unless it goes up abstract (it stays) or stays abstract (pure).
		if (bResult && !(psMove->bUp && psMember->bAbstract))
		{
			uiStart = FindIDETextOffset(szText, psMember->sDeclared.iLine, psMember->sDeclared.iColumn);
			uiEnd = FindIDETextOffset(szText, psMember->sDeclared.iEndLine, psMember->sDeclared.iEndColumn);
			if (psMember->bAbstract && psMember->bFunction)
			{
				szDeclaration.Init();
				AppendPureText(psMember, szText, &szDeclaration);
				cEdits.GetEdits(psMove->sClass.szFileName.Text())->Replace(FindIDETextOffset(szText, psMember->sDeclared.iLine, 0), FindIDELineEnd(szText, uiEnd), szDeclaration.Text());
				szDeclaration.Kill();
			}
			else
			{
				ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
				cEdits.GetEdits(psMove->sClass.szFileName.Text())->Replace(uiStart, uiEnd, "");
			}
			if (szOutsideText)
			{
				uiStart = FindIDETextOffset(szOutsideText, psMember->sOutside.iLine, psMember->sOutside.iColumn);
				uiEnd = FindIDETextOffset(szOutsideText, psMember->sOutside.iEndLine, psMember->sOutside.iEndColumn);
				if (szCopies.Empty())
				{
					ExtendIDEDeletion(szOutsideText, &uiStart, &uiEnd, true);
					cEdits.GetEdits(psMember->szOutsideFile.Text())->Replace(uiStart, uiEnd, "");
				}
				else
				{
					//Where it was: the copies of it that stay in this file.
					for (; (szOutsideText[uiEnd] == ' ') || (szOutsideText[uiEnd] == '\t') || (szOutsideText[uiEnd] == ';'); uiEnd++)
					{
					}
					cEdits.GetEdits(psMember->szOutsideFile.Text())->Replace(FindIDECommentStart(szOutsideText, uiStart), FindIDELineEnd(szOutsideText, uiEnd), szCopies.Text());
				}
			}
		}
		szCopies.Kill();
	}

	bResult = bResult && (uiMoved > 0) && (uiTargets > 0);
	if (bResult)
	{
		for (i = 0; i < asInserts.NumElements(); i++)
		{
			psInsert = asInserts.Get(i);
			cEdits.GetEdits(psInsert->szFileName.Text())->Insert(psInsert->uiAt, psInsert->szText.Text());
		}
		cEdits.Apply();

		szMessage.Init(psMove->bUp ? "Pulled " : "Pushed ");
		if (uiMoved == 1)
		{
			szMessage.Append(szLast);
		}
		else
		{
			szMessage.Append((uint32)uiMoved);
			szMessage.Append(" members");
		}
		szMessage.Append(psMove->bUp ? " up from " : " down from ");
		GetIDEScopedName(psMove->sClass.szKey.Text(), &szMessage);
		szMessage.Append(" to ");
		uiAt = 0;
		for (j = 0; j < psMove->asTargets.NumElements(); j++)
		{
			if (psMove->bUp ? (j == psMove->uiTarget) : psMove->asTargets.Get(j)->bChosen)
			{
				szMessage.Append((uiAt > 0) ? ", " : "");
				GetIDEScopedName(psMove->asTargets.Get(j)->szKey.Text(), &szMessage);
				uiAt++;
			}
		}
		szMessage.Append(".");
		ShowMoveStatus(&szMessage, pszStatus);
		szMessage.Kill();
	}
	for (i = 0; i < asInserts.NumElements(); i++)
	{
		asInserts.Get(i)->szText.Kill();
		asInserts.Get(i)->szFileName.Kill();
	}
	asInserts.Kill();
	szLast.Kill();
	cEdits.Kill();
	KillUnits();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMoveMembers::ShowMoveStatus(CChars* pszMessage, CChars* pszStatus)
{
	if (pszStatus)
	{
		pszStatus->Set(pszMessage->Text());
	}
	else
	{
		mpcContent->ShowStatus(pszMessage->Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks: the class, where its members can go,
// and each member.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEMoveMembers::PrintMove(SIDEMembersMove* psMove, CChars* pszDest)
{
	SIDEMovedMember*	psMember;
	size				i;

	pszDest->Append(psMove->bUp ? "Pull Members Up: " : "Push Members Down: ");
	pszDest->Append(psMove->sClass.szKey);
	if (!psMove->bMovable)
	{
		pszDest->Append(" can't be: ")->Append(psMove->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	pszDest->Append(" to");
	for (i = 0; i < psMove->asTargets.NumElements(); i++)
	{
		pszDest->Append((i > 0) ? ", " : " ");
		pszDest->Append(psMove->asTargets.Get(i)->szKey);
		pszDest->Append(psMove->asTargets.Get(i)->bTemplate ? " (template)" : "");
	}
	pszDest->Append('\n');
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		pszDest->Append("  ")->Append(gaszIDEAccess[psMember->iAccess])->Append(psMember->bFunction ? " function " : " field ")->Append(psMember->szName);
		if (psMember->bOutside)
		{
			pszDest->Append(", defined in ")->Append(GetIDEFileNamePart(psMember->szOutsideFile.Text()));
		}
		pszDest->Append(psMember->bChosen ? ", chosen" : "");
		if (!psMember->szWhyNot.Empty())
		{
			pszDest->Append(", can't be moved: ")->Append(psMember->szWhyNot);
		}
		pszDest->Append('\n');
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog: where the members go (Up: one of the base classes; Down:
// the deriving classes, each ticked), the members, each ticked if it is
// to be moved, and the functions again, each ticked if it is to be
// abstract.  False if it is cancelled.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::ShowDialog(SIDEMembersMove* psMove)
{
	CArrayTemplatePtr<CCheckBox>	apcTargets;
	CArrayTemplatePtr<CCheckBox>	apcMembers;
	CArrayTemplatePtr<CCheckBox>	apcAbstract;
	CArrayInt						aiAbstract;
	Ptr<CComboBox>					pTarget;
	Ptr<CCheckBox>					pBox;
	SIDEMovedMember*				psMember;
	const char**					aszTargets;
	CChars							szTitle;
	CChars							szLabel;
	size							i;
	bool							bOK;

	szTitle.Init(psMove->bUp ? "Pull Members Up from " : "Push Members Down from ");
	GetIDEScopedName(psMove->sClass.szKey.Text(), &szTitle);
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szTitle.Text());
	szTitle.Kill();

	apcTargets.Init();
	apcMembers.Init();
	apcAbstract.Init();
	aiAbstract.Init();
	if (psMove->bUp)
	{
		aszTargets = (const char**)malloc(psMove->asTargets.NumElements() * sizeof(char*));
		for (i = 0; i < psMove->asTargets.NumElements(); i++)
		{
			aszTargets[i] = psMove->asTargets.Get(i)->szKey.Text();
		}
		pTarget = mpDialog->AddComboBox("To class:", aszTargets, psMove->asTargets.NumElements(), (int)psMove->uiTarget);
		free(aszTargets);
	}
	else
	{
		mpDialog->AddDivider("To classes");
		for (i = 0; i < psMove->asTargets.NumElements(); i++)
		{
			pBox = mpDialog->AddCheckBox(psMove->asTargets.Get(i)->szKey.Text(), psMove->asTargets.Get(i)->bChosen);
			apcTargets.Add(&pBox);
		}
	}
	mpDialog->AddDivider("Members");
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		szLabel.Init(psMember->szName);
		if (!psMember->szWhyNot.Empty())
		{
			szLabel.Append("  (")->Append(psMember->szWhyNot)->Append(")");
		}
		pBox = mpDialog->AddCheckBox(szLabel.Text(), psMember->bChosen);
		apcMembers.Add(&pBox);
		szLabel.Kill();
	}
	mpDialog->AddDivider(psMove->bUp ? "Make abstract" : "Keep abstract");
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		aiAbstract.Add(psMember->bFunction ? (int)apcAbstract.NumElements() : -1);
		if (psMember->bFunction)
		{
			pBox = mpDialog->AddCheckBox(psMember->szName.Text(), psMember->bAbstract);
			apcAbstract.Add(&pBox);
		}
	}
	pBox = NULL;
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bOK = mpDialog->Show(560);
	if (bOK)
	{
		if (psMove->bUp)
		{
			psMove->uiTarget = (size)pTarget->GetSelected();
		}
		for (i = 0; i < apcTargets.NumElements(); i++)
		{
			psMove->asTargets.Get(i)->bChosen = apcTargets.GetPtr(i)->IsTicked();
		}
		for (i = 0; i < psMove->apsMembers.NumElements(); i++)
		{
			psMember = psMove->apsMembers.GetPtr(i);
			psMember->bChosen = apcMembers.GetPtr(i)->IsTicked();
			psMember->bAbstract = psMember->bChosen && (aiAbstract.GetValue(i) >= 0) && apcAbstract.GetPtr((size)aiAbstract.GetValue(i))->IsTicked();
		}
	}
	aiAbstract.Kill();
	apcAbstract.Kill();
	apcMembers.Kill();
	apcTargets.Kill();
	pTarget = NULL;
	mpDialog = NULL;
	return bOK;
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Pull Members Up (bUp) or Push Members Down for the class at
// the caret: its dialog, then, if anything stands in the way, Problems
// Detected, and then the move.  False if nothing was moved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMoveMembers::ShowMove(bool bUp)
{
	SIDEMembersMove	sMove;
	CArrayChars		aszConflicts;
	CChars			szMessage;
	bool			bResult;

	sMove.Init();
	if (!FindMove(bUp, &sMove))
	{
		mpcContent->ShowStatus(bUp ? "There is no class at the caret to pull members up from." : "There is no class at the caret to push members down from.");
		sMove.Kill();
		return false;
	}
	if (!sMove.bMovable)
	{
		szMessage.Init(bUp ? "Can't pull " : "Can't push ");
		GetIDEScopedName(sMove.sClass.szKey.Text(), &szMessage);
		szMessage.Append(bUp ? "'s members up: " : "'s members down: ");
		szMessage.Append(sMove.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sMove.Kill();
		return false;
	}
	bResult = ShowDialog(&sMove);
	if (bResult)
	{
		aszConflicts.Init();
		FindConflicts(&sMove, &aszConflicts);
		bResult = (aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&aszConflicts);
		aszConflicts.Kill();
		bResult = bResult && Move(&sMove);
	}
	sMove.Kill();
	return bResult;
}
