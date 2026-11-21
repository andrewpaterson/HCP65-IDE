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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/PointerFunctions.h"
#include "IDEContent.h"
#include "IDEDefinitions.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEConvertMacro.h"
#include "IDEContextActions.h"


//The files of one that Convert Macro checks as they would be: more than these aren't.
#define IDE_MACRO_CHECKED_FILES		30

//What the #define's value is written in, to ask the compiler its type.
#define IDE_MACRO_PROBE				"static void IDEMacroProbe(void) { (void)("


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMacroUse::Init(void)
{
	szFileName.Init();
	uiStart = 0;
	uiEnd = 0;
	iLine = 0;
	iColumn = 0;
	iKind = IDE_MACRO_USE_CODE;
	aszArguments.Init();
	aiArgumentStarts.Init();
	aiArgumentEnds.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMacroUse::Kill(void)
{
	aiArgumentEnds.Kill();
	aiArgumentStarts.Kill();
	aszArguments.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMacro::Init(void)
{
	szName.Init();
	bFunction = false;
	bVariadic = false;
	aszParameters.Init();
	szBody.Init();
	szComment.Init();
	szFileName.Init();
	uiStart = 0;
	uiEnd = 0;
	iLine = 0;
	uiUndefs = 0;
	uiDefines = 0;
	asUses.Init();
	iCaretUse = -1;
	bPossible = false;
	szWhyNot.Init();
	szConverted.Init();
	aszConflicts.Init();
	asRun.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEMacro::Kill(void)
{
	size	i;

	for (i = 0; i < asRun.NumElements(); i++)
	{
		asRun.Get(i)->szComment.Kill();
		asRun.Get(i)->szValue.Kill();
		asRun.Get(i)->szName.Kill();
	}
	asRun.Kill();
	aszConflicts.Kill();
	szConverted.Kill();
	szWhyNot.Kill();
	for (i = 0; i < asUses.NumElements(); i++)
	{
		asUses.Get(i)->Kill();
	}
	asUses.Kill();
	szFileName.Kill();
	szComment.Kill();
	szBody.Kill();
	aszParameters.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszDefineName.Init();
	mszDefineFile.Init();
	miDefineLine = 0;
	miDefineColumn = 0;
	mbDefineFound = false;
	muiDefineIndexings = MAX_SIZE;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	U_String(mszDefineName);
	U_String(mszDefineFile);
	U_Int32(miDefineLine);
	U_Int32(miDefineColumn);
	U_Bool(mbDefineFound);
	U_Size(muiDefineIndexings);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::Free(void)
{
	mszDefineFile.Kill();
	mszDefineName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::Save(CObjectWriter* pcFile) { return false; }
bool CIDEConvertMacro::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ has no macros, and so no key for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.ConvertMacro", "Convert Macro...", "", Ptr<CImageCel>(), this, &CIDEConvertMacro::QueueConvert)->SetEnabledCheck(this, &CIDEConvertMacro::CanConvert);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ConvertMacro"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: Convert Macro is for a macro's name.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bMacro)
	{
		psContext->Offer("Refactor.ConvertMacro");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The caret is on a name in a C or C++ file: whether it is a macro's is
// found when it is asked for (every file is searched for its #define).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::CanConvert(void)
{
	EIDEFileType	eType;
	CChars			szWord;
	size			uiStart;
	bool			bWord;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	eType = CIDEIcons::GetFileType(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	if ((eType != IFT_CPP) && (eType != IFT_C) && (eType != IFT_Header))
	{
		return false;
	}
	szWord.Init();
	bWord = FindWordAtCaret(&szWord, &uiStart);
	szWord.Kill();
	return bWord;
}


void CIDEConvertMacro::QueueConvert(void) { mpcContent->GetWindow()->Post(this, &CIDEConvertMacro::ShowQueuedConvert); }
void CIDEConvertMacro::ShowQueuedConvert(void) { ShowConvert(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::Refuse(SIDEMacro* psMacro, const char* szWhyNot, SIDEMacroUse* psUse)
{
	psMacro->bPossible = false;
	psMacro->szWhyNot.Set(szWhyNot);
	if (psUse)
	{
		psMacro->szWhyNot.Append(" (")->Append(GetIDEFileNamePart(psUse->szFileName.Text()))->Append(" line ")->Append((int32)(psUse->iLine + 1))->Append(")");
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The name the active editor's caret is on, or just after, and where it
// starts in the editor's text.  False if it is on none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::FindWordAtCaret(CChars* pszWord, size* puiStart)
{
	Ptr<CTextArea>	pEditor;
	const char*		szText;
	size			uiStart;
	size			uiEnd;

	pEditor = mpcContent->GetDocuments()->GetActiveTab()->GetEditor();
	szText = pEditor->GetText();
	if (!pEditor->GetSelection(&uiStart, &uiEnd))
	{
		uiStart = pEditor->GetCaretPosition();
	}
	if (!IsIDENameChar(szText[uiStart]) && (uiStart > 0) && IsIDENameChar(szText[uiStart - 1]))
	{
		uiStart--;
	}
	if (!IsIDENameChar(szText[uiStart]))
	{
		return false;
	}
	for (; (uiStart > 0) && IsIDENameChar(szText[uiStart - 1]); uiStart--)
	{
	}
	for (uiEnd = uiStart; IsIDENameChar(szText[uiEnd]); uiEnd++)
	{
	}
	pszWord->Clear();
	pszWord->AppendSubString(szText, uiStart, uiEnd);
	*puiStart = uiStart;
	return IsIDEIdentifier(pszWord->Text());
}


//////////////////////////////////////////////////////////////////////////
//
// Past a string or character literal that starts at uiAt.
//
//////////////////////////////////////////////////////////////////////////
static size SkipIDELiteral(const char* szText, size uiAt)
{
	char	cQuote;

	cQuote = szText[uiAt];
	for (uiAt++; (szText[uiAt] != '\0') && (szText[uiAt] != cQuote) && (szText[uiAt] != '\n'); uiAt++)
	{
		if ((szText[uiAt] == '\\') && (szText[uiAt + 1] != '\0'))
		{
			uiAt++;
		}
	}
	return (szText[uiAt] == cQuote) ? uiAt + 1 : uiAt;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the line a place is on starts: the line the preprocessor reads,
// which a \ at a line's end carries on to the next.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDELogicalLine(const char* szText, size uiAt)
{
	size	uiLine;
	size	uiBefore;

	for (uiLine = uiAt; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
	{
	}
	while (uiLine >= 2)
	{
		uiBefore = uiLine - 1;
		if ((uiBefore > 0) && (szText[uiBefore - 1] == '\r'))
		{
			uiBefore--;
		}
		if ((uiBefore == 0) || (szText[uiBefore - 1] != '\\'))
		{
			break;
		}
		for (uiLine = uiBefore - 1; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
		{
		}
	}
	return uiLine;
}


//////////////////////////////////////////////////////////////////////////
//
// The directive a line starts with: its name's length and where it is,
// and where what follows it starts.  0 if the line isn't a directive's.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDEMacroDirective(const char* szText, size uiLine, size* puiName, size* puiAfter)
{
	size	p;
	size	uiLength;

	for (p = uiLine; (szText[p] == ' ') || (szText[p] == '\t'); p++)
	{
	}
	if (szText[p] != '#')
	{
		return 0;
	}
	for (p++; (szText[p] == ' ') || (szText[p] == '\t'); p++)
	{
	}
	*puiName = p;
	for (uiLength = 0; (szText[p + uiLength] >= 'a') && (szText[p + uiLength] <= 'z'); uiLength++)
	{
	}
	for (p += uiLength; (szText[p] == ' ') || (szText[p] == '\t'); p++)
	{
	}
	*puiAfter = p;
	return uiLength;
}


//////////////////////////////////////////////////////////////////////////
//
// A piece of a text added to pszDest without the white space at its
// ends.
//
//////////////////////////////////////////////////////////////////////////
static void AppendIDETrimmed(CChars* pszDest, const char* szText, size uiStart, size uiEnd)
{
	for (; (uiStart < uiEnd) && ((szText[uiStart] == ' ') || (szText[uiStart] == '\t') || (szText[uiStart] == '\r') || (szText[uiStart] == '\n')); uiStart++)
	{
	}
	for (; (uiEnd > uiStart) && ((szText[uiEnd - 1] == ' ') || (szText[uiEnd - 1] == '\t') || (szText[uiEnd - 1] == '\r') || (szText[uiEnd - 1] == '\n')); uiEnd--)
	{
	}
	pszDest->AppendSubString(szText, uiStart, uiEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// The line and column (from 0) of a place in a text.
//
//////////////////////////////////////////////////////////////////////////
static void FindIDELineColumn(const char* szText, size uiAt, int* piLine, int* piColumn)
{
	size	i;

	*piLine = 0;
	*piColumn = 0;
	for (i = 0; i < uiAt; i++)
	{
		if (szText[i] == '\n')
		{
			(*piLine)++;
			*piColumn = 0;
		}
		else
		{
			(*piColumn)++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A #define read from its file's text: its parameters, what it is
// replaced by (the lines a \ joins made one, without its comment), the
// // comment after it, and where it ends.  uiLine is where its line
// starts and uiName where its name is.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::ReadDefine(const char* szText, size uiLine, size uiName, SIDEMacro* psDest)
{
	CChars	szBody;
	size	p;
	size	uiStart;
	size	uiNext;

	psDest->uiStart = uiLine;
	for (p = uiName; IsIDENameChar(szText[p]); p++)
	{
	}
	psDest->szName.Clear();
	psDest->szName.AppendSubString(szText, uiName, p);
	psDest->bFunction = (szText[p] == '(');
	psDest->bVariadic = false;
	psDest->aszParameters.ReInit();
	if (psDest->bFunction)
	{
		p++;
		while ((szText[p] != '\0') && (szText[p] != ')') && (szText[p] != '\n'))
		{
			for (uiStart = p; (szText[p] != '\0') && (szText[p] != ')') && (szText[p] != ',') && (szText[p] != '\n'); p++)
			{
			}
			szBody.Init();
			AppendIDETrimmed(&szBody, szText, uiStart, p);
			if (szBody.Equals("..."))
			{
				psDest->bVariadic = true;
				psDest->aszParameters.Add("__VA_ARGS__");
			}
			else if (!szBody.Empty())
			{
				psDest->aszParameters.Add(szBody.Text());
			}
			szBody.Kill();
			if (szText[p] == ',')
			{
				p++;
			}
		}
		if (szText[p] == ')')
		{
			p++;
		}
	}

	szBody.Init();
	psDest->szComment.Clear();
	while ((szText[p] != '\0') && (szText[p] != '\n'))
	{
		if ((szText[p] == '"') || (szText[p] == '\''))
		{
			uiNext = SkipIDELiteral(szText, p);
			szBody.AppendSubString(szText, p, uiNext);
			p = uiNext;
		}
		else if ((szText[p] == '/') && (szText[p + 1] == '/'))
		{
			for (uiNext = p; (szText[uiNext] != '\0') && (szText[uiNext] != '\n') && (szText[uiNext] != '\r'); uiNext++)
			{
			}
			psDest->szComment.AppendSubString(szText, p, uiNext);
			p = uiNext;
			if (szText[p] == '\r')
			{
				p++;
			}
		}
		else if ((szText[p] == '\\') && ((szText[p + 1] == '\n') || ((szText[p + 1] == '\r') && (szText[p + 2] == '\n'))))
		{
			//The next line is this one's: one space for the line's end and what it starts with.
			p += (szText[p + 1] == '\n') ? 2 : 3;
			for (; (szText[p] == ' ') || (szText[p] == '\t'); p++)
			{
			}
			szBody.StripWhitespace();
			szBody.Append(' ');
		}
		else
		{
			if (szText[p] != '\r')
			{
				szBody.Append(szText[p]);
			}
			p++;
		}
	}
	psDest->szBody.Clear();
	AppendIDETrimmed(&psDest->szBody, szBody.Text(), 0, szBody.Length());
	szBody.Kill();
	psDest->uiEnd = (szText[p] == '\n') ? p + 1 : p;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place the solution's C and C++ files write szName in their code
// (not in a comment or a string): its #define read into psDest (the
// first, if there are more: *puiDefines says how many), its #undefs
// counted, and every other place a use, with a function-like macro's
// arguments as they are written.  The use the active editor's caret is
// on is psDest's iCaretUse.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::FindUses(const char* szName, SIDEMacro* psDest, size* puiDefines)
{
	CIDECodeModel*				pcModel;
	CIDEFileTexts*				pcTexts;
	CArrayIDENameOccurrences	asFound;
	SIDENameOccurrence*			psFound;
	SIDELineCounter				sLines;
	SIDEMacroUse*				psUse;
	CTextFile					cFile;
	CChars						szArgument;
	EIDEFileType				eType;
	const char*					szFileName;
	const char*					szText;
	size						uiLength;
	size						uiNameLength;
	size						uiLine;
	size						uiDirective;
	size						uiName;
	size						uiAfter;
	size						uiCaret;
	size						uiStart;
	size						p;
	size						i;
	size						j;
	int							iDepth;
	bool						bCaret;

	pcModel = mpcContent->GetCodeModel();
	pcTexts = &mpcContent->GetFindInFiles();
	uiNameLength = strlen(szName);
	*puiDefines = 0;
	szArgument.Init();
	bCaret = FindWordAtCaret(&szArgument, &uiCaret);
	szArgument.Kill();
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFileName = pcModel->GetProjectFile(i);
		eType = CIDEIcons::GetFileType(szFileName);
		if ((eType != IFT_CPP) && (eType != IFT_C) && (eType != IFT_Header))
		{
			continue;
		}
		cFile.Init();
		if (!pcTexts->GetFileText(szFileName, &cFile, &szText, &uiLength) || !strstr(szText, szName))
		{
			cFile.Kill();
			continue;
		}
		asFound.Init();
		sLines.Init(szText);
		FindIDECppOccurrences(szText, uiLength, szName, true, false, &sLines, &asFound);
		for (j = 0; j < asFound.NumElements(); j++)
		{
			psFound = asFound.Get(j);
			uiLine = FindIDELogicalLine(szText, psFound->uiStart);
			uiDirective = FindIDEMacroDirective(szText, uiLine, &uiName, &uiAfter);
			if ((uiDirective == 6) && (strncmp(&szText[uiName], "define", 6) == 0) && (uiAfter == psFound->uiStart))
			{
				if (*puiDefines == 0)
				{
					ReadDefine(szText, uiLine, psFound->uiStart, psDest);
					psDest->szFileName.Set(szFileName);
					psDest->iLine = psFound->iLine;
				}
				(*puiDefines)++;
				continue;
			}
			if ((uiDirective == 5) && (strncmp(&szText[uiName], "undef", 5) == 0))
			{
				psDest->uiUndefs++;
				continue;
			}
			psUse = psDest->asUses.Add();
			psUse->Init();
			psUse->szFileName.Set(szFileName);
			psUse->uiStart = psFound->uiStart;
			psUse->uiEnd = psFound->uiStart + uiNameLength;
			psUse->iLine = psFound->iLine;
			psUse->iColumn = psFound->iColumn;
			if ((uiDirective == 6) && (strncmp(&szText[uiName], "define", 6) == 0))
			{
				psUse->iKind = IDE_MACRO_USE_DEFINE;
			}
			else if (uiDirective != 0)
			{
				psUse->iKind = IDE_MACRO_USE_DIRECTIVE;
			}
			if (bCaret && (psFound->uiStart == uiCaret) && (StringInsensitiveCompare(szFileName, mpcContent->GetDocuments()->GetActiveTab()->GetFileName()) == 0))
			{
				psDest->iCaretUse = (int)(psDest->asUses.NumElements() - 1);
			}
		}
		asFound.Kill();
		cFile.Kill();
	}

	//The arguments of each use, read from its file's text once it is known that it has any: its #define may come
	//after its uses.
	for (i = 0; psDest->bFunction && (*puiDefines > 0) && (i < psDest->asUses.NumElements()); i++)
	{
		psUse = psDest->asUses.Get(i);
		if (psUse->iKind == IDE_MACRO_USE_DIRECTIVE)
		{
			continue;
		}
		cFile.Init();
		if (pcTexts->GetFileText(psUse->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			for (p = psUse->uiEnd; (szText[p] == ' ') || (szText[p] == '\t') || (szText[p] == '\r') || (szText[p] == '\n') || ((szText[p] == '\\') && ((szText[p + 1] == '\n') || (szText[p + 1] == '\r'))); p++)
			{
			}
			if (szText[p] != '(')
			{
				psUse->iKind = IDE_MACRO_USE_NAME;
			}
			else
			{
				iDepth = 1;
				uiStart = p + 1;
				for (p++; (szText[p] != '\0') && (iDepth > 0); p++)
				{
					if ((szText[p] == '"') || (szText[p] == '\''))
					{
						p = SkipIDELiteral(szText, p) - 1;
					}
					else if (szText[p] == '(')
					{
						iDepth++;
					}
					else if ((szText[p] == ')') || ((szText[p] == ',') && (iDepth == 1) && !(psDest->bVariadic && (psUse->aszArguments.NumElements() + 1 >= psDest->aszParameters.NumElements()))))
					{
						iDepth -= (szText[p] == ')') ? 1 : 0;
						if ((iDepth == 0) || (szText[p] == ','))
						{
							szArgument.Init();
							AppendIDETrimmed(&szArgument, szText, uiStart, p);
							if ((szText[p] == ',') || !szArgument.Empty() || (psUse->aszArguments.NumElements() > 0))
							{
								psUse->aszArguments.Add(szArgument.Text());
								for (; (uiStart < p) && ((szText[uiStart] == ' ') || (szText[uiStart] == '\t') || (szText[uiStart] == '\r') || (szText[uiStart] == '\n')); uiStart++)
								{
								}
								psUse->aiArgumentStarts.Add((int)uiStart);
								psUse->aiArgumentEnds.Add((int)(uiStart + szArgument.Length()));
							}
							szArgument.Kill();
							uiStart = p + 1;
						}
					}
				}
				psUse->uiEnd = p;
			}
		}
		cFile.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where szName is #defined in the solution's C and C++ files: its first
// #define, at its name.  The index has no macros, so it is read from the
// files' text, without parsing it: each place the name is written is a
// #define's if "# define" is all that is before it on its line.  That is
// quick enough to ask as the pointer moves over a name (the declaration
// shown with Control held), and what was asked last is kept while the
// index is as it was, as the pointer asks for the same name again and
// again.  psDest's file name is Init'ed here if it is found.  False if it
// isn't #defined.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::FindDefine(const char* szName, SIDELocation* psDest)
{
	CIDECodeModel*	pcModel;
	CIDEFileTexts*	pcTexts;
	CTextFile		cFile;
	EIDEFileType	eType;
	const char*		szFileName;
	const char*		szText;
	const char*		szFound;
	size			uiLength;
	size			uiNameLength;
	size			uiAt;
	size			p;
	size			i;

	pcModel = mpcContent->GetCodeModel();
	uiNameLength = strlen(szName);
	if ((uiNameLength == 0) || !IsIDEIdentifier(szName))
	{
		return false;
	}
	if (!mszDefineName.Equals(szName) || (muiDefineIndexings != pcModel->NumIndexings()))
	{
		mszDefineName.Set(szName);
		muiDefineIndexings = pcModel->NumIndexings();
		mbDefineFound = false;
		pcTexts = &mpcContent->GetFindInFiles();
		for (i = 0; !mbDefineFound && (i < pcModel->NumProjectFiles()); i++)
		{
			szFileName = pcModel->GetProjectFile(i);
			eType = CIDEIcons::GetFileType(szFileName);
			if ((eType != IFT_CPP) && (eType != IFT_C) && (eType != IFT_Header))
			{
				continue;
			}
			cFile.Init();
			if (pcTexts->GetFileText(szFileName, &cFile, &szText, &uiLength))
			{
				for (szFound = strstr(szText, szName); szFound && !mbDefineFound; szFound = strstr(szFound + uiNameLength, szName))
				{
					uiAt = (size)(szFound - szText);
					if (IsIDENameChar(szText[uiAt + uiNameLength]) || (uiAt < 8))
					{
						continue;
					}

					//Back over the gap to "define", over the gap to '#', and over the gap to the line's start.
					for (p = uiAt; (p > 0) && ((szText[p - 1] == ' ') || (szText[p - 1] == '\t')); p--)
					{
					}
					if ((p == uiAt) || (p < 6) || (strncmp(&szText[p - 6], "define", 6) != 0))
					{
						continue;
					}
					for (p -= 6; (p > 0) && ((szText[p - 1] == ' ') || (szText[p - 1] == '\t')); p--)
					{
					}
					if ((p == 0) || (szText[p - 1] != '#'))
					{
						continue;
					}
					for (p--; (p > 0) && ((szText[p - 1] == ' ') || (szText[p - 1] == '\t')); p--)
					{
					}
					if ((p > 0) && (szText[p - 1] != '\n'))
					{
						continue;
					}

					mbDefineFound = true;
					mszDefineFile.Set(szFileName);
					miDefineLine = 0;
					miDefineColumn = 0;
					for (p = 0; p < uiAt; p++)
					{
						if (szText[p] == '\n')
						{
							miDefineLine++;
							miDefineColumn = 0;
						}
						else
						{
							miDefineColumn++;
						}
					}
				}
			}
			cFile.Kill();
		}
	}

	if (!mbDefineFound)
	{
		return false;
	}
	memset(psDest, 0, sizeof(SIDELocation));
	psDest->szFileName.Init(mszDefineFile);
	psDest->iLine = miDefineLine;
	psDest->iColumn = miDefineColumn;
	psDest->iLength = (int32)uiNameLength;
	psDest->uiRole = CPPIR_Definition;
	psDest->iContainer = -1;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The #define of the name the active editor's caret is on, or just after,
// with the name.  False if there is no editor, no name there, or it isn't
// #defined.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::FindDefineAtCaret(CChars* pszName, SIDELocation* psDest)
{
	size	uiStart;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull() || !FindWordAtCaret(pszName, &uiStart))
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	return FindDefine(pszName->Text(), psDest);
}


//////////////////////////////////////////////////////////////////////////
//
// What a #define found by FindDefine is, in words: its first line as it
// is written ("#define IDE_RECENT_FILES 50") and where it is
// ("IDERecent.h:36").  False if its file can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::ReadDefineInfo(SIDELocation* psDefine, CChars* pszDeclaration, CChars* pszWhere)
{
	CTextFile	cFile;
	const char*	szText;
	size		uiLength;
	size		uiName;
	size		uiNameInDest;
	bool		bResult;

	cFile.Init();
	bResult = false;
	if (mpcContent->GetFindInFiles()->GetFileText(psDefine->szFileName.Text(), &cFile, &szText, &uiLength))
	{
		uiName = FindIDETextPosition(szText, uiLength, psDefine->iLine, psDefine->iColumn);
		bResult = ReadIDEDeclaration(szText, uiLength, uiName, (size)psDefine->iLength, 0, pszDeclaration, &uiNameInDest, NULL);
	}
	cFile.Kill();
	if (bResult)
	{
		AppendIDEPlaceName(psDefine, pszWhere);
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The macro the active editor's caret is on (its name, at its #define or
// anywhere it is written): its #define and its uses.  False if the name
// there isn't #defined in the solution's files.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::FindMacro(SIDEMacro* psDest)
{
	CChars	szWord;
	size	uiStart;
	bool	bFound;

	if (!CanConvert())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szWord.Init();
	bFound = FindWordAtCaret(&szWord, &uiStart);
	if (bFound)
	{
		FindUses(szWord.Text(), psDest, &psDest->uiDefines);
		bFound = (psDest->uiDefines > 0);
	}
	szWord.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the macro could be something else: nothing names it that only
// a macro can be named by.  False, with why, if not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::IsUsedAsMacroOnly(SIDEMacro* psMacro)
{
	SIDEMacroUse*	psUse;
	size			i;

	if (psMacro->uiDefines > 1)
	{
		Refuse(psMacro, "it is #defined more than once");
		return true;
	}
	if (psMacro->uiUndefs > 0)
	{
		Refuse(psMacro, "an #undef names it");
		return true;
	}
	if (CIDEIcons::GetFileType(psMacro->szFileName.Text()) == IFT_C)
	{
		Refuse(psMacro, "it is C's: there it must stay a macro");
		return true;
	}
	for (i = 0; i < psMacro->asUses.NumElements(); i++)
	{
		psUse = psMacro->asUses.Get(i);
		if (psUse->iKind == IDE_MACRO_USE_DIRECTIVE)
		{
			Refuse(psMacro, "an #if names it", psUse);
			return true;
		}
		if (CIDEIcons::GetFileType(psUse->szFileName.Text()) == IFT_C)
		{
			Refuse(psMacro, "C code uses it", psUse);
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The type of an expression as a declaration writes it, from the kept
// check: without a reference, and without const where it isn't a
// pointer; an array's is a pointer to its elements.  A parameter that is
// an object is a const reference to it.  False if it has none that can be
// written.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::AppendValueType(CIDERefactorUnit* pcUnit, CCPPSyntax* pcExpression, bool bParameter, CChars* pszDest)
{
	SCPP65Type*	psType;
	CChars		szType;
	bool		bWritten;

	psType = pcUnit->GetPass()->GetType(pcExpression);
	if (!psType)
	{
		return false;
	}
	if (psType->IsReference())
	{
		psType = psType->psBase;
	}
	if (psType->IsVoid() || psType->IsFunction())
	{
		return false;
	}
	szType.Init();
	if (psType->IsArray())
	{
		bWritten = pcUnit->GetPass()->AppendType(psType->psBase, &szType);
		szType.Append('*');
	}
	else
	{
		bWritten = pcUnit->GetPass()->AppendType((psType->IsPointer() || psType->IsMemberPointer()) ? psType : psType->Unqualified(), &szType);
		if (bWritten && bParameter && psType->IsClass())
		{
			szType.Insert(0, "const ");
			szType.Append('&');
		}
	}
	if (bWritten)
	{
		pszDest->Append(szType);
	}
	szType.Kill();
	return bWritten;
}


//////////////////////////////////////////////////////////////////////////
//
// The errors of the checks of the macro's file and the files that use it
// (the first IDE_MACRO_CHECKED_FILES of them), with szNewText as the
// macro's file's text (as it is, if NULL).  A file that can't be parsed
// counts nothing.
//
//////////////////////////////////////////////////////////////////////////
size CIDEConvertMacro::CountErrors(SIDEMacro* psMacro, char* szNewText)
{
	CIDECodeModel*	pcModel;
	CIDEFileTexts*	pcTexts;
	CArrayChars		aszFiles;
	CTextFile		cFile;
	const char*		szText;
	size			uiLength;
	size			uiErrors;
	size			uiFile;
	size			i;
	bool			bHeader;

	pcModel = mpcContent->GetCodeModel();
	pcTexts = &mpcContent->GetFindInFiles();
	aszFiles.Init();
	aszFiles.Add(psMacro->szFileName.Text());
	for (i = 0; (i < psMacro->asUses.NumElements()) && (aszFiles.NumElements() < IDE_MACRO_CHECKED_FILES); i++)
	{
		if (aszFiles.GetIndex(psMacro->asUses.Get(i)->szFileName.Text()) == ARRAY_ELEMENT_NOT_FOUND)
		{
			aszFiles.Add(psMacro->asUses.Get(i)->szFileName.Text());
		}
	}

	//A header's text is the code model's for every unit: the files that use the macro are checked with it.
	bHeader = szNewText && (CIDEIcons::GetFileType(psMacro->szFileName.Text()) == IFT_Header);
	if (bHeader)
	{
		pcModel->WaitForIndexing();
		pcModel->SetHeaderText(psMacro->szFileName.Text(), szNewText);
	}
	uiErrors = 0;
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		uiFile = CountIDETrialErrors(pcModel, pcTexts, aszFiles.GetText(i), (i == 0) ? szNewText : NULL, NULL, NULL);
		uiErrors += (uiFile == MAX_SIZE) ? 0 : uiFile;
		if (bHeader && (i > 0))
		{
			//The check of a header gives the code model its own text: the macro's file has the new one again.
			pcModel->SetHeaderText(psMacro->szFileName.Text(), szNewText);
		}
	}
	if (bHeader)
	{
		cFile.Init();
		if (pcTexts->GetFileText(psMacro->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			pcModel->SetHeaderText(psMacro->szFileName.Text(), szText);
		}
		cFile.Kill();
	}
	aszFiles.Kill();
	return uiErrors;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the macro's file and the files that use it compile as well
// with szConverted where the file has uiStart to uiEnd as they do now:
// their checks have no more errors than they have.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::CheckConverted(SIDEMacro* psMacro, size uiStart, size uiEnd, const char* szConverted)
{
	CTextFile	cFile;
	CChars		szNew;
	const char*	szText;
	size		uiLength;
	size		uiBefore;
	bool		bResult;

	cFile.Init();
	bResult = false;
	if (mpcContent->GetFindInFiles()->GetFileText(psMacro->szFileName.Text(), &cFile, &szText, &uiLength))
	{
		szNew.Init();
		szNew.AppendSubString(szText, 0, uiStart);
		szNew.Append(szConverted)->Append('\n');
		szNew.Append(&szText[uiEnd]);
		uiBefore = CountErrors(psMacro, NULL);
		bResult = CountErrors(psMacro, szNew.Text()) <= uiBefore;
		szNew.Kill();
	}
	cFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What a #define of a value becomes: a constant of the type the compiler
// gives the value, which is asked by checking the file with the value
// written as an expression where the #define is.  A string becomes a
// const char array (sizeof stays what it was), unless a use joins it to
// another string.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::FindConstantConversion(SIDEMacro* psMacro)
{
	CIDECodeModel*		pcModel;
	CIDEFileTexts*		pcTexts;
	CIDETrialTexts		cTrial;
	CIDERefactorUnit	cUnit;
	CCPPSyntax*			pcNode;
	SIDEMacroUse*		psUse;
	CTextFile			cFile;
	CChars				szNew;
	CChars				szType;
	const char*			szText;
	size				uiLength;
	size				uiBefore;
	size				uiAfter;
	size				i;
	int32				iFirst;
	int32				iLast;
	int					iColumn;
	bool				bString;

	pcModel = mpcContent->GetCodeModel();
	pcTexts = &mpcContent->GetFindInFiles();
	if (psMacro->szBody.Empty())
	{
		Refuse(psMacro, "it has no value");
		return;
	}
	bString = psMacro->szBody.StartsWith("\"") && psMacro->szBody.EndsWith("\"");
	for (i = 0; bString && (i < psMacro->asUses.NumElements()); i++)
	{
		psUse = psMacro->asUses.Get(i);
		cFile.Init();
		if (pcTexts->GetFileText(psUse->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			for (uiBefore = psUse->uiStart; (uiBefore > 0) && ((szText[uiBefore - 1] == ' ') || (szText[uiBefore - 1] == '\t') || (szText[uiBefore - 1] == '\r') || (szText[uiBefore - 1] == '\n')); uiBefore--)
			{
			}
			for (uiAfter = psUse->uiEnd; (szText[uiAfter] == ' ') || (szText[uiAfter] == '\t') || (szText[uiAfter] == '\r') || (szText[uiAfter] == '\n'); uiAfter++)
			{
			}
			if (((uiBefore > 0) && (szText[uiBefore - 1] == '"')) || (szText[uiAfter] == '"'))
			{
				Refuse(psMacro, "a use joins it to another string", psUse);
				cFile.Kill();
				return;
			}
		}
		cFile.Kill();
	}

	szType.Init();
	if (bString)
	{
		psMacro->szConverted.Set("const char ");
		psMacro->szConverted.Append(psMacro->szName)->Append("[] = ")->Append(psMacro->szBody)->Append(';');
	}
	else
	{
		cFile.Init();
		if (pcTexts->GetFileText(psMacro->szFileName.Text(), &cFile, &szText, &uiLength))
		{
			szNew.Init();
			szNew.AppendSubString(szText, 0, psMacro->uiStart);
			szNew.Append(IDE_MACRO_PROBE)->Append(psMacro->szBody)->Append("); }\n");
			szNew.Append(&szText[psMacro->uiEnd]);
			cTrial.mpcTexts = pcTexts;
			cTrial.mszFileName = psMacro->szFileName.Text();
			cTrial.mszText = szNew.Text();
			cTrial.mszUnit = NULL;
			cTrial.mszUnitText = NULL;
			cUnit.Init();
			iColumn = (int)strlen(IDE_MACRO_PROBE) - 1;
			if (cUnit.Parse(pcModel, &cTrial, psMacro->szFileName.Text(), true) &&
				cUnit.FindTokensIn(psMacro->iLine, iColumn, psMacro->iLine, iColumn + (int)psMacro->szBody.Length() + 2, &iFirst, &iLast))
			{
				pcNode = cUnit.FindNodeOfTokens(iFirst, iLast);
				if (pcNode && cUnit.IsExpression(pcNode))
				{
					AppendValueType(&cUnit, pcNode, false, &szType);
				}
			}
			cUnit.Kill();
			szNew.Kill();
			if (CIDEIcons::GetFileType(psMacro->szFileName.Text()) == IFT_Header)
			{
				pcModel->SetHeaderText(psMacro->szFileName.Text(), szText);
			}
		}
		cFile.Kill();
		if (szType.Empty())
		{
			Refuse(psMacro, "its value isn't one the compiler gives a type where it is #defined");
			szType.Kill();
			return;
		}
		if (szType.EndsWith("*"))
		{
			psMacro->szConverted.Set(szType);
			psMacro->szConverted.Append(" const ");
		}
		else
		{
			psMacro->szConverted.Set("const ");
			psMacro->szConverted.Append(szType)->Append(' ');
		}
		psMacro->szConverted.Append(psMacro->szName)->Append(" = ")->Append(psMacro->szBody)->Append(';');
	}
	szType.Kill();
	if (!psMacro->szComment.Empty())
	{
		psMacro->szConverted.Append("  ")->Append(psMacro->szComment);
	}
	psMacro->bPossible = true;
	if (!CheckConverted(psMacro, psMacro->uiStart, psMacro->uiEnd, psMacro->szConverted.Text()))
	{
		Refuse(psMacro, "the code that uses it doesn't compile with it a constant");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The next token of a macro's body from *puiAt, for what is done to its
// parameters there: a name, a literal, or one or more punctuation
// characters that are one operator.  Its text in pszToken; false at the
// body's end.
//
//////////////////////////////////////////////////////////////////////////
static bool NextIDEBodyToken(const char* szBody, size* puiAt, CChars* pszToken)
{
	static const char*	aszOperators[] = { "<<=", ">>=", "...", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "##", "::" };
	size				p;
	size				uiEnd;
	size				i;

	for (p = *puiAt; (szBody[p] == ' ') || (szBody[p] == '\t'); p++)
	{
	}
	pszToken->Clear();
	if (szBody[p] == '\0')
	{
		*puiAt = p;
		return false;
	}
	uiEnd = p + 1;
	if ((szBody[p] == '"') || (szBody[p] == '\''))
	{
		uiEnd = SkipIDELiteral(szBody, p);
	}
	else if (IsIDENameChar(szBody[p]))
	{
		for (; IsIDENameChar(szBody[uiEnd]) || (szBody[uiEnd] == '.'); uiEnd++)
		{
		}
		if (!((szBody[p] >= '0') && (szBody[p] <= '9')))
		{
			for (uiEnd = p + 1; IsIDENameChar(szBody[uiEnd]); uiEnd++)
			{
			}
		}
	}
	else
	{
		for (i = 0; i < sizeof(aszOperators) / sizeof(aszOperators[0]); i++)
		{
			if (strncmp(&szBody[p], aszOperators[i], strlen(aszOperators[i])) == 0)
			{
				uiEnd = p + strlen(aszOperators[i]);
				break;
			}
		}
	}
	pszToken->AppendSubString(szBody, p, uiEnd);
	*puiAt = uiEnd;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What a macro with parameters becomes: an inline function of its name.
// Each file that uses it is checked, and at each use the compiler's type
// of each argument (where the body first has it) is its parameter's, and
// the type of what the use made is its result's; a use that made a
// statement makes it void.  Types that differ between uses become
// template parameters, shared by the parameters whose types change
// together.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::FindFunctionConversion(SIDEMacro* psMacro)
{
	CIDECodeModel*		pcModel;
	CIDEFileTexts*		pcTexts;
	CIDERefactorUnit	cUnit;
	CIDEEffectFinder	cEffects;
	CCPPSyntax*			pcNode;
	SIDEMacroUse*		psUse;
	CTextFile			cFile;
	CArrayChars			aszVectors;		//Each parameter's types at the uses, one after another; the result's last.
	CArrayChars			aszTypes;		//Each one's type, if it has one type at every use.
	CArrayInt			aiCounts;		//How often the body has each parameter.
	CArrayChars			aszChecked;
	CArrayChars			aszTemplates;	//The vectors that are template parameters, in their order.
	CChars				szToken;
	CChars				szBefore;
	CChars				szBeforeThat;
	CChars				szType;
	CChars				szConflict;
	CChars				szBody;
	const char*			szText;
	size				uiLength;
	size				uiParameters;
	size				uiUses;
	size				uiAt;
	size				i;
	size				j;
	size				k;
	int32				iFirst;
	int32				iLast;
	int32				iArgumentFirst;
	int32				iArgumentLast;
	int					iLine;
	int					iColumn;
	int					iEndLine;
	int					iEndColumn;
	bool				bStatement;
	bool				bParameter;

	pcModel = mpcContent->GetCodeModel();
	pcTexts = &mpcContent->GetFindInFiles();
	uiParameters = psMacro->aszParameters.NumElements();
	if (psMacro->bVariadic)
	{
		Refuse(psMacro, "it takes any number of arguments (...)");
		return;
	}
	if (psMacro->szBody.Empty())
	{
		Refuse(psMacro, "it is replaced by nothing");
		return;
	}

	//Its body: how often it has each parameter, and nothing a function can't do with one.
	aiCounts.Init();
	for (i = 0; i < uiParameters; i++)
	{
		aiCounts.Add(0);
	}
	szToken.Init();
	szBefore.Init();
	szBeforeThat.Init();
	uiAt = 0;
	psMacro->bPossible = true;
	while (psMacro->bPossible && NextIDEBodyToken(psMacro->szBody.Text(), &uiAt, &szToken))
	{
		if (szToken.Equals("#") || szToken.Equals("##"))
		{
			Refuse(psMacro, "its body has # or ##: only a macro can make a string or a name of its arguments");
		}
		for (i = 0; i < uiParameters; i++)
		{
			if (szToken.Equals(psMacro->aszParameters.GetText(i)))
			{
				aiCounts.SetValue(i, aiCounts.GetValue(i) + 1);
				if (szBefore.Equals("++") || szBefore.Equals("--") || (szBefore.Equals("(") && (szBeforeThat.Equals("++") || szBeforeThat.Equals("--"))))
				{
					Refuse(psMacro, "it changes its argument: a function would change its own copy");
				}
			}
		}
		//An assignment to a parameter, or a ++ after one: "x = ", "(x) += ", "x++".
		if ((szToken.EndsWith("=") && !szToken.Equals("==") && !szToken.Equals("!=") && !szToken.Equals("<=") && !szToken.Equals(">=")) || szToken.Equals("++") || szToken.Equals("--"))
		{
			bParameter = false;
			for (i = 0; i < uiParameters; i++)
			{
				bParameter |= szBefore.Equals(psMacro->aszParameters.GetText(i)) || (szBefore.Equals(")") && szBeforeThat.Equals(psMacro->aszParameters.GetText(i)));
			}
			if (bParameter)
			{
				Refuse(psMacro, "it changes its argument: a function would change its own copy");
			}
		}
		szBeforeThat.Set(szBefore);
		szBefore.Set(szToken);
	}
	szBeforeThat.Kill();
	szBefore.Kill();
	szToken.Kill();
	if (!psMacro->bPossible)
	{
		aiCounts.Kill();
		return;
	}

	aszVectors.Init();
	for (i = 0; i <= uiParameters; i++)
	{
		aszVectors.Add("");
	}
	aszChecked.Init();
	uiUses = 0;
	bStatement = false;
	for (i = 0; psMacro->bPossible && (i < psMacro->asUses.NumElements()) && (aszChecked.NumElements() < IDE_MACRO_CHECKED_FILES); i++)
	{
		psUse = psMacro->asUses.Get(i);
		if (aszChecked.GetIndex(psUse->szFileName.Text()) != ARRAY_ELEMENT_NOT_FOUND)
		{
			continue;
		}
		aszChecked.Add(psUse->szFileName.Text());
		cFile.Init();
		cUnit.Init();
		if (pcTexts->GetFileText(psUse->szFileName.Text(), &cFile, &szText, &uiLength) && cUnit.Parse(pcModel, pcTexts, psUse->szFileName.Text(), true))
		{
			//Every use this file has.
			for (j = i; psMacro->bPossible && (j < psMacro->asUses.NumElements()); j++)
			{
				psUse = psMacro->asUses.Get(j);
				if (!psUse->szFileName.EqualsIgnoreCase(aszChecked.GetText(aszChecked.NumElements() - 1)) || (psUse->iKind != IDE_MACRO_USE_CODE))
				{
					continue;
				}
				if (psUse->aszArguments.NumElements() != uiParameters)
				{
					Refuse(psMacro, "a use gives it the wrong number of arguments", psUse);
					break;
				}
				FindIDELineColumn(szText, psUse->uiEnd, &iEndLine, &iEndColumn);
				if (!cUnit.FindExpansionTokens(psUse->iLine, psUse->iColumn, iEndLine, iEndColumn, &iFirst, &iLast))
				{
					//Code the configuration leaves out, or in a macro's arguments: nothing is known of it here.
					continue;
				}
				//All of one value, or of one statement (whose ; comes after the use).
				pcNode = cUnit.FindNodeOfTokens(iFirst, iLast);
				if (!pcNode || (pcNode->miFirstToken != iFirst) || ((pcNode->miLastToken != iLast) && !((pcNode->miLastToken == iLast + 1) && cUnit.IsToken(iLast + 1, ";") && !cUnit.IsExpression(pcNode))))
				{
					Refuse(psMacro, "what it makes isn't one value or one statement where it is used: its body needs brackets round it", psUse);
					break;
				}
				szType.Init();
				if (!cUnit.IsExpression(pcNode))
				{
					bStatement = true;
					szType.Append("void");
				}
				else if (!AppendValueType(&cUnit, pcNode, false, &szType))
				{
					szType.Append("void");
				}
				aszVectors.Get(uiParameters)->Append(szType)->Append('|');
				szType.Kill();

				for (k = 0; psMacro->bPossible && (k < uiParameters); k++)
				{
					FindIDELineColumn(szText, (size)psUse->aiArgumentStarts.GetValue(k), &iLine, &iColumn);
					FindIDELineColumn(szText, (size)psUse->aiArgumentEnds.GetValue(k), &iEndLine, &iEndColumn);
					szType.Init();
					if (cUnit.FindSpelledTokens(iLine, iColumn, iEndLine, iEndColumn, iFirst, iLast, (size)aiCounts.GetValue(k), &iArgumentFirst, &iArgumentLast))
					{
						//The innermost node of a name alone is the name: the expression it is, then.
						pcNode = cUnit.FindNodeOfTokens(iArgumentFirst, iArgumentLast);
						for (; pcNode && !cUnit.IsExpression(pcNode) && pcNode->mpcParent && (pcNode->mpcParent->miFirstToken == iArgumentFirst) && (pcNode->mpcParent->miLastToken == iArgumentLast); pcNode = pcNode->mpcParent)
						{
						}
						if (!pcNode || !cUnit.IsExpression(pcNode) || (pcNode->miFirstToken != iArgumentFirst) || (pcNode->miLastToken != iArgumentLast))
						{
							szConflict.Init("its argument ");
							szConflict.Append(psUse->aszArguments.GetText(k))->Append(" isn't one value in what it makes: the body needs brackets round ")->Append(psMacro->aszParameters.GetText(k));
							Refuse(psMacro, szConflict.Text(), psUse);
							szConflict.Kill();
						}
						else if (!AppendValueType(&cUnit, pcNode, true, &szType))
						{
							szConflict.Init("the type of its argument ");
							szConflict.Append(psUse->aszArguments.GetText(k))->Append(" can't be written");
							Refuse(psMacro, szConflict.Text(), psUse);
							szConflict.Kill();
						}
						else if (aiCounts.GetValue(k) > 1)
						{
							cEffects.mbEffects = false;
							cEffects.Look(pcNode);
							if (cEffects.mbEffects)
							{
								szConflict.Init();
								szConflict.Append(psUse->aszArguments.GetText(k))->Append(", in ")->Append(GetIDEFileNamePart(psUse->szFileName.Text()))->Append(" at line ")->Append((int32)(psUse->iLine + 1));
								szConflict.Append(", is worked out ")->Append((int32)aiCounts.GetValue(k))->Append(" times by the macro and would be once by a function.");
								psMacro->aszConflicts.Add(szConflict.Text());
								szConflict.Kill();
							}
						}
					}
					aszVectors.Get(k)->Append(szType)->Append('|');
					szType.Kill();
				}
				uiUses++;
			}
		}
		cUnit.Kill();
		cFile.Kill();
	}
	aszChecked.Kill();
	if (psMacro->bPossible && (uiUses == 0))
	{
		Refuse(psMacro, "nothing uses it: its arguments' types can't be told");
	}

	//Each one's type: the one it has at every use that says, or a template parameter.
	aszTypes.Init();
	aszTemplates.Init();
	for (i = 0; psMacro->bPossible && (i <= uiParameters); i++)
	{
		szType.Init();
		szToken.Init();
		bParameter = true;
		uiAt = 0;
		for (j = 0; j < aszVectors.Get(i)->Length(); j++)
		{
			if (aszVectors.Get(i)->Text()[j] != '|')
			{
				continue;
			}
			szToken.Clear();
			szToken.AppendSubString(aszVectors.GetText(i), uiAt, j);
			uiAt = j + 1;
			if (szToken.Empty())
			{
				continue;
			}
			if (szType.Empty())
			{
				szType.Set(szToken);
			}
			else if (!szType.Equals(szToken.Text()))
			{
				bParameter = false;
			}
		}
		szToken.Kill();
		if (!bParameter && strstr(aszVectors.GetText(i), "||"))
		{
			Refuse(psMacro, "its arguments' types differ between its uses, and at one of them a type can't be told");
		}
		else if (szType.Empty())
		{
			szConflict.Init("no use says what type its argument ");
			szConflict.Append(psMacro->aszParameters.GetText(i))->Append(" is");
			Refuse(psMacro, szConflict.Text());
			szConflict.Kill();
		}
		else if (bParameter)
		{
			aszTypes.Add(szType.Text());
		}
		else
		{
			//Its types differ: the template parameter of the parameters whose types are these at every use.
			k = aszTemplates.GetIndex(aszVectors.GetText(i));
			if ((k == ARRAY_ELEMENT_NOT_FOUND) && (i == uiParameters))
			{
				Refuse(psMacro, "its result's type differs between its uses and follows none of its arguments'");
			}
			else
			{
				if (k == ARRAY_ELEMENT_NOT_FOUND)
				{
					aszTemplates.Add(aszVectors.GetText(i));
					k = aszTemplates.NumElements() - 1;
				}
				szType.Set("T");
				if (k > 0)
				{
					szType.Append((int32)(k + 1));
				}
				aszTypes.Add(szType.Text());
			}
		}
		szType.Kill();
	}

	if (psMacro->bPossible)
	{
		psMacro->szConverted.Clear();
		if (aszTemplates.NumElements() > 0)
		{
			psMacro->szConverted.Append("template<");
			for (i = 0; i < aszTemplates.NumElements(); i++)
			{
				psMacro->szConverted.Append((i > 0) ? ", class T" : "class T");
				if (i > 0)
				{
					psMacro->szConverted.Append((int32)(i + 1));
				}
			}
			psMacro->szConverted.Append(">\n");
		}
		psMacro->szConverted.Append("inline ")->Append(aszTypes.GetText(uiParameters))->Append(' ')->Append(psMacro->szName)->Append('(');
		for (i = 0; i < uiParameters; i++)
		{
			psMacro->szConverted.Append((i > 0) ? ", " : "")->Append(aszTypes.GetText(i))->Append(' ')->Append(psMacro->aszParameters.GetText(i));
		}
		psMacro->szConverted.Append((uiParameters == 0) ? "void) { " : ") { ");
		szBody.Init(psMacro->szBody);
		if (!bStatement && !aszTypes.Get(uiParameters)->Equals("void"))
		{
			psMacro->szConverted.Append("return ")->Append(szBody)->Append("; }");
		}
		else
		{
			//"do { ... } while (0)" is its statements.
			if (szBody.StartsWith("do") && szBody.EndsWith(")") && strstr(szBody.Text(), "{") && strrchr(szBody.Text(), '}') && (strncmp(strrchr(szBody.Text(), '}') + 1, " while", 6) == 0))
			{
				szBody.RemoveEnd((size)(strrchr(szBody.Text(), '}') - szBody.Text()));
				szBody.RemoveFromStart((size)(strstr(szBody.Text(), "{") - szBody.Text()) + 1);
				szBody.StripWhitespace();
			}
			psMacro->szConverted.Append(szBody);
			psMacro->szConverted.Append((szBody.EndsWith(";") || szBody.EndsWith("}")) ? " }" : "; }");
		}
		szBody.Kill();
		if (!psMacro->szComment.Empty())
		{
			psMacro->szConverted.Append("  ")->Append(psMacro->szComment);
		}
		if (!CheckConverted(psMacro, psMacro->uiStart, psMacro->uiEnd, psMacro->szConverted.Text()))
		{
			Refuse(psMacro, "the code doesn't compile with it a function: its body or its parameters' types name something that isn't declared where it is #defined, or a use gives it what a function can't take");
		}
	}
	aszTemplates.Kill();
	aszTypes.Kill();
	aszVectors.Kill();
	aiCounts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a #define's value is a whole number, written plainly: "8",
// "0x10", "-1", "(1)", "16UL".
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEWholeNumber(const char* szValue)
{
	size	i;
	size	uiDigits;
	bool	bBracket;
	bool	bHex;

	i = 0;
	bBracket = (szValue[i] == '(');
	i += bBracket ? 1 : 0;
	i += (szValue[i] == '-') ? 1 : 0;
	bHex = (szValue[i] == '0') && ((szValue[i + 1] == 'x') || (szValue[i + 1] == 'X'));
	i += bHex ? 2 : 0;
	for (uiDigits = 0; ((szValue[i] >= '0') && (szValue[i] <= '9')) || (bHex && (((szValue[i] >= 'a') && (szValue[i] <= 'f')) || ((szValue[i] >= 'A') && (szValue[i] <= 'F')))); i++, uiDigits++)
	{
	}
	for (; (szValue[i] == 'u') || (szValue[i] == 'U') || (szValue[i] == 'l') || (szValue[i] == 'L'); i++)
	{
	}
	if (bBracket && (szValue[i] == ')'))
	{
		i++;
	}
	else if (bBracket)
	{
		return false;
	}
	return (uiDigits > 0) && (szValue[i] == '\0');
}


//////////////////////////////////////////////////////////////////////////
//
// The run of #defines of whole numbers the macro stands in: the lines
// before and after it, one after another with nothing between, that each
// #define a name as a whole number.  Empty if the macro isn't one.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::FindRun(SIDEMacro* psMacro)
{
	SIDEMacro			sOther;
	SIDEMacroConstant*	psConstant;
	CTextFile			cFile;
	const char*			szText;
	size				uiLength;
	size				uiLine;
	size				uiFirst;
	size				uiName;
	size				uiAfter;
	size				uiDirective;
	bool				bConstant;

	if (psMacro->bFunction || !IsIDEWholeNumber(psMacro->szBody.Text()) || (psMacro->uiDefines != 1))
	{
		return;
	}
	cFile.Init();
	if (mpcContent->GetFindInFiles()->GetFileText(psMacro->szFileName.Text(), &cFile, &szText, &uiLength))
	{
		//Up to the run's first line, then down it.
		uiFirst = psMacro->uiStart;
		while (uiFirst > 0)
		{
			for (uiLine = uiFirst - 1; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
			{
			}
			uiDirective = FindIDEMacroDirective(szText, uiLine, &uiName, &uiAfter);
			bConstant = (uiDirective == 6) && (strncmp(&szText[uiName], "define", 6) == 0);
			if (bConstant)
			{
				sOther.Init();
				ReadDefine(szText, uiLine, uiAfter, &sOther);
				bConstant = !sOther.bFunction && IsIDEWholeNumber(sOther.szBody.Text()) && (sOther.uiEnd == uiFirst);
				sOther.Kill();
			}
			if (!bConstant)
			{
				break;
			}
			uiFirst = uiLine;
		}
		for (uiLine = uiFirst; szText[uiLine] != '\0'; )
		{
			uiDirective = FindIDEMacroDirective(szText, uiLine, &uiName, &uiAfter);
			if ((uiDirective != 6) || (strncmp(&szText[uiName], "define", 6) != 0))
			{
				break;
			}
			sOther.Init();
			ReadDefine(szText, uiLine, uiAfter, &sOther);
			bConstant = !sOther.bFunction && IsIDEWholeNumber(sOther.szBody.Text());
			if (bConstant)
			{
				psConstant = psMacro->asRun.Add();
				psConstant->szName.Init(sOther.szName);
				psConstant->szValue.Init(sOther.szBody);
				psConstant->szComment.Init(sOther.szComment);
				psConstant->uiStart = sOther.uiStart;
				psConstant->uiEnd = sOther.uiEnd;
				uiLine = sOther.uiEnd;
			}
			sOther.Kill();
			if (!bConstant)
			{
				break;
			}
		}
	}
	cFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What Convert Macro would make of the macro (szConverted), or why it
// can't (bPossible, szWhyNot), what would change with it (aszConflicts),
// and the run of #defines an enum could be made of (asRun).
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::FindConversion(SIDEMacro* psMacro)
{
	psMacro->bPossible = true;
	FindRun(psMacro);
	if (IsUsedAsMacroOnly(psMacro))
	{
		return;
	}
	if (psMacro->bFunction)
	{
		FindFunctionConversion(psMacro);
	}
	else
	{
		FindConstantConversion(psMacro);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::ShowMessage(CChars* pszMessage, CChars* pszStatus)
{
	if (pszStatus)
	{
		pszStatus->Set(*pszMessage);
	}
	else
	{
		mpcContent->ShowStatus(pszMessage->Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The macro's #define made what FindConversion found, as one undo.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::Convert(SIDEMacro* psMacro, CChars* pszStatus)
{
	CIDEFileEdits	cFiles;
	CChars			szMessage;
	CChars			szText;
	bool			bResult;

	if (!psMacro->bPossible)
	{
		return false;
	}
	cFiles.Init(mpcContent);
	bResult = cFiles.GetText(psMacro->szFileName.Text()) != NULL;
	if (bResult)
	{
		szText.Init(psMacro->szConverted);
		szText.Append('\n');
		cFiles.GetEdits(psMacro->szFileName.Text())->Replace(psMacro->uiStart, psMacro->uiEnd, szText.Text());
		szText.Kill();
		cFiles.Apply();
		szMessage.Init("Converted the macro ");
		szMessage.Append(psMacro->szName)->Append(psMacro->bFunction ? (psMacro->szConverted.StartsWith("template") ? " to a function template." : " to a function.") : " to a constant.");
		ShowMessage(&szMessage, pszStatus);
		szMessage.Kill();
	}
	cFiles.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Why the run of #defines can't be an enum named szEnumName (no name: an
// enum without one), NULL if it can.  Each of them must be able to stop
// being a macro, as the one at the caret must.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEConvertMacro::GetEnumProblem(SIDEMacro* psMacro, const char* szEnumName)
{
	SIDEMacro	sOther;
	size		uiDefines;
	size		i;
	bool		bMacroOnly;

	if (psMacro->asRun.NumElements() == 0)
	{
		return "It isn't a #define of a whole number.";
	}
	if ((szEnumName[0] != '\0') && !IsIDEIdentifier(szEnumName))
	{
		return "The enum's name isn't a name.";
	}
	if ((szEnumName[0] != '\0') && mpcContent->GetCodeModel()->HasSymbol(szEnumName))
	{
		return "There is something of the enum's name already.";
	}
	for (i = 0; i < psMacro->asRun.NumElements(); i++)
	{
		sOther.Init();
		FindUses(psMacro->asRun.Get(i)->szName.Text(), &sOther, &uiDefines);
		sOther.uiDefines = uiDefines;
		bMacroOnly = IsUsedAsMacroOnly(&sOther);
		if (bMacroOnly)
		{
			psMacro->szWhyNot.Set(sOther.szName);
			psMacro->szWhyNot.Append(" can't be an enumerator: ")->Append(sOther.szWhyNot)->Append('.');
		}
		sOther.Kill();
		if (bMacroOnly)
		{
			return psMacro->szWhyNot.Text();
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The run of #defines made one enum, each an enumerator with its value
// (without, where they count up from 0) and its comment.  False, with
// why as the status, if the code wouldn't compile with it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::ConvertToEnum(SIDEMacro* psMacro, const char* szEnumName, CChars* pszStatus)
{
	CIDEFileEdits		cFiles;
	SIDEMacroConstant*	psConstant;
	CChars				szEnum;
	CChars				szMessage;
	size				uiStart;
	size				uiEnd;
	size				i;
	bool				bCounting;
	bool				bResult;

	bCounting = true;
	for (i = 0; i < psMacro->asRun.NumElements(); i++)
	{
		psConstant = psMacro->asRun.Get(i);
		bCounting &= (psConstant->szValue.Text()[0] != '(') && (strtol(psConstant->szValue.Text(), NULL, 0) == (long)i);
	}
	szEnum.Init("enum");
	if (szEnumName[0] != '\0')
	{
		szEnum.Append(' ')->Append(szEnumName);
	}
	szEnum.Append("\n{\n");
	for (i = 0; i < psMacro->asRun.NumElements(); i++)
	{
		psConstant = psMacro->asRun.Get(i);
		szEnum.Append('\t')->Append(psConstant->szName);
		if (!bCounting)
		{
			szEnum.Append(" = ")->Append(psConstant->szValue);
		}
		szEnum.Append(',');
		if (!psConstant->szComment.Empty())
		{
			szEnum.Append("\t\t")->Append(psConstant->szComment);
		}
		szEnum.Append('\n');
	}
	szEnum.Append("};");
	uiStart = psMacro->asRun.Get(0)->uiStart;
	uiEnd = psMacro->asRun.Get(psMacro->asRun.NumElements() - 1)->uiEnd;

	szMessage.Init();
	bResult = CheckConverted(psMacro, uiStart, uiEnd, szEnum.Text());
	if (!bResult)
	{
		szMessage.Append("Can't make an enum of ")->Append(psMacro->szName)->Append(" and the #defines with it: the code that uses them doesn't compile with them enumerators.");
	}
	else
	{
		cFiles.Init(mpcContent);
		bResult = cFiles.GetText(psMacro->szFileName.Text()) != NULL;
		if (bResult)
		{
			szEnum.Append('\n');
			cFiles.GetEdits(psMacro->szFileName.Text())->Replace(uiStart, uiEnd, szEnum.Text());
			cFiles.Apply();
			szMessage.Append("Converted ")->Append((uint32)psMacro->asRun.NumElements())->Append((psMacro->asRun.NumElements() == 1) ? " #define to the enum " : " #defines to the enum ");
			szMessage.Append((szEnumName[0] != '\0') ? szEnumName : "(unnamed)")->Append('.');
		}
		cFiles.Kill();
	}
	ShowMessage(&szMessage, pszStatus);
	szMessage.Kill();
	szEnum.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What the preprocessor makes of a use of the macro with these arguments
// (none, for a macro without parameters), added to pszDest: its body with
// each parameter's name replaced by its argument, #x made a string of
// the argument and a ## b joined.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::AppendExpansion(SIDEMacro* psMacro, CArrayChars* paszArguments, CChars* pszDest)
{
	CChars		szWord;
	CChars		szArgument;
	const char*	szBody;
	size		p;
	size		uiNext;
	size		i;
	size		j;
	size		uiParameter;
	bool		bString;
	bool		bJoin;

	szBody = psMacro->szBody.Text();
	bString = false;
	bJoin = false;
	for (p = 0; szBody[p] != '\0'; p = uiNext)
	{
		uiNext = p + 1;
		if ((szBody[p] == '"') || (szBody[p] == '\''))
		{
			uiNext = SkipIDELiteral(szBody, p);
			pszDest->AppendSubString(szBody, p, uiNext);
		}
		else if (psMacro->bFunction && (szBody[p] == '#') && (szBody[p + 1] == '#'))
		{
			//What is on its two sides is joined.
			for (; pszDest->EndsWith(" ") || pszDest->EndsWith("\t"); )
			{
				pszDest->RemoveLastCharacter();
			}
			for (uiNext = p + 2; (szBody[uiNext] == ' ') || (szBody[uiNext] == '\t'); uiNext++)
			{
			}
			bJoin = true;
			continue;
		}
		else if (psMacro->bFunction && (szBody[p] == '#'))
		{
			for (; (szBody[uiNext] == ' ') || (szBody[uiNext] == '\t'); uiNext++)
			{
			}
			bString = true;
			continue;
		}
		else if (IsIDENameChar(szBody[p]) && !((szBody[p] >= '0') && (szBody[p] <= '9')))
		{
			for (; IsIDENameChar(szBody[uiNext]); uiNext++)
			{
			}
			szWord.Init();
			szWord.AppendSubString(szBody, p, uiNext);
			uiParameter = psMacro->bFunction ? psMacro->aszParameters.GetIndex(szWord.Text()) : ARRAY_ELEMENT_NOT_FOUND;
			if (uiParameter == ARRAY_ELEMENT_NOT_FOUND)
			{
				pszDest->Append(szWord);
			}
			else
			{
				//Its argument; "..."'s is every argument from there on.
				szArgument.Init();
				for (i = uiParameter; (i < paszArguments->NumElements()) && ((i == uiParameter) || (psMacro->bVariadic && (uiParameter + 1 == psMacro->aszParameters.NumElements()))); i++)
				{
					szArgument.Append((i > uiParameter) ? ", " : "")->Append(paszArguments->GetText(i));
				}
				if (bString)
				{
					pszDest->Append('"');
					for (j = 0; j < szArgument.Length(); j++)
					{
						if ((szArgument.Text()[j] == '"') || (szArgument.Text()[j] == '\\'))
						{
							pszDest->Append('\\');
						}
						pszDest->Append(szArgument.Text()[j]);
					}
					pszDest->Append('"');
				}
				else
				{
					pszDest->Append(szArgument);
				}
				szArgument.Kill();
			}
			szWord.Kill();
		}
		else if (IsIDENameChar(szBody[p]))
		{
			//A number: its letters aren't names.
			for (; IsIDENameChar(szBody[uiNext]) || (szBody[uiNext] == '.'); uiNext++)
			{
			}
			pszDest->AppendSubString(szBody, p, uiNext);
		}
		else
		{
			pszDest->Append(szBody[p]);
		}
		bString = false;
		bJoin = false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A text with every use of the macro in it replaced by what the
// preprocessor makes of the use (an argument of a use, which may use the
// macro again), added to pszDest.
//
//////////////////////////////////////////////////////////////////////////
void CIDEConvertMacro::AppendExpanded(SIDEMacro* psMacro, const char* szText, CChars* pszDest)
{
	CArrayChars	aszArguments;
	CChars		szArgument;
	CChars		szExpanded;
	size		uiNameLength;
	size		uiStart;
	size		uiNext;
	size		p;
	size		q;
	int			iDepth;

	uiNameLength = psMacro->szName.Length();
	for (p = 0; szText[p] != '\0'; p = uiNext)
	{
		uiNext = p + 1;
		if ((szText[p] == '"') || (szText[p] == '\''))
		{
			uiNext = SkipIDELiteral(szText, p);
			pszDest->AppendSubString(szText, p, uiNext);
			continue;
		}
		if (!IsIDENameChar(szText[p]))
		{
			pszDest->Append(szText[p]);
			continue;
		}
		for (; IsIDENameChar(szText[uiNext]); uiNext++)
		{
		}
		if ((uiNext - p != uiNameLength) || (strncmp(&szText[p], psMacro->szName.Text(), uiNameLength) != 0))
		{
			pszDest->AppendSubString(szText, p, uiNext);
			continue;
		}
		aszArguments.Init();
		if (psMacro->bFunction)
		{
			for (q = uiNext; (szText[q] == ' ') || (szText[q] == '\t') || (szText[q] == '\r') || (szText[q] == '\n'); q++)
			{
			}
			if (szText[q] != '(')
			{
				pszDest->AppendSubString(szText, p, uiNext);
				aszArguments.Kill();
				continue;
			}
			iDepth = 1;
			uiStart = q + 1;
			for (q++; (szText[q] != '\0') && (iDepth > 0); q++)
			{
				if ((szText[q] == '"') || (szText[q] == '\''))
				{
					q = SkipIDELiteral(szText, q) - 1;
				}
				else if (szText[q] == '(')
				{
					iDepth++;
				}
				else if ((szText[q] == ')') || ((szText[q] == ',') && (iDepth == 1) && !(psMacro->bVariadic && (aszArguments.NumElements() + 1 >= psMacro->aszParameters.NumElements()))))
				{
					iDepth -= (szText[q] == ')') ? 1 : 0;
					if ((iDepth == 0) || (szText[q] == ','))
					{
						szArgument.Init();
						AppendIDETrimmed(&szArgument, szText, uiStart, q);
						szExpanded.Init();
						AppendExpanded(psMacro, szArgument.Text(), &szExpanded);
						if ((szText[q] == ',') || !szExpanded.Empty() || (aszArguments.NumElements() > 0))
						{
							aszArguments.Add(szExpanded.Text());
						}
						szExpanded.Kill();
						szArgument.Kill();
						uiStart = q + 1;
					}
				}
			}
			uiNext = q;
		}
		AppendExpansion(psMacro, &aszArguments, pszDest);
		aszArguments.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Why the macro can't be inlined as iMode asks (IDE_MACRO_INLINE_), NULL
// if it can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEConvertMacro::GetInlineProblem(SIDEMacro* psMacro, int iMode)
{
	SIDEMacroUse*	psUse;
	size			i;

	if (psMacro->uiDefines > 1)
	{
		return "It is #defined more than once.";
	}
	for (i = 0; i < psMacro->asUses.NumElements(); i++)
	{
		psUse = psMacro->asUses.Get(i);
		if ((iMode == IDE_MACRO_INLINE_THIS) && ((int)i != psMacro->iCaretUse))
		{
			continue;
		}
		psMacro->szWhyNot.Clear();
		if (psUse->iKind == IDE_MACRO_USE_DIRECTIVE)
		{
			psMacro->szWhyNot.Append("An #if names it: only a macro can be there");
		}
		else if ((psUse->iKind == IDE_MACRO_USE_NAME) && (iMode == IDE_MACRO_INLINE_ALL))
		{
			psMacro->szWhyNot.Append("Its name is written without arguments, which isn't a use of it");
		}
		else if ((psUse->iKind == IDE_MACRO_USE_NAME) && (iMode == IDE_MACRO_INLINE_THIS))
		{
			return "It isn't used there: its name has no arguments after it.";
		}
		else if (psMacro->bFunction && !psMacro->bVariadic && (psUse->aszArguments.NumElements() != psMacro->aszParameters.NumElements()))
		{
			psMacro->szWhyNot.Append("A use gives it the wrong number of arguments");
		}
		if (!psMacro->szWhyNot.Empty())
		{
			psMacro->szWhyNot.Append(" (")->Append(GetIDEFileNamePart(psUse->szFileName.Text()))->Append(" line ")->Append((int32)(psUse->iLine + 1))->Append(").");
			return psMacro->szWhyNot.Text();
		}
	}
	if ((iMode == IDE_MACRO_INLINE_THIS) && (psMacro->iCaretUse == -1))
	{
		return "The caret isn't on a use of it.";
	}
	if ((iMode == IDE_MACRO_INLINE_ALL) && (psMacro->uiUndefs > 0))
	{
		return "An #undef names it.";
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Inline Macro: the use at the caret, or every use (and then the #define
// is taken out), replaced by what the preprocessor makes of it, as one
// undo.  A use inside another's arguments is replaced with it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::Inline(SIDEMacro* psMacro, int iMode, CChars* pszStatus)
{
	CIDEFileEdits	cFiles;
	CArrayChars		aszArguments;
	SIDEMacroUse*	psUse;
	SIDEMacroUse*	psOuter;
	CChars			szExpansion;
	CChars			szExpanded;
	CChars			szMessage;
	size			uiInlined;
	size			i;
	size			j;
	bool			bInner;

	if (GetInlineProblem(psMacro, iMode))
	{
		return false;
	}
	cFiles.Init(mpcContent);
	uiInlined = 0;
	for (i = 0; i < psMacro->asUses.NumElements(); i++)
	{
		psUse = psMacro->asUses.Get(i);
		if (((iMode == IDE_MACRO_INLINE_THIS) && ((int)i != psMacro->iCaretUse)) || !cFiles.GetText(psUse->szFileName.Text()))
		{
			continue;
		}
		bInner = false;
		for (j = 0; (iMode == IDE_MACRO_INLINE_ALL) && (j < psMacro->asUses.NumElements()) && !bInner; j++)
		{
			psOuter = psMacro->asUses.Get(j);
			bInner = (j != i) && psOuter->szFileName.EqualsIgnoreCase(psUse->szFileName.Text()) && (psOuter->uiStart < psUse->uiStart) && (psOuter->uiEnd >= psUse->uiEnd);
		}
		uiInlined++;
		if (bInner)
		{
			continue;
		}
		aszArguments.Init();
		for (j = 0; j < psUse->aszArguments.NumElements(); j++)
		{
			szExpanded.Init();
			if (iMode == IDE_MACRO_INLINE_ALL)
			{
				AppendExpanded(psMacro, psUse->aszArguments.GetText(j), &szExpanded);
			}
			else
			{
				szExpanded.Append(psUse->aszArguments.GetText(j));
			}
			aszArguments.Add(szExpanded.Text());
			szExpanded.Kill();
		}
		szExpansion.Init();
		AppendExpansion(psMacro, &aszArguments, &szExpansion);
		cFiles.GetEdits(psUse->szFileName.Text())->Replace(psUse->uiStart, psUse->uiEnd, szExpansion.Text());
		szExpansion.Kill();
		aszArguments.Kill();
	}
	if ((iMode == IDE_MACRO_INLINE_ALL) && cFiles.GetText(psMacro->szFileName.Text()))
	{
		cFiles.GetEdits(psMacro->szFileName.Text())->Replace(psMacro->uiStart, psMacro->uiEnd, "");
	}
	cFiles.Apply();
	cFiles.Kill();

	szMessage.Init("Inlined the macro ");
	szMessage.Append(psMacro->szName);
	if (iMode == IDE_MACRO_INLINE_THIS)
	{
		szMessage.Append(" here.");
	}
	else
	{
		szMessage.Append(" at ")->Append((uint32)uiInlined)->Append((uiInlined == 1) ? " use" : " uses")->Append(" and took its #define out.");
	}
	ShowMessage(&szMessage, pszStatus);
	szMessage.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEConvertMacro::PrintMacro(SIDEMacro* psMacro, CChars* pszDest)
{
	SIDEMacroUse*	psUse;
	size			i;
	size			j;

	pszDest->Append("Macro: ")->Append(psMacro->szName);
	if (psMacro->bFunction)
	{
		pszDest->Append('(');
		for (i = 0; i < psMacro->aszParameters.NumElements(); i++)
		{
			pszDest->Append((i > 0) ? ", " : "")->Append((psMacro->bVariadic && (i + 1 == psMacro->aszParameters.NumElements())) ? "..." : psMacro->aszParameters.GetText(i));
		}
		pszDest->Append(')');
	}
	pszDest->Append(" [")->Append(psMacro->szBody)->Append("], ")->Append(GetIDEFileNamePart(psMacro->szFileName.Text()))->Append(" line ")->Append((int32)(psMacro->iLine + 1));
	if (psMacro->uiDefines > 1)
	{
		pszDest->Append(", #defined ")->Append((uint32)psMacro->uiDefines)->Append(" times");
	}
	if (psMacro->uiUndefs > 0)
	{
		pszDest->Append(", #undef'd");
	}
	pszDest->AppendNewLine();
	for (i = 0; i < psMacro->asUses.NumElements(); i++)
	{
		psUse = psMacro->asUses.Get(i);
		pszDest->Append("  ")->Append(GetIDEFileNamePart(psUse->szFileName.Text()))->Append(" line ")->Append((int32)(psUse->iLine + 1))->Append(": ");
		switch (psUse->iKind)
		{
		case IDE_MACRO_USE_DIRECTIVE:
			pszDest->Append("on an #if");
			break;
		case IDE_MACRO_USE_DEFINE:
			pszDest->Append("in a macro's body");
			break;
		case IDE_MACRO_USE_NAME:
			pszDest->Append("its name, without arguments");
			break;
		default:
			pszDest->Append("used");
			break;
		}
		for (j = 0; j < psUse->aszArguments.NumElements(); j++)
		{
			pszDest->Append((j == 0) ? " with [" : ", [")->Append(psUse->aszArguments.GetText(j))->Append(']');
		}
		pszDest->Append(((int)i == psMacro->iCaretUse) ? ", at the caret" : "")->AppendNewLine();
	}
	if (psMacro->bPossible)
	{
		pszDest->Append("Converted: ")->Append(psMacro->szConverted)->AppendNewLine();
	}
	else if (!psMacro->szWhyNot.Empty())
	{
		pszDest->Append("Convert Macro: ")->Append(psMacro->szName)->Append(" can't be: ")->Append(psMacro->szWhyNot)->AppendNewLine();
	}
	for (i = 0; i < psMacro->aszConflicts.NumElements(); i++)
	{
		pszDest->Append("Conflict: ")->Append(psMacro->aszConflicts.GetText(i))->AppendNewLine();
	}
	if (psMacro->asRun.NumElements() > 0)
	{
		pszDest->Append("Enum of:");
		for (i = 0; i < psMacro->asRun.NumElements(); i++)
		{
			pszDest->Append(' ')->Append(psMacro->asRun.Get(i)->szName);
		}
		pszDest->AppendNewLine();
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// Convert Macro's dialog, for the macro at the caret: what it would
// become and, for a #define of a whole number, whether the #defines it
// stands among become an enum instead (and the enum's name).  What would
// change with it is shown first, as Problems Detected.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::ShowConvert(void)
{
	SIDEMacro		sMacro;
	Ptr<CCheckBox>	pEnum;
	Ptr<CTextField>	pEnumName;
	CChars			szMessage;
	CChars			szEnumName;
	bool			bEnum;
	bool			bResult;

	sMacro.Init();
	if (!FindMacro(&sMacro))
	{
		mpcContent->ShowStatus("There is no macro at the caret: the name there isn't #defined in the solution's files.");
		sMacro.Kill();
		return false;
	}
	FindConversion(&sMacro);
	if (!sMacro.bPossible && ((sMacro.asRun.NumElements() == 0) || (sMacro.uiDefines > 1)))
	{
		szMessage.Init("Can't convert the macro ");
		szMessage.Append(sMacro.szName)->Append(": ")->Append(sMacro.szWhyNot)->Append('.');
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sMacro.Kill();
		return false;
	}

	szMessage.Init("Convert Macro ");
	szMessage.Append(sMacro.szName);
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Clear();
	if (sMacro.bPossible)
	{
		szMessage.Append("It becomes: ")->Append(sMacro.szConverted);
		szMessage.Replace("\n", " ");
	}
	else
	{
		szMessage.Append("It can't be a constant: ")->Append(sMacro.szWhyNot);
	}
	mpDialog->AddDivider(szMessage.Text());
	szMessage.Kill();
	if (sMacro.asRun.NumElements() > 0)
	{
		szMessage.Init("Make an enum of ");
		if (sMacro.asRun.NumElements() == 1)
		{
			szMessage.Append("it");
		}
		else
		{
			szMessage.Append("the ")->Append((uint32)sMacro.asRun.NumElements())->Append(" #defines here");
		}
		szMessage.Append(" instead");
		pEnum = mpDialog->AddCheckBox(szMessage.Text(), !sMacro.bPossible);
		pEnumName = mpDialog->AddTextField("The enum's name:", "");
		szMessage.Kill();
	}
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(560);
	bEnum = bResult && pEnum.IsNotNull() && pEnum->IsTicked();
	szEnumName.Init(pEnumName.IsNotNull() ? pEnumName->GetText() : "");
	pEnum = NULL;
	pEnumName = NULL;
	mpDialog = NULL;
	if (bResult && bEnum)
	{
		if (GetEnumProblem(&sMacro, szEnumName.Text()))
		{
			mpcContent->ShowStatus(GetEnumProblem(&sMacro, szEnumName.Text()));
			bResult = false;
		}
		bResult = bResult && ConvertToEnum(&sMacro, szEnumName.Text());
	}
	else if (bResult)
	{
		bResult = sMacro.bPossible && ((sMacro.aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&sMacro.aszConflicts));
		bResult = bResult && Convert(&sMacro);
	}
	szEnumName.Kill();
	sMacro.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Inline Macro's dialog, for the macro at the caret: every use and the
// #define taken out, or only the use the caret is on.  *pbMacro says
// whether there is a macro at the caret: if not, Inline... tries a
// function.  False if nothing was done.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEConvertMacro::ShowInline(bool* pbMacro)
{
	SIDEMacro		sMacro;
	Ptr<CComboBox>	pMode;
	CChars			szMessage;
	const char*		aszModes[2];
	int				iMode;
	bool			bResult;

	sMacro.Init();
	*pbMacro = FindMacro(&sMacro);
	if (!*pbMacro)
	{
		sMacro.Kill();
		return false;
	}
	szMessage.Init("Inline Macro ");
	szMessage.Append(sMacro.szName);
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	aszModes[IDE_MACRO_INLINE_ALL] = "All uses, and take the #define out";
	aszModes[IDE_MACRO_INLINE_THIS] = "This use only";
	pMode = mpDialog->AddComboBox("Inline:", aszModes, (sMacro.iCaretUse != -1) ? 2 : 1, IDE_MACRO_INLINE_ALL);
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(480);
	iMode = bResult ? pMode->GetSelected() : IDE_MACRO_INLINE_ALL;
	pMode = NULL;
	mpDialog = NULL;
	if (bResult && GetInlineProblem(&sMacro, iMode))
	{
		szMessage.Init("Can't inline the macro ");
		szMessage.Append(sMacro.szName)->Append(": ")->Append(GetInlineProblem(&sMacro, iMode));
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		bResult = false;
	}
	bResult = bResult && Inline(&sMacro, iMode);
	sMacro.Kill();
	return bResult;
}
