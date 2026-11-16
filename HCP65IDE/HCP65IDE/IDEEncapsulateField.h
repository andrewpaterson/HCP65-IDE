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
#ifndef __IDE_ENCAPSULATE_FIELD_H__
#define __IDE_ENCAPSULATE_FIELD_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
struct SIDECaretContext;


//How a place uses the field, and so what it becomes.
#define IDE_FIELD_READ			0	//Read: "x.miLevel" becomes "x.GetLevel()".
#define IDE_FIELD_ASSIGNED		1	//All of what an = assigns: "x.miLevel = v" becomes "x.SetLevel(v)".
#define IDE_FIELD_COMPOUND		2	//A += or the like, as a statement: "x.miLevel += v" becomes "x.SetLevel(x.GetLevel() + v)".
#define IDE_FIELD_STEPPED		3	//++ or --, as a statement: "x.miLevel++" becomes "x.SetLevel(x.GetLevel() + 1)".
#define IDE_FIELD_OBJECT_MEMBER	4	//A field that is an object, a member of it named: "x.mcBox.Get()" becomes "x.GetBox()->Get()".
#define IDE_FIELD_OBJECT_ADDRESS	5	//...or its address taken: "&x.mcBox" becomes "x.GetBox()".
#define IDE_FIELD_UNCHANGEABLE	6	//Some other way: it stays as it is, and is said.


//A place the field is named.
struct SIDEFieldUse
{
	CChars	szFileName;
	int32	iLine;			//From 0.
	int		iKind;			//IDE_FIELD_.
	bool	bInside;		//In one of its class's own functions.
	size	uiStart;		//What is replaced, in its file's text,
	size	uiEnd;
	CChars	szObject;		//what the field is named through there: "x.", "p->", or nothing,
	CChars	szValue;		//what is assigned (or added),
	size	uiValueEnd;		//where that ends (the setter's ) goes there: what is assigned may name the field too),
	CChars	szOperator;		//and how: "+" for +=, "-" for --.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDEFieldUse>	CArrayIDEFieldUses;


//What Encapsulate Field found for the field at the caret, and what is asked of it.
struct SIDEEncapsulation
{
	bool				bPossible;
	CChars				szWhyNot;
	CChars				szKey;
	CChars				szName;				//"miLevel".
	CChars				szClassKey;
	CChars				szType;				//As its declaration writes it: "int", "char*", "CBox".
	bool				bObject;			//Its type is a class's: its getter gives a pointer to it, and it has no setter.
	bool				bPublic;
	CChars				szFileName;			//Its class's file,
	size				uiDeclaration;		//and its declaration's statement there.
	size				uiDeclarationEnd;
	CArrayIDEFieldUses	asUses;
	CChars				szGetter;			//The names suggested, then those chosen.
	CChars				szSetter;
	CChars				szParameter;		//The setter's parameter: the field's name without its m.
	bool				bGetter;			//What is asked: a getter,
	bool				bSetter;			//a setter,
	bool				bInside;			//the class's own functions use them too,
	bool				bProtect;			//and the field becomes protected.

	void	Init(void);
	void	Kill(void);
};


//IntelliJ's Encapsulate Fields (Refactorings.txt, 12), for the field at the caret: the class gets a getter and a
//setter for it, declared at the end of its public part and defined on one line each at the bottom of the class's
//.cpp, where the one-line functions are ("int CTank::GetLevel(void) const { return miLevel; }"), the getter const
//(question 44); in the class itself where it has no .cpp or is a template.  The places that name the field outside
//the class's own functions (in them too, if asked) call those instead.  The field can be made protected, which is
//what it is for.
//
//How each place uses the field is the compiler's answer (CCPPSemanticPass::FindChanges, from a kept check of its
//file): a read becomes the getter; all of what an = assigns becomes the setter; a += or a ++ that is a statement by
//itself becomes the setter of the getter's value changed.  A field that is an object has a getter that gives a
//pointer to it and no setter, as functions here give objects: a member named through it goes through the getter
//("GetBox()->Get()"), and its address is the getter.  A place that uses it another way (its address taken, a
//reference bound to it, a ++ inside an expression) stays as it is and is shown first, as Problems Detected: it
//won't compile if the field is made protected.
class CIDEEncapsulateField : public CObject
{
CONSTRUCTABLE(CIDEEncapsulateField);
DESTRUCTABLE(CIDEEncapsulateField);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpDialog;  //While it is shown.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanEncapsulate(void);
	void	QueueEncapsulate(void);
	bool	ShowEncapsulate(void);
	bool	FindEncapsulation(SIDEEncapsulation* psDest);
	const char*	GetProblem(SIDEEncapsulation* psEncapsulation);
	void	FindConflicts(SIDEEncapsulation* psEncapsulation, CArrayChars* paszDest);
	bool	Encapsulate(SIDEEncapsulation* psEncapsulation, CChars* pszStatus = NULL);
	char*	PrintEncapsulation(SIDEEncapsulation* psEncapsulation, CChars* pszDest);

protected:
	void	ShowQueuedEncapsulate(void);
	bool	Refuse(SIDEEncapsulation* psDest, const char* szWhyNot);
	void	ReadUses(SIDEEncapsulation* psDest);
	bool	IsReplaced(SIDEEncapsulation* psEncapsulation, SIDEFieldUse* psUse);
};


#endif // __IDE_ENCAPSULATE_FIELD_H__

