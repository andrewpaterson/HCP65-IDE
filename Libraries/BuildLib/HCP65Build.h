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
#ifndef __H_C_P_65_BUILD_H__
#define __H_C_P_65_BUILD_H__
#include "BaseLib/NativeThread.h"
#include "ThreadLib/WorkerThreadJobs.h"
#include "CppParserLib/CPP65Builder.h"
#include "Build.h"


//Compiles the solution's projects for the W65C816 or the 65C64 with the HCP65 compiler (CCPP65Builder), on a thread of
//its own with the units compiled on the jobs' threads, each unit's listing written with its source lines (.line) in
//Listing\<toolchain>\<configuration>.  <stdint.h> and the other system headers are Libc65's.  The units are compiled,
//not linked: linking a program needs Libc65's runtime too.  A build can't be stopped part way; cancelling it says so
//once the compile it was in has finished.
class CHCP65Build : public CBuild
{
protected:
	CChars					mszLibc65;
	CCPP65Builder			mcBuilder;
	CWorkerThreadJobs		mcJobs;
	CNativeThread			mcThread;
	volatile bool			mbCompiled;		//Set by the thread when it has finished.
	bool					mbResult;
	bool					mbCancel;

public:
	void		Init(CVcxSolution* pcSolution, CBuildConfiguration* pcConfiguration, const char* szRoot) override;
	void		Kill(void) override;

	void		SetLibc65Directory(const char* szDirectory);

	bool		Start(CArrayChars* paszLines) override;
	bool		Poll(CArrayChars* paszLines) override;
	void		Cancel(void) override;

protected:
	static void	CompileThread(void* pvBuild);
	void		AddProjects(CArrayChars* paszLines);
	void		AddLines(const char* szText, CArrayChars* paszLines);
	void		Wait(void);
};


#endif // __H_C_P_65_BUILD_H__

