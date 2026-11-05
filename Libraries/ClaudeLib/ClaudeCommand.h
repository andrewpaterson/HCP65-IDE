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
#ifndef __CLAUDE_COMMAND_H__
#define __CLAUDE_COMMAND_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"
#include "ThreadLib/Process.h"
#include "ClaudeStream.h"


//One request to Claude Code, run headless (claude -p) in a folder, as GitLib runs git: the user's own claude, signed
//in as they are, with their settings and the folder's CLAUDE.md.  What it does arrives as events while it runs (Start
//and Poll), so a window's tick can show them as they happen.
//
//Each request is a run of its own, but the conversation carries on: the session's id is kept from one to the next and
//passed back with --resume, until Forget starts a new conversation.
//
//No one can answer a question from a headless run, so the permission mode says what it may do without asking.  It is
//acceptEdits unless set otherwise: Claude can read and change files in the folder, and a tool it would have had to ask
//to use (such as running a command) is refused, which the finished event lists.  Tools named with SetAllowedTools are
//allowed as well.
//
//The request is passed on the command line, which Windows limits to 32767 characters, so send where things are (a
//file and its lines) rather than their text.
class CClaudeCommand
{
protected:
	CProcess		mcProcess;
	CClaudeStream	mcStream;
	CChars			mszOutput;			//Read since the last Poll.
	CChars			mszErrors;			//Everything the run wrote to its standard error.
	CChars			mszPermissionMode;
	CChars			mszAllowedTools;	//Comma separated, "" for none more.
	CChars			mszModel;			//"" for the user's own choice.
	bool			mbStarted;			//Since Init: whether there is a result.

public:
	void			Init(void);
	void			Kill(void);

	void			SetPermissionMode(const char* szMode);
	void			SetAllowedTools(const char* szTools);
	void			SetModel(const char* szModel);

	bool			Start(const char* szDirectory, const char* szRequest);
	bool			Poll(CArrayClaudeEvents* paEvents);
	void			Terminate(void);
	void			Forget(void);

	bool			IsRunning(void);
	bool			IsFinished(void);
	bool			Succeeded(void);
	int				GetExitCode(void);
	char*			GetSessionId(void);
	void			SetSessionId(const char* szSessionId);
	CArrayChars*	GetChangedFiles(void);
	CChars*			GetErrors(void);

	void			GetArguments(const char* szRequest, CArrayChars* paszArguments);
};


void SetClaudeExecutable(const char* szExecutable);
char* GetClaudeExecutable(void);


#endif // __CLAUDE_COMMAND_H__

