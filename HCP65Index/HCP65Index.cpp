/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65Index

Codaphela HCP65Index is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65Index is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65Index.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <stdio.h>
#include "BaseLib/TypeNames.h"
#include "BaseLib/TypeConverter.h"
#include "BaseLib/GlobalMemory.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/ArrayChars.h"
#include "BuildLib/BuildWorkspace.h"
#include "BuildLib/BuildCommandLine.h"
#include "IndexLib/WorkspaceIndex.h"
#include "IndexLib/SymbolTable.h"
#include "IndexLib/ReferenceResolver.h"
#include "IndexLib/PageExamples.h"
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"


#define HCP65_INDEX_EXIT_OK			0
#define HCP65_INDEX_EXIT_FAILED		1
#define HCP65_INDEX_EXIT_USAGE		2


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void PrintUsage(void)
{
	printf("hcp65index -root <folder> -out <file> [-problems] [-libc65 <directory>] <solution.sln>...\n");
	printf("  Indexes the solutions' C++ together, as the IDE does (each solution's first configuration), and writes the\n");
	printf("  symbol table: one line a symbol, <key> TAB <kind> TAB <file> TAB <line>, the file relative to -root.\n");
	printf("  Only symbols declared under -root are written.  -problems lists what the units couldn't include.\n");
	printf("  A solution whose configuration is the W65C816's or the 65C64's is indexed as hcp65build compiles it, with\n");
	printf("  Libc65's headers: -libc65's, or HCP65\\Libraries\\Libc65 found above the first solution.\n");
	printf("hcp65index -resolve <table> -files <list> <page.md>...\n");
	printf("  Resolves the pages' references to code ([`Name`]) against a written table and a list of files (one a line,\n");
	printf("  relative to the table's root), and prints each reference's link or error and each plain span's warning\n");
	printf("  (CReferenceResolver::PrintPageReferences).\n");
	printf("hcp65index -examples [-libc65 <directory>] <solution.sln>... -pages <page.md>...\n");
	printf("  Compiles each page's ```cpp blocks as a unit of the project the page is an item of, after the lines of its\n");
	printf("  hidden <!-- cpp ... --> comment, and prints each error as <page>:<line>:<column>: error: <message>\n");
	printf("  (CPageExamples).\n");
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions added to a new workspace.  HCP65_INDEX_EXIT_FAILED, each
// one that couldn't be read said, if any couldn't.
//
//////////////////////////////////////////////////////////////////////////
static int OpenSolutions(CBuildWorkspace* pcWorkspace, CArrayChars* paszSolutions)
{
	size	uiSolution;
	size	i;
	int		iResult;

	iResult = HCP65_INDEX_EXIT_OK;
	pcWorkspace->Init();
	for (i = 0; i < paszSolutions->NumElements(); i++)
	{
		uiSolution = pcWorkspace->AddSolution(paszSolutions->GetText(i));
		if ((uiSolution == MAX_SIZE) || !pcWorkspace->GetSolution(uiSolution)->mbRead)
		{
			fprintf(stderr, "%s:1:1: error: hcp65index can't read the solution.\n", paszSolutions->GetText(i));
			iResult = HCP65_INDEX_EXIT_FAILED;
		}
	}
	return iResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's index, with this machine's compiler's headers and, for
// the solutions read with an HCP65 configuration, Libc65's: szLibc65 if
// it isn't NULL, else the one found above the first solution.
//
//////////////////////////////////////////////////////////////////////////
static void InitIndex(CWorkspaceIndex* pcIndex, CBuildWorkspace* pcWorkspace, const char* szLibc65, CArrayChars* paszSolutions)
{
	CFileUtil	cFileUtil;
	CChars		szDirectory;
	CChars		szFound;

	pcIndex->Init(pcWorkspace);
	pcIndex->AddDefaultSystemDirectories();
	if (szLibc65 != NULL)
	{
		pcIndex->SetLibc65Directory(szLibc65);
		return;
	}
	szDirectory.Init(paszSolutions->GetText(0));
	cFileUtil.FullPath(&szDirectory);
	cFileUtil.RemoveLastFromPath(&szDirectory);
	szFound.Init();
	if (CBuildCommandLine::FindLibc65(szDirectory.Text(), &szFound))
	{
		pcIndex->SetLibc65Directory(szFound.Text());
	}
	szFound.Kill();
	szDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions indexed into one symbol table for the website's
// references to code (HCP65/Website/DocsPlan.txt D1).
//
//////////////////////////////////////////////////////////////////////////
static int IndexSolutions(const char* szRoot, const char* szOut, const char* szLibc65, CArrayChars* paszSolutions, bool bProblems)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CSymbolTable		cTable;
	CArrayChars			aszProblems;
	size				i;
	size				uiIndexed;
	size				uiToIndex;
	int					iResult;

	iResult = OpenSolutions(&cWorkspace, paszSolutions);

	if (iResult == HCP65_INDEX_EXIT_OK)
	{
		InitIndex(&cIndex, &cWorkspace, szLibc65, paszSolutions);
		if (cIndex.IsMissingSystemDirectories())
		{
			fprintf(stderr, "hcp65index: warning: some of the compiler's own headers aren't on this machine.\n");
		}
		cIndex.StartIndexing();
		cIndex.WaitForIndexing();
		cIndex.GetIndexingProgress(&uiIndexed, &uiToIndex);

		aszProblems.Init();
		cIndex.TakeIndexingProblems(&aszProblems);
		if (bProblems)
		{
			for (i = 0; i < aszProblems.NumElements(); i++)
			{
				printf("%s\n", aszProblems.GetText(i));
			}
		}

		cTable.Init(szRoot);
		cTable.Gather(cIndex.GetIndex());
		if (!cTable.Write(szOut))
		{
			fprintf(stderr, "%s:1:1: error: hcp65index can't write the symbol table.\n", szOut);
			iResult = HCP65_INDEX_EXIT_FAILED;
		}
		else
		{
			printf("hcp65index: %d symbols from %d units in %d ms; %d units couldn't include everything.\n", (int)cTable.NumEntries(), (int)uiIndexed, (int)cIndex.GetIndexingMilliseconds(), (int)cIndex.NumIndexingProblemUnits());
		}
		cTable.Kill();
		aszProblems.Kill();
		cIndex.Kill();
	}
	cWorkspace.Kill();
	return iResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The pages' references to code resolved against a written table, for the
// website's build (HCP65/Website/DocsPlan.txt E1): the same code the IDE
// resolves them with.
//
//////////////////////////////////////////////////////////////////////////
static int ResolvePages(const char* szTable, const char* szFiles, CArrayChars* paszPages)
{
	CSymbolTable			cTable;
	CArrayChars				aszFiles;
	CReferenceResolver		cResolver;
	CTextFile				cPage;
	CArrayPageReferences	asReferences;
	CChars					szPrinted;
	size					i;
	int						iResult;

	iResult = HCP65_INDEX_EXIT_OK;
	cTable.Init(NULL);
	aszFiles.Init();
	if (!cTable.Read(szTable))
	{
		fprintf(stderr, "%s:1:1: error: hcp65index can't read the symbol table.\n", szTable);
		iResult = HCP65_INDEX_EXIT_FAILED;
	}
	else if (!CReferenceResolver::ReadFiles(szFiles, &aszFiles))
	{
		fprintf(stderr, "%s:1:1: error: hcp65index can't read the list of files.\n", szFiles);
		iResult = HCP65_INDEX_EXIT_FAILED;
	}
	else
	{
		cResolver.Init(&cTable, &aszFiles);
		szPrinted.Init();
		for (i = 0; i < paszPages->NumElements(); i++)
		{
			cPage.Init();
			if (!cPage.Read(paszPages->GetText(i)))
			{
				fprintf(stderr, "%s:1:1: error: hcp65index can't read the page.\n", paszPages->GetText(i));
				iResult = HCP65_INDEX_EXIT_FAILED;
			}
			else
			{
				asReferences.Init();
				cResolver.ResolvePage(cPage.Text(), &asReferences);
				szPrinted.Clear();
				CReferenceResolver::PrintPageReferences(paszPages->GetText(i), &asReferences, &szPrinted);
				fputs(szPrinted.Text(), stdout);
				CReferenceResolver::KillPageReferences(&asReferences);
			}
			cPage.Kill();
		}
		szPrinted.Kill();
		cResolver.Kill();
	}
	aszFiles.Kill();
	cTable.Kill();
	return iResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The pages' ```cpp blocks compiled and checked, each as a unit of the
// project the page is an item of (HCP65/Website/DocsPlan.txt F1): the
// solutions' projects are read, not indexed.
//
//////////////////////////////////////////////////////////////////////////
static int CheckExamples(const char* szLibc65, CArrayChars* paszSolutions, CArrayChars* paszPages)
{
	CBuildWorkspace				cWorkspace;
	CWorkspaceIndex				cIndex;
	CPageExamples				cExamples;
	CArrayPageExampleProblems	asProblems;
	CTextFile					cPage;
	CChars						szPage;
	CChars						szPrinted;
	CFileUtil					cFileUtil;
	size						uiProject;
	size						i;
	int							iResult;

	iResult = OpenSolutions(&cWorkspace, paszSolutions);

	if (iResult == HCP65_INDEX_EXIT_OK)
	{
		InitIndex(&cIndex, &cWorkspace, szLibc65, paszSolutions);
		szPrinted.Init();
		for (i = 0; i < paszPages->NumElements(); i++)
		{
			szPage.Init(paszPages->GetText(i));
			cFileUtil.FullPath(&szPage);
			uiProject = cIndex.FindItemProject(szPage.Text());
			cPage.Init();
			if (uiProject == MAX_SIZE)
			{
				printf("%s:1:1: error: The page isn't an item of a project of the solutions, so its examples can't be compiled.\n", paszPages->GetText(i));
			}
			else if (!cPage.Read(paszPages->GetText(i)))
			{
				fprintf(stderr, "%s:1:1: error: hcp65index can't read the page.\n", paszPages->GetText(i));
				iResult = HCP65_INDEX_EXIT_FAILED;
			}
			else
			{
				cExamples.Init();
				asProblems.Init();
				cExamples.Scan(cPage.Text());
				cExamples.Check(&cIndex, uiProject, szPage.Text(), &asProblems);
				szPrinted.Clear();
				CPageExamples::PrintProblems(paszPages->GetText(i), &asProblems, &szPrinted);
				fputs(szPrinted.Text(), stdout);
				CPageExamples::KillProblems(&asProblems);
				cExamples.Kill();
			}
			cPage.Kill();
			szPage.Kill();
		}
		szPrinted.Kill();
		cIndex.Kill();
	}
	cWorkspace.Kill();
	return iResult;
}


//////////////////////////////////////////////////////////////////////////
//
// hcp65index -root <folder> -out <file> [-problems] [-libc65 <directory>] <solution.sln>...
// hcp65index -resolve <table> -files <list> <page.md>...
// hcp65index -examples [-libc65 <directory>] <solution.sln>... -pages <page.md>...
//
//////////////////////////////////////////////////////////////////////////
int __cdecl main(int argc, char* argv[])
{
	CArrayChars		aszArguments;	//The solutions, or with -resolve the pages.
	CArrayChars		aszPages;		//With -examples, after -pages.
	bool			bExamples;
	bool			bPages;
	const char*		szRoot;
	const char*		szOut;
	const char*		szTable;
	const char*		szFiles;
	const char*		szLibc65;
	bool			bProblems;
	bool			bUsage;
	int				i;
	int				iResult;

	TypesInit();
	TypeConverterInit();
	MemoryInit();

	szRoot = NULL;
	szOut = NULL;
	szTable = NULL;
	szFiles = NULL;
	szLibc65 = NULL;
	bProblems = false;
	bUsage = false;
	bExamples = false;
	bPages = false;
	aszArguments.Init();
	aszPages.Init();
	for (i = 1; i < argc; i++)
	{
		if ((StringCompare(argv[i], "-root") == 0) && (i + 1 < argc))
		{
			szRoot = argv[++i];
		}
		else if ((StringCompare(argv[i], "-out") == 0) && (i + 1 < argc))
		{
			szOut = argv[++i];
		}
		else if ((StringCompare(argv[i], "-resolve") == 0) && (i + 1 < argc))
		{
			szTable = argv[++i];
		}
		else if ((StringCompare(argv[i], "-files") == 0) && (i + 1 < argc))
		{
			szFiles = argv[++i];
		}
		else if ((StringCompare(argv[i], "-libc65") == 0) && (i + 1 < argc))
		{
			szLibc65 = argv[++i];
		}
		else if (StringCompare(argv[i], "-problems") == 0)
		{
			bProblems = true;
		}
		else if (StringCompare(argv[i], "-examples") == 0)
		{
			bExamples = true;
		}
		else if (StringCompare(argv[i], "-pages") == 0)
		{
			bPages = true;
		}
		else if (argv[i][0] == '-')
		{
			bUsage = true;
		}
		else if (bPages)
		{
			aszPages.Add(argv[i]);
		}
		else
		{
			aszArguments.Add(argv[i]);
		}
	}

	if (!bUsage && bExamples && bPages && (szTable == NULL) && (szFiles == NULL) && (szRoot == NULL) && (szOut == NULL) && (aszArguments.NumElements() > 0))
	{
		iResult = CheckExamples(szLibc65, &aszArguments, &aszPages);
	}
	else if (!bUsage && !bExamples && !bPages && (szTable != NULL) && (szFiles != NULL) && (szRoot == NULL) && (szOut == NULL))
	{
		iResult = ResolvePages(szTable, szFiles, &aszArguments);
	}
	else if (!bUsage && !bExamples && !bPages && (szTable == NULL) && (szFiles == NULL) && (szRoot != NULL) && (szOut != NULL) && (aszArguments.NumElements() > 0))
	{
		iResult = IndexSolutions(szRoot, szOut, szLibc65, &aszArguments, bProblems);
	}
	else
	{
		PrintUsage();
		iResult = HCP65_INDEX_EXIT_USAGE;
	}
	aszPages.Kill();
	aszArguments.Kill();

	MemoryKill();
	TypeConverterKill();
	TypesKill();
	return iResult;
}

