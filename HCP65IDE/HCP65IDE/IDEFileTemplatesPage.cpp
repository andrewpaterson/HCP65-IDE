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
#include <string.h>
#include "WindowLib/FillContainer.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/RoundedBoxCanvasDraw.h"
#include "IDEContent.h"
#include "IDEFileTemplatesPage.h"


#define IDE_TEMPLATES_PAGE_GAP		6
#define IDE_TEMPLATES_PAGE_LIST		220


//The variables every Files and Includes template has (SetIDETemplateFileVariables).
#define IDE_TEMPLATES_FILE_VARIABLES	"NAME, FILE_NAME, HEADER_FILENAME, INCLUDE_GUARD, DIRECTORY, CLASS_NAME, CLASS_STEM, PROJECT_NAME, LICENCE, USER, AUTHOR, DATE, TIME, YEAR, MONTH, DAY, HOUR, MINUTE, MONTH_NAME_SHORT, MONTH_NAME_FULL, DAY_NAME_SHORT, DAY_NAME_FULL, PRODUCT_NAME, NAMESPACES_OPEN, NAMESPACES_CLOSE"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::Init(CIDEContent* pcContent, Ptr<CDialog> pDialog)
{
	PreInit();
	CSettingsPage::Init();
	mpcContent = pcContent;
	mpDialog = pDialog;
	mcTemplates.Init();
	mcTemplates.Copy(pcContent->GetFileTemplates());
	meScheme = mcTemplates.GetDirectory(ITS_Workspace)[0] != '\0' ? ITS_Workspace : ITS_Default;
	meTab = ITT_Files;
	maszRows.Init();
	mbUpdating = false;
	mszKeepProblem.Init();
	mpBody = CreateBody();

	mpScheme->GetSelectionEvent()->Add(this, &CIDEFileTemplatesPage::SchemeChanged);
	mpTabs->GetSelectionEvent()->Add(this, &CIDEFileTemplatesPage::TabChanged);
	mpList->GetSelectionEvent()->Add(this, &CIDEFileTemplatesPage::RowSelected);
	mpEditor->GetChangeEvent()->Add(this, &CIDEFileTemplatesPage::TextChanged);
	mpName->GetChangeEvent()->Add(this, &CIDEFileTemplatesPage::NameChanged);
	mpExtension->GetChangeEvent()->Add(this, &CIDEFileTemplatesPage::ExtensionChanged);
	mpFileName->GetChangeEvent()->Add(this, &CIDEFileTemplatesPage::FileNameChanged);
	mpAdd->GetClickEvent()->Add(this, &CIDEFileTemplatesPage::AddClicked);
	mpRemove->GetClickEvent()->Add(this, &CIDEFileTemplatesPage::RemoveClicked);
	mpCopy->GetClickEvent()->Add(this, &CIDEFileTemplatesPage::CopyClicked);
	mpReset->GetClickEvent()->Add(this, &CIDEFileTemplatesPage::ResetClicked);

	ShowList(NULL);
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::Class(void)
{
	CSettingsPage::Class();
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	U_Unknown(CIDEFileTemplates, mcTemplates);
	U_Enum(meScheme);
	U_Enum(meTab);
	U_Unknown(CArrayChars, maszRows);
	U_Bool(mbUpdating);
	U_String(mszKeepProblem);
	M_Pointer(mpBody);
	M_Pointer(mpScheme);
	M_Pointer(mpAuthor);
	M_Pointer(mpTabs);
	M_Pointer(mpList);
	M_Pointer(mpScrollPane);
	M_Pointer(mpAdd);
	M_Pointer(mpRemove);
	M_Pointer(mpCopy);
	M_Pointer(mpReset);
	M_Pointer(mpName);
	M_Pointer(mpExtension);
	M_Pointer(mpFileName);
	M_Pointer(mpEditor);
	M_Pointer(mpError);
	M_Pointer(mpVariables);
	M_Pointer(mpStatus);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running so the page
// takes itself off the widgets still here.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::Free(void)
{
	if (mpScheme.IsNotNull())
	{
		mpScheme->GetSelectionEvent()->Remove(this);
		mpTabs->GetSelectionEvent()->Remove(this);
		mpList->GetSelectionEvent()->Remove(this);
		mpEditor->GetChangeEvent()->Remove(this);
		mpName->GetChangeEvent()->Remove(this);
		mpExtension->GetChangeEvent()->Remove(this);
		mpFileName->GetChangeEvent()->Remove(this);
		mpAdd->GetClickEvent()->Remove(this);
		mpRemove->GetClickEvent()->Remove(this);
		mpCopy->GetClickEvent()->Remove(this);
		mpReset->GetClickEvent()->Remove(this);
	}
	maszRows.Kill();
	mszKeepProblem.Kill();
	mcTemplates.Kill();
	CSettingsPage::Free();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplatesPage::Save(CObjectWriter* pcFile) { return false; }
bool CIDEFileTemplatesPage::Load(CObjectReader* pcFile) { return false; }
const char* CIDEFileTemplatesPage::GetTitle(void) const { return "File and Code Templates"; }
char* CIDEFileTemplatesPage::GetKeepProblem(void) { return mszKeepProblem.Text(); }
Ptr<CComponent> CIDEFileTemplatesPage::GetBody(void) const { return mpBody; }
Ptr<CComponent> CIDEFileTemplatesPage::GetFirstEditor(void) const { return mpList; }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
Ptr<CLabel> CIDEFileTemplatesPage::CreateLabel(const char* szText)
{
	Ptr<CLabel>	pLabel;

	pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme->GetFont(), szText);
	pLabel->SetAlignment(CSH_Left, CSV_Center);
	return pLabel;
}


//////////////////////////////////////////////////////////////////////////
//
// A button, with a gap in the column after it.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CButton> CIDEFileTemplatesPage::CreateButton(Ptr<CGridContainer> pButtons, int32 iColumn, const char* szCaption)
{
	Ptr<CButton>		pButton;
	Ptr<CFillContainer>	pGap;

	pButton = OMalloc<CButton>(mpDialog->GetWindow(), mpDialog->GetLook()->pcTheme, szCaption);
	pButton->SetAlignment(CSH_Left, CSV_Center);
	pButtons->AddComponent(pButton, iColumn, 0);
	pGap = OMalloc<CFillContainer>(mpDialog->GetWindow());
	pGap->SetDesiredSize(IDE_TEMPLATES_PAGE_GAP, 1);
	pButtons->AddComponent(pGap, iColumn + 1, 0);
	return pButton;
}


//////////////////////////////////////////////////////////////////////////
//
// The scheme and the author along the top, the tabs, the list and its
// buttons on the left, the template chosen on the right.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CComponent> CIDEFileTemplatesPage::CreateBody(void)
{
	Ptr<CWindow>			pWindow;
	SDialogLook*			psLook;
	Ptr<CGridContainer>		pBody;
	Ptr<CGridContainer>		pTop;
	Ptr<CGridContainer>		pLeft;
	Ptr<CGridContainer>		pRight;
	Ptr<CGridContainer>		pButtons;
	Ptr<CFillContainer>		pGap;
	SInsets					sInsets;
	EIDETemplateTab			aeTabs[] = { ITT_Files, ITT_Includes, ITT_Code };
	size					i;

	pWindow = mpDialog->GetWindow();
	psLook = mpDialog->GetLook();

	//Scheme and author.
	mpScheme = OMalloc<CComboBox>(pWindow, psLook->pcTheme);
	mpScheme->AddItem("Default");
	mpScheme->AddItem("Workspace");
	mpScheme->Select((int32)(meScheme == ITS_Workspace ? 1 : 0));
	mpScheme->SetAlignment(CSH_Left, CSV_Center);
	mpAuthor = OMalloc<CTextField>(pWindow, psLook->pcTheme, mcTemplates.GetAuthor());
	mpAuthor->SetAlignment(CSH_Stretch, CSV_Center);
	pTop = OMalloc<CGridContainer>(pWindow);
	pTop->SetAlignment(CSH_Stretch, CSV_Top);
	pTop->AddComponent(CreateLabel("Scheme:"), 0, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_TEMPLATES_PAGE_GAP, 1);
	pTop->AddComponent(pGap, 1, 0);
	pTop->AddComponent(mpScheme, 2, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_TEMPLATES_PAGE_GAP * 4, 1);
	pTop->AddComponent(pGap, 3, 0);
	pTop->AddComponent(CreateLabel("Author (empty for git's user.name):"), 4, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_TEMPLATES_PAGE_GAP, 1);
	pTop->AddComponent(pGap, 5, 0);
	pTop->AddComponent(mpAuthor, 6, 0, 1, 1, 1, 0);

	//The tabs choose the list's templates: their pages are empty.
	mpTabs = OMalloc<CTabbedContainer>(pWindow, psLook->pcTheme);
	mpTabs->SetAlignment(CSH_Stretch, CSV_Top);
	for (i = 0; i < 3; i++)
	{
		pGap = OMalloc<CFillContainer>(pWindow);
		pGap->SetDesiredSize(1, 1);
		mpTabs->AddTab(CIDEFileTemplates::GetTabName(aeTabs[i]), pGap);
	}

	//The list and its buttons.
	sInsets.Init(0, 0);
	mpList = OMalloc<CListContainer>(pWindow, psLook->pcTheme);
	mpList->SetHighlightDraws(OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListSelected), 4, &sInsets), OMalloc<CRoundedBoxCanvasDraw>(psLook->pcTheme->GetColour(TC_ListHover), 4, &sInsets));
	mpList->SetAlignment(CSH_Stretch, CSV_Top);
	mpScrollPane = OMalloc<CScrollPane>(pWindow, psLook->pcTheme);
	mpScrollPane->SetContent(mpList);
	mpScrollPane->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpScrollPane->SetDesiredSize(IDE_TEMPLATES_PAGE_LIST, 1);
	pButtons = OMalloc<CGridContainer>(pWindow);
	pButtons->SetAlignment(CSH_Stretch, CSV_Top);
	mpAdd = CreateButton(pButtons, 0, "Add");
	mpRemove = CreateButton(pButtons, 2, "Remove");
	mpCopy = CreateButton(pButtons, 4, "Copy");
	mpReset = CreateButton(pButtons, 6, "Reset");
	pButtons->AddComponent(OMalloc<CFillContainer>(pWindow), 7, 0, 1, 1, 1, 0);
	pLeft = OMalloc<CGridContainer>(pWindow);
	pLeft->SetAlignment(CSH_Stretch, CSV_Stretch);
	pLeft->AddComponent(mpScrollPane, 0, 0, 1, 1, 1, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_TEMPLATES_PAGE_GAP);
	pLeft->AddComponent(pGap, 0, 1);
	pLeft->AddComponent(pButtons, 0, 2, 1, 1, 1, 0);

	//The template chosen.
	mpName = OMalloc<CTextField>(pWindow, psLook->pcTheme, "");
	mpName->SetAlignment(CSH_Stretch, CSV_Center);
	mpExtension = OMalloc<CTextField>(pWindow, psLook->pcTheme, "");
	mpExtension->SetAlignment(CSH_Stretch, CSV_Center);
	mpFileName = OMalloc<CTextField>(pWindow, psLook->pcTheme, "");
	mpFileName->SetAlignment(CSH_Stretch, CSV_Center);
	mpEditor = OMalloc<CCodeEditor>(pWindow, psLook->pcTheme, mpcContent->GetCodeFamily(), "");
	mpEditor->SetAlignment(CSH_Stretch, CSV_Stretch);
	mpError = CreateLabel("");
	mpVariables = OMalloc<CLabel>(pWindow, mpcContent->GetDimFont(), "");
	mpVariables->SetAlignment(CSH_Left, CSV_Top);
	pRight = OMalloc<CGridContainer>(pWindow);
	pRight->SetAlignment(CSH_Stretch, CSV_Stretch);
	pRight->AddComponent(CreateLabel("Name:"), 0, 0);
	pRight->AddComponent(mpName, 1, 0, 1, 1, 1, 0);
	pRight->AddComponent(CreateLabel("Extension:"), 0, 1);
	pRight->AddComponent(mpExtension, 1, 1, 1, 1, 1, 0);
	pRight->AddComponent(CreateLabel("File name:"), 0, 2);
	pRight->AddComponent(mpFileName, 1, 2, 1, 1, 1, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_TEMPLATES_PAGE_GAP);
	pRight->AddComponent(pGap, 0, 3);
	pRight->AddComponent(mpEditor, 0, 4, 2, 1, 1, 1);
	pRight->AddComponent(mpError, 0, 5, 2, 1, 1, 0);
	pRight->AddComponent(mpVariables, 0, 6, 2, 1, 1, 0);

	mpStatus = CreateLabel("");
	pBody = OMalloc<CGridContainer>(pWindow);
	pBody->SetAlignment(CSH_Stretch, CSV_Stretch);
	pBody->AddComponent(pTop, 0, 0, 3, 1, 1, 0);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(1, IDE_TEMPLATES_PAGE_GAP);
	pBody->AddComponent(pGap, 0, 1);
	pBody->AddComponent(mpTabs, 0, 2, 3, 1, 1, 0);
	pBody->AddComponent(pLeft, 0, 3, 1, 1, 0, 1);
	pGap = OMalloc<CFillContainer>(pWindow);
	pGap->SetDesiredSize(IDE_TEMPLATES_PAGE_GAP * 2, 1);
	pBody->AddComponent(pGap, 1, 3);
	pBody->AddComponent(pRight, 2, 3, 1, 1, 1, 1);
	pBody->AddComponent(mpStatus, 0, 4, 3, 1, 1, 0);
	return pBody;
}


//////////////////////////////////////////////////////////////////////////
//
// The tab's templates as the scheme has them, each once, a child after
// its parent; the one called szSelect selected (or the first), and shown
// on the right unless bTemplate is false (it is being typed in).
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::ShowList(const char* szSelect, bool bTemplate)
{
	SIDEFileTemplate*	psEntry;
	SIDEFileTemplate*	psTemplate;
	SIDEFileTemplate*	psChild;
	CArrayChars			aszNames;
	CChars				szCaption;
	Ptr<CLabel>			pLabel;
	Ptr<CFont>			pFont;
	size				i;
	size				j;
	int32				iSelect;

	mbUpdating = true;
	aszNames.Init();
	for (i = 0; i < mcTemplates.NumTemplates(); i++)
	{
		psEntry = mcTemplates.GetTemplate(i);
		if ((psEntry->eTab == meTab) && (psEntry->eScheme <= meScheme) && !aszNames.Contains(psEntry->szName.Text()))
		{
			aszNames.Add(psEntry->szName.Text());
		}
	}

	maszRows.ReInit();
	mpList->RemoveAllComponents();
	for (i = 0; i < aszNames.NumElements(); i++)
	{
		psTemplate = mcTemplates.Get(meTab, aszNames.GetText(i), meScheme);
		if (!psTemplate->szParent.Empty() && aszNames.Contains(psTemplate->szParent.Text()))
		{
			continue;
		}
		maszRows.Add(psTemplate->szName.Text());
		for (j = 0; j < aszNames.NumElements(); j++)
		{
			psChild = mcTemplates.Get(meTab, aszNames.GetText(j), meScheme);
			if (psChild->szParent.Equals(psTemplate->szName))
			{
				maszRows.Add(psChild->szName.Text());
			}
		}
	}

	iSelect = maszRows.NumElements() > 0 ? 0 : -1;
	for (i = 0; i < maszRows.NumElements(); i++)
	{
		psTemplate = mcTemplates.Get(meTab, maszRows.GetText(i), meScheme);
		szCaption.Init(psTemplate->szParent.Empty() ? "" : "    ");
		szCaption.Append(psTemplate->szName);
		if ((psTemplate->eTab == ITT_Files) && !psTemplate->szExtension.Empty())
		{
			szCaption.Append(" (.")->Append(psTemplate->szExtension)->Append(')');
		}
		pFont = IsChanged(psTemplate) ? mpcContent->GetFontColours()->GetFont(mpDialog->GetLook()->pcTheme->GetColour(TC_Accent)) : mpDialog->GetLook()->pcTheme->GetFont();
		pLabel = OMalloc<CLabel>(mpDialog->GetWindow(), pFont, szCaption.Text());
		pLabel->SetAlignment(CSH_Left, CSV_Center);
		mpList->AddItem(pLabel);
		szCaption.Kill();
		if (szSelect && psTemplate->szName.Equals(szSelect))
		{
			iSelect = (int32)i;
		}
	}
	aszNames.Kill();
	mpList->Select(iSelect);
	mbUpdating = false;
	if (bTemplate)
	{
		ShowTemplate();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The selected template's fields, text, error and variables.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::ShowTemplate(void)
{
	SIDEFileTemplate*	psTemplate;
	CChars				szVariables;

	mbUpdating = true;
	psTemplate = GetSelected();
	mpName->SetText(psTemplate ? psTemplate->szName.Text() : "");
	mpExtension->SetText(psTemplate ? psTemplate->szExtension.Text() : "");
	mpFileName->SetText(psTemplate ? psTemplate->szFileName.Text() : "");
	mpEditor->GetTextArea()->SetText(psTemplate ? psTemplate->szText.Text() : "");
	szVariables.Init();
	AppendVariables(psTemplate, &szVariables);
	mpVariables->SetText(szVariables.Text());
	szVariables.Kill();
	mbUpdating = false;
	ShowErrorAndColours();
}


//////////////////////////////////////////////////////////////////////////
//
// The editor's text coloured (references, directives, comments) and its
// error, if it has one, under it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::ShowErrorAndColours(void)
{
	static const char*	aszDirectives[] = { "#set", "#if", "#elseif", "#else", "#end", "#foreach", "#parse", "#include", "#break", "#{" };
	Ptr<CTextArea>		pArea;
	Ptr<CWindowTheme>	pTheme;
	CTemplateText		cText;
	const char*			sz;
	size				i;
	size				j;
	size				k;

	pArea = mpEditor->GetTextArea();
	pTheme = mpDialog->GetLook()->pcTheme;
	sz = pArea->GetText();
	pArea->ClearSpans();
	for (i = 0; sz[i] != '\0'; i++)
	{
		if ((sz[i] == '#') && ((sz[i + 1] == '#') || (sz[i + 1] == '*')))
		{
			//A comment, to the end of its line or its *#.
			for (j = i + 2; (sz[j] != '\0') && ((sz[i + 1] == '#') ? (sz[j] != '\n') : !((sz[j - 1] == '*') && (sz[j] == '#'))); j++)
			{
			}
			j += ((sz[i + 1] == '*') && (sz[j] != '\0')) ? 1 : 0;
			pArea->AddSpan(i, j - i, pTheme->GetColour(TC_DimText));
			i = j - 1;
		}
		else if (sz[i] == '#')
		{
			for (k = 0; k < sizeof(aszDirectives) / sizeof(char*); k++)
			{
				j = strlen(aszDirectives[k]);
				if (strncmp(&sz[i], aszDirectives[k], j) == 0)
				{
					for (j += i; ((sz[j] >= 'a') && (sz[j] <= 'z')) || (sz[j] == '}'); j++)
					{
					}
					pArea->AddSpan(i, j - i, pTheme->GetColour(TC_BrightText));
					i = j - 1;
					break;
				}
			}
		}
		else if ((sz[i] == '$') && ((sz[i + 1] == '{') || (sz[i + 1] == '!') || ((sz[i + 1] >= 'A') && (sz[i + 1] <= 'Z')) || ((sz[i + 1] >= 'a') && (sz[i + 1] <= 'z')) || (sz[i + 1] == '_')))
		{
			j = i + 1;
			if (sz[j] == '!')
			{
				j++;
			}
			if (sz[j] == '{')
			{
				for (; (sz[j] != '\0') && (sz[j] != '}') && (sz[j] != '\n'); j++)
				{
				}
				j += (sz[j] == '}') ? 1 : 0;
			}
			else
			{
				for (; ((sz[j] >= 'A') && (sz[j] <= 'Z')) || ((sz[j] >= 'a') && (sz[j] <= 'z')) || ((sz[j] >= '0') && (sz[j] <= '9')) || (sz[j] == '_'); j++)
				{
				}
			}
			pArea->AddSpan(i, j - i, pTheme->GetColour(TC_Accent));
			i = j - 1;
		}
	}

	cText.Init();
	mpError->SetText(cText.Parse(sz) ? "" : cText.GetError());
	cText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// What a template can use, under its text.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::AppendVariables(SIDEFileTemplate* psTemplate, CChars* pszDest)
{
	if (!psTemplate)
	{
		return;
	}
	pszDest->Append("Variables: ");
	if (psTemplate->eTab != ITT_Code)
	{
		pszDest->Append(IDE_TEMPLATES_FILE_VARIABLES);
		pszDest->Append((psTemplate->eTab == ITT_Files) ? ", and from a refactoring INCLUDES and BODY." : ".");
	}
	else if (psTemplate->szName.Equals(IDE_TEMPLATE_NEW_FUNCTION_BODY))
	{
		pszDest->Append("FUNCTION_NAME, BODY (each line ended).");
	}
	else if (psTemplate->szName.Equals(IDE_TEMPLATE_GETTER_BODY))
	{
		pszDest->Append("FIELD, IS_OBJECT (true for a getter of an object, which gives its address).");
	}
	else if (psTemplate->szName.Equals(IDE_TEMPLATE_SETTER_BODY))
	{
		pszDest->Append("FIELD, PARAMETER.");
	}
	else if (psTemplate->szName.Equals(IDE_TEMPLATE_INTERFACE_COMMENT))
	{
		pszDest->Append("INDENT, PARAMETER, MEMBERS (a list, for #foreach).");
	}
	else
	{
		pszDest->Append("none.");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The selected row's template as the scheme has it, or NULL.
//
//////////////////////////////////////////////////////////////////////////
SIDEFileTemplate* CIDEFileTemplatesPage::GetSelected(void)
{
	char*	szName;

	szName = GetSelectedName();
	return szName ? mcTemplates.Get(meTab, szName, meScheme) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CIDEFileTemplatesPage::GetSelectedName(void)
{
	int32	iRow;

	iRow = mpList->GetSelected();
	return ((iRow >= 0) && ((size)iRow < maszRows.NumElements())) ? maszRows.GetText((size)iRow) : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the scheme has a template of its own (changed or added).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplatesPage::IsChanged(SIDEFileTemplate* psTemplate)
{
	return (meScheme != ITS_BuiltIn) && (mcTemplates.Find(psTemplate->eTab, psTemplate->szName.Text(), meScheme) != NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a scheme under this one has the template (so Reset gives that
// back, and it can't be removed or renamed).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileTemplatesPage::HasUnder(SIDEFileTemplate* psTemplate)
{
	return (meScheme > ITS_BuiltIn) && (mcTemplates.Get(psTemplate->eTab, psTemplate->szName.Text(), (EIDETemplateScheme)(meScheme - 1)) != NULL);
}


//////////////////////////////////////////////////////////////////////////
//
// The scheme's own entry for the selected template, made from the one it
// has now if it hasn't one: what an edit changes.
//
//////////////////////////////////////////////////////////////////////////
SIDEFileTemplate* CIDEFileTemplatesPage::TakeSelected(void)
{
	SIDEFileTemplate*	psTemplate;
	CChars				szName;
	CChars				szExtension;
	CChars				szFileName;
	CChars				szParent;
	CChars				szText;

	psTemplate = GetSelected();
	if (!psTemplate || (psTemplate->eScheme == meScheme))
	{
		return psTemplate;
	}
	szName.Init(psTemplate->szName);
	szExtension.Init(psTemplate->szExtension);
	szFileName.Init(psTemplate->szFileName);
	szParent.Init(psTemplate->szParent);
	szText.Init(psTemplate->szText);
	psTemplate = mcTemplates.Set(meScheme, meTab, szName.Text(), szExtension.Text(), szFileName.Text(), szParent.Text(), szText.Text());
	szText.Kill();
	szParent.Kill();
	szFileName.Kill();
	szExtension.Kill();
	szName.Kill();
	return psTemplate;
}


//////////////////////////////////////////////////////////////////////////
//
// szName, or with a number after it, so no template of the tab has it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::MakeUniqueName(const char* szName, CChars* pszDest)
{
	int	i;

	pszDest->Set(szName);
	for (i = 2; mcTemplates.Get(meTab, pszDest->Text()); i++)
	{
		pszDest->Set(szName);
		pszDest->Append(' ')->Append(i);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::SelectName(const char* szName)
{
	ShowList(szName);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::SchemeChanged(void)
{
	CChars	szName;

	if (mbUpdating)
	{
		return;
	}
	szName.Init(GetSelectedName() ? GetSelectedName() : "");
	meScheme = (mpScheme->GetSelected() == 1) ? ITS_Workspace : ITS_Default;
	ShowList(szName.Text());
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::TabChanged(void)
{
	EIDETemplateTab	aeTabs[] = { ITT_Files, ITT_Includes, ITT_Code };
	size			uiTab;

	if (mbUpdating)
	{
		return;
	}
	uiTab = mpTabs->IndexOf(mpTabs->GetSelected());
	meTab = (uiTab < 3) ? aeTabs[uiTab] : ITT_Files;
	ShowList(NULL);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::RowSelected(void)
{
	if (!mbUpdating)
	{
		ShowTemplate();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The text typed is the scheme's template's (the row turns the accent
// colour the first time).
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::TextChanged(void)
{
	SIDEFileTemplate*	psTemplate;
	bool				bWasChanged;

	if (mbUpdating || !GetSelected())
	{
		return;
	}
	bWasChanged = IsChanged(GetSelected());
	psTemplate = TakeSelected();
	psTemplate->SetText(mpEditor->GetTextArea()->GetText());
	ShowErrorAndColours();
	if (!bWasChanged)
	{
		ShowList(psTemplate->szName.Text(), false);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A template the scheme added can be renamed; one under it can't.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::NameChanged(void)
{
	SIDEFileTemplate*	psTemplate;
	SIDEFileTemplate*	psChild;
	CChars				szOld;
	size				i;

	psTemplate = GetSelected();
	if (mbUpdating || !psTemplate)
	{
		return;
	}
	if (HasUnder(psTemplate) || (psTemplate->eScheme != meScheme))
	{
		mpStatus->SetText("Only a template this scheme added can be renamed.");
		return;
	}
	if ((mpName->GetText()[0] == '\0') || (mcTemplates.Get(meTab, mpName->GetText(), meScheme) && !psTemplate->szName.Equals(mpName->GetText())))
	{
		mpStatus->SetText("A template's name must be its tab's only one of that name.");
		return;
	}
	mpStatus->SetText("");
	szOld.Init(psTemplate->szName);
	psTemplate->szName.Set(mpName->GetText());
	for (i = 0; i < mcTemplates.NumTemplates(); i++)
	{
		psChild = mcTemplates.GetTemplate(i);
		if ((psChild->eTab == meTab) && psChild->szParent.Equals(szOld))
		{
			psChild->szParent.Set(mpName->GetText());
		}
	}
	maszRows.Get((size)mpList->GetSelected())->Set(mpName->GetText());
	szOld.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::ExtensionChanged(void)
{
	if (!mbUpdating && GetSelected())
	{
		TakeSelected()->szExtension.Set(mpExtension->GetText());
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::FileNameChanged(void)
{
	if (!mbUpdating && GetSelected())
	{
		TakeSelected()->szFileName.Set(mpFileName->GetText());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A new template of the tab, the scheme's, empty.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::AddClicked(CButton* pcButton)
{
	CChars	szName;

	szName.Init();
	MakeUniqueName("Unnamed", &szName);
	mcTemplates.Set(meScheme, meTab, szName.Text(), (meTab == ITT_Files) ? "h" : "", (meTab == ITT_Files) ? "${NAME}" : "", "", "");
	mpStatus->SetText("");
	SelectName(szName.Text());
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A template the scheme added is taken away; one under it is Reset's.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::RemoveClicked(CButton* pcButton)
{
	SIDEFileTemplate*	psTemplate;
	CChars				szName;

	psTemplate = GetSelected();
	if (!psTemplate)
	{
		return;
	}
	if (HasUnder(psTemplate) || (psTemplate->eScheme != meScheme))
	{
		mpStatus->SetText((psTemplate->eScheme == ITS_BuiltIn) || mcTemplates.IsBuiltIn(meTab, psTemplate->szName.Text()) ? "A built-in template can't be removed: Reset gives it back as it was." : "Only a template this scheme added can be removed: Reset gives the one under it back.");
		return;
	}
	szName.Init(psTemplate->szName);
	mcTemplates.Remove(meScheme, meTab, szName.Text());
	mpStatus->SetText("");
	ShowList(NULL);
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A copy of the selected template, the scheme's, under a name of its own.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::CopyClicked(CButton* pcButton)
{
	SIDEFileTemplate*	psTemplate;
	CChars				szName;
	CChars				szExtension;
	CChars				szFileName;
	CChars				szText;

	psTemplate = GetSelected();
	if (!psTemplate)
	{
		return;
	}
	szName.Init(psTemplate->szName);
	szName.Append(" Copy");
	szExtension.Init(psTemplate->szExtension);
	szFileName.Init(psTemplate->szFileName);
	szText.Init(psTemplate->szText);
	MakeUniqueName(szName.Text(), &szName);
	mcTemplates.Set(meScheme, meTab, szName.Text(), szExtension.Text(), szFileName.Text(), "", szText.Text());
	mpStatus->SetText("");
	SelectName(szName.Text());
	szText.Kill();
	szFileName.Kill();
	szExtension.Kill();
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The scheme's change taken away: the template is the one under it again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::ResetClicked(CButton* pcButton)
{
	SIDEFileTemplate*	psTemplate;
	CChars				szName;

	psTemplate = GetSelected();
	if (!psTemplate || !IsChanged(psTemplate) || !HasUnder(psTemplate))
	{
		mpStatus->SetText("Only a template the scheme changed can be reset.");
		return;
	}
	szName.Init(psTemplate->szName);
	mcTemplates.Remove(meScheme, meTab, szName.Text());
	mpStatus->SetText("");
	SelectName(szName.Text());
	szName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// OK: the copy is the IDE's templates, written to the schemes'
// directories, and the author kept with the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::Keep(void)
{
	CIDEFileTemplates*	pcTemplates;
	CChars				szMessage;
	EIDETemplateScheme	aeSchemes[] = { ITS_Default, ITS_Workspace };
	size				i;
	size				j;
	bool				bAny;

	pcTemplates = mpcContent->GetFileTemplates();
	mcTemplates.SetAuthor(mpAuthor->GetText());
	pcTemplates->Copy(&mcTemplates);
	szMessage.Init();
	for (i = 0; i < 2; i++)
	{
		bAny = false;
		for (j = 0; j < pcTemplates->NumTemplates(); j++)
		{
			bAny = bAny || (pcTemplates->GetTemplate(j)->eScheme == aeSchemes[i]);
		}
		if (pcTemplates->GetDirectory(aeSchemes[i])[0] == '\0')
		{
			if (bAny)
			{
				szMessage.Append(szMessage.Empty() ? "" : " ")->Append((aeSchemes[i] == ITS_Default) ? "The Default scheme's templates can't be kept: the IDE keeps no settings." : "The Workspace scheme's templates can't be kept: no workspace is open.");
			}
		}
		else if (!pcTemplates->Write(aeSchemes[i]))
		{
			szMessage.Append(szMessage.Empty() ? "" : " ")->Append(pcTemplates->GetError());
		}
	}
	mpcContent->GetSettings()->WriteSettings();
	mszKeepProblem.Set(szMessage);
	printf("Templates: kept%s%s\n", szMessage.Empty() ? "" : ": ", szMessage.Text());
	fflush(stdout);
	szMessage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// For checking, what a user would do on the page, separated by
// semicolons: scheme:Default or scheme:Workspace, tab:Files, Includes or
// Code, select:<name>, text:<text> (\n is a new line, \t a tab), name:,
// extension: and filename:<text>, author:<text>, add, remove, copy,
// reset, and show, which prints the list's rows (changed ones marked)
// and the selected one's error.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileTemplatesPage::ApplyEdits(const char* szEdits)
{
	CArrayChars			aszEdits;
	CChars				szText;
	SIDEFileTemplate*	psTemplate;
	EIDETemplateTab		aeTabs[] = { ITT_Files, ITT_Includes, ITT_Code };
	size				i;
	size				j;
	char*				sz;

	aszEdits.Init();
	aszEdits.Split(szEdits, ';');
	for (i = 0; i < aszEdits.NumElements(); i++)
	{
		sz = aszEdits.GetText(i);
		if (strncmp(sz, "scheme:", 7) == 0)
		{
			mpScheme->Select(strcmp(&sz[7], "Workspace") == 0 ? 1 : 0);
			SchemeChanged();
		}
		else if (strncmp(sz, "tab:", 4) == 0)
		{
			for (j = 0; j < 3; j++)
			{
				if (strcmp(CIDEFileTemplates::GetTabName(aeTabs[j]), &sz[4]) == 0)
				{
					mpTabs->Select(j);
					meTab = aeTabs[j];
					ShowList(NULL);
				}
			}
		}
		else if (strncmp(sz, "select:", 7) == 0)
		{
			SelectName(&sz[7]);
		}
		else if (strncmp(sz, "text:", 5) == 0)
		{
			szText.Init(&sz[5]);
			szText.Replace("\\n", "\n");
			szText.Replace("\\t", "\t");
			mpEditor->GetTextArea()->SetText(szText.Text());
			TextChanged();
			szText.Kill();
		}
		else if (strncmp(sz, "name:", 5) == 0)
		{
			mpName->SetText(&sz[5]);
			NameChanged();
		}
		else if (strncmp(sz, "extension:", 10) == 0)
		{
			mpExtension->SetText(&sz[10]);
			ExtensionChanged();
		}
		else if (strncmp(sz, "filename:", 9) == 0)
		{
			mpFileName->SetText(&sz[9]);
			FileNameChanged();
		}
		else if (strncmp(sz, "author:", 7) == 0)
		{
			mpAuthor->SetText(&sz[7]);
		}
		else if (strcmp(sz, "add") == 0)
		{
			AddClicked(NULL);
		}
		else if (strcmp(sz, "remove") == 0)
		{
			RemoveClicked(NULL);
		}
		else if (strcmp(sz, "copy") == 0)
		{
			CopyClicked(NULL);
		}
		else if (strcmp(sz, "reset") == 0)
		{
			ResetClicked(NULL);
		}
		else if (strcmp(sz, "show") == 0)
		{
			for (j = 0; j < maszRows.NumElements(); j++)
			{
				psTemplate = mcTemplates.Get(meTab, maszRows.GetText(j), meScheme);
				printf("Templates row: %s%s%s\n", psTemplate->szParent.Empty() ? "" : "  ", psTemplate->szName.Text(), IsChanged(psTemplate) ? " [changed]" : "");
			}
			printf("Templates selected: %s, error [%s], status [%s]\n", GetSelectedName() ? GetSelectedName() : "none", mpError->GetText(), mpStatus->GetText());
		}
		fflush(stdout);
	}
	aszEdits.Kill();
}
