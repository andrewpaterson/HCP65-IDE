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
#ifndef __IDE_CONVERT_MACRO_H__
#define __IDE_CONVERT_MACRO_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
struct SIDECaretContext;


//Where a macro's name is written, other than in its own #define.
#define IDE_MACRO_USE_CODE			0	//In code: the preprocessor replaces it there.
#define IDE_MACRO_USE_DIRECTIVE		1	//On an #if, #ifdef, #ifndef or #elif line: only a macro can be there.
#define IDE_MACRO_USE_DEFINE		2	//In another macro's body: replaced wherever that one is used.
#define IDE_MACRO_USE_NAME			3	//A function-like macro's name with no ( after it: not a use of it.


struct SIDEMacroUse
{
	CChars		szFileName;
	size		uiStart;		//Its name, in its file's text,
	size		uiEnd;			//and the end of what the preprocessor replaces: its name, or its arguments' ).
	int32		iLine;			//From 0.
	int32		iColumn;
	int			iKind;			//IDE_MACRO_USE_.
	CArrayChars	aszArguments;	//As written, trimmed.
	CArrayInt	aiArgumentStarts;	//Where each is in the text,
	CArrayInt	aiArgumentEnds;

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDEMacroUse>	CArrayIDEMacroUses;


//One #define of a run that becomes an enum.
struct SIDEMacroConstant
{
	CChars	szName;
	CChars	szValue;
	CChars	szComment;
	size	uiStart;
	size	uiEnd;
};


typedef CArrayTemplate<SIDEMacroConstant>	CArrayIDEMacroConstants;


//The macro at the caret: its #define, and every place its name is written in the solution's files.
struct SIDEMacro
{
	CChars				szName;
	bool				bFunction;		//It has parameters,
	bool				bVariadic;		//the last of them "...".
	CArrayChars			aszParameters;
	CChars				szBody;			//What it is replaced by, on one line.
	CChars				szComment;		//The // comment after it, if any.
	CChars				szFileName;		//Its #define's file,
	size				uiStart;		//and its lines there, to the start of the line after.
	size				uiEnd;
	int32				iLine;			//From 0.
	size				uiDefines;		//The #defines of its name: more than one, and nothing is done to it.
	size				uiUndefs;		//The #undefs of it.
	CArrayIDEMacroUses	asUses;
	int					iCaretUse;		//The use the caret is on, -1 if it is on the #define.

	bool				bPossible;		//Convert Macro: whether it can be,
	CChars				szWhyNot;
	CChars				szConverted;	//what its #define becomes,
	CArrayChars			aszConflicts;	//and what would change with it.
	CArrayIDEMacroConstants	asRun;		//The #defines of whole numbers it stands among (itself too), for an enum of them.

	void	Init(void);
	void	Kill(void);
};


//What Inline Macro replaces.
#define IDE_MACRO_INLINE_ALL	0	//Every use, and the #define is taken out.
#define IDE_MACRO_INLINE_THIS	1	//The use at the caret.


//Macros made into what the compiler knows (Refactorings.txt, 16), for the macro whose name the caret is on, at its
//#define or at a use.  A macro isn't in the index: its #define and its uses are found by its name in the solution's
//files' texts, as the preprocessor would (not in comments or strings), and nothing is done to a name #defined
//more than once.
//  - Convert Macro, a #define of a value: it becomes a constant, "const int MAX_UNITS = 8;", of the type the
//    compiler gives its value (a kept check of the #define's file with the value written as an expression); a
//    string becomes a const char array.  Or, a #define of a whole number among others: all of that run of #defines
//    become one enum's enumerators.
//  - Convert Macro, a macro with parameters: it becomes an inline function of the same name.  Its parameters' types
//    and its result's are those the compiler gives its arguments and what it makes at each use (a kept check of
//    each file that uses it); where they differ between uses it becomes a function template, the parameters whose
//    types change together sharing a template parameter.  An argument the macro's body has more than once, or not
//    at all, is worked out once by a function: a use where that argument does something (a call, a ++) is shown
//    first as a conflict.
//  - Inline Macro (Inline..., Ctrl+Alt+N, on a macro's name): the use at the caret, or every use, is replaced by
//    what the preprocessor would make of it (its arguments put in its body, # and ## done), and with every use the
//    #define is taken out.
//Refused: a macro an #if, #ifdef or #undef names (only a macro can be there), one C code uses, one whose body has #
//or ##, changes one of its arguments or names something that isn't declared where the macro is (the files are
//checked as they would be, and must have no more errors than they had).
//
//Classes a macro makes from a text included more than once (as ARRAY_BLOCK made BaseLib's arrays) are T6's, and not
//this; BaseLib's are real templates now (CommonLibraries' ArrayTemplatesPlan.txt).
class CIDEConvertMacro : public CObject
{
CONSTRUCTABLE(CIDEConvertMacro);
DESTRUCTABLE(CIDEConvertMacro);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>	mpDialog;  //While it is shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanConvert(void);
	void	QueueConvert(void);
	bool	ShowConvert(void);
	bool	ShowInline(bool* pbMacro);
	bool	FindMacro(SIDEMacro* psDest);
	void	FindConversion(SIDEMacro* psMacro);
	bool	Convert(SIDEMacro* psMacro, CChars* pszStatus = NULL);
	const char*	GetEnumProblem(SIDEMacro* psMacro, const char* szEnumName);
	bool	ConvertToEnum(SIDEMacro* psMacro, const char* szEnumName, CChars* pszStatus = NULL);
	const char*	GetInlineProblem(SIDEMacro* psMacro, int iMode);
	bool	Inline(SIDEMacro* psMacro, int iMode, CChars* pszStatus = NULL);
	char*	PrintMacro(SIDEMacro* psMacro, CChars* pszDest);

protected:
	void	ShowQueuedConvert(void);
	bool	Refuse(SIDEMacro* psMacro, const char* szWhyNot, SIDEMacroUse* psUse = NULL);
	bool	FindWordAtCaret(CChars* pszWord, size* puiStart);
	void	ReadDefine(const char* szText, size uiLine, size uiName, SIDEMacro* psDest);
	void	FindUses(const char* szName, SIDEMacro* psDest, size* puiDefines);
	bool	IsUsedAsMacroOnly(SIDEMacro* psMacro);
	void	FindConstantConversion(SIDEMacro* psMacro);
	void	FindFunctionConversion(SIDEMacro* psMacro);
	void	FindRun(SIDEMacro* psMacro);
	bool	AppendValueType(CIDERefactorUnit* pcUnit, CCPPSyntax* pcExpression, bool bParameter, CChars* pszDest);
	size	CountErrors(SIDEMacro* psMacro, char* szNewText);
	bool	CheckConverted(SIDEMacro* psMacro, size uiStart, size uiEnd, const char* szConverted);
	void	AppendExpansion(SIDEMacro* psMacro, CArrayChars* paszArguments, CChars* pszDest);
	void	AppendExpanded(SIDEMacro* psMacro, const char* szText, CChars* pszDest);
	void	ShowMessage(CChars* pszMessage, CChars* pszStatus);
};


#endif // __IDE_CONVERT_MACRO_H__
