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
#ifndef __IDE_MEMBER_QUALIFIER_H__
#define __IDE_MEMBER_QUALIFIER_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEChangeSignature.h"


class CIDEContent;


//What Make const or Make static found for the member function at the caret.
struct SIDEQualifierChange
{
	bool			bStatic;		//Make static, not Make const.
	bool			bPossible;
	CChars			szWhyNot;
	SIDESignature	sSignature;		//Every declaration, definition and call of it and of what is changed with it.
	CChars			szClassKey;		//Its class.
	CArrayChars		aszConflicts;	//What its bodies do that a const function can't.

	void	Init(void);
	void	Kill(void);
};


//A member function made const, or static (Refactorings.txt, 18), for the function the caret is on:
//  - Make Member Function const: " const" after its parameters in every declaration and definition of it, of the
//    functions that override it and that it overrides (they must stay one function), and of the members of other
//    classes a template's body calls with it by one name (T2: CHolder<M>::Start calling mp->Read() ties every M's
//    Read together).  Each definition's body is checked against what it changes, from a kept check of its file
//    (CCPPSemanticPass::FindChanges): a field of its object assigned, stepped, given to a reference that isn't
//    const, its address taken or a function that isn't const called on it, and a call of another member function
//    of its own that isn't const.  Those are shown first, as Problems Detected: they won't compile in a const
//    function (which is how the next function to make const is found).
//  - Make Member Function static: "static" before it in its class, and a const after its parameters taken away;
//    the calls that name an object ("cVat.Count()", "pcVat->Count()") name its class instead ("CVat::Count()"),
//    where what names the object does nothing else.  Refused if it is virtual or overrides, or if a body uses
//    its object: a field or a member function that isn't static, or this.
//The files changed are checked as they would be, and nothing is changed if they have more errors than they had.
class CIDEMemberQualifier : public CObject
{
CONSTRUCTABLE(CIDEMemberQualifier);
DESTRUCTABLE(CIDEMemberQualifier);
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
	bool	CanChange(void);
	void	QueueMakeConst(void);
	void	QueueMakeStatic(void);
	bool	Show(bool bStatic);
	bool	Find(bool bStatic, SIDEQualifierChange* psDest);
	bool	Change(SIDEQualifierChange* psChange, CChars* pszStatus = NULL);
	bool	IsDeclaredWith(const char* szKey, const char* szWord);

protected:
	void	MakeConstQueued(void);
	void	MakeStaticQueued(void);
	bool	Refuse(SIDEQualifierChange* psDest, const char* szWhyNot, const char* szFileName = NULL, int32 iLine = -1);
	void	FindBodyUses(SIDEQualifierChange* psDest, SIDESignatureSite* psSite);
	void	AddConflict(SIDEQualifierChange* psDest, const char* szWhat, const char* szKey, const char* szAfter, const char* szFileName, int32 iLine);
};


#endif // __IDE_MEMBER_QUALIFIER_H__
