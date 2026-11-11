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
#ifndef __RUN_CONFIGURATION_H__
#define __RUN_CONFIGURATION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/MarkupTag.h"


//A named way to run what the workspace builds, as IntelliJ IDEA has them: the build configuration it builds and is
//debugged as ("Debug|W65C816"), and the external tool it starts: the executable, the folder it runs in and arguments
//to give it (split at spaces).  Paths may be relative to the workspace's file.  Without a tool, one that builds for the
//W65C816 starts the default emulator, with its folder and arguments if it has them, and one that builds with GCC
//starts what it linked of the startup solution's startup project, in the project's folder.  Kept in the workspace file
//(CBuildWorkspace).
class CRunConfiguration
{
protected:
	CChars	mszName;
	CChars	mszConfiguration;
	CChars	mszTool;
	CChars	mszDirectory;
	CChars	mszArguments;

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
	bool	HasTool(void);
	bool	IsDefault(void);

	void	SetName(const char* szName);
	void	SetConfiguration(const char* szConfiguration);
	void	SetTool(const char* szTool, const char* szDirectory, const char* szArguments);
};


typedef CArrayTemplate<CRunConfiguration>	CArrayRunConfigurations;


#define RUN_CONFIGURATION_TAG	"RunConfiguration"


#endif // __RUN_CONFIGURATION_H__

