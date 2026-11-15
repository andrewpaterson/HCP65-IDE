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
#ifndef __IDE_INLINE_FUNCTION_H__
#define __IDE_INLINE_FUNCTION_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEChangeSignature.h"


class CIDEContent;


//Which calls are inlined.
#define IDE_INLINE_ALL_AND_DELETE	0	//Every call, and the function taken out.
#define IDE_INLINE_ALL				1	//Every call; the function stays.
#define IDE_INLINE_THIS				2	//The call at the caret.

//What a piece of the function's body becomes where it is inlined.
#define IDE_INLINE_PARAMETER	0	//A parameter's name: the call's argument.
#define IDE_INLINE_MEMBER		1	//A member named alone: the call's object before it ("cTank." or "pcTank->").
#define IDE_INLINE_THIS_WORD	2	//this: the call's object.
#define IDE_INLINE_SCOPED		3	//A type or constant of the class named alone: the class's name before it.
#define IDE_INLINE_TEMPLATE		4	//A class template's parameter: the instance's argument at the call.

//Where a call is written.
#define IDE_CALL_IN_EXPRESSION	0	//Part of an expression.
#define IDE_CALL_STATEMENT		1	//A statement by itself: "Fill(3);".
#define IDE_CALL_ASSIGNED		2	//All of what a statement assigns: "x = Level();".
#define IDE_CALL_RETURNED		3	//All of what a statement returns: "return Level();".


//A place in the function's body that is written differently where the body is inlined.
struct SIDEInlineReplace
{
	size	uiStart;	//In the definition's file's text.
	size	uiEnd;
	int		iKind;		//IDE_INLINE_.
	int		iIndex;		//Which parameter, or template parameter.
	bool	bOperand;	//It is an operand there: an argument of more than one thing goes in parentheses.
};


typedef CArrayTemplate<SIDEInlineReplace>	CArrayIDEInlineReplaces;


//A call of the function.
struct SIDEInlineCall
{
	CChars		szFileName;
	int32		iLine;				//Of the function's name in it, from 0.
	size		uiStart;			//The call, from its object to its ), in its file's text.
	size		uiEnd;
	CArrayChars	aszArguments;		//As they are written.
	CArrayInt	aiEffects;			//Whether each does something.
	CChars		szObject;			//What it is called on, as it goes before a member: "cTank.", "pcTank->"; empty for none or this.
	CChars		szThis;				//...and as it stands for this: "&cTank", "pcTank"; "this" for none.
	bool		bObjectEffects;		//What it is called on does something (a call), so can't be written more than once.
	bool		bOutside;			//It isn't in a function of the class or of one that derives from it.
	int			iContext;			//IDE_CALL_.
	size		uiStatement;		//The statement it is in: its line's start,
	size		uiStatementEnd;		//and its end.
	CChars		szIndent;			//The statement's indentation.
	CChars		szAssigned;			//IDE_CALL_ASSIGNED: what is assigned.
	bool		bWrapped;			//The function's returned expression needs parentheses here.
	size		uiDeclarations;		//Where the function's own variables' declarations go in the function the call is in,
	bool		bDeclarations;		//which has declarations there already.
	CArrayChars	aszNames;			//The names the function the call is in writes.
	CArrayChars	aszInstance;		//The call's object's class template's arguments, if they are needed and known.
	bool		bSelected;			//It is the call at the caret.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplatePtr<SIDEInlineCall>	CArrayIDEInlineCalls;


//What Inline Function found for the function at the caret.
struct SIDEFunctionInlining
{
	bool					bPossible;
	CChars					szWhyNot;
	SIDESignature			sSignature;			//Its declarations, definition and calls (Change Signature's).
	CChars					szFileName;			//The file its definition is in.
	CChars					szClassKey;			//Its class, empty if it has none.
	CChars					szClassName;		//...as it is written before a type or constant of it.
	bool					bExpression;		//Its body is "return <expression>;": it can go wherever a call is.
	size					uiValue;			//What it returns: the expression of its last statement's return,
	size					uiValueEnd;			//in the definition's file's text; empty (both 0) if it returns nothing.
	bool					bValueEffects;		//...which does something.
	size					uiStatements;		//Its statements before that return, from the first's line to the last's end,
	size					uiStatementsEnd;	//after its own variables' declarations; empty if there are none.
	CChars					szBodyIndent;		//Their indentation.
	CArrayChars				aszDeclarations;	//Its own variables' declarations, each as its line without indentation,
	CArrayChars				aszDeclared;		//and their names.
	CArrayIDEInlineReplaces	asReplaces;			//In the order they are written.
	CArrayInt				aiParameterUses;	//How many times the body names each parameter.
	CArrayChars				aszTemplateParameters;	//Its class template's parameters' names, if its body names any.
	CChars					szNotPublic;		//A member its body names that isn't public (the first), empty if none.
	bool					bNamesMembers;
	CArrayIDEInlineCalls	apsCalls;
	bool					bSelectedCall;		//The caret is on a call of it.

	void	Init(void);
	void	Kill(void);
};


//IntelliJ's Inline Method (Ctrl+Alt+N on a function, the same key as Inline Variable; Refactorings.txt, 10): the
//function's body is written where it is called, at every call or at the one at the caret, and the function taken
//out if asked.
//
//A function whose body is "return <expression>;" goes wherever a call of it is: the call becomes the expression, in
//parentheses where what it is written into would take it apart.  One with statements goes only where its call is a
//statement by itself, or all of what a statement assigns or returns: the statements are written there, at that
//statement's indentation, and what it returned is assigned, returned, or dropped.  Its own variables' declarations
//go with the declarations of the function it is inlined into.
//
//In the body, each parameter becomes the call's argument (in parentheses where it is more than one thing and is an
//operand); a member named alone gets the call's object before it ("cTank.miLevel", "pcTank->Fill()"), this becomes
//the object, and a type or constant of the class gets the class's name.  A class template's member inlined where the
//call isn't in the template has the template's parameters replaced by the instance's arguments there, which the
//compiler gives from the object's type (T3).
//
//Refused: a virtual function, a recursive one, one that returns part of the way through, one that changes a
//parameter, a constructor, destructor or operator, a function template.  Shown first as Problems Detected: a call
//that is part of an expression where the function has statements (it stays a call, and the function isn't taken
//out); an argument that does something and would be written more than once, or not at all; a member that isn't
//public named where the call is outside the class; one of its variables' names that the function it goes into uses;
//and, where it is to be taken out, a place it is named without being called.
class CIDEInlineFunction : public CObject
{
CONSTRUCTABLE(CIDEInlineFunction);
DESTRUCTABLE(CIDEInlineFunction);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpDialog;  //While it is shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	ShowInline(void);
	bool	FindInlining(SIDEFunctionInlining* psDest);
	void	FindConflicts(SIDEFunctionInlining* psInlining, int iMode, CArrayChars* paszDest);
	bool	Inline(SIDEFunctionInlining* psInlining, int iMode, CChars* pszStatus = NULL);
	char*	PrintInlining(SIDEFunctionInlining* psInlining, CChars* pszDest);

protected:
	bool	Refuse(SIDEFunctionInlining* psDest, const char* szWhyNot);
	bool	ReadBody(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, const char* szText, SIDEFunctionInlining* psDest, CCPPSyntax** ppcValue);
	void	ReadReplaces(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, const char* szText, SIDEFunctionInlining* psDest);
	void	AddReplace(SIDEFunctionInlining* psDest, size uiStart, size uiEnd, int iKind, int iIndex, bool bOperand);
	bool	ReadCall(SIDESignatureCall* psCall, CCPPSyntax* pcValue, SIDEFunctionInlining* psDest, int iCaretLine, const char* szCaretFile);
	bool	IsInlined(SIDEFunctionInlining* psInlining, SIDEInlineCall* psCall, int iMode);
	void	AppendBody(SIDEFunctionInlining* psInlining, SIDEInlineCall* psCall, const char* szText, size uiStart, size uiEnd, CChars* pszDest);
	void	AppendPlace(SIDEInlineCall* psCall, CChars* pszDest);
};


#endif // __IDE_INLINE_FUNCTION_H__

