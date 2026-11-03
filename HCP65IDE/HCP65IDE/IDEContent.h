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
#ifndef __IDE_CONTENT_H__
#define __IDE_CONTENT_H__
#include "SupportLib/FontColours.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/ListListener.h"
#include "VisualStudioLib/VcxProject.h"
#include "SolutionExplorer.h"


class CIDEContent;


//Hears Solution Explorer's files being opened (a double click or enter).
class CSolutionExplorerListener : public CListListener
{
CONSTRUCTABLE(CSolutionExplorerListener);
public:
	CIDEContent*	mpcContent;

	void	Init(CIDEContent* pcContent);
	void	ItemActivated(CUnknown* pcSource, void* pvContext) override;
};


//The IDE's window: Solution Explorer on the left and the file being edited on the right, in a fixed width font.
class CIDEContent
{
protected:
	Ptr<CWindow>				mpWindow;
	Ptr<CFont>					mpWhite;
	Ptr<CFontColours>			mpFontColours;
	Ptr<CWindowTheme>			mpTheme;
	Ptr<CFontFamily>			mpCodeFamily;
	Ptr<CTextArea>				mpEditor;
	CSolutionExplorer			mcExplorer;
	CSolutionExplorerListener	mcExplorerListener;
	CVcxProject					mcProject;
	bool						mbProject;

public:
	bool			Init(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont);
	void			Kill(void);

	bool			OpenProject(const char* szProjectFile);
	bool			OpenFile(const char* szFileName);
	void			ExplorerActivated(void);

	Ptr<CTextArea>		GetEditor(void);
	CSolutionExplorer*	GetExplorer(void);
};


Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder);


#endif // __IDE_CONTENT_H__

