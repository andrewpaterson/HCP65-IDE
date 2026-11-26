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
#include <ctype.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDEChooser.h"
#include "IDERename.h"
#include "IDESymbolInfo.h"
#include "IDECompletion.h"


#define IDE_COMPLETION_PRINTED	40		//The most rows a check is told of.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	miFileIcon = -1;
	memset(maiSymbolIcons, 0, sizeof(maiSymbolIcons));
	mcTyper.Init(pcContent);
	mcParse.Init(pcContent);
	muiAsked = 0;
	mcExpected.Init(pcContent, &mcTyper);
	mbByType = false;
	memset(&msContext, 0, sizeof(SIDECompletionContext));
	masEntries.Init();
	mbReplaceWord = false;
	mcFilledFor = 0;
	muiSeenCaret = 0;
	muiSeenChanges = 0;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpPopup);
	U_Int32(miFileIcon);
	U_Data(int32[IST_Count], maiSymbolIcons);
	U_Unknown(CIDELightTyper, mcTyper);
	U_Unknown(CIDEExpressionTyper, mcParse);
	U_Size(muiAsked);
	U_Unknown(CIDEExpectedTypes, mcExpected);
	U_Bool(mbByType);
	M_Pointer(mpTab);
	U_Data(SIDECompletionContext, msContext);
	U_Unknown(CArrayIDECompletionEntries, masEntries);
	U_Bool(mbReplaceWord);
	U_Char(mcFilledFor);
	M_Pointer(mpSeenTab);
	U_Size(muiSeenCaret);
	U_Size(muiSeenChanges);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::Free(void)
{
	if (mpPopup.IsNotNull())
	{
		mpPopup->GetChosenEvent()->Remove(this);
		mpPopup->GetClosedEvent()->Remove(this);
	}
	KillEntries();
	masEntries.Kill();
	mcExpected.Kill();
	mcParse.Kill();
	mcTyper.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE is closing: the checked parse lets go of the edits thread,
// which finishes what it has of it first.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::Detach(void)
{
	mcParse.Kill();
	mcParse.Init(mpcContent);
	muiAsked = 0;
}


bool CIDECompletion::Save(CObjectWriter* pcFile) { return false; }
bool CIDECompletion::Load(CObjectReader* pcFile) { return false; }


void CIDECompletion::KillEntries(void)
{
	size	i;

	for (i = 0; i < masEntries.NumElements(); i++)
	{
		masEntries.Get(i)->szKey.Kill();
		masEntries.Get(i)->szText.Kill();
	}
	masEntries.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// Edit's, with IntelliJ IDEA's keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Edit.CompleteCode", "Complete Code", "Control+Space", Ptr<CImageCel>(), this, &CIDECompletion::Complete)->SetEnabledCheck(this, &CIDECompletion::HasEditor);
	pCommands->Add("Edit.CompleteCodeByType", "Complete Code by Type", "Control+Shift+Space", Ptr<CImageCel>(), this, &CIDECompletion::CompleteByType)->SetEnabledCheck(this, &CIDECompletion::HasEditor);
}


void CIDECompletion::AddEditItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.CompleteCode"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.CompleteCodeByType"));
}


bool CIDECompletion::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
// An editor's keys that are the list's while it is open: they go before
// the editor's own, which don't get what the list takes.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::AddEditorActions(Ptr<CTextArea> pEditor)
{
	static const EComponentInputCommand	aeCommands[] = { CIC_Up, CIC_Down, CIC_PageUp, CIC_PageDown, CIC_Enter, CIC_Tab, CIC_ClearSelection };
	size								i;

	for (i = 0; i < sizeof(aeCommands) / sizeof(EComponentInputCommand); i++)
	{
		pEditor->AddInputAction(aeCommands[i], this, &CIDECompletion::KeyAction)->SetGoesFirst(true);
	}
}


void CIDECompletion::KeyAction(SComponentInputEvent* psEvent)
{
	if (!IsOpen())
	{
		psEvent->bPass = true;
		return;
	}
	switch (psEvent->eCommand)
	{
	case CIC_Up:				mpPopup->Up();			break;
	case CIC_Down:				mpPopup->Down();		break;
	case CIC_PageUp:			mpPopup->PageUp();		break;
	case CIC_PageDown:			mpPopup->PageDown();	break;
	case CIC_ClearSelection:	Close();				break;
	case CIC_Enter:
	case CIC_Tab:
		if (!Choose(psEvent->eCommand == CIC_Tab))
		{
			//Nothing to write: the key is the editor's.
			Close();
			psEvent->bPass = true;
		}
		break;
	default:
		psEvent->bPass = true;
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The list, made once: a chooser the editor drives, the best match first.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::MakePopup(void)
{
	if (mpPopup.IsNotNull())
	{
		return;
	}
	mpPopup = OMalloc<CChooserPopup>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), mpcContent->GetMatchFont());
	mpPopup->SetDriven(true);
	mpPopup->SetSortMatches(true);
	mpPopup->SetShorterFirst(false);  //They are added nearest in scope first.
	mpPopup->GetChosenEvent()->Add(this, &CIDECompletion::EntryChosen);
	mpPopup->GetClosedEvent()->Add(this, &CIDECompletion::PopupClosed);
	AddIDEChooserIcons(mpcContent, mpPopup, &miFileIcon, maiSymbolIcons);
}


//////////////////////////////////////////////////////////////////////////
//
// Complete Code (Control+Space): the list of what can be written at the
// caret; with one entry for what is typed of it, that entry is written at
// once.  False, the status line saying so, where there is nothing to
// list.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::Complete(void)
{
	if (!Open(false))
	{
		mpcContent->ShowStatus("Nothing is known to complete what is at the caret.");
		return false;
	}
	ChooseOnlyEntry();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The list has just been asked for: if it has one entry for what is
// typed of the name, and no more are coming, that entry is written.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::ChooseOnlyEntry(void)
{
	if (IsOpen() && !IsWorking() && (mpPopup->NumRows() == 1) && (msContext.uiPrefixStart < msContext.uiCaret))
	{
		Choose(false);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Complete Code by Type (Control+Shift+Space): the list of what can be
// written at the caret that has the type wanted there.  False, the status
// line saying which, where no type is known to be wanted or nothing has
// it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::CompleteByType(void)
{
	if (!Open(false, true))
	{
		mpcContent->ShowStatus((mcExpected.NumTypes() == 0) ? "No type is known to be wanted at the caret." : "Nothing known here has the type wanted at the caret.");
		return false;
	}
	ChooseOnlyEntry();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The list opened under the active editor's caret, its left where the
// name being written starts, filtered by what is written of it; with
// bByType, of what has a type wanted there.  False, with no list, where
// nothing is completed (a comment, a string), what is before the caret
// can't be typed, or nothing matches; bAutomatic is the list opening by
// itself, which then stays shut.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::Open(bool bAutomatic, bool bByType)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CInt32Rect			cBounds;
	SInt32Vec2			sPoint;

	Close();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetEditor()->IsEditable())
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	if (!FindIDECompletionContext(pEditor->GetText(), strlen(pEditor->GetText()), pEditor->GetCaretPosition(), &msContext))
	{
		return false;
	}
	MakePopup();
	mpTab = pTab;
	mbByType = bByType;
	KillEntries();
	mpPopup->RemoveAllItems();
	mcTyper.SetPlace(pTab, msContext.uiCaret);
	Fill();
	AskParse();
	if (((masEntries.NumElements() == 0) && !IsWorking()) || !mpcContent->GetDocuments()->GetCaretPoint(&sPoint))
	{
		Close();
		return false;
	}

	pEditor->GetActualBounds(&cBounds);
	sPoint.x = cBounds.GetLeft() + pEditor->GetPositionX(msContext.uiPrefixStart, false);
	mpPopup->Open(sPoint, pEditor);
	mpPopup->SetNote(IsWorking() ? IDE_COMPLETION_WORKING : "");
	ShowPrefix();
	SeeEditor();
	if ((mpPopup->NumRows() == 0) && !IsWorking())
	{
		Close();
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The checked parse is asked what is before the "." or "->" being
// completed after, in a file a project has.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::AskParse(void)
{
	muiAsked = 0;
	if (!mbByType && (msContext.eKind == ICK_Member) && (msContext.uiChainStart < msContext.uiChainEnd) && (CIDEIcons::GetFileType(mpTab->GetFileName()) != IFT_Other))
	{
		muiAsked = mcParse.Ask(mpTab, msContext.uiChainStart, msContext.uiChainEnd, msContext.uiWordEnd, msContext.bArrow);
	}
}


bool CIDECompletion::IsWorking(void) { return (muiAsked != 0) && mcParse.IsAsking(); }


//////////////////////////////////////////////////////////////////////////
//
// The checked parse has answered for this list: the fields and functions
// of what it says the expression is are added, those that aren't listed
// already, and the list is no longer working.  It closes if it has
// nothing after all.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::TakeParse(void)
{
	CArrayIDEScopeSymbols	asSymbols;
	size					uiAdded;

	muiAsked = 0;
	uiAdded = 0;
	if (mcParse.GetKey()[0] != '\0')
	{
		asSymbols.Init();
		mcTyper.FindMembers(mcParse.GetKey(), &asSymbols);
		uiAdded = FillSymbols(&asSymbols, true);
		KillIDEScopeSymbols(&asSymbols);
	}
	mpPopup->SetNote("");
	if (uiAdded > 0)
	{
		mpPopup->ItemsChanged();
	}
	if (mpPopup->NumRows() == 0)
	{
		Close();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What is written so far of the name being completed.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::GetPrefix(CChars* pszDest)
{
	pszDest->Append(&mpTab->GetEditor()->GetText()[msContext.uiPrefixStart], msContext.uiCaret - msContext.uiPrefixStart);
}


//////////////////////////////////////////////////////////////////////////
//
// The first character of what is written of the name, in lower case; 0
// if none is.
//
//////////////////////////////////////////////////////////////////////////
char CIDECompletion::GetFirstCharacter(void)
{
	if (msContext.uiPrefixStart >= msContext.uiCaret)
	{
		return 0;
	}
	return (char)tolower((unsigned char)mpTab->GetEditor()->GetText()[msContext.uiPrefixStart]);
}


//////////////////////////////////////////////////////////////////////////
//
// The list is filtered by what is written of the name now.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::ShowPrefix(void)
{
	CChars	szPrefix;

	szPrefix.Init();
	GetPrefix(&szPrefix);
	mpPopup->SetFilterText(szPrefix.Text());
	szPrefix.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The entries for what is being completed, found for the first character
// written of the name (mcFilledFor).  False if there are none to give:
// the light typer can't say what is before a "." or "->".
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::Fill(void)
{
	CArrayIDEScopeSymbols	asSymbols;
	SIDELightType			sType;
	const char*				szText;
	size					uiMaximum;
	bool					bResult;

	szText = mpTab->GetEditor()->GetText();
	bResult = true;
	mcFilledFor = GetFirstCharacter();
	uiMaximum = (mcFilledFor == 0) ? IDE_COMPLETION_UNTYPED : 0;
	if (mbByType)
	{
		//What is wanted is by what is before the whole expression being written, and only the nearest are looked at.
		if ((msContext.eKind == ICK_Include) || !mcExpected.Find(szText, strlen(szText), (msContext.eKind == ICK_Name) ? msContext.uiPrefixStart : msContext.uiChainStart))
		{
			return false;
		}
		uiMaximum = IDE_COMPLETION_SMART;
	}
	sType.Init();
	asSymbols.Init();
	switch (msContext.eKind)
	{
	case ICK_Name:
		mcTyper.FindVisible(&asSymbols, NULL, 0, mcFilledFor, uiMaximum);
		FillSymbols(&asSymbols, false);
		if (!mbByType)
		{
			FillKeywords();
		}
		break;
	case ICK_Member:
		bResult = mcTyper.TypeOfChain(szText, msContext.uiChainStart, msContext.uiChainEnd, &sType) && mcTyper.Dereference(&sType, msContext.bArrow) && !sType.szKey.Empty();
		if (bResult)
		{
			mcTyper.FindMembers(sType.szKey.Text(), &asSymbols);
			FillSymbols(&asSymbols, true);
		}
		break;
	case ICK_Scope:
		bResult = (msContext.uiChainStart == msContext.uiChainEnd) || mcTyper.TypeOfChain(szText, msContext.uiChainStart, msContext.uiChainEnd, &sType);
		if (bResult)
		{
			mcTyper.FindMembers(sType.szKey.Text(), &asSymbols, NULL, 0, mcFilledFor, uiMaximum);
			FillSymbols(&asSymbols, false);
		}
		break;
	case ICK_Include:
		FillIncludes();
		break;
	default:
		bResult = false;
		break;
	}
	KillIDEScopeSymbols(&asSymbols);
	sType.Kill();
	return bResult && (masEntries.NumElements() > 0);
}


//////////////////////////////////////////////////////////////////////////
//
// An entry for each of pasSymbols that can be written: not a constructor,
// a destructor or an operator, and after "." or "->" (bMembersOnly) only
// fields and functions.  One that is written as another already is (a
// function a base class has too) is left out, and so is one that has an
// entry already, when the list is being added to.  How many were added.  A function's row has its
// parameters; a row's detail is the type its symbol is declared with
// for the first IDE_COMPLETION_TYPED, whose declarations are read, and
// the scope it is in for the rest.
//
//////////////////////////////////////////////////////////////////////////
size CIDECompletion::FillSymbols(CArrayIDEScopeSymbols* pasSymbols, bool bMembersOnly)
{
	CArrayInt	aiFits;
	size		uiAdded;
	size		i;

	if (!mbByType)
	{
		return AddSymbols(pasSymbols, bMembersOnly, NULL, ITF_None);
	}

	//By type: what is exactly a type wanted, then what converts to one.  Each symbol's fit is worked out once.
	aiFits.Init();
	for (i = 0; i < pasSymbols->NumElements(); i++)
	{
		aiFits.Add((int)mcExpected.GetFit(pasSymbols->Get(i)));
	}
	uiAdded = AddSymbols(pasSymbols, bMembersOnly, &aiFits, ITF_Same);
	uiAdded += AddSymbols(pasSymbols, bMembersOnly, &aiFits, ITF_Converts);
	aiFits.Kill();
	return uiAdded;
}


//////////////////////////////////////////////////////////////////////////
//
// The same, of those of pasSymbols whose fit (paiFits, one for each) is
// eFit; of all of them without paiFits.
//
//////////////////////////////////////////////////////////////////////////
size CIDECompletion::AddSymbols(CArrayIDEScopeSymbols* pasSymbols, bool bMembersOnly, CArrayInt* paiFits, EIDETypeFit eFit)
{
	SIDEScopeSymbol*	psSymbol;
	SIDESymbolInfo		sInfo;
	CArrayChars			aszWritten;
	CArrayIDEClassAccesses	acAccesses;
	CChars				szRow;
	CChars				szDetail;
	CChars				szClass;
	EIDECompletionWrite	eWrite;
	size				uiFirst;
	size				i;
	size				j;
	int					iIcon;
	bool				bFunction;
	bool				bUnnamed;
	bool				bRead;

	uiFirst = masEntries.NumElements();
	aszWritten.Init();
	acAccesses.Init();
	for (i = 0; i < pasSymbols->NumElements(); i++)
	{
		psSymbol = pasSymbols->Get(i);
		if (paiFits && (paiFits->GetValue(i) != (int)eFit))
		{
			continue;
		}
		bFunction = (psSymbol->uiKind == CPPSK_Function) || (psSymbol->uiKind == CPPSK_FunctionTemplate);
		if (bMembersOnly && !bFunction && (psSymbol->uiKind != CPPSK_Field) && (psSymbol->uiKind != CPPSK_Variable))
		{
			continue;
		}
		if (((psSymbol->uiKind != CPPSK_Enumerator) && !IsAccessible(psSymbol, &acAccesses)) || ((uiFirst > 0) && HasEntry(psSymbol->szKey.Text())))
		{
			continue;
		}
		if (bFunction)
		{
			//Not written by name: a constructor (named as its class is), a destructor, an operator.
			GetIDESymbolName(psSymbol->szScope.Text(), &szClass);
			bUnnamed = szClass.Equals(psSymbol->szName) || (psSymbol->szName.Text()[0] == '~') || ((strncmp(psSymbol->szName.Text(), "operator", 8) == 0) && !IsIDENameChar(psSymbol->szName.Text()[8]));
			szClass.Kill();
			if (bUnnamed)
			{
				continue;
			}
		}

		//What a scope outside any declares is declared once; a class's members and a function's locals may not be.
		szRow.Init(psSymbol->szName);
		szRow.Append(psSymbol->szParameters);
		if ((!psSymbol->szScope.Empty() || (psSymbol->szKey.Text()[0] == '@')) && (aszWritten.NumElements() < IDE_COMPLETION_UNIQUE))
		{
			if (aszWritten.GetIndex(szRow.Text()) != ARRAY_ELEMENT_NOT_FOUND)
			{
				szRow.Kill();
				continue;
			}
			aszWritten.Add(szRow.Text());
		}

		//Its declaration, for its type and a function's parameters as they are written.
		szDetail.Init();
		sInfo.Init();
		bRead = (masEntries.NumElements() < IDE_COMPLETION_TYPED) && ReadIDESymbolInfo(mpcContent, psSymbol->szKey.Text(), &sInfo);
		if (bRead && ((psSymbol->uiKind == CPPSK_Variable) || (psSymbol->uiKind == CPPSK_Field) || (psSymbol->uiKind == CPPSK_Parameter) || bFunction))
		{
			AppendIDEDeclaredType(&sInfo, &szDetail);
		}
		else
		{
			szDetail.Append(psSymbol->szScope);
		}
		eWrite = ICW_Name;
		if (bFunction)
		{
			eWrite = (psSymbol->szParameters.Equals("()") || psSymbol->szParameters.Equals("(void)") || psSymbol->szParameters.StartsWith("() ") || psSymbol->szParameters.StartsWith("(void) ")) ? ICW_Call : ICW_CallArguments;
			if (bRead)
			{
				szRow.Set(psSymbol->szName);
				szRow.Append('(');
				for (j = 0; j < sInfo.aszParameters.NumElements(); j++)
				{
					szRow.Append((j > 0) ? ", " : "");
					szRow.Append(sInfo.aszParameters.GetText(j));
				}
				szRow.Append(')');
			}
		}
		iIcon = GetIDESymbolIconType(psSymbol->uiKind);
		AddEntry(szRow.Text(), szDetail.Text(), (iIcon >= 0) ? maiSymbolIcons[iIcon] : maiSymbolIcons[IST_Variable], psSymbol->szName.Text(), eWrite, psSymbol->szKey.Text());
		sInfo.Kill();
		szDetail.Kill();
		szRow.Kill();
	}
	for (i = 0; i < acAccesses.NumElements(); i++)
	{
		acAccesses.Get(i)->Kill();
	}
	acAccesses.Kill();
	aszWritten.Kill();
	return masEntries.NumElements() - uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the symbol szKey has an entry already.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::HasEntry(const char* szKey)
{
	size	i;

	for (i = 0; i < masEntries.NumElements(); i++)
	{
		if (masEntries.Get(i)->szKey.Equals(szKey))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether what a class declares can be used where the caret is: anything
// public; anything at all in the class's own members; what is protected
// in the members of a class derived from it.  What isn't a class's is
// always.  pacAccesses keeps each class's access once it is read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::IsAccessible(SIDEScopeSymbol* psSymbol, CArrayIDEClassAccesses* pacAccesses)
{
	CIDEClassAccess*	pcAccess;
	EIDEAccess			eAccess;
	size				i;

	if (psSymbol->szScope.Empty() || psSymbol->szScope.Equals(mcTyper.GetClass()))
	{
		return true;
	}
	pcAccess = NULL;
	for (i = 0; (pcAccess == NULL) && (i < pacAccesses->NumElements()); i++)
	{
		if (psSymbol->szScope.Equals(pacAccesses->Get(i)->GetClass()))
		{
			pcAccess = pacAccesses->Get(i);
		}
	}
	if (pcAccess == NULL)
	{
		pcAccess = pacAccesses->Add();
		pcAccess->Init();
		pcAccess->Read(mpcContent, psSymbol->szScope.Text());
	}
	eAccess = pcAccess->GetAccess(&psSymbol->sDeclared);
	if (eAccess == IA_Public)
	{
		return true;
	}

	//Protected: the caret's class has the symbol's among its bases, which are the scopes after it before any other.
	return (eAccess == IA_Protected) && (mcTyper.GetClass()[0] != '\0') && (mcTyper.GetScopes()->GetIndex(psSymbol->szScope.Text()) != ARRAY_ELEMENT_NOT_FOUND);
}


//////////////////////////////////////////////////////////////////////////
//
// An #include's name: the folders, then the headers, that the include
// directories have after the folders written so far.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::FillIncludes(void)
{
	CArrayChars	aszFiles;
	CArrayChars	aszFolders;
	CChars		szFolder;
	size		i;

	szFolder.Init();
	szFolder.Append(&mpTab->GetEditor()->GetText()[msContext.uiIncludeStart], msContext.uiPrefixStart - msContext.uiIncludeStart);
	aszFiles.Init();
	aszFolders.Init();
	mpcContent->GetCodeModel()->FindIncludeNames(mpTab->GetFileName(), szFolder.Text(), msContext.bSystemInclude, &aszFiles, &aszFolders);
	for (i = 0; i < aszFolders.NumElements(); i++)
	{
		AddEntry(aszFolders.GetText(i), "folder", -1, aszFolders.GetText(i), ICW_Folder, "");
	}
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		AddEntry(aszFiles.GetText(i), "", (miFileIcon < 0) ? -1 : miFileIcon + (int32)IFT_Header, aszFiles.GetText(i), ICW_Name, "");
	}
	aszFolders.Kill();
	aszFiles.Kill();
	szFolder.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The language's words, after everything that is declared.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::FillKeywords(void)
{
	size	i;

	for (i = 0; i < NumIDECompletionKeywords(); i++)
	{
		AddEntry(GetIDECompletionKeyword(i), "", -1, GetIDECompletionKeyword(i), ICW_Name, "");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// An entry and its row: szName is what the row shows, which starts with
// szText, what is written and what the name being typed is matched with.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::AddEntry(const char* szName, const char* szDetail, int32 iIcon, const char* szText, EIDECompletionWrite eWrite, const char* szKey)
{
	SIDECompletionEntry*	psEntry;

	psEntry = masEntries.Add();
	psEntry->szText.Init(szText);
	psEntry->eWrite = eWrite;
	psEntry->szKey.Init(szKey);
	mpPopup->AddItem(szName, szDetail, iIcon, masEntries.NumElements() - 1, strlen(szText));
}


//////////////////////////////////////////////////////////////////////////
//
// Enter or Tab: the selected entry is chosen, and written (EntryChosen)
// in place of what is typed of the name, or with bReplaceWord of all of
// the name the caret is in.  False if no row is selected.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::Choose(bool bReplaceWord)
{
	if (!IsOpen())
	{
		return false;
	}
	mbReplaceWord = bReplaceWord;
	return mpPopup->ChooseSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol the selected entry is (its key), and the point beside the
// list, at its top right, where something about it can be shown.  False
// if the list is closed, nothing is selected or the entry is no symbol.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECompletion::GetSelectedSymbol(CChars* pszKey, SInt32Vec2* psBeside)
{
	SGoToItem*	psItem;
	CInt32Rect	cBounds;

	psItem = IsOpen() ? mpPopup->GetSelectedItem() : NULL;
	if ((psItem == NULL) || (psItem->uiData >= masEntries.NumElements()) || masEntries.Get(psItem->uiData)->szKey.Empty())
	{
		return false;
	}
	pszKey->Set(masEntries.Get(psItem->uiData)->szKey);
	mpcContent->GetWindow()->Layout();
	mpPopup->GetPopup()->GetActualBounds(&cBounds);
	psBeside->Init(cBounds.GetRight() + 2, cBounds.GetTop() - INFO_POPUP_GAP);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// An entry was chosen, by a key or a press on its row; the list has
// closed.  It is written where the name being completed is: a function
// with its brackets unless one follows already, the caret between them
// if it takes anything, and its parameters shown; a folder with its
// slash, and the list again for what it has; a header with the quote or
// bracket that ends its #include's name, unless it is there.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::EntryChosen(void)
{
	Ptr<CIDEEditorTab>		pTab;
	Ptr<CTextArea>			pEditor;
	SIDECompletionEntry*	psEntry;
	SIDECompletionContext	sNow;
	SGoToItem*				psItem;
	STextRange				sRange;
	CChars					szText;
	CChars					szKey;
	EIDECompletionWrite		eWrite;
	size					uiCaret;
	bool					bReplaceWord;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	psItem = mpPopup->GetChosen();
	bReplaceWord = mbReplaceWord;
	mbReplaceWord = false;
	if (pTab.IsNull() || (psItem == NULL) || (psItem->uiData >= masEntries.NumElements()))
	{
		return;
	}
	pEditor = pTab->GetEditor();
	psEntry = masEntries.Get(psItem->uiData);
	eWrite = psEntry->eWrite;

	sRange.uiStart = msContext.uiPrefixStart;
	sRange.uiEnd = pEditor->GetCaretPosition();
	if (bReplaceWord && FindIDECompletionContext(pEditor->GetText(), strlen(pEditor->GetText()), pEditor->GetCaretPosition(), &sNow))
	{
		sRange.uiEnd = sNow.uiWordEnd;
	}
	if (sRange.uiEnd < sRange.uiStart)
	{
		return;
	}

	szKey.Init(psEntry->szKey);
	szText.Init(psEntry->szText);
	uiCaret = sRange.uiStart + szText.Length();
	if (eWrite == ICW_Folder)
	{
		szText.Append('/');
		uiCaret++;
	}
	else if (((eWrite == ICW_Call) || (eWrite == ICW_CallArguments)) && (pEditor->GetText()[sRange.uiEnd] != '('))
	{
		szText.Append("()");
		uiCaret += (eWrite == ICW_Call) ? 2 : 1;
	}
	else if (eWrite == ICW_CallArguments)
	{
		//Its bracket is there already: the caret goes after it.
		uiCaret++;
	}
	else if ((msContext.eKind == ICK_Include) && (pEditor->GetText()[sRange.uiEnd] != (msContext.bSystemInclude ? '>' : '"')))
	{
		//A header ends its #include's name.
		szText.Append(msContext.bSystemInclude ? '>' : '"');
		uiCaret++;
	}
	pEditor->ReplaceRanges(&sRange, 1, szText.Text());
	pEditor->SetCaretPosition(uiCaret);
	szText.Kill();
	SeeEditor();

	if (eWrite == ICW_CallArguments)
	{
		mpcContent->GetQuickInfo()->ShowParameterInfoOf(szKey.Text());
	}
	else if (eWrite == ICW_Folder)
	{
		Open(true);
	}
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::Close(void)
{
	if (mpPopup.IsNotNull() && mpPopup->IsOpen())
	{
		mpPopup->Close();
	}
	PopupClosed();
}


//////////////////////////////////////////////////////////////////////////
//
// The list has gone, however it went: the checked parse's answer isn't
// wanted.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::PopupClosed(void)
{
	mpTab = NULL;
	if (muiAsked != 0)
	{
		muiAsked = 0;
		mcParse.Forget();
	}
}


bool CIDECompletion::IsOpen(void) { return mpTab.IsNotNull() && mpPopup.IsNotNull() && mpPopup->IsOpen(); }


void CIDECompletion::TabClosed(Ptr<CIDEEditorTab> pTab)
{
	if (mpTab == pTab)
	{
		Close();
	}
	if (mpSeenTab == pTab)
	{
		mpSeenTab = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The active editor as it is now is what the next tick compares with.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::SeeEditor(void)
{
	mpSeenTab = mpcContent->GetDocuments()->GetActiveTab();
	if (mpSeenTab.IsNotNull())
	{
		muiSeenCaret = mpSeenTab->GetEditor()->GetCaretPosition();
		muiSeenChanges = mpSeenTab->GetEditor()->GetChangeCount();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick.  Open, the list follows what is typed: it is filtered
// by the name as it is now, and closes when the caret has left the name,
// nothing matches it, or the editor hasn't the keys.  Closed, it opens by
// itself when the character just typed ends a "." a "->" or a "::".  The
// checked parse's answer is taken when it has come.  How long until the
// next look for it, -1 when none is waited for.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDECompletion::Poll(void)
{
	Ptr<CIDEEditorTab>		pTab;
	Ptr<CTextArea>			pEditor;
	SIDECompletionContext	sNow;
	const char*				szText;
	size					uiCaret;
	bool					bTyped;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (mpTab.IsNotNull() && (!mpPopup->IsOpen() || (pTab != mpTab) || (mpcContent->GetWindow()->GetFocus()->GetFocussedComponent() != pTab->GetEditor())))
	{
		Close();
	}
	if (mcParse.Poll() && IsOpen() && (muiAsked != 0) && (mcParse.GetAnswered() == muiAsked))
	{
		TakeParse();
	}
	if (pTab.IsNull())
	{
		mpSeenTab = NULL;
		return -1;
	}
	pEditor = pTab->GetEditor();
	uiCaret = pEditor->GetCaretPosition();
	if ((pTab == mpSeenTab) && (uiCaret == muiSeenCaret) && (pEditor->GetChangeCount() == muiSeenChanges))
	{
		return IsWorking() ? IDE_COMPLETION_POLL : -1;
	}
	bTyped = (pTab == mpSeenTab) && (uiCaret == muiSeenCaret + 1) && (pEditor->GetChangeCount() != muiSeenChanges);
	SeeEditor();
	szText = pEditor->GetText();

	if (IsOpen())
	{
		if (FindIDECompletionContext(szText, strlen(szText), uiCaret, &sNow) && (sNow.eKind == msContext.eKind) && (sNow.uiPrefixStart == msContext.uiPrefixStart))
		{
			msContext = sNow;
			if (((msContext.eKind == ICK_Name) || (msContext.eKind == ICK_Scope)) && (GetFirstCharacter() != mcFilledFor))
			{
				//The entries were found for another first character: for this one, again.
				KillEntries();
				mpPopup->RemoveAllItems();
				Fill();
				mpPopup->ItemsChanged();
			}
			ShowPrefix();
			if ((mpPopup->NumRows() > 0) || IsWorking())
			{
				return IsWorking() ? IDE_COMPLETION_POLL : -1;
			}
		}
		Close();
		return -1;
	}

	if (bTyped && (uiCaret >= 2) && ((szText[uiCaret - 1] == '.') || ((szText[uiCaret - 1] == '>') && (szText[uiCaret - 2] == '-')) || ((szText[uiCaret - 1] == ':') && (szText[uiCaret - 2] == ':'))))
	{
		Open(true);
	}
	return IsWorking() ? IDE_COMPLETION_POLL : -1;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Completion: none", or what is being completed, what is
// written of it (and the types wanted, by type: "name of CWheel* [pc]")
// and how many rows match ("Completion: member [Get] 2
// rows", ", working" after it while the checked parse is waited for) with
// the rows under it, the selected one marked, each its name
// and its detail: "  > GetValue() | int".  At most
// IDE_COMPLETION_PRINTED rows.
//
//////////////////////////////////////////////////////////////////////////
void CIDECompletion::Print(CChars* pszDest)
{
	static const char*	aszKinds[] = { "none", "name", "member", "scope", "include" };
	SGoToItem*			psItem;
	size				i;

	pszDest->Append("Completion: ");
	if (!IsOpen())
	{
		pszDest->Append("none\n");
		return;
	}
	pszDest->Append(aszKinds[msContext.eKind]);
	if (mbByType)
	{
		pszDest->Append(" of ");
		mcExpected.Print(pszDest);
	}
	pszDest->Append(" [");
	GetPrefix(pszDest);
	pszDest->Append("] ");
	pszDest->Append((int32)mpPopup->NumRows());
	pszDest->Append(IsWorking() ? " rows, working\n" : " rows\n");
	for (i = 0; (i < mpPopup->NumRows()) && (i < IDE_COMPLETION_PRINTED); i++)
	{
		psItem = mpPopup->GetRowItem(i);
		pszDest->Append(((int32)i == mpPopup->GetSelectedRow()) ? "  > " : "    ");
		pszDest->Append(psItem->szName);
		if (!psItem->szLocation.Empty())
		{
			pszDest->Append(" | ");
			pszDest->Append(psItem->szLocation);
		}
		pszDest->Append('\n');
	}
}
