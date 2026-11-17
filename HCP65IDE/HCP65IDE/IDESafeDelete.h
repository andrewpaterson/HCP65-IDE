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
#ifndef __IDE_SAFE_DELETE_H__
#define __IDE_SAFE_DELETE_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
struct SIDECaretContext;


//What Safe Delete found for a symbol: what would be taken out, and what would be left naming it.
struct SIDESafeDeletion
{
	CChars				szKey;
	CChars				szName;			//With its scopes: "CFoo::Init".
	uint8				uiKind;			//ECPPSymbolKind.
	CArrayChars			aszKeys;		//Every symbol taken out: it, and what is declared in what is taken out (a class's members,
										//an enum's enumerators, a function's parameters and locals).
	CArrayIDEFileRanges	asRanges;		//Each declaration and definition taken out, as the syntax tree has it...
	CArrayInt			aiStatements;	//...and whether each is a whole declaration (its lines go with it) or a part of one.
	CArrayIDELocations	asUsages;		//Each place left that names something taken out.
	size				uiOverriders;	//The functions in derived classes that override it (their declarations are in asUsages).
	size				uiAssembly;		//The places hand written assembly names it by its labels.
	size				uiComments;		//With the searches: its name in comments and strings,
	size				uiText;			//and as text the index doesn't have (and in the projects' text files).
	bool				bDeletable;
	CChars				szWhyNot;
	CArrayChars			aszFiles;		//A class's files that are left with nothing in them (comments, an include guard, #includes).
	bool				bDeleteFiles;	//They are deleted with it: set where there are any, and unset by who asks.

	void	Init(void);
	void	Kill(void);
	size	NumUsages(void) const;
};


typedef CArrayTemplatePtr<CIDERefactorUnit>	CArrayIDERefactorUnits;


//IntelliJ's Safe Delete (Alt+Delete; Refactorings.txt, 1): the symbol at the caret is taken out, with its declarations
//and definitions in every file, only if nothing is left that uses it.  A class goes with its members and their
//definitions, an enum with its enumerators, a namespace with what is in it.  If it is used, the usages are counted and
//can be looked at (Find Usages), and it can be deleted anyway.  As IntelliJ's, it can also look for the name in
//comments and strings and as text.
//
//What counts as a use: a reference left outside what is taken out (a call from a function that is itself taken out
//doesn't); a function in a derived class overriding it; a label of it in hand written assembly; a template's body
//naming it through an instance (it is a reference, Refactorings.txt T1).
//
//A whole declaration is taken out with its lines, the comment lines directly above it and the blank lines that
//were between it and what follows; one declarator of several ("int a, b;") or an enumerator goes with its comma.
//It can't be deleted (the status line says why) if a declaration of it is made by a macro or left out by the
//configuration, is in a file that isn't in the solution, or is a parameter (Change Signature's).
class CIDESafeDelete : public CObject
{
CONSTRUCTABLE(CIDESafeDelete);
DESTRUCTABLE(CIDESafeDelete);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>			mpDialog;  //While one is shown.
	Ptr<CCheckBox>			mpCommentsBox;
	Ptr<CCheckBox>			mpFilesBox;  //"Delete files", where the class's files have nothing else.
	Ptr<CCheckBox>			mpTextBox;
	CArrayIDERefactorUnits	mapcUnits;  //The files parsed for the deletion being found.
	bool					mbComments;  //Search in comments and strings.
	bool					mbText;  //Search for text occurrences.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanSafeDelete(void);
	void	QueueSafeDelete(void);
	bool	ShowSafeDelete(void);
	bool	FindDeletion(SIDESafeDeletion* psDest);
	bool	Delete(SIDESafeDeletion* psDeletion, CChars* pszStatus = NULL);
	void	FindEmptiedFiles(SIDESafeDeletion* psDeletion);
	size	DeleteFiles(SIDESafeDeletion* psDeletion);
	bool	FindFileBeside(SIDESafeDeletion* psDeletion, const char* szFileName, CChars* pszDest);
	void	SetSearches(bool bComments, bool bText);
	char*	PrintDeletion(SIDESafeDeletion* psDeletion, CChars* pszDest);

protected:
	void				ShowQueuedSafeDelete(void);
	bool				Plan(SIDESafeDeletion* psDeletion);
	bool				AddDeclarations(SIDESafeDeletion* psDeletion, const char* szKey);
	void				FindUsages(SIDESafeDeletion* psDeletion);
	void				SearchFiles(SIDESafeDeletion* psDeletion);
	CIDERefactorUnit*	GetUnit(const char* szFileName);
	void				KillUnits(void);
	bool				FindDeclarationRange(SIDELocation* psLocation, SIDEFileRange* psDest, bool* pbStatement, bool* pbForward, const char** pszWhyNot);
	bool				IsInRanges(SIDESafeDeletion* psDeletion, SIDELocation* psLocation);
	void				ShowDeleteStatus(CChars* pszMessage, CChars* pszStatus);
	void				AppendUsages(SIDESafeDeletion* psDeletion, CChars* pszDest);
};


size	FindIDETextOffset(const char* szText, int32 iLine, int32 iColumn);
size	FindIDENextLine(const char* szText, size uiAt);
size	FindIDELineStart(const char* szText, size uiAt);
void	ExtendIDEDeletion(const char* szText, size* puiStart, size* puiEnd, bool bStatement);


#endif // __IDE_SAFE_DELETE_H__

