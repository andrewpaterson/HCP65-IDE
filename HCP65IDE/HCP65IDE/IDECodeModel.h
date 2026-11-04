/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_CODE_MODEL_H__
#define __IDE_CODE_MODEL_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "CppParserLib/Config.h"
#include "CppParserLib/CPPHighlighter.h"
#include "VisualStudioLib/VcxSolution.h"


//What the IDE knows of the solution's code: each file preprocessed and parsed as the solution's configuration builds
//it, with its project's include directories and defines, as MSVC would see them.
class CIDECodeModel
{
protected:
	CVcxSolution*	mpcSolution;
	CChars			mszConfiguration;		//The solution configuration, "Debug|Win32".
	CArrayChars		maszSystemDirectories;	//Where the compiler's own headers are ($(IncludePath)).

public:
	void	Init(CVcxSolution* pcSolution);
	void	Kill(void);

	void	SetConfiguration(const char* szConfiguration);
	char*	GetConfiguration(void);
	void	AddSystemDirectory(const char* szDirectory);
	void	AddDefaultSystemDirectories(void);

	bool	Highlight(size uiProject, const char* szFileName, CArrayHighlights* pacDest, CChars* pszErrors);

protected:
	CVcxConfiguration*	GetProjectConfiguration(CVcxSolutionProject* pcProject);
	void				AddCompilerDefines(CVcxConfiguration* pcConfiguration, CConfig* pcConfig);
};


#endif // __IDE_CODE_MODEL_H__

