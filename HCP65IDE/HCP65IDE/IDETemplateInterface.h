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
#ifndef __IDE_TEMPLATE_INTERFACE_H__
#define __IDE_TEMPLATE_INTERFACE_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/Label.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
struct SIDECaretContext;


//How the interface is written.
#define IDE_INTERFACE_COMMENT	0	//A comment over the template: "//M must have:" and the members.
#define IDE_INTERFACE_CLASS		1	//An abstract class before the template, the members pure virtual.


//What a template's body needs of its arguments: for the template the caret is in, each member it names through a
//value whose type is one of its type parameters (mp->Init(), cItem.miCount) or by the parameter's own name
//(M::Count).  The parallel arrays have one entry for each member, in the order the template first names them.
struct SIDETemplateInterface
{
	bool		bPossible;
	CChars		szWhyNot;
	CChars		szFileName;			//The file the template is in.
	CChars		szTemplate;			//Its name: "CHolder".
	CArrayChars	aszParameters;		//Its type parameters' names.
	CArrayChars	aszMemberParameters;	//The parameter each member is needed of,
	CArrayChars	aszMemberNames;		//its name,
	CArrayInt	aiMemberCalled;		//whether it is called (a function), and with how many arguments plus one, 0 if it isn't,
	CArrayChars	aszMemberDeclarations;	//and how one of the classes the template is used with declares it ("void	Init(void);");
										//empty where no instance of the template says.
	CArrayInt	aiMemberFunctions;	//Whether that declaration is a function's.
	size		uiAt;				//Where the template's line starts, in the file's text.
	CChars		szIndent;			//Its indentation.

	void	Init(void);
	void	Kill(void);
};


//Write Template's Interface (Refactorings.txt, 24): a template says nowhere what its argument must have; its body
//just uses it.  For the class or function template the caret is in, the members its body needs of each type
//parameter are found (and, in a class template's functions defined outside it, by whatever names those give the
//parameters), each with the declaration it has in a class the template is used with, where the index knows one.
//They are shown, and written if asked:
//  - as a comment over the template, "//M must have:" and each member's declaration; or
//  - as an abstract class before the template, each function pure virtual: what an argument can derive from to be
//    held to it.  Only if every member is a function with a known declaration, and for one parameter.
//Nothing else is changed: the classes the template is used with are left as they are.
class CIDETemplateInterface : public CObject
{
CONSTRUCTABLE(CIDETemplateInterface);
DESTRUCTABLE(CIDETemplateInterface);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>			mpDialog;  //While it is shown.
	Ptr<CComboBox>			mpKindBox;
	Ptr<CTextField>			mpNameField;
	Ptr<CLabel>				mpError;
	SIDETemplateInterface*	mpsShown;  //What the dialog shows.  Not held.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanWrite(void);
	void	QueueShow(void);
	bool	Show(void);
	bool	FindInterface(SIDETemplateInterface* psDest);
	const char*	GetProblem(SIDETemplateInterface* psInterface, int iKind, const char* szName);
	bool	Write(SIDETemplateInterface* psInterface, int iKind, const char* szName, CChars* pszStatus = NULL);
	char*	PrintInterface(SIDETemplateInterface* psInterface, CChars* pszDest);

protected:
	void	ShowQueued(void);
	bool	Approve(void);
	bool	Refuse(SIDETemplateInterface* psDest, const char* szWhyNot);
	void	AddMember(SIDETemplateInterface* psDest, const char* szParameter, const char* szName, int iCalled, const char* szFileName, int32 iLine, int32 iColumn);
	bool	FindDeclaration(const char* szFileName, int32 iLine, int32 iColumn, CChars* pszDest, bool* pbFunction);
	void	AppendMember(SIDETemplateInterface* psInterface, size uiMember, CChars* pszDest);
	void	AppendAligned(SIDETemplateInterface* psInterface, size uiMember, const char* szBefore, CChars* pszDest);
};


#endif // __IDE_TEMPLATE_INTERFACE_H__
