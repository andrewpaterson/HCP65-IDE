#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Logger.h"
#include "BaseLib/TextFile.h"
#include "VisualStudioLib/VcxProject.h"
#include "TestLib/Assert.h"
#include "TestLib/AssertFile.h"


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
	SLogConfig	sLogConfig;

	cProject.Init();
	sLogConfig = gcLogger.SetSilent();
	AssertFalse(cProject.Read("Input" _FS_ "MiniLib" _FS_ "NotThere.vcxproj"));
	gcLogger.SetConfig(&sLogConfig);
	cProject.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The path of szName in the project's directory.
//
//////////////////////////////////////////////////////////////////////////
static void ProjectPath(CVcxProject* pcProject, const char* szName, CChars* pszDest)
{
	pszDest->Init(pcProject->GetDirectory());
	pszDest->Append(FILE_SEPARATOR);
	pszDest->Append(szName);
}


//////////////////////////////////////////////////////////////////////////
//
// Items renamed in a copy of Input/RenameItem/Before: only the Include
// values naming them change, in the .vcxproj and its .filters, the rest
// of each file byte for byte as it was (Input/RenameItem/Expected).  An
// item in a subdirectory keeps its directories and backslashes,
// EngineCache.cpp (whose name starts with Engine) keeps its name, and
// another project naming the file renames its own item.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectRenameItem(void)
{
	CVcxProject	cProject;
	CVcxProject	cOther;
	CVcxItem*	pcItem;
	CChars		szPath;
	CChars		szHeader;
	CFileUtil	cFileUtil;
	SLogConfig	sLogConfig;

	cFileUtil.RemoveDir("Output" _FS_ "RenameItem");
	AssertTrue(cFileUtil.TouchDir("Output" _FS_ "RenameItem"));
	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "RenameItem" _FS_ "Before", "Output" _FS_ "RenameItem"));

	cProject.Init();
	AssertTrue(cProject.Read("Output" _FS_ "RenameItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj"));
	AssertSize(5, cProject.NumItems());

	//The header, then the source named in another case, then the one in a subdirectory.
	ProjectPath(&cProject, "Engine.h", &szHeader);
	AssertTrue(cProject.RenameItem(szHeader.Text(), "CPPEngine.h"));
	ProjectPath(&cProject, "engine.CPP", &szPath);
	AssertTrue(cProject.RenameItem(szPath.Text(), "CPPEngine.cpp"));
	szPath.Kill();
	ProjectPath(&cProject, "Source" _FS_ "Engine" _FS_ "Engine_Linux.cpp", &szPath);
	AssertTrue(cProject.RenameItem(szPath.Text(), "CPPEngine_Linux.cpp"));
	szPath.Kill();
	AssertFile("Input" _FS_ "RenameItem" _FS_ "Expected" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj", "Output" _FS_ "RenameItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj");
	AssertFile("Input" _FS_ "RenameItem" _FS_ "Expected" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj.filters", "Output" _FS_ "RenameItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj.filters");

	//The project was read again: the items have their new names and keep their filters.
	AssertSize(5, cProject.NumItems());
	AssertNull(cProject.FindItem("ClInclude", "Engine.h"));
	pcItem = cProject.FindItem("ClInclude", "CPPEngine.h");
	AssertNotNull(pcItem);
	AssertString("Header Files", pcItem->GetFilter());
	pcItem = cProject.FindItem("ClCompile", "Source\\Engine\\CPPEngine_Linux.cpp");
	AssertNotNull(pcItem);
	AssertString("Source Files", pcItem->GetFilter());
	AssertNotNull(cProject.FindItem("ClCompile", "EngineCache.cpp"));

	//Another project's item of the same file.
	cOther.Init();
	AssertTrue(cOther.Read("Output" _FS_ "RenameItem" _FS_ "Other" _FS_ "Other.vcxproj"));
	AssertTrue(cOther.RenameItem(szHeader.Text(), "CPPEngine.h"));
	AssertFile("Input" _FS_ "RenameItem" _FS_ "Expected" _FS_ "Other" _FS_ "Other.vcxproj", "Output" _FS_ "RenameItem" _FS_ "Other" _FS_ "Other.vcxproj");
	AssertNotNull(cOther.FindItem("ClInclude", "..\\RenameLib\\CPPEngine.h"));
	cOther.Kill();

	//Not an item, or not a file name: nothing changes.
	sLogConfig = gcLogger.SetSilent();
	AssertFalse(cProject.RenameItem(szHeader.Text(), "Engine.h"));
	gcLogger.SetConfig(&sLogConfig);
	szHeader.Kill();
	ProjectPath(&cProject, "CPPEngine.h", &szHeader);
	AssertFalse(cProject.RenameItem(szHeader.Text(), ""));
	AssertFalse(cProject.RenameItem(szHeader.Text(), "Sub\\Engine.h"));
	AssertFalse(cProject.RenameItem(szHeader.Text(), "Sub/Engine.h"));
	AssertFile("Input" _FS_ "RenameItem" _FS_ "Expected" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj", "Output" _FS_ "RenameItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj");
	AssertNotNull(cProject.FindItem("ClInclude", "CPPEngine.h"));
	szHeader.Kill();

	cProject.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "RenameItem");
}


//////////////////////////////////////////////////////////////////////////
//
// Items added beside others in a copy of Input/RenameItem/Before: each is
// the kind of item the one it is beside is, written after it with its
// directories, and in its filter, but without what the other is built
// with (Engine_Linux.cpp is excluded from a build: Piston.cpp isn't).
// Nothing is added beside a file that
// isn't an item, or for a file that is one already.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectAddItemBeside(void)
{
	CVcxProject	cProject;
	CVcxItem*	pcItem;
	CChars		szPath;
	CTextFile	cFile;
	CFileUtil	cFileUtil;

	cFileUtil.RemoveDir("Output" _FS_ "AddItem");
	AssertTrue(cFileUtil.TouchDir("Output" _FS_ "AddItem"));
	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "RenameItem" _FS_ "Before", "Output" _FS_ "AddItem"));

	cProject.Init();
	AssertTrue(cProject.Read("Output" _FS_ "AddItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj"));
	AssertSize(5, cProject.NumItems());

	ProjectPath(&cProject, "Engine.h", &szPath);
	AssertTrue(cProject.AddItemBeside(szPath.Text(), "Piston.h"));
	AssertFalse(cProject.AddItemBeside(szPath.Text(), "Piston.h"));
	AssertFalse(cProject.AddItemBeside(szPath.Text(), "Sub\\Piston.h"));
	szPath.Kill();
	ProjectPath(&cProject, "Source" _FS_ "Engine" _FS_ "Engine_Linux.cpp", &szPath);
	AssertTrue(cProject.AddItemBeside(szPath.Text(), "Piston.cpp"));
	szPath.Kill();
	ProjectPath(&cProject, "NotThere.cpp", &szPath);
	AssertFalse(cProject.AddItemBeside(szPath.Text(), "Other.cpp"));
	szPath.Kill();

	AssertSize(7, cProject.NumItems());
	AssertNotNull(cProject.FindItem("ClInclude", "Engine.h"));
	pcItem = cProject.FindItem("ClInclude", "Piston.h");
	AssertNotNull(pcItem);
	AssertString("Header Files", pcItem->GetFilter());
	pcItem = cProject.FindItem("ClCompile", "Source\\Engine\\Piston.cpp");
	AssertNotNull(pcItem);
	AssertString("Source Files", pcItem->GetFilter());

	//Each is on the line after the one it is beside.
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "AddItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj"));
	cFile.mcText.Replace("\r", "");
	AssertTrue(cFile.mcText.Contains("    <ClInclude Include=\"Engine.h\" />\n    <ClInclude Include=\"Piston.h\" />\n"));
	AssertTrue(cFile.mcText.Contains("    </ClCompile>\n    <ClCompile Include=\"Source\\Engine\\Piston.cpp\" />\n  </ItemGroup>\n"));
	cFile.Kill();

	//Taken out again, the project and its filters are as they were, byte for byte; an item with what it is built
	//with in it goes whole.  What isn't an item isn't taken out.
	ProjectPath(&cProject, "Piston.h", &szPath);
	AssertTrue(cProject.RemoveItem(szPath.Text()));
	AssertFalse(cProject.RemoveItem(szPath.Text()));
	szPath.Kill();
	ProjectPath(&cProject, "Source" _FS_ "Engine" _FS_ "Piston.cpp", &szPath);
	AssertTrue(cProject.RemoveItem(szPath.Text()));
	szPath.Kill();
	AssertSize(5, cProject.NumItems());
	AssertNull(cProject.FindItem("ClInclude", "Piston.h"));
	AssertFile("Input" _FS_ "RenameItem" _FS_ "Before" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj", "Output" _FS_ "AddItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj");
	AssertFile("Input" _FS_ "RenameItem" _FS_ "Before" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj.filters", "Output" _FS_ "AddItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj.filters");
	ProjectPath(&cProject, "Source" _FS_ "Engine" _FS_ "Engine_Linux.cpp", &szPath);
	AssertTrue(cProject.RemoveItem(szPath.Text()));
	szPath.Kill();
	AssertSize(4, cProject.NumItems());
	AssertNull(cProject.FindItem("ClCompile", "Source\\Engine\\Engine_Linux.cpp"));
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "AddItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj"));
	AssertFalse(cFile.mcText.Contains("Engine_Linux"));
	cFile.Kill();

	cProject.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "AddItem");
}


//////////////////////////////////////////////////////////////////////////
//
// Items added with nothing to go beside (New's), in a copy of
// Input/RenameItem/Before: each the kind its extension says, after the
// last of its kind, in the filter given or none; a kind the project has
// none of yet in an ItemGroup of its own.  Nothing is added for a file
// that is an item already, or isn't under the project's directory.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectAddItem(void)
{
	CVcxProject	cProject;
	CVcxItem*	pcItem;
	CChars		szPath;
	CTextFile	cFile;
	CFileUtil	cFileUtil;

	cFileUtil.RemoveDir("Output" _FS_ "AddNewItem");
	AssertTrue(cFileUtil.TouchDir("Output" _FS_ "AddNewItem"));
	AssertTrue(cFileUtil.CopyDir("Input" _FS_ "RenameItem" _FS_ "Before", "Output" _FS_ "AddNewItem"));

	cProject.Init();
	AssertTrue(cProject.Read("Output" _FS_ "AddNewItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj"));
	AssertSize(5, cProject.NumItems());

	ProjectPath(&cProject, "Piston.h", &szPath);
	AssertTrue(cProject.AddItem(szPath.Text(), "Header Files"));
	AssertFalse(cProject.AddItem(szPath.Text(), "Header Files"));
	szPath.Kill();
	ProjectPath(&cProject, "Source" _FS_ "Valve.cpp", &szPath);
	AssertTrue(cProject.AddItem(szPath.Text(), NULL));
	szPath.Kill();
	ProjectPath(&cProject, "Notes.txt", &szPath);
	AssertTrue(cProject.AddItem(szPath.Text(), ""));
	szPath.Kill();
	ProjectPath(&cProject, "Engine.h", &szPath);
	AssertFalse(cProject.AddItem(szPath.Text(), NULL));
	szPath.Kill();
	AssertFalse(cProject.AddItem("Elsewhere" _FS_ "Other.h", NULL));

	AssertSize(8, cProject.NumItems());
	pcItem = cProject.FindItem("ClInclude", "Piston.h");
	AssertNotNull(pcItem);
	AssertString("Header Files", pcItem->GetFilter());
	pcItem = cProject.FindItem("ClCompile", "Source\\Valve.cpp");
	AssertNotNull(pcItem);
	AssertString("", pcItem->GetFilter());
	AssertNotNull(cProject.FindItem("None", "Notes.txt"));

	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "AddNewItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj"));
	cFile.mcText.Replace("\r", "");
	AssertTrue(cFile.mcText.Contains("    <ClInclude Include=\"Piston.h\" />\n"));
	AssertTrue(cFile.mcText.Contains("    <None Include=\"Notes.txt\" />\n  </ItemGroup>\n"));
	cFile.Kill();
	cFile.Init();
	AssertTrue(cFile.Read("Output" _FS_ "AddNewItem" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj.filters"));
	cFile.mcText.Replace("\r", "");
	AssertTrue(cFile.mcText.Contains("    <ClInclude Include=\"Piston.h\">\n      <Filter>Header Files</Filter>\n    </ClInclude>\n"));
	AssertTrue(cFile.mcText.Contains("    <ClCompile Include=\"Source\\Valve.cpp\" />\n"));
	cFile.Kill();

	cProject.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "AddNewItem");
}


//////////////////////////////////////////////////////////////////////////
//
// What each configuration compiles: a ClCompile item, unless its own
// ExcludedFromBuild leaves it out (for the configuration its condition
// names, or for all with none) or, the item saying nothing, the
// configuration's item definition leaves every source out.  An item's own
// false keeps it in such a configuration.  RenameLib's Engine_Linux.cpp is
// excluded from its one configuration.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectExcludedFromBuild(void)
{
	CVcxProject			cProject;
	CVcxConfiguration*	pcDebug;
	CVcxConfiguration*	pcRelease;
	CTextFile			cFile;
	CFileUtil			cFileUtil;

	cFileUtil.RemoveDir("Output" _FS_ "Excluded");
	AssertTrue(cFileUtil.TouchDir("Output" _FS_ "Excluded"));
	cFile.Init("<Project>\n"
		"  <ItemGroup Label=\"ProjectConfigurations\">\n"
		"    <ProjectConfiguration Include=\"Debug|x64\"/>\n"
		"    <ProjectConfiguration Include=\"Release|x64\"/>\n"
		"  </ItemGroup>\n"
		"  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
		"    <ClCompile>\n"
		"      <ExcludedFromBuild>true</ExcludedFromBuild>\n"
		"    </ClCompile>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemGroup>\n"
		"    <ClCompile Include=\"Plain.cpp\"/>\n"
		"    <ClCompile Include=\"NotInDebug.cpp\">\n"
		"      <ExcludedFromBuild Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">true</ExcludedFromBuild>\n"
		"    </ClCompile>\n"
		"    <ClCompile Include=\"Never.cpp\">\n"
		"      <ExcludedFromBuild> True </ExcludedFromBuild>\n"
		"    </ClCompile>\n"
		"    <ClCompile Include=\"InRelease.cpp\">\n"
		"      <ExcludedFromBuild Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">false</ExcludedFromBuild>\n"
		"    </ClCompile>\n"
		"    <ClCompile Include=\"Other.cpp\">\n"
		"      <ExcludedFromBuild Condition=\"'$(Platform)'=='ARM'\">true</ExcludedFromBuild>\n"
		"    </ClCompile>\n"
		"    <ClInclude Include=\"Plain.h\"/>\n"
		"  </ItemGroup>\n"
		"</Project>\n");
	AssertTrue(cFile.Write("Output" _FS_ "Excluded" _FS_ "Excluded.vcxproj"));
	cFile.Kill();

	cProject.Init();
	AssertTrue(cProject.Read("Output" _FS_ "Excluded" _FS_ "Excluded.vcxproj"));
	pcDebug = cProject.FindConfiguration("Debug|x64");
	pcRelease = cProject.FindConfiguration("Release|x64");
	AssertNotNull(pcDebug);
	AssertNotNull(pcRelease);

	AssertTrue(cProject.IsCompiled(cProject.FindItem("ClCompile", "Plain.cpp"), pcDebug));
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "NotInDebug.cpp"), pcDebug));
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "Never.cpp"), pcDebug));
	AssertTrue(cProject.IsCompiled(cProject.FindItem("ClCompile", "InRelease.cpp"), pcDebug));
	AssertTrue(cProject.IsCompiled(cProject.FindItem("ClCompile", "Other.cpp"), pcDebug));
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClInclude", "Plain.h"), pcDebug));

	//Release's item definition leaves every source out, but the one that says it is built.
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "Plain.cpp"), pcRelease));
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "NotInDebug.cpp"), pcRelease));
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "Never.cpp"), pcRelease));
	AssertTrue(cProject.IsCompiled(cProject.FindItem("ClCompile", "InRelease.cpp"), pcRelease));

	//No configuration: every source.
	AssertTrue(cProject.IsCompiled(cProject.FindItem("ClCompile", "Never.cpp"), NULL));
	cProject.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "Excluded");

	cProject.Init();
	AssertTrue(cProject.Read("Input" _FS_ "RenameItem" _FS_ "Before" _FS_ "RenameLib" _FS_ "RenameLib.vcxproj"));
	pcDebug = cProject.FindConfiguration("Debug|Win32");
	AssertTrue(cProject.IsCompiled(cProject.FindItem("ClCompile", "Engine.cpp"), pcDebug));
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "Source\\Engine\\Engine_Linux.cpp"), pcDebug));
	cProject.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool TestVcxProjectEndsWith(const char* szText, const char* szEnd)
{
	size	uiText;
	size	uiEnd;

	uiText = strlen(szText);
	uiEnd = strlen(szEnd);
	return (uiText >= uiEnd) && (strcmp(szText + uiText - uiEnd, szEnd) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Writes szText as Output\Sheets\<szName>.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectWriteSheet(const char* szName, const char* szText)
{
	CTextFile	cFile;
	CFileUtil	cFileUtil;
	CChars		szFileName;

	szFileName.Init("Output" _FS_ "Sheets");
	cFileUtil.AppendToPath(&szFileName, szName);
	cFileUtil.TouchDir(szFileName.Text(), true);
	cFile.Init(szText);
	AssertTrue(cFile.Write(szFileName.Text()));
	cFile.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A project with groups for toolchains' configurations it doesn't list,
// and property sheets: one imported for every configuration, which
// imports another from its own folder, and one imported for Release|x64
// only.  The toolchains' configurations are found (a whole name, or every
// listed configuration name for a condition on the platform alone); the
// sheets' groups are read where they are imported, for the configurations
// their conditions and their import's are for; a value naming itself adds
// to what was set before; and a toolchain's configuration takes its
// ConfigurationType from the configuration it is built like.
//
//////////////////////////////////////////////////////////////////////////
void TestVcxProjectSheets(void)
{
	CVcxProject			cProject;
	CVcxConfiguration*	pcConfiguration;
	CArrayChars			aszValues;
	CFileUtil			cFileUtil;
	CChars				szCommon;

	cFileUtil.RemoveDir("Output" _FS_ "Sheets");
	TestVcxProjectWriteSheet("Common" _FS_ "Common.props",
		"<Project>\n"
		"  <PropertyGroup>\n"
		"    <CommonDir>$(MSBuildThisFileDirectory)</CommonDir>\n"
		"    <IncludePath>$(CommonDir)Include;$(IncludePath)</IncludePath>\n"
		"  </PropertyGroup>\n"
		"  <ItemDefinitionGroup>\n"
		"    <ClCompile><PreprocessorDefinitions>ALL;%(PreprocessorDefinitions)</PreprocessorDefinitions></ClCompile>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemDefinitionGroup Condition=\"'$(Configuration)'=='Debug'\">\n"
		"    <ClCompile><PreprocessorDefinitions>DBG;%(PreprocessorDefinitions)</PreprocessorDefinitions></ClCompile>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemDefinitionGroup Condition=\"'$(Platform)'=='GCC-x64'\">\n"
		"    <ClCompile>\n"
		"      <PreprocessorDefinitions>LINUX_GNU;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
		"      <AdditionalOptions>-m64  -DNAME=\"a b\" %(AdditionalOptions)</AdditionalOptions>\n"
		"    </ClCompile>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemDefinitionGroup Condition=\"exists('Nothing')\">\n"
		"    <ClCompile><PreprocessorDefinitions>NEVER</PreprocessorDefinitions></ClCompile>\n"
		"  </ItemDefinitionGroup>\n"
		"</Project>\n");
	TestVcxProjectWriteSheet("Repository.props",
		"<Project>\n"
		"  <Import Project=\"$(MSBuildThisFileDirectory)Common\\common.props\"/>\n"
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\"/>\n"
		"  <Import Project=\"Missing.props\"/>\n"
		"</Project>\n");
	TestVcxProjectWriteSheet("ReleaseOnly.props",
		"<Project>\n"
		"  <ItemDefinitionGroup>\n"
		"    <ClCompile><PreprocessorDefinitions>RELEASE_ONLY;%(PreprocessorDefinitions)</PreprocessorDefinitions></ClCompile>\n"
		"  </ItemDefinitionGroup>\n"
		"</Project>\n");
	TestVcxProjectWriteSheet("Project" _FS_ "Sheets.vcxproj",
		"<Project>\n"
		"  <ItemGroup Label=\"ProjectConfigurations\">\n"
		"    <ProjectConfiguration Include=\"Debug|Win32\"/>\n"
		"    <ProjectConfiguration Include=\"Debug|x64\"/>\n"
		"    <ProjectConfiguration Include=\"Release|x64\"/>\n"
		"  </ItemGroup>\n"
		"  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\" Label=\"Configuration\">\n"
		"    <ConfigurationType>StaticLibrary</ConfigurationType>\n"
		"  </PropertyGroup>\n"
		"  <ImportGroup Label=\"PropertySheets\">\n"
		"    <Import Project=\"..\\Repository.props\"/>\n"
		"  </ImportGroup>\n"
		"  <ImportGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\" Label=\"PropertySheets\">\n"
		"    <Import Project=\"$(ProjectDir)..\\ReleaseOnly.props\"/>\n"
		"    <Import Project=\"$(SolutionDir)Unknown.props\"/>\n"
		"  </ImportGroup>\n"
		"  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
		"    <IncludePath>$(ProjectDir)Extra;$(IncludePath)</IncludePath>\n"
		"  </PropertyGroup>\n"
		"  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|GCC-x64'\">\n"
		"    <ClCompile>\n"
		"      <PreprocessorDefinitions>MINE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
		"      <AdditionalOptions>-O3 %(AdditionalOptions)</AdditionalOptions>\n"
		"    </ClCompile>\n"
		"    <Link><AdditionalDependencies>X11;Xext;%(AdditionalDependencies)</AdditionalDependencies></Link>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|HCP65C816'\">\n"
		"    <HCP65Compile><NoFloats>true</NoFloats></HCP65Compile>\n"
		"  </ItemDefinitionGroup>\n"
		"  <ItemGroup>\n"
		"    <ClCompile Include=\"Windows.cpp\">\n"
		"      <ExcludedFromBuild Condition=\"'$(Platform)'=='GCC-x64'\">true</ExcludedFromBuild>\n"
		"    </ClCompile>\n"
		"  </ItemGroup>\n"
		"</Project>\n");

	cProject.Init();
	AssertTrue(cProject.Read("Output" _FS_ "Sheets" _FS_ "Project" _FS_ "Sheets.vcxproj"));

	//The three it lists, then the toolchains': GCC-x64's for Debug and Release, and the HCP65's one.
	AssertSize(6, cProject.NumConfigurations());
	AssertString("Debug|Win32", cProject.GetConfiguration(0)->GetName());
	AssertString("Debug|x64", cProject.GetConfiguration(1)->GetName());
	AssertString("Release|x64", cProject.GetConfiguration(2)->GetName());
	AssertFalse(cProject.GetConfiguration(2)->IsToolchain());
	AssertNotNull(cProject.FindConfiguration("Release|GCC-x64"));
	AssertNotNull(cProject.FindConfiguration("Debug|GCC-x64"));
	AssertNotNull(cProject.FindConfiguration("Release|HCP65C816"));
	AssertNull(cProject.FindConfiguration("Debug|HCP65C816"));
	AssertTrue(cProject.FindConfiguration("Release|GCC-x64")->IsToolchain());

	//The sheets, each once: the one every configuration imports, the one it imports, and Release|x64's.
	AssertSize(3, cProject.NumSheets());
	AssertTrue(TestVcxProjectEndsWith(cProject.GetSheet(0), "Repository.props"));
	AssertTrue(TestVcxProjectEndsWith(cProject.GetSheet(1), "Common.props"));
	AssertTrue(TestVcxProjectEndsWith(cProject.GetSheet(2), "ReleaseOnly.props"));

	aszValues.Init();
	cProject.GetPreprocessorDefinitions(cProject.FindConfiguration("Debug|Win32"), ".", &aszValues);
	AssertSize(2, aszValues.NumElements());
	AssertString("DBG", aszValues.GetText(0));
	AssertString("ALL", aszValues.GetText(1));

	aszValues.ReInit();
	cProject.GetPreprocessorDefinitions(cProject.FindConfiguration("Release|x64"), ".", &aszValues);
	AssertSize(2, aszValues.NumElements());
	AssertString("RELEASE_ONLY", aszValues.GetText(0));
	AssertString("ALL", aszValues.GetText(1));

	aszValues.ReInit();
	cProject.GetPreprocessorDefinitions(cProject.FindConfiguration("Debug|GCC-x64"), ".", &aszValues);
	AssertSize(3, aszValues.NumElements());
	AssertString("LINUX_GNU", aszValues.GetText(0));
	AssertString("DBG", aszValues.GetText(1));
	AssertString("ALL", aszValues.GetText(2));

	//The project's own group adds to the sheets'.
	pcConfiguration = cProject.FindConfiguration("Release|GCC-x64");
	aszValues.ReInit();
	cProject.GetPreprocessorDefinitions(pcConfiguration, ".", &aszValues);
	AssertSize(3, aszValues.NumElements());
	AssertString("MINE", aszValues.GetText(0));
	AssertString("LINUX_GNU", aszValues.GetText(1));
	AssertString("ALL", aszValues.GetText(2));
	aszValues.ReInit();
	cProject.GetAdditionalOptions("ClCompile", pcConfiguration, ".", &aszValues);
	AssertSize(3, aszValues.NumElements());
	AssertString("-O3", aszValues.GetText(0));
	AssertString("-m64", aszValues.GetText(1));
	AssertString("-DNAME=\"a b\"", aszValues.GetText(2));
	aszValues.ReInit();
	cProject.GetAdditionalOptions("Link", pcConfiguration, ".", &aszValues);
	AssertSize(0, aszValues.NumElements());
	cProject.GetAdditionalDependencies(pcConfiguration, ".", &aszValues);
	AssertSize(2, aszValues.NumElements());
	AssertString("X11", aszValues.GetText(0));
	AssertString("Xext", aszValues.GetText(1));

	//What Release|x64 says once, Release|GCC-x64 has too: its type, and its include directories, the project's
	//own before the sheet's, which is named from the sheet's folder.
	AssertString("StaticLibrary", pcConfiguration->GetProperty("ConfigurationType"));
	AssertNull(pcConfiguration->GetOwnProperty("ConfigurationType"));
	AssertNull(cProject.FindConfiguration("Debug|GCC-x64")->GetProperty("ConfigurationType"));
	szCommon.Init("Output" _FS_ "Sheets" _FS_ "Common" _FS_ "Include");
	cFileUtil.FullPath(&szCommon);
	aszValues.ReInit();
	cProject.GetIncludeDirectories(pcConfiguration, ".", &aszValues);
	AssertSize(2, aszValues.NumElements());
	AssertTrue(TestVcxProjectEndsWith(aszValues.GetText(0), "Extra"));
	AssertString(szCommon.Text(), aszValues.GetText(1));
	aszValues.ReInit();
	cProject.GetIncludeDirectories(cProject.FindConfiguration("Debug|GCC-x64"), ".", &aszValues);
	AssertSize(1, aszValues.NumElements());
	AssertString(szCommon.Text(), aszValues.GetText(0));
	aszValues.ReInit();
	cProject.GetIncludeDirectories(cProject.FindConfiguration("Release|x64"), ".", &aszValues);
	AssertSize(2, aszValues.NumElements());
	AssertTrue(TestVcxProjectEndsWith(aszValues.GetText(0), "Extra"));
	AssertString(szCommon.Text(), aszValues.GetText(1));
	szCommon.Kill();
	aszValues.Kill();

	//The HCP65's: its own tags, and its type from Release's first listed configuration (there is no Release|Win32).
	pcConfiguration = cProject.FindConfiguration("Release|HCP65C816");
	AssertString("true", pcConfiguration->GetProperty("HCP65Compile.NoFloats"));
	AssertString("StaticLibrary", pcConfiguration->GetProperty("ConfigurationType"));

	//A file excluded from a platform is excluded from each of its configurations.
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "Windows.cpp"), cProject.FindConfiguration("Debug|GCC-x64")));
	AssertFalse(cProject.IsCompiled(cProject.FindItem("ClCompile", "Windows.cpp"), cProject.FindConfiguration("Release|GCC-x64")));
	AssertTrue(cProject.IsCompiled(cProject.FindItem("ClCompile", "Windows.cpp"), cProject.FindConfiguration("Release|x64")));

	cProject.Kill();
	cFileUtil.RemoveDir("Output" _FS_ "Sheets");
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
	TestVcxProjectRenameItem();
	TestVcxProjectAddItemBeside();
	TestVcxProjectAddItem();
	TestVcxProjectExcludedFromBuild();
	TestVcxProjectSheets();

	TestStatistics();
}

