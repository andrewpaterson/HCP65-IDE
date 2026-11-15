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
#ifndef __IDE_EXTRACT_METHOD_H__
#define __IDE_EXTRACT_METHOD_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEChangeSignature.h"


class CIDEContent;


//What a variable of the function is to the new function.
#define IDE_EXTRACT_NONE		0	//Nothing: the selection doesn't name it.
#define IDE_EXTRACT_VALUE		1	//A parameter: the selection reads what it had.
#define IDE_EXTRACT_REFERENCE	2	//A reference parameter: the selection changes it and it is used afterwards (or it is an object).
#define IDE_EXTRACT_RETURNED	3	//What the new function returns: the one variable the selection changes that is used afterwards.
#define IDE_EXTRACT_MOVED		4	//Its own variable: only the selection uses it, and its declaration goes with it.
#define IDE_EXTRACT_DECLARED	5	//Its own variable too, declared again: the selection gives it a value first, and no one reads that after.


//A variable (or parameter) of the function the selection is in, as the selection uses it (Refactorings.txt, F3: flow
//over the syntax, which says more is used than is where it can't tell).
struct SIDEExtractVariable
{
	CChars	szKey;
	CChars	szName;
	CChars	szBefore;			//Its declaration to its name: "int", "char*", "CChars".
	CChars	szAfter;			//...and after its name: "[64]".
	int32	iDeclaredToken;		//Where it is declared, for their order.
	bool	bObject;			//Its type is a class's, not a pointer to one: it is given by reference, never copied.
	bool	bArray;
	bool	bDeclaredInside;	//The selection declares it.
	bool	bHasValue;			//It can have a value before the selection: a parameter, one its declaration gives a value, or one named before.
	bool	bReadInside;		//The selection reads it before anything in the selection is sure to have given it a value.
	bool	bWrittenInside;
	bool	bUsedInside;
	bool	bUsedBefore;
	bool	bUsedAfter;			//After the selection, or around it in a loop the selection is in.
	bool	bAlone;				//Its declaration declares nothing else, so it can be taken out:
	size	uiDeclaration;		//the declaration's statement, in the file's text.
	size	uiDeclarationEnd;
	int		iRole;				//IDE_EXTRACT_.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDEExtractVariable>	CArrayIDEExtractVariables;


//What Extract Method found for the active editor's selection.  Positions are in the editor's text as it is.
struct SIDEExtraction
{
	bool						bPossible;
	CChars						szWhyNot;
	CChars						szFileName;
	bool						bExpression;		//An expression is selected, not statements.
	CChars						szType;				//Then: its type, which the new function returns.
	size						uiStart;			//What is replaced by the call: the expression, or the statements' lines
	size						uiEnd;				//(from the first one's indentation to the last one's end).
	CChars						szIndent;			//The statements' indentation.
	CArrayIDEExtractVariables	asVariables;		//In the order they are declared.
	bool						bReturns;			//The selection ends by returning from the function: so does the call.
	CChars						szFunctionReturn;	//What the function it is in returns, as it is written.
	SIDESignatureSite			sFunction;			//The function it is in, as its definition is written.
	CChars						szClassKey;			//Its class, empty if it has none.
	CChars						szQualifier;		//What its name is written after: "CTank::", "CList<M>::", or nothing.
	CChars						szTemplateArguments;	//A function template's parameters as a call names them, "<M>"; empty otherwise.
	bool						bBefore;			//The new function is defined before the one it is in (a function of no class is
													//declared nowhere else), not after it.
	CArrayChars					aszUsed;			//The names the file writes.

	void	Init(void);
	void	Kill(void);
};


//IntelliJ's Extract Method (Ctrl+Alt+M; Refactorings.txt, 9): the selected statements, or expression, become a
//function, and a call of it is written where they were.
//
//What the function is given and gives back is worked out from how the selection uses the function's variables and
//parameters (F3).  One the selection reads is a parameter; one it changes that is used afterwards is what it returns
//("iSum = Sum(iA, iB);"), or, if there is more than one of those, each is a reference parameter; an object (a
//variable whose type is a class) is always given by reference, never copied.  One that only the selection uses is
//the new function's own, and its declaration goes with it; one the selection gives a value first, that nothing reads
//afterwards, is declared again in the new function.  A variable counts as read by the selection unless a statement of
//the selection itself (not one inside an if or a loop) assigns it before anything reads it; and as used afterwards
//if anything after the selection names it, or, where the selection is in a loop, anything in the loop.
//
//A selection that ends by returning from the function becomes "return Name(...);"; one that returns part of the way
//through, or leaves by break, continue or goto, is refused: it has more than one way out.
//
//In a member function the new function is a member: declared at the end of the class's protected part and defined
//after the function it came from, with the class's name (and a class template's template<...>) written as that
//function writes them (T4); const if that function is.  A function of no class gets one defined just before it.  From
//a function template the new function is a template of the same parameters, and the call names them.
class CIDEExtractMethod : public CObject
{
CONSTRUCTABLE(CIDEExtractMethod);
DESTRUCTABLE(CIDEExtractMethod);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpDialog;  //While it is shown.
	Ptr<CTextField>		mpNameField;
	Ptr<CLabel>			mpSignature;
	Ptr<CLabel>			mpError;
	SIDEExtraction*		mpsShown;  //What the dialog is extracting.  Not held.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	bool	CanExtract(void);
	void	QueueExtract(void);
	bool	ShowExtract(void);
	bool	FindExtraction(SIDEExtraction* psDest);
	const char*	GetNameProblem(SIDEExtraction* psExtraction, const char* szName);
	bool	Extract(SIDEExtraction* psExtraction, const char* szName, CChars* pszStatus = NULL);
	void	AppendSignature(SIDEExtraction* psExtraction, const char* szName, bool bQualified, CChars* pszDest);
	void	AppendCall(SIDEExtraction* psExtraction, const char* szName, CChars* pszDest);
	char*	PrintExtraction(SIDEExtraction* psExtraction, CChars* pszDest);

protected:
	void	ShowQueuedExtract(void);
	bool	ApproveName(void);
	bool	Refuse(SIDEExtraction* psDest, const char* szWhyNot);
	bool	FindFunction(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, const char* szText, SIDEExtraction* psDest);
	void	FindVariables(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, CCPPSyntax* pcBlock, int32 iFirst, int32 iLast, const char* szText, SIDEExtraction* psDest);
	void	ChooseRoles(SIDEExtraction* psDest);
	void	AppendReturnType(SIDEExtraction* psExtraction, CChars* pszDest);
	void	AppendParameters(SIDEExtraction* psExtraction, bool bNamesOnly, CChars* pszDest);
};


#endif // __IDE_EXTRACT_METHOD_H__

