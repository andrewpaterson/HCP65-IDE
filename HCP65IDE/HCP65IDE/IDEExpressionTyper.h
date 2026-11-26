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
#ifndef __IDE_EXPRESSION_TYPER_H__
#define __IDE_EXPRESSION_TYPER_H__
#include "StandardLib/Pointer.h"
#include "IDECodeModel.h"


class CIDEContent;
class CIDEEditorTab;


//What an expression of a file is, asked of a checked parse (CIDEExpressionTyper): the file's text before the
//expression, the expression, and the text after what is being written after it, which are copies, as the editor goes
//on being typed in while the edits thread parses; whether "->" follows it; and, for a header, the unit it is parsed
//in with that unit's text, and the header's own text to give the code model back.  Then the answer: the key of the
//class or enum whose members "." or "->" after the expression reaches, empty if it has none.
struct SIDETypeQuestion
{
	CIDECodeModel*	pcModel;
	size			uiNumber;
	CChars			szFileName;
	CChars			szBefore;
	CChars			szExpression;
	CChars			szAfter;
	bool			bArrow;
	bool			bHeader;
	CChars			szHeaderText;
	CChars			szUnit;
	CChars			szUnitText;
	bool			bUnitText;
	size			uiJob;		//The edits thread's number for it, 0 until it is taken.
	CChars			szKey;

	void	Init(CIDECodeModel* pcInModel);
	void	Kill(void);
};


//Works out what the expression before a "." or "->" is with the compiler's own type checker, for what the light
//typer can't say (an operator a class defines, a template's parameter, a function whose overloads return different
//things): NextIDEWork.txt's item 5, the kept parse.  The file is parsed and checked on the edits thread
//(CIDECodeModel::RunEditsJob) with the expression put in brackets in place of what is being typed after it, so that
//it is a whole expression whatever follows the caret; the brackets' type is the answer.  If "->" follows and the
//expression is a class, not a pointer, it is parsed again as a call of the class's operator->, whose type is.
//
//Ask gives it a question, which waits until the edits thread is free; Poll, each tick, starts it then and says when
//the last question asked has its answer (GetKey).  A question asked before that one's answer came is dropped.  The
//parse takes as long as the file's check does: tens of milliseconds for a small file, a second or two for a large.
class CIDEExpressionTyper
{
protected:
	CIDEContent*		mpcContent;  //Not held.
	SIDETypeQuestion*	mpsWaiting;  //Asked, not yet taken by the edits thread.
	SIDETypeQuestion*	mpsRunning;  //The edits thread has it.
	SIDETypeQuestion*	mpsAnswered;  //The last asked, answered.
	size				muiAsked;  //How many have been asked: the last one's number.

public:
	void		Init(CIDEContent* pcContent);
	void		Kill(void);

	size		Ask(Ptr<CIDEEditorTab> pTab, size uiStart, size uiEnd, size uiAfter, bool bArrow);
	bool		Poll(void);
	bool		IsAsking(void) const;
	void		Forget(void);
	size		GetAnswered(void) const;
	const char*	GetKey(void) const;

protected:
	static void	Job(void* pvData);
	static bool	TypeGroup(SIDETypeQuestion* psQuestion, const char* szGroup, CChars* pszKey, int32* piLevels);
	void		Free(SIDETypeQuestion** ppsQuestion);
};


#endif // __IDE_EXPRESSION_TYPER_H__

