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
#include "CppCompilerLib/CPPSyntaxEnumSpecifier.h"
#include "CppCompilerLib/CPPSyntaxEnumerator.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDEDefinitions.h"
#include "IDEContextActions.h"
#include "IDEConvertEnum.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEEnumConversion::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szKey.Init();
	szName.Init();
	uiEnumerators = 0;
	aszEnumFiles.Init();
	aiEnumAts.Init();
	aszUseFiles.Init();
	aiUseAts.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEEnumConversion::Kill(void)
{
	aiUseAts.Kill();
	aszUseFiles.Kill();
	aiEnumAts.Kill();
	aszEnumFiles.Kill();
	szName.Kill();
	szKey.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::Class(void)
{
	U_Pointer(mpcContent);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::Free(void)
{
}


bool CIDEConvertEnum::Save(CObjectWriter* pcFile) { return false; }
bool CIDEConvertEnum::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Refactor's Convert Enum to Enum Class.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.EnumToEnumClass", "Convert Enum to Enum Class", "", Ptr<CImageCel>(), this, &CIDEConvertEnum::QueueConvert)->SetEnabledCheck(this, &CIDEConvertEnum::CanConvert);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.EnumToEnumClass"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: it is for an enum or an enumerator.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bSymbol && ((psContext->uiKind == CPPSK_Enum) || (psContext->uiKind == CPPSK_Enumerator)))
	{
		psContext->Offer("Refactor.EnumToEnumClass");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::CanConvert(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::QueueConvert(void) { mpcContent->GetWindow()->Post(this, &CIDEConvertEnum::ConvertQueued); }
void CIDEConvertEnum::ConvertQueued(void) { ConvertAtCaret(); }


//////////////////////////////////////////////////////////////////////////
//
// Why it can't be, with the place if there is one: "(Tank.cpp line 12)".
// Returns true: something was found, though it can't be converted.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::Refuse(SIDEEnumConversion* psDest, const char* szWhyNot, const char* szFileName, int32 iLine)
{
	psDest->bPossible = false;
	if (psDest->szWhyNot.Empty())
	{
		psDest->szWhyNot.Set(szWhyNot);
		if (szFileName)
		{
			psDest->szWhyNot.Append(" (")->Append(GetIDEFileNamePart(szFileName));
			if (iLine >= 0)
			{
				psDest->szWhyNot.Append(" line ")->Append((int32)(iLine + 1));
			}
			psDest->szWhyNot.Append(")");
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether "enum" is written before a name at uiName, and where it ends.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::FindEnumKeyword(const char* szText, size uiName, size* puiKeywordEnd)
{
	size	i;

	for (i = uiName; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t') || (szText[i - 1] == '\r') || (szText[i - 1] == '\n')); i--)
	{
	}
	if ((i < 4) || (i == uiName) || (strncmp(&szText[i - 4], "enum", 4) != 0) || ((i > 4) && IsIDENameChar(szText[i - 5])))
	{
		return false;
	}
	*puiKeywordEnd = i;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the name at uiName is written after the enum's name and "::".
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::IsQualified(const char* szText, size uiName, const char* szEnumName)
{
	size	i;
	size	uiEnd;

	for (i = uiName; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t')); i--)
	{
	}
	if ((i < 2) || (szText[i - 1] != ':') || (szText[i - 2] != ':'))
	{
		return false;
	}
	for (i -= 2; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t')); i--)
	{
	}
	uiEnd = i;
	for (; (i > 0) && IsIDENameChar(szText[i - 1]); i--)
	{
	}
	return (uiEnd - i == strlen(szEnumName)) && (strncmp(&szText[i], szEnumName, uiEnd - i) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The places an enumerator is named that are given the enum's name: not
// those in the enum's own list, nor one written with it already.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertEnum::FindUses(const char* szEnumeratorKey, const char* szEnumerator, const char* szEnumFile, size uiListStart, size uiListEnd, SIDEEnumConversion* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CTextFile			cFile;
	char*				szText;
	size				uiLength;
	size				uiAt;
	size				i;
	uint8				uiKind;

	pcModel = mpcContent->GetCodeModel();
	asLocations.Init();
	pcModel->GetSymbolLocations(szEnumeratorKey, &asLocations, &uiKind);
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole != CPPIR_Reference)
		{
			continue;
		}
		cFile.Init();
		szText = NULL;
		if (!mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength) || !szText)
		{
			Refuse(psDest, "a file that names one of its enumerators can't be read", psLocation->szFileName.Text());
			cFile.Kill();
			continue;
		}
		uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
		if ((strncmp(&szText[uiAt], szEnumerator, strlen(szEnumerator)) != 0) || IsIDENameChar(szText[uiAt + strlen(szEnumerator)]))
		{
			Refuse(psDest, "a macro names one of its enumerators", psLocation->szFileName.Text(), psLocation->iLine);
		}
		else if ((StringInsensitiveCompare(psLocation->szFileName.Text(), szEnumFile) == 0) && (uiAt >= uiListStart) && (uiAt < uiListEnd))
		{
			//In its own list: the enumerators before one are in reach there.
		}
		else if (IsQualified(szText, uiAt, psDest->szName.Text()))
		{
		}
		else if (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE)
		{
			Refuse(psDest, "a file that isn't one of the solution's names one of its enumerators", psLocation->szFileName.Text(), psLocation->iLine);
		}
		else
		{
			psDest->aszUseFiles.Add(psLocation->szFileName.Text());
			psDest->aiUseAts.Add((int)uiAt);
		}
		cFile.Kill();
	}
	KillIDELocations(&asLocations);
}


//////////////////////////////////////////////////////////////////////////
//
// What Convert Enum to Enum Class has to work with, for the enum at the
// caret or the enum of the enumerator there.  False if neither is there;
// psDest->bPossible says whether it can be converted, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::FindConversion(SIDEEnumConversion* psDest)
{
	CIDECodeModel*				pcModel;
	Ptr<CIDEEditorTab>			pTab;
	CIDERefactorUnit			cUnit;
	CArrayIDELocations			asLocations;
	SIDELocation*				psLocation;
	SIDELocation*				psDefined;
	CCPPSyntax*					pcNode;
	CCPPSyntaxEnumSpecifier*	pcSpecifier;
	CCPPSyntaxEnumerator*		pcEnumerator;
	SCPPSyntaxRange				sRange;
	CTextFile					cFile;
	CChars						szKey;
	CChars						szEnumFile;
	CChars						szEnumeratorKey;
	CChars						szEnumerator;
	char*						szText;
	size						uiLength;
	size						uiListStart;
	size						uiListEnd;
	size						uiAt;
	size						uiEnd;
	size						i;
	int							iLine;
	int							iColumn;
	uint8						uiRole;
	uint8						uiKind;

	pcModel = mpcContent->GetCodeModel();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szKey.Init();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) || !pcModel->GetSymbolAt(pTab->GetFileName(), iLine, iColumn, &szKey, &uiKind) ||
		((uiKind != CPPSK_Enum) && (uiKind != CPPSK_Enumerator)))
	{
		szKey.Kill();
		return false;
	}
	GetIDESymbolName(szKey.Text(), &psDest->szName);
	psDest->bPossible = true;

	//Where it (or the enumerator) is defined, and the enum's syntax there.
	asLocations.Init();
	pcModel->GetSymbolLocations(szKey.Text(), &asLocations, &uiKind);
	psDefined = NULL;
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole == CPPIR_Definition) || ((psLocation->uiRole == CPPIR_Declaration) && !psDefined))
		{
			psDefined = psLocation;
			if (psLocation->uiRole == CPPIR_Definition)
			{
				break;
			}
		}
	}
	szEnumFile.Init(psDefined ? psDefined->szFileName.Text() : "");
	cUnit.Init();
	cFile.Init();
	szText = NULL;
	pcSpecifier = NULL;
	if (psDefined && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), szEnumFile.Text(), false) && mpcContent->GetFindInFiles()->GetFileText(szEnumFile.Text(), &cFile, &szText, &uiLength) && szText)
	{
		pcNode = cUnit.FindNodeAt(psDefined->iLine, psDefined->iColumn);
		pcSpecifier = pcNode ? (CCPPSyntaxEnumSpecifier*)cUnit.FindAncestor(pcNode, CPPS_EnumSpecifier) : NULL;
	}
	KillIDELocations(&asLocations);
	uiListStart = 0;
	uiListEnd = 0;
	if (!pcSpecifier || !pcSpecifier->mbBody || !cUnit.GetRange(pcSpecifier, &sRange))
	{
		Refuse(psDest, "its definition can't be read (a macro makes it, or it isn't in the solution)");
	}
	else
	{
		uiListStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
		uiListEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		if (!pcSpecifier->mpcName || !pcSpecifier->mpcName->GetLast() || !cUnit.GetTokensRange(pcSpecifier->mpcName->GetLast()->miFirstToken, pcSpecifier->mpcName->GetLast()->miFirstToken, &sRange) ||
			!pcModel->GetSymbolAt(szEnumFile.Text(), sRange.iLine, sRange.iColumn, &psDest->szKey, &uiKind) || (uiKind != CPPSK_Enum))
		{
			psDest->szName.Set("The enum");
			Refuse(psDest, "it has no name");
		}
		else
		{
			psDest->szName.Clear();
			GetIDESymbolName(psDest->szKey.Text(), &psDest->szName);
			if (pcSpecifier->mbScoped)
			{
				Refuse(psDest, "it is an enum class already");
			}
		}
	}

	//Its declarations: each "enum" becomes "enum class".
	if (psDest->bPossible)
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
		for (i = 0; i < asLocations.NumElements(); i++)
		{
			psLocation = asLocations.Get(i);
			if (psLocation->uiRole == CPPIR_Reference)
			{
				continue;
			}
			cFile.Kill();
			cFile.Init();
			szText = NULL;
			if (!mpcContent->GetFindInFiles()->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength) || !szText || (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE))
			{
				Refuse(psDest, "it is declared in a file that isn't one of the solution's", psLocation->szFileName.Text(), psLocation->iLine);
				continue;
			}
			uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
			if (!FindEnumKeyword(szText, uiAt, &uiEnd))
			{
				Refuse(psDest, "a declaration of it can't be read (a macro makes it, or it is written with a scope)", psLocation->szFileName.Text(), psLocation->iLine);
				continue;
			}
			psDest->aszEnumFiles.Add(psLocation->szFileName.Text());
			psDest->aiEnumAts.Add((int)uiEnd);
		}
		KillIDELocations(&asLocations);
	}

	//Its enumerators' uses.
	for (i = 0; psDest->bPossible && (i < pcSpecifier->macpEnumerators.NumElements()); i++)
	{
		pcEnumerator = (CCPPSyntaxEnumerator*)pcSpecifier->macpEnumerators.GetPtr(i);
		szEnumeratorKey.Init();
		szEnumerator.Init();
		if (!cUnit.GetTokensRange(pcEnumerator->miName, pcEnumerator->miName, &sRange) || !pcModel->GetSymbolAt(szEnumFile.Text(), sRange.iLine, sRange.iColumn, &szEnumeratorKey, &uiKind))
		{
			Refuse(psDest, "one of its enumerators can't be read (a macro makes it)");
		}
		else
		{
			cUnit.AppendTokens(pcEnumerator->miName, pcEnumerator->miName, &szEnumerator);
			FindUses(szEnumeratorKey.Text(), szEnumerator.Text(), szEnumFile.Text(), uiListStart, uiListEnd, psDest);
			psDest->uiEnumerators++;
		}
		szEnumerator.Kill();
		szEnumeratorKey.Kill();
	}
	cFile.Kill();
	cUnit.Kill();
	szEnumFile.Kill();
	szKey.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// How many files a list of them names.
//
//////////////////////////////////////////////////////////////////////////
static size CountIDEEnumFiles(CArrayChars* paszFiles, CArrayChars* paszMore)
{
	CArrayChars	aszSeen;
	size		uiFiles;
	size		i;

	aszSeen.Init();
	for (i = 0; i < paszFiles->NumElements(); i++)
	{
		aszSeen.AddIfUnique(paszFiles->GetText(i));
	}
	for (i = 0; paszMore && (i < paszMore->NumElements()); i++)
	{
		aszSeen.AddIfUnique(paszMore->GetText(i));
	}
	uiFiles = aszSeen.NumElements();
	aszSeen.Kill();
	return uiFiles;
}


//////////////////////////////////////////////////////////////////////////
//
// The enum made an enum class and its enumerators' uses given its name, if
// the files compile that way.  One undo across them.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::Convert(SIDEEnumConversion* psConversion, CChars* pszStatus)
{
	CIDEFileEdits	cFiles;
	CChars			szMessage;
	CChars			szPrefix;
	size			uiBefore;
	size			uiAfter;
	size			i;
	bool			bResult;

	szMessage.Init();
	bResult = psConversion->bPossible;
	cFiles.Init(mpcContent);
	if (!bResult)
	{
		szMessage.Append("Can't make ")->Append(psConversion->szName)->Append(" an enum class: ")->Append(psConversion->szWhyNot)->Append('.');
	}
	szPrefix.Init(psConversion->szName);
	szPrefix.Append("::");
	for (i = 0; bResult && (i < psConversion->aszEnumFiles.NumElements()); i++)
	{
		bResult = (cFiles.GetText(psConversion->aszEnumFiles.GetText(i)) != NULL);
		if (bResult)
		{
			cFiles.GetEdits(psConversion->aszEnumFiles.GetText(i))->Insert((size)psConversion->aiEnumAts.GetValue(i), " class");
		}
	}
	for (i = 0; bResult && (i < psConversion->aszUseFiles.NumElements()); i++)
	{
		bResult = (cFiles.GetText(psConversion->aszUseFiles.GetText(i)) != NULL);
		if (bResult)
		{
			cFiles.GetEdits(psConversion->aszUseFiles.GetText(i))->Insert((size)psConversion->aiUseAts.GetValue(i), szPrefix.Text());
		}
	}
	szPrefix.Kill();
	if (!bResult && szMessage.Empty())
	{
		szMessage.Append("Can't make ")->Append(psConversion->szName)->Append(" an enum class: a file it is in couldn't be opened.");
	}
	if (bResult)
	{
		uiBefore = cFiles.CountErrors(false);
		uiAfter = cFiles.CountErrors(true);
		if (uiAfter > uiBefore)
		{
			bResult = false;
			szMessage.Append("Can't make ")->Append(psConversion->szName)->Append(" an enum class: its values are used as integers, or integers as its values (");
			szMessage.Append((uint32)(uiAfter - uiBefore))->Append((uiAfter - uiBefore == 1) ? " error" : " errors")->Append(" with it one).");
		}
	}
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Made ")->Append(psConversion->szName)->Append(" an enum class: ")->Append((uint32)psConversion->aszUseFiles.NumElements());
		szMessage.Append((psConversion->aszUseFiles.NumElements() == 1) ? " use of its enumerators given its name, in " : " uses of its enumerators given its name, in ");
		i = CountIDEEnumFiles(&psConversion->aszEnumFiles, &psConversion->aszUseFiles);
		szMessage.Append((uint32)i)->Append((i == 1) ? " file." : " files.");
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
// The command: the enum at the caret found and converted.  False, with
// pszStatus left empty, if there is none there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertEnum::ConvertAtCaret(CChars* pszStatus)
{
	SIDEEnumConversion	sConversion;
	bool				bResult;

	sConversion.Init();
	if (!FindConversion(&sConversion))
	{
		if (!pszStatus)
		{
			mpcContent->ShowStatus("There is no enum at the caret to make an enum class.");
		}
		sConversion.Kill();
		return false;
	}
	bResult = Convert(&sConversion, pszStatus);
	sConversion.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEConvertEnum::PrintConversion(SIDEEnumConversion* psConversion, CChars* pszDest)
{
	size	uiFiles;

	pszDest->Append("Convert Enum: ")->Append(psConversion->szName);
	if (!psConversion->bPossible)
	{
		pszDest->Append(" can't be: ")->Append(psConversion->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	uiFiles = CountIDEEnumFiles(&psConversion->aszUseFiles, NULL);
	pszDest->Append(", ")->Append((uint32)psConversion->uiEnumerators)->Append((psConversion->uiEnumerators == 1) ? " enumerator, " : " enumerators, ");
	pszDest->Append((uint32)psConversion->aszEnumFiles.NumElements())->Append((psConversion->aszEnumFiles.NumElements() == 1) ? " declaration, " : " declarations, ");
	pszDest->Append((uint32)psConversion->aszUseFiles.NumElements())->Append((psConversion->aszUseFiles.NumElements() == 1) ? " use in " : " uses in ");
	pszDest->Append((uint32)uiFiles)->Append((uiFiles == 1) ? " file\n" : " files\n");
	return pszDest->Text();
}
