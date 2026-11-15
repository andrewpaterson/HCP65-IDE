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
#ifndef __IDE_TEMPLATE_PARAMETER_H__
#define __IDE_TEMPLATE_PARAMETER_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEChangeSignature.h"
#include "IDEDefinitions.h"


class CIDEContent;


//What Extract Template Parameter found at the caret: a type named in a function.
struct SIDETemplateExtraction
{
	bool			bPossible;
	CChars			szWhyNot;
	CChars			szType;			//The type as it is written: "CGauge", "int".
	CChars			szTypeKey;		//Its symbol; empty for a type of the language's own.
	SIDESignature	sSignature;		//The function: every declaration, definition and call.
	CChars			szSuggested;	//A name for the parameter that the function hasn't: "M".
	bool			bDeduced;		//The type is in the function's parameters: a call's arguments say what it is.

	void	Init(void);
	void	Kill(void);
};


//Extract Template Parameter (Refactorings.txt, 20), for the type the caret is on in a function's declaration or
//its body: the function becomes a function template, the type one of its parameters.
//  - "template<class M>" goes over every declaration and definition of the function, and the type becomes M
//    everywhere they name it ("typename M::X" where they name a type of its own).
//  - A template's definition must be where its uses are compiled: one in a source file whose function a header
//    declares goes to the header, in place of the declaration (a member function's, after its class, its banner
//    with it).
//  - The calls stay as they are where the type is in the function's parameters (the compiler works M out from the
//    arguments); otherwise each names it: "Make<CGauge>()".
//Refused: a virtual function, one overridden or called by a template with other classes' functions, one that is
//a template already or a class template's, one named without being called, a constructor or a destructor.  The
//files changed are checked as they would be.
//
//The class's turn (a class made a template, a typedef of it with the type it had keeping its users compiling) and
//the inverse (a parameter every instance gives the same argument taken out) are still to do.
class CIDETemplateParameter : public CObject
{
CONSTRUCTABLE(CIDETemplateParameter);
DESTRUCTABLE(CIDETemplateParameter);
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
	bool	CanExtract(void);
	void	QueueExtract(void);
	bool	ShowExtract(void);
	bool	FindExtraction(SIDETemplateExtraction* psDest);
	const char*	GetProblem(SIDETemplateExtraction* psExtraction, const char* szName);
	bool	Extract(SIDETemplateExtraction* psExtraction, const char* szName, CChars* pszStatus = NULL);

protected:
	void	ShowQueuedExtract(void);
	bool	Refuse(SIDETemplateExtraction* psDest, const char* szWhyNot);
	void	FindTypePlaces(SIDETemplateExtraction* psExtraction, CIDERefactorUnit* pcUnit, const char* szFileName, const char* szText, SCPPSyntaxRange* psRange, CArrayInt* paiStarts, CArrayInt* paiTypenames);
	void	AppendReplaced(SIDETemplateExtraction* psExtraction, const char* szName, const char* szText, size uiStart, size uiEnd, CArrayInt* paiStarts, CArrayInt* paiTypenames, CChars* pszDest);
};


#endif // __IDE_TEMPLATE_PARAMETER_H__
