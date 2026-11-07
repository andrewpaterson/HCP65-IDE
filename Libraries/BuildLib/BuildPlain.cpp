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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "BuildWorkspace.h"
#include "BuildPlain.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool BuildIsOption(const char* szArgument, const char* szName)
{
	if ((szArgument[0] == '-') && (szArgument[1] == '-'))
	{
		szArgument += 2;
	}
	else if (szArgument[0] == '-')
	{
		szArgument++;
	}
#ifdef _WIN32
	else if (szArgument[0] == '/')
	{
		szArgument++;
	}
#endif
	else
	{
		return false;
	}
	return StringInsensitiveCompare(szArgument, szName) == 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool BuildLooksLikeOption(const char* szArgument)
{
#ifdef _WIN32
	return (szArgument[0] == '-') || (szArgument[0] == '/');
#else
	return szArgument[0] == '-';
#endif
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool BuildParseNumber(const char* szNumber, int64* piValue)
{
	char*		szEnd;
	long long	lli;

	if ((szNumber == NULL) || (*szNumber == '\0'))
	{
		return false;
	}
	if (*szNumber == '$')
	{
		if (szNumber[1] == '\0')
		{
			return false;
		}
		lli = strtoll(szNumber + 1, &szEnd, 16);
	}
	else
	{
		lli = strtoll(szNumber, &szEnd, 0);
	}
	if (*szEnd != '\0')
	{
		return false;
	}
	*piValue = (int64)lli;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool BuildParseUint32(const char* szNumber, uint32* puiValue)
{
	int64	iValue;

	if (!BuildParseNumber(szNumber, &iValue) || (iValue < 0) || (iValue > 0xFFFFFFFFLL))
	{
		return false;
	}
	*puiValue = (uint32)iValue;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildPlain::Init(void)
{
	mcDescription.Init();
	meToolchain = BT_W65C816;
	mbNoFloats = false;
	mbNoMathTables = false;
	mszOutput.Init();
	mszListings.Init();
	mszExecutable.Init();
	mszEntry.Init();
	maszSymbols.Init();
	mszImage.Init();
	mszModule.Init();
	muiSize = 0;
	muiFill = 0xFF;
	muiLoad = 0;
	muiLimit = 0;
	mbSize = false;
	mbFill = false;
	mbLoad = false;
	mbLimit = false;
	maszRegions.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildPlain::Kill(void)
{
	maszRegions.Kill();
	mszModule.Kill();
	mszImage.Kill();
	maszSymbols.Kill();
	mszEntry.Kill();
	mszExecutable.Kill();
	mszListings.Kill();
	mszOutput.Kill();
	mcDescription.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The value after the option at *puiIndex, which is moved on to it.
// False, with why in pszError, if there is none.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildPlain::NeedValue(CArrayChars* paszArguments, size* puiIndex, const char* szOption, CChars* pszError)
{
	if (*puiIndex + 1 >= paszArguments->NumElements())
	{
		pszError->Set("Option ");
		pszError->Append(szOption);
		pszError->Append(" needs a value.");
		return false;
	}
	(*puiIndex)++;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The plain option at *puiIndex of paszArguments, with its value (which
// *puiIndex is moved on to).  1 if it was one, 0 if it isn't a plain
// option, and -1, with why in pszError, if it is wrong.
//
//////////////////////////////////////////////////////////////////////////
int CBuildPlain::ParseOption(CArrayChars* paszArguments, size* puiIndex, CChars* pszError)
{
	const char*				szOption;
	const char*				szValue;
	CHCP65BuildProject*		pcProject;
	CBuildConfiguration		cConfiguration;
	EBuildToolchain			eToolchain;
	int64					iValue;
	uint32					uiStart;
	uint32					uiEnd;
	uint32					uiOffset;
	const char*				szEquals;
	bool					bHCP65;
	bool					bSubDirectories;

	szOption = paszArguments->GetText(*puiIndex);
	if (BuildIsOption(szOption, "program") || BuildIsOption(szOption, "lib"))
	{
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		mcDescription.AddProject(szValue, ".", NULL, BuildIsOption(szOption, "lib"));
		return 1;
	}
	if (BuildIsOption(szOption, "dir") || BuildIsOption(szOption, "D"))
	{
		pcProject = mcDescription.GetLastProject();
		if (!pcProject)
		{
			pszError->Set(szOption);
			pszError->Append(" must follow a -program or -lib.");
			return -1;
		}
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		if (BuildIsOption(szOption, "dir"))
		{
			pcProject->mszDirectory.Set(szValue);
		}
		else
		{
			pcProject->maszDefines.Add(szValue);
		}
		return 1;
	}
	if (BuildIsOption(szOption, "I") || BuildIsOption(szOption, "I+"))
	{
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		bSubDirectories = BuildIsOption(szOption, "I+");
		pcProject = mcDescription.GetLastProject();
		if (pcProject)
		{
			pcProject->mcIncludes.Add(szValue, bSubDirectories);
		}
		else
		{
			mcDescription.mcIncludes.Add(szValue, bSubDirectories);
		}
		return 1;
	}
	if (BuildIsOption(szOption, "target"))
	{
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		bHCP65 = CBuildConfiguration::FindToolchain(szValue, &eToolchain);
		if (bHCP65)
		{
			cConfiguration.Init(BUILD_PLAIN_CONFIGURATION, eToolchain, BUILD_PLAIN_CONFIGURATION);
			bHCP65 = cConfiguration.IsHCP65();
			cConfiguration.Kill();
		}
		if (!bHCP65)
		{
			pszError->Set("Unknown target ");
			pszError->Append(szValue);
			pszError->Append(": W65C816 or 65C64.");
			return -1;
		}
		meToolchain = eToolchain;
		return 1;
	}
	if (BuildIsOption(szOption, "nofloats"))
	{
		mbNoFloats = true;
		return 1;
	}
	if (BuildIsOption(szOption, "no-mathtables"))
	{
		mbNoMathTables = true;
		return 1;
	}
	if (BuildIsOption(szOption, "o") || BuildIsOption(szOption, "listings") || BuildIsOption(szOption, "exe") || BuildIsOption(szOption, "entry") || BuildIsOption(szOption, "image") ||
		BuildIsOption(szOption, "module"))
	{
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		if (BuildIsOption(szOption, "o"))
		{
			mszOutput.Set(szValue);
		}
		else if (BuildIsOption(szOption, "listings"))
		{
			mszListings.Set(szValue);
		}
		else if (BuildIsOption(szOption, "exe"))
		{
			mszExecutable.Set(szValue);
		}
		else if (BuildIsOption(szOption, "entry"))
		{
			mszEntry.Set(szValue);
		}
		else if (BuildIsOption(szOption, "module"))
		{
			mszModule.Set(szValue);
		}
		else
		{
			mszImage.Set(szValue);
		}
		return 1;
	}
	if (BuildIsOption(szOption, "limit") || BuildIsOption(szOption, "size") || BuildIsOption(szOption, "fill") || BuildIsOption(szOption, "load"))
	{
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		if (!BuildParseNumber(szValue, &iValue) || (iValue < 0) || (iValue > 0xFFFFFFFFLL) || (BuildIsOption(szOption, "fill") && (iValue > 0xFF)))
		{
			pszError->Set(szOption);
			pszError->Append(" isn't given a number");
			pszError->Append(BuildIsOption(szOption, "fill") ? " from 0 to 255: " : " (decimal, 0x or $ hexadecimal): ");
			pszError->Append(szValue);
			pszError->Append('.');
			return -1;
		}
		if (BuildIsOption(szOption, "limit"))
		{
			muiLimit = (uint32)iValue;
			mbLimit = true;
		}
		else if (BuildIsOption(szOption, "size"))
		{
			muiSize = (uint32)iValue;
			mbSize = true;
		}
		else if (BuildIsOption(szOption, "fill"))
		{
			muiFill = (uint32)iValue;
			mbFill = true;
		}
		else
		{
			muiLoad = (uint32)iValue;
			mbLoad = true;
		}
		return 1;
	}
	if (BuildIsOption(szOption, "symbol"))
	{
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		szEquals = strchr(szValue, '=');
		if (!szEquals || (szEquals == szValue) || !BuildParseNumber(szEquals + 1, &iValue))
		{
			pszError->Set("-symbol is given <name>=<number>, not ");
			pszError->Append(szValue);
			pszError->Append('.');
			return -1;
		}
		maszSymbols.Add(szValue);
		return 1;
	}
	if (BuildIsOption(szOption, "region"))
	{
		if (!NeedValue(paszArguments, puiIndex, szOption, pszError))
		{
			return -1;
		}
		szValue = paszArguments->GetText(*puiIndex);
		if (!ParseRegion(szValue, &uiStart, &uiEnd, &uiOffset))
		{
			pszError->Set("-region is given <start>,<end>,<offset>, not ");
			pszError->Append(szValue);
			pszError->Append('.');
			return -1;
		}
		maszRegions.Add(szValue);
		return 1;
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
// A source of the last -program or -lib, relative to its directory.
// False, with why in pszError, if there is no project yet.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildPlain::AddSource(const char* szSource, CChars* pszError)
{
	CHCP65BuildProject*	pcProject;

	pcProject = mcDescription.GetLastProject();
	if (!pcProject)
	{
		pszError->Set("The source ");
		pszError->Append(szSource);
		pszError->Append(" must follow a -program or -lib.");
		return false;
	}
	pcProject->maszSources.Add(szSource);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Checks the options go together once they are all parsed.  False, with
// why in pszError, if they don't.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildPlain::Finish(CChars* pszError)
{
	CHCP65BuildProject*	pcProject;
	size				i;
	uint32				uiStart;
	uint32				uiEnd;
	uint32				uiOffset;

	if (mcDescription.NumProjects() == 0)
	{
		pszError->Set("A plain command line needs a -program or -lib.");
		return false;
	}
	for (i = 0; i < mcDescription.NumProjects(); i++)
	{
		pcProject = mcDescription.GetProject(i);
		if (pcProject->maszSources.NumElements() == 0)
		{
			pszError->Set(pcProject->mbLibrary ? "-lib " : "-program ");
			pszError->Append(pcProject->mszName);
			pszError->Append(" has no sources.");
			return false;
		}
	}
	if (NumPrograms() > 1)
	{
		pszError->Set("Link one of an executable (-exe), an image (-image) and a module (-module).");
		return false;
	}
	if (mszExecutable.Empty() && (maszSymbols.NumElements() > 0))
	{
		pszError->Set("-symbol is for an executable: it needs -exe.");
		return false;
	}
	if (mszExecutable.Empty() && mszModule.Empty() && !mszEntry.Empty())
	{
		pszError->Set("-entry is for an executable or a module: it needs -exe or -module.");
		return false;
	}
	if ((!mszExecutable.Empty() || !mszModule.Empty()) && mszEntry.Empty())
	{
		pszError->Set(mszExecutable.Empty() ? "-module needs its entry symbol (-entry)." : "-exe needs its entry symbol (-entry).");
		return false;
	}
	if (mszImage.Empty() && (mbSize || mbFill || mbLoad || (maszRegions.NumElements() > 0)))
	{
		pszError->Set("-size, -fill, -load and -region are for an image: they need -image.");
		return false;
	}
	if ((NumPrograms() == 0) && mbLimit)
	{
		pszError->Set("-limit is for a program: it needs -exe, -image or -module.");
		return false;
	}
	if (!mszImage.Empty())
	{
		if (!mbSize || (muiSize == 0))
		{
			pszError->Set("-image needs its size (-size).");
			return false;
		}
		if (maszRegions.NumElements() == 0)
		{
			pszError->Set("-image needs a -region.");
			return false;
		}
		for (i = 0; i < maszRegions.NumElements(); i++)
		{
			ParseRegion(maszRegions.GetText(i), &uiStart, &uiEnd, &uiOffset);
			if ((uiEnd <= uiStart) || (uiOffset > muiSize) || (uiEnd - uiStart > muiSize - uiOffset))
			{
				pszError->Set("-region ");
				pszError->Append(maszRegions.GetText(i));
				pszError->Append(" doesn't fit in the image's size.");
				return false;
			}
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// How many of -exe, -image and -module were given.
//
//////////////////////////////////////////////////////////////////////////
size CBuildPlain::NumPrograms(void)
{
	return (mszExecutable.Empty() ? 0 : 1) + (mszImage.Empty() ? 0 : 1) + (mszModule.Empty() ? 0 : 1);
}


//////////////////////////////////////////////////////////////////////////
//
// The configuration the options describe: Plain|<target>, with the link
// settings and the executable, image or module.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildPlain::MakeConfiguration(CBuildConfiguration* pcConfiguration, CChars* pszError)
{
	CChars				szName;
	CAsm65Executable*	pcExecutable;
	CAsm65Image*		pcImage;
	CChars				szSymbol;
	int64				iValue;
	uint32				uiStart;
	uint32				uiEnd;
	uint32				uiOffset;
	size				i;
	size				uiEquals;

	szName.Init(BUILD_PLAIN_CONFIGURATION "|");
	szName.Append(CBuildConfiguration::GetToolchainName(meToolchain));
	pcConfiguration->Init(szName.Text(), meToolchain, BUILD_PLAIN_CONFIGURATION);
	szName.Kill();
	pcConfiguration->SetNoFloats(mbNoFloats);
	pcConfiguration->SetMathTables(!mbNoMathTables);
	if (!mszExecutable.Empty())
	{
		pcExecutable = pcConfiguration->SetExecutable(mszExecutable.Text(), mszEntry.Text(), muiLimit);
		for (i = 0; i < maszSymbols.NumElements(); i++)
		{
			szSymbol.Init(maszSymbols.GetText(i));
			uiEquals = szSymbol.Find(0, '=');
			BuildParseNumber(szSymbol.Text(uiEquals + 1), &iValue);
			szSymbol.SetLength(uiEquals);
			pcExecutable->AddSymbol(szSymbol.Text(), iValue);
			szSymbol.Kill();
		}
	}
	else if (!mszModule.Empty())
	{
		pcConfiguration->SetModule(mszModule.Text(), mszEntry.Text(), muiLimit);
	}
	else if (!mszImage.Empty())
	{
		pcImage = pcConfiguration->SetImage(mszImage.Text(), muiSize, (uint8)muiFill, muiLimit, muiLoad);
		for (i = 0; i < maszRegions.NumElements(); i++)
		{
			ParseRegion(maszRegions.GetText(i), &uiStart, &uiEnd, &uiOffset);
			pcImage->AddRegion(uiStart, uiEnd, uiOffset);
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What to compile with every path made full (relative ones are to the
// current directory): what a solution build would describe.  The projects
// are labelled with the configuration (Plain|<target>).
//
//////////////////////////////////////////////////////////////////////////
void CBuildPlain::MakeDescription(CHCP65BuildDescription* pcDest)
{
	CHCP65BuildProject*	pcProject;
	CHCP65BuildProject*	pcCopy;
	CChars				szPath;
	CChars				szLabel;
	size				i;
	size				j;

	szLabel.Init(BUILD_PLAIN_CONFIGURATION "|");
	szLabel.Append(CBuildConfiguration::GetToolchainName(meToolchain));
	pcDest->Clear();
	szPath.Init();
	for (i = 0; i < mcDescription.mcIncludes.NumIncludes(); i++)
	{
		MakeFullPath(mcDescription.mcIncludes.GetDirectory(i), &szPath);
		pcDest->mcIncludes.Add(szPath.Text(), mcDescription.mcIncludes.HasSubDirectories(i));
	}
	for (i = 0; i < mcDescription.NumProjects(); i++)
	{
		pcProject = mcDescription.GetProject(i);
		MakeFullPath(pcProject->mszDirectory.Text(), &szPath);
		pcCopy = pcDest->AddProject(pcProject->mszName.Text(), szPath.Text(), szLabel.Text(), pcProject->mbLibrary);
		for (j = 0; j < pcProject->mcIncludes.NumIncludes(); j++)
		{
			MakeFullPath(pcProject->mcIncludes.GetDirectory(j), &szPath);
			pcCopy->mcIncludes.Add(szPath.Text(), pcProject->mcIncludes.HasSubDirectories(j));
		}
		pcCopy->maszSources.Copy(&pcProject->maszSources);
		pcCopy->maszDefines.Copy(&pcProject->maszDefines);
	}
	szPath.Kill();
	szLabel.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The plain command line a solution's build is the same as, an argument
// at a time, added to paszArguments: its options, then each project's with
// its sources in a list of their own in szUnitsDirectory
// (<Project>.units, read with @), which are written.  Paths are relative
// to the current directory, where the command line must be run.  The
// lists written are added to paszWritten.  False if one couldn't be.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildPlain::Describe(CArrayChars* paszArguments, const char* szUnitsDirectory, CHCP65BuildDescription* pcDescription, CBuildConfiguration* pcConfiguration, const char* szLibc65, CArrayChars* paszWritten)
{
	CFileUtil				cFileUtil;
	CTextFile				cFile;
	CChars					szUnits;
	CChars					szUnitsText;
	CChars					szPath;
	CChars					szArgument;
	CHCP65BuildProject*		pcProject;
	CAsm65Executable*		pcExecutable;
	CAsm65Image*			pcImage;
	CAsm65Module*			pcModule;
	SAsm65ImageRegion*		psRegion;
	char					szNumber[64];
	size					i;
	size					j;
	bool					bResult;

	szPath.Init();
	paszArguments->Add("-target");
	paszArguments->Add(CBuildConfiguration::GetToolchainName(pcConfiguration->GetToolchain()));
	MakeWrittenPath(szLibc65, &szPath);
	paszArguments->Add("-libc65");
	paszArguments->Add(szPath.Text());
	if (pcConfiguration->IsNoFloats())
	{
		paszArguments->Add("-nofloats");
	}
	if (!pcConfiguration->IsMathTables())
	{
		paszArguments->Add("-no-mathtables");
	}
	for (i = 0; i < pcDescription->mcIncludes.NumIncludes(); i++)
	{
		MakeWrittenPath(pcDescription->mcIncludes.GetDirectory(i), &szPath);
		paszArguments->Add(pcDescription->mcIncludes.HasSubDirectories(i) ? "-I+" : "-I");
		paszArguments->Add(szPath.Text());
	}

	bResult = true;
	for (i = 0; i < pcDescription->NumProjects(); i++)
	{
		pcProject = pcDescription->GetProject(i);
		paszArguments->Add(pcProject->mbLibrary ? "-lib" : "-program");
		paszArguments->Add(pcProject->mszName.Text());
		MakeWrittenPath(pcProject->mszDirectory.Text(), &szPath);
		paszArguments->Add("-dir");
		paszArguments->Add(szPath.Text());
		for (j = 0; j < pcProject->mcIncludes.NumIncludes(); j++)
		{
			MakeWrittenPath(pcProject->mcIncludes.GetDirectory(j), &szPath);
			paszArguments->Add(pcProject->mcIncludes.HasSubDirectories(j) ? "-I+" : "-I");
			paszArguments->Add(szPath.Text());
		}
		for (j = 0; j < pcProject->maszDefines.NumElements(); j++)
		{
			paszArguments->Add("-D");
			paszArguments->Add(pcProject->maszDefines.GetText(j));
		}

		szUnits.Init(szUnitsDirectory);
		cFileUtil.AppendToPath(&szUnits, pcProject->mszName.Text());
		szUnits.Append(BUILD_PLAIN_UNITS_EXTENSION);
		szUnitsText.Init();
		for (j = 0; j < pcProject->maszSources.NumElements(); j++)
		{
			szUnitsText.Append(pcProject->maszSources.GetText(j))->Append('\n');
		}
		cFile.Init(szUnitsText.Text());
		bResult &= cFile.Write(szUnits.Text());
		cFile.Kill();
		paszWritten->Add(szUnits.Text());
		szUnitsText.Kill();
		MakeWrittenPath(szUnits.Text(), &szPath);
		szArgument.Init("@");
		szArgument.Append(szPath);
		paszArguments->Add(szArgument.Text());
		szArgument.Kill();
		szUnits.Kill();
	}

	if (pcConfiguration->HasExecutable())
	{
		pcExecutable = pcConfiguration->GetExecutable();
		paszArguments->Add("-exe");
		paszArguments->Add(pcExecutable->GetFile());
		paszArguments->Add("-entry");
		paszArguments->Add(pcExecutable->GetEntry());
		if (pcExecutable->GetLimit() != 0)
		{
			snprintf(szNumber, sizeof(szNumber), "0x%X", (unsigned int)pcExecutable->GetLimit());
			paszArguments->Add("-limit");
			paszArguments->Add(szNumber);
		}
		for (i = 0; i < pcExecutable->NumSymbols(); i++)
		{
			snprintf(szNumber, sizeof(szNumber), "=0x%llX", (unsigned long long)pcExecutable->GetSymbolValue(i));
			szArgument.Init(pcExecutable->GetSymbolName(i));
			szArgument.Append(szNumber);
			paszArguments->Add("-symbol");
			paszArguments->Add(szArgument.Text());
			szArgument.Kill();
		}
	}
	else if (pcConfiguration->HasModule())
	{
		pcModule = pcConfiguration->GetModule();
		paszArguments->Add("-module");
		paszArguments->Add(pcModule->GetFile());
		paszArguments->Add("-entry");
		paszArguments->Add(pcModule->GetEntry());
		if (pcModule->GetLimit() != 0)
		{
			snprintf(szNumber, sizeof(szNumber), "0x%X", (unsigned int)pcModule->GetLimit());
			paszArguments->Add("-limit");
			paszArguments->Add(szNumber);
		}
	}
	else if (pcConfiguration->HasImage())
	{
		pcImage = pcConfiguration->GetImage();
		paszArguments->Add("-image");
		paszArguments->Add(pcImage->GetFile());
		snprintf(szNumber, sizeof(szNumber), "0x%X", (unsigned int)pcImage->GetSize());
		paszArguments->Add("-size");
		paszArguments->Add(szNumber);
		snprintf(szNumber, sizeof(szNumber), "0x%X", (unsigned int)pcImage->GetFill());
		paszArguments->Add("-fill");
		paszArguments->Add(szNumber);
		if (pcImage->GetLimit() != 0)
		{
			snprintf(szNumber, sizeof(szNumber), "0x%X", (unsigned int)pcImage->GetLimit());
			paszArguments->Add("-limit");
			paszArguments->Add(szNumber);
		}
		snprintf(szNumber, sizeof(szNumber), "0x%X", (unsigned int)pcImage->GetLoad());
		paszArguments->Add("-load");
		paszArguments->Add(szNumber);
		for (i = 0; i < pcImage->NumRegions(); i++)
		{
			psRegion = pcImage->GetRegion(i);
			snprintf(szNumber, sizeof(szNumber), "0x%X,0x%X,0x%X", (unsigned int)psRegion->uiStart, (unsigned int)psRegion->uiEnd, (unsigned int)psRegion->uiOffset);
			paszArguments->Add("-region");
			paszArguments->Add(szNumber);
		}
	}
	szPath.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Writes the plain command line a solution's build is the same as
// (Describe) into szFile, an argument a line (hcp65build @szFile reads
// it), with each project's sources in <Project>.units beside it.
// szComment is written first.  The arguments are added to paszArguments
// and the files written to paszWritten.  False if one couldn't be.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildPlain::Write(const char* szFile, CHCP65BuildDescription* pcDescription, CBuildConfiguration* pcConfiguration, const char* szLibc65, const char* szComment, CArrayChars* paszArguments, CArrayChars* paszWritten)
{
	CFileUtil	cFileUtil;
	CTextFile	cFile;
	CChars		szText;
	CChars		szName;
	CChars		szDirectory;
	size		uiFirst;
	size		i;
	bool		bResult;

	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFile, &szName, &szDirectory);
	szName.Kill();

	uiFirst = paszArguments->NumElements();
	bResult = Describe(paszArguments, szDirectory.Text(), pcDescription, pcConfiguration, szLibc65, paszWritten);

	szText.Init();
	if (szComment)
	{
		szText.Append("# ")->Append(szComment)->Append('\n');
	}
	for (i = uiFirst; i < paszArguments->NumElements(); i++)
	{
		szText.Append(paszArguments->GetText(i))->Append('\n');
	}
	cFile.Init(szText.Text());
	bResult &= cFile.Write(szFile);
	cFile.Kill();
	paszWritten->Add(szFile);
	szText.Kill();
	szDirectory.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// szProgram run with paszArguments as one PowerShell command line:
// & <program> <arguments>, each argument PowerShell would read as
// something else (@BaseLib.units is splatting) in single quotes.
//
//////////////////////////////////////////////////////////////////////////
void CBuildPlain::MakePowerShellLine(const char* szProgram, CArrayChars* paszArguments, CChars* pszDest)
{
	size	i;

	pszDest->Set("& ");
	AppendPowerShellArgument(szProgram, pszDest);
	for (i = 0; i < paszArguments->NumElements(); i++)
	{
		pszDest->Append(' ');
		AppendPowerShellArgument(paszArguments->GetText(i), pszDest);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szArgument as PowerShell is given it: as it is if it is only letters,
// digits and characters PowerShell passes on unchanged, else in single
// quotes (with a single quote in it doubled).
//
//////////////////////////////////////////////////////////////////////////
void CBuildPlain::AppendPowerShellArgument(const char* szArgument, CChars* pszDest)
{
	const char*	sz;
	bool		bPlain;

	bPlain = (*szArgument != '\0');
	for (sz = szArgument; *sz != '\0'; sz++)
	{
		if (!(((*sz >= 'a') && (*sz <= 'z')) || ((*sz >= 'A') && (*sz <= 'Z')) || ((*sz >= '0') && (*sz <= '9')) || (strchr("-+_.\\/:=", *sz) != NULL)))
		{
			bPlain = false;
			break;
		}
	}
	if (bPlain)
	{
		pszDest->Append(szArgument);
		return;
	}
	pszDest->Append('\'');
	for (sz = szArgument; *sz != '\0'; sz++)
	{
		if (*sz == '\'')
		{
			pszDest->Append('\'');
		}
		pszDest->Append(*sz);
	}
	pszDest->Append('\'');
}


//////////////////////////////////////////////////////////////////////////
//
// "Start,End,Offset", each a number.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildPlain::ParseRegion(const char* szRegion, uint32* puiStart, uint32* puiEnd, uint32* puiOffset)
{
	CChars		szCopy;
	CArrayChars	aszParts;
	bool		bResult;

	szCopy.Init(szRegion);
	aszParts.Init();
	szCopy.Split(&aszParts, ',');
	bResult = (aszParts.NumElements() == 3) && BuildParseUint32(aszParts.GetText(0), puiStart) && BuildParseUint32(aszParts.GetText(1), puiEnd) && BuildParseUint32(aszParts.GetText(2), puiOffset);
	aszParts.Kill();
	szCopy.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// szPath made full: relative to the current directory unless absolute.
//
//////////////////////////////////////////////////////////////////////////
void CBuildPlain::MakeFullPath(const char* szPath, CChars* pszDest)
{
	CFileUtil	cFileUtil;

	pszDest->Set(szPath);
	cFileUtil.FullPath(pszDest);
}


//////////////////////////////////////////////////////////////////////////
//
// szPath (full) as it is written in a plain command line: relative to the
// current directory, "." for it.
//
//////////////////////////////////////////////////////////////////////////
void CBuildPlain::MakeWrittenPath(const char* szPath, CChars* pszDest)
{
	CFileUtil	cFileUtil;
	CChars		szCurrent;
	CChars		szFull;

	szCurrent.Init();
	cFileUtil.CurrentDirectory(&szCurrent);
	szFull.Init(szPath);
	cFileUtil.FullPath(&szFull);
	MakeRelativePath(szCurrent.Text(), szFull.Text(), pszDest);
	if (pszDest->Empty())
	{
		pszDest->Set(".");
	}
	szFull.Kill();
	szCurrent.Kill();
}


char* CBuildPlain::GetOutput(void) { return mszOutput.Text(); }
char* CBuildPlain::GetListings(void) { return mszListings.Text(); }
EBuildToolchain CBuildPlain::GetToolchain(void) { return meToolchain; }
CHCP65BuildDescription* CBuildPlain::GetDescription(void) { return &mcDescription; }

