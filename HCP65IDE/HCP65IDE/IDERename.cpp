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


#define IDE_RENAME_SETTINGS_TAG			"Rename"
#define IDE_RENAME_COMMENTS_ATTRIBUTE		"SearchInComments"
#define IDE_RENAME_TEXT_ATTRIBUTE			"SearchForText"
#define IDE_RENAME_TYPE_COMMENTS_ATTRIBUTE	"SearchInCommentsForTypes"
#define IDE_RENAME_TYPE_TEXT_ATTRIBUTE		"SearchForTextForTypes"
#define IDE_RENAME_FILES_ATTRIBUTE			"RenameFilesForTypes"


//How Rename found a place: the index has it, or a search found it as text the index doesn't have or in a comment or string.
enum EIDERenamePlace
{
	IRP_Index,
	IRP_Text,
	IRP_Comment,
};


//A place Rename changes: the file (its index in the rename's files) and where the old name starts in its text.
struct SIDEPlace
{
	size	uiFile;
	size	uiStart;
	uint8	uiKind;		//EIDERenamePlace.
};





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
bool IsIDENameChar(char c)
{
	return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_');
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void AddIDEOccurrence(SIDELineCounter* psLines, size uiStart, bool bComment, CArrayIDENameOccurrences* pasDest)
{
	SIDENameOccurrence*	psOccurrence;

	psLines->MoveTo(uiStart);
	psOccurrence = pasDest->Add();
	psOccurrence->uiStart = uiStart;
	psOccurrence->iLine = psLines->iLine;
	psOccurrence->iColumn = (int32)(uiStart - psLines->uiLineStart);
	psOccurrence->bComment = bComment;
}


//////////////////////////////////////////////////////////////////////////
//
// Each place szName is a whole word in szText from uiStart to uiEnd.
//
//////////////////////////////////////////////////////////////////////////
void FindIDEWords(const char* szText, size uiStart, size uiEnd, const char* szName, size uiNameLength, bool bComment, SIDELineCounter* psLines, CArrayIDENameOccurrences* pasDest)
{
	size	i;

	for (i = uiStart; i + uiNameLength <= uiEnd; i++)
	{
		if ((szText[i] == szName[0]) && (strncmp(&szText[i], szName, uiNameLength) == 0) &&
			((i == 0) || !IsIDENameChar(szText[i - 1])) && !IsIDENameChar(szText[i + uiNameLength]))
		{
			AddIDEOccurrence(psLines, i, bComment, pasDest);
			i += uiNameLength - 1;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where a C or C++ file names szName: as an identifier in its code
// (bCode), and as a word in its comments, strings and character literals
// (bComments).  The file isn't preprocessed: code an #if leaves out is
// searched as any other.
//
//////////////////////////////////////////////////////////////////////////
void FindIDECppOccurrences(const char* szText, size uiLength, const char* szName, bool bCode, bool bComments, SIDELineCounter* psLines, CArrayIDENameOccurrences* pasDest)
{
	size		uiNameLength;
	size		i;
	size		j;
	size		k;
	char		c;
	char		cNext;
	const char*	szEnd;
	CChars		szClose;

	uiNameLength = strlen(szName);
	i = 0;
	while (i < uiLength)
	{
		c = szText[i];
		cNext = (i + 1 < uiLength) ? szText[i + 1] : '\0';
		if ((c == '/') && (cNext == '/'))
		{
			szEnd = strchr(&szText[i], '\n');
			j = szEnd ? (size)(szEnd - szText) : uiLength;
			if (bComments)
			{
				FindIDEWords(szText, i + 2, j, szName, uiNameLength, true, psLines, pasDest);
			}
			i = j;
		}
		else if ((c == '/') && (cNext == '*'))
		{
			szEnd = strstr(&szText[i + 2], "*/");
			j = szEnd ? (size)(szEnd - szText) : uiLength;
			if (bComments)
			{
				FindIDEWords(szText, i + 2, j, szName, uiNameLength, true, psLines, pasDest);
			}
			i = szEnd ? j + 2 : uiLength;
		}
		else if ((c == '"') || (c == '\''))
		{
			for (j = i + 1; (j < uiLength) && (szText[j] != c) && (szText[j] != '\n'); j++)
			{
				if ((szText[j] == '\\') && (j + 1 < uiLength))
				{
					j++;
				}
			}
			if (bComments)
			{
				FindIDEWords(szText, i + 1, j, szName, uiNameLength, true, psLines, pasDest);
			}
			i = ((j < uiLength) && (szText[j] == c)) ? j + 1 : j;
		}
		else if (((c >= '0') && (c <= '9')) || ((c == '.') && (cNext >= '0') && (cNext <= '9')))
		{
			//A number, with its suffix, exponent sign and digit separators.
			for (j = i + 1; j < uiLength; j++)
			{
				if (IsIDENameChar(szText[j]) || (szText[j] == '.') || ((szText[j] == '\'') && (j + 1 < uiLength) && IsIDENameChar(szText[j + 1])))
				{
					continue;
				}
				if (((szText[j] == '+') || (szText[j] == '-')) && strchr("eEpP", szText[j - 1]))
				{
					continue;
				}
				break;
			}
			i = j;
		}
		else if (IsIDENameChar(c))
		{
			for (j = i + 1; (j < uiLength) && IsIDENameChar(szText[j]); j++)
			{
			}
			if ((j < uiLength) && (szText[j] == '"') && (szText[j - 1] == 'R') &&
				((j - i == 1) || ((j - i == 2) && strchr("uUL", szText[i])) || ((j - i == 3) && (strncmp(&szText[i], "u8", 2) == 0))))
			{
				//A raw string: R"delimiter( ... )delimiter".
				for (k = j + 1; (k < uiLength) && (szText[k] != '(') && (k - j <= 17); k++)
				{
				}
				szClose.Init(")");
				szClose.AppendSubString(szText, j + 1, k);
				szClose.Append('"');
				szEnd = strstr(&szText[k < uiLength ? k : uiLength], szClose.Text());
				if (bComments && (k < uiLength))
				{
					FindIDEWords(szText, k + 1, szEnd ? (size)(szEnd - szText) : uiLength, szName, uiNameLength, true, psLines, pasDest);
				}
				i = szEnd ? (size)(szEnd - szText) + szClose.Length() : uiLength;
				szClose.Kill();
				continue;
			}
			if (bCode && (j - i == uiNameLength) && (strncmp(&szText[i], szName, uiNameLength) == 0))
			{
				AddIDEOccurrence(psLines, i, false, pasDest);
			}
			i = j;
		}
		else
		{
			i++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A text file Rename's text search looks in.
//
//////////////////////////////////////////////////////////////////////////
bool IsIDERenameTextFile(const char* szFileName)
{
	size	uiLength;

	uiLength = strlen(szFileName);
	return ((uiLength > 4) && (StringInsensitiveCompare(&szFileName[uiLength - 4], ".txt") == 0)) ||
		   ((uiLength > 3) && (StringInsensitiveCompare(&szFileName[uiLength - 3], ".md") == 0));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	msRenameTarget.Init();

	//As IntelliJ's: a type's name is looked for in comments and as text, anything else's isn't.
	mabComments[0] = false;
	mabText[0] = false;
	mabComments[1] = true;
	mabText[1] = true;
	mbFiles = true;
	maszClassFiles.Init();
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
	M_Pointer(mpCommentsBox);
	M_Pointer(mpTextBox);
	M_Pointer(mpFilesBox);
	U_Unknown(CArrayChars, maszClassFiles);
	U_Bool(mbFiles);
	U_Unknown(SIDERenameTarget, msRenameTarget);
	U_BoolArray(mabComments, 2);
	U_BoolArray(mabText, 2);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's memory is freed without its destructor running, so it
// takes itself off what it listens to.
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::Free(void)
{
	maszClassFiles.Kill();
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

	//The index is brought up to date with the editors first.
	mpcContent->GetDocuments()->UpdateEditedTabsNow();

	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iStartColumn, &uiRole) ||
		!mpcContent->GetCodeModel()->FindRenameTarget(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iStartColumn, psDest))
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
	//In Solution Explorer, as in IntelliJ's Project view, it renames the selected file.
	if (IsExplorerFocussed())
	{
		mpcContent->GetFileRename()->RenameExplorerFile();
		return;
	}
	mpcContent->GetWindow()->Post(this, &CIDERename::ShowQueuedRename);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether Solution Explorer has the keys: Rename is Rename File there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::IsExplorerFocussed(void)
{
	return mpcContent->GetWindow()->GetFocus()->GetFocussedComponent() == mpcContent->GetSolutions()->GetExplorer()->GetTree();
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
	CFileUtil			cFileUtil;
	CChars				szFiles;
	CChars				szFileName;
	Ptr<CLabel>			pFiles;
	size				i;

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
	mpCommentsBox = mpRenameDialog->AddCheckBox("Search in comments and strings", mabComments[sTarget.bType]);
	mpTextBox = mpRenameDialog->AddCheckBox("Search for text occurrences", mabText[sTarget.bType]);

	//A type's files can be renamed with it, as IntelliJ renames a class's file.
	maszClassFiles.ReInit();
	mpFilesBox = NULL;
	if (mpcContent->GetFileRename()->FindClassFiles(&sTarget, &maszClassFiles))
	{
		szFiles.Init();
		for (i = 0; i < maszClassFiles.NumElements(); i++)
		{
			szFileName.Init();
			cFileUtil.GetFileName(&szFileName, maszClassFiles.GetText(i));
			szFiles.Append(i > 0 ? ", " : "");
			szFiles.Append(szFileName);
			szFileName.Kill();
		}
		mpFilesBox = mpRenameDialog->AddCheckBox("Rename files", mbFiles);
		pFiles = OMalloc<CLabel>(mpRenameDialog->GetWindow(), mpcContent->GetDimFont(), szFiles.Text());
		pFiles->SetAlignment(CSH_Left, CSV_Center);
		mpRenameDialog->AddEditor("", pFiles);
		pFiles = NULL;
		szFiles.Kill();
	}
	mpRenameError = OMalloc<CLabel>(mpRenameDialog->GetWindow(), mpcContent->GetDimFont(), "");
	mpRenameDialog->AddEditor("", mpRenameError);
	mpRenameDialog->SetOKCaption("Refactor");
	mpRenameDialog->SetApprover(this, &CIDERename::ApproveRename);
	szPrompt.Kill();

	bResult = false;
	mpcContent->GetScript()->WatchDialog(mpRenameDialog);
	if (mpRenameDialog->Show(480))
	{
		//The searches are remembered for the next type, or the next anything else.
		if ((mabComments[sTarget.bType] != mpCommentsBox->IsTicked()) || (mabText[sTarget.bType] != mpTextBox->IsTicked()))
		{
			mabComments[sTarget.bType] = mpCommentsBox->IsTicked();
			mabText[sTarget.bType] = mpTextBox->IsTicked();
			mpcContent->GetSettings()->WriteSettings();
		}
		if (mpFilesBox.IsNotNull() && (mpFilesBox->IsTicked() != mbFiles))
		{
			mbFiles = mpFilesBox->IsTicked();
			mpcContent->GetSettings()->WriteSettings();
		}
		szNewName.Init(mpRenameField->GetText());
		bResult = RenameClass(&sTarget, szNewName.Text(), mpFilesBox.IsNotNull() && mpFilesBox->IsTicked());
		szNewName.Kill();
	}
	mpRenameDialog->ClearApprover();
	mpcContent->GetScript()->StopWatchingDialog();
	mpRenameField = NULL;
	mpRenameError = NULL;
	mpCommentsBox = NULL;
	mpTextBox = NULL;
	mpFilesBox = NULL;
	maszClassFiles.ReInit();
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
	CChars					szFilesProblem;
	CArrayIDEFileRenames	asRenames;

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

	//The type's files must be able to have the new name's stem.
	szFilesProblem.Init();
	if ((szProblem == NULL) && mpFilesBox.IsNotNull() && mpFilesBox->IsTicked())
	{
		asRenames.Init();
		if (!mpcContent->GetFileRename()->AddClassRenames(msRenameTarget.szName.Text(), szName, &maszClassFiles, &asRenames))
		{
			szProblem = "Its files need the new name to start with the same letter.";
		}
		else if (mpcContent->GetFileRename()->FindProblem(&asRenames, &szFilesProblem))
		{
			szFilesProblem.Insert(0, "Its files can't be renamed: ");
			szFilesProblem.Append('.');
			szProblem = szFilesProblem.Text();
		}
		KillIDEFileRenames(&asRenames);
	}

	mpRenameError->SetText(szProblem ? szProblem : "");
	szFilesProblem.Kill();
	return szProblem == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The index in paszFiles of szFileName, added with its editor opened
// (not shown) if it isn't open.  MAX_SIZE if it can't be opened.
//
//////////////////////////////////////////////////////////////////////////
size CIDERename::FindRenameFile(CArrayChars* paszFiles, const char* szFileName)
{
	size				uiFile;
	Ptr<CIDEEditorTab>	pTab;

	for (uiFile = 0; uiFile < paszFiles->NumElements(); uiFile++)
	{
		if (StringCompare(paszFiles->GetText(uiFile), szFileName) == 0)
		{
			return uiFile;
		}
	}
	pTab = mpcContent->GetDocuments()->FindTab(szFileName);
	if (pTab.IsNull())
	{
		pTab = mpcContent->GetDocuments()->OpenFileTab(szFileName, mpcContent->GetCodeModel()->FindProjectOfFile(szFileName), false);
	}
	if (pTab.IsNull())
	{
		return MAX_SIZE;
	}
	paszFiles->Add(szFileName);
	return uiFile;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename's searches: each of the solution's files looked in for szName
// in comments and strings (bComments), and as code the index doesn't
// have or in the projects' text files (bText), the places found added
// to pasPlaces.  A name in code the index has is another symbol's, or
// already one of the places.  False, with the file in pszChanged, if a
// file's editor doesn't have the text that was searched.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::SearchRenameFiles(const char* szName, bool bComments, bool bText, CArrayChars* paszFiles, CArrayIDEPlaces* pasPlaces, CChars* pszChanged)
{
	CIDECodeModel*				pModel;
	size						uiProjectFile;
	char*						szFileName;
	EIDEFileType				eType;
	bool						bCpp;
	CTextFile					cFile;
	char*						szText;
	size						uiLength;
	SIDELineCounter				sLines;
	CArrayIDENameOccurrences	asFound;
	SIDENameOccurrence*			psFound;
	size						i;
	uint8						uiRole;
	size						uiFile;
	char*						szEditorText;
	SIDEPlace*					psPlace;
	size						uiNameLength;
	bool						bResult;

	pModel = mpcContent->GetCodeModel();
	uiNameLength = strlen(szName);
	bResult = true;
	for (uiProjectFile = 0; (uiProjectFile < pModel->NumProjectFiles()) && bResult; uiProjectFile++)
	{
		szFileName = pModel->GetProjectFile(uiProjectFile);
		eType = CIDEIcons::GetFileType(szFileName);
		bCpp = (eType == IFT_CPP) || (eType == IFT_C) || (eType == IFT_Header);
		if (!bCpp && !(bText && IsIDERenameTextFile(szFileName)))
		{
			continue;
		}

		cFile.Init();
		if (!mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength))
		{
			cFile.Kill();
			continue;
		}
		asFound.Init();
		sLines.Init(szText);
		if (bCpp)
		{
			FindIDECppOccurrences(szText, uiLength, szName, bText, bComments, &sLines, &asFound);
		}
		else
		{
			FindIDEWords(szText, 0, uiLength, szName, uiNameLength, false, &sLines, &asFound);
		}
		cFile.Kill();

		for (i = 0; i < asFound.NumElements(); i++)
		{
			psFound = asFound.Get(i);
			if (bCpp && !psFound->bComment && pModel->FindRoleAt(szFileName, psFound->iLine, psFound->iColumn, &uiRole))
			{
				continue;
			}
			uiFile = FindRenameFile(paszFiles, szFileName);
			szEditorText = (uiFile != MAX_SIZE) ? mpcContent->GetDocuments()->FindTab(szFileName)->GetEditor()->GetText() : NULL;
			if ((szEditorText == NULL) || (psFound->uiStart + uiNameLength > strlen(szEditorText)) || (strncmp(&szEditorText[psFound->uiStart], szName, uiNameLength) != 0))
			{
				pszChanged->Set(szFileName);
				bResult = false;
				break;
			}
			psPlace = pasPlaces->Add();
			psPlace->uiFile = uiFile;
			psPlace->uiStart = psFound->uiStart;
			psPlace->uiKind = psFound->bComment ? IRP_Comment : IRP_Text;
		}
		asFound.Kill();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Changes every place psTarget's name is written to szNewName, in each
// file's editor (opening it if it isn't open), as one edit per file and
// one undo across them all (Edit, Undo Across Files, or Control+Z in any
// of them).  With the searches remembered for its kind (see
// SetSearches) the name is changed in comments and strings, and as text
// the index doesn't have, too.  Nothing is changed if any place no longer
// has the old name: the file was edited since it was indexed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::RenameSymbol(SIDERenameTarget* psTarget, const char* szNewName, CArrayChars* paszEdited, CChars* pszStatus)
{
	size				uiAssembly;
	size				uiUnsure;
	CArrayChars			aszFiles;
	CArrayIDEPlaces		asPlaces;
	SIDELocation*		psLocation;
	Ptr<CTextArea>		pEditor;
	size				i;
	size				j;
	size				uiFile;
	size				uiStart;
	size				uiLength;
	char*				szText;
	bool				bChanged;
	CChars				szMessage;
	CChars				szChanged;
	size				auiPlaces[3];
	SIDEPlace*			psPlace;
	CArrayTextRange		asRanges;
	STextRange			sRange;
	bool				bFound;
	bool				bComments;
	bool				bText;

	//Each file's editor, opened (not shown) if it isn't open.
	aszFiles.Init();
	asPlaces.Init();
	szChanged.Init();
	uiAssembly = 0;
	uiUnsure = 0;
	bChanged = true;
	for (i = 0; i < psTarget->asLocations.NumElements(); i++)
	{
		psLocation = psTarget->asLocations.Get(i);
		uiFile = FindRenameFile(&aszFiles, psLocation->szFileName.Text());
		if (uiFile == MAX_SIZE)
		{
			szChanged.Set(psLocation->szFileName);
			bChanged = false;
			break;
		}

		//The old name must still be there; a destructor's place may include its ~.
		pEditor = mpcContent->GetDocuments()->FindTab(aszFiles.GetText(uiFile))->GetEditor();
		if ((size)psLocation->iLine >= pEditor->NumLines())
		{
			szChanged.Set(psLocation->szFileName);
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
			szChanged.Set(psLocation->szFileName);
			bChanged = false;
			break;
		}
		psPlace = asPlaces.Add();
		psPlace->uiFile = uiFile;
		psPlace->uiStart = uiStart;
		psPlace->uiKind = IRP_Index;
	}

	//IntelliJ's searches in comments and strings, and for text occurrences.
	bComments = mabComments[psTarget->bType];
	bText = mabText[psTarget->bType];
	if (bChanged && (bComments || bText))
	{
		bChanged = SearchRenameFiles(psTarget->szName.Text(), bComments, bText, &aszFiles, &asPlaces, &szChanged);
	}

	if (!bChanged)
	{
		szMessage.Init("Rename stopped: ");
		szMessage.Append(szChanged);
		szMessage.Append(" has changed since it was indexed.  Save it and try again once it has been indexed.");
		ShowRenameStatus(&szMessage, pszStatus);
		szMessage.Kill();
	}
	else
	{
		//One edit per file, its places in order, the same place once.
		auiPlaces[IRP_Index] = 0;
		auiPlaces[IRP_Text] = 0;
		auiPlaces[IRP_Comment] = 0;
		mpcContent->GetEdit()->BeginUndoGroup();
		for (uiFile = 0; uiFile < aszFiles.NumElements(); uiFile++)
		{
			asRanges.Init();
			for (i = 0; i < asPlaces.NumElements(); i++)
			{
				psPlace = asPlaces.Get(i);
				if (psPlace->uiFile != uiFile)
				{
					continue;
				}
				bFound = false;
				for (j = 0; j < asRanges.NumElements(); j++)
				{
					if (asRanges.Get(j)->uiStart >= psPlace->uiStart)
					{
						bFound = (asRanges.Get(j)->uiStart == psPlace->uiStart);
						break;
					}
				}
				if (!bFound)
				{
					sRange.uiStart = psPlace->uiStart;
					sRange.uiEnd = psPlace->uiStart + psTarget->szName.Length();
					asRanges.InsertAt(&sRange, j);
					auiPlaces[psPlace->uiKind]++;
				}
			}
			mpcContent->GetDocuments()->FindTab(aszFiles.GetText(uiFile))->GetEditor()->ReplaceRanges(asRanges.GetData(), asRanges.NumElements(), szNewName);
			asRanges.Kill();
		}
		uiAssembly = RenameAssembly(psTarget, szNewName, &aszFiles, &uiUnsure);
		mpcContent->GetEdit()->EndUndoGroup();

		szMessage.Init("Renamed ");
		szMessage.Append(psTarget->szName);
		szMessage.Append(" to ");
		szMessage.Append(szNewName);
		szMessage.Append(": ");
		szMessage.Append((int)auiPlaces[IRP_Index]);
		szMessage.Append(auiPlaces[IRP_Index] == 1 ? " place" : " places");
		if (auiPlaces[IRP_Text] > 0)
		{
			szMessage.Append(", ");
			szMessage.Append((int)auiPlaces[IRP_Text]);
			szMessage.Append(auiPlaces[IRP_Text] == 1 ? " text occurrence" : " text occurrences");
		}
		if (auiPlaces[IRP_Comment] > 0)
		{
			szMessage.Append(", ");
			szMessage.Append((int)auiPlaces[IRP_Comment]);
			szMessage.Append(" in comments and strings");
		}
		if (uiAssembly > 0)
		{
			szMessage.Append(", ");
			szMessage.Append((int)uiAssembly);
			szMessage.Append(" in assembly");
		}
		szMessage.Append(" in ");
		szMessage.Append((int)aszFiles.NumElements());
		szMessage.Append(aszFiles.NumElements() == 1 ? " file." : " files.");
		if (uiUnsure > 0)
		{
			szMessage.Append("  ");
			szMessage.Append((int)uiUnsure);
			szMessage.Append((uiUnsure == 1) ? " assembly label fits" : " assembly labels fit");
			szMessage.Append(" this overload and another: left, check by hand.");
		}
		ShowRenameStatus(&szMessage, pszStatus);
		szMessage.Kill();
	}

	szChanged.Kill();
	asPlaces.Kill();
	for (i = 0; paszEdited && bChanged && (i < aszFiles.NumElements()); i++)
	{
		paszEdited->Add(aszFiles.GetText(i));
	}
	aszFiles.Kill();
	return bChanged;
}


//////////////////////////////////////////////////////////////////////////
//
// The symbol renamed in the solution's hand written assembly (.asm and
// .inc items): in the labels the compiler gives it (FindIDEAssemblyEdits),
// and with the comment search in comments, each file's edits from its end
// back, in its editor (opened, not shown, if it isn't open; added to
// paszFiles).  An overloaded function's label is changed when its
// parameters fit this overload and no other (the labels that fit others
// too are counted in *puiUnsure and left).  The number of places changed
// (a name in a parameter list and its length are one).
//
//////////////////////////////////////////////////////////////////////////
size CIDERename::RenameAssembly(SIDERenameTarget* psTarget, const char* szNewName, CArrayChars* paszFiles, size* puiUnsure)
{
	CIDECodeModel*			pcModel;
	SIDEAssemblyRename		sRename;
	EIDEAssemblyRename		eKind;
	CArrayIDEAssemblyEdits	asEdits;
	CTextFile				cFile;
	char*					szText;
	char*					szFileName;
	size					uiLength;
	size					uiFile;
	size					uiEdits;
	size					i;
	size					j;
	bool					bComments;
	Ptr<CTextArea>			pEditor;
	STextRange				sRange;
	SIDEAssemblyEdit*		psEdit;

	eKind = (EIDEAssemblyRename)psTarget->uiAssembly;
	bComments = mabComments[psTarget->bType];
	if ((eKind == IAR_None) && !bComments)
	{
		return 0;
	}
	sRename.Init(eKind, psTarget->szName.Text(), szNewName, psTarget->szScope.Text());
	if (psTarget->bOverloaded)
	{
		sRename.szKey.Set(psTarget->szKey);
		for (i = 0; i < psTarget->aszOverloads.NumElements(); i++)
		{
			sRename.aszOverloads.Add(psTarget->aszOverloads.GetText(i));
		}
	}
	pcModel = mpcContent->GetCodeModel();
	uiEdits = 0;
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFileName = pcModel->GetProjectFile(i);
		if (!IsIDEAssemblyFile(szFileName))
		{
			continue;
		}

		//Looked in as it is first, so only files that have the name are opened.
		cFile.Init();
		asEdits.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength))
		{
			FindIDEAssemblyEdits(szText, uiLength, &sRename, bComments, &asEdits);
		}
		cFile.Kill();
		uiFile = (asEdits.NumElements() > 0) ? FindRenameFile(paszFiles, szFileName) : MAX_SIZE;
		KillIDEAssemblyEdits(&asEdits);
		if (uiFile == MAX_SIZE)
		{
			continue;
		}

		pEditor = mpcContent->GetDocuments()->FindTab(paszFiles->GetText(uiFile))->GetEditor();
		asEdits.Init();
		FindIDEAssemblyEdits(pEditor->GetText(), strlen(pEditor->GetText()), &sRename, bComments, &asEdits);
		for (j = asEdits.NumElements(); j > 0; j--)
		{
			psEdit = asEdits.Get(j - 1);
			sRange.uiStart = psEdit->uiStart;
			sRange.uiEnd = psEdit->uiStart + psEdit->uiLength;
			pEditor->ReplaceRanges(&sRange, 1, psEdit->szReplacement.Text());
			if (!psEdit->bLength)
			{
				uiEdits++;
			}
		}
		KillIDEAssemblyEdits(&asEdits);
	}
	*puiUnsure = sRename.uiUnsure;
	sRename.Kill();
	return uiEdits;
}


//////////////////////////////////////////////////////////////////////////
//
// What Rename did, on the status line, or appended to pszStatus.
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::ShowRenameStatus(CChars* pszMessage, CChars* pszStatus)
{
	if (pszStatus)
	{
		pszStatus->Append(pszMessage);
	}
	else
	{
		mpcContent->ShowStatus(pszMessage->Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Rename, and with bFiles a type's files renamed with it (RenameFile.txt
// step 6): its files (CIDEFileRename::FindClassFiles) given the new
// name's stem by Rename File, the symbol's places, the #includes and the
// files renamed as one undo group, and every file edited saved, as
// Rename File saves them.  Nothing changes if the files can't be renamed
// (the status line says why).  False if nothing was renamed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERename::RenameClass(SIDERenameTarget* psTarget, const char* szNewName, bool bFiles)
{
	Ptr<CIDEFileRename>		pFileRename;
	CArrayChars				aszClassFiles;
	CArrayChars				aszEdited;
	CArrayIDEFileRenames	asRenames;
	CChars					szStatus;
	SIDEFileRename*			psRename;
	Ptr<CIDEEditorTab>		pTab;
	size					i;
	size					j;
	bool					bResult;

	pFileRename = mpcContent->GetFileRename();
	aszClassFiles.Init();
	asRenames.Init();
	if (!bFiles || !pFileRename->FindClassFiles(psTarget, &aszClassFiles) || !pFileRename->AddClassRenames(psTarget->szName.Text(), szNewName, &aszClassFiles, &asRenames))
	{
		KillIDEFileRenames(&asRenames);
		aszClassFiles.Kill();
		return RenameSymbol(psTarget, szNewName);
	}
	aszClassFiles.Kill();

	szStatus.Init();
	if (pFileRename->FindProblem(&asRenames, &szStatus))
	{
		szStatus.Insert(0, "Can't rename its files: ");
		szStatus.Append('.');
		mpcContent->ShowStatus(szStatus.Text());
		szStatus.Kill();
		KillIDEFileRenames(&asRenames);
		return false;
	}

	aszEdited.Init();
	mpcContent->GetEdit()->BeginUndoGroup();
	bResult = RenameSymbol(psTarget, szNewName, &aszEdited, &szStatus);
	if (bResult)
	{
		szStatus.Append("  ");
		bResult = pFileRename->RenameFiles(&asRenames, mabComments[psTarget->bType], mabText[psTarget->bType], &szStatus);
	}
	mpcContent->GetEdit()->EndUndoGroup();

	//The symbol's edits are saved too, in the files' new names.
	for (i = 0; i < aszEdited.NumElements(); i++)
	{
		for (j = 0; j < asRenames.NumElements(); j++)
		{
			psRename = asRenames.Get(j);
			if (StringCompare(aszEdited.GetText(i), psRename->szOldPath.Text()) == 0)
			{
				aszEdited.Get(i)->Set(psRename->szNewPath);
			}
		}
		pTab = mpcContent->GetDocuments()->FindTab(aszEdited.GetText(i));
		if (pTab.IsNotNull())
		{
			mpcContent->GetDocuments()->SaveFile(pTab);
		}
	}
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
	aszEdited.Kill();
	KillIDEFileRenames(&asRenames);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename's searches for every kind of symbol, as the script's
// renamesearch step sets them.  Not written to the settings.
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::SetSearches(bool bComments, bool bText)
{
	mabComments[0] = bComments;
	mabComments[1] = bComments;
	mabText[0] = bText;
	mabText[1] = bText;
}


//////////////////////////////////////////////////////////////////////////
//
// Rename's part of how the IDE was left, in the settings' file (see
// CIDESettings): its searches, <Rename SearchInComments="false" .../>.
// One the file doesn't say stays as it is.
//
//////////////////////////////////////////////////////////////////////////
void CIDERename::ReadSettings(CMarkupTag* pcSettings)
{
	static const char*	aszAttributes[] = { IDE_RENAME_COMMENTS_ATTRIBUTE, IDE_RENAME_TEXT_ATTRIBUTE, IDE_RENAME_TYPE_COMMENTS_ATTRIBUTE, IDE_RENAME_TYPE_TEXT_ATTRIBUTE };
	bool*				apbValues[] = { &mabComments[0], &mabText[0], &mabComments[1], &mabText[1] };
	CMarkupTag*			pcTag;
	char*				szValue;
	size				i;

	pcTag = pcSettings->GetTag(IDE_RENAME_SETTINGS_TAG);
	if (pcTag == NULL)
	{
		return;
	}
	szValue = pcTag->GetStringAttribute(IDE_RENAME_FILES_ATTRIBUTE);
	if (szValue)
	{
		mbFiles = (StringCompare(szValue, "true") == 0);
	}
	for (i = 0; i < 4; i++)
	{
		szValue = pcTag->GetStringAttribute(aszAttributes[i]);
		if (szValue)
		{
			*apbValues[i] = (StringCompare(szValue, "true") == 0);
		}
	}
}


void CIDERename::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin(IDE_RENAME_SETTINGS_TAG);
	pcWriter->Attribute(IDE_RENAME_COMMENTS_ATTRIBUTE, mabComments[0]);
	pcWriter->Attribute(IDE_RENAME_TEXT_ATTRIBUTE, mabText[0]);
	pcWriter->Attribute(IDE_RENAME_TYPE_COMMENTS_ATTRIBUTE, mabComments[1]);
	pcWriter->Attribute(IDE_RENAME_TYPE_TEXT_ATTRIBUTE, mabText[1]);
	pcWriter->Attribute(IDE_RENAME_FILES_ATTRIBUTE, mbFiles);
	pcWriter->End();
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
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.RenameFile"));
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

	if (IsExplorerFocussed())
	{
		return mpcContent->GetFileRename()->CanRenameExplorerFile();
	}
	sTarget.Init();
	bRenamable = FindRenameTarget(&sTarget) && sTarget.bRenamable;
	sTarget.Kill();
	return bRenamable;
}
