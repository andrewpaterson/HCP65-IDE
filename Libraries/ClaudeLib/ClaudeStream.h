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
#ifndef __CLAUDE_STREAM_H__
#define __CLAUDE_STREAM_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"
#include "JSONDocument.h"
#include "ClaudeEvent.h"


//Reads Claude Code's --output-format stream-json output, a JSON object a line, as it arrives in pieces, into events.
//It keeps what a run says about itself: its session's id (to carry the conversation on with --resume), whether it has
//finished and how, and the files it changed, so they can be read again.
//
//A file counts as changed once a tool that changes files (Edit, Write, MultiEdit, NotebookEdit) has used it without
//an error: a tool use that was refused or failed changes nothing.
class CClaudeStream
{
protected:
	CJSONDocument	mcDocument;
	CChars			mszPartial;			//Output after the last line end.
	CChars			mszSessionId;
	CArrayChars		maszChangedFiles;	//In the order they were first changed.
	CArrayChars		maszPendingIds;		//Uses of tools that change files, not yet answered.
	CArrayChars		maszPendingFiles;	//And the files they change.
	bool			mbFinished;
	bool			mbSucceeded;

public:
	void			Init(void);
	void			Kill(void);
	void			Restart(void);

	void			Append(const char* szOutput, size uiLength, CArrayClaudeEvents* paEvents);
	void			End(CArrayClaudeEvents* paEvents);
	void			ParseLine(const char* szLine, size uiLength, CArrayClaudeEvents* paEvents);

	char*			GetSessionId(void);
	void			SetSessionId(const char* szSessionId);
	CArrayChars*	GetChangedFiles(void);
	bool			IsFinished(void);
	bool			Succeeded(void);

protected:
	void			ParseInit(size uiRoot, CArrayClaudeEvents* paEvents);
	void			ParseAssistant(size uiRoot, CArrayClaudeEvents* paEvents);
	void			ParseUser(size uiRoot, CArrayClaudeEvents* paEvents);
	void			ParseResult(size uiRoot, CArrayClaudeEvents* paEvents);
	void			ParseToolUse(size uiBlock, CArrayClaudeEvents* paEvents);
	void			ParseToolResult(size uiBlock, CArrayClaudeEvents* paEvents);
	void			GetContentText(size uiContent, CChars* pszDest);
	void			ToolAnswered(const char* szId, bool bError);
	void			KeepSessionId(size uiRoot);
};


bool IsClaudeEditTool(const char* szTool);


#endif // __CLAUDE_STREAM_H__

