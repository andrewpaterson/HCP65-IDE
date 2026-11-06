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
#ifndef __IDE_BUILD_PANE_H__
#define __IDE_BUILD_PANE_H__
#include "BaseLib/ArrayChars.h"
#include "StandardLib/Object.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/ProgressBar.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/ScrollPane.h"


//The Build tab under the editor: what the build wrote, which is kept scrolled to its end as lines are added, over a
//bar showing how far the build has got: blue while it runs, green when it has succeeded and red when it hasn't.
class CIDEBuildPane : public CObject
{
CONSTRUCTABLE(CIDEBuildPane);
DESTRUCTABLE(CIDEBuildPane);
protected:
	Ptr<CGridContainer>	mpPanel;
	Ptr<CTextArea>		mpOutput;
	Ptr<CProgressBar>	mpProgress;
	Ptr<CScrollPane>	mpPane;

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFontFamily> pOutputFamily, ARGB32 sOutputColour);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				Clear(void);
	void				AppendLines(CArrayChars* paszLines);
	void				SetProgress(float fFraction, ARGB32 sColour);

	Ptr<CComponent>		GetComponent(void);
	char*				GetOutput(void);
	Ptr<CProgressBar>	GetProgressBar(void);
};


#endif // __IDE_BUILD_PANE_H__

