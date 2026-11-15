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


//What Extract Template Parameter found at the caret: a type named in a function, or in a class outside its
//functions' declarations.
struct SIDETemplateExtraction
{
	bool			bPossible;
	CChars			szWhyNot;
	CChars			szType;			//The type as it is written: "CGauge", "int".
	CChars			szTypeKey;		//Its symbol; empty for a type of the language's own.
	SIDESignature	sSignature;		//The function: every declaration, definition and call.
	CChars			szSuggested;	//A name for the parameter that the function hasn't: "M".
	bool			bDeduced;		//The type is in the function's parameters: a call's arguments say what it is.
	bool			bClass;			//It is named in a class, which is what becomes a template:
	CChars			szClassKey;		//the class,
	CChars			szClassFile;	//the file that defines it,
	SCPPSyntaxRange	sClass;			//and its definition there, to its ;.
	CChars			szSuggestedClass;	//A name for the template: "CTankTemplate".

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
//
//For a type named in a class outside its functions' declarations (a field's type), the class becomes a class
//template, as "Refactoring Towards Seams in C++" has it: the class is given another name and "template<class M>",
//the type is M throughout it and its members, its members' definitions go to its header after it (a template's
//must be there) as "template<class M> ... CTankTemplate<M>::Fill(...)", and after them a typedef makes the class's
//old name the template with the type it had, "typedef CTankTemplate<CGauge> CTank;": the code that uses CTank
//compiles as it did.  Refused: a class that is declared ahead of its definition somewhere (a typedef can't be), a
//class in a class or a namespace, one with a static field defined outside it.
//
//The inverse (a parameter every instance gives the same argument taken out) is still to do.
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
	const char*	GetClassProblem(SIDETemplateExtraction* psExtraction, const char* szName, const char* szTemplateName);
	bool	ExtractClass(SIDETemplateExtraction* psExtraction, const char* szName, const char* szTemplateName, CChars* pszStatus = NULL);

protected:
	void	ShowQueuedExtract(void);
	bool	Refuse(SIDETemplateExtraction* psDest, const char* szWhyNot);
	void	FindTypePlaces(SIDETemplateExtraction* psExtraction, CIDERefactorUnit* pcUnit, const char* szFileName, const char* szText, SCPPSyntaxRange* psRange, CArrayInt* paiStarts, CArrayInt* paiTypenames);
	void	AppendReplaced(SIDETemplateExtraction* psExtraction, const char* szName, const char* szText, size uiStart, size uiEnd, CArrayInt* paiStarts, CArrayInt* paiTypenames, CChars* pszDest);
	void	FindClass(SIDETemplateExtraction* psDest, CIDERefactorUnit* pcUnit, CCPPSyntax* pcNode, const char* szFileName);
	void	FindNamePlaces(CIDERefactorUnit* pcUnit, const char* szText, SCPPSyntaxRange* psRange, const char* szName, CArrayInt* paiStarts);
	void	AppendClassText(SIDETemplateExtraction* psExtraction, CIDERefactorUnit* pcUnit, const char* szFileName, const char* szText, SCPPSyntaxRange* psRange, SCPPSyntaxRange* psQualified, const char* szOwnName,
							const char* szName, const char* szTemplateName, CChars* pszDest);
};


#endif // __IDE_TEMPLATE_PARAMETER_H__
