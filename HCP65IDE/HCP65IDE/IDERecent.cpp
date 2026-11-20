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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/DebugOutput.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLWriter.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"
#include "IDERecent.h"


#define IDE_RECENT_FILE_TAG		"RecentFile"
#define IDE_RECENT_NOT_TOOL		-1


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	masFiles.Init();
	maszClosed.Init();
	masEdits.Init();
	muiEdit = 0;
	meChooser = IRC_None;
	maiChoices.Init();
	mbChangedOnly = false;
	mbDirty = false;
	miDirty = 0;
	mbLettingGo = false;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CArrayIDERecentFiles, masFiles);
	U_Unknown(CArrayChars, maszClosed);
	U_Unknown(CArrayIDENavigationPlaces, masEdits);
	U_Size(muiEdit);
	M_Pointer(mpChooser);
	U_Enum(meChooser);
	U_Unknown(CArrayInt, maiChoices);
	U_Bool(mbChangedOnly);
	U_Bool(mbDirty);
	U_Int64(miDirty);
	U_Bool(mbLettingGo);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::Free(void)
{
	size	i;

	if (mpChooser.IsNotNull())
	{
		mpChooser->GetChosenEvent()->Remove(this);
		mpChooser->GetButtonClickEvent()->Remove(this);
	}
	RemoveAll();
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		masEdits.Get(i)->Kill();
	}
	maiChoices.Kill();
	masEdits.Kill();
	maszClosed.Kill();
	masFiles.Kill();
}


bool CIDERecent::Save(CObjectWriter* pcFile) { return false; }
bool CIDERecent::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Navigate's and Window's, with IntelliJ IDEA's keys.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.RecentFiles", "Recent Files", "Control+E", Ptr<CImageCel>(), this, &CIDERecent::ShowRecentFiles);
	pCommands->Add("Navigate.RecentLocations", "Recent Locations", "Control+Shift+E", Ptr<CImageCel>(), this, &CIDERecent::ShowRecentLocations);
	pCommands->Add("Navigate.LastEditLocation", "Last Edit Location", "Control+Shift+Backspace", Ptr<CImageCel>(), this, &CIDERecent::GoToLastEdit);
	pCommands->Add("Navigate.NextEditLocation", "Next Edit Location", "", Ptr<CImageCel>(), this, &CIDERecent::GoToNextEdit);
	pCommands->Add("Navigate.SelectIn", "Select In...", "Alt+F1", Ptr<CImageCel>(), this, &CIDERecent::ShowSelectIn)->SetEnabledCheck(this, &CIDERecent::HasEditor);
	pCommands->Add("Window.Switcher", "Switcher", "Control+Tab", Ptr<CImageCel>(), this, &CIDERecent::ShowSwitcher);
	pCommands->Add("Window.SwitcherBackward", "Switcher Backward", "Control+Shift+Tab", Ptr<CImageCel>(), this, &CIDERecent::ShowSwitcherBackward);
	pCommands->Add("Window.NextTab", "Select Next Tab", "Alt+Cursor Right", Ptr<CImageCel>(), this, &CIDERecent::SelectNextTab)->SetEnabledCheck(this, &CIDERecent::HasEditor);
	pCommands->Add("Window.PreviousTab", "Select Previous Tab", "Alt+Cursor Left", Ptr<CImageCel>(), this, &CIDERecent::SelectPreviousTab)->SetEnabledCheck(this, &CIDERecent::HasEditor);
	pCommands->Add("Window.ReopenClosedTab", "Reopen Closed Tab", "", Ptr<CImageCel>(), this, &CIDERecent::ReopenClosedTab)->SetEnabledCheck(this, &CIDERecent::CanReopenClosedTab);
}


void CIDERecent::AddNavigateItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.RecentFiles"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.RecentLocations"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.LastEditLocation"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.NextEditLocation"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.SelectIn"));
}


void CIDERecent::AddWindowItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("Window.Switcher"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Window.NextTab"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Window.PreviousTab"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("Window.ReopenClosedTab"));
}


bool CIDERecent::HasEditor(void) { return mpcContent->GetDocuments()->GetActiveTab().IsNotNull(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int32 CIDERecent::FindFile(const char* szFile)
{
	size	i;

	for (i = 0; i < masFiles.NumElements(); i++)
	{
		if (masFiles.Get(i)->szFile.Equals(szFile))
		{
			return (int32)i;
		}
	}
	return -1;
}


void CIDERecent::RemoveAll(void)
{
	size	i;

	for (i = 0; i < masFiles.NumElements(); i++)
	{
		masFiles.Get(i)->szFile.Kill();
	}
	masFiles.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// A file is remembered with where its caret is: the most recent
// (bFront) or where it was in the list, the last if it wasn't.  The
// oldest go once there are too many.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::Remember(const char* szFile, int32 iLine, int32 iColumn, bool bFront)
{
	SIDERecentFile*	psFile;
	SIDERecentFile	sFile;
	int32			iFile;

	iFile = FindFile(szFile);
	if (iFile >= 0)
	{
		psFile = masFiles.Get((size)iFile);
		psFile->iLine = iLine;
		psFile->iColumn = iColumn;
		if (bFront && (iFile > 0))
		{
			memcpy(&sFile, psFile, sizeof(SIDERecentFile));
			masFiles.RemoveAt((size)iFile);
			masFiles.InsertAt(&sFile, 0);
		}
		return;
	}
	sFile.szFile.Init(szFile);
	sFile.iLine = iLine;
	sFile.iColumn = iColumn;
	masFiles.InsertAt(&sFile, bFront ? 0 : masFiles.NumElements());
	while (masFiles.NumElements() > IDE_RECENT_FILES)
	{
		masFiles.Tail()->szFile.Kill();
		masFiles.RemoveAt(masFiles.NumElements() - 1);
	}
}


void CIDERecent::RecentChanged(void)
{
	if (!mbDirty)
	{
		mbDirty = true;
		miDirty = GetDebugTickCount();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A tab was chosen (CIDEContent::DocumentSelected): its file is the most
// recent.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::FileShown(Ptr<CIDEEditorTab> pTab)
{
	int	iLine;
	int	iColumn;

	if (pTab.IsNull() || mbLettingGo)
	{
		return;
	}
	if ((masFiles.NumElements() > 0) && masFiles.Get(0)->szFile.Equals(pTab->GetFileName()))
	{
		return;
	}
	if (!pTab->GetCaretPlace(&iLine, &iColumn))
	{
		iLine = 0;
		iColumn = 0;
	}
	Remember(pTab->GetFileName(), iLine, iColumn, true);
	RecentChanged();
}


//////////////////////////////////////////////////////////////////////////
//
// A tab is closing (CIDEContent::PaneClosed): where its caret is, is
// where it reopens, and it is the last closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::FileClosed(Ptr<CIDEEditorTab> pTab)
{
	int	iLine;
	int	iColumn;

	if (pTab.IsNull() || mbLettingGo)
	{
		return;
	}
	if (!pTab->GetCaretPlace(&iLine, &iColumn))
	{
		iLine = 0;
		iColumn = 0;
	}
	Remember(pTab->GetFileName(), iLine, iColumn, false);
	maszClosed.Add(pTab->GetFileName());
	RecentChanged();
}


//////////////////////////////////////////////////////////////////////////
//
// An edit of an open file (CIDEDocuments::DocumentChanged), after the
// places have followed it: where it ended is a place edited, the last one
// again if it is near it.  Last Edit Location starts from the newest.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::FileEdited(const char* szFile, SIDEPlaceChange* psChange)
{
	SIDENavigationPlace*	psPlace;
	int32					iLine;
	int32					iColumn;

	//Where the caret is left: after text put in, or where text was taken out.
	iLine = (int32)psChange->psChange->uiLine;
	iColumn = psChange->iColumn;
	if (psChange->psChange->eChange == TDC_Inserted)
	{
		iLine = (int32)psChange->psChange->LastLine();
		iColumn = psChange->iLastLength;
	}
	psPlace = (masEdits.NumElements() > 0) ? masEdits.Tail() : NULL;
	if ((psPlace == NULL) || !psPlace->szFile.Equals(szFile) || (psPlace->iLine > iLine + IDE_RECENT_EDIT_LINES) || (iLine > psPlace->iLine + IDE_RECENT_EDIT_LINES))
	{
		if (masEdits.NumElements() >= IDE_RECENT_EDITS)
		{
			masEdits.Get(0)->Kill();
			masEdits.RemoveAt(0);
		}
		psPlace = masEdits.Add();
		psPlace->Init();
		psPlace->szFile.Set(szFile);
		psPlace->uiProject = mpcContent->GetCodeModel()->FindProjectOfFile(szFile);
	}
	psPlace->iLine = iLine;
	psPlace->iColumn = iColumn;
	muiEdit = masEdits.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// An edit of an open file: the places edited in it move with their text.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange)
{
	SIDENavigationPlace*	psPlace;
	size					i;

	for (i = 0; i < masEdits.NumElements(); i++)
	{
		psPlace = masEdits.Get(i);
		if (psPlace->szFile.Equals(szFile))
		{
			AdjustIDEPlace(&psPlace->iLine, &psPlace->iColumn, psChange);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace is about to close its files: the user's file is written
// with the open ones' carets where they are (WriteUser), and the tabs'
// closing isn't remembered as the user's (until Clear).
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::LetGo(void)
{
	if ((masFiles.NumElements() > 0) && mpcContent->IsKeepingSettings())
	{
		mpcContent->GetUserFile()->Write();
	}
	mbDirty = false;
	mbLettingGo = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions have gone: so has where the user was in them.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::Clear(void)
{
	size	i;

	RemoveAll();
	maszClosed.ReInit();
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		masEdits.Get(i)->Kill();
	}
	masEdits.ReInit();
	muiEdit = 0;
	mbDirty = false;
	mbLettingGo = false;
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: the user's file is written a while after the recent
// files last changed, where settings are kept.  Milliseconds until it
// should be looked at again, -1 if not until something changes.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDERecent::Poll(int64 iNow)
{
	if (!mbDirty)
	{
		return -1;
	}
	if (iNow - miDirty < IDE_RECENT_WRITE)
	{
		return IDE_RECENT_WRITE - (iNow - miDirty);
	}
	mbDirty = false;
	if (mpcContent->IsKeepingSettings())
	{
		mpcContent->GetUserFile()->Write();
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The chooser, begun for one of its uses.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::BeginChooser(EIDERecentChooser eChooser, const char* szTitle, bool bFilter, bool bPreview, const char* szButton)
{
	if (mpChooser.IsNull())
	{
		mpChooser = OMalloc<CIDEChooser>(mpcContent);
		mpChooser->GetChosenEvent()->Add(this, &CIDERecent::Chosen);
		mpChooser->GetButtonClickEvent()->Add(this, &CIDERecent::ButtonClicked);
	}
	meChooser = eChooser;
	maiChoices.ReInit();
	mpChooser->Begin(szTitle, bFilter, bPreview, szButton);
	mpChooser->SetSelectName(false);
	mpChooser->SetGoesToChosen((eChooser == IRC_Files) || (eChooser == IRC_Locations));
}


//////////////////////////////////////////////////////////////////////////
//
// Opened in the middle of the window's top, over whatever has the keys.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::OpenChooser(size uiSelected)
{
	Ptr<CComponent>	pOwner;
	SInt32Vec2		sPosition;
	SInt32Vec2		sSize;

	sSize = mpcContent->GetWindow()->GetCanvas()->GetActualSize();
	sPosition.Init(sSize.x / 4, sSize.y / 6);
	pOwner = mpcContent->GetWindow()->GetFocus()->GetFocussedComponent();
	if (pOwner.IsNull() && mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
	{
		pOwner = mpcContent->GetDocuments()->GetActiveTab()->GetEditor();
	}
	return mpChooser->OpenAt(sPosition, pOwner, uiSelected);
}


//////////////////////////////////////////////////////////////////////////
//
// A row for a file: its name, the folder it is in after it, and where its
// caret goes.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::AddFilePlace(const char* szFile, int32 iLine, int32 iColumn)
{
	SIDELocation	sLocation;
	CChars			szWhere;
	const char*		szName;

	szName = GetIDEFileNamePart(szFile);
	szWhere.Init();
	if (szName > szFile + 1)
	{
		szWhere.Append(szFile, (size)(szName - szFile) - 1);
	}
	memset(&sLocation, 0, sizeof(SIDELocation));
	sLocation.szFileName.Init(szFile);
	sLocation.iLine = iLine;
	sLocation.iColumn = iColumn;
	sLocation.iContainer = -1;
	mpChooser->AddPlace(szName, szWhere.Text(), mpChooser->GetFileIcon(szFile), &sLocation);
	sLocation.szFileName.Kill();
	szWhere.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Recent Files (Control+E): the files shown in editors but the active
// one, the open ones first, each most recent first; the first is the
// file shown before this one.  Typing filters.  Choosing one opens it
// where its caret was.  False, the status line saying so, with none.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::ShowRecentFiles(void)
{
	Ptr<CIDEEditorTab>	pActive;
	Ptr<CIDEEditorTab>	pTab;
	SIDERecentFile*		psFile;
	size				i;
	size				uiPass;
	size				uiAdded;
	int					iLine;
	int					iColumn;

	pActive = mpcContent->GetDocuments()->GetActiveTab();
	BeginChooser(IRC_Files, "Recent Files", true, false, NULL);
	uiAdded = 0;
	for (uiPass = 0; uiPass < 2; uiPass++)
	{
		for (i = 0; i < masFiles.NumElements(); i++)
		{
			psFile = masFiles.Get(i);
			pTab = mpcContent->GetDocuments()->FindTab(psFile->szFile.Text());
			if ((pTab.IsNotNull() != (uiPass == 0)) || (pTab.IsNotNull() && (pTab == pActive)))
			{
				continue;
			}
			iLine = psFile->iLine;
			iColumn = psFile->iColumn;
			if (pTab.IsNotNull())
			{
				pTab->GetCaretPlace(&iLine, &iColumn);
			}
			AddFilePlace(psFile->szFile.Text(), iLine, iColumn);
			uiAdded++;
		}
	}
	if (uiAdded == 0)
	{
		mpcContent->ShowStatus("There are no other recent files.");
		return false;
	}
	return OpenChooser(0);
}


//////////////////////////////////////////////////////////////////////////
//
// Recent Locations (Control+Shift+E): the places Back has, the newest
// first, or with Show Changed Only the places edited; each row its line's
// text with its file and line after it, and the code previewed.  A place
// on the line of the one listed before it isn't listed again.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::ShowRecentLocations(void)
{
	CIDENavigationHistory*	pcHistory;
	CIDEFileLines			cLines;
	SIDENavigationPlace*	psPlace;
	SIDELocation			sLocation;
	CChars					szName;
	CChars					szWhere;
	const char*				szLastFile;
	size					uiNumPlaces;
	size					uiAdded;
	size					i;
	int32					iLastLine;

	pcHistory = &mpcContent->GetNavigationHistory();
	uiNumPlaces = mbChangedOnly ? masEdits.NumElements() : pcHistory->NumBack();
	BeginChooser(IRC_Locations, mbChangedOnly ? "Recent Locations: Changed" : "Recent Locations", true, true, mbChangedOnly ? "Show All" : "Show Changed Only");
	cLines.Init(&mpcContent->GetDocuments());
	szLastFile = NULL;
	iLastLine = -1;
	uiAdded = 0;
	for (i = uiNumPlaces; i > 0; i--)
	{
		psPlace = mbChangedOnly ? masEdits.Get(i - 1) : pcHistory->GetBackPlace(i - 1);
		if (szLastFile && (psPlace->iLine == iLastLine) && psPlace->szFile.Equals(szLastFile))
		{
			continue;
		}
		szLastFile = psPlace->szFile.Text();
		iLastLine = psPlace->iLine;

		szName.Init();
		cLines.AppendLine(psPlace->szFile.Text(), psPlace->iLine, &szName);
		szName.Replace("\t", " ");
		szName.StripWhitespace();
		while (szName.Contains("  "))
		{
			szName.Replace("  ", " ");
		}
		if (szName.Empty())
		{
			szName.Append("(an empty line)");
		}
		szWhere.Init(GetIDEFileNamePart(psPlace->szFile.Text()));
		szWhere.Append(':');
		szWhere.Append((int32)(psPlace->iLine + 1));
		memset(&sLocation, 0, sizeof(SIDELocation));
		sLocation.szFileName.Init(psPlace->szFile);
		sLocation.iLine = psPlace->iLine;
		sLocation.iColumn = psPlace->iColumn;
		sLocation.iContainer = -1;
		mpChooser->AddPlace(szName.Text(), szWhere.Text(), mpChooser->GetFileIcon(psPlace->szFile.Text()), &sLocation);
		sLocation.szFileName.Kill();
		szWhere.Kill();
		szName.Kill();
		uiAdded++;
	}
	cLines.Kill();
	if (uiAdded == 0)
	{
		if (mbChangedOnly)
		{
			//Nothing edited: all of them are shown instead, if there are any.
			mbChangedOnly = false;
			if (pcHistory->NumBack() > 0)
			{
				mpcContent->ShowStatus("Nothing has been edited yet: every recent location is listed.");
				return ShowRecentLocations();
			}
		}
		mpcContent->ShowStatus("There are no recent locations yet.");
		return false;
	}
	return OpenChooser(0);
}


//////////////////////////////////////////////////////////////////////////
//
// Recent Locations' button: the other list, at the next tick, once the
// chooser that was clicked has closed.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::ButtonClicked(void)
{
	if (meChooser == IRC_Locations)
	{
		mbChangedOnly = !mbChangedOnly;
		mpcContent->GetWindow()->Post(this, &CIDERecent::ReopenLocations);
	}
}


void CIDERecent::ReopenLocations(void)
{
	ShowRecentLocations();
}


//////////////////////////////////////////////////////////////////////////
//
// The Switcher (Control+Tab, and with Shift backwards): the open files,
// most recent first, then the tool windows.  Held (Control is down) it
// stays open with the file before this one selected (or the last row,
// backwards), each Control+Tab moving on, and letting Control go chooses.
// Not held, as a quick Control+Tab is by the time it gets here, it goes
// straight to the file before this one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::OpenSwitcher(bool bBackward, bool bHeld)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CDockPane>		pPane;
	SIDELocation		sLocation;
	size				i;
	size				uiFiles;
	size				uiPlaces;
	int					iLine;
	int					iColumn;
	int					iNotTool;
	int					iTool;

	if (StepSwitcher(bBackward))
	{
		return true;
	}

	BeginChooser(IRC_Switcher, "Switcher", false, false, NULL);
	iNotTool = IDE_RECENT_NOT_TOOL;
	uiFiles = 0;
	for (i = 0; i < masFiles.NumElements(); i++)
	{
		pTab = mpcContent->GetDocuments()->FindTab(masFiles.Get(i)->szFile.Text());
		if (pTab.IsNotNull())
		{
			iLine = 0;
			iColumn = 0;
			pTab->GetCaretPlace(&iLine, &iColumn);
			AddFilePlace(pTab->GetFileName(), iLine, iColumn);
			maiChoices.Add(iNotTool);
			uiFiles++;
		}
	}
	uiPlaces = uiFiles;
	for (i = 0; i < mpcContent->GetDock()->NumPanes(); i++)
	{
		pPane = mpcContent->GetDock()->GetPane(i);
		if (pPane->GetKind() == DPK_Tool)
		{
			if (uiPlaces == uiFiles)
			{
				mpChooser->AddHeading("Tool Windows");
			}
			memset(&sLocation, 0, sizeof(SIDELocation));
			sLocation.szFileName.Init();
			sLocation.iContainer = -1;
			mpChooser->AddPlace(pPane->GetTitle(), "", -1, &sLocation);
			sLocation.szFileName.Kill();
			iTool = (int)i;
			maiChoices.Add(iTool);
			uiPlaces++;
		}
	}
	if (uiPlaces == 0)
	{
		return false;
	}
	if (!OpenChooser(bBackward ? uiPlaces - 1 : ((uiFiles > 1) ? 1 : 0)))
	{
		return false;
	}
	if (!bHeld)
	{
		return mpChooser->GetPopup()->ChooseSelected();
	}

	//Letting Control go chooses: whichever Control, heard by the list, which has the keys.  Up and Down, which the
	//list doesn't take with Control held, move as Control+Shift+Tab and Control+Tab do.
	mpChooser->GetPopup()->GetList()->AddInputAction(IAF_KeyUp, "Left Control", this, &CIDERecent::SwitcherReleased);
	mpChooser->GetPopup()->GetList()->AddInputAction(IAF_KeyUp, "Right Control", this, &CIDERecent::SwitcherReleased);
	mpChooser->GetPopup()->GetList()->AddInputAction(IAF_Chord, "Control+Cursor Up", this, &CIDERecent::SwitcherUp);
	mpChooser->GetPopup()->GetList()->AddInputAction(IAF_Chord, "Control+Cursor Down", this, &CIDERecent::SwitcherDown);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The open Switcher's next row is selected, or the one before, round the
// ends and over the heading.  False if it isn't open.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::StepSwitcher(bool bBackward)
{
	Ptr<CChooserPopup>	pPopup;
	size				i;
	int32				iRow;
	int32				iStep;

	if ((meChooser != IRC_Switcher) || mpChooser.IsNull() || !mpChooser->IsOpen())
	{
		return false;
	}
	pPopup = mpChooser->GetPopup();
	iStep = bBackward ? -1 : 1;
	iRow = pPopup->GetSelectedRow();
	for (i = 0; i < pPopup->NumRows(); i++)
	{
		iRow = (iRow + iStep + (int32)pPopup->NumRows()) % (int32)pPopup->NumRows();
		if (pPopup->SelectRow(iRow))
		{
			break;
		}
	}
	return true;
}


void CIDERecent::SwitcherUp(SComponentInputEvent* psEvent) { StepSwitcher(true); }
void CIDERecent::SwitcherDown(SComponentInputEvent* psEvent) { StepSwitcher(false); }


bool CIDERecent::ShowSwitcher(void) { return OpenSwitcher(false, mpcContent->GetWindow()->GetComponentInput()->IsControl()); }
bool CIDERecent::ShowSwitcherBackward(void) { return OpenSwitcher(true, mpcContent->GetWindow()->GetComponentInput()->IsControl()); }


void CIDERecent::SwitcherReleased(SComponentInputEvent* psEvent)
{
	if ((meChooser == IRC_Switcher) && mpChooser.IsNotNull() && mpChooser->IsOpen())
	{
		mpChooser->GetPopup()->ChooseSelected();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Select In (Alt+F1): where the active file can be shown selected.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::ShowSelectIn(void)
{
	SIDELocation	sLocation;
	size			i;
	const char*		aszTargets[] = { "Solution Explorer", "Structure" };

	if (!HasEditor())
	{
		return false;
	}
	BeginChooser(IRC_SelectIn, "Select In", false, false, NULL);
	for (i = 0; i < 2; i++)
	{
		memset(&sLocation, 0, sizeof(SIDELocation));
		sLocation.szFileName.Init();
		sLocation.iContainer = -1;
		mpChooser->AddPlace(aszTargets[i], "", -1, &sLocation);
		sLocation.szFileName.Kill();
	}
	return OpenChooser(0);
}


//////////////////////////////////////////////////////////////////////////
//
// One was chosen that the chooser doesn't go to itself: the Switcher's
// file or tool window is shown, with the keys; Select In's target shows
// the active file selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::Chosen(void)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDELocation*		psPlace;
	int32				iChosen;
	int					iTool;

	iChosen = mpChooser->GetChosenIndex();
	if (iChosen < 0)
	{
		return;
	}
	if (meChooser == IRC_Switcher)
	{
		iTool = ((size)iChosen < maiChoices.NumElements()) ? maiChoices.GetValue((size)iChosen) : IDE_RECENT_NOT_TOOL;
		if (iTool != IDE_RECENT_NOT_TOOL)
		{
			if ((size)iTool < mpcContent->GetDock()->NumPanes())
			{
				mpcContent->GetDock()->Activate(mpcContent->GetDock()->GetPane((size)iTool));
			}
			return;
		}
		psPlace = mpChooser->GetPlace((size)iChosen);
		pTab = mpcContent->GetDocuments()->FindTab(psPlace->szFileName.Text());
		if (pTab.IsNotNull())
		{
			mpcContent->GetDock()->Activate(pTab->GetDockPane());
			mpcContent->GetDocuments()->FocusActiveEditor();
		}
	}
	else if (meChooser == IRC_SelectIn)
	{
		if (iChosen == 0)
		{
			mpcContent->GetSolutions()->ShowActiveFileInExplorer();
			mpcContent->GetDock()->Activate(mpcContent->GetSolutions()->GetExplorerTool());
			mpcContent->GetWindow()->GetFocus()->SetFocus(mpcContent->GetSolutions()->GetExplorer()->GetTree());
		}
		else
		{
			mpcContent->GetStructure()->ShowStructureWindow();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Select Next Tab and Select Previous Tab (Alt+Right, Alt+Left): the
// document beside the active one in its group's tabs, round the ends.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::StepTab(bool bNext)
{
	Ptr<CDockPane>	pActive;
	Ptr<CDockGroup>	pGroup;
	size			uiNumPanes;
	size			i;

	pActive = mpcContent->GetDock()->GetActiveDocument();
	if (pActive.IsNull() || pActive->GetGroup().IsNull())
	{
		return false;
	}
	pGroup = pActive->GetGroup();
	uiNumPanes = pGroup->NumPanes();
	for (i = 0; i < uiNumPanes; i++)
	{
		if (pGroup->GetPane(i) == pActive)
		{
			break;
		}
	}
	if ((i == uiNumPanes) || (uiNumPanes < 2))
	{
		return false;
	}
	i = bNext ? ((i + 1) % uiNumPanes) : ((i + uiNumPanes - 1) % uiNumPanes);
	mpcContent->GetDock()->Activate(pGroup->GetPane(i));
	return true;
}


bool CIDERecent::SelectNextTab(void) { return StepTab(true); }
bool CIDERecent::SelectPreviousTab(void) { return StepTab(false); }


//////////////////////////////////////////////////////////////////////////
//
// Reopen Closed Tab: the file of the tab closed last that isn't open is
// opened where its caret was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::CanReopenClosedTab(void)
{
	size	i;

	for (i = 0; i < maszClosed.NumElements(); i++)
	{
		if (mpcContent->GetDocuments()->FindTab(maszClosed.GetText(i)).IsNull())
		{
			return true;
		}
	}
	return false;
}


bool CIDERecent::ReopenClosedTab(void)
{
	SIDERecentFile*	psFile;
	SIDELocation	sLocation;
	CChars			szFile;
	int32			iFile;
	bool			bResult;

	bResult = false;
	while (!bResult && (maszClosed.NumElements() > 0))
	{
		szFile.Init(maszClosed.Tail()->Text());
		maszClosed.RemoveTail();
		if (mpcContent->GetDocuments()->FindTab(szFile.Text()).IsNull())
		{
			iFile = FindFile(szFile.Text());
			psFile = (iFile >= 0) ? masFiles.Get((size)iFile) : NULL;
			memset(&sLocation, 0, sizeof(SIDELocation));
			sLocation.szFileName.Init(szFile);
			sLocation.iLine = psFile ? psFile->iLine : 0;
			sLocation.iColumn = psFile ? psFile->iColumn : 0;
			sLocation.iContainer = -1;
			bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation);
			sLocation.szFileName.Kill();
		}
		szFile.Kill();
	}
	if (!bResult)
	{
		mpcContent->ShowStatus("No tab has been closed that isn't open again.");
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Last Edit Location (Control+Shift+Backspace): the caret goes to the
// place edited before the one it is at (the newest, the first time), and
// Next Edit Location to the one after.  False, the status line saying so,
// with none that way.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERecent::GoToEdit(size uiEdit)
{
	SIDENavigationPlace*	psPlace;
	SIDELocation			sLocation;
	bool					bResult;

	psPlace = masEdits.Get(uiEdit);
	memset(&sLocation, 0, sizeof(SIDELocation));
	sLocation.szFileName.Init(psPlace->szFile);
	sLocation.iLine = psPlace->iLine;
	sLocation.iColumn = psPlace->iColumn;
	sLocation.iContainer = -1;
	bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation);
	sLocation.szFileName.Kill();
	if (bResult)
	{
		muiEdit = uiEdit;
	}
	return bResult;
}


bool CIDERecent::GoToLastEdit(void)
{
	Ptr<CIDEEditorTab>		pTab;
	SIDENavigationPlace*	psPlace;
	size					uiEdit;
	int						iLine;
	int						iColumn;

	if ((masEdits.NumElements() == 0) || (muiEdit == 0))
	{
		mpcContent->ShowStatus((masEdits.NumElements() == 0) ? "Nothing has been edited yet." : "There is no edit location before this one.");
		return false;
	}
	uiEdit = muiEdit - 1;

	//Already there (the caret hasn't left where it last edited): the one before it.
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	psPlace = masEdits.Get(uiEdit);
	if ((muiEdit == masEdits.NumElements()) && (uiEdit > 0) && pTab.IsNotNull() && pTab->GetCaretPlace(&iLine, &iColumn) && (iLine == psPlace->iLine) && psPlace->szFile.Equals(pTab->GetFileName()))
	{
		uiEdit--;
	}
	return GoToEdit(uiEdit);
}


bool CIDERecent::GoToNextEdit(void)
{
	if (muiEdit + 1 >= masEdits.NumElements())
	{
		mpcContent->ShowStatus("There is no edit location after this one.");
		return false;
	}
	return GoToEdit(muiEdit + 1);
}


//////////////////////////////////////////////////////////////////////////
//
// The user's file: <RecentFile File="..." Line="12" Column="3"/> for
// each, the most recent first, its place (where its caret is, or was as
// it closed) from 1.  Only where settings are kept: a run that is being
// checked doesn't leave them behind.
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::WriteUser(CXMLWriter* pcWriter)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDERecentFile*		psFile;
	size				i;
	int					iLine;
	int					iColumn;

	if (!mpcContent->IsKeepingSettings())
	{
		return;
	}
	for (i = 0; i < masFiles.NumElements(); i++)
	{
		//An open one's caret is where it is now.
		psFile = masFiles.Get(i);
		pTab = mpcContent->GetDocuments()->FindTab(psFile->szFile.Text());
		if (pTab.IsNotNull() && pTab->GetCaretPlace(&iLine, &iColumn))
		{
			psFile->iLine = iLine;
			psFile->iColumn = iColumn;
		}
		pcWriter->Begin(IDE_RECENT_FILE_TAG);
		pcWriter->Attribute("File", psFile->szFile.Text());
		pcWriter->Attribute("Line", (int32)(psFile->iLine + 1));
		pcWriter->Attribute("Column", (int32)(psFile->iColumn + 1));
		pcWriter->End();
	}
}


void CIDERecent::ReadUser(CMarkupTag* pcRoot)
{
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szFile;
	char*			szLine;
	char*			szColumn;

	Clear();
	for (pcTag = pcRoot ? pcRoot->GetTag(&sIterator) : NULL; pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
	{
		if (StringCompare(pcTag->GetName(), IDE_RECENT_FILE_TAG) != 0)
		{
			continue;
		}
		szFile = pcTag->GetAttribute((char*)"File");
		szLine = pcTag->GetAttribute((char*)"Line");
		szColumn = pcTag->GetAttribute((char*)"Column");
		if (szFile && (FindFile(szFile) < 0))
		{
			Remember(szFile, (szLine && (atoi(szLine) > 0)) ? atoi(szLine) - 1 : 0, (szColumn && (atoi(szColumn) > 0)) ? atoi(szColumn) - 1 : 0, false);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: "Recent files: Shapes.cpp:12, Main.cpp:1 (closed)" and
// "Edit locations: 2 at 1".
//
//////////////////////////////////////////////////////////////////////////
void CIDERecent::Print(CChars* pszDest)
{
	SIDERecentFile*	psFile;
	size			i;

	pszDest->Append("Recent files:");
	for (i = 0; i < masFiles.NumElements(); i++)
	{
		psFile = masFiles.Get(i);
		pszDest->Append((i > 0) ? ", " : " ");
		pszDest->Append(GetIDEFileNamePart(psFile->szFile.Text()));
		if (mpcContent->GetDocuments()->FindTab(psFile->szFile.Text()).IsNull())
		{
			pszDest->Append(':');
			pszDest->Append((int32)(psFile->iLine + 1));
			pszDest->Append(" (closed)");
		}
	}
	pszDest->Append("\nEdit locations: ");
	pszDest->Append((uint32)masEdits.NumElements());
	pszDest->Append(" at ");
	pszDest->Append((uint32)muiEdit);
	pszDest->Append('\n');
}


size CIDERecent::NumFiles(void) const { return masFiles.NumElements(); }
SIDERecentFile* CIDERecent::GetFile(size uiIndex) { return masFiles.Get(uiIndex); }
size CIDERecent::NumEdits(void) const { return masEdits.NumElements(); }
Ptr<CIDEChooser> CIDERecent::GetChooser(void) const { return mpChooser; }

