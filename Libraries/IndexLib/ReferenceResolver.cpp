/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela IndexLib

Codaphela IndexLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela IndexLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela IndexLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include <ctype.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "WorkspaceIndex.h"
#include "MarkdownSpans.h"
#include "ReferenceResolver.h"


//////////////////////////////////////////////////////////////////////////
//
// A character of a name, as Python's \w has it (UTF-8's bytes count).
//
//////////////////////////////////////////////////////////////////////////
static bool IsReferenceWordChar(char c)
{
	return isalnum((unsigned char)c) || (c == '_') || ((unsigned char)c >= 0x80);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool IsReferenceSpace(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f') || (c == '\v');
}


//////////////////////////////////////////////////////////////////////////
//
// szText[0, uiLength) without the white space around it.
//
//////////////////////////////////////////////////////////////////////////
static void SetStripped(CChars* pszDest, const char* szText, size uiLength)
{
	size	uiStart;

	for (uiStart = 0; (uiStart < uiLength) && IsReferenceSpace(szText[uiStart]); uiStart++)
	{
	}
	while ((uiLength > uiStart) && IsReferenceSpace(szText[uiLength - 1]))
	{
		uiLength--;
	}
	pszDest->Clear();
	pszDest->AppendSubString(&szText[uiStart], uiLength - uiStart);
}


//////////////////////////////////////////////////////////////////////////
//
// A name's symbols in the order a reference chooses them: types (classes,
// class templates, enums) first, typedefs last, then by file and line.
//
//////////////////////////////////////////////////////////////////////////
static int GetReferenceRank(SSymbolTableEntry* psEntry)
{
	if ((psEntry->eKind == CPPSK_Class) || (psEntry->eKind == CPPSK_ClassTemplate) || (psEntry->eKind == CPPSK_Enum))
	{
		return 0;
	}
	return (psEntry->eKind == CPPSK_Typedef) ? 2 : 1;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static int CompareReferenceNames(const void* pvLeft, const void* pvRight)
{
	SReferenceName*	psLeft;
	SReferenceName*	psRight;
	int				iCompare;

	psLeft = (SReferenceName*)pvLeft;
	psRight = (SReferenceName*)pvRight;
	iCompare = StringCompare(psLeft->szName.Text(), psRight->szName.Text());
	if (iCompare != 0)
	{
		return iCompare;
	}
	iCompare = GetReferenceRank(psLeft->psEntry) - GetReferenceRank(psRight->psEntry);
	if (iCompare != 0)
	{
		return iCompare;
	}
	iCompare = StringCompare(psLeft->psEntry->szFile.Text(), psRight->psEntry->szFile.Text());
	if (iCompare != 0)
	{
		return iCompare;
	}
	return psLeft->psEntry->iLine - psRight->psEntry->iLine;
}


//////////////////////////////////////////////////////////////////////////
//
// The names references can use for pcTable's symbols.  paszFiles are the
// files a reference can name (relative to the table's root); they aren't
// copied.
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::Init(CSymbolTable* pcTable, CArrayChars* paszFiles)
{
	size				i;
	SSymbolTableEntry*	psEntry;
	CChars				szName;
	CChars				szRest;
	size				uiEnum;
	size				uiLast;

	mpcTable = pcTable;
	mpaszFiles = paszFiles;
	masNames.Init();

	szName.Init();
	szRest.Init();
	for (i = 0; i < pcTable->NumEntries(); i++)
	{
		psEntry = pcTable->Get(i);
		SplitName(psEntry->szKey.Text(), &szName, &szRest);
		AddName(szName.Text(), psEntry);
		if (psEntry->eKind == CPPSK_Enumerator)
		{
			//E65Access::MA65_Read is MA65_Read too, N::EKind::EK_Square N::EK_Square.
			uiLast = szName.FindFromEnd("::");
			if ((uiLast != ARRAY_ELEMENT_NOT_FOUND) && (uiLast > 0))
			{
				uiEnum = szName.FindFromEnd(uiLast - 1, "::");
				szRest.Clear();
				if (uiEnum != ARRAY_ELEMENT_NOT_FOUND)
				{
					szRest.AppendSubString(szName.Text(), uiEnum + 2);
				}
				szRest.Append(&szName.Text()[uiLast + 2]);
				AddName(szRest.Text(), psEntry);
			}
		}
	}
	szRest.Kill();
	szName.Kill();
	masNames.QuickSort(CompareReferenceNames);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::Kill(void)
{
	size	i;

	for (i = 0; i < masNames.NumElements(); i++)
	{
		masNames.Get(i)->szName.Kill();
	}
	masNames.Kill();
	mpaszFiles = NULL;
	mpcTable = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::AddName(const char* szName, SSymbolTableEntry* psEntry)
{
	SReferenceName*	psName;

	psName = masNames.Add();
	psName->szName.Init(szName);
	psName->psEntry = psEntry;
}


//////////////////////////////////////////////////////////////////////////
//
// The first of szName's names, MAX_SIZE if there is none.
//
//////////////////////////////////////////////////////////////////////////
size CReferenceResolver::FindFirstName(const char* szName)
{
	size	uiLow;
	size	uiHigh;
	size	uiMiddle;

	uiLow = 0;
	uiHigh = masNames.NumElements();
	while (uiLow < uiHigh)
	{
		uiMiddle = uiLow + (uiHigh - uiLow) / 2;
		if (StringCompare(masNames.Get(uiMiddle)->szName.Text(), szName) < 0)
		{
			uiLow = uiMiddle + 1;
		}
		else
		{
			uiHigh = uiMiddle;
		}
	}
	if ((uiLow < masNames.NumElements()) && (StringCompare(masNames.Get(uiLow)->szName.Text(), szName) == 0))
	{
		return uiLow;
	}
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol szText names, NULL if none.  Its key exactly, else its name:
// the first of an overloaded name's, or with parameters the one that has
// them.
//
//////////////////////////////////////////////////////////////////////////
SSymbolTableEntry* CReferenceResolver::ResolveSymbol(const char* szText)
{
	CChars				szWanted;
	CChars				szName;
	CChars				szRest;
	CArrayChars			aszWanted;
	CChars				szWantedAfter;
	CArrayChars			aszHas;
	CChars				szHasAfter;
	CChars				szHasName;
	CChars				szHasRest;
	SSymbolTableEntry*	psEntry;
	SSymbolTableEntry*	psFound;
	size				uiFirst;
	size				i;
	size				j;
	bool				bSame;

	szWanted.Init();
	SetStripped(&szWanted, szText, strlen(szText));
	psFound = mpcTable->Find(szWanted.Text());
	if (psFound)
	{
		szWanted.Kill();
		return psFound;
	}

	szName.Init();
	szRest.Init();
	SplitName(szWanted.Text(), &szName, &szRest);
	uiFirst = FindFirstName(szName.Text());
	if ((uiFirst != MAX_SIZE) && szRest.Empty())
	{
		psFound = masNames.Get(uiFirst)->psEntry;
	}
	else if (uiFirst != MAX_SIZE)
	{
		aszWanted.Init();
		szWantedAfter.Init();
		if (SplitParameters(szRest.Text(), &aszWanted, &szWantedAfter))
		{
			aszHas.Init();
			szHasAfter.Init();
			szHasName.Init();
			szHasRest.Init();
			for (i = uiFirst; (psFound == NULL) && (i < masNames.NumElements()) && masNames.Get(i)->szName.Equals(szName.Text()); i++)
			{
				psEntry = masNames.Get(i)->psEntry;
				SplitName(psEntry->szKey.Text(), &szHasName, &szHasRest);
				aszHas.ReInit();
				if (SplitParameters(szHasRest.Text(), &aszHas, &szHasAfter) && (aszHas.NumElements() == aszWanted.NumElements()))
				{
					bSame = szWantedAfter.Empty() || szWantedAfter.Equals(szHasAfter.Text());
					for (j = 0; bSame && (j < aszHas.NumElements()); j++)
					{
						bSame = aszHas.Get(j)->Equals(aszWanted.Get(j)->Text());
					}
					if (bSame)
					{
						psFound = psEntry;
					}
				}
			}
			szHasRest.Kill();
			szHasName.Kill();
			szHasAfter.Kill();
			aszHas.Kill();
		}
		szWantedAfter.Kill();
		aszWanted.Kill();
	}
	szRest.Kill();
	szName.Kill();
	szWanted.Kill();
	return psFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The files whose path is szText or ends with "/" and it, added to
// paszDest; how many there are.
//
//////////////////////////////////////////////////////////////////////////
size CReferenceResolver::ResolveFiles(const char* szText, CArrayChars* paszDest)
{
	CChars	szWanted;
	CChars	szEnd;
	CChars*	pszFile;
	size	i;
	size	uiFound;

	szWanted.Init();
	SetStripped(&szWanted, szText, strlen(szText));
	while (szWanted.StartsWith('/'))
	{
		szWanted.RemoveFromStart(1);
	}
	while (szWanted.EndsWith('/'))
	{
		szWanted.RemoveFromEnd(1);
	}
	szEnd.Init("/");
	szEnd.Append(szWanted.Text());

	uiFound = 0;
	for (i = 0; (mpaszFiles != NULL) && (i < mpaszFiles->NumElements()); i++)
	{
		pszFile = mpaszFiles->Get(i);
		if (pszFile->Equals(szWanted.Text()) || pszFile->EndsWith(szEnd.Text()))
		{
			paszDest->Add(pszFile->Text());
			uiFound++;
		}
	}
	szEnd.Kill();
	szWanted.Kill();
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Each reference in the page, linked or not, and each plain code span
// that could be one, added to pasDest in the page's order.
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::ResolvePage(const char* szPage, CArrayPageReferences* pasDest)
{
	CMarkdownSpans		cSpans;
	SMarkdownSpan*		psSpan;
	SPageReference*		psReference;
	SSymbolTableEntry*	psEntry;
	size				i;

	cSpans.Init();
	cSpans.Scan(szPage);
	for (i = 0; i < cSpans.NumSpans(); i++)
	{
		psSpan = cSpans.Get(i);
		if (psSpan->eKind == MSK_Reference)
		{
			psReference = pasDest->Add();
			psReference->szFile.Init();
			psReference->szMessage.Init();
			psReference->szText.Init(psSpan->szText.Text());
			psReference->iLine = psSpan->iLine;
			psReference->iColumn = psSpan->iColumn - 1;
			psReference->iEnd = psSpan->iEnd + 1;
			psReference->iText = psSpan->iText;
			ResolveReference(psSpan->szText.Text(), psReference);
		}
		else if ((psSpan->eKind == MSK_Plain) && CouldBeReference(psSpan->szText.Text()))
		{
			psEntry = ResolveSymbol(psSpan->szText.Text());
			if (psEntry && IsInHeader(psEntry))
			{
				psReference = pasDest->Add();
				psReference->eKind = PRK_Warning;
				psReference->iLine = psSpan->iLine;
				psReference->iColumn = psSpan->iColumn;
				psReference->iEnd = psSpan->iEnd;
				psReference->iText = psSpan->iText;
				psReference->szText.Init(psSpan->szText.Text());
				psReference->psSymbol = psEntry;
				psReference->szFile.Init(psEntry->szFile.Text());
				psReference->iTargetLine = psEntry->iLine;
				psReference->szMessage.Init();
				psReference->szMessage.Append("`");
				psReference->szMessage.Append(psSpan->szText.Text());
				psReference->szMessage.Append("` names ");
				psReference->szMessage.Append(CSymbolTable::GetKindName(psEntry->eKind));
				psReference->szMessage.Append(" ");
				psReference->szMessage.Append(psEntry->szKey.Text());
				psReference->szMessage.Append(" (");
				psReference->szMessage.Append(psEntry->szFile.Text());
				psReference->szMessage.Append(":");
				psReference->szMessage.Append(psEntry->iLine);
				psReference->szMessage.Append("): write [`");
				psReference->szMessage.Append(psSpan->szText.Text());
				psReference->szMessage.Append("`] to link to it");
			}
		}
	}
	cSpans.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What [`szText`] names: psDest's kind, link and message.
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::ResolveReference(const char* szText, SPageReference* psDest)
{
	CArrayChars			aszFiles;
	SSymbolTableEntry*	psEntry;
	size				uiFound;
	size				i;

	psDest->eKind = PRK_Error;
	psDest->psSymbol = NULL;
	psDest->iTargetLine = 0;
	if (IsFileReference(szText))
	{
		aszFiles.Init();
		uiFound = ResolveFiles(szText, &aszFiles);
		if (uiFound == 1)
		{
			psDest->eKind = PRK_Link;
			psDest->szFile.Set(aszFiles.GetText(0));
		}
		else if (uiFound == 0)
		{
			psDest->szMessage.Append("[`");
			psDest->szMessage.Append(szText);
			psDest->szMessage.Append("`] names no file in the repositories");
		}
		else
		{
			psDest->szMessage.Append("[`");
			psDest->szMessage.Append(szText);
			psDest->szMessage.Append("`] names ");
			psDest->szMessage.Append((int)uiFound);
			psDest->szMessage.Append(" files (");
			for (i = 0; (i < uiFound) && (i < 4); i++)
			{
				if (i > 0)
				{
					psDest->szMessage.Append(", ");
				}
				psDest->szMessage.Append(aszFiles.GetText(i));
			}
			if (uiFound > 4)
			{
				psDest->szMessage.Append(", ...");
			}
			psDest->szMessage.Append("): write more of its path");
		}
		aszFiles.Kill();
		return;
	}

	psEntry = ResolveSymbol(szText);
	if (psEntry)
	{
		psDest->eKind = PRK_Link;
		psDest->psSymbol = psEntry;
		psDest->szFile.Set(psEntry->szFile.Text());
		psDest->iTargetLine = psEntry->iLine;
	}
	else
	{
		psDest->szMessage.Append("[`");
		psDest->szMessage.Append(szText);
		psDest->szMessage.Append("`] names nothing the indexed solutions declare");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A name with a '/' or an extension ("web.sh") is a file's; one with
// parameters or a '::' is a symbol's.
//
//////////////////////////////////////////////////////////////////////////
bool CReferenceResolver::IsFileReference(const char* szText)
{
	const char*	szDot;
	const char*	sz;

	if (strchr(szText, '(') || strstr(szText, "::"))
	{
		return false;
	}
	if (strchr(szText, '/'))
	{
		return true;
	}
	szDot = strrchr(szText, '.');
	if ((szDot == NULL) || !isalpha((unsigned char)szDot[1]) || ((unsigned char)szDot[1] >= 0x80))
	{
		return false;
	}
	for (sz = &szDot[2]; *sz != '\0'; sz++)
	{
		if (!IsReferenceWordChar(*sz))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a plain code span is worth a warning if it names a symbol: not
// a file, not code in another language (inlinehilite's `#!asm lda`, or
// `#!text abs` for a word that isn't C++), and not a name of one or two
// letters (the registers X, Y, S, PC).
//
//////////////////////////////////////////////////////////////////////////
bool CReferenceResolver::CouldBeReference(const char* szText)
{
	CChars	szName;
	CChars	szRest;
	bool	bCould;

	if (((szText[0] == '#') && (szText[1] == '!')) || IsFileReference(szText))
	{
		return false;
	}
	szName.Init();
	szRest.Init();
	SplitName(szText, &szName, &szRest);
	bCould = szName.Length() >= 3;
	szRest.Kill();
	szName.Kill();
	return bCould;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the symbol's place is in a header, not only in a .c or .cpp
// file (where each program's main is).
//
//////////////////////////////////////////////////////////////////////////
bool CReferenceResolver::IsInHeader(SSymbolTableEntry* psEntry)
{
	return !IsIndexedSourceFile(psEntry->szFile.Text());
}


//////////////////////////////////////////////////////////////////////////
//
// A key's name and the rest: "CShape::Init(int) const" is "CShape::Init"
// and "(int) const"; "CFoo::operator()(int)" is "CFoo::operator()" and
// "(int)".
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::SplitName(const char* szKey, CChars* pszName, CChars* pszRest)
{
	const char*	szOperator;
	const char*	sz;
	const char*	szEnd;
	size		uiLength;

	uiLength = strlen(szKey);
	szEnd = NULL;
	for (szOperator = strstr(szKey, "operator"); (szOperator != NULL) && (szEnd == NULL); szOperator = strstr(szOperator + 1, "operator"))
	{
		if ((szOperator > szKey) && IsReferenceWordChar(szOperator[-1]))
		{
			continue;
		}
		for (sz = szOperator + 8; IsReferenceSpace(*sz); sz++)
		{
		}
		if (*sz != '(')
		{
			continue;
		}
		for (sz++; IsReferenceSpace(*sz); sz++)
		{
		}
		if (*sz == ')')
		{
			szEnd = sz + 1;
		}
	}
	if (szEnd == NULL)
	{
		szEnd = strchr(szKey, '(');
		if (szEnd == NULL)
		{
			szEnd = &szKey[uiLength];
		}
	}
	SetStripped(pszName, szKey, szEnd - szKey);
	SetStripped(pszRest, szEnd, &szKey[uiLength] - szEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// "(int, CArrayTemplate<M, N>*) const" is the parameters "int" and
// "CArrayTemplate<M,N>*", each normalised, and after them "const"; "(void)"
// is none.  False if szText isn't a parameter list.
//
//////////////////////////////////////////////////////////////////////////
bool CReferenceResolver::SplitParameters(const char* szText, CArrayChars* paszParameters, CChars* pszAfter)
{
	int			iDepth;
	size		i;
	size		uiClose;
	size		uiStart;
	CChars*		pszParameter;
	size		uiLength;

	if (szText[0] != '(')
	{
		return false;
	}
	uiLength = strlen(szText);
	iDepth = 0;
	uiClose = MAX_SIZE;
	for (i = 0; (i < uiLength) && (uiClose == MAX_SIZE); i++)
	{
		if ((szText[i] == '(') || (szText[i] == '<') || (szText[i] == '['))
		{
			iDepth++;
		}
		else if ((szText[i] == ')') || (szText[i] == '>') || (szText[i] == ']'))
		{
			iDepth--;
			if (iDepth == 0)
			{
				uiClose = i;
			}
		}
	}
	if (uiClose == MAX_SIZE)
	{
		return false;
	}

	iDepth = 0;
	uiStart = 1;
	for (i = 1; i <= uiClose; i++)
	{
		if ((i < uiClose) && ((szText[i] == '(') || (szText[i] == '<') || (szText[i] == '[')))
		{
			iDepth++;
		}
		else if ((i < uiClose) && ((szText[i] == ')') || (szText[i] == '>') || (szText[i] == ']')))
		{
			iDepth--;
		}
		else if ((i == uiClose) || ((szText[i] == ',') && (iDepth == 0)))
		{
			pszParameter = paszParameters->Add("");
			NormaliseType(&szText[uiStart], i - uiStart, pszParameter);
			if (pszParameter->Empty())
			{
				paszParameters->RemoveTail();
			}
			uiStart = i + 1;
		}
	}
	if ((paszParameters->NumElements() == 1) && paszParameters->Get(0)->Equals("void"))
	{
		paszParameters->ReInit();
	}
	SetStripped(pszAfter, &szText[uiClose + 1], uiLength - uiClose - 1);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A parameter's type written one way: "const char *" and "char const*"
// are both "charconst*".
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::NormaliseType(const char* szText, size uiLength, CChars* pszDest)
{
	CChars		szStripped;
	CChars		szMoved;
	const char*	sz;
	const char*	szWord;
	const char*	szAfter;
	const char*	szClose;
	size		i;

	szStripped.Init();
	SetStripped(&szStripped, szText, uiLength);
	szMoved.Init();
	sz = szStripped.Text();
	if ((strncmp(sz, "const", 5) == 0) && IsReferenceSpace(sz[5]))
	{
		for (szWord = &sz[5]; IsReferenceSpace(*szWord); szWord++)
		{
		}
		for (szAfter = szWord; IsReferenceWordChar(*szAfter) || (*szAfter == ':'); szAfter++)
		{
		}
		if (szAfter > szWord)
		{
			if (*szAfter == '<')
			{
				szClose = strrchr(szAfter, '>');
				if (szClose)
				{
					szAfter = szClose + 1;
				}
			}
			szMoved.AppendSubString(szWord, szAfter - szWord);
			szMoved.Append(" const");
			szMoved.Append(szAfter);
			sz = szMoved.Text();
		}
	}

	pszDest->Clear();
	for (i = 0; sz[i] != '\0'; i++)
	{
		if (!IsReferenceSpace(sz[i]))
		{
			pszDest->Append(sz[i]);
		}
	}
	szMoved.Kill();
	szStripped.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A list of files a reference can name, one a line (blank lines skipped),
// added to paszDest.  False if the file can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CReferenceResolver::ReadFiles(const char* szFileName, CArrayChars* paszDest)
{
	CTextFile	cFile;
	const char*	szLine;
	const char*	szEnd;
	CChars*		pszFile;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	for (szLine = cFile.Text(); *szLine != '\0'; szLine = (*szEnd == '\0') ? szEnd : szEnd + 1)
	{
		szEnd = strchr(szLine, '\n');
		if (szEnd == NULL)
		{
			szEnd = &szLine[strlen(szLine)];
		}
		pszFile = paszDest->Add("");
		SetStripped(pszFile, szLine, szEnd - szLine);
		if (pszFile->Empty())
		{
			paszDest->RemoveTail();
		}
	}
	cFile.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CReferenceResolver::KillPageReferences(CArrayPageReferences* pasReferences)
{
	size	i;

	for (i = 0; i < pasReferences->NumElements(); i++)
	{
		pasReferences->Get(i)->szMessage.Kill();
		pasReferences->Get(i)->szFile.Kill();
		pasReferences->Get(i)->szText.Kill();
	}
	pasReferences->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// One line a reference or warning, as hcp65index -resolve prints them for
// the website's build (HCP65/Website/tools/code_references.py), lines and
// columns 1 based:
//	<page> TAB <line> TAB <column> TAB link TAB <after its ']'> TAB <file> TAB <line in it, 0 for a file>
//	<page> TAB <line> TAB <column> TAB error|warning TAB <message>
//
//////////////////////////////////////////////////////////////////////////
char* CReferenceResolver::PrintPageReferences(const char* szPage, CArrayPageReferences* pasReferences, CChars* pszDest)
{
	size			i;
	SPageReference*	psReference;

	for (i = 0; i < pasReferences->NumElements(); i++)
	{
		psReference = pasReferences->Get(i);
		pszDest->Append(szPage);
		pszDest->Append('\t');
		pszDest->Append(psReference->iLine + 1);
		pszDest->Append('\t');
		pszDest->Append(psReference->iColumn + 1);
		pszDest->Append('\t');
		if (psReference->eKind == PRK_Link)
		{
			pszDest->Append("link\t");
			pszDest->Append(psReference->iEnd);
			pszDest->Append('\t');
			pszDest->Append(psReference->szFile.Text());
			pszDest->Append('\t');
			pszDest->Append(psReference->iTargetLine);
		}
		else
		{
			pszDest->Append((psReference->eKind == PRK_Error) ? "error\t" : "warning\t");
			pszDest->Append(psReference->szMessage.Text());
		}
		pszDest->AppendNewLine();
	}
	return pszDest->Text();
}

