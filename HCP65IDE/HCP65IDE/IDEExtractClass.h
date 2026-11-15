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
#ifndef __IDE_EXTRACT_CLASS_H__
#define __IDE_EXTRACT_CLASS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDEMoveMembers.h"


class CIDEContent;


//What is made of the members chosen.
#define IDE_EXTRACT_SUPERCLASS	0	//A class the class then derives from, with the members.
#define IDE_EXTRACT_INTERFACE	1	//A class the class then derives from, with the functions pure virtual; the class keeps its own.
#define IDE_EXTRACT_DELEGATE	2	//A class the class then has as a field, with the members.


//IntelliJ's Extract Superclass, Extract Interface and Extract Class (Extract Delegate) (Refactorings.txt, 11), for
//the class the caret is in.  Each makes a new class of members chosen from it, in a header of its own (named for
//the new class without its C, in the class's header's directory, made as Move Class to Its Own Files makes one) and
//a source file of its own if any of the members is defined in one:
//  - Extract Superclass: the members move to the new class (Pull Members Up's way: each declaration with the
//    comment over it, under its access; a definition outside the class renamed for the new class), and the class
//    derives from it.  A function can be left where it is and only declared in the new class, pure virtual.
//  - Extract Interface: the functions chosen are declared pure virtual in the new class, the class keeps its own,
//    and derives from it.
//  - Extract Class: the members move to the new class, all public, and the class gets a field of it where the
//    first of them was; every place one of them is named outside what moved gets the field before the name
//    ("miLevel" becomes "mcGauge.miLevel", "pcTank->Fill()" becomes "pcTank->mcGauge.Fill()").
//The class's header #includes the new one.  Where the class is a class template and what moves names its
//parameters, the new class is a template of the same parameters and the class derives from (or has) it with them;
//a function defined outside the class then goes with its body in the new class (T4).
//
//What stands in the way is shown first (Problems Detected), as Pull Members Up's: a member that uses one of the
//class's that isn't going with it, and the rest.  One undo empties the new files and puts everything back, as Move
//Class's does.
class CIDEExtractClass : public CObject
{
CONSTRUCTABLE(CIDEExtractClass);
DESTRUCTABLE(CIDEExtractClass);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpDialog;  //While it is shown.
	Ptr<CTextField>		mpNameField;
	Ptr<CTextField>		mpFieldField;
	Ptr<CLabel>			mpError;
	SIDEMembersMove*	mpsShown;  //Not held.
	int					miShownKind;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	bool	CanExtract(void);
	void	QueueSuperclass(void);
	void	QueueInterface(void);
	void	QueueDelegate(void);
	bool	ShowExtract(int iKind);
	bool	FindExtraction(SIDEMembersMove* psDest);
	bool	ParseMembers(SIDEMembersMove* psMove, const char* szMembers, const char* szAbstract, CChars* pszProblem);
	bool	FindProblem(SIDEMembersMove* psMove, int iKind, const char* szName, const char* szField, CChars* pszDest);
	void	FindConflicts(SIDEMembersMove* psMove, int iKind, const char* szName, CArrayChars* paszDest);
	bool	Extract(SIDEMembersMove* psMove, int iKind, const char* szName, const char* szField, CChars* pszStatus = NULL);

protected:
	void	ShowQueuedSuperclass(void);
	void	ShowQueuedInterface(void);
	void	ShowQueuedDelegate(void);
	bool	ApproveName(void);
	void	SetTarget(SIDEMembersMove* psMove, int iKind, const char* szName);
	bool	NamesParameters(SIDEMembersMove* psMove, CIDEFileEdits* pcEdits, CChars* pszTemplate, CChars* pszArguments);
	bool	IsMoved(SIDEMembersMove* psMove, const char* szFileName, int32 iLine);
};


#endif // __IDE_EXTRACT_CLASS_H__

