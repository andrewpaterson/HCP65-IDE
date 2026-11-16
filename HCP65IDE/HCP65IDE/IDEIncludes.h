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
#ifndef __IDE_INCLUDES_H__
#define __IDE_INCLUDES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayInt.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
struct SIDECaretContext;


//What becomes of one of the file's #includes.
#define IDE_INCLUDE_KEPT		0	//Something the file names comes from it.
#define IDE_INCLUDE_LEFT		1	//Not looked at: a <> one, one inside an #if, one whose file isn't found.
#define IDE_INCLUDE_REMOVED		2	//Nothing the file names comes from it.
#define IDE_INCLUDE_FORWARD		3	//Only classes named as pointers or references come from it: it becomes their forward declarations.
#define IDE_INCLUDE_NEEDED		4	//It was to go, but the file doesn't compile without it.


struct SIDEIncludeUse
{
	int			iWhat;			//IDE_INCLUDE_.
	CChars		szWritten;		//As the file writes it: "Gauge.h" with its quotes.
	CChars		szWhy;			//"CGauge comes from it".
	CArrayChars	aszClasses;		//IDE_INCLUDE_FORWARD: the declarations written, "class CGauge;".

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDEIncludeUse>	CArrayIDEIncludeUses;


//What Optimise #includes found for a file.
struct SIDEIncludePlan
{
	CChars					szFileName;
	CArrayIDEIncludes		asIncludes;		//Its #includes, as it writes them,
	CArrayIDEIncludeUses	asUses;			//and what becomes of each.
	CArrayChars				aszAdded;		//The headers it is given an #include of (their files),
	CArrayChars				aszAddedWhy;	//and what comes from each.
	CArrayChars				aszDeclared;	//The forward declarations it is given besides: "class CPipe;".
	CArrayChars				aszDependants;	//The files that got a name through an #include that goes,
	CArrayChars				aszDependantHeaders;	//and the header each is given an #include of for it.
	size					uiErrors;		//The errors the file's check has as it is.
	bool					bOrder;			//Its #includes are put in order too, and those added go where they belong.
	bool					bReordered;		//Made with its edits: those it had change their order.

	void	Init(void);
	void	Kill(void);
};


struct SIDEGraphFile
{
	CChars		szFileName;
	bool		bRead;
	bool		bOpaque;		//Found by a <> #include: what it #includes isn't followed.
	CArrayInt	aiIncludes;
};


typedef CArrayTemplate<SIDEGraphFile>	CArrayIDEGraphFiles;


//Which file #includes which, read from the files' texts as it is asked (every #include a text writes, whatever #if
//it is in).  A file a <> #include finds is a leaf: the compiler's own headers aren't followed.
class CIDEIncludeGraph
{
protected:
	CIDECodeModel*		mpcModel;
	CIDEFileTexts*		mpcTexts;
	CArrayIDEGraphFiles	masFiles;

public:
	void	Init(CIDECodeModel* pcModel, CIDEFileTexts* pcTexts);
	void	Kill(void);

	int		Get(const char* szFileName);
	char*	GetFileName(int iFile);
	bool	IsOpaque(int iFile);
	void	FindReached(int iFrom, int iCutFrom, CArrayInt* paiCutTo, CArrayInt* paiDest);
	bool	Reaches(int iFrom, int iTo, int iCutFrom, CArrayInt* paiCutTo);

protected:
	void	Read(int iFile);
};


//A file's #includes put right (Refactorings.txt, 15; Refactor's Optimise #includes, Ctrl+Alt+O, IntelliJ IDEA's
//Optimize Imports), for the file in the editor:
//  - an #include nothing the file names comes from is removed.  What the file names, and which file declares each
//    thing, is the index's answer (CIDECodeModel::FindNeeds); a thing declared in a header an #include only reaches
//    (through the #includes of what it finds) counts for that #include if the file has none of the header itself.
//    One whose macros the file uses is kept (the index has no macros: the #defines are read from the texts);
//  - an #include only classes come from, each named only as a pointer or a reference, becomes their forward
//    declarations ("class CGauge;"), after the #includes;
//  - a header that declares something the file names, and that none of its #includes reaches (it compiles where
//    whoever #includes it has the header first), is given an #include; one of the solution's only;
//  - a name the file's check doesn't know, that one header of the solution declares, gets that header's #include:
//    type the name, then Ctrl+Alt+O.
//A header's #include that goes may be what brings a name to the files that #include the header: each that names
//something it brought, and no longer reaches its header, is given its own #include of that header.
//
//Then the file is checked as it would be (CIDERefactorUnit, its text and its unit's as they will be): if it has
//more errors than it had, the #includes that went are tried one at a time and those it needs are kept (a header
//that needs another before it).  <> #includes, those inside an #if, and a file's #include of its own header are
//left as they are.
//
//
//And they are put in order (there is no command to sort them alone): the <> ones first, as they are; then each
//library's ("BaseLib/Chars.h": the directory a name is written with is its library), the libraries from the lowest
//up, one that another's headers reach before it (COrderer has them so), a library's own in the order they are
//with one added after its last; then the headers of the file's own directory, as they are, one added last.  The
//file's own header stays where it is.  The lines that are sorted are those of the run the file's last #include is
//in, with nothing between them.  If the file has more errors with them in order (a header needs another before
//it) they are left in the order they are, and one added goes after the last.
//
//The files that got a name through an #include that goes are other files than the one asked about: what each is
//given is shown first (as IntelliJ's Problems Detected), and Cancel changes nothing.  Each is given its #include
//after the last of the library it is of, its own #includes left in their order.
class CIDEIncludes : public CObject
{
CONSTRUCTABLE(CIDEIncludes);
DESTRUCTABLE(CIDEIncludes);
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
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanOptimise(void);
	void	QueueOptimise(void);
	bool	FindPlan(SIDEIncludePlan* psDest);
	bool	Optimise(SIDEIncludePlan* psPlan, CChars* pszStatus = NULL);
	char*	PrintPlan(SIDEIncludePlan* psPlan, CChars* pszDest);

protected:
	void	OptimiseQueued(void);
	bool	IsOnlyPointedTo(CIDERefactorUnit* pcUnit, const char* szFileName, const char* szKey);
	bool	AppendForwardDeclaration(const char* szKey, const char* szDeclared, CChars* pszDest);
	bool	FindUsedMacro(CIDEIncludeGraph* pcGraph, int iInclude, const char* szText, CChars* pszDest);
	void	FindUnknownNames(SIDEIncludePlan* psPlan, CIDERefactorUnit* pcUnit, CIDEIncludeGraph* pcGraph, const char* szText);
	void	SortDependants(SIDEIncludePlan* psPlan);
	void	FindDependants(SIDEIncludePlan* psPlan);
	void	MakeEdits(SIDEIncludePlan* psPlan, const char* szText, CIDETextEdits* pcEdits);
	bool	MakeOrderedEdits(const char* szFileName, const char* szText, CArrayIDEIncludes* pasIncludes, CArrayInt* paiGone, CArrayChars* paszAdded, bool bSort, CIDETextEdits* pcEdits, size* puiFirst, size* puiLast, bool* pbReordered);
	void	OrderLibraries(CArrayChars* paszLibraries, CArrayChars* paszLibraryFiles, CArrayChars* paszFileLibraries, CArrayChars* paszDest);
	bool	MakeDependantEdits(SIDEIncludePlan* psPlan, const char* szFileName, const char* szText, CIDETextEdits* pcEdits);
	size	CountErrors(const char* szFileName, char* szText, const char* szUnit, char* szUnitText);
	size	CountPlanErrors(SIDEIncludePlan* psPlan, const char* szText);
	void	AddPlanned(SIDEIncludePlan* psPlan, const char* szHeader, const char* szKey);
};


#endif // __IDE_INCLUDES_H__
