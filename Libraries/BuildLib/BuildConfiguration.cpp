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
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "Assembler65Lib/Asm65Attributes.h"
#include "BuildConfiguration.h"


#define BUILD_CONFIGURATION_TAG			"Configuration"
#define BUILD_CONFIGURATION_NAME		"Name"
#define BUILD_CONFIGURATION_TOOLCHAIN	"Toolchain"
#define BUILD_CONFIGURATION_SOLUTION	"Solution"
#define BUILD_CONFIGURATION_IMAGE		"Image"
#define BUILD_CONFIGURATION_EXECUTABLE	"Executable"
#define BUILD_CONFIGURATION_MODULE		"Module"
#define BUILD_CONFIGURATION_LOAD		"Load"
#define BUILD_CONFIGURATION_LOAD_FILE	"File"
#define BUILD_CONFIGURATION_LOAD_ADDRESS	"Address"
#define BUILD_CONFIGURATION_CARD		"Card"
#define BUILD_CONFIGURATION_CARD_PATH	"CardPath"
#define BUILD_CONFIGURATION_BREAKPOINT_SPACE	"BreakpointSpace"
#define BUILD_CONFIGURATION_NO_FLOATS	"NoFloats"
#define BUILD_CONFIGURATION_MATH_TABLES	"MathTables"


//The toolchains' names, as saved, by EBuildToolchain.
static const char* gaszBuildToolchainNames[BT_NumToolchains] = { "W65C816", "65C64", "MSVC", "GCC" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::Init(const char* szName, EBuildToolchain eToolchain, const char* szSolutionConfiguration)
{
	mszName.Init(szName);
	meToolchain = eToolchain;
	mszSolutionConfiguration.Init(szSolutionConfiguration);
	mszBreakpointSpace.Init();
	mbNoFloats = false;
	mbMathTables = true;
	meProgram = BP_None;
	mcImage.Init();
	mcExecutable.Init();
	mcModule.Init();
	maszLoadFiles.Init();
	mauiLoadAddresses.Init();
	mszCard.Init();
	mszCardPath.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// An empty configuration, to be loaded.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::Init(void)
{
	Init("", BT_MSVC, "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::Kill(void)
{
	mszCardPath.Kill();
	mszCard.Kill();
	mauiLoadAddresses.Kill();
	maszLoadFiles.Kill();
	mcModule.Kill();
	mcExecutable.Kill();
	mcImage.Kill();
	mszBreakpointSpace.Kill();
	mszSolutionConfiguration.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// <Configuration Name="Debug|W65C816" Toolchain="W65C816" Solution="Debug|Win32"/>, with its <Image>, <Executable> or
// <Module> inside it if it has one.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SaveXML(CXMLWriter* pcWriter)
{
	size	i;

	pcWriter->Begin(BUILD_CONFIGURATION_TAG);
	pcWriter->Attribute(BUILD_CONFIGURATION_NAME, mszName.Text());
	pcWriter->Attribute(BUILD_CONFIGURATION_TOOLCHAIN, GetToolchainName(meToolchain));
	pcWriter->Attribute(BUILD_CONFIGURATION_SOLUTION, mszSolutionConfiguration.Text());
	if (!mszBreakpointSpace.Empty())
	{
		pcWriter->Attribute(BUILD_CONFIGURATION_BREAKPOINT_SPACE, mszBreakpointSpace.Text());
	}
	if (mbNoFloats)
	{
		pcWriter->Attribute(BUILD_CONFIGURATION_NO_FLOATS, "true");
	}
	if (!mbMathTables)
	{
		pcWriter->Attribute(BUILD_CONFIGURATION_MATH_TABLES, "false");
	}
	if (HasCard())
	{
		pcWriter->Attribute(BUILD_CONFIGURATION_CARD, mszCard.Text());
		pcWriter->Attribute(BUILD_CONFIGURATION_CARD_PATH, mszCardPath.Text());
	}
	if (meProgram == BP_Image)
	{
		mcImage.SaveXML(pcWriter);
	}
	else if (meProgram == BP_Executable)
	{
		mcExecutable.SaveXML(pcWriter);
	}
	else if (meProgram == BP_Module)
	{
		mcModule.SaveXML(pcWriter);
	}
	for (i = 0; i < maszLoadFiles.NumElements(); i++)
	{
		pcWriter->Begin(BUILD_CONFIGURATION_LOAD);
		pcWriter->Attribute(BUILD_CONFIGURATION_LOAD_FILE, maszLoadFiles.GetText(i));
		Asm65WriteHexAttribute(pcWriter, BUILD_CONFIGURATION_LOAD_ADDRESS, *mauiLoadAddresses.Get(i));
		pcWriter->End();
	}
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// False if pcTag isn't a configuration with a name, a toolchain there is
// and a solution configuration, or if its <Image>, <Executable>,
// <Module> or a <Load> isn't one (they are only for an HCP65 toolchain,
// which has one of an image, an executable and a module at most), or Card
// is without CardPath.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::LoadXML(CMarkupTag* pcTag)
{
	char*			szName;
	char*			szToolchain;
	char*			szSolution;
	char*			szNoFloats;
	char*			szMathTables;
	EBuildToolchain	eToolchain;
	CMarkupTag*		pcImage;
	CMarkupTag*		pcExecutable;
	CMarkupTag*		pcModule;
	CMarkupTag*		pcLoad;
	STagIterator	sIterator;
	char*			szFile;
	uint32			uiAddress;

	if (StringCompare(pcTag->GetName(), BUILD_CONFIGURATION_TAG) != 0)
	{
		return false;
	}
	szName = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_NAME);
	szToolchain = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_TOOLCHAIN);
	szSolution = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_SOLUTION);
	if (!szName || (szName[0] == '\0') || !szToolchain || !FindToolchain(szToolchain, &eToolchain) || !szSolution || (szSolution[0] == '\0'))
	{
		return false;
	}

	mszName.Set(szName);
	meToolchain = eToolchain;
	mszSolutionConfiguration.Set(szSolution);
	SetBreakpointSpace(pcTag->GetAttribute((char*)BUILD_CONFIGURATION_BREAKPOINT_SPACE));
	szNoFloats = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_NO_FLOATS);
	SetNoFloats((szNoFloats != NULL) && (strcmp(szNoFloats, "true") == 0));
	szMathTables = pcTag->GetAttribute((char*)BUILD_CONFIGURATION_MATH_TABLES);
	SetMathTables((szMathTables == NULL) || (strcmp(szMathTables, "false") != 0));

	pcImage = pcTag->GetTag(BUILD_CONFIGURATION_IMAGE);
	if (pcImage)
	{
		if (!IsHCP65() || !mcImage.LoadXML(pcImage))
		{
			return false;
		}
		meProgram = BP_Image;
	}
	pcExecutable = pcTag->GetTag(BUILD_CONFIGURATION_EXECUTABLE);
	if (pcExecutable)
	{
		if (!IsHCP65() || (meProgram != BP_None) || !mcExecutable.LoadXML(pcExecutable))
		{
			return false;
		}
		meProgram = BP_Executable;
	}
	pcModule = pcTag->GetTag(BUILD_CONFIGURATION_MODULE);
	if (pcModule)
	{
		if (!IsHCP65() || (meProgram != BP_None) || !mcModule.LoadXML(pcModule))
		{
			return false;
		}
		meProgram = BP_Module;
	}

	for (pcLoad = pcTag->GetTag(BUILD_CONFIGURATION_LOAD, &sIterator); pcLoad; pcLoad = pcTag->GetNextTag(&sIterator))
	{
		szFile = pcLoad->GetStringAttribute(BUILD_CONFIGURATION_LOAD_FILE);
		if (!IsHCP65() || !szFile || (szFile[0] == '\0') || !Asm65ReadUint32Attribute(pcLoad, BUILD_CONFIGURATION_LOAD_ADDRESS, &uiAddress))
		{
			return false;
		}
		AddLoad(szFile, uiAddress);
	}

	SetCard(pcTag->GetAttribute((char*)BUILD_CONFIGURATION_CARD), pcTag->GetAttribute((char*)BUILD_CONFIGURATION_CARD_PATH));
	if ((mszCard.Empty() != mszCardPath.Empty()) || (HasCard() && !IsHCP65()))
	{
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether it is built by the HCP65 compiler, which writes listings.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::IsHCP65(void)
{
	return (meToolchain == BT_W65C816) || (meToolchain == BT_65C64);
}


//////////////////////////////////////////////////////////////////////////
//
// The processor what it builds runs on, as an emulator names it ("W65C816",
// "65C64"); NULL for this computer's (MSVC, GCC).
//
//////////////////////////////////////////////////////////////////////////
const char* CBuildConfiguration::GetProcessorName(void)
{
	return IsHCP65() ? GetToolchainName(meToolchain) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The name's configuration: Debug of Debug|W65C816, all of a name without
// a |.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::GetConfigurationName(CChars* pszDest)
{
	const char*	szBar;

	szBar = strchr(mszName.Text(), '|');
	if (szBar)
	{
		pszDest->Append(mszName.Text(), (size)(szBar - mszName.Text()));
	}
	else
	{
		pszDest->Append(mszName);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The name's platform: W65C816 of Debug|W65C816, the toolchain's name for
// a name without a |.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::GetPlatformName(CChars* pszDest)
{
	const char*	szBar;

	szBar = strchr(mszName.Text(), '|');
	pszDest->Append(szBar ? szBar + 1 : GetToolchainName(meToolchain));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether its configuration is szConfiguration and its platform
// szPlatform.  NULL matches any.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::Is(const char* szConfiguration, const char* szPlatform)
{
	CChars	szConfigurationName;
	CChars	szPlatformName;
	bool	bResult;

	szConfigurationName.Init();
	szPlatformName.Init();
	GetConfigurationName(&szConfigurationName);
	GetPlatformName(&szPlatformName);
	bResult = ((szConfiguration == NULL) || szConfigurationName.Equals(szConfiguration)) && ((szPlatform == NULL) || szPlatformName.Equals(szPlatform));
	szPlatformName.Kill();
	szConfigurationName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
const char* CBuildConfiguration::GetToolchainName(EBuildToolchain eToolchain)
{
	return ((eToolchain >= 0) && (eToolchain < BT_NumToolchains)) ? gaszBuildToolchainNames[eToolchain] : "";
}


//////////////////////////////////////////////////////////////////////////
//
// The toolchain called szName, whatever its case.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::FindToolchain(const char* szName, EBuildToolchain* peToolchain)
{
	int	i;

	for (i = 0; i < BT_NumToolchains; i++)
	{
		if (StringInsensitiveCompare(szName, gaszBuildToolchainNames[i]) == 0)
		{
			*peToolchain = (EBuildToolchain)i;
			return true;
		}
	}
	return false;
}


char* CBuildConfiguration::GetName(void) { return mszName.Text(); }
EBuildToolchain CBuildConfiguration::GetToolchain(void) { return meToolchain; }
char* CBuildConfiguration::GetSolutionConfiguration(void) { return mszSolutionConfiguration.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// Whether a build links what it compiles into an image (GetImage).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::HasImage(void)
{
	return meProgram == BP_Image;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CAsm65Image* CBuildConfiguration::GetImage(void)
{
	return &mcImage;
}


//////////////////////////////////////////////////////////////////////////
//
// The emulator's breakpoint space ("Kernel", "User") debugging puts the
// breakpoints in; NULL or "" for every space.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SetBreakpointSpace(const char* szSpace)
{
	mszBreakpointSpace.Set(szSpace ? szSpace : "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CBuildConfiguration::GetBreakpointSpace(void)
{
	return mszBreakpointSpace.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// Floating point is an error (CCPP65Builder::SetNoFloats): the kernal has
// none.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SetNoFloats(bool bNoFloats)
{
	mbNoFloats = bNoFloats;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::IsNoFloats(void)
{
	return mbNoFloats;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the program runs with the maths tables in memory (true unless
// set; CCPP65Builder::SetMathTables): the kernal and the operating system,
// which loads them, run without.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SetMathTables(bool bMathTables)
{
	mbMathTables = bMathTables;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::IsMathTables(void)
{
	return mbMathTables;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a build links what it compiles into an executable, a program a
// kernal loads (GetExecutable).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::HasExecutable(void)
{
	return meProgram == BP_Executable;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CAsm65Executable* CBuildConfiguration::GetExecutable(void)
{
	return &mcExecutable;
}


//////////////////////////////////////////////////////////////////////////
//
// The build links an executable (in place of what it linked before), as
// an <Executable> in the configurations' file says: its symbols are added
// to what is returned.  A configuration made other than from a file
// (hcp65build's plain command line) is given one this way.
//
//////////////////////////////////////////////////////////////////////////
CAsm65Executable* CBuildConfiguration::SetExecutable(const char* szFile, const char* szEntry, uint32 uiLimit)
{
	ClearProgram();
	mcExecutable.Kill();
	mcExecutable.Init(szFile, szEntry, uiLimit);
	meProgram = BP_Executable;
	return &mcExecutable;
}


//////////////////////////////////////////////////////////////////////////
//
// The build links an image (in place of what it linked before), as an
// <Image> says: its regions are added to what is returned.
//
//////////////////////////////////////////////////////////////////////////
CAsm65Image* CBuildConfiguration::SetImage(const char* szFile, uint32 uiSize, uint8 uiFill, uint32 uiLimit, uint32 uiLoad)
{
	ClearProgram();
	mcImage.Kill();
	mcImage.Init(szFile, uiSize, uiFill, uiLimit, uiLoad);
	meProgram = BP_Image;
	return &mcImage;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a build links what it compiles into a module, a relocatable
// driver a kernal loads (GetModule).
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::HasModule(void)
{
	return meProgram == BP_Module;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
CAsm65Module* CBuildConfiguration::GetModule(void)
{
	return &mcModule;
}


//////////////////////////////////////////////////////////////////////////
//
// The build links a module (in place of what it linked before), as a
// <Module> says.
//
//////////////////////////////////////////////////////////////////////////
CAsm65Module* CBuildConfiguration::SetModule(const char* szFile, const char* szEntry, uint32 uiLimit)
{
	ClearProgram();
	mcModule.Kill();
	mcModule.Init(szFile, szEntry, uiLimit);
	meProgram = BP_Module;
	return &mcModule;
}


//////////////////////////////////////////////////////////////////////////
//
// The image, executable and module emptied: the build links nothing until
// one is set.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::ClearProgram(void)
{
	mcModule.Kill();
	mcModule.Init();
	mcExecutable.Kill();
	mcExecutable.Init();
	mcImage.Kill();
	mcImage.Init();
	meProgram = BP_None;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a build links (an image, an executable or a module) or only
// compiles.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::IsLinked(void)
{
	return meProgram != BP_None;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
EBuildProgram CBuildConfiguration::GetProgram(void)
{
	return meProgram;
}


//////////////////////////////////////////////////////////////////////////
//
// The file the build links into (its image's, executable's or module's),
// "" if it doesn't link.
//
//////////////////////////////////////////////////////////////////////////
char* CBuildConfiguration::GetProgramFile(void)
{
	switch (meProgram)
	{
	case BP_Image:
		return mcImage.GetFile();
	case BP_Executable:
		return mcExecutable.GetFile();
	case BP_Module:
		return mcModule.GetFile();
	default:
		return (char*)"";
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The highest address the build's link may place anything at, zero for
// none (or if it doesn't link).
//
//////////////////////////////////////////////////////////////////////////
uint32 CBuildConfiguration::GetProgramLimit(void)
{
	switch (meProgram)
	{
	case BP_Image:
		return mcImage.GetLimit();
	case BP_Executable:
		return mcExecutable.GetLimit();
	case BP_Module:
		return mcModule.GetLimit();
	default:
		return 0;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A file debugging loads into the waiting emulator at uiAddress before
// anything runs (a kernal ROM for a program).  Relative to the
// configurations' file.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::AddLoad(const char* szFile, uint32 uiAddress)
{
	maszLoadFiles.Add(szFile);
	mauiLoadAddresses.Add(uiAddress);
}


size CBuildConfiguration::NumLoads(void)						{ return maszLoadFiles.NumElements(); }
char* CBuildConfiguration::GetLoadFile(size uiIndex)			{ return maszLoadFiles.GetText(uiIndex); }
uint32 CBuildConfiguration::GetLoadAddress(size uiIndex)		{ return *mauiLoadAddresses.Get(uiIndex); }


//////////////////////////////////////////////////////////////////////////
//
// The SD card image (relative to the configurations' file) debugging
// writes the executable onto, as szPath on its FAT volume; NULL or "" for
// none.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::SetCard(const char* szCard, const char* szPath)
{
	mszCard.Set(szCard ? szCard : "");
	mszCardPath.Set(szPath ? szPath : "");
}


bool CBuildConfiguration::HasCard(void)		{ return !mszCard.Empty() && !mszCardPath.Empty(); }
char* CBuildConfiguration::GetCard(void)	{ return mszCard.Text(); }
char* CBuildConfiguration::GetCardPath(void)	{ return mszCardPath.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// Whether what it builds is put on its card for the kernal to run: an
// executable (a program) or a module (a driver), with a Card.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::IsOnCard(void)
{
	return ((meProgram == BP_Executable) || (meProgram == BP_Module)) && HasCard();
}

