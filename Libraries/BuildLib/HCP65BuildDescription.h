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
#ifndef __H_C_P_65_BUILD_DESCRIPTION_H__
#define __H_C_P_65_BUILD_DESCRIPTION_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/ArrayTemplate.h"


//Include directories in the order they are added to the compiler, each with or without its sub directories.
class CHCP65BuildIncludes
{
protected:
	CArrayChars	maszDirectories;
	CArrayInt	maiSubDirectories;

public:
	void	Init(void);
	void	Kill(void);

	void	Add(const char* szDirectory, bool bSubDirectories);
	size	NumIncludes(void);
	char*	GetDirectory(size uiIndex);
	bool	HasSubDirectories(size uiIndex);
};


//A project of an HCP65 build: its include directories (added just before its units), its sources (relative to its
//directory, a full path) as a library of the compiler (CCPP65Builder::AddProjectSources), and its defines.  A library
//project's units are linked only when something uses them; a program's always are.  Its line in the build's output
//is "Name: Label" (the project's configuration, from a solution).  A solution's project that couldn't be read has no
//sources and its project file in mszUnreadable, so the build says so.
class CHCP65BuildProject
{
public:
	CChars				mszName;
	CChars				mszDirectory;
	CChars				mszLabel;
	CChars				mszUnreadable;
	CHCP65BuildIncludes	mcIncludes;
	CArrayChars			maszSources;
	CArrayChars			maszDefines;
	bool				mbLibrary;

	void	Init(const char* szName, const char* szDirectory, const char* szLabel, bool bLibrary);
	void	Kill(void);
};


typedef CArrayTemplate<CHCP65BuildProject>	CArrayHCP65BuildProjects;


//What an HCP65 build compiles: include directories for every unit, then the projects in order.  CHCP65Build describes
//a solution this way (Describe) and hcp65build's plain command line gives one, so the two add exactly the same to the
//compiler.
class CHCP65BuildDescription
{
public:
	CHCP65BuildIncludes			mcIncludes;
	CArrayHCP65BuildProjects	macProjects;

	void				Init(void);
	void				Kill(void);
	void				Clear(void);

	CHCP65BuildProject*	AddProject(const char* szName, const char* szDirectory, const char* szLabel, bool bLibrary);
	size				NumProjects(void);
	CHCP65BuildProject*	GetProject(size uiIndex);
	CHCP65BuildProject*	GetLastProject(void);
};


#endif // __H_C_P_65_BUILD_DESCRIPTION_H__

