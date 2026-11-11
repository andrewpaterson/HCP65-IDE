#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/Logger.h"
#include "BuildLib/BuildWorkspace.h"
#include "TestLib/Assert.h"


#define TEST_WORKSPACE_PROGRAM		"Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln"
#define TEST_WORKSPACE_KERNEL		"Input" _FS_ "Workspace" _FS_ "Kernel" _FS_ "Kernel.sln"
#define TEST_WORKSPACE_DRIVER		"Input" _FS_ "Workspace" _FS_ "Driver" _FS_ "Driver.sln"
#define TEST_WORKSPACE_TOOLS		"Input" _FS_ "Workspace" _FS_ "Tools" _FS_ "Tools.sln"
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
	//Neither builds a driver, a module the kernal loads.
	cWorkspace.GetModulePrograms("Release|W65C816", &aiOrder);
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
// A driver's solution beside the program's and the kernal's: debugging the
// kernal, the driver on its card is a module it loads, not a process, and
// without a card (Debug) it is neither.  It is never the startup solution's
// own module, and it is built after the kernal it loads.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceModules(void)
{
	CBuildWorkspace			cWorkspace;
	CArrayInt				aiOrder;
	CBuildConfiguration*	pcDriver;
	size					uiKernel;
	size					uiDriver;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_PROGRAM));
	uiKernel = cWorkspace.AddSolution(TEST_WORKSPACE_KERNEL);
	uiDriver = cWorkspace.AddSolution(TEST_WORKSPACE_DRIVER);
	AssertSize(1, uiKernel);
	AssertSize(2, uiDriver);
	AssertSize(4, cWorkspace.NumProjects());

	pcDriver = cWorkspace.FindConfiguration(uiDriver, "Release|W65C816");
	AssertTrue(pcDriver->HasModule());
	AssertInt(BP_Module, pcDriver->GetProgram());
	AssertString("d.drv", pcDriver->GetProgramFile());
	AssertInt(0x1000, pcDriver->GetProgramLimit());
	AssertString("_KernalCreateDriver", pcDriver->GetModule()->GetEntry());
	AssertString("\\d.drv", pcDriver->GetCardPath());
	AssertString("Kernel", pcDriver->GetBreakpointSpace());
	AssertTrue(pcDriver->IsOnCard());
	AssertTrue(cWorkspace.FindConfiguration(uiDriver, "Debug|W65C816")->HasModule());
	AssertFalse(cWorkspace.FindConfiguration(uiDriver, "Debug|W65C816")->IsOnCard());

	//The driver loads the kernal's ROM, as the program does: the kernal is built first.
	AssertTrue(cWorkspace.Loads(uiDriver, uiKernel, "Release|W65C816"));
	AssertFalse(cWorkspace.Loads(uiKernel, uiDriver, "Release|W65C816"));
	aiOrder.Init();
	cWorkspace.GetBuildOrder("Release|W65C816", &aiOrder);
	AssertSize(3, aiOrder.NumElements());
	AssertInt((int)uiKernel, aiOrder.GetValue(0));

	//Debugging the kernal: the driver is its module and the program its process.
	cWorkspace.SetStartup(uiKernel);
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Release|W65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt((int)uiDriver, aiOrder.GetValue(0));
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Release|W65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));

	//Debug's driver isn't on a card, so the kernal has nothing to load.
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Debug|W65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());

	//Debugging the program, the driver is still a module (the program doesn't load it).
	cWorkspace.SetStartup(0);
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Release|W65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt((int)uiDriver, aiOrder.GetValue(0));

	//The startup solution's own driver isn't one of the others.
	cWorkspace.SetStartup(uiDriver);
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Release|W65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Release|W65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));
	aiOrder.Kill();

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
	SLogConfig		sLogConfig;

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
	AssertTrue(strstr(cFile.Text(), "File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln\"") != NULL);
	AssertTrue(strstr(cFile.Text(), "File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Kernel" _FS_ "Kernel.sln\" Startup=\"true\"") != NULL);
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
	//The missing solution is logged as an error, which a Debug build would break on.
	sLogConfig = gcLogger.SetSilent();
	AssertFalse(cRead.Open("Output" _FS_ "Bad" BUILD_WORKSPACE_EXTENSION));
	AssertString("", cRead.GetConfiguration());
	AssertFalse(cRead.Open("Output" _FS_ "Missing" BUILD_WORKSPACE_EXTENSION));
	gcLogger.SetConfig(&sLogConfig);

	cRead.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// One default for each build configuration name, only while there are
// none; found by name and build configuration; written, read back and
// replaced; and a workspace file without any (from before they existed)
// still reads, its configuration still chosen.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceRunConfigurations(void)
{
	CBuildWorkspace			cWorkspace;
	CBuildWorkspace			cRead;
	CArrayRunConfigurations	asReplace;
	CArrayChars				aszNames;
	CRunConfiguration*		pcRun;
	CTextFile				cFile;
	CFileUtil				cFileUtil;
	SLogConfig				sLogConfig;
	size					i;

	cFileUtil.MakeDir("Output");
	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_PROGRAM));
	AssertSize(0, cWorkspace.NumRunConfigurations());
	cWorkspace.AddDefaultRunConfigurations();
	aszNames.Init();
	cWorkspace.GetConfigurationNames(&aszNames);
	AssertTrue(aszNames.NumElements() > 0);
	AssertSize(aszNames.NumElements(), cWorkspace.NumRunConfigurations());
	for (i = 0; i < aszNames.NumElements(); i++)
	{
		pcRun = cWorkspace.GetRunConfiguration(i);
		AssertString(aszNames.GetText(i), pcRun->GetName());
		AssertString(aszNames.GetText(i), pcRun->GetConfiguration());
		AssertFalse(pcRun->HasTool());
		AssertTrue(pcRun->IsDefault());
	}

	pcRun = cWorkspace.AddRunConfiguration("On Emulator", aszNames.GetText(0));
	AssertNotNull(pcRun);
	AssertFalse(pcRun->IsDefault());
	pcRun->SetName(aszNames.GetText(0));
	AssertTrue(pcRun->IsDefault());
	pcRun->SetTool("", "Tools", "");
	AssertFalse(pcRun->IsDefault());
	pcRun->SetTool("", "", "--fill");
	AssertFalse(pcRun->IsDefault());
	pcRun->SetName("On Emulator");
	pcRun->SetTool("Tools" _FS_ "Emulator", "Tools", "--fill --other");
	AssertTrue(pcRun->HasTool());
	AssertNull(cWorkspace.AddRunConfiguration("On Emulator", aszNames.GetText(0)));
	AssertTrue(pcRun == cWorkspace.FindRunConfiguration("On Emulator"));
	AssertNull(cWorkspace.FindRunConfiguration("Nope"));
	AssertString(aszNames.GetText(0), cWorkspace.FindRunConfigurationBuilding(aszNames.GetText(0))->GetName());
	AssertNull(cWorkspace.FindRunConfigurationBuilding("No|Such"));

	cWorkspace.AddDefaultRunConfigurations();
	AssertSize(aszNames.NumElements() + 1, cWorkspace.NumRunConfigurations());

	cWorkspace.SetRunConfigurationName("On Emulator");
	cWorkspace.SetConfiguration(aszNames.GetText(0));
	AssertTrue(cWorkspace.Write(TEST_WORKSPACE_FILE));
	cRead.Init();
	AssertTrue(cRead.Open(TEST_WORKSPACE_FILE));
	AssertString("On Emulator", cRead.GetRunConfigurationName());
	AssertSize(aszNames.NumElements() + 1, cRead.NumRunConfigurations());
	pcRun = cRead.FindRunConfiguration("On Emulator");
	AssertNotNull(pcRun);
	AssertString(aszNames.GetText(0), pcRun->GetConfiguration());
	AssertString("Tools" _FS_ "Emulator", pcRun->GetTool());
	AssertString("Tools", pcRun->GetDirectory());
	AssertString("--fill --other", pcRun->GetArguments());

	AssertNotNull(cWorkspace.AddRunConfiguration("Last", aszNames.GetText(0)));
	cWorkspace.RemoveRunConfiguration(cWorkspace.NumRunConfigurations() - 2);
	AssertSize(aszNames.NumElements() + 1, cWorkspace.NumRunConfigurations());
	AssertNull(cWorkspace.FindRunConfiguration("On Emulator"));
	AssertString("Last", cWorkspace.GetRunConfiguration(cWorkspace.NumRunConfigurations() - 1)->GetName());

	asReplace.Init();
	asReplace.Add()->Init(pcRun);
	asReplace.Get(0)->SetName("Only");
	cRead.SetRunConfigurations(&asReplace);
	asReplace.Get(0)->Kill();
	asReplace.Kill();
	AssertSize(1, cRead.NumRunConfigurations());
	AssertString("Only", cRead.GetRunConfiguration(0)->GetName());
	AssertString("--fill --other", cRead.GetRunConfiguration(0)->GetArguments());

	cFile.Init("<Workspace Configuration=\"Debug|W65C816\">\n\t<Solution File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln\"/>\n</Workspace>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Old" BUILD_WORKSPACE_EXTENSION));
	cFile.Kill();
	AssertTrue(cRead.Open("Output" _FS_ "Old" BUILD_WORKSPACE_EXTENSION));
	AssertString("Debug|W65C816", cRead.GetConfiguration());
	AssertString("", cRead.GetRunConfigurationName());
	AssertSize(0, cRead.NumRunConfigurations());

	cFile.Init("<Workspace>\n\t<Solution File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln\"/>\n\t<RunConfiguration Configuration=\"Debug|W65C816\"/>\n</Workspace>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Bad" BUILD_WORKSPACE_EXTENSION));
	cFile.Kill();
	sLogConfig = gcLogger.SetSilent();
	AssertFalse(cRead.Open("Output" _FS_ "Bad" BUILD_WORKSPACE_EXTENSION));
	gcLogger.SetConfig(&sLogConfig);

	aszNames.Kill();
	cRead.Kill();
	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A solution's startup project is the application chosen, else its first:
// Tools has two (First and Second) and a library, Program one.  It is
// kept in the workspace file by name, only for a solution it was chosen
// for; one that has gone is the first again.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceStartupProject(void)
{
	CBuildWorkspace				cWorkspace;
	CBuildWorkspace				cRead;
	CBuildWorkspaceSolution*	pcTools;
	CBuildWorkspaceSolution*	pcProgram;
	CFileUtil					cFileUtil;
	CTextFile					cFile;

	cFileUtil.MakeDir("Output");
	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_TOOLS));
	AssertSize(1, cWorkspace.AddSolution(TEST_WORKSPACE_PROGRAM));
	pcTools = cWorkspace.GetSolution(0);
	pcProgram = cWorkspace.GetSolution(1);

	//First, Second and Shared: the library isn't an application.
	AssertSize(3, pcTools->mcSolution.NumProjects());
	AssertTrue(pcTools->IsApplication(0));
	AssertTrue(pcTools->IsApplication(1));
	AssertFalse(pcTools->IsApplication(2));
	AssertFalse(pcTools->IsApplication(3));
	AssertSize(2, pcTools->NumApplications());
	AssertSize(1, pcProgram->NumApplications());

	//Not chosen: the first application.
	AssertSize(0, cWorkspace.GetStartupProject(0));
	AssertString("First", cWorkspace.GetStartupProjectName(0));
	AssertString("Program", cWorkspace.GetStartupProjectName(1));
	AssertString("", cWorkspace.GetStartupProjectName(2));
	AssertSize(MAX_SIZE, cWorkspace.GetStartupProject(2));

	//A library can't be chosen, nor a project that isn't there.
	AssertFalse(cWorkspace.SetStartupProject(0, 2));
	AssertFalse(cWorkspace.SetStartupProject(0, 3));
	AssertFalse(cWorkspace.SetStartupProject(2, 0));
	AssertString("First", cWorkspace.GetStartupProjectName(0));
	AssertTrue(cWorkspace.SetStartupProject(0, 1));
	AssertSize(1, cWorkspace.GetStartupProject(0));
	AssertString("Second", cWorkspace.GetStartupProjectName(0));

	//Written for Tools, which it was chosen for, and not for Program.
	AssertTrue(cWorkspace.Write(TEST_WORKSPACE_FILE));
	cFile.Init();
	AssertTrue(cFile.Read(TEST_WORKSPACE_FILE));
	AssertTrue(strstr(cFile.Text(), "Tools.sln\" Startup=\"true\" StartupProject=\"Second\"/>") != NULL);
	AssertTrue(strstr(cFile.Text(), "Program.sln\"/>") != NULL);
	cFile.Kill();

	cRead.Init();
	AssertTrue(cRead.Open(TEST_WORKSPACE_FILE));
	AssertString("Second", cRead.GetStartupProjectName(0));
	AssertString("Program", cRead.GetStartupProjectName(1));
	AssertTrue(cRead.GetSolution(1)->mszStartupProject.Empty());

	//A startup project that has gone (or is a library now): the first application, the name kept.
	cRead.GetSolution(0)->mszStartupProject.Set("Gone");
	AssertString("First", cRead.GetStartupProjectName(0));
	cRead.GetSolution(0)->mszStartupProject.Set("Shared");
	AssertString("First", cRead.GetStartupProjectName(0));
	cRead.GetSolution(0)->mszStartupProject.Set("second");
	AssertString("Second", cRead.GetStartupProjectName(0));

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
	TestBuildWorkspaceModules();
	TestBuildWorkspaceFile();
	TestBuildWorkspaceRunConfigurations();
	TestBuildWorkspaceStartupProject();

	TestStatistics();
}

