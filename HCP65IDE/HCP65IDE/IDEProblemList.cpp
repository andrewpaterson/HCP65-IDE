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
#include <stdlib.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/ArrayChars.h"
#include "IDEMessageOutput.h"
#include "IDEProblemList.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblemList::Init(void)
{
	masProblems.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblemList::Kill(void)
{
	Clear();
	masProblems.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblemList::Clear(void)
{
	size	i;

	for (i = 0; i < masProblems.NumElements(); i++)
	{
		masProblems.Get(i)->szMessage.Kill();
	}
	masProblems.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The problems become pcSource's, which is left empty.
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblemList::TakeFrom(CIDEProblemList* pcSource)
{
	size			i;
	SIDEProblem*	psProblem;

	Clear();
	for (i = 0; i < pcSource->NumProblems(); i++)
	{
		psProblem = pcSource->Get(i);
		Add(psProblem->iLine, psProblem->iColumn, psProblem->bWarning, psProblem->szMessage.Text());
	}
	pcSource->Clear();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblemList::Add(int32 iLine, int32 iColumn, bool bWarning, const char* szMessage)
{
	SIDEProblem*	psProblem;

	psProblem = masProblems.Add();
	psProblem->iLine = iLine;
	psProblem->iColumn = iColumn;
	psProblem->bWarning = bWarning;
	psProblem->szMessage.Init(szMessage);
}


//////////////////////////////////////////////////////////////////////////
//
// pcErrors' errors in szFileName (each names its file, by its full path:
// the code model's are made so, CProject::SetFullPathErrors) added, as
// warnings if bWarning (without the compiler's "Warning: ").  One with
// no place is at the file's start.
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblemList::AddErrors(CCPPErrors* pcErrors, const char* szFileName, bool bWarning)
{
	size		i;
	const char*	szText;
	const char*	szMessage;
	char*		szFile;

	for (i = 0; i < pcErrors->NumErrors(); i++)
	{
		szFile = pcErrors->GetFile(i);
		if ((szFile == NULL) || !IsSameFile(szFile, szFileName))
		{
			continue;
		}
		szText = pcErrors->Get(i);
		szMessage = strstr(szText, "): ");
		if (szMessage)
		{
			szMessage += 3;
		}
		else
		{
			szMessage = strstr(szText, ": ");
			szMessage = szMessage ? szMessage + 2 : szText;
		}
		if (bWarning && (strncmp(szMessage, "Warning: ", 9) == 0))
		{
			szMessage += 9;  //It is shown as a warning.
		}
		Add((pcErrors->GetLine(i) >= 0) ? pcErrors->GetLine(i) : 0, (pcErrors->GetColumn(i) >= 0) ? pcErrors->GetColumn(i) : 0, bWarning, szMessage);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szMessages' lines ("<file> (<line>, <column>): <message>", as a
// preprocessor's errors are written) that are in szFileName added.
//
//////////////////////////////////////////////////////////////////////////
void CIDEProblemList::AddMessages(const char* szMessages, const char* szFileName, bool bWarning)
{
	CArrayChars		aszLines;
	CChars			szFile;
	int32			iLine;
	int32			iColumn;
	EIDEMessageKind	eKind;
	const char*		szMessage;
	size			i;

	aszLines.Init();
	aszLines.Split(szMessages, '\n');
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		szFile.Init();
		if (CIDEMessageOutput::ParseMessageLine(aszLines.GetText(i), &szFile, &iLine, &iColumn, &eKind) && IsSameFile(szFile.Text(), szFileName))
		{
			szMessage = strstr(aszLines.GetText(i), "): ");
			Add(iLine - 1, iColumn - 1, bWarning || (eKind == IMK_Warning), szMessage ? szMessage + 3 : aszLines.GetText(i));
		}
		szFile.Kill();
	}
	aszLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Errors first, then warnings; each in the order they are in the file.
//
//////////////////////////////////////////////////////////////////////////
static int CompareIDEProblems(const void* pv1, const void* pv2)
{
	SIDEProblem*	ps1;
	SIDEProblem*	ps2;

	ps1 = (SIDEProblem*)pv1;
	ps2 = (SIDEProblem*)pv2;
	if (ps1->bWarning != ps2->bWarning)
	{
		return ps1->bWarning ? 1 : -1;
	}
	if (ps1->iLine != ps2->iLine)
	{
		return (ps1->iLine < ps2->iLine) ? -1 : 1;
	}
	if (ps1->iColumn != ps2->iColumn)
	{
		return (ps1->iColumn < ps2->iColumn) ? -1 : 1;
	}
	return 0;
}


void CIDEProblemList::Sort(void)
{
	masProblems.QuickSort(CompareIDEProblems);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CIDEProblemList::NumProblems(void) const { return masProblems.NumElements(); }
SIDEProblem* CIDEProblemList::Get(size uiIndex) { return masProblems.Get(uiIndex); }


size CIDEProblemList::NumErrors(void)
{
	size	i;
	size	uiErrors;

	uiErrors = 0;
	for (i = 0; i < masProblems.NumElements(); i++)
	{
		uiErrors += masProblems.Get(i)->bWarning ? 0 : 1;
	}
	return uiErrors;
}


size CIDEProblemList::NumWarnings(void) { return NumProblems() - NumErrors(); }


//////////////////////////////////////////////////////////////////////////
//
// Whether two file names are the same file's: compared with '\' and '/'
// alike and case ignored, as the IDE names files on Windows and Linux.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEProblemList::IsSameFile(const char* szFileName, const char* szOther)
{
	size	i;
	char	c1;
	char	c2;

	for (i = 0; ; i++)
	{
		c1 = szFileName[i];
		c2 = szOther[i];
		c1 = (c1 == '\\') ? '/' : (((c1 >= 'A') && (c1 <= 'Z')) ? (char)(c1 - 'A' + 'a') : c1);
		c2 = (c2 == '\\') ? '/' : (((c2 >= 'A') && (c2 <= 'Z')) ? (char)(c2 - 'A' + 'a') : c2);
		if (c1 != c2)
		{
			return false;
		}
		if (c1 == '\0')
		{
			return true;
		}
	}
}

