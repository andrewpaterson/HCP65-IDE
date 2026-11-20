/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_DEFINITIONS_H__
#define __IDE_DEFINITIONS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextArea.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEChangeSignature.h"


class CIDEContent;
struct SIDECaretContext;


//The function at the caret as its declaration and its definition write it (Refactorings.txt, F5: the two as one
//thing).  Its declaration is where it is declared apart from its definition: in its class, or a prototype.
struct SIDEFunction
{
	CChars				szKey;
	CChars				szName;			//With its scopes: "CFoo::Init".
	bool				bFunction;
	SIDESignatureSite	sDeclaration;
	bool				bDeclaration;
	SIDESignatureSite	sDefinition;
	bool				bDefinition;
	bool				bUndeclared;	//A member defined outside a class that doesn't declare it: no symbol, read from its file.

	void	Init(void);
	void	Kill(void);
};


//The edits a refactoring makes to several files' editors, each file's as one edit (CIDETextEdits) and all of them
//as one undo group.  A file's editor is opened, not shown, when it is first asked for, so positions are the
//editor's text's.
class CIDEFileEdits
{
protected:
	CIDEContent*						mpcContent;
	CArrayChars							maszFiles;
	CArrayTemplatePtr<CIDETextEdits>	mapcEdits;
	CArrayChars							maszNotes;

public:
	void			Init(CIDEContent* pcContent);
	void			Kill(void);

	Ptr<CTextArea>	GetEditor(const char* szFileName);
	const char*		GetText(const char* szFileName);
	CIDETextEdits*	GetEdits(const char* szFileName);
	size			NumFiles(void) const;
	size			CountErrors(bool bEdited);
	void			Apply(void);
	bool			NoteOnce(const char* szNote);
};


//What a declaration and its definition are to each other, C++'s own refactorings (Refactorings.txt, 13 and 14, and
//the first of Move, 3):
//  - Generate Definition (Ctrl+Shift+D, as CLion's): a function declared and not defined is defined, with an empty
//    body, in the file its class's other functions are defined in and in their order (after the one declared before
//    it), under a comment like theirs; a class template's member under its template<...> in the header.
//  - Generate Declaration: a function defined and not declared is declared, in its class after its other functions
//    and lined up with them, or in the header of its source file.
//  - The two brought back into step.  A member whose definition was edited (another parameter, say) is a member
//    its class doesn't declare, and the class's declaration of that name is left with no definition: Generate
//    Declaration on the definition changes that declaration to match it (its default values kept, and override),
//    and Generate Definition on the declaration changes the definition to match the declaration (the names its
//    body uses kept where a parameter's type is as it was).  What the caret is on is what is right.
//  - Move Definition out of Class: a function defined in its class is left declared there and defined outside, as
//    Generate Definition would place it, with its body.  A function outside any class that is defined in a header
//    (inline) is defined in the header's source file instead, without its inline, and declared in the header.
//  - Move Definition into Class: the reverse; one statement stays on the declaration's line, as the house style has
//    a class's short functions, more go under it.  A function outside any class that a header declares is defined
//    in that header, inline, and stays declared where it was.
//Each is one undo.  The status line says what was done, or why it can't be.
class CIDEDefinitions : public CObject
{
CONSTRUCTABLE(CIDEDefinitions);
DESTRUCTABLE(CIDEDefinitions);
protected:
	CIDEContent*						mpcContent;  //Not held: the content holds it.
	CArrayTemplatePtr<CIDERefactorUnit>	mapcUnits;  //The files parsed for the function being looked at.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanChange(void);
	bool	FindFunction(SIDEFunction* psDest, CChars* pszWhyNot);
	bool	FindUndeclaredFunction(SIDEFunction* psDest);
	bool	GenerateDefinition(void);
	bool	GenerateDeclaration(void);
	bool	MoveDefinitionOut(void);
	bool	MoveDefinitionIn(void);
	void	AppendBodyOutside(const char* szBody, CChars* pszDest);
	void	AppendBodyInClass(const char* szBody, const char* szIndent, CChars* pszDest);

protected:
	void				GenerateDefinitionCommand(void);
	void				GenerateDeclarationCommand(void);
	void				MoveDefinitionOutCommand(void);
	void				MoveDefinitionInCommand(void);
	CIDERefactorUnit*	GetUnit(const char* szFileName);
	void				KillUnits(void);
	bool				ReadFunction(SIDEFunction* psDest, const char* szOperation);
	void				Refuse(const char* szOperation, SIDEFunction* psFunction, const char* szWhyNot);
	bool				FindDefinitionPlace(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, int iFiles, CChars* pszFileName, size* puiAt, int* piWhere, CChars* pszBanner, const char** pszWhyNot);
	void				AppendDefinition(SIDEFunction* psFunction, const char* szDeclarationText, const char* szBanner, const char* szPrefix, const char* szInitialisers, const char* szBody, CChars* pszDest);
	bool				PutDefinition(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, int iFiles, const char* szPrefix, const char* szInitialisers, const char* szBody, CChars* pszFileName, size* puiAt, const char** pszWhyNot);
	bool				FindUndefinedNamesake(CIDERefactorUnit* pcUnit, CArrayIDELocations* pasDeclared, CArrayIDELocations* pasDefined, SCPPSyntaxRange* psClass, const char* szOwnName, SIDESignatureSite* psDest);
	bool				FindUndeclaredDefinition(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, SIDESignatureSite* psDest);
	void				AppendParameterType(SIDESignatureSite* psSite, const char* szText, size uiParameter, CChars* pszDest);
	void				MatchSite(SIDESignatureSite* psChanged, const char* szChangedText, SIDESignatureSite* psModel, const char* szModelText, bool bDeclaration, CIDETextEdits* pcEdits);
	bool				MoveFunctionToSource(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, CChars* pszFileName, const char** pszWhyNot);
	bool				MoveFunctionToHeader(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, CChars* pszFileName, const char** pszWhyNot);
	void				AppendParameters(SIDESignatureSite* psSite, const char* szText, bool bDefaults, CChars* pszDest);
	void				AppendBanner(const char* szText, size uiDefinition, CChars* pszDest, const char** pszWhyNot);
	bool				AppendNewBanner(CChars* pszDest, const char** pszWhyNot);
	size				FindBannerStart(const char* szText, size uiDefinition);
	void				ShowDefinition(const char* szFileName, size uiAt);
};


#endif // __IDE_DEFINITIONS_H__

