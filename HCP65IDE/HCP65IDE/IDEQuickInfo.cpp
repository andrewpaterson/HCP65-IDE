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
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDEIntroduceVariable.h"
#include "IDEQuickInfo.h"


#define IDE_LINK_COLOUR			Set32BitColour((uint8)0x58, (uint8)0x9D, (uint8)0xF6)
#define IDE_QUICK_OVERLOADS		12		//The most functions of one name Parameter Info lists.
#define IDE_NO_PARAMETERS		"<no parameters>"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	muiLinkStart = 0;
	muiLinkEnd = 0;
	muiLinkChanges = 0;
	mszLinkName.Init();
	muiAskedPosition = MAX_SIZE;
	muiAskedChanges = 0;
	mbLinkHeld = false;
	meInfo = IQI_None;
	muiInfoCaret = 0;
	muiInfoChanges = 0;
	muiInfoOpen = 0;
	mszInfoName.Init();
	muiInfoArgument = 0;
	masOverloads.Init();
	maiBold.Init();
	mpLinkPopup = OMalloc<CInfoPopup>(pcContent->GetWindow(), pcContent->GetTheme(), pcContent->GetCodeFamily());
	mpInfoPopup = OMalloc<CInfoPopup>(pcContent->GetWindow(), pcContent->GetTheme(), pcContent->GetCodeFamily());
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpLinkPopup);
	M_Pointer(mpInfoPopup);
	M_Pointer(mpDefinitions);
	M_Pointer(mpLinkTab);
	U_Size(muiLinkStart);
	U_Size(muiLinkEnd);
	U_Size(muiLinkChanges);
	U_String(mszLinkName);
	M_Pointer(mpAskedTab);
	U_Size(muiAskedPosition);
	U_Size(muiAskedChanges);
	U_Bool(mbLinkHeld);
	U_Enum(meInfo);
	M_Pointer(mpInfoTab);
	U_Size(muiInfoCaret);
	U_Size(muiInfoChanges);
	U_Size(muiInfoOpen);
	U_String(mszInfoName);
	U_Size(muiInfoArgument);
	U_Unknown(CArrayIDESymbolInfos, masOverloads);
	U_Unknown(CArrayInt, maiBold);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Free(void)
{
	maiBold.Kill();
	KillIDESymbolInfos(&masOverloads);
	mszInfoName.Kill();
	mszLinkName.Kill();
}


bool CIDEQuickInfo::Save(CObjectWriter* pcFile) { return false; }
bool CIDEQuickInfo::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The IDE is closing: the chooser's preview lets go of the edits thread.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Detach(void)
{
	if (mpDefinitions.IsNotNull())
	{
		mpDefinitions->Detach();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// View's, with IntelliJ IDEA's keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("View.QuickDefinition", "Quick Definition", "Control+Shift+I", Ptr<CImageCel>(), this, &CIDEQuickInfo::ShowQuickDefinition)->SetEnabledCheck(this, &CIDEQuickInfo::HasEditor);
	pCommands->Add("View.ParameterInfo", "Parameter Info", "Control+P", Ptr<CImageCel>(), this, &CIDEQuickInfo::ShowParameterInfo)->SetEnabledCheck(this, &CIDEQuickInfo::HasEditor);
	pCommands->Add("View.TypeInfo", "Type Info", "Control+Shift+P", Ptr<CImageCel>(), this, &CIDEQuickInfo::ShowTypeInfo)->SetEnabledCheck(this, &CIDEQuickInfo::HasEditor);
}


void CIDEQuickInfo::AddViewItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.QuickDefinition"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.ParameterInfo"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.TypeInfo"));
}


bool CIDEQuickInfo::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: the name under the pointer while Control is held, and
// the popup over the caret's line as the caret moves.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Poll(void)
{
	PollLink();
	PollInfo();
}


//////////////////////////////////////////////////////////////////////////
//
// With Control held (alone) and the pointer on a character of the active
// editor, the name there is looked up once for each place the pointer
// rests on; otherwise nothing is underlined.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::PollLink(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CComponentInput*	pcInput;
	CInt32Rect			cBounds;
	SInt32Vec2			sPointer;
	size				uiPosition;
	bool				bOver;

	if (mbLinkHeld)
	{
		return;
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pcInput = mpcContent->GetWindow()->GetComponentInput();
	uiPosition = 0;
	bOver = pTab.IsNotNull() && pcInput->IsControl() && !pcInput->IsShift() && !pcInput->IsAlt() && mpcContent->GetWindow()->GetFocus()->IsPointerIn();
	if (bOver)
	{
		pEditor = pTab->GetEditor();
		bOver = mpcContent->GetWindow()->GetFocus()->GetMouseOverComponent() == pEditor;
	}
	if (bOver)
	{
		pEditor->GetActualBounds(&cBounds);
		sPointer = mpcContent->GetWindow()->GetFocus()->GetPointer();
		bOver = pEditor->FindCharacterAt(sPointer.x - cBounds.GetLeft(), sPointer.y - cBounds.GetTop(), &uiPosition);
	}
	if (!bOver)
	{
		ClearLink();
		mpAskedTab = NULL;
		muiAskedPosition = MAX_SIZE;
		return;
	}
	if ((mpAskedTab == pTab) && (muiAskedPosition == uiPosition) && (muiAskedChanges == pEditor->GetChangeCount()))
	{
		return;
	}
	mpAskedTab = pTab;
	muiAskedPosition = uiPosition;
	muiAskedChanges = pEditor->GetChangeCount();
	if ((mpLinkTab == pTab) && (uiPosition >= muiLinkStart) && (uiPosition < muiLinkEnd) && (muiLinkChanges == pEditor->GetChangeCount()))
	{
		return;
	}
	ShowLinkAt(pTab, uiPosition);
}


//////////////////////////////////////////////////////////////////////////
//
// The name at uiPosition of pTab's text, if it is a symbol the index has
// declared somewhere, is underlined, and its declaration shown under it
// with where it is declared.  False, with nothing underlined, if it
// isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowLinkAt(Ptr<CIDEEditorTab> pTab, size uiPosition)
{
	Ptr<CTextArea>	pEditor;
	SIDESymbolInfo	sInfo;
	STextRange		sRange;
	CInt32Rect		cBounds;
	CInt32Rect		cLine;
	SInt32Vec2		sPoint;
	CChars			szKey;
	CChars			szText;
	size			uiStart;
	size			uiEnd;
	size			uiLine;
	uint8			uiKind;
	bool			bResult;

	ClearLink();
	pEditor = pTab->GetEditor();
	if (!FindIDENameAt(pEditor->GetText(), uiPosition, &uiStart, &uiEnd))
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	uiLine = pEditor->FindLine(uiStart);
	szKey.Init();
	sInfo.Init();
	bResult = mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), (int)uiLine, (int)(uiStart - pEditor->GetLineStart(uiLine)), &szKey, &uiKind) &&
			  ReadIDESymbolInfo(mpcContent, szKey.Text(), &sInfo);
	if (bResult)
	{
		sRange.uiStart = uiStart;
		sRange.uiEnd = uiEnd;
		pEditor->SetHighlightRanges(pTab->GetLinkLayer(IDE_LINK_COLOUR), &sRange, 1);
		mpLinkTab = pTab;
		muiLinkStart = uiStart;
		muiLinkEnd = uiEnd;
		muiLinkChanges = pEditor->GetChangeCount();
		mszLinkName.Set("");
		mszLinkName.Append(&pEditor->GetText()[uiStart], uiEnd - uiStart);

		szText.Init(sInfo.szDeclaration);
		szText.Append('\n');
		szText.Append(sInfo.szWhere);
		mpLinkPopup->SetText(szText.Text());
		mpLinkPopup->AddSpan(sInfo.szDeclaration.Length() + 1, sInfo.szWhere.Length(), mpcContent->GetTheme()->GetColour(TC_DimText));
		szText.Kill();

		mpcContent->GetWindow()->Layout();
		pEditor->GetActualBounds(&cBounds);
		pEditor->GetLineBounds(uiLine, &cLine);
		sPoint.Init(cBounds.GetLeft() + pEditor->GetPositionX(uiStart, false), cBounds.GetTop() + cLine.GetBottom());
		mpLinkPopup->Show(sPoint, false);
	}
	sInfo.Kill();
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// For a check's script: what ShowLinkAt showed stays though Control isn't
// down, until it is let go.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::HoldLink(bool bHeld)
{
	mbLinkHeld = bHeld;
	if (!bHeld)
	{
		ClearLink();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Nothing is underlined and no declaration shown.  False if none was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ClearLink(void)
{
	if (mpLinkTab.IsNull())
	{
		return false;
	}
	mpLinkTab->GetEditor()->ClearHighlightRanges(mpLinkTab->GetLinkLayer(IDE_LINK_COLOUR));
	mpLinkTab = NULL;
	mpLinkPopup->Hide();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A tab is closing: nothing of it is kept.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::TabClosed(Ptr<CIDEEditorTab> pTab)
{
	if (mpLinkTab == pTab)
	{
		mpLinkTab = NULL;
		mpLinkPopup->Hide();
	}
	if (mpAskedTab == pTab)
	{
		mpAskedTab = NULL;
		muiAskedPosition = MAX_SIZE;
	}
	if (mpInfoTab == pTab)
	{
		HideInfo();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Escape in an editor: the popup over the caret goes.  It is the editor's
// own too (the selection cleared) when there is none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::EscapeAction(SComponentInputEvent* psEvent)
{
	if (!HideInfo())
	{
		psEvent->bPass = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The popup over the caret's line is taken away.  False if it wasn't
// shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::HideInfo(void)
{
	bool	bShown;

	bShown = (meInfo != IQI_None) && mpInfoPopup->IsShown();
	meInfo = IQI_None;
	mpInfoTab = NULL;
	mpInfoPopup->Hide();
	KillOverloads();
	return bShown;
}


void CIDEQuickInfo::KillOverloads(void)
{
	KillIDESymbolInfos(&masOverloads);
	masOverloads.Init();
	maiBold.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The popup over the caret's line follows the caret: Type Info goes when
// it moves or the text changes; Parameter Info is worked out again, and
// goes when the caret has left the call.  Either goes with its tab, and
// is forgotten if a press closed it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::PollInfo(void)
{
	Ptr<CIDEEditorTab>	pTab;

	if (meInfo == IQI_None)
	{
		return;
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (!mpInfoPopup->IsShown() || (pTab != mpInfoTab) || (mpcContent->GetWindow()->GetFocus()->GetFocussedComponent() != pTab->GetEditor()))
	{
		HideInfo();
		return;
	}
	if ((pTab->GetEditor()->GetCaretPosition() == muiInfoCaret) && (pTab->GetEditor()->GetChangeCount() == muiInfoChanges))
	{
		return;
	}
	if (meInfo == IQI_Type)
	{
		HideInfo();
	}
	else
	{
		UpdateParameterInfo(false);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The popup is put over the line the active editor's caret is on, its
// left at the caret.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::ShowOverCaret(void)
{
	Ptr<CTextArea>	pEditor;
	CInt32Rect		cBounds;
	CInt32Rect		cCaret;
	SInt32Vec2		sPoint;

	mpcContent->GetWindow()->Layout();
	pEditor = mpInfoTab->GetEditor();
	pEditor->GetActualBounds(&cBounds);
	pEditor->GetCaretBounds(&cCaret);
	sPoint.Init(cBounds.GetLeft() + cCaret.GetLeft(), cBounds.GetTop() + cCaret.GetTop());
	mpInfoPopup->Show(sPoint, true);
}


//////////////////////////////////////////////////////////////////////////
//
// Parameter Info (Control+P): the parameters of the call the caret is in.
// False, the status line saying why, if it is in none or what is called
// isn't a function the index has.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowParameterInfo(void)
{
	if (!HasEditor())
	{
		return false;
	}
	if (meInfo != IQI_None)
	{
		HideInfo();
	}
	return UpdateParameterInfo(true);
}


//////////////////////////////////////////////////////////////////////////
//
// The call the caret is in is found from the text; if it is the call
// shown already (its bracket where it was, the same name before it) only
// the caret's argument is looked at again, so typing an argument doesn't
// ask the index anything.  Otherwise the function called is the symbol
// its name is, with the others of that name beside it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::UpdateParameterInfo(bool bSay)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CChars				szKey;
	CChars				szName;
	const char*			szText;
	size				uiNameStart;
	size				uiNameEnd;
	size				uiOpen;
	size				uiArgument;
	size				uiLine;
	uint8				uiKind;
	bool				bFound;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pEditor = pTab->GetEditor();
	szText = pEditor->GetText();
	if (!FindIDECall(szText, pEditor->GetCaretPosition(), &uiNameStart, &uiNameEnd, &uiOpen, &uiArgument))
	{
		HideInfo();
		if (bSay)
		{
			mpcContent->ShowStatus("The caret isn't among a call's arguments.");
		}
		return false;
	}

	szName.Init();
	szName.Append(&szText[uiNameStart], uiNameEnd - uiNameStart);
	if ((meInfo != IQI_Parameters) || (mpInfoTab != pTab) || (uiOpen != muiInfoOpen) || !mszInfoName.Equals(szName))
	{
		KillOverloads();
		mpcContent->GetDocuments()->UpdateEditedTabsNow();
		uiLine = pEditor->FindLine(uiNameStart);
		szKey.Init();
		uiKind = 0;
		bFound = mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), (int)uiLine, (int)(uiNameStart - pEditor->GetLineStart(uiLine)), &szKey, &uiKind) &&
				 ((uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate));
		if (bFound)
		{
			FindOverloads(szKey.Text());
		}
		szKey.Kill();
		if (masOverloads.NumElements() == 0)
		{
			HideInfo();
			if (bSay)
			{
				szName.Insert(0, "No parameters are known for ");
				szName.Append(": it isn't a function the index has declared.");
				mpcContent->ShowStatus(szName.Text());
			}
			szName.Kill();
			return false;
		}
		mszInfoName.Set(szName);
		muiInfoOpen = uiOpen;
	}
	szName.Kill();

	meInfo = IQI_Parameters;
	mpInfoTab = pTab;
	muiInfoArgument = uiArgument;
	muiInfoCaret = pEditor->GetCaretPosition();
	muiInfoChanges = pEditor->GetChangeCount();
	ShowParameters();
	ShowOverCaret();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The function szKey and the others of its name that its class (or, with
// no class, its file) declares beside it, in the order they are declared:
// each one's parameters as they are written.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::FindOverloads(const char* szKey)
{
	CArrayIDELocations	asDeclared;
	CArrayIDELocations	asDefined;
	SIDESymbolInfo*		psInfo;
	SIDELocation*		psLocation;
	CChars				szName;
	CChars				szOtherKey;
	CChars				szOtherName;
	size				i;
	size				j;
	uint8				uiKind;
	bool				bKnown;

	psInfo = masOverloads.Add();
	psInfo->Init();
	if (!ReadIDESymbolInfo(mpcContent, szKey, psInfo))
	{
		psInfo->Kill();
		masOverloads.RemoveAt(0);
		return;
	}

	szName.Init();
	GetIDEScopedName(szKey, &szName);
	asDeclared.Init();
	asDefined.Init();
	mpcContent->GetCodeModel()->FindSiblingFunctions(szKey, masOverloads.Get(0)->sPlace.szFileName.Text(), &asDeclared, &asDefined);
	for (i = 0; (i < asDeclared.NumElements()) && (masOverloads.NumElements() < IDE_QUICK_OVERLOADS); i++)
	{
		psLocation = asDeclared.Get(i);
		szOtherKey.Init();
		szOtherName.Init();
		if (mpcContent->GetCodeModel()->GetSymbolAt(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, &szOtherKey, &uiKind))
		{
			GetIDEScopedName(szOtherKey.Text(), &szOtherName);
			bKnown = false;
			for (j = 0; j < masOverloads.NumElements(); j++)
			{
				bKnown = bKnown || masOverloads.Get(j)->szKey.Equals(szOtherKey);
			}
			if (!bKnown && szOtherName.Equals(szName))
			{
				psInfo = masOverloads.Add();
				psInfo->Init();
				if (!ReadIDESymbolInfo(mpcContent, szOtherKey.Text(), psInfo))
				{
					psInfo->Kill();
					masOverloads.RemoveAt(masOverloads.NumElements() - 1);
				}
			}
		}
		szOtherName.Kill();
		szOtherKey.Kill();
	}
	KillIDELocations(&asDefined);
	KillIDELocations(&asDeclared);
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The popup's text: a row for each function, its parameters with commas
// between them (or that it has none), the one the caret's argument is
// given to in the text's colour and the rest dim.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::ShowParameters(void)
{
	SIDESymbolInfo*		psInfo;
	CChars				szText;
	size				i;
	size				j;
	int					iStart;
	int					iLength;

	szText.Init();
	maiBold.ReInit();
	for (i = 0; i < masOverloads.NumElements(); i++)
	{
		psInfo = masOverloads.Get(i);
		if (i > 0)
		{
			szText.Append('\n');
		}
		if (psInfo->aszParameters.NumElements() == 0)
		{
			szText.Append(IDE_NO_PARAMETERS);
		}
		for (j = 0; j < psInfo->aszParameters.NumElements(); j++)
		{
			if (j > 0)
			{
				szText.Append(", ");
			}
			if (j == muiInfoArgument)
			{
				iStart = (int)szText.Length();
				iLength = (int)psInfo->aszParameters.Get(j)->Length();
				maiBold.Add(iStart);
				maiBold.Add(iLength);
			}
			szText.Append(psInfo->aszParameters.Get(j)->Text());
		}
	}
	//The code's family has no bold face: the rest is dim instead.
	mpInfoPopup->SetText(szText.Text());
	if (maiBold.NumElements() > 0)
	{
		mpInfoPopup->AddSpan(0, szText.Length(), mpcContent->GetTheme()->GetColour(TC_DimText));
	}
	for (i = 0; i + 1 < maiBold.NumElements(); i += 2)
	{
		mpInfoPopup->AddSpan((size)maiBold.GetValue(i), (size)maiBold.GetValue(i + 1), mpcContent->GetTheme()->GetColour(TC_Text));
	}
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Type Info (Control+Shift+P): the type of the selected expression, as
// Introduce Variable would declare a variable for it, or with nothing
// selected the type the symbol at the caret is declared with.  False, the
// status line saying so, if neither gives one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowTypeInfo(void)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDEIntroduction	sIntroduction;
	SIDESymbolInfo		sInfo;
	CChars				szType;
	CChars				szKey;
	size				uiStart;
	size				uiEnd;
	uint8				uiKind;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	HideInfo();
	szType.Init();
	if (pTab->GetEditor()->GetSelection(&uiStart, &uiEnd))
	{
		sIntroduction.Init();
		if (mpcContent->GetIntroduceVariable()->FindIntroduction(&sIntroduction))
		{
			szType.Append(sIntroduction.szType);
		}
		sIntroduction.Kill();
	}
	if (szType.Empty())
	{
		szKey.Init();
		sInfo.Init();
		if (mpcContent->GetNavigator()->FindCaretSymbol(&szKey, &uiKind) && ReadIDESymbolInfo(mpcContent, szKey.Text(), &sInfo))
		{
			AppendIDEDeclaredType(&sInfo, &szType);
		}
		sInfo.Kill();
		szKey.Kill();
	}
	if (szType.Empty())
	{
		szType.Kill();
		mpcContent->ShowStatus("No type is known for what is at the caret.");
		return false;
	}

	meInfo = IQI_Type;
	mpInfoTab = pTab;
	muiInfoCaret = pTab->GetEditor()->GetCaretPosition();
	muiInfoChanges = pTab->GetEditor()->GetChangeCount();
	mpInfoPopup->SetText(szType.Text());
	szType.Kill();
	ShowOverCaret();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Quick Definition (Control+Shift+I): the definition of the symbol at the
// caret (its declaration if it has none) and what implements it, in a
// chooser that previews the code of the one selected; Enter goes to it.
// False, the status line saying so, if there is no symbol there or the
// index has it declared nowhere.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowQuickDefinition(void)
{
	CArrayIDETargets	asTargets;
	SIDETarget*			psTarget;
	CChars				szKey;
	CChars				szTitle;
	size				i;
	size				j;
	uint8				uiKind;
	bool				bResult;

	if (!HasEditor())
	{
		return false;
	}
	szKey.Init();
	if (!mpcContent->GetNavigator()->FindCaretSymbol(&szKey, &uiKind))
	{
		szKey.Kill();
		mpcContent->ShowStatus("There is no symbol at the caret to show the definition of.");
		return false;
	}

	asTargets.Init();
	mpcContent->GetCodeModel()->FindTarget(szKey.Text(), &asTargets);
	mpcContent->GetCodeModel()->FindImplementations(szKey.Text(), &asTargets);

	//One place once.
	for (i = asTargets.NumElements(); i > 1; i--)
	{
		psTarget = asTargets.Get(i - 1);
		for (j = 0; j < i - 1; j++)
		{
			if ((asTargets.Get(j)->sLocation.iLine == psTarget->sLocation.iLine) && asTargets.Get(j)->sLocation.szFileName.Equals(psTarget->sLocation.szFileName))
			{
				psTarget->szKey.Kill();
				psTarget->sLocation.szFileName.Kill();
				asTargets.RemoveAt(i - 1);
				break;
			}
		}
	}

	szTitle.Init("Definition of ");
	GetIDEScopedName(szKey.Text(), &szTitle);
	bResult = asTargets.NumElements() > 0;
	if (bResult)
	{
		if (asTargets.NumElements() > 1)
		{
			szTitle.Append("  ");
			szTitle.Append((uint32)asTargets.NumElements());
		}
		if (mpDefinitions.IsNull())
		{
			mpDefinitions = OMalloc<CIDEChooser>(mpcContent);
		}
		mpDefinitions->Begin(szTitle.Text(), false, true, NULL);
		mpDefinitions->SetSelectName(true);
		for (i = 0; i < asTargets.NumElements(); i++)
		{
			mpDefinitions->AddTarget(asTargets.Get(i));
		}
		bResult = mpDefinitions->OpenAtCaret();
	}
	else
	{
		szTitle.Insert(0, "No ");
		szTitle.SetChar(3, 'd');
		szTitle.Append(" is known to the index.");
		mpcContent->ShowStatus(szTitle.Text());
	}
	szTitle.Kill();
	KillIDETargets(&asTargets);
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Link: [Area] virtual int Area(void) = 0 | CShape,
// Shape.h:15" and "Info: parameters" with its rows under it, the caret's
// parameter in square brackets; "none" for each that isn't shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Print(CChars* pszDest)
{
	CChars	szText;
	size	i;

	pszDest->Append("Link: ");
	if (IsLinkShown())
	{
		szText.Init(mpLinkPopup->GetText());
		szText.Replace("\n", " | ");
		pszDest->Append('[');
		pszDest->Append(mszLinkName);
		pszDest->Append("] ");
		pszDest->Append(szText);
		szText.Kill();
	}
	else
	{
		pszDest->Append("none");
	}
	pszDest->Append("\nInfo: ");
	if (IsInfoShown())
	{
		pszDest->Append((meInfo == IQI_Parameters) ? "parameters" : "type");
		szText.Init(mpInfoPopup->GetText());
		for (i = maiBold.NumElements(); i >= 2; i -= 2)
		{
			szText.Insert((size)(maiBold.GetValue(i - 2) + maiBold.GetValue(i - 1)), "]");
			szText.Insert((size)maiBold.GetValue(i - 2), "[");
		}
		szText.Replace("\n", "\n  ");
		pszDest->Append("\n  ");
		pszDest->Append(szText);
		szText.Kill();
	}
	else
	{
		pszDest->Append("none");
	}
	pszDest->Append('\n');
}


bool CIDEQuickInfo::IsLinkShown(void) { return mpLinkTab.IsNotNull() && mpLinkPopup->IsShown(); }
bool CIDEQuickInfo::IsInfoShown(void) { return (meInfo != IQI_None) && mpInfoPopup->IsShown(); }
Ptr<CIDEChooser> CIDEQuickInfo::GetDefinitionsChooser(void) const { return mpDefinitions; }

