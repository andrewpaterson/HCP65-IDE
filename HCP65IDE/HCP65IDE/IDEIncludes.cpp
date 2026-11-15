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
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/PointerFunctions.h"
#include "IDEContent.h"
#include "IDEDefinitions.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEIncludes.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEIncludeUse::Init(void)
{
	iWhat = IDE_INCLUDE_KEPT;
	szWritten.Init();
	szWhy.Init();
	aszClasses.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEIncludeUse::Kill(void)
{
	aszClasses.Kill();
	szWhy.Kill();
	szWritten.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEIncludePlan::Init(void)
{
	szFileName.Init();
	asIncludes.Init();
	asUses.Init();
	aszAdded.Init();
	aszAddedWhy.Init();
	aszDeclared.Init();
	aszDependants.Init();
	aszDependantHeaders.Init();
	uiErrors = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEIncludePlan::Kill(void)
{
	size	i;

	aszDependantHeaders.Kill();
	aszDependants.Kill();
	aszDeclared.Kill();
	aszAddedWhy.Kill();
	aszAdded.Kill();
	for (i = 0; i < asUses.NumElements(); i++)
	{
		asUses.Get(i)->Kill();
	}
	asUses.Kill();
	KillIDEIncludes(&asIncludes);
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludeGraph::Init(CIDECodeModel* pcModel, CIDEFileTexts* pcTexts)
{
	mpcModel = pcModel;
	mpcTexts = pcTexts;
	masFiles.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludeGraph::Kill(void)
{
	size	i;

	for (i = 0; i < masFiles.NumElements(); i++)
	{
		masFiles.Get(i)->aiIncludes.Kill();
		masFiles.Get(i)->szFileName.Kill();
	}
	masFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file's number here, given one if it has none.
//
//////////////////////////////////////////////////////////////////////////
int CIDEIncludeGraph::Get(const char* szFileName)
{
	SIDEGraphFile*	psFile;
	size			i;

	for (i = 0; i < masFiles.NumElements(); i++)
	{
		if (StringInsensitiveCompare(masFiles.Get(i)->szFileName.Text(), szFileName) == 0)
		{
			return (int)i;
		}
	}
	psFile = masFiles.Add();
	psFile->szFileName.Init(szFileName);
	psFile->bRead = false;
	psFile->bOpaque = false;
	psFile->aiIncludes.Init();
	return (int)(masFiles.NumElements() - 1);
}


char* CIDEIncludeGraph::GetFileName(int iFile) { return masFiles.Get((size)iFile)->szFileName.Text(); }
bool CIDEIncludeGraph::IsOpaque(int iFile) { return masFiles.Get((size)iFile)->bOpaque; }


//////////////////////////////////////////////////////////////////////////
//
// The files a file #includes, read from its text once.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludeGraph::Read(int iFile)
{
	CTextFile			cFile;
	CArrayIDEIncludes	asIncludes;
	SIDEInclude*		psInclude;
	CChars				szFileName;
	char*				szText;
	size				uiLength;
	size				i;
	int					iIncluded;

	if (masFiles.Get((size)iFile)->bRead)
	{
		return;
	}
	masFiles.Get((size)iFile)->bRead = true;
	if (masFiles.Get((size)iFile)->bOpaque)
	{
		return;
	}

	szFileName.Init(masFiles.Get((size)iFile)->szFileName);
	cFile.Init();
	if (mpcTexts->GetFileText(szFileName.Text(), &cFile, &szText, &uiLength))
	{
		asIncludes.Init();
		mpcModel->FindIncludesIn(szFileName.Text(), szText, &asIncludes);
		for (i = 0; i < asIncludes.NumElements(); i++)
		{
			psInclude = asIncludes.Get(i);
			if (psInclude->szFileName.Empty())
			{
				continue;
			}
			iIncluded = Get(psInclude->szFileName.Text());
			if (psInclude->bSystem && !masFiles.Get((size)iIncluded)->bRead)
			{
				masFiles.Get((size)iIncluded)->bOpaque = true;
			}
			masFiles.Get((size)iFile)->aiIncludes.AddIfUnique(iIncluded);
		}
		KillIDEIncludes(&asIncludes);
	}
	cFile.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Every file iFrom reaches through #includes (not itself, unless one of
// them #includes it), added to paiDest.  The #includes that iCutFrom has
// of the files of paiCutTo aren't followed (if it isn't NULL): what would
// be reached without them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludeGraph::FindReached(int iFrom, int iCutFrom, CArrayInt* paiCutTo, CArrayInt* paiDest)
{
	CArrayInt	aiStack;
	CArrayChar	acSeen;
	size		i;
	size		j;
	int			iFile;
	int			iIncluded;
	bool		bCut;

	acSeen.Init();
	aiStack.Init();
	aiStack.Add(iFrom);
	while (aiStack.NumElements() > 0)
	{
		iFile = aiStack.GetValue(aiStack.NumElements() - 1);
		aiStack.RemoveAt(aiStack.NumElements() - 1);
		Read(iFile);
		for (i = 0; i < masFiles.Get((size)iFile)->aiIncludes.NumElements(); i++)
		{
			iIncluded = masFiles.Get((size)iFile)->aiIncludes.GetValue(i);
			bCut = false;
			for (j = 0; paiCutTo && (iFile == iCutFrom) && (j < paiCutTo->NumElements()) && !bCut; j++)
			{
				bCut = (paiCutTo->GetValue(j) == iIncluded);
			}
			while (acSeen.NumElements() < masFiles.NumElements())
			{
				acSeen.Add(0);
			}
			if (bCut || acSeen.GetValue((size)iIncluded))
			{
				continue;
			}
			acSeen.SetValue((size)iIncluded, 1);
			paiDest->Add(iIncluded);
			aiStack.Add(iIncluded);
		}
	}
	aiStack.Kill();
	acSeen.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludeGraph::Reaches(int iFrom, int iTo, int iCutFrom, CArrayInt* paiCutTo)
{
	CArrayInt	aiReached;
	size		i;
	bool		bReaches;

	aiReached.Init();
	FindReached(iFrom, iCutFrom, paiCutTo, &aiReached);
	bReaches = false;
	for (i = 0; (i < aiReached.NumElements()) && !bReaches; i++)
	{
		bReaches = (aiReached.GetValue(i) == iTo);
	}
	aiReached.Kill();
	return bReaches;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::Class(void)
{
	U_Pointer(mpcContent);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::Save(CObjectWriter* pcFile) { return false; }
bool CIDEIncludes::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Ctrl+Alt+O is IntelliJ's Optimize Imports.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.OptimiseIncludes", "Optimise #includes", "Control+Alt+O", Ptr<CImageCel>(), this, &CIDEIncludes::QueueOptimise)->SetEnabledCheck(this, &CIDEIncludes::CanOptimise);
	pCommands->Add("Refactor.SortIncludes", "Sort #includes", "", Ptr<CImageCel>(), this, &CIDEIncludes::QueueSort)->SetEnabledCheck(this, &CIDEIncludes::CanOptimise);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.OptimiseIncludes"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.SortIncludes"));
}


//////////////////////////////////////////////////////////////////////////
//
// The editor has a C or C++ file.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::CanOptimise(void)
{
	EIDEFileType	eType;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	eType = CIDEIcons::GetFileType(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	return (eType == IFT_CPP) || (eType == IFT_C) || (eType == IFT_Header);
}


void CIDEIncludes::QueueOptimise(void) { mpcContent->GetWindow()->Post(this, &CIDEIncludes::OptimiseQueued); }
void CIDEIncludes::QueueSort(void) { mpcContent->GetWindow()->Post(this, &CIDEIncludes::SortQueued); }
void CIDEIncludes::SortQueued(void) { Sort(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::OptimiseQueued(void)
{
	SIDEIncludePlan	sPlan;

	sPlan.Init();
	if (FindPlan(&sPlan))
	{
		Optimise(&sPlan);
	}
	sPlan.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether two files are one thing's source and header: the same name
// before their extensions.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEOwnHeader(const char* szFileName, const char* szHeader)
{
	const char*	szName;
	const char*	szHeaderName;
	const char*	szDot;
	const char*	szHeaderDot;

	szName = GetIDEFileNamePart(szFileName);
	szHeaderName = GetIDEFileNamePart(szHeader);
	szDot = strrchr(szName, '.');
	szHeaderDot = strrchr(szHeaderName, '.');
	if (!szDot || !szHeaderDot || ((szDot - szName) != (szHeaderDot - szHeaderName)))
	{
		return false;
	}
	return MemICmp(szName, szHeaderName, (size)(szDot - szName)) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a text has szName as a whole word.
//
//////////////////////////////////////////////////////////////////////////
static bool HasIDEWord(const char* szText, const char* szName)
{
	const char*	szFound;
	size		uiLength;

	uiLength = strlen(szName);
	for (szFound = strstr(szText, szName); szFound; szFound = strstr(szFound + 1, szName))
	{
		if (((szFound == szText) || !IsIDENameChar(szFound[-1])) && !IsIDENameChar(szFound[uiLength]))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the next line of a text starts.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDELineAfter(const char* szText, size uiAt)
{
	for (; (szText[uiAt] != '\0') && (szText[uiAt] != '\n'); uiAt++)
	{
	}
	return (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a line of a text starts with a word (after any white space, and
// after its # if it starts with one), followed by something that isn't
// part of a name.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDELineOf(const char* szText, size uiLine, const char* szWord, size* puiAfter)
{
	size	uiLength;

	for (; (szText[uiLine] == ' ') || (szText[uiLine] == '\t'); uiLine++)
	{
	}
	if ((szWord[0] == '#') && (szText[uiLine] == '#'))
	{
		szWord++;
		for (uiLine++; (szText[uiLine] == ' ') || (szText[uiLine] == '\t'); uiLine++)
		{
		}
	}
	uiLength = strlen(szWord);
	if ((strncmp(&szText[uiLine], szWord, uiLength) != 0) || IsIDENameChar(szText[uiLine + uiLength]))
	{
		return false;
	}
	for (uiLine += uiLength; (szText[uiLine] == ' ') || (szText[uiLine] == '\t'); uiLine++)
	{
	}
	*puiAfter = uiLine;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a file that has no #include gets its first: after its include
// guard's #define, or its #pragma once, or else after the comment it
// opens with.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDEIncludeTop(const char* szText)
{
	const char*	szEnd;
	size		uiAt;
	size		uiNext;
	size		uiAfter;

	uiAt = 0;
	for (;;)
	{
		for (; (szText[uiAt] == ' ') || (szText[uiAt] == '\t') || (szText[uiAt] == '\r') || (szText[uiAt] == '\n'); uiAt++)
		{
		}
		if ((szText[uiAt] == '/') && (szText[uiAt + 1] == '*'))
		{
			szEnd = strstr(&szText[uiAt + 2], "*/");
			if (!szEnd)
			{
				return 0;
			}
			uiAt = FindIDELineAfter(szText, (size)(szEnd - szText));
		}
		else if ((szText[uiAt] == '/') && (szText[uiAt + 1] == '/'))
		{
			uiAt = FindIDELineAfter(szText, uiAt);
		}
		else
		{
			break;
		}
	}
	if (IsIDELineOf(szText, uiAt, "#ifndef", &uiAfter))
	{
		uiNext = FindIDELineAfter(szText, uiAt);
		if (IsIDELineOf(szText, uiNext, "#define", &uiAfter))
		{
			return FindIDELineAfter(szText, uiNext);
		}
	}
	else if (IsIDELineOf(szText, uiAt, "#pragma", &uiAfter))
	{
		return FindIDELineAfter(szText, uiAt);
	}
	return uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a file gets another #include: after the last it has outside any
// #if, or where its first goes.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDEIncludePlace(CArrayIDEIncludes* pasIncludes, const char* szText)
{
	size	i;

	for (i = pasIncludes->NumElements(); i > 0; i--)
	{
		if (!pasIncludes->Get(i - 1)->bConditional)
		{
			return pasIncludes->Get(i - 1)->uiLineEnd;
		}
	}
	return FindIDEIncludeTop(szText);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether every place the file names the class szKey names it as a
// pointer's or a reference's type in a declaration ("CGauge* pcGauge",
// "const CGauge& cGauge"): all it needs of the class is its forward
// declaration.  A member of the class named is another symbol, which the
// caller sees.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::IsOnlyPointedTo(CIDERefactorUnit* pcUnit, const char* szFileName, const char* szKey)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	size				i;
	int32				iToken;
	uint8				uiKind;
	bool				bOnly;

	if (!pcUnit->IsParsed())
	{
		return false;
	}
	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind);
	bOnly = true;
	for (i = 0; (i < asLocations.NumElements()) && bOnly; i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole != CPPIR_Reference) || (StringInsensitiveCompare(psLocation->szFileName.Text(), szFileName) != 0))
		{
			continue;
		}
		bOnly = !(psLocation->uiFlags & CPPIF_MACRO) && ((psLocation->uiUsage == CPPUT_Field) || (psLocation->uiUsage == CPPUT_Local) || (psLocation->uiUsage == CPPUT_Global) ||
				(psLocation->uiUsage == CPPUT_Parameter) || (psLocation->uiUsage == CPPUT_Function) || (psLocation->uiUsage == CPPUT_Alias));
		iToken = bOnly ? pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn) : -1;
		if (iToken == -1)
		{
			bOnly = false;
			break;
		}
		for (iToken++; pcUnit->IsToken(iToken, "const") || pcUnit->IsToken(iToken, "volatile"); iToken++)
		{
		}
		bOnly = pcUnit->IsToken(iToken, "*") || pcUnit->IsToken(iToken, "&") || pcUnit->IsToken(iToken, "&&");
	}
	KillIDELocations(&asLocations);
	return bOnly;
}


//////////////////////////////////////////////////////////////////////////
//
// A class's forward declaration, "class CGauge;" or "struct SMark;" as
// its definition in szDeclared says.  False if it can't have one: it is
// in a class or a namespace, or is defined some other way (a typedef of
// an unnamed struct).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::AppendForwardDeclaration(const char* szKey, const char* szDeclared, CChars* pszDest)
{
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CTextFile			cFile;
	const char*			szWord;
	char*				szText;
	size				uiLength;
	size				uiAt;
	size				i;
	uint8				uiKind;

	if (strchr(szKey, ':') || strchr(szKey, '<') || strchr(szKey, '('))
	{
		return false;
	}
	szWord = NULL;
	asLocations.Init();
	mpcContent->GetCodeModel()->GetSymbolLocations(szKey, &asLocations, &uiKind);
	for (i = 0; (i < asLocations.NumElements()) && !szWord; i++)
	{
		psLocation = asLocations.Get(i);
		if ((psLocation->uiRole != CPPIR_Definition) || (StringInsensitiveCompare(psLocation->szFileName.Text(), szDeclared) != 0))
		{
			continue;
		}
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(szDeclared, &cFile, &szText, &uiLength))
		{
			//The word before its name.
			uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
			for (; (uiAt > 0) && ((szText[uiAt - 1] == ' ') || (szText[uiAt - 1] == '\t')); uiAt--)
			{
			}
			if ((uiAt >= 5) && (strncmp(&szText[uiAt - 5], "class", 5) == 0) && ((uiAt == 5) || !IsIDENameChar(szText[uiAt - 6])))
			{
				szWord = "class";
			}
			else if ((uiAt >= 6) && (strncmp(&szText[uiAt - 6], "struct", 6) == 0) && ((uiAt == 6) || !IsIDENameChar(szText[uiAt - 7])))
			{
				szWord = "struct";
			}
		}
		cFile.Kill();
		break;
	}
	KillIDELocations(&asLocations);
	if (szWord)
	{
		pszDest->Append(szWord)->Append(' ')->Append(szKey)->Append(';');
	}
	return szWord != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A macro that szText uses and that the file iInclude, or a file it
// reaches, #defines: its name in pszDest (true).  The index has no
// macros, so the #defines are read from the texts: one an #if leaves out
// is one too, which only keeps an #include that could have gone.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::FindUsedMacro(CIDEIncludeGraph* pcGraph, int iInclude, const char* szText, CChars* pszDest)
{
	CArrayInt	aiFiles;
	CTextFile	cFile;
	CChars		szName;
	char*		szFile;
	size		uiLength;
	size		uiLine;
	size		uiAfter;
	size		i;
	size		j;
	bool		bFound;

	aiFiles.Init();
	aiFiles.Add(iInclude);
	pcGraph->FindReached(iInclude, -1, NULL, &aiFiles);
	bFound = false;
	for (i = 0; (i < aiFiles.NumElements()) && !bFound; i++)
	{
		if (pcGraph->IsOpaque(aiFiles.GetValue(i)))
		{
			continue;
		}
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(pcGraph->GetFileName(aiFiles.GetValue(i)), &cFile, &szFile, &uiLength))
		{
			for (uiLine = 0; (szFile[uiLine] != '\0') && !bFound; uiLine = FindIDELineAfter(szFile, uiLine))
			{
				if (!IsIDELineOf(szFile, uiLine, "#define", &uiAfter))
				{
					continue;
				}
				szName.Init();
				for (j = uiAfter; IsIDENameChar(szFile[j]); j++)
				{
					szName.Append(szFile[j]);
				}
				if (!szName.Empty() && HasIDEWord(szText, szName.Text()))
				{
					pszDest->Set(szName);
					bFound = true;
				}
				szName.Kill();
			}
		}
		cFile.Kill();
	}
	aiFiles.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The file is to be given an #include of a header, for the thing szKey
// that comes from it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::AddPlanned(SIDEIncludePlan* psPlan, const char* szHeader, const char* szKey)
{
	CChars	szWhy;
	size	i;

	for (i = 0; i < psPlan->aszAdded.NumElements(); i++)
	{
		if (StringInsensitiveCompare(psPlan->aszAdded.GetText(i), szHeader) == 0)
		{
			return;
		}
	}
	szWhy.Init();
	GetIDEScopedName(szKey, &szWhy);
	szWhy.Append(" comes from it");
	psPlan->aszAdded.Add(szHeader);
	psPlan->aszAddedWhy.Add(szWhy.Text());
	szWhy.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The names the file doesn't know, that exactly one header of the
// solution declares and the file doesn't reach: each header is given an
// #include.  A name it doesn't know is one the index has nothing for
// where it is written, on a line the file's parse or its check has an
// error at ("CValve cValve;" with no CValve: the error is at cValve), and
// that isn't named through anything (after a ., a -> or a ::).
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::FindUnknownNames(SIDEIncludePlan* psPlan, CIDERefactorUnit* pcUnit, CIDEIncludeGraph* pcGraph, const char* szText)
{
	CIDECodeModel*		pcModel;
	CCPPErrors*			pcErrors;
	CCPPTokeniser*		pcTokeniser;
	SPreprocessedToken*	psSource;
	CArrayInt			aiLines;
	CArrayChars			aszHeaders;
	CChars				szName;
	CChars				szKey;
	size				uiList;
	size				i;
	uint8				uiKind;

	if (!pcUnit->IsChecked())
	{
		return;
	}
	pcModel = mpcContent->GetCodeModel();
	aiLines.Init();
	for (uiList = 0; uiList < 2; uiList++)
	{
		pcErrors = (uiList == 0) ? pcUnit->GetTree()->GetErrors() : pcUnit->GetPass()->GetErrors();
		for (i = 0; i < pcErrors->NumErrors(); i++)
		{
			if (pcErrors->GetFile(i) && (StringInsensitiveCompare(GetIDEFileNamePart(pcErrors->GetFile(i)), GetIDEFileNamePart(psPlan->szFileName.Text())) == 0))
			{
				aiLines.AddIfUnique(pcErrors->GetLine(i));
			}
		}
	}

	pcTokeniser = pcUnit->GetTokeniser();
	for (i = 0; (aiLines.NumElements() > 0) && (i < pcTokeniser->NumTokens()); i++)
	{
		psSource = pcTokeniser->GetSource(pcTokeniser->Get(i));
		if ((psSource->pcFile != pcUnit->GetFile()) || (psSource->iExpansion != -1) || !psSource->IsIdentifier() || (aiLines.Find(psSource->iLine) == ARRAY_ELEMENT_NOT_FOUND) ||
			pcUnit->IsToken((int32)i - 1, ".") || pcUnit->IsToken((int32)i - 1, "->") || pcUnit->IsToken((int32)i - 1, "::"))
		{
			continue;
		}
		szKey.Init();
		szName.Init();
		szName.AppendSubString(psSource->szText, 0, psSource->uiLength);
		aszHeaders.Init();
		if (!pcModel->GetSymbolAt(psPlan->szFileName.Text(), psSource->iLine, psSource->iColumn, &szKey, &uiKind) && (pcModel->FindHeadersDeclaring(szName.Text(), &aszHeaders) == 1) &&
			(StringInsensitiveCompare(aszHeaders.GetText(0), psPlan->szFileName.Text()) != 0) && (pcModel->FindProjectOfFile(aszHeaders.GetText(0)) != MAX_SIZE) &&
			!pcGraph->Reaches(pcGraph->Get(psPlan->szFileName.Text()), pcGraph->Get(aszHeaders.GetText(0)), -1, NULL))
		{
			AddPlanned(psPlan, aszHeaders.GetText(0), szName.Text());
		}
		aszHeaders.Kill();
		szName.Kill();
		szKey.Kill();
	}
	aiLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Optimise #includes would do to the file in the editor.  False if
// the editor has no C or C++ file.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::FindPlan(SIDEIncludePlan* psDest)
{
	CIDECodeModel*		pcModel;
	CIDEFileTexts*		pcTexts;
	CIDERefactorUnit	cUnit;
	CIDEIncludeGraph	cGraph;
	CTextFile			cFile;
	CArrayIDENeeds		asNeeds;
	CArrayInt			aiNodes;
	CArrayInt			aiReached;
	CArrayInt			aiCredits;
	CArrayInt			aiOthers;
	SIDENeed*			psNeed;
	SIDEInclude*		psInclude;
	SIDEIncludeUse*		psUse;
	CChars				szLine;
	CChars				szMacro;
	char*				szText;
	size				uiLength;
	size				i;
	size				j;
	size				k;
	int					iFile;
	int					iDeclared;
	int					iProvider;
	bool				bForward;
	bool				bHeader;

	if (!CanOptimise())
	{
		return false;
	}
	pcModel = mpcContent->GetCodeModel();
	pcTexts = &mpcContent->GetFindInFiles();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	psDest->szFileName.Set(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	bHeader = CIDEIcons::GetFileType(psDest->szFileName.Text()) == IFT_Header;

	cUnit.Init();
	cUnit.Parse(pcModel, pcTexts, psDest->szFileName.Text(), true);
	psDest->uiErrors = cUnit.NumErrors();
	cFile.Init();
	if (!pcTexts->GetFileText(psDest->szFileName.Text(), &cFile, &szText, &uiLength))
	{
		cFile.Kill();
		cUnit.Kill();
		return false;
	}
	pcModel->FindIncludesIn(psDest->szFileName.Text(), szText, &psDest->asIncludes);

	cGraph.Init(pcModel, pcTexts);
	iFile = cGraph.Get(psDest->szFileName.Text());
	aiNodes.Init();
	aiCredits.Init();
	aiOthers.Init();
	for (i = 0; i < psDest->asIncludes.NumElements(); i++)
	{
		psInclude = psDest->asIncludes.Get(i);
		psDest->asUses.Add()->Init();
		psDest->asUses.Get(i)->szWritten.AppendSubString(szText, psInclude->uiNameStart - 1, psInclude->uiNameEnd + 1);
		aiNodes.Add(psInclude->szFileName.Empty() ? -1 : cGraph.Get(psInclude->szFileName.Text()));
		aiCredits.Add(0);
		aiOthers.Add(0);
	}

	//What the file names, and the #include each thing comes from: the one of its header, or the first that reaches it.
	asNeeds.Init();
	pcModel->FindNeeds(psDest->szFileName.Text(), NULL, &asNeeds);
	for (i = 0; i < asNeeds.NumElements(); i++)
	{
		psNeed = asNeeds.Get(i);
		iDeclared = cGraph.Get(psNeed->szDeclared.Text());
		iProvider = -1;
		for (j = 0; (j < aiNodes.NumElements()) && (iProvider == -1); j++)
		{
			if (aiNodes.GetValue(j) == iDeclared)
			{
				iProvider = (int)j;
			}
		}
		for (j = 0; (j < aiNodes.NumElements()) && (iProvider == -1); j++)
		{
			if ((aiNodes.GetValue(j) != -1) && cGraph.Reaches(aiNodes.GetValue(j), iDeclared, -1, NULL))
			{
				iProvider = (int)j;
			}
		}

		szLine.Init();
		bForward = (psNeed->uiKind == CPPSK_Class) && IsOnlyPointedTo(&cUnit, psDest->szFileName.Text(), psNeed->szKey.Text()) &&
				   AppendForwardDeclaration(psNeed->szKey.Text(), psNeed->szDeclared.Text(), &szLine);
		if (iProvider != -1)
		{
			psUse = psDest->asUses.Get((size)iProvider);
			aiCredits.SetValue((size)iProvider, aiCredits.GetValue((size)iProvider) + 1);
			if (psUse->szWhy.Empty())
			{
				GetIDEScopedName(psNeed->szKey.Text(), &psUse->szWhy);
			}
			if (bForward)
			{
				if (!psNeed->bDeclaredByUser)
				{
					psUse->aszClasses.Add(szLine.Text());
				}
			}
			else
			{
				aiOthers.SetValue((size)iProvider, 1);
			}
		}
		else if (pcModel->FindProjectOfFile(psNeed->szDeclared.Text()) == MAX_SIZE)
		{
			//Not one of the solution's: it may be past a <> #include, which isn't followed.  Whatever reaches one may bring it.
			for (j = 0; j < aiNodes.NumElements(); j++)
			{
				if (aiNodes.GetValue(j) == -1)
				{
					continue;
				}
				aiReached.Init();
				aiReached.Add(aiNodes.GetValue(j));
				cGraph.FindReached(aiNodes.GetValue(j), -1, NULL, &aiReached);
				for (k = 0; k < aiReached.NumElements(); k++)
				{
					if (cGraph.IsOpaque(aiReached.GetValue(k)))
					{
						aiCredits.SetValue(j, aiCredits.GetValue(j) + 1);
						aiOthers.SetValue(j, 1);
						if (psDest->asUses.Get(j)->szWhy.Empty())
						{
							GetIDEScopedName(psNeed->szKey.Text(), &psDest->asUses.Get(j)->szWhy);
						}
						break;
					}
				}
				aiReached.Kill();
			}
		}
		else if (bForward)
		{
			//Nothing the file #includes reaches its header, and its forward declaration is all the file needs.
			if (!psNeed->bDeclaredByUser)
			{
				psDest->aszDeclared.AddIfUnique(szLine.Text());
			}
		}
		else if ((CIDEIcons::GetFileType(psNeed->szDeclared.Text()) == IFT_Header) && !cGraph.Reaches(iDeclared, iFile, -1, NULL))
		{
			//...or it needs the header (which doesn't #include the file itself).
			AddPlanned(psDest, psNeed->szDeclared.Text(), psNeed->szKey.Text());
		}
		szLine.Kill();
	}

	for (i = 0; i < psDest->asIncludes.NumElements(); i++)
	{
		psInclude = psDest->asIncludes.Get(i);
		psUse = psDest->asUses.Get(i);
		if (psInclude->bSystem || psInclude->bConditional || psInclude->szFileName.Empty())
		{
			psUse->iWhat = IDE_INCLUDE_LEFT;
			psUse->szWhy.Set(psInclude->bSystem ? "a <> #include" : (psInclude->bConditional ? "inside an #if" : "its file isn't found"));
		}
		else if (!bHeader && IsIDEOwnHeader(psDest->szFileName.Text(), psInclude->szFileName.Text()))
		{
			psUse->szWhy.Set("the file's own header");
		}
		else if (aiCredits.GetValue(i) == 0)
		{
			szMacro.Init();
			if (FindUsedMacro(&cGraph, aiNodes.GetValue(i), szText, &szMacro))
			{
				psUse->szWhy.Set("its macro ");
				psUse->szWhy.Append(szMacro)->Append(" is used");
			}
			else
			{
				psUse->iWhat = IDE_INCLUDE_REMOVED;
				psUse->szWhy.Set("nothing named here comes from it");
			}
			szMacro.Kill();
		}
		else if (!aiOthers.GetValue(i))
		{
			psUse->iWhat = IDE_INCLUDE_FORWARD;
			psUse->szWhy.Clear();
		}
		else
		{
			if (aiCredits.GetValue(i) > 1)
			{
				psUse->szWhy.Append(" and ")->Append((int32)(aiCredits.GetValue(i) - 1))->Append(" more");
			}
			psUse->szWhy.Append((aiCredits.GetValue(i) > 1) ? " come from it" : " comes from it");
		}
	}
	KillIDENeeds(&asNeeds);
	aiOthers.Kill();
	aiCredits.Kill();
	aiNodes.Kill();

	FindUnknownNames(psDest, &cUnit, &cGraph, szText);
	cGraph.Kill();
	cFile.Kill();
	cUnit.Kill();
	FindDependants(psDest);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The files that got a name through one of the header's #includes that
// go (they #include the header, name something declared in what the
// #include brought, and reach that no other way): each is given its own
// #include of the thing's header.  An #include that brought one of the
// compiler's own headers (a <> #include, somewhere in what it reaches) is
// kept instead: who names what of those isn't known.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::FindDependants(SIDEIncludePlan* psPlan)
{
	CIDECodeModel*		pcModel;
	CIDEIncludeGraph	cGraph;
	CArrayIDENeeds		asNeeds;
	CArrayInt			aiCut;
	CArrayInt			aiKept;
	CArrayInt			aiLost;
	CArrayInt			aiReached;
	CArrayChars			aszLost;
	SIDENeed*			psNeed;
	SIDEInclude*		psInclude;
	SIDEIncludeUse*		psUse;
	size				i;
	size				j;
	size				k;
	int					iFile;
	int					iUser;
	int					iLost;
	bool				bHas;
	bool				bAgain;

	psPlan->aszDependants.ReInit();
	psPlan->aszDependantHeaders.ReInit();
	if (CIDEIcons::GetFileType(psPlan->szFileName.Text()) != IFT_Header)
	{
		return;
	}
	pcModel = mpcContent->GetCodeModel();
	cGraph.Init(pcModel, &mpcContent->GetFindInFiles());
	iFile = cGraph.Get(psPlan->szFileName.Text());
	aiCut.Init();
	aiKept.Init();
	aiLost.Init();
	do
	{
		//What the header no longer reaches.
		bAgain = false;
		aiCut.ReInit();
		aiKept.ReInit();
		aiLost.ReInit();
		for (i = 0; i < psPlan->asIncludes.NumElements(); i++)
		{
			psUse = psPlan->asUses.Get(i);
			if ((psUse->iWhat == IDE_INCLUDE_REMOVED) || (psUse->iWhat == IDE_INCLUDE_FORWARD))
			{
				aiCut.AddIfUnique(cGraph.Get(psPlan->asIncludes.Get(i)->szFileName.Text()));
			}
		}
		cGraph.FindReached(iFile, iFile, &aiCut, &aiKept);
		for (i = 0; (i < psPlan->asIncludes.NumElements()) && !bAgain; i++)
		{
			psInclude = psPlan->asIncludes.Get(i);
			psUse = psPlan->asUses.Get(i);
			if ((psUse->iWhat != IDE_INCLUDE_REMOVED) && (psUse->iWhat != IDE_INCLUDE_FORWARD))
			{
				continue;
			}
			aiReached.Init();
			aiReached.Add(cGraph.Get(psInclude->szFileName.Text()));
			cGraph.FindReached(aiReached.GetValue(0), -1, NULL, &aiReached);
			for (j = 0; (j < aiReached.NumElements()) && !bAgain; j++)
			{
				iLost = aiReached.GetValue(j);
				bHas = false;
				for (k = 0; (k < aiKept.NumElements()) && !bHas; k++)
				{
					bHas = (aiKept.GetValue(k) == iLost);
				}
				if (bHas || (iLost == iFile))
				{
					continue;
				}
				if (cGraph.IsOpaque(iLost))
				{
					psUse->iWhat = IDE_INCLUDE_KEPT;
					psUse->szWhy.Set("it brings <");
					psUse->szWhy.Append(GetIDEFileNamePart(cGraph.GetFileName(iLost)))->Append("> to the files that #include this one");
					bAgain = true;
				}
				else
				{
					aiLost.AddIfUnique(iLost);
				}
			}
			aiReached.Kill();
		}
	}
	while (bAgain);

	if (aiLost.NumElements() > 0)
	{
		aszLost.Init();
		for (i = 0; i < aiLost.NumElements(); i++)
		{
			aszLost.Add(cGraph.GetFileName(aiLost.GetValue(i)));
		}
		asNeeds.Init();
		pcModel->FindNeeds(NULL, &aszLost, &asNeeds);
		for (i = 0; i < asNeeds.NumElements(); i++)
		{
			psNeed = asNeeds.Get(i);
			if ((StringInsensitiveCompare(psNeed->szUser.Text(), psPlan->szFileName.Text()) == 0) || (pcModel->FindProjectOfFile(psNeed->szUser.Text()) == MAX_SIZE))
			{
				continue;
			}
			bHas = false;
			for (j = 0; (j < psPlan->aszDependants.NumElements()) && !bHas; j++)
			{
				bHas = (StringInsensitiveCompare(psPlan->aszDependants.GetText(j), psNeed->szUser.Text()) == 0) &&
					   (StringInsensitiveCompare(psPlan->aszDependantHeaders.GetText(j), psNeed->szDeclared.Text()) == 0);
			}
			if (bHas)
			{
				continue;
			}
			iUser = cGraph.Get(psNeed->szUser.Text());
			if (cGraph.Reaches(iUser, iFile, -1, NULL) && !cGraph.Reaches(iUser, cGraph.Get(psNeed->szDeclared.Text()), iFile, &aiCut))
			{
				psPlan->aszDependants.Add(psNeed->szUser.Text());
				psPlan->aszDependantHeaders.Add(psNeed->szDeclared.Text());
			}
		}
		KillIDENeeds(&asNeeds);
		aszLost.Kill();
	}
	aiLost.Kill();
	aiKept.Kill();
	aiCut.Kill();
	cGraph.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The plan's edits of the file's own text: the #includes that go taken
// out, those added put after the last it has, and the forward
// declarations after them (with those the text has there already, or two
// lines down).
//
//////////////////////////////////////////////////////////////////////////
void CIDEIncludes::MakeEdits(SIDEIncludePlan* psPlan, const char* szText, CIDETextEdits* pcEdits)
{
	CIDECodeModel*	pcModel;
	CArrayChars		aszDeclared;
	SIDEIncludeUse*	psUse;
	CChars			szAdded;
	CChars			szName;
	size			uiPlace;
	size			uiAfter;
	size			uiWord;
	size			i;
	size			j;

	pcModel = mpcContent->GetCodeModel();
	aszDeclared.Init();
	for (i = 0; i < psPlan->asIncludes.NumElements(); i++)
	{
		psUse = psPlan->asUses.Get(i);
		if ((psUse->iWhat == IDE_INCLUDE_REMOVED) || (psUse->iWhat == IDE_INCLUDE_FORWARD))
		{
			pcEdits->Replace(psPlan->asIncludes.Get(i)->uiLineStart, psPlan->asIncludes.Get(i)->uiLineEnd, "");
		}
		for (j = 0; (psUse->iWhat == IDE_INCLUDE_FORWARD) && (j < psUse->aszClasses.NumElements()); j++)
		{
			aszDeclared.AddIfUnique(psUse->aszClasses.GetText(j));
		}
	}
	for (i = 0; i < psPlan->aszDeclared.NumElements(); i++)
	{
		aszDeclared.AddIfUnique(psPlan->aszDeclared.GetText(i));
	}

	uiPlace = FindIDEIncludePlace(&psPlan->asIncludes, szText);
	szAdded.Init();
	for (i = 0; i < psPlan->aszAdded.NumElements(); i++)
	{
		szName.Init();
		pcModel->MakeIncludeName(psPlan->szFileName.Text(), psPlan->aszAdded.GetText(i), &szName);
		szAdded.Append("#include \"")->Append(szName)->Append("\"\n");
		szName.Kill();
	}
	if (aszDeclared.NumElements() > 0)
	{
		//The forward declarations the text has after its #includes, if any: these go before them.
		for (uiAfter = uiPlace; (szText[uiAfter] == ' ') || (szText[uiAfter] == '\t') || (szText[uiAfter] == '\r') || (szText[uiAfter] == '\n'); uiAfter++)
		{
		}
		if ((IsIDELineOf(szText, uiAfter, "class", &uiWord) || IsIDELineOf(szText, uiAfter, "struct", &uiWord)) && (strchr(&szText[uiAfter], ';') < strchr(&szText[uiAfter], '\n')) &&
			(memchr(&szText[uiAfter], '{', (size)(strchr(&szText[uiAfter], ';') - &szText[uiAfter])) == NULL))
		{
			if (!szAdded.Empty())
			{
				pcEdits->Insert(uiPlace, szAdded.Text());
				szAdded.Clear();
			}
			uiPlace = uiAfter;
		}
		else
		{
			szAdded.Append("\n\n");
		}
		for (i = 0; i < aszDeclared.NumElements(); i++)
		{
			szAdded.Append(aszDeclared.GetText(i))->Append('\n');
		}
	}
	if (!szAdded.Empty())
	{
		pcEdits->Insert(uiPlace, szAdded.Text());
	}
	szAdded.Kill();
	aszDeclared.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The #includes a file that depended on the header's is given, after its
// last: the edits of its text.  False if it is given none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::MakeDependantEdits(SIDEIncludePlan* psPlan, const char* szFileName, const char* szText, CIDETextEdits* pcEdits)
{
	CIDECodeModel*		pcModel;
	CArrayIDEIncludes	asIncludes;
	CChars				szAdded;
	CChars				szName;
	size				i;
	size				j;
	bool				bHas;

	pcModel = mpcContent->GetCodeModel();
	asIncludes.Init();
	pcModel->FindIncludesIn(szFileName, szText, &asIncludes);
	szAdded.Init();
	for (i = 0; i < psPlan->aszDependants.NumElements(); i++)
	{
		if (StringInsensitiveCompare(psPlan->aszDependants.GetText(i), szFileName) != 0)
		{
			continue;
		}
		bHas = false;
		for (j = 0; (j < asIncludes.NumElements()) && !bHas; j++)
		{
			bHas = !asIncludes.Get(j)->bConditional && (StringInsensitiveCompare(asIncludes.Get(j)->szFileName.Text(), psPlan->aszDependantHeaders.GetText(i)) == 0);
		}
		if (!bHas)
		{
			szName.Init();
			pcModel->MakeIncludeName(szFileName, psPlan->aszDependantHeaders.GetText(i), &szName);
			szAdded.Append("#include \"")->Append(szName)->Append("\"\n");
			szName.Kill();
		}
	}
	bHas = !szAdded.Empty();
	if (bHas)
	{
		pcEdits->Insert(FindIDEIncludePlace(&asIncludes, szText), szAdded.Text());
	}
	szAdded.Kill();
	KillIDEIncludes(&asIncludes);
	return bHas;
}


//////////////////////////////////////////////////////////////////////////
//
// The errors a file's check has (its parse's too), with szText as its
// text (as it is, if NULL) and szUnitText as its unit's.  MAX_SIZE if it
// can't be parsed.
//
//////////////////////////////////////////////////////////////////////////
size CIDEIncludes::CountErrors(const char* szFileName, char* szText, const char* szUnit, char* szUnitText)
{
	return CountIDETrialErrors(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), szFileName, szText, szUnit, szUnitText);
}


//////////////////////////////////////////////////////////////////////////
//
// The errors the file's check would have with the plan done (the file's
// text, and its unit's if the unit is given an #include).
//
//////////////////////////////////////////////////////////////////////////
size CIDEIncludes::CountPlanErrors(SIDEIncludePlan* psPlan, const char* szText)
{
	CIDECodeModel*	pcModel;
	CIDETextEdits	cEdits;
	CIDETextEdits	cUnitEdits;
	CTextFile		cFile;
	CChars			szNew;
	CChars			szUnitNew;
	CChars			szUnit;
	char*			szUnitText;
	size			uiLength;
	size			uiProject;
	size			uiErrors;
	bool			bUnit;

	pcModel = mpcContent->GetCodeModel();
	cEdits.Init();
	MakeEdits(psPlan, szText, &cEdits);
	szNew.Init();
	cEdits.ApplyTo(szText, &szNew);
	cEdits.Kill();

	szUnit.Init();
	szUnitNew.Init();
	bUnit = false;
	if (pcModel->GetIncluder(psPlan->szFileName.Text(), &uiProject, &szUnit) && (StringInsensitiveCompare(szUnit.Text(), psPlan->szFileName.Text()) != 0))
	{
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(szUnit.Text(), &cFile, &szUnitText, &uiLength))
		{
			cUnitEdits.Init();
			bUnit = MakeDependantEdits(psPlan, szUnit.Text(), szUnitText, &cUnitEdits);
			if (bUnit)
			{
				cUnitEdits.ApplyTo(szUnitText, &szUnitNew);
			}
			cUnitEdits.Kill();
		}
		cFile.Kill();
	}
	uiErrors = CountErrors(psPlan->szFileName.Text(), szNew.Text(), szUnit.Text(), bUnit ? szUnitNew.Text() : NULL);
	szUnitNew.Kill();
	szUnit.Kill();
	szNew.Kill();
	return uiErrors;
}


//////////////////////////////////////////////////////////////////////////
//
// The plan done, as one undo.  The file is checked as it would be first:
// if it would have more errors than it has, the #includes that go are
// tried one at a time, and those it has more errors without are kept.
// False if there was nothing to do.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::Optimise(SIDEIncludePlan* psPlan, CChars* pszStatus)
{
	CIDEFileEdits	cFiles;
	CArrayInt		aiWhats;
	CArrayChars		aszEdited;
	SIDEIncludeUse*	psUse;
	CChars			szMessage;
	char*			szText;
	char*			szOther;
	size			uiRemoved;
	size			uiForward;
	size			uiNeeded;
	size			uiParts;
	size			i;
	bool			bResult;

	cFiles.Init(mpcContent);
	szText = cFiles.GetText(psPlan->szFileName.Text());
	uiRemoved = 0;
	for (i = 0; i < psPlan->asUses.NumElements(); i++)
	{
		psUse = psPlan->asUses.Get(i);
		uiRemoved += ((psUse->iWhat == IDE_INCLUDE_REMOVED) || (psUse->iWhat == IDE_INCLUDE_FORWARD)) ? 1 : 0;
	}
	if (szText && (uiRemoved > 0) && (CountPlanErrors(psPlan, szText) > psPlan->uiErrors))
	{
		//Each alone; then those that could go alone, together.
		aiWhats.Init();
		for (i = 0; i < psPlan->asUses.NumElements(); i++)
		{
			aiWhats.Add(psPlan->asUses.Get(i)->iWhat);
			if ((aiWhats.GetValue(i) == IDE_INCLUDE_REMOVED) || (aiWhats.GetValue(i) == IDE_INCLUDE_FORWARD))
			{
				psPlan->asUses.Get(i)->iWhat = IDE_INCLUDE_KEPT;
			}
		}
		for (i = 0; i < psPlan->asUses.NumElements(); i++)
		{
			if ((aiWhats.GetValue(i) != IDE_INCLUDE_REMOVED) && (aiWhats.GetValue(i) != IDE_INCLUDE_FORWARD))
			{
				continue;
			}
			psPlan->asUses.Get(i)->iWhat = aiWhats.GetValue(i);
			FindDependants(psPlan);
			if (CountPlanErrors(psPlan, szText) > psPlan->uiErrors)
			{
				aiWhats.SetValue(i, IDE_INCLUDE_NEEDED);
			}
			psPlan->asUses.Get(i)->iWhat = IDE_INCLUDE_KEPT;
		}
		for (i = 0; i < psPlan->asUses.NumElements(); i++)
		{
			if (psPlan->asUses.Get(i)->szWhy.StartsWith("it brings <"))
			{
				continue;
			}
			psPlan->asUses.Get(i)->iWhat = aiWhats.GetValue(i);
		}
		FindDependants(psPlan);
		if (CountPlanErrors(psPlan, szText) > psPlan->uiErrors)
		{
			for (i = 0; i < psPlan->asUses.NumElements(); i++)
			{
				psUse = psPlan->asUses.Get(i);
				if ((psUse->iWhat == IDE_INCLUDE_REMOVED) || (psUse->iWhat == IDE_INCLUDE_FORWARD))
				{
					psUse->iWhat = IDE_INCLUDE_NEEDED;
				}
			}
			FindDependants(psPlan);
		}
		for (i = 0; i < psPlan->asUses.NumElements(); i++)
		{
			if (psPlan->asUses.Get(i)->iWhat == IDE_INCLUDE_NEEDED)
			{
				psPlan->asUses.Get(i)->szWhy.Set("the file doesn't compile without it");
			}
		}
		aiWhats.Kill();
	}
	if (szText && (uiRemoved > 0) && (CIDEIcons::GetFileType(psPlan->szFileName.Text()) == IFT_Header))
	{
		//The checks gave the code model the header's text as it would be.
		mpcContent->GetCodeModel()->SetHeaderText(psPlan->szFileName.Text(), szText);
	}

	uiRemoved = 0;
	uiForward = 0;
	uiNeeded = 0;
	for (i = 0; i < psPlan->asUses.NumElements(); i++)
	{
		psUse = psPlan->asUses.Get(i);
		uiRemoved += (psUse->iWhat == IDE_INCLUDE_REMOVED) ? 1 : 0;
		uiForward += (psUse->iWhat == IDE_INCLUDE_FORWARD) ? 1 : 0;
		uiNeeded += (psUse->iWhat == IDE_INCLUDE_NEEDED) ? 1 : 0;
	}
	bResult = szText && ((uiRemoved + uiForward + psPlan->aszAdded.NumElements() + psPlan->aszDeclared.NumElements()) > 0);
	aszEdited.Init();
	if (bResult)
	{
		MakeEdits(psPlan, szText, cFiles.GetEdits(psPlan->szFileName.Text()));
		for (i = 0; i < psPlan->aszDependants.NumElements(); i++)
		{
			if (aszEdited.GetIndex(psPlan->aszDependants.GetText(i)) != ARRAY_ELEMENT_NOT_FOUND)
			{
				continue;
			}
			szOther = cFiles.GetText(psPlan->aszDependants.GetText(i));
			if (szOther && MakeDependantEdits(psPlan, psPlan->aszDependants.GetText(i), szOther, cFiles.GetEdits(psPlan->aszDependants.GetText(i))))
			{
				aszEdited.Add(psPlan->aszDependants.GetText(i));
			}
		}
		cFiles.Apply();
	}

	szMessage.Init();
	if (bResult)
	{
		szMessage.Append("Optimised the #includes of ")->Append(GetIDEFileNamePart(psPlan->szFileName.Text()))->Append(": ");
		uiParts = 0;
		if (uiRemoved > 0)
		{
			szMessage.Append((uint32)uiRemoved)->Append(" removed");
			uiParts++;
		}
		if (uiForward > 0)
		{
			szMessage.Append((uiParts > 0) ? ", " : "")->Append((uint32)uiForward)->Append((uiForward == 1) ? " made a forward declaration" : " made forward declarations");
			uiParts++;
		}
		if (psPlan->aszAdded.NumElements() > 0)
		{
			szMessage.Append((uiParts > 0) ? ", " : "")->Append((uint32)psPlan->aszAdded.NumElements())->Append(" added");
			uiParts++;
		}
		if (psPlan->aszDeclared.NumElements() > 0)
		{
			szMessage.Append((uiParts > 0) ? ", " : "")->Append((uint32)psPlan->aszDeclared.NumElements());
			szMessage.Append((psPlan->aszDeclared.NumElements() == 1) ? " forward declaration added" : " forward declarations added");
		}
		if (aszEdited.NumElements() > 0)
		{
			szMessage.Append("; ")->Append((uint32)aszEdited.NumElements())->Append((aszEdited.NumElements() == 1) ? " file that got a name through it given its own #include" : " files that got names through it given their own #includes");
		}
	}
	else
	{
		szMessage.Append("The #includes of ")->Append(GetIDEFileNamePart(psPlan->szFileName.Text()))->Append(" are as they should be");
	}
	if (uiNeeded > 0)
	{
		szMessage.Append("; ")->Append((uint32)uiNeeded)->Append(" kept that nothing named here comes from: the file doesn't compile without ")->Append((uiNeeded == 1) ? "it" : "them");
	}
	szMessage.Append('.');
	if (pszStatus)
	{
		pszStatus->Set(szMessage);
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	aszEdited.Kill();
	cFiles.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Sort #includes, for the file in the editor: each run of #include lines
// (one after another, nothing between) is put in order, the file's own
// header first, then by directory in the order the directories first
// come in the run ("BaseLib/..." before "WindowLib/..." if it is now),
// by name in each.  Nothing is done if the file's check would have more
// errors with them in order: a header needs another before it.  False if
// nothing was changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEIncludes::Sort(CChars* pszStatus)
{
	CIDECodeModel*		pcModel;
	CIDEFileEdits		cFiles;
	CIDETextEdits		cTrial;
	CIDETextEdits*		pcEdits;
	CArrayIDEIncludes	asIncludes;
	CArrayInt			aiOrder;
	CArrayInt			aiGroups;
	SIDEInclude*		psInclude;
	SIDEInclude*		psOther;
	CChars				szRun;
	CChars				szNew;
	CChars				szMessage;
	CChars				szFileName;
	char*				szText;
	size				uiFirst;
	size				uiLast;
	size				uiSlash;
	size				uiOtherSlash;
	size				uiPass;
	size				i;
	size				j;
	int					iMoved;
	int					iCompare;
	bool				bChanged;
	bool				bHeader;
	bool				bResult;

	if (!CanOptimise())
	{
		return false;
	}
	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szFileName.Init(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	bHeader = CIDEIcons::GetFileType(szFileName.Text()) == IFT_Header;
	cFiles.Init(mpcContent);
	szText = cFiles.GetText(szFileName.Text());
	asIncludes.Init();
	pcModel->FindIncludesIn(szFileName.Text(), szText, &asIncludes);
	bChanged = false;
	cTrial.Init();
	for (uiPass = 0; uiPass < 2; uiPass++)
	{
		//Once for the check of what it would be, once for the editor.
		pcEdits = (uiPass == 0) ? &cTrial : cFiles.GetEdits(szFileName.Text());
		for (uiFirst = 0; uiFirst < asIncludes.NumElements(); uiFirst = uiLast + 1)
		{
			for (uiLast = uiFirst; (uiLast + 1 < asIncludes.NumElements()) && (asIncludes.Get(uiLast + 1)->uiLineStart == asIncludes.Get(uiLast)->uiLineEnd); uiLast++)
			{
			}
			if (uiLast == uiFirst)
			{
				continue;
			}

			//Each one's directory's place among the run's directories, then an insertion sort.
			aiOrder.Init();
			aiGroups.Init();
			for (i = uiFirst; i <= uiLast; i++)
			{
				psInclude = asIncludes.Get(i);
				for (uiSlash = psInclude->uiNameEnd; (uiSlash > psInclude->uiNameStart) && (szText[uiSlash - 1] != '/') && (szText[uiSlash - 1] != '\\'); uiSlash--)
				{
				}
				aiGroups.Add((int)i);
				for (j = uiFirst; j < i; j++)
				{
					psOther = asIncludes.Get(j);
					for (uiOtherSlash = psOther->uiNameEnd; (uiOtherSlash > psOther->uiNameStart) && (szText[uiOtherSlash - 1] != '/') && (szText[uiOtherSlash - 1] != '\\'); uiOtherSlash--)
					{
					}
					if ((psOther->bSystem == psInclude->bSystem) && (uiOtherSlash - psOther->uiNameStart == uiSlash - psInclude->uiNameStart) &&
						(MemICmp(&szText[psOther->uiNameStart], &szText[psInclude->uiNameStart], uiSlash - psInclude->uiNameStart) == 0))
					{
						aiGroups.SetValue(i - uiFirst, aiGroups.GetValue(j - uiFirst));
						break;
					}
				}
				if (!bHeader && !psInclude->szFileName.Empty() && IsIDEOwnHeader(szFileName.Text(), psInclude->szFileName.Text()))
				{
					aiGroups.SetValue(i - uiFirst, -1);
				}
				aiOrder.Add((int)i);
			}
			for (i = 1; i < aiOrder.NumElements(); i++)
			{
				iMoved = aiOrder.GetValue(i);
				psInclude = asIncludes.Get((size)iMoved);
				for (j = i; j > 0; j--)
				{
					psOther = asIncludes.Get((size)aiOrder.GetValue(j - 1));
					iCompare = aiGroups.GetValue((size)aiOrder.GetValue(j - 1) - uiFirst) - aiGroups.GetValue((size)iMoved - uiFirst);
					if (iCompare == 0)
					{
						szRun.Init();
						szRun.AppendSubString(szText, psOther->uiNameStart, psOther->uiNameEnd);
						szNew.Init();
						szNew.AppendSubString(szText, psInclude->uiNameStart, psInclude->uiNameEnd);
						iCompare = StringInsensitiveCompare(szRun.Text(), szNew.Text());
						szNew.Kill();
						szRun.Kill();
					}
					if (iCompare <= 0)
					{
						break;
					}
					aiOrder.SetValue(j, aiOrder.GetValue(j - 1));
				}
				aiOrder.SetValue(j, iMoved);
			}

			szRun.Init();
			for (i = 0; i < aiOrder.NumElements(); i++)
			{
				psInclude = asIncludes.Get((size)aiOrder.GetValue(i));
				szRun.AppendSubString(szText, psInclude->uiLineStart, psInclude->uiLineEnd);
				if (!szRun.EndsWith("\n"))
				{
					szRun.Append('\n');
				}
				bChanged |= (aiOrder.GetValue(i) != (int)(uiFirst + i));
			}
			pcEdits->Replace(asIncludes.Get(uiFirst)->uiLineStart, asIncludes.Get(uiLast)->uiLineEnd, szRun.Text());
			szRun.Kill();
			aiGroups.Kill();
			aiOrder.Kill();
		}
		if (!bChanged)
		{
			break;
		}
		if (uiPass == 0)
		{
			szNew.Init();
			cTrial.ApplyTo(szText, &szNew);
			bResult = CountErrors(szFileName.Text(), szNew.Text(), NULL, NULL) <= CountErrors(szFileName.Text(), NULL, NULL, NULL);
			szNew.Kill();
			if (bHeader)
			{
				pcModel->SetHeaderText(szFileName.Text(), szText);
			}
			if (!bResult)
			{
				break;
			}
		}
	}
	cTrial.Kill();

	szMessage.Init();
	if (!bChanged)
	{
		szMessage.Append("The #includes of ")->Append(GetIDEFileNamePart(szFileName.Text()))->Append(" are in order.");
	}
	else if (uiPass < 2)
	{
		szMessage.Append("The #includes of ")->Append(GetIDEFileNamePart(szFileName.Text()))->Append(" stay as they are: the file doesn't compile with them in order (a header needs another before it).");
		bChanged = false;
	}
	else
	{
		cFiles.Apply();
		szMessage.Append("Sorted the #includes of ")->Append(GetIDEFileNamePart(szFileName.Text()))->Append('.');
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
	KillIDEIncludes(&asIncludes);
	cFiles.Kill();
	szFileName.Kill();
	return bChanged;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEIncludes::PrintPlan(SIDEIncludePlan* psPlan, CChars* pszDest)
{
	CIDECodeModel*	pcModel;
	SIDEInclude*	psInclude;
	SIDEIncludeUse*	psUse;
	CChars			szName;
	size			i;
	size			j;

	pcModel = mpcContent->GetCodeModel();
	pszDest->Append("Optimise #includes: ")->Append(GetIDEFileNamePart(psPlan->szFileName.Text()))->AppendNewLine();
	{
		for (i = 0; i < psPlan->asIncludes.NumElements(); i++)
		{
			psInclude = psPlan->asIncludes.Get(i);
			psUse = psPlan->asUses.Get(i);
			pszDest->Append("  ")->Append(psUse->szWritten);
			pszDest->Append(" (line ")->Append((int32)(psInclude->iLine + 1))->Append("): ");
			switch (psUse->iWhat)
			{
			case IDE_INCLUDE_KEPT:
				pszDest->Append("kept, ");
				break;
			case IDE_INCLUDE_LEFT:
				pszDest->Append("left, ");
				break;
			case IDE_INCLUDE_REMOVED:
				pszDest->Append("removed, ");
				break;
			case IDE_INCLUDE_NEEDED:
				pszDest->Append("needed, ");
				break;
			default:
				pszDest->Append("a forward declaration");
				for (j = 0; j < psUse->aszClasses.NumElements(); j++)
				{
					pszDest->Append((j == 0) ? ": " : " ")->Append(psUse->aszClasses.GetText(j));
				}
				break;
			}
			pszDest->Append(psUse->szWhy)->AppendNewLine();
		}
	}
	for (i = 0; i < psPlan->aszAdded.NumElements(); i++)
	{
		szName.Init();
		pcModel->MakeIncludeName(psPlan->szFileName.Text(), psPlan->aszAdded.GetText(i), &szName);
		pszDest->Append("  added: #include \"")->Append(szName)->Append("\", ")->Append(psPlan->aszAddedWhy.GetText(i))->AppendNewLine();
		szName.Kill();
	}
	for (i = 0; i < psPlan->aszDeclared.NumElements(); i++)
	{
		pszDest->Append("  added: ")->Append(psPlan->aszDeclared.GetText(i))->AppendNewLine();
	}
	for (i = 0; i < psPlan->aszDependants.NumElements(); i++)
	{
		szName.Init();
		pcModel->MakeIncludeName(psPlan->aszDependants.GetText(i), psPlan->aszDependantHeaders.GetText(i), &szName);
		pszDest->Append("  ")->Append(GetIDEFileNamePart(psPlan->aszDependants.GetText(i)))->Append(" is given #include \"")->Append(szName)->Append("\"")->AppendNewLine();
		szName.Kill();
	}
	return pszDest->Text();
}
