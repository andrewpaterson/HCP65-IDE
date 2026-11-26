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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/ArrayChars.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "WorkspaceIndex.h"
#include "SymbolTable.h"


//////////////////////////////////////////////////////////////////////////
//
// The entries in order of their keys.
//
//////////////////////////////////////////////////////////////////////////
static int CompareSymbolTableEntries(const void* pvLeft, const void* pvRight)
{
	return StringCompare(((SSymbolTableEntry*)pvLeft)->szKey.Text(), ((SSymbolTableEntry*)pvRight)->szKey.Text());
}


//////////////////////////////////////////////////////////////////////////
//
// szRoot is the folder the table's file names are relative to.
//
//////////////////////////////////////////////////////////////////////////
void CSymbolTable::Init(const char* szRoot)
{
	CFileUtil	cFileUtil;

	mszRoot.Init(szRoot);
	cFileUtil.FullPath(&mszRoot);
	if (!mszRoot.EndsWith(FILE_SEPARATOR))
	{
		mszRoot.Append(FILE_SEPARATOR);
	}
	masEntries.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CSymbolTable::Kill(void)
{
	size				i;
	SSymbolTableEntry*	psEntry;

	for (i = 0; i < masEntries.NumElements(); i++)
	{
		psEntry = masEntries.Get(i);
		psEntry->szFile.Kill();
		psEntry->szKey.Kill();
	}
	masEntries.Kill();
	mszRoot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Adds each symbol of pcIndex a page can name, with the place a reference
// to it links to.  A key already in the table keeps its place.
//
//////////////////////////////////////////////////////////////////////////
void CSymbolTable::Gather(CCPPIndex* pcIndex)
{
	CArrayChars			aszFiles;
	size				i;
	CChars*				pszFile;
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psPlace;
	SSymbolTableEntry	sEntry;
	size				iIndex;

	//Each of the index's files relative to the root, empty when it isn't under it.
	aszFiles.Init();
	for (i = 0; i < pcIndex->NumFiles(); i++)
	{
		pszFile = aszFiles.Add("");
		GetRelativeFile(pcIndex->GetFileName(i), pszFile);
	}

	for (i = 0; i < pcIndex->NumSymbols(); i++)
	{
		pcSymbol = pcIndex->GetSymbol(i);
		if (!IsTabled(pcSymbol))
		{
			continue;
		}
		psPlace = FindPlace(pcIndex, &aszFiles, pcSymbol);
		if (psPlace == NULL)
		{
			continue;
		}

		sEntry.szKey.Fake(pcSymbol->mszKey.Text());
		if (masEntries.FindInSorted(&sEntry, CompareSymbolTableEntries, &iIndex))
		{
			continue;
		}
		sEntry.szKey.Init(pcSymbol->mszKey.Text());
		sEntry.eKind = pcSymbol->meKind;
		sEntry.szFile.Init(aszFiles.Get(psPlace->uiFile)->Text());
		sEntry.iLine = psPlace->iLine + 1;
		masEntries.InsertAt(&sEntry, iIndex);
	}
	aszFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a page can name the symbol: a type, a function, a variable that
// isn't inside a function, a field or an enumerator.  Not a namespace (it
// has no one place), a template's parameters, a function's parameters or
// labels.
//
//////////////////////////////////////////////////////////////////////////
bool CSymbolTable::IsTabled(CCPPIndexSymbol* pcSymbol)
{
	switch (pcSymbol->meKind)
	{
	case CPPSK_Class:
	case CPPSK_Enum:
	case CPPSK_Typedef:
	case CPPSK_ClassTemplate:
	case CPPSK_AliasTemplate:
	case CPPSK_Function:
	case CPPSK_FunctionTemplate:
	case CPPSK_Variable:
	case CPPSK_Field:
	case CPPSK_Enumerator:
	case CPPSK_VariableTemplate:
		return true;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol's declaration or definition a reference to it links to, NULL
// if it has none under the root or it is declared inside a function (a
// function's own definition is a name in the function).
//
//////////////////////////////////////////////////////////////////////////
SCPPIndexLocation* CSymbolTable::FindPlace(CCPPIndex* pcIndex, CArrayChars* paszFiles, CCPPIndexSymbol* pcSymbol)
{
	size				i;
	SCPPIndexLocation*	psLocation;
	SCPPIndexLocation*	psPlace;
	CCPPIndexSymbol*	pcContainer;

	psPlace = NULL;
	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if ((psLocation->uiRole != CPPIR_Declaration) && (psLocation->uiRole != CPPIR_Definition))
		{
			continue;
		}
		if (paszFiles->Get(psLocation->uiFile)->Empty())
		{
			continue;
		}
		if (psLocation->iContainer >= 0)
		{
			pcContainer = pcIndex->GetSymbol(psLocation->iContainer);
			if ((pcContainer != pcSymbol) && ((pcContainer->meKind == CPPSK_Function) || (pcContainer->meKind == CPPSK_FunctionTemplate)))
			{
				return NULL;
			}
		}
		if ((psPlace == NULL) || (ComparePlaces(paszFiles, pcSymbol->meKind, psLocation, psPlace) < 0))
		{
			psPlace = psLocation;
		}
	}
	return psPlace;
}


//////////////////////////////////////////////////////////////////////////
//
// Less than zero when psLeft is the better place to link to: one in a
// header; then a type's definition (where its members are) or anything
// else's declaration (a function's comment and signature); then the first
// in file name and line order, so the table is the same however the
// index's units were merged.
//
//////////////////////////////////////////////////////////////////////////
int CSymbolTable::ComparePlaces(CArrayChars* paszFiles, ECPPSymbolKind eKind, SCPPIndexLocation* psLeft, SCPPIndexLocation* psRight)
{
	bool	bLeftHeader;
	bool	bRightHeader;
	uint8	uiPreferred;
	int		iCompare;

	bLeftHeader = !IsIndexedSourceFile(paszFiles->Get(psLeft->uiFile)->Text());
	bRightHeader = !IsIndexedSourceFile(paszFiles->Get(psRight->uiFile)->Text());
	if (bLeftHeader != bRightHeader)
	{
		return bLeftHeader ? -1 : 1;
	}

	if (psLeft->uiRole != psRight->uiRole)
	{
		uiPreferred = ((eKind == CPPSK_Class) || (eKind == CPPSK_ClassTemplate) || (eKind == CPPSK_Enum)) ? CPPIR_Definition : CPPIR_Declaration;
		return (psLeft->uiRole == uiPreferred) ? -1 : 1;
	}

	iCompare = StringCompare(paszFiles->Get(psLeft->uiFile)->Text(), paszFiles->Get(psRight->uiFile)->Text());
	if (iCompare != 0)
	{
		return iCompare;
	}
	if (psLeft->iLine != psRight->iLine)
	{
		return (psLeft->iLine < psRight->iLine) ? -1 : 1;
	}
	return psLeft->iColumn - psRight->iColumn;
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName relative to the root with '/' between folders, false (and
// pszDest empty) if it isn't under the root.
//
//////////////////////////////////////////////////////////////////////////
bool CSymbolTable::GetRelativeFile(const char* szFileName, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	bool		bUnder;

	pszDest->Set(szFileName);
	cFileUtil.FullPath(pszDest);
#ifdef _WIN32
	bUnder = pszDest->StartsWithIgnoreCase(mszRoot.Text());
#else
	bUnder = pszDest->StartsWith(mszRoot.Text());
#endif
	if (!bUnder)
	{
		pszDest->Set("");
		return false;
	}
	pszDest->RemoveFromStart(mszRoot.Length());
	pszDest->Replace(FILE_SEPARATOR[0], '/');
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The table as Write writes it.
//
//////////////////////////////////////////////////////////////////////////
char* CSymbolTable::Print(CChars* pszDest)
{
	size				i;
	SSymbolTableEntry*	psEntry;

	for (i = 0; i < masEntries.NumElements(); i++)
	{
		psEntry = masEntries.Get(i);
		pszDest->Append(psEntry->szKey.Text());
		pszDest->Append('\t');
		pszDest->Append(GetKindName(psEntry->eKind));
		pszDest->Append('\t');
		pszDest->Append(psEntry->szFile.Text());
		pszDest->Append('\t');
		pszDest->Append(psEntry->iLine);
		pszDest->AppendNewLine();
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CSymbolTable::Write(const char* szFileName)
{
	CTextFile	cFile;
	CChars		szText;
	bool		bWritten;

	szText.Init();
	Print(&szText);
	cFile.Init(szText.Text());
	szText.Kill();
	bWritten = cFile.Write(szFileName);
	cFile.Kill();
	return bWritten;
}


//////////////////////////////////////////////////////////////////////////
//
// The kind's name in the written table.
//
//////////////////////////////////////////////////////////////////////////
const char* CSymbolTable::GetKindName(ECPPSymbolKind eKind)
{
	switch (eKind)
	{
	case CPPSK_Class:				return "class";
	case CPPSK_Enum:				return "enum";
	case CPPSK_Typedef:				return "typedef";
	case CPPSK_ClassTemplate:		return "class-template";
	case CPPSK_AliasTemplate:		return "alias-template";
	case CPPSK_Function:			return "function";
	case CPPSK_FunctionTemplate:	return "function-template";
	case CPPSK_Variable:			return "variable";
	case CPPSK_Field:				return "field";
	case CPPSK_Enumerator:			return "enumerator";
	case CPPSK_VariableTemplate:	return "variable-template";
	default:						return "other";
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The kind GetKindName names, false if it names none.
//
//////////////////////////////////////////////////////////////////////////
bool CSymbolTable::GetKind(const char* szName, ECPPSymbolKind* peKind)
{
	int		i;

	for (i = 0; i <= CPPSK_TemplateTemplateParameter; i++)
	{
		if ((StringCompare(GetKindName((ECPPSymbolKind)i), szName) == 0) && (StringCompare(szName, "other") != 0))
		{
			*peKind = (ECPPSymbolKind)i;
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Adds what a table Write wrote.  False (with what was read before the
// line kept) if a line isn't one; a key already in the table keeps its
// place.
//
//////////////////////////////////////////////////////////////////////////
bool CSymbolTable::Read(const char* szFileName)
{
	CTextFile			cFile;
	const char*			szLine;
	const char*			szEnd;
	const char*			aszFields[4];
	size				auiLengths[4];
	const char*			sz;
	size				uiField;
	SSymbolTableEntry	sEntry;
	CChars				szKind;
	size				uiIndex;
	bool				bRead;

	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}

	bRead = true;
	szKind.Init();
	for (szLine = cFile.Text(); bRead && (*szLine != '\0'); szLine = (*szEnd == '\0') ? szEnd : szEnd + 1)
	{
		szEnd = strchr(szLine, '\n');
		if (szEnd == NULL)
		{
			szEnd = &szLine[strlen(szLine)];
		}
		uiField = 0;
		aszFields[0] = szLine;
		for (sz = szLine; (sz < szEnd) && (uiField < 4); sz++)
		{
			if (*sz == '\t')
			{
				auiLengths[uiField] = sz - aszFields[uiField];
				uiField++;
				if (uiField < 4)
				{
					aszFields[uiField] = sz + 1;
				}
			}
		}
		if (uiField != 3)
		{
			bRead = false;
			break;
		}
		auiLengths[3] = szEnd - aszFields[3];
		if ((auiLengths[3] > 0) && (aszFields[3][auiLengths[3] - 1] == '\r'))
		{
			auiLengths[3]--;
		}

		szKind.Clear();
		szKind.AppendSubString(aszFields[1], auiLengths[1]);
		if ((auiLengths[3] == 0) || !GetKind(szKind.Text(), &sEntry.eKind))
		{
			bRead = false;
			break;
		}
		sEntry.iLine = atoi(aszFields[3]);
		sEntry.szKey.Init();
		sEntry.szKey.AppendSubString(aszFields[0], auiLengths[0]);
		if (masEntries.FindInSorted(&sEntry, CompareSymbolTableEntries, &uiIndex))
		{
			sEntry.szKey.Kill();
			continue;
		}
		sEntry.szFile.Init();
		sEntry.szFile.AppendSubString(aszFields[2], auiLengths[2]);
		masEntries.InsertAt(&sEntry, uiIndex);
	}
	szKind.Kill();
	cFile.Kill();
	return bRead;
}


size CSymbolTable::NumEntries(void) const { return masEntries.NumElements(); }
SSymbolTableEntry* CSymbolTable::Get(size uiIndex) { return masEntries.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// NULL if the table hasn't szKey.
//
//////////////////////////////////////////////////////////////////////////
SSymbolTableEntry* CSymbolTable::Find(const char* szKey)
{
	SSymbolTableEntry	sEntry;
	size				iIndex;

	sEntry.szKey.Fake(szKey);
	if (masEntries.FindInSorted(&sEntry, CompareSymbolTableEntries, &iIndex))
	{
		return masEntries.Get(iIndex);
	}
	return NULL;
}

