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
#include <stdlib.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/ArrayChars.h"
#include "CppCompilerLib/CPPSemanticPass.h"
#include "WorkspaceIndex.h"
#include "MarkdownSpans.h"
#include "PageExamples.h"


//The function a block of statements is compiled in.
#define PAGE_EXAMPLE_FUNCTION	"void PageExample(void)\n{\n"


//Words a block that declares starts with.
static const char*	gaszPageExampleDeclarers[] = { "class", "struct", "union", "enum", "namespace", "template", "typedef", "extern", "using", "#" };


//////////////////////////////////////////////////////////////////////////
//
// Whether the line (iLength bytes) has nothing in it from iStart.
//
//////////////////////////////////////////////////////////////////////////
static bool IsPageExampleBlank(const char* szLine, int iStart, int iLength)
{
	int		i;

	for (i = iStart; i < iLength; i++)
	{
		if ((szLine[i] != ' ') && (szLine[i] != '\t') && (szLine[i] != '\r'))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The line (iLength bytes) without the white space around it is szText.
//
//////////////////////////////////////////////////////////////////////////
static bool IsPageExampleLine(const char* szLine, int iLength, const char* szText)
{
	int		iStart;
	int		iEnd;
	int		iText;

	for (iStart = 0; (iStart < iLength) && ((szLine[iStart] == ' ') || (szLine[iStart] == '\t')); iStart++)
	{
	}
	for (iEnd = iLength; (iEnd > iStart) && ((szLine[iEnd - 1] == ' ') || (szLine[iEnd - 1] == '\t') || (szLine[iEnd - 1] == '\r')); iEnd--)
	{
	}
	iText = (int)strlen(szText);
	return (iEnd - iStart == iText) && (strncmp(&szLine[iStart], szText, iText) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether two file names are one file: their separators and case aside.
//
//////////////////////////////////////////////////////////////////////////
static bool IsPageExampleFile(const char* szFileName, const char* szOther)
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


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::Init(void)
{
	mszHidden.Init();
	miHidden = -1;
	miHiddenLines = 0;
	masExamples.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::Kill(void)
{
	size	i;

	for (i = 0; i < masExamples.NumElements(); i++)
	{
		masExamples.Get(i)->szCode.Kill();
	}
	masExamples.Kill();
	mszHidden.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The page's hidden comment and its ```cpp blocks.  A fence is ``` or ~~~
// (three or more), closed by a run of as many or more of the same; the
// hidden comment is outside fences.
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::Scan(const char* szPage)
{
	const char*		szLine;
	const char*		szEnd;
	int				iLength;
	int				iLine;
	char			cFence;
	int				iFence;
	char			cRun;
	int				iRun;
	int				iAfter;
	bool			bHidden;
	SPageExample*	psExample;

	cFence = '\0';
	iFence = 0;
	bHidden = false;
	psExample = NULL;
	szLine = szPage;
	for (iLine = 0;; iLine++)
	{
		szEnd = strchr(szLine, '\n');
		iLength = szEnd ? (int)(szEnd - szLine) : (int)strlen(szLine);

		if (bHidden)
		{
			if (IsPageExampleLine(szLine, iLength, "-->"))
			{
				bHidden = false;
			}
			else
			{
				mszHidden.AppendSubString(szLine, iLength);
				mszHidden.Append('\n');
				miHiddenLines++;
			}
		}
		else if (iFence > 0)
		{
			iRun = CMarkdownSpans::FindFence(szLine, iLength, &cRun, &iAfter);
			if ((iRun > 0) && (cRun == cFence) && (iRun >= iFence) && IsPageExampleBlank(szLine, iAfter, iLength))
			{
				iFence = 0;
				psExample = NULL;
			}
			else if (psExample != NULL)
			{
				psExample->szCode.AppendSubString(szLine, iLength);
				psExample->szCode.Append('\n');
				psExample->iLines++;
			}
		}
		else
		{
			iRun = CMarkdownSpans::FindFence(szLine, iLength, &cRun, &iAfter);
			if (iRun > 0)
			{
				cFence = cRun;
				iFence = iRun;
				if (IsCheckedFence(&szLine[iAfter], iLength - iAfter))
				{
					psExample = masExamples.Add();
					psExample->iFence = iLine;
					psExample->iLines = 0;
					psExample->bDeclares = false;
					psExample->szCode.Init();
				}
			}
			else if (IsPageExampleLine(szLine, iLength, "<!-- cpp"))
			{
				bHidden = true;
				if (miHidden < 0)
				{
					miHidden = iLine + 1;
				}
			}
		}

		if (!szEnd)
		{
			break;
		}
		szLine = szEnd + 1;
	}

	for (iLine = 0; iLine < (int)masExamples.NumElements(); iLine++)
	{
		psExample = masExamples.Get(iLine);
		psExample->bDeclares = IsDeclaring(psExample->szCode.Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a fence's info string (after its backticks) makes it a block
// that is checked: cpp, or {.cpp ...} without .no-check.
//
//////////////////////////////////////////////////////////////////////////
bool CPageExamples::IsCheckedFence(const char* szInfo, int iLength)
{
	int		i;
	int		iWord;
	bool	bCpp;

	for (i = 0; (i < iLength) && ((szInfo[i] == ' ') || (szInfo[i] == '\t')); i++)
	{
	}
	if ((i + 3 <= iLength) && (strncmp(&szInfo[i], "cpp", 3) == 0))
	{
		return (i + 3 == iLength) || (szInfo[i + 3] == ' ') || (szInfo[i + 3] == '\t') || (szInfo[i + 3] == '\r');
	}
	if ((i >= iLength) || (szInfo[i] != '{'))
	{
		return false;
	}

	bCpp = false;
	for (i++; (i < iLength) && (szInfo[i] != '}');)
	{
		for (; (i < iLength) && ((szInfo[i] == ' ') || (szInfo[i] == '\t')); i++)
		{
		}
		for (iWord = i; (i < iLength) && (szInfo[i] != ' ') && (szInfo[i] != '\t') && (szInfo[i] != '}'); i++)
		{
		}
		if ((i - iWord == 4) && (strncmp(&szInfo[iWord], ".cpp", 4) == 0))
		{
			bCpp = true;
		}
		else if ((i - iWord == 9) && (strncmp(&szInfo[iWord], ".no-check", 9) == 0))
		{
			return false;
		}
	}
	return bCpp;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a block declares, so is compiled at file scope: its first line
// of code (not blank, not a // comment) starts at column 0 with a word
// that declares (class, namespace, #include...), or is followed by "{"
// at column 0 (a function written out).
//
//////////////////////////////////////////////////////////////////////////
bool CPageExamples::IsDeclaring(const char* szCode)
{
	CArrayChars		aszLines;
	const char*		szLine;
	const char*		szFirst;
	size			i;
	size			j;
	size			uiLength;
	bool			bDeclares;

	aszLines.Init();
	aszLines.Split(szCode, '\n');
	bDeclares = false;
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		szLine = aszLines.GetText(i);
		for (szFirst = szLine; (*szFirst == ' ') || (*szFirst == '\t'); szFirst++)
		{
		}
		if (IsPageExampleBlank(szLine, 0, (int)strlen(szLine)) || (strncmp(szFirst, "//", 2) == 0))
		{
			continue;
		}
		if ((szLine[0] == ' ') || (szLine[0] == '\t'))
		{
			break;
		}
		for (j = 0; j < sizeof(gaszPageExampleDeclarers) / sizeof(char*); j++)
		{
			uiLength = strlen(gaszPageExampleDeclarers[j]);
			if ((strncmp(szLine, gaszPageExampleDeclarers[j], uiLength) == 0) && ((szLine[0] == '#') || (szLine[uiLength] == ' ') || (szLine[uiLength] == '\t') || (szLine[uiLength] == '<') || (szLine[uiLength] == '\0')))
			{
				bDeclares = true;
			}
		}
		for (j = i + 1; !bDeclares && (j < aszLines.NumElements()); j++)
		{
			szLine = aszLines.GetText(j);
			if (!IsPageExampleBlank(szLine, 0, (int)strlen(szLine)))
			{
				bDeclares = (szLine[0] == '{');
				break;
			}
		}
		break;
	}
	aszLines.Kill();
	return bDeclares;
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
size CPageExamples::NumExamples(void) { return masExamples.NumElements(); }
SPageExample* CPageExamples::Get(size uiIndex) { return masExamples.Get(uiIndex); }
bool CPageExamples::HasHidden(void) { return miHidden >= 0; }
const char* CPageExamples::GetHidden(void) { return mszHidden.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// The text compiled for an example: the hidden lines, then the block, in
// a function if it doesn't declare.
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::MakeUnit(size uiIndex, CChars* pszDest)
{
	SPageExample*	psExample;

	psExample = masExamples.Get(uiIndex);
	pszDest->Append(&mszHidden);
	if (psExample->bDeclares)
	{
		pszDest->Append(&psExample->szCode);
	}
	else
	{
		pszDest->Append(PAGE_EXAMPLE_FUNCTION);
		pszDest->Append(&psExample->szCode);
		pszDest->Append("}\n");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The page line (0 based) of a line of an example's unit: a hidden line's
// own, a line of the block its own, and the function around a block its
// opening fence's.
//
//////////////////////////////////////////////////////////////////////////
int CPageExamples::GetPageLine(size uiIndex, int iUnitLine)
{
	SPageExample*	psExample;
	int				iStart;

	psExample = masExamples.Get(uiIndex);
	if ((iUnitLine >= 0) && (iUnitLine < miHiddenLines))
	{
		return miHidden + iUnitLine;
	}
	iStart = miHiddenLines + (psExample->bDeclares ? 0 : 2);
	if ((iUnitLine >= iStart) && (iUnitLine < iStart + psExample->iLines))
	{
		return psExample->iFence + 1 + (iUnitLine - iStart);
	}
	return psExample->iFence;
}


//////////////////////////////////////////////////////////////////////////
//
// The file an example is compiled as: beside its page, so that what it
// includes is found as from the page's folder.
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::GetUnitFile(const char* szPageFile, size uiIndex, CChars* pszDest)
{
	pszDest->Append(szPageFile);
	pszDest->Append('.');
	pszDest->Append((int)(uiIndex + 1));
	pszDest->Append(".cpp");
}


//////////////////////////////////////////////////////////////////////////
//
// Each example compiled as a unit of the workspace's project uiProject
// (the page's) and checked: the preprocessor's, the parser's and the
// check's errors in the example's own text, at their page lines.  The
// page must have been scanned.  False if an example couldn't be compiled
// at all.
//
//////////////////////////////////////////////////////////////////////////
bool CPageExamples::Check(CWorkspaceIndex* pcIndex, size uiProject, const char* szPageFile, CArrayPageExampleProblems* pasDest)
{
	CParsedUnit			cUnit;
	CCPPSemanticPass	cPass;
	CChars				szText;
	CChars				szUnitFile;
	CChars				szErrors;
	size				i;
	bool				bResult;

	bResult = true;
	for (i = 0; i < masExamples.NumElements(); i++)
	{
		szText.Init();
		szUnitFile.Init();
		szErrors.Init();
		MakeUnit(i, &szText);
		GetUnitFile(szPageFile, i, &szUnitFile);
		cUnit.Init();
		if (!pcIndex->ParseUnit(uiProject, szUnitFile.Text(), szText.Text(), &cUnit, &szErrors))
		{
			AddProblem(pasDest, masExamples.Get(i)->iFence, 0, "The page's project can't compile its examples.");
			bResult = false;
		}
		else
		{
			AddMessages(pasDest, szErrors.Text(), i, szUnitFile.Text());
			AddErrors(pasDest, cUnit.mcTree.GetErrors(), i, szUnitFile.Text());
			cPass.Init();
			cPass.SetOptions(&cUnit.mcOptions);
			cPass.Check(&cUnit.mcTree, CPPT_Host, false);
			AddErrors(pasDest, cPass.GetErrors(), i, szUnitFile.Text());
			cPass.Kill();
		}
		cUnit.Kill();
		szErrors.Kill();
		szUnitFile.Kill();
		szText.Kill();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::AddProblem(CArrayPageExampleProblems* pasDest, int iLine, int iColumn, const char* szMessage)
{
	SPageExampleProblem*	psProblem;

	psProblem = pasDest->Add();
	psProblem->iLine = iLine;
	psProblem->iColumn = iColumn;
	psProblem->szMessage.Init(szMessage);
}


//////////////////////////////////////////////////////////////////////////
//
// The errors in the example's unit file (szUnitFile), at their page
// lines.  An error in a header is the header's.
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::AddErrors(CArrayPageExampleProblems* pasDest, CCPPErrors* pcErrors, size uiIndex, const char* szUnitFile)
{
	size		i;
	const char*	szText;
	const char*	szMessage;
	char*		szFile;
	int			iLine;

	for (i = 0; i < pcErrors->NumErrors(); i++)
	{
		szFile = pcErrors->GetFile(i);
		if ((szFile == NULL) || !IsPageExampleFile(szFile, szUnitFile))
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
		iLine = GetPageLine(uiIndex, (pcErrors->GetLine(i) >= 0) ? pcErrors->GetLine(i) : 0);
		AddProblem(pasDest, iLine, (pcErrors->GetColumn(i) >= 0) ? pcErrors->GetColumn(i) : 0, szMessage);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The preprocessor's messages (CCPPErrors' text: "file (line, column):
// message", a line each) that are in the example's unit file.
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::AddMessages(CArrayPageExampleProblems* pasDest, const char* szMessages, size uiIndex, const char* szUnitFile)
{
	CArrayChars		aszLines;
	CChars			szFile;
	const char*		szLine;
	const char*		szClose;
	const char*		szOpen;
	int				iLine;
	int				iColumn;
	size			i;

	aszLines.Init();
	aszLines.Split(szMessages, '\n');
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		szLine = aszLines.GetText(i);
		szClose = strstr(szLine, "): ");
		if (szClose == NULL)
		{
			continue;
		}
		for (szOpen = szClose; (szOpen > szLine) && (*szOpen != '('); szOpen--)
		{
		}
		if ((szOpen <= szLine) || (szOpen[-1] != ' '))
		{
			continue;
		}
		szFile.Init();
		szFile.AppendSubString(szLine, szOpen - 1);
		if (IsPageExampleFile(szFile.Text(), szUnitFile))
		{
			iLine = atoi(szOpen + 1) - 1;
			szOpen = strchr(szOpen, ',');
			iColumn = ((szOpen != NULL) && (szOpen < szClose)) ? atoi(szOpen + 1) - 1 : 0;
			AddProblem(pasDest, GetPageLine(uiIndex, iLine), (iColumn >= 0) ? iColumn : 0, szClose + 3);
		}
		szFile.Kill();
	}
	aszLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::KillProblems(CArrayPageExampleProblems* pasProblems)
{
	size	i;

	for (i = 0; i < pasProblems->NumElements(); i++)
	{
		pasProblems->Get(i)->szMessage.Kill();
	}
	pasProblems->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A line each, as the website's build and the IDE's Build tab read them:
// "<page>:<line>:<column>: error: <message>", from 1.
//
//////////////////////////////////////////////////////////////////////////
void CPageExamples::PrintProblems(const char* szPageFile, CArrayPageExampleProblems* pasProblems, CChars* pszDest)
{
	SPageExampleProblem*	psProblem;
	size					i;

	for (i = 0; i < pasProblems->NumElements(); i++)
	{
		psProblem = pasProblems->Get(i);
		pszDest->Append(szPageFile);
		pszDest->Append(':');
		pszDest->Append(psProblem->iLine + 1);
		pszDest->Append(':');
		pszDest->Append(psProblem->iColumn + 1);
		pszDest->Append(": error: ");
		pszDest->Append(&psProblem->szMessage);
		pszDest->Append('\n');
	}
}

