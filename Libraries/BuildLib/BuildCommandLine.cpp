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
#include "BaseLib/TextFile.h"
#include "BuildCommandLine.h"


#define BUILD_POLL_MILLISECONDS		10
#define BUILD_MAX_ARGUMENT_FILES	8		//How deep @files can read others.
#ifdef _WIN32
#define BUILD_TOOL_NAME				"hcp65build.exe"
#else
#define BUILD_TOOL_NAME				"hcp65build"
#endif


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildCommandLine::Init(void)
{
	mszFile.Init();
	mszConfiguration.Init();
	mszLibc65.Init();
	mszShowPlain.Init();
	mbCard = false;
	mbQuiet = false;
	mbHelp = false;
	mbPlain = false;
	mcPlain.Init();
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
	mcPlain.Kill();
	mszShowPlain.Kill();
	mszLibc65.Kill();
	mszConfiguration.Kill();
	mszFile.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::IsOption(const char* szArgument, const char* szName)
{
	return BuildIsOption(szArgument, szName);
}


//////////////////////////////////////////////////////////////////////////
//
// szArgument added to paszDest, or if it is @file the arguments in the
// file: one a line, without blank lines and lines starting with #, and
// each that is itself @file read the same way (to a depth of
// BUILD_MAX_ARGUMENT_FILES).  False, with why written, if a file can't be
// read.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::ExpandArguments(const char* szArgument, CArrayChars* paszDest, size uiDepth)
{
	CTextFile	cFile;
	CChars		szText;
	CArrayChars	aszLines;
	CChars		szLine;
	CChars*		pszLine;
	size		i;
	bool		bResult;

	if ((szArgument[0] != '@') || (szArgument[1] == '\0'))
	{
		paszDest->Add(szArgument);
		return true;
	}
	if (uiDepth >= BUILD_MAX_ARGUMENT_FILES)
	{
		szLine.Init("Argument files read each other too deeply at ");
		szLine.Append(szArgument);
		szLine.Append('.');
		WriteLine(szLine.Text());
		szLine.Kill();
		return false;
	}

	cFile.Init();
	if (!cFile.Read(szArgument + 1))
	{
		cFile.Kill();
		szLine.Init("Could not read the argument file ");
		szLine.Append(szArgument + 1);
		szLine.Append('.');
		WriteLine(szLine.Text());
		szLine.Kill();
		return false;
	}
	szText.Init(cFile.Text());
	cFile.Kill();
	aszLines.Init();
	szText.Replace('\r', '\n');
	szText.Split(&aszLines, '\n');
	szText.Kill();

	bResult = true;
	for (i = 0; bResult && (i < aszLines.NumElements()); i++)
	{
		pszLine = aszLines.Get(i);
		pszLine->StripWhitespace();
		if (pszLine->Empty() || (pszLine->GetChar(0) == '#'))
		{
			continue;
		}
		bResult = ExpandArguments(pszLine->Text(), paszDest, uiDepth + 1);
	}
	aszLines.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The arguments after the tool's name (aszArgv[0]).  False, with why
// written, if they are wrong: no file, two files, an unknown option or an
// option without its value; for a plain command line (a -program or -lib
// in them) what CBuildPlain refuses.  -help (or -?) asks only for the
// usage.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::Parse(int iArgc, const char* const* aszArgv)
{
	CArrayChars	aszArguments;
	size		i;
	const char*	szArgument;
	CChars		szLine;
	CChars		szError;
	int			iPlain;
	bool		bResult;

	aszArguments.Init();
	bResult = true;
	for (i = 1; bResult && (i < (size)iArgc); i++)
	{
		bResult = ExpandArguments(aszArgv[i], &aszArguments, 0);
	}
	for (i = 0; bResult && (i < aszArguments.NumElements()); i++)
	{
		szArgument = aszArguments.GetText(i);
		if (IsOption(szArgument, "program") || IsOption(szArgument, "lib"))
		{
			mbPlain = true;
		}
	}

	szLine.Init();
	szError.Init();
	for (i = 0; bResult && (i < aszArguments.NumElements()); i++)
	{
		szArgument = aszArguments.GetText(i);
		if (IsOption(szArgument, "help") || IsOption(szArgument, "?") || IsOption(szArgument, "h"))
		{
			mbHelp = true;
		}
		else if (IsOption(szArgument, "quiet"))
		{
			mbQuiet = true;
		}
		else if (IsOption(szArgument, "card") || IsOption(szArgument, "config") || IsOption(szArgument, "show-plain"))
		{
			if (mbPlain)
			{
				szLine.Set(szArgument);
				szLine.Append(" is for a solution, not a plain command line (-program, -lib).");
				bResult = false;
			}
			else if (IsOption(szArgument, "card"))
			{
				mbCard = true;
			}
			else if (i + 1 >= aszArguments.NumElements())
			{
				szLine.Set("Option ");
				szLine.Append(szArgument);
				szLine.Append(" needs a value.");
				bResult = false;
			}
			else
			{
				i++;
				(IsOption(szArgument, "config") ? &mszConfiguration : &mszShowPlain)->Set(aszArguments.GetText(i));
			}
		}
		else if (IsOption(szArgument, "libc65"))
		{
			if (i + 1 >= aszArguments.NumElements())
			{
				szLine.Set("Option ");
				szLine.Append(szArgument);
				szLine.Append(" needs a value.");
				bResult = false;
			}
			else
			{
				i++;
				mszLibc65.Set(aszArguments.GetText(i));
			}
		}
		else if (BuildLooksLikeOption(szArgument))
		{
			iPlain = mcPlain.ParseOption(&aszArguments, &i, &szError);
			if (iPlain < 0)
			{
				szLine.Set(szError);
				bResult = false;
			}
			else if ((iPlain == 0) || !mbPlain)
			{
				szLine.Set(iPlain == 0 ? "Unknown option " : "Option ");
				szLine.Append(szArgument);
				szLine.Append(iPlain == 0 ? "." : " is for a plain command line (-program, -lib), not a solution.");
				bResult = false;
			}
		}
		else if (mbPlain)
		{
			if (!mcPlain.AddSource(szArgument, &szError))
			{
				szLine.Set(szError);
				bResult = false;
			}
		}
		else if (!mszFile.Empty())
		{
			szLine.Set("Only one file can be built: ");
			szLine.Append(mszFile);
			szLine.Append(" and ");
			szLine.Append(szArgument);
			szLine.Append('.');
			bResult = false;
		}
		else
		{
			mszFile.Set(szArgument);
		}
	}

	if (bResult && !mbHelp)
	{
		if (mbPlain)
		{
			if (!mcPlain.Finish(&szError))
			{
				szLine.Set(szError);
				bResult = false;
			}
		}
		else if (mszFile.Empty())
		{
			szLine.Set("No solution, project, configurations or workspace to build.");
			bResult = false;
		}
	}
	if (!szLine.Empty())
	{
		WriteLine(szLine.Text());
	}
	szError.Kill();
	szLine.Kill();
	aszArguments.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Libc65's directory: -libc65's (which must be there), else found above
// szToolDirectory (NULL for the current directory), else above szFile's
// directory (or the current directory).  False, with why written, if it
// isn't.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildCommandLine::ResolveLibc65(const char* szToolDirectory, const char* szFile)
{
	CFileUtil	cFileUtil;
	CChars		szDirectory;
	CChars		szLine;
	bool		bResult;

	if (!mszLibc65.Empty())
	{
		cFileUtil.FullPath(&mszLibc65);
		if (!cFileUtil.IsDirectory(mszLibc65.Text()))
		{
			szLine.Init("Libc65 isn't at ");
			szLine.Append(mszLibc65);
			szLine.Append('.');
			WriteLine(szLine.Text());
			szLine.Kill();
			return false;
		}
		return true;
	}

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
		szDirectory.Set("");  //SplitPath appends.
		if (szFile)
		{
			szLine.Init();
			cFileUtil.SplitPath(szFile, &szLine, &szDirectory);
			szLine.Kill();
		}
		else
		{
			cFileUtil.CurrentDirectory(&szDirectory);
		}
		bResult = FindLibc65(szDirectory.Text(), &mszLibc65);
	}
	szDirectory.Kill();
	if (!bResult)
	{
		WriteLine("Could not find Libc65 (HCP65\\Libraries\\Libc65) above the tool's directory or the file's: say where with -libc65.");
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Builds the file in the configuration (Parse), or the plain command line,
// and returns the exit code: BUILD_EXIT_SUCCEEDED, BUILD_EXIT_FAILED or
// BUILD_EXIT_USAGE.  szToolDirectory is where Libc65 is looked for above,
// when -libc65 didn't say (NULL for the current directory), then above the
// file.  With -show-plain the solution's build is written as a plain
// command line instead.
//
//////////////////////////////////////////////////////////////////////////
int CBuildCommandLine::Run(const char* szToolDirectory)
{
	CFileUtil	cFileUtil;
	CArrayInt	aiOrder;
	CChars		szLine;
	size		i;
	bool		bResult;
	int			iResult;

	if (mbHelp)
	{
		WriteUsage();
		return BUILD_EXIT_SUCCEEDED;
	}
	if (mbPlain)
	{
		return RunPlain(szToolDirectory);
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

	if (!ResolveLibc65(szToolDirectory, mszFile.Text()))
	{
		return BUILD_EXIT_USAGE;
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

	if (!mszShowPlain.Empty())
	{
		iResult = ShowPlain(&aiOrder, szToolDirectory);
		aiOrder.Kill();
		return iResult;
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
// Builds the plain command line (CBuildPlain) with CHCP65Build as a
// solution build is, its root the current directory, and returns the exit
// code.
//
//////////////////////////////////////////////////////////////////////////
int CBuildCommandLine::RunPlain(const char* szToolDirectory)
{
	CBuildConfiguration	cConfiguration;
	CHCP65Build			cBuild;
	CArrayChars			aszLines;
	CFileUtil			cFileUtil;
	CChars				szRoot;
	CChars				szPath;
	CChars				szError;
	bool				bRunning;
	EBuildState			eState;

	if (!ResolveLibc65(szToolDirectory, NULL))
	{
		return BUILD_EXIT_USAGE;
	}

	szError.Init();
	mcPlain.MakeConfiguration(&cConfiguration, &szError);
	szError.Kill();
	szRoot.Init();
	cFileUtil.CurrentDirectory(&szRoot);
	cBuild.Init(NULL, &cConfiguration, szRoot.Text());
	cBuild.SetLibc65Directory(mszLibc65.Text());
	szPath.Init();
	if (!StrEmpty(mcPlain.GetOutput()))
	{
		szPath.Set(mcPlain.GetOutput());
		cFileUtil.FullPath(&szPath);
		cBuild.SetOutputDirectory(szPath.Text());
	}
	if (!StrEmpty(mcPlain.GetListings()))
	{
		szPath.Set(mcPlain.GetListings());
		cFileUtil.FullPath(&szPath);
		cBuild.SetListingDirectory(szPath.Text());
	}
	szPath.Kill();
	mcPlain.MakeDescription(cBuild.GetDescription());

	aszLines.Init();
	bRunning = cBuild.Start(&aszLines);
	WriteLines(&aszLines);
	while (bRunning)
	{
		ThreadSleep(BUILD_POLL_MILLISECONDS);
		bRunning = cBuild.Poll(&aszLines);
		WriteLines(&aszLines);
	}
	aszLines.Kill();
	eState = cBuild.GetState();

	cBuild.Kill();
	szRoot.Kill();
	cConfiguration.Kill();
	return (eState == BS_Succeeded) ? BUILD_EXIT_SUCCEEDED : BUILD_EXIT_FAILED;
}


//////////////////////////////////////////////////////////////////////////
//
// Writes the one solution of paiOrder's build in the configuration as a
// plain command line (CBuildPlain::Write) to -show-plain's file, instead
// of building it, and prints it as one PowerShell command line: the tool
// (in szToolDirectory, if known) with the file's arguments, its @ lists
// quoted.  Returns the exit code.
//
//////////////////////////////////////////////////////////////////////////
int CBuildCommandLine::ShowPlain(CArrayInt* paiOrder, const char* szToolDirectory)
{
	CBuildWorkspaceSolution*	pcSolution;
	CBuildConfiguration*		pcConfiguration;
	CHCP65Build					cBuild;
	CHCP65BuildDescription*		pcDescription;
	CArrayChars					aszWritten;
	CArrayChars					aszArguments;
	CChars						szProgram;
	CFileUtil					cFileUtil;
	CChars						szLine;
	CChars						szCurrent;
	size						uiSolution;
	size						i;
	bool						bResult;

	szLine.Init();
	if (paiOrder->NumElements() != 1)
	{
		szLine.Set("-show-plain writes one solution's build, but ");
		szLine.Append(mszFile);
		szLine.Append(" builds ");
		szLine.Append((uint32)paiOrder->NumElements());
		szLine.Append(" in ");
		szLine.Append(mszConfiguration);
		szLine.Append(": give the solution.");
		WriteLine(szLine.Text());
		szLine.Kill();
		return BUILD_EXIT_USAGE;
	}
	uiSolution = (size)paiOrder->GetValue(0);
	pcSolution = mcWorkspace.GetSolution(uiSolution);
	pcConfiguration = mcWorkspace.FindConfiguration(uiSolution, mszConfiguration.Text());
	if (!pcConfiguration->IsHCP65())
	{
		szLine.Set("-show-plain is for an HCP65 configuration (W65C816 or 65C64), not ");
		szLine.Append(mszConfiguration);
		szLine.Append('.');
		WriteLine(szLine.Text());
		szLine.Kill();
		return BUILD_EXIT_USAGE;
	}

	cBuild.Init(&pcSolution->mcSolution, pcConfiguration, NULL);
	cBuild.Describe();
	pcDescription = cBuild.GetDescription();
	for (i = 0; i < pcDescription->NumProjects(); i++)
	{
		if (!pcDescription->GetProject(i)->mszUnreadable.Empty())
		{
			szLine.Set("Could not read ");
			szLine.Append(pcDescription->GetProject(i)->mszUnreadable);
			szLine.Append('.');
			WriteLine(szLine.Text());
			szLine.Kill();
			cBuild.Kill();
			return BUILD_EXIT_FAILED;
		}
	}

	szCurrent.Init();
	cFileUtil.CurrentDirectory(&szCurrent);
	szLine.Set(pcSolution->mszFile);
	szLine.Append(" in ");
	szLine.Append(mszConfiguration);
	szLine.Append(" as a plain command line.  Run in ");
	szLine.Append(szCurrent);
	szLine.Append(": hcp65build @");
	szLine.Append(mszShowPlain);
	szCurrent.Kill();
	aszWritten.Init();
	aszArguments.Init();
	bResult = CBuildPlain::Write(mszShowPlain.Text(), pcDescription, pcConfiguration, mszLibc65.Text(), szLine.Text(), &aszArguments, &aszWritten);
	for (i = 0; i < aszWritten.NumElements(); i++)
	{
		szLine.Set(bResult ? "Wrote " : "Writing ");
		szLine.Append(aszWritten.GetText(i));
		szLine.Append('.');
		WriteLine(szLine.Text());
	}
	if (!bResult)
	{
		WriteLine("Could not write them all.");
	}
	else
	{
		//The same as one PowerShell command line, run where -show-plain was.
		szProgram.Init();
		if (szToolDirectory)
		{
			szProgram.Set(szToolDirectory);
			cFileUtil.AppendToPath(&szProgram, BUILD_TOOL_NAME);
		}
		else
		{
			szProgram.Set(BUILD_TOOL_NAME);
		}
		szCurrent.Init();
		cFileUtil.CurrentDirectory(&szCurrent);
		szLine.Set("PowerShell, in ");
		szLine.Append(szCurrent);
		szLine.Append(':');
		szCurrent.Kill();
		WriteLine("");
		WriteLine(szLine.Text());
		CBuildPlain::MakePowerShellLine(szProgram.Text(), &aszArguments, &szLine);
		WriteLine(szLine.Text());
		szProgram.Kill();
	}
	aszArguments.Kill();
	aszWritten.Kill();
	cBuild.Kill();
	szLine.Kill();
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
	WriteLine("hcp65build: compiles and links HCP65 programs from the command line, as HCP65-IDE does.");
	WriteLine("");
	WriteLine("  hcp65build <file.sln | file.vcxproj | file.hcp65build | file.hcp65workspace> [options]");
	WriteLine("");
	WriteLine("A .hcp65build builds the .sln of the same name beside it (else the .vcxproj).");
	WriteLine("");
	WriteLine("  -config \"Release|W65C816\"  The build configuration (from the .hcp65build beside the solution).");
	WriteLine("                             Default: the workspace's last one, else the first HCP65 one.");
	WriteLine("  -card                      Also put each executable on its configuration's card (Card, CardPath).");
	WriteLine("  -show-plain <file>         Write the build as a plain command line (and a .units file per project)");
	WriteLine("                             instead of building: hcp65build @<file> builds the same.");
	WriteLine("");
	WriteLine("Output goes in Output\\<toolchain>\\<configuration> and listings in Listing\\ beside the solution.");
	WriteLine("");
	WriteLine("  hcp65build -program <name> [-dir <dir>] [-D <define>]... <source>... [-lib <name> ...]... [options]");
	WriteLine("");
	WriteLine("A plain command line: what to compile and link, with no solution.");
	WriteLine("");
	WriteLine("  -program <name>, -lib <name>  A project: its sources follow.  A library's units are linked only when used.");
	WriteLine("  -dir <dir>                    The project's directory, its sources relative to it (default: current).");
	WriteLine("  -D <name>[=<value>]           A define for the project's units.");
	WriteLine("  -I <dir>, -I+ <dir>           An include directory, -I+ with its sub directories.  Before the first");
	WriteLine("                                -program or -lib it is every unit's; after, that project's.");
	WriteLine("  -target W65C816|65C64         The toolchain (default W65C816).");
	WriteLine("  -nofloats, -no-mathtables     No floating point; no maths tables (MathMult).");
	WriteLine("  -exe <file> -entry <symbol> [-limit <n>] [-symbol <name>=<n>]...");
	WriteLine("                                Link an executable.");
	WriteLine("  -image <file> -size <n> [-fill <n>] [-limit <n>] [-load <n>] -region <start>,<end>,<offset>...");
	WriteLine("                                Link an image.  Without -exe or -image the units are only compiled.");
	WriteLine("  -o <dir>, -listings <dir>     Where the program and listings go (default: Output\\<target>\\Plain and");
	WriteLine("                                Listing\\<target>\\Plain in the current directory).");
	WriteLine("");
	WriteLine("Both:");
	WriteLine("");
	WriteLine("  -libc65 <directory>        Libc65 (default: HCP65\\Libraries\\Libc65 above the tool or the file).");
	WriteLine("  -quiet                     Leave out the progress lines (each unit compiled, project, segment).");
	WriteLine("  @<file>                    Arguments from a file, one a line (# starts a comment line).");
	WriteLine("  -help                      This.");
	WriteLine("");
	WriteLine("Numbers are decimal, 0x or $ hexadecimal.  Exit code: 0 built, 1 build failed, 2 bad command line or file.");
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
bool CBuildCommandLine::IsPlain(void) { return mbPlain; }
CBuildPlain* CBuildCommandLine::GetPlain(void) { return &mcPlain; }

