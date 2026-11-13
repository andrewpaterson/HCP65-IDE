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
#include "BaseLib/Timer.h"
#include "CppCompilerLib/CppParser.h"
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
// psSource's place given to psDest with its file's name, which psSource
// is left without: it is still to be killed, as it would have been.
//
//////////////////////////////////////////////////////////////////////////
void MoveIDELocation(SIDELocation* psDest, SIDELocation* psSource)
{
	memcpy(psDest, psSource, sizeof(SIDELocation));
	psSource->szFileName.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// The file's name in its path: what is after the last separator, of
// either kind.
//
//////////////////////////////////////////////////////////////////////////
const char* GetIDEFileNamePart(const char* szPath)
{
	const char*	szName;
	const char*	szPosition;

	szName = szPath;
	for (szPosition = szPath; *szPosition != '\0'; szPosition++)
	{
		if ((*szPosition == '/') || (*szPosition == '\\'))
		{
			szName = szPosition + 1;
		}
	}
	return szName;
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
	mbSystemDirectoriesMissing = false;

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
	mcProjectFiles.Init(false);
	maszProjectFiles.Init();
	mbProjectFiles = false;

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
	maszProjectFiles.Kill();
	mcProjectFiles.Kill();
	mcProblems.Kill();
	maszProblems.Kill();
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


#ifdef _WIN32
//The folders Visual Studio is installed in, each holding <version>\<edition>\VC\Tools\MSVC\<toolset>.
static const char* gaszVisualStudioDirectories[] = { "C:/Program Files (x86)/Microsoft Visual Studio", "C:/Program Files/Microsoft Visual Studio", NULL };
#define IDE_WINDOWS_KITS_INCLUDE_DIRECTORY	"C:/Program Files (x86)/Windows Kits/10/Include"
//The Windows SDK's folders MSVC's INCLUDE has, in its order (after the toolset's own include).
static const char* gaszWindowsKitsDirectories[] = { "ucrt", "shared", "um", NULL };


//////////////////////////////////////////////////////////////////////////
//
// Whether the version szName ("14.29.30133", "10.0.19041.0") is newer
// than szOther, NULL for none.  A name that doesn't start with a number
// isn't.
//
//////////////////////////////////////////////////////////////////////////
static bool IsNewerVersion(const char* szName, const char* szOther)
{
	int	aiName[4] = { 0, 0, 0, 0 };
	int	aiOther[4] = { 0, 0, 0, 0 };
	int	i;

	if (sscanf(szName, "%d.%d.%d.%d", &aiName[0], &aiName[1], &aiName[2], &aiName[3]) < 1)
	{
		return false;
	}
	if (szOther == NULL)
	{
		return true;
	}
	sscanf(szOther, "%d.%d.%d.%d", &aiOther[0], &aiOther[1], &aiOther[2], &aiOther[3]);
	for (i = 0; i < 4; i++)
	{
		if (aiName[i] != aiOther[i])
		{
			return aiName[i] > aiOther[i];
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The full path of szDirectory's folder with the newest version name
// that has szInside in it (szInside may be ""), appended to pszDest.
// False if it has none.
//
//////////////////////////////////////////////////////////////////////////
static bool FindNewestVersionDirectory(const char* szDirectory, const char* szInside, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszFound;
	CChars		szName;
	CChars		szParent;
	CChars		szNewest;
	CChars		szPath;
	size		i;
	bool		bFound;

	aszFound.Init();
	szName.Init();
	szParent.Init();
	szNewest.Init();
	szPath.Init();
	if (cFileUtil.IsDirectory(szDirectory))
	{
		cFileUtil.FindAllDirectories(szDirectory, &aszFound);
	}
	for (i = 0; i < aszFound.NumElements(); i++)
	{
		szName.Set("");
		szParent.Set("");
		cFileUtil.SplitPath(aszFound.GetText(i), &szName, &szParent);
		szPath.Set(aszFound.GetText(i));
		szPath.Append('/');
		szPath.Append(szInside);
		if (cFileUtil.IsDirectory(szPath.Text()) && IsNewerVersion(szName.Text(), szNewest.Empty() ? NULL : szNewest.Text()))
		{
			szNewest.Set(szName);
		}
	}
	bFound = !szNewest.Empty();
	if (bFound)
	{
		pszDest->Append(szDirectory);
		pszDest->Append('/');
		pszDest->Append(szNewest);
	}
	szPath.Kill();
	szNewest.Kill();
	szParent.Kill();
	szName.Kill();
	aszFound.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The newest MSVC toolset's include folder (VC\Tools\MSVC\<toolset>\
// include) of any Visual Studio installed, set in pszDest.  False if
// there is none.
//
//////////////////////////////////////////////////////////////////////////
static bool FindVisualStudioInclude(CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszVersions;
	CArrayChars	aszEditions;
	CChars		szTools;
	CChars		szToolset;
	CChars		szNewest;
	CChars		szName;
	CChars		szParent;
	size		i;
	size		j;
	size		k;

	szNewest.Init();
	szName.Init();
	szParent.Init();
	for (i = 0; gaszVisualStudioDirectories[i] != NULL; i++)
	{
		aszVersions.Init();
		if (cFileUtil.IsDirectory(gaszVisualStudioDirectories[i]))
		{
			cFileUtil.FindAllDirectories(gaszVisualStudioDirectories[i], &aszVersions);
		}
		for (j = 0; j < aszVersions.NumElements(); j++)
		{
			aszEditions.Init();
			cFileUtil.FindAllDirectories(aszVersions.GetText(j), &aszEditions);
			for (k = 0; k < aszEditions.NumElements(); k++)
			{
				szTools.Init(aszEditions.GetText(k));
				szTools.Append("/VC/Tools/MSVC");
				szToolset.Init();
				if (FindNewestVersionDirectory(szTools.Text(), "include", &szToolset))
				{
					szName.Set("");
					szParent.Set("");
					cFileUtil.SplitPath(szToolset.Text(), &szName, &szParent);
					if (szNewest.Empty() || IsNewerVersion(szName.Text(), szNewest.Text()))
					{
						szNewest.Set(szName);
						pszDest->Set(szToolset);
						pszDest->Append("/include");
					}
				}
				szToolset.Kill();
				szTools.Kill();
			}
			aszEditions.Kill();
		}
		aszVersions.Kill();
	}
	szParent.Kill();
	szName.Kill();
	szNewest.Kill();
	return !pszDest->Empty();
}
#endif


//////////////////////////////////////////////////////////////////////////
//
// The compiler's own headers on this machine, those that exist; those that
// don't are logged, as without them almost nothing can be parsed (see
// IsMissingSystemDirectories): MinGW's on Linux, and on Windows the newest
// Visual Studio MSVC toolset's and Windows SDK's, in MSVC's INCLUDE order.
// For checking, HCP65_IDE_SYSTEM_ROOT is where MinGW's are looked for in
// place of the root folder.  Every configuration has them, after its own
// include directories, until a build configuration says which it has.
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
		else
		{
			mbSystemDirectoriesMissing = true;
			gcLogger.Info2(__METHOD__, " System headers [", szName.Text(), "] aren't there.", NULL);
		}
	}
	if (szNewest.Empty())
	{
		mbSystemDirectoriesMissing = true;
		gcLogger.Info2(__METHOD__, " No MinGW GCC is in [", szPath.Text(), "].", NULL);
	}
	szName.Set(szRoot);
	szName.Append(IDE_MINGW_INCLUDE_DIRECTORY);
	if (cFileUtil.IsDirectory(szName.Text()))
	{
		AddSystemDirectory(szName.Text());
	}
	else
	{
		mbSystemDirectoriesMissing = true;
		gcLogger.Info2(__METHOD__, " System headers [", szName.Text(), "] aren't there.", NULL);
	}
	aszFound.Kill();
	szNewest.Kill();
	szParent.Kill();
	szName.Kill();
	szPath.Kill();
	szRoot.Kill();
#endif
#ifdef _WIN32
	//As MSVC's INCLUDE has them: the toolset's own headers, then the Windows SDK's.
	CFileUtil	cFileUtil;
	CChars		szInclude;
	CChars		szKits;
	CChars		szName;
	size		i;

	szInclude.Init();
	if (FindVisualStudioInclude(&szInclude))
	{
		szInclude.Replace('\\', '/');
		AddSystemDirectory(szInclude.Text());
	}
	else
	{
		mbSystemDirectoriesMissing = true;
		gcLogger.Info2(__METHOD__, " No Visual Studio MSVC toolset was found.", NULL);
	}
	szInclude.Kill();

	szKits.Init();
	if (FindNewestVersionDirectory(IDE_WINDOWS_KITS_INCLUDE_DIRECTORY, "ucrt", &szKits))
	{
		szKits.Replace('\\', '/');
		szName.Init();
		for (i = 0; gaszWindowsKitsDirectories[i] != NULL; i++)
		{
			szName.Set(szKits);
			szName.Append('/');
			szName.Append(gaszWindowsKitsDirectories[i]);
			if (cFileUtil.IsDirectory(szName.Text()))
			{
				AddSystemDirectory(szName.Text());
			}
			else
			{
				mbSystemDirectoriesMissing = true;
				gcLogger.Info2(__METHOD__, " System headers [", szName.Text(), "] aren't there.", NULL);
			}
		}
		szName.Kill();
	}
	else
	{
		mbSystemDirectoriesMissing = true;
		gcLogger.Info2(__METHOD__, " No Windows SDK is in [", IDE_WINDOWS_KITS_INCLUDE_DIRECTORY, "].", NULL);
	}
	szKits.Kill();
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

	HighlightFile(&cUnit, pcFile, pacDest, NULL);
	cUnit.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The highlights of pcFile (the unit's own file or a header it included)
// added to pacDest, and its syntax ranges to pasRanges unless it is NULL.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::HighlightFile(CIDEParsedUnit* pcUnit, CSourceFile* pcFile, CArrayHighlights* pacDest, CArrayCPPSyntaxRanges* pasRanges)
{
	CCPPHighlighter	cHighlighter;
	size			i;

	cHighlighter.Init();
	cHighlighter.Highlight(pcFile, pcUnit->mpcUnit->GetTokens(), &pcUnit->mcTokeniser, &pcUnit->mcTree);
	for (i = 0; i < cHighlighter.NumHighlights(); i++)
	{
		pacDest->Add(cHighlighter.Get(i));
	}
	if (pasRanges)
	{
		cHighlighter.FindSyntaxRanges(pcFile, &pcUnit->mcTokeniser, &pcUnit->mcTree, pasRanges);
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
		//Quoted includes look in the file's own directory first, and in the folders in it for one named with its folder
		//("Sub/Header.h").
		pcLibrary = pcProject->AddLibrary(szDirectory.Text(), szDirectory.Text(), true);
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
// The CppCompilerLib project for the workspace's project uiProject, made
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

	//A header that can't be found (as the compiler's own often can't) costs only what it would have added.
	pcProject->SetSkipFailedDirectives(true);
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
	maszProblems.Kill();
	maszProblems.Init();
	mcProblems.Kill();
	mcProblems.Init();
	muiProblemUnits = 0;
	miIndexStart = GetDebugTickCount();
	miIndexEnd = -1;
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

	//With nothing to index it is done already.
	if (mapsJobs.NumElements() == 0)
	{
		mcIndexMutex.Lock();
		miIndexEnd = miIndexStart;
		mcIndexMutex.Unlock();
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
	CArrayChars			aszErrors;
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

	aszErrors.Init();
	aszErrors.Split(szErrors.Text(), '\n');

	mcIndexMutex.Lock();
	mcIndex.Merge(&cGathered);
	for (i = 0; i < cGathered.maszFiles.NumElements(); i++)
	{
		if (mcIncluders.Get(cGathered.maszFiles.GetText(i), -1) < 0)
		{
			mcIncluders.Put(cGathered.maszFiles.GetText(i), (int)psJob->uiIndex);
		}
	}

	//What it couldn't include (a header's problem is the same in every unit that includes it, so it is kept once).
	if (!szErrors.Empty())
	{
		muiProblemUnits++;
	}
	for (i = 0; i < aszErrors.NumElements(); i++)
	{
		if ((aszErrors.GetText(i)[0] != '\0') && (mcProblems.Get(aszErrors.GetText(i), -1) < 0))
		{
			mcProblems.Put(aszErrors.GetText(i), 0);
			maszProblems.Add(aszErrors.GetText(i));
		}
	}
	muiIndexed++;
	if (muiIndexed == mapsJobs.NumElements())
	{
		miIndexEnd = GetDebugTickCount();
	}
	mcIndexMutex.Unlock();

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
// What the units indexed since the last call couldn't include (or other
// preprocessor errors), each once in an indexing, added to paszDest.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::TakeIndexingProblems(CArrayChars* paszDest)
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
size CIDECodeModel::NumIndexingProblemUnits(void)
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
int64 CIDECodeModel::GetIndexingMilliseconds(void)
{
	int64	iMilliseconds;

	mcIndexMutex.Lock();
	iMilliseconds = (miIndexEnd >= 0) ? (miIndexEnd - miIndexStart) : -1;
	mcIndexMutex.Unlock();
	return iMilliseconds;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a symbol is defined, or declared if no file indexed so far
// defines it.  NULL if it is only used.
//
//////////////////////////////////////////////////////////////////////////
static SCPPIndexLocation* GetIDESymbolPlace(CCPPIndexSymbol* pcSymbol)
{
	SCPPIndexLocation*	psLocation;

	psLocation = pcSymbol->GetDefinition();
	return psLocation ? psLocation : pcSymbol->GetDeclaration();
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

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	psLocation = pcSymbol ? GetIDESymbolPlace(pcSymbol) : NULL;
	if (psLocation)
	{
		CopyLocation(psLocation, psDest);
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

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		pszKey->Append(pcSymbol->mszKey);
		CopyLocations(pcSymbol, pasDest);
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The places in szFileName that name what is named at a place in it (0
// based), as FindReferences finds them but only that file's: for an
// editor to show the usages of the symbol at its caret.  False if nothing
// indexed is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindFileReferences(const char* szFileName, int iLine, int iColumn, CArrayIDELocations* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;

	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		CopyLocationsInFile(pcSymbol, szFileName, pasDest);
	}
	mcIndexMutex.Unlock();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// How what is named at a place (0 based) is named there: declared,
// defined or used (ECPPIndexRole).  False if nothing indexed is named
// there.  Nothing is copied, so it can be asked whenever a command wants
// to know if it has a symbol to work on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindRoleAt(const char* szFileName, int iLine, int iColumn, uint8* puiRole)
{
	CCPPIndexSymbol*	pcSymbol;
	SCPPIndexLocation*	psLocation;
	size				i;
	bool				bFound;

	bFound = false;
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol)
	{
		for (i = 0; (i < pcSymbol->NumLocations()) && !bFound; i++)
		{
			psLocation = pcSymbol->GetLocation(i);
			if ((psLocation->iLine == iLine) && (psLocation->iColumn <= iColumn) && (iColumn < psLocation->iColumn + psLocation->iLength) &&
				(StringInsensitiveCompare(mcIndex.GetFileName(psLocation->uiFile), szFileName) == 0))
			{
				*puiRole = psLocation->uiRole;
				bFound = true;
			}
		}
	}
	mcIndexMutex.Unlock();
	return bFound;
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
	psDest->uiUsage = psLocation->uiUsage;
	psDest->iContainer = psLocation->iContainer;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place a symbol is named added to pasDest.  Call it with the
// index's mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::CopyLocations(CCPPIndexSymbol* pcSymbol, CArrayIDELocations* pasDest)
{
	size	i;

	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		CopyLocation(pcSymbol->GetLocation(i), pasDest->Add());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol's places in szFileName, added to pasDest.  Locations are by
// the index's file number: the name is compared once for each run of them
// in a file.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::CopyLocationsInFile(CCPPIndexSymbol* pcSymbol, const char* szFileName, CArrayIDELocations* pasDest)
{
	SCPPIndexLocation*	psLocation;
	size				i;
	size				uiFile;
	bool				bInFile;

	uiFile = MAX_SIZE;
	bInFile = false;
	for (i = 0; i < pcSymbol->NumLocations(); i++)
	{
		psLocation = pcSymbol->GetLocation(i);
		if (psLocation->uiFile != uiFile)
		{
			uiFile = psLocation->uiFile;
			bInFile = StringInsensitiveCompare(mcIndex.GetFileName(uiFile), szFileName) == 0;
		}
		if (bInFile)
		{
			CopyLocation(psLocation, pasDest->Add());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The index's symbols of the member functions that override, or are
// overridden by, the virtual member function pcSymbol (not its own) added
// to paiDest: none if it isn't one or has none.  Call it with the index's
// mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindOverrides(CCPPIndexSymbol* pcSymbol, CArrayInt* paiDest)
{
	CArrayInt	aiHierarchy;
	size		i;

	aiHierarchy.Init();
	if ((pcSymbol->meKind == CPPSK_Function) && mcIndex.FindOverrides(pcSymbol, &aiHierarchy))
	{
		for (i = 0; i < aiHierarchy.NumElements(); i++)
		{
			if (mcIndex.GetSymbol((size)aiHierarchy.GetValue(i)) != pcSymbol)
			{
				paiDest->Add(aiHierarchy.GetValue(i));
			}
		}
	}
	aiHierarchy.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The first of the workspace's projects that has szFileName (a full
// path, whatever its case) as an item, MAX_SIZE if none has.  The items'
// paths are worked out once (FindProjectFiles): finding each on the disk,
// for its case, took Find Usages 95ms a usage.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindProjectOfFile(const char* szFileName)
{
	int32	iProject;

	FindProjectFiles();
	iProject = mcProjectFiles.Get(szFileName, -1);
	return (iProject >= 0) ? (size)iProject : MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's projects' files, each once by its full path, in the
// projects' order: what Go to File searches, what Go to Class and Go to
// Symbol show symbols from, and what Rename can edit.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::NumProjectFiles(void)
{
	FindProjectFiles();
	return maszProjectFiles.NumElements();
}


char* CIDECodeModel::GetProjectFile(size uiIndex)
{
	FindProjectFiles();
	return maszProjectFiles.GetText(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// The full paths of the files called szName (a file name, whatever its
// case) that the projects have as items, then those the index has (headers
// they include), each once, appended to paszDest.  How many there are.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindFilesNamed(const char* szName, CArrayChars* paszDest)
{
	CFileUtil	cFileUtil;
	CChars		szFile;
	size		uiFound;
	size		i;
	size		j;
	bool		bHave;
	char*		szPath;

	FindProjectFiles();
	uiFound = 0;
	mcIndexMutex.Lock();
	for (i = 0; i < maszProjectFiles.NumElements() + mcIndex.NumFiles(); i++)
	{
		szPath = (i < maszProjectFiles.NumElements()) ? maszProjectFiles.GetText(i) : mcIndex.GetFileName(i - maszProjectFiles.NumElements());
		szFile.Init();
		cFileUtil.GetFileName(&szFile, szPath);
		if (szFile.EqualsIgnoreCase(szName))
		{
			bHave = false;
			for (j = paszDest->NumElements() - uiFound; j < paszDest->NumElements(); j++)
			{
				bHave = bHave || paszDest->Get(j)->EqualsIgnoreCase(szPath);
			}
			if (!bHave)
			{
				paszDest->Add(szPath);
				uiFound++;
			}
		}
		szFile.Kill();
	}
	mcIndexMutex.Unlock();
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Every item of the workspace's projects by its full path, to the first
// project that has it, found if they haven't been since the workspace
// changed (ForgetProjectFiles).
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindProjectFiles(void)
{
	size			i;
	size			j;
	CVcxProject*	pcProject;
	CChars			szPath;

	if (mbProjectFiles)
	{
		return;
	}
	mbProjectFiles = true;
	mcProjectFiles.Kill();
	mcProjectFiles.Init(false);
	maszProjectFiles.ReInit();
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
			if (mcProjectFiles.Get(szPath.Text(), -1) < 0)
			{
				mcProjectFiles.Put(szPath.Text(), (int32)i);
				maszProjectFiles.Add(szPath.Text());
			}
			szPath.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The projects' items are found again the next time they are wanted: the
// workspace's solutions, or a project's items, have changed.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::ForgetProjectFiles(void)
{
	mbProjectFiles = false;
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
// machine the IDE runs on; on Linux, where MinGW's headers stand in for
// the Windows SDK's, GCC's name for the target too.
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
void CIDECodeModel::SetConfiguration(const char* szConfiguration)
{
	Reset();
	mszConfiguration.Set(szConfiguration);
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is being indexed and the projects will be made again, and their
// items found again: before the workspace's solutions change, as the jobs
// use them.  StartIndexing indexes them again.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Reset(void)
{
	KillJobs();
	mcProjectsMutex.Lock();
	KillProjects();
	mcProjectsMutex.Unlock();
	ForgetProjectFiles();
}


char* CIDECodeModel::GetConfiguration(void) { return mszConfiguration.Text(); }
void CIDECodeModel::SetEditsDelay(int iMilliseconds) { miEditsDelay = iMilliseconds; }
size CIDECodeModel::NumIndexings(void) { return muiIndexings; }
void CIDECodeModel::AddSystemDirectory(const char* szDirectory) { maszSystemDirectories.Add(szDirectory); }
CArrayChars* CIDECodeModel::GetSystemDirectories(void) { return &maszSystemDirectories; }
bool CIDECodeModel::IsMissingSystemDirectories(void) { return mbSystemDirectoriesMissing; }


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
// whose name the pattern matches, declared in one of the projects' files.
// Each is shown as its name with its parameters,
// where it's declared and its file; its data is its symbol's index.
// aiIcons are the dialog's icons by EIDESymbolType.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindGoToSymbols(CNameMatcher* pcMatcher, bool bClasses, int32* aiIcons, CGoToItems* pcDest)
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

	FindProjectFiles();
	mcIndexMutex.Lock();
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		eType = GetIDESymbolType(pcSymbol->meKind);
		if ((eType == IST_Count) || (bClasses && (eType != IST_Class) && (eType != IST_Enum)) || pcSymbol->mszKey.Contains('@'))
		{
			continue;
		}
		psLocation = GetIDESymbolPlace(pcSymbol);
		if (psLocation == NULL)
		{
			continue;
		}
		szFile = mcIndex.GetFileName(psLocation->uiFile);
		if (mcProjectFiles.Get(szFile, -1) < 0)
		{
			continue;
		}

		SplitIDESymbolKey(pcSymbol->mszKey.Text(), &szScope, &szName, &szParameters);
		if (!szName.Empty() && pcMatcher->Match(szName.Text(), szName.Length(), pcMatcher->GetPattern(), StrLen(pcMatcher->GetPattern()), &sMatch))
		{
			szName.Append(szParameters);
			szLocation.Init(szScope);
			if (!szLocation.Empty())
			{
				szLocation.Append("  ");
			}
			szLocation.Append(GetIDEFileNamePart(szFile));
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
	SCPPIndexLocation*	psLocation;

	mcIndexMutex.Lock();
	psLocation = (uiSymbol < mcIndex.NumSymbols()) ? GetIDESymbolPlace(mcIndex.GetSymbol(uiSymbol)) : NULL;
	if (psLocation)
	{
		CopyLocation(psLocation, psDest);
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
	bType = false;
	uiAssembly = IAR_None;
	szScope.Init();
	bOverloaded = false;
	aszOverloads.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDERenameTarget::Kill(void)
{
	aszOverloads.Kill();
	szScope.Kill();
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
// How hand written assembly's labels name pcSymbol (EIDEAssemblyRename,
// RenameFile.txt's assembly step), its scope as its key has it, and
// whether it is a function overloaded in its scope (whose labels can't be
// told from its overloads' without their parameters' types).  Call it with
// the index's mutex held.
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::FindAssemblyRename(CCPPIndexSymbol* pcSymbol, SIDERenameTarget* psDest)
{
	const char*			szKey;
	const char*			szParameters;
	const char*			szSeparator;
	const char*			szLast;
	CCPPIndexSymbol*	pcScope;
	CCPPIndexSymbol*	pcOther;
	CChars				szPrefix;
	bool				bClassScope;
	size				i;

	szKey = pcSymbol->mszKey.Text();
	psDest->uiAssembly = IAR_None;
	if (szKey[0] == '@')
	{
		return;
	}
	szParameters = strchr(szKey, '(');
	szLast = NULL;
	for (szSeparator = strstr(szKey, "::"); szSeparator && (!szParameters || (szSeparator < szParameters)); szSeparator = strstr(szSeparator + 2, "::"))
	{
		szLast = szSeparator;
	}
	if (szLast)
	{
		psDest->szScope.Set(szKey);
		psDest->szScope.SetLength((size)(szLast - szKey));
	}
	pcScope = psDest->szScope.Empty() ? NULL : mcIndex.FindSymbol(psDest->szScope.Text());
	bClassScope = pcScope && ((pcScope->meKind == CPPSK_Class) || (pcScope->meKind == CPPSK_ClassTemplate));

	if ((pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_Enum))
	{
		psDest->uiAssembly = IAR_Type;
	}
	else if ((pcSymbol->meKind == CPPSK_Field) && bClassScope)
	{
		psDest->uiAssembly = IAR_Field;
	}
	else if ((pcSymbol->meKind == CPPSK_Function) || (pcSymbol->meKind == CPPSK_Variable))
	{
		psDest->uiAssembly = bClassScope ? IAR_Member : IAR_Function;
		if ((pcSymbol->meKind == CPPSK_Function) && szParameters)
		{
			szPrefix.Init();
			szPrefix.AppendSubString(szKey, szParameters + 1);
			for (i = 0; i < mcIndex.NumSymbols(); i++)
			{
				pcOther = mcIndex.GetSymbol(i);
				if ((pcOther->meKind == CPPSK_Function) && pcOther->mszKey.StartsWith(szPrefix.Text()))
				{
					psDest->aszOverloads.Add(pcOther->mszKey.Text());
					psDest->bOverloaded |= (pcOther != pcSymbol);
				}
			}
			if (!psDest->bOverloaded)
			{
				psDest->aszOverloads.ReInit();
			}
			szPrefix.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Rename's target at iLine, iColumn (0 based) of szFileName: the symbol
// named there and every place its name is written.  A class's
// constructors and destructors are renamed with it.  It can't be renamed
// (bRenamable false, szWhyNot says why) if it is an operator, a
// destructor or a compiler type, or if a place can't be changed (see
// GetIDERenameProblem: it must be in one of the projects' files).  False
// if no symbol is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindRenameTarget(const char* szFileName, int iLine, int iColumn, SIDERenameTarget* psDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcMember;
	const char*			szProblem;
	CChars				szConstructor;
	CChars				szDestructor;
	size				i;
	CArrayInt			aiOverrides;

	FindProjectFiles();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbolAt(szFileName, iLine, iColumn);
	if (pcSymbol == NULL)
	{
		mcIndexMutex.Unlock();
		return false;
	}

	psDest->szKey.Set(pcSymbol->mszKey);
	GetIDESymbolName(pcSymbol->mszKey.Text(), &psDest->szName);
	psDest->bType = (pcSymbol->meKind == CPPSK_Namespace) || (pcSymbol->meKind == CPPSK_Class) || (pcSymbol->meKind == CPPSK_Enum) ||
					(pcSymbol->meKind == CPPSK_Typedef) || (pcSymbol->meKind == CPPSK_ClassTemplate) || (pcSymbol->meKind == CPPSK_AliasTemplate);
	FindAssemblyRename(pcSymbol, psDest);
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
		szProblem = GetIDERenameProblem(&mcIndex, pcSymbol, &mcProjectFiles);
	}
	CopyLocations(pcSymbol, &psDest->asLocations);

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
				szProblem = GetIDERenameProblem(&mcIndex, pcMember, &mcProjectFiles);
				CopyLocations(pcMember, &psDest->asLocations);
			}
		}
		szDestructor.Kill();
		szConstructor.Kill();
	}

	//A virtual member function is renamed with what overrides it and what it overrides.
	if (szProblem == NULL)
	{
		aiOverrides.Init();
		FindOverrides(pcSymbol, &aiOverrides);
		psDest->uiOverrides = aiOverrides.NumElements();
		for (i = 0; i < aiOverrides.NumElements(); i++)
		{
			pcMember = mcIndex.GetSymbol((size)aiOverrides.GetValue(i));
			if (szProblem == NULL)
			{
				szProblem = GetIDERenameProblem(&mcIndex, pcMember, &mcProjectFiles);
			}
			CopyLocations(pcMember, &psDest->asLocations);
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
	CArrayInt			aiOverrides;
	size				i;
	size				uiOverrides;

	aiOverrides.Init();
	mcIndexMutex.Lock();
	pcSymbol = mcIndex.FindSymbol(szKey);
	if (pcSymbol)
	{
		FindOverrides(pcSymbol, &aiOverrides);
		for (i = 0; i < aiOverrides.NumElements(); i++)
		{
			CopyLocations(mcIndex.GetSymbol((size)aiOverrides.GetValue(i)), pasDest);
		}
	}
	mcIndexMutex.Unlock();
	uiOverrides = aiOverrides.NumElements();
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
// it is written in, or the class of the member function it is in, or
// empty for a free function or anything outside a definition.
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
}


//An #include in a file's text: where its name is (between its quotes or angle brackets) and on which line.
struct SIDEIncludeDirective
{
	size	uiNameStart;
	size	uiNameEnd;
	int32	iLine;		//0 based.
	size	uiLineStart;
	bool	bSystem;
};


typedef CArrayTemplate<SIDEIncludeDirective>	CArrayIDEIncludeDirectives;


//////////////////////////////////////////////////////////////////////////
//
// Where the next comment starts on a line from uiStart (before uiEnd),
// past any string or character literal: MAX_SIZE if none.  pbBlock says
// if it is a /* comment, rather than a // one.
//
//////////////////////////////////////////////////////////////////////////
static size FindIDELineComment(const char* szText, size uiStart, size uiEnd, bool* pbBlock)
{
	size	i;
	char	cQuote;

	for (i = uiStart; i + 1 < uiEnd; i++)
	{
		if ((szText[i] == '"') || (szText[i] == '\''))
		{
			cQuote = szText[i];
			for (i++; (i < uiEnd) && (szText[i] != cQuote); i++)
			{
				if (szText[i] == '\\')
				{
					i++;
				}
			}
		}
		else if ((szText[i] == '/') && ((szText[i + 1] == '/') || (szText[i + 1] == '*')))
		{
			*pbBlock = (szText[i + 1] == '*');
			return i;
		}
	}
	return MAX_SIZE;
}


//////////////////////////////////////////////////////////////////////////
//
// Each #include in a file's text, as written: the text isn't
// preprocessed, so an #include an #if leaves out is one too, but one in a
// comment isn't.  "# include" and indented ones are found.
//
//////////////////////////////////////////////////////////////////////////
static void FindIDEIncludeDirectives(const char* szText, size uiLength, CArrayIDEIncludeDirectives* pasDest)
{
	size					uiLineStart;
	size					uiLineEnd;
	size					p;
	size					uiComment;
	const char*				szNewLine;
	const char*				szClose;
	char					cClose;
	bool					bInBlock;
	bool					bBlock;
	int32					iLine;
	SIDEIncludeDirective*	psDirective;

	bInBlock = false;
	iLine = 0;
	for (uiLineStart = 0; uiLineStart < uiLength; uiLineStart = uiLineEnd + 1, iLine++)
	{
		szNewLine = (const char*)memchr(&szText[uiLineStart], '\n', uiLength - uiLineStart);
		uiLineEnd = szNewLine ? (size)(szNewLine - szText) : uiLength;
		p = uiLineStart;
		if (bInBlock)
		{
			szClose = strstr(&szText[p], "*/");
			if ((szClose == NULL) || ((size)(szClose - szText) >= uiLineEnd))
			{
				continue;
			}
			p = (size)(szClose - szText) + 2;
			bInBlock = false;
		}

		while ((p < uiLineEnd) && ((szText[p] == ' ') || (szText[p] == '\t')))
		{
			p++;
		}
		if ((p < uiLineEnd) && (szText[p] == '#'))
		{
			p++;
			while ((p < uiLineEnd) && ((szText[p] == ' ') || (szText[p] == '\t')))
			{
				p++;
			}
			if ((p + 7 < uiLineEnd) && (strncmp(&szText[p], "include", 7) == 0) && ((szText[p + 7] == ' ') || (szText[p + 7] == '\t') || (szText[p + 7] == '"') || (szText[p + 7] == '<')))
			{
				p += 7;
				while ((p < uiLineEnd) && ((szText[p] == ' ') || (szText[p] == '\t')))
				{
					p++;
				}
				if ((p < uiLineEnd) && ((szText[p] == '"') || (szText[p] == '<')))
				{
					cClose = (szText[p] == '"') ? '"' : '>';
					szClose = (const char*)memchr(&szText[p + 1], cClose, uiLineEnd - (p + 1));
					if (szClose && ((size)(szClose - szText) > p + 1))
					{
						psDirective = pasDest->Add();
						psDirective->uiNameStart = p + 1;
						psDirective->uiNameEnd = (size)(szClose - szText);
						psDirective->iLine = iLine;
						psDirective->uiLineStart = uiLineStart;
						psDirective->bSystem = (cClose == '>');
						p = psDirective->uiNameEnd + 1;
					}
				}
			}
		}

		//A /* comment left open at the end of the line hides the lines after it.
		for (;;)
		{
			uiComment = FindIDELineComment(szText, p, uiLineEnd, &bBlock);
			if ((uiComment == MAX_SIZE) || !bBlock)
			{
				break;
			}
			szClose = strstr(&szText[uiComment + 2], "*/");
			if ((szClose == NULL) || ((size)(szClose - szText) >= uiLineEnd))
			{
				bInBlock = true;
				break;
			}
			p = (size)(szClose - szText) + 2;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Every #include, in the projects' C and C++ files, that finds
// szFileName (a full path) as the compiler would (see FindIncludeFile),
// added to pasDest: the includer, the line, and the column and length of
// the file's name in the #include (its last part, after any directories),
// which is what renaming the file changes.  An #include an #if leaves out
// is found too; one in a comment isn't.  The file must still be where
// szFileName says: an #include is found by looking for its file.  The
// number found.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindIncludesOf(const char* szFileName, CIDEFileTexts* pcTexts, CArrayIDELocations* pasDest)
{
	CFileUtil					cFileUtil;
	CChars						szTargetName;
	CChars						szTargetDirectory;
	CChars						szName;
	CChars						szFound;
	CTextFile					cFile;
	CArrayIDEIncludeDirectives	asDirectives;
	SIDEIncludeDirective*		psDirective;
	SIDELocation*				psLocation;
	EIDEFileType				eType;
	char*						szIncluder;
	char*						szText;
	size						uiLength;
	size						uiProject;
	size						uiNamePart;
	size						uiFound;
	size						i;
	size						j;

	FindProjectFiles();
	szTargetName.Init();
	szTargetDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szTargetName, &szTargetDirectory);
	uiFound = 0;
	for (i = 0; i < maszProjectFiles.NumElements(); i++)
	{
		szIncluder = maszProjectFiles.GetText(i);
		eType = CIDEIcons::GetFileType(szIncluder);
		if ((eType != IFT_CPP) && (eType != IFT_C) && (eType != IFT_Header))
		{
			continue;
		}
		cFile.Init();
		if (!pcTexts->GetFileText(szIncluder, &cFile, &szText, &uiLength))
		{
			cFile.Kill();
			continue;
		}

		asDirectives.Init();
		FindIDEIncludeDirectives(szText, uiLength, &asDirectives);
		uiProject = MAX_SIZE;
		for (j = 0; j < asDirectives.NumElements(); j++)
		{
			psDirective = asDirectives.Get(j);

			//Only an #include whose last part is the file's name can find it.
			for (uiNamePart = psDirective->uiNameEnd; (uiNamePart > psDirective->uiNameStart) && (szText[uiNamePart - 1] != '/') && (szText[uiNamePart - 1] != '\\'); uiNamePart--)
			{
			}
			if ((psDirective->uiNameEnd - uiNamePart != szTargetName.Length()) || (MemICmp(&szText[uiNamePart], szTargetName.Text(), szTargetName.Length()) != 0))
			{
				continue;
			}

			if (uiProject == MAX_SIZE)
			{
				uiProject = FindProjectOfFile(szIncluder);
			}
			szName.Init();
			szName.AppendSubString(szText, psDirective->uiNameStart, psDirective->uiNameEnd);
			szName.Replace('\\', '/');
			szFound.Init();
			if (FindIncludeFile(uiProject, szIncluder, szName.Text(), psDirective->bSystem, &szFound) && (StringInsensitiveCompare(szFound.Text(), szFileName) == 0))
			{
				psLocation = pasDest->Add();
				psLocation->szFileName.Init(szIncluder);
				psLocation->iLine = psDirective->iLine;
				psLocation->iColumn = (int32)(uiNamePart - psDirective->uiLineStart);
				psLocation->iLength = (int32)(psDirective->uiNameEnd - uiNamePart);
				psLocation->uiRole = 0;
				psLocation->uiFlags = 0;
				psLocation->uiUsage = 0;
				psLocation->iContainer = -1;
				uiFound++;
			}
			szFound.Kill();
			szName.Kill();
		}
		asDirectives.Kill();
		cFile.Kill();
	}
	szTargetDirectory.Kill();
	szTargetName.Kill();
	return uiFound;
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
