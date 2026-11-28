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
#include "BuildUpToDate.h"
#include "GCCBuild.h"


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
	CChars				szLibrary;
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
	size				uiKept;
	size				uiMade;
	CArrayChars			aszInputs;
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
	uiKept = 0;
	uiMade = 0;
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
			//would be the project's objects' folder.  A project's objects and its library are under its own
			//configuration's name, so the configurations that build it the same way share them: Reference|GCC-x64
			//builds the libraries as Release|GCC-x64, by the solution's rows, and compiles none of them again.
			szProjectOutput.Init();
			GetProjectDirectory(psProject, szOutput.Text(), &szProjectOutput);
			szLibrary.Init(szProjectOutput);
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
						//What it was compiled from last time: its source and the headers that included (Source.d, by -MMD).
						aszInputs.Init();
						aszInputs.Add(szSource.Text());
						szSource.Set(szObject);
						szSource.RemoveFromEnd(1);
						szSource.Append('d');
						if (!ReadBuildDependencies(szSource.Text(), &aszInputs))
						{
							//Not compiled yet (or not by this build): the file that isn't there is what makes it out of date.
							aszInputs.Add(szSource.Text());
						}
						KeepOutput(szObject.Text(), &aszInputs) ? uiKept++ : uiMade++;
						aszInputs.Kill();
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
				szTarget.Init(psProject->bLibrary ? szLibrary : szOutput);
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
					AddOptions(psProject, "Link", &psCommand->aszArguments);
					psCommand->aszArguments.Add("-o");
					psCommand->aszArguments.Add(szTarget.Text());
				}
				uiFirstObject = psCommand->aszArguments.NumElements();
				for (j = 0; j < psProject->pcProject->NumItems(); j++)
				{
					pcItem = psProject->pcProject->GetItem(j);
					if (psProject->pcProject->IsCompiled(pcItem, psProject->pcConfiguration))
					{
						//Named from the file as it is on disk, as its compile named it: a project's spelling of a
						//name can differ in case.
						szSource.Init();
						psProject->pcProject->GetFullPath(pcItem, &szSource);
						szObject.Init();
						GetObjectName(szProjectOutput.Text(), szSource.Text(), &szObject);
						psCommand->aszArguments.Add(szObject.Text());
						szObject.Kill();
						szSource.Kill();
					}
				}
				if (psCommand->aszArguments.NumElements() == uiFirstObject)
				{
					//A project with no sources has nothing to archive or link.
					RemoveLastCommand();
					psCommand = NULL;
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
				if (psCommand != NULL)
				{
					//Made from its objects and, for a program, the solution's libraries: the files among its arguments.
					aszInputs.Init();
					for (j = uiFirstObject; j < psCommand->aszArguments.NumElements(); j++)
					{
						if (cFileUtil.IsAbsolutePath(psCommand->aszArguments.GetText(j)))
						{
							aszInputs.Add(psCommand->aszArguments.GetText(j));
						}
					}
					if (!psProject->bLibrary)
					{
						AddDependencies(psProject, &psCommand->aszArguments);
					}
					KeepOutput(szTarget.Text(), &aszInputs) ? uiKept++ : uiMade++;
					aszInputs.Kill();
				}
				szDescription.Kill();
				szTarget.Kill();
			}
			szLibrary.Kill();
			szProjectOutput.Kill();
		}
	}
	if (uiKept > 0)
	{
		szDescription.Init("Up to date: ");
		szDescription.Append((uint32)uiKept)->Append(" of ")->Append((uint32)(uiKept + uiMade))->Append('.');
		paszLines->Add(szDescription.Text());
		szDescription.Kill();
	}
	aszLibraries.Kill();
	szOutput.Kill();
	asProjects.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a project's objects and library go: the output folder of its own
// configuration if that is a GCC-x64 one (Output\GCC-x64\Release for a
// library a Reference|GCC-x64 build builds as Release|GCC-x64), else the
// build's, szOutput.
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::GetProjectDirectory(SBuildProject* psProject, const char* szOutput, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	pszDest->Set(szOutput);
	if (psProject->pcConfiguration->IsToolchain())
	{
		//Beside the build's own folder, under the project's configuration's name.
		cFileUtil.RemoveLastFromPath(pszDest);
		cFileUtil.AppendToPath(pszDest, psProject->pcConfiguration->GetConfiguration());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The command added last makes szOutput from paszInputs.  If that is up
// to date and this isn't Rebuild All, the command is taken away again and
// the result is true; otherwise what it made before is removed for it to
// make again (BuildUpToDate.h).
//
//////////////////////////////////////////////////////////////////////////
bool CGCCBuild::KeepOutput(const char* szOutput, CArrayChars* paszInputs)
{
	SBuildCommand*	psCommand;

	psCommand = masCommands.Tail();
	if (!mbRebuild && IsBuildOutputUpToDate(psCommand, szOutput, paszInputs))
	{
		RemoveLastCommand();
		return true;
	}
	PrepareBuildOutput(psCommand, szOutput);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// g++ -c -std=c++17 -MMD -I... -D... Source.cpp -o Source.o
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
	psCommand->aszArguments.Add("-MMD");  //Source.d beside Source.o: what it included, for the next build to look at.
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
	AddOptions(psProject, "ClCompile", &psCommand->aszArguments);
	psCommand->aszArguments.Add(szSource);
	psCommand->aszArguments.Add("-o");
	psCommand->aszArguments.Add(szObject);
}


//////////////////////////////////////////////////////////////////////////
//
// A tool's options for a project (szTool is "ClCompile" or "Link"): its
// GCC-x64 configuration's AdditionalOptions.  A project built as its x64
// configuration has none (AddWithoutConfiguration names it).
//
//////////////////////////////////////////////////////////////////////////
void CGCCBuild::AddOptions(SBuildProject* psProject, const char* szTool, CArrayChars* paszArguments)
{
	if (psProject->pcConfiguration->IsToolchain())
	{
		psProject->pcProject->GetAdditionalOptions(szTool, psProject->pcConfiguration, mpcSolution->GetDirectory(), paszArguments);
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
// their x64 one: with its include directories and defines, and no options.
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

