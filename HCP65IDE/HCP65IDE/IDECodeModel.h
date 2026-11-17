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
#ifndef __IDE_CODE_MODEL_H__
#define __IDE_CODE_MODEL_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/Mutex.h"
#include "BaseLib/MapStringInt.h"
#include "BaseLib/TextFile.h"
#include "ThreadLib/WorkerThreadJobs.h"
#include "CppCompilerLib/Config.h"
#include "CppCompilerLib/Project.h"
#include "CppCompilerLib/PPHeaderCache.h"
#include "CppCompilerLib/CppTokeniser.h"
#include "CppCompilerLib/CPPSyntaxTree.h"
#include "CppCompilerLib/CPPHighlighter.h"
#include "CppCompilerLib/CPPIndex.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildLib/BuildWorkspace.h"
#include "WindowLib/GoToDialog.h"
#include "IDEIcons.h"
#include "IDERenameAssembly.h"


//A file preprocessed and parsed as its project builds it.
class CIDEParsedUnit
{
public:
	CTranslationUnit*	mpcUnit;
	CCPPTokeniser		mcTokeniser;
	CCPPSyntaxTree		mcTree;
	bool				mbTree;		//The tree is made once the unit is tokenised.
	bool				mbContents;	//Its contents were given, not read from its file.

	void	Init(void);
	void	Kill(void);
};


//A place found in the index, copied out of it for the editor.
struct SIDELocation
{
	CChars	szFileName;
	int32	iLine;		//0 based.
	int32	iColumn;	//0 based.
	int32	iLength;
	uint8	uiRole;		//ECPPIndexRole.
	uint8	uiFlags;	//CPPIF_AMBIGUOUS.
	uint8	uiUsage;	//ECPPUsageType.
	int32	iContainer;	//The index's symbol of the class or function it is in, -1 if neither.
};


typedef CArrayTemplate<SIDELocation>	CArrayIDELocations;


//An #include a file writes (CIDECodeModel::FindIncludesIn), where it is in the file's text.
struct SIDEInclude
{
	size	uiLineStart;	//Its line,
	size	uiLineEnd;		//and the start of the next.
	size	uiNameStart;	//The name it gives, between its quotes or angle brackets.
	size	uiNameEnd;
	int32	iLine;			//From 0.
	bool	bSystem;		//<> rather than "".
	bool	bConditional;	//Inside an #if, other than its file's include guard.
	CChars	szFileName;		//The file it finds, empty if none.
};


typedef CArrayTemplate<SIDEInclude>	CArrayIDEIncludes;


//A symbol a file names that another file declares (CIDECodeModel::FindNeeds).
struct SIDENeed
{
	CChars	szUser;			//The file that names it,
	CChars	szKey;
	uint8	uiKind;			//ECPPSymbolKind.
	CChars	szDeclared;		//the file that declares it (a class's or an enum's is the one that defines it),
	bool	bDeclaredByUser;	//and whether the file that names it declares it too (a class's forward declaration).
};


typedef CArrayTemplate<SIDENeed>	CArrayIDENeeds;


//What Rename found at a place in a file: the symbol (its key and name), every place its name is written (the
//declarations, definitions and uses, a class's constructors and destructor too) and, if it can't be renamed, why.
//Where the code model gets a file's text: its editor's if it is open, else the disk's (pcFile reads it and keeps it).
class CIDEFileTexts
{
public:
	virtual bool	GetFileText(const char* szFileName, CTextFile* pcFile, char** pszText, size* puiLength) =0;
};


struct SIDERenameTarget;
typedef CArrayTemplatePtr<SIDERenameTarget>	CArrayIDERenameTargets;


struct SIDERenameTarget
{
	CChars				szKey;
	CChars				szName;
	CArrayIDELocations	asLocations;
	bool				bRenamable;
	CChars				szWhyNot;
	size				uiOverrides;	//The virtual member functions renamed with it: what overrides it and what it overrides.
	bool				bType;			//A class, enum, typedef, template or namespace: Rename's searches are remembered apart for them.
	size				uiFamily;		//The members of other classes renamed with it: a template's body names them and it by one name
										//(mp->Init() in CHolder<M>, instantiated with each of their classes), and theirs in turn.
	CChars				szFamily;		//Their keys' names with their scopes, for showing: "CBar::Init, CBaz::Init".
	CChars				szFamilyWhere;	//The template's class or function that names them (the first found): "CHolder::Start".
	CArrayIDERenameTargets	apsWith;	//What is renamed with it, its overrides and its family: each one's key, name, scope and
										//assembly, without its places (which are in asLocations).
	uint8				uiAssembly;		//How hand written assembly's labels name it (EIDEAssemblyRename).
	CChars				szScope;		//The scopes its name is in, as its key has them ("NS::CFat"), empty for none.
	bool				bCLinkage;		//A function or variable declared inside extern "C": its label is its name alone.
	bool				bOverloaded;	//A function with others of its name in its scope.
	CArrayChars			aszOverloads;	//Then every one's key, its own too: assembly labels are told apart by their parameters.

	void	Init(void);
	void	Kill(void);
};


//A range of a file's text that a refactoring changes or takes out: lines and columns from 0, its end exclusive, as
//the syntax tree gives a node's (CIDERefactorUnit::GetRange); then where it starts and ends in the file's text.
struct SIDEFileRange
{
	CChars	szFileName;
	int32	iLine;
	int32	iColumn;
	int32	iEndLine;
	int32	iEndColumn;
	size	uiStart;
	size	uiEnd;

	void	Init(const char* szFile, int32 iStartLine, int32 iStartColumn, int32 iLastLine, int32 iLastColumn);
	void	Kill(void);
	bool	Has(const char* szFile, int32 iAtLine, int32 iAtColumn);
};


typedef CArrayTemplate<SIDEFileRange>	CArrayIDEFileRanges;


class CIDECodeModel;
//A file to index on a worker thread.
struct SIDEIndexJob
{
	CIDECodeModel*	pcModel;
	size			uiIndex;		//In the model's jobs.
	size			uiProject;		//The workspace's.
	CChars			szFileName;
};


typedef CArrayTemplatePtr<SIDEIndexJob>	CArrayIDEIndexJobs;


//An open file worked out again after it was edited (CIDECodeModel::ProcessEditedFiles): highlighted, indexed again, or
//both, from one parse.  A header is parsed in a unit that includes it (its text set with SetHeaderText first), or on
//its own if there is none.  The texts are copies: the editors can change while it is processed on the edits thread.
struct SIDEEditedFile
{
	size				uiProject;		//The workspace's.
	CChars				szFileName;
	CChars				szText;			//Its editor's text.
	bool				bText;			//False: the file is read.
	size				uiChanges;		//Its editor's change count when the text was copied.
	bool				bHighlight;
	bool				bIndex;

	bool				bUnit;			//A header: the unit it is parsed in.
	size				uiUnitProject;
	CChars				szUnit;
	CChars				szUnitText;		//The unit's editor's text.
	bool				bUnitText;		//False: the unit is read.

	CArrayHighlights	acHighlights;	//Made: the file's highlights, if bHighlighted.
	bool				bHighlighted;
	CArrayCPPSyntaxRanges	asSyntaxRanges;	//Made with the highlights: what each node of its syntax was parsed from.
	bool				bIndexed;		//Made: the index has the file from szText (or the file's own text).

	void	Init(size uiProject, const char* szFileName, const char* szText, size uiChanges, bool bHighlight, bool bIndex);
	void	SetUnit(size uiProject, const char* szUnit, const char* szText);
	void	Kill(void);
};


typedef CArrayTemplatePtr<SIDEEditedFile>	CArrayIDEEditedFiles;
typedef CArrayTemplatePtr<CProject>		CArrayIDEProjects;


//What the IDE knows of the workspace's code: each file preprocessed and parsed as its solution's configuration builds
//it, with its project's include directories and defines, as MSVC would see them; and an index of every name the
//solutions' .cpp files declare and use, built on worker threads (StartIndexing).  Projects are the workspace's, each
//once however many solutions have it.
class CIDECodeModel
{
friend class CIDERefactorUnit;
protected:
	CBuildWorkspace*	mpcWorkspace;
	CChars				mszConfiguration;		//The build configuration, "Release|W65C816"; each solution builds its own solution configuration for it.
	CArrayChars			maszSystemDirectories;	//Where the compiler's own headers are ($(IncludePath)).
	bool				mbSystemDirectoriesMissing;	//Some that are needed aren't on this machine.

	CArrayIDEProjects	mapcProjects;			//A CppCompilerLib project for each of the workspace's, NULL until used.
	CPPHeaderCache		mcHeaderCache;			//Headers' preprocessed output, kept for all the units.
	CMutex				mcProjectsMutex;		//The projects, their libraries and units are made one thread at a time.

	CCPPIndex			mcIndex;
	CMutex				mcIndexMutex;			//The index and the counts.
	CWorkerThreadJobs	mcJobs;
	bool				mbJobs;
	CArrayIDEIndexJobs	mapsJobs;
	size				muiIndexed;
	size				muiIndexings;			//How many times the index has been started (StartIndexing).
	bool				mbCancelled;			//The jobs not started yet do nothing (KillJobs).
	CMapStringInt		mcIncluders;			//A file to the first indexed unit that included it (the job's index).
	CArrayChars			maszProblems;			//What the units indexed couldn't include, each once, until taken (TakeIndexingProblems).
	CMapStringInt		mcProblems;				//Every one found since the index was started.
	size				muiProblemUnits;		//How many units had any.
	int64				miIndexStart;			//When the index was started (GetDebugTickCount),
	int64				miIndexEnd;				//and when its last unit was done, -1 until then.
	CMapStringInt		mcProjectFiles;			//Each project item's full path, in any case, to its project's index (FindProjectOfFile).
	CArrayChars			maszProjectFiles;		//The same full paths, in the projects' order.
	bool				mbProjectFiles;			//They have been found since the workspace last changed (FindProjectFiles).

	CWorkerThreadJobs	mcEdits;				//The edits thread: one, working out the edited files away from the UI thread.
	bool				mbEdits;				//It has been started.
	CArrayIDEEditedFiles	mapsEdited;			//The files given it, until they are taken.
	bool				mbEditsBusy;			//Under the index mutex.
	int					miEditsDelay;			//Milliseconds the edits thread waits before it starts, for checking.
	bool				mbIndexTemplates;		//The index checks each template in its own words too: what a template nothing instantiates names is in it.

public:
	void	Init(CBuildWorkspace* pcWorkspace);
	void	Kill(void);

	void	SetConfiguration(const char* szConfiguration);
	void	Reset(void);
	char*	GetConfiguration(void);
	void	AddSystemDirectory(const char* szDirectory);
	void	AddDefaultSystemDirectories(void);
	CArrayChars*	GetSystemDirectories(void);
	bool	IsMissingSystemDirectories(void) const;

	bool	Highlight(size uiProject, const char* szFileName, const char* szContents, CArrayHighlights* pacDest, CChars* pszErrors);

	void	StartIndexing(void);
	void	WaitForIndexing(void);
	void	GetIndexingProgress(size* puiIndexed, size* puiToIndex);
	void	TakeIndexingProblems(CArrayChars* paszDest);
	size	NumIndexingProblemUnits(void);
	int64	GetIndexingMilliseconds(void);
	bool	IsIndexing(void);
	size	NumIndexings(void) const;
	void	SetHeaderText(const char* szFileName, const char* szText);
	bool	GetIncluder(const char* szFileName, size* puiProject, CChars* pszUnit);
	void	ReindexAll(void);
	void	SetIndexTemplates(bool bIndexTemplates);
	bool	IsIndexingTemplates(void) const;
	void	ProcessEditedFiles(CArrayIDEEditedFiles* papsFiles, bool bNow);
	bool	TakeEditedFiles(CArrayIDEEditedFiles* papsDest, bool bWait);
	bool	IsProcessingEditedFiles(void);
	void	RunEditsThread(void);
	void	SetEditsDelay(int iMilliseconds);
	bool	FindDefinition(const char* szFileName, int iLine, int iColumn, SIDELocation* psDest);
	bool	FindReferences(const char* szFileName, int iLine, int iColumn, CArrayIDELocations* pasDest, CChars* pszKey);
	bool	FindRoleAt(const char* szFileName, int iLine, int iColumn, uint8* puiRole);
	bool	FindFileReferences(const char* szFileName, int iLine, int iColumn, CArrayIDELocations* pasDest);
	size	FindProjectOfFile(const char* szFileName);
	size	NumProjectFiles(void);
	char*	GetProjectFile(size uiIndex);
	void	ForgetProjectFiles(void);
	size	FindFilesNamed(const char* szName, CArrayChars* paszDest);
	void	FindGoToSymbols(CNameMatcher* pcMatcher, bool bClasses, int32* aiIcons, CGoToItems* pcDest);
	bool	GetSymbolLocation(size uiSymbol, SIDELocation* psDest);
	bool	FindRenameTarget(const char* szFileName, int iLine, int iColumn, SIDERenameTarget* psDest);
	void	FindAssemblyRename(CCPPIndexSymbol* pcSymbol, SIDERenameTarget* psDest);
	bool	HasSymbol(const char* szKey);
	bool	GetSymbolAt(const char* szFileName, int iLine, int iColumn, CChars* pszKey, uint8* puiKind);
	bool	GetSymbolLocations(const char* szKey, CArrayIDELocations* pasDest, uint8* puiKind);
	size	FindSymbolsDeclaredIn(CArrayIDEFileRanges* pasRanges, CArrayChars* paszKeys);
	size	FindSymbolsUsedIn(CArrayIDEFileRanges* pasRanges, CArrayChars* paszKeys);
	size	FindSymbolsIn(CArrayIDEFileRanges* pasRanges, CArrayChars* paszKeys, bool bUsed);
	size	FindClassBases(const char* szKey, bool bAll, CArrayChars* paszKeys);
	size	FindClassDerived(const char* szKey, bool bAll, CArrayChars* paszKeys);
	bool	GetSymbolKey(int32 iSymbol, CChars* pszKey, uint8* puiKind);
	void	FindNamespaces(CArrayChars* paszDest);
	size	FindOverriders(const char* szKey, CArrayIDELocations* pasDest);
	size	FindSiblingFunctions(const char* szKey, const char* szFileName, CArrayIDELocations* pasDeclared, CArrayIDELocations* pasDefined);
	bool	FindAssemblyTarget(const char* szKey, SIDERenameTarget* psDest);
	size	FindOverrideLocations(const char* szKey, CArrayIDELocations* pasDest);
	void	GetUsageGroup(SIDELocation* psLocation, CChars* pszProject, CChars* pszClass);
	bool	FindIncludeFile(size uiProject, const char* szIncluder, const char* szName, bool bSystem, CChars* pszDest);
	size	FindIncludesOf(const char* szFileName, CIDEFileTexts* pcTexts, CArrayIDELocations* pasDest);
	size	FindIncludesIn(const char* szFileName, const char* szText, CArrayIDEIncludes* pasDest);
	bool	MakeIncludeName(const char* szIncluder, const char* szTarget, CChars* pszDest);
	size	FindNeeds(const char* szUser, CArrayChars* paszDeclared, CArrayIDENeeds* pasDest);
	size	FindHeadersDeclaring(const char* szName, CArrayChars* paszDest);

	void	IndexUnit(SIDEIndexJob* psJob);
	bool	IsIndexedByAnother(const char* szFileName, size uiJob);

protected:
	bool				ParseUnit(size uiProject, const char* szFileName, const char* szContents, CIDEParsedUnit* pcDest, CChars* pszErrors);
	SIDEIndexJob*		FindIncluder(const char* szFileName);
	size				FindIncludeProject(const char* szIncluder);
	void				GetIncludeDirectories(size uiProject, const char* szIncluder, bool bSystem, CArrayChars* paszDest);
	int					FindDeclaringFile(CCPPIndexSymbol* pcSymbol);
	CSourceFile*		FindFileInUnit(CIDEParsedUnit* pcUnit, const char* szFileName);
	CTranslationUnit*	AddUnit(size uiProject, const char* szFileName);
	CProject*			GetProject(size uiProject);
	CVcxConfiguration*	GetProjectConfiguration(size uiProject);
	const char*			GetSolutionConfiguration(size uiSolution);
	void				AddCompilerDefines(CVcxConfiguration* pcConfiguration, CConfig* pcConfig);
	void				KillProjects(void);
	void				FindProjectFiles(void);
	void				KillJobs(void);
	void				WaitForEditedFiles(void);
	void				ProcessEditedFile(SIDEEditedFile* psFile);
	void				HighlightFile(CIDEParsedUnit* pcUnit, CSourceFile* pcFile, CArrayHighlights* pacDest, CArrayCPPSyntaxRanges* pasRanges);
	void				GatherUnit(CIDEParsedUnit* pcUnit, CCPPIndexUnit* pcDest, CCPPIndexedFiles* pcIndexed);
	void				CopyLocation(SCPPIndexLocation* psLocation, SIDELocation* psDest);
	void				CopyLocations(CCPPIndexSymbol* pcSymbol, CArrayIDELocations* pasDest);
	void				CopyLocationsInFile(CCPPIndexSymbol* pcSymbol, const char* szFileName, CArrayIDELocations* pasDest);
	void				FindOverrides(CCPPIndexSymbol* pcSymbol, CArrayInt* paiDest);
	bool				IsDerivedClass(CCPPIndexSymbol* pcClass, CCPPIndexSymbol* pcBase, int iDepth);
};


void KillIDELocations(CArrayIDELocations* pasLocations);
void KillIDEIncludes(CArrayIDEIncludes* pasIncludes);
void KillIDENeeds(CArrayIDENeeds* pasNeeds);
void KillIDEFileRanges(CArrayIDEFileRanges* pasRanges);
void MoveIDELocation(SIDELocation* psDest, SIDELocation* psSource);
const char* GetIDEFileNamePart(const char* szPath);
void KillIDEEditedFiles(CArrayIDEEditedFiles* papsFiles);
void GetIDESymbolName(const char* szKey, CChars* pszDest);
void GetIDEScopedName(const char* szKey, CChars* pszDest);
void GetIDESymbolScope(const char* szKey, CChars* pszDest);


#endif // __IDE_CODE_MODEL_H__

