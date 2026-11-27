/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <stdlib.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "VisualStudioLib/VcxConfiguration.h"
#include "BuildConfiguration.h"


//What a project says in its .vcxproj for an HCP65 configuration (ConfigurationsPlan.txt C4), each a setting of a
//tool as MSVC's are of ClCompile and Link:
//
//  <ItemDefinitionGroup Condition="'$(Platform)'=='HCP65C816'">
//    <HCP65Compile>
//      <NoFloats>true</NoFloats>
//    </HCP65Compile>
//    <HCP65Link>
//      <Executable>modplay.exe</Executable>
//      <Entry>UserStart</Entry>
//      <Limit>0x200000</Limit>
//      <Symbols>UserHeapSize=0x180000</Symbols>
//    </HCP65Link>
//  </ItemDefinitionGroup>
//
//HCP65Compile's settings are named as the configurations file's attributes are.  Those of the whole program
//are read from the startup project's group; those about one project's code (gaszOwnOptions) from each project's
//own, the startup project's being what a project that says nothing has.  HCP65Link is the startup project's: one of Executable, Module or Image names the file
//linked, with Entry and Limit, an executable's Symbols (Name=Value;...), and an image's Size, Fill, Load, Text and
//Regions (Start-End@Offset;...).  A setting a project hasn't is as the configuration had it.
#define HCP65_COMPILE	"HCP65Compile."
#define HCP65_LINK		"HCP65Link."

static const char* gaszOwnOptions[] = { "Debug", "Unoptimised", "WarnMissingOverride", "WarnStringToPointer", "MultiWordCasts", "CConversions", NULL };


//////////////////////////////////////////////////////////////////////////
//
// A tool's setting in the configuration, NULL if it hasn't it or it is
// empty.
//
//////////////////////////////////////////////////////////////////////////
static char* GetSetting(CVcxConfiguration* pcConfiguration, const char* szTool, const char* szName)
{
	CChars	szProperty;
	char*	szValue;

	szProperty.Init(szTool);
	szProperty.Append(szName);
	szValue = pcConfiguration->GetProperty(szProperty.Text());
	szProperty.Kill();
	return (szValue && (szValue[0] != '\0')) ? szValue : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A true or false setting: false, and *pbValue as it was, if the
// configuration hasn't it.
//
//////////////////////////////////////////////////////////////////////////
static bool GetBoolean(CVcxConfiguration* pcConfiguration, const char* szName, bool* pbValue)
{
	char*	szValue;

	szValue = GetSetting(pcConfiguration, HCP65_COMPILE, szName);
	if (szValue == NULL)
	{
		return false;
	}
	*pbValue = StringInsensitiveCompare(szValue, "true") == 0;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A number, in hexadecimal (0x) or decimal, that fits 32 bits: all of
// szText to cEnd (or its end).  False if it isn't one.
//
//////////////////////////////////////////////////////////////////////////
static bool ParseNumber(const char* szText, char cEnd, int64 iMaximum, int64* piValue, const char** pszAfter)
{
	char*		szEnd;
	long long	lli;

	lli = strtoll(szText, &szEnd, 0);
	if ((szEnd == szText) || ((*szEnd != cEnd) && (*szEnd != '\0')) || (lli < 0) || (lli > iMaximum))
	{
		return false;
	}
	*piValue = (int64)lli;
	if (pszAfter)
	{
		*pszAfter = (*szEnd != '\0') ? szEnd + 1 : szEnd;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// An HCP65Link number: uiDefault if the configuration hasn't it.  False
// if it has and it isn't one.
//
//////////////////////////////////////////////////////////////////////////
static bool GetNumber(CVcxConfiguration* pcConfiguration, const char* szName, uint32 uiDefault, uint32* puiValue)
{
	char*	szValue;
	int64	iValue;

	*puiValue = uiDefault;
	szValue = GetSetting(pcConfiguration, HCP65_LINK, szName);
	if (szValue == NULL)
	{
		return true;
	}
	if (!ParseNumber(szValue, '\0', 0xFFFFFFFFLL, &iValue, NULL))
	{
		return false;
	}
	*puiValue = (uint32)iValue;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Takes from the startup project's configuration what it says of the
// whole program: HCP65Compile's NoFloats, MathTables, PreferSize,
// Exceptions, KeepUnusedObjects and StubMissing, and what HCP65Link links
// it into.  Its own options (ReadOwnOptions) become the program's too:
// they are what the runtime and a project that says nothing of its own
// are compiled with.  What it doesn't say is left as it is.  False, with
// the setting that is wrong in pszError, if HCP65Link can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::ReadProject(CVcxConfiguration* pcConfiguration, CChars* pszError)
{
	bool	bValue;

	if (!IsHCP65() || (pcConfiguration == NULL))
	{
		return true;
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_NO_FLOATS, &bValue))
	{
		SetNoFloats(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_MATH_TABLES, &bValue))
	{
		SetMathTables(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_PREFER_SIZE, &bValue))
	{
		SetPreferSize(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_EXCEPTIONS, &bValue))
	{
		SetExceptions(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_KEEP_UNUSED_OBJECTS, &bValue))
	{
		SetKeepUnusedObjects(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_STUB_MISSING, &bValue))
	{
		SetStubMissing(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_DEBUG, &bValue))
	{
		SetDebug(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_WARN_MISSING_OVERRIDE, &bValue))
	{
		SetWarnMissingOverride(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_WARN_STRING_TO_POINTER, &bValue))
	{
		SetWarnStringToPointer(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_MULTI_WORD_CASTS, &bValue))
	{
		SetMultiWordCasts(bValue);
	}
	if (GetBoolean(pcConfiguration, BUILD_CONFIGURATION_C_CONVERSIONS, &bValue))
	{
		SetCConversions(bValue);
	}
	return ReadProjectLink(pcConfiguration, pszError);
}


//////////////////////////////////////////////////////////////////////////
//
// What the project's HCP65Link links the program into, in place of what
// the configuration had; nothing changes if it names no file.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::ReadProjectLink(CVcxConfiguration* pcConfiguration, CChars* pszError)
{
	CAsm65Executable*	pcExecutable;
	CAsm65Image*		pcImage;
	char*				szExecutable;
	char*				szModule;
	char*				szImage;
	char*				szEntry;
	char*				szList;
	const char*			sz;
	const char*			szEquals;
	CChars				szName;
	uint32				uiLimit;
	uint32				uiSize;
	uint32				uiFill;
	uint32				uiLoad;
	uint32				uiText;
	int64				iStart;
	int64				iEnd;
	int64				iOffset;
	bool				bResult;

	szExecutable = GetSetting(pcConfiguration, HCP65_LINK, BUILD_CONFIGURATION_EXECUTABLE);
	szModule = GetSetting(pcConfiguration, HCP65_LINK, BUILD_CONFIGURATION_MODULE);
	szImage = GetSetting(pcConfiguration, HCP65_LINK, BUILD_CONFIGURATION_IMAGE);
	if (!szExecutable && !szModule && !szImage)
	{
		return true;
	}
	if (((szExecutable != NULL) + (szModule != NULL) + (szImage != NULL)) != 1)
	{
		pszError->Set("HCP65Link names more than one of Executable, Module and Image.");
		return false;
	}

	szEntry = GetSetting(pcConfiguration, HCP65_LINK, "Entry");
	bResult = GetNumber(pcConfiguration, "Limit", 0, &uiLimit);
	if (!bResult)
	{
		pszError->Set("HCP65Link's Limit isn't a number.");
	}
	else if (szImage)
	{
		bResult = GetNumber(pcConfiguration, "Size", 0, &uiSize) && GetNumber(pcConfiguration, "Fill", 0, &uiFill) && (uiFill <= 0xFF) && GetNumber(pcConfiguration, "Load", 0, &uiLoad) && GetNumber(pcConfiguration, "Text", 0, &uiText) && (uiSize != 0);
		if (!bResult)
		{
			pszError->Set("HCP65Link's Image needs a Size, and its Size, Fill, Load and Text are numbers.");
			return false;
		}
		pcImage = SetImage(szImage, uiSize, (uint8)uiFill, uiLimit, uiLoad);
		pcImage->SetText(uiText);
		szList = GetSetting(pcConfiguration, HCP65_LINK, "Regions");
		for (sz = szList; bResult && sz && (*sz != '\0'); )
		{
			bResult = ParseNumber(sz, '-', 0xFFFFFFFFLL, &iStart, &sz) && ParseNumber(sz, '@', 0xFFFFFFFFLL, &iEnd, &sz) && ParseNumber(sz, ';', 0xFFFFFFFFLL, &iOffset, &sz);
			if (bResult)
			{
				pcImage->AddRegion((uint32)iStart, (uint32)iEnd, (uint32)iOffset);
			}
		}
		if (!bResult)
		{
			pszError->Set("HCP65Link's Regions are Start-End@Offset, separated by semicolons.");
		}
	}
	else if (szEntry == NULL)
	{
		bResult = false;
		pszError->Set("HCP65Link's Executable or Module needs an Entry.");
	}
	else if (szModule)
	{
		SetModule(szModule, szEntry, uiLimit);
	}
	else
	{
		pcExecutable = SetExecutable(szExecutable, szEntry, uiLimit);
		szList = GetSetting(pcConfiguration, HCP65_LINK, "Symbols");
		for (sz = szList; bResult && sz && (*sz != '\0'); )
		{
			szEquals = strchr(sz, '=');
			bResult = (szEquals != NULL) && (szEquals != sz);
			if (bResult)
			{
				szName.Init();
				szName.Append(sz, (size)(szEquals - sz));
				bResult = ParseNumber(szEquals + 1, ';', 0x7FFFFFFFFFFFFFFFLL, &iStart, &sz);
				if (bResult)
				{
					pcExecutable->AddSymbol(szName.Text(), iStart);
				}
				szName.Kill();
			}
		}
		if (!bResult)
		{
			pszError->Set("HCP65Link's Symbols are Name=Value, separated by semicolons.");
		}
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What a project's configuration says of its own code, added to
// paszOptions as "Name=true" or "Name=false", those it says: Debug,
// Unoptimised, WarnMissingOverride, WarnStringToPointer, MultiWordCasts
// and CConversions.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfiguration::ReadOwnOptions(CVcxConfiguration* pcConfiguration, CArrayChars* paszOptions)
{
	CChars	szOption;
	bool	bValue;
	size	i;

	for (i = 0; gaszOwnOptions[i] != NULL; i++)
	{
		if (GetBoolean(pcConfiguration, gaszOwnOptions[i], &bValue))
		{
			szOption.Init(gaszOwnOptions[i]);
			szOption.Append(bValue ? "=true" : "=false");
			paszOptions->Add(szOption.Text());
			szOption.Kill();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// One of a project's own options (ReadOwnOptions' "Debug=true") set in
// pcOptions, or for Unoptimised in *pbUnoptimised.  False if it isn't
// one.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfiguration::SetOwnOption(const char* szOption, CCPPCompileOptions* pcOptions, bool* pbUnoptimised)
{
	const char*	szEquals;
	size		uiLength;
	bool		bValue;

	szEquals = strchr(szOption, '=');
	if ((szEquals == NULL) || ((strcmp(szEquals + 1, "true") != 0) && (strcmp(szEquals + 1, "false") != 0)))
	{
		return false;
	}
	bValue = szEquals[1] == 't';
	uiLength = (size)(szEquals - szOption);
	if ((uiLength == 5) && (strncmp(szOption, "Debug", uiLength) == 0))
	{
		pcOptions->mbDebuggable = bValue;
	}
	else if ((uiLength == 11) && (strncmp(szOption, "Unoptimised", uiLength) == 0))
	{
		*pbUnoptimised = bValue;
	}
	else if ((uiLength == 19) && (strncmp(szOption, "WarnMissingOverride", uiLength) == 0))
	{
		pcOptions->mbWarnMissingOverride = bValue;
	}
	else if ((uiLength == 19) && (strncmp(szOption, "WarnStringToPointer", uiLength) == 0))
	{
		pcOptions->mbWarnStringToPointer = bValue;
	}
	else if ((uiLength == 14) && (strncmp(szOption, "MultiWordCasts", uiLength) == 0))
	{
		pcOptions->mbMultiWordCasts = bValue;
	}
	else if ((uiLength == 12) && (strncmp(szOption, "CConversions", uiLength) == 0))
	{
		pcOptions->mbCConversions = bValue;
	}
	else
	{
		return false;
	}
	return true;
}
