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
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "BaseLib/StringHelper.h"
#include "CppParserLib/CppParser.h"
#include "IDECodeModel.h"


//Each thread indexing has a whole parsed unit; more than this many is no faster, and uses more memory.
#define IDE_INDEX_THREADS	8


//The Windows SDK and MSVC library headers aren't on Linux; the MinGW ones stand in for them.
const char* gaszLinuxSystemDirectories[] =
{
	"/usr/lib/gcc/x86_64-w64-mingw32/13-win32/include/c++",
	"/usr/lib/gcc/x86_64-w64-mingw32/13-win32/include/c++/x86_64-w64-mingw32",
	"/usr/lib/gcc/x86_64-w64-mingw32/13-win32/include",
	"/usr/share/mingw-w64/include",
	NULL
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEParsedUnit::Init(void)
{
	mpcUnit = NULL;
	mcTokeniser.Init();
	mbTree = false;
	mbContents = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The unit's preprocessed tokens and its contents are freed too (a unit
// keeps its tokens otherwise); it can be processed again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEParsedUnit::Kill(void)
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
//
//////////////////////////////////////////////////////////////////////////
void KillIDELocations(CArrayIDELocations* pasLocations)
{
	size	i;

	for (i = 0; i < pasLocations->NumElements(); i++)
	{
		pasLocations->Get(i)->szFileName.Kill();
	}
	pasLocations->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A worker thread indexing one file.
//
//////////////////////////////////////////////////////////////////////////
void IDEIndexJob(void* pvData)
{
	SIDEIndexJob*	psJob;

	psJob = (SIDEIndexJob*)pvData;
	psJob->pcModel->IndexUnit(psJob);
}


//////////////////////////////////////////////////////////////////////////
//
// The first of the solution's configurations is used until another is
// chosen, as Visual Studio does with a solution it hasn't opened before.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Init(CVcxSolution* pcSolution)
{
	mpcSolution = pcSolution;
	mszConfiguration.Init();
	if (pcSolution->NumConfigurations() > 0)
	{
		mszConfiguration.Set(pcSolution->GetConfiguration(0));
	}
	maszSystemDirectories.Init();

	mapcProjects.Init();
	mcHeaderCache.Init();
	mcProjectsMutex.Init();

	mcIndex.Init();
	mcIndexMutex.Init();
	mbJobs = false;
	mapsJobs.Init();
	muiIndexed = 0;
	mbCancelled = false;
	mcIncluders.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// Waits for the indexing to finish first.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Kill(void)
{
	KillJobs();
	mcIncluders.Kill();
	mcIndexMutex.Kill();
	mcIndex.Kill();

	KillProjects();
	mcProjectsMutex.Kill();
	mcHeaderCache.Kill();
	mapcProjects.Kill();

	maszSystemDirectories.Kill();
	mszConfiguration.Kill();
	mpcSolution = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::KillProjects(void)
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
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::KillJobs(void)
{
	size			i;
	SIDEIndexJob*	psJob;

	//The jobs that haven't started find they are cancelled, so only those being parsed are waited for.
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
// The compiler's own headers on this machine, those that exist.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::AddDefaultSystemDirectories(void)
{
#ifdef LINUX_GNU
	CFileUtil	cFileUtil;
	size		i;

	for (i = 0; gaszLinuxSystemDirectories[i] != NULL; i++)
	{
		if (cFileUtil.IsDirectory(gaszLinuxSystemDirectories[i]))
		{
			AddSystemDirectory(gaszLinuxSystemDirectories[i]);
		}
	}
#endif
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName (a full path) preprocessed and parsed as a file of the
// solution's project uiProject, and its highlights put in pacDest.  A
// header is highlighted from a unit that includes it, if one has been
// indexed.  The
// preprocessor's errors are appended to pszErrors.  False if the project
// isn't loaded; a file with errors is still highlighted as far as it
// could be parsed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::Highlight(size uiProject, const char* szFileName, const char* szContents, CArrayHighlights* pacDest, CChars* pszErrors)
{
	CIDEParsedUnit	cUnit;
	CCPPHighlighter	cHighlighter;
	size			i;
	SIDEIndexJob*	psIncluder;
	CSourceFile*	pcFile;

	//A header is parsed as part of a unit that includes it, with what comes before it there.  Until a unit that
	//includes it has been indexed, or while it is being edited (szContents, the editor's text), it is parsed on its
	//own.
	cUnit.Init();
	pcFile = NULL;
	psIncluder = szContents ? NULL : FindIncluder(szFileName);
	if (psIncluder && ParseUnit(psIncluder->uiProject, psIncluder->szFileName.Text(), NULL, &cUnit, pszErrors))
	{
		pcFile = FindFileInUnit(&cUnit, szFileName);
		if (pcFile == NULL)
		{
			cUnit.Kill();
			cUnit.Init();
		}
	}
	if (pcFile == NULL)
	{
		if (!ParseUnit(uiProject, szFileName, szContents, &cUnit, pszErrors))
		{
			cUnit.Kill();
			return false;
		}
		pcFile = cUnit.mpcUnit;
	}

	cHighlighter.Init();
	cHighlighter.Highlight(pcFile, cUnit.mpcUnit->GetTokens(), &cUnit.mcTokeniser, &cUnit.mcTree);
	for (i = 0; i < cHighlighter.NumHighlights(); i++)
	{
		pacDest->Add(cHighlighter.Get(i));
	}
	cHighlighter.Kill();
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The file (or szContents as it, if they aren't NULL) preprocessed with
// its project's include directories and defines, and parsed into pcDest.
// False if the project isn't loaded.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::ParseUnit(size uiProject, const char* szFileName, const char* szContents, CIDEParsedUnit* pcDest, CChars* pszErrors)
{
	CProject*	pcProject;
	CCPPParser	cParser;

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
SIDEIndexJob* CIDECodeModel::FindIncluder(const char* szFileName)
{
	int	iJob;

	mcIndexMutex.Lock();
	iJob = mcIncluders.Get(szFileName, -1);
	mcIndexMutex.Unlock();
	return (iJob >= 0) ? mapsJobs.GetPtr((size)iJob) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The file a parsed unit's tokens came from that is named szFileName,
// NULL if none of its tokens did.
//
//////////////////////////////////////////////////////////////////////////
CSourceFile* CIDECodeModel::FindFileInUnit(CIDEParsedUnit* pcUnit, const char* szFileName)
{
	size				i;
	SPreprocessedToken*	psSource;
	CSourceFile*		pcLast;

	pcLast = NULL;
	for (i = 0; i < pcUnit->mcTokeniser.NumTokens(); i++)
	{
		psSource = pcUnit->mcTokeniser.GetSource(pcUnit->mcTokeniser.Get(i));
		if (psSource->pcFile && (psSource->pcFile != pcLast))
		{
			pcLast = psSource->pcFile;
			if (StringCompare(pcLast->FullName(), szFileName) == 0)
			{
				return pcLast;
			}
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The translation unit for szFileName in its project's library for the
// file's directory, which is made with the project's configuration the
// first time.  NULL if the project isn't loaded.
//
//////////////////////////////////////////////////////////////////////////
CTranslationUnit* CIDECodeModel::AddUnit(size uiProject, const char* szFileName)
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

	if (uiProject >= mpcSolution->NumProjects())
	{
		return NULL;
	}
	pcSolutionProject = mpcSolution->GetProject(uiProject);
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
		//Quoted includes look in the file's own directory first.
		pcLibrary = pcProject->AddLibrary(szDirectory.Text(), szDirectory.Text(), false);
		pcConfig = pcLibrary->AddConfiguration(mszConfiguration.Text());
		pcConfig->mbHostDefines = false;
		pcConfiguration = GetProjectConfiguration(pcSolutionProject);
		AddCompilerDefines(pcConfiguration, pcConfig);
		aszDefines.Init();
		if (pcConfiguration != NULL)
		{
			pcVcxProject->GetPreprocessorDefinitions(pcConfiguration, mpcSolution->GetDirectory(), &aszDefines);
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
// The CppParserLib project for the solution's project uiProject, made
// the first time: the project's include directories, then the
// compiler's.  Call it with the projects' mutex held.
//
//////////////////////////////////////////////////////////////////////////
CProject* CIDECodeModel::GetProject(size uiProject)
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
	pcSolutionProject = mpcSolution->GetProject(uiProject);
	pcConfiguration = GetProjectConfiguration(pcSolutionProject);
	aszDirectories.Init();
	if (pcConfiguration != NULL)
	{
		pcSolutionProject->GetProject()->GetIncludeDirectories(pcConfiguration, mpcSolution->GetDirectory(), &aszDirectories);
	}
	for (i = 0; i < aszDirectories.NumElements(); i++)
	{
		if (cFileUtil.IsDirectory(aszDirectories.GetText(i)))
		{
			pcProject->AddIncludeDirectory(aszDirectories.GetText(i), true, false);
		}
	}
	aszDirectories.Kill();
	for (i = 0; i < maszSystemDirectories.NumElements(); i++)
	{
		pcProject->AddIncludeDirectory(maszSystemDirectories.GetText(i), true, true);
	}
	mapcProjects.SetPtr(uiProject, pcProject);
	return pcProject;
}


//////////////////////////////////////////////////////////////////////////
//
// Every .cpp of every loaded project is indexed on worker threads.  The
// index can be asked while it is being built; it is what has been added
// so far.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::StartIndexing(void)
{
	size					i;
	size					j;
	CVcxProject*			pcProject;
	CVcxItem*				pcItem;
	SIDEIndexJob*			psJob;

	KillJobs();
	mcIndexMutex.Lock();
	mcIndex.Kill();
	mcIndex.Init();
	mcIncluders.Kill();
	mcIncluders.Init();
	muiIndexed = 0;
	mcIndexMutex.Unlock();

	for (i = 0; i < mpcSolution->NumProjects(); i++)
	{
		pcProject = mpcSolution->GetProject(i)->GetProject();
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
			psJob = NewMalloc<SIDEIndexJob>();
			psJob->pcModel = this;
			psJob->uiIndex = mapsJobs.NumElements();
			psJob->uiProject = i;
			psJob->szFileName.Init();
			pcProject->GetFullPath(pcItem, &psJob->szFileName);
			mapsJobs.Add(psJob);
		}
	}

	mcJobs.Init(IDE_INDEX_THREADS);
	mbJobs = true;
	for (i = 0; i < mapsJobs.NumElements(); i++)
	{
		mcJobs.Run(&IDEIndexJob, mapsJobs.GetPtr(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// On a worker thread: one file parsed and added to the index.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::IndexUnit(SIDEIndexJob* psJob)
{
	CIDEParsedUnit	cUnit;
	CCPPIndexUnit	cGathered;
	CChars			szErrors;
	bool			bParsed;
	bool			bCancelled;
	size			i;

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
	bParsed = ParseUnit(psJob->uiProject, psJob->szFileName.Text(), NULL, &cUnit, &szErrors);
	if (bParsed)
	{
		CCPPIndex::Gather(&cUnit.mcTokeniser, &cUnit.mcTree, &cGathered);
	}
	cUnit.Kill();

	mcIndexMutex.Lock();
	mcIndex.Merge(&cGathered);
	for (i = 0; i < cGathered.maszFiles.NumElements(); i++)
	{
		if (mcIncluders.Get(cGathered.maszFiles.GetText(i), -1) < 0)
		{
			mcIncluders.Put(cGathered.maszFiles.GetText(i), (int)psJob->uiIndex);
		}
	}
	muiIndexed++;
	mcIndexMutex.Unlock();

	cGathered.Kill();
	szErrors.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::WaitForIndexing(void)
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
void CIDECodeModel::GetIndexingProgress(size* puiIndexed, size* puiToIndex)
{
	mcIndexMutex.Lock();
	*puiIndexed = muiIndexed;
	*puiToIndex = mapsJobs.NumElements();
	mcIndexMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// Where what is named at a place (0 based) is defined, or declared if
// no file indexed so far defines it.  False if nothing indexed is named
// there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindDefinition(const char* szFileName, int iLine, int iColumn, SIDELocation* psDest)
{
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;

	psLocation = NULL;
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		psLocation = pcSymbol->GetDefinition();
		if (psLocation == NULL)
		{
			psLocation = pcSymbol->GetDeclaration();
		}
		if (psLocation)
		{
			CopyLocation(psLocation, psDest);
		}
	}
	mcIndexMutex.Unlock();
	return psLocation != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place what is named at a place is declared, defined and used,
// added to pasDest, and its key to pszKey.  False if nothing indexed is
// named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindReferences(const char* szFileName, int iLine, int iColumn, CArrayIDELocations* pasDest, CChars* pszKey)
{
	CCPPIndexSymbol*	pcSymbol;
	size				i;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		pszKey->Append(pcSymbol->mszKey);
		for (i = 0; i < pcSymbol->NumLocations(); i++)
		{
			CopyLocation(pcSymbol->GetLocation(i), pasDest->Add());
		}
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Call it with the index's mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::CopyLocation(SCPPIndexLocation* psLocation, SIDELocation* psDest)
{
	psDest->szFileName.Init(mcIndex.GetFileName(psLocation->uiFile));
	psDest->iLine = psLocation->iLine;
	psDest->iColumn = psLocation->iColumn;
	psDest->iLength = psLocation->iLength;
	psDest->uiRole = psLocation->uiRole;
	psDest->uiFlags = psLocation->uiFlags;
}


//////////////////////////////////////////////////////////////////////////
//
// The first of the solution's projects that has szFileName (a full
// path, whatever its case) as an item, MAX_SIZE if none has.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindProjectOfFile(const char* szFileName)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CChars			szPath;
	bool			bFound;

	for (i = 0; i < mpcSolution->NumProjects(); i++)
	{
		pcProject = mpcSolution->GetProject(i)->GetProject();
		if (pcProject == NULL)
		{
			continue;
		}
		for (j = 0; j < pcProject->NumItems(); j++)
		{
			szPath.Init();
			pcProject->GetFullPath(pcProject->GetItem(j), &szPath);
			bFound = szPath.EqualsIgnoreCase(szFileName);
			szPath.Kill();
			if (bFound)
			{
				return i;
			}
		}
	}
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The project configuration the solution configuration builds, or the
// project's first if the solution doesn't say.
//
//////////////////////////////////////////////////////////////////////////
CVcxConfiguration* CIDECodeModel::GetProjectConfiguration(CVcxSolutionProject* pcProject)
{
	CVcxSolutionProjectConfiguration*	pcSolutionConfiguration;
	CVcxConfiguration*					pcConfiguration;
	CVcxProject*						pcVcxProject;

	pcVcxProject = pcProject->GetProject();
	pcSolutionConfiguration = pcProject->FindConfiguration(mszConfiguration.Text());
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
// What MSVC defines itself for the configuration's platform, whatever
// machine the IDE runs on.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::AddCompilerDefines(CVcxConfiguration* pcConfiguration, CConfig* pcConfig)
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
	}
	else
	{
		pcConfig->AddDefine("_M_IX86=600");
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
void CIDECodeModel::SetConfiguration(const char* szConfiguration)
{
	KillJobs();
	mcProjectsMutex.Lock();
	KillProjects();
	mszConfiguration.Set(szConfiguration);
	mcProjectsMutex.Unlock();
}


char* CIDECodeModel::GetConfiguration(void) { return mszConfiguration.Text(); }
void CIDECodeModel::AddSystemDirectory(const char* szDirectory) { maszSystemDirectories.Add(szDirectory); }

