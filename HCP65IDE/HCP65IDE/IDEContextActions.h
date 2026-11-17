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
#ifndef __IDE_CONTEXT_ACTIONS_H__
#define __IDE_CONTEXT_ACTIONS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;


//What is at the active editor's caret (or selected), found once for every refactoring to say whether it is for it
//(each one's AddContextActions): from the index, what symbol is named there and how it is declared; from the file's
//syntax, what the caret is in and what the selection is.  All of it is cheap: no refactoring's own search is made, so
//one offered may still say, once it has looked, why it can't be done.
struct SIDECaretContext
{
	CChars		szFileName;
	bool		bCode;					//A C or C++ file: nothing else is found for any other.
	int			iLine;					//The caret, or the selection's start,
	int			iColumn;
	int			iEndLine;				//and the selection's end.
	int			iEndColumn;
	bool		bSelection;
	bool		bInclude;				//The caret's line is an #include.

	bool		bSymbol;				//A symbol of the index is named at the caret:
	CChars		szKey;
	uint8		uiKind;					//ECPPSymbolKind,
	uint8		uiRole;					//ECPPIndexRole: declared, defined or used there.
	bool		bFunction;				//A function or a function template,
	bool		bMember;				//of a class (a field is too).
	CChars		szClassKey;				//The class named there, or the class of the member declared there; else empty.
	bool		bClass;					//The class itself is named.
	size		uiBases;				//How many classes of the solution's it derives from,
	size		uiDerived;				//and how many derive from it.
	bool		bDeclared;				//A function: it is declared without a body,
	bool		bDefined;				//it is defined,
	bool		bHeaderDefined;			//in a header;
	bool		bStatic;
	bool		bVirtual;
	bool		bConst;
	bool		bSpecial;				//it is a constructor or a destructor;
	bool		bParameters;			//it has parameters.
	bool		bPointer;				//A variable, parameter or field: it is declared a pointer,
	bool		bReference;				//or a reference.

	bool		bParsed;				//The file was parsed: what follows is from its syntax.
	bool		bMacro;					//The caret is on a macro's name, where it is used or #defined.
	bool		bInFunction;			//The caret, or all of the selection, is in a function's body,
	bool		bInMemberFunction;		//a member function's.
	bool		bUndeclaredFunction;	//The caret is on the name of a member function defined that its class doesn't declare.
	bool		bExpression;			//The selection is an expression; or the caret is on a number or a string.
	bool		bStatements;			//The selection is whole statements.
	bool		bTypeSelected;			//The selection is in a type as a declaration writes it.
	bool		bType;					//The caret is on a type where it is used: a class, an enum, a typedef or one of the language's.

	CArrayChars	aszCommands;			//The commands offered, in the order they are shown.

	void	Init(void);
	void	Kill(void);
	void	Offer(const char* szCommand);
};


//Show Context Actions (Alt+Enter, as IntelliJ's): the refactorings that are for what the caret is on, or what is
//selected, in a popup under the caret; the one chosen is run as its own command is.  Each refactoring says whether it
//is offered (its AddContextActions) from what was found at the caret (SIDECaretContext): Make Member Function static
//on a member function that isn't static or virtual, Convert Pointer to Reference on a variable that is a pointer,
//Extract Method on statements selected in a function's body, and so on; Rename where there is anything to rename.
//Where there is nothing, the status line says so.
class CIDEContextActions : public CObject
{
CONSTRUCTABLE(CIDEContextActions);
DESTRUCTABLE(CIDEContextActions);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CMenuContainer>	mpPopup;  //Made when it is first shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				AddCommands(Ptr<CCommandSet> pCommands);
	bool				CanShow(void);
	void				QueueShow(void);
	bool				Show(void);
	bool				FindContext(SIDECaretContext* psDest);
	size				FindActions(SIDECaretContext* psDest);
	Ptr<CMenuContainer>	GetPopup(void) const;

protected:
	void	ShowQueued(void);
	void	FindSymbol(SIDECaretContext* psDest);
	void	FindSyntax(SIDECaretContext* psDest, const char* szText, size uiCaret);
	bool	IsStatement(CCPPSyntax* pcNode);
	bool	IsTypeWord(CIDERefactorUnit* pcUnit, int32 iToken);
};


#endif // __IDE_CONTEXT_ACTIONS_H__
