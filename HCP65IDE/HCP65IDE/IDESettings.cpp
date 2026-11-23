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
#include "BaseLib/FileUtil.h"
#include "BaseLib/Logger.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/XMLFile.h"
#include "BaseLib/XMLWriter.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "IDEContent.h"
#include "IDESettings.h"


//The keymap's groups, in the order they're shown: the part of a command's name before its dot and what's shown.
static SKeymapGroup	gasIDEKeymapGroups[] =
{
	{ "File", "File" },
	{ "Edit", "Edit" },
	{ "Navigate", "Navigate" },
	{ "Refactor", "Refactor" },
	{ "View", "View" },
	{ "Git", "Git" },
	{ "Build", "Build" },
	{ "Run", "Run" },
	{ "Debug", "Debug" },
	{ "Window", "Window" },
	{ "Explorer", "Solution Explorer" },
	{ "Editor", "Editor Actions" },
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mszKeymapFile.Init();
	mszSettingsFile.Init();
	mbKeyRepeat = true;
	muiKeyRepeatDelay = IDE_KEY_REPEAT_DELAY;
	muiKeyRepeatRate = IDE_KEY_REPEAT_RATE;
	muiCodeFontSize = IDE_CODE_FONT_SIZE;
	mbWheelZoom = true;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::Class(void)
{
	U_Pointer(mpcContent);
	U_String(mszKeymapFile);
	U_String(mszSettingsFile);
	U_Bool(mbKeyRepeat);
	U_UInt32(muiKeyRepeatDelay);
	U_UInt32(muiKeyRepeatRate);
	U_UInt32(muiCodeFontSize);
	U_Bool(mbWheelZoom);
	M_Pointer(mpDialog);
	M_Pointer(mpKeymapPage);
	M_Pointer(mpTemplatesPage);
	M_Pointer(mpDuplicatesPage);
	M_Pointer(mpClaudePage);
	M_Pointer(mpEditorPage);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::Free(void)
{
	mszSettingsFile.Kill();
	mszKeymapFile.Kill();
}


char* CIDESettings::GetKeymapFile(void) { return mszKeymapFile.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// The keymap page's dialogs are watched by the IDE's script, for its
// checks (see CKeymapPageWatcher).
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::WatchDialog(Ptr<CDialog> pDialog) { mpcContent->GetScript()->WatchDialog(pDialog); }
void CIDESettings::StopWatchingDialog(void) { mpcContent->GetScript()->StopWatchingDialog(); }
void CIDESettings::ShowCaptureWhenDrawn(CKeymapPage* pcPage) { mpcContent->GetScript()->SetDialogDrawnCall(pcPage, &CKeymapPage::ShowCapture); }
bool CIDESettings::Save(CObjectWriter* pcFile) { return false; }
bool CIDESettings::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("File.Settings", "Settings...", "Control+Alt+S", Ptr<CImageCel>(), this, &CIDESettings::RequestDialog);
	pCommands->Add("View.IncreaseFontSize", "Increase Font Size", "Control+=", Ptr<CImageCel>(), this, &CIDESettings::IncreaseFontSize);
	pCommands->Add("View.DecreaseFontSize", "Decrease Font Size", "Control+-", Ptr<CImageCel>(), this, &CIDESettings::DecreaseFontSize);
	pCommands->Add("View.ResetFontSize", "Reset Font Size", "Control+0", Ptr<CImageCel>(), this, &CIDESettings::ResetFontSize);
}


//////////////////////////////////////////////////////////////////////////
//
// The user's keys, from szFile, before the commands are bound.  It being
// missing only means none have been changed; one that can't be read is
// said, and the commands keep their own keys.  "" keeps none.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ReadKeymap(const char* szFile)
{
	CFileUtil	cFileUtil;

	mszKeymapFile.Set(szFile);
	if (!mszKeymapFile.Empty() && cFileUtil.Exists(szFile))
	{
		if (!mpcContent->GetCommands()->ReadKeys(szFile))
		{
			gcLogger.Warning2(__METHOD__, " Keymap [", szFile, "] couldn't be read: the default keys are used.", NULL);
			mpcContent->ShowStatus("The keymap couldn't be read: the default keys are used.");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the IDE keeps of how the user left it, from szFile, as it starts:
// each part of the IDE reads its own from the file's root (Find Usages its
// groups).  It being missing only means nothing has been kept; "" keeps
// nothing.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ReadSettings(const char* szFile)
{
	CXMLFile	cFile;
	CFileUtil	cFileUtil;
	CChars		szFileName;
	CChars		szDirectory;
	CMarkupTag*	pcRoot;

	mszSettingsFile.Set(szFile);
	if (mszSettingsFile.Empty() || !cFileUtil.Exists(szFile))
	{
		return;
	}
	szFileName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFile, &szFileName, &szDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(".");
	}
	cFile.Init();
	pcRoot = (cFile.Read(szFileName.Text(), szDirectory.Text()) && cFile.mcMarkup.mpcDoc) ? cFile.mcMarkup.mpcDoc->GetRootTag() : NULL;
	if (pcRoot && (StringCompare(pcRoot->GetName(), IDE_SETTINGS_TAG) == 0))
	{
		mpcContent->GetUsages()->ReadSettings(pcRoot);
		mpcContent->GetFindInFiles()->ReadSettings(pcRoot);
		mpcContent->GetDuplicates()->ReadSettings(pcRoot);
		mpcContent->GetRename()->ReadSettings(pcRoot);
		mpcContent->GetFileRename()->ReadSettings(pcRoot);
		mpcContent->GetFileTemplates()->ReadSettings(pcRoot);
		mpcContent->GetAskClaude()->ReadSettings(pcRoot);
		mpcContent->GetStructure()->ReadSettings(pcRoot);
		mpcContent->GetFind()->ReadSettings(pcRoot);
		mpcContent->GetSolutions()->ReadSettings(pcRoot);
		mpcContent->GetNavigator()->ReadSettings(pcRoot);
		ReadKeyRepeat(pcRoot);
		ReadCodeFont(pcRoot);
	}
	else
	{
		gcLogger.Warning2(__METHOD__, " Settings [", szFile, "] couldn't be read.", NULL);
	}
	cFile.Kill();
	szDirectory.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What ReadSettings reads, written when a part of it changes.  False if
// it couldn't be written; true if it was, or nothing is kept.
//
//////////////////////////////////////////////////////////////////////////
bool CIDESettings::WriteSettings(void)
{
	CXMLWriter	cWriter;
	CFileUtil	cFileUtil;
	bool		bResult;

	if (mszSettingsFile.Empty())
	{
		return true;
	}
	cFileUtil.TouchDir(mszSettingsFile.Text(), true);
	cWriter.Init();
	cWriter.Begin(IDE_SETTINGS_TAG);
	mpcContent->GetUsages()->WriteSettings(&cWriter);
	mpcContent->GetFindInFiles()->WriteSettings(&cWriter);
	mpcContent->GetDuplicates()->WriteSettings(&cWriter);
	mpcContent->GetRename()->WriteSettings(&cWriter);
	mpcContent->GetFileRename()->WriteSettings(&cWriter);
	mpcContent->GetFileTemplates()->WriteSettings(&cWriter);
	mpcContent->GetAskClaude()->WriteSettings(&cWriter);
	mpcContent->GetStructure()->WriteSettings(&cWriter);
	mpcContent->GetFind()->WriteSettings(&cWriter);
	mpcContent->GetSolutions()->WriteSettings(&cWriter);
	mpcContent->GetNavigator()->WriteSettings(&cWriter);
	cWriter.Begin("KeyRepeat");
	cWriter.Attribute("On", mbKeyRepeat);
	cWriter.Attribute("Delay", (int32)muiKeyRepeatDelay);
	cWriter.Attribute("Rate", (int32)muiKeyRepeatRate);
	cWriter.End();
	cWriter.Begin("CodeFont");
	cWriter.Attribute("Size", (int32)muiCodeFontSize);
	cWriter.Attribute("WheelZoom", mbWheelZoom);
	cWriter.End();
	cWriter.End();
	bResult = cWriter.Write(mszSettingsFile.Text());
	cWriter.Kill();
	return bResult;
}


char* CIDESettings::GetSettingsFile(void) { return mszSettingsFile.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// <KeyRepeat On="true" Delay="500" Rate="30"/> in the settings: whether
// held keys repeat, after how many milliseconds and how many times a
// second.  Without it, or with a number out of range, they repeat as
// Windows' defaults do.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ReadKeyRepeat(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;
	char*		szValue;
	int			iValue;

	pcTag = pcSettings->GetTag("KeyRepeat");
	if (pcTag == NULL)
	{
		return;
	}
	szValue = pcTag->GetStringAttribute("On");
	mbKeyRepeat = (szValue == NULL) || (StringCompare(szValue, "false") != 0);
	szValue = pcTag->GetStringAttribute("Delay");
	iValue = (szValue != NULL) ? atoi(szValue) : 0;
	muiKeyRepeatDelay = ((iValue >= IDE_KEY_REPEAT_MIN_DELAY) && (iValue <= IDE_KEY_REPEAT_MAX_DELAY)) ? (uint32)iValue : IDE_KEY_REPEAT_DELAY;
	szValue = pcTag->GetStringAttribute("Rate");
	iValue = (szValue != NULL) ? atoi(szValue) : 0;
	muiKeyRepeatRate = ((iValue >= IDE_KEY_REPEAT_MIN_RATE) && (iValue <= IDE_KEY_REPEAT_MAX_RATE)) ? (uint32)iValue : IDE_KEY_REPEAT_RATE;
}


//////////////////////////////////////////////////////////////////////////
//
// The Editor page's: held keys repeat (bRepeat) after uiDelay
// milliseconds, uiRate times a second, from now on.  Written with the
// settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::SetKeyRepeat(bool bRepeat, uint32 uiDelay, uint32 uiRate)
{
	mbKeyRepeat = bRepeat;
	muiKeyRepeatDelay = uiDelay;
	muiKeyRepeatRate = uiRate;
	ApplyKeyRepeat();
	WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// The window's input is given the key repeat, once it has one.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ApplyKeyRepeat(void)
{
	CComponentInput*	pcInput;

	pcInput = mpcContent->GetWindow().IsNotNull() ? mpcContent->GetWindow()->GetComponentInput() : NULL;
	if (pcInput)
	{
		pcInput->SetRepeat(mbKeyRepeat ? muiKeyRepeatDelay : 0, 1000 / muiKeyRepeatRate);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// <CodeFont Size="13" WheelZoom="true"/> in the settings: the code's font
// size, as it was last zoomed, and whether Control and the wheel zoom it.
// A size out of range is the default's.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ReadCodeFont(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;
	char*		szValue;
	int			iValue;

	pcTag = pcSettings->GetTag("CodeFont");
	if (pcTag == NULL)
	{
		return;
	}
	szValue = pcTag->GetStringAttribute("Size");
	iValue = (szValue != NULL) ? atoi(szValue) : 0;
	muiCodeFontSize = ((iValue >= IDE_CODE_FONT_MIN_SIZE) && (iValue <= IDE_CODE_FONT_MAX_SIZE)) ? (uint32)iValue : IDE_CODE_FONT_SIZE;
	szValue = pcTag->GetStringAttribute("WheelZoom");
	mbWheelZoom = (szValue == NULL) || (StringCompare(szValue, "false") != 0);
	ApplyCodeFontSize();
}


//////////////////////////////////////////////////////////////////////////
//
// The Editor page's: the code's font size, and whether Control and the
// wheel zoom it.  Written with the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::SetCodeFont(uint32 uiSize, bool bWheelZoom)
{
	mbWheelZoom = bWheelZoom;
	ZoomCodeFont(uiSize);
	WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// Every code view (editors, previews, the Build, Run and Indexing tool
// windows' text, diffs) is in the code font family's default size: each
// text area in it is laid out again in the new size as the window is
// (CTextArea::SeeFamilySize).
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ApplyCodeFontSize(void)
{
	if (mpcContent->GetCodeFamily().IsNotNull())
	{
		mpcContent->GetCodeFamily()->SetDefaultPixelSize((float32)muiCodeFontSize);
		if (mpcContent->GetWindow().IsNotNull())
		{
			mpcContent->GetWindow()->InvalidateLayout();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The code zoomed to uiSize (kept in range), as IntelliJ's Increase and
// Decrease Font Size zoom all its editors: the active editor's caret is
// kept in view and the size is shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ZoomCodeFont(uint32 uiSize)
{
	Ptr<CIDEEditorTab>	pTab;
	char				szStatus[64];

	uiSize = (uiSize < IDE_CODE_FONT_MIN_SIZE) ? IDE_CODE_FONT_MIN_SIZE : ((uiSize > IDE_CODE_FONT_MAX_SIZE) ? IDE_CODE_FONT_MAX_SIZE : uiSize);
	if (uiSize == muiCodeFontSize)
	{
		return;
	}
	muiCodeFontSize = uiSize;
	ApplyCodeFontSize();
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNotNull())
	{
		mpcContent->GetWindow()->Layout();
		pTab->GetEditor()->ShowCaret();
	}
	snprintf(szStatus, sizeof(szStatus), "Font size: %d", (int)muiCodeFontSize);
	mpcContent->ShowStatus(szStatus);
}


void CIDESettings::IncreaseFontSize(void) { ZoomCodeFont(muiCodeFontSize + 1); WriteSettings(); }
void CIDESettings::DecreaseFontSize(void) { ZoomCodeFont(muiCodeFontSize - 1); WriteSettings(); }
void CIDESettings::ResetFontSize(void) { ZoomCodeFont(IDE_CODE_FONT_SIZE); WriteSettings(); }


//////////////////////////////////////////////////////////////////////////
//
// After View's other items: the code's zoom.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::AddViewItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddSeparator();
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.IncreaseFontSize"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.DecreaseFontSize"));
	pMenu->AddItem(mpcContent->GetCommands()->Find("View.ResetFontSize"));
}


//////////////////////////////////////////////////////////////////////////
//
// Scroll panes pass Control and the wheel on (CScrollPane::WheelAction):
// pRoot, the window's container, gets it from whatever it was over.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::AddZoomWheel(Ptr<CComponent> pRoot)
{
	pRoot->AddInputAction(IAF_Wheel, NULL, this, &CIDESettings::ZoomWheel);
}


//////////////////////////////////////////////////////////////////////////
//
// Control and the wheel: over code (a text area in the code font) it
// zooms one size a notch, up bigger, unless the Editor page turned that
// off; anywhere else, or then, it scrolls what it is over as the wheel
// alone does.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ZoomWheel(SComponentInputEvent* psEvent)
{
	Ptr<CComponent>		pComponent;
	Ptr<CTextArea>		pText;
	Ptr<CScrollPane>	pPane;

	if (!psEvent->bControl)
	{
		psEvent->bPass = true;
		return;
	}
	for (pComponent = mpcContent->GetWindow()->GetFocus()->GetMouseOverComponent(); pComponent.IsNotNull(); pComponent = pComponent->GetParent())
	{
		pText = pComponent;
		if (mbWheelZoom && pText.IsNotNull() && pText.IsInstanceOfTemplateClass() && (pText->GetFontFamily() == mpcContent->GetCodeFamily()))
		{
			if (psEvent->iDown != 0)
			{
				ZoomCodeFont((uint32)((int32)muiCodeFontSize - psEvent->iDown));
				WriteSettings();
			}
			return;
		}
		pPane = pComponent;
		if (pPane.IsNotNull() && pPane.IsInstanceOfTemplateClass() && pPane->Wheel(psEvent->iRight, psEvent->iDown))
		{
			return;
		}
	}
	psEvent->bPass = true;
}


bool CIDESettings::IsKeyRepeat(void) const { return mbKeyRepeat; }
uint32 CIDESettings::GetKeyRepeatDelay(void) const { return muiKeyRepeatDelay; }
uint32 CIDESettings::GetKeyRepeatRate(void) const { return muiKeyRepeatRate; }
uint32 CIDESettings::GetCodeFontSize(void) const { return muiCodeFontSize; }
bool CIDESettings::IsWheelZoom(void) const { return mbWheelZoom; }


//////////////////////////////////////////////////////////////////////////
//
// A dialog can't be shown from the keys or a menu item's input action.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::RequestDialog(void)
{
	mpcContent->GetWindow()->Post(this, &CIDESettings::ShowDialog);
}


//////////////////////////////////////////////////////////////////////////
//
// What OK keeps applies now, and is written for next time.
//
//////////////////////////////////////////////////////////////////////////
void CIDESettings::ShowDialog(void)
{
	Ptr<CWindow>	pWindow;
	CChars			szProblem;
	bool			bOK;

	pWindow = mpcContent->GetWindow();
	mpDialog = OMalloc<CSettingsDialog>(pWindow, mpcContent->GetDialogLook());
	mpKeymapPage = OMalloc<CKeymapPage>(mpDialog->GetDialog(), mpcContent->GetCommands(), pWindow->GetComponentInput(), gasIDEKeymapGroups, sizeof(gasIDEKeymapGroups) / sizeof(gasIDEKeymapGroups[0]), this);
	mpDialog->AddPage(mpKeymapPage);
	mpTemplatesPage = OMalloc<CIDEFileTemplatesPage>(mpcContent, mpDialog->GetDialog());
	mpDialog->AddPage(mpTemplatesPage);
	mpDuplicatesPage = OMalloc<CIDEDuplicatesPage>(mpcContent, mpDialog->GetDialog());
	mpDialog->AddPage(mpDuplicatesPage);
	mpClaudePage = OMalloc<CIDEClaudePage>(mpcContent, mpDialog->GetDialog());
	mpDialog->AddPage(mpClaudePage);
	mpEditorPage = OMalloc<CIDEEditorPage>(mpcContent, mpDialog->GetDialog());
	mpDialog->AddPage(mpEditorPage);
	if (getenv("HCP65_IDE_KEYMAP_EDITS") != NULL)
	{
		mpKeymapPage->ApplyEdits(getenv("HCP65_IDE_KEYMAP_EDITS"));
	}
	if (getenv("HCP65_IDE_TEMPLATE_EDITS") != NULL)
	{
		mpTemplatesPage->ApplyEdits(getenv("HCP65_IDE_TEMPLATE_EDITS"));
	}
	if (getenv("HCP65_IDE_DUPLICATES_EDITS") != NULL)
	{
		mpDuplicatesPage->ApplyEdits(getenv("HCP65_IDE_DUPLICATES_EDITS"));
	}
	if (getenv("HCP65_IDE_CLAUDE_EDITS") != NULL)
	{
		mpClaudePage->ApplyEdits(getenv("HCP65_IDE_CLAUDE_EDITS"));
	}
	if (getenv("HCP65_IDE_EDITOR_EDITS") != NULL)
	{
		mpEditorPage->ApplyEdits(getenv("HCP65_IDE_EDITOR_EDITS"));
	}
	mpcContent->GetScript()->WatchDialog(mpDialog->GetDialog());
	bOK = mpDialog->Show();
	mpcContent->GetScript()->StopWatchingDialog();
	szProblem.Init(mpTemplatesPage->GetKeepProblem());
	mpKeymapPage = NULL;
	mpTemplatesPage = NULL;
	mpDuplicatesPage = NULL;
	mpClaudePage = NULL;
	mpEditorPage = NULL;
	mpDialog = NULL;

	if (bOK)
	{
		mpcContent->GetCommands()->BindKeys(pWindow->GetComponentInput());
		if (mszKeymapFile.Empty())
		{
			mpcContent->ShowStatus("Changed the keymap.");
		}
		else if (mpcContent->GetCommands()->WriteKeys(mszKeymapFile.Text()))
		{
			mpcContent->ShowStatus("Saved the keymap.");
		}
		else
		{
			mpcContent->ShowStatus("Changed the keymap, but it couldn't be saved for next time.");
		}
		if (!szProblem.Empty())
		{
			mpcContent->ShowStatus(szProblem.Text());
		}
	}
	szProblem.Kill();
}

