/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __COMMAND_BUILD_H__
#define __COMMAND_BUILD_H__
#include "ThreadLib/Process.h"
#include "Build.h"


//A program a build runs, with what to call it while it runs.
struct SBuildCommand
{
	CChars		szExecutable;
	CArrayChars	aszArguments;
	CChars		szDirectory;
	CChars		szDescription;
};


typedef CArrayTemplate<SBuildCommand>	CArrayBuildCommands;


//A build that runs programs (compilers, linkers) one after another, each in its own process, until one fails (exits
//with other than 0).  What they write is the build's output.  A subclass says what to run (AddCommands).
class CCommandBuild : public CBuild
{
protected:
	CArrayBuildCommands	masCommands;
	size				muiCommand;		//The one running.
	CProcess			mcProcess;

public:
			void			Init(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot) override;
			void			Kill(void) override;

			bool			Start(CArrayChars* paszLines) override;
			bool			Poll(CArrayChars* paszLines) override;
			void			Cancel(void) override;

			SBuildCommand*	AddCommand(const char* szExecutable, const char* szDirectory, const char* szDescription);
			size			NumCommands(void);
			SBuildCommand*	GetCommand(size uiIndex);

protected:
	virtual	bool			AddCommands(CArrayChars* paszLines) =0;
			bool			StartCommand(CArrayChars* paszLines);
			void			ClearCommands(void);
};


#endif // __COMMAND_BUILD_H__

