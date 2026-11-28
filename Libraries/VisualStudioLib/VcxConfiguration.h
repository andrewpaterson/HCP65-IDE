/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __VCX_CONFIGURATION_H__
#define __VCX_CONFIGURATION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/MapStringString.h"


//The platforms of the configurations Visual Studio doesn't build: GCC's, and the HCP65 compiler's for its two
//processors.  A project has such a configuration ("Release|GCC-x64") when a group of its own or of a property sheet it
//imports is for one; it isn't among its <ProjectConfigurations>, so Visual Studio never sees it.
#define VCX_PLATFORM_GCC_X64	"GCC-x64"
#define VCX_PLATFORM_HCP65C816	"HCP65C816"
#define VCX_PLATFORM_HCP65C64	"HCP65C64"


//Where a tool's setting was set for a configuration: the file (the project's or a property sheet's, its full path),
//the Condition of the group it is in as the file writes it (empty for none), and the value written there.
class CVcxSettingSource
{
public:
	CChars	mszName;		//"HCP65Compile.NoFloats".
	CChars	mszFile;
	CChars	mszCondition;
	CChars	mszValue;

	void	Init(const char* szName, const char* szFile, const char* szCondition, const char* szValue);
	void	Kill(void);
};


typedef CArrayTemplate<CVcxSettingSource>	CArrayVcxSettingSource;


//A project configuration, "Debug|Win32", and the properties the project sets for it: those of its property groups by
//name ("ConfigurationType", "IncludePath") and those of its item definition groups by tool and name
//("ClCompile.PreprocessorDefinitions", "Link.SubSystem").  A toolchain's configuration ("Debug|GCC-x64") has its own
//item definitions (its defines and options), and what a project says once for Visual Studio's configurations it takes
//from its base, the Visual Studio configuration of the same name it is built like ("Debug|x64"): its include
//directories, and the property groups' properties it hasn't itself.
//
//It keeps where each of its item definition groups' settings was set (its sources), in the order they were read: the
//last of a setting's is where its value is from, the ones before it what that replaced (a sheet's, under a project's).
class CVcxConfiguration
{
protected:
	CChars				mszName;
	CChars				mszConfiguration;
	CChars				mszPlatform;
	CMapStringString	mcProperties;
	CVcxConfiguration*	mpcBase;
	CArrayVcxSettingSource	masSources;

public:
	void		Init(const char* szName);
	void		Kill(void);

	const char*	GetName(void) const;
	const char*	GetConfiguration(void) const;
	const char*	GetPlatform(void) const;
	void		SetProperty(const char* szName, const char* szValue);
	char*		GetProperty(const char* szName);
	char*		GetOwnProperty(const char* szName);
	bool		IsToolchain(void) const;
	void		SetBase(CVcxConfiguration* pcBase);
	size		NumProperties(void);
	void		AddSource(const char* szName, const char* szFile, const char* szCondition, const char* szValue);
	size		NumSources(void) const;
	CVcxSettingSource*	GetSource(size uiIndex);
	void		GetSources(const char* szName, CArrayTemplatePtr<CVcxSettingSource>* papcDest);
};


typedef CArrayTemplate<CVcxConfiguration>	CArrayVcxConfiguration;


//The platform of the Visual Studio configuration a toolchain's is built like (x64 for GCC-x64, Win32 for the HCP65's);
//NULL for a platform that isn't a toolchain's.
const char* GetVcxToolchainBasePlatform(const char* szPlatform);


#endif // __VCX_CONFIGURATION_H__

