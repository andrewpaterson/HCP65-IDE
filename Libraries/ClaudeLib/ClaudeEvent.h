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
#ifndef __CLAUDE_EVENT_H__
#define __CLAUDE_EVENT_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"


enum EClaudeEventType
{
	CET_Started,		//The session started.  Detail: the model.
	CET_Text,			//Claude wrote Text.
	CET_ToolUse,		//Claude used Tool.  Detail: what on (a file, pattern or command); File: the file it changes, if any.
	CET_ToolResult,		//A tool's result.  Text: what it gave back; Error if it failed or was refused.
	CET_Finished,		//Claude finished.  Text: its answer; Error if it failed; Detail: the cost in US dollars; Denied: the tools it was refused.
	CET_Output,			//Text: a line that wasn't Claude's JSON, such as a message from the program itself.
};


//One thing that happened in a run of Claude Code, read from its stream-json output.  A line of output can be more
//than one event: a message from Claude with text and two tool uses is three.
class CClaudeEvent
{
protected:
	EClaudeEventType	meType;
	CChars				mszText;
	CChars				mszTool;
	CChars				mszDetail;
	CChars				mszFile;
	CChars				mszDenied;
	bool				mbError;

public:
	void				Init(EClaudeEventType eType);
	void				Kill(void);

	void				SetText(const char* szText);
	void				SetTool(const char* szTool);
	void				SetDetail(const char* szDetail);
	void				SetFile(const char* szFile);
	void				AddDenied(const char* szTool);
	void				SetError(bool bError);

	EClaudeEventType	GetType(void);
	char*				GetText(void);
	char*				GetTool(void);
	char*				GetDetail(void);
	char*				GetFile(void);
	char*				GetDenied(void);
	bool				IsError(void);
};


//Events in the order they happened.  Each is killed with the array.
class CArrayClaudeEvents
{
protected:
	CArrayTemplate<CClaudeEvent>	maEvents;

public:
	void			Init(void);
	void			Kill(void);
	void			Clear(void);

	CClaudeEvent*	Add(EClaudeEventType eType);
	CClaudeEvent*	Get(size uiIndex);
	size			NumElements(void);
};


#endif // __CLAUDE_EVENT_H__

