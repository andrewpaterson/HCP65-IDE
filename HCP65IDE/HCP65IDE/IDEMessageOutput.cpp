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
#include <stdio.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/ArrayChars.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEMessageOutput.h"


//////////////////////////////////////////////////////////////////////////
//
// The messages of pPane's output, called szNoun on the status line ("No
// <noun> after this one.").  Enter, a double click and Jump to Source in
// the output open the message there.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::Init(CIDEContent* pcContent, Ptr<CIDEBuildPane> pPane, const char* szNoun)
{
	Ptr<CTextArea>	pOutput;

	PreInit();
	mpcContent = pcContent;
	mpPane = pPane;
	mszNoun.Init(szNoun);
	muiStepLine = MAX_SIZE;

	pOutput = mpPane->GetOutputArea();
	pOutput->SetEnterEvent(true);
	pOutput->GetEnterEvent()->Add(this, &CIDEMessageOutput::EnterPressed);
	pOutput->SetDoubleClickEvent(true);
	pOutput->GetDoubleClickEvent()->Add(this, &CIDEMessageOutput::DoubleClicked);
	mpJumpAction = pOutput->AddInputAction(CIC_JumpToSource, this, &CIDEMessageOutput::JumpToSource);

	mpMenu = OMalloc<CIDEOutputMenu>(mpcContent, pOutput);
	mpMenu->AddSeparator();
	mpMenu->AddOwnCommand("Output.JumpToSource", "Jump to Source", "F4", this, &CIDEMessageOutput::JumpToSource)->SetEnabledCheck(this, &CIDEMessageOutput::IsOnMessage);
	mpMenu->AddSeparator();
	mpMenu->AddOwnCommand("Output.ClearAll", "Clear All", "", this, &CIDEMessageOutput::ClearOutput);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpPane);
	M_Pointer(mpJumpAction);
	M_Pointer(mpMenu);
	U_String(mszNoun);
	U_Size(muiStepLine);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::Free(void)
{
	Ptr<CTextArea>	pOutput;

	if (mpPane.IsNotNull() && mpPane->GetOutputArea().IsNotNull())
	{
		pOutput = mpPane->GetOutputArea();
		pOutput->GetEnterEvent()->Remove(this);
		pOutput->GetDoubleClickEvent()->Remove(this);
		if (mpJumpAction.IsNotNull())
		{
			pOutput->RemoveInputAction(mpJumpAction);
		}
	}
	mszNoun.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The messages aren't saved.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::Save(CObjectWriter* pcFile) { return false; }
bool CIDEMessageOutput::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The output was cleared: no message has been gone to.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::Clear(void)
{
	muiStepLine = MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the output has a message with a place that can be found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::HasMessages(void)
{
	CArrayIDEOutputMessages	asMessages;
	size					uiMessages;

	asMessages.Init();
	uiMessages = FindMessages(&asMessages);
	KillMessages(&asMessages);
	return uiMessages > 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::KillMessages(CArrayIDEOutputMessages* pasMessages)
{
	size	i;

	for (i = 0; i < pasMessages->NumElements(); i++)
	{
		pasMessages->Get(i)->szFile.Kill();
	}
	pasMessages->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Next or Previous Occurrence: the message after (or before) the current
// one, where the tab's caret was put, or else the one last gone to, is
// selected and its place opened in its editor, which has the keys unless
// bKeepKeys (the tab was stepped in).  Past the last (or first) nothing
// moves.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::Step(bool bNext, bool bKeepKeys)
{
	CArrayIDEOutputMessages	asMessages;
	SIDEOutputMessage*		psMessage;
	SIDEOutputMessage*		psFound;
	Ptr<CTextArea>			pOutput;
	CChars					szStatus;
	size					uiCurrent;
	size					i;

	pOutput = mpPane->GetOutputArea();
	asMessages.Init();
	FindMessages(&asMessages);

	//A caret put somewhere (not left following the output at its end) says where the current message is.
	uiCurrent = muiStepLine;
	if (pOutput->GetCaretPosition() < strlen(pOutput->GetText()))
	{
		uiCurrent = pOutput->FindLine(pOutput->GetCaretPosition());
	}
	psFound = NULL;
	for (i = 0; i < asMessages.NumElements(); i++)
	{
		psMessage = asMessages.Get(bNext ? i : asMessages.NumElements() - 1 - i);
		if ((uiCurrent == MAX_SIZE) || (bNext ? (psMessage->uiOutputLine > uiCurrent) : (psMessage->uiOutputLine < uiCurrent)))
		{
			psFound = psMessage;
			break;
		}
	}
	if (psFound)
	{
		Open(psFound, bKeepKeys);
	}
	else
	{
		szStatus.Init("No ");
		szStatus.Append(mszNoun);
		szStatus.Append(bNext ? " after this one." : " before this one.");
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
	}
	KillMessages(&asMessages);
	return psFound != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The message on the tab's line uiLine (from 0) is selected and its place
// opened, as Step does.  False if there isn't one there, and nothing is
// done.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::OpenLine(size uiLine, bool bKeepKeys)
{
	CArrayIDEOutputMessages	asMessages;
	SIDEOutputMessage*		psFound;
	size					i;

	asMessages.Init();
	FindMessages(&asMessages);
	psFound = NULL;
	for (i = 0; i < asMessages.NumElements(); i++)
	{
		if (asMessages.Get(i)->uiOutputLine == uiLine)
		{
			psFound = asMessages.Get(i);
			break;
		}
	}
	if (psFound)
	{
		Open(psFound, bKeepKeys);
	}
	KillMessages(&asMessages);
	return psFound != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// psMessage's line is selected in the tab and its place opened in its
// editor, its word selected, as IntelliJ IDEA's Build window does.  The
// editor has the keys unless bKeepKeys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::Open(SIDEOutputMessage* psMessage, bool bKeepKeys)
{
	Ptr<CTextArea>	pOutput;
	SIDELocation	sLocation;

	pOutput = mpPane->GetOutputArea();
	muiStepLine = psMessage->uiOutputLine;
	pOutput->Select(pOutput->GetLineStart(psMessage->uiOutputLine), pOutput->GetLineEnd(psMessage->uiOutputLine));
	memset(&sLocation, 0, sizeof(SIDELocation));
	sLocation.szFileName.Init(psMessage->szFile);
	sLocation.iLine = psMessage->iLine - 1;
	sLocation.iColumn = (psMessage->iColumn > 0) ? psMessage->iColumn - 1 : 0;
	sLocation.iContainer = -1;
	if (mpcContent->GetDocuments()->OpenLocation(&sLocation))
	{
		mpcContent->GetDocuments()->SelectWordAtCaret();
	}
	sLocation.szFileName.Kill();
	if (bKeepKeys)
	{
		mpcContent->GetWindow()->GetFocus()->SetFocus(pOutput);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Enter, a double click and Jump to Source in the tab: the message on the
// caret's line is opened, and its editor has the keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::EnterPressed(void)
{
	Ptr<CTextArea>	pOutput;

	pOutput = mpPane->GetOutputArea();
	OpenLine(pOutput->FindLine(pOutput->GetCaretPosition()), false);
}


void CIDEMessageOutput::DoubleClicked(CTextArea* pcTextArea, size uiPosition)
{
	OpenLine(pcTextArea->FindLine(uiPosition), false);
}


void CIDEMessageOutput::JumpToSource(void)
{
	EnterPressed();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the caret's line is a message that says a place: the menu's
// Jump to Source is dim on any other.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::IsOnMessage(void)
{
	Ptr<CTextArea>	pOutput;
	CChars			szLine;
	CChars			szFile;
	size			uiLine;
	int32			iLine;
	int32			iColumn;
	EIDEMessageKind	eKind;
	bool			bResult;

	pOutput = mpPane->GetOutputArea();
	uiLine = pOutput->FindLine(pOutput->GetCaretPosition());
	szLine.Init();
	szLine.AppendSubString(pOutput->GetText(), pOutput->GetLineStart(uiLine), pOutput->GetLineEnd(uiLine));
	szFile.Init();
	bResult = ParseMessageLine(szLine.Text(), &szFile, &iLine, &iColumn, &eKind);
	szFile.Kill();
	szLine.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The menu's Clear All: the tab's text goes, and its messages with it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEMessageOutput::ClearOutput(void)
{
	mpPane->Clear();
	Clear();
}


Ptr<CIDEOutputMenu> CIDEMessageOutput::GetMenu(void) const { return mpMenu; }


//////////////////////////////////////////////////////////////////////////
//
// The tab's messages with a place in a file that can be found, in the
// order it has them, appended to pasDest.  Each file is found by its name
// (CIDECodeModel::FindFilesNamed), the one next to the unit being
// compiled ("Kernel.cpp (1/12)") if more than one has it.  How many there
// are.
//
//////////////////////////////////////////////////////////////////////////
size CIDEMessageOutput::FindMessages(CArrayIDEOutputMessages* pasDest)
{
	CArrayChars			aszLines;
	CChars				szUnit;
	CChars				szName;
	CChars				szPath;
	SIDEOutputMessage*	psMessage;
	int32				iLine;
	int32				iColumn;
	EIDEMessageKind		eKind;
	size				i;
	size				uiFound;

	aszLines.Init();
	aszLines.Split(mpPane->GetOutput(), '\n');
	szUnit.Init();
	uiFound = 0;
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		szName.Init();
		szPath.Init();
		if (ParseUnitLine(aszLines.GetText(i), &szName))
		{
			szUnit.Clear();
			FindMessageFile(szName.Text(), NULL, &szUnit);
		}
		else if (ParseMessageLine(aszLines.GetText(i), &szName, &iLine, &iColumn, &eKind) && FindMessageFile(szName.Text(), szUnit.Text(), &szPath))
		{
			psMessage = pasDest->Add();
			psMessage->uiOutputLine = i;
			psMessage->szFile.Init(szPath);
			psMessage->iLine = iLine;
			psMessage->iColumn = iColumn;
			psMessage->eKind = eKind;
			uiFound++;
		}
		szPath.Kill();
		szName.Kill();
	}
	szUnit.Kill();
	aszLines.Kill();
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The full path of the file a message names: szName itself if it is one,
// or the file called szName in szUnit's directory (the unit being
// compiled, "" or NULL if it isn't known), or the first file called it.
// False if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::FindMessageFile(const char* szName, const char* szUnit, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszFound;
	CChars		szUnitName;
	CChars		szUnitDirectory;
	CChars		szName2;
	CChars		szDirectory;
	size		i;
	size		uiChosen;

	if (cFileUtil.IsAbsolutePath(szName))
	{
		pszDest->Append(szName);
		return true;
	}
	aszFound.Init();
	if (mpcContent->GetCodeModel()->FindFilesNamed(szName, &aszFound) == 0)
	{
		aszFound.Kill();
		return false;
	}
	uiChosen = 0;
	if ((szUnit != NULL) && (szUnit[0] != '\0'))
	{
		szUnitName.Init();
		szUnitDirectory.Init();
		cFileUtil.SplitPath(szUnit, &szUnitName, &szUnitDirectory);
		for (i = 0; i < aszFound.NumElements(); i++)
		{
			szName2.Init();
			szDirectory.Init();
			cFileUtil.SplitPath(aszFound.GetText(i), &szName2, &szDirectory);
			if (szDirectory.EqualsIgnoreCase(szUnitDirectory))
			{
				uiChosen = i;
				szDirectory.Kill();
				szName2.Kill();
				break;
			}
			szDirectory.Kill();
			szName2.Kill();
		}
		szUnitDirectory.Kill();
		szUnitName.Kill();
	}
	pszDest->Append(aszFound.GetText(uiChosen));
	aszFound.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A message with a place: "<file> (<line>, <column>): ...", the HCP65
// compiler's and the code model's, "<file>(<line>,<column>): ..." or
// "<file>(<line>): ...", MSVC's, or "<file>:<line>:<column>: <kind>: ..."
// or "<file>:<line>: <kind>: ...", GCC's (its kind is error, fatal
// error, warning or note).  The file, line, column (1 if there isn't one)
// and kind of it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::ParseMessageLine(const char* szLine, CChars* pszFile, int32* piLine, int32* piColumn, EIDEMessageKind* peKind)
{
	const char*	szMessage;

	if (ParseBracketedPlace(szLine, pszFile, piLine, piColumn, &szMessage) || ParseColonPlace(szLine, pszFile, piLine, piColumn, &szMessage))
	{
		*peKind = FindKind(szMessage);
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// "<file> (<line>, <column>): " or "<file>(<line>): ": *pszMessage is
// what follows.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::ParseBracketedPlace(const char* szLine, CChars* pszFile, int32* piLine, int32* piColumn, const char** pszMessage)
{
	const char*	szClose;
	const char*	szOpen;
	const char*	sz;
	int			iLine;
	int			iColumn;
	int			iRead;
	size		uiFileLength;

	szClose = strstr(szLine, "): ");
	if (szClose == NULL)
	{
		return false;
	}
	for (szOpen = szClose; (szOpen > szLine) && (*szOpen != '('); szOpen--)
	{
	}
	if ((*szOpen != '(') || (szOpen == szLine))
	{
		return false;
	}
	for (sz = szOpen + 1; sz < szClose; sz++)
	{
		if (!(((*sz >= '0') && (*sz <= '9')) || (*sz == ',') || (*sz == ' ')))
		{
			return false;
		}
	}
	iLine = 0;
	iColumn = 1;
	iRead = sscanf(szOpen + 1, "%d , %d", &iLine, &iColumn);
	if ((iRead < 1) || (iLine < 1))
	{
		return false;
	}
	uiFileLength = (size)(szOpen - szLine);
	while ((uiFileLength > 0) && (szLine[uiFileLength - 1] == ' '))
	{
		uiFileLength--;
	}
	if (uiFileLength == 0)
	{
		return false;
	}
	pszFile->AppendSubString(szLine, 0, uiFileLength);
	*piLine = iLine;
	*piColumn = (iRead >= 2) ? iColumn : 1;
	*pszMessage = szClose + 3;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// GCC's "<file>:<line>:<column>: <kind>: " or "<file>:<line>: <kind>: ",
// its kind error, fatal error, warning or note (so "In file included
// from" and "In function" lines aren't messages).  A drive's colon
// ("C:\...") is part of the file.  *pszMessage is the kind and what
// follows it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::ParseColonPlace(const char* szLine, CChars* pszFile, int32* piLine, int32* piColumn, const char** pszMessage)
{
	static const char*	aszKinds[] = { "error: ", "fatal error: ", "warning: ", "note: ", NULL };
	const char*	szColon;
	const char*	sz;
	int32		iLine;
	int32		iColumn;
	size		i;

	for (szColon = strchr(szLine, ':'); szColon != NULL; szColon = strchr(szColon + 1, ':'))
	{
		if ((szColon == szLine) || ((szColon == szLine + 1) && ((szColon[1] == '\\') || (szColon[1] == '/'))))
		{
			continue;
		}
		sz = szColon + 1;
		if (!ReadNumber(&sz, &iLine) || (*sz != ':') || (iLine < 1))
		{
			continue;
		}
		sz++;
		iColumn = 1;
		if (ReadNumber(&sz, &iColumn))
		{
			if (*sz != ':')
			{
				continue;
			}
			sz++;
		}
		if (*sz != ' ')
		{
			continue;
		}
		sz++;
		for (i = 0; aszKinds[i] != NULL; i++)
		{
			if (strncmp(sz, aszKinds[i], strlen(aszKinds[i])) == 0)
			{
				pszFile->AppendSubString(szLine, 0, (size)(szColon - szLine));
				*piLine = iLine;
				*piColumn = (iColumn > 0) ? iColumn : 1;
				*pszMessage = sz;
				return true;
			}
		}
		return false;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The digits at *psz, which is moved past them.  False if there aren't
// any.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::ReadNumber(const char** psz, int32* piNumber)
{
	const char*	sz;
	int32		iNumber;

	sz = *psz;
	iNumber = 0;
	while ((*sz >= '0') && (*sz <= '9'))
	{
		iNumber = iNumber * 10 + (*sz - '0');
		sz++;
	}
	if (sz == *psz)
	{
		return false;
	}
	*psz = sz;
	*piNumber = iNumber;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What a message after its place says it is: a warning if it starts
// "Warning: " (the HCP65 compiler's), "#warning" (a preprocessor's
// #warning), "warning: " (GCC's) or "warning C" (MSVC's "warning C4100:"),
// a note if it starts "note: " (GCC's), and otherwise an error.
//
//////////////////////////////////////////////////////////////////////////
EIDEMessageKind CIDEMessageOutput::FindKind(const char* szMessage)
{
	if ((strncmp(szMessage, "Warning: ", 9) == 0) || (strncmp(szMessage, "warning: ", 9) == 0) || (strncmp(szMessage, "#warning", 8) == 0) || (strncmp(szMessage, "warning C", 9) == 0))
	{
		return IMK_Warning;
	}
	if (strncmp(szMessage, "note: ", 6) == 0)
	{
		return IMK_Note;
	}
	return IMK_Error;
}


//////////////////////////////////////////////////////////////////////////
//
// A unit's line as it is compiled: "<file> (<count>/<total>)".  The file
// of it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEMessageOutput::ParseUnitLine(const char* szLine, CChars* pszName)
{
	const char*	szOpen;
	const char*	sz;
	size		uiLength;
	bool		bSlash;

	uiLength = strlen(szLine);
	if ((uiLength < 6) || (szLine[uiLength - 1] != ')'))
	{
		return false;
	}
	szOpen = strrchr(szLine, '(');
	if ((szOpen == NULL) || (szOpen < szLine + 2) || (szOpen[-1] != ' '))
	{
		return false;
	}
	bSlash = false;
	for (sz = szOpen + 1; sz < szLine + uiLength - 1; sz++)
	{
		if (*sz == '/')
		{
			bSlash = true;
		}
		else if ((*sz < '0') || (*sz > '9'))
		{
			return false;
		}
	}
	if (!bSlash)
	{
		return false;
	}
	pszName->AppendSubString(szLine, 0, (size)(szOpen - 1 - szLine));
	return true;
}

