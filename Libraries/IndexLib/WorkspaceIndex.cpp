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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/Timer.h"
#include "CppCompilerLib/CppParser.h"
#include "CppCompilerLib/CPPSemanticPass.h"
#include "SystemHeaders.h"
#include "WorkspaceIndex.h"


//Each thread indexing has a whole parsed unit; more than this many is no faster, and uses more memory.
#define WORKSPACE_INDEX_THREADS	8


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CParsedUnit::Init(void)
{
	mpcUnit = NULL;
	mcTokeniser.Init();
	mbTree = false;
	mbContents = false;
	mcOptions = *GetDefaultCPPCompileOptions();
	meTarget = CPPT_Host;
}


//////////////////////////////////////////////////////////////////////////
//
// The unit's preprocessed tokens and its contents are freed too (a unit
// keeps its tokens otherwise); it can be processed again.
//
//////////////////////////////////////////////////////////////////////////
void CParsedUnit::Kill(void)
{
	if (mbTree)
	{
		mcTree.Kill();
		mbTree = false;
	}
	mcTokeniser.Kill();
	if (mpcUnit)
	{
		mpcUnit->GetTokens()->Clear();
		if (mbContents)
		{
			mpcUnit->ClearContents();
			mbContents = false;
		}
		mpcUnit->Unload();
		mpcUnit = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file is C or C++ to be indexed, by its extension whatever its
// case: a project's .asm files are compiled too, its headers come in
// through the units that include them.
//
//////////////////////////////////////////////////////////////////////////
bool IsIndexedSourceFile(const char* szFileName)
{
	CChars	szName;

	szName.Fake(szFileName);
	return szName.EndsWithIgnoreCase(".cpp") || szName.EndsWithIgnoreCase(".cc") || szName.EndsWithIgnoreCase(".cxx") || szName.EndsWithIgnoreCase(".c");
}


//////////////////////////////////////////////////////////////////////////
//
// A worker thread indexing one file.
//
//////////////////////////////////////////////////////////////////////////
static void WorkspaceIndexJob(void* pvData)
{
	SWorkspaceIndexJob*	psJob;

	psJob = (SWorkspaceIndexJob*)pvData;
	psJob->pcIndex->IndexUnit(psJob);
}


//////////////////////////////////////////////////////////////////////////
//
// Until a configuration is chosen each solution's first is used, as
// Visual Studio does with a solution it hasn't opened before.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::Init(CBuildWorkspace* pcWorkspace)
{
	mbIndexTemplates = true;
	mpcWorkspace = pcWorkspace;
	mszConfiguration.Init();
	maszSystemDirectories.Init();
	mbSystemDirectoriesMissing = false;
	mszLibc65.Init();

	mapcProjects.Init();
	mcHeaderCache.Init();
	mcProjectsMutex.Init();

	mcIndex.Init();
	mcIndexMutex.Init();
	mbJobs = false;
	mapsJobs.Init();
	muiIndexed = 0;
	muiIndexings = 0;
	mbCancelled = false;
	mcIncluders.Init();
	maszProblems.Init();
	mcProblems.Init();
	muiProblemUnits = 0;
	miIndexStart = 0;
	miIndexEnd = -1;

	mapsPages.Init();
	mbPagesIndexed = true;
	mcSiteTable.Init(NULL);
	maszSiteFiles.Init();
	mszSiteRoot.Init();
	mbSiteSymbols = false;
	mszDocsSite.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// Waits for the indexing to finish first.  A subclass stops its own users
// of the projects first (KillJobs).
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::Kill(void)
{
	StopJobs();
	KillPages();
	mapsPages.Kill();
	mszDocsSite.Kill();
	mszSiteRoot.Kill();
	maszSiteFiles.Kill();
	mcSiteTable.Kill();
	mcProblems.Kill();
	maszProblems.Kill();
	mcIncluders.Kill();
	mcIndexMutex.Kill();
	mcIndex.Kill();

	KillProjects();
	mcProjectsMutex.Kill();
	mcHeaderCache.Kill();
	mapcProjects.Kill();

	mszLibc65.Kill();
	maszSystemDirectories.Kill();
	mszConfiguration.Kill();
	mpcWorkspace = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::KillProjects(void)
{
	size		i;
	CProject*	pcProject;

	for (i = 0; i < mapcProjects.NumElements(); i++)
	{
		pcProject = mapcProjects.GetPtr(i);
		SafeKill(pcProject);
	}
	mapcProjects.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Other threads that use the projects (the HCP65 IDE's edits thread) are
// waited for, and with bForget what they made is forgotten: it is of the
// projects and the index as they were.  Nothing else uses them here.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::WaitForProjectUsers(bool bForget)
{
}


//////////////////////////////////////////////////////////////////////////
//
// The jobs, and the projects' other users (WaitForProjectUsers), stopped.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::KillJobs(void)
{
	WaitForProjectUsers(true);
	StopJobs();
}


//////////////////////////////////////////////////////////////////////////
//
// The jobs that haven't started find they are cancelled, so only those
// being parsed are waited for.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::StopJobs(void)
{
	size				i;
	SWorkspaceIndexJob*	psJob;

	if (mbJobs)
	{
		mcIndexMutex.Lock();
		mbCancelled = true;
		mcIndexMutex.Unlock();
		mcJobs.Wait();
		mcJobs.Kill();
		mbJobs = false;
	}
	mbCancelled = false;
	for (i = 0; i < mapsJobs.NumElements(); i++)
	{
		psJob = mapsJobs.GetPtr(i);
		psJob->szFileName.Kill();
		free(psJob);
	}
	mapsJobs.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The compiler's own headers on this machine (FindSystemHeaderDirectories),
// those that exist; those that don't are logged, as without them almost
// nothing can be parsed (see IsMissingSystemDirectories).  A project has
// them after its own include directories, unless its solution is being
// read with an HCP65 configuration (see SetLibc65Directory).
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::AddDefaultSystemDirectories(void)
{
	if (!FindSystemHeaderDirectories(&maszSystemDirectories))
	{
		mbSystemDirectoriesMissing = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where the HCP65 compiler's own headers are: Libc65 (<stdint.h>).  A
// project whose solution is read with an HCP65 configuration (a W65C816's
// or a 65C64's) is indexed as hcp65build compiles it: these are its
// system headers, not this machine's, and it has the target's defines,
// not MSVC's (AddTargetDefines).  So the code in #ifdef __W65C816__ is
// what is indexed, the #ifdef _WIN32 beside it isn't, and a header the
// 65 hasn't isn't found on the host.  Said before anything is indexed.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::SetLibc65Directory(const char* szDirectory)
{
	CFileUtil	cFileUtil;

	mszLibc65.Set(szDirectory);
	if (!mszLibc65.Empty())
	{
		cFileUtil.FullPath(&mszLibc65);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The file (or szContents as it, if they aren't NULL) preprocessed with
// its project's include directories and defines, and parsed into pcDest,
// with the compile options its build configuration has (CBuildConfiguration
// ::SetCompileOptions) and its target (an int is 16 bits to the W65C816)
// for checking it.  False if the project isn't
// loaded.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::ParseUnit(size uiProject, const char* szFileName, const char* szContents, CParsedUnit* pcDest, CChars* pszErrors)
{
	CProject*				pcProject;
	CCPPParser				cParser;
	CBuildConfiguration*	pcConfiguration;

	pcDest->mpcUnit = AddUnit(uiProject, szFileName);
	if (pcDest->mpcUnit == NULL)
	{
		return false;
	}
	if (szContents)
	{
		pcDest->mpcUnit->SetContents(szContents);
		pcDest->mbContents = true;
	}

	mcProjectsMutex.Lock();
	pcProject = GetProject(uiProject);
	pcConfiguration = FindBuildConfiguration(mpcWorkspace->GetProject(uiProject)->uiSolution);
	if (pcConfiguration != NULL)
	{
		pcConfiguration->SetCompileOptions(&pcDest->mcOptions);
		pcDest->meTarget = pcConfiguration->GetTarget();
	}
	mcProjectsMutex.Unlock();
	pcProject->ProcessUnit(pcDest->mpcUnit, mszConfiguration.Text(), pszErrors);

	pcDest->mcTokeniser.Tokenise(pcDest->mpcUnit->GetTokens());
	pcDest->mcTree.Init(&pcDest->mcTokeniser);
	pcDest->mbTree = true;
	cParser.Init(&pcDest->mcTokeniser, &pcDest->mcTree);
	cParser.Parse();
	cParser.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The first indexed unit whose names were in szFileName, NULL if none
// has been indexed yet.  A unit is its own includer.
//
//////////////////////////////////////////////////////////////////////////
SWorkspaceIndexJob* CWorkspaceIndex::FindIncluder(const char* szFileName)
{
	int32*	piJob;
	int32	iJob;

	mcIndexMutex.Lock();
	piJob = mcIncluders.Get(szFileName);
	iJob = piJob ? *piJob : -1;
	mcIndexMutex.Unlock();
	return (iJob >= 0) ? mapsJobs.GetPtr((size)iJob) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The translation unit for szFileName in its project's library for the
// file's directory, which is made with the project's configuration the
// first time.  NULL if the project isn't loaded.
//
//////////////////////////////////////////////////////////////////////////
CTranslationUnit* CWorkspaceIndex::AddUnit(size uiProject, const char* szFileName)
{
	CVcxSolutionProject*	pcSolutionProject;
	CVcxProject*			pcVcxProject;
	CVcxConfiguration*		pcConfiguration;
	CProject*				pcProject;
	CLibrary*				pcLibrary;
	CConfig*				pcConfig;
	CTranslationUnit*		pcUnit;
	CArrayChars				aszDefines;
	CFileUtil				cFileUtil;
	CChars					szName;
	CChars					szDirectory;
	size					i;

	pcSolutionProject = mpcWorkspace->GetSolutionProject(uiProject);
	if (pcSolutionProject == NULL)
	{
		return NULL;
	}
	pcVcxProject = pcSolutionProject->GetProject();
	if (pcVcxProject == NULL)
	{
		return NULL;
	}

	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);

	mcProjectsMutex.Lock();
	pcProject = GetProject(uiProject);
	pcLibrary = pcProject->GetLibrary(szDirectory.Text());
	if (pcLibrary == NULL)
	{
		//Quoted includes look in the file's own directory first, and in the folders in it for one named with its folder
		//("Sub/Header.h").
		pcLibrary = pcProject->AddLibrary(szDirectory.Text(), szDirectory.Text(), true);
		pcConfig = pcLibrary->AddConfiguration(mszConfiguration.Text());
		pcConfig->mbHostDefines = false;
		pcConfiguration = GetProjectConfiguration(uiProject);
		AddTargetDefines(uiProject, pcConfiguration, pcConfig);
		aszDefines.Init();
		if (pcConfiguration != NULL)
		{
			pcVcxProject->GetPreprocessorDefinitions(pcConfiguration, mpcWorkspace->GetProjectSolution(uiProject)->GetDirectory(), &aszDefines);
		}
		for (i = 0; i < aszDefines.NumElements(); i++)
		{
			pcConfig->AddDefine(aszDefines.GetText(i));
		}
		aszDefines.Kill();
	}
	pcUnit = pcLibrary->AddTranslationUnit(szName.Text());
	mcProjectsMutex.Unlock();

	szDirectory.Kill();
	szName.Kill();
	return pcUnit;
}


//////////////////////////////////////////////////////////////////////////
//
// The CppCompilerLib project for the workspace's project uiProject, made
// the first time: the project's include directories, then the
// compiler's: this machine's, or Libc65 for a project read with an HCP65
// configuration.  Call it with the projects' mutex held.
//
//////////////////////////////////////////////////////////////////////////
CProject* CWorkspaceIndex::GetProject(size uiProject)
{
	CProject*				pcProject;
	CVcxSolutionProject*	pcSolutionProject;
	CVcxConfiguration*		pcConfiguration;
	CArrayChars				aszDirectories;
	CFileUtil				cFileUtil;
	size					i;

	while (mapcProjects.NumElements() <= uiProject)
	{
		mapcProjects.Add(NULL);
	}
	pcProject = mapcProjects.GetPtr(uiProject);
	if (pcProject)
	{
		return pcProject;
	}

	pcProject = NewMalloc<CProject>();
	pcProject->Init();
	pcProject->SetHeaderCache(&mcHeaderCache);

	//A header that can't be found (as the compiler's own often can't) costs only what it would have added.
	pcProject->SetSkipFailedDirectives(true);
	pcProject->SetFullPathErrors(true);  //The IDE's Indexing tab opens the header a problem is in, of those with its name.
	pcSolutionProject = mpcWorkspace->GetSolutionProject(uiProject);
	pcConfiguration = GetProjectConfiguration(uiProject);
	aszDirectories.Init();
	if (pcConfiguration != NULL)
	{
		pcSolutionProject->GetProject()->GetIncludeDirectories(pcConfiguration, mpcWorkspace->GetProjectSolution(uiProject)->GetDirectory(), &aszDirectories);
	}
	for (i = 0; i < aszDirectories.NumElements(); i++)
	{
		if (cFileUtil.IsDirectory(aszDirectories.GetText(i)))
		{
			pcProject->AddIncludeDirectory(aszDirectories.GetText(i), true, false);
		}
	}
	aszDirectories.Kill();
	if (FindHCP65Configuration(uiProject) != NULL)
	{
		if (!mszLibc65.Empty())
		{
			pcProject->AddIncludeDirectory(mszLibc65.Text(), false, true);
		}
	}
	else
	{
		for (i = 0; i < maszSystemDirectories.NumElements(); i++)
		{
			pcProject->AddIncludeDirectory(maszSystemDirectories.GetText(i), true, true);
		}
	}
	mapcProjects.SetPtr(uiProject, pcProject);
	return pcProject;
}


//////////////////////////////////////////////////////////////////////////
//
// Every .cpp and .c of every loaded project is indexed on worker threads,
// then (on the thread that indexed the last) the references in the
// projects' pages (IndexPages).  The index can be asked while it is being
// built; it is what has been added so far.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::StartIndexing(void)
{
	size				i;
	size				j;
	CVcxProject*		pcProject;
	CVcxItem*			pcItem;
	SWorkspaceIndexJob*	psJob;
	CChars				szMkdocsFile;

	KillJobs();
	mcIndexMutex.Lock();
	mcIndex.Kill();
	mcIndex.Init();
	mcIncluders.ReInit();
	maszProblems.Kill();
	maszProblems.Init();
	mcProblems.ReInit();
	muiProblemUnits = 0;
	miIndexStart = GetDebugTickCount();
	miIndexEnd = -1;
	muiIndexed = 0;
	muiIndexings++;
	mbPagesIndexed = false;
	mcIndexMutex.Unlock();
	szMkdocsFile.Init();
	if (FindDocsSite(&szMkdocsFile))
	{
		SetDocsSite(szMkdocsFile.Text());
	}
	szMkdocsFile.Kill();
	AddPages();

	for (i = 0; i < mpcWorkspace->NumProjects(); i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
		if (pcProject == NULL)
		{
			continue;
		}
		for (j = 0; j < pcProject->NumItems(); j++)
		{
			pcItem = pcProject->GetItem(j);
			if (pcItem->GetType() != VIT_ClCompile)
			{
				continue;
			}
			psJob = NewMalloc<SWorkspaceIndexJob>();
			psJob->szFileName.Init();
			pcProject->GetFullPath(pcItem, &psJob->szFileName);

			//Only C and C++ are indexed: an HCP65 project compiles its .asm files too.
			if (!IsIndexedSourceFile(psJob->szFileName.Text()))
			{
				psJob->szFileName.Kill();
				SafeFree(psJob);
				continue;
			}
			psJob->pcIndex = this;
			psJob->uiIndex = mapsJobs.NumElements();
			psJob->uiProject = i;
			mapsJobs.Add(psJob);
		}
	}

	//With no C++ to index only the pages are.
	if (mapsJobs.NumElements() == 0)
	{
		IndexPages();
		mcIndexMutex.Lock();
		mbPagesIndexed = true;
		miIndexEnd = GetDebugTickCount();
		mcIndexMutex.Unlock();
	}

	mcJobs.Init(WORKSPACE_INDEX_THREADS);
	mbJobs = true;
	for (i = 0; i < mapsJobs.NumElements(); i++)
	{
		mcJobs.Run(&WorkspaceIndexJob, mapsJobs.GetPtr(i));
	}
}


//The files other units indexed (in mcIncluders) while a unit is indexed: their names are skipped (CCPPIndex::Gather).
//Asked once per file and unit.
class CWorkspaceIndexedFiles : public CCPPIndexedFiles
{
public:
	CWorkspaceIndex*						mpcIndex;
	size									muiJob;
	CHashMapTemplate<CSourceFile*, bool>	mcAsked;

	void	Init(CWorkspaceIndex* pcIndex, size uiJob)
	{
		mpcIndex = pcIndex;
		muiJob = uiJob;
		mcAsked.Init();
	}

	void	Kill(void)
	{
		mcAsked.Kill();
	}

	bool	IsIndexed(CSourceFile* pcFile) override
	{
		bool*	pbAsked;
		bool	bIndexed;

		pbAsked = mcAsked.Get(&pcFile);
		if (pbAsked)
		{
			return *pbAsked;
		}
		bIndexed = mpcIndex->IsIndexedByAnother(pcFile->FullName(), muiJob);
		*mcAsked.Put(&pcFile) = bIndexed;
		return bIndexed;
	}
};


//////////////////////////////////////////////////////////////////////////
//
// Whether a unit other than job uiJob has indexed szFileName.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::IsIndexedByAnother(const char* szFileName, size uiJob)
{
	int32*	piJob;
	int32	iJob;

	mcIndexMutex.Lock();
	piJob = mcIncluders.Get(szFileName);
	iJob = piJob ? *piJob : -1;
	mcIndexMutex.Unlock();
	return (iJob >= 0) && ((size)iJob != uiJob);
}


//////////////////////////////////////////////////////////////////////////
//
// On a worker thread: one file parsed, its expressions typed (so names
// like pc->Init are resolved) and added to the index.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::IndexUnit(SWorkspaceIndexJob* psJob)
{
	CParsedUnit				cUnit;
	CCPPIndexUnit			cGathered;
	CWorkspaceIndexedFiles	cIndexed;
	CChars					szErrors;
	CArrayChars				aszErrors;
	bool					bCancelled;
	size					i;
	int32*					piJob;
	bool					bAdded;
	bool					bLast;

	mcIndexMutex.Lock();
	bCancelled = mbCancelled;
	mcIndexMutex.Unlock();
	if (bCancelled)
	{
		return;
	}

	//Parsed and gathered on this thread; only the merge waits for the other threads.
	szErrors.Init();
	cUnit.Init();
	cGathered.Init();
	if (ParseUnit(psJob->uiProject, psJob->szFileName.Text(), NULL, &cUnit, &szErrors))
	{
		//A header another unit indexed first isn't gathered again.
		cIndexed.Init(this, psJob->uiIndex);
		GatherUnit(&cUnit, &cGathered, &cIndexed);
		cIndexed.Kill();
	}
	cUnit.Kill();

	aszErrors.Init();
	aszErrors.Split(szErrors.Text(), '\n');

	mcIndexMutex.Lock();
	mcIndex.Merge(&cGathered);
	for (i = 0; i < cGathered.mcFiles.NumElements(); i++)
	{
		piJob = mcIncluders.FindOrAdd(cGathered.mcFiles.GetKey(i), cGathered.mcFiles.GetKeyLength(i), &bAdded);
		if (bAdded)
		{
			*piJob = (int32)psJob->uiIndex;
		}
	}

	//What it couldn't include (a header's problem is the same in every unit that includes it, so it is kept once).
	if (!szErrors.Empty())
	{
		muiProblemUnits++;
	}
	for (i = 0; i < aszErrors.NumElements(); i++)
	{
		if (aszErrors.GetText(i)[0] != '\0')
		{
			mcProblems.FindOrAddIndex(aszErrors.GetText(i), &bAdded);
			if (bAdded)
			{
				maszProblems.Add(aszErrors.GetText(i));
			}
		}
	}
	muiIndexed++;
	bLast = muiIndexed == mapsJobs.NumElements();
	mcIndexMutex.Unlock();

	//The last unit's thread resolves the pages' references, with all the C++ indexed.
	if (bLast)
	{
		IndexPages();
		mcIndexMutex.Lock();
		mbPagesIndexed = true;
		miIndexEnd = GetDebugTickCount();
		mcIndexMutex.Unlock();
	}

	aszErrors.Kill();
	cGathered.Kill();
	szErrors.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A parsed unit's expressions typed (so names like pc->Init are
// resolved) and its names gathered into pcDest, but for those in the
// files pcIndexed says are indexed already (NULL: all of them).
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::GatherUnit(CParsedUnit* pcUnit, CCPPIndexUnit* pcDest, CCPPIndexedFiles* pcIndexed)
{
	CCPPSemanticPass	cPass;

	cPass.Init();
	cPass.SetTemplatesAsThemselves(mbIndexTemplates);
	cPass.SetOptions(&pcUnit->mcOptions);
	cPass.Check(&pcUnit->mcTree, pcUnit->meTarget);
	CCPPIndex::Gather(&pcUnit->mcTokeniser, &pcUnit->mcTree, pcDest, &cPass, pcIndexed);
	cPass.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The header szFileName is preprocessed from szText (its editor's text)
// from now on, or from the file again if szText is NULL.  The headers'
// output kept for all the units that depends on it is forgotten first:
// it points into the header's old text.  Call it once the indexing has
// finished (IsIndexing).
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::SetHeaderText(const char* szFileName, const char* szText)
{
	size				i;
	CProject*			pcProject;
	CHeaderFile*		pcHeader;
	CSourceFile*		pcFile;
	CArrayPPHeaderFiles	apcHeaders;

	//Another user of the projects (the IDE's edits thread) may be preprocessing the header.
	WaitForProjectUsers(false);
	mcProjectsMutex.Lock();
	apcHeaders.Init();
	for (i = 0; i < mapcProjects.NumElements(); i++)
	{
		pcProject = mapcProjects.GetPtr(i);
		pcHeader = pcProject ? pcProject->GetIncludeFile((char*)szFileName) : NULL;
		if (pcHeader)
		{
			pcFile = pcHeader;
			apcHeaders.Add(&pcFile);
		}
	}
	mcHeaderCache.Forget(&apcHeaders);
	for (i = 0; i < apcHeaders.NumElements(); i++)
	{
		pcFile = *apcHeaders.Get(i);
		pcFile->Unload();
		if (szText)
		{
			pcFile->SetContents(szText);
		}
		else
		{
			pcFile->ClearContents();
		}
	}
	apcHeaders.Kill();
	mcProjectsMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// The project and file of the first unit indexed that included the
// header szFileName (or is szFileName).  False if none has.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::GetIncluder(const char* szFileName, size* puiProject, CChars* pszUnit)
{
	SWorkspaceIndexJob*	psJob;

	psJob = FindIncluder(szFileName);
	if (psJob == NULL)
	{
		return false;
	}
	*puiProject = psJob->uiProject;
	pszUnit->Set(psJob->szFileName);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Everything indexed again, a header having changed: its projects are
// made again (so their files are read again) and the headers'
// preprocessed output, kept for all the units, is forgotten.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::ReindexAll(void)
{
	Reset();
	mcHeaderCache.Kill();
	mcHeaderCache.Init();
	StartIndexing();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the index checks each template in its own words too (see
// mbIndexTemplates).  It is what the next indexing does.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::SetIndexTemplates(bool bIndexTemplates) { mbIndexTemplates = bIndexTemplates; }
bool CWorkspaceIndex::IsIndexingTemplates(void) const { return mbIndexTemplates; }


//////////////////////////////////////////////////////////////////////////
//
// Whether the index is still being built (StartIndexing): its units, then
// its pages.
//
//////////////////////////////////////////////////////////////////////////
bool CWorkspaceIndex::IsIndexing(void)
{
	bool	bIndexing;

	mcIndexMutex.Lock();
	bIndexing = (muiIndexed < mapsJobs.NumElements()) || !mbPagesIndexed;
	mcIndexMutex.Unlock();
	return bIndexing;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::WaitForIndexing(void)
{
	if (mbJobs)
	{
		mcJobs.Wait();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// How many of the solution's files have been indexed, and how many are
// being.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::GetIndexingProgress(size* puiIndexed, size* puiToIndex)
{
	mcIndexMutex.Lock();
	*puiIndexed = muiIndexed;
	*puiToIndex = mapsJobs.NumElements();
	mcIndexMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// What the units indexed since the last call couldn't include (or other
// preprocessor errors), each once in an indexing, added to paszDest.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::TakeIndexingProblems(CArrayChars* paszDest)
{
	size	i;

	mcIndexMutex.Lock();
	for (i = 0; i < maszProblems.NumElements(); i++)
	{
		paszDest->Add(maszProblems.GetText(i));
	}
	maszProblems.Kill();
	maszProblems.Init();
	mcIndexMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// How many of the units indexed so far had problems.
//
//////////////////////////////////////////////////////////////////////////
size CWorkspaceIndex::NumIndexingProblemUnits(void)
{
	size	uiUnits;

	mcIndexMutex.Lock();
	uiUnits = muiProblemUnits;
	mcIndexMutex.Unlock();
	return uiUnits;
}


//////////////////////////////////////////////////////////////////////////
//
// How long the index took to build, -1 while it is being.
//
//////////////////////////////////////////////////////////////////////////
int64 CWorkspaceIndex::GetIndexingMilliseconds(void)
{
	int64	iMilliseconds;

	mcIndexMutex.Lock();
	iMilliseconds = (miIndexEnd >= 0) ? (miIndexEnd - miIndexStart) : -1;
	mcIndexMutex.Unlock();
	return iMilliseconds;
}


//////////////////////////////////////////////////////////////////////////
//
// The project configuration its solution's configuration builds, or the
// project's first if the solution doesn't say.
//
//////////////////////////////////////////////////////////////////////////
CVcxConfiguration* CWorkspaceIndex::GetProjectConfiguration(size uiProject)
{
	CVcxSolutionProjectConfiguration*	pcSolutionConfiguration;
	CVcxConfiguration*					pcConfiguration;
	CVcxSolutionProject*				pcProject;
	CVcxProject*						pcVcxProject;
	const char*							szSolutionConfiguration;

	pcProject = mpcWorkspace->GetSolutionProject(uiProject);
	pcVcxProject = pcProject->GetProject();
	szSolutionConfiguration = GetSolutionConfiguration(mpcWorkspace->GetProject(uiProject)->uiSolution);
	pcSolutionConfiguration = (szSolutionConfiguration != NULL) ? pcProject->FindConfiguration(szSolutionConfiguration) : NULL;
	if (pcSolutionConfiguration != NULL)
	{
		pcConfiguration = pcVcxProject->FindConfiguration(pcSolutionConfiguration->GetProjectConfiguration());
		if (pcConfiguration != NULL)
		{
			return pcConfiguration;
		}
	}
	if (pcVcxProject->NumConfigurations() > 0)
	{
		return pcVcxProject->GetConfiguration(0);
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The solution configuration ("Release|Win32") a solution builds for the
// build configuration: its build configuration of that name's, or its
// first's if it hasn't one, or the solution's own first.  NULL if it has
// none.
//
//////////////////////////////////////////////////////////////////////////
const char* CWorkspaceIndex::GetSolutionConfiguration(size uiSolution)
{
	CBuildWorkspaceSolution*	pcSolution;
	CBuildConfiguration*		pcConfiguration;

	pcSolution = mpcWorkspace->GetSolution(uiSolution);
	pcConfiguration = FindBuildConfiguration(uiSolution);
	if (pcConfiguration != NULL)
	{
		return pcConfiguration->GetSolutionConfiguration();
	}
	return (pcSolution->mcSolution.NumConfigurations() > 0) ? pcSolution->mcSolution.GetConfiguration(0) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The build configuration a solution is read with: its build configuration
// of the index's configuration's name, or its first if it hasn't one.
// NULL if it has none.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CWorkspaceIndex::FindBuildConfiguration(size uiSolution)
{
	CBuildWorkspaceSolution*	pcSolution;
	CBuildConfiguration*		pcConfiguration;

	pcSolution = mpcWorkspace->GetSolution(uiSolution);
	pcConfiguration = mpcWorkspace->FindConfiguration(uiSolution, mszConfiguration.Text());
	if ((pcConfiguration == NULL) && (pcSolution->mcConfigurations.NumConfigurations() > 0))
	{
		pcConfiguration = pcSolution->mcConfigurations.GetConfiguration(0);
	}
	return pcConfiguration;
}


//////////////////////////////////////////////////////////////////////////
//
// The build configuration the project's solution is read with, if it is
// an HCP65 one (the W65C816's or the 65C64's); NULL if it is the host's.
//
//////////////////////////////////////////////////////////////////////////
CBuildConfiguration* CWorkspaceIndex::FindHCP65Configuration(size uiProject)
{
	CBuildConfiguration*	pcConfiguration;

	pcConfiguration = FindBuildConfiguration(mpcWorkspace->GetProject(uiProject)->uiSolution);
	return ((pcConfiguration != NULL) && pcConfiguration->IsHCP65()) ? pcConfiguration : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// What the compiler that builds the project defines itself: the target's
// own for an HCP65 configuration, from the code the build uses
// (CCPPTarget::AddDefines, with the configuration's compile options), so
// the two can't differ; MSVC's for any other.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::AddTargetDefines(size uiProject, CVcxConfiguration* pcConfiguration, CConfig* pcConfig)
{
	CBuildConfiguration*	pcBuild;
	CCPPCompileOptions		cOptions;

	pcBuild = FindHCP65Configuration(uiProject);
	if (pcBuild == NULL)
	{
		AddCompilerDefines(pcConfiguration, pcConfig);
		return;
	}
	cOptions = *GetDefaultCPPCompileOptions();
	pcBuild->SetCompileOptions(&cOptions);
	GetCPPTarget(pcBuild->GetTarget())->AddDefines(pcConfig, &cOptions);
}


//////////////////////////////////////////////////////////////////////////
//
// What MSVC defines itself for the configuration's platform, whatever
// machine it runs on; on Linux, where MinGW's headers stand in for the
// Windows SDK's, GCC's name for the target too.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::AddCompilerDefines(CVcxConfiguration* pcConfiguration, CConfig* pcConfig)
{
	pcConfig->AddDefine("_MSC_VER=1929");
	pcConfig->AddDefine("_MSC_FULL_VER=192930153");
	pcConfig->AddDefine("_WIN32=1");
	pcConfig->AddDefine("_INTEGRAL_MAX_BITS=64");
	pcConfig->AddDefine("_MT=1");
	pcConfig->AddDefine("_CPPRTTI=1");
	pcConfig->AddDefine("_CPPUNWIND=1");
	pcConfig->AddDefine("_NATIVE_WCHAR_T_DEFINED=1");
	pcConfig->AddDefine("_WCHAR_T_DEFINED=1");
	if ((pcConfiguration != NULL) && (StringInsensitiveCompare(pcConfiguration->GetPlatform(), "x64") == 0))
	{
		pcConfig->AddDefine("_WIN64=1");
		pcConfig->AddDefine("_M_X64=100");
		pcConfig->AddDefine("_M_AMD64=100");
#ifdef LINUX_GNU
		//MinGW's headers, standing in for the Windows SDK's, know the target by GCC's names for it (winnt.h stops with
		//"#error No supported target architecture" without them).
		pcConfig->AddDefine("__x86_64__=1");
		pcConfig->AddDefine("__amd64__=1");
#endif
	}
	else
	{
		pcConfig->AddDefine("_M_IX86=600");
#ifdef LINUX_GNU
		pcConfig->AddDefine("__i386__=1");
#endif
	}
	if ((pcConfiguration != NULL) && (StringInsensitiveCompare(pcConfiguration->GetConfiguration(), "Debug") == 0))
	{
		pcConfig->AddDefine("_DEBUG=1");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration can only change while nothing is being indexed: the
// projects are made again with it.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::SetConfiguration(const char* szConfiguration)
{
	Reset();
	mszConfiguration.Set(szConfiguration);
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is being indexed and the projects will be made again: before
// the workspace's solutions change, as the jobs use them.  StartIndexing
// indexes them again.  A subclass forgets what it found of the projects
// too.
//
//////////////////////////////////////////////////////////////////////////
void CWorkspaceIndex::Reset(void)
{
	KillJobs();
	mcProjectsMutex.Lock();
	KillProjects();
	mcProjectsMutex.Unlock();
	mcIndexMutex.Lock();
	KillPages();
	mbPagesIndexed = true;
	mcIndexMutex.Unlock();
}


char* CWorkspaceIndex::GetConfiguration(void) { return mszConfiguration.Text(); }
size CWorkspaceIndex::NumIndexings(void) const { return muiIndexings; }
void CWorkspaceIndex::AddSystemDirectory(const char* szDirectory) { maszSystemDirectories.Add(szDirectory); }
CArrayChars* CWorkspaceIndex::GetSystemDirectories(void) { return &maszSystemDirectories; }
bool CWorkspaceIndex::IsMissingSystemDirectories(void) const { return mbSystemDirectoriesMissing; }
CCPPIndex* CWorkspaceIndex::GetIndex(void) { return &mcIndex; }

