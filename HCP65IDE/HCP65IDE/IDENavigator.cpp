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
	CIDECodeModel*	pcCodeModel;
	size		i;
	SNameMatch	sMatch;
	char*		szPath;
	CChars		szFolder;
	CChars		szName;
	CFileUtil	cFileUtil;
	const char*	szHome;
	size		uiHome;

	pcCodeModel = mpcContent->GetCodeModel();
	if (uiTab != IDE_GO_TO_FILE)
	{
		mpcContent->GetDocuments()->UpdateEditedTabsNow();
		pcCodeModel->FindGoToSymbols(pcMatcher, uiTab == IDE_GO_TO_CLASS, maiGoToSymbolIcons, pcDest);
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
	pCommands->Add("Navigate.GoToSource", "Go to Source", "Control+B", Ptr<CImageCel>(), this, &CIDENavigator::GoToSource)->SetEnabledCheck(this, &CIDENavigator::CanGoToSource);
	pCommands->Add("Navigate.SwitchHeaderSource", "Switch Header/Source", "Alt+O", Ptr<CImageCel>(), this, &CIDENavigator::SwitchHeaderSource);
	pCommands->Add("Navigate.GoToImplementation", "Go to Implementation(s)", "Control+Alt+B", Ptr<CImageCel>(), this, &CIDENavigator::GoToImplementation)->SetEnabledCheck(this, &CIDENavigator::CanGoToImplementation);
	pCommands->Add("Navigate.GoToSuperMethod", "Go to Super Method", "Control+U", Ptr<CImageCel>(), this, &CIDENavigator::GoToSuperMethod)->SetEnabledCheck(this, &CIDENavigator::CanGoToSuperMethod);
}


void CIDENavigator::GoToClass(void) { QueueGoTo(IDE_GO_TO_CLASS); }
void CIDENavigator::GoToFile(void) { QueueGoTo(IDE_GO_TO_FILE); }
void CIDENavigator::GoToSymbol(void) { QueueGoTo(IDE_GO_TO_SYMBOL); }


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
	mpGoTo = NULL;
	return bResult;
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
// Control+B, as IntelliJ's: on an #include the file it names is opened;
// on a symbol's own declaration or definition, a popup of its usages;
// anywhere else, the definition of what is at the caret (as F12).  False
// if there's nothing to go to.
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
// Whether Go to Source has somewhere to go: an #include's file, or a
// symbol at the caret.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENavigator::CanGoToSource(void)
{
	CChars	szFile;
	int		iLine;
	int		iColumn;
	uint8	uiRole;
	bool	bResult;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	szFile.Init();
	bResult = FindIncludedFile(&szFile);
	szFile.Kill();
	return bResult || mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
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
	static const char*	aszSources[] = { "cpp", "c", "cc", "cxx" };
	static const char*	aszHeaders[] = { "h", "hpp", "hxx", "inl" };
	const char**		aszWanted;
	size				uiWanted;
	CFileUtil			cFileUtil;
	CChars				szBase;
	CChars				szPath;
	CChars				szName;
	CChars				szFound;
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
	szFound.Init();
	bFound = false;
	for (i = 0; (i < uiWanted) && !bFound; i++)
	{
		szPath.Init(szBase);
		szPath.Append('.');
		szPath.Append(aszWanted[i]);
		if (cFileUtil.Exists(szPath.Text()))
		{
			szFound.Set(szPath);
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
			bFound = FindProjectFileEnding(szPath.Text(), &szFound);
			szPath.Kill();
		}
		szName.Kill();
	}
	szBase.Kill();

	bFound = bFound && OpenProjectFile(szFound.Text());
	szFound.Kill();
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
// The function whose definition the active editor's caret is in, as the
// refactorings find it: the file parsed as it is now, the function
// definition round the caret, and the symbol its name is.  False if the
// caret isn't in one.
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
	int								iLine;
	int								iColumn;
	bool							bFound;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull() || !pTab->GetCaretPlace(&iLine, &iColumn))
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
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
// Both are offered where a symbol is named at the caret; Go to Super
// Method anywhere in an editor too, as the caret may be in a function.
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

