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
#include "Assembler65Lib/Asm65816Disassembler.h"
#include "IDEDebugView.h"


#define IDE_DEBUG_REGISTERS_PER_ROW		3
#define IDE_DEBUG_REGISTERS_WIDTH		420
#define IDE_DEBUG_W65C816				"W65C816"
#define IDE_DEBUG_W65C816_FLAGS			"NVMXDIZC"
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
//
//////////////////////////////////////////////////////////////////////////
static void AppendIDEDebugAddress(CChars* psz, uint32 uiAddress)
{
	char	sz[16];

	sprintf(sz, "$%02X:%04X", (uiAddress >> 16) & 0xFF, uiAddress & 0xFFFF);
	psz->Append(sz);
}


//////////////////////////////////////////////////////////////////////////
//
// The state, then the registers at the last stop (a W65C816's P as
// letters too, upper case when set), then the breakpoints.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::UpdateRegisters(CIDEDebugger* pcDebugger)
{
	CChars			sz;
	CDebugClient*	pcClient;
	SDebugRegister*	psRegister;
	uint64			uiValue;
	size			i;
	int				iBit;
	char			szValue[64];
	bool			b65C816;

	sz.Init(pcDebugger->GetStatus());
	sz.AppendNewLine();

	pcClient = pcDebugger->GetClient();
	if ((pcClient != NULL) && (pcDebugger->GetState() != IDS_Connecting) && (pcDebugger->GetState() != IDS_Starting))
	{
		b65C816 = strcmp(pcClient->GetProcessorName(), IDE_DEBUG_W65C816) == 0;
		sz.Append(pcClient->GetProcessorName());
		sz.Append(pcDebugger->IsStopped() ? "" : " (the values at the last stop)");
		sz.AppendNewLine();
		for (i = 0; i < pcClient->NumRegisters(); i++)
		{
			psRegister = pcClient->GetRegister(i);
			uiValue = pcClient->GetRegisterValue(i);
			sprintf(szValue, "%-3s= $%0*llX", psRegister->szName, (int)((psRegister->uiBits + 3) / 4), (unsigned long long)uiValue);
			sz.Append(szValue);
			if ((psRegister->uiKind == DRK_Flags) && b65C816 && (strcmp(psRegister->szName, "P") == 0))
			{
				sz.Append(' ');
				for (iBit = 7; iBit >= 0; iBit--)
				{
					sz.Append((char)(((uiValue >> iBit) & 1) ? IDE_DEBUG_W65C816_FLAGS[7 - iBit] : (IDE_DEBUG_W65C816_FLAGS[7 - iBit] - 'A' + 'a')));
				}
			}
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
		AppendIDEDebugAddress(&sz, pcDebugger->GetBreakpoint(i)->uiAddress);
	}

	mpRegisters->SetText(sz.Text());
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The memory read at the last stop.  If the view's instructions don't
// land on the program address it is disassembled from there instead.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::UpdateDisassembly(CIDEDebugger* pcDebugger)
{
	CArrayChars		aszLines;
	uint8*			puiMemory;
	uint32			uiMemoryAddress;
	size			uiMemorySize;
	uint32			uiProgram;
	size			i;
	bool			bFound;
	uint32			uiAddress;
	ARGB32			sColour;
	CChars			sz;

	mauiLineAddresses.Kill();
	mauiLineAddresses.Init();
	mpDisassembly->SetText("");

	puiMemory = pcDebugger->GetMemory(&uiMemoryAddress, &uiMemorySize);
	if (puiMemory == NULL)
	{
		return;
	}

	uiProgram = pcDebugger->GetProgramAddress();
	aszLines.Init();
	Disassemble(pcDebugger, puiMemory, uiMemoryAddress, uiMemorySize, uiMemoryAddress, &mauiLineAddresses, &aszLines);
	bFound = false;
	for (i = 0; i < mauiLineAddresses.NumElements(); i++)
	{
		bFound |= *mauiLineAddresses.Get(i) == uiProgram;
	}
	if (!bFound && (uiProgram >= uiMemoryAddress) && (uiProgram < uiMemoryAddress + uiMemorySize))
	{
		mauiLineAddresses.Kill();
		mauiLineAddresses.Init();
		aszLines.Kill();
		aszLines.Init();
		Disassemble(pcDebugger, puiMemory, uiMemoryAddress, uiMemorySize, uiProgram, &mauiLineAddresses, &aszLines);
	}

	sz.Init();
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		uiAddress = *mauiLineAddresses.Get(i);
		sColour = msColour;
		if (pcDebugger->FindBreakpoint(uiAddress) != NULL)
		{
			sColour = IDE_DEBUG_BREAKPOINT_COLOUR;
		}
		if ((uiAddress == uiProgram) && pcDebugger->IsStopped())
		{
			sColour = IDE_DEBUG_PROGRAM_COLOUR;
		}
		sz.Set(aszLines.GetText(i));
		if (i + 1 < aszLines.NumElements())
		{
			sz.AppendNewLine();
		}
		mpDisassembly->AppendText(sz.Text(), sColour);
	}
	sz.Kill();
	aszLines.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The instructions from uiStart to the end of the memory: a W65C816's
// with the widths at the stop, anything else's as words.  Each line is
// "=> * $00:8002  E8           inx".
//
//////////////////////////////////////////////////////////////////////////
void CIDEDebugView::Disassemble(CIDEDebugger* pcDebugger, uint8* puiMemory, uint32 uiMemoryAddress, size uiMemorySize, uint32 uiStart, CArrayIDEAddresses* pauiAddresses, CArrayChars* paszLines)
{
	CDebugClient*	pcClient;
	SAsm65816Widths	sWidths;
	size			uiOffset;
	size			uiLength;
	size			uiP;
	size			uiE;
	uint32			uiAddress;
	uint32			uiProgram;
	CChars*			psz;
	CChars			szInstruction;
	char			szByte[8];
	size			i;
	bool			b65C816;
	uint64			uiPValue;

	pcClient = pcDebugger->GetClient();
	if (pcClient == NULL)
	{
		return;
	}
	b65C816 = strcmp(pcClient->GetProcessorName(), IDE_DEBUG_W65C816) == 0;
	uiP = FindRegister(pcClient, "P");
	uiE = FindRegister(pcClient, "E");
	uiPValue = (uiP != MAX_SIZE) ? pcClient->GetRegisterValue(uiP) : 0x30;
	sWidths.bMemory8 = (uiPValue & 0x20) != 0;
	sWidths.bIndex8 = (uiPValue & 0x10) != 0;
	if ((uiE != MAX_SIZE) && (pcClient->GetRegisterValue(uiE) != 0))
	{
		sWidths.bMemory8 = true;
		sWidths.bIndex8 = true;
	}
	uiProgram = pcDebugger->GetProgramAddress();

	uiOffset = uiStart - uiMemoryAddress;
	while (uiOffset < uiMemorySize)
	{
		uiAddress = uiMemoryAddress + (uint32)uiOffset;
		szInstruction.Init();
		if (b65C816)
		{
			uiLength = Asm65816Disassemble(uiAddress, &puiMemory[uiOffset], uiMemorySize - uiOffset, &sWidths, &szInstruction);
		}
		else
		{
			uiLength = (uiMemorySize - uiOffset >= 2) ? 2 : 0;
			if (uiLength != 0)
			{
				sprintf(szByte, "$%02X%02X", puiMemory[uiOffset + 1], puiMemory[uiOffset]);
				szInstruction.Append(".word ");
				szInstruction.Append(szByte);
			}
		}
		if (uiLength == 0)
		{
			szInstruction.Kill();
			break;
		}

		pauiAddresses->Add(&uiAddress);
		psz = paszLines->Add();
		psz->Append(((uiAddress == uiProgram) && pcDebugger->IsStopped()) ? "=> " : "   ");
		psz->Append((pcDebugger->FindBreakpoint(uiAddress) != NULL) ? "* " : "  ");
		AppendIDEDebugAddress(psz, uiAddress);
		psz->Append("  ");
		for (i = 0; i < 4; i++)
		{
			if (i < uiLength)
			{
				sprintf(szByte, "%02X ", puiMemory[uiOffset + i]);
				psz->Append(szByte);
			}
			else
			{
				psz->Append("   ");
			}
		}
		psz->Append(' ');
		psz->Append(szInstruction.Text());
		szInstruction.Kill();
		uiOffset += uiLength;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The register named szName, MAX_SIZE if there is none.
//
//////////////////////////////////////////////////////////////////////////
size CIDEDebugView::FindRegister(CDebugClient* pcClient, const char* szName)
{
	size	i;

	for (i = 0; i < pcClient->NumRegisters(); i++)
	{
		if (strcmp(pcClient->GetRegister(i)->szName, szName) == 0)
		{
			return i;
		}
	}
	return MAX_SIZE;
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
