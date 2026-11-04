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
#ifndef __IDE_EDITOR_TAB_H__
#define __IDE_EDITOR_TAB_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/TabComponent.h"
#include "CppParserLib/CPPHighlighter.h"


//A file open in the editor's tabs: its text in a fixed width font, scrolled in its page, with what is known of it for
//highlighting it again as it is edited.
class CIDEEditorTab : public CObject
{
CONSTRUCTABLE(CIDEEditorTab);
DESTRUCTABLE(CIDEEditorTab);
protected:
	Ptr<CWindow>		mpWindow;
	Ptr<CTextArea>		mpEditor;
	Ptr<CScrollPane>	mpPane;			//The tab's page.
	Ptr<CTabComponent>	mpTab;
	CChars				mszFileName;
	size				muiProject;		//The solution's project it is in, MAX_SIZE if none.
	size				muiSeenChanges;	//The editor's change count when it was last looked at.
	int64				miChangedTime;	//When the text last changed (GetDebugTickCount).
	bool				mbRecolour;		//The text changed and hasn't been highlighted since.
	bool				mbEdited;		//The text was changed since the file was opened.

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFontFamily> pFamily, ARGB32 sColour, const char* szFileName, size uiProject, const char* szText);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				SetTab(Ptr<CTabComponent> pTab);
	void				SetHighlights(CArrayHighlights* pacHighlights);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				SetCaretPlace(int iLine, int iColumn);
	bool				NeedsRecolour(int64 iNow, int64* piWait);

	Ptr<CTextArea>		GetEditor(void);
	Ptr<CScrollPane>	GetPane(void);
	Ptr<CTabComponent>	GetTab(void);
	char*				GetFileName(void);
	size				GetProject(void);
	bool				IsEdited(void);
};


#endif // __IDE_EDITOR_TAB_H__

