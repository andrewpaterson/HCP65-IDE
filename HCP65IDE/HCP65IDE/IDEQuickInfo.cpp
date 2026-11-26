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
#include "BaseLib/TextFile.h"
#include "BaseLib/Timer.h"
#include "IDEIntroduceVariable.h"
#include "IDELocationPreview.h"
#include "IDEResultsPane.h"
#include "IDEQuickInfo.h"


#define IDE_LINK_COLOUR			Set32BitColour((uint8)0x58, (uint8)0x9D, (uint8)0xF6)
#define IDE_QUICK_OVERLOADS		12		//The most functions of one name Parameter Info lists.
#define IDE_NO_PARAMETERS		"<no parameters>"
#define IDE_QUICK_COLOURS		64		//The most coloured parts of a declaration.
#define IDE_DOCUMENTED_IN		"Documentation:"


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
	meLinkCursor = NC_Text;
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
	muiDocumentationCaret = 0;
	muiDocumentationChanges = 0;
	mbDocumentationPinned = false;
	mszDocumentationName.Init();
	mszDocumentationEntry.Init();
	masDocumentationLinks.Init();
	mbDocumentationPressed = false;
	muiDocumentationPressed = MAX_SIZE;
	mszTipName.Init();
	mbToolTips = true;
	mbTipHeld = false;
	muiRestStart = 0;
	muiRestChanges = 0;
	muiRestCaret = 0;
	msRestPointer.Init(0, 0);
	miRestTime = 0;
	mbRestDone = false;
	mpDocumentation = OMalloc<CInfoPopup>(pcContent->GetWindow(), pcContent->GetTheme(), pcContent->GetCodeFamily());
	mpDocumentation->SetTakesPointer(true);
	mpDocumentation->GetPressEvent()->Add(this, &CIDEQuickInfo::DocumentationPressed);
	mpTip = OMalloc<CInfoPopup>(pcContent->GetWindow(), pcContent->GetTheme(), pcContent->GetCodeFamily());
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
	U_Enum(meLinkCursor);
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
	M_Pointer(mpDocumentation);
	M_Pointer(mpDocumentationTab);
	U_Size(muiDocumentationCaret);
	U_Size(muiDocumentationChanges);
	U_Bool(mbDocumentationPinned);
	U_String(mszDocumentationName);
	U_String(mszDocumentationEntry);
	U_Unknown(CArrayIDEDocumentationLinks, masDocumentationLinks);
	U_Bool(mbDocumentationPressed);
	U_Size(muiDocumentationPressed);
	M_Pointer(mpTip);
	M_Pointer(mpTipTab);
	U_String(mszTipName);
	U_Bool(mbToolTips);
	U_Bool(mbTipHeld);
	M_Pointer(mpRestTab);
	U_Size(muiRestStart);
	U_Size(muiRestChanges);
	U_Size(muiRestCaret);
	U_Data(SInt32Vec2, msRestPointer);
	U_Data(int64, miRestTime);
	U_Bool(mbRestDone);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Free(void)
{
	if (mpDocumentation.IsNotNull())
	{
		mpDocumentation->GetPressEvent()->Remove(this);
	}
	mszTipName.Kill();
	KillDocumentationLinks();
	masDocumentationLinks.Kill();
	mszDocumentationEntry.Kill();
	mszDocumentationName.Kill();
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
	pCommands->Add("View.QuickDocumentation", "Quick Documentation", "Control+Q", Ptr<CImageCel>(), this, &CIDEQuickInfo::ShowQuickDocumentation)->SetEnabledCheck(this, &CIDEQuickInfo::HasEditor);
}


void CIDEQuickInfo::AddViewItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.QuickDefinition"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.ParameterInfo"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.TypeInfo"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.QuickDocumentation"));
}


bool CIDEQuickInfo::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: the name under the pointer while Control is held, the
// popup over the caret's line and Quick Documentation as the caret moves,
// and the tool tip of the name the pointer rests on.  How long until the
// tip is due, -1 if none is waited for.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEQuickInfo::Poll(int64 iNow)
{
	PollLink();
	PollInfo();
	PollDocumentation();
	return PollTip(iNow);
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
// declared somewhere, is underlined, the pointer is a hand over the
// editor, and its declaration is shown under it with where it is
// declared.  A name the index hasn't that is a macro's shows its #define
// the same way (CIDEConvertMacro::FindDefine).  False, with nothing
// underlined, if it is neither.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowLinkAt(Ptr<CIDEEditorTab> pTab, size uiPosition)
{
	Ptr<CTextArea>	pEditor;
	SIDESymbolInfo	sInfo;
	SIDELocation	sDefine;
	STextRange		sRange;
	CInt32Rect		cBounds;
	CInt32Rect		cLine;
	SInt32Vec2		sPoint;
	CChars			szKey;
	CChars			szText;
	CChars			szName;
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
	if (!bResult)
	{
		//Not a symbol: a macro, perhaps, whose #define is its declaration.
		szName.Init();
		szName.Append(&pEditor->GetText()[uiStart], uiEnd - uiStart);
		if (mpcContent->GetConvertMacro()->FindDefine(szName.Text(), &sDefine))
		{
			sInfo.szDeclaration.Clear();
			sInfo.szWhere.Clear();
			bResult = mpcContent->GetConvertMacro()->ReadDefineInfo(&sDefine, &sInfo.szDeclaration, &sInfo.szWhere);
			sDefine.szFileName.Kill();
		}
		szName.Kill();
	}
	if (bResult)
	{
		sRange.uiStart = uiStart;
		sRange.uiEnd = uiEnd;
		pEditor->SetHighlightRanges(pTab->GetLinkLayer(IDE_LINK_COLOUR), &sRange, 1);
		meLinkCursor = pEditor->GetCursor();
		pEditor->SetCursor(NC_Hand);
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
// Nothing is underlined, no declaration is shown and the pointer is the
// editor's own shape again.  False if nothing was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ClearLink(void)
{
	if (mpLinkTab.IsNull())
	{
		return false;
	}
	mpLinkTab->GetEditor()->ClearHighlightRanges(mpLinkTab->GetLinkLayer(IDE_LINK_COLOUR));
	if (mpLinkTab->GetEditor()->GetCursor() == NC_Hand)
	{
		mpLinkTab->GetEditor()->SetCursor(meLinkCursor);
	}
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
	if (mpDocumentationTab == pTab)
	{
		HideDocumentation();
	}
	if (mpTipTab == pTab)
	{
		HideTip();
	}
	if (mpRestTab == pTab)
	{
		mpRestTab = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Escape in an editor: a name's tool tip, Quick Documentation and the
// popup over the caret go.  It is the editor's own too (the selection
// cleared) when there is none of them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::EscapeAction(SComponentInputEvent* psEvent)
{
	bool	bHidden;

	bHidden = HideTip();
	bHidden |= HideDocumentation();
	bHidden |= HideInfo();
	if (!bHidden)
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
// Parameter Info for the call the caret is in, which is known to be of
// the function szKey: as completion knows of the one it has just written,
// whose call the index hasn't seen yet.  Nothing is said if it can't be
// shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowParameterInfoOf(const char* szKey)
{
	if (!HasEditor())
	{
		return false;
	}
	if (meInfo != IQI_None)
	{
		HideInfo();
	}
	return UpdateParameterInfo(false, szKey);
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
bool CIDEQuickInfo::UpdateParameterInfo(bool bSay, const char* szKey)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CChars				szFound;
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
		if (szKey != NULL)
		{
			FindOverloads(szKey);
		}
		else
		{
			mpcContent->GetDocuments()->UpdateEditedTabsNow();
			uiLine = pEditor->FindLine(uiNameStart);
			szFound.Init();
			uiKind = 0;
			bFound = mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), (int)uiLine, (int)(uiNameStart - pEditor->GetLineStart(uiLine)), &szFound, &uiKind) &&
					 ((uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate));
			if (bFound)
			{
				FindOverloads(szFound.Text());
			}
			szFound.Kill();
		}
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
// selected the type the symbol at the caret is declared with.  Where the
// caret is on no symbol with a type (a call's bracket, an operator, a
// number), the type of the expression it is in, which is the file's
// checked parse: only then.  False, the status line saying so, if none
// gives one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowTypeInfo(void)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDEIntroduction	sIntroduction;
	SIDESymbolInfo		sInfo;
	CChars				szType;
	CChars				szKey;
	CChars				szExpression;
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
	if (szType.Empty() && !pTab->GetEditor()->GetSelection(&uiStart, &uiEnd))
	{
		szKey.Init();
		szExpression.Init();
		mpcContent->GetIntroduceVariable()->FindCaretExpressionType(&szExpression, &szType, &szKey);
		szExpression.Kill();
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
// A macro's name, which is no symbol of the index's, has its #define.
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
	asTargets.Init();
	if (mpcContent->GetNavigator()->FindCaretSymbol(&szKey, &uiKind))
	{
		mpcContent->GetCodeModel()->FindTarget(szKey.Text(), &asTargets);
		mpcContent->GetCodeModel()->FindImplementations(szKey.Text(), &asTargets);
	}
	else
	{
		psTarget = asTargets.Add();
		psTarget->szKey.Init();
		psTarget->uiKind = 0;
		if (!mpcContent->GetConvertMacro()->FindDefineAtCaret(&szKey, &psTarget->sLocation))
		{
			psTarget->szKey.Kill();
			asTargets.Kill();
			szKey.Kill();
			mpcContent->ShowStatus("There is no symbol at the caret to show the definition of.");
			return false;
		}
		psTarget->szKey.Set(szKey);
	}

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


static const char* GetIDECursorName(ENativeCursor eCursor)
{
	return (eCursor == NC_Hand) ? "hand" : ((eCursor == NC_Text) ? "text" : ((eCursor == NC_Arrow) ? "arrow" : "other"));
}


//////////////////////////////////////////////////////////////////////////
//
// What is written about the name at uiPosition of pTab's text (or ending
// there, as a name the caret is after): the symbol the index has there,
// or a macro's #define and the comment over that.  The name and where it
// starts are given too.  False if it is neither, or there is no name
// there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::FindDocumented(Ptr<CIDEEditorTab> pTab, size uiPosition, SIDESymbolDocumentation* psDest, CChars* pszName, size* puiStart)
{
	Ptr<CTextArea>	pEditor;
	SIDELocation	sDefine;
	CTextFile		cFile;
	CChars			szKey;
	CChars			szName;
	const char*		szFileText;
	size			uiFileLength;
	size			uiStart;
	size			uiEnd;
	size			uiLine;
	uint8			uiKind;
	bool			bResult;

	pEditor = pTab->GetEditor();
	if (!FindIDENameAt(pEditor->GetText(), uiPosition, &uiStart, &uiEnd) && ((uiPosition == 0) || !FindIDENameAt(pEditor->GetText(), uiPosition - 1, &uiStart, &uiEnd)))
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	uiLine = pEditor->FindLine(uiStart);
	szKey.Init();
	bResult = mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), (int)uiLine, (int)(uiStart - pEditor->GetLineStart(uiLine)), &szKey, &uiKind) &&
			  ReadIDESymbolDocumentation(mpcContent, szKey.Text(), psDest);
	szKey.Kill();
	if (!bResult)
	{
		//Not a symbol: a macro, perhaps, whose #define is its declaration, with the comment over it.
		szName.Init();
		szName.Append(&pEditor->GetText()[uiStart], uiEnd - uiStart);
		if (mpcContent->GetConvertMacro()->FindDefine(szName.Text(), &sDefine))
		{
			psDest->sInfo.szDeclaration.Clear();
			psDest->sInfo.szWhere.Clear();
			bResult = mpcContent->GetConvertMacro()->ReadDefineInfo(&sDefine, &psDest->sInfo.szDeclaration, &psDest->sInfo.szWhere);
			if (bResult)
			{
				psDest->sInfo.sPlace.szFileName.Set(sDefine.szFileName);
				psDest->sInfo.sPlace.iLine = sDefine.iLine;
				psDest->sInfo.sPlace.iColumn = sDefine.iColumn;
				psDest->sInfo.sPlace.iLength = sDefine.iLength;
				cFile.Init();
				if (mpcContent->GetFindInFiles()->GetFileText(sDefine.szFileName.Text(), &cFile, &szFileText, &uiFileLength))
				{
					ReadIDEComment(szFileText, uiFileLength, FindIDETextPosition(szFileText, uiFileLength, sDefine.iLine, sDefine.iColumn), &psDest->szComment);
				}
				cFile.Kill();
			}
			sDefine.szFileName.Kill();
		}
		szName.Kill();
	}
	if (bResult)
	{
		pszName->Set("");
		pszName->Append(&pEditor->GetText()[uiStart], uiEnd - uiStart);
		*puiStart = uiStart;
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What is written about a symbol (psDocumented) set as pPopup's text and
// coloured: its declaration in the code's colours, where it is declared,
// dim, the comment over it, and (not for a tip) the documentation pages
// that name it.  A tip has only the first IDE_TIP_COMMENT_LINES of the
// comment.  Not a tip, the places that can be gone to are kept
// (masDocumentationLinks): where it is declared, and each page.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::WriteDocumented(SIDESymbolDocumentation* psDocumented, bool bTip, Ptr<CInfoPopup> pPopup)
{
	SIDEDocumentationLink*	psLink;
	SIDELocation*			psPage;
	SIDELineColour			asColours[IDE_QUICK_COLOURS];
	CChars					szText;
	const char*				szComment;
	const char*				szFile;
	size					uiCommentEnd;
	size					uiColours;
	size					uiWhere;
	size					uiHeading;
	size					uiLines;
	size					uiPlace;
	size					i;
	bool					bMore;

	//The declaration, where it is, the comment (a tip's cut to its first lines), the pages.
	szText.Init(psDocumented->sInfo.szDeclaration);
	szText.Append('\n');
	uiWhere = szText.Length();
	szText.Append(psDocumented->sInfo.szWhere);
	bMore = false;
	if (!psDocumented->szComment.Empty())
	{
		szText.Append("\n\n");
		szComment = psDocumented->szComment.Text();
		uiCommentEnd = psDocumented->szComment.Length();
		if (bTip)
		{
			uiLines = 0;
			for (i = 0; i < uiCommentEnd; i++)
			{
				if (szComment[i] == '\n')
				{
					uiLines++;
					if (uiLines == IDE_TIP_COMMENT_LINES)
					{
						uiCommentEnd = i;
						bMore = true;
					}
				}
			}
		}
		szText.Append(szComment, uiCommentEnd);
		if (bMore)
		{
			szText.Append("\n...");
		}
	}
	if (!bTip)
	{
		KillDocumentationLinks();
		//Where it is declared goes there: its file and line, after its scope.
		szFile = strrchr(psDocumented->sInfo.szWhere.Text(), ' ');
		uiPlace = (szFile != NULL) ? (size)(szFile + 1 - psDocumented->sInfo.szWhere.Text()) : 0;
		psLink = masDocumentationLinks.Add();
		psLink->uiStart = uiWhere + uiPlace;
		psLink->uiLength = psDocumented->sInfo.szWhere.Length() - uiPlace;
		psLink->sPlace = psDocumented->sInfo.sPlace;
		psLink->sPlace.szFileName.Init(psDocumented->sInfo.sPlace.szFileName);
	}
	uiHeading = 0;
	if (!bTip && (psDocumented->asPages.NumElements() > 0))
	{
		szText.Append("\n\n");
		uiHeading = szText.Length();
		szText.Append(IDE_DOCUMENTED_IN);
		for (i = 0; i < psDocumented->asPages.NumElements(); i++)
		{
			psPage = psDocumented->asPages.Get(i);
			szText.Append("\n  ");
			psLink = masDocumentationLinks.Add();
			psLink->uiStart = szText.Length();
			szText.Append(GetIDEFileNamePart(psPage->szFileName.Text()));
			szText.Append(':');
			szText.Append((int32)(psPage->iLine + 1));
			psLink->uiLength = szText.Length() - psLink->uiStart;
			psLink->sPlace = *psPage;
			psLink->sPlace.szFileName.Init(psPage->szFileName);
		}
	}

	pPopup->SetText(szText.Text());
	uiColours = FindIDELineColours(szText.Text(), psDocumented->sInfo.szDeclaration.Length(), asColours, IDE_QUICK_COLOURS);
	for (i = 0; i < uiColours; i++)
	{
		pPopup->AddSpan(asColours[i].uiStart, asColours[i].uiLength, GetHighlightColour(asColours[i].eKind));
	}
	pPopup->AddSpan(uiWhere, psDocumented->sInfo.szWhere.Length(), mpcContent->GetTheme()->GetColour(TC_DimText));
	if (bMore)
	{
		pPopup->AddSpan(szText.Length() - 3, 3, mpcContent->GetTheme()->GetColour(TC_DimText));
	}
	if (uiHeading > 0)
	{
		pPopup->AddSpan(uiHeading, strlen(IDE_DOCUMENTED_IN), mpcContent->GetTheme()->GetColour(TC_DimText));
	}
	if (!bTip)
	{
		for (i = 0; i < masDocumentationLinks.NumElements(); i++)
		{
			psLink = masDocumentationLinks.Get(i);
			pPopup->AddSpan(psLink->uiStart, psLink->uiLength, IDE_LINK_COLOUR);
		}
	}
	szText.Kill();
}


void CIDEQuickInfo::KillDocumentationLinks(void)
{
	size	i;

	for (i = 0; i < masDocumentationLinks.NumElements(); i++)
	{
		masDocumentationLinks.Get(i)->sPlace.szFileName.Kill();
	}
	masDocumentationLinks.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Quick Documentation (Control+Q): what is written about the symbol at
// the caret, under the caret's line.  While it shows for where the caret
// is, the command again keeps it: it follows the caret from then, and the
// command once more lets it go with the caret again.  False, the status
// line saying so, if nothing at the caret has a declaration the index or
// a #define gives.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowQuickDocumentation(void)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	if (mpcContent->GetCompletion()->IsOpen())
	{
		//Of the entry selected in completion's list, beside the list.
		if (!ShowEntryDocumentation())
		{
			HideDocumentation();
			mpcContent->ShowStatus("Nothing is known about the entry selected.");
			return false;
		}
		return true;
	}
	if (IsDocumentationShown() && (mpDocumentationTab == pTab) && (pTab->GetEditor()->GetCaretPosition() == muiDocumentationCaret) && (pTab->GetEditor()->GetChangeCount() == muiDocumentationChanges))
	{
		mbDocumentationPinned = !mbDocumentationPinned;
		mpcContent->ShowStatus(mbDocumentationPinned ? "Quick Documentation follows the caret until Escape." : "Quick Documentation goes when the caret moves.");
		return true;
	}
	mbDocumentationPinned = false;
	if (!ShowDocumentationAt(pTab, pTab->GetEditor()->GetCaretPosition()))
	{
		HideDocumentation();
		mpcContent->ShowStatus("Nothing is known about what is at the caret.");
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Quick Documentation of the entry selected in completion's list, shown
// beside the list; it follows the selection from then (PollDocumentation)
// and goes with the list.  False if the entry is no symbol (a word of the
// language, a file) or nothing is known of it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowEntryDocumentation(void)
{
	SIDESymbolDocumentation	sDocumented;
	Ptr<CIDEEditorTab>		pTab;
	Ptr<CTextArea>			pEditor;
	CInt32Rect				cLine;
	CChars					szKey;
	SInt32Vec2				sPoint;
	bool					bResult;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	szKey.Init();
	sDocumented.Init();
	bResult = pTab.IsNotNull() && mpcContent->GetCompletion()->GetSelectedSymbol(&szKey, &sPoint) && ReadIDESymbolDocumentation(mpcContent, szKey.Text(), &sDocumented);
	if (bResult)
	{
		WriteDocumented(&sDocumented, false, mpDocumentation);
		mszDocumentationName.Kill();
		GetIDESymbolName(szKey.Text(), &mszDocumentationName);
		mszDocumentationEntry.Set(szKey);
		HideTip();
		pEditor = pTab->GetEditor();
		mpDocumentationTab = pTab;
		muiDocumentationCaret = pEditor->GetCaretPosition();
		muiDocumentationChanges = pEditor->GetChangeCount();
		mbDocumentationPinned = false;
		mbDocumentationPressed = false;
		pEditor->GetLineBounds(0, &cLine);
		mpDocumentation->SetMaximumHeight(IDE_DOCUMENTATION_LINES * cLine.GetHeight());
		mpDocumentation->Show(sPoint, false);
	}
	sDocumented.Kill();
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Quick Documentation of the name at uiPosition of pTab's text, shown
// under the caret's line, its left at the name.  False, with what was
// shown left, if there is nothing to show for it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowDocumentationAt(Ptr<CIDEEditorTab> pTab, size uiPosition)
{
	SIDESymbolDocumentation	sDocumented;
	Ptr<CTextArea>	pEditor;
	CInt32Rect		cBounds;
	CInt32Rect		cLine;
	SInt32Vec2		sPoint;
	size			uiStart;

	sDocumented.Init();
	if (!FindDocumented(pTab, uiPosition, &sDocumented, &mszDocumentationName, &uiStart))
	{
		sDocumented.Kill();
		return false;
	}
	WriteDocumented(&sDocumented, false, mpDocumentation);
	sDocumented.Kill();
	mszDocumentationEntry.Set("");
	HideTip();
	pEditor = pTab->GetEditor();
	mpDocumentationTab = pTab;
	muiDocumentationCaret = pEditor->GetCaretPosition();
	muiDocumentationChanges = pEditor->GetChangeCount();
	mbDocumentationPressed = false;

	mpcContent->GetWindow()->Layout();
	pEditor->GetActualBounds(&cBounds);
	pEditor->GetLineBounds(pEditor->FindLine(uiStart), &cLine);
	mpDocumentation->SetMaximumHeight(IDE_DOCUMENTATION_LINES * cLine.GetHeight());
	sPoint.Init(cBounds.GetLeft() + pEditor->GetPositionX(uiStart, false), cBounds.GetTop() + cLine.GetBottom());
	mpDocumentation->Show(sPoint, false);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Quick Documentation follows the caret: it goes when the caret moves or
// the text changes, or once kept (Control+Q again) it shows what the
// caret has come to, staying as it is where that is nothing.  It goes
// with its tab, when the editor hasn't the keys, and is forgotten if a
// press closed it.  A press on it, seen here a tick later so that the
// press isn't the editor's too, goes where the part pressed goes, or
// gives the editor back the keys the press took.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::PollDocumentation(void)
{
	Ptr<CIDEEditorTab>		pTab;
	Ptr<CTextArea>			pEditor;
	SIDEDocumentationLink*	psLink;
	SIDELocation			sPlace;
	SInt32Vec2				sPoint;
	CChars					szKey;
	size					i;

	if (mpDocumentationTab.IsNull())
	{
		return;
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (mbDocumentationPressed)
	{
		mbDocumentationPressed = false;
		for (i = 0; i < masDocumentationLinks.NumElements(); i++)
		{
			psLink = masDocumentationLinks.Get(i);
			if ((muiDocumentationPressed >= psLink->uiStart) && (muiDocumentationPressed < psLink->uiStart + psLink->uiLength))
			{
				sPlace = psLink->sPlace;
				sPlace.szFileName.Init(psLink->sPlace.szFileName);
				HideDocumentation();
				mpcContent->GetDocuments()->OpenLocation(&sPlace);
				sPlace.szFileName.Kill();
				return;
			}
		}
		if ((pTab == mpDocumentationTab) && mpDocumentation->IsShown())
		{
			mpcContent->GetWindow()->GetFocus()->SetFocus(pTab->GetEditor());
		}
	}
	if (!mpDocumentation->IsShown() || (pTab != mpDocumentationTab) || (mpcContent->GetWindow()->GetFocus()->GetFocussedComponent() != pTab->GetEditor()))
	{
		HideDocumentation();
		return;
	}
	if (!mszDocumentationEntry.Empty())
	{
		//Of an entry of completion's list: it follows the selection there, and goes with the list.
		szKey.Init();
		if (!mpcContent->GetCompletion()->IsOpen() || !mpcContent->GetCompletion()->GetSelectedSymbol(&szKey, &sPoint) || (!szKey.Equals(mszDocumentationEntry) && !ShowEntryDocumentation()))
		{
			HideDocumentation();
		}
		szKey.Kill();
		return;
	}
	pEditor = pTab->GetEditor();
	if ((pEditor->GetCaretPosition() == muiDocumentationCaret) && (pEditor->GetChangeCount() == muiDocumentationChanges))
	{
		return;
	}
	if (!mbDocumentationPinned)
	{
		HideDocumentation();
		return;
	}
	if (!ShowDocumentationAt(pTab, pEditor->GetCaretPosition()))
	{
		muiDocumentationCaret = pEditor->GetCaretPosition();
		muiDocumentationChanges = pEditor->GetChangeCount();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A press on Quick Documentation, told before the press is given to
// anything: kept for the next tick (PollDocumentation).
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::DocumentationPressed(CInfoPopup* pcPopup, size uiPosition)
{
	mbDocumentationPressed = true;
	muiDocumentationPressed = uiPosition;
}


//////////////////////////////////////////////////////////////////////////
//
// For a check's script: as a press on the uiLink'th part of Quick
// Documentation that goes somewhere (0 is where the symbol is declared,
// the pages follow).  False if it isn't shown or has no such part.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::PressDocumentationLink(size uiLink)
{
	if (!IsDocumentationShown() || (uiLink >= masDocumentationLinks.NumElements()))
	{
		return false;
	}
	DocumentationPressed(&mpDocumentation, masDocumentationLinks.Get(uiLink)->uiStart);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Quick Documentation is taken away.  False if it wasn't shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::HideDocumentation(void)
{
	bool	bShown;

	bShown = IsDocumentationShown();
	mpDocumentationTab = NULL;
	mszDocumentationEntry.Set("");
	mbDocumentationPinned = false;
	mbDocumentationPressed = false;
	mpDocumentation->Hide();
	KillDocumentationLinks();
	return bShown;
}


//////////////////////////////////////////////////////////////////////////
//
// Page Up, Page Down, and Control with Up and Down in an editor: Quick
// Documentation is scrolled if it is shown with more than it shows at
// once.  Otherwise they are the editor's own.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::ScrollAction(SComponentInputEvent* psEvent)
{
	bool	bScrolled;

	bScrolled = false;
	if (IsDocumentationShown())
	{
		switch (psEvent->eCommand)
		{
		case CIC_PageUp:	bScrolled = mpDocumentation->ScrollPages(-1);	break;
		case CIC_PageDown:	bScrolled = mpDocumentation->ScrollPages(1);	break;
		case CIC_LineUp:	bScrolled = mpDocumentation->Scroll(-1);		break;
		case CIC_LineDown:	bScrolled = mpDocumentation->Scroll(1);			break;
		default:															break;
		}
	}
	if (!bScrolled)
	{
		psEvent->bPass = true;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The tool tip of the name the pointer rests on.  With the settings'
// tick, and the pointer on a name of the active editor, which has the
// keys, with no modifier held and no other popup open: once the pointer
// has stayed still on the name for IDE_TIP_DELAY its tip is shown, and
// stays while the pointer is on the name.  The caret moving or the text
// changing takes it away, and it isn't shown again (nor looked for again,
// where the name had none) until the pointer is on another name.  How
// long until it is due, -1 if nothing is waited for.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEQuickInfo::PollTip(int64 iNow)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CComponentInput*	pcInput;
	Ptr<CFocus>			pFocus;
	CInt32Rect			cBounds;
	SInt32Vec2			sPointer;
	size				uiPosition;
	size				uiStart;
	size				uiEnd;
	bool				bOver;

	if (mbTipHeld)
	{
		return -1;
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pcInput = mpcContent->GetWindow()->GetComponentInput();
	pFocus = mpcContent->GetWindow()->GetFocus();
	uiPosition = 0;
	uiStart = 0;
	sPointer.Init(0, 0);
	bOver = mbToolTips && pTab.IsNotNull() && !pcInput->IsControl() && !pcInput->IsShift() && !pcInput->IsAlt() && pFocus->IsPointerIn() &&
			(mpcContent->GetWindow()->NumPopups() == (size)(mpTip->IsShown() ? 1 : 0));
	if (bOver)
	{
		pEditor = pTab->GetEditor();
		bOver = (pFocus->GetMouseOverComponent() == pEditor) && (pFocus->GetFocussedComponent() == pEditor);
	}
	if (bOver)
	{
		pEditor->GetActualBounds(&cBounds);
		sPointer = pFocus->GetPointer();
		bOver = pEditor->FindCharacterAt(sPointer.x - cBounds.GetLeft(), sPointer.y - cBounds.GetTop(), &uiPosition) && FindIDENameAt(pEditor->GetText(), uiPosition, &uiStart, &uiEnd);
	}
	if (!bOver)
	{
		HideTip();
		mpRestTab = NULL;
		return -1;
	}

	if ((mpRestTab != pTab) || (muiRestStart != uiStart) || (muiRestChanges != pEditor->GetChangeCount()))
	{
		//Another name (or the text has changed under this one): the wait starts again.
		HideTip();
		mpRestTab = pTab;
		muiRestStart = uiStart;
		muiRestChanges = pEditor->GetChangeCount();
		muiRestCaret = pEditor->GetCaretPosition();
		msRestPointer = sPointer;
		miRestTime = iNow;
		mbRestDone = false;
	}
	else if (muiRestCaret != pEditor->GetCaretPosition())
	{
		//A key or a press moved the caret: not again on this name.
		HideTip();
		muiRestCaret = pEditor->GetCaretPosition();
		mbRestDone = true;
	}
	else if (!mbRestDone && ((msRestPointer.x != sPointer.x) || (msRestPointer.y != sPointer.y)))
	{
		msRestPointer = sPointer;
		miRestTime = iNow;
	}
	if (mbRestDone)
	{
		return -1;
	}
	if (iNow - miRestTime < IDE_TIP_DELAY)
	{
		return IDE_TIP_DELAY - (iNow - miRestTime);
	}
	mbRestDone = true;
	ShowTipAt(pTab, uiPosition);
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The tool tip of the name at uiPosition of pTab's text, shown under the
// name: its declaration, where it is declared and the first lines of the
// comment over it.  False, with no tip shown, if the name is no symbol
// the index has declared and no macro.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEQuickInfo::ShowTipAt(Ptr<CIDEEditorTab> pTab, size uiPosition)
{
	SIDESymbolDocumentation	sDocumented;
	Ptr<CTextArea>	pEditor;
	CInt32Rect		cBounds;
	CInt32Rect		cLine;
	SInt32Vec2		sPoint;
	size			uiStart;

	HideTip();
	sDocumented.Init();
	if (!FindDocumented(pTab, uiPosition, &sDocumented, &mszTipName, &uiStart))
	{
		sDocumented.Kill();
		return false;
	}
	WriteDocumented(&sDocumented, true, mpTip);
	sDocumented.Kill();
	pEditor = pTab->GetEditor();
	mpTipTab = pTab;
	mpcContent->GetWindow()->Layout();
	pEditor->GetActualBounds(&cBounds);
	pEditor->GetLineBounds(pEditor->FindLine(uiStart), &cLine);
	sPoint.Init(cBounds.GetLeft() + pEditor->GetPositionX(uiStart, false), cBounds.GetTop() + cLine.GetBottom());
	mpTip->Show(sPoint, false);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// For a check's script: what ShowTipAt showed stays wherever the pointer
// is, until it is let go.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::HoldTip(bool bHeld)
{
	mbTipHeld = bHeld;
	if (!bHeld)
	{
		HideTip();
	}
}


bool CIDEQuickInfo::HideTip(void)
{
	bool	bShown;

	bShown = IsTipShown();
	mpTipTab = NULL;
	mpTip->Hide();
	return bShown;
}


//////////////////////////////////////////////////////////////////////////
//
// The Settings page's: whether a name the pointer rests on has a tool
// tip.  Written with the settings by whoever sets it (CIDEStructure's
// <Editor>).
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::SetToolTips(bool bToolTips)
{
	mbToolTips = bToolTips;
	if (!bToolTips)
	{
		HideTip();
		mpRestTab = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Link: [Area] virtual int Area(void) = 0 | CShape,
// Shape.h:15" and "Info: parameters" with its rows under it, the caret's
// parameter in square brackets; "Documentation: [Area]" (", kept" once
// it follows the caret, ", scrolled 3" by its lines) with its lines under
// it, what goes somewhere in square brackets; "Tip: [Area] ..." on one
// line; "none" for each that isn't shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDEQuickInfo::Print(CChars* pszDest)
{
	Ptr<CIDEEditorTab>	pTab;
	CInt32Rect			cLine;
	CChars				szText;
	size				i;

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
	//"Pointer: hand" while a link has the editor's pointer, "Pointer: hand, hand over it" with the pointer there too.
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNotNull())
	{
		pszDest->Append("\nPointer: ");
		pszDest->Append(GetIDECursorName(pTab->GetEditor()->GetCursor()));
		if (mpcContent->GetWindow()->GetFocus()->GetMouseOverComponent() == pTab->GetEditor())
		{
			pszDest->Append(", ");
			pszDest->Append(GetIDECursorName(mpcContent->GetWindow()->GetNativeWindow()->GetCursor()));
			pszDest->Append(" over it");
		}
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
	//"Documentation: [Area]" (", kept" once it follows the caret) with its lines under it, the parts that go
	//somewhere in square brackets; "Tip: [Area] ..." on one line.
	pszDest->Append("\nDocumentation: ");
	if (IsDocumentationShown())
	{
		pszDest->Append('[');
		pszDest->Append(mszDocumentationName);
		pszDest->Append(mbDocumentationPinned ? "], kept" : "]");
		if ((mpDocumentation->GetScrollPosition() > 0) && mpDocumentation->GetTextArea()->GetLineBounds(0, &cLine))
		{
			pszDest->Append(", scrolled ");
			pszDest->Append((int32)(mpDocumentation->GetScrollPosition() / cLine.GetHeight()));
		}
		szText.Init(mpDocumentation->GetText());
		for (i = masDocumentationLinks.NumElements(); i >= 1; i--)
		{
			szText.Insert(masDocumentationLinks.Get(i - 1)->uiStart + masDocumentationLinks.Get(i - 1)->uiLength, "]");
			szText.Insert(masDocumentationLinks.Get(i - 1)->uiStart, "[");
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
	pszDest->Append("\nTip: ");
	if (IsTipShown())
	{
		szText.Init(mpTip->GetText());
		szText.Replace("\n", " | ");
		pszDest->Append('[');
		pszDest->Append(mszTipName);
		pszDest->Append("] ");
		pszDest->Append(szText);
		szText.Kill();
	}
	else
	{
		pszDest->Append("none");
	}
	pszDest->Append('\n');
}


bool CIDEQuickInfo::IsDocumentationShown(void) { return mpDocumentationTab.IsNotNull() && mpDocumentation->IsShown(); }
bool CIDEQuickInfo::IsDocumentationPinned(void) const { return mbDocumentationPinned; }
size CIDEQuickInfo::NumDocumentationLinks(void) const { return masDocumentationLinks.NumElements(); }
bool CIDEQuickInfo::IsTipShown(void) { return mpTipTab.IsNotNull() && mpTip->IsShown(); }
bool CIDEQuickInfo::IsToolTips(void) const { return mbToolTips; }
bool CIDEQuickInfo::IsLinkShown(void) { return mpLinkTab.IsNotNull() && mpLinkPopup->IsShown(); }
bool CIDEQuickInfo::IsInfoShown(void) { return (meInfo != IQI_None) && mpInfoPopup->IsShown(); }
Ptr<CIDEChooser> CIDEQuickInfo::GetDefinitionsChooser(void) const { return mpDefinitions; }

