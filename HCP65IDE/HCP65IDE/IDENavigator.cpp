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
#include "BaseLib/FileUtil.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/StringHelper.h"
#include "CppCompilerLib/CPPIndex.h"
#include "CppCompilerLib/CPPSyntaxFunctionDefinition.h"
#include "CppCompilerLib/CPPSyntaxDeclarator.h"
#include "CppCompilerLib/CPPSyntaxName.h"
#include "SupportLib/PNGWriter.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDENavigator.h"
#include "IDERefactorUnit.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpNavigateMenu = NULL;
	mpGoTo = NULL;
	mszGoToPattern.Init();
	miGoToFileIcon = -1;
	miQueuedGoTo = -1;
	memset(maiGoToSymbolIcons, 0, sizeof(maiGoToSymbolIcons));
	maszRecentClasses.Init();
	maszRecentSymbols.Init();
	maszRecentActions.Init();
	mszQueuedAction.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpNavigateMenu);
	M_Pointer(mpGoTo);
	M_Pointer(mpTargets);
	M_Pointer(mpGoToPreview);
	U_Unknown(CArrayChars, maszRecentClasses);
	U_Unknown(CArrayChars, maszRecentSymbols);
	U_Unknown(CArrayChars, maszRecentActions);
	U_String(mszQueuedAction);
	U_String(mszGoToPattern);
	U_Int32(miGoToFileIcon);
	U_Int32(miQueuedGoTo);
	U_Int32Array(maiGoToSymbolIcons, IST_Count);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::Free(void)
{
	mszQueuedAction.Kill();
	maszRecentActions.Kill();
	maszRecentSymbols.Kill();
	maszRecentClasses.Kill();
	mszGoToPattern.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::Save(CObjectWriter* pcFile) { return false; }
bool CIDENavigator::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Go to Class (uiTab 0) and Go to Symbol (2) find the index's symbols
// declared in the projects' files, Go to File (1) the files.  A file is
// shown by its name, with its folder, "~" for the home folder, after it.
// The content is its Go to dialog's searcher.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::GoToSearch(size uiTab, CNameMatcher* pcMatcher, CGoToItems* pcDest)
{
	CIDECodeModel*		pcCodeModel;
	Ptr<CIDEEditorTab>	pActive;
	CArrayChars*		paszRecent;
	size		i;
	size		j;
	size		uiRank;
	SNameMatch	sMatch;
	const char*	szPath;
	CChars		szFolder;
	CChars		szName;
	CFileUtil	cFileUtil;
	const char*	szHome;
	size		uiHome;

	pcCodeModel = mpcContent->GetCodeModel();
	if (uiTab == IDE_GO_TO_ACTION)
	{
		FindGoToActions(pcMatcher, pcDest);
		return;
	}
	if (uiTab != IDE_GO_TO_FILE)
	{
		mpcContent->GetDocuments()->UpdateEditedTabsNow();
		pcCodeModel->FindGoToSymbols(pcMatcher, uiTab == IDE_GO_TO_CLASS, maiGoToSymbolIcons, pcDest);
		if (pcMatcher->IsEmpty())
		{
			//With nothing typed, what Go to went to last comes first.
			paszRecent = (uiTab == IDE_GO_TO_CLASS) ? &maszRecentClasses : &maszRecentSymbols;
			for (i = paszRecent->NumElements(); i > 0; i--)
			{
				PutGoToFirst(pcDest, (size)pcCodeModel->FindSymbolIndex(paszRecent->GetText(i - 1)), paszRecent->NumElements() - i);
			}
		}
		return;
	}

	szHome = getenv("HOME");
	uiHome = szHome ? strlen(szHome) : 0;
	for (i = 0; i < pcCodeModel->NumProjectFiles(); i++)
	{
		szPath = pcCodeModel->GetProjectFile(i);
		if (pcMatcher->MatchPath(szPath, &sMatch))
		{
			szName.Init();
			szFolder.Init();
			cFileUtil.SplitPath(szPath, &szName, &szFolder);
			if ((uiHome > 0) && (strncmp(szFolder.Text(), szHome, uiHome) == 0))
			{
				szFolder.Replace(szHome, "~");
			}
			pcDest->Add(szName.Text(), szFolder.Text(), miGoToFileIcon + (int32)CIDEIcons::GetFileType(szPath), i, &sMatch);
			szFolder.Kill();
			szName.Kill();
		}
	}

	//With nothing typed, the recent files come first, the last shown before the active one at the top.
	if (pcMatcher->IsEmpty())
	{
		pActive = mpcContent->GetDocuments()->GetActiveTab();
		uiRank = 0;
		for (i = 0; i < mpcContent->GetRecent()->NumFiles(); i++)
		{
			szPath = mpcContent->GetRecent()->GetFile(i)->szFile.Text();
			if (pActive.IsNotNull() && (StringCompare(szPath, pActive->GetFileName()) == 0))
			{
				continue;
			}
			for (j = 0; j < pcCodeModel->NumProjectFiles(); j++)
			{
				if (StringCompare(pcCodeModel->GetProjectFile(j), szPath) == 0)
				{
					PutGoToFirst(pcDest, j, uiRank);
					uiRank++;
					break;
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The item of pcDest whose data is uiData is given a score that puts it
// before those not asked for, and after those asked for with a lower
// uiRank.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::PutGoToFirst(CGoToItems* pcDest, size uiData, size uiRank)
{
	size	i;

	for (i = 0; i < pcDest->NumItems(); i++)
	{
		if (pcDest->Get(i)->uiData == uiData)
		{
			pcDest->Get(i)->sMatch.iScore = IDE_GO_TO_RECENT_SCORE - (int32)uiRank;
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Action (Control+Shift+A): the IDE's commands, each found by its
// caption as a name is found, with its menu (its name's first part) and
// its keys after it; its data is its place in the command set.  Commands
// with no caption (those a tab's menu makes for itself) aren't listed.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::FindGoToActions(CNameMatcher* pcMatcher, CGoToItems* pcDest)
{
	Ptr<CCommandSet>	pCommands;
	Ptr<CCommand>		pCommand;
	SNameMatch			sMatch;
	CChars				szName;
	CChars				szWhere;
	const char*			szDot;
	size				uiRecent;
	size				i;

	pCommands = mpcContent->GetCommands();
	for (i = 0; i < pCommands->NumCommands(); i++)
	{
		pCommand = pCommands->GetCommand(i);
		szName.Init(pCommand->GetCaption() ? pCommand->GetCaption() : "");
		if (szName.EndsWith("..."))
		{
			szName.SetLength(szName.Length() - 3);
		}
		if (!szName.Empty() && pcMatcher->Match(szName.Text(), &sMatch))
		{
			//With nothing typed, the commands it ran last come first.
			uiRecent = pcMatcher->IsEmpty() ? maszRecentActions.GetIndex(pCommand->GetName()) : ARRAY_ELEMENT_NOT_FOUND;
			if (uiRecent != ARRAY_ELEMENT_NOT_FOUND)
			{
				sMatch.iScore = IDE_GO_TO_RECENT_SCORE - (int32)(maszRecentActions.NumElements() - 1 - uiRecent);
			}
			szWhere.Init();
			szDot = strchr(pCommand->GetName(), '.');
			if (szDot)
			{
				szWhere.Append(pCommand->GetName(), (size)(szDot - pCommand->GetName()));
			}
			if (pCommand->NumKeys() > 0)
			{
				szWhere.Append("  ");
				pCommand->AppendKeysText(&szWhere, mpcContent->GetWindow()->GetComponentInput());
			}
			pcDest->Add(szName.Text(), szWhere.Text(), -1, i, &sMatch);
			szWhere.Kill();
		}
		szName.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Navigate, as IntelliJ's: Back and Forward, then Class, File and Symbol,
// each the Go to dialog on its tab (see AddCommands).
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDENavigator::CreateNavigateMenu(void)
{
	Ptr<CCommandSet>	pCommands;

	pCommands = mpcContent->GetCommands();
	mpNavigateMenu = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	mpcContent->GetNavigationHistory()->AddNavigateItems(mpNavigateMenu);
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToClass"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToFile"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToSymbol"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToAction"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToLine"));
	mpNavigateMenu->AddSeparator();
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToSource"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToDefinition"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToImplementation"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToTypeDeclaration"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToSuperMethod"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.GoToRelatedSymbol"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.SwitchHeaderSource"));
	mpNavigateMenu->AddSeparator();
	mpcContent->GetStructure()->AddNavigateItems(mpNavigateMenu);
	mpNavigateMenu->AddSeparator();
	mpcContent->GetBookmarks()->AddNavigateItems(mpNavigateMenu);
	mpNavigateMenu->AddSeparator();
	mpcContent->GetMarks()->AddNavigateItems(mpNavigateMenu);
	mpNavigateMenu->AddSeparator();
	mpcContent->GetRecent()->AddNavigateItems(mpNavigateMenu);
	mpNavigateMenu->AddSeparator();
	mpcContent->GetHierarchy()->AddNavigateItems(mpNavigateMenu);
	mpNavigateMenu->AddSeparator();
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.FindUsages"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.ShowUsages"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.FindReferences"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.FindUsagesSettings"));
	mpNavigateMenu->AddSeparator();
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.NextOccurrence"));
	mpNavigateMenu->AddItem(pCommands->Find("Navigate.PreviousOccurrence"));
	return mpNavigateMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Class (Control+N), File (Control+Shift+N) and Symbol
// (Control+Alt+Shift+N) as IntelliJ IDEA's, Go to Source (Control+B)
// while there is an #include or a definition to go to, and Visual
// Assist's Switch Header/Source (Alt+O).
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Navigate.GoToClass", "Class...", "Control+N", Ptr<CImageCel>(), this, &CIDENavigator::GoToClass);
	pCommands->Add("Navigate.GoToFile", "File...", "Control+Shift+N", Ptr<CImageCel>(), this, &CIDENavigator::GoToFile);
	pCommands->Add("Navigate.GoToSymbol", "Symbol...", "Control+Alt+Shift+N", Ptr<CImageCel>(), this, &CIDENavigator::GoToSymbol);
	pCommands->Add("Navigate.GoToAction", "Action...", "Control+Shift+A", Ptr<CImageCel>(), this, &CIDENavigator::GoToAction);
	pCommands->Add("Navigate.GoToSource", "Go to Source", "Control+B", Ptr<CImageCel>(), this, &CIDENavigator::GoToSource)->SetEnabledCheck(this, &CIDENavigator::CanGoToSource);
	pCommands->Add("Navigate.SwitchHeaderSource", "Switch Header/Source", "Alt+O", Ptr<CImageCel>(), this, &CIDENavigator::SwitchHeaderSource);
	pCommands->Add("Navigate.GoToImplementation", "Go to Implementation(s)", "Control+Alt+B", Ptr<CImageCel>(), this, &CIDENavigator::GoToImplementation)->SetEnabledCheck(this, &CIDENavigator::CanGoToImplementation);
	pCommands->Add("Navigate.GoToSuperMethod", "Go to Super Method", "Control+U", Ptr<CImageCel>(), this, &CIDENavigator::GoToSuperMethod)->SetEnabledCheck(this, &CIDENavigator::CanGoToSuperMethod);
	pCommands->Add("Navigate.GoToTypeDeclaration", "Go to Type Declaration", "Control+Shift+B", Ptr<CImageCel>(), this, &CIDENavigator::GoToTypeDeclaration)->SetEnabledCheck(this, &CIDENavigator::CanGoToSuperMethod);
	pCommands->Add("Navigate.GoToRelatedSymbol", "Go to Related Symbol", "Control+Alt+Home", Ptr<CImageCel>(), this, &CIDENavigator::GoToRelatedSymbol)->SetEnabledCheck(this, &CIDENavigator::CanGoToSuperMethod);
}


void CIDENavigator::GoToClass(void) { QueueGoTo(IDE_GO_TO_CLASS); }
void CIDENavigator::GoToFile(void) { QueueGoTo(IDE_GO_TO_FILE); }
void CIDENavigator::GoToSymbol(void) { QueueGoTo(IDE_GO_TO_SYMBOL); }
void CIDENavigator::GoToAction(void) { QueueGoTo(IDE_GO_TO_ACTION); }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Navigate, Class (Control+N), File (Control+Shift+N) and
// Symbol (Control+Alt+Shift+N): the Go to dialog on that tab with what
// was last typed in it.  The file, or the symbol's definition (or its
// declaration), is opened, at the line typed after a colon if one was.
// False if nothing was chosen.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::ShowGoTo(size uiTab)
{
	bool	bResult;

	if (!CreateGoTo(uiTab))
	{
		return false;
	}
	bResult = false;
	mpcContent->GetScript()->WatchDialog(mpGoTo->GetDialog());
	if (mpGoTo->Show())
	{
		bResult = OpenGoToChoice();
	}
	mszGoToPattern.Set(mpGoTo->GetPattern());
	mpcContent->GetScript()->StopWatchingDialog();
	ReleaseGoTo();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog is let go of: its preview first, which the edits thread may
// still have a file to colour for.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::ReleaseGoTo(void)
{
	if (mpGoTo.IsNotNull())
	{
		mpGoTo->GetSelectedEvent()->Remove(this);
	}
	if (mpGoToPreview.IsNotNull())
	{
		mpGoToPreview->Detach();
		mpGoToPreview = NULL;
	}
	mpGoTo = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Another result is selected in the dialog: its place's code is
// previewed.  A file's is its start (or the line typed after a colon), a
// symbol's where it is defined or declared; an action has none.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::GoToSelected(void)
{
	SGoToItem*		psItem;
	SIDELocation	sLocation;
	bool			bPlace;

	if (mpGoTo.IsNull() || mpGoToPreview.IsNull())
	{
		return;
	}
	psItem = mpGoTo->GetSelectedItem();
	bPlace = false;
	memset(&sLocation, 0, sizeof(SIDELocation));
	sLocation.iContainer = -1;
	if (psItem && (mpGoTo->GetTab() == IDE_GO_TO_FILE))
	{
		if (psItem->uiData < mpcContent->GetCodeModel()->NumProjectFiles())
		{
			sLocation.szFileName.Init(mpcContent->GetCodeModel()->GetProjectFile(psItem->uiData));
			bPlace = true;
		}
	}
	else if (psItem && (mpGoTo->GetTab() != IDE_GO_TO_ACTION))
	{
		bPlace = mpcContent->GetCodeModel()->GetSymbolLocation(psItem->uiData, &sLocation);
	}
	if (bPlace)
	{
		if (mpGoTo->GetLine() > 0)
		{
			sLocation.iLine = mpGoTo->GetLine() - 1;
			sLocation.iColumn = 0;
			sLocation.iLength = 0;
		}
		mpGoToPreview->ShowLocation(&sLocation);
		sLocation.szFileName.Kill();
	}
	else
	{
		mpGoToPreview->Clear();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Class or Symbol went to the index's symbol uiSymbol: it is the
// last of that tab's recent ones (their keys, the most recent last), of
// which thirty are kept, in the workspace's user's file.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::RememberGoTo(size uiTab, size uiSymbol)
{
	CArrayChars*	paszRecent;
	CChars			szKey;
	uint8			uiKind;

	paszRecent = (uiTab == IDE_GO_TO_CLASS) ? &maszRecentClasses : &maszRecentSymbols;
	szKey.Init();
	if (mpcContent->GetCodeModel()->GetSymbolKey((int32)uiSymbol, &szKey, &uiKind))
	{
		if (paszRecent->Contains(szKey.Text()))
		{
			paszRecent->Remove(szKey.Text());
		}
		paszRecent->Add(szKey.Text());
		while (paszRecent->NumElements() > IDE_GO_TO_RECENT)
		{
			paszRecent->Remove((size)0);
		}
		if (mpcContent->IsKeepingSettings())
		{
			mpcContent->GetUserFile()->Write();
		}
	}
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Action chose the command: it is the last of the recent ones (their
// names, the most recent last), which the Actions tab lists first with
// nothing typed.  They are the IDE's, not a workspace's: kept with the
// settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::RememberAction(const char* szCommand)
{
	if (maszRecentActions.Contains(szCommand))
	{
		maszRecentActions.Remove(szCommand);
	}
	maszRecentActions.Add(szCommand);
	while (maszRecentActions.NumElements() > IDE_GO_TO_RECENT_ACTIONS)
	{
		maszRecentActions.Remove((size)0);
	}
	mpcContent->GetSettings()->WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// The settings' <GoTo>, with a <RecentAction Name=""/> for each command
// Go to Action ran, the oldest first.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::WriteSettings(CXMLWriter* pcWriter)
{
	size	i;

	pcWriter->Begin(IDE_GO_TO_TAG);
	for (i = 0; i < maszRecentActions.NumElements(); i++)
	{
		pcWriter->Begin(IDE_GO_TO_ACTION_TAG);
		pcWriter->Attribute("Name", maszRecentActions.GetText(i));
		pcWriter->End();
	}
	pcWriter->End();
}


void CIDENavigator::ReadSettings(CMarkupTag* pcSettings)
{
	CMarkupTag*		pcGoTo;
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	char*			szName;

	pcGoTo = pcSettings->GetTag(IDE_GO_TO_TAG);
	if (pcGoTo == NULL)
	{
		return;
	}
	maszRecentActions.ReInit();
	for (pcTag = pcGoTo->GetTag(&sIterator); pcTag; pcTag = pcGoTo->GetNextTag(&sIterator))
	{
		szName = (StringCompare(pcTag->GetName(), IDE_GO_TO_ACTION_TAG) == 0) ? pcTag->GetAttribute((char*)"Name") : NULL;
		if (szName && (szName[0] != '\0') && !maszRecentActions.Contains(szName) && (maszRecentActions.NumElements() < IDE_GO_TO_RECENT_ACTIONS))
		{
			maszRecentActions.Add(szName);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The workspace's user's file's <RecentClass Key=""/> and <RecentSymbol
// Key=""/>: what Go to Class and Go to Symbol went to, the oldest first.
// A key is the index's (CShape::Area(void)); one whose symbol has gone
// since is kept, and puts nothing first.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::WriteUser(CXMLWriter* pcWriter)
{
	size	i;

	if (!mpcContent->IsKeepingSettings())
	{
		return;
	}
	for (i = 0; i < maszRecentClasses.NumElements(); i++)
	{
		pcWriter->Begin(IDE_RECENT_CLASS_TAG);
		pcWriter->Attribute("Key", maszRecentClasses.GetText(i));
		pcWriter->End();
	}
	for (i = 0; i < maszRecentSymbols.NumElements(); i++)
	{
		pcWriter->Begin(IDE_RECENT_SYMBOL_TAG);
		pcWriter->Attribute("Key", maszRecentSymbols.GetText(i));
		pcWriter->End();
	}
}


void CIDENavigator::ReadUser(CMarkupTag* pcRoot)
{
	CMarkupTag*		pcTag;
	STagIterator	sIterator;
	CArrayChars*	paszRecent;
	char*			szKey;

	maszRecentClasses.ReInit();
	maszRecentSymbols.ReInit();
	for (pcTag = pcRoot ? pcRoot->GetTag(&sIterator) : NULL; pcTag; pcTag = pcRoot->GetNextTag(&sIterator))
	{
		if (StringCompare(pcTag->GetName(), IDE_RECENT_CLASS_TAG) == 0)
		{
			paszRecent = &maszRecentClasses;
		}
		else if (StringCompare(pcTag->GetName(), IDE_RECENT_SYMBOL_TAG) == 0)
		{
			paszRecent = &maszRecentSymbols;
		}
		else
		{
			continue;
		}
		szKey = pcTag->GetAttribute((char*)"Key");
		if (szKey && (szKey[0] != '\0') && !paszRecent->Contains(szKey) && (paszRecent->NumElements() < IDE_GO_TO_RECENT))
		{
			paszRecent->Add(szKey);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The command Go to Action chose is run, at the tick after the dialog has
// closed: it may show a dialog of its own.  One that can't be run where
// the caret is says so.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::RunQueuedAction(void)
{
	Ptr<CCommand>	pCommand;
	CChars			szStatus;

	pCommand = mpcContent->GetCommands()->Find(mszQueuedAction.Text());
	mszQueuedAction.Clear();
	if (pCommand.IsNull())
	{
		return;
	}
	if (!pCommand->IsEnabled())
	{
		szStatus.Init(pCommand->GetCaption());
		if (szStatus.EndsWith("..."))
		{
			szStatus.SetLength(szStatus.Length() - 3);
		}
		szStatus.Append(" isn't available here.");
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
		return;
	}
	pCommand->Run();
}


//////////////////////////////////////////////////////////////////////////
//
// Go to is shown at the next tick: ShowModal can't be called from the
// input (a key or a menu click) that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::QueueGoTo(size uiTab)
{
	miQueuedGoTo = (int32)uiTab;
	mpcContent->GetWindow()->Post(this, &CIDENavigator::ShowQueuedGoTo);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::ShowQueuedGoTo(void)
{
	ShowGoTo((size)miQueuedGoTo);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog, started on uiTab, not shown.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::CreateGoTo(size uiTab)
{
	size	i;

	mpGoTo = OMalloc<CGoToDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Go To", this, mpcContent->GetMatchFont());
	mpGoTo->AddTab("Classes", mpcContent->GetCommands()->Find("Navigate.GoToClass"));
	mpGoTo->AddTab("Files", mpcContent->GetCommands()->Find("Navigate.GoToFile"));
	mpGoTo->AddTab("Symbols", mpcContent->GetCommands()->Find("Navigate.GoToSymbol"));
	mpGoTo->AddTab("Actions", mpcContent->GetCommands()->Find("Navigate.GoToAction"));

	//The selected result's code under the results, as a chooser previews its places.
	mpGoTo->AddPreview(mpcContent->GetDimFont(), mpcContent->GetCodeFamily());
	mpGoToPreview = OMalloc<CIDELocationPreview>(mpcContent, mpGoTo->GetPreview());
	mpGoTo->GetSelectedEvent()->Add(this, &CIDENavigator::GoToSelected);
	miGoToFileIcon = -1;
	for (i = 0; i < mpcContent->NumFileTypeIcons(); i++)
	{
		if (i == 0)
		{
			miGoToFileIcon = mpGoTo->AddIcon(mpcContent->GetFileTypeIcon(i));
		}
		else
		{
			mpGoTo->AddIcon(mpcContent->GetFileTypeIcon(i));
		}
	}
	for (i = 0; i < IST_Count; i++)
	{
		maiGoToSymbolIcons[i] = mpGoTo->AddIcon(mpcContent->GetSymbolIcon(i));
	}
	mpGoTo->Start(uiTab, mszGoToPattern.Text());
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Opens what the dialog chose.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::OpenGoToChoice(void)
{
	SGoToItem*		psItem;
	SIDELocation	sLocation;
	bool			bResult;
	int32			iLine;

	psItem = mpGoTo->GetChosen();
	if (psItem == NULL)
	{
		return false;
	}
	iLine = mpGoTo->GetLine();
	if (mpGoTo->GetTab() == IDE_GO_TO_ACTION)
	{
		if (psItem->uiData >= mpcContent->GetCommands()->NumCommands())
		{
			return false;
		}
		mszQueuedAction.Set(mpcContent->GetCommands()->GetCommand(psItem->uiData)->GetName());
		RememberAction(mszQueuedAction.Text());
		mpcContent->GetWindow()->Post(this, &CIDENavigator::RunQueuedAction);
		return true;
	}
	if (mpGoTo->GetTab() == IDE_GO_TO_FILE)
	{
		if (psItem->uiData >= mpcContent->GetCodeModel()->NumProjectFiles())
		{
			return false;
		}
		bResult = OpenProjectFile(mpcContent->GetCodeModel()->GetProjectFile(psItem->uiData));
		if (bResult && (iLine > 0))
		{
			mpcContent->GetDocuments()->SetCaretPlace(iLine - 1, 0);
		}
		return bResult;
	}

	if (!mpcContent->GetCodeModel()->GetSymbolLocation(psItem->uiData, &sLocation))
	{
		return false;
	}
	RememberGoTo(mpGoTo->GetTab(), psItem->uiData);
	if (iLine > 0)
	{
		sLocation.iLine = iLine - 1;
		sLocation.iColumn = 0;
	}
	bResult = mpcContent->GetDocuments()->OpenLocation(&sLocation);
	sLocation.szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CGoToDialog> CIDENavigator::GetGoTo(void) const { return mpGoTo; }



//////////////////////////////////////////////////////////////////////////
//
// Whether szPath ends with szEnd, whatever the case of either and
// whichever separators they have.
//
//////////////////////////////////////////////////////////////////////////
static bool EndsWithIDEPath(const char* szPath, const char* szEnd)
{
	size	uiPath;
	size	uiEnd;
	char	cPath;
	char	cEnd;

	uiPath = strlen(szPath);
	uiEnd = strlen(szEnd);
	if (uiPath < uiEnd)
	{
		return false;
	}
	for (; uiEnd > 0; uiEnd--, uiPath--)
	{
		cPath = szPath[uiPath - 1];
		cEnd = szEnd[uiEnd - 1];
		cPath = (cPath == '\\') ? '/' : (((cPath >= 'A') && (cPath <= 'Z')) ? (char)(cPath - 'A' + 'a') : cPath);
		cEnd = (cEnd == '\\') ? '/' : (((cEnd >= 'A') && (cEnd <= 'Z')) ? (char)(cEnd - 'A' + 'a') : cEnd);
		if (cPath != cEnd)
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The first of the projects' files whose path ends with szEnd (a
// separator and a name, say), in pszDest.  False if none does.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::FindProjectFileEnding(const char* szEnd, CChars* pszDest)
{
	CIDECodeModel*	pcCodeModel;
	size			i;

	pcCodeModel = mpcContent->GetCodeModel();
	for (i = 0; i < pcCodeModel->NumProjectFiles(); i++)
	{
		if (EndsWithIDEPath(pcCodeModel->GetProjectFile(i), szEnd))
		{
			pszDest->Set(pcCodeModel->GetProjectFile(i));
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName opened as its project's file (the active file's project if
// no project has it), its editor given the keys.  False if it can't be
// opened.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::OpenProjectFile(const char* szFileName)
{
	size	uiProject;

	uiProject = mpcContent->GetCodeModel()->FindProjectOfFile(szFileName);
	if ((uiProject == MAX_SIZE) && mpcContent->GetDocuments()->GetActiveTab().IsNotNull())
	{
		uiProject = mpcContent->GetDocuments()->GetActiveTab()->GetProject();
	}
	return mpcContent->GetDocuments()->OpenFile(szFileName, uiProject) && mpcContent->GetDocuments()->FocusActiveEditor();
}


//////////////////////////////////////////////////////////////////////////
//
// The file an #include on the active editor's caret line names, and
// whether it's a <system> one.  False if the line isn't an #include.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GetIncludeAtCaret(CChars* pszName, bool* pbSystem)
{
	CChars		szLine;
	char*		szText;
	char*		szEnd;
	char		cClose;
	bool		bResult;
	int			iLine;
	int			iColumn;

	if (!mpcContent->GetDocuments()->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	szLine.Init();
	mpcContent->GetDocuments()->GetFileLine(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, &szLine);
	szText = szLine.Text();
	bResult = false;
	if (szText[0] == '#')
	{
		szText++;
		while ((*szText == ' ') || (*szText == '\t'))
		{
			szText++;
		}
		if (strncmp(szText, "include", 7) == 0)
		{
			szText += 7;
			while ((*szText == ' ') || (*szText == '\t'))
			{
				szText++;
			}
			if ((*szText == '"') || (*szText == '<'))
			{
				cClose = (*szText == '"') ? '"' : '>';
				*pbSystem = (cClose == '>');
				szEnd = strchr(szText + 1, cClose);
				if (szEnd && (szEnd > szText + 1))
				{
					pszName->Init();
					pszName->AppendSubString(szText + 1, szEnd);
					bResult = true;
				}
			}
		}
	}
	szLine.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file the active editor's #include names, found as the compiler
// would (see CIDECodeModel::FindIncludeFile), or else a solution file
// with that path.  False if the caret line isn't an #include or the file
// isn't found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::FindIncludedFile(CChars* pszDest)
{
	CChars	szName;
	CChars	szEnd;
	bool	bSystem;
	bool	bFound;

	if (!GetIncludeAtCaret(&szName, &bSystem))
	{
		return false;
	}
	bFound = mpcContent->GetCodeModel()->FindIncludeFile(mpcContent->GetDocuments()->GetActiveTab()->GetProject(), mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), szName.Text(), bSystem, pszDest);
	if (!bFound)
	{
		//A solution file whose path ends with it.
		szEnd.Init("/");
		szEnd.Append(szName);
		bFound = FindProjectFileEnding(szEnd.Text(), pszDest);
		szEnd.Kill();
	}
	szName.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// On a documentation page, where the reference to code at the caret links
// to, as the website's link does (HCP65/Website/DocsPlan.txt E4): its
// symbol's declaration in its header, or a file, the website's symbol
// table saying where for what the workspace doesn't have.  With bGo it is
// gone to.  False if the caret isn't on a reference that links.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GoToPageReference(bool bGo)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDELocation		sLocation;
	int					iLine;
	int					iColumn;
	int					iTarget;
	bool				bFound;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !CWorkspaceIndex::IsPageFile(pTab->GetFileName()) || !mpcContent->GetDocuments()->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	sLocation.szFileName.Init();
	bFound = mpcContent->GetCodeModel()->FindPageReference(pTab->GetFileName(), iLine, iColumn, &sLocation.szFileName, &iTarget);
	if (bFound && bGo)
	{
		sLocation.iLine = (iTarget > 0) ? iTarget - 1 : 0;
		sLocation.iColumn = 0;
		sLocation.iLength = 0;
		sLocation.uiRole = CPPIR_Declaration;
		sLocation.uiFlags = 0;
		sLocation.uiUsage = CPPUT_Other;
		sLocation.iContainer = -1;
		bFound = mpcContent->GetDocuments()->OpenLocation(&sLocation);
	}
	sLocation.szFileName.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Control+B, as IntelliJ's: on an #include the file it names is opened;
// on a symbol's own declaration or definition, a popup of its usages;
// on a documentation page's reference to code, where it links to;
// anywhere else, the definition of what is at the caret (as F12), which
// for a macro's name is its #define.  False if there's nothing to go to.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GoToSource(void)
{
	CChars	szFile;
	bool	bResult;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	if (GoToPageReference(true))
	{
		return true;
	}
	szFile.Init();
	if (FindIncludedFile(&szFile))
	{
		bResult = OpenProjectFile(szFile.Text());
		szFile.Kill();
		return bResult;
	}
	szFile.Kill();

	//On its own declaration or definition there is nowhere to go: its usages are shown, as IntelliJ's does.
	if (mpcContent->GetUsages()->IsOnOwnDeclaration())
	{
		return mpcContent->GetUsages()->ShowUsagesPopup();
	}
	return mpcContent->GoToDefinition();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Go to Source has somewhere to go: an #include's file, a page's
// reference to code, a symbol at the caret, or a macro's name there (its
// #define).
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::CanGoToSource(void)
{
	SIDELocation	sDefine;
	CChars			szFile;
	int				iLine;
	int				iColumn;
	uint8			uiRole;
	bool			bResult;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	szFile.Init();
	bResult = GoToPageReference(false) || FindIncludedFile(&szFile) || mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
	if (!bResult && mpcContent->GetConvertMacro()->FindDefineAtCaret(&szFile, &sDefine))
	{
		sDefine.szFileName.Kill();
		bResult = true;
	}
	szFile.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+O, as Visual Assist's: a .cpp (.c, .cc, .cxx) opens its header
// (.h, .hpp, .hxx, .inl) of the same name, and a header its .cpp: the one
// in the same folder, else a solution file of that name.  False if there
// isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::SwitchHeaderSource(void)
{
	CChars	szFound;
	bool	bFound;

	szFound.Init();
	bFound = FindHeaderSource(&szFound) && OpenProjectFile(szFound.Text());
	szFound.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The active file's other half: a .cpp's (.c's, .cc's, .cxx's) header
// (.h, .hpp, .hxx, .inl) of the same name, or a header's .cpp: the one in
// the same folder, else a solution file of that name.  False if there
// isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::FindHeaderSource(CChars* pszDest)
{
	static const char*	aszSources[] = { "cpp", "c", "cc", "cxx" };
	static const char*	aszHeaders[] = { "h", "hpp", "hxx", "inl" };
	const char**		aszWanted;
	size				uiWanted;
	CFileUtil			cFileUtil;
	CChars				szBase;
	CChars				szPath;
	CChars				szName;
	EIDEFileType		eType;
	size				i;
	bool				bFound;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	eType = CIDEIcons::GetFileType(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	if (eType == IFT_Header)
	{
		aszWanted = aszSources;
		uiWanted = sizeof(aszSources) / sizeof(char*);
	}
	else if ((eType == IFT_CPP) || (eType == IFT_C))
	{
		aszWanted = aszHeaders;
		uiWanted = sizeof(aszHeaders) / sizeof(char*);
	}
	else
	{
		return false;
	}

	//The same name, beside it.
	szBase.Init(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	cFileUtil.RemoveExtension(&szBase);
	bFound = false;
	for (i = 0; (i < uiWanted) && !bFound; i++)
	{
		szPath.Init(szBase);
		szPath.Append('.');
		szPath.Append(aszWanted[i]);
		if (cFileUtil.Exists(szPath.Text()))
		{
			pszDest->Set(szPath);
			bFound = true;
		}
		szPath.Kill();
	}

	//Else a solution file of that name.
	if (!bFound)
	{
		szName.Init();
		cFileUtil.GetFileName(&szName, szBase.Text());
		for (i = 0; (i < uiWanted) && !bFound; i++)
		{
			szPath.Init("/");
			szPath.Append(szName);
			szPath.Append('.');
			szPath.Append(aszWanted[i]);
			bFound = FindProjectFileEnding(szPath.Text(), pszDest);
			szPath.Kill();
		}
		szName.Kill();
	}
	szBase.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The editors' context menu's Go to Source (Control+B), enabled when the
// menu opens if there is an #include or a definition to go to.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::AddEditorItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Navigate.GoToSource"));
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol named at the active editor's caret (CIDEDocuments::
// FindSymbolPlace), once the index has what was typed: its key and kind.
// False if none is.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::FindCaretSymbol(CChars* pszKey, uint8* puiKind)
{
	Ptr<CIDEEditorTab>	pTab;
	int					iLine;
	int					iColumn;
	uint8				uiRole;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	return mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) &&
		   mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), iLine, iColumn, pszKey, puiKind);
}


//////////////////////////////////////////////////////////////////////////
//
// The function whose definition the active editor's caret is in: the one
// the file's outline has round the caret (anywhere in it, between its
// tokens too), or failing that as the refactorings find it, the file
// parsed as it is now and the function definition round the caret; and
// the symbol its name is.  False if the caret isn't in one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::FindCaretFunction(CChars* pszKey, uint8* puiKind)
{
	Ptr<CIDEEditorTab>				pTab;
	CIDERefactorUnit				cUnit;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CCPPSyntax*						pcNode;
	CCPPSyntaxName*					pcName;
	SCPPSyntaxRange					sRange;
	CIDEOutline*					pcOutline;
	SIDEOutlineEntry*				psEntry;
	int32							iEntry;
	int								iLine;
	int								iColumn;
	bool							bFound;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();

	//The file's outline knows the function a place is in, between its tokens too: the symbol is its name's last part.
	pcOutline = pTab->GetOutline();
	iEntry = pcOutline ? pcOutline->FindFunctionAt(iLine, iColumn) : -1;
	if (iEntry >= 0)
	{
		psEntry = pcOutline->Get((size)iEntry);
		if (psEntry->bDefinition && (psEntry->sName.iEndColumn > 0) &&
			mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), psEntry->sName.iEndLine, psEntry->sName.iEndColumn - 1, pszKey, puiKind))
		{
			return true;
		}
	}

	cUnit.Init();
	bFound = false;
	if (cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), pTab->GetFileName(), false))
	{
		pcNode = cUnit.FindNodeAt(iLine, iColumn);
		pcFunction = pcNode ? (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcNode, CPPS_FunctionDefinition) : NULL;
		pcName = (pcFunction && pcFunction->mpcDeclarator) ? pcFunction->mpcDeclarator->GetName() : NULL;
		bFound = pcName && pcName->GetLast() && cUnit.GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sRange) &&
				 mpcContent->GetCodeModel()->GetSymbolAt(pTab->GetFileName(), sRange.iLine, sRange.iColumn, pszKey, puiKind);
	}
	cUnit.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Where every "go to" with several answers ends.  With no target the
// status line says there are none ("No implementations of CVat::Fill.");
// one is gone to, its name selected; several are listed in a chooser
// under the caret, with a preview of each, for one to be chosen.  A
// target that is where the caret already is isn't one.  szWhat is what
// they are, in the plural ("implementations"); szOf is the key of what
// they are of.  True if there was anywhere to go.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GoToTargets(const char* szWhat, const char* szOf, CArrayIDETargets* pasTargets)
{
	Ptr<CIDEEditorTab>	pTab;
	SIDETarget*			psTarget;
	CChars				szText;
	CChars				szName;
	size				i;
	size				uiTargets;
	int					iLine;
	int					iColumn;

	//Where the caret is isn't somewhere to go.
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNotNull() && pTab->GetCaretPlace(&iLine, &iColumn))
	{
		for (i = pasTargets->NumElements(); i > 0; i--)
		{
			psTarget = pasTargets->Get(i - 1);
			if ((psTarget->sLocation.iLine == iLine) && (iColumn >= psTarget->sLocation.iColumn) && (iColumn <= psTarget->sLocation.iColumn + psTarget->sLocation.iLength) &&
				(StringCompare(psTarget->sLocation.szFileName.Text(), pTab->GetFileName()) == 0))
			{
				psTarget->szKey.Kill();
				psTarget->sLocation.szFileName.Kill();
				pasTargets->RemoveAt(i - 1);
			}
		}
	}

	szName.Init();
	GetIDEScopedName(szOf, &szName);
	uiTargets = pasTargets->NumElements();
	if (uiTargets == 0)
	{
		szText.Init("No ");
		szText.Append(szWhat);
		szText.Append(" of ");
		szText.Append(szName);
		szText.Append(".");
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
	}
	else if (uiTargets == 1)
	{
		mpcContent->GetDocuments()->OpenLocation(&pasTargets->Get(0)->sLocation, true);
	}
	else
	{
		//"Implementations of CVat::Fill  3".
		szText.Init(szWhat);
		szText.SetChar(0, (char)toupper((unsigned char)szWhat[0]));
		szText.Append(" of ");
		szText.Append(szName);
		szText.Append("  ");
		szText.Append((uint32)uiTargets);
		if (mpTargets.IsNull())
		{
			mpTargets = OMalloc<CIDEChooser>(mpcContent);
		}
		mpTargets->Begin(szText.Text(), true, true, NULL);
		szText.Kill();
		mpTargets->SetSelectName(true);
		for (i = 0; i < uiTargets; i++)
		{
			mpTargets->AddTarget(pasTargets->Get(i));
		}
		mpTargets->OpenAtCaret();
	}
	szName.Kill();
	return uiTargets > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Implementation (Control+Alt+B), as IntelliJ's: from a virtual
// member function, the functions that override it; from a class, the
// classes derived from it; from a function's declaration, its definition.
// False if no symbol is named at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GoToImplementation(void)
{
	CArrayIDETargets	asTargets;
	CChars				szKey;
	uint8				uiKind;
	bool				bResult;

	szKey.Init();
	bResult = FindCaretSymbol(&szKey, &uiKind);
	if (bResult)
	{
		asTargets.Init();
		mpcContent->GetCodeModel()->FindImplementations(szKey.Text(), &asTargets);
		GoToTargets("implementations", szKey.Text(), &asTargets);
		KillIDETargets(&asTargets);
	}
	else
	{
		mpcContent->ShowStatus("There is no symbol at the caret to go to the implementations of.");
	}
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Super Method (Control+U), as IntelliJ's: from a member function
// (named at the caret, or the one the caret is in), the functions it
// overrides in its class's base classes; from a class, its base classes.
// False if the caret is in neither.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GoToSuperMethod(void)
{
	CArrayIDETargets	asTargets;
	CChars				szKey;
	uint8				uiKind;
	bool				bResult;

	szKey.Init();
	bResult = FindCaretSymbol(&szKey, &uiKind) && ((uiKind == CPPSK_Function) || (uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate));
	if (!bResult)
	{
		szKey.Clear();
		bResult = FindCaretFunction(&szKey, &uiKind);
	}
	if (bResult)
	{
		asTargets.Init();
		mpcContent->GetCodeModel()->FindSuperMethods(szKey.Text(), &asTargets);
		GoToTargets(((uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate)) ? "super methods" : "base classes", szKey.Text(), &asTargets);
		KillIDETargets(&asTargets);
	}
	else
	{
		mpcContent->ShowStatus("The caret isn't in a member function or on a class to go to the super of.");
	}
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The type of the variable, field, parameter or function szKey, as its
// declaration names it: the class, enum or typedef written there, found
// in the file that declares it, parsed as it is now (CIDERefactorUnit::
// FindDeclaredTypeName).  A class, an enum or a typedef is its own.
// False if its type has no name of its own (int, auto) or isn't in the
// index.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::FindTypeOf(const char* szKey, uint8 uiKind, CChars* pszTypeKey)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	CIDERefactorUnit	cUnit;
	SIDELocation*		psLocation;
	SCPPSyntaxRange		sRange;
	size				i;
	uint8				uiFoundKind;
	uint8				uiTypeKind;
	bool				bFound;

	if ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate) || (uiKind == CPPSK_Enum) || (uiKind == CPPSK_Typedef) || (uiKind == CPPSK_AliasTemplate))
	{
		pszTypeKey->Set(szKey);
		return true;
	}

	pcModel = mpcContent->GetCodeModel();
	asLocations.Init();
	bFound = false;
	if (pcModel->GetSymbolLocations(szKey, &asLocations, &uiFoundKind))
	{
		//Its declarations in turn, until one names a type the index has: a function's may be written in two files.
		for (i = 0; (i < asLocations.NumElements()) && !bFound; i++)
		{
			psLocation = asLocations.Get(i);
			if (psLocation->uiRole == CPPIR_Reference)
			{
				continue;
			}
			cUnit.Init();
			if (cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psLocation->szFileName.Text(), false) &&
				cUnit.FindDeclaredTypeName(psLocation->iLine, psLocation->iColumn, &sRange))
			{
				bFound = pcModel->GetSymbolAt(psLocation->szFileName.Text(), sRange.iLine, sRange.iColumn, pszTypeKey, &uiTypeKind);
			}
			cUnit.Kill();
		}
	}
	KillIDELocations(&asLocations);
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Type Declaration (Control+Shift+B), as IntelliJ's: the class,
// enum or typedef that is the type of the variable, field, parameter or
// function named at the caret.  Where no symbol is named there, or its
// declaration names no type to go to, the type of the expression the
// caret is in (a call's result, with the caret on its bracket), which is
// the file's checked parse.  False if neither has a type to go to.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GoToTypeDeclaration(void)
{
	CArrayIDETargets	asTargets;
	CChars				szKey;
	CChars				szTypeKey;
	CChars				szText;
	CChars				szExpression;
	uint8				uiKind;
	bool				bResult;

	szKey.Init();
	szTypeKey.Init();
	bResult = FindCaretSymbol(&szKey, &uiKind);
	if (!bResult || !FindTypeOf(szKey.Text(), uiKind, &szTypeKey))
	{
		//The expression's type, if it is one the index has.
		szText.Init();
		szExpression.Init();
		szTypeKey.Clear();
		if (mpcContent->GetIntroduceVariable()->FindCaretExpressionType(&szExpression, &szText, &szTypeKey) && !szTypeKey.Empty())
		{
			asTargets.Init();
			mpcContent->GetCodeModel()->FindTarget(szTypeKey.Text(), &asTargets);
			GoToTargets("type declarations", szExpression.Text(), &asTargets);
			KillIDETargets(&asTargets);
			szExpression.Kill();
			szText.Kill();
			szTypeKey.Kill();
			szKey.Kill();
			return true;
		}
		szTypeKey.Clear();
		if (!bResult && !szText.Empty())
		{
			//An expression with a type the index has no declaration of (int).
			szText.Insert(0, "The type of the expression at the caret, ");
			szText.Append(", has no declaration to go to.");
			mpcContent->ShowStatus(szText.Text());
			szExpression.Kill();
			szText.Kill();
			szTypeKey.Kill();
			szKey.Kill();
			return false;
		}
		szExpression.Kill();
		szText.Kill();
	}
	if (!bResult)
	{
		mpcContent->ShowStatus("There is no symbol at the caret to go to the type of.");
	}
	else if (!FindTypeOf(szKey.Text(), uiKind, &szTypeKey))
	{
		szText.Init("The type of ");
		GetIDEScopedName(szKey.Text(), &szText);
		szText.Append(" has no declaration to go to.");
		mpcContent->ShowStatus(szText.Text());
		szText.Kill();
	}
	else
	{
		asTargets.Init();
		mpcContent->GetCodeModel()->FindTarget(szTypeKey.Text(), &asTargets);
		GoToTargets("type declarations", szKey.Text(), &asTargets);
		KillIDETargets(&asTargets);
	}
	szTypeKey.Kill();
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Related Symbol (Control+Alt+Home), as IntelliJ's: a chooser of
// everything the caret has a way to, under headings: the file's other
// half (its header, or its source); the other declarations and the
// definition of the symbol at the caret; what it overrides, or its base
// classes; and what overrides it, or the classes derived from it.  With
// one thing related it is gone to; with none the status line says so.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::GoToRelatedSymbol(void)
{
	Ptr<CIDEEditorTab>	pTab;
	CArrayIDETargets	asSupers;
	CArrayIDETargets	asImplementations;
	CArrayIDELocations	asLocations;
	SIDELocation		sFile;
	SIDELocation*		psLocation;
	CChars				szKey;
	CChars				szOther;
	CChars				szName;
	size				i;
	size				uiPlaces;
	size				uiDeclarations;
	int					iLine;
	int					iColumn;
	uint8				uiKind;
	bool				bSymbol;
	bool				bOther;
	bool				bFunction;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	szKey.Init();
	szOther.Init();
	szName.Init();
	asSupers.Init();
	asImplementations.Init();
	asLocations.Init();
	bOther = FindHeaderSource(&szOther);
	bSymbol = FindCaretSymbol(&szKey, &uiKind);
	if (!bSymbol)
	{
		szKey.Clear();
		bSymbol = FindCaretFunction(&szKey, &uiKind);
	}
	bFunction = bSymbol && ((uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate));
	uiDeclarations = 0;
	if (bSymbol)
	{
		GetIDEScopedName(szKey.Text(), &szName);
		mpcContent->GetCodeModel()->FindSuperMethods(szKey.Text(), &asSupers);
		mpcContent->GetCodeModel()->FindImplementations(szKey.Text(), &asImplementations);
		mpcContent->GetCodeModel()->GetSymbolLocations(szKey.Text(), &asLocations, &uiKind);

		//Its declarations and its definition, but the one the caret is on.
		for (i = asLocations.NumElements(); i > 0; i--)
		{
			psLocation = asLocations.Get(i - 1);
			if ((psLocation->uiRole == CPPIR_Reference) ||
				((psLocation->iLine == iLine) && (StringCompare(psLocation->szFileName.Text(), pTab->GetFileName()) == 0)))
			{
				psLocation->szFileName.Kill();
				asLocations.RemoveAt(i - 1);
			}
		}
		uiDeclarations = asLocations.NumElements();

		//A function nothing overrides has its own definition as its implementation: it is among its declarations here.
		if ((asImplementations.NumElements() == 1) && asImplementations.Get(0)->szKey.Equals(szKey.Text()))
		{
			KillIDETargets(&asImplementations);
			asImplementations.Init();
		}
	}

	uiPlaces = (bOther ? 1 : 0) + uiDeclarations + asSupers.NumElements() + asImplementations.NumElements();
	if (uiPlaces == 0)
	{
		mpcContent->ShowStatus("Nothing is related to what is at the caret.");
	}
	else
	{
		if (mpTargets.IsNull())
		{
			mpTargets = OMalloc<CIDEChooser>(mpcContent);
		}
		mpTargets->Begin(bSymbol ? szName.Text() : GetIDEFileNamePart(pTab->GetFileName()), true, true, NULL);
		mpTargets->SetSelectName(true);
		if (bOther)
		{
			mpTargets->AddHeading((CIDEIcons::GetFileType(szOther.Text()) == IFT_Header) ? "Header" : "Source");
			sFile.szFileName.Init(szOther);
			sFile.iLine = 0;
			sFile.iColumn = 0;
			sFile.iLength = 0;
			sFile.uiRole = CPPIR_Declaration;
			sFile.uiFlags = 0;
			sFile.uiUsage = 0;
			sFile.iContainer = -1;
			mpTargets->AddPlace(GetIDEFileNamePart(szOther.Text()), "", mpTargets->GetFileIcon(szOther.Text()), &sFile);
			sFile.szFileName.Kill();
		}
		if (uiDeclarations > 0)
		{
			mpTargets->AddHeading("Declarations and definition");
			for (i = 0; i < uiDeclarations; i++)
			{
				mpTargets->AddPlace(szName.Text(), NULL, mpTargets->GetSymbolIcon(uiKind), asLocations.Get(i));
			}
		}
		if (asSupers.NumElements() > 0)
		{
			mpTargets->AddHeading(bFunction ? "Super methods" : "Base classes");
			for (i = 0; i < asSupers.NumElements(); i++)
			{
				mpTargets->AddTarget(asSupers.Get(i));
			}
		}
		if (asImplementations.NumElements() > 0)
		{
			mpTargets->AddHeading(bFunction ? "Implementations" : "Derived classes");
			for (i = 0; i < asImplementations.NumElements(); i++)
			{
				mpTargets->AddTarget(asImplementations.Get(i));
			}
		}

		if (uiPlaces == 1)
		{
			mpcContent->GetDocuments()->OpenLocation(mpTargets->GetPlace(0), !bOther);
		}
		else
		{
			mpTargets->OpenAtCaret();
		}
	}
	KillIDELocations(&asLocations);
	KillIDETargets(&asImplementations);
	KillIDETargets(&asSupers);
	szName.Kill();
	szOther.Kill();
	szKey.Kill();
	return uiPlaces > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Control+click on the character at uiPosition of an editor, as
// IntelliJ's: the caret is put on it and Go to Source goes from there (an
// #include's file, a symbol's definition, or on its own declaration the
// chooser of its usages).  Where nothing is named the status line says
// no definition was found, as Go to Source's does; the caret stays where
// it was put.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::LinkPressed(CTextArea* pcEditor, size uiPosition)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->FindTab(pcEditor);
	if (pTab.IsNull())
	{
		return;
	}
	if (pTab != mpcContent->GetDocuments()->GetActiveTab())
	{
		mpcContent->GetDocuments()->SetActiveTab(pTab);
	}
	pTab->GetEditor()->SetCaretPosition(uiPosition);
	GoToSource();
}


//////////////////////////////////////////////////////////////////////////
//
// Go to Implementation is offered where a symbol is named at the caret;
// Go to Super Method anywhere in an editor, as the caret may be in a
// function, and Go to Type Declaration too, as it may be in an expression.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::CanGoToImplementation(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


bool CIDENavigator::CanGoToSuperMethod(void)
{
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull();
}


//////////////////////////////////////////////////////////////////////////
//
// The IDE is closing: the chooser's preview isn't to be given anything.
//
//////////////////////////////////////////////////////////////////////////
void CIDENavigator::Detach(void)
{
	if (mpTargets.IsNotNull())
	{
		mpTargets->Detach();
	}
}


Ptr<CIDEChooser> CIDENavigator::GetTargetsChooser(void) const { return mpTargets; }

