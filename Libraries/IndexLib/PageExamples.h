/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela IndexLib

Codaphela IndexLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela IndexLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela IndexLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __PAGE_EXAMPLES_H__
#define __PAGE_EXAMPLES_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "CppCompilerLib/CPPErrors.h"


//A ```cpp block of a page: an example that compiles (HCP65/Website/DocsPlan.txt F1).
struct SPageExample
{
	int		iFence;		//0 based: the line of its opening fence.
	int		iLines;		//Of code, between the fences.
	bool	bDeclares;	//Compiled at file scope, else as the body of a function.
	CChars	szCode;		//Its lines, each with its '\n'.
};


typedef CArrayTemplate<SPageExample>	CArrayPageExamples;


//What a check of an example found, at its place in the page.
struct SPageExampleProblem
{
	int		iLine;		//0 based, in the page.
	int		iColumn;	//0 based.
	CChars	szMessage;
};


typedef CArrayTemplate<SPageExampleProblem>	CArrayPageExampleProblems;


class CWorkspaceIndex;
//A page's ```cpp blocks and the C++ its hidden comment puts before each: "<!-- cpp" on a line of its own, then its
//#includes and the declarations of the names its examples use without declaring, then "-->".  A ```cpp block is
//checked; a ```{.cpp .no-check} block (or any other) isn't.  Each is compiled as a unit of the project the page is
//an item of, as its build configuration compiles it, and checked as the IDE checks an edited file: the hidden
//lines, then the block, at file scope if it declares (a function written out, a class...) or else as the body of a
//function.  Its problems are given at their lines in the page.
class CPageExamples
{
protected:
	CChars					mszHidden;		//The hidden comment's lines, each with its '\n'.
	int						miHidden;		//0 based: the page line of its first, -1 with no hidden comment.
	int						miHiddenLines;
	CArrayPageExamples		masExamples;

public:
	void					Init(void);
	void					Kill(void);

	void					Scan(const char* szPage);
	size					NumExamples(void);
	SPageExample*			Get(size uiIndex);
	bool					HasHidden(void);
	const char*				GetHidden(void);

	void					MakeUnit(size uiIndex, CChars* pszDest);
	int						GetPageLine(size uiIndex, int iUnitLine);
	void					GetUnitFile(const char* szPageFile, size uiIndex, CChars* pszDest);
	bool					Check(CWorkspaceIndex* pcIndex, size uiProject, const char* szPageFile, CArrayPageExampleProblems* pasDest);

	static bool				IsDeclaring(const char* szCode);
	static bool				IsCheckedFence(const char* szInfo, int iLength);
	static void				KillProblems(CArrayPageExampleProblems* pasProblems);
	static void				PrintProblems(const char* szPageFile, CArrayPageExampleProblems* pasProblems, CChars* pszDest);

protected:
	void					AddProblem(CArrayPageExampleProblems* pasDest, int iLine, int iColumn, const char* szMessage);
	void					AddErrors(CArrayPageExampleProblems* pasDest, CCPPErrors* pcErrors, size uiIndex, const char* szUnitFile);
	void					AddMessages(CArrayPageExampleProblems* pasDest, const char* szMessages, size uiIndex, const char* szUnitFile);
};


#endif // __PAGE_EXAMPLES_H__

