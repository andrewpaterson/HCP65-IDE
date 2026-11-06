#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BuildLib/BuildWorkspace.h"
#include "TestLib/Assert.h"


#define TEST_WORKSPACE_PROGRAM		"Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln"
#define TEST_WORKSPACE_KERNEL		"Input" _FS_ "Workspace" _FS_ "Kernel" _FS_ "Kernel.sln"
#define TEST_WORKSPACE_FILE			"Output" _FS_ "Test" BUILD_WORKSPACE_EXTENSION


//////////////////////////////////////////////////////////////////////////
//
// A path relative to a directory: up out of the directory's own, then
// down; on another drive, the path itself.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceRelativePath(void)
{
	CChars	sz;

	sz.Init();
	MakeRelativePath("D:" _FS_ "Code" _FS_ "Work", "D:" _FS_ "Code" _FS_ "Kernal" _FS_ "Kernal.sln", &sz);
	AssertString(".." _FS_ "Kernal" _FS_ "Kernal.sln", sz.Text());
	MakeRelativePath("D:" _FS_ "Code", "D:" _FS_ "code" _FS_ "Boot" _FS_ "Boot.sln", &sz);
	AssertString("Boot" _FS_ "Boot.sln", sz.Text());
	MakeRelativePath("C:" _FS_ "Code", "D:" _FS_ "Boot.sln", &sz);
	AssertString("D:" _FS_ "Boot.sln", sz.Text());
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A program's solution and its kernal's: their projects numbered once (the
// shared one once), their configurations by name, and the kernal built
// first because the program loads what it builds.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceSolutions(void)
{
	CBuildWorkspace			cWorkspace;
	CArrayChars				aszNames;
	CArrayInt				aiOrder;
	CVcxSolutionProject*	pcProject;
	size					uiKernel;
	size					uiShared;
	size					i;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_PROGRAM));
	AssertSize(1, cWorkspace.NumSolutions());
	AssertSize(0, cWorkspace.GetStartup());
	AssertSize(2, cWorkspace.NumProjects());
	AssertString("", cWorkspace.GetFile());

	uiKernel = cWorkspace.AddSolution(TEST_WORKSPACE_KERNEL);
	AssertSize(1, uiKernel);
	AssertTrue(cWorkspace.GetSolution(uiKernel)->mbRead);
	AssertSize(MAX_SIZE, cWorkspace.AddSolution(TEST_WORKSPACE_KERNEL));
	AssertSize(0, cWorkspace.GetStartup());

	//Program and Shared (the program's), then Kernel: Shared isn't numbered again.
	AssertSize(3, cWorkspace.NumProjects());
	AssertString("Program", cWorkspace.GetSolutionProject(0)->GetName());
	AssertString("Shared", cWorkspace.GetSolutionProject(1)->GetName());
	AssertString("Kernel", cWorkspace.GetSolutionProject(2)->GetName());
	AssertTrue(cWorkspace.GetProjectSolution(2) == &cWorkspace.GetSolution(uiKernel)->mcSolution);
	uiShared = MAX_SIZE;
	for (i = 0; i < cWorkspace.GetSolution(uiKernel)->mcSolution.NumProjects(); i++)
	{
		pcProject = cWorkspace.GetSolution(uiKernel)->mcSolution.GetProject(i);
		if (strcmp(pcProject->GetName(), "Shared") == 0)
		{
			uiShared = cWorkspace.FindProject(uiKernel, i);
		}
	}
	AssertSize(1, uiShared);
	AssertSize(0, cWorkspace.FindProject(0, 0));

	//The names of both's configurations, the startup solution's first.
	aszNames.Init();
	cWorkspace.GetConfigurationNames(&aszNames);
	AssertSize(2, aszNames.NumElements());
	AssertString("Release|W65C816", aszNames.GetText(0));
	AssertString("Debug|W65C816", aszNames.GetText(1));
	aszNames.Kill();
	AssertTrue(cWorkspace.FindConfiguration(uiKernel, "Release|W65C816")->HasImage());
	AssertTrue(cWorkspace.FindConfiguration(uiKernel, "Debug|W65C816") == NULL);

	//The toolbar's: the names' parts, and either solution's configuration by name, the startup solution's first.
	aszNames.Init();
	cWorkspace.GetConfigurationNames(&aszNames, false);
	AssertSize(2, aszNames.NumElements());
	AssertString("Release", aszNames.GetText(0));
	AssertString("Debug", aszNames.GetText(1));
	aszNames.ReInit();
	cWorkspace.GetConfigurationNames(&aszNames, true);
	AssertSize(1, aszNames.NumElements());
	AssertString("W65C816", aszNames.GetText(0));
	aszNames.Kill();
	AssertTrue(cWorkspace.FindAnyConfiguration("Release|W65C816") == cWorkspace.FindConfiguration(0, "Release|W65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration(NULL, "W65C816") == cWorkspace.FindConfiguration(0, "Release|W65C816"));
	cWorkspace.SetStartup(uiKernel);
	AssertSize(uiKernel, cWorkspace.GetStartup());
	AssertTrue(cWorkspace.FindAnyConfiguration("Release|W65C816") == cWorkspace.FindConfiguration(uiKernel, "Release|W65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration(NULL, "W65C816") == cWorkspace.FindConfiguration(uiKernel, "Release|W65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration("Debug|W65C816") == cWorkspace.FindConfiguration(0, "Debug|W65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration("Debug", NULL) == cWorkspace.FindConfiguration(0, "Debug|W65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration("Profile|W65C816") == NULL);
	AssertTrue(cWorkspace.FindAnyConfiguration("Release", "x64") == NULL);
	aszNames.Init();
	cWorkspace.GetConfigurationNames(&aszNames);
	AssertString("Release|W65C816", aszNames.GetText(0));
	aszNames.Kill();
	cWorkspace.SetStartup(0);

	//The program loads the kernal's ROM: the kernal is built first.  Debug is only the program's.
	AssertTrue(cWorkspace.Loads(0, uiKernel, "Release|W65C816"));
	AssertFalse(cWorkspace.Loads(uiKernel, 0, "Release|W65C816"));
	aiOrder.Init();
	cWorkspace.GetBuildOrder("Release|W65C816", &aiOrder);
	AssertSize(2, aiOrder.NumElements());
	AssertInt((int)uiKernel, aiOrder.GetValue(0));
	AssertInt(0, aiOrder.GetValue(1));
	aiOrder.ReInit();
	cWorkspace.GetBuildOrder("Debug|W65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));

	//Debugging the program, the kernal is what it loads, not a process.  Debugging the kernal, the program on its
	//card is one; without a card (Debug) it isn't.
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Release|W65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());
	cWorkspace.SetStartup(uiKernel);
	cWorkspace.GetProcessPrograms("Release|W65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));
	AssertString("\\p.exe", cWorkspace.FindConfiguration(0, "Release|W65C816")->GetCardPath());
	AssertString("User", cWorkspace.FindConfiguration(0, "Release|W65C816")->GetBreakpointSpace());
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Debug|W65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());
	cWorkspace.SetStartup(0);
	aiOrder.Kill();

	//Removing the startup solution makes the next one it.
	AssertTrue(cWorkspace.RemoveSolution(0));
	AssertSize(1, cWorkspace.NumSolutions());
	AssertSize(0, cWorkspace.GetStartup());
	AssertSize(2, cWorkspace.NumProjects());
	AssertString("Kernel", cWorkspace.GetSolutionProject(0)->GetName());
	AssertFalse(cWorkspace.RemoveSolution(1));

	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A workspace file lists the solutions relative to it and which is the
// startup one, and reads back the same; one that names a solution not
// there isn't read.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceFile(void)
{
	CBuildWorkspace	cWorkspace;
	CBuildWorkspace	cRead;
	CFileUtil		cFileUtil;
	CTextFile		cFile;
	CChars			szFull;

	cFileUtil.MakeDir("Output");
	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_PROGRAM));
	AssertSize(1, cWorkspace.AddSolution(TEST_WORKSPACE_KERNEL));
	cWorkspace.SetStartup(1);
	AssertString("", cWorkspace.GetConfiguration());
	cWorkspace.SetConfiguration("Debug|W65C816");
	AssertTrue(cWorkspace.Write(TEST_WORKSPACE_FILE));
	szFull.Init(TEST_WORKSPACE_FILE);
	cFileUtil.FullPath(&szFull);
	AssertString(szFull.Text(), cWorkspace.GetFile());
	szFull.Kill();

	cFile.Init();
	AssertTrue(cFile.Read(TEST_WORKSPACE_FILE));
	AssertTrue(strstr(cFile.Text(), "<Workspace Configuration=\"Debug|W65C816\">") != NULL);
	AssertTrue(strstr(cFile.Text(), "File=\"..\\Input\\Workspace\\Program\\Program.sln\"") != NULL);
	AssertTrue(strstr(cFile.Text(), "File=\"..\\Input\\Workspace\\Kernel\\Kernel.sln\" Startup=\"true\"") != NULL);
	cFile.Kill();

	cRead.Init();
	AssertTrue(cRead.Open(TEST_WORKSPACE_FILE));
	AssertSize(2, cRead.NumSolutions());
	AssertSize(1, cRead.GetStartup());
	AssertString("Debug|W65C816", cRead.GetConfiguration());
	AssertTrue(cRead.GetStartupSolution()->mszFile.EndsWith("Kernel.sln"));
	AssertSize(3, cRead.NumProjects());

	cFile.Init("<Workspace>\n\t<Solution File=\"Missing.sln\"/>\n</Workspace>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Bad" BUILD_WORKSPACE_EXTENSION));
	cFile.Kill();
	AssertFalse(cRead.Open("Output" _FS_ "Bad" BUILD_WORKSPACE_EXTENSION));
	AssertString("", cRead.GetConfiguration());
	AssertFalse(cRead.Open("Output" _FS_ "Missing" BUILD_WORKSPACE_EXTENSION));

	cRead.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspace(void)
{
	BeginTests();

	TestBuildWorkspaceRelativePath();
	TestBuildWorkspaceSolutions();
	TestBuildWorkspaceFile();

	TestStatistics();
}

