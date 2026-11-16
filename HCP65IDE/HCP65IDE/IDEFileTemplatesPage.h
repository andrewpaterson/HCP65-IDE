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
#ifndef __IDE_FILE_TEMPLATES_PAGE_H__
#define __IDE_FILE_TEMPLATES_PAGE_H__
#include "BaseLib/ArrayChars.h"
#include "WindowLib/Button.h"
#include "WindowLib/CodeEditor.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/ListContainer.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/SettingsDialog.h"
#include "WindowLib/TabbedContainer.h"
#include "WindowLib/TextField.h"
#include "IDEFileTemplates.h"


class CIDEContent;


//Settings' File and Code Templates, as IntelliJ IDEA's: the scheme (Default, the user's for every workspace, or
//Workspace, kept with the workspace's code); the Files, Includes and Code tabs; each tab's templates (one changed in
//the scheme in the accent colour, a child under its parent); Add, Remove, Copy and Reset; the one chosen's name,
//extension and file name, and its text in a code editor that colours ${...} and the directives, with its error
//under it; the variables it can use; and the Author ${AUTHOR} is (empty: git's user.name).  It changes a copy of the
//templates, which OK (Keep) makes the IDE's and writes to the schemes' directories.
class CIDEFileTemplatesPage : public CSettingsPage
{
CONSTRUCTABLE(CIDEFileTemplatesPage);
DESTRUCTABLE(CIDEFileTemplatesPage);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>				mpDialog;
	CIDEFileTemplates			mcTemplates;  //The copy changed until OK.
	EIDETemplateScheme			meScheme;
	EIDETemplateTab				meTab;
	CArrayChars					maszRows;  //The name of each row of the list.
	bool						mbUpdating;  //The page is being filled, not changed by the user.
	CChars						mszKeepProblem;  //Why OK couldn't keep the templates, or empty.
	Ptr<CComponent>				mpBody;
	Ptr<CComboBox>				mpScheme;
	Ptr<CTextField>				mpAuthor;
	Ptr<CTabbedContainer>		mpTabs;
	Ptr<CListContainer>			mpList;
	Ptr<CScrollPane>			mpScrollPane;
	Ptr<CButton>				mpAdd;
	Ptr<CButton>				mpRemove;
	Ptr<CButton>				mpCopy;
	Ptr<CButton>				mpReset;
	Ptr<CTextField>				mpName;
	Ptr<CTextField>				mpExtension;
	Ptr<CTextField>				mpFileName;
	Ptr<CCodeEditor>			mpEditor;
	Ptr<CLabel>					mpError;
	Ptr<CLabel>					mpVariables;
	Ptr<CLabel>					mpStatus;

public:
	void			Init(CIDEContent* pcContent, Ptr<CDialog> pDialog);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	const char*		GetTitle(void) override;
	Ptr<CComponent>	GetBody(void) override;
	Ptr<CComponent>	GetFirstEditor(void) override;
	void			Keep(void) override;

	void			ApplyEdits(const char* szEdits);
	char*			GetKeepProblem(void);

protected:
	Ptr<CComponent>		CreateBody(void);
	Ptr<CButton>		CreateButton(Ptr<CGridContainer> pButtons, int32 iColumn, const char* szCaption);
	Ptr<CLabel>			CreateLabel(const char* szText);
	void				ShowList(const char* szSelect, bool bTemplate = true);
	void				ShowTemplate(void);
	void				ShowErrorAndColours(void);
	SIDEFileTemplate*	GetSelected(void);
	char*				GetSelectedName(void);
	bool				IsChanged(SIDEFileTemplate* psTemplate);
	bool				HasUnder(SIDEFileTemplate* psTemplate);
	SIDEFileTemplate*	TakeSelected(void);
	void				MakeUniqueName(const char* szName, CChars* pszDest);
	void				AppendVariables(SIDEFileTemplate* psTemplate, CChars* pszDest);
	void				SelectName(const char* szName);

	void				SchemeChanged(void);
	void				TabChanged(void);
	void				RowSelected(void);
	void				TextChanged(void);
	void				NameChanged(void);
	void				ExtensionChanged(void);
	void				FileNameChanged(void);
	void				AddClicked(CButton* pcButton);
	void				RemoveClicked(CButton* pcButton);
	void				CopyClicked(CButton* pcButton);
	void				ResetClicked(CButton* pcButton);
};


#endif // __IDE_FILE_TEMPLATES_PAGE_H__
