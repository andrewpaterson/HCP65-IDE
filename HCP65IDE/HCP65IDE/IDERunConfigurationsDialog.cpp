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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/FileUtil.h"
#include "StandardLib/ClassDefines.h"
#include "StandardLib/Objects.h"
#include "WindowLib/FillContainer.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/RoundedBoxCanvasDraw.h"
#include "IDERunConfigurationsDialog.h"


#define IDE_RUN_DIALOG_WIDTH		760
#define IDE_RUN_DIALOG_HEIGHT		400
#define IDE_RUN_DIALOG_LIST_WIDTH	240
#define IDE_RUN_DIALOG_GAP			12
#define IDE_RUN_DIALOG_NEW_NAME		"New Configuration"


//////////////////////////////////////////////////////////////////////////
//
// Copies of pcWorkspace's run configurations, szSelected selected.  The
// workspace isn't held.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Init(Ptr<CWindow> pOwner, SDialogLook* psLook, CBuildWorkspace* pcWorkspace, const char* szSelected)
{
	Ptr<CComponent>		pBody;
	CRunConfiguration	cPrivate;
	size				i;

	PreInit();

	masEdited.Init();
	maszBuildConfigurations.Init();
	miSelected = -1;
	mbUpdating = false;
	maszCards.Init();
	maszSolutions.Init();
	mszPrivateDirectory.Init();
	cPrivate.Init();
	pcWorkspace->GetRunConfigurationDirectory(&cPrivate, &mszPrivateDirectory);
	cPrivate.Kill();
	pcWorkspace->GetConfigurationNames(&maszBuildConfigurations);
	for (i = 0; i < pcWorkspace->NumSolutions(); i++)
	{
		maszSolutions.Add(pcWorkspace->GetSolution(i)->mcSolution.GetDirectory());
	}
	for (i = 0; i < pcWorkspace->NumRunConfigurations(); i++)
	{
		masEdited.Add()->Init(pcWorkspace->GetRunConfiguration(i));
		if ((miSelected == -1) || (StringCompare(pcWorkspace->GetRunConfiguration(i)->GetName(), szSelected) == 0))
		{
			miSelected = (int32)i;
		}
	}

	mpDialog = OMalloc<CDialog>(pOwner, psLook, "Run Configurations");
	mpDialog->SetResizable(IDE_RUN_DIALOG_WIDTH / 2, IDE_RUN_DIALOG_HEIGHT / 2);
	//The body is made first: it makes the component that has the keys when the dialog opens.
	pBody = CreateBody();
	for (i = 0; i < pcWorkspace->NumSolutions(); i++)
	{
		mpStored->AddItem(pcWorkspace->GetSolution(i)->mcSolution.GetName());
	}
	FindCards(pcWorkspace);
	mpDialog->SetBody(pBody, mpName);
	mpDialog->SetApprover(this, &CIDERunConfigurationsDialog::Approve);
	mpDialog->AddExtraButton(IDE_RUN_DIALOG_OPEN_CARD);

	mpList->GetSelectionEvent()->Add(this, &CIDERunConfigurationsDialog::SelectionChanged);
	mpName->GetChangeEvent()->Add(this, &CIDERunConfigurationsDialog::NameChanged);
	mpCard->GetSelectionEvent()->Add(this, &CIDERunConfigurationsDialog::CardChanged);
	mpAdd->GetClickEvent()->Add(this, &CIDERunConfigurationsDialog::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDERunConfigurationsDialog::RemoveClicked);

	ShowList();
	ShowSelected();

	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Class(void)
{
	M_Pointer(mpDialog);
	M_Pointer(mpList);
	M_Pointer(mpScrollPane);
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpName);
	M_Pointer(mpBuildConfiguration);
	M_Pointer(mpTool);
	M_Pointer(mpDirectory);
	M_Pointer(mpArguments);
	M_Pointer(mpLoadFile);
	M_Pointer(mpLoadAddress);
	M_Pointer(mpCard);
	M_Pointer(mpCardPrograms);
	M_Pointer(mpStored);
	M_Pointer(mpError);
	U_Unknown(CArrayRunConfigurations, masEdited);
	U_Unknown(CArrayChars, maszBuildConfigurations);
	U_Unknown(CArrayChars, maszCards);
	U_Unknown(CArrayChars, maszSolutions);
	U_Unknown(CChars, mszPrivateDirectory);
	U_Int32(miSelected);
	U_Bool(mbUpdating);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running so the
// dialog takes itself off the widgets still here.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Free(void)
{
	if (mpDialog.IsNotNull())
	{
		mpDialog->ClearApprover();
	}
	if (mpList.IsNotNull())
	{
		mpList->GetSelectionEvent()->Remove(this);
	}
	if (mpName.IsNotNull())
	{
		mpName->GetChangeEvent()->Remove(this);
	}
	if (mpCard.IsNotNull())
	{
		mpCard->GetSelectionEvent()->Remove(this);
	}
	if (mpAdd.IsNotNull())
	{
		mpAdd->GetClickEvent()->Remove(this);
	}
	if (mpRemove.IsNotNull())
	{
		mpRemove->GetClickEvent()->Remove(this);
	}
	KillEdited();
	masEdited.Kill();
	maszBuildConfigurations.Kill();
	maszCards.Kill();
	maszSolutions.Kill();
	mszPrivateDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunConfigurationsDialog::Save(CObjectWriter* pcFile) { return false; }
bool CIDERunConfigurationsDialog::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::KillEdited(void)
{
	size	i;

	for (i = 0; i < masEdited.NumElements(); i++)
	{
		masEdited.Get(i)->Kill();
	}
	masEdited.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The list and its buttons on the left, the form on the right.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDERunConfigurationsDialog::CreateBody(void)
{
	Ptr<CWindow>			pWindow;
	SDialogLook*			psLook;
	Ptr<CGridContainer>		pBody;
	Ptr<CGridContainer>		pLeft;
	Ptr<CGridContainer>		pButtons;
	Ptr<CGridContainer>		pForm;
	Ptr<CGridContainer>		pLoad;
	Ptr<CLabel>				pAt;
	Ptr<CFillContainer>		pGap;
	SInsets					sInsets;
	int32					iRow;
	size					i;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();

	sInsets.Init(0, 0);
	mpList = OMalloc<CListContainer>(pWindow, psLook->pcTheme);
	mpList->SetHighlightDraws(OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListSelected), 4, &sInsets), OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListHover), 4, &sInsets));
	mpList->SetAlignment(CSH_Stretch, CSV_Top);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, psLook->pcTheme);
	mpScrollPane->SetContent(mpList);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpScrollPane->SetDesiredSize(IDE_RUN_DIALOG_LIST_WIDTH, -1);

	mpAdd = OMalloc<CButton>(pWindow, psLook->pcTheme, "Add");
	mpAdd->SetAlignment(CSH_Left, CSV_Center);
	mpRemove = OMalloc<CButton>(pWindow, psLook->pcTheme, "Remove");
	mpRemove->SetAlignment(CSH_Left, CSV_Center);
	pButtons = OMalloc<CGridContainer>(pWindow);
	pButtons->SetAlignment(CSH_Stretch, CSV_Top);
	pButtons->AddComponent(mpAdd, 0, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_RUN_DIALOG_GAP / 2, 1);
	pButtons->AddComponent(pGap, 1, 0);
	pButtons->AddComponent(mpRemove, 2, 0);
	pButtons->AddComponent(OMalloc<CFillContainer>(pWindow), 3, 0, 1, 1, 1, 0);  //Keeps the buttons on the left.

	pLeft = OMalloc<CGridContainer>(pWindow);
	pLeft->SetAlignment(CSH_Stretch, CSV_Stretch);
	pLeft->AddComponent(mpScrollPane, 0, 0, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_RUN_DIALOG_GAP / 2);
	pLeft->AddComponent(pGap, 0, 1);
	pLeft->AddComponent(pButtons, 0, 2, 1, 1, 1, 0);

	mpName = CreateField();
	mpBuildConfiguration = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpBuildConfiguration->SetAlignment(CSH_Stretch, CSV_Center);
	for (i = 0; i < maszBuildConfigurations.NumElements(); i++)
	{
		mpBuildConfiguration->AddItem(maszBuildConfigurations.GetText(i));
	}
	mpTool = CreateField();
	mpDirectory = CreateField();
	mpArguments = CreateField();

	//The file loaded first and its address, on one row.
	mpLoadFile = CreateField();
	mpLoadAddress = OMalloc<CTextField>(pWindow, psLook->pcTheme, "");
	mpLoadAddress->SetColumns(8);
	mpLoadAddress->SetAlignment(CSH_Left, CSV_Center);
	pAt = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), " At ");
	pAt->SetAlignment(CSH_Left, CSV_Center);
	pLoad = OMalloc<CGridContainer>(pWindow);
	pLoad->SetAlignment(CSH_Stretch, CSV_Center);
	pLoad->AddComponent(mpLoadFile, 0, 0, 1, 1, 1, 0);
	pLoad->AddComponent(pAt, 1, 0);
	pLoad->AddComponent(mpLoadAddress, 2, 0);

	//The card by name, and under it what its file says.
	mpCard = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpCard->SetAlignment(CSH_Stretch, CSV_Center);
	mpCard->AddItem(IDE_RUN_DIALOG_NO_CARD);
	mpCardPrograms = OMalloc<CGridContainer>(pWindow);
	mpCardPrograms->SetAlignment(CSH_Stretch, CSV_Top);
	mpStored = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpStored->SetAlignment(CSH_Stretch, CSV_Center);
	mpStored->AddItem(IDE_RUN_DIALOG_PRIVATE);

	mpError = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), "");
	mpError->SetAlignment(CSH_Left, CSV_Center);

	pForm = OMalloc<CGridContainer>(pWindow);
	pForm->SetAlignment(CSH_Stretch, CSV_Top);
	iRow = 0;
	AddFormRow(pForm, &iRow, "Name", mpName);
	AddFormRow(pForm, &iRow, "Build configuration", mpBuildConfiguration);
	AddFormRow(pForm, &iRow, "Tool", mpTool);
	AddFormRow(pForm, &iRow, "Working directory", mpDirectory);
	AddFormRow(pForm, &iRow, "Arguments", mpArguments);
	AddFormRow(pForm, &iRow, "Load first", pLoad);
	AddFormRow(pForm, &iRow, "Card", mpCard);
	AddFormRow(pForm, &iRow, "", mpCardPrograms);
	AddFormRow(pForm, &iRow, "Stored in", mpStored);
	AddFormRow(pForm, &iRow, NULL, mpError);

	//What is left over goes under the rows, which keeps them at the top.
	pGap = OMalloc<CFillContainer>(pWindow);
	pForm->AddComponent(pGap, 0, iRow, 2, 1, 1, 1);

	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->AddComponent(pLeft, 0, 0, 1, 1, 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_RUN_DIALOG_GAP, 1);
	pBody->AddComponent(pGap, 1, 0);
	pBody->AddComponent(pForm, 2, 0, 1, 1, 1, 1);
	return pBody;
}


//////////////////////////////////////////////////////////////////////////
//
// A label and its editor, as CDialog's own rows are.  Without a label the
// editor is across both columns.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddFormRow(Ptr<CGridContainer> pForm, int32* piRow, const char* szLabel, Ptr<CComponent> pEditor)
{
	Ptr<CWindow>		pWindow;
	SDialogLook*		psLook;
	Ptr<CLabel>			pLabel;
	Ptr<CFillContainer>	pGap;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();
	if (*piRow > 0)
	{
		pGap = OMalloc<CFillContainer>(pWindow);
		pGap->SetDesiredSize(1, psLook->pcTheme->GetMetric(TM_DialogRowGap));
		pForm->AddComponent(pGap, 0, *piRow);
		(*piRow)++;
	}
	if (szLabel)
	{
		pLabel = OMalloc<CLabel>(pWindow, psLook->pcTheme->GetFont(), szLabel);
		pLabel->SetDesiredSize(pLabel->GetRequiredSize().x + psLook->pcTheme->GetMetric(TM_DialogLabelGap), -1);
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		pForm->AddComponent(pLabel, 0, *piRow);
		pForm->AddComponent(pEditor, 1, *piRow, 1, 1, 1, 0);
	}
	else
	{
		pForm->AddComponent(pEditor, 0, *piRow, 2, 1, 1, 0);
	}
	(*piRow)++;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTextField> CIDERunConfigurationsDialog::CreateField(void)
{
	Ptr<CTextField>	pField;

	pField = OMalloc<CTextField>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, "");
	pField->SetColumns(10);
	pField->SetAlignment(CSH_Stretch, CSV_Center);
	return pField;
}


//////////////////////////////////////////////////////////////////////////
//
// A row for each run configuration, the selected one selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::ShowList(void)
{
	Ptr<CLabel>	pLabel;
	size		i;

	mbUpdating = true;
	mpList->RemoveAllComponents();
	for (i = 0; i < masEdited.NumElements(); i++)
	{
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), masEdited.Get(i)->GetName());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpList->AddItem(pLabel);
	}
	mpList->Select(miSelected);
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The selected run configuration in the form, which is empty if there are
// none.  A build configuration that no solution has any more is added, so
// that it isn't lost by looking at it.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::ShowSelected(void)
{
	CRunConfiguration*	pcRun;
	CChars				szText;
	CChars				szDirectory;
	char				szAddress[16];
	size				i;

	mbUpdating = true;
	pcRun = (miSelected >= 0) ? masEdited.Get(miSelected) : NULL;
	mpName->SetText(pcRun ? pcRun->GetName() : "");
	mpTool->SetText(pcRun ? pcRun->GetTool() : "");
	mpDirectory->SetText(pcRun ? pcRun->GetDirectory() : "");
	mpArguments->SetText(pcRun ? pcRun->GetArguments() : "");
	if (pcRun)
	{
		SelectBuildConfiguration(pcRun->GetConfiguration());
	}
	szText.Init();
	if (pcRun && (pcRun->NumLoads() > 0))
	{
		snprintf(szAddress, sizeof(szAddress), "0x%X", (unsigned int)pcRun->GetLoadAddress(0));
		szText.Set(szAddress);
	}
	mpLoadFile->SetText((pcRun && (pcRun->NumLoads() > 0)) ? pcRun->GetLoadFile(0) : "");
	mpLoadAddress->SetText(szText.Text());

	//Its card by its file, which is added if it isn't one of those found, so that it isn't lost by looking at it.
	mpCard->Select(0);
	if (pcRun && (pcRun->GetCard()[0] != '\0'))
	{
		szDirectory.Init();
		GetDirectory(pcRun, &szDirectory);
		FullPathFrom(szDirectory.Text(), pcRun->GetCard(), &szText);
		szDirectory.Kill();
		AddCard(szText.Text());
		mpCard->Select((int32)maszCards.GetIndex(szText.Text()) + 1);
	}
	szText.Kill();
	mpStored->Select(0);
	for (i = 0; pcRun && pcRun->IsShared() && (i < maszSolutions.NumElements()); i++)
	{
		if (StringCompare(maszSolutions.GetText(i), pcRun->GetShared()) == 0)
		{
			mpStored->Select((int32)i + 1);
		}
	}
	ShowCard();
	mpError->SetText((pcRun && (pcRun->NumLoads() > 1)) ? IDE_RUN_DIALOG_SEVERAL_LOADS : "");
	mbUpdating = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The card files a run configuration here might name: those in each
// solution's folder and the three above it, and in a Cards folder of any
// of those (HCP65/Cards from HCP65/Drivers/VIABoard), in name order.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::FindCards(CBuildWorkspace* pcWorkspace)
{
	CFileUtil	cFileUtil;
	CChars		szDirectory;
	size		i;
	int			iUp;

	szDirectory.Init();
	for (i = 0; i < pcWorkspace->NumSolutions(); i++)
	{
		szDirectory.Set(pcWorkspace->GetSolution(i)->mcSolution.GetDirectory());
		for (iUp = 0; (iUp <= 3) && !szDirectory.Empty(); iUp++)
		{
			AddCardsIn(szDirectory.Text());
			cFileUtil.RemoveLastFromPath(&szDirectory);
		}
	}
	szDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The card files in szDirectory and its Cards folder.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddCardsIn(const char* szDirectory)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszFiles;
	CChars		szCards;
	size		i;

	aszFiles.Init();
	cFileUtil.FindFilesWithExtension(szDirectory, &BUILD_CARD_EXTENSION[1], &aszFiles);
	szCards.Init(szDirectory);
	cFileUtil.AppendToPath(&szCards, "Cards");
	if (cFileUtil.IsDirectory(szCards.Text()))
	{
		cFileUtil.FindFilesWithExtension(szCards.Text(), &BUILD_CARD_EXTENSION[1], &aszFiles);
	}
	szCards.Kill();
	aszFiles.QuickSort();
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		AddCard(aszFiles.GetText(i));
	}
	aszFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A card file (a full path) in the drop-down, by its name without its
// extension, once.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddCard(const char* szFile)
{
	CFileUtil	cFileUtil;
	CChars		szName;
	CChars		szDirectory;

	if (maszCards.Contains(szFile))
	{
		return;
	}
	maszCards.Add(szFile);
	szName.Init();
	szDirectory.Init();
	cFileUtil.SplitPath(szFile, &szName, &szDirectory);
	cFileUtil.RemoveExtension(&szName);
	mpCard->AddItem(szName.Text());
	szDirectory.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Under the card: what its file says, its image and then each program's
// path on the card and the file that goes there; that it can't be read;
// or nothing for no card.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::ShowCard(void)
{
	CBuildCard		cCard;
	Ptr<CLabel>		pLabel;
	CChars			szLine;
	size			i;
	int32			iRow;

	mpCardPrograms->RemoveAllComponents();
	if (mpCard->GetSelected() <= 0)
	{
		return;
	}
	cCard.Init();
	szLine.Init();
	iRow = 0;
	if (!cCard.Read(maszCards.GetText((size)mpCard->GetSelected() - 1)))
	{
		szLine.Set("The card file can't be read.");
	}
	else
	{
		szLine.Set("Image: ");
		szLine.Append(cCard.GetImageText());
	}
	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), szLine.Text());
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	mpCardPrograms->AddComponent(pLabel, 0, iRow++, 2, 1, 1, 0);
	for (i = 0; i < cCard.NumPrograms(); i++)
	{
		szLine.Set(cCard.GetProgramPath(i));
		szLine.Append("   ");
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), szLine.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpCardPrograms->AddComponent(pLabel, 0, iRow);
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), cCard.GetProgramFileText(i));
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpCardPrograms->AddComponent(pLabel, 1, iRow++, 1, 1, 1, 0);
	}
	szLine.Kill();
	cCard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Another card chosen.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::CardChanged(void)
{
	if (!mbUpdating)
	{
		ShowCard();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The folder a run configuration's paths are relative to: its solution's
// if it is shared, else the workspace file's.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::GetDirectory(CRunConfiguration* pcRun, CChars* pszDest)
{
	pszDest->Set(pcRun->IsShared() ? pcRun->GetShared() : mszPrivateDirectory.Text());
}


//////////////////////////////////////////////////////////////////////////
//
// szFull as it is written in a run configuration kept in szDirectory:
// relative to it, with Windows' separators as the committed ones have.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::MakeRelative(const char* szDirectory, const char* szFull, CChars* pszDest)
{
	MakeRelativePath(szDirectory, szFull, pszDest);
	pszDest->Replace('/', '\\');
}


//////////////////////////////////////////////////////////////////////////
//
// The run configuration is kept beside the solution in szShared, or in
// the workspace (NULL).  Its paths, which are relative to where it is
// kept, are written again for there so they name the same files.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Store(CRunConfiguration* pcRun, const char* szShared)
{
	CRunConfiguration	cOld;
	CChars				szOld;
	CChars				szNew;
	CChars				szFull;
	CChars				szPath;
	size				i;

	if (StringCompare(pcRun->GetShared(), szShared ? szShared : "") == 0)
	{
		return;
	}
	szOld.Init();
	szNew.Init();
	szFull.Init();
	szPath.Init();
	GetDirectory(pcRun, &szOld);
	cOld.Init(pcRun);
	pcRun->SetShared(szShared);
	GetDirectory(pcRun, &szNew);
	if (cOld.GetCard()[0] != '\0')
	{
		FullPathFrom(szOld.Text(), cOld.GetCard(), &szFull);
		MakeRelative(szNew.Text(), szFull.Text(), &szPath);
		pcRun->SetCard(szPath.Text());
	}
	pcRun->ClearLoads();
	for (i = 0; i < cOld.NumLoads(); i++)
	{
		FullPathFrom(szOld.Text(), cOld.GetLoadFile(i), &szFull);
		MakeRelative(szNew.Text(), szFull.Text(), &szPath);
		pcRun->AddLoad(szPath.Text(), cOld.GetLoadAddress(i));
	}
	cOld.Kill();
	szPath.Kill();
	szFull.Kill();
	szNew.Kill();
	szOld.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::SelectBuildConfiguration(const char* szConfiguration)
{
	size	i;

	for (i = 0; i < mpBuildConfiguration->NumItems(); i++)
	{
		if (StringCompare(mpBuildConfiguration->GetItemText(i), szConfiguration) == 0)
		{
			mpBuildConfiguration->Select((int32)i);
			return;
		}
	}
	if (szConfiguration[0] != '\0')
	{
		mpBuildConfiguration->Select((int32)mpBuildConfiguration->AddItem(szConfiguration));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What the form shows is kept in the selected run configuration.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::Keep(void)
{
	CRunConfiguration*	pcRun;
	CChars				szCard;
	CChars				szDirectory;

	if (miSelected < 0)
	{
		return;
	}
	pcRun = masEdited.Get(miSelected);
	pcRun->SetName(mpName->GetText());
	if (mpBuildConfiguration->GetSelectedText() != NULL)
	{
		pcRun->SetConfiguration(mpBuildConfiguration->GetSelectedText());
	}
	pcRun->SetTool(mpTool->GetText(), mpDirectory->GetText(), mpArguments->GetText());
	if (pcRun->NumLoads() <= 1)
	{
		//More than one are its file's to edit: they are kept as they are.
		pcRun->ClearLoads();
		if (mpLoadFile->GetText()[0] != '\0')
		{
			pcRun->AddLoad(mpLoadFile->GetText(), (uint32)strtoul(mpLoadAddress->GetText(), NULL, 0));
		}
	}
	Store(pcRun, (mpStored->GetSelected() > 0) ? maszSolutions.GetText((size)mpStored->GetSelected() - 1) : NULL);
	szCard.Init();
	if (mpCard->GetSelected() > 0)
	{
		szDirectory.Init();
		GetDirectory(pcRun, &szDirectory);
		MakeRelative(szDirectory.Text(), maszCards.GetText((size)mpCard->GetSelected() - 1), &szCard);
		szDirectory.Kill();
	}
	pcRun->SetCard(szCard.Text());
	szCard.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Another row selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::SelectionChanged(void)
{
	if (mbUpdating || (mpList->GetSelected() < 0))
	{
		return;
	}
	Keep();
	miSelected = mpList->GetSelected();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The name typed shows in the list as it is typed.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::NameChanged(void)
{
	if (mbUpdating || (miSelected < 0))
	{
		return;
	}
	Keep();
	ShowList();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddClicked(CButton* pcButton)
{
	AddConfiguration();
	mpDialog->GetWindow()->GetFocus()->SetFocus(mpName);
}


void CIDERunConfigurationsDialog::RemoveClicked(CButton* pcButton) { RemoveConfiguration(); }


//////////////////////////////////////////////////////////////////////////
//
// A new one, named so it can be told apart, for the build configuration of
// the one selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::AddConfiguration(void)
{
	CChars	szName;
	CChars	szConfiguration;
	int		iNumber;
	size	i;
	bool	bUsed;

	Keep();
	szName.Init(IDE_RUN_DIALOG_NEW_NAME);
	iNumber = 1;
	do
	{
		bUsed = false;
		for (i = 0; i < masEdited.NumElements(); i++)
		{
			bUsed |= StringCompare(masEdited.Get(i)->GetName(), szName.Text()) == 0;
		}
		if (bUsed)
		{
			iNumber++;
			szName.Set(IDE_RUN_DIALOG_NEW_NAME " ");
			szName.Append(iNumber);
		}
	} while (bUsed);

	szConfiguration.Init();
	if (miSelected >= 0)
	{
		szConfiguration.Set(masEdited.Get(miSelected)->GetConfiguration());
	}
	else if (maszBuildConfigurations.NumElements() > 0)
	{
		szConfiguration.Set(maszBuildConfigurations.GetText(0));
	}
	masEdited.Add()->Init(szName.Text(), szConfiguration.Text());
	miSelected = (int32)masEdited.NumElements() - 1;
	szConfiguration.Kill();
	szName.Kill();

	ShowList();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// The selected one goes, and the one after it (or now last) is selected.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::RemoveConfiguration(void)
{
	if (miSelected < 0)
	{
		return;
	}
	masEdited.Get(miSelected)->Kill();
	masEdited.RemoveAt(miSelected);
	if (miSelected >= (int32)masEdited.NumElements())
	{
		miSelected = (int32)masEdited.NumElements() - 1;
	}
	ShowList();
	ShowSelected();
}


//////////////////////////////////////////////////////////////////////////
//
// OK closes the dialog only if every run configuration has a name of its
// own; otherwise the first that hasn't is selected and the form says why.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunConfigurationsDialog::Approve(void)
{
	size		i;
	size		j;
	const char*	szProblem;
	char*		szEnd;

	//The address typed is looked at before it is kept as a number.
	if ((miSelected >= 0) && (masEdited.Get(miSelected)->NumLoads() <= 1) && (mpLoadFile->GetText()[0] != '\0'))
	{
		strtoul(mpLoadAddress->GetText(), &szEnd, 0);
		if ((szEnd == mpLoadAddress->GetText()) || (*szEnd != '\0'))
		{
			mpError->SetText("The address the file is loaded at isn't a number.");
			return false;
		}
	}
	Keep();
	mpError->SetText("");
	szProblem = NULL;
	for (i = 0; (i < masEdited.NumElements()) && (szProblem == NULL); i++)
	{
		if (masEdited.Get(i)->GetName()[0] == '\0')
		{
			szProblem = "A run configuration needs a name.";
		}
		for (j = 0; (j < i) && (szProblem == NULL); j++)
		{
			if (StringCompare(masEdited.Get(i)->GetName(), masEdited.Get(j)->GetName()) == 0)
			{
				szProblem = "Another run configuration has this name.";
			}
		}
		if (szProblem != NULL)
		{
			miSelected = (int32)i;
			ShowList();
			ShowSelected();
			mpError->SetText(szProblem);
		}
	}
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, what a user would do in the dialog, separated by
// semicolons: add, remove, select:<row>, name:<text>, build:<text>,
// tool:<text>, directory:<text>, arguments:<text>, load:<file>,
// at:<address>, card:<name>, stored:<row>, print, which prints the
// selected one as the form shows it, and check, which prints whether OK
// would close it and why not.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::ApplyEdits(const char* szEdits)
{
	CArrayChars	aszEdits;
	size		i;
	size		j;
	char*		sz;
	bool		bApproved;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		if (strcmp(sz, "add") == 0)
		{
			AddConfiguration();
		}
		else if (strcmp(sz, "remove") == 0)
		{
			RemoveConfiguration();
		}
		else if (strncmp(sz, "select:", 7) == 0)
		{
			mpList->Select(atoi(&sz[7]));
		}
		else if (strncmp(sz, "name:", 5) == 0)
		{
			mpName->SetText(&sz[5]);
		}
		else if (strncmp(sz, "build:", 6) == 0)
		{
			SelectBuildConfiguration(&sz[6]);
		}
		else if (strncmp(sz, "tool:", 5) == 0)
		{
			mpTool->SetText(&sz[5]);
		}
		else if (strncmp(sz, "directory:", 10) == 0)
		{
			mpDirectory->SetText(&sz[10]);
		}
		else if (strncmp(sz, "arguments:", 10) == 0)
		{
			mpArguments->SetText(&sz[10]);
		}
		else if (strncmp(sz, "load:", 5) == 0)
		{
			mpLoadFile->SetText(&sz[5]);
		}
		else if (strncmp(sz, "at:", 3) == 0)
		{
			mpLoadAddress->SetText(&sz[3]);
		}
		else if (strncmp(sz, "card:", 5) == 0)
		{
			for (j = 0; j < mpCard->NumItems(); j++)
			{
				if (StringCompare(mpCard->GetItemText(j), &sz[5]) == 0)
				{
					mpCard->Select((int32)j);
				}
			}
			ShowCard();
		}
		else if (strncmp(sz, "stored:", 7) == 0)
		{
			mpStored->Select(atoi(&sz[7]));
		}
		else if (strcmp(sz, "print") == 0)
		{
			printf("Run form: [%s] load [%s] at [%s] card [%s] stored in [%s] cards:", mpName->GetText(), mpLoadFile->GetText(), mpLoadAddress->GetText(), mpCard->GetSelectedText(), mpStored->GetSelectedText());
			for (j = 1; j < mpCard->NumItems(); j++)
			{
				printf(" %s", mpCard->GetItemText(j));
			}
			printf("; on it: %d lines\n", (int)mpCardPrograms->NumComponents());
			fflush(stdout);
		}
		else if (strcmp(sz, "check") == 0)
		{
			bApproved = Approve();
			printf("Run dialog: %s [%s]\n", bApproved ? "OK" : "refused", mpError->GetText());
			fflush(stdout);
		}
	}
	aszEdits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// True if OK closed it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERunConfigurationsDialog::Show(void)
{
	bool	bResult;

	bResult = mpDialog->Show(IDE_RUN_DIALOG_WIDTH, IDE_RUN_DIALOG_HEIGHT);
	if (bResult)
	{
		Keep();
	}
	return bResult;
}


Ptr<CDialog> CIDERunConfigurationsDialog::GetDialog(void) const { return mpDialog; }
CArrayRunConfigurations* CIDERunConfigurationsDialog::GetEdited(void) { return &masEdited; }


//////////////////////////////////////////////////////////////////////////
//
// The selected one's card file, a full path; empty if it has none.
//
//////////////////////////////////////////////////////////////////////////
void CIDERunConfigurationsDialog::GetSelectedCard(CChars* pszDest)
{
	pszDest->Set(((miSelected >= 0) && (mpCard->GetSelected() > 0)) ? maszCards.GetText((size)mpCard->GetSelected() - 1) : "");
}


char* CIDERunConfigurationsDialog::GetSelectedName(void) { return (miSelected >= 0) ? masEdited.Get(miSelected)->GetName() : (char*)""; }

