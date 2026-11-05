/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela ClaudeLib

Codaphela ClaudeLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela ClaudeLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela ClaudeLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "ClaudeStream.h"


//The tools that change files, and the member of their input naming the file.
const char* gaszClaudeEditTools[] =
{
	"Edit", "file_path",
	"MultiEdit", "file_path",
	"Write", "file_path",
	"NotebookEdit", "notebook_path",
	NULL
};

//The members of a tool's input that say what it was used on, the first found is shown.
const char* gaszClaudeToolDetails[] =
{
	"file_path",
	"notebook_path",
	"pattern",
	"command",
	"path",
	"url",
	"query",
	"description",
	"prompt",
	NULL
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::Init(void)
{
	mcDocument.Init();
	mszPartial.Init();
	mszSessionId.Init();
	maszChangedFiles.Init();
	maszPendingIds.Init();
	maszPendingFiles.Init();
	mbFinished = false;
	mbSucceeded = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::Kill(void)
{
	maszPendingFiles.Kill();
	maszPendingIds.Kill();
	maszChangedFiles.Kill();
	mszSessionId.Kill();
	mszPartial.Kill();
	mcDocument.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// For another run: nothing read, nothing changed and not finished.  The
// session's id is kept, so the next run can carry the conversation on.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::Restart(void)
{
	mszPartial.Clear();
	maszChangedFiles.ReInit();
	maszPendingIds.ReInit();
	maszPendingFiles.ReInit();
	mbFinished = false;
	mbSucceeded = false;
}


//////////////////////////////////////////////////////////////////////////
//
// Adds the events of each line ended in szOutput's uiLength bytes to
// paEvents.  A line can arrive over more than one Append.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::Append(const char* szOutput, size uiLength, CArrayClaudeEvents* paEvents)
{
	const char*	szLine;
	const char*	szEnd;
	const char*	szNewLine;

	szLine = szOutput;
	szEnd = szOutput + uiLength;
	for (;;)
	{
		szNewLine = (const char*)memchr(szLine, '\n', (size)(szEnd - szLine));
		if (szNewLine == NULL)
		{
			mszPartial.Append(szLine, (size)(szEnd - szLine));
			return;
		}

		if (mszPartial.Empty())
		{
			ParseLine(szLine, (size)(szNewLine - szLine), paEvents);
		}
		else
		{
			mszPartial.Append(szLine, (size)(szNewLine - szLine));
			ParseLine(mszPartial.Text(), mszPartial.Length(), paEvents);
			mszPartial.Clear();
		}
		szLine = szNewLine + 1;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The output has ended: a last line with no line end is read too.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::End(CArrayClaudeEvents* paEvents)
{
	if (!mszPartial.Empty())
	{
		ParseLine(mszPartial.Text(), mszPartial.Length(), paEvents);
		mszPartial.Clear();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One line of output, without its line end.  A line that isn't a JSON
// object is an output event; blank lines and kinds of message the IDE
// doesn't show (rate limits, hooks) are none.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ParseLine(const char* szLine, size uiLength, CArrayClaudeEvents* paEvents)
{
	size			uiRoot;
	char*			szType;
	CChars			szText;
	CClaudeEvent*	pcEvent;

	if ((uiLength > 0) && (szLine[uiLength - 1] == '\r'))
	{
		uiLength--;
	}
	if (uiLength == 0)
	{
		return;
	}

	if (!mcDocument.Parse(szLine, uiLength) || (mcDocument.GetType(mcDocument.GetRoot()) != JT_Object))
	{
		szText.Init();
		szText.Append(szLine, uiLength);
		pcEvent = paEvents->Add(CET_Output);
		pcEvent->SetText(szText.Text());
		szText.Kill();
		return;
	}

	uiRoot = mcDocument.GetRoot();
	szType = mcDocument.GetString(uiRoot, "type");
	if (szType == NULL)
	{
		return;
	}
	if (strcmp(szType, "system") == 0)
	{
		ParseInit(uiRoot, paEvents);
	}
	else if (strcmp(szType, "assistant") == 0)
	{
		ParseAssistant(uiRoot, paEvents);
	}
	else if (strcmp(szType, "user") == 0)
	{
		ParseUser(uiRoot, paEvents);
	}
	else if (strcmp(szType, "result") == 0)
	{
		ParseResult(uiRoot, paEvents);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// {"type":"system","subtype":"init","session_id":...,"model":...}.  Other
// system messages (hooks, compacting) aren't shown.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ParseInit(size uiRoot, CArrayClaudeEvents* paEvents)
{
	char*			szSubtype;
	char*			szModel;
	CClaudeEvent*	pcEvent;

	szSubtype = mcDocument.GetString(uiRoot, "subtype");
	if ((szSubtype == NULL) || (strcmp(szSubtype, "init") != 0))
	{
		return;
	}

	KeepSessionId(uiRoot);
	pcEvent = paEvents->Add(CET_Started);
	szModel = mcDocument.GetString(uiRoot, "model");
	if (szModel)
	{
		pcEvent->SetDetail(szModel);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// {"type":"assistant","message":{"content":[{"type":"text",...},
// {"type":"tool_use",...}]}}.  Thinking isn't shown.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ParseAssistant(size uiRoot, CArrayClaudeEvents* paEvents)
{
	size			uiContent;
	size			uiBlock;
	char*			szType;
	char*			szText;
	CClaudeEvent*	pcEvent;

	uiContent = mcDocument.GetMember(mcDocument.GetMember(uiRoot, "message"), "content");
	for (uiBlock = mcDocument.GetFirst(uiContent); uiBlock != JSON_NONE; uiBlock = mcDocument.GetNext(uiBlock))
	{
		szType = mcDocument.GetString(uiBlock, "type");
		if (szType == NULL)
		{
			continue;
		}
		if (strcmp(szType, "text") == 0)
		{
			szText = mcDocument.GetString(uiBlock, "text");
			if (szText && (szText[0] != '\0'))
			{
				pcEvent = paEvents->Add(CET_Text);
				pcEvent->SetText(szText);
			}
		}
		else if (strcmp(szType, "tool_use") == 0)
		{
			ParseToolUse(uiBlock, paEvents);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// {"type":"tool_use","id":...,"name":"Edit","input":{"file_path":...}}.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ParseToolUse(size uiBlock, CArrayClaudeEvents* paEvents)
{
	char*			szName;
	char*			szId;
	char*			szDetail;
	char*			szFile;
	size			uiInput;
	size			i;
	CClaudeEvent*	pcEvent;

	szName = mcDocument.GetString(uiBlock, "name");
	if (szName == NULL)
	{
		return;
	}
	uiInput = mcDocument.GetMember(uiBlock, "input");

	pcEvent = paEvents->Add(CET_ToolUse);
	pcEvent->SetTool(szName);
	for (i = 0; gaszClaudeToolDetails[i] != NULL; i++)
	{
		szDetail = mcDocument.GetString(uiInput, gaszClaudeToolDetails[i]);
		if (szDetail)
		{
			pcEvent->SetDetail(szDetail);
			break;
		}
	}

	for (i = 0; gaszClaudeEditTools[i] != NULL; i += 2)
	{
		if (strcmp(szName, gaszClaudeEditTools[i]) == 0)
		{
			szFile = mcDocument.GetString(uiInput, gaszClaudeEditTools[i + 1]);
			szId = mcDocument.GetString(uiBlock, "id");
			if (szFile && szId)
			{
				pcEvent->SetFile(szFile);
				maszPendingIds.Add(szId);
				maszPendingFiles.Add(szFile);
			}
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// {"type":"user","message":{"content":[{"type":"tool_result",...}]}}: the
// results of Claude's tool uses.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ParseUser(size uiRoot, CArrayClaudeEvents* paEvents)
{
	size	uiContent;
	size	uiBlock;
	char*	szType;

	uiContent = mcDocument.GetMember(mcDocument.GetMember(uiRoot, "message"), "content");
	for (uiBlock = mcDocument.GetFirst(uiContent); uiBlock != JSON_NONE; uiBlock = mcDocument.GetNext(uiBlock))
	{
		szType = mcDocument.GetString(uiBlock, "type");
		if (szType && (strcmp(szType, "tool_result") == 0))
		{
			ParseToolResult(uiBlock, paEvents);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// {"type":"tool_result","tool_use_id":...,"content":...,"is_error":true}.
// Its content is a string, or a list of blocks of which the text is shown.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ParseToolResult(size uiBlock, CArrayClaudeEvents* paEvents)
{
	CClaudeEvent*	pcEvent;
	CChars			szText;
	bool			bError;
	char*			szId;

	bError = mcDocument.GetBool(uiBlock, "is_error", false);
	szText.Init();
	GetContentText(mcDocument.GetMember(uiBlock, "content"), &szText);
	pcEvent = paEvents->Add(CET_ToolResult);
	pcEvent->SetText(szText.Text());
	pcEvent->SetError(bError);
	szText.Kill();

	szId = mcDocument.GetString(uiBlock, "tool_use_id");
	if (szId)
	{
		ToolAnswered(szId, bError);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A string, or the texts of a list of {"type":"text","text":...} blocks
// with line ends between them.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::GetContentText(size uiContent, CChars* pszDest)
{
	size	uiBlock;
	char*	szText;

	if (mcDocument.GetType(uiContent) == JT_String)
	{
		pszDest->Append(mcDocument.GetText(uiContent));
		return;
	}

	for (uiBlock = mcDocument.GetFirst(uiContent); uiBlock != JSON_NONE; uiBlock = mcDocument.GetNext(uiBlock))
	{
		szText = mcDocument.GetString(uiBlock, "text");
		if (szText)
		{
			if (!pszDest->Empty())
			{
				pszDest->Append('\n');
			}
			pszDest->Append(szText);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A tool use has its result.  If it changes a file and didn't fail, the
// file has changed.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ToolAnswered(const char* szId, bool bError)
{
	size	uiIndex;

	uiIndex = maszPendingIds.GetIndex(szId);
	if (uiIndex == ARRAY_ELEMENT_NOT_FOUND)
	{
		return;
	}

	if (!bError)
	{
		maszChangedFiles.AddIfUnique(maszPendingFiles.GetText(uiIndex));
	}
	maszPendingIds.Remove(uiIndex);
	maszPendingFiles.Remove(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// {"type":"result","subtype":"success","is_error":false,"result":...,
// "total_cost_usd":0.07,"permission_denials":[{"tool_name":...}]}.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::ParseResult(size uiRoot, CArrayClaudeEvents* paEvents)
{
	CClaudeEvent*	pcEvent;
	char*			szText;
	char*			szSubtype;
	char*			szTool;
	size			uiDenial;
	bool			bError;

	KeepSessionId(uiRoot);
	szSubtype = mcDocument.GetString(uiRoot, "subtype");
	bError = mcDocument.GetBool(uiRoot, "is_error", false) || (szSubtype == NULL) || (strcmp(szSubtype, "success") != 0);

	pcEvent = paEvents->Add(CET_Finished);
	pcEvent->SetError(bError);
	szText = mcDocument.GetString(uiRoot, "result");
	if (szText)
	{
		pcEvent->SetText(szText);
	}
	else if (szSubtype)
	{
		//An error without an answer, such as "error_max_turns".
		pcEvent->SetText(szSubtype);
	}
	szText = mcDocument.GetString(uiRoot, "total_cost_usd");
	if (szText)
	{
		pcEvent->SetDetail(szText);
	}
	for (uiDenial = mcDocument.GetFirst(mcDocument.GetMember(uiRoot, "permission_denials")); uiDenial != JSON_NONE; uiDenial = mcDocument.GetNext(uiDenial))
	{
		szTool = mcDocument.GetString(uiDenial, "tool_name");
		if (szTool)
		{
			pcEvent->AddDenied(szTool);
		}
	}

	mbFinished = true;
	mbSucceeded = !bError;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::KeepSessionId(size uiRoot)
{
	char*	szSessionId;

	szSessionId = mcDocument.GetString(uiRoot, "session_id");
	if (szSessionId && (szSessionId[0] != '\0'))
	{
		SetSessionId(szSessionId);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The session the next run carries on, "" for a new one.
//
//////////////////////////////////////////////////////////////////////////
void CClaudeStream::SetSessionId(const char* szSessionId)
{
	mszSessionId.Clear();
	mszSessionId.Append(szSessionId);
}


//////////////////////////////////////////////////////////////////////////
//
// True if szTool changes files.
//
//////////////////////////////////////////////////////////////////////////
bool IsClaudeEditTool(const char* szTool)
{
	size	i;

	for (i = 0; gaszClaudeEditTools[i] != NULL; i += 2)
	{
		if (strcmp(szTool, gaszClaudeEditTools[i]) == 0)
		{
			return true;
		}
	}
	return false;
}


char* CClaudeStream::GetSessionId(void) { return mszSessionId.Text(); }  //"" before a run has started.
CArrayChars* CClaudeStream::GetChangedFiles(void) { return &maszChangedFiles; }  //As Claude named them: full paths.
bool CClaudeStream::IsFinished(void) { return mbFinished; }  //Once the result has been read.
bool CClaudeStream::Succeeded(void) { return mbSucceeded; }

