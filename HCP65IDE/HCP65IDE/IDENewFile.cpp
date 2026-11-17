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
#include "BaseLib/FileUtil.h"
#include "TemplateLib/TemplateStrings.h"
#include "TemplateLib/TemplateText.h"
#include "IDEContent.h"
#include "IDEDefinitions.h"
#include "IDEFileTemplates.h"
#include "IDETemplateVariables.h"
#include "IDENewFile.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENewPlace::Init(void)
{
	pcProject = NULL;
	uiProject = MAX_SIZE;
	szDirectory.Init();
	szFilter.Init();
	szListBeside.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDENewPlace::Kill(void)
{
	szListBeside.Kill();
	szFilter.Kill();
	szDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	maszTemplates.Init();
	mpFileMenu = OMalloc<CMenuContainer>(pcContent->GetWindow(), pcContent->GetTheme());
	mpPopupMenu = OMalloc<CMenuContainer>(pcContent->GetWindow(), pcContent->GetTheme());
	mpFileMenu->GetOpeningEvent()->Add(this, &CIDENewFile::FileMenuOpening);
	mpPopupMenu->GetOpeningEvent()->Add(this, &CIDENewFile::PopupMenuOpening);
	mpFileMenu->GetItemClickEvent()->Add(this, &CIDENewFile::ItemClicked);
	mpPopupMenu->GetItemClickEvent()->Add(this, &CIDENewFile::ItemClicked);
	FillMenu(mpFileMenu, false);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpFileMenu);
	M_Pointer(mpPopupMenu);
	U_Unknown(CArrayChars, maszTemplates);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running so it takes
// itself off the menus.
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::Free(void)
{
	if (mpFileMenu.IsNotNull())
	{
		mpFileMenu->GetOpeningEvent()->Remove(this);
		mpFileMenu->GetItemClickEvent()->Remove(this);
	}
	if (mpPopupMenu.IsNotNull())
	{
		mpPopupMenu->GetOpeningEvent()->Remove(this);
		mpPopupMenu->GetItemClickEvent()->Remove(this);
	}
	maszTemplates.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::Save(CObjectWriter* pcFile) { return false; }
bool CIDENewFile::Load(CObjectReader* pcFile) { return false; }
Ptr<CMenuContainer> CIDENewFile::GetFileMenu(void) const { return mpFileMenu; }


//////////////////////////////////////////////////////////////////////////
//
// Alt+Insert's menu, as it is when it opens.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CMenuContainer> CIDENewFile::GetPopupMenu(void)
{
	FillMenu(mpPopupMenu, true);
	return mpPopupMenu;
}


//////////////////////////////////////////////////////////////////////////
//
// Alt+Insert, as IntelliJ's New: the menu of Files templates, at what is
// selected; and Save File as Template.
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("File.New", "New...", "Alt+Insert", Ptr<CImageCel>(), this, &CIDENewFile::OpenPopupCommand)->SetEnabledCheck(this, &CIDENewFile::CanCreate);
	pCommands->Add("File.SaveAsTemplate", "Save File as Template...", "", Ptr<CImageCel>(), this, &CIDENewFile::SaveAsTemplateCommand)->SetEnabledCheck(this, &CIDENewFile::CanSaveAsTemplate);
}


//////////////////////////////////////////////////////////////////////////
//
// A solution is open to make the files in.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::CanCreate(void)
{
	return mpcContent->GetWorkspace()->NumProjects() > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The menu: each Files template that isn't another's child; Alt+Insert's
// (bRefactorings) then Copy Class, for a class at the caret (question 27).
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::FillMenu(Ptr<CMenuContainer> pMenu, bool bRefactorings)
{
	CIDEFileTemplates*	pcTemplates;
	SIDEFileTemplate*	psTemplate;
	CArrayChars			aszSeen;
	CChars				szCaption;
	size				i;

	pcTemplates = mpcContent->GetFileTemplates();
	pMenu->RemoveAllItems();
	maszTemplates.ReInit();
	aszSeen.Init();
	for (i = 0; i < pcTemplates->NumTemplates(); i++)
	{
		psTemplate = pcTemplates->GetTemplate(i);
		if ((psTemplate->eTab != ITT_Files) || aszSeen.Contains(psTemplate->szName.Text()))
		{
			continue;
		}
		aszSeen.Add(psTemplate->szName.Text());
		psTemplate = pcTemplates->Get(ITT_Files, psTemplate->szName.Text());
		if (!psTemplate->szParent.Empty())
		{
			continue;
		}
		maszTemplates.Add(psTemplate->szName.Text());
		szCaption.Init(psTemplate->szName);
		szCaption.Append("...");
		pMenu->AddItem(szCaption.Text());
		szCaption.Kill();
	}
	aszSeen.Kill();
	if (bRefactorings)
	{
		pMenu->AddSeparator();
		pMenu->AddItem(mpcContent->GetCommands()->Find("Refactor.CopyClass"));
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::FileMenuOpening(CMenuContainer* pcMenu) { FillMenu(mpFileMenu, false); }
void CIDENewFile::PopupMenuOpening(CMenuContainer* pcMenu) { FillMenu(mpPopupMenu, true); }


//////////////////////////////////////////////////////////////////////////
//
// Alt+Insert: the menu opened under Solution Explorer's selected row, or
// at the editor's caret.
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::OpenPopupCommand(void)
{
	Ptr<CComponent>		pOwner;
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pOwner = mpcContent->GetExplorer()->GetTree();
	if (!pOwner->IsFocussed() && pTab.IsNotNull())
	{
		pOwner = pTab->GetEditor();
	}
	FillMenu(mpPopupMenu, true);
	mpPopupMenu->Open(pOwner->GetContextMenuPosition(), pOwner, true);
}


//////////////////////////////////////////////////////////////////////////
//
// A template chosen in either menu: its dialog.
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::ItemClicked(CMenuItem* pcItem)
{
	Ptr<CMenuItem>	pItem;
	CChars			szTemplate;
	size			i;
	bool			bFound;

	for (i = 0; i < maszTemplates.NumElements(); i++)
	{
		pItem = (mpFileMenu->NumItems() > i) ? mpFileMenu->GetItem(i) : Ptr<CMenuItem>();
		bFound = pItem.IsNotNull() && (pcItem == &pItem);
		pItem = (mpPopupMenu->NumItems() > i) ? mpPopupMenu->GetItem(i) : Ptr<CMenuItem>();
		bFound = bFound || (pItem.IsNotNull() && (pcItem == &pItem));
		if (bFound)
		{
			szTemplate.Init(maszTemplates.GetText(i));
			mpPopupMenu->Close();
			ShowNewDialog(szTemplate.Text());
			szTemplate.Kill();
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A template and its children: the Files templates whose parent it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::FindTemplateFiles(const char* szTemplate, CArrayChars* paszDest)
{
	CIDEFileTemplates*	pcTemplates;
	SIDEFileTemplate*	psTemplate;
	size				i;

	pcTemplates = mpcContent->GetFileTemplates();
	paszDest->Add(szTemplate);
	for (i = 0; i < pcTemplates->NumTemplates(); i++)
	{
		psTemplate = pcTemplates->GetTemplate(i);
		if ((psTemplate->eTab == ITT_Files) && !paszDest->Contains(psTemplate->szName.Text()))
		{
			psTemplate = pcTemplates->Get(ITT_Files, psTemplate->szName.Text());
			if (psTemplate->szParent.Equals(szTemplate))
			{
				paszDest->Add(psTemplate->szName.Text());
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The name written in the dialog: ${NAME}, and ${CLASS_NAME} and
// ${CLASS_STEM} (a class's files are named by its stem).
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::SetNameValues(const char* szName, CTemplateValues* pcDest)
{
	CChars	szStem;

	szStem.Init();
	AppendTemplateWithoutPrefix(szName, &szStem);
	pcDest->Set("NAME", szName);
	pcDest->Set("CLASS_NAME", szName);
	pcDest->Set("CLASS_STEM", szStem.Text());
	szStem.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A Files template's file's name: its File name (with the name's values)
// and its extension.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::EvaluateFileName(const char* szTemplate, CTemplateValues* pcValues, CChars* pszDest)
{
	SIDEFileTemplate*	psTemplate;
	CTemplateText		cText;
	bool				bResult;

	psTemplate = mpcContent->GetFileTemplates()->Get(ITT_Files, szTemplate);
	if (!psTemplate)
	{
		return false;
	}
	cText.Init();
	bResult = cText.Parse(psTemplate->szFileName.Empty() ? "${NAME}" : psTemplate->szFileName.Text()) && cText.Evaluate(pcValues, NULL, pszDest);
	cText.Kill();
	if (bResult && !psTemplate->szExtension.Empty())
	{
		pszDest->Append('.')->Append(psTemplate->szExtension);
	}
	return bResult && !pszDest->Empty() && !strchr(pszDest->Text(), '$');
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file of the solution has the name already.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::IsNamedInSolution(const char* szFileName)
{
	CArrayChars	aszFound;
	bool		bFound;

	aszFound.Init();
	bFound = mpcContent->GetCodeModel()->FindFilesNamed(szFileName, &aszFound) > 0;
	aszFound.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the files go: Solution Explorer's selected project (its
// directory), filter (the directory of its files, or the project's) or
// file (its directory and filter); with none selected, the active file's.
// False, with why, if there is nowhere.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::FindPlace(SIDENewPlace* psDest, const char** pszWhyNot)
{
	CBuildWorkspace*		pcWorkspace;
	CSolutionExplorer*		pcExplorer;
	SSolutionExplorerNode*	psNode;
	CVcxSolutionProject*	pcSolutionProject;
	Ptr<CIDEEditorTab>		pTab;
	CArrayInt				aiItems;
	CChars					szPath;
	CChars					szFile;
	const char*				szInFile;
	const char*				szFilter;
	size					i;

	pcWorkspace = mpcContent->GetWorkspace();
	pcExplorer = mpcContent->GetExplorer();
	psNode = pcExplorer->GetNode(pcExplorer->GetTree()->GetSelectedNode());
	szInFile = NULL;
	szFilter = NULL;
	szFile.Init();
	if (psNode && ((psNode->eType == SENT_Project) || (psNode->eType == SENT_Filter) || (psNode->eType == SENT_File)) && (psNode->uiSolution < pcWorkspace->NumSolutions()))
	{
		pcSolutionProject = pcWorkspace->GetSolution(psNode->uiSolution)->mcSolution.GetProject(psNode->uiProject);
		psDest->pcProject = pcSolutionProject ? pcSolutionProject->GetProject() : NULL;
		if (psDest->pcProject && (psNode->eType == SENT_Filter) && (psNode->uiIndex < psDest->pcProject->NumFilters()))
		{
			szFilter = psDest->pcProject->GetFilter(psNode->uiIndex)->GetName();
			aiItems.Init();
			psDest->pcProject->GetItemsInFilter(szFilter, &aiItems);
			if (aiItems.NumElements() > 0)
			{
				psDest->pcProject->GetFullPath(psDest->pcProject->GetItem((size)aiItems.GetValue(0)), &szFile);
			}
			aiItems.Kill();
		}
		else if (psDest->pcProject && (psNode->eType == SENT_File))
		{
			pcExplorer->GetNodeFilePath(pcExplorer->GetTree()->GetSelectedNode(), &szFile);
		}
	}
	else
	{
		pTab = mpcContent->GetDocuments()->GetActiveTab();
		if (pTab.IsNotNull() && (pTab->GetProject() < pcWorkspace->NumProjects()))
		{
			psDest->pcProject = pcWorkspace->GetSolutionProject(pTab->GetProject())->GetProject();
			szFile.Set(pTab->GetFileName());
		}
	}
	if (!psDest->pcProject)
	{
		*pszWhyNot = "Select a project, a filter or a file in Solution Explorer for the files to go in.";
		szFile.Kill();
		return false;
	}
	for (i = 0; i < pcWorkspace->NumProjects(); i++)
	{
		if (pcWorkspace->GetSolutionProject(i)->GetProject() == psDest->pcProject)
		{
			psDest->uiProject = i;
		}
	}

	//The directory and filter: the file's, if there is one.
	if (!szFile.Empty())
	{
		szInFile = GetIDEFileNamePart(szFile.Text());
		psDest->szDirectory.AppendSubString(szFile.Text(), szInFile);
		psDest->szListBeside.Set(szFile);
		for (i = 0; !szFilter && (i < psDest->pcProject->NumItems()); i++)
		{
			szPath.Init();
			psDest->pcProject->GetFullPath(psDest->pcProject->GetItem(i), &szPath);
			if (StringInsensitiveCompare(szPath.Text(), szFile.Text()) == 0)
			{
				psDest->szFilter.Set(psDest->pcProject->GetItem(i)->GetFilter());
			}
			szPath.Kill();
		}
	}
	else
	{
		psDest->szDirectory.Set(psDest->pcProject->GetDirectory());
		if (!psDest->szDirectory.EndsWith(_FS_) && !psDest->szDirectory.EndsWith("/"))
		{
			psDest->szDirectory.Append(_FS_);
		}
	}
	if (szFilter)
	{
		psDest->szFilter.Set(szFilter);
	}
	szFile.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// New's dialog for a template: the name, and a field for each variable
// its files use that has no value (IntelliJ's); then the files made.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::ShowNewDialog(const char* szTemplate)
{
	SIDENewPlace			sPlace;
	CTemplateValues			cValues;
	CArrayChars				aszFiles;
	CArrayChars				aszMissing;
	Ptr<CTextField>			apFields[IDE_NEW_MAXIMUM_FIELDS];
	CTemplateText			cText;
	SIDEFileTemplate*		psTemplate;
	Ptr<CDialog>			pDialog;
	Ptr<CTextField>			pName;
	CChars					szTitle;
	CChars					szLabel;
	CChars					szStatus;
	const char*				szWhyNot;
	size					i;
	bool					bResult;

	sPlace.Init();
	szWhyNot = NULL;
	if (!FindPlace(&sPlace, &szWhyNot))
	{
		mpcContent->ShowStatus(szWhyNot);
		sPlace.Kill();
		return false;
	}

	//What the templates ask for that nothing gives.
	aszFiles.Init();
	FindTemplateFiles(szTemplate, &aszFiles);
	cValues.Init();
	szTitle.Init(sPlace.szDirectory);
	szTitle.Append("New");
	SetIDETemplateFileVariables(mpcContent, szTitle.Text(), sPlace.pcProject, "CNew", &cValues);
	SetNameValues("CNew", &cValues);
	cValues.Set("INCLUDES", "");
	cValues.Set("BODY", "");
	aszMissing.Init();
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		psTemplate = mpcContent->GetFileTemplates()->Get(ITT_Files, aszFiles.GetText(i));
		cText.Init();
		if (psTemplate && cText.Parse(psTemplate->szText.Text()))
		{
			cText.FindMissing(&cValues, mpcContent->GetFileTemplates(), &aszMissing);
		}
		cText.Kill();
	}

	szTitle.Set("New ");
	szTitle.Append(szTemplate);
	pDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szTitle.Text());
	szTitle.Kill();
	pName = pDialog->AddTextField("Name:", "");
	for (i = 0; (i < aszMissing.NumElements()) && (i < IDE_NEW_MAXIMUM_FIELDS); i++)
	{
		szLabel.Init(aszMissing.GetText(i));
		szLabel.Append(':');
		apFields[i] = pDialog->AddTextField(szLabel.Text(), "");
		szLabel.Kill();
	}
	pDialog->SetOKCaption("Create");
	mpcContent->GetScript()->WatchDialog(pDialog);
	bResult = pDialog->Show(480);
	mpcContent->GetScript()->StopWatchingDialog();

	cValues.Kill();
	cValues.Init();
	for (i = 0; (i < aszMissing.NumElements()) && (i < IDE_NEW_MAXIMUM_FIELDS); i++)
	{
		cValues.Set(aszMissing.GetText(i), apFields[i]->GetText());
	}
	szStatus.Init();
	bResult = bResult && Create(szTemplate, pName->GetText(), &cValues, &szStatus);
	if (!szStatus.Empty())
	{
		mpcContent->ShowStatus(szStatus.Text());
	}
	szStatus.Kill();
	for (i = 0; i < IDE_NEW_MAXIMUM_FIELDS; i++)
	{
		apFields[i] = NULL;
	}
	pName = NULL;
	pDialog = NULL;
	cValues.Kill();
	aszMissing.Kill();
	aszFiles.Kill();
	sPlace.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A template's files, named szName (a class's name for C++ Class), made
// where FindPlace says, with pcGiven's values (the dialog's) over the
// predefined ones; opened, the first shown.  False, with why in
// pszStatus, if the name is taken or a template has an error; nothing is
// made then.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::Create(const char* szTemplate, const char* szName, CTemplateValues* pcGiven, CChars* pszStatus)
{
	SIDENewPlace		sPlace;
	CIDEFileEdits		cEdits;
	CTemplateValues		cValues;
	CArrayChars			aszFiles;
	CArrayChars			aszPaths;
	CArrayChars			aszTexts;
	CFileUtil			cFileUtil;
	CChars				szFileName;
	CChars				szPath;
	CChars				szText;
	Ptr<CIDEMoveClass>	pCreator;
	const char*			szWhyNot;
	size				i;
	size				j;
	bool				bResult;

	szWhyNot = NULL;
	if (!szName || (szName[0] == '\0') || strchr(szName, '/') || strchr(szName, '\\'))
	{
		pszStatus->Set("New: a name is needed (without a directory).");
		return false;
	}
	sPlace.Init();
	if (!FindPlace(&sPlace, &szWhyNot))
	{
		pszStatus->Set(szWhyNot);
		sPlace.Kill();
		return false;
	}

	//Each file's name and text.
	aszFiles.Init();
	aszPaths.Init();
	aszTexts.Init();
	FindTemplateFiles(szTemplate, &aszFiles);
	bResult = true;
	for (i = 0; bResult && (i < aszFiles.NumElements()); i++)
	{
		cValues.Init();
		SetNameValues(szName, &cValues);
		szFileName.Init();
		bResult = EvaluateFileName(aszFiles.GetText(i), &cValues, &szFileName);
		szPath.Init(sPlace.szDirectory);
		szPath.Append(szFileName);
		if (!bResult)
		{
			pszStatus->Set("New: the ");
			pszStatus->Append(aszFiles.GetText(i))->Append(" template's file name can't be made from the name.");
		}
		else if (cFileUtil.Exists(szPath.Text()) || IsNamedInSolution(szFileName.Text()) || aszPaths.Contains(szPath.Text()))
		{
			pszStatus->Set("New: a file of the solution is called ");
			pszStatus->Append(szFileName)->Append(" already.");
			bResult = false;
		}
		else
		{
			cValues.Kill();
			cValues.Init();
			SetIDETemplateFileVariables(mpcContent, szPath.Text(), sPlace.pcProject, szName, &cValues);
			SetNameValues(szName, &cValues);
			cValues.Set("NAME", szFileName.Text());
			for (j = 0; j < pcGiven->NumValues(); j++)
			{
				cValues.Set(pcGiven->GetName(j), pcGiven->Get(pcGiven->GetName(j)));
			}
			cValues.Set("INCLUDES", "");
			cValues.Set("BODY", "");
			szText.Init();
			bResult = mpcContent->GetFileTemplates()->Apply(ITT_Files, aszFiles.GetText(i), &cValues, &szText);
			if (bResult)
			{
				aszPaths.Add(szPath.Text());
				aszTexts.Add(szText.Text());
			}
			else
			{
				pszStatus->Set("New: ");
				pszStatus->Append(mpcContent->GetFileTemplates()->GetError())->Append('.');
			}
			szText.Kill();
		}
		szPath.Kill();
		szFileName.Kill();
		cValues.Kill();
	}

	//Made, filled as edits (one undo with their making), saved, staged and opened.
	if (bResult)
	{
		pCreator = mpcContent->GetMoveClass();
		pCreator->BeginCreating();
		for (i = 0; bResult && (i < aszPaths.NumElements()); i++)
		{
			bResult = pCreator->CreateNewFile(aszPaths.GetText(i), sPlace.pcProject, sPlace.szFilter.Text(), sPlace.szListBeside.Text());
		}
		cEdits.Init(mpcContent);
		for (i = 0; bResult && (i < aszPaths.NumElements()); i++)
		{
			bResult = (cEdits.GetText(aszPaths.GetText(i)) != NULL);
			if (bResult)
			{
				cEdits.GetEdits(aszPaths.GetText(i))->Insert(0, aszTexts.GetText(i));
			}
		}
		if (bResult)
		{
			cEdits.Apply();
			mpcContent->GetDocuments()->SaveAllFiles();
		}
		pCreator->EndCreating(bResult);
		cEdits.Kill();
		mpcContent->GetCodeModel()->Reset();
		mpcContent->GetSolutions()->WorkspaceChanged(true);
		if (bResult)
		{
			for (i = aszPaths.NumElements(); i > 0; i--)
			{
				mpcContent->GetDocuments()->OpenFileTab(aszPaths.GetText(i - 1), sPlace.uiProject, true);
			}
			pszStatus->Set("Made ");
			for (i = 0; i < aszPaths.NumElements(); i++)
			{
				pszStatus->Append((i == 0) ? "" : ((i + 1 == aszPaths.NumElements()) ? " and " : ", "))->Append(GetIDEFileNamePart(aszPaths.GetText(i)));
			}
			pszStatus->Append(" from the ")->Append(szTemplate)->Append(" template.");
		}
		else
		{
			pszStatus->Set("New: the files couldn't be made.");
		}
	}
	aszTexts.Kill();
	aszPaths.Kill();
	aszFiles.Kill();
	sPlace.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Save File as Template is for a file open in the editor.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::CanSaveAsTemplate(void)
{
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull();
}


//////////////////////////////////////////////////////////////////////////
//
// File, Save File as Template...: its name asked (the file's, to begin
// with), then the template made.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::SaveAsTemplateCommand(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CDialog>		pDialog;
	Ptr<CTextField>		pName;
	CChars				szName;
	CChars				szStatus;
	const char*			szFile;
	const char*			szDot;
	bool				bResult;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		mpcContent->ShowStatus("Open a file to save it as a template.");
		return false;
	}
	szFile = GetIDEFileNamePart(pTab->GetFileName());
	szDot = strrchr(szFile, '.');
	szName.Init();
	if (szDot)
	{
		szName.AppendSubString(szFile, szDot);
	}
	else
	{
		szName.Append(szFile);
	}
	pDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Save File as Template");
	pName = pDialog->AddTextField("Template name:", szName.Text());
	pDialog->SetOKCaption("Save");
	mpcContent->GetScript()->WatchDialog(pDialog);
	bResult = pDialog->Show(420);
	mpcContent->GetScript()->StopWatchingDialog();
	szName.Set(pName->GetText());
	pName = NULL;
	pDialog = NULL;
	szStatus.Init();
	bResult = bResult && SaveAsTemplate(pTab->GetFileName(), pTab->GetEditor()->GetText(), szName.Text(), &szStatus);
	if (!szStatus.Empty())
	{
		mpcContent->ShowStatus(szStatus.Text());
	}
	szStatus.Kill();
	szName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A file's text (szFileName's) as a new Files template called szName,
// the Workspace scheme's (or, with no workspace, the Default scheme's),
// written: what Velocity would read in it escaped, and its include
// guard, its header's name and its class's name (C and its stem) made
// their variables, for the user to finish in Settings.  A class's
// template names its files by ${CLASS_STEM}, any other by ${NAME}.
// False, with why in pszStatus, if the name is a Files template's
// already or there is no scheme to keep it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDENewFile::SaveAsTemplate(const char* szFileName, const char* szText, const char* szName, CChars* pszStatus)
{
	CIDEFileTemplates*	pcTemplates;
	EIDETemplateScheme	eScheme;
	CChars				szTemplate;
	CChars				szStem;
	CChars				szExtension;
	const char*			szFile;
	const char*			szDot;
	bool				bClass;
	bool				bResult;

	pcTemplates = mpcContent->GetFileTemplates();
	if (!szName || (szName[0] == '\0'))
	{
		pszStatus->Set("Save File as Template: a name is needed.");
		return false;
	}
	if (pcTemplates->Get(ITT_Files, szName))
	{
		pszStatus->Set("Save File as Template: a Files template is called ");
		pszStatus->Append(szName)->Append(" already.");
		return false;
	}
	eScheme = (pcTemplates->GetDirectory(ITS_Workspace)[0] != '\0') ? ITS_Workspace : ITS_Default;
	if (pcTemplates->GetDirectory(eScheme)[0] == '\0')
	{
		pszStatus->Set("Save File as Template: there is no workspace, and the IDE keeps no settings, to keep it in.");
		return false;
	}

	szFile = GetIDEFileNamePart(szFileName);
	szDot = strrchr(szFile, '.');
	szStem.Init();
	szExtension.Init();
	if (szDot)
	{
		szStem.AppendSubString(szFile, szDot);
		szExtension.Append(szDot + 1);
	}
	else
	{
		szStem.Append(szFile);
	}
	szTemplate.Init();
	MakeTemplateText(szText, szStem.Text(), &szTemplate, &bClass);
	pcTemplates->Set(eScheme, ITT_Files, szName, szExtension.Text(), bClass ? "${CLASS_STEM}" : "${NAME}", "", szTemplate.Text());
	bResult = pcTemplates->Write(eScheme);
	if (bResult)
	{
		pszStatus->Set("Saved ");
		pszStatus->Append(szFile)->Append(" as the ")->Append((eScheme == ITS_Workspace) ? "Workspace" : "Default")->Append(" scheme's template ")->Append(szName);
		pszStatus->Append(": finish it in Settings, File and Code Templates.");
	}
	else
	{
		pszStatus->Set("Save File as Template: ");
		pszStatus->Append(pcTemplates->GetError());
	}
	szTemplate.Kill();
	szExtension.Kill();
	szStem.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the source at sz starts a word that Velocity would take as a
// directive or a comment after a '#'.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDETemplateDirectiveAt(const char* sz)
{
	static const char*	aszWords[] = { "set", "if", "elseif", "else", "end", "foreach", "parse", "include", "break" };
	size				i;
	size				uiLength;

	if ((sz[0] == '#') || (sz[0] == '*') || (sz[0] == '{'))
	{
		return true;
	}
	for (i = 0; i < sizeof(aszWords) / sizeof(char*); i++)
	{
		uiLength = strlen(aszWords[i]);
		if ((strncmp(sz, aszWords[i], uiLength) == 0) && !IsIDENameChar(sz[uiLength]))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Each whole word szWord in pszText made szReplacement.
//
//////////////////////////////////////////////////////////////////////////
static bool ReplaceIDETemplateWord(CChars* pszText, const char* szWord, const char* szReplacement)
{
	CChars		szResult;
	const char*	sz;
	size		uiLength;
	size		i;
	bool		bFound;

	uiLength = strlen(szWord);
	if (uiLength == 0)
	{
		return false;
	}
	sz = pszText->Text();
	szResult.Init();
	bFound = false;
	for (i = 0; sz[i] != '\0'; i++)
	{
		if ((strncmp(&sz[i], szWord, uiLength) == 0) && ((i == 0) || !IsIDENameChar(sz[i - 1])) && !IsIDENameChar(sz[i + uiLength]))
		{
			szResult.Append(szReplacement);
			i += uiLength - 1;
			bFound = true;
		}
		else
		{
			szResult.Append(sz[i]);
		}
	}
	pszText->Set(szResult);
	szResult.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// A file's text as a template's (SaveAsTemplate): a $ or # Velocity would
// read written \$ or \#, then the file's include guard ${INCLUDE_GUARD},
// its header's name (szStem.h) ${HEADER_FILENAME} and its class (C and
// szStem) ${CLASS_NAME}; *pbClass if it has that class.
//
//////////////////////////////////////////////////////////////////////////
void CIDENewFile::MakeTemplateText(const char* szText, const char* szStem, CChars* pszDest, bool* pbClass)
{
	CChars		szGuard;
	CChars		szWord;
	const char*	szIfndef;
	size		i;
	char		c;
	char		cNext;

	for (i = 0; szText[i] != '\0'; i++)
	{
		c = szText[i];
		cNext = szText[i + 1];
		if ((c == '$') && ((cNext == '{') || (cNext == '!') || IsIDENameChar(cNext) || ((i > 0) && (szText[i - 1] == '\\'))))
		{
			pszDest->Append("\\$");
		}
		else if ((c == '#') && (IsIDETemplateDirectiveAt(&szText[i + 1]) || ((i > 0) && (szText[i - 1] == '\\'))))
		{
			pszDest->Append("\\#");
		}
		else if (c != '\r')
		{
			pszDest->Append(c);
		}
	}

	//The guard: the name after the first #ifndef.
	szGuard.Init();
	szIfndef = strstr(pszDest->Text(), "#ifndef ");
	if (szIfndef)
	{
		for (i = 8; IsIDENameChar(szIfndef[i]); i++)
		{
			szGuard.Append(szIfndef[i]);
		}
	}
	if (!szGuard.Empty())
	{
		ReplaceIDETemplateWord(pszDest, szGuard.Text(), "${INCLUDE_GUARD}");
	}
	szGuard.Kill();

	szWord.Init(szStem);
	szWord.Append(".h");
	pszDest->Replace(szWord.Text(), "${HEADER_FILENAME}");
	szWord.Set("C");
	szWord.Append(szStem);
	*pbClass = ReplaceIDETemplateWord(pszDest, szWord.Text(), "${CLASS_NAME}");
	szWord.Kill();
}
