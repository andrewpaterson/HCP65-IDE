/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "BaseLib/StringHelper.h"
#include "SupportLib/PNGWriter.h"
#include "StandardLib/ClassDefines.h"
#include "IDEContent.h"
#include "IDERename.h"


//A place Rename changes: the file (its index in the rename's files) and where the old name starts in its text.
struct SIDEPlace
{
	size	uiFile;
	size	uiStart;
};


typedef CArrayTemplate<SIDEPlace>	CArrayIDEPlaces;
typedef CArrayTemplate<size>		CArrayIDESizes;


//C++'s keywords, which a name can't be.
static const char* gaszIDEKeywords[] =
{
	"alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
	"const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast",
	"else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int",
	"long", "mutable", "namespace", "new", "noexcept", "nullptr", "operator", "private", "protected", "public",
	"register", "reinterpret_cast", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast",
	"struct", "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
	"union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	msRenameTarget.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpRenameDialog);
	M_Pointer(mpRenameField);
	M_Pointer(mpRenameError);
	U_Unknown(SIDERenameTarget, msRenameTarget);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::Free(void)
{
	msRenameTarget.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::Save(CObjectWriter* pcFile) { return false; }
bool CIDERename::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// The symbol named at the active editor's caret, or by its selection: a
// selection must be in one name, on one line, else it isn't renamable.
// False if no symbol is named there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::FindRenameTarget(SIDERenameTarget* psDest)
{
	Ptr<CTextArea>	pEditor;
	size			uiStart;
	size			uiEnd;
	int				iLine;
	int				iStartColumn;
	int				iEndColumn;
	uint8			uiRole;
	bool			bInName;
	SIDELocation*	psLocation;
	size			i;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}

	//What can be edited: the solutions' files.  The index is brought up to date with the editors first.
	mpcContent->GetNavigator()->FindGoToFiles();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();

	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iStartColumn, &uiRole) ||
		!mpcContent->GetCodeModel()->FindRenameTarget(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iStartColumn, mpcContent->GetNavigator()->GetSolutionFileSet(), psDest))
	{
		return false;
	}
	pEditor = mpcContent->GetDocuments()->GetActiveTab()->GetEditor();
	if (!pEditor->GetSelection(&uiStart, &uiEnd))
	{
		return true;
	}

	//The selection is all in the name named where it starts.
	bInName = false;
	if (pEditor->FindLine(uiEnd) == (size)iLine)
	{
		iEndColumn = (int)(uiEnd - pEditor->GetLineStart((size)iLine));
		for (i = 0; i < psDest->asLocations.NumElements(); i++)
		{
			psLocation = psDest->asLocations.Get(i);
			if ((psLocation->iLine == (int32)iLine) && psLocation->szFileName.Equals(mpcContent->GetDocuments()->GetActiveTab()->GetFileName()) &&
				(psLocation->iColumn <= iStartColumn) && (iEndColumn <= psLocation->iColumn + psLocation->iLength))
			{
				bInName = true;
				break;
			}
		}
	}
	if (!bInName)
	{
		psDest->bRenamable = false;
		psDest->szWhyNot.Set("the selection isn't a name");
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename is shown at the next tick: ShowModal can't be called from the
// input (Shift+F6 or a menu click) that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::QueueRename(void)
{
	mpcContent->GetWindow()->Post(this, &CIDERename::ShowQueuedRename);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::ShowQueuedRename(void)
{
	ShowRename();
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Rename: the new name for the symbol at the caret asked for,
// then every place it is written changed (see RenameSymbol).  False if
// nothing was renamed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::ShowRename(void)
{
	SIDERenameTarget	sTarget;
	CChars				szPrompt;
	CChars				szNewName;
	bool				bResult;

	sTarget.Init();
	if (!FindRenameTarget(&sTarget))
	{
		mpcContent->ShowStatus("Nothing to rename at the caret.");
		sTarget.Kill();
		return false;
	}
	if (!sTarget.bRenamable)
	{
		szPrompt.Init("Can't rename ");
		szPrompt.Append(sTarget.szName);
		szPrompt.Append(": ");
		szPrompt.Append(sTarget.szWhyNot);
		szPrompt.Append(".");
		mpcContent->ShowStatus(szPrompt.Text());
		szPrompt.Kill();
		sTarget.Kill();
		return false;
	}

	msRenameTarget.Kill();
	msRenameTarget.Init();
	msRenameTarget.szKey.Set(sTarget.szKey);
	msRenameTarget.szName.Set(sTarget.szName);

	szPrompt.Init("Rename '");
	szPrompt.Append(sTarget.szName);
	szPrompt.Append("'");
	if (sTarget.uiOverrides > 0)
	{
		//IntelliJ renames a method's whole hierarchy with it.
		szPrompt.Append(" and ");
		szPrompt.Append((uint32)sTarget.uiOverrides);
		szPrompt.Append(sTarget.uiOverrides == 1 ? " override" : " overrides");
	}
	szPrompt.Append(" to:");
	mpRenameDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Rename");
	mpRenameField = mpRenameDialog->AddTextField(szPrompt.Text(), sTarget.szName.Text());
	mpRenameField->SelectAll();
	mpRenameError = OMalloc<CLabel>(mpRenameDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpRenameDialog->AddEditor("", mpRenameError);
	mpRenameDialog->SetOKCaption("Refactor");
	mpRenameDialog->SetApprover(this, &CIDERename::ApproveRename);
	szPrompt.Kill();

	bResult = false;
	mpcContent->GetScript()->WatchDialog(mpRenameDialog);
	if (mpRenameDialog->Show(480))
	{
		szNewName.Init(mpRenameField->GetText());
		bResult = RenameSymbol(&sTarget, szNewName.Text());
		szNewName.Kill();
	}
	mpRenameDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpRenameField = NULL;
	mpRenameError = NULL;
	mpRenameDialog = NULL;
	sTarget.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The new name must be a C++ identifier, not a keyword, not the old name
// and not a name the symbol's scope already has.  The dialog says why
// not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::ApproveRename(void)
{
	char*		szName;
	size		i;
	const char*	szProblem;
	CChars		szNewKey;
	size		uiNameStart;

	szName = mpRenameField->GetText();
	szProblem = NULL;
	if (!IsIDEIdentifier(szName))
	{
		szProblem = "Not a C++ name.";
	}
	for (i = 0; (szProblem == NULL) && (i < sizeof(gaszIDEKeywords) / sizeof(char*)); i++)
	{
		if (strcmp(szName, gaszIDEKeywords[i]) == 0)
		{
			szProblem = "A C++ keyword can't be a name.";
		}
	}
	if ((szProblem == NULL) && msRenameTarget.szName.Equals(szName))
	{
		szProblem = "It is already called that.";
	}

	//The same key with the new name: something in the same scope already has it.
	if ((szProblem == NULL) && (msRenameTarget.szKey.Text()[0] != '@'))
	{
		uiNameStart = FindIDENameInKey(msRenameTarget.szKey.Text(), msRenameTarget.szName.Text());
		if (uiNameStart != MAX_SIZE)
		{
			szNewKey.Init();
			szNewKey.AppendSubString(msRenameTarget.szKey.Text(), 0, uiNameStart);
			szNewKey.Append(szName);
			szNewKey.Append(&msRenameTarget.szKey.Text()[uiNameStart + msRenameTarget.szName.Length()]);
			if (mpcContent->GetCodeModel()->HasSymbol(szNewKey.Text()))
			{
				szProblem = "Something here already has that name.";
			}
			szNewKey.Kill();
		}
	}

	mpRenameError->SetText(szProblem ? szProblem : "");
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Changes every place psTarget's name is written to szNewName, in each
// file's editor (opening it if it isn't open), as one edit per file and
// one undo across them all (Edit, Undo Across Files, or Control+Z in any
// of them).  Nothing is changed if any place no longer has the old name:
// the file was edited since it was indexed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::RenameSymbol(SIDERenameTarget* psTarget, const char* szNewName)
{
	CArrayChars			aszFiles;
	CArrayIDEPlaces		asPlaces;
	SIDELocation*		psLocation;
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	size				i;
	size				j;
	size				uiFile;
	size				uiStart;
	size				uiLength;
	char*				szText;
	bool				bChanged;
	CChars				szMessage;
	size				uiPlaces;
	SIDEPlace*			psPlace;
	CArrayIDESizes		auiStarts;
	CArrayIDESizes		auiEnds;
	bool				bFound;

	//Each file's editor, opened (not shown) if it isn't open.
	aszFiles.Init();
	asPlaces.Init();
	bChanged = true;
	for (i = 0; i < psTarget->asLocations.NumElements(); i++)
	{
		psLocation = psTarget->asLocations.Get(i);
		for (uiFile = 0; uiFile < aszFiles.NumElements(); uiFile++)
		{
			if (StringCompare(aszFiles.GetText(uiFile), psLocation->szFileName.Text()) == 0)
			{
				break;
			}
		}
		if (uiFile == aszFiles.NumElements())
		{
			pTab = mpcContent->GetDocuments()->FindTab(psLocation->szFileName.Text());
			if (pTab.IsNull())
			{
				pTab = mpcContent->GetDocuments()->OpenFileTab(psLocation->szFileName.Text(), mpcContent->GetCodeModel()->FindProjectOfFile(psLocation->szFileName.Text()), false);
			}
			if (pTab.IsNull())
			{
				bChanged = false;
				break;
			}
			aszFiles.Add(psLocation->szFileName.Text());
		}

		//The old name must still be there; a destructor's place may include its ~.
		pEditor = mpcContent->GetDocuments()->FindTab(aszFiles.GetText(uiFile))->GetEditor();
		if ((size)psLocation->iLine >= pEditor->NumLines())
		{
			bChanged = false;
			break;
		}
		uiStart = pEditor->GetLineStart((size)psLocation->iLine) + (size)psLocation->iColumn;
		uiLength = (size)psLocation->iLength;
		szText = pEditor->GetText();
		if ((uiStart < strlen(szText)) && (szText[uiStart] == '~') && (uiLength == psTarget->szName.Length() + 1))
		{
			uiStart++;
			uiLength--;
		}
		if ((uiLength != psTarget->szName.Length()) || (strncmp(&szText[uiStart], psTarget->szName.Text(), uiLength) != 0))
		{
			bChanged = false;
			break;
		}
		psPlace = asPlaces.Add();
		psPlace->uiFile = uiFile;
		psPlace->uiStart = uiStart;
	}

	if (!bChanged)
	{
		szMessage.Init("Rename stopped: ");
		szMessage.Append(psLocation->szFileName.Text());
		szMessage.Append(" has changed since it was indexed.  Save it and try again once it has been indexed.");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
	}
	else
	{
		//One edit per file, its places in order, the same place once.
		uiPlaces = 0;
		mpcContent->GetEdit()->BeginUndoGroup();
		for (uiFile = 0; uiFile < aszFiles.NumElements(); uiFile++)
		{
			auiStarts.Init();
			auiEnds.Init();
			for (i = 0; i < asPlaces.NumElements(); i++)
			{
				psPlace = asPlaces.Get(i);
				if (psPlace->uiFile != uiFile)
				{
					continue;
				}
				bFound = false;
				for (j = 0; j < auiStarts.NumElements(); j++)
				{
					if (*auiStarts.Get(j) >= psPlace->uiStart)
					{
						bFound = (*auiStarts.Get(j) == psPlace->uiStart);
						break;
					}
				}
				if (!bFound)
				{
					auiStarts.InsertAt(&psPlace->uiStart, j);
				}
			}
			for (i = 0; i < auiStarts.NumElements(); i++)
			{
				*auiEnds.Add() = *auiStarts.Get(i) + psTarget->szName.Length();
			}
			if (mpcContent->GetDocuments()->FindTab(aszFiles.GetText(uiFile))->GetEditor()->ReplaceRanges(auiStarts.GetData(), auiEnds.GetData(), auiStarts.NumElements(), szNewName))
			{
				uiPlaces += auiStarts.NumElements();
			}
			auiEnds.Kill();
			auiStarts.Kill();
		}
		mpcContent->GetEdit()->EndUndoGroup();

		szMessage.Init("Renamed ");
		szMessage.Append(psTarget->szName);
		szMessage.Append(" to ");
		szMessage.Append(szNewName);
		szMessage.Append(": ");
		szMessage.Append((int)uiPlaces);
		szMessage.Append(uiPlaces == 1 ? " place in " : " places in ");
		szMessage.Append((int)aszFiles.NumElements());
		szMessage.Append(aszFiles.NumElements() == 1 ? " file." : " files.");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
	}

	asPlaces.Kill();
	aszFiles.Kill();
	return bChanged;
}


//////////////////////////////////////////////////////////////////////////
//
// A C++ identifier: a letter or underscore then letters, digits and
// underscores.
//
//////////////////////////////////////////////////////////////////////////
bool IsIDEIdentifier(const char* szName)
{
	size	i;
	char	c;

	if ((szName == NULL) || (szName[0] == '\0'))
	{
		return false;
	}
	for (i = 0; szName[i] != '\0'; i++)
	{
		c = szName[i];
		if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || (c == '_') || ((i > 0) && (c >= '0') && (c <= '9'))))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where szName, a symbol's name, is in its key: after its scope's last
// "::", before its parameters.  MAX_SIZE if it isn't.
//
//////////////////////////////////////////////////////////////////////////
size FindIDENameInKey(const char* szKey, const char* szName)
{
	const char*	szParameters;
	const char*	szFound;
	const char*	szLast;
	size		uiNameLength;

	uiNameLength = strlen(szName);
	szParameters = strchr(szKey, '(');
	szLast = NULL;
	for (szFound = strstr(szKey, szName); szFound && (!szParameters || (szFound < szParameters)); szFound = strstr(szFound + 1, szName))
	{
		szLast = szFound;
	}
	if ((szLast == NULL) || ((szLast[uiNameLength] != '\0') && (szLast[uiNameLength] != '(')))
	{
		return MAX_SIZE;
	}
	return (size)(szLast - szKey);
}


//////////////////////////////////////////////////////////////////////////
//
// Rename (Shift+F6, as IntelliJ IDEA's), while what is at the caret can
// be renamed.
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.Rename", "Rename...", "Shift+F6", Ptr<CImageCel>(), this, &CIDERename::QueueRename)->SetEnabledCheck(this, &CIDERename::CanRename);
}


//////////////////////////////////////////////////////////////////////////
//
// The editors' context menu's Rename, and Refactor's Extract Method (not
// done yet, so always disabled).
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::AddEditorItems(Ptr<CMenuContainer> pMenu)
{
	Ptr<CMenuContainer>	pRefactor;

	pMenu->AddItem(mpcContent->GetCommands()->Find("Refactor.Rename"));
	pRefactor = OMalloc<CMenuContainer>(mpcContent->GetWindow(), mpcContent->GetTheme());
	pRefactor->AddItem("Extract Method...")->SetEnabled(false);
	pMenu->AddItem("Refactor", NULL, pRefactor);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether what is at the caret can be renamed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::CanRename(void)
{
	SIDERenameTarget	sTarget;
	bool				bRenamable;

	sTarget.Init();
	bRenamable = FindRenameTarget(&sTarget) && sTarget.bRenamable;
	sTarget.Kill();
	return bRenamable;
}
