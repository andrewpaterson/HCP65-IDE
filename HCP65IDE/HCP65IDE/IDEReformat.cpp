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
#include "BaseLib/TextFile.h"
#include "StandardLib/Objects.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDEDefinitions.h"
#include "IDEFileTemplates.h"
#include "IDEReformat.h"


//What the status line calls each kind of change, one of them and several.
static const char*	gaszIDEReformatChanges[CFC_NumChanges][2] =
{
	{ "line indented", "lines indented" }, { "brace moved", "braces moved" }, { "line split after a brace", "lines split after a brace" },
	{ "comma spaced", "commas spaced" }, { "keyword spaced", "keywords spaced" }, { "operator spaced", "operators spaced" },
	{ "declaration lined up", "declarations lined up" }, { "star put against its type", "stars put against their types" },
	{ "banner added", "banners added" }, { "line end trimmed", "line ends trimmed" },
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEReformat::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	memset(mauiCounts, 0, sizeof(mauiCounts));
	PostInit();
}


void CIDEReformat::Class(void)
{
	U_Pointer(mpcContent);
	U_Data(size[CFC_NumChanges], mauiCounts);
}


void CIDEReformat::Free(void) {}
bool CIDEReformat::Save(CObjectWriter* pcFile) { return false; }
bool CIDEReformat::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Reformat Code (Control+Alt+L, as IntelliJ's) and Solution Explorer's.
// Each is done at the next tick, outside the input: it can open files.
//
//////////////////////////////////////////////////////////////////////////
void CIDEReformat::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Edit.ReformatCode", "Reformat Code", "Control+Alt+L", Ptr<CImageCel>(), this, &CIDEReformat::QueueReformat)->SetEnabledCheck(this, &CIDEReformat::CanReformatActiveFile);
	pCommands->Add("Explorer.ReformatCode", "Reformat Code", "", Ptr<CImageCel>(), this, &CIDEReformat::QueueExplorerReformat)->SetEnabledCheck(this, &CIDEReformat::CanReformatExplorerSelection);
}


void CIDEReformat::AddEditItems(Ptr<CMenuContainer> pMenu) { pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.ReformatCode")); }
void CIDEReformat::AddExplorerItems(Ptr<CMenuContainer> pMenu) { pMenu->AddItem(mpcContent->GetCommands()->Find("Explorer.ReformatCode")); }
void CIDEReformat::QueueReformat(void) { mpcContent->GetWindow()->Post(this, &CIDEReformat::ReformatQueued); }
void CIDEReformat::QueueExplorerReformat(void) { mpcContent->GetWindow()->Post(this, &CIDEReformat::ExplorerReformatQueued); }
void CIDEReformat::ReformatQueued(void) { ReformatActiveFile(); }
void CIDEReformat::ExplorerReformatQueued(void) { ReformatExplorerSelection(); }


//////////////////////////////////////////////////////////////////////////
//
// Whether a file is one Reformat Code lays out: C or C++, a source file
// or a header.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEReformat::IsReformatted(const char* szFileName)
{
	EIDEFileType	eType;

	eType = CIDEIcons::GetFileType(szFileName);
	return (eType == IFT_CPP) || (eType == IFT_C) || (eType == IFT_Header);
}


bool CIDEReformat::CanReformatActiveFile(void)
{
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && IsReformatted(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
}


//////////////////////////////////////////////////////////////////////////
//
// The banner a function with no comment gets is the Definition Banner
// template's; the house's if the template has an error.
//
//////////////////////////////////////////////////////////////////////////
void CIDEReformat::SetBanner(CCodeFormatter* pcFormatter)
{
	CChars	szBanner;

	szBanner.Init();
	if (mpcContent->GetFileTemplates()->ApplyCode(IDE_TEMPLATE_DEFINITION_BANNER, &szBanner, NULL) && !szBanner.Empty())
	{
		pcFormatter->SetBanner(szBanner.Text());
	}
	szBanner.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The formatter's edits of a file added to pcEdits, those in [uiFrom,
// uiTo) of its text (MAX_SIZE: to its end), and counted.  The file's
// editor is opened, not shown, if it isn't open.  False if nothing of it
// changes.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEReformat::AddFileEdits(CIDEFileEdits* pcEdits, const char* szFileName, size uiFrom, size uiTo)
{
	CCodeFormatter	cFormatter;
	CIDETextEdits*	pcFileEdits;
	SCodeEdit*		psEdit;
	CChars			szText;
	const char*		szSource;
	size			i;
	size			uiAdded;

	szSource = pcEdits->GetText(szFileName);
	if (szSource == NULL)
	{
		return false;
	}
	cFormatter.Init();
	SetBanner(&cFormatter);
	cFormatter.Format(szSource, CIDEIcons::GetFileType(szFileName) != IFT_Header);
	uiAdded = 0;
	pcFileEdits = NULL;
	for (i = 0; i < cFormatter.NumEdits(); i++)
	{
		psEdit = cFormatter.GetEdit(i);
		if ((psEdit->uiStart >= uiFrom) && (psEdit->uiEnd <= uiTo))
		{
			if (pcFileEdits == NULL)
			{
				pcFileEdits = pcEdits->GetEdits(szFileName);
			}
			szText.Init();
			szText.Append(cFormatter.GetEditText(psEdit), psEdit->uiLength);
			pcFileEdits->Replace(psEdit->uiStart, psEdit->uiEnd, szText.Text());
			szText.Kill();
			uiAdded++;
		}
	}
	if ((uiAdded == cFormatter.NumEdits()) || (uiAdded == 0))
	{
		for (i = 0; (i < CFC_NumChanges) && (uiAdded > 0); i++)
		{
			mauiCounts[i] += cFormatter.GetCount((ECodeFormatChange)i);
		}
	}
	cFormatter.Kill();
	return uiAdded > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// What was changed, for the status line: "3 lines indented, 1 banner
// added".  Nothing for a selection's, which aren't counted by kind.
//
//////////////////////////////////////////////////////////////////////////
void CIDEReformat::AppendCounts(CChars* pszDest)
{
	size	i;
	bool	bFirst;

	bFirst = true;
	for (i = 0; i < CFC_NumChanges; i++)
	{
		if (mauiCounts[i] > 0)
		{
			pszDest->Append(bFirst ? ": " : ", ");
			pszDest->Append((uint64)mauiCounts[i]);
			pszDest->Append(' ');
			pszDest->Append(gaszIDEReformatChanges[i][(mauiCounts[i] == 1) ? 0 : 1]);
			bFirst = false;
		}
	}
	pszDest->Append('.');
}


//////////////////////////////////////////////////////////////////////////
//
// Reformat Code: the active file, or the lines its selection is on, laid
// out in the house style as one undo.  False if it isn't C or C++ or it
// is in the style already; the status line says which.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEReformat::ReformatActiveFile(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	CIDEFileEdits		cEdits;
	CChars				szStatus;
	size				uiStart;
	size				uiEnd;
	bool				bSelection;
	bool				bChanged;

	if (!CanReformatActiveFile())
	{
		mpcContent->ShowStatus("Reformat Code lays out C and C++: open a source file or a header.");
		return false;
	}
	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pEditor = pTab->GetEditor();
	bSelection = pEditor->GetSelection(&uiStart, &uiEnd);
	if (bSelection)
	{
		//The lines the selection is on, whole.
		uiStart = pEditor->GetLineStart(pEditor->FindLine(uiStart));
		uiEnd = pEditor->GetLineEnd(pEditor->FindLine((uiEnd > uiStart) ? uiEnd - 1 : uiEnd));
	}
	else
	{
		uiStart = 0;
		uiEnd = MAX_SIZE;
	}

	memset(mauiCounts, 0, sizeof(mauiCounts));
	cEdits.Init(mpcContent);
	bChanged = AddFileEdits(&cEdits, pTab->GetFileName(), uiStart, uiEnd);
	if (bChanged)
	{
		cEdits.Apply();
	}
	cEdits.Kill();

	szStatus.Init(bChanged ? "Reformatted " : "");
	szStatus.Append(bSelection ? "the selected lines of " : "");
	szStatus.Append(GetIDEFileNamePart(pTab->GetFileName()));
	if (bChanged)
	{
		AppendCounts(&szStatus);
	}
	else
	{
		szStatus.Append(bSelection ? ": they are in the house style already." : " is in the house style already.");
	}
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
	return bChanged;
}


//////////////////////////////////////////////////////////////////////////
//
// The files Reformat Code lays out under a node of Solution Explorer's
// tree: the node's own, or those of everything in it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEReformat::FindExplorerFiles(int32 iNode, CArrayChars* paszFiles)
{
	Ptr<CTreeContainer>	pTree;
	CChars				szFile;
	int32				iChild;

	pTree = mpcContent->GetSolutions()->GetExplorer()->GetTree();
	szFile.Init();
	if (mpcContent->GetSolutions()->GetExplorer()->GetNodeFilePath(iNode, &szFile) && IsReformatted(szFile.Text()) && (paszFiles->GetIndex(szFile.Text()) == ARRAY_ELEMENT_NOT_FOUND))
	{
		paszFiles->Add(szFile.Text());
	}
	szFile.Kill();
	for (iChild = pTree->GetFirstChild(iNode); iChild != TREE_NO_NODE; iChild = pTree->GetNextSibling(iChild))
	{
		FindExplorerFiles(iChild, paszFiles);
	}
}


bool CIDEReformat::CanReformatExplorerSelection(void)
{
	return mpcContent->GetSolutions()->GetExplorer()->GetTree()->GetSelectedNode() != TREE_NO_NODE;
}


//////////////////////////////////////////////////////////////////////////
//
// Solution Explorer's Reformat Code: the selected file, or every C and
// C++ file of the selected folder, project or solution, laid out as one
// undo.  A file that changes is opened, not shown, and left changed.
// How many files changed; the status line says, and what was changed.
//
//////////////////////////////////////////////////////////////////////////
size CIDEReformat::ReformatExplorerSelection(void)
{
	CCodeFormatter		cFormatter;
	CIDEFileEdits		cEdits;
	CArrayChars			aszFiles;
	CTextFile			cFile;
	CChars				szStatus;
	const char*			szText;
	size				uiLength;
	size				uiChanged;
	size				i;
	bool				bChanges;

	aszFiles.Init();
	if (CanReformatExplorerSelection())
	{
		FindExplorerFiles(mpcContent->GetSolutions()->GetExplorer()->GetTree()->GetSelectedNode(), &aszFiles);
	}
	if (aszFiles.NumElements() == 0)
	{
		aszFiles.Kill();
		mpcContent->ShowStatus("Reformat Code lays out C and C++: select a source file or a header, or a folder or project that has some.");
		return 0;
	}

	memset(mauiCounts, 0, sizeof(mauiCounts));
	uiChanged = 0;
	cEdits.Init(mpcContent);
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		//A file is opened only if it changes: its text is read first, as its editor has it or from the disk.
		cFile.Init();
		bChanges = false;
		if (mpcContent->GetFindInFiles()->GetFileText(aszFiles.GetText(i), &cFile, &szText, &uiLength))
		{
			cFormatter.Init();
			SetBanner(&cFormatter);
			cFormatter.Format(szText, CIDEIcons::GetFileType(aszFiles.GetText(i)) != IFT_Header);
			bChanges = cFormatter.IsChanged();
			cFormatter.Kill();
		}
		cFile.Kill();
		if (bChanges && AddFileEdits(&cEdits, aszFiles.GetText(i), 0, MAX_SIZE))
		{
			uiChanged++;
		}
	}
	if (uiChanged > 0)
	{
		cEdits.Apply();
	}
	cEdits.Kill();

	szStatus.Init();
	if (uiChanged == 0)
	{
		szStatus.Append((aszFiles.NumElements() == 1) ? GetIDEFileNamePart(aszFiles.GetText(0)) : "Every file there");
		szStatus.Append(" is in the house style already.");
	}
	else
	{
		szStatus.Append("Reformatted ");
		if (aszFiles.NumElements() == 1)
		{
			szStatus.Append(GetIDEFileNamePart(aszFiles.GetText(0)));
		}
		else
		{
			szStatus.Append((uint64)uiChanged)->Append(" of ")->Append((uint64)aszFiles.NumElements())->Append(" files");
		}
		AppendCounts(&szStatus);
		szStatus.Append("  Save All keeps them.");
	}
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
	aszFiles.Kill();
	return uiChanged;
}

