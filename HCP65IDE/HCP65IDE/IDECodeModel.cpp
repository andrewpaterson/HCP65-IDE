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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/PointerFunctions.h"
#include "BaseLib/StringHelper.h"
#include "CppParserLib/CppParser.h"
#include "IDECodeModel.h"


//Each thread indexing has a whole parsed unit; more than this many is no faster, and uses more memory.
#define IDE_INDEX_THREADS	8


//The Windows SDK and MSVC library headers aren't on Linux; the MinGW ones stand in for them: these in the folder of the
//newest MinGW GCC installed, then its C library's.
#define IDE_MINGW_GCC_DIRECTORY		"/usr/lib/gcc/x86_64-w64-mingw32"
#define IDE_MINGW_INCLUDE_DIRECTORY	"/usr/share/mingw-w64/include"
const char* gaszMinGWGCCDirectories[] =
{
	"include/c++",
	"include/c++/x86_64-w64-mingw32",
	"include",
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
// Until a configuration is chosen each solution's first is used, as
// Visual Studio does with a solution it hasn't opened before.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Init(CBuildWorkspace* pcWorkspace)
{
	mpcWorkspace = pcWorkspace;
	mszConfiguration.Init();
	maszSystemDirectories.Init();

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

	mbEdits = false;
	mapsEdited.Init();
	mbEditsBusy = false;
	miEditsDelay = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Waits for the indexing to finish first.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Kill(void)
{
	KillJobs();
	if (mbEdits)
	{
		mcEdits.Kill();
		mbEdits = false;
	}
	mapsEdited.Kill();
	mcIncluders.Kill();
	mcIndexMutex.Kill();
	mcIndex.Kill();

	KillProjects();
	mcProjectsMutex.Kill();
	mcHeaderCache.Kill();
	mapcProjects.Kill();

	maszSystemDirectories.Kill();
	mszConfiguration.Kill();
	mpcWorkspace = NULL;
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

	//The edits thread uses the projects and the jobs (GetIncluder) too.  What it made is forgotten: it is of the
	//projects and the index as they were.
	WaitForEditedFiles();
	KillIDEEditedFiles(&mapsEdited);

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


#ifdef LINUX_GNU
//////////////////////////////////////////////////////////////////////////
//
// Whether the MinGW GCC folder called szName ("13-win32") is newer than
// the one called szOther, NULL for none.  A folder whose name doesn't start
// with a version isn't.  Of one version, win32 threads are preferred.
//
//////////////////////////////////////////////////////////////////////////
static bool IsNewerMinGWVersion(const char* szName, const char* szOther)
{
	int	aiName[3] = { 0, 0, 0 };
	int	aiOther[3] = { 0, 0, 0 };
	int	i;

	if (sscanf(szName, "%d.%d.%d", &aiName[0], &aiName[1], &aiName[2]) < 1)
	{
		return false;
	}
	if (szOther == NULL)
	{
		return true;
	}
	sscanf(szOther, "%d.%d.%d", &aiOther[0], &aiOther[1], &aiOther[2]);
	for (i = 0; i < 3; i++)
	{
		if (aiName[i] != aiOther[i])
		{
			return aiName[i] > aiOther[i];
		}
	}
	return (strstr(szName, "win32") != NULL) && (strstr(szOther, "win32") == NULL);
}
#endif


//////////////////////////////////////////////////////////////////////////
//
// The compiler's own headers on this machine, those that exist.  For
// checking, HCP65_IDE_SYSTEM_ROOT is where they are looked for in place of
// the root folder.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::AddDefaultSystemDirectories(void)
{
#ifdef LINUX_GNU
	CFileUtil	cFileUtil;
	CArrayChars	aszFound;
	CChars		szRoot;
	CChars		szPath;
	CChars		szName;
	CChars		szNewest;
	CChars		szParent;
	size		i;

	szRoot.Init((getenv("HCP65_IDE_SYSTEM_ROOT") != NULL) ? getenv("HCP65_IDE_SYSTEM_ROOT") : "");
	szPath.Init();
	szName.Init();
	szParent.Init();
	szNewest.Init();
	aszFound.Init();
	szPath.Set(szRoot);
	szPath.Append(IDE_MINGW_GCC_DIRECTORY);
	if (cFileUtil.IsDirectory(szPath.Text()))
	{
		cFileUtil.FindAllDirectories(szPath.Text(), &aszFound);
	}
	for (i = 0; i < aszFound.NumElements(); i++)
	{
		szName.Set("");
		szParent.Set("");
		cFileUtil.SplitPath(aszFound.GetText(i), &szName, &szParent);
		if (IsNewerMinGWVersion(szName.Text(), szNewest.Empty() ? NULL : szNewest.Text()))
		{
			szNewest.Set(szName);
		}
	}
	for (i = 0; !szNewest.Empty() && (gaszMinGWGCCDirectories[i] != NULL); i++)
	{
		szName.Set(szPath);
		szName.Append('/');
		szName.Append(szNewest);
		szName.Append('/');
		szName.Append(gaszMinGWGCCDirectories[i]);
		if (cFileUtil.IsDirectory(szName.Text()))
		{
			AddSystemDirectory(szName.Text());
		}
	}
	szName.Set(szRoot);
	szName.Append(IDE_MINGW_INCLUDE_DIRECTORY);
	if (cFileUtil.IsDirectory(szName.Text()))
	{
		AddSystemDirectory(szName.Text());
	}
	aszFound.Kill();
	szNewest.Kill();
	szParent.Kill();
	szName.Kill();
	szPath.Kill();
	szRoot.Kill();
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

	HighlightFile(&cUnit, pcFile, pacDest);
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The highlights of pcFile (the unit's own file or a header it included)
// added to pacDest.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::HighlightFile(CIDEParsedUnit* pcUnit, CSourceFile* pcFile, CArrayHighlights* pacDest)
{
	CCPPHighlighter	cHighlighter;
	size			i;

	cHighlighter.Init();
	cHighlighter.Highlight(pcFile, pcUnit->mpcUnit->GetTokens(), &pcUnit->mcTokeniser, &pcUnit->mcTree);
	for (i = 0; i < cHighlighter.NumHighlights(); i++)
	{
		pacDest->Add(cHighlighter.Get(i));
	}
	cHighlighter.Kill();
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
		//Quoted includes look in the file's own directory first.
		pcLibrary = pcProject->AddLibrary(szDirectory.Text(), szDirectory.Text(), false);
		pcConfig = pcLibrary->AddConfiguration(mszConfiguration.Text());
		pcConfig->mbHostDefines = false;
		pcConfiguration = GetProjectConfiguration(uiProject);
		AddCompilerDefines(pcConfiguration, pcConfig);
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
// The CppParserLib project for the workspace's project uiProject, made
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
	muiIndexings++;
	mcIndexMutex.Unlock();

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
			psJob = NewMalloc<SIDEIndexJob>();
			psJob->szFileName.Init();
			pcProject->GetFullPath(pcItem, &psJob->szFileName);

			//Only C and C++ are indexed: an HCP65 project compiles its .asm files too.
			if ((CIDEIcons::GetFileType(psJob->szFileName.Text()) != IFT_CPP) && (CIDEIcons::GetFileType(psJob->szFileName.Text()) != IFT_C))
			{
				psJob->szFileName.Kill();
				SafeFree(psJob);
				continue;
			}
			psJob->pcModel = this;
			psJob->uiIndex = mapsJobs.NumElements();
			psJob->uiProject = i;
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


//The files other units indexed (in mcIncluders) while a unit is indexed: their names are skipped (CCPPIndex::Gather).
//Asked once per file and unit.
class CIDEIndexedFiles : public CCPPIndexedFiles
{
public:
	CIDECodeModel*			mpcModel;
	size					muiJob;
	CMapLongTemplate<bool>	mcAsked;

	void	Init(CIDECodeModel* pcModel, size uiJob)
	{
		mpcModel = pcModel;
		muiJob = uiJob;
		mcAsked.Init(true);
	}

	void	Kill(void)
	{
		mcAsked.Kill();
	}

	bool	IsIndexed(CSourceFile* pcFile) override
	{
		bool*	pbAsked;
		bool	bIndexed;

		pbAsked = mcAsked.Get((int64)(size)pcFile);
		if (pbAsked)
		{
			return *pbAsked;
		}
		bIndexed = mpcModel->IsIndexedByAnother(pcFile->FullName(), muiJob);
		mcAsked.Put((int64)(size)pcFile, &bIndexed);
		return bIndexed;
	}
};


//////////////////////////////////////////////////////////////////////////
//
// Whether a unit other than job uiJob has indexed szFileName.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::IsIndexedByAnother(const char* szFileName, size uiJob)
{
	int	iJob;

	mcIndexMutex.Lock();
	iJob = mcIncluders.Get(szFileName, -1);
	mcIndexMutex.Unlock();
	return (iJob >= 0) && ((size)iJob != uiJob);
}


//////////////////////////////////////////////////////////////////////////
//
// On a worker thread: one file parsed, its expressions typed (so names
// like pc->Init are resolved) and added to the index.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::IndexUnit(SIDEIndexJob* psJob)
{
	CIDEParsedUnit		cUnit;
	CCPPIndexUnit		cGathered;
	CIDEIndexedFiles	cIndexed;
	CChars				szErrors;
	bool				bCancelled;
	size				i;

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
// A parsed unit's expressions typed (so names like pc->Init are
// resolved) and its names gathered into pcDest, but for those in the
// files pcIndexed says are indexed already (NULL: all of them).
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::GatherUnit(CIDEParsedUnit* pcUnit, CCPPIndexUnit* pcDest, CCPPIndexedFiles* pcIndexed)
{
	CCPPSemanticPass	cPass;

	cPass.Init();
	cPass.Check(&pcUnit->mcTree);
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
void CIDECodeModel::SetHeaderText(const char* szFileName, const char* szText)
{
	size				i;
	CProject*			pcProject;
	CHeaderFile*		pcHeader;
	CSourceFile*		pcFile;
	CArrayPPHeaderFiles	apcHeaders;

	//The edits thread may be preprocessing the header.
	WaitForEditedFiles();
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
bool CIDECodeModel::GetIncluder(const char* szFileName, size* puiProject, CChars* pszUnit)
{
	SIDEIndexJob*	psJob;

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
void CIDECodeModel::ReindexAll(void)
{
	Reset();
	mcHeaderCache.Kill();
	mcHeaderCache.Init();
	StartIndexing();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the index is still being built (StartIndexing).
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::IsIndexing(void)
{
	size	uiIndexed;
	size	uiToIndex;

	GetIndexingProgress(&uiIndexed, &uiToIndex);
	return uiIndexed < uiToIndex;
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
	psDest->iContainer = psLocation->iContainer;
}


//////////////////////////////////////////////////////////////////////////
//
// The first of the workspace's projects that has szFileName (a full
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

	for (i = 0; i < mpcWorkspace->NumProjects(); i++)
	{
		pcProject = mpcWorkspace->GetSolutionProject(i)->GetProject();
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
// The project configuration its solution's configuration builds, or the
// project's first if the solution doesn't say.
//
//////////////////////////////////////////////////////////////////////////
CVcxConfiguration* CIDECodeModel::GetProjectConfiguration(size uiProject)
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
const char* CIDECodeModel::GetSolutionConfiguration(size uiSolution)
{
	CBuildWorkspaceSolution*	pcSolution;
	CBuildConfiguration*		pcConfiguration;

	pcSolution = mpcWorkspace->GetSolution(uiSolution);
	pcConfiguration = mpcWorkspace->FindConfiguration(uiSolution, mszConfiguration.Text());
	if ((pcConfiguration == NULL) && (pcSolution->mcConfigurations.NumConfigurations() > 0))
	{
		pcConfiguration = pcSolution->mcConfigurations.GetConfiguration(0);
	}
	if (pcConfiguration != NULL)
	{
		return pcConfiguration->GetSolutionConfiguration();
	}
	return (pcSolution->mcSolution.NumConfigurations() > 0) ? pcSolution->mcSolution.GetConfiguration(0) : NULL;
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
	Reset();
	mszConfiguration.Set(szConfiguration);
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is being indexed and the projects will be made again: before
// the workspace's solutions change, as the jobs use them.  StartIndexing
// indexes them again.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Reset(void)
{
	KillJobs();
	mcProjectsMutex.Lock();
	KillProjects();
	mcProjectsMutex.Unlock();
}


char* CIDECodeModel::GetConfiguration(void) { return mszConfiguration.Text(); }
void CIDECodeModel::SetEditsDelay(int iMilliseconds) { miEditsDelay = iMilliseconds; }
size CIDECodeModel::NumIndexings(void) { return muiIndexings; }
void CIDECodeModel::AddSystemDirectory(const char* szDirectory) { maszSystemDirectories.Add(szDirectory); }
CArrayChars* CIDECodeModel::GetSystemDirectories(void) { return &maszSystemDirectories; }


//////////////////////////////////////////////////////////////////////////
//
// The kind of icon a symbol of eKind has, IST_Count for one Go to Symbol
// doesn't list (parameters, labels ...).
//
//////////////////////////////////////////////////////////////////////////
static EIDESymbolType GetIDESymbolType(ECPPSymbolKind eKind)
{
	switch (eKind)
	{
	case CPPSK_Class:
	case CPPSK_ClassTemplate:
		return IST_Class;
	case CPPSK_Enum:
		return IST_Enum;
	case CPPSK_Typedef:
	case CPPSK_AliasTemplate:
		return IST_Typedef;
	case CPPSK_Namespace:
		return IST_Namespace;
	case CPPSK_Function:
	case CPPSK_FunctionTemplate:
		return IST_Function;
	case CPPSK_Variable:
	case CPPSK_Field:
	case CPPSK_Enumerator:
		return IST_Variable;
	default:
		return IST_Count;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Splits a symbol's key, "CChars::Append(char const*)", into where it is
// declared ("CChars", "" at file scope), its name ("Append") and its
// parameters ("(char const*)", "" if it isn't a function).
//
//////////////////////////////////////////////////////////////////////////
static void SplitIDESymbolKey(const char* szKey, CChars* pszScope, CChars* pszName, CChars* pszParameters)
{
	size	uiLength;
	size	uiEnd;
	size	uiStart;
	int		iDepth;

	uiLength = strlen(szKey);
	uiEnd = uiLength;

	//The parameters are the brackets the key ends with.
	if ((uiLength > 0) && (szKey[uiLength - 1] == ')'))
	{
		iDepth = 0;
		for (uiEnd = uiLength; uiEnd > 0; uiEnd--)
		{
			if (szKey[uiEnd - 1] == ')')
			{
				iDepth++;
			}
			else if (szKey[uiEnd - 1] == '(')
			{
				iDepth--;
				if (iDepth == 0)
				{
					uiEnd--;
					break;
				}
			}
		}
	}
	pszParameters->Init();
	pszParameters->AppendSubString(szKey, uiEnd, uiLength);

	//The name is after the last "::" outside a template's angle brackets.
	iDepth = 0;
	for (uiStart = uiEnd; uiStart > 0; uiStart--)
	{
		if (szKey[uiStart - 1] == '>')
		{
			iDepth++;
		}
		else if ((szKey[uiStart - 1] == '<') && (iDepth > 0))
		{
			iDepth--;
		}
		else if ((iDepth == 0) && (uiStart >= 2) && (szKey[uiStart - 1] == ':') && (szKey[uiStart - 2] == ':'))
		{
			break;
		}
	}
	pszName->Init();
	pszName->AppendSubString(szKey, uiStart, uiEnd);
	pszScope->Init();
	if (uiStart >= 2)
	{
		pszScope->AppendSubString(szKey, 0, uiStart - 2);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Class's results (classes and enums) or, without bClasses, Go to
// Symbol's (every named thing but parameters, labels and locals): those
// whose name the pattern matches, declared in one of pcFiles (the
// solution's files).  Each is shown as its name with its parameters,
// where it's declared and its file; its data is its symbol's index.
// aiIcons are the dialog's icons by EIDESymbolType.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindGoToSymbols(CNameMatcher* pcMatcher, bool bClasses, CMapStringInt* pcFiles, int32* aiIcons, CGoToItems* pcDest)
{
	size				i;
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;
	EIDESymbolType		eType;
	CChars				szScope;
	CChars				szName;
	CChars				szParameters;
	CChars				szLocation;
	SNameMatch			sMatch;
	char*				szFile;
	char*				szShortFile;

	mcIndexMutex.Lock();
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		eType = GetIDESymbolType(pcSymbol->meKind);
		if ((eType == IST_Count) || (bClasses && (eType != IST_Class) && (eType != IST_Enum)) || pcSymbol->mszKey.Contains('@'))
		{
			continue;
		}
		psLocation = pcSymbol->GetDefinition();
		if (psLocation == NULL)
		{
			psLocation = pcSymbol->GetDeclaration();
		}
		if (psLocation == NULL)
		{
			continue;
		}
		szFile = mcIndex.GetFileName(psLocation->uiFile);
		if (pcFiles->Get(szFile, -1) < 0)
		{
			continue;
		}

		SplitIDESymbolKey(pcSymbol->mszKey.Text(), &szScope, &szName, &szParameters);
		if (!szName.Empty() && pcMatcher->Match(szName.Text(), szName.Length(), pcMatcher->GetPattern(), StrLen(pcMatcher->GetPattern()), &sMatch))
		{
			szName.Append(szParameters);
			szShortFile = strrchr(szFile, '/');
			szShortFile = szShortFile ? szShortFile + 1 : szFile;
			szLocation.Init(szScope);
			if (!szLocation.Empty())
			{
				szLocation.Append("  ");
			}
			szLocation.Append(szShortFile);
			szLocation.Append(":");
			szLocation.Append(psLocation->iLine + 1);
			pcDest->Add(szName.Text(), szLocation.Text(), aiIcons[eType], i, &sMatch);
			szLocation.Kill();
		}
		szParameters.Kill();
		szName.Kill();
		szScope.Kill();
	}
	mcIndexMutex.Unlock();
}


//////////////////////////////////////////////////////////////////////////
//
// Where symbol uiSymbol (of FindGoToSymbols) is defined, or declared.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::GetSymbolLocation(size uiSymbol, SIDELocation* psDest)
{
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;

	psLocation = NULL;
	mcIndexMutex.Lock();
	if (uiSymbol < mcIndex.NumSymbols())
	{
		pcSymbol = mcIndex.GetSymbol(uiSymbol);
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
//
//////////////////////////////////////////////////////////////////////////
void SIDERenameTarget::Init(void)
{
	szKey.Init();
	szName.Init();
	asLocations.Init();
	bRenamable = false;
	szWhyNot.Init();
	uiOverrides = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDERenameTarget::Kill(void)
{
	szWhyNot.Kill();
	KillIDELocations(&asLocations);
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A symbol's name as written: a local's is after its key's last colon
// ("@A.cpp:6:12:iCount"), anything else's is after its scope.
//
//////////////////////////////////////////////////////////////////////////
void GetIDESymbolName(const char* szKey, CChars* pszDest)
{
	CChars		szScope;
	CChars		szParameters;
	const char*	szColon;

	if (szKey[0] == '@')
	{
		szColon = strrchr(szKey, ':');
		pszDest->Init(szColon ? szColon + 1 : szKey);
		return;
	}
	SplitIDESymbolKey(szKey, &szScope, pszDest, &szParameters);
	szParameters.Kill();
	szScope.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Why Rename can't change pcSymbol's places, NULL if it can: each must be
// in one of pcFiles (the solution's, what can be edited), written as the
// name (not made by a macro) and not a call that could be to another
// overload.
//
//////////////////////////////////////////////////////////////////////////
static const char* GetIDERenameProblem(CCPPIndex* pcIndex, CCPPIndexSymbol* pcSymbol, CMapStringInt* pcFiles)
{
	size				i;
	SCPPIndexLocation*	psLocation;

	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if (pcFiles->Get(pcIndex->GetFileName(psLocation->uiFile), -1) < 0)
		{
			return "it is declared or used in a file that isn't in the solution";
		}
		if (psLocation->uiFlags & CPPIF_MACRO)
		{
			return "its name is made by a macro";
		}
		if (psLocation->uiFlags & CPPIF_AMBIGUOUS)
		{
			return "a call to it can't be told from a call to another overload";
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename's target at iLine, iColumn (0 based) of szFileName: the symbol
// named there and every place its name is written.  A class's
// constructors and destructors are renamed with it.  It can't be renamed
// (bRenamable false, szWhyNot says why) if it is an operator, a
// destructor or a compiler type, or if a place can't be changed (see
// GetIDERenameProblem).  False if no symbol is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindRenameTarget(const char* szFileName, int iLine, int iColumn, CMapStringInt* pcFiles, SIDERenameTarget* psDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcMember;
	const char*			szProblem;
	CChars				szConstructor;
	CChars				szDestructor;
	size				i;
	size				j;
	CArrayInt			aiOverrides;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol == NULL)
	{
		mcIndexMutex.Unlock();
		return false;
	}

	psDest->szKey.Set(pcSymbol->mszKey);
	GetIDESymbolName(pcSymbol->mszKey.Text(), &psDest->szName);
	szProblem = NULL;
	if ((pcSymbol->meKind == CPPSK_BuiltinType) || psDest->szName.Empty())
	{
		szProblem = "it is the compiler's";
	}
	else if (psDest->szName.StartsWith("operator"))
	{
		szProblem = "it is an operator";
	}
	else if (psDest->szName.Text()[0] == '~')
	{
		szProblem = "a destructor is renamed with its class";
	}
	if (szProblem == NULL)
	{
		szProblem = GetIDERenameProblem(&mcIndex, pcSymbol, pcFiles);
	}
	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		CopyLocation(pcSymbol->GetLocation(i), psDest->asLocations.Add());
	}

	//A class's constructors and destructor are named for it.
	if ((szProblem == NULL) && ((pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_ClassTemplate)))
	{
		szConstructor.Init(pcSymbol->mszKey);
		szConstructor.Append("::");
		szConstructor.Append(psDest->szName);
		szConstructor.Append('(');
		szDestructor.Init(pcSymbol->mszKey);
		szDestructor.Append("::~");
		szDestructor.Append(psDest->szName);
		szDestructor.Append('(');
		for (i = 0; (i < mcIndex.NumSymbols()) && (szProblem == NULL); i++)
		{
			pcMember = mcIndex.GetSymbol(i);
			if (pcMember->mszKey.StartsWith(szConstructor.Text()) || pcMember->mszKey.StartsWith(szDestructor.Text()))
			{
				szProblem = GetIDERenameProblem(&mcIndex, pcMember, pcFiles);
				for (j = 0; j < pcMember->NumLocations(); j++)
				{
					CopyLocation(pcMember->GetLocation(j), psDest->asLocations.Add());
				}
			}
		}
		szDestructor.Kill();
		szConstructor.Kill();
	}

	//A virtual member function is renamed with what overrides it and what it overrides.
	if ((szProblem == NULL) && (pcSymbol->meKind == CPPSK_Function))
	{
		aiOverrides.Init();
		if (mcIndex.FindOverrides(pcSymbol, &aiOverrides))
		{
			for (i = 0; i < aiOverrides.NumElements(); i++)
			{
				pcMember = mcIndex.GetSymbol((size)aiOverrides.GetValue(i));
				if (pcMember == pcSymbol)
				{
					continue;
				}
				psDest->uiOverrides++;
				if (szProblem == NULL)
				{
					szProblem = GetIDERenameProblem(&mcIndex, pcMember, pcFiles);
				}
				for (j = 0; j < pcMember->NumLocations(); j++)
				{
					CopyLocation(pcMember->GetLocation(j), psDest->asLocations.Add());
				}
			}
		}
		aiOverrides.Kill();
	}
	mcIndexMutex.Unlock();

	psDest->bRenamable = (szProblem == NULL);
	if (szProblem)
	{
		psDest->szWhyNot.Set(szProblem);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The places of the member functions that override, or are overridden
// by, the virtual member function szKey (not its own) added to pasDest.
// How many functions they are, 0 if szKey isn't a virtual function or
// has no overrides.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindOverrideLocations(const char* szKey, CArrayIDELocations* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcMember;
	CArrayInt			aiOverrides;
	size				i;
	size				j;
	size				uiOverrides;

	uiOverrides = 0;
	aiOverrides.Init();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	if (pcSymbol && (pcSymbol->meKind == CPPSK_Function) && mcIndex.FindOverrides(pcSymbol, &aiOverrides))
	{
		for (i = 0; i < aiOverrides.NumElements(); i++)
		{
			pcMember = mcIndex.GetSymbol((size)aiOverrides.GetValue(i));
			if (pcMember != pcSymbol)
			{
				uiOverrides++;
				for (j = 0; j < pcMember->NumLocations(); j++)
				{
					CopyLocation(pcMember->GetLocation(j), pasDest->Add());
				}
			}
		}
	}
	mcIndexMutex.Unlock();
	aiOverrides.Kill();
	return uiOverrides;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the index has a symbol with szKey, a rename's new name say.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::HasSymbol(const char* szKey)
{
	bool				bFound;
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	bFound = (pcSymbol != NULL) && (pcSymbol->NumLocations() > 0);
	mcIndexMutex.Unlock();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Where Find Usages puts a usage: the project of its file (the first that
// has it, "Other files" if none does) and the class it is in: the class
// it is written in, or the class of the member function it is in, or its
// file's name for a free function or anything outside a definition.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::GetUsageGroup(SIDELocation* psLocation, CChars* pszProject, CChars* pszClass)
{
	size				uiProject;
	CVcxProject*		pcProject;
	CCPPIndexSymbol*	pcContainer;
	CChars				szScope;
	CChars				szName;
	CChars				szParameters;
	const char*			szFile;

	uiProject = FindProjectOfFile(psLocation->szFileName.Text());
	pcProject = (uiProject != MAX_SIZE) ? mpcWorkspace->GetSolutionProject(uiProject)->GetProject() : NULL;
	pszProject->Init(pcProject ? pcProject->GetName() : "Other files");

	pszClass->Init();
	mcIndexMutex.Lock();
	if ((psLocation->iContainer >= 0) && ((size)psLocation->iContainer < mcIndex.NumSymbols()))
	{
		pcContainer = mcIndex.GetSymbol((size)psLocation->iContainer);
		if ((pcContainer->meKind == CPPSK_Class) || (pcContainer->meKind == CPPSK_ClassTemplate))
		{
			pszClass->Append(pcContainer->mszKey);
		}
		else
		{
			//A member function's class is its scope.
			SplitIDESymbolKey(pcContainer->mszKey.Text(), &szScope, &szName, &szParameters);
			pszClass->Append(szScope);
			szParameters.Kill();
			szName.Kill();
			szScope.Kill();
		}
	}
	mcIndexMutex.Unlock();

	if (pszClass->Empty())
	{
		szFile = strrchr(psLocation->szFileName.Text(), '/');
		pszClass->Append(szFile ? szFile + 1 : psLocation->szFileName.Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The file an #include names, as the compiler would find it: "" includes
// next to the file that includes them first, then each in project
// uiProject's include directories, then the compiler's.  False if it
// isn't found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindIncludeFile(size uiProject, const char* szIncluder, const char* szName, bool bSystem, CChars* pszDest)
{
	CFileUtil			cFileUtil;
	CArrayChars			aszDirectories;
	CChars				szDirectory;
	CChars				szFileName;
	CChars				szPath;
	CVcxConfiguration*	pcConfiguration;
	size				i;
	bool				bFound;

	aszDirectories.Init();
	if (!bSystem && szIncluder)
	{
		szFileName.Init();
		szDirectory.Init();
		cFileUtil.SplitPath(szIncluder, &szFileName, &szDirectory);
		aszDirectories.Add(szDirectory.Text());
		szDirectory.Kill();
		szFileName.Kill();
	}
	if (uiProject < mpcWorkspace->NumProjects())
	{
		pcConfiguration = GetProjectConfiguration(uiProject);
		if (pcConfiguration && mpcWorkspace->GetSolutionProject(uiProject)->GetProject())
		{
			mpcWorkspace->GetSolutionProject(uiProject)->GetProject()->GetIncludeDirectories(pcConfiguration, mpcWorkspace->GetProjectSolution(uiProject)->GetDirectory(), &aszDirectories);
		}
	}
	for (i = 0; i < maszSystemDirectories.NumElements(); i++)
	{
		aszDirectories.Add(maszSystemDirectories.GetText(i));
	}

	bFound = false;
	for (i = 0; (i < aszDirectories.NumElements()) && !bFound; i++)
	{
		szPath.Init(aszDirectories.GetText(i));
		cFileUtil.AppendToPath(&szPath, szName);
		if (cFileUtil.Exists(szPath.Text()))
		{
			cFileUtil.FullPath(&szPath);
			pszDest->Set(szPath);
			bFound = true;
		}
		szPath.Kill();
	}
	aszDirectories.Kill();
	return bFound;
}
