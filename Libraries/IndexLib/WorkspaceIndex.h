/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela IndexLib

Codaphela IndexLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela IndexLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela IndexLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __WORKSPACE_INDEX_H__
#define __WORKSPACE_INDEX_H__
#include "BaseLib/Chars.h"
#include "BaseLib/HashMapString.h"
#include "BaseLib/HashSetString.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/Mutex.h"
#include "ThreadLib/WorkerThreadJobs.h"
#include "CppCompilerLib/Config.h"
#include "CppCompilerLib/Project.h"
#include "CppCompilerLib/PPHeaderCache.h"
#include "CppCompilerLib/CppTokeniser.h"
#include "CppCompilerLib/CPPSyntaxTree.h"
#include "CppCompilerLib/CPPIndex.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildLib/BuildWorkspace.h"
#include "SymbolTable.h"
#include "ReferenceResolver.h"


//A file preprocessed and parsed as its project builds it.
class CParsedUnit
{
public:
	CTranslationUnit*	mpcUnit;
	CCPPTokeniser		mcTokeniser;
	CCPPSyntaxTree		mcTree;
	bool				mbTree;		//The tree is made once the unit is tokenised.
	bool				mbContents;	//Its contents were given, not read from its file.
	CCPPCompileOptions	mcOptions;	//How its project's build configuration compiles it: a check of it is made with them.

	void	Init(void);
	void	Kill(void);
};


class CWorkspaceIndex;
//A file to index on a worker thread.
struct SWorkspaceIndexJob
{
	CWorkspaceIndex*	pcIndex;
	size				uiIndex;		//In the index's jobs.
	size				uiProject;		//The workspace's.
	CChars				szFileName;
};


typedef CArrayTemplatePtr<SWorkspaceIndexJob>	CArrayWorkspaceIndexJobs;
typedef CArrayTemplatePtr<CProject>				CArrayWorkspaceProjects;


//A documentation page of the workspace (a Markdown item of a project) and what its spans name (HCP65/Website/
//DocsPlan.txt E3).
struct SWorkspacePage
{
	CChars					szFile;			//Its full path.
	size					uiProject;		//The workspace's.
	CArrayPageReferences	asReferences;	//Its references and the plain spans that could be (CReferenceResolver::
											//ResolvePage), a link's file a full path and its psSymbol NULL.
};


typedef CArrayTemplatePtr<SWorkspacePage>		CArrayWorkspacePages;


//The C++ of a workspace's solutions as their build configuration compiles it: each file preprocessed and parsed with
//its project's include directories and defines, as MSVC would see them, and an index (CCPPIndex) of every name the
//projects' .cpp and .c files declare and use, built on worker threads (StartIndexing).  Projects are the workspace's,
//each once however many solutions have it.  The HCP65 IDE's code model is one (CIDECodeModel); a program without a
//window can index a workspace with one too, the same way.
//The projects' Markdown items are its documentation pages: once the C++ is indexed each page's references to code
//([`CShape::Init`]) are resolved (CReferenceResolver) and become places of what they name, of usage type
//CPPUT_Documentation, so Find Usages and Rename see them.  What the workspace doesn't have can be resolved from the
//website's symbol table (SetSiteSymbols, found beside the mkdocs.yml FindDocsSite finds): such a reference links, but
//is no place in the index.
class CWorkspaceIndex
{
protected:
	CBuildWorkspace*			mpcWorkspace;
	CChars						mszConfiguration;		//The build configuration, "Release|W65C816"; each solution builds its own solution configuration for it.
	CArrayChars					maszSystemDirectories;	//Where the compiler's own headers are ($(IncludePath)).
	bool						mbSystemDirectoriesMissing;	//Some that are needed aren't on this machine.

	CArrayWorkspaceProjects		mapcProjects;			//A CppCompilerLib project for each of the workspace's, NULL until used.
	CPPHeaderCache				mcHeaderCache;			//Headers' preprocessed output, kept for all the units.
	CMutex						mcProjectsMutex;		//The projects, their libraries and units are made one thread at a time.

	CCPPIndex					mcIndex;
	CMutex						mcIndexMutex;			//The index and the counts.
	CWorkerThreadJobs			mcJobs;
	bool						mbJobs;
	CArrayWorkspaceIndexJobs	mapsJobs;
	size						muiIndexed;
	size						muiIndexings;			//How many times the index has been started (StartIndexing).
	bool						mbCancelled;			//The jobs not started yet do nothing (KillJobs).
	CHashMapString<int32>		mcIncluders;			//A file to the first indexed unit that included it (the job's index).
	CArrayChars					maszProblems;			//What the units indexed couldn't include, each once, until taken (TakeIndexingProblems).
	CHashSetString<>			mcProblems;				//Every one found since the index was started.
	size						muiProblemUnits;		//How many units had any.
	int64						miIndexStart;			//When the index was started (GetDebugTickCount),
	int64						miIndexEnd;				//and when its last unit was done, -1 until then.
	bool						mbIndexTemplates;		//The index checks each template in its own words too: what a template nothing instantiates names is in it.

	CArrayWorkspacePages		mapsPages;				//The workspace's documentation pages (with mcIndexMutex).
	bool						mbPagesIndexed;			//Their references are resolved; false from StartIndexing until then.
	CSymbolTable				mcSiteTable;			//The website's symbol table, for what the workspace doesn't have,
	CArrayChars					maszSiteFiles;			//the files its references can name,
	CChars						mszSiteRoot;			//the folder they are relative to (ending in a separator),
	bool						mbSiteSymbols;			//and whether it was read (SetSiteSymbols).
	CChars						mszDocsSite;			//The website's mkdocs.yml (SetDocsSite), empty if there is none.

public:
	void			Init(CBuildWorkspace* pcWorkspace);
	void			Kill(void);

	void			SetConfiguration(const char* szConfiguration);
	virtual void	Reset(void);
	char*			GetConfiguration(void);
	void			AddSystemDirectory(const char* szDirectory);
	void			AddDefaultSystemDirectories(void);
	CArrayChars*	GetSystemDirectories(void);
	bool			IsMissingSystemDirectories(void) const;

	void			StartIndexing(void);
	void			WaitForIndexing(void);
	void			GetIndexingProgress(size* puiIndexed, size* puiToIndex);
	void			TakeIndexingProblems(CArrayChars* paszDest);
	size			NumIndexingProblemUnits(void);
	int64			GetIndexingMilliseconds(void);
	bool			IsIndexing(void);
	size			NumIndexings(void) const;
	void			SetHeaderText(const char* szFileName, const char* szText);
	bool			GetIncluder(const char* szFileName, size* puiProject, CChars* pszUnit);
	void			ReindexAll(void);
	void			SetIndexTemplates(bool bIndexTemplates);
	bool			IsIndexingTemplates(void) const;
	CCPPIndex*		GetIndex(void);

	void			IndexUnit(SWorkspaceIndexJob* psJob);
	bool			IsIndexedByAnother(const char* szFileName, size uiJob);

	bool			SetSiteSymbols(const char* szTable, const char* szFiles, const char* szRoot);
	bool			SetDocsSite(const char* szMkdocsFile);
	char*			GetDocsSite(void);
	virtual bool	FindDocsSite(CChars* pszMkdocsFile);
	bool			HasSiteSymbols(void) const;
	void			IndexPages(void);
	bool			IndexPage(const char* szFileName, const char* szText);
	size			NumPages(void);
	size			FindItemProject(const char* szFileName);
	bool			ParseUnit(size uiProject, const char* szFileName, const char* szContents, CParsedUnit* pcDest, CChars* pszErrors);
	void			GetPageFiles(CArrayChars* paszDest);
	bool			IsPage(const char* szFileName);
	size			GetPageReferences(const char* szFileName, CArrayPageReferences* pasDest);
	bool			FindPageReference(const char* szFileName, int iLine, int iColumn, CChars* pszFile, int* piLine);
	static bool		IsPageFile(const char* szFileName);

protected:
	virtual void			WaitForProjectUsers(bool bForget);
	void					GatherUnit(CParsedUnit* pcUnit, CCPPIndexUnit* pcDest, CCPPIndexedFiles* pcIndexed);
	SWorkspaceIndexJob*		FindIncluder(const char* szFileName);
	CTranslationUnit*		AddUnit(size uiProject, const char* szFileName);
	CProject*				GetProject(size uiProject);
	CVcxConfiguration*		GetProjectConfiguration(size uiProject);
	const char*				GetSolutionConfiguration(size uiSolution);
	CBuildConfiguration*	FindBuildConfiguration(size uiSolution);
	void					AddCompilerDefines(CVcxConfiguration* pcConfiguration, CConfig* pcConfig);
	void					KillProjects(void);
	void					KillJobs(void);
	void					StopJobs(void);
	void					AddPages(void);
	void					KillPages(void);
	SWorkspacePage*			FindPage(const char* szFileName);
	void					GetResolvableFiles(CArrayChars* paszDest);
	void					ResolvePage(SWorkspacePage* psPage, const char* szText, CReferenceResolver* pcLive, CReferenceResolver* pcSite);
	void					AddPagePlaces(CCPPIndexUnit* pcUnit, const char* szPage, SPageReference* psReference, CReferenceResolver* pcLive);
	void					AddPagePlace(CCPPIndexUnit* pcUnit, SSymbolTableEntry* psEntry, const char* szPage, int iLine, int iColumn, int iLength);
};


bool IsIndexedSourceFile(const char* szFileName);


#endif // __WORKSPACE_INDEX_H__

