#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/Logger.h"
#include "BuildLib/BuildWorkspace.h"
#include "VisualStudioLib/VcxSolutionRows.h"
#include "TestLib/Assert.h"


#define TEST_WORKSPACE_PROGRAM		"Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln"
#define TEST_WORKSPACE_KERNEL		"Input" _FS_ "Workspace" _FS_ "Kernel" _FS_ "Kernel.sln"
#define TEST_WORKSPACE_DRIVER		"Input" _FS_ "Workspace" _FS_ "Driver" _FS_ "Driver.sln"
#define TEST_WORKSPACE_TOOLS		"Input" _FS_ "Workspace" _FS_ "Tools" _FS_ "Tools.sln"
#define TEST_WORKSPACE_OS			"Input" _FS_ "Workspace" _FS_ "OS" _FS_ "OS.sln"
#define TEST_WORKSPACE_MODPLAY_RUN	".." _FS_ ".." _FS_ ".." _FS_ ".." _FS_ "HCP65" _FS_ "ModPlay" _FS_ ".run" _FS_ "ModPlay Debug.hcp65run"
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
// The startup solution debugged in a build configuration as a run
// configuration says (ConfigurationsPlan.txt C8): every solution's program
// that is on the workspace's card (Input/Workspace/Card.hcp65card) is on
// it, and with bLoad the startup solution loads the kernal's ROM first.
// The run configuration is a private one, its paths from the startup
// solution's folder; it is applied again when the startup solution or the
// configuration changes.
//
//////////////////////////////////////////////////////////////////////////
static void TestBuildWorkspaceDebug(CBuildWorkspace* pcWorkspace, const char* szConfiguration, bool bLoad)
{
	CRunConfiguration*	pcRun;
	CChars				szError;
	size				i;

	for (i = pcWorkspace->NumRunConfigurations(); i-- > 0;)
	{
		if (strcmp(pcWorkspace->GetRunConfiguration(i)->GetName(), "Debugged") == 0)
		{
			pcWorkspace->RemoveRunConfiguration(i);
		}
	}
	pcRun = pcWorkspace->AddRunConfiguration("Debugged", szConfiguration);
	pcRun->SetCard("..\\Card.hcp65card");
	if (bLoad)
	{
		pcRun->AddLoad("..\\Kernel\\Output\\HCP65C816\\Release\\k.rom", 0);
	}
	szError.Init();
	AssertTrue(pcWorkspace->ApplyRunConfiguration(pcRun, &szError));
	AssertString("", szError.Text());
	szError.Kill();
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
	AssertSize(3, cWorkspace.NumProjects());
	AssertString("", cWorkspace.GetFile());

	uiKernel = cWorkspace.AddSolution(TEST_WORKSPACE_KERNEL);
	AssertSize(1, uiKernel);
	AssertTrue(cWorkspace.GetSolution(uiKernel)->mbRead);
	AssertSize(MAX_SIZE, cWorkspace.AddSolution(TEST_WORKSPACE_KERNEL));
	AssertSize(0, cWorkspace.GetStartup());

	//Program, Shared and Libc65 (the program's), then Kernel: Shared and Libc65, the runtime both build, aren't
	//numbered again.
	AssertSize(4, cWorkspace.NumProjects());
	AssertString("Program", cWorkspace.GetSolutionProject(0)->GetName());
	AssertString("Shared", cWorkspace.GetSolutionProject(1)->GetName());
	AssertString("Libc65", cWorkspace.GetSolutionProject(2)->GetName());
	AssertString("Kernel", cWorkspace.GetSolutionProject(3)->GetName());
	AssertTrue(cWorkspace.GetProjectSolution(3) == &cWorkspace.GetSolution(uiKernel)->mcSolution);
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

	//The names of both's configurations, which are their solutions' (the HCP65's two and Visual Studio's), the
	//startup solution's first.
	aszNames.Init();
	cWorkspace.GetConfigurationNames(&aszNames);
	AssertSize(3, aszNames.NumElements());
	AssertString("Debug|HCP65C816", aszNames.GetText(0));
	AssertString("Release|HCP65C816", aszNames.GetText(1));
	AssertString("Release|Win32", aszNames.GetText(2));
	aszNames.Kill();
	AssertTrue(cWorkspace.FindConfiguration(uiKernel, "Release|HCP65C816")->HasImage());
	AssertTrue(cWorkspace.FindConfiguration(uiKernel, "Debug|HCP65C816") == NULL);

	//The toolbar's: the names' parts, and either solution's configuration by name, the startup solution's first.
	aszNames.Init();
	cWorkspace.GetConfigurationNames(&aszNames, false);
	AssertSize(2, aszNames.NumElements());
	AssertString("Debug", aszNames.GetText(0));
	AssertString("Release", aszNames.GetText(1));
	aszNames.ReInit();
	cWorkspace.GetConfigurationNames(&aszNames, true);
	AssertSize(2, aszNames.NumElements());
	AssertString("HCP65C816", aszNames.GetText(0));
	AssertString("Win32", aszNames.GetText(1));
	aszNames.Kill();
	AssertTrue(cWorkspace.FindAnyConfiguration("Release|HCP65C816") == cWorkspace.FindConfiguration(0, "Release|HCP65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration(NULL, "HCP65C816") == cWorkspace.FindConfiguration(0, "Debug|HCP65C816"));
	cWorkspace.SetStartup(uiKernel);
	AssertSize(uiKernel, cWorkspace.GetStartup());
	AssertTrue(cWorkspace.FindAnyConfiguration("Release|HCP65C816") == cWorkspace.FindConfiguration(uiKernel, "Release|HCP65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration(NULL, "HCP65C816") == cWorkspace.FindConfiguration(uiKernel, "Release|HCP65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration("Debug|HCP65C816") == cWorkspace.FindConfiguration(0, "Debug|HCP65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration("Debug", NULL) == cWorkspace.FindConfiguration(0, "Debug|HCP65C816"));
	AssertTrue(cWorkspace.FindAnyConfiguration("Profile|HCP65C816") == NULL);
	AssertTrue(cWorkspace.FindAnyConfiguration("Release", "x64") == NULL);
	aszNames.Init();
	cWorkspace.GetConfigurationNames(&aszNames);
	AssertString("Release|HCP65C816", aszNames.GetText(0));
	aszNames.Kill();
	cWorkspace.SetStartup(0);

	//The program loads the kernal's ROM, as its run configuration says: the kernal is built first.  Debug is only
	//the program's.
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", true);
	AssertTrue(cWorkspace.Loads(0, uiKernel, "Release|HCP65C816"));
	AssertFalse(cWorkspace.Loads(uiKernel, 0, "Release|HCP65C816"));
	aiOrder.Init();
	cWorkspace.GetBuildOrder("Release|HCP65C816", &aiOrder);
	AssertSize(2, aiOrder.NumElements());
	AssertInt((int)uiKernel, aiOrder.GetValue(0));
	AssertInt(0, aiOrder.GetValue(1));
	aiOrder.ReInit();
	cWorkspace.GetBuildOrder("Debug|HCP65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));

	//Debugging the program, the kernal is what it loads, not a process.  Debugging the kernal, the program on its
	//card is one; without a card (Debug) it isn't.
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Release|HCP65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());
	cWorkspace.SetStartup(uiKernel);
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", false);
	cWorkspace.GetProcessPrograms("Release|HCP65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));
	AssertString("\\p.exe", cWorkspace.FindConfiguration(0, "Release|HCP65C816")->GetCardPath());
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Debug|HCP65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());
	//Neither builds a driver, a module the kernal loads.
	cWorkspace.GetModulePrograms("Release|HCP65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());
	cWorkspace.SetStartup(0);
	aiOrder.Kill();

	//Removing the startup solution makes the next one it.
	AssertTrue(cWorkspace.RemoveSolution(0));
	AssertSize(1, cWorkspace.NumSolutions());
	AssertSize(0, cWorkspace.GetStartup());
	AssertSize(3, cWorkspace.NumProjects());
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
	AssertSize(5, cWorkspace.NumProjects());

	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", true);
	pcDriver = cWorkspace.FindConfiguration(uiDriver, "Release|HCP65C816");
	AssertTrue(pcDriver->HasModule());
	AssertInt(BP_Module, pcDriver->GetProgram());
	AssertString("d.drv", pcDriver->GetProgramFile());
	AssertInt(0x1000, pcDriver->GetProgramLimit());
	AssertString("_KernalCreateDriver", pcDriver->GetModule()->GetEntry());
	AssertString("\\d.drv", pcDriver->GetCardPath());
	AssertTrue(pcDriver->IsOnCard());
	AssertTrue(cWorkspace.FindConfiguration(uiDriver, "Debug|HCP65C816")->HasModule());
	AssertFalse(cWorkspace.FindConfiguration(uiDriver, "Debug|HCP65C816")->IsOnCard());

	//Debugged, the driver loads the kernal's ROM, as the program does: the kernal is built first.
	cWorkspace.SetStartup(uiDriver);
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", true);
	AssertTrue(cWorkspace.Loads(uiDriver, uiKernel, "Release|HCP65C816"));
	AssertFalse(cWorkspace.Loads(uiKernel, uiDriver, "Release|HCP65C816"));
	aiOrder.Init();
	cWorkspace.GetBuildOrder("Release|HCP65C816", &aiOrder);
	AssertSize(3, aiOrder.NumElements());
	AssertTrue(aiOrder.Find((int)uiKernel) < aiOrder.Find((int)uiDriver));

	//Debugging the kernal: the driver is its module and the program its process.
	cWorkspace.SetStartup(uiKernel);
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", false);
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Release|HCP65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt((int)uiDriver, aiOrder.GetValue(0));
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Release|HCP65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));

	//Debug's driver isn't on a card, so the kernal has nothing to load.
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Debug|HCP65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());

	//Debugging the program, the driver is still a module (the program doesn't load it).
	cWorkspace.SetStartup(0);
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", true);
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Release|HCP65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt((int)uiDriver, aiOrder.GetValue(0));

	//The startup solution's own driver isn't one of the others.
	cWorkspace.SetStartup(uiDriver);
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", true);
	aiOrder.ReInit();
	cWorkspace.GetModulePrograms("Release|HCP65C816", &aiOrder);
	AssertSize(0, aiOrder.NumElements());
	aiOrder.ReInit();
	cWorkspace.GetProcessPrograms("Release|HCP65C816", &aiOrder);
	AssertSize(1, aiOrder.NumElements());
	AssertInt(0, aiOrder.GetValue(0));
	aiOrder.Kill();

	cWorkspace.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// An image put on a card (the HCP65's kernal, OS.SYS, which the boot
// loader in the ROM loads where it is linked) beside a program and the
// ROM: debugging the program or the ROM, the image's line table is read.
// Without a card (Debug) it isn't one, nor is the startup solution's own.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceImages(void)
{
	CBuildWorkspace			cWorkspace;
	CArrayInt				aiImages;
	CBuildConfiguration*	pcOS;
	size					uiKernel;
	size					uiOS;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_WORKSPACE_PROGRAM));
	uiKernel = cWorkspace.AddSolution(TEST_WORKSPACE_KERNEL);
	uiOS = cWorkspace.AddSolution(TEST_WORKSPACE_OS);
	AssertSize(2, uiOS);

	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", true);
	pcOS = cWorkspace.FindConfiguration(uiOS, "Release|HCP65C816");
	AssertTrue(pcOS->HasImage());
	AssertInt(BP_Image, pcOS->GetProgram());
	AssertString("\\OS\\OS.SYS", pcOS->GetCardPath());
	AssertTrue(pcOS->IsOnCard());
	AssertFalse(cWorkspace.FindConfiguration(uiOS, "Debug|HCP65C816")->IsOnCard());

	//Debugging the program: OS.SYS is an image, not a module; the ROM isn't on a card.
	aiImages.Init();
	cWorkspace.GetImagePrograms("Release|HCP65C816", &aiImages);
	AssertSize(1, aiImages.NumElements());
	AssertInt((int)uiOS, aiImages.GetValue(0));
	aiImages.ReInit();
	cWorkspace.GetModulePrograms("Release|HCP65C816", &aiImages);
	AssertSize(0, aiImages.NumElements());
	aiImages.ReInit();
	cWorkspace.GetImagePrograms("Debug|HCP65C816", &aiImages);
	AssertSize(0, aiImages.NumElements());

	//Debugging the ROM it is too; debugging OS.SYS itself, it isn't one of the others.
	cWorkspace.SetStartup(uiKernel);
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", false);
	aiImages.ReInit();
	cWorkspace.GetImagePrograms("Release|HCP65C816", &aiImages);
	AssertSize(1, aiImages.NumElements());
	AssertInt((int)uiOS, aiImages.GetValue(0));
	cWorkspace.SetStartup(uiOS);
	TestBuildWorkspaceDebug(&cWorkspace, "Release|HCP65C816", true);
	aiImages.ReInit();
	cWorkspace.GetImagePrograms("Release|HCP65C816", &aiImages);
	AssertSize(0, aiImages.NumElements());
	aiImages.Kill();

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
	cWorkspace.SetConfiguration("Debug|HCP65C816");
	AssertTrue(cWorkspace.Write(TEST_WORKSPACE_FILE));
	szFull.Init(TEST_WORKSPACE_FILE);
	cFileUtil.FullPath(&szFull);
	AssertString(szFull.Text(), cWorkspace.GetFile());
	szFull.Kill();

	cFile.Init();
	AssertTrue(cFile.Read(TEST_WORKSPACE_FILE));
	AssertTrue(strstr(cFile.Text(), "<Workspace Configuration=\"Debug|HCP65C816\">") != NULL);
	AssertTrue(strstr(cFile.Text(), "File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln\"") != NULL);
	AssertTrue(strstr(cFile.Text(), "File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Kernel" _FS_ "Kernel.sln\" Startup=\"true\"") != NULL);
	cFile.Kill();

	cRead.Init();
	AssertTrue(cRead.Open(TEST_WORKSPACE_FILE));
	AssertSize(2, cRead.NumSolutions());
	AssertSize(1, cRead.GetStartup());
	AssertString("Debug|HCP65C816", cRead.GetConfiguration());
	AssertTrue(cRead.GetStartupSolution()->mszFile.EndsWith("Kernel.sln"));
	AssertSize(4, cRead.NumProjects());

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

	cFile.Init("<Workspace Configuration=\"Debug|HCP65C816\">\n\t<Solution File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln\"/>\n</Workspace>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Old" BUILD_WORKSPACE_EXTENSION));
	cFile.Kill();
	AssertTrue(cRead.Open("Output" _FS_ "Old" BUILD_WORKSPACE_EXTENSION));
	AssertString("Debug|HCP65C816", cRead.GetConfiguration());
	AssertString("", cRead.GetRunConfigurationName());
	AssertSize(0, cRead.NumRunConfigurations());

	cFile.Init("<Workspace>\n\t<Solution File=\".." _FS_ "Input" _FS_ "Workspace" _FS_ "Program" _FS_ "Program.sln\"/>\n\t<RunConfiguration Configuration=\"Debug|HCP65C816\"/>\n</Workspace>\n");
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

	//First, Second, Shared and Libc65: the library isn't an application, nor the runtime.
	AssertSize(4, pcTools->mcSolution.NumProjects());
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
// A program with no configurations file: what it is linked into and the
// whole program's compile options are its project's, read from the HCP65
// groups of the configuration each build configuration builds it in
// (ConfigurationsPlan.txt C4).  A group for the platform alone is every
// configuration's of it; a setting no group has is as it was.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceProjectLinked(void)
{
	CBuildWorkspace			cWorkspace;
	CBuildConfiguration*	pcConfiguration;
	CBuildConfiguration		cBad;
	CVcxSolution			cSolution;
	CChars					szError;

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open("Input" _FS_ "Workspace" _FS_ "Linked" _FS_ "Linked.sln"));

	pcConfiguration = cWorkspace.FindConfiguration(0, "Release|HCP65C816");
	AssertNotNull(pcConfiguration);
	AssertTrue(pcConfiguration->HasExecutable());
	AssertString("l.exe", pcConfiguration->GetProgramFile());
	AssertString("UserStart", pcConfiguration->GetExecutable()->GetEntry());
	AssertInt(0x8000, pcConfiguration->GetProgramLimit());
	AssertSize(2, pcConfiguration->GetExecutable()->NumSymbols());
	AssertString("UserHeapSize", pcConfiguration->GetExecutable()->GetSymbolName(0));
	AssertLong(0x100, pcConfiguration->GetExecutable()->GetSymbolValue(0));
	AssertString("UserStack", pcConfiguration->GetExecutable()->GetSymbolName(1));
	AssertLong(64, pcConfiguration->GetExecutable()->GetSymbolValue(1));
	AssertTrue(pcConfiguration->IsNoFloats());
	AssertFalse(pcConfiguration->IsMathTables());
	AssertTrue(pcConfiguration->IsPreferSize());
	AssertFalse(pcConfiguration->IsExceptions());

	pcConfiguration = cWorkspace.FindConfiguration(0, "Release|HCP65C64");
	AssertNotNull(pcConfiguration);
	AssertTrue(pcConfiguration->HasImage());
	AssertString("l.rom", pcConfiguration->GetProgramFile());
	AssertInt(0x110, pcConfiguration->GetImage()->GetSize());
	AssertInt(0xEA, pcConfiguration->GetImage()->GetFill());
	AssertInt(0xFF00, pcConfiguration->GetImage()->GetLoad());
	AssertInt(0x40000, pcConfiguration->GetImage()->GetText());
	AssertSize(2, pcConfiguration->GetImage()->NumRegions());
	AssertInt(0x10000, pcConfiguration->GetImage()->GetRegion(1)->uiStart);
	AssertInt(0x10010, pcConfiguration->GetImage()->GetRegion(1)->uiEnd);
	AssertInt(0x100, pcConfiguration->GetImage()->GetRegion(1)->uiOffset);
	AssertFalse(pcConfiguration->IsNoFloats());
	AssertTrue(pcConfiguration->IsMathTables());
	AssertFalse(pcConfiguration->IsPreferSize());
	cWorkspace.Kill();

	//An HCP65Link that can't be read says which setting, and a host's configuration reads none.
	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "Workspace" _FS_ "Linked" _FS_ "Bad.vcxproj"));
	szError.Init();
	cBad.Init("Release|HCP65C816", BT_W65C816, "Release|Win32");
	AssertFalse(cBad.ReadProject(cBad.FindProjectConfiguration(cSolution.GetProject(0)->GetProject(), "Release|Win32"), &szError));
	AssertString("HCP65Link's Regions are Start-End@Offset, separated by semicolons.", szError.Text());
	cBad.Kill();
	cBad.Init("Release|GCC-x64", BT_GCC, "Release|Win32");
	AssertTrue(cBad.ReadProject(cBad.FindProjectConfiguration(cSolution.GetProject(0)->GetProject(), "Release|Win32"), &szError));
	AssertFalse(cBad.IsLinked());
	cBad.Kill();
	szError.Kill();
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A shared run configuration is a file of its own in the .run folder of a
// solution, read with the solution; a private one of its name is renamed.
// It says how its build configuration is debugged: the card, a card file
// listing the programs on it, and what is loaded first.  Applied, each
// solution whose program the card lists is on the card at its path, and
// the startup solution's reads the line table of the card's kernal, which
// no solution here builds (ConfigurationsPlan.txt C8, question 27).
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceSharedRunConfigurations(void)
{
	CBuildWorkspace			cWorkspace;
	CBuildWorkspace			cRead;
	CBuildConfiguration*	pcProgram;
	CBuildConfiguration*	pcDriver;
	CRunConfiguration*		pcRun;
	CBuildCard				cCard;
	CTextFile				cFile;
	CFileUtil				cFileUtil;
	CChars					szFull;
	CChars					szError;
	size					uiDriver;

	cFileUtil.RemoveDir("Output" _FS_ "Shared");
	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "Workspace", "Output" _FS_ "Shared"));
	AssertTrue(cFileUtil.TouchDir("Output" _FS_ "Shared" _FS_ "Cards"));
	cCard.Init();
	cCard.SetImage("..\\Card.img");
	cCard.AddProgram("..\\OS\\Output\\$(Platform)\\$(Configuration)\\os.sys", "\\OS\\OS.SYS", "Kernel");
	cCard.AddProgram("..\\Driver\\Output\\$(Platform)\\$(Configuration)\\d.drv", "\\OS\\D.DRV", NULL);
	cCard.AddProgram("..\\Program\\Output\\$(Platform)\\$(Configuration)\\p.exe", "\\run.exe", NULL);
	AssertTrue(cCard.Write("Output" _FS_ "Shared" _FS_ "Cards" _FS_ "Test.hcp65card"));
	cCard.Kill();
	cCard.Init();
	AssertTrue(cCard.Read("Output" _FS_ "Shared" _FS_ "Cards" _FS_ "Test.hcp65card"));
	AssertSize(3, cCard.NumPrograms());
	AssertString("Kernel", cCard.GetProgramLines(0));
	AssertString("", cCard.GetProgramLines(2));
	szFull.Init("Output" _FS_ "Shared" _FS_ "Program" _FS_ "Output" _FS_ "HCP65C816" _FS_ "Debug" _FS_ "p.exe");
	cFileUtil.FullPath(&szFull);
	AssertSize(2, cCard.FindProgram(szFull.Text(), "Debug|HCP65C816"));
	AssertSize(ARRAY_ELEMENT_NOT_FOUND, cCard.FindProgram(szFull.Text(), "Release|HCP65C816"));
	cCard.Kill();
	AssertFalse(cCard.Read("Output" _FS_ "Shared" _FS_ "Cards" _FS_ "None.hcp65card"));
	cCard.Kill();

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open("Output" _FS_ "Shared" _FS_ "Program" _FS_ "Program.sln"));
	uiDriver = cWorkspace.AddSolution("Output" _FS_ "Shared" _FS_ "Driver" _FS_ "Driver.sln");
	AssertSize(1, uiDriver);
	AssertSize(0, cWorkspace.NumRunConfigurations());
	pcRun = cWorkspace.AddRunConfiguration("On Card", "Release|HCP65C816");
	AssertFalse(pcRun->HasDebugging());
	AssertFalse(pcRun->IsShared());
	pcRun->SetShared(cWorkspace.GetSolution(0)->mcSolution.GetDirectory());
	pcRun->SetCard("..\\Cards\\Test.hcp65card");
	pcRun->AddLoad("..\\Kernel\\Output\\HCP65C816\\Release\\k.rom", 0x8000);
	AssertTrue(pcRun->HasDebugging());
	AssertFalse(pcRun->IsDefault());
	cWorkspace.AddRunConfiguration("Mine", "Debug|HCP65C816")->SetTool("Emulator", "", "");
	pcRun = cWorkspace.FindRunConfiguration("On Card");
	AssertTrue(cWorkspace.Write("Output" _FS_ "Shared" _FS_ "Test" BUILD_WORKSPACE_EXTENSION));

	//The shared one is its own file, written as the run configuration is; the workspace file has the private one.
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "On Card.hcp65run"));
	AssertTrue(strstr(cFile.Text(), "<RunConfiguration Name=\"On Card\" Configuration=\"Release|HCP65C816\" Card=\"..\\Cards\\Test.hcp65card\">") != NULL);
	AssertTrue(strstr(cFile.Text(), "<Load File=\"..\\Kernel\\Output\\HCP65C816\\Release\\k.rom\" Address=\"0x8000\"/>") != NULL);
	cFile.Kill();
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "Shared" _FS_ "Test" BUILD_WORKSPACE_EXTENSION));
	AssertTrue(strstr(cFile.Text(), "Mine") != NULL);
	AssertTrue(strstr(cFile.Text(), "On Card") == NULL);
	cFile.Kill();

	//Applied: the program and the driver are on the card where it says; nothing said where before.
	pcProgram = cWorkspace.FindConfiguration(0, "Release|HCP65C816");
	pcDriver = cWorkspace.FindConfiguration(uiDriver, "Release|HCP65C816");
	AssertString("", pcProgram->GetCardPath());
	szError.Init();
	AssertTrue(cWorkspace.ApplyRunConfiguration(pcRun, &szError));
	AssertString("\\run.exe", pcProgram->GetCardPath());
	szFull.Set("Output" _FS_ "Shared" _FS_ "Card.img");
	cFileUtil.FullPath(&szFull);
	AssertString(szFull.Text(), pcProgram->GetCard());
	AssertTrue(pcProgram->IsOnCard());
	AssertString("User", pcProgram->GetBreakpointSpace());
	AssertSize(1, pcProgram->NumLoads());
	szFull.Set("Output" _FS_ "Shared" _FS_ "Kernel" _FS_ "Output" _FS_ "HCP65C816" _FS_ "Release" _FS_ "k.rom");
	cFileUtil.FullPath(&szFull);
	AssertString(szFull.Text(), pcProgram->GetLoadFile(0));
	AssertInt(0x8000, pcProgram->GetLoadAddress(0));
	AssertSize(1, pcProgram->NumLines());
	szFull.Set("Output" _FS_ "Shared" _FS_ "OS" _FS_ "Output" _FS_ "HCP65C816" _FS_ "Release" _FS_ "os.sys");
	cFileUtil.FullPath(&szFull);
	AssertString(szFull.Text(), pcProgram->GetLinesFile(0));
	AssertString("Kernel", pcProgram->GetLinesSpace(0));
	AssertString("\\OS\\OS.SYS", pcProgram->GetLinesCardPath(0));
	AssertString("\\OS\\D.DRV", pcDriver->GetCardPath());
	AssertTrue(pcDriver->IsOnCard());
	AssertSize(0, pcDriver->NumLoads());
	AssertString("Kernel", pcDriver->GetBreakpointSpace());
	//Debug's weren't touched, and a run configuration that says nothing of debugging changes nothing.
	AssertFalse(cWorkspace.FindConfiguration(0, "Debug|HCP65C816")->IsOnCard());
	AssertTrue(cWorkspace.ApplyRunConfiguration(cWorkspace.FindRunConfiguration("Mine"), &szError));
	AssertFalse(cWorkspace.FindConfiguration(0, "Debug|HCP65C816")->IsOnCard());

	//With the kernal's solution here its line table is the workspace's to read, as an image on the card.
	cWorkspace.AddSolution("Output" _FS_ "Shared" _FS_ "OS" _FS_ "OS.sln");
	AssertTrue(cWorkspace.ApplyRunConfiguration(pcRun, &szError));
	AssertSize(0, pcProgram->NumLines());
	AssertString("\\OS\\OS.SYS", cWorkspace.FindConfiguration(2, "Release|HCP65C816")->GetCardPath());

	//A card file that isn't there.
	pcRun->SetCard("..\\Cards\\None.hcp65card");
	AssertFalse(cWorkspace.ApplyRunConfiguration(pcRun, &szError));
	AssertTrue(szError.Contains("None.hcp65card] of the run configuration On Card can't be read."));
	szError.Kill();
	cWorkspace.Kill();

	//Read with its solution.  A private one of a shared one's name is renamed.
	cFile.Init("<RunConfiguration Name=\"Mine\" Configuration=\"Release|HCP65C816\"/>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "Mine.hcp65run"));
	cFile.Kill();
	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Shared" _FS_ "Test" BUILD_WORKSPACE_EXTENSION));
	AssertSize(3, cRead.NumRunConfigurations());
	AssertTrue(cRead.FindRunConfiguration("On Card")->IsShared());
	AssertString("..\\Cards\\Test.hcp65card", cRead.FindRunConfiguration("On Card")->GetCard());
	AssertSize(1, cRead.FindRunConfiguration("On Card")->NumLoads());
	AssertTrue(cRead.FindRunConfiguration("Mine")->IsShared());
	AssertString("Release|HCP65C816", cRead.FindRunConfiguration("Mine")->GetConfiguration());
	AssertFalse(cRead.FindRunConfiguration("Mine (private)")->IsShared());
	AssertString("Emulator", cRead.FindRunConfiguration("Mine (private)")->GetTool());

	//A shared one is its solution's: choosing it makes that the startup solution, and making a solution the startup
	//one chooses its own for the configuration built, or failing that the configuration's name, or its first.
	AssertSize(0, cRead.FindRunConfigurationSolution(cRead.FindRunConfiguration("On Card")));
	AssertSize(MAX_SIZE, cRead.FindRunConfigurationSolution(cRead.FindRunConfiguration("Mine (private)")));
	AssertString("Mine", cRead.FindSolutionRunConfiguration(0, "Release|HCP65C816")->GetName());
	AssertString("Mine", cRead.FindSolutionRunConfiguration(0, "Release|HCP65C64")->GetName());
	AssertString("Mine", cRead.FindSolutionRunConfiguration(0, "Debug|HCP65C816")->GetName());
	AssertNull(cRead.FindSolutionRunConfiguration(1, "Release|HCP65C816"));

	//A private one that is only a build configuration's default goes when a shared one builds the configuration.
	cRead.AddRunConfiguration("Release|HCP65C816", "Release|HCP65C816");
	cRead.AddRunConfiguration("Debug|HCP65C816", "Debug|HCP65C816");
	AssertTrue(cRead.Write("Output" _FS_ "Shared" _FS_ "Defaults" BUILD_WORKSPACE_EXTENSION));
	cRead.Kill();
	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Shared" _FS_ "Defaults" BUILD_WORKSPACE_EXTENSION));
	AssertNull(cRead.FindRunConfiguration("Release|HCP65C816"));
	AssertNotNull(cRead.FindRunConfiguration("Debug|HCP65C816"));
	AssertNotNull(cRead.FindRunConfiguration("Mine (private)"));

	//A file as the HCP65's solutions have them is written back as it is: saving the workspace changes no committed file.
	AssertTrue(cFileUtil.Copy(TEST_WORKSPACE_MODPLAY_RUN, "Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "ModPlay Debug.hcp65run"));
	cRead.Kill();
	cRead.Init();
	AssertTrue(cRead.Read("Output" _FS_ "Shared" _FS_ "Defaults" BUILD_WORKSPACE_EXTENSION));
	AssertNotNull(cRead.FindRunConfiguration("ModPlay Debug"));
	AssertTrue(cFileUtil.Delete("Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "ModPlay Debug.hcp65run"));
	cRead.FindRunConfiguration("Mine")->SetTool("Changed", "", "");
	AssertTrue(cRead.Write("Output" _FS_ "Shared" _FS_ "Defaults" BUILD_WORKSPACE_EXTENSION));
	AssertTrue(cFileUtil.Compare("Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "ModPlay Debug.hcp65run", TEST_WORKSPACE_MODPLAY_RUN));
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "Mine.hcp65run"));
	AssertTrue(strstr(cFile.Text(), "Tool=\"Changed\"") != NULL);
	cFile.Kill();

	//Made private, its file goes and the workspace file has it.
	cRead.FindRunConfiguration("On Card")->SetShared(NULL);
	AssertTrue(cRead.Write("Output" _FS_ "Shared" _FS_ "Test" BUILD_WORKSPACE_EXTENSION));
	AssertFalse(cFileUtil.Exists("Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "On Card.hcp65run"));
	AssertTrue(cFileUtil.Exists("Output" _FS_ "Shared" _FS_ "Program" _FS_ ".run" _FS_ "Mine.hcp65run"));
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "Shared" _FS_ "Test" BUILD_WORKSPACE_EXTENSION));
	AssertTrue(strstr(cFile.Text(), "On Card") != NULL);
	cFile.Kill();
	cRead.Kill();
	szFull.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A workspace written while its solution's platforms were Visual Studio's
// own (Debug|x64, Release|x64), read after they were renamed MSVC-x64
// (ConfigurationsPlan.txt C11-2, C11-3): the configuration it had chosen,
// the run configurations named after the build configurations and the
// one of the user's own that builds one all have the names the solution
// has now, none is repeated, and it is written with them.
//
//////////////////////////////////////////////////////////////////////////
void TestBuildWorkspaceRenamedPlatforms(void)
{
	CBuildWorkspace		cWorkspace;
	CBuildWorkspace		cRead;
	CFileUtil			cFileUtil;
	CTextFile			cFile;
	size				uiRuns;

#define TEST_RENAMED_DIRECTORY	"Output" _FS_ "Renamed"
#define TEST_RENAMED_SOLUTION	TEST_RENAMED_DIRECTORY _FS_ "Program.sln"
#define TEST_RENAMED_WORKSPACE	TEST_RENAMED_DIRECTORY _FS_ "Renamed" BUILD_WORKSPACE_EXTENSION
	cFileUtil.RemoveDir(TEST_RENAMED_DIRECTORY);
	AssertTrue(cFileUtil.TouchDir(TEST_RENAMED_DIRECTORY));
	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "OptionsGCC", TEST_RENAMED_DIRECTORY));

	cWorkspace.Init();
	AssertTrue(cWorkspace.Open(TEST_RENAMED_SOLUTION));
	cWorkspace.AddDefaultRunConfigurations();
	uiRuns = cWorkspace.NumRunConfigurations();
	AssertNotNull(cWorkspace.FindRunConfiguration("Release|x64"));
	cWorkspace.AddRunConfiguration("Mine", "Debug|x64");
	cWorkspace.SetConfiguration("Release|x64");
	cWorkspace.SetRunConfigurationName("Release|x64");
	AssertTrue(cWorkspace.Write(TEST_RENAMED_WORKSPACE));
	cWorkspace.Kill();

	AssertTrue(RenameVcxSolutionConfiguration(TEST_RENAMED_SOLUTION, "Debug|x64", "Debug|MSVC-x64"));
	AssertTrue(RenameVcxSolutionConfiguration(TEST_RENAMED_SOLUTION, "Release|x64", "Release|MSVC-x64"));

	cRead.Init();
	AssertTrue(cRead.Open(TEST_RENAMED_WORKSPACE));
	cRead.AddDefaultRunConfigurations();
	AssertString("Release|MSVC-x64", cRead.GetConfiguration());
	AssertString("Release|MSVC-x64", cRead.GetRunConfigurationName());
	AssertSize(uiRuns + 1, cRead.NumRunConfigurations());
	AssertNull(cRead.FindRunConfiguration("Release|x64"));
	AssertNull(cRead.FindRunConfiguration("Debug|x64"));
	AssertNotNull(cRead.FindRunConfiguration("Release|MSVC-x64"));
	AssertString("Release|MSVC-x64", cRead.FindRunConfiguration("Release|MSVC-x64")->GetConfiguration());
	AssertString("Debug|MSVC-x64", cRead.FindRunConfiguration("Mine")->GetConfiguration());
	AssertString("Debug|GCC-x64", cRead.FindRunConfiguration("Debug|GCC-x64")->GetConfiguration());
	AssertTrue(cRead.Write(TEST_RENAMED_WORKSPACE));
	cRead.Kill();

	cFile.Init();
	AssertTrue(cFile.Read(TEST_RENAMED_WORKSPACE));
	AssertTrue(strstr(cFile.Text(), "Release|MSVC-x64") != NULL);
	AssertTrue(strstr(cFile.Text(), "|x64\"") == NULL);
	cFile.Kill();
	cFileUtil.RemoveDir(TEST_RENAMED_DIRECTORY);
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
	TestBuildWorkspaceImages();
	TestBuildWorkspaceFile();
	TestBuildWorkspaceRunConfigurations();
	TestBuildWorkspaceRenamedPlatforms();
	TestBuildWorkspaceStartupProject();
	TestBuildWorkspaceProjectLinked();
	TestBuildWorkspaceSharedRunConfigurations();

	TestStatistics();
}

