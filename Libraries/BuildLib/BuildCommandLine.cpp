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
#include <stdio.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/NativeThread.h"
#include "BaseLib/FatImageFile.h"
#include "BuildCommandLine.h"


#define BUILD_POLL_MILLISECONDS		10


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCommandLine::Init(void)
{
	mszFile.Init();
	mszConfiguration.Init();
	mszLibc65.Init();
	mbCard = false;
	mbQuiet = false;
	mbHelp = false;
	mcWorkspace.Init();
	mcBuild.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCommandLine::Kill(void)
{
	mcBuild.Kill();
	mcWorkspace.Kill();
	mszLibc65.Kill();
	mszConfiguration.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szArgument is the option szName: -name, --name or (on Windows)
// /name, any case.  A path on Linux starts with / so isn't an option.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::IsOption(const char* szArgument, const char* szName)
{
	if ((szArgument[0] == '-') && (szArgument[1] == '-'))
	{
		szArgument += 2;
	}
	else if (szArgument[0] == '-')
	{
		szArgument++;
	}
#ifdef _WIN32
	else if (szArgument[0] == '/')
	{
		szArgument++;
	}
#endif
	else
	{
		return false;
	}
	return StringInsensitiveCompare(szArgument, szName) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The arguments after the tool's name (aszArgv[0]).  False, with why
// written, if they are wrong: no file, two files, an unknown option or an
// option without its value.  -help (or -?) asks only for the usage.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::Parse(int iArgc, const char* const* aszArgv)
{
	int			i;
	const char*	szArgument;
	CChars		szLine;

	for (i = 1; i < iArgc; i++)
	{
		szArgument = aszArgv[i];
		if (IsOption(szArgument, "help") || IsOption(szArgument, "?") || IsOption(szArgument, "h"))
		{
			mbHelp = true;
		}
		else if (IsOption(szArgument, "card"))
		{
			mbCard = true;
		}
		else if (IsOption(szArgument, "quiet"))
		{
			mbQuiet = true;
		}
		else if (IsOption(szArgument, "config") || IsOption(szArgument, "libc65"))
		{
			if (i + 1 >= iArgc)
			{
				szLine.Init("Option ");
				szLine.Append(szArgument);
				szLine.Append(" needs a value.");
				WriteLine(szLine.Text());
				szLine.Kill();
				return false;
			}
			i++;
			if (IsOption(szArgument, "config"))
			{
				mszConfiguration.Set(aszArgv[i]);
			}
			else
			{
				mszLibc65.Set(aszArgv[i]);
			}
		}
		else if ((szArgument[0] == '-')
#ifdef _WIN32
			|| (szArgument[0] == '/')
#endif
			)
		{
			szLine.Init("Unknown option ");
			szLine.Append(szArgument);
			szLine.Append('.');
			WriteLine(szLine.Text());
			szLine.Kill();
			return false;
		}
		else if (!mszFile.Empty())
		{
			szLine.Init("Only one file can be built: ");
			szLine.Append(mszFile);
			szLine.Append(" and ");
			szLine.Append(szArgument);
			szLine.Append('.');
			WriteLine(szLine.Text());
			szLine.Kill();
			return false;
		}
		else
		{
			mszFile.Set(szArgument);
		}
	}

	if (mbHelp)
	{
		return true;
	}
	if (mszFile.Empty())
	{
		WriteLine("No solution, project, configurations or workspace to build.");
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Builds the file in the configuration (Parse) and returns the exit code:
// BUILD_EXIT_SUCCEEDED, BUILD_EXIT_FAILED or BUILD_EXIT_USAGE.
// szToolDirectory is where Libc65 is looked for above, when -libc65
// didn't say (NULL for the current directory), then above the file.
//
//////////////////////////////////////////////////////////////////////////
int CBuildCommandLine::Run(const char* szToolDirectory)
{
	CFileUtil	cFileUtil;
	CArrayInt	aiOrder;
	CChars		szLine;
	CChars		szDirectory;
	size		i;
	bool		bResult;

	if (mbHelp)
	{
		WriteUsage();
		return BUILD_EXIT_SUCCEEDED;
	}

	if (!cFileUtil.Exists(mszFile.Text()))
	{
		szLine.Init("Could not find ");
		szLine.Append(mszFile);
		szLine.Append('.');
		WriteLine(szLine.Text());
		szLine.Kill();
		return BUILD_EXIT_USAGE;
	}
	cFileUtil.FullPath(&mszFile);
	if (mszFile.EndsWithIgnoreCase(BUILD_CONFIGURATIONS_EXTENSION))
	{
		//A solution's configurations: the solution (or project) beside them is built.
		szLine.Init();
		if (!FindSolutionFile(mszFile.Text(), &szLine))
		{
			szLine.Set("Could not find the .sln or .vcxproj that ");
			szLine.Append(mszFile);
			szLine.Append(" is for (the same name beside it).");
			WriteLine(szLine.Text());
			szLine.Kill();
			return BUILD_EXIT_USAGE;
		}
		mszFile.Set(szLine);
		szLine.Kill();
	}
	if (!mcWorkspace.Open(mszFile.Text()) || (mcWorkspace.NumSolutions() == 0))
	{
		szLine.Init("Could not read ");
		szLine.Append(mszFile);
		szLine.Append(" (a .sln, .vcxproj, ");
		szLine.Append(BUILD_CONFIGURATIONS_EXTENSION);
		szLine.Append(" or ");
		szLine.Append(BUILD_WORKSPACE_EXTENSION);
		szLine.Append(").");
		WriteLine(szLine.Text());
		szLine.Kill();
		return BUILD_EXIT_USAGE;
	}

	if (mszLibc65.Empty())
	{
		szDirectory.Init();
		if (szToolDirectory)
		{
			szDirectory.Set(szToolDirectory);
		}
		else
		{
			cFileUtil.CurrentDirectory(&szDirectory);
		}
		bResult = FindLibc65(szDirectory.Text(), &mszLibc65);
		if (!bResult)
		{
			//A tool built outside the tree (on Linux, in ~/build): the solution is in it.
			szLine.Init();
			szDirectory.Set("");  //SplitPath appends.
			cFileUtil.SplitPath(mszFile.Text(), &szLine, &szDirectory);
			szLine.Kill();
			bResult = FindLibc65(szDirectory.Text(), &mszLibc65);
		}
		szDirectory.Kill();
		if (!bResult)
		{
			WriteLine("Could not find Libc65 (HCP65\\Libraries\\Libc65) above the tool's directory or the file's: say where with -libc65.");
			return BUILD_EXIT_USAGE;
		}
	}
	else
	{
		cFileUtil.FullPath(&mszLibc65);
		if (!cFileUtil.IsDirectory(mszLibc65.Text()))
		{
			szLine.Init("Libc65 isn't at ");
			szLine.Append(mszLibc65);
			szLine.Append('.');
			WriteLine(szLine.Text());
			szLine.Kill();
			return BUILD_EXIT_USAGE;
		}
	}

	if (!ChooseConfiguration())
	{
		return BUILD_EXIT_USAGE;
	}

	aiOrder.Init();
	mcWorkspace.GetBuildOrder(mszConfiguration.Text(), &aiOrder);
	if (aiOrder.NumElements() == 0)
	{
		szLine.Init("Nothing in ");
		szLine.Append(mszFile);
		szLine.Append(" has the configuration ");
		szLine.Append(mszConfiguration);
		szLine.Append('.');
		WriteLine(szLine.Text());
		szLine.Kill();
		aiOrder.Kill();
		return BUILD_EXIT_USAGE;
	}

	bResult = true;
	for (i = 0; bResult && (i < aiOrder.NumElements()); i++)
	{
		bResult = BuildSolution((size)aiOrder.GetValue(i), aiOrder.NumElements() > 1);
	}
	aiOrder.Kill();
	return bResult ? BUILD_EXIT_SUCCEEDED : BUILD_EXIT_FAILED;
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration -config named (it must be one of the workspace's),
// else the workspace's last chosen one, else the first HCP65
// configuration (the first of any if none is HCP65).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::ChooseConfiguration(void)
{
	CArrayChars				aszNames;
	CBuildConfiguration*	pcConfiguration;
	CChars					szLine;
	size					i;

	aszNames.Init();
	mcWorkspace.GetConfigurationNames(&aszNames);
	if (!mszConfiguration.Empty())
	{
		if (!mcWorkspace.FindAnyConfiguration(mszConfiguration.Text()))
		{
			szLine.Init("No configuration ");
			szLine.Append(mszConfiguration);
			szLine.Append(". There are:");
			for (i = 0; i < aszNames.NumElements(); i++)
			{
				szLine.Append(" \"");
				szLine.Append(aszNames.GetText(i));
				szLine.Append('"');
			}
			szLine.Append('.');
			WriteLine(szLine.Text());
			szLine.Kill();
			aszNames.Kill();
			return false;
		}
		aszNames.Kill();
		return true;
	}

	if (!StrEmpty(mcWorkspace.GetConfiguration()) && mcWorkspace.FindAnyConfiguration(mcWorkspace.GetConfiguration()))
	{
		mszConfiguration.Set(mcWorkspace.GetConfiguration());
		aszNames.Kill();
		return true;
	}
	for (i = 0; i < aszNames.NumElements(); i++)
	{
		pcConfiguration = mcWorkspace.FindAnyConfiguration(aszNames.GetText(i));
		if (pcConfiguration && pcConfiguration->IsHCP65())
		{
			mszConfiguration.Set(aszNames.GetText(i));
			aszNames.Kill();
			return true;
		}
	}
	if (aszNames.NumElements() > 0)
	{
		mszConfiguration.Set(aszNames.GetText(0));
		aszNames.Kill();
		return true;
	}
	aszNames.Kill();
	WriteLine("There are no build configurations.");
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Builds the workspace's solution uiSolution, polling until it has
// finished, its lines written as they come (after a heading saying which
// solution, bHeading).  With -card its program then goes on its card.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::BuildSolution(size uiSolution, bool bHeading)
{
	CBuildWorkspaceSolution*	pcSolution;
	CBuildConfiguration*		pcConfiguration;
	CArrayChars					aszLines;
	CChars						szLine;
	bool						bRunning;

	pcSolution = mcWorkspace.GetSolution(uiSolution);
	pcConfiguration = mcWorkspace.FindConfiguration(uiSolution, mszConfiguration.Text());
	if (bHeading)
	{
		szLine.Init("------ ");
		szLine.Append(pcSolution->mcSolution.GetName());
		szLine.Append(": ");
		szLine.Append(mszConfiguration);
		szLine.Append(" ------");
		WriteLine(szLine.Text());
		szLine.Kill();
	}

	aszLines.Init();
	bRunning = mcBuild.Start(&pcSolution->mcSolution, pcConfiguration, NULL, mszLibc65.Text(), &aszLines);
	WriteLines(&aszLines);
	while (bRunning)
	{
		ThreadSleep(BUILD_POLL_MILLISECONDS);
		bRunning = mcBuild.Poll(&aszLines);
		WriteLines(&aszLines);
	}
	aszLines.Kill();

	if (mcBuild.GetState() != BS_Succeeded)
	{
		return false;
	}
	if (mbCard && pcConfiguration->HasExecutable() && pcConfiguration->HasCard())
	{
		return PutOnCard(uiSolution, pcConfiguration);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The built executable onto the configuration's card (an SD card image,
// relative to the solution's .hcp65build) as its CardPath, as the IDE
// does before debugging.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::PutOnCard(size uiSolution, CBuildConfiguration* pcConfiguration)
{
	CBuildWorkspaceSolution*	pcSolution;
	CFileUtil					cFileUtil;
	CChars						szProgram;
	CChars						szCard;
	CChars						szName;
	CChars						szLine;
	bool						bResult;

	pcSolution = mcWorkspace.GetSolution(uiSolution);
	szProgram.Init();
	CBuild::GetProgramPath(pcSolution->mcSolution.GetDirectory(), pcConfiguration, &szProgram);

	szCard.Init();
	if (cFileUtil.IsAbsolutePath(pcConfiguration->GetCard()))
	{
		szCard.Set(pcConfiguration->GetCard());
	}
	else
	{
		szLine.Init();
		szName.Init();
		CBuildConfigurations::GetFileName(pcSolution->mszFile.Text(), &szLine);
		cFileUtil.SplitPath(szLine.Text(), &szName, &szCard);
		cFileUtil.AppendToPath(&szCard, pcConfiguration->GetCard());
		cFileUtil.FullPath(&szCard);
		szName.Kill();
		szLine.Kill();
	}

	bResult = FatPutDiskFileOnImage(szCard.Text(), pcConfiguration->GetCardPath(), szProgram.Text());
	szLine.Init(bResult ? "Wrote " : "Could not write ");
	szLine.Append(szProgram);
	szLine.Append(" onto ");
	szLine.Append(szCard);
	szLine.Append(" as ");
	szLine.Append(pcConfiguration->GetCardPath());
	szLine.Append(bResult ? "." : " (an SD card image with a FAT volume).");
	WriteLine(szLine.Text());
	szLine.Kill();
	szCard.Kill();
	szProgram.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Writes the lines and empties paszLines.  -quiet leaves out the
// progress lines (IsProgressLine).
//
//////////////////////////////////////////////////////////////////////////
void CBuildCommandLine::WriteLines(CArrayChars* paszLines)
{
	size	i;
	char*	szLine;

	for (i = 0; i < paszLines->NumElements(); i++)
	{
		szLine = paszLines->GetText(i);
		if (!mbQuiet || !IsProgressLine(szLine))
		{
			WriteLine(szLine);
		}
	}
	paszLines->ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szLine only says how the build is going, which -quiet leaves
// out: a unit compiled ("Maths.cpp (12/40)", "Maths.cpp (compiled
// again)"), a project added ("Tiny: Debug|Win32") or a segment linked
// ("Segment $010000, 512 bytes.").  Errors and warnings aren't all
// marked as such ("stdlib.cpp (8, 10): Could not include ..."), so
// everything else is kept.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::IsProgressLine(const char* szLine)
{
	size		uiLength;
	const char*	szOpen;
	const char*	sz;
	bool		bSlash;

	if (strncmp(szLine, "Segment $", 9) == 0)
	{
		return true;
	}
	uiLength = strlen(szLine);
	if ((uiLength > 0) && (szLine[uiLength - 1] == ')'))
	{
		if ((uiLength >= 17) && (strcmp(&szLine[uiLength - 17], " (compiled again)") == 0))
		{
			return true;
		}
		szOpen = strrchr(szLine, '(');
		if (szOpen && (szOpen > szLine) && (szOpen[-1] == ' '))
		{
			//(n/m): digits, one slash, digits.
			bSlash = false;
			for (sz = szOpen + 1; *sz != ')'; sz++)
			{
				if (*sz == '/')
				{
					if (bSlash || (sz == szOpen + 1))
					{
						return false;
					}
					bSlash = true;
				}
				else if ((*sz < '0') || (*sz > '9'))
				{
					return false;
				}
			}
			return bSlash && (sz[-1] != '/');
		}
		return false;
	}
	//"Project: Configuration|Platform", a single word before the colon.
	sz = strstr(szLine, ": ");
	if (!sz || memchr(szLine, ' ', (size)(sz - szLine)))
	{
		return false;
	}
	return strchr(sz + 2, '|') && !strchr(sz + 2, ' ');
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCommandLine::WriteLine(const char* szLine)
{
	printf("%s\n", szLine);
	fflush(stdout);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCommandLine::WriteUsage(void)
{
	WriteLine("hcp65build: builds an HCP65-IDE solution, project or workspace from the command line.");
	WriteLine("");
	WriteLine("  hcp65build <file.sln | file.vcxproj | file.hcp65build | file.hcp65workspace> [options]");
	WriteLine("");
	WriteLine("A .hcp65build builds the .sln of the same name beside it (else the .vcxproj).");
	WriteLine("");
	WriteLine("  -config \"Release|W65C816\"  The build configuration (from the .hcp65build beside the solution).");
	WriteLine("                             Default: the workspace's last one, else the first HCP65 one.");
	WriteLine("  -card                      Also put each executable on its configuration's card (Card, CardPath).");
	WriteLine("  -libc65 <directory>        Libc65 (default: HCP65\\Libraries\\Libc65 above the tool or the file).");
	WriteLine("  -quiet                     Leave out the progress lines (each unit compiled, project, segment).");
	WriteLine("  -help                      This.");
	WriteLine("");
	WriteLine("Output goes in Output\\<toolchain>\\<configuration> and listings in Listing\\ beside the solution.");
	WriteLine("Exit code: 0 built, 1 build failed, 2 bad command line or file.");
}


//////////////////////////////////////////////////////////////////////////
//
// Libc65 in szDirectory or a directory above it: the first of
// <dir>\HCP65\Libraries\Libc65 found going up.  False if none is.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::FindLibc65(const char* szDirectory, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szAbove;
	CChars		szTry;
	size		uiLength;

	szAbove.Init(szDirectory);
	cFileUtil.FullPath(&szAbove);
	for (;;)
	{
		szTry.Init(szAbove);
		cFileUtil.AppendToPath(&szTry, "HCP65");
		cFileUtil.AppendToPath(&szTry, "Libraries");
		cFileUtil.AppendToPath(&szTry, "Libc65");
		if (cFileUtil.IsDirectory(szTry.Text()))
		{
			pszDest->Set(szTry);
			szTry.Kill();
			szAbove.Kill();
			return true;
		}
		szTry.Kill();

		uiLength = szAbove.Length();
		cFileUtil.RemoveLastFromPath(&szAbove);
		if (szAbove.Empty() || (szAbove.Length() >= uiLength))
		{
			szAbove.Kill();
			return false;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The solution a configurations file (szConfigurations, .hcp65build) is
// for: the .sln of the same name beside it, else the .vcxproj (a project
// opened as its own solution), as CBuildConfigurations::GetFileName names
// them.  False if neither is there.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::FindSolutionFile(const char* szConfigurations, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	pszDest->Set(szConfigurations);
	cFileUtil.RemoveExtension(pszDest);
	pszDest->Append(".sln");
	if (cFileUtil.Exists(pszDest->Text()))
	{
		return true;
	}
	cFileUtil.RemoveExtension(pszDest);
	pszDest->Append(".vcxproj");
	if (cFileUtil.Exists(pszDest->Text()))
	{
		return true;
	}
	pszDest->Set("");
	return false;
}


char* CBuildCommandLine::GetFile(void) { return mszFile.Text(); }
char* CBuildCommandLine::GetConfiguration(void) { return mszConfiguration.Text(); }
char* CBuildCommandLine::GetLibc65(void) { return mszLibc65.Text(); }
bool CBuildCommandLine::IsCard(void) { return mbCard; }
bool CBuildCommandLine::IsQuiet(void) { return mbQuiet; }
bool CBuildCommandLine::IsHelp(void) { return mbHelp; }

