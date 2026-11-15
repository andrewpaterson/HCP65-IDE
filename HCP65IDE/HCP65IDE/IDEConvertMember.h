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
#ifndef __IDE_CONVERT_MEMBER_H__
#define __IDE_CONVERT_MEMBER_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEChangeSignature.h"
#include "IDEDefinitions.h"


class CIDEContent;


//A function made a member function, and a member function a function (Refactorings.txt, 19), for the function the
//caret is on: its first parameter is its object.
//  - Convert Function to Member Function: a function whose first parameter is a pointer or a reference to a class
//    of the solution becomes that class's.  It is declared in the class (at the end of its public part), its own
//    declarations go, and its definition stays where it is as "C::Name" without the parameter: in its body "p->x"
//    becomes "x" ("this->x" where a parameter or a local has that name), and p alone becomes this.  Every call
//    names its object first: "Depth(&cVat, 2)" becomes "cVat.Depth(2)", "Depth(pcVat, 2)" "pcVat->Depth(2)", and
//    "Depth(this, 2)" "Depth(2)".  A parameter that points to a const class makes it a const function.
//  - Convert Member Function to Function: the other way.  Its class's declaration of it goes and it is declared
//    after its class, with a first parameter that points to its class ("CVat* pcVat"; const for a const
//    function); in its body what it names of its object is named through the parameter ("pcVat->miDepth") and
//    this is the parameter; "cVat.Depth(2)" becomes "Depth(&cVat, 2)" and a call from its class's own functions
//    "Depth(this, 2)".
//Refused: a function named without being called, a virtual or static one, a constructor, a destructor or an
//operator, one defined in its class (Move Definition out of Class first) or not defined in the solution, one whose
//body changes the parameter that is to be its object.  The files changed are checked as they would be, and
//nothing is changed if they have more errors than they had: a member function that uses what is private can't be
//a function.
class CIDEConvertMember : public CObject
{
CONSTRUCTABLE(CIDEConvertMember);
DESTRUCTABLE(CIDEConvertMember);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	CChars			mszProblem;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	bool	CanConvert(void);
	void	QueueToMember(void);
	void	QueueToFunction(void);
	bool	Convert(bool bToMember, CChars* pszStatus = NULL);

protected:
	void	ToMemberQueued(void);
	void	ToFunctionQueued(void);
	bool	Refuse(const char* szWhyNot, const char* szFileName = NULL, int32 iLine = -1);
	size	Offset(const char* szText, int32 iLine, int32 iColumn);
	bool	TokenOffsets(CIDERefactorUnit* pcUnit, const char* szText, int32 iFirstToken, int32 iLastToken, size* puiStart, size* puiEnd);
	void	FindLocalNames(SIDESignatureSite* psSite, CArrayChars* paszDest);
	bool	ToMember(SIDESignature* psSignature, CIDEFileEdits* pcFiles, CChars* pszDone, size* puiCalls);
	bool	ToMemberBody(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, bool bPointer, CIDEFileEdits* pcFiles);
	bool	ToMemberCall(SIDESignature* psSignature, SIDESignatureCall* psCall, bool bPointer, CIDEFileEdits* pcFiles);
	bool	ToFunction(SIDESignature* psSignature, CIDEFileEdits* pcFiles, CChars* pszDone, size* puiCalls);
	bool	ToFunctionBody(CIDERefactorUnit* pcUnit, SIDESignatureSite* psSite, const char* szClassKey, const char* szParameter, CIDEFileEdits* pcFiles);
	bool	ToFunctionCall(SIDESignature* psSignature, SIDESignatureCall* psCall, SIDESignatureSite* psDefinition, const char* szParameter, CIDEFileEdits* pcFiles);
};


#endif // __IDE_CONVERT_MEMBER_H__
