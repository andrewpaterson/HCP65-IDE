/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_INLINE_VARIABLE_H__
#define __IDE_INLINE_VARIABLE_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayInt.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;


//What Inline Variable found for the local variable at the caret: its one value, where it is used, and what goes with
//it.  Positions are in its file's editor's text as it is.
struct SIDEInlining
{
	bool		bPossible;
	CChars		szWhyNot;
	CChars		szFileName;
	CChars		szKey;
	CChars		szName;
	CChars		szValue;			//Its value, as it is written.
	CArrayInt	aiUseStarts;		//Each place it is read: its name's start,
	CArrayInt	aiUseEnds;			//its end,
	CArrayInt	aiUseWrapped;		//and whether the value needs parentheses there.
	size		uiDeclaration;		//Its declaration's statement,
	size		uiDeclarationEnd;
	bool		bAssignment;		//and, where the declaration doesn't give its value, the statement that assigns it.
	size		uiAssignment;
	size		uiAssignmentEnd;
	CArrayChars	aszConflicts;		//What stands in the way, a line each: shown before it is done.

	void	Init(void);
	void	Kill(void);
};


//IntelliJ's Inline Variable (Ctrl+Alt+N; Refactorings.txt, 6): a function's own variable that has one value is taken
//out, and its value written where it was used.  Its value is what its declaration gives it ("int iCount = Count();")
//or, as the house style declares locals at the top of a function, what the one statement that assigns it gives it
//("iCount = Count();"); the declaration and that statement go.  The value gets parentheses where the expression it is
//written into would otherwise take it apart ("iSum * 2" for an iSum of "iA + iB").
//
//Refused: a variable that is assigned more than once, or changed some other way (++, +=, its address taken, bound
//to a reference that isn't const, a function that isn't const called on it: the compiler says which,
//CCPPSemanticPass::FindChanges; in a template a call through one of its parameters counts unless every instance's
//function is const); one declared with others in one declaration; one used before its value, or assigned in a block
//its uses are outside.
//
//What stands in the way is shown first (Problems Detected): a value that does something (a call, an assignment) and
//would be done more than once, or not at all; a variable the value reads that is changed between the value and a
//use of it.
class CIDEInlineVariable : public CObject
{
CONSTRUCTABLE(CIDEInlineVariable);
DESTRUCTABLE(CIDEInlineVariable);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	bool	CanInline(void);
	void	QueueInline(void);
	bool	ShowInline(void);
	bool	FindInlining(SIDEInlining* psDest);
	bool	Inline(SIDEInlining* psInlining, CChars* pszStatus = NULL);
	char*	PrintInlining(SIDEInlining* psInlining, CChars* pszDest);
	bool	IsWrapped(CCPPSyntax* pcValue, CCPPSyntax* pcUse);

protected:
	void	ShowQueuedInline(void);
	bool	Refuse(SIDEInlining* psDest, const char* szWhyNot, int32 iLine = -1);
	void	FindValueConflicts(CIDERefactorUnit* pcUnit, SCPPSyntaxRange* psValue, int32 iLastUseLine, size uiUses, bool bEffects, SIDEInlining* psDest);
};


#endif // __IDE_INLINE_VARIABLE_H__

