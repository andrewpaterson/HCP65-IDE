/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela GitLib

Codaphela GitLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela GitLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela GitLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __GIT_COMMAND_H__
#define __GIT_COMMAND_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"
#include "ThreadLib/Process.h"


//One run of the git program in a repository, as IntelliJ IDEA runs it: the command line, not a library, so the user's
//git and its settings (credentials, hooks, attributes) are used.  Its output is kept byte for byte (a file's contents,
//or a list of names split by '\0's) and its errors apart.  It runs without waiting (Start and Poll), so a window's
//tick can carry on, or is waited for (Run).
//
//git is run with options that keep its output the same whoever runs it: it never pages or colours, writes file names
//as they are, and takes no optional locks, so looking at a repository doesn't get in the way of the user's own git
//commands.  Its input is empty; commands that might ask for a password (fetch, push) aren't run yet.
class CGitCommand
{
protected:
	CProcess	mcProcess;
	CChars		mszOutput;
	CChars		mszErrors;
	bool		mbStarted;	//Since Init: whether there is a result.

public:
	void	Init(void);
	void	Kill(void);

	bool	Start(const char* szRepository, CArrayChars* paszArguments);
	bool	Poll(void);
	bool	Run(const char* szRepository, CArrayChars* paszArguments);
	void	Terminate(void);

	bool	IsRunning(void);
	bool	Succeeded(void);
	int		GetExitCode(void);
	CChars*	GetOutput(void);
	CChars*	GetErrors(void);
};


void SetGitExecutable(const char* szExecutable);
char* GetGitExecutable(void);


#endif // __GIT_COMMAND_H__

