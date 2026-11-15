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
#ifndef __IDE_MOVE_CLASS_H__
#define __IDE_MOVE_CLASS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEDefinitions.h"


class CIDEContent;


//What Move Class to Its Own Files found for the class at the caret: where it is defined, and each definition of one
//of its members outside it.
struct SIDEClassMove
{
	CChars					szKey;
	CChars					szName;			//With its scopes.
	bool					bMovable;
	CChars					szWhyNot;
	CChars					szHeader;		//The file the class is defined in.
	SCPPSyntaxRange			sClass;			//All of its definition there: from its template<...> to its ;.
	CArrayIDEFileRanges		asOutside;		//Its members' definitions outside it, in the order they are written.
	CChars					szSource;		//The file, other than its header, they are in; empty if none is.
	CChars					szStem;			//The name suggested for its files, without an extension.
	CArrayChars				aszUsers;		//The other files that name it or a member of it, outside what moves.
	bool					bHeaderUses;	//What stays in its header names it or a member of it.
	bool					bNeedsHeader;	//It, or a definition that moves with it, names something that stays in its header.

	void	Init(void);
	void	Kill(void);
};


//Move Class to Its Own Files (Refactorings.txt, 3; F6, IntelliJ's Move): a class that shares a header with other
//things gets a header of its own and, if any of its members are defined in a source file, a source file of its own,
//in the directories the files it leaves are in.
//
//The new header has the comment the old one starts with (the licence), an include guard named as the old one's is,
//the old header's #includes, the class with the comment over it, and the members the old header defines outside the
//class (a class template's, inline ones).  The new source file has the old one's opening comment, an #include of
//the new header, the old one's #includes, and the members' definitions with the comments over them.  They go out of
//the old files.
//
//The new header is #included where the class is needed: by the old header if what stays in it names the class;
//otherwise by each file that names the class or one of its members, beside its #include of the old header (written
//as that one is, with its directories) or after its last #include.  If the class names something that stays in the
//old header (its base class), the new header #includes the old one; if each needs the other the class can't be
//moved until one of them is declared ahead by hand.  What an #include isn't needed for any more is left: that is
//another refactoring's (15).
//
//The new files are items of every project the old ones are items of, beside them (CVcxProject::AddItemBeside), and
//lines of a CMakeLists.txt that lists the old ones.  They are made empty and filled as an edit: one undo empties them
//again and puts the class back, so what builds before and after an undo is the same.  The files changed are saved and
//the projects read again, as after Rename File.
class CIDEMoveClass : public CObject
{
CONSTRUCTABLE(CIDEMoveClass);
DESTRUCTABLE(CIDEMoveClass);
protected:
	CIDEContent*						mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>						mpDialog;  //While it is shown.
	Ptr<CTextField>						mpNameField;
	Ptr<CLabel>							mpError;
	SIDEClassMove*						mpsMove;  //The move the dialog is shown for.  Not held.
	CArrayTemplatePtr<CIDERefactorUnit>	mapcUnits;  //The files parsed for the move being found.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	bool	CanMove(void);
	void	QueueMove(void);
	bool	ShowMove(void);
	bool	FindMove(SIDEClassMove* psDest);
	bool	FindProblem(SIDEClassMove* psMove, const char* szStem, CChars* pszDest);
	bool	Move(SIDEClassMove* psMove, const char* szStem, CChars* pszStatus = NULL);
	char*	PrintMove(SIDEClassMove* psMove, CChars* pszDest);

protected:
	void				ShowQueuedMove(void);
	bool				ApproveMove(void);
	CIDERefactorUnit*	GetUnit(const char* szFileName);
	void				KillUnits(void);
	void				AddOutside(SIDEClassMove* psMove, const char* szFileName, SCPPSyntaxRange* psRange);
	bool				IsMoved(SIDEClassMove* psMove, const char* szFileName, int32 iLine);
	void				FindUsers(SIDEClassMove* psMove);
	void				AppendNewPath(const char* szBeside, const char* szStem, const char* szExtension, CChars* pszDest);
	void				AppendBlock(const char* szText, int32 iLine, int32 iColumn, int32 iEndLine, int32 iEndColumn, CChars* pszDest);
	void				AppendIncludes(const char* szText, CChars* pszDest);
	void				AppendGuard(const char* szText, const char* szOldPath, const char* szStem, CChars* pszGuard, bool* pbPragma, bool* pbComment);
	size				FindOpeningEnd(const char* szText);
	bool				FindIncludePlace(const char* szText, const char* szIncluded, const char* szNewName, size* puiAt, CChars* pszLine);
	bool				CreateFile(const char* szPath, const char* szBeside);
	void				AddToListFiles(const char* szBeside, const char* szNewName);
};


#endif // __IDE_MOVE_CLASS_H__

