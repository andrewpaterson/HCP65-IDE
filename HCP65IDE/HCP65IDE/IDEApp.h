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
#ifndef __IDE_APP_H__
#define __IDE_APP_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "InputLib/Input.h"
#include "InputLib/NativeInput.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowInputSource.h"
#include "WindowLib/ComponentInput.h"
#include "WindowLib/NativeWindowFactory.h"
#include "IDEContent.h"


//The IDE: its window, the window's content and the input that drives it.  main makes it, adds it to the root, sets it up,
//runs it and removes it from the root.
class CIDEApp : public CObject
{
CONSTRUCTABLE(CIDEApp);
DESTRUCTABLE(CIDEApp);
protected:
	Ptr<CWindow>			mpWindow;
	Ptr<CIDEContent>		mpContent;
	Ptr<CInput>				mpInput;
	Ptr<CWindowInputSource>	mpInputSource;
	CComponentInput			mcComponentInput;
	bool					mbValid;
	bool					mbInputs;  //The component input is initialised.

public:
	void	Init(void);
	bool	Setup(CNativeWindowFactory* pcNativeFactory, CNativeInput* pcNativeInput, const char* szInputDirectory, const char* szSolution);
	void	Class(void) override;
	void	Free(void) override;

	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool	Run(void);
	void	DockScripted(const char* szMove);
};


#endif // __IDE_APP_H__

