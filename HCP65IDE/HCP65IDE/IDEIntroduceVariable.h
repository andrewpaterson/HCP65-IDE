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
#ifndef __IDE_INTRODUCE_VARIABLE_H__
#define __IDE_INTRODUCE_VARIABLE_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/TextField.h"
#include "WindowLib/Label.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
struct SIDECaretContext;


//What the expression becomes: a variable of its function, a field of its function's class, a constant of its file,
//or a parameter of its function.
#define IDE_INTRODUCE_VARIABLE	0
#define IDE_INTRODUCE_FIELD		1
#define IDE_INTRODUCE_CONSTANT	2
#define IDE_INTRODUCE_PARAMETER	3


//A whitespace run before a declared name in a function's declarations, and the column the name is in.
struct SIDEDeclaredName
{
	size	uiSpaceStart;
	size	uiSpaceEnd;
	size	uiSpaceColumn;	//The column the run starts in.
	size	uiNameColumn;
};


typedef CArrayTemplate<SIDEDeclaredName>	CArrayIDEDeclaredNames;


//What Introduce Variable found for the active editor's selection: the expression, its type as it will be written,
//every place the same expression is written in the function, and where the variable's declaration and its assignment
//go.  Positions are in the editor's text as it is.
struct SIDEIntroduction
{
	bool					bPossible;
	CChars					szWhyNot;
	CChars					szFileName;
	CChars					szExpression;		//As it is written.
	CChars					szType;
	CChars					szSuggested;		//A name for it.
	CArrayTextRange			asOccurrences;		//In order; the selected one is uiSelected.
	size					uiSelected;
	size					uiAnchorOne;		//The start of the line of the statement the assignment goes before, for the selected
	size					uiAnchorAll;		//occurrence alone and for all of them...
	bool					bStatementOne;		//...or the statement is the expression itself, and becomes the assignment.
	bool					bStatementAll;
	size					uiDeclaration;		//Where the declaration's line goes: after the function's declarations, or at its body's start.
	bool					bDeclarations;		//The function has declarations at its start.
	CChars					szIndent;			//A statement's indentation in the function's body.
	CArrayIDEDeclaredNames	asDeclared;			//The names the function's declarations declare, for lining the new one up with them.
	CArrayChars				aszUsed;			//Every name written in the function: the variable's must not be one.
	CChars					szFunctionKey;		//The function it is in, and where its name is written there (from 0): what Introduce
	int32					iFunctionLine;		//Parameter changes the signature of.  Empty if the index doesn't know it.
	int32					iFunctionColumn;
	CChars					szClassKey;			//The class the function is a member of, empty if it is none's: Introduce Field's.
	bool					bConstant;			//It names no variable and calls nothing: it can be a constant.
	bool					bUsesLocals;		//It names the function's own variables or parameters,
	bool					bUsesMembers;		//or members of the function's class (or this): a call of the function can't give it.

	void	Init(void);
	void	Kill(void);
};


//IntelliJ's Introduce Variable (Ctrl+Alt+V; Refactorings.txt, 5): the selected expression becomes a local variable.
//Its type is written as its declaration wrote it where it is one thing with a declared type (a call's function's
//return type, a variable's or a field's: uint32 stays uint32), and otherwise as the compiler worked it out; in a
//template in the template's own words (M*), and it is refused if only the template's arguments decide it.  Every
//place the same expression is written in the function can be replaced with it, but those that are assigned to.
//
//As the house style has locals: the variable is declared with the function's declarations at the top of its body,
//its name lined up with theirs (they are moved over if its type is longer), and assigned before the statement that
//first uses it; a statement that is only the expression becomes the assignment.
//
//With nothing selected, as IntelliJ's: the expressions the caret is in are offered in a popup under it, the innermost
//first, and the one chosen is selected and introduced; if there is only one it is taken.
//
//Its other kinds (Refactorings.txt, 7) are the same extraction to another place:
//  - Introduce Field (Ctrl+Alt+F): in a member function, the expression becomes a field of the class, declared at
//    the end of the class's protected part (where its fields are; a struct's public) and lined up with the one above
//    it, and assigned where the variable would have been.  Its name is suggested with the field's m before it.
//  - Introduce Constant (Ctrl+Alt+C): an expression that names no variable and calls nothing becomes a #define, as
//    constants are written here, after the file's last #define or #include, in parentheses if it is more than one
//    thing.  Its name is suggested in capitals.
//  - Introduce Parameter (Ctrl+Alt+P): the expression becomes a parameter of its function, added last by Change
//    Signature (every declaration, definition and override of it), and every call of the function passes the
//    expression: so it can't use the function's own variables or parameters, or its class's members.
class CIDEIntroduceVariable : public CObject
{
CONSTRUCTABLE(CIDEIntroduceVariable);
DESTRUCTABLE(CIDEIntroduceVariable);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpDialog;  //While it is shown.
	Ptr<CTextField>		mpNameField;
	Ptr<CLabel>			mpError;
	Ptr<CCheckBox>		mpAllBox;
	SIDEIntroduction	msShown;  //What the dialog is introducing.
	bool				mbAll;  //Replace all occurrences, as last chosen.
	int					miKind;  //What is being introduced (IDE_INTRODUCE_).
	CChars				mszNameProblem;  //Why a name can't be had, where it has to be made up to say.
	Ptr<CMenuContainer>	mpChooser;  //The expressions at the caret to choose one of, when nothing is selected.
	CArrayTextRange		masCandidates;  //Where each is in the editor's text, the innermost first.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanIntroduce(void);
	void	QueueIntroduce(void);
	void	QueueIntroduceField(void);
	void	QueueIntroduceConstant(void);
	void	QueueIntroduceParameter(void);
	bool	ShowIntroduce(int iKind = -1);
	const char*	GetKindProblem(SIDEIntroduction* psIntroduction, int iKind);
	void	AppendSuggestedName(SIDEIntroduction* psIntroduction, int iKind, CChars* pszDest);
	size	FindCandidates(CArrayChars* paszTexts);
	bool	ChooseCandidate(size uiCandidate);
	Ptr<CMenuContainer>	GetChooser(void);
	bool	FindIntroduction(SIDEIntroduction* psDest);
	bool	Introduce(SIDEIntroduction* psIntroduction, const char* szName, bool bAll, CChars* pszStatus = NULL, int iKind = IDE_INTRODUCE_VARIABLE);
	const char*	GetNameProblem(SIDEIntroduction* psIntroduction, const char* szName, int iKind = IDE_INTRODUCE_VARIABLE);
	char*	PrintIntroduction(SIDEIntroduction* psIntroduction, CChars* pszDest);

protected:
	void		ShowQueuedIntroduce(void);
	void		PostIntroduce(void);
	void		FindContext(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, CCPPSyntax* pcExpression, SIDEIntroduction* psDest);
	bool		IntroduceElsewhere(SIDEIntroduction* psIntroduction, const char* szName, bool bAll, CChars* pszStatus, int iKind);
	bool		FindDefinePlace(const char* szText, size* puiAt);
	void		ChooserClicked(CMenuItem* pcItem);
	bool		ApproveName(void);
	bool		Refuse(SIDEIntroduction* psDest, const char* szWhyNot);
	bool		FindType(CIDERefactorUnit* pcUnit, CCPPSyntax* pcExpression, SIDEIntroduction* psDest);
	void		FindOccurrences(CIDERefactorUnit* pcUnit, CCPPSyntax* pcBody, CCPPSyntax* pcSelected, const char* szText, SIDEIntroduction* psDest, CArraySyntax* papcNodes);
	CCPPSyntax*	FindAnchor(CArraySyntax* papcNodes, size uiFirst, size uiLast);
	void		FindDeclarations(CIDERefactorUnit* pcUnit, CCPPSyntaxCompoundStatement* pcBody, const char* szText, SIDEIntroduction* psDest);
	void		SuggestName(CIDERefactorUnit* pcUnit, CCPPSyntax* pcExpression, SIDEIntroduction* psDest);
	void		ShowIntroduceStatus(CChars* pszMessage, CChars* pszStatus);
};


size	FindIDEVisualColumn(const char* szText, size uiLineStart, size uiPosition, size uiTabWidth);
void	AppendIDETabsTo(CChars* pszDest, size uiFromColumn, size uiToColumn, size uiTabWidth);


#endif // __IDE_INTRODUCE_VARIABLE_H__

