/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "BaseLib/FileUtil.h"
#include "VisualStudioLib/VcxItem.h"
#include "GCCBuild.h"


#ifdef _WIN32
#define GCC_EXECUTABLE_EXTENSION	".exe"
#else
#define GCC_EXECUTABLE_EXTENSION	""
#endif


//////////////////////////////////////////////////////////////////////////
//
// The compiles, then the archives, then the links: an application can use
// any of the solution's libraries.
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

	asProjects.Init();
	GetProjects(&asProjects);
	szOutput.Init();
	GetOutputDirectory(&szOutput);
	aszLibraries.Init();
	for (iPass = 0; iPass < 3; iPass++)
	{
		for (i = 0; i < asProjects.NumElements(); i++)
		{
			psProject = asProjects.Get(i);
			szProjectOutput.Init(szOutput);
			cFileUtil.AppendToPath(&szProjectOutput, psProject->pcProject->GetName());
			if (iPass == 0)
			{
				//Compile: Output\GCC\Debug\Project\Source.o.
				cFileUtil.TouchDir(szProjectOutput.Text());
				aszIncludes.Init();
				aszDefines.Init();
				psProject->pcProject->GetIncludeDirectories(psProject->pcConfiguration, mpcSolution->GetDirectory(), &aszIncludes);
				psProject->pcProject->GetPreprocessorDefinitions(psProject->pcConfiguration, mpcSolution->GetDirectory(), &aszDefines);
				for (j = 0; j < psProject->pcProject->NumItems(); j++)
				{
					pcItem = psProject->pcProject->GetItem(j);
					if (pcItem->GetType() == VIT_ClCompile)
					{
						szSource.Init();
						psProject->pcProject->GetFullPath(pcItem, &szSource);
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
				//Archive a library (lib<Project>.a) or link an application (<Project>[.exe]) from its objects.
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
					szTarget.Append(GCC_EXECUTABLE_EXTENSION);
					psCommand = AddCommand("g++", mpcSolution->GetDirectory(), szDescription.Append(psProject->pcProject->GetName())->Text());
					psCommand->aszArguments.Add("-o");
					psCommand->aszArguments.Add(szTarget.Text());
				}
				uiFirstObject = psCommand->aszArguments.NumElements();
				for (j = 0; j < psProject->pcProject->NumItems(); j++)
				{
					pcItem = psProject->pcProject->GetItem(j);
					if (pcItem->GetType() == VIT_ClCompile)
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
	psCommand->aszArguments.Add(szSource);
	psCommand->aszArguments.Add("-o");
	psCommand->aszArguments.Add(szObject);
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

