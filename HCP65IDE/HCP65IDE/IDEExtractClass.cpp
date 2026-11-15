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
#include "CppCompilerLib/CPPIndex.h"
#include "WindowLib/CheckBox.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEMoveClass.h"
#include "IDEExtractClass.h"


static const char*	gaszIDEExtractAccess[] = { "public", "protected", "private" };
static const char*	gaszIDEExtractKinds[] = { "superclass", "interface", "class" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractClass::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpsShown = NULL;
	miShownKind = IDE_EXTRACT_SUPERCLASS;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractClass::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	M_Pointer(mpNameField);
	M_Pointer(mpFieldField);
	M_Pointer(mpError);
	U_Pointer(mpsShown);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractClass::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::Save(CObjectWriter* pcFile) { return false; }
bool CIDEExtractClass::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ gives none of them a key.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractClass::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.ExtractSuperclass", "Extract Superclass...", "", Ptr<CImageCel>(), this, &CIDEExtractClass::QueueSuperclass)->SetEnabledCheck(this, &CIDEExtractClass::CanExtract);
	pCommands->Add("Refactor.ExtractInterface", "Extract Interface...", "", Ptr<CImageCel>(), this, &CIDEExtractClass::QueueInterface)->SetEnabledCheck(this, &CIDEExtractClass::CanExtract);
	pCommands->Add("Refactor.ExtractClass", "Extract Class...", "", Ptr<CImageCel>(), this, &CIDEExtractClass::QueueDelegate)->SetEnabledCheck(this, &CIDEExtractClass::CanExtract);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractClass::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ExtractSuperclass"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ExtractInterface"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ExtractClass"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::CanExtract(void)
{
	return mpcContent->GetMoveMembers()->CanMove();
}


void CIDEExtractClass::QueueSuperclass(void) { mpcContent->GetWindow()->Post(this, &CIDEExtractClass::ShowQueuedSuperclass); }
void CIDEExtractClass::QueueInterface(void) { mpcContent->GetWindow()->Post(this, &CIDEExtractClass::ShowQueuedInterface); }
void CIDEExtractClass::QueueDelegate(void) { mpcContent->GetWindow()->Post(this, &CIDEExtractClass::ShowQueuedDelegate); }
void CIDEExtractClass::ShowQueuedSuperclass(void) { ShowExtract(IDE_EXTRACT_SUPERCLASS); }
void CIDEExtractClass::ShowQueuedInterface(void) { ShowExtract(IDE_EXTRACT_INTERFACE); }
void CIDEExtractClass::ShowQueuedDelegate(void) { ShowExtract(IDE_EXTRACT_DELEGATE); }


//////////////////////////////////////////////////////////////////////////
//
// The class the caret is in and its members (Pull Members Up's), the one
// at the caret chosen.  False if there is no class there; bMovable is
// whether it has members to make a class of.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::FindExtraction(SIDEMembersMove* psDest)
{
	if (!mpcContent->GetMoveMembers()->FindMove(true, psDest))
	{
		return false;
	}
	psDest->bMovable = psDest->apsMembers.NumElements() > 0;
	psDest->szWhyNot.Set(psDest->bMovable ? "" : "it has no functions or fields");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The members chosen, as a script writes them: "Fill(int);miLevel", and
// those of them to be only declared, pure virtual, in a superclass.
// False, with why, if a name isn't one of the class's members.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::ParseMembers(SIDEMembersMove* psMove, const char* szMembers, const char* szAbstract, CChars* pszProblem)
{
	CChars	szSpec;
	bool	bResult;

	szSpec.Init("|");
	szSpec.Append(szMembers);
	szSpec.Append("|");
	szSpec.Append(szAbstract);
	bResult = mpcContent->GetMoveMembers()->ParseSpec(psMove, szSpec.Text(), pszProblem);
	szSpec.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The new class as where the members go: Pull Members Up's conflicts and
// its texts are worked out against it, though no file has it yet.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractClass::SetTarget(SIDEMembersMove* psMove, int iKind, const char* szName)
{
	SIDEMoveClass*	psTo;
	size			i;

	for (i = 0; i < psMove->asTargets.NumElements(); i++)
	{
		psMove->asTargets.Get(i)->Kill();
	}
	psMove->asTargets.ReInit();
	psTo = psMove->asTargets.Add();
	psTo->Init();
	psTo->szKey.Set(szName);
	psTo->bChosen = true;
	psMove->uiTarget = 0;
	psMove->bUp = true;
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		if (iKind == IDE_EXTRACT_INTERFACE)
		{
			psMove->apsMembers.GetPtr(i)->bAbstract = psMove->apsMembers.GetPtr(i)->bChosen;
		}
		else if (iKind == IDE_EXTRACT_DELEGATE)
		{
			psMove->apsMembers.GetPtr(i)->bAbstract = false;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Why the new class can't have a name (or, a class extracted as a field,
// the field can't): it isn't a name, something has it, its files are
// there, nothing is chosen, or an interface is asked for of a field.
// False if it can.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::FindProblem(SIDEMembersMove* psMove, int iKind, const char* szName, const char* szField, CChars* pszDest)
{
	CFileUtil			cFileUtil;
	SIDEMovedMember*	psMember;
	CChars				szPath;
	CChars				szKey;
	size				uiChosen;
	size				i;

	uiChosen = 0;
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		if (!psMember->bChosen)
		{
			continue;
		}
		uiChosen++;
		if ((iKind == IDE_EXTRACT_INTERFACE) && !psMember->bFunction)
		{
			pszDest->Set("an interface has only functions, and ");
			pszDest->Append(psMember->szName);
			pszDest->Append(" isn't one");
			return true;
		}
	}
	if (uiChosen == 0)
	{
		pszDest->Set("no member is chosen");
	}
	else if (!IsIDEIdentifier(szName))
	{
		pszDest->Set("the new class's name isn't a name");
	}
	else if (mpcContent->GetCodeModel()->HasSymbol(szName))
	{
		pszDest->Set("there is a ");
		pszDest->Append(szName);
		pszDest->Append(" already");
	}
	else if (iKind == IDE_EXTRACT_DELEGATE)
	{
		szKey.Init(psMove->sClass.szKey);
		szKey.Append("::");
		szKey.Append(szField);
		if (!IsIDEIdentifier(szField))
		{
			pszDest->Set("the field's name isn't a name");
		}
		else if (mpcContent->GetCodeModel()->HasSymbol(szKey.Text()))
		{
			pszDest->Set("the class has a ");
			pszDest->Append(szField);
			pszDest->Append(" already");
		}
		szKey.Kill();
	}
	if (pszDest->Empty())
	{
		szPath.Init();
		mpcContent->GetMoveClass()->AppendNewPath(psMove->sClass.szFileName.Text(), &szName[((szName[0] == 'C') && (szName[1] >= 'A') && (szName[1] <= 'Z')) ? 1 : 0], ".h", &szPath);
		if (cFileUtil.Exists(szPath.Text()))
		{
			pszDest->Set(GetIDEFileNamePart(szPath.Text()));
			pszDest->Append(" is there already");
		}
		szPath.Kill();
	}
	return !pszDest->Empty();
}


//////////////////////////////////////////////////////////////////////////
//
// What stands in the way: Pull Members Up's conflicts against the new
// class.  A class extracted as a field has its members public, so none
// "will be private".
//
//////////////////////////////////////////////////////////////////////////
void CIDEExtractClass::FindConflicts(SIDEMembersMove* psMove, int iKind, const char* szName, CArrayChars* paszDest)
{
	CArrayChars	aszAll;
	size		i;

	SetTarget(psMove, iKind, szName);
	aszAll.Init();
	mpcContent->GetMoveMembers()->FindConflicts(psMove, &aszAll);
	for (i = 0; i < aszAll.NumElements(); i++)
	{
		if (((iKind == IDE_EXTRACT_DELEGATE) && aszAll.Get(i)->Contains(" will be private in ")) || ((iKind == IDE_EXTRACT_INTERFACE) && aszAll.Get(i)->Contains(" can't be abstract")))
		{
			continue;
		}
		paszDest->Add(aszAll.GetText(i));
	}
	aszAll.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a line of a file is in what moves to the new class: a chosen
// member's declaration in the class, or its definition outside it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::IsMoved(SIDEMembersMove* psMove, const char* szFileName, int32 iLine)
{
	SIDEMovedMember*	psMember;
	size				i;

	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		if (!psMember->bChosen || psMember->bAbstract)
		{
			continue;
		}
		if ((StringInsensitiveCompare(szFileName, psMove->sClass.szFileName.Text()) == 0) && (iLine >= psMember->sDeclared.iLine) && (iLine <= psMember->sDeclared.iEndLine))
		{
			return true;
		}
		if (psMember->bOutside && (StringInsensitiveCompare(szFileName, psMember->szOutsideFile.Text()) == 0) && (iLine >= psMember->sOutside.iLine) && (iLine <= psMember->sOutside.iEndLine))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether what moves names a parameter of the class template the class
// is: then the new class is a template of the same parameters.  Its
// template<...> as the class writes it, and the parameters as a use of
// the new class names them ("<M>").  False, and nothing, if the class is
// no template or nothing that moves names its parameters.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::NamesParameters(SIDEMembersMove* psMove, CIDEFileEdits* pcEdits, CChars* pszTemplate, CChars* pszArguments)
{
	SIDEMovedMember*	psMember;
	CArrayChars			aszPieces;
	CArrayChars			aszNames;
	CChars				szInside;
	CChars				szPiece;
	CChars				szMoved;
	const char*			szFound;
	char*				szText;
	char*				szOutside;
	size				uiClass;
	size				uiEnd;
	size				uiStart;
	size				i;
	size				j;
	bool				bNames;

	szText = psMove->sClass.bTemplate ? pcEdits->GetText(psMove->sClass.szFileName.Text()) : NULL;
	if (!szText)
	{
		return false;
	}
	//Its template<...>: what ends just before the class.
	uiClass = FindIDETextOffset(szText, psMove->sClass.sRange.iLine, psMove->sClass.sRange.iColumn);
	for (uiEnd = uiClass; (uiEnd > 0) && ((szText[uiEnd - 1] == ' ') || (szText[uiEnd - 1] == '\t') || (szText[uiEnd - 1] == '\r') || (szText[uiEnd - 1] == '\n')); uiEnd--)
	{
	}
	if ((uiEnd == 0) || (szText[uiEnd - 1] != '>'))
	{
		return false;
	}
	for (uiStart = uiEnd; (uiStart > 0) && (strncmp(&szText[uiStart], "template", 8) != 0); uiStart--)
	{
	}
	if (strncmp(&szText[uiStart], "template", 8) != 0)
	{
		return false;
	}
	pszTemplate->AppendSubString(szText, uiStart, uiEnd);

	szInside.Init();
	szInside.AppendSubString(szText, uiStart + (size)(strchr(&szText[uiStart], '<') - &szText[uiStart]) + 1, uiEnd - 1);
	aszPieces.Init();
	szInside.Split(&aszPieces, ',');
	aszNames.Init();
	pszArguments->Append('<');
	for (i = 0; i < aszPieces.NumElements(); i++)
	{
		szPiece.Init(aszPieces.GetText(i));
		if (szPiece.Contains("="))
		{
			szPiece.SetLength(szPiece.Find('='));
		}
		szPiece.StripWhitespace();
		for (j = szPiece.Length(); (j > 0) && IsIDENameChar(szPiece.Text()[j - 1]); j--)
		{
		}
		aszNames.Add(&szPiece.Text()[j]);
		pszArguments->Append((i > 0) ? ", " : "");
		pszArguments->Append(&szPiece.Text()[j]);
		szPiece.Kill();
	}
	pszArguments->Append('>');
	aszPieces.Kill();
	szInside.Kill();

	//Does a chosen member's text name one of them?
	bNames = false;
	for (i = 0; !bNames && (i < psMove->apsMembers.NumElements()); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		if (!psMember->bChosen)
		{
			continue;
		}
		szMoved.Init();
		AppendIDERangeText(szText, &psMember->sDeclared, &szMoved);
		szOutside = psMember->bOutside ? pcEdits->GetText(psMember->szOutsideFile.Text()) : NULL;
		if (szOutside)
		{
			AppendIDERangeText(szOutside, &psMember->sDefinition.sBody, &szMoved);
		}
		for (j = 0; !bNames && (j < aszNames.NumElements()); j++)
		{
			for (szFound = strstr(szMoved.Text(), aszNames.GetText(j)); szFound && !bNames; szFound = strstr(szFound + 1, aszNames.GetText(j)))
			{
				bNames = ((szFound == szMoved.Text()) || !IsIDENameChar(szFound[-1])) && !IsIDENameChar(szFound[aszNames.Get(j)->Length()]);
			}
		}
		szMoved.Kill();
	}
	aszNames.Kill();
	if (!bNames)
	{
		pszTemplate->Clear();
		pszArguments->Clear();
	}
	return bNames;
}


//////////////////////////////////////////////////////////////////////////
//
// The new class made of the chosen members: see the class's comment.
// One undo; the files changed are saved and the projects read again, as
// after Move Class.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::Extract(SIDEMembersMove* psMove, int iKind, const char* szName, const char* szField, CChars* pszStatus)
{
	CIDECodeModel*		pcModel;
	CIDEMoveMembers*	pcMembers;
	CIDEMoveClass*		pcMoveClass;
	CIDEFileEdits		cFiles;
	CArrayIDELocations	asLocations;
	SIDEMovedMember*	psMember;
	SIDEMoveClass*		psTo;
	SIDELocation*		psLocation;
	CChars				aszSections[3];
	CChars				szTemplate;
	CChars				szArguments;
	CChars				szUsed;
	CChars				szHeaderBlocks;
	CChars				szSourceBlocks;
	CChars				szSourceFile;
	CChars				szNewHeader;
	CChars				szNewSource;
	CChars				szHeaderText;
	CChars				szSourceText;
	CChars				szGuard;
	CChars				szDeclaration;
	CChars				szBlock;
	CChars				szLine;
	CChars				szMessage;
	const char*			szStem;
	char*				szText;
	char*				szOutside;
	char*				szUser;
	size				uiStart;
	size				uiEnd;
	size				uiAt;
	size				uiBrace;
	size				uiMoved;
	size				i;
	size				j;
	int					iAccess;
	uint8				uiKind;
	bool				bTemplate;
	bool				bInline;
	bool				bFieldPut;
	bool				bBases;
	bool				bPragma;
	bool				bComment;
	bool				bResult;

	pcModel = mpcContent->GetCodeModel();
	pcMembers = &mpcContent->GetMoveMembers();
	pcMoveClass = &mpcContent->GetMoveClass();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	SetTarget(psMove, iKind, szName);
	cFiles.Init(mpcContent);
	szText = cFiles.GetText(psMove->sClass.szFileName.Text());
	if (!szText)
	{
		cFiles.Kill();
		return false;
	}
	szStem = &szName[((szName[0] == 'C') && (szName[1] >= 'A') && (szName[1] <= 'Z')) ? 1 : 0];
	szTemplate.Init();
	szArguments.Init();
	bTemplate = NamesParameters(psMove, &cFiles, &szTemplate, &szArguments);
	psTo = psMove->asTargets.Get(0);
	psTo->szKey.Append(szArguments);
	psTo->bTemplate = bTemplate;
	szUsed.Init(szName);
	szUsed.Append(szArguments);

	//Each chosen member: into the new class, under its access; out of the class unless it is only declared there.
	for (i = 0; i < 3; i++)
	{
		aszSections[i].Init();
	}
	szHeaderBlocks.Init();
	szSourceBlocks.Init();
	szSourceFile.Init();
	uiMoved = 0;
	bFieldPut = false;
	for (i = 0; i < psMove->apsMembers.NumElements(); i++)
	{
		psMember = psMove->apsMembers.GetPtr(i);
		if (!psMember->bChosen || !psMember->szWhyNot.Empty())
		{
			continue;
		}
		uiMoved++;
		szDeclaration.Init();
		if (psMember->bAbstract)
		{
			pcMembers->AppendPureText(psMember, szText, &szDeclaration);
		}
		else
		{
			pcMembers->AppendDeclarationText(psMove, psMember, psTo, &cFiles, &szDeclaration);
		}
		iAccess = (iKind == IDE_EXTRACT_DELEGATE) ? IDE_ACCESS_PUBLIC : psMember->iAccess;
		aszSections[iAccess].Append(szDeclaration);
		szDeclaration.Kill();
		if (psMember->bAbstract)
		{
			continue;
		}

		uiStart = FindIDETextOffset(szText, psMember->sDeclared.iLine, psMember->sDeclared.iColumn);
		uiEnd = FindIDETextOffset(szText, psMember->sDeclared.iEndLine, psMember->sDeclared.iEndColumn);
		if ((iKind == IDE_EXTRACT_DELEGATE) && !bFieldPut)
		{
			//The field of the new class, where the first of them was.
			for (; (szText[uiEnd] == ' ') || (szText[uiEnd] == '\t') || (szText[uiEnd] == ';'); uiEnd++)
			{
			}
			szLine.Init(szUsed);
			szLine.Append('\t');
			szLine.Append(szField);
			szLine.Append(';');
			cFiles.GetEdits(psMove->sClass.szFileName.Text())->Replace(uiStart, uiEnd, szLine.Text());
			szLine.Kill();
			bFieldPut = true;
		}
		else
		{
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			cFiles.GetEdits(psMove->sClass.szFileName.Text())->Replace(uiStart, uiEnd, "");
		}

		szOutside = psMember->bOutside ? cFiles.GetText(psMember->szOutsideFile.Text()) : NULL;
		if (szOutside)
		{
			bInline = psMember->bFunction && (psMove->sClass.bTemplate || psTo->bTemplate);
			if (!bInline)
			{
				szBlock.Init();
				pcMembers->AppendOutsideText(psMember, psTo, szOutside, &szBlock);
				if (StringInsensitiveCompare(psMember->szOutsideFile.Text(), psMove->sClass.szFileName.Text()) == 0)
				{
					szHeaderBlocks.Append("\n\n");
					szHeaderBlocks.Append(szBlock);
				}
				else
				{
					szSourceBlocks.Append("\n\n");
					szSourceBlocks.Append(szBlock);
					if (szSourceFile.Empty())
					{
						szSourceFile.Set(psMember->szOutsideFile);
					}
				}
				szBlock.Kill();
			}
			uiStart = FindIDETextOffset(szOutside, psMember->sOutside.iLine, psMember->sOutside.iColumn);
			uiEnd = FindIDETextOffset(szOutside, psMember->sOutside.iEndLine, psMember->sOutside.iEndColumn);
			ExtendIDEDeletion(szOutside, &uiStart, &uiEnd, true);
			cFiles.GetEdits(psMember->szOutsideFile.Text())->Replace(uiStart, uiEnd, "");
		}

		//A class had as a field: each place the member is named, outside what moves, names it through the field.
		if (iKind == IDE_EXTRACT_DELEGATE)
		{
			asLocations.Init();
			pcModel->GetSymbolLocations(psMember->szKey.Text(), &asLocations, &uiKind);
			for (j = 0; j < asLocations.NumElements(); j++)
			{
				psLocation = asLocations.Get(j);
				if ((psLocation->uiRole != CPPIR_Reference) || IsMoved(psMove, psLocation->szFileName.Text(), psLocation->iLine) || (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE))
				{
					continue;
				}
				szUser = cFiles.GetText(psLocation->szFileName.Text());
				if (szUser)
				{
					szLine.Init(szField);
					szLine.Append('.');
					cFiles.GetEdits(psLocation->szFileName.Text())->Insert(FindIDETextOffset(szUser, psLocation->iLine, psLocation->iColumn), szLine.Text());
					szLine.Kill();
				}
			}
			KillIDELocations(&asLocations);
		}
	}

	//The class derives from the new class (a superclass, an interface), and its header #includes the new one.
	uiAt = FindIDETextOffset(szText, psMove->sClass.sRange.iLine, psMove->sClass.sRange.iColumn);
	for (uiBrace = uiAt; (szText[uiBrace] != '\0') && (szText[uiBrace] != '{'); uiBrace++)
	{
	}
	if (iKind != IDE_EXTRACT_DELEGATE)
	{
		bBases = false;
		for (i = uiAt; i < uiBrace; i++)
		{
			bBases = bBases || ((szText[i] == ':') && (szText[i + 1] != ':') && (szText[i - 1] != ':'));
		}
		for (uiAt = uiBrace; (uiAt > 0) && ((szText[uiAt - 1] == ' ') || (szText[uiAt - 1] == '\t') || (szText[uiAt - 1] == '\r') || (szText[uiAt - 1] == '\n')); uiAt--)
		{
		}
		szLine.Init(bBases ? ", public " : " : public ");
		szLine.Append(szUsed);
		cFiles.GetEdits(psMove->sClass.szFileName.Text())->Insert(uiAt, szLine.Text());
		szLine.Kill();
	}
	szNewHeader.Init();
	pcMoveClass->AppendNewPath(psMove->sClass.szFileName.Text(), szStem, ".h", &szNewHeader);
	szNewSource.Init();
	if (!szSourceBlocks.Empty())
	{
		pcMoveClass->AppendNewPath(szSourceFile.Text(), szStem, ".cpp", &szNewSource);
	}
	szLine.Init();
	pcMoveClass->FindIncludePlace(szText, "", GetIDEFileNamePart(szNewHeader.Text()), &uiAt, &szLine);
	if (!szLine.Empty())
	{
		cFiles.GetEdits(psMove->sClass.szFileName.Text())->Insert(uiAt, szLine.Text());
	}
	szLine.Kill();

	//The new header: the class's header's opening comment, a guard, its #includes, and the new class.
	szHeaderText.Init();
	szGuard.Init();
	pcMoveClass->AppendGuard(szText, psMove->sClass.szFileName.Text(), szStem, &szGuard, &bPragma, &bComment);
	szHeaderText.AppendSubString(szText, 0, pcMoveClass->FindOpeningEnd(szText));
	if (bPragma)
	{
		szHeaderText.Append("#pragma once\n");
	}
	else
	{
		szHeaderText.Append("#ifndef ")->Append(szGuard)->Append("\n#define ")->Append(szGuard)->Append('\n');
	}
	pcMoveClass->AppendIncludes(szText, &szHeaderText);
	szHeaderText.Append("\n\n");
	if (bTemplate)
	{
		szHeaderText.Append(szTemplate)->Append('\n');
	}
	szHeaderText.Append("class ")->Append(szName)->Append("\n{\n");
	bInline = false;
	for (i = 3; i > 0; i--)
	{
		//Private, protected, public: as classes here are written.
		if (!aszSections[i - 1].Empty())
		{
			szHeaderText.Append(bInline ? "\n" : "");
			szHeaderText.Append(gaszIDEExtractAccess[i - 1])->Append(":\n")->Append(aszSections[i - 1]);
			bInline = true;
		}
	}
	szHeaderText.Append("};\n");
	szHeaderText.Append(szHeaderBlocks);
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

	//Its source file, if anything of it is defined in one.
	szSourceText.Init();
	szOutside = szNewSource.Empty() ? NULL : cFiles.GetText(szSourceFile.Text());
	if (szOutside)
	{
		szSourceText.AppendSubString(szOutside, 0, pcMoveClass->FindOpeningEnd(szOutside));
		szSourceText.Append("#include \"")->Append(GetIDEFileNamePart(szNewHeader.Text()))->Append("\"\n");
		pcMoveClass->AppendIncludes(szOutside, &szSourceText);
		szSourceText.Append(szSourceBlocks);
	}

	bResult = (uiMoved > 0) && pcMoveClass->CreateFile(szNewHeader.Text(), psMove->sClass.szFileName.Text());
	bResult = bResult && (szNewSource.Empty() || pcMoveClass->CreateFile(szNewSource.Text(), szSourceFile.Text()));
	bResult = bResult && cFiles.GetText(szNewHeader.Text()) && (szNewSource.Empty() || cFiles.GetText(szNewSource.Text()));
	if (bResult)
	{
		cFiles.GetEdits(szNewHeader.Text())->Insert(0, szHeaderText.Text());
		if (!szNewSource.Empty())
		{
			cFiles.GetEdits(szNewSource.Text())->Insert(0, szSourceText.Text());
		}
		cFiles.Apply();
		mpcContent->GetDocuments()->SaveAllFiles();
		mpcContent->GetCodeModel()->Reset();
		mpcContent->GetSolutions()->WorkspaceChanged(true);

		szMessage.Init("Extracted the ");
		szMessage.Append(gaszIDEExtractKinds[iKind]);
		szMessage.Append(' ');
		szMessage.Append(szName);
		szMessage.Append(" from ");
		GetIDEScopedName(psMove->sClass.szKey.Text(), &szMessage);
		szMessage.Append(": ");
		szMessage.Append((uint32)uiMoved);
		szMessage.Append((uiMoved == 1) ? " member, in " : " members, in ");
		szMessage.Append(GetIDEFileNamePart(szNewHeader.Text()));
		if (!szNewSource.Empty())
		{
			szMessage.Append(" and ");
			szMessage.Append(GetIDEFileNamePart(szNewSource.Text()));
		}
		szMessage.Append(".");
		if (pszStatus)
		{
			pszStatus->Set(szMessage);
		}
		else
		{
			mpcContent->ShowStatus(szMessage.Text());
		}
		szMessage.Kill();
	}
	szSourceText.Kill();
	szHeaderText.Kill();
	szNewSource.Kill();
	szNewHeader.Kill();
	szSourceFile.Kill();
	szSourceBlocks.Kill();
	szHeaderBlocks.Kill();
	for (i = 0; i < 3; i++)
	{
		aszSections[i].Kill();
	}
	szUsed.Kill();
	szArguments.Kill();
	szTemplate.Kill();
	cFiles.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog says why the names written can't be had.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::ApproveName(void)
{
	CChars	szProblem;
	bool	bProblem;

	szProblem.Init();
	bProblem = FindProblem(mpsShown, miShownKind, mpNameField->GetText(), mpFieldField.IsNotNull() ? mpFieldField->GetText() : "", &szProblem);
	if (bProblem && szProblem.Equals("no member is chosen"))
	{
		//What is chosen is ticked in the dialog, and looked at when it is done.
		bProblem = false;
		szProblem.Clear();
	}
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
// Extract Superclass, Interface or Class for the class at the caret: its
// dialog (the new class's name, the field's for a class, and a tick for
// each member), Problems Detected if anything stands in the way, then the
// extraction.  False if nothing changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExtractClass::ShowExtract(int iKind)
{
	SIDEMembersMove					sMove;
	CArrayTemplatePtr<CCheckBox>	apcMembers;
	CArrayChars						aszConflicts;
	Ptr<CCheckBox>					pBox;
	CChars							szMessage;
	CChars							szName;
	CChars							szField;
	CChars							szProblem;
	size							i;
	bool							bResult;

	sMove.Init();
	if (!FindExtraction(&sMove))
	{
		mpcContent->ShowStatus("There is no class at the caret to extract from.");
		sMove.Kill();
		return false;
	}
	szMessage.Init();
	GetIDEScopedName(sMove.sClass.szKey.Text(), &szMessage);
	if (!sMove.bMovable)
	{
		szMessage.Insert(0, "Can't extract from ");
		szMessage.Append(": ");
		szMessage.Append(sMove.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sMove.Kill();
		return false;
	}
	szMessage.Insert(0, (iKind == IDE_EXTRACT_SUPERCLASS) ? "Extract Superclass from " : ((iKind == IDE_EXTRACT_INTERFACE) ? "Extract Interface from " : "Extract Class from "));
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	mpsShown = &sMove;
	miShownKind = iKind;
	mpNameField = mpDialog->AddTextField("New class:", "");
	mpFieldField = NULL;
	if (iKind == IDE_EXTRACT_DELEGATE)
	{
		mpFieldField = mpDialog->AddTextField("Field:", "");
	}
	mpDialog->AddDivider("Members");
	apcMembers.Init();
	for (i = 0; i < sMove.apsMembers.NumElements(); i++)
	{
		pBox = mpDialog->AddCheckBox(sMove.apsMembers.GetPtr(i)->szName.Text(), sMove.apsMembers.GetPtr(i)->bChosen);
		apcMembers.Add(&pBox);
	}
	pBox = NULL;
	mpError = OMalloc<CLabel>(mpDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpDialog->AddEditor("", mpError);
	mpDialog->SetOKCaption("Refactor");
	mpDialog->SetApprover(this, &CIDEExtractClass::ApproveName);
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(560);
	szName.Init(bResult ? mpNameField->GetText() : "");
	szField.Init((bResult && mpFieldField.IsNotNull()) ? mpFieldField->GetText() : "");
	for (i = 0; bResult && (i < apcMembers.NumElements()); i++)
	{
		sMove.apsMembers.GetPtr(i)->bChosen = apcMembers.GetPtr(i)->IsTicked();
	}
	apcMembers.Kill();
	mpDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpNameField = NULL;
	mpFieldField = NULL;
	mpError = NULL;
	mpDialog = NULL;
	mpsShown = NULL;
	if (bResult)
	{
		szProblem.Init();
		if (FindProblem(&sMove, iKind, szName.Text(), szField.Text(), &szProblem))
		{
			szProblem.Insert(0, "Can't extract it: ");
			szProblem.Append(".");
			mpcContent->ShowStatus(szProblem.Text());
			bResult = false;
		}
		szProblem.Kill();
	}
	if (bResult)
	{
		aszConflicts.Init();
		FindConflicts(&sMove, iKind, szName.Text(), &aszConflicts);
		bResult = (aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&aszConflicts);
		aszConflicts.Kill();
		bResult = bResult && Extract(&sMove, iKind, szName.Text(), szField.Text());
	}
	szField.Kill();
	szName.Kill();
	sMove.Kill();
	return bResult;
}
