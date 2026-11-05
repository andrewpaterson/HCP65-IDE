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
#include <stdio.h>
#include <string.h>
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "EmulatorDebugLib/DebugFormat.h"
#include "IDEDebugView.h"


#define IDE_DEBUG_REGISTERS_PER_ROW		3
#define IDE_DEBUG_REGISTERS_WIDTH		420
#define IDE_DEBUG_BYTES_SHOWN			4		//An instruction's bytes shown; more are left out.
#define IDE_DEBUG_PROGRAM_COLOUR		Set32BitColour((uint8)0xFF, (uint8)0xE0, (uint8)0x60)	//The instruction at the program address.
#define IDE_DEBUG_BREAKPOINT_COLOUR		Set32BitColour((uint8)0xF0, (uint8)0x70, (uint8)0x70)	//An instruction with a breakpoint.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static Ptr<CTextArea> CreateIDEDebugText(Ptr<CWindow> pWindow, Ptr<CFontFamily> pFamily, ARGB32 sColour)
{
	Ptr<CTextArea>	pText;

	pText = OMalloc<CTextArea>(pWindow, pFamily, "");
	pText->SetEditable(false);
	pText->SetTiledText(true);
	pText->SetColour(sColour);
	pText->SetAlignment(CSH_Left, CSV_Top);
	return pText;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFontFamily> pFamily, ARGB32 sColour)
{
	PreInit();

	msColour = sColour;
	mauiLineAddresses.Init();

	mpRegisters = CreateIDEDebugText(pWindow, pFamily, sColour);
	mpRegistersPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpRegistersPane->SetContent(mpRegisters);
	mpRegistersPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpDisassembly = CreateIDEDebugText(pWindow, pFamily, sColour);
	mpDisassemblyPane = OMalloc<CScrollPane>(pWindow, pTheme);
	mpDisassemblyPane->SetContent(mpDisassembly);
	mpDisassemblyPane->SetAlignment(CSH_Stretch, CSV_Stretch);

	mpRegistersPane->SetDesiredSize(IDE_DEBUG_REGISTERS_WIDTH, -1);
	mpPanel = OMalloc<CGridContainer>(pWindow);
	mpPanel->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpPanel->AddComponent(mpDisassemblyPane, 0, 0, 1, 1, 1, 1);
	mpPanel->AddComponent(mpRegistersPane, 1, 0, 1, 1, 0, 1);

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::Class(void)
{
	M_Pointer(mpPanel);
	M_Pointer(mpRegisters);
	M_Pointer(mpRegistersPane);
	M_Pointer(mpDisassembly);
	M_Pointer(mpDisassemblyPane);
	U_Unknown(CArrayIDEAddresses, mauiLineAddresses);
	U_Data(ARGB32, msColour);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::Free(void)
{
	mauiLineAddresses.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugView::Save(CObjectWriter* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugView::Load(CObjectReader* pcFile)
{
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// With bKeepCaret the disassembly's caret stays on the address it was on
// (a breakpoint was toggled there); otherwise it goes to the program
// address.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::Update(CIDEDebugger* pcDebugger, bool bKeepCaret)
{
	uint32	uiCaret;
	bool	bCaret;

	bCaret = bKeepCaret && GetCaretAddress(&uiCaret);
	UpdateRegisters(pcDebugger);
	UpdateDisassembly(pcDebugger);
	if (bCaret)
	{
		SetCaretAddress(uiCaret);
	}
	else if (pcDebugger->IsStopped())
	{
		SetCaretAddress(pcDebugger->GetProgramAddress());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The state, then the registers at the last stop as the emulator
// describes them (a flags register's letters too), then the breakpoints.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::UpdateRegisters(CIDEDebugger* pcDebugger)
{
	CChars			sz;
	CDebugClient*	pcClient;
	SDebugRegister*	psRegister;
	size			i;
	size			uiNameLength;
	EIDEDebugState	eState;

	sz.Init(pcDebugger->GetStatus());
	sz.AppendNewLine();

	pcClient = pcDebugger->GetClient();
	eState = pcDebugger->GetState();
	if ((pcClient != NULL) && (eState != IDS_Connecting) && (eState != IDS_Starting))
	{
		sz.Append(pcClient->GetProcessorName());
		sz.Append(pcDebugger->IsStopped() ? "" : " (the values at the last stop)");
		sz.AppendNewLine();
		for (i = 0; i < pcClient->NumRegisters(); i++)
		{
			psRegister = pcClient->GetRegister(i);
			uiNameLength = strlen(psRegister->szName);
			sz.Append(psRegister->szName);
			sz.Append(' ', (uiNameLength < 3) ? 3 - uiNameLength : 0);
			sz.Append("= ");
			AppendDebugRegisterValue(&sz, psRegister, pcClient->GetRegisterValue(i));
			if (((i + 1) % IDE_DEBUG_REGISTERS_PER_ROW == 0) || (i + 1 == pcClient->NumRegisters()))
			{
				sz.AppendNewLine();
			}
			else
			{
				sz.Append("   ");
			}
		}
	}

	sz.Append("Breakpoints:");
	if (pcDebugger->NumBreakpoints() == 0)
	{
		sz.Append(" none");
	}
	for (i = 0; i < pcDebugger->NumBreakpoints(); i++)
	{
		sz.Append(' ');
		pcDebugger->AppendAddress(&sz, pcDebugger->GetBreakpoint(i)->uiAddress);
	}

	mpRegisters->SetText(sz.Text());
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The instructions the emulator last decoded, a line each:
// "=> * $00:8002  E8           inx".
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::UpdateDisassembly(CIDEDebugger* pcDebugger)
{
	CDebugClient*		pcClient;
	SDebugInstruction*	psInstruction;
	uint32				uiProgram;
	bool				bAtProgram;
	bool				bBreakpoint;
	ARGB32				sColour;
	size				i;
	size				j;
	char				szByte[8];
	CChars				sz;

	mauiLineAddresses.Kill();
	mauiLineAddresses.Init();
	mpDisassembly->SetText("");

	pcClient = pcDebugger->GetClient();
	if ((pcClient == NULL) || (pcDebugger->GetState() == IDS_Connecting))
	{
		return;
	}
	if (!pcClient->CanDecode())
	{
		mpDisassembly->SetText("The emulator can't disassemble its processor's instructions.");
		return;
	}

	uiProgram = pcDebugger->GetProgramAddress();
	sz.Init();
	for (i = 0; i < pcClient->NumInstructions(); i++)
	{
		psInstruction = pcClient->GetInstruction(i);
		mauiLineAddresses.Add(&psInstruction->uiAddress);
		bAtProgram = (psInstruction->uiAddress == uiProgram) && pcDebugger->IsStopped();
		bBreakpoint = pcDebugger->FindBreakpoint(psInstruction->uiAddress) != NULL;

		sz.Set(bAtProgram ? "=> " : "   ");
		sz.Append(bBreakpoint ? "* " : "  ");
		pcClient->AppendAddress(&sz, psInstruction->uiAddress);
		sz.Append("  ");
		for (j = 0; j < IDE_DEBUG_BYTES_SHOWN; j++)
		{
			if (j < psInstruction->uiLength)
			{
				sprintf(szByte, "%02X ", psInstruction->auiBytes[j]);
				sz.Append(szByte);
			}
			else
			{
				sz.Append("   ");
			}
		}
		sz.Append((psInstruction->uiLength > IDE_DEBUG_BYTES_SHOWN) ? "+ " : "  ");
		sz.Append(psInstruction->szText);
		if (i + 1 < pcClient->NumInstructions())
		{
			sz.AppendNewLine();
		}

		sColour = bAtProgram ? IDE_DEBUG_PROGRAM_COLOUR : (bBreakpoint ? IDE_DEBUG_BREAKPOINT_COLOUR : msColour);
		mpDisassembly->AppendText(sz.Text(), sColour);
	}
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The address of the disassembly line the caret is on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugView::GetCaretAddress(uint32* puiAddress)
{
	size	uiLine;

	uiLine = mpDisassembly->FindLine(mpDisassembly->GetCaretPosition());
	if (uiLine >= mauiLineAddresses.NumElements())
	{
		return false;
	}
	*puiAddress = *mauiLineAddresses.Get(uiLine);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The caret to the start of the line at uiAddress, if it is shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDebugView::SetCaretAddress(uint32 uiAddress)
{
	size	i;

	for (i = 0; i < mauiLineAddresses.NumElements(); i++)
	{
		if (*mauiLineAddresses.Get(i) == uiAddress)
		{
			mpDisassembly->SetCaretPosition(mpDisassembly->GetLineStart(i));
			return true;
		}
	}
	return false;
}


Ptr<CComponent> CIDEDebugView::GetComponent(void) { return mpPanel; }
Ptr<CTextArea> CIDEDebugView::GetDisassembly(void) { return mpDisassembly; }
char* CIDEDebugView::GetRegistersText(void) { return mpRegisters->GetText(); }
char* CIDEDebugView::GetDisassemblyText(void) { return mpDisassembly->GetText(); }
