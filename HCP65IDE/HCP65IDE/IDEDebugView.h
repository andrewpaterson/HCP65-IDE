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
#ifndef __IDE_DEBUG_VIEW_H__
#define __IDE_DEBUG_VIEW_H__
#include "BaseLib/ArrayTemplate.h"
#include "StandardLib/Object.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/GridContainer.h"
#include "IDEDebugger.h"


typedef CArrayTemplate<uint32>	CArrayIDEAddresses;


//The debugger's tab under the editor: the disassembly (the instructions from the program address, "=>" at it and "*"
//at a breakpoint) beside the registers (the state, the registers at the last stop and the breakpoints).  F9 in the
//disassembly toggles a breakpoint at the caret's instruction.  Update shows the debugger as it is now.
class CIDEDebugView : public CObject
{
CONSTRUCTABLE(CIDEDebugView);
DESTRUCTABLE(CIDEDebugView);
protected:
	Ptr<CGridContainer>	mpPanel;
	Ptr<CTextArea>		mpRegisters;
	Ptr<CScrollPane>	mpRegistersPane;
	Ptr<CTextArea>		mpDisassembly;
	Ptr<CScrollPane>	mpDisassemblyPane;
	CArrayIDEAddresses	mauiLineAddresses;	//Each disassembly line's address.
	ARGB32				msColour;

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFontFamily> pFamily, ARGB32 sColour);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				Update(CIDEDebugger* pcDebugger, bool bKeepCaret);
	bool				GetCaretAddress(uint32* puiAddress);
	bool				SetCaretAddress(uint32 uiAddress);

	Ptr<CComponent>		GetComponent(void);
	Ptr<CTextArea>		GetDisassembly(void);
	char*				GetRegistersText(void);
	char*				GetDisassemblyText(void);

protected:
	void				UpdateRegisters(CIDEDebugger* pcDebugger);
	void				UpdateDisassembly(CIDEDebugger* pcDebugger);
	void				Disassemble(CIDEDebugger* pcDebugger, uint8* puiMemory, uint32 uiMemoryAddress, size uiMemorySize, uint32 uiStart, CArrayIDEAddresses* pauiAddresses, CArrayChars* paszLines);
	size				FindRegister(CDebugClient* pcClient, const char* szName);
};


#endif // __IDE_DEBUG_VIEW_H__
