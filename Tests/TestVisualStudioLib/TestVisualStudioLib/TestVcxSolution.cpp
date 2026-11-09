#include "BaseLib/FileUtil.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/Logger.h"
#include "VisualStudioLib/VcxSolution.h"
#include "VisualStudioLib/VcxPath.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// A solution with projects, a solution folder with items, a nested
// project, dependencies, configurations that map to other project
// configurations, a project path in the wrong case and a project that
// isn't there.  Its .sln has a byte order mark and CRLF line ends.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxSolutionMini(void)
{
	CVcxSolution						cSolution;
	CVcxSolutionProject*				pcMini;
	CVcxSolutionProject*				pcFolder;
	CVcxSolutionProject*				pcSupport;
	CVcxSolutionProject*				pcMissing;
	CVcxSolutionProjectConfiguration*	pcConfiguration;
	CArrayInt							aiIndices;
	CChars								szExpected;
	CFileUtil							cFileUtil;

	cSolution.Init();
	AssertTrue(cSolution.Read("Input" _FS_ "MiniSolution" _FS_ "MiniSolution.sln"));
	AssertString("MiniSolution", cSolution.GetName());
	AssertString("12.00", cSolution.GetFormatVersion());
	AssertString("17.8.34525.116", cSolution.GetVisualStudioVersion());
	AssertString("10.0.40219.1", cSolution.GetMinimumVisualStudioVersion());
	AssertString("{49BCF8E7-2AB6-40A6-B73F-4715E8018800}", cSolution.GetSolutionGuid());
	AssertFalse(cSolution.IsHideSolutionNode());

	AssertSize(2, cSolution.NumConfigurations());
	AssertString("Debug|Win32", cSolution.GetConfiguration(0));
	AssertString("Release|x64", cSolution.GetConfiguration(1));

	//The projects in the order the .sln lists them.
	AssertSize(4, cSolution.NumProjects());
	AssertSize(2, cSolution.NumLoadedProjects());
	pcMini = cSolution.GetProject(0);
	pcFolder = cSolution.GetProject(1);
	pcSupport = cSolution.GetProject(2);
	pcMissing = cSolution.GetProject(3);

	AssertString("MiniLib", pcMini->GetName());
	AssertString("..\\MiniLib\\MiniLib.vcxproj", pcMini->GetPath());
	AssertString("{0A1B2C3D-4E5F-4061-8293-A4B5C6D7E8F9}", pcMini->GetGuid());
	AssertTrue(pcMini->IsVcxProject());
	AssertFalse(pcMini->IsFolder());
	AssertTrue(pcMini->IsLoaded());
	AssertString("MiniLib", pcMini->GetProject()->GetName());
	AssertString("{0A1B2C3D-4E5F-4061-8293-A4B5C6D7E8F9}", pcMini->GetProject()->GetGuid());
	AssertString("", pcMini->GetParentGuid());
	AssertSize(1, pcMini->NumDependencies());
	AssertString("{221022F1-B103-456A-8F55-721F33F2E26D}", pcMini->GetDependency(0));

	AssertString("Libraries", pcFolder->GetName());
	AssertTrue(pcFolder->IsFolder());
	AssertFalse(pcFolder->IsVcxProject());
	AssertFalse(pcFolder->IsLoaded());
	AssertSize(2, pcFolder->NumSolutionItems());
	AssertString("ReadMe.txt", pcFolder->GetSolutionItem(0));
	AssertString("..\\MiniLib\\Notes.md", pcFolder->GetSolutionItem(1));

	//Found though the .sln has its path in another case.
	AssertTrue(pcSupport->IsLoaded());
	AssertString("SupportLib", pcSupport->GetProject()->GetName());
	szExpected.Init("Input" _FS_ "SupportLib" _FS_ "SupportLib.vcxproj");
	cFileUtil.FullPath(&szExpected);
	AssertString(szExpected.Text(), pcSupport->GetFullPath());
	szExpected.Kill();
	AssertString("{11111111-2222-3333-4444-555555555555}", pcSupport->GetParentGuid());

	AssertString("Missing", pcMissing->GetName());
	AssertTrue(pcMissing->IsVcxProject());
	AssertFalse(pcMissing->IsLoaded());
	AssertNull(pcMissing->GetProject());

	//GUIDs and names aren't case sensitive.
	AssertTrue(cSolution.FindProject("{221022f1-b103-456a-8f55-721f33f2e26d}") == pcSupport);
	AssertTrue(cSolution.FindProjectByName("minilib") == pcMini);
	AssertNull(cSolution.FindProject("{ABCDEFAB-0000-0000-0000-000000000000}"));

	//The project configuration each solution configuration builds.
	AssertSize(2, pcMini->NumConfigurations());
	pcConfiguration = pcMini->FindConfiguration("Debug|Win32");
	AssertString("Debug|Win32", pcConfiguration->GetProjectConfiguration());
	AssertTrue(pcConfiguration->IsBuild());
	AssertFalse(pcConfiguration->IsDeploy());
	pcConfiguration = pcMini->FindConfiguration("release|X64");
	AssertString("Release|x64", pcConfiguration->GetProjectConfiguration());
	AssertFalse(pcConfiguration->IsBuild());
	pcConfiguration = pcSupport->FindConfiguration("Release|x64");
	AssertString("Release|Win32", pcConfiguration->GetProjectConfiguration());
	AssertTrue(pcConfiguration->IsBuild());
	AssertTrue(pcConfiguration->IsDeploy());
	AssertSize(0, pcFolder->NumConfigurations());
	AssertNull(pcMissing->FindConfiguration("Debug|Win32"));

	//The tree Solution Explorer shows.
	aiIndices.Init();
	cSolution.GetChildProjects("", &aiIndices);
	AssertSize(3, aiIndices.NumElements());
	AssertInt(0, aiIndices.GetValue(0));
	AssertInt(1, aiIndices.GetValue(1));
	AssertInt(3, aiIndices.GetValue(2));
	aiIndices.ReInit();
	cSolution.GetChildProjects("{11111111-2222-3333-4444-555555555555}", &aiIndices);
	AssertSize(1, aiIndices.NumElements());
	AssertInt(2, aiIndices.GetValue(0));
	aiIndices.Kill();

	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Reading only the .sln leaves every project unloaded.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxSolutionWithoutProjects(void)
{
	CVcxSolution	cSolution;

	cSolution.Init();
	AssertTrue(cSolution.Read("Input" _FS_ "MiniSolution" _FS_ "MiniSolution.sln", false));
	AssertSize(4, cSolution.NumProjects());
	AssertSize(0, cSolution.NumLoadedProjects());
	AssertString("", cSolution.GetProject(0)->GetFullPath());
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A .vcxproj opened on its own is in a solution of its own.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxSolutionFromProject(void)
{
	CVcxSolution			cSolution;
	CVcxSolutionProject*	pcProject;
	CChars					szExpected;
	CFileUtil				cFileUtil;
	SLogConfig				sLogConfig;

	cSolution.Init();
	AssertTrue(cSolution.ReadProject("Input" _FS_ "MiniLib" _FS_ "MiniLib.vcxproj"));
	AssertString("MiniLib", cSolution.GetName());
	szExpected.Init("Input" _FS_ "MiniLib" _FS_ "MiniLib.sln");
	cFileUtil.FullPath(&szExpected);
	AssertString(szExpected.Text(), cSolution.GetFileName());
	szExpected.Kill();
	AssertSize(1, cSolution.NumProjects());
	AssertSize(1, cSolution.NumLoadedProjects());
	pcProject = cSolution.GetProject(0);
	AssertString("MiniLib", pcProject->GetName());
	AssertString("MiniLib.vcxproj", pcProject->GetPath());
	AssertString("{0A1B2C3D-4E5F-4061-8293-A4B5C6D7E8F9}", pcProject->GetGuid());
	AssertSize(2, cSolution.NumConfigurations());
	AssertString("Release|x64", cSolution.GetConfiguration(1));
	AssertString("Release|x64", pcProject->FindConfiguration("Release|x64")->GetProjectConfiguration());
	AssertTrue(pcProject->FindConfiguration("Release|x64")->IsBuild());
	cSolution.Kill();

	cSolution.Init();
	sLogConfig = gcLogger.SetSilent();
	AssertFalse(cSolution.ReadProject("Input" _FS_ "MiniLib" _FS_ "NotThere.vcxproj"));
	gcLogger.SetConfig(&sLogConfig);
	cSolution.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A solution that isn't there, or one that ends inside a Project.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxSolutionBad(void)
{
	CVcxSolution	cSolution;
	SLogConfig		sLogConfig;

	sLogConfig = gcLogger.SetSilent();
	cSolution.Init();
	AssertFalse(cSolution.Read("Input" _FS_ "MiniSolution" _FS_ "NotThere.sln"));
	cSolution.Kill();

	cSolution.Init();
	AssertFalse(cSolution.Read("Input" _FS_ "MiniSolution" _FS_ "Broken.sln"));
	cSolution.Kill();
	gcLogger.SetConfig(&sLogConfig);
}


//////////////////////////////////////////////////////////////////////////
//
// Paths from Visual Studio files have '\' separators and any case.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxPath(void)
{
	CFileUtil	cFileUtil;
	CChars		szDirectory;
	CChars		szPath;
	CChars		szExpected;

	szDirectory.Init("Input" _FS_ "MiniSolution");
	cFileUtil.FullPath(&szDirectory);

	szExpected.Init("Input" _FS_ "MiniLib" _FS_ "MiniLib.vcxproj.filters");
	cFileUtil.FullPath(&szExpected);
	szPath.Init();
	AssertTrue(ResolveVcxPath(szDirectory.Text(), "..\\minilib\\MINILIB.vcxproj.FILTERS", &szPath));
	AssertString(szExpected.Text(), szPath.Text());
	szPath.Kill();
	szExpected.Kill();

	szExpected.Init("Input" _FS_ "MiniSolution" _FS_ "ReadMe.txt");
	cFileUtil.FullPath(&szExpected);
	szPath.Init();
	AssertTrue(ResolveVcxPath(szDirectory.Text(), "README.TXT", &szPath));
	AssertString(szExpected.Text(), szPath.Text());
	szPath.Kill();
	szExpected.Kill();

	szExpected.Init("Input" _FS_ "MiniSolution" _FS_ "Not" _FS_ "There.txt");
	cFileUtil.FullPath(&szExpected);
	szPath.Init();
	AssertFalse(ResolveVcxPath(szDirectory.Text(), "Not\\There.txt", &szPath));
	AssertString(szExpected.Text(), szPath.Text());
	szPath.Kill();
	szExpected.Kill();

	szDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestVcxSolution(void)
{
	BeginTests();

	TestVcxPath();
	TestVcxSolutionMini();
	TestVcxSolutionWithoutProjects();
	TestVcxSolutionFromProject();
	TestVcxSolutionBad();

	TestStatistics();
}

