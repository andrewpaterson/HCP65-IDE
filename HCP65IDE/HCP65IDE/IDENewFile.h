/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __IDE_NEW_FILE_H__
#define __IDE_NEW_FILE_H__
#include "BaseLib/ArrayChars.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/MenuContainer.h"
#include "VisualStudioLib/VcxProject.h"
#include "TemplateLib/TemplateValues.h"


class CIDEContent;


//The most variables with no value New's dialog asks for.
#define IDE_NEW_MAXIMUM_FIELDS	16


//Where New puts its files: a project, the directory they go in, and the filter they are in (empty for none).
struct SIDENewPlace
{
	CVcxProject*	pcProject;
	size			uiProject;		//The workspace's index of it, for opening the files.
	CChars			szDirectory;	//Ends with a separator.
	CChars			szFilter;
	CChars			szListBeside;	//A file of the project in the directory, whose CMakeLists.txt lines the new files get; or empty.

	void	Init(void);
	void	Kill(void);
};


//New, as IntelliJ IDEA's (Alt+Insert, and File, New): a Files template's files made in the project, filter or
//directory selected in Solution Explorer (or the active file's).  The dialog asks the name (C++ Class's is the class's,
//"CKeg", and its files are named by its stem, "Keg") and a field for each variable the template uses that has no
//value.  The files are made, added to the project and its filters and to the CMakeLists.txt that list the files
//beside them, staged in git and opened; one undo takes them away.  A name a file of the solution has already is
//refused.  Save File as Template (File) makes a Files template of the file in the editor.
class CIDENewFile : public CObject
{
CONSTRUCTABLE(CIDENewFile);
DESTRUCTABLE(CIDENewFile);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CMenuContainer>		mpFileMenu;  //File, New: filled as it opens.
	Ptr<CMenuContainer>		mpPopupMenu;  //Alt+Insert's.
	CArrayChars				maszTemplates;  //The Files templates the menus show, by item.

public:
	void				Init(CIDEContent* pcContent);
	void				Class(void) override;
	void				Free(void) override;
	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				AddCommands(Ptr<CCommandSet> pCommands);
	Ptr<CMenuContainer>	GetFileMenu(void);
	Ptr<CMenuContainer>	GetPopupMenu(void);
	bool				CanCreate(void);
	void				OpenPopupCommand(void);
	bool				ShowNewDialog(const char* szTemplate);
	bool				Create(const char* szTemplate, const char* szName, CTemplateValues* pcGiven, CChars* pszStatus);
	bool				FindPlace(SIDENewPlace* psDest, const char** pszWhyNot);
	bool				CanSaveAsTemplate(void);
	bool				SaveAsTemplateCommand(void);
	bool				SaveAsTemplate(const char* szFileName, const char* szText, const char* szName, CChars* pszStatus);
	void				MakeTemplateText(const char* szText, const char* szStem, CChars* pszDest, bool* pbClass);
	void				FindTemplateFiles(const char* szTemplate, CArrayChars* paszDest);

protected:
	void				FillMenu(Ptr<CMenuContainer> pMenu, bool bRefactorings);
	void				FileMenuOpening(CMenuContainer* pcMenu);
	void				PopupMenuOpening(CMenuContainer* pcMenu);
	void				ItemClicked(CMenuItem* pcItem);
	void				SetNameValues(const char* szName, CTemplateValues* pcDest);
	bool				EvaluateFileName(const char* szTemplate, CTemplateValues* pcValues, CChars* pszDest);
	bool				IsNamedInSolution(const char* szFileName);
};


#endif // __IDE_NEW_FILE_H__
