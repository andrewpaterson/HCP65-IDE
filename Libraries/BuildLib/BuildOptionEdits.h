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
#ifndef __BUILD_OPTION_EDITS_H__
#define __BUILD_OPTION_EDITS_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/Chars.h"
#include "VisualStudioLib/VcxSolution.h"
#include "BuildConfiguration.h"


//Where an option's value is from, the most particular first: a line of the project's own for one configuration
//("'$(Configuration)|$(Platform)'=='Release|HCP65C816'"), one for every configuration of the platform
//("'$(Platform)'=='HCP65C816'"), one of the project's with another condition, a property sheet the project imports,
//the program (what the startup project says is what a project that says nothing of its own code is compiled with),
//or nothing: the option's default.  An option whose lines add up (SBuildOption::bAdds) may have a line in more than
//one group of the sheets: the last read is BOP_Sheet's, the one before it BOP_Sheet2's and the one before that
//BOP_Sheet3's.
enum EBuildOptionPlace
{
	BOP_Configuration,
	BOP_Platform,
	BOP_Project,
	BOP_Sheet,
	BOP_Sheet2,
	BOP_Sheet3,
	BOP_Program,
	BOP_Default,
	BOP_NumPlaces
};


//The toolchain an option is of: the dialog shows a configuration the options of its own.
enum EBuildOptionKind
{
	BOK_HCP65,
	BOK_GCC,
	BOK_MSVC,
	BOK_NumKinds
};


//An option a project says for a toolchain.  The HCP65 compiler's and linker's are in a project's HCP65Compile or
//HCP65Link (BuildConfigurationProject.cpp); g++'s are the ClCompile and Link settings a GCC build reads
//(CGCCBuild: the defines, the options, and what a program is linked with); MSVC's are the ClCompile and Link
//settings the projects and their sheets set (ConfigurationsPlan.txt C9, stage 2's survey), each with the values
//Visual Studio's property pages give it.
//One of the whole program's is read from the startup project alone; the others are each project's own.
//
//An HCP65 option has one value, the most particular place's that says it.  One of g++'s adds up (bAdds): each place
//has a line of its own, and a line that names the setting itself ("-g %(AdditionalOptions)") adds to what the less
//particular say, so the option's value is all of them (CBuildOptionEdits::GetEffective).  The value and the place
//the dialog shows are then one place's line, and choosing another place shows that one's, moving nothing.  It starts
//on the project's own, so what is typed is the project's and a sheet's line is changed only by choosing it.
struct SBuildOption
{
	const char*			szTool;
	const char*			szName;
	const char*			szLabel;
	bool				bProgram;
	bool				bBoolean;		//"true" or "false"; text if not.
	const char*			szDefault;
	EBuildOptionKind	eKind;
	bool				bAdds;
	char				cSeparator;		//Between the parts of one that adds up: ';' or ' '.
	const char*			szChoices;		//The values it may have, separated by semicolons; NULL if it is typed.  It may also
										//have none: nothing says it.
	const char*			szSection;		//The section of its page it is in ("Optimisation"); NULL for the page's own two.
};


size				NumBuildOptions(void);
const SBuildOption*	GetBuildOption(size uiIndex);
size				FindBuildOption(const char* szName, EBuildOptionKind eKind = BOK_NumKinds);
void				GetBuildOptionKey(size uiIndex, CChars* pszDest);
bool				GetBuildOptionKind(const char* szPlatform, EBuildOptionKind* peKind);


//One option of one project as one solution configuration builds it: what each place says in the files, and its value
//and the place that is from, as read and as edited.
class CBuildOptionEdit
{
public:
	bool				mabSet[BOP_NumPlaces];
	bool				mabEdited[BOP_NumPlaces];		//Of one that adds up: the place's line was set or taken away.
	CChars				maszValues[BOP_NumPlaces];
	CChars				maszFiles[BOP_NumPlaces];		//The project's or the sheet's.
	CChars				maszConditions[BOP_NumPlaces];	//As the file writes it.
	size				muiSheetProjects;				//How many of the solution's projects import the sheet.
	bool				mbConfigurationRead;			//The project had a line for the one configuration when it was read.
	EBuildOptionPlace	mePlace;
	CChars				mszValue;
	bool				mbEdited;

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<CBuildOptionEdit>	CArrayBuildOptionEdits;


//A project as one solution configuration builds it, with an edit for each option (by the option's index).
class CBuildProjectOptionEdits
{
public:
	CChars					mszName;
	CChars					mszFile;
	CChars					mszGuid;
	CChars					mszRow;				//The project configuration the solution's row names: "Release|Win32".
	CChars					mszConfiguration;	//The name of the configuration its options are read in: "Release".
	CChars					mszPlatform;		//That configuration's platform, which its lines' conditions name: "HCP65C816", and "x64"
												//for a solution's Release|MSVC-x64.
	bool					mbStartup;
	CArrayBuildOptionEdits	masOptions;

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<CBuildProjectOptionEdits>	CArrayBuildProjectOptionEdits;


//A solution configuration ("Release|HCP65C816") and the projects it builds.  It is read from the rows of mszSource:
//itself, or the configuration a new one is a copy of.
class CBuildConfigurationOptionEdits
{
public:
	CChars							mszName;
	CChars							mszSource;
	CChars							mszPlatform;
	CArrayBuildProjectOptionEdits	masProjects;
	size							muiStartup;		//MAX_SIZE if the startup project isn't one of them.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplatePtr<CBuildConfigurationOptionEdits>	CArrayBuildConfigurationOptionEdits;


//The options of a solution's HCP65 and GCC configurations, edited where they are kept: in its projects and the property
//sheets they import (ConfigurationsPlan.txt C9, questions 33 to 35).  Each option of each project has a value and
//the place it is from.  SetValue changes the value where it is from, so a sheet's changes every project that takes
//it; SetPlace makes it the project's own, for one configuration or the platform, or takes its own lines away again so
//a sheet's, the program's or the default is its value.  Nothing is written until Apply, which sets each edited
//setting in its file in place (SetVcxSetting).
//
//It has a configuration for each one the solution is to have, whatever its toolchain: Apply first makes the solution's
//rows say so.  One added under a new name with another's as its source is a copy of it, one whose name is changed is
//renamed, and a solution configuration that none is for is taken away (question 35); the projects are left alone.
//
//A solution configuration builds a project as one of the project's own configurations, which may be called something
//else (Fast|HCP65C816 built as the project's Release).  A line for one configuration is then that other one's: so an
//edit that is one configuration's first gives the project a configuration of the solution configuration's name, a
//copy of the one it was built as, and the solution's row names it (NeedsOwnConfiguration, GetNote).
//
//What an edit changes for another configuration of the project, or another project with the same sheet, shows there
//at once.
class CBuildOptionEdits
{
protected:
	CVcxSolution*						mpcSolution;	//Not held.
	CChars								mszStartupProject;
	CArrayBuildConfigurationOptionEdits	mapcConfigurations;

public:
	void		Init(CVcxSolution* pcSolution, const char* szStartupProject);
	void		Kill(void);

	size		Add(const char* szName, const char* szSource);
	void		Remove(size uiConfiguration);
	void		SetName(size uiConfiguration, const char* szName);
	size		NumConfigurations(void) const;
	CBuildConfigurationOptionEdits*	GetConfiguration(size uiConfiguration);

	size		NumProjects(size uiConfiguration);
	CBuildProjectOptionEdits*	GetProject(size uiConfiguration, size uiProject);
	size		GetStartupProject(size uiConfiguration);

	const char*			GetValue(size uiConfiguration, size uiProject, size uiOption);
	EBuildOptionPlace	GetPlace(size uiConfiguration, size uiProject, size uiOption);
	size				GetPlaces(size uiConfiguration, size uiProject, size uiOption, EBuildOptionPlace* paePlaces);
	void				GetPlaceText(size uiConfiguration, size uiProject, size uiOption, EBuildOptionPlace ePlace, CChars* pszDest);
	void				SetValue(size uiConfiguration, size uiProject, size uiOption, const char* szValue);
	void				SetPlace(size uiConfiguration, size uiProject, size uiOption, EBuildOptionPlace ePlace);
	void				GetEffective(size uiConfiguration, size uiProject, size uiOption, CChars* pszDest);
	bool				IsSet(size uiConfiguration, size uiProject, size uiOption);

	bool		IsEdited(void);
	bool		NeedsOwnConfiguration(size uiConfiguration, size uiProject);
	void		GetNote(size uiConfiguration, CChars* pszDest);
	bool		Check(size uiConfiguration, CChars* pszError);
	bool		Apply(const char* szSolutionFile, CChars* pszNote);

protected:
	void		ReadProject(CBuildConfigurationOptionEdits* pcConfiguration, CVcxSolutionProject* pcSolutionProject, CVcxSolutionProjectConfiguration* pcRow, EBuildToolchain eToolchain);
	void		ReadOption(CBuildConfigurationOptionEdits* pcConfiguration, CBuildProjectOptionEdits* pcProject, CVcxConfiguration* pcProjectConfiguration, size uiOption);
	CBuildOptionEdit*	GetEdit(size uiConfiguration, size uiProject, size uiOption);
	EBuildOptionPlace	GetBasePlace(CBuildConfigurationOptionEdits* pcConfiguration, CBuildProjectOptionEdits* pcProject, CBuildOptionEdit* pcEdit, size uiOption);
	void		SetWhereSet(CBuildConfigurationOptionEdits* pcConfiguration, CBuildProjectOptionEdits* pcProject, CBuildOptionEdit* pcEdit, size uiOption);
	void		Changed(CBuildOptionEdit* pcOrigin, size uiOption, EBuildOptionPlace ePlace, bool bSet, const char* szValue);
	size		CountSheetProjects(const char* szSheet);
	void		SetAddedValue(size uiConfiguration, size uiProject, size uiOption, const char* szValue);
	void		GetOwnCondition(CBuildConfigurationOptionEdits* pcConfiguration, const char* szConfiguration, CChars* pszDest);
	bool		ApplyConfigurations(const char* szSolutionFile);
	bool		ApplyProject(const char* szSolutionFile, size uiConfiguration, size uiProject, CChars* pszNote);
};


#endif // __BUILD_OPTION_EDITS_H__

