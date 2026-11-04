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
#include "CppParserLib/Project.h"
#include "CppParserLib/CppTokeniser.h"
#include "CppParserLib/CppParser.h"
#include "IDECodeModel.h"


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
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::Kill(void)
{
	maszSystemDirectories.Kill();
	mszConfiguration.Kill();
	mpcSolution = NULL;
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
// solution's project uiProject, and its highlights put in pacDest.  The
// preprocessor's errors are appended to pszErrors.  False if the project
// isn't loaded or the file couldn't be read; a file with errors is still
// highlighted as far as it could be parsed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::Highlight(size uiProject, const char* szFileName, CArrayHighlights* pacDest, CChars* pszErrors)
{
	CVcxSolutionProject*	pcSolutionProject;
	CVcxProject*			pcProject;
	CVcxConfiguration*		pcConfiguration;
	CProject				cProject;
	CLibrary*				pcLibrary;
	CConfig*				pcConfig;
	CTranslationUnit*		pcUnit;
	CArrayChars				aszValues;
	CFileUtil				cFileUtil;
	CChars					szName;
	CChars					szDirectory;
	CCPPTokeniser			cTokeniser;
	CCPPSyntaxTree			cTree;
	CCPPParser				cParser;
	CCPPHighlighter			cHighlighter;
	size					i;

	pcSolutionProject = mpcSolution->GetProject(uiProject);
	pcProject = pcSolutionProject->GetProject();
	if (pcProject == NULL)
	{
		return false;
	}
	pcConfiguration = GetProjectConfiguration(pcSolutionProject);

	//Quoted includes look in the file's own directory first, then the project's include directories, then the compiler's.
	cProject.Init();
	aszValues.Init();
	if (pcConfiguration != NULL)
	{
		pcProject->GetIncludeDirectories(pcConfiguration, mpcSolution->GetDirectory(), &aszValues);
	}
	for (i = 0; i < aszValues.NumElements(); i++)
	{
		if (cFileUtil.IsDirectory(aszValues.GetText(i)))
		{
			cProject.AddIncludeDirectory(aszValues.GetText(i), true, false);
		}
	}
	for (i = 0; i < maszSystemDirectories.NumElements(); i++)
	{
		cProject.AddIncludeDirectory(maszSystemDirectories.GetText(i), true, true);
	}

	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFileName, &szName, &szDirectory);
	pcLibrary = cProject.AddLibrary(pcProject->GetName(), szDirectory.Text(), false);
	pcConfig = pcLibrary->AddConfiguration(mszConfiguration.Text());
	pcConfig->mbHostDefines = false;
	AddCompilerDefines(pcConfiguration, pcConfig);
	aszValues.ReInit();
	if (pcConfiguration != NULL)
	{
		pcProject->GetPreprocessorDefinitions(pcConfiguration, mpcSolution->GetDirectory(), &aszValues);
	}
	for (i = 0; i < aszValues.NumElements(); i++)
	{
		pcConfig->AddDefine(aszValues.GetText(i));
	}
	aszValues.Kill();

	pcUnit = pcLibrary->AddTranslationUnit(szName.Text());
	szDirectory.Kill();
	szName.Kill();

	cProject.ProcessUnit(pcUnit, mszConfiguration.Text(), pszErrors);

	cTokeniser.Init();
	cTokeniser.Tokenise(pcUnit->GetTokens());
	cTree.Init(&cTokeniser);
	cParser.Init(&cTokeniser, &cTree);
	cParser.Parse();
	cParser.Kill();

	cHighlighter.Init();
	cHighlighter.Highlight(pcUnit, pcUnit->GetTokens(), &cTokeniser, &cTree);
	for (i = 0; i < cHighlighter.NumHighlights(); i++)
	{
		pacDest->Add(cHighlighter.Get(i));
	}
	cHighlighter.Kill();

	cTree.Kill();
	cTokeniser.Kill();
	cProject.Kill();
	return true;
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
//
//////////////////////////////////////////////////////////////////////////
void CIDECodeModel::SetConfiguration(const char* szConfiguration) { mszConfiguration.Set(szConfiguration); }
char* CIDECodeModel::GetConfiguration(void) { return mszConfiguration.Text(); }
void CIDECodeModel::AddSystemDirectory(const char* szDirectory) { maszSystemDirectories.Add(szDirectory); }

