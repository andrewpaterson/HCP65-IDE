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
#ifndef __IDE_CLAUDE_PAGE_H__
#define __IDE_CLAUDE_PAGE_H__
#include "WindowLib/Button.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/FileChooser.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/SettingsDialog.h"
#include "WindowLib/TextField.h"


class CIDEContent;


//Settings' Claude page: the folder Claude Code is started in for the Claude tab's requests (CIDEAskClaude), which is
//where it looks for its CLAUDE.md and keeps its conversations: the startup solution's folder, the repository that
//folder is in, or a folder typed here or chosen with the button beside it.  OK keeps it, and it is written with the
//IDE's settings.
class CIDEClaudePage : public CSettingsPage
{
CONSTRUCTABLE(CIDEClaudePage);
DESTRUCTABLE(CIDEClaudePage);
protected:
	CIDEContent*		mpcContent;  //Not held.
	Ptr<CDialog>		mpDialog;  //The settings dialog's.
	Ptr<CComboBox>		mpDirectory;  //Its items are in EIDEClaudeDirectory's order.
	Ptr<CTextField>		mpFolder;
	Ptr<CButton>		mpBrowse;  //Beside the folder: chooses it.
	Ptr<CFileChooser>	mpChooser;  //While the folder is being chosen.
	Ptr<CGridContainer>	mpBody;

public:
	void			Init(CIDEContent* pcContent, Ptr<CDialog> pDialog);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	const char*		GetTitle(void) const override;
	Ptr<CComponent>	GetBody(void) const override;
	Ptr<CComponent>	GetFirstEditor(void) const override;
	void			Keep(void) override;

	void			ApplyEdits(const char* szEdits);
	void			BrowseClicked(void);
	void			ChooseFolder(void);
	Ptr<CButton>	GetBrowseButton(void) const;

protected:
	Ptr<CLabel>		CreateLabel(const char* szText);
};


#endif // __IDE_CLAUDE_PAGE_H__

