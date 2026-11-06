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
#ifndef __IDE_CODE_MODEL_H__
#define __IDE_CODE_MODEL_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/Mutex.h"
#include "BaseLib/MapStringInt.h"
#include "ThreadLib/WorkerThreadJobs.h"
#include "CppParserLib/Config.h"
#include "CppParserLib/Project.h"
#include "CppParserLib/PPHeaderCache.h"
#include "CppParserLib/CppTokeniser.h"
#include "CppParserLib/CPPSyntaxTree.h"
#include "CppParserLib/CPPHighlighter.h"
#include "CppParserLib/CPPIndex.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildLib/BuildWorkspace.h"
#include "WindowLib/GoToDialog.h"
#include "IDEIcons.h"


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
	int32	iContainer;	//The index's symbol of the class or function it is in, -1 if neither.
};


typedef CArrayTemplate<SIDELocation>	CArrayIDELocations;


//What Rename found at a place in a file: the symbol (its key and name), every place its name is written (the
//declarations, definitions and uses, a class's constructors and destructor too) and, if it can't be renamed, why.
struct SIDERenameTarget
{
	CChars				szKey;
	CChars				szName;
	CArrayIDELocations	asLocations;
	bool				bRenamable;
	CChars				szWhyNot;

	void	Init(void);
	void	Kill(void);
};


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
typedef CArrayTemplatePtr<CProject>		CArrayIDEProjects;


//What the IDE knows of the workspace's code: each file preprocessed and parsed as its solution's configuration builds
//it, with its project's include directories and defines, as MSVC would see them; and an index of every name the
//solutions' .cpp files declare and use, built on worker threads (StartIndexing).  Projects are the workspace's, each
//once however many solutions have it.
class CIDECodeModel
{
protected:
	CBuildWorkspace*	mpcWorkspace;
	CChars				mszConfiguration;		//The build configuration, "Release|W65C816"; each solution builds its own solution configuration for it.
	CArrayChars			maszSystemDirectories;	//Where the compiler's own headers are ($(IncludePath)).

	CArrayIDEProjects	mapcProjects;			//A CppParserLib project for each of the workspace's, NULL until used.
	CPPHeaderCache		mcHeaderCache;			//Headers' preprocessed output, kept for all the units.
	CMutex				mcProjectsMutex;		//The projects, their libraries and units are made one thread at a time.

	CCPPIndex			mcIndex;
	CMutex				mcIndexMutex;			//The index and the counts.
	CWorkerThreadJobs	mcJobs;
	bool				mbJobs;
	CArrayIDEIndexJobs	mapsJobs;
	size				muiIndexed;
	bool				mbCancelled;			//The jobs not started yet do nothing (KillJobs).
	CMapStringInt		mcIncluders;			//A file to the first indexed unit that included it (the job's index).

public:
	void	Init(CBuildWorkspace* pcWorkspace);
	void	Kill(void);

	void	SetConfiguration(const char* szConfiguration);
	void	Reset(void);
	char*	GetConfiguration(void);
	void	AddSystemDirectory(const char* szDirectory);
	void	AddDefaultSystemDirectories(void);

	bool	Highlight(size uiProject, const char* szFileName, const char* szContents, CArrayHighlights* pacDest, CChars* pszErrors);

	void	StartIndexing(void);
	void	WaitForIndexing(void);
	void	GetIndexingProgress(size* puiIndexed, size* puiToIndex);
	bool	FindDefinition(const char* szFileName, int iLine, int iColumn, SIDELocation* psDest);
	bool	FindReferences(const char* szFileName, int iLine, int iColumn, CArrayIDELocations* pasDest, CChars* pszKey);
	size	FindProjectOfFile(const char* szFileName);
	void	FindGoToSymbols(CNameMatcher* pcMatcher, bool bClasses, CMapStringInt* pcFiles, int32* aiIcons, CGoToItems* pcDest);
	bool	GetSymbolLocation(size uiSymbol, SIDELocation* psDest);
	bool	FindRenameTarget(const char* szFileName, int iLine, int iColumn, CMapStringInt* pcFiles, SIDERenameTarget* psDest);
	bool	HasSymbol(const char* szKey);
	void	GetUsageGroup(SIDELocation* psLocation, CChars* pszProject, CChars* pszClass);
	bool	FindIncludeFile(size uiProject, const char* szIncluder, const char* szName, bool bSystem, CChars* pszDest);

	void	IndexUnit(SIDEIndexJob* psJob);

protected:
	bool				ParseUnit(size uiProject, const char* szFileName, const char* szContents, CIDEParsedUnit* pcDest, CChars* pszErrors);
	SIDEIndexJob*		FindIncluder(const char* szFileName);
	CSourceFile*		FindFileInUnit(CIDEParsedUnit* pcUnit, const char* szFileName);
	CTranslationUnit*	AddUnit(size uiProject, const char* szFileName);
	CProject*			GetProject(size uiProject);
	CVcxConfiguration*	GetProjectConfiguration(size uiProject);
	const char*			GetSolutionConfiguration(size uiSolution);
	void				AddCompilerDefines(CVcxConfiguration* pcConfiguration, CConfig* pcConfig);
	void				KillProjects(void);
	void				KillJobs(void);
	void				CopyLocation(SCPPIndexLocation* psLocation, SIDELocation* psDest);
};


void KillIDELocations(CArrayIDELocations* pasLocations);
void GetIDESymbolName(const char* szKey, CChars* pszDest);


#endif // __IDE_CODE_MODEL_H__

