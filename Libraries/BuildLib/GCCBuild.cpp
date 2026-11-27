/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "BaseLib/FileUtil.h"
#include "VisualStudioLib/VcxItem.h"
#include "GCCBuild.h"


//For a project with no GCC-x64 configuration of its own, built as its x64 one (ConfigurationsPlan.txt C0, until C7-4,
//when every project has one from its property sheet): what CommonLibraries/Tests/TestBaseLib/CMakeLists.txt's
//baselib_options gives a 64-bit build, which explains each.  A project with the configuration gets none of these: its
//options are its own.
//Every compile, after the project's own defines.
static const char* gaszGCCCompileOptions[] = { "-DLINUX_GNU", "-m64", "-march=haswell", "-fno-strict-aliasing", "-fno-exceptions", "-Wno-ignored-attributes", "-Wno-write-strings", "-Wno-multichar", "-Wno-class-memaccess", "-Wno-overloaded-virtual", "-Wno-comment", "-Wno-unused-parameter", NULL };
//Then a Debug configuration's, or any other's.
static const char* gaszGCCDebugCompileOptions[] = { "-g", NULL };
static const char* gaszGCCReleaseCompileOptions[] = { "-O3", "-ffunction-sections", "-fdata-sections", "-fno-stack-protector", "-U_FORTIFY_SOURCE", "-fno-lifetime-dse", "-fno-math-errno", "-fno-trapping-math", NULL };
//Every link, then a Release link's.
static const char* gaszGCCLinkOptions[] = { "-m64", NULL };
static const char* gaszGCCReleaseLinkOptions[] = { "-Wl,--gc-sections", NULL };


//////////////////////////////////////////////////////////////////////////
//
// Where a build of the solution in szRoot links the application szProject
// (<project> in the configuration's Output folder): what a run
// configuration without a tool runs for the startup project.
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::GetExecutablePath(const char* szRoot, CBuildConfiguration* pcConfiguration, const char* szProject, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	GetDirectory(szRoot, pcConfiguration, "Output", pszDest);
	cFileUtil.FullPath(pszDest);
	cFileUtil.AppendToPath(pszDest, szProject);
}


//////////////////////////////////////////////////////////////////////////
//
// The compiles, then the archives, then the links: an application can use
// any of the solution's libraries.  A Makefile project's build command
// runs among the compiles, in the project's turn (one without a command is
// compiled as any other).
//
//////////////////////////////////////////////////////////////////////////
bool CGCCBuild::AddCommands(CArrayChars* paszLines)
{
	CArrayBuildProjects	asProjects;
	SBuildProject*		psProject;
	CArrayChars			aszIncludes;
	CArrayChars			aszDefines;
	CArrayChars			aszLibraries;
	CChars				szOutput;
	CChars				szProjectOutput;
	CChars				szSource;
	CChars				szObject;
	CChars				szTarget;
	CChars				szDescription;
	CFileUtil			cFileUtil;
	SBuildCommand*		psCommand;
	CVcxItem*			pcItem;
	size				i;
	size				j;
	size				uiFirstObject;
	int					iPass;
	bool				bFolder;

#ifndef LINUX_GNU
	//GCC-x64 is g++ for 64-bit Linux (ConfigurationsPlan.txt question 19).
	paszLines->Add("A GCC-x64 configuration is built on Linux.");
	return false;
#endif // LINUX_GNU

	asProjects.Init();
	GetProjects(&asProjects);
	AddWithoutConfiguration(&asProjects, paszLines);
	//Full paths: the commands run in the project's (or solution's) directory, not this program's.
	szOutput.Init();
	GetOutputDirectory(&szOutput);
	cFileUtil.FullPath(&szOutput);
	aszLibraries.Init();
	for (iPass = 0; iPass < 3; iPass++)
	{
		for (i = 0; i < asProjects.NumElements(); i++)
		{
			psProject = asProjects.Get(i);
			if (psProject->bCommand)
			{
				//Built by its own command, in its turn with the others' compiles.
				if (iPass == 0)
				{
					AddMakefileCommand(psProject);
				}
				continue;
			}
			//Objects in a folder of their own: an application is linked beside them as Output\GCC-x64\Debug\Project, which
			//would be the project's objects' folder.
			szProjectOutput.Init(szOutput);
			cFileUtil.AppendToPath(&szProjectOutput, "Objects");
			cFileUtil.AppendToPath(&szProjectOutput, psProject->pcProject->GetName());
			if (iPass == 0)
			{
				//Compile: Output\GCC-x64\Debug\Objects\Project\Source.o, the folder made for the first.
				bFolder = false;
				aszIncludes.Init();
				aszDefines.Init();
				psProject->pcProject->GetIncludeDirectories(psProject->pcConfiguration, mpcSolution->GetDirectory(), &aszIncludes);
				psProject->pcProject->GetPreprocessorDefinitions(psProject->pcConfiguration, mpcSolution->GetDirectory(), &aszDefines);
				for (j = 0; j < aszIncludes.NumElements(); j++)
				{
					cFileUtil.FullPath(aszIncludes.Get(j));
				}
				for (j = 0; j < psProject->pcProject->NumItems(); j++)
				{
					pcItem = psProject->pcProject->GetItem(j);
					if (psProject->pcProject->IsCompiled(pcItem, psProject->pcConfiguration))
					{
						szSource.Init();
						psProject->pcProject->GetFullPath(pcItem, &szSource);
						cFileUtil.FullPath(&szSource);
						if (!bFolder)
						{
							cFileUtil.TouchDir(szProjectOutput.Text());
							bFolder = true;
						}
						szObject.Init();
						GetObjectName(szProjectOutput.Text(), szSource.Text(), &szObject);
						AddCompile(psProject, szSource.Text(), szObject.Text(), &aszIncludes, &aszDefines);
						szObject.Kill();
						szSource.Kill();
					}
				}
				aszDefines.Kill();
				aszIncludes.Kill();
			}
			else if ((iPass == 1) == psProject->bLibrary)
			{
				//Archive a library (lib<Project>.a) or link an application (<Project>) from its objects.
				szTarget.Init(szOutput);
				szDescription.Init(psProject->bLibrary ? "Archiving " : "Linking ");
				if (psProject->bLibrary)
				{
					cFileUtil.AppendToPath(&szTarget, "lib");
					szTarget.Append(psProject->pcProject->GetName());
					szTarget.Append(".a");
					psCommand = AddCommand("ar", mpcSolution->GetDirectory(), szDescription.Append(psProject->pcProject->GetName())->Text());
					psCommand->aszArguments.Add("rcs");
					psCommand->aszArguments.Add(szTarget.Text());
					aszLibraries.Add(szTarget.Text());
				}
				else
				{
					cFileUtil.AppendToPath(&szTarget, psProject->pcProject->GetName());
					psCommand = AddCommand("g++", mpcSolution->GetDirectory(), szDescription.Append(psProject->pcProject->GetName())->Text());
					AddOptions(psProject, "Link", gaszGCCLinkOptions, NULL, gaszGCCReleaseLinkOptions, &psCommand->aszArguments);
					psCommand->aszArguments.Add("-o");
					psCommand->aszArguments.Add(szTarget.Text());
				}
				uiFirstObject = psCommand->aszArguments.NumElements();
				for (j = 0; j < psProject->pcProject->NumItems(); j++)
				{
					pcItem = psProject->pcProject->GetItem(j);
					if (psProject->pcProject->IsCompiled(pcItem, psProject->pcConfiguration))
					{
						szObject.Init();
						GetObjectName(szProjectOutput.Text(), pcItem->GetFileName(), &szObject);
						psCommand->aszArguments.Add(szObject.Text());
						szObject.Kill();
					}
				}
				if (psCommand->aszArguments.NumElements() == uiFirstObject)
				{
					//A project with no sources has nothing to archive or link.
					psCommand->aszArguments.Kill();
					psCommand->szDescription.Kill();
					psCommand->szDirectory.Kill();
					psCommand->szExecutable.Kill();
					masCommands.RemoveTail();
					if (psProject->bLibrary)
					{
						aszLibraries.RemoveTail();
					}
				}
				else if (!psProject->bLibrary && (aszLibraries.NumElements() > 0))
				{
					//The libraries in any order.
					psCommand->aszArguments.Add("-Wl,--start-group");
					for (j = 0; j < aszLibraries.NumElements(); j++)
					{
						psCommand->aszArguments.Add(aszLibraries.GetText(j));
					}
					psCommand->aszArguments.Add("-Wl,--end-group");
				}
				if (!psProject->bLibrary && (psCommand->aszArguments.NumElements() != uiFirstObject))
				{
					AddDependencies(psProject, &psCommand->aszArguments);
				}
				szDescription.Kill();
				szTarget.Kill();
			}
			szProjectOutput.Kill();
		}
	}
	aszLibraries.Kill();
	szOutput.Kill();
	asProjects.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// g++ -c -std=c++17 -I... -D... Source.cpp -o Source.o
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::AddCompile(SBuildProject* psProject, const char* szSource, const char* szObject, CArrayChars* paszIncludes, CArrayChars* paszDefines)
{
	SBuildCommand*	psCommand;
	CChars			szDescription;
	CChars			szArgument;
	CFileUtil		cFileUtil;
	size			i;

	szDescription.Init();
	cFileUtil.GetFileName(&szDescription, (char*)szSource);
	psCommand = AddCommand("g++", psProject->pcProject->GetDirectory(), szDescription.Text());
	szDescription.Kill();

	psCommand->aszArguments.Add("-c");
	psCommand->aszArguments.Add("-std=c++17");
	for (i = 0; i < paszIncludes->NumElements(); i++)
	{
		szArgument.Init("-I");
		szArgument.Append(paszIncludes->GetText(i));
		psCommand->aszArguments.Add(szArgument.Text());
		szArgument.Kill();
	}
	for (i = 0; i < paszDefines->NumElements(); i++)
	{
		szArgument.Init("-D");
		szArgument.Append(paszDefines->GetText(i));
		psCommand->aszArguments.Add(szArgument.Text());
		szArgument.Kill();
	}
	AddOptions(psProject, "ClCompile", gaszGCCCompileOptions, gaszGCCDebugCompileOptions, gaszGCCReleaseCompileOptions, &psCommand->aszArguments);
	psCommand->aszArguments.Add(szSource);
	psCommand->aszArguments.Add("-o");
	psCommand->aszArguments.Add(szObject);
}


//////////////////////////////////////////////////////////////////////////
//
// A tool's options for a project (szTool is "ClCompile" or "Link"): its
// GCC-x64 configuration's own AdditionalOptions, or for a project built as
// its x64 one the lists above, every configuration's and then Debug's or
// any other's.
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::AddOptions(SBuildProject* psProject, const char* szTool, const char** paszAll, const char** paszDebug, const char** paszRelease, CArrayChars* paszArguments)
{
	const char**	paszConfiguration;
	size			i;

	if (psProject->pcConfiguration->IsToolchain())
	{
		psProject->pcProject->GetAdditionalOptions(szTool, psProject->pcConfiguration, mpcSolution->GetDirectory(), paszArguments);
		return;
	}
	paszConfiguration = IsDebugConfiguration() ? paszDebug : paszRelease;
	for (i = 0; paszAll[i] != NULL; i++)
	{
		paszArguments->Add(paszAll[i]);
	}
	for (i = 0; paszConfiguration && (paszConfiguration[i] != NULL); i++)
	{
		paszArguments->Add(paszConfiguration[i]);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What a program is linked with besides the solution's libraries: its
// GCC-x64 configuration's AdditionalDependencies, a name with no path or
// extension as -l<name> (X11 is -lX11) and any other as written.
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::AddDependencies(SBuildProject* psProject, CArrayChars* paszArguments)
{
	CArrayChars	aszLibraries;
	CChars		szArgument;
	char*		szLibrary;
	size		i;

	if (!psProject->pcConfiguration->IsToolchain())
	{
		return;
	}
	aszLibraries.Init();
	psProject->pcProject->GetAdditionalDependencies(psProject->pcConfiguration, mpcSolution->GetDirectory(), &aszLibraries);
	for (i = 0; i < aszLibraries.NumElements(); i++)
	{
		szLibrary = aszLibraries.GetText(i);
		if (strpbrk(szLibrary, "./\\") || (szLibrary[0] == '-'))
		{
			paszArguments->Add(szLibrary);
		}
		else
		{
			szArgument.Init("-l");
			szArgument.Append(szLibrary);
			paszArguments->Add(szArgument.Text());
			szArgument.Kill();
		}
	}
	aszLibraries.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Says once which projects have no GCC-x64 configuration and are built as
// their x64 one, with the options every such project is given.
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::AddWithoutConfiguration(CArrayBuildProjects* pasProjects, CArrayChars* paszLines)
{
	CChars	szLine;
	size	i;

	szLine.Init();
	for (i = 0; i < pasProjects->NumElements(); i++)
	{
		if (!pasProjects->Get(i)->bCommand && !pasProjects->Get(i)->pcConfiguration->IsToolchain())
		{
			szLine.Append(szLine.Empty() ? "No " VCX_PLATFORM_GCC_X64 " configuration, built as " : ", ");
			if (szLine.EndsWith(" as "))
			{
				szLine.Append(pasProjects->Get(i)->pcConfiguration->GetPlatform());
				szLine.Append(": ");
			}
			szLine.Append(pasProjects->Get(i)->pcProject->GetName());
		}
	}
	if (!szLine.Empty())
	{
		szLine.Append('.');
		paszLines->Add(szLine.Text());
	}
	szLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the solution configuration built is a Debug one ("Debug|x64"),
// as CMake's CONFIG:Debug: any other is built as Release.
//
//////////////////////////////////////////////////////////////////////////
bool CGCCBuild::IsDebugConfiguration(void)
{
	CChars	szSolution;
	bool	bDebug;

	szSolution.Init(mpcConfiguration->GetSolutionConfiguration());
	bDebug = szSolution.StartsWith("Debug|");
	szSolution.Kill();
	return bDebug;
}


//////////////////////////////////////////////////////////////////////////
//
// szProjectOutput\Source.o for szSource (a path or a name).
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::GetObjectName(const char* szProjectOutput, const char* szSource, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szName;

	szName.Init();
	cFileUtil.GetFileName(&szName, (char*)szSource);
	cFileUtil.RemoveExtension(&szName);
	szName.Append(".o");
	pszDest->Set(szProjectOutput);
	cFileUtil.AppendToPath(pszDest, szName.Text());
	szName.Kill();
}

