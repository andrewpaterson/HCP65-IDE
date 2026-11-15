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
#ifndef __IDE_INTRODUCE_TYPEDEF_H__
#define __IDE_INTRODUCE_TYPEDEF_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayInt.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;


//Where a typedef is written: in the file, before what first uses it; in a class template, before the member that
//first uses it; or in a function, at the start of its body.
#define IDE_TYPEDEF_FILE		0
#define IDE_TYPEDEF_CLASS		1
#define IDE_TYPEDEF_FUNCTION	2


//What Introduce Typedef found for the active editor's selection: the type as it is written, every place it is written
//again where the typedef can be seen, and where the typedef goes.  Positions are in the editor's text as it is.
struct SIDETypedef
{
	bool		bPossible;
	CChars		szWhyNot;
	CChars		szFileName;
	CChars		szType;				//As it is written.
	int			iPlace;				//IDE_TYPEDEF_.
	CArrayInt	aiStarts;			//Each place it is written, in order: where it starts
	CArrayInt	aiEnds;				//and ends.
	size		uiSelected;			//Which of them is the selection.
	CArrayInt	aiTypedefAt;		//Where the typedef's line goes if that place is the first replaced (for each place),
	CChars		szIndent;			//and its indentation.
	CArrayChars	aszUsed;			//The names the file writes: the typedef's must not be one.

	void	Init(void);
	void	Kill(void);
};


//Introduce Typedef (Ctrl+Alt+K, as CLion's; Refactorings.txt, 8): the selected type is given a name, and the other
//places it is written, token for token, can be given the name too.
//
//The typedef is written as they are here, "typedef CArrayTemplate<SIDEThing>	CArrayThings;", two blank lines before
//whatever in the file first uses it (a function, a class, with the comment over it).  A type that names a template's
//parameter can only be named where the parameter is: in a class template, the typedef is a member of the class,
//before the member that first uses it, and the places are those in the class; otherwise it is a function's own, at
//the start of its body, and the places are those in the function.
//
//A place the type is written before the typedef can't use it, so the typedef goes before the first of the places
//replaced.  Places a macro makes, and other files', aren't looked at.
class CIDEIntroduceTypedef : public CObject
{
CONSTRUCTABLE(CIDEIntroduceTypedef);
DESTRUCTABLE(CIDEIntroduceTypedef);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpDialog;  //While it is shown.
	Ptr<CTextField>		mpNameField;
	Ptr<CLabel>			mpError;
	Ptr<CCheckBox>		mpAllBox;
	SIDETypedef*		mpsShown;  //What the dialog is introducing.  Not held.
	bool				mbAll;  //Replace all occurrences, as last chosen.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	bool	CanIntroduce(void);
	void	QueueIntroduce(void);
	bool	ShowIntroduce(void);
	bool	FindTypedef(SIDETypedef* psDest);
	const char*	GetNameProblem(SIDETypedef* psTypedef, const char* szName);
	bool	Introduce(SIDETypedef* psTypedef, const char* szName, bool bAll, CChars* pszStatus = NULL);
	char*	PrintTypedef(SIDETypedef* psTypedef, CChars* pszDest);

protected:
	void	ShowQueuedIntroduce(void);
	bool	ApproveName(void);
	bool	Refuse(SIDETypedef* psDest, const char* szWhyNot);
	bool	IsTypeToken(CIDERefactorUnit* pcUnit, int32 iToken);
	bool	IsSameTokens(CIDERefactorUnit* pcUnit, int32 iFirst, int32 iLast, int32 iAt);
};


#endif // __IDE_INTRODUCE_TYPEDEF_H__

