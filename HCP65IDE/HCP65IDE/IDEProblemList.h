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
#ifndef __IDE_PROBLEM_LIST_H__
#define __IDE_PROBLEM_LIST_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "CppCompilerLib/CPPErrors.h"


//An error or warning a build of a file would give, where it is in the file.
struct SIDEProblem
{
	int32	iLine;  //From 0.
	int32	iColumn;  //From 0.
	bool	bWarning;
	CChars	szMessage;  //Without its file and place.
};


typedef CArrayTemplate<SIDEProblem>	CArrayIDEProblems;


//One file's problems (the Problems pane's Current File, CIDEProblems): what the code model's check of it found, the
//preprocessor's, the parser's and the check's, errors first, then warnings, each in the order they are in the file.
//Only those in the file: an error in a header it includes is the header's.
class CIDEProblemList
{
protected:
	CArrayIDEProblems	masProblems;

public:
	void			Init(void);
	void			Kill(void);
	void			Clear(void);
	void			TakeFrom(CIDEProblemList* pcSource);

	void			Add(int32 iLine, int32 iColumn, bool bWarning, const char* szMessage);
	void			AddErrors(CCPPErrors* pcErrors, const char* szFileName, bool bWarning);
	void			AddMessages(const char* szMessages, const char* szFileName, bool bWarning);
	void			Sort(void);

	size			NumProblems(void) const;
	size			NumErrors(void);
	size			NumWarnings(void);
	SIDEProblem*	Get(size uiIndex);

	static bool		IsSameFile(const char* szFileName, const char* szOther);
};


#endif // __IDE_PROBLEM_LIST_H__

