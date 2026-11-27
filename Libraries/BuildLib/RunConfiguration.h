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
#ifndef __RUN_CONFIGURATION_H__
#define __RUN_CONFIGURATION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayUint32.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"


//A named way to run what the workspace builds, as IntelliJ IDEA has them: the build configuration it builds and is
//debugged as ("Debug|HCP65C816"), and the external tool it starts: the executable, the folder it runs in and arguments
//to give it (split at spaces).  Paths may be relative to the workspace's file.  Without a tool, one that builds for the
//W65C816 starts the default emulator, with its folder and arguments if it has them, and one that builds with GCC
//starts what it linked of the startup solution's startup project, in the project's folder.
//
//One that debugs an HCP65 program also says how (ConfigurationsPlan.txt C8): the card it runs on, a card file
//(CBuildCard) which lists the programs on it; the files the emulator is given first (<Load File Address/>, the boot
//loader's ROM); and, only to override what is worked out from what is debugged, the breakpoint space.
//
//	<RunConfiguration Name="ModPlay" Configuration="Release|HCP65C816" Card="..\Cards\BootCard.hcp65card">
//		<Load File="..\Boot\Output\HCP65C816\Release\Boot.rom" Address="0x0"/>
//	</RunConfiguration>
//
//A private one is kept in the workspace file (CBuildWorkspace), its paths relative to that file.  A shared one is a
//file of its own, <Name>.hcp65run in the .run folder beside a solution, committed as IntelliJ IDEA's "Store as
//project file" are: GetShared is that solution's folder, which its paths are relative to.
class CRunConfiguration
{
protected:
	CChars			mszName;
	CChars			mszConfiguration;
	CChars			mszTool;
	CChars			mszDirectory;
	CChars			mszArguments;
	CChars			mszCard;
	CArrayChars		maszLoadFiles;
	CArrayUint32	mauiLoadAddresses;
	CChars			mszBreakpointSpace;
	CChars			mszShared;

public:
	void	Init(void);
	void	Init(const char* szName, const char* szConfiguration);
	void	Init(CRunConfiguration* pcSource);
	void	Kill(void);

	void	SaveXML(CXMLWriter* pcWriter);
	bool	LoadXML(CMarkupTag* pcTag);

	char*	GetName(void);
	char*	GetConfiguration(void);
	char*	GetTool(void);
	char*	GetDirectory(void);
	char*	GetArguments(void);
	bool	HasTool(void) const;
	bool	IsDefault(void);
	char*	GetCard(void);
	size	NumLoads(void) const;
	char*	GetLoadFile(size uiIndex);
	uint32	GetLoadAddress(size uiIndex);
	char*	GetBreakpointSpace(void);
	bool	HasDebugging(void) const;
	bool	IsShared(void) const;
	char*	GetShared(void);
	void	GetSharedFile(CChars* pszDest);

	void	SetName(const char* szName);
	void	SetConfiguration(const char* szConfiguration);
	void	SetTool(const char* szTool, const char* szDirectory, const char* szArguments);
	void	SetCard(const char* szCard);
	void	ClearLoads(void);
	void	AddLoad(const char* szFile, uint32 uiAddress);
	void	SetBreakpointSpace(const char* szSpace);
	void	SetShared(const char* szDirectory);
};


typedef CArrayTemplate<CRunConfiguration>	CArrayRunConfigurations;


#define RUN_CONFIGURATION_TAG		"RunConfiguration"
#define RUN_CONFIGURATION_FOLDER	".run"
#define RUN_CONFIGURATION_EXTENSION	".hcp65run"


#endif // __RUN_CONFIGURATION_H__

