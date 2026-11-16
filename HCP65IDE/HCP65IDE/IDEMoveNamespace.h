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
#ifndef __IDE_MOVE_NAMESPACE_H__
#define __IDE_MOVE_NAMESPACE_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TextField.h"
#include "WindowLib/CompletionField.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEDefinitions.h"


class CIDEContent;
struct SIDECaretContext;


//A namespace block a declaration is in, in its file: from the namespace's line to the end of its closing brace's.
struct SIDENamespaceBlock
{
	CChars	szName;
	size	uiStart;		//The start of its namespace line.
	size	uiOpenEnd;		//After its opening brace's line.
	size	uiCloseStart;	//The start of its closing brace's line.
	size	uiEnd;			//After its closing brace's line.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDENamespaceBlock>	CArrayIDENamespaceBlocks;


//A declaration of what moves: in the old namespace's blocks (asBlocks, outermost first: it goes into the new one's), or
//written outside them with its name qualified (only the qualifier changes).
struct SIDENamespaceSite
{
	CChars						szFileName;
	size						uiStart;		//From the comment over it...
	size						uiEnd;			//...to the end of its last line.
	CArrayIDENamespaceBlocks	asBlocks;
	bool						bQualified;
	size						uiQualifierStart;	//A qualified one's "Old::" before its name.
	size						uiQualifierEnd;

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDENamespaceSite>	CArrayIDENamespaceSites;


//An edit of a file that Move to Namespace makes: what is at uiStart to uiEnd becomes szText.
struct SIDENamespaceEdit
{
	CChars	szFileName;
	size	uiStart;
	size	uiEnd;
	CChars	szText;

	void	Init(const char* szFileName, size uiStart, size uiEnd, const char* szText);
	void	Kill(void);
};


typedef CArrayTemplate<SIDENamespaceEdit>	CArrayIDENamespaceEdits;


//What Move to Namespace found for the declaration at the caret.
struct SIDENamespaceMove
{
	CChars					szKey;
	CChars					szName;			//Its own name, without its namespace.
	CChars					szNamespace;	//The namespace it is in, "" for the global namespace.
	uint8					uiKind;			//ECPPSymbolKind.
	bool					bMovable;
	CChars					szWhyNot;
	CArrayIDENamespaceSites	asSites;
	CArrayChars				aszMembers;		//A class's members' keys, whose definitions move with it.

	void	Init(void);
	void	Kill(void);
};


//Move to Namespace (Refactorings.txt, 21; Refactor, Move to Namespace...): a class, enum, typedef, function, variable
//or template at namespace scope goes to another namespace (a new one, or the global namespace).  Its declarations
//stay where they are in their files: the old namespace's block is closed round each and the new one's opened (a block
//left empty goes; one the declaration starts or ends goes before or after it), written as the file's blocks are or
//in the house style (the brace on a line of its own, the body a tab in).  One written outside its namespace with a
//qualified name (int Old::F(void)) has its qualifier changed.  A class's members' definitions in the old namespace's
//blocks move with it.
//
//Every use (the index's, as Rename finds them) names it from where it is: unqualified in the new namespace, inside
//it, or under a using-directive of it; otherwise with the new namespace from the namespaces the use and it share.  A
//using-declaration of it names the new namespace, or goes where it isn't needed.  With "Add using-declarations",
//a file whose uses would be qualified gets one using-declaration of it after its #includes instead.  What moves
//names things of the old namespace without their namespace: they are qualified where the new namespace can't see
//them.  Problems Detected comes first: the new namespace has the name already; functions of the old namespace that
//take the class (argument dependent lookup won't find them from it any more).
class CIDEMoveNamespace : public CObject
{
CONSTRUCTABLE(CIDEMoveNamespace);
DESTRUCTABLE(CIDEMoveNamespace);
protected:
	CIDEContent*						mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>						mpDialog;  //While it is shown.
	Ptr<CCompletionField>				mpNamespaceField;  //Completing over the solution's namespaces.
	Ptr<CCheckBox>						mpUsingBox;
	bool								mbUsings;  //What the box was ticked as last.
	CArrayTemplatePtr<CIDERefactorUnit>	mapcUnits;  //The files parsed for the move being found.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanMove(void);
	void	QueueMove(void);
	void	ShowMove(void);
	void	NamespaceTyped(CTextField* pcField);

	bool	FindMove(SIDENamespaceMove* psDest);
	bool	Move(SIDENamespaceMove* psMove, const char* szNamespace, bool bUsings, CChars* pszStatus);
	char*	PrintMove(SIDENamespaceMove* psMove, CChars* pszDest);
	void	KillUnits(void);

	//For Move to File, which moves what is in a namespace into a block of it.
	bool	FindSite(const char* szFileName, int32 iLine, int32 iColumn, bool bClassMember, CIDEFileEdits* pcFiles, SIDENamespaceSite* psDest, CChars* pszWhyNot);
	bool	IsNamespace(const char* szKey);
	bool	FindBlockIn(const char* szFileName, const char* szNamespace, CIDEFileEdits* pcFiles, SIDENamespaceBlock* psDest);
	void	WidenToEmptyBlocks(SIDENamespaceSite* psSite, const char* szText, size* puiStart, size* puiEnd);
	void	AppendBlockLines(const char* szNamespace, size uiFrom, const char* szIndent, size uiDepth, bool bOpen, CChars* pszDest);

protected:
	CIDERefactorUnit*	GetUnit(const char* szFileName);
	void				FindBlocks(CIDERefactorUnit* pcUnit, const char* szText, CCPPSyntax* pcNode, CArrayIDENamespaceBlocks* pasDest, bool* pbUnnamed);
	void				FindScopeAt(CIDERefactorUnit* pcUnit, const char* szText, int32 iToken, CChars* pszDest);
	void				NamespaceOf(const char* szKey, CChars* pszDest);
	int32				FindQualifier(CIDERefactorUnit* pcUnit, int32 iName);
	bool				HasUsingDirective(CIDERefactorUnit* pcUnit, int32 iBefore, const char* szNamespace);
	void				FindUsingDirectives(CIDERefactorUnit* pcUnit, int32 iBefore, CArrayChars* paszDest);
	void				AppendShortestQualifier(CIDERefactorUnit* pcUnit, int32 iName, const char* szFrom, const char* szTo, CChars* pszDest);
	size				FindNeighbourBlock(const char* szText, size uiStart, size uiEnd, CArrayChars* paszNew, size uiShared);
	void				AppendQualifier(const char* szFrom, const char* szTo, CChars* pszDest);
	size				FindIndent(const char* szText, size uiLine);
	void				MoveSite(SIDENamespaceSite* psSite, const char* szOld, const char* szNew, CIDEFileEdits* pcFiles, CArrayIDENamespaceEdits* pasDest);
	void				QualifyInside(SIDENamespaceMove* psMove, SIDENamespaceSite* psSite, const char* szNew, CIDEFileEdits* pcFiles, CArrayIDENamespaceEdits* pasDest);
	void				FindConflicts(SIDENamespaceMove* psMove, const char* szNew, CIDEFileEdits* pcFiles, CArrayChars* paszDest);
	void				JoinSites(SIDENamespaceMove* psMove, CIDEFileEdits* pcFiles);
};


#endif // __IDE_MOVE_NAMESPACE_H__
