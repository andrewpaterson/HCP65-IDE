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
#ifndef __IDE_DUPLICATES_H__
#define __IDE_DUPLICATES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/TreeContainer.h"
#include "IDEResultsPane.h"
#include "IDERefactorUnit.h"
#include "IDESteppable.h"
#include "IDEScope.h"
#include "IDEDuplicateMatcher.h"


class CIDEContent;
class CMarkupTag;
class CXMLWriter;
struct SIDEPlaceChange;


#define IDE_DUPLICATES_MINIMUM_STATEMENTS	3	//The smallest run of a function's statements listed, as Settings has it to begin with.
#define IDE_DUPLICATES_MINIMUM_TOKENS		20


//What Find Duplicates looks for.
enum EIDEDuplicatesKind
{
	IDK_Expression,		//The selected expression.
	IDK_Statements,		//The selected statements, of one block.
	IDK_Function,		//The function the caret is in: its whole body, and runs of its statements.
};


//A statement of what is looked for: of one of the function's blocks (by its number), at its place in the block.
struct SIDEDuplicateStatement
{
	size	uiBlock;
	size	uiIndex;
	int32	iFirstToken;
	int32	iLastToken;
	size	uiStatements;	//It and those inside it (CountIDEStatements).
};


typedef CArrayTemplate<SIDEDuplicateStatement>	CArrayIDEDuplicateStatements;


//A place something looked for is written again: its file (by index, in the files found in) and where it is, lines
//and columns from 0, its end exclusive.  The group it is of.
struct SIDEDuplicatePlace
{
	size	uiGroup;
	size	uiFile;
	int32	iLine;
	int32	iColumn;
	int32	iEndLine;
	int32	iEndColumn;
};


typedef CArrayTemplate<SIDEDuplicatePlace>	CArrayIDEDuplicatePlaces;


//What was found written again: the whole of what was looked for, or a run of the function's statements (the
//statements from uiFirst, uiCount of them, of a block), with its size.
struct SIDEDuplicateGroup
{
	bool	bWhole;
	size	uiBlock;
	size	uiFirst;
	size	uiCount;
	int32	iFirstToken;	//Its tokens in the file looked from.
	int32	iLastToken;
	size	uiTokens;
	size	uiStatements;
	int32	iLine;			//Where it is in the file looked from, lines from 0.
	int32	iEndLine;
	size	uiPlaces;
};


typedef CArrayTemplate<SIDEDuplicateGroup>	CArrayIDEDuplicateGroups;


//IntelliJ IDEA's Locate Duplicates as Find Duplicates (Edit, Control+Alt+Shift+D): where else in a scope (CIDEScope)
//the code at the caret is written, its names apart (CIDEDuplicateMatcher).  Selected, an expression is looked for
//(inside any statement: "x * x + y * y" is in "d = sqrt(a * a + b * b);") and statements of one block are looked
//for as a run of statements.  With nothing selected, the function the caret is in is: the places its whole body is
//written are listed first, then the runs of its statements written elsewhere, the longest first, of at least
//muiMinimumStatements statements (counting those inside them) and muiMinimumTokens tokens (Settings, Find
//Duplicates).  The function's variables and parameters may have other names at another place, always the same
//others, that are that place's function's variables and parameters; with mbLiteralsFree (the dialog's tick) its
//literals may be others of the same kind.
//
//The dialog asks for the scope and the tick.  The files of the scope (the workspace's project files in it, C and C++
//ones) are searched a few at a time, at each window tick, so the IDE carries on meanwhile: a file is parsed
//(CIDERefactorUnit) only if it has every name some statement looked for must be written with.  The Duplicates tool
//window lists what was found: a group for each part of what was looked for, each of its places under it, which opens
//it (a results pane, CIDEResultsPane: a place's node's data is its index in masPlaces, and one).  Their places follow
//their files' edits, and Next and Previous Occurrence step through them.
class CIDEDuplicates : public CObject, public CIDESteppable, public CIDEResultsSource
{
friend class CIDEScript;
CONSTRUCTABLE(CIDEDuplicates);
DESTRUCTABLE(CIDEDuplicates);
protected:
	CIDEContent*					mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>					mpDialog;  //While it is shown.

	//The settings: the dialog's and the Settings page's.
	EIDEScope						meScope;
	CChars							mszDirectory;
	bool							mbLiteralsFree;
	size							muiMinimumStatements;
	size							muiMinimumTokens;

	//Where what is looked for is: the selection, or the caret (an empty selection) in a function.  Rerun looks there.
	CChars							mszWhatFile;
	int32							miWhatLine;
	int32							miWhatColumn;
	int32							miWhatEndLine;
	int32							miWhatEndColumn;

	//What is looked for, while it is looked for (mbSearching): in mcUnit, the file it is in, parsed.
	bool							mbSearching;
	bool							mbParsed;  //mcUnit is.
	EIDEDuplicatesKind				meKind;
	CIDERefactorUnit				mcUnit;
	CIDEDuplicateMatcher			mcMatcher;
	CArrayChars						maszFree;  //The function's variables and parameters.
	CArrayChars						maszFixed;  //The names what is looked for must be written with, for passing over a file:
	CArrayInt						maiRequired;  //sets of them (by index, each set ended by -1), one of which a file must have all of.
	CArrayIDEDuplicateStatements	masStatements;  //Each block's, in order: an expression is one, of no block.
	CArrayInt						maiBlockStarts;  //Where each block's are in masStatements: the function's body is block 0.
	size							muiSelectedFirst;  //IDK_Statements: the selected statements, of block 0.
	size							muiSelectedCount;
	int32							miFunctionFirst;  //The function's tokens.
	int32							miFunctionLast;
	CChars							mszWhat;  //What is looked for, as the tree's title names it.
	CIDEScope						mcScope;
	CArrayChars						maszToSearch;  //The scope's files.
	size							muiNextFile;

	//What was found.
	CArrayChars						maszFiles;  //The files places are in.
	CArrayIDEDuplicateGroups		masGroups;
	CArrayIDEDuplicatePlaces		masPlaces;  //By group, then file, then where.
	CChars							mszFoundTitle;
	bool							mbFound;
	bool							mbMoved;  //Edits have moved places since the tree was shown, the last at miMoved.
	int64							miMoved;

	//The Duplicates tool window.
	Ptr<CIDEResultsPane>			mpPane;
	Ptr<CDockPane>					mpTool;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void					AddCommands(Ptr<CCommandSet> pCommands);
	void					AddEditItems(Ptr<CMenuContainer> pMenu);
	void					CreatePane(SDockPlace* psPlace);
	bool					CanFindDuplicates(void);
	void					QueueFindDuplicates(void);
	bool					ShowDialog(void);
	bool					TakeWhat(void);
	bool					Start(bool bRerun, bool bWait);
	bool					IsSearching(void) const;
	void					WaitForSearch(void);

	void					SetSettings(EIDEScope eScope, const char* szDirectory, bool bLiteralsFree);
	void					SetMinimums(size uiStatements, size uiTokens);
	EIDEScope				GetScope(void) const;
	char*					GetDirectory(void);
	bool					IsLiteralsFree(void) const;
	size					GetMinimumStatements(void) const;
	size					GetMinimumTokens(void) const;
	void					ReadSettings(CMarkupTag* pcSettings);
	void					WriteSettings(CXMLWriter* pcWriter);

	void					Clear(void);
	void					AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	int64					Poll(int64 iNow);
	Ptr<CTreeContainer>		GetTree(void);
	Ptr<CIDEResultsPane>	GetResultsPane(void) const;
	Ptr<CDockPane>			GetTool(void) const;
	size					NumPlaces(void) const;

	Ptr<CDockPane>			GetSteppablePane(void) override;
	const char*				GetSteppableName(void) override;
	bool					HasSteps(void) override;
	bool					Step(bool bNext, bool bKeepKeys) override;
	bool					GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) override;
	bool					CanRerunResults(CIDEResultsPane* pcPane) override;
	void					RerunResults(CIDEResultsPane* pcPane) override;

protected:
	void	ShowQueuedDialog(void);
	bool	FindWhat(CChars* pszWhyNot);
	bool	FindSelected(int32 iFirst, int32 iLast, CChars* pszWhyNot);
	bool	FindFunction(CCPPSyntaxFunctionDefinition* pcFunction, CChars* pszWhyNot);
	void	AddBlocks(CCPPSyntax* pcNode);
	void	AddFixedNames(int32 iFirst, int32 iLast);
	void	FindRequired(void);
	void	AddRequired(int32 iFirst, int32 iLast);
	void	AppendFunctionName(CCPPSyntaxFunctionDefinition* pcFunction, CChars* pszDest);
	void	StopSearch(void);
	bool	SearchSome(int64 iUntil);
	bool	MightBeIn(const char* szText);
	void	SearchFile(const char* szFileName);
	void	SearchUnit(CIDERefactorUnit* pcUnit, const char* szFileName);
	void	SearchExpressions(CIDERefactorUnit* pcUnit, const char* szFileName);
	void	SearchBlock(CIDERefactorUnit* pcUnit, const char* szFileName, CCPPSyntaxCompoundStatement* pcBlock, CArrayChars* paszFreeThere);
	void	SearchRuns(CIDERefactorUnit* pcUnit, const char* szFileName, CCPPSyntaxCompoundStatement* pcBlock, CArrayChars* paszFreeThere, size uiBlock, size uiFirst, size uiCount);
	size	MatchRun(CIDERefactorUnit* pcUnit, CCPPSyntaxCompoundStatement* pcBlock, size uiAt, CArrayChars* paszFreeThere, size uiFirst, size uiMost);
	bool	IsOwnPlace(CIDERefactorUnit* pcUnit, int32 iFirst, int32 iLast, int32 iPatternFirst, int32 iPatternLast);
	void	AddPlace(CIDERefactorUnit* pcUnit, const char* szFileName, int32 iFirst, int32 iLast, bool bWhole, size uiBlock, size uiFirst, size uiCount);
	void	FinishSearch(void);
	void	ShowResults(bool bKeepSelected);
	void	RemoveContainedPlaces(void);
	void	SortPlaces(void);
	void	AppendGroupName(SIDEDuplicateGroup* psGroup, CChars* pszDest);
	SIDEDuplicateStatement*	GetStatement(size uiBlock, size uiIndex);
	size	NumBlockStatements(size uiBlock);
};


#endif // __IDE_DUPLICATES_H__

