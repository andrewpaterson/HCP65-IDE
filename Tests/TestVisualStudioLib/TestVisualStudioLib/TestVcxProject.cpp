#include "BaseLib/FileUtil.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayChars.h"
#include "VisualStudioLib/VcxProject.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// A small project with every part read: configurations and their
// properties, items of each kind, filters and a project reference.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectMini(void)
{
	CVcxProject			cProject;
	CVcxConfiguration*	pcDebug;
	CVcxConfiguration*	pcRelease;
	CVcxItem*			pcItem;
	CVcxFilter*			pcFilter;
	CArrayInt			aiIndices;
	CChars				szParent;
	CChars				szPath;
	CChars				szExpected;
	CFileUtil			cFileUtil;

	cProject.Init();
	AssertTrue(cProject.Read("Input" _FS_ "MiniLib" _FS_ "MiniLib.vcxproj"));
	AssertString("MiniLib", cProject.GetName());
	AssertString("{0A1B2C3D-4E5F-4061-8293-A4B5C6D7E8F9}", cProject.GetGuid());
	AssertString("MiniLib", cProject.GetRootNamespace());
	AssertTrue(cProject.HasFilters());

	//The configurations and the properties set for each.
	AssertSize(2, cProject.NumConfigurations());
	pcDebug = cProject.GetConfiguration(0);
	AssertString("Debug|Win32", pcDebug->GetName());
	AssertString("Debug", pcDebug->GetConfiguration());
	AssertString("Win32", pcDebug->GetPlatform());
	pcRelease = cProject.FindConfiguration("Release|x64");
	AssertTrue(pcRelease == cProject.GetConfiguration(1));
	AssertString("Release", pcRelease->GetConfiguration());
	AssertString("x64", pcRelease->GetPlatform());
	AssertNull(cProject.FindConfiguration("Release|Win32"));

	AssertString("Application", pcDebug->GetProperty("ConfigurationType"));
	AssertString("StaticLibrary", pcRelease->GetProperty("ConfigurationType"));
	AssertString("$(SolutionDir)Out\\", pcDebug->GetProperty("OutDir"));
	AssertString("$(SolutionDir)Out\\", pcRelease->GetProperty("OutDir"));
	AssertString("..\\Include\\;$(IncludePath)", pcDebug->GetProperty("IncludePath"));
	AssertNull(pcRelease->GetProperty("IncludePath"));
	AssertString("_DEBUG;MINI;%(PreprocessorDefinitions)", pcDebug->GetProperty("ClCompile.PreprocessorDefinitions"));
	AssertString("NDEBUG;%(PreprocessorDefinitions)", pcRelease->GetProperty("ClCompile.PreprocessorDefinitions"));
	AssertString("Level3", pcDebug->GetProperty("ClCompile.WarningLevel"));
	AssertString("Console", pcDebug->GetProperty("Link.SubSystem"));
	AssertNull(pcRelease->GetProperty("Link.SubSystem"));

	//The globals are properties of every configuration too.
	AssertString("MiniLib", pcRelease->GetProperty("RootNamespace"));

	//The items in the order the project lists them, each with its filter.
	AssertSize(7, cProject.NumItems());
	pcItem = cProject.GetItem(0);
	AssertInt(VIT_ClCompile, pcItem->GetType());
	AssertString("ClCompile", pcItem->GetElement());
	AssertString("Main.cpp", pcItem->GetInclude());
	AssertString("Main.cpp", pcItem->GetFileName());
	AssertString("Source", pcItem->GetFilter());
	pcItem = cProject.GetItem(1);
	AssertString("Source\\Engine\\Engine.cpp", pcItem->GetInclude());
	AssertString("Engine.cpp", pcItem->GetFileName());
	AssertString("Source\\Engine", pcItem->GetFilter());
	pcItem = cProject.GetItem(2);
	AssertInt(VIT_ClInclude, pcItem->GetType());
	AssertString("Source\\Engine", pcItem->GetFilter());
	pcItem = cProject.GetItem(3);
	AssertString("Common.h", pcItem->GetInclude());
	AssertString("", pcItem->GetFilter());
	AssertInt(VIT_None, cProject.GetItem(4)->GetType());
	AssertString("Docs", cProject.GetItem(4)->GetFilter());
	AssertInt(VIT_Text, cProject.GetItem(5)->GetType());
	AssertString("Docs", cProject.GetItem(5)->GetFilter());
	AssertInt(VIT_Image, cProject.GetItem(6)->GetType());
	AssertString("", cProject.GetItem(6)->GetFilter());

	//Paths aren't case sensitive, but the element is.
	AssertTrue(cProject.FindItem("ClCompile", "MAIN.CPP") == cProject.GetItem(0));
	AssertNull(cProject.FindItem("ClInclude", "Main.cpp"));
	AssertNull(cProject.FindItem("ClCompile", "Missing.cpp"));

	//The filters.
	AssertSize(3, cProject.NumFilters());
	pcFilter = cProject.GetFilter(1);
	AssertString("Source\\Engine", pcFilter->GetName());
	AssertString("Engine", pcFilter->GetLeafName());
	AssertSize(1, pcFilter->GetDepth());
	AssertString("{66666666-7777-8888-9999-000000000000}", pcFilter->GetUniqueIdentifier());
	szParent.Init();
	AssertTrue(pcFilter->GetParentName(&szParent));
	AssertString("Source", szParent.Text());
	szParent.Clear();
	AssertFalse(cProject.GetFilter(0)->GetParentName(&szParent));
	AssertString("", szParent.Text());
	szParent.Kill();
	AssertSize(0, cProject.GetFilter(0)->GetDepth());
	AssertString("txt;md", cProject.FindFilter("docs")->GetExtensions());
	AssertNull(cProject.FindFilter("Headers"));

	//The tree Solution Explorer shows: the filters in each filter and the items in each.
	aiIndices.Init();
	cProject.GetChildFilters("", &aiIndices);
	AssertSize(2, aiIndices.NumElements());
	AssertInt(0, aiIndices.GetValue(0));
	AssertInt(2, aiIndices.GetValue(1));
	aiIndices.ReInit();
	cProject.GetChildFilters("source", &aiIndices);
	AssertSize(1, aiIndices.NumElements());
	AssertInt(1, aiIndices.GetValue(0));
	aiIndices.ReInit();
	cProject.GetChildFilters("Source\\Engine", &aiIndices);
	AssertSize(0, aiIndices.NumElements());
	aiIndices.ReInit();
	cProject.GetItemsInFilter("Source\\Engine", &aiIndices);
	AssertSize(2, aiIndices.NumElements());
	AssertInt(1, aiIndices.GetValue(0));
	AssertInt(2, aiIndices.GetValue(1));
	aiIndices.ReInit();
	cProject.GetItemsInFilter("", &aiIndices);
	AssertSize(2, aiIndices.NumElements());
	AssertInt(3, aiIndices.GetValue(0));
	AssertInt(6, aiIndices.GetValue(1));
	aiIndices.Kill();

	//The referenced projects.
	AssertSize(1, cProject.NumReferences());
	AssertString("..\\BaseLib\\BaseLib.vcxproj", cProject.GetReference(0)->GetInclude());
	AssertString("{e25d2dac-0e80-42b2-8fa7-99b837cf7588}", cProject.GetReference(0)->GetProject());

	//An item's file is found from the project's directory.
	szExpected.Init("Input" _FS_ "MiniLib" _FS_ "Source" _FS_ "Engine" _FS_ "Engine.cpp");
	cFileUtil.FullPath(&szExpected);
	szPath.Init();
	cProject.GetFullPath(cProject.GetItem(1), &szPath);
	AssertString(szExpected.Text(), szPath.Text());
	szPath.Kill();
	szExpected.Kill();

	cProject.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A real project: SupportLib's.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectSupportLib(void)
{
	CVcxProject		cProject;
	CVcxItem*		pcItem;
	CArrayInt		aiIndices;
	size			i;
	size			uiClCompile;
	size			uiClInclude;
	size			uiNoFilter;

	cProject.Init();
	AssertTrue(cProject.Read("Input" _FS_ "SupportLib" _FS_ "SupportLib.vcxproj"));
	AssertString("SupportLib", cProject.GetName());
	AssertString("{221022F1-B103-456A-8F55-721F33F2E26D}", cProject.GetGuid());
	AssertSize(4, cProject.NumConfigurations());
	AssertString("StaticLibrary", cProject.FindConfiguration("Release|x64")->GetProperty("ConfigurationType"));
	AssertSize(0, cProject.NumReferences());

	uiClCompile = 0;
	uiClInclude = 0;
	uiNoFilter = 0;
	for (i = 0; i < cProject.NumItems(); i++)
	{
		pcItem = cProject.GetItem(i);
		if (pcItem->GetType() == VIT_ClCompile)
		{
			uiClCompile++;
		}
		else if (pcItem->GetType() == VIT_ClInclude)
		{
			uiClInclude++;
		}
		if (pcItem->GetFilter()[0] == '\0')
		{
			uiNoFilter++;
		}
	}
	AssertSize(246, uiClCompile);
	AssertSize(261, uiClInclude);
	AssertSize(507, cProject.NumItems());
	AssertSize(0, uiNoFilter);
	AssertSize(85, cProject.NumFilters());

	aiIndices.Init();
	cProject.GetItemsInFilter("Source\\Map\\Tile\\Specific", &aiIndices);
	AssertSize(6, aiIndices.NumElements());
	AssertTrue(cProject.FindItem("ClCompile", "TileLayerCel.cpp") != NULL);
	AssertString("Source\\Map\\Tile\\Specific", cProject.FindItem("ClCompile", "TileLayerCel.cpp")->GetFilter());
	aiIndices.Kill();

	cProject.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// MSBuild macros, and the include directories and defines the compiler
// is given.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectMacros(void)
{
	CVcxProject			cProject;
	CVcxConfiguration*	pcDebug;
	CVcxConfiguration*	pcRelease;
	CChars				szExpanded;
	CChars				szExpected;
	CChars				szSolution;
	CArrayChars			aszValues;
	CFileUtil			cFileUtil;

	cProject.Init();
	AssertTrue(cProject.Read("Input" _FS_ "MiniLib" _FS_ "MiniLib.vcxproj"));
	pcDebug = cProject.FindConfiguration("Debug|Win32");
	pcRelease = cProject.FindConfiguration("Release|x64");
	szSolution.Init("Input");
	cFileUtil.FullPath(&szSolution);

	szExpanded.Init();
	cProject.ExpandMacros("$(ProjectName)-$(Configuration)-$(Platform)-$(RootNamespace)-$(Unknown)-%(PreprocessorDefinitions).", pcRelease, szSolution.Text(), &szExpanded);
	AssertString("MiniLib-Release-x64-MiniLib--.", szExpanded.Text());
	szExpanded.Clear();
	cProject.ExpandMacros("$(OutDir)", pcDebug, szSolution.Text(), &szExpanded);
	szExpected.Init(szSolution);
	szExpected.Append(FILE_SEPARATOR[0]);
	szExpected.Append("Out\\");
	AssertString(szExpected.Text(), szExpanded.Text());
	szExpected.Kill();
	szExpanded.Clear();
	cProject.ExpandMacros("$(ProjectDir)", pcDebug, szSolution.Text(), &szExpanded);
	szExpected.Init(cProject.GetDirectory());
	szExpected.Append(FILE_SEPARATOR[0]);
	AssertString(szExpected.Text(), szExpanded.Text());
	szExpected.Kill();
	szExpanded.Kill();

	//..\Include\ from the project's directory; $(IncludePath) is the compiler's own.
	aszValues.Init();
	cProject.GetIncludeDirectories(pcDebug, szSolution.Text(), &aszValues);
	AssertSize(1, aszValues.NumElements());
	szExpected.Init("Input" _FS_ "Include");
	cFileUtil.FullPath(&szExpected);
	AssertString(szExpected.Text(), aszValues.GetText(0));
	szExpected.Kill();
	aszValues.ReInit();
	cProject.GetIncludeDirectories(pcRelease, szSolution.Text(), &aszValues);
	AssertSize(0, aszValues.NumElements());

	aszValues.ReInit();
	cProject.GetPreprocessorDefinitions(pcDebug, szSolution.Text(), &aszValues);
	AssertSize(2, aszValues.NumElements());
	AssertString("_DEBUG", aszValues.GetText(0));
	AssertString("MINI", aszValues.GetText(1));
	aszValues.Kill();

	szSolution.Kill();
	cProject.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A project that isn't there, or a file that isn't a project.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectBad(void)
{
	CVcxProject	cProject;

	cProject.Init();
	AssertFalse(cProject.Read("Input" _FS_ "MiniLib" _FS_ "NotThere.vcxproj"));
	cProject.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProject(void)
{
	BeginTests();

	TestVcxProjectMini();
	TestVcxProjectSupportLib();
	TestVcxProjectMacros();
	TestVcxProjectBad();

	TestStatistics();
}

