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
#include "IndexLib/WorkspaceIndex.h"
#include "IndexLib/SymbolTable.h"


#define HCP65_INDEX_EXIT_OK			0
#define HCP65_INDEX_EXIT_FAILED		1
#define HCP65_INDEX_EXIT_USAGE		2


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static void PrintUsage(void)
{
	printf("hcp65index -root <folder> -out <file> [-problems] <solution.sln>...\n");
	printf("  Indexes the solutions' C++ together, as the IDE does (each solution's first configuration), and writes the\n");
	printf("  symbol table: one line a symbol, <key> TAB <kind> TAB <file> TAB <line>, the file relative to -root.\n");
	printf("  Only symbols declared under -root are written.  -problems lists what the units couldn't include.\n");
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions indexed into one symbol table for the website's
// references to code (HCP65/Website/DocsPlan.txt D1).
//
//////////////////////////////////////////////////////////////////////////
static int IndexSolutions(const char* szRoot, const char* szOut, CArrayChars* paszSolutions, bool bProblems)
{
	CBuildWorkspace		cWorkspace;
	CWorkspaceIndex		cIndex;
	CSymbolTable		cTable;
	CArrayChars			aszProblems;
	size				i;
	size				uiSolution;
	size				uiIndexed;
	size				uiToIndex;
	int					iResult;

	iResult = HCP65_INDEX_EXIT_OK;
	cWorkspace.Init();
	for (i = 0; i < paszSolutions->NumElements(); i++)
	{
		uiSolution = cWorkspace.AddSolution(paszSolutions->GetText(i));
		if ((uiSolution == MAX_SIZE) || !cWorkspace.GetSolution(uiSolution)->mbRead)
		{
			fprintf(stderr, "%s:1:1: error: hcp65index can't read the solution.\n", paszSolutions->GetText(i));
			iResult = HCP65_INDEX_EXIT_FAILED;
		}
	}

	if (iResult == HCP65_INDEX_EXIT_OK)
	{
		cIndex.Init(&cWorkspace);
		cIndex.AddDefaultSystemDirectories();
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
// hcp65index -root <folder> -out <file> [-problems] <solution.sln>...
//
//////////////////////////////////////////////////////////////////////////
int __cdecl main(int argc, char* argv[])
{
	CArrayChars		aszSolutions;
	const char*		szRoot;
	const char*		szOut;
	bool			bProblems;
	bool			bUsage;
	int				i;
	int				iResult;

	TypesInit();
	TypeConverterInit();
	MemoryInit();

	szRoot = NULL;
	szOut = NULL;
	bProblems = false;
	bUsage = false;
	aszSolutions.Init();
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
		else if (StringCompare(argv[i], "-problems") == 0)
		{
			bProblems = true;
		}
		else if (argv[i][0] == '-')
		{
			bUsage = true;
		}
		else
		{
			aszSolutions.Add(argv[i]);
		}
	}

	if (bUsage || (szRoot == NULL) || (szOut == NULL) || (aszSolutions.NumElements() == 0))
	{
		PrintUsage();
		iResult = HCP65_INDEX_EXIT_USAGE;
	}
	else
	{
		iResult = IndexSolutions(szRoot, szOut, &aszSolutions, bProblems);
	}
	aszSolutions.Kill();

	MemoryKill();
	TypeConverterKill();
	TypesKill();
	return iResult;
}

