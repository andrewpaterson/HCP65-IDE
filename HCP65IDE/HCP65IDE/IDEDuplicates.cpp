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
#include "BaseLib/Timer.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/XMLWriter.h"
#include "BaseLib/FileUtil.h"
#include "WindowLib/Label.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/TextField.h"
#include "IDEContent.h"
#include "IDEIcons.h"
#include "IDEDuplicates.h"


#define IDE_DUPLICATES_SETTINGS_TAG			"FindDuplicates"
#define IDE_DUPLICATES_SCOPE_ATTRIBUTE		"Scope"
#define IDE_DUPLICATES_DIRECTORY_ATTRIBUTE	"Directory"
#define IDE_DUPLICATES_LITERALS_ATTRIBUTE	"Literals"
#define IDE_DUPLICATES_STATEMENTS_ATTRIBUTE	"Statements"
#define IDE_DUPLICATES_TOKENS_ATTRIBUTE		"Tokens"
#define IDE_DUPLICATES_SLICE				30		//Milliseconds of searching at each window tick.
#define IDE_DUPLICATES_REFRESH				500		//Milliseconds after the last edit that moved a place until the tree shows it.
#define IDE_DUPLICATES_WHAT_LENGTH			60		//The most of an expression the title shows.


//The variables and parameters of a function of a unit searched, found once.
struct SIDEFunctionLocals
{
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CArrayChars						aszNames;
};


typedef CArrayTemplate<SIDEFunctionLocals>	CArrayIDEFunctionLocals;


//Collects the blocks under a node (CIDEDuplicates::AddBlocks and SearchUnit): its own first.
class CIDEBlockFinder : public CCPPSyntaxChildVisitor
{
public:
	CArrayTemplatePtr<CCPPSyntaxCompoundStatement>	mapcBlocks;

	void	Visit(CCPPSyntax* pcChild) override
	{
		Look(pcChild);
	}

	void	Look(CCPPSyntax* pcNode)
	{
		if (pcNode->GetKind() == CPPS_CompoundStatement)
		{
			mapcBlocks.Add((CCPPSyntaxCompoundStatement*)pcNode);
		}
		pcNode->VisitFields(this);
	}
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mpDialog = NULL;
	meScope = IS_Solution;
	mszDirectory.Init();
	mbLiteralsFree = false;
	muiMinimumStatements = IDE_DUPLICATES_MINIMUM_STATEMENTS;
	muiMinimumTokens = IDE_DUPLICATES_MINIMUM_TOKENS;
	mszWhatFile.Init();
	miWhatLine = 0;
	miWhatColumn = 0;
	miWhatEndLine = 0;
	miWhatEndColumn = 0;
	mbSearching = false;
	mbParsed = false;
	meKind = IDK_Function;
	maszFree.Init();
	maszFixed.Init();
	maiRequired.Init();
	masStatements.Init();
	maiBlockStarts.Init();
	muiSelectedFirst = 0;
	muiSelectedCount = 0;
	miFunctionFirst = -1;
	miFunctionLast = -1;
	mszWhat.Init();
	mcScope.Init();
	maszToSearch.Init();
	muiNextFile = 0;
	maszFiles.Init();
	masGroups.Init();
	masPlaces.Init();
	mszFoundTitle.Init();
	mbFound = false;
	mbMoved = false;
	miMoved = 0;
	mpPane = NULL;
	mpTool = NULL;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
	U_Enum(meScope);
	U_String(mszDirectory);
	U_Bool(mbLiteralsFree);
	U_Size(muiMinimumStatements);
	U_Size(muiMinimumTokens);
	U_String(mszWhatFile);
	U_Int32(miWhatLine);
	U_Int32(miWhatColumn);
	U_Int32(miWhatEndLine);
	U_Int32(miWhatEndColumn);
	U_Bool(mbSearching);
	U_Bool(mbParsed);
	U_Enum(meKind);
	U_Unknown(CIDERefactorUnit, mcUnit);
	U_Unknown(CIDEDuplicateMatcher, mcMatcher);
	U_Unknown(CArrayChars, maszFree);
	U_Unknown(CArrayChars, maszFixed);
	U_Unknown(CArrayInt, maiRequired);
	U_Unknown(CArrayIDEDuplicateStatements, masStatements);
	U_Unknown(CArrayInt, maiBlockStarts);
	U_Size(muiSelectedFirst);
	U_Size(muiSelectedCount);
	U_Int32(miFunctionFirst);
	U_Int32(miFunctionLast);
	U_String(mszWhat);
	U_Unknown(CIDEScope, mcScope);
	U_Unknown(CArrayChars, maszToSearch);
	U_Size(muiNextFile);
	U_Unknown(CArrayChars, maszFiles);
	U_Unknown(CArrayIDEDuplicateGroups, masGroups);
	U_Unknown(CArrayIDEDuplicatePlaces, masPlaces);
	U_String(mszFoundTitle);
	U_Bool(mbFound);
	U_Bool(mbMoved);
	U_Int64(miMoved);
	M_Pointer(mpPane);
	M_Pointer(mpTool);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::Free(void)
{
	StopSearch();
	mszFoundTitle.Kill();
	masPlaces.Kill();
	masGroups.Kill();
	maszFiles.Kill();
	maszToSearch.Kill();
	mcScope.Kill();
	mszWhat.Kill();
	maiBlockStarts.Kill();
	masStatements.Kill();
	maiRequired.Kill();
	maszFixed.Kill();
	maszFree.Kill();
	mszWhatFile.Kill();
	mszDirectory.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Find Duplicates (Control+Alt+Shift+D; IntelliJ IDEA's Locate Duplicates
// has no key) on the Edit menu, after Find in Files.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Edit.FindDuplicates", "Find Duplicates...", "Control+Alt+Shift+D", Ptr<CImageCel>(), this, &CIDEDuplicates::QueueFindDuplicates)->SetEnabledCheck(this, &CIDEDuplicates::CanFindDuplicates);
}


void CIDEDuplicates::AddEditItems(Ptr<CMenuContainer> pMenu)
{
	pMenu->AddItem(mpcContent->GetCommands()->Find("Edit.FindDuplicates"));
}


//////////////////////////////////////////////////////////////////////////
//
// The Duplicates tool window: the tree of what was found.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::CreatePane(SDockPlace* psPlace)
{
	mpPane = OMalloc<CIDEResultsPane>(mpcContent, this, "duplicates", true);
	mpPane->AddViewCommands();
	mpTool = mpcContent->GetDock()->AddPane(IDE_DUPLICATES_ID, "Duplicates", NULL, mpPane->GetComponent(), DPK_Tool, psPlace);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the active editor is of a C or C++ file, which Find Duplicates
// can look in.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::CanFindDuplicates(void)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	return pTab.IsNotNull() && (CIDEIcons::GetFileType(pTab->GetFileName()) != IFT_Other) && mpDialog.IsNull();
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog is shown at the next tick: ShowModal can't be called from
// the input (a key or a menu click) that asks for it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::QueueFindDuplicates(void)
{
	if (CanFindDuplicates())
	{
		mpcContent->GetWindow()->Post(this, &CIDEDuplicates::ShowQueuedDialog);
	}
}


void CIDEDuplicates::ShowQueuedDialog(void) { ShowDialog(); }


//////////////////////////////////////////////////////////////////////////
//
// What is looked for is what the active editor has selected, or the
// function its caret is in (TakeWhat); the dialog says which, and asks
// for the scope, as IntelliJ's asks for the analysis scope, and whether
// literals may be others.  Find starts the search.  False if nothing can
// be looked for there (the status line says why) or it was cancelled.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::ShowDialog(void)
{
	Ptr<CComboBox>		pScope;
	Ptr<CTextField>		pDirectory;
	Ptr<CCheckBox>		pLiterals;
	Ptr<CLabel>			pWhat;
	const char*			aszScopes[IS_Count];
	EIDEScope			aeScopes[IS_Count];
	CFileUtil			cFileUtil;
	CChars				szDirectory;
	CChars				szWhyNot;
	size				uiScopes;
	size				i;
	int					iSelected;
	bool				bResult;

	if (!TakeWhat())
	{
		return false;
	}
	StopSearch();
	szWhyNot.Init();
	if (!FindWhat(&szWhyNot))
	{
		szWhyNot.Insert(0, "Find Duplicates: ");
		mpcContent->ShowStatus(szWhyNot.Text());
		szWhyNot.Kill();
		StopSearch();
		return false;
	}
	szWhyNot.Kill();

	uiScopes = 0;
	iSelected = 0;
	for (i = 0; i < IS_Count; i++)
	{
		if (CIDEScope::IsOffered(mpcContent, (EIDEScope)i))
		{
			if ((EIDEScope)i == meScope)
			{
				iSelected = (int)uiScopes;
			}
			aeScopes[uiScopes] = (EIDEScope)i;
			aszScopes[uiScopes] = CIDEScope::GetScopeName((EIDEScope)i);
			uiScopes++;
		}
	}
	szDirectory.Init(mszDirectory);
	if (szDirectory.Empty())
	{
		szDirectory.Set(mszWhatFile);
		cFileUtil.RemoveLastFromPath(&szDirectory);
	}

	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), "Find Duplicates");
	pWhat = OMalloc<CLabel>(mpcContent->GetWindow(), mpcContent->GetDimFont(), mszWhat.Text());
	mpDialog->AddEditor("Of:", pWhat);
	pScope = mpDialog->AddComboBox("Scope:", aszScopes, uiScopes, iSelected);
	pDirectory = mpDialog->AddTextField("Directory:", szDirectory.Text());
	pLiterals = mpDialog->AddCheckBox("Literals may differ", mbLiteralsFree);
	mpDialog->SetOKCaption("Find");
	szDirectory.Kill();

	bResult = false;
	mpcContent->GetScript()->WatchDialog(mpDialog);
	if (mpDialog->Show(520))
	{
		iSelected = pScope->GetSelected();
		SetSettings(((iSelected >= 0) && ((size)iSelected < uiScopes)) ? aeScopes[iSelected] : IS_Solution, pDirectory->GetText(), pLiterals->IsTicked());
		bResult = Start(false, false);
	}
	else
	{
		StopSearch();
	}
	mpcContent->GetScript()->StopWatchingDialog();
	mpDialog = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Where what is looked for is, from the active editor: its selection, or
// its caret.  False if there is no editor.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::TakeWhat(void)
{
	Ptr<CIDEEditorTab>	pTab;
	Ptr<CTextArea>		pEditor;
	size				uiStart;
	size				uiEnd;
	size				uiLine;
	size				uiEndLine;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	if (pTab.IsNull())
	{
		return false;
	}
	pEditor = pTab->GetEditor();
	if (!pEditor->GetSelection(&uiStart, &uiEnd))
	{
		uiStart = pEditor->GetCaretPosition();
		uiEnd = uiStart;
	}
	uiLine = pEditor->FindLine(uiStart);
	uiEndLine = pEditor->FindLine(uiEnd);
	mszWhatFile.Set(pTab->GetFileName());
	miWhatLine = (int32)uiLine;
	miWhatColumn = (int32)(uiStart - pEditor->GetLineStart(uiLine));
	miWhatEndLine = (int32)uiEndLine;
	miWhatEndColumn = (int32)(uiEnd - pEditor->GetLineStart(uiEndLine));
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What is looked for, from where TakeWhat said: the file parsed into
// mcUnit, and the statements (or the expression) with the names they
// must be written with and the function's variables and parameters,
// which they needn't.  pszWhyNot says why not if there is nothing there
// to look for.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::FindWhat(CChars* pszWhyNot)
{
	CCPPSyntax*						pcNode;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	int32							iFirst;
	int32							iLast;

	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	mcUnit.Init();
	mbParsed = true;
	if (!mcUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), mszWhatFile.Text(), false) || !mcUnit.GetTree()->GetRoot())
	{
		pszWhyNot->Set("the file isn't a project's, or can't be parsed");
		return false;
	}
	maszFree.ReInit();
	maszFixed.ReInit();
	maiRequired.ReInit();
	masStatements.ReInit();
	maiBlockStarts.ReInit();
	mszWhat.Clear();

	if ((miWhatLine != miWhatEndLine) || (miWhatColumn != miWhatEndColumn))
	{
		if (!mcUnit.FindTokensIn(miWhatLine, miWhatColumn, miWhatEndLine, miWhatEndColumn, &iFirst, &iLast))
		{
			pszWhyNot->Set("nothing of the file's is selected");
			return false;
		}
		return FindSelected(iFirst, iLast, pszWhyNot);
	}

	pcNode = mcUnit.FindNodeAt(miWhatLine, miWhatColumn);
	pcFunction = pcNode ? (CCPPSyntaxFunctionDefinition*)mcUnit.FindAncestor(pcNode, CPPS_FunctionDefinition) : NULL;
	if (!pcFunction || !pcFunction->mpcBody || (pcFunction->mpcBody->GetKind() != CPPS_CompoundStatement))
	{
		pszWhyNot->Set("select an expression or statements, or put the caret in a function");
		return false;
	}
	return FindFunction(pcFunction, pszWhyNot);
}


//////////////////////////////////////////////////////////////////////////
//
// The selection, the tokens iFirst to iLast: an expression, or whole
// statements of one block (as Extract Method takes them).  The function
// it is in has its variables and parameters, if it is in one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::FindSelected(int32 iFirst, int32 iLast, CChars* pszWhyNot)
{
	CCPPSyntax*						pcStart;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcParent;
	CCPPSyntaxCompoundStatement*	pcBlock;
	CCPPSyntaxCompoundStatement*	pcCandidate;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	SIDEDuplicateStatement*			psStatement;
	SCPPSyntaxRange					sRange;
	CTextFile						cFile;
	CChars							szText;
	const char*						szFileText;
	size							uiLength;
	size							i;
	size							j;

	pcStart = mcUnit.FindNodeOfTokens(iFirst, iFirst);
	pcFunction = pcStart ? (CCPPSyntaxFunctionDefinition*)mcUnit.FindAncestor(pcStart, CPPS_FunctionDefinition) : NULL;
	if (pcFunction)
	{
		FindIDEFunctionLocals(&mcUnit, pcFunction, &maszFree);
		miFunctionFirst = pcFunction->miFirstToken;
		miFunctionLast = pcFunction->miLastToken;
	}
	else
	{
		miFunctionFirst = -1;
		miFunctionLast = -1;
	}

	//An expression.
	pcNode = mcUnit.FindNodeOfTokens(iFirst, iLast);
	while (pcNode && !mcUnit.IsExpression(pcNode) && pcNode->mpcParent && (pcNode->mpcParent->miFirstToken == iFirst) && (pcNode->mpcParent->miLastToken == iLast))
	{
		pcNode = pcNode->mpcParent;
	}
	if (pcNode && mcUnit.IsExpression(pcNode) && (pcNode->miFirstToken == iFirst) && (pcNode->miLastToken == iLast))
	{
		meKind = IDK_Expression;
		psStatement = masStatements.Add();
		psStatement->uiBlock = MAX_SIZE;
		psStatement->uiIndex = 0;
		psStatement->iFirstToken = iFirst;
		psStatement->iLastToken = iLast;
		psStatement->uiStatements = 0;
		AddFixedNames(iFirst, iLast);
		szText.Init();
		cFile.Init();
		szFileText = NULL;
		if (mcUnit.GetTokensRange(iFirst, iLast, &sRange) && mpcContent->GetFindInFiles()->GetFileText(mszWhatFile.Text(), &cFile, &szFileText, &uiLength) && szFileText)
		{
			AppendIDERangeText(szFileText, &sRange, &szText);
		}
		else
		{
			mcUnit.AppendTokens(iFirst, iLast, &szText);
		}
		cFile.Kill();
		szText.Replace('\n', ' ');
		szText.Replace('\t', ' ');
		if (szText.Length() > IDE_DUPLICATES_WHAT_LENGTH)
		{
			szText.SetLength(IDE_DUPLICATES_WHAT_LENGTH);
			szText.Append("...");
		}
		mszWhat.Append('\'')->Append(szText)->Append('\'');
		szText.Kill();
		mcMatcher.Init(&mcUnit, &maszFree, mbLiteralsFree);
		return true;
	}

	//Statements of one block: the block's are block 0.
	pcBlock = NULL;
	for (pcParent = pcStart; pcParent && !pcBlock; pcParent = pcParent->mpcParent)
	{
		if (pcParent->GetKind() != CPPS_CompoundStatement)
		{
			continue;
		}
		pcCandidate = (CCPPSyntaxCompoundStatement*)pcParent;
		for (i = 0; !pcBlock && (i < pcCandidate->macpStatements.NumElements()); i++)
		{
			if (pcCandidate->macpStatements.GetPtr(i)->miFirstToken != iFirst)
			{
				continue;
			}
			for (j = i; !pcBlock && (j < pcCandidate->macpStatements.NumElements()); j++)
			{
				if (pcCandidate->macpStatements.GetPtr(j)->miLastToken == iLast)
				{
					pcBlock = pcCandidate;
					muiSelectedFirst = i;
					muiSelectedCount = j - i + 1;
				}
			}
		}
	}
	if (!pcBlock || !mcUnit.GetTokensRange(iFirst, iLast, &sRange))
	{
		pszWhyNot->Set("the selection isn't an expression, or whole statements of one block");
		return false;
	}
	meKind = IDK_Statements;
	AddBlocks(pcBlock);
	AddFixedNames(iFirst, iLast);
	mszWhat.Append("the statements at line ")->Append(sRange.iLine + 1);
	if (sRange.iEndLine > sRange.iLine)
	{
		mszWhat.Append(" to ")->Append(sRange.iEndLine + 1);
	}
	mcMatcher.Init(&mcUnit, &maszFree, mbLiteralsFree);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The function the caret is in: each of its blocks' statements, its body
// block 0, with its variables and parameters.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::FindFunction(CCPPSyntaxFunctionDefinition* pcFunction, CChars* pszWhyNot)
{
	meKind = IDK_Function;
	FindIDEFunctionLocals(&mcUnit, pcFunction, &maszFree);
	miFunctionFirst = pcFunction->miFirstToken;
	miFunctionLast = pcFunction->miLastToken;
	AddBlocks(pcFunction->mpcBody);
	if (masStatements.NumElements() == 0)
	{
		pszWhyNot->Set("the function has no statements");
		return false;
	}
	AddFixedNames(pcFunction->mpcBody->miFirstToken, pcFunction->mpcBody->miLastToken);
	AppendFunctionName(pcFunction, &mszWhat);
	mcMatcher.Init(&mcUnit, &maszFree, mbLiteralsFree);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The blocks under pcNode (it first, if it is one), each a block of what
// is looked for with its statements.  Only the first is taken for
// selected statements: what is in them is matched as part of them.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AddBlocks(CCPPSyntax* pcNode)
{
	CIDEBlockFinder					cFinder;
	CCPPSyntaxCompoundStatement*	pcBlock;
	CCPPSyntax*						pcStatement;
	SIDEDuplicateStatement*			psStatement;
	size							uiBlocks;
	size							i;
	size							j;

	cFinder.mapcBlocks.Init();
	cFinder.Look(pcNode);
	uiBlocks = (meKind == IDK_Statements) ? 1 : cFinder.mapcBlocks.NumElements();
	for (i = 0; (i < uiBlocks) && (i < cFinder.mapcBlocks.NumElements()); i++)
	{
		pcBlock = cFinder.mapcBlocks.GetPtr(i);
		maiBlockStarts.Add((int)masStatements.NumElements());
		for (j = 0; j < pcBlock->macpStatements.NumElements(); j++)
		{
			pcStatement = pcBlock->macpStatements.GetPtr(j);
			psStatement = masStatements.Add();
			psStatement->uiBlock = maiBlockStarts.NumElements() - 1;
			psStatement->uiIndex = j;
			psStatement->iFirstToken = pcStatement->miFirstToken;
			psStatement->iLastToken = pcStatement->miLastToken;
			psStatement->uiStatements = CountIDEStatements(pcStatement);
		}
	}
	cFinder.mapcBlocks.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The names the tokens iFirst to iLast are written with that aren't the
// function's own, once each: a file that has none of what a statement
// is written with can't have it written again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AddFixedNames(int32 iFirst, int32 iLast)
{
	CIDEDuplicateMatcher	cMatcher;
	SPreprocessedToken*		psSource;
	int32					iToken;

	cMatcher.Init(&mcUnit, &maszFree, false);
	for (iToken = iFirst; iToken <= iLast; iToken++)
	{
		if (cMatcher.IsFixedIdentifier(iToken))
		{
			psSource = mcUnit.GetTokeniser()->GetSource(mcUnit.GetTokeniser()->Get((size)iToken));
			if (psSource && (maszFixed.GetIndex(psSource->szText, psSource->uiLength) == ARRAY_ELEMENT_NOT_FOUND))
			{
				maszFixed.Add(psSource->szText, 0, psSource->uiLength);
			}
		}
	}
	cMatcher.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The function's name as it is written: "CMix::Sum".
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AppendFunctionName(CCPPSyntaxFunctionDefinition* pcFunction, CChars* pszDest)
{
	CCPPSyntaxName*	pcName;

	pcName = pcFunction->mpcDeclarator ? pcFunction->mpcDeclarator->GetName() : NULL;
	if (pcName && (pcName->miFirstToken >= 0))
	{
		mcUnit.AppendTokens(pcName->miFirstToken, pcName->miLastToken, pszDest);
	}
	else
	{
		pszDest->Append("the function");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The search starts: in the scope as the IDE is now, or with bRerun in
// the one it last looked in, and for what TakeWhat last took (FindWhat,
// unless the dialog found it already).  The tool window shows how far it
// has got, then what was found.  With bWait it is searched to the end
// now; otherwise a little at each window tick (Poll).  False if there is
// nothing to look for.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::Start(bool bRerun, bool bWait)
{
	CIDECodeModel*	pcModel;
	CChars			szWhyNot;
	char*			szFile;
	size			i;

	if (mbSearching)
	{
		StopSearch();
	}
	if (!mbParsed)
	{
		szWhyNot.Init();
		if (!FindWhat(&szWhyNot))
		{
			szWhyNot.Insert(0, "Find Duplicates: ");
			mpcContent->ShowStatus(szWhyNot.Text());
			szWhyNot.Kill();
			StopSearch();
			return false;
		}
		szWhyNot.Kill();
	}
	mcMatcher.Init(&mcUnit, &maszFree, mbLiteralsFree);
	FindRequired();
	if (!bRerun)
	{
		mcScope.Capture(mpcContent, meScope, mszDirectory.Text());
	}

	//The scope's files that a project builds, C and C++ ones.
	pcModel = mpcContent->GetCodeModel();
	maszToSearch.ReInit();
	for (i = 0; i < pcModel->NumProjectFiles(); i++)
	{
		szFile = pcModel->GetProjectFile(i);
		if ((CIDEIcons::GetFileType(szFile) != IFT_Other) && mcScope.Contains(mpcContent, szFile))
		{
			maszToSearch.Add(szFile);
		}
	}
	muiNextFile = 0;
	maszFiles.ReInit();
	masGroups.ReInit();
	masPlaces.ReInit();
	mbFound = false;
	mbMoved = false;
	mbSearching = true;

	ShowResults(false);
	if (mpTool.IsNotNull())
	{
		mpcContent->GetDock()->Show(mpTool);
	}
	if (bWait)
	{
		WaitForSearch();
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The search under way goes to its end now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::WaitForSearch(void)
{
	while (mbSearching && SearchSome(MAX_INT64))
	{
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What was being looked for is let go of, and what was parsed for it.
// What was found stays.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::StopSearch(void)
{
	if (mbParsed)
	{
		mcMatcher.Kill();
		mcUnit.Kill();
		mbParsed = false;
	}
	mbSearching = false;
	maszToSearch.ReInit();
	muiNextFile = 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The next of the scope's files are searched, until the time is
// iUntil (at least one is).  Whether there are more.  After the last,
// the tree shows what was found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::SearchSome(int64 iUntil)
{
	do
	{
		if (muiNextFile >= maszToSearch.NumElements())
		{
			FinishSearch();
			return false;
		}
		SearchFile(maszToSearch.GetText(muiNextFile));
		muiNextFile++;
	}
	while (GetDebugTickCount() < iUntil);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The sets of names a file must have all of one of to have something
// looked for written in it: an expression's or the selected statements'
// names; for a function, its whole body's, and for each of its
// statements those of the smallest run from it that is big enough to be
// listed.  Any run listed starts with one of those.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::FindRequired(void)
{
	SIDEDuplicateStatement*	psFirst;
	SIDEDuplicateStatement*	psLast;
	size					uiBlock;
	size					uiCount;
	size					uiStatements;
	size					p;
	size					q;

	maiRequired.ReInit();
	if (meKind == IDK_Expression)
	{
		AddRequired(masStatements.Get(0)->iFirstToken, masStatements.Get(0)->iLastToken);
		return;
	}
	if (meKind == IDK_Statements)
	{
		AddRequired(GetStatement(0, muiSelectedFirst)->iFirstToken, GetStatement(0, muiSelectedFirst + muiSelectedCount - 1)->iLastToken);
		return;
	}

	uiCount = NumBlockStatements(0);
	if (uiCount > 0)
	{
		AddRequired(GetStatement(0, 0)->iFirstToken, GetStatement(0, uiCount - 1)->iLastToken);
	}
	for (uiBlock = 0; uiBlock < maiBlockStarts.NumElements(); uiBlock++)
	{
		uiCount = NumBlockStatements(uiBlock);
		for (p = 0; p < uiCount; p++)
		{
			psFirst = GetStatement(uiBlock, p);
			uiStatements = 0;
			for (q = p; q < uiCount; q++)
			{
				psLast = GetStatement(uiBlock, q);
				uiStatements += psLast->uiStatements;
				if ((uiStatements >= muiMinimumStatements) && ((size)(psLast->iLastToken - psFirst->iFirstToken + 1) >= muiMinimumTokens))
				{
					AddRequired(psFirst->iFirstToken, psLast->iLastToken);
					break;
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A set of names a file may need all of: those the tokens iFirst to
// iLast must be written with (a name may be in it twice).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AddRequired(int32 iFirst, int32 iLast)
{
	SPreprocessedToken*	psSource;
	size				uiName;
	int32				iToken;

	for (iToken = iFirst; iToken <= iLast; iToken++)
	{
		if (mcMatcher.IsFixedIdentifier(iToken))
		{
			psSource = mcUnit.GetTokeniser()->GetSource(mcUnit.GetTokeniser()->Get((size)iToken));
			uiName = psSource ? maszFixed.GetIndex(psSource->szText, psSource->uiLength) : ARRAY_ELEMENT_NOT_FOUND;
			if (uiName != ARRAY_ELEMENT_NOT_FOUND)
			{
				maiRequired.Add((int)uiName);
			}
		}
	}
	maiRequired.Add(-1);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a file's text could have something looked for written in it:
// it has every name of one of the sets (FindRequired), as text, so it may
// only seem to.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::MightBeIn(const char* szText)
{
	CArrayInt	aiHas;
	size		i;
	int			iName;
	bool		bAll;

	aiHas.Init();
	for (i = 0; i < maszFixed.NumElements(); i++)
	{
		aiHas.Add(strstr(szText, maszFixed.GetText(i)) ? 1 : 0);
	}
	bAll = true;
	for (i = 0; i < maiRequired.NumElements(); i++)
	{
		iName = maiRequired.GetValue(i);
		if (iName < 0)
		{
			if (bAll)
			{
				aiHas.Kill();
				return true;
			}
			bAll = true;
		}
		else if (aiHas.GetValue((size)iName) == 0)
		{
			bAll = false;
		}
	}
	aiHas.Kill();
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A file of the scope: its text, as its editor has it if it is open, is
// looked at first, and only if it might have something is it parsed and
// searched.  The file looked from is searched as it was parsed for what
// is looked for.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SearchFile(const char* szFileName)
{
	CIDERefactorUnit	cUnit;
	CTextFile			cFile;
	const char*			szText;
	size				uiLength;
	bool				bMight;

	cFile.Init();
	bMight = mpcContent->GetFindInFiles()->GetFileText(szFileName, &cFile, &szText, &uiLength) && szText && MightBeIn(szText);
	cFile.Kill();
	if (!bMight)
	{
		return;
	}
	if (StringInsensitiveCompare(szFileName, mszWhatFile.Text()) == 0)
	{
		SearchUnit(&mcUnit, szFileName);
		return;
	}
	cUnit.Init();
	if (cUnit.Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), szFileName, false) && cUnit.GetTree()->GetRoot())
	{
		SearchUnit(&cUnit, szFileName);
	}
	cUnit.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A parsed file: its expressions, or each of its blocks (with the
// variables and parameters of the function it is in, which can stand
// for the function looked from's), is compared with what is looked for.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SearchUnit(CIDERefactorUnit* pcUnit, const char* szFileName)
{
	CIDEBlockFinder					cFinder;
	CArrayIDEFunctionLocals			asLocals;
	SIDEFunctionLocals*				psLocals;
	CCPPSyntaxCompoundStatement*	pcBlock;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	size							i;
	size							j;

	if (meKind == IDK_Expression)
	{
		SearchExpressions(pcUnit, szFileName);
		return;
	}

	cFinder.mapcBlocks.Init();
	cFinder.Look(pcUnit->GetTree()->GetRoot());
	asLocals.Init();
	for (i = 0; i < cFinder.mapcBlocks.NumElements(); i++)
	{
		pcBlock = cFinder.mapcBlocks.GetPtr(i);
		if ((pcBlock->macpStatements.NumElements() == 0) || !IsIDETokenOfFile(pcUnit, pcBlock->miFirstToken))
		{
			continue;
		}
		pcFunction = (CCPPSyntaxFunctionDefinition*)pcUnit->FindAncestor(pcBlock, CPPS_FunctionDefinition);
		psLocals = NULL;
		for (j = 0; pcFunction && !psLocals && (j < asLocals.NumElements()); j++)
		{
			if (asLocals.Get(j)->pcFunction == pcFunction)
			{
				psLocals = asLocals.Get(j);
			}
		}
		if (pcFunction && !psLocals)
		{
			psLocals = asLocals.Add();
			psLocals->pcFunction = pcFunction;
			psLocals->aszNames.Init();
			FindIDEFunctionLocals(pcUnit, pcFunction, &psLocals->aszNames);
		}
		SearchBlock(pcUnit, szFileName, pcBlock, psLocals ? &psLocals->aszNames : NULL);
	}
	for (i = 0; i < asLocals.NumElements(); i++)
	{
		asLocals.Get(i)->aszNames.Kill();
	}
	asLocals.Kill();
	cFinder.mapcBlocks.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The expressions of the unit that are as many tokens as the one looked
// for, compared with it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SearchExpressions(CIDERefactorUnit* pcUnit, const char* szFileName)
{
	CIDEDuplicatePlaceFinder		cFinder;
	CCPPSyntax*						pcNode;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	SIDEDuplicateStatement*			psExpression;
	CArrayInt						aiFirsts;
	CArrayChars						aszThere;
	CArrayChars						aszNames;
	size							i;
	int32							iTokens;
	int32							iAt;

	psExpression = masStatements.Get(0);
	iTokens = psExpression->iLastToken - psExpression->iFirstToken + 1;
	aiFirsts.Init();
	cFinder.mpcUnit = pcUnit;
	cFinder.mpaiFirsts = &aiFirsts;
	cFinder.miTokens = iTokens;
	cFinder.mbExpression = true;
	pcUnit->GetTree()->GetRoot()->VisitFields(&cFinder);
	aszNames.Init();
	for (i = 0; i < aiFirsts.NumElements(); i++)
	{
		iAt = aiFirsts.GetValue(i);
		if (!IsIDETokenOfFile(pcUnit, iAt) || IsOwnPlace(pcUnit, iAt, iAt + iTokens - 1, psExpression->iFirstToken, psExpression->iLastToken))
		{
			continue;
		}
		pcNode = pcUnit->FindNodeOfTokens(iAt, iAt);
		pcFunction = pcNode ? (CCPPSyntaxFunctionDefinition*)pcUnit->FindAncestor(pcNode, CPPS_FunctionDefinition) : NULL;
		aszThere.Init();
		if (pcFunction)
		{
			FindIDEFunctionLocals(pcUnit, pcFunction, &aszThere);
		}
		mcMatcher.StartNames(&aszNames);
		if (mcMatcher.Match(psExpression->iFirstToken, iTokens, pcUnit, iAt, &aszThere, &aszNames))
		{
			AddPlace(pcUnit, szFileName, iAt, iAt + iTokens - 1, true, MAX_SIZE, 0, 1);
		}
		aszThere.Kill();
	}
	aszNames.Kill();
	aiFirsts.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A block of the unit, compared with each block of what is looked for.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SearchBlock(CIDERefactorUnit* pcUnit, const char* szFileName, CCPPSyntaxCompoundStatement* pcBlock, CArrayChars* paszFreeThere)
{
	size	uiBlock;

	for (uiBlock = 0; uiBlock < maiBlockStarts.NumElements(); uiBlock++)
	{
		SearchRuns(pcUnit, szFileName, pcBlock, paszFreeThere, uiBlock, (size)maiBlockStarts.GetValue(uiBlock), NumBlockStatements(uiBlock));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The runs of statements of block uiBlock (uiCount of them from
// masStatements' uiFirst) that pcBlock's statements are, from each of
// its statements on.  Selected statements are found only whole.  A
// function's are found from each of its statements, as many as are the
// same: a run already found isn't found again from its second statement,
// a place inside the function looked from is found only after the run it
// is the same as (not both ways round), and a run smaller than the
// minimums isn't listed, unless it is the function's whole body, written
// as the whole body of another.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SearchRuns(CIDERefactorUnit* pcUnit, const char* szFileName, CCPPSyntaxCompoundStatement* pcBlock, CArrayChars* paszFreeThere, size uiBlock, size uiFirst, size uiCount)
{
	uint8*					puiCovered;
	SIDEDuplicateStatement*	psFirst;
	SIDEDuplicateStatement*	psLast;
	size					uiThere;
	size					uiStatements;
	size					uiTokens;
	size					uiRun;
	size					p;
	size					c;
	size					k;
	int32					iAtFirst;
	int32					iAtLast;
	bool					bWhole;

	uiThere = pcBlock->macpStatements.NumElements();
	if (meKind == IDK_Statements)
	{
		for (c = 0; c < uiThere; c++)
		{
			uiRun = MatchRun(pcUnit, pcBlock, c, paszFreeThere, uiFirst + muiSelectedFirst, muiSelectedCount);
			if (uiRun == muiSelectedCount)
			{
				psFirst = masStatements.Get(uiFirst + muiSelectedFirst);
				psLast = masStatements.Get(uiFirst + muiSelectedFirst + uiRun - 1);
				iAtFirst = pcBlock->macpStatements.GetPtr(c)->miFirstToken;
				iAtLast = pcBlock->macpStatements.GetPtr(c + uiRun - 1)->miLastToken;
				if (!IsOwnPlace(pcUnit, iAtFirst, iAtLast, psFirst->iFirstToken, psLast->iLastToken))
				{
					AddPlace(pcUnit, szFileName, iAtFirst, iAtLast, true, uiBlock, muiSelectedFirst, uiRun);
				}
			}
		}
		return;
	}

	puiCovered = (uint8*)calloc(uiCount * uiThere + 1, 1);
	for (p = 0; p < uiCount; p++)
	{
		for (c = 0; c < uiThere; c++)
		{
			if (puiCovered[p * uiThere + c])
			{
				continue;
			}
			uiRun = MatchRun(pcUnit, pcBlock, c, paszFreeThere, uiFirst + p, uiCount - p);
			for (k = 0; k < uiRun; k++)
			{
				puiCovered[(p + k) * uiThere + c + k] = 1;
			}
			if (uiRun == 0)
			{
				continue;
			}
			psFirst = masStatements.Get(uiFirst + p);
			psLast = masStatements.Get(uiFirst + p + uiRun - 1);
			iAtFirst = pcBlock->macpStatements.GetPtr(c)->miFirstToken;
			iAtLast = pcBlock->macpStatements.GetPtr(c + uiRun - 1)->miLastToken;
			if (IsOwnPlace(pcUnit, iAtFirst, iAtLast, psFirst->iFirstToken, psLast->iLastToken) ||
				((pcUnit == &mcUnit) && (iAtFirst >= miFunctionFirst) && (iAtFirst <= miFunctionLast) && (iAtFirst < psFirst->iFirstToken)))
			{
				continue;
			}
			bWhole = (uiBlock == 0) && (p == 0) && (uiRun == uiCount) && (c == 0) && (uiThere == uiRun) && pcBlock->mpcParent && (pcBlock->mpcParent->GetKind() == CPPS_FunctionDefinition);
			uiStatements = 0;
			for (k = 0; k < uiRun; k++)
			{
				uiStatements += masStatements.Get(uiFirst + p + k)->uiStatements;
			}
			uiTokens = (size)(psLast->iLastToken - psFirst->iFirstToken + 1);
			if (bWhole || ((uiStatements >= muiMinimumStatements) && (uiTokens >= muiMinimumTokens)))
			{
				AddPlace(pcUnit, szFileName, iAtFirst, iAtLast, bWhole, uiBlock, p, uiRun);
			}
		}
	}
	free(puiCovered);
}


//////////////////////////////////////////////////////////////////////////
//
// How many of pcBlock's statements from uiAt are, one after another, the
// statements looked for from masStatements' uiFirst, at most uiMost:
// sharing the names they give the function's variables and parameters.
//
//////////////////////////////////////////////////////////////////////////
size CIDEDuplicates::MatchRun(CIDERefactorUnit* pcUnit, CCPPSyntaxCompoundStatement* pcBlock, size uiAt, CArrayChars* paszFreeThere, size uiFirst, size uiMost)
{
	CArrayChars				aszNames;
	CCPPSyntax*				pcThere;
	SIDEDuplicateStatement*	psStatement;
	size					uiRun;
	int32					iTokens;

	aszNames.Init();
	mcMatcher.StartNames(&aszNames);
	for (uiRun = 0; (uiRun < uiMost) && (uiAt + uiRun < pcBlock->macpStatements.NumElements()); uiRun++)
	{
		psStatement = masStatements.Get(uiFirst + uiRun);
		pcThere = pcBlock->macpStatements.GetPtr(uiAt + uiRun);
		iTokens = psStatement->iLastToken - psStatement->iFirstToken + 1;
		if ((pcThere->miFirstToken < 0) || (pcThere->miLastToken - pcThere->miFirstToken + 1 != iTokens) ||
			!mcMatcher.Match(psStatement->iFirstToken, iTokens, pcUnit, pcThere->miFirstToken, paszFreeThere ? paszFreeThere : &maszFixed, &aszNames))
		{
			break;
		}
	}
	aszNames.Kill();
	return uiRun;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the tokens iFirst to iLast of pcUnit are (or overlap) the
// tokens iPatternFirst to iPatternLast of what is looked for itself.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::IsOwnPlace(CIDERefactorUnit* pcUnit, int32 iFirst, int32 iLast, int32 iPatternFirst, int32 iPatternLast)
{
	return (pcUnit == &mcUnit) && (iFirst <= iPatternLast) && (iLast >= iPatternFirst);
}


//////////////////////////////////////////////////////////////////////////
//
// A place found, the tokens iFirst to iLast of pcUnit (the file
// szFileName's own), of the group of the whole of what is looked for or
// of the run (uiCount statements from uiFirst of block uiBlock).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AddPlace(CIDERefactorUnit* pcUnit, const char* szFileName, int32 iFirst, int32 iLast, bool bWhole, size uiBlock, size uiFirst, size uiCount)
{
	SIDEDuplicateGroup*		psGroup;
	SIDEDuplicatePlace*		psPlace;
	SIDEDuplicateStatement*	psFirst;
	SIDEDuplicateStatement*	psLast;
	SCPPSyntaxRange			sRange;
	SCPPSyntaxRange			sPattern;
	size					uiGroup;
	size					uiFile;
	size					i;

	if (!pcUnit->GetTokensRange(iFirst, iLast, &sRange))
	{
		return;
	}
	for (uiGroup = 0; uiGroup < masGroups.NumElements(); uiGroup++)
	{
		psGroup = masGroups.Get(uiGroup);
		if ((psGroup->bWhole == bWhole) && (psGroup->uiBlock == uiBlock) && (psGroup->uiFirst == uiFirst) && (psGroup->uiCount == uiCount))
		{
			break;
		}
	}
	if (uiGroup == masGroups.NumElements())
	{
		psFirst = (uiBlock == MAX_SIZE) ? masStatements.Get(0) : GetStatement(uiBlock, uiFirst);
		psLast = (uiBlock == MAX_SIZE) ? psFirst : GetStatement(uiBlock, uiFirst + uiCount - 1);
		psGroup = masGroups.Add();
		psGroup->bWhole = bWhole;
		psGroup->uiBlock = uiBlock;
		psGroup->uiFirst = uiFirst;
		psGroup->uiCount = uiCount;
		psGroup->iFirstToken = psFirst->iFirstToken;
		psGroup->iLastToken = psLast->iLastToken;
		psGroup->uiTokens = (size)(psLast->iLastToken - psFirst->iFirstToken + 1);
		psGroup->uiStatements = 0;
		for (i = 0; (uiBlock != MAX_SIZE) && (i < uiCount); i++)
		{
			psGroup->uiStatements += GetStatement(uiBlock, uiFirst + i)->uiStatements;
		}
		psGroup->iLine = 0;
		psGroup->iEndLine = 0;
		if (mcUnit.GetTokensRange(psGroup->iFirstToken, psGroup->iLastToken, &sPattern))
		{
			psGroup->iLine = sPattern.iLine;
			psGroup->iEndLine = sPattern.iEndLine;
		}
		psGroup->uiPlaces = 0;
	}

	uiFile = maszFiles.GetIndex(szFileName);
	if (uiFile == ARRAY_ELEMENT_NOT_FOUND)
	{
		uiFile = maszFiles.NumElements();
		maszFiles.Add(szFileName);
	}
	psPlace = masPlaces.Add();
	psPlace->uiGroup = uiGroup;
	psPlace->uiFile = uiFile;
	psPlace->iLine = sRange.iLine;
	psPlace->iColumn = sRange.iColumn;
	psPlace->iEndLine = sRange.iEndLine;
	psPlace->iEndColumn = sRange.iEndColumn;
}


//////////////////////////////////////////////////////////////////////////
//
// The search has got to its end: what was found is put in order, and
// shown.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::FinishSearch(void)
{
	CChars	szStatus;
	size	uiGroups;

	RemoveContainedPlaces();
	SortPlaces();

	mszFoundTitle.Set("Duplicates of ");
	mszFoundTitle.Append(mszWhat);
	mszFoundTitle.Append(' ');
	mcScope.AppendName(mpcContent, &mszFoundTitle);
	mszFoundTitle.Append("  ");
	mszFoundTitle.Append((uint32)masPlaces.NumElements());
	mszFoundTitle.Append((masPlaces.NumElements() == 1) ? " place" : " places");
	uiGroups = masGroups.NumElements();
	if (meKind == IDK_Function)
	{
		mszFoundTitle.Append(", ")->Append((uint32)uiGroups)->Append((uiGroups == 1) ? " part" : " parts");
	}

	StopSearch();
	mbFound = true;
	ShowResults(false);
	szStatus.Init("Find Duplicates: ");
	if (masPlaces.NumElements() == 0)
	{
		szStatus.Append("no duplicates found.");
	}
	else
	{
		szStatus.Append((uint32)masPlaces.NumElements())->Append((masPlaces.NumElements() == 1) ? " place" : " places")->Append(" found.");
	}
	mpcContent->ShowStatus(szStatus.Text());
	szStatus.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A place inside another of the same file, of a group that is (or is
// in) the other's, is the other's part written again: it isn't listed.
// The groups left with no places go.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::RemoveContainedPlaces(void)
{
	SIDEDuplicatePlace*	psPlace;
	SIDEDuplicatePlace*	psOther;
	SIDEDuplicateGroup*	psGroup;
	SIDEDuplicateGroup*	psOtherGroup;
	CArrayInt			aiNewGroups;
	size				i;
	size				j;
	bool				bInside;

	for (i = masPlaces.NumElements(); i > 0; i--)
	{
		psPlace = masPlaces.Get(i - 1);
		psGroup = masGroups.Get(psPlace->uiGroup);
		bInside = false;
		for (j = 0; !bInside && (j < masPlaces.NumElements()); j++)
		{
			psOther = masPlaces.Get(j);
			if ((j == i - 1) || (psOther->uiFile != psPlace->uiFile) || (psOther->uiGroup == psPlace->uiGroup))
			{
				continue;
			}
			psOtherGroup = masGroups.Get(psOther->uiGroup);
			bInside = (psOtherGroup->iFirstToken <= psGroup->iFirstToken) && (psOtherGroup->iLastToken >= psGroup->iLastToken) &&
					  ((psOther->iLine < psPlace->iLine) || ((psOther->iLine == psPlace->iLine) && (psOther->iColumn <= psPlace->iColumn))) &&
					  ((psOther->iEndLine > psPlace->iEndLine) || ((psOther->iEndLine == psPlace->iEndLine) && (psOther->iEndColumn >= psPlace->iEndColumn)));
		}
		if (bInside)
		{
			masPlaces.RemoveAt(i - 1);
		}
	}

	for (i = 0; i < masGroups.NumElements(); i++)
	{
		masGroups.Get(i)->uiPlaces = 0;
	}
	for (i = 0; i < masPlaces.NumElements(); i++)
	{
		masGroups.Get(masPlaces.Get(i)->uiGroup)->uiPlaces++;
	}
	aiNewGroups.Init();
	for (i = 0, j = 0; i < masGroups.NumElements(); i++)
	{
		aiNewGroups.Add((masGroups.Get(i)->uiPlaces > 0) ? (int)j++ : -1);
	}
	for (i = masGroups.NumElements(); i > 0; i--)
	{
		if (masGroups.Get(i - 1)->uiPlaces == 0)
		{
			masGroups.RemoveAt(i - 1);
		}
	}
	for (i = 0; i < masPlaces.NumElements(); i++)
	{
		psPlace = masPlaces.Get(i);
		psPlace->uiGroup = (size)aiNewGroups.GetValue(psPlace->uiGroup);
	}
	aiNewGroups.Kill();
}


//The order of the groups: the whole first, then the biggest, then as they are in the function.
static int CompareIDEDuplicateGroups(const void* pv1, const void* pv2)
{
	SIDEDuplicateGroup*	ps1;
	SIDEDuplicateGroup*	ps2;

	ps1 = (SIDEDuplicateGroup*)pv1;
	ps2 = (SIDEDuplicateGroup*)pv2;
	if (ps1->bWhole != ps2->bWhole)
	{
		return ps1->bWhole ? -1 : 1;
	}
	if (ps1->uiTokens != ps2->uiTokens)
	{
		return (ps1->uiTokens > ps2->uiTokens) ? -1 : 1;
	}
	return (ps1->iFirstToken < ps2->iFirstToken) ? -1 : ((ps1->iFirstToken > ps2->iFirstToken) ? 1 : 0);
}


//The order of the places: by group, then file, then where in it.
static int CompareIDEDuplicatePlaces(const void* pv1, const void* pv2)
{
	SIDEDuplicatePlace*	ps1;
	SIDEDuplicatePlace*	ps2;

	ps1 = (SIDEDuplicatePlace*)pv1;
	ps2 = (SIDEDuplicatePlace*)pv2;
	if (ps1->uiGroup != ps2->uiGroup)
	{
		return (ps1->uiGroup < ps2->uiGroup) ? -1 : 1;
	}
	if (ps1->uiFile != ps2->uiFile)
	{
		return (ps1->uiFile < ps2->uiFile) ? -1 : 1;
	}
	if (ps1->iLine != ps2->iLine)
	{
		return (ps1->iLine < ps2->iLine) ? -1 : 1;
	}
	return (ps1->iColumn < ps2->iColumn) ? -1 : ((ps1->iColumn > ps2->iColumn) ? 1 : 0);
}


//////////////////////////////////////////////////////////////////////////
//
// The groups in their order, the places' group numbers following them,
// and the places in theirs.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SortPlaces(void)
{
	CArrayIDEDuplicateGroups	asSorted;
	CArrayInt					aiNewGroups;
	SIDEDuplicatePlace*			psPlace;
	size						i;
	size						j;

	asSorted.Init();
	asSorted.Copy(&masGroups);
	asSorted.QuickSort(CompareIDEDuplicateGroups);
	aiNewGroups.Init();
	for (i = 0; i < masGroups.NumElements(); i++)
	{
		for (j = 0; j < asSorted.NumElements(); j++)
		{
			if (memcmp(masGroups.Get(i), asSorted.Get(j), sizeof(SIDEDuplicateGroup)) == 0)
			{
				break;
			}
		}
		aiNewGroups.Add((int)j);
	}
	for (i = 0; i < masPlaces.NumElements(); i++)
	{
		psPlace = masPlaces.Get(i);
		psPlace->uiGroup = (size)aiNewGroups.GetValue(psPlace->uiGroup);
	}
	aiNewGroups.Kill();
	masGroups.ReInit();
	masGroups.Copy(&asSorted);
	asSorted.Kill();
	masPlaces.QuickSort(CompareIDEDuplicatePlaces);
}


//////////////////////////////////////////////////////////////////////////
//
// What a group is, as its tree row says: "Lines 12 to 15, 4 statements,
// 37 tokens".
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AppendGroupName(SIDEDuplicateGroup* psGroup, CChars* pszDest)
{
	if (psGroup->bWhole && (meKind == IDK_Function))
	{
		pszDest->Append("The whole body");
	}
	else if (psGroup->iEndLine > psGroup->iLine)
	{
		pszDest->Append("Lines ")->Append(psGroup->iLine + 1)->Append(" to ")->Append(psGroup->iEndLine + 1);
	}
	else
	{
		pszDest->Append("Line ")->Append(psGroup->iLine + 1);
	}
	if (psGroup->uiStatements > 0)
	{
		pszDest->Append(", ")->Append((uint32)psGroup->uiStatements)->Append((psGroup->uiStatements == 1) ? " statement" : " statements");
	}
	pszDest->Append(", ")->Append((uint32)psGroup->uiTokens)->Append((psGroup->uiTokens == 1) ? " token" : " tokens");
}


//////////////////////////////////////////////////////////////////////////
//
// The tree: under what was looked for (with how far the search has got,
// while it goes on), each part of it written again (a function's, not
// for a selection, which is the one part), under it each file, and under
// each file each place's first line, as it is now.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::ShowResults(bool bKeepSelected)
{
	Ptr<CTreeContainer>	pTree;
	CIDEFileLines		cLines;
	SIDEDuplicatePlace*	psPlace;
	SIDEDuplicateGroup*	psGroup;
	CChars				szText;
	CChars				szTitle;
	const char*			szFile;
	size				uiCount;
	size				uiEnd;
	size				i;
	size				j;
	int64				iSelectedData;
	int32				iRoot;
	int32				iGroup;
	int32				iFile;

	if (mpPane.IsNull())
	{
		return;
	}
	pTree = mpPane->GetTree();
	iSelectedData = mpPane->BeginShow(bKeepSelected);
	if (mbSearching)
	{
		szTitle.Init("Duplicates of ");
		szTitle.Append(mszWhat);
		szTitle.Append(": searching, ")->Append((uint32)muiNextFile)->Append(" of ")->Append((uint32)maszToSearch.NumElements())->Append(" files");
		pTree->AddNode(TREE_NO_NODE, szTitle.Text(), NULL, 0);
		szTitle.Kill();
		mpPane->EndShow(iSelectedData);
		return;
	}
	if (!mbFound)
	{
		mpPane->EndShow(iSelectedData);
		return;
	}

	iRoot = pTree->AddNode(TREE_NO_NODE, mszFoundTitle.Text(), NULL, 0);
	pTree->SetExpanded(iRoot, true);
	cLines.Init(&mpcContent->GetDocuments());
	for (i = 0; i < masPlaces.NumElements(); i = uiEnd)
	{
		psGroup = masGroups.Get(masPlaces.Get(i)->uiGroup);
		for (uiEnd = i + 1; (uiEnd < masPlaces.NumElements()) && (masPlaces.Get(uiEnd)->uiGroup == masPlaces.Get(i)->uiGroup); uiEnd++)
		{
		}
		iGroup = iRoot;
		if (meKind == IDK_Function)
		{
			szTitle.Init();
			AppendGroupName(psGroup, &szTitle);
			szText.Init("  ");
			szText.Append((uint32)(uiEnd - i))->Append((uiEnd - i == 1) ? " place" : " places");
			iGroup = mpPane->AddGroup(iRoot, szTitle.Text(), szText.Text());
			pTree->SetExpanded(iGroup, true);
			szText.Kill();
			szTitle.Kill();
		}
		for (j = i; j < uiEnd; j += uiCount)
		{
			psPlace = masPlaces.Get(j);
			for (uiCount = 1; (j + uiCount < uiEnd) && (masPlaces.Get(j + uiCount)->uiFile == psPlace->uiFile); uiCount++)
			{
			}
			szFile = maszFiles.GetText(psPlace->uiFile);
			szText.Init("  ");
			szText.Append((uint32)uiCount)->Append((uiCount == 1) ? " place" : " places");
			iFile = mpPane->AddGroup(iGroup, GetIDEFileNamePart(szFile), szText.Text(), szFile);
			pTree->SetExpanded(iFile, true);
			szText.Kill();
			for (; uiCount > 0; uiCount--, j++)
			{
				psPlace = masPlaces.Get(j);
				mpPane->AddResult(iFile, &cLines, szFile, psPlace->iLine, psPlace->iColumn, -1, NULL, (int64)j + 1);
			}
			uiCount = 0;
		}
	}
	cLines.Kill();
	mpPane->EndShow(iSelectedData);
}


//////////////////////////////////////////////////////////////////////////
//
// The results pane's source: the place a node's data is of (its index in
// masPlaces, and one), all of it selected.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest)
{
	SIDEDuplicatePlace*	psPlace;

	if ((iData <= 0) || ((size)iData > masPlaces.NumElements()))
	{
		return false;
	}
	psPlace = masPlaces.Get((size)iData - 1);
	memset(psDest, 0, sizeof(SIDELocation));
	psDest->szFileName.Init(maszFiles.GetText(psPlace->uiFile));
	psDest->iLine = psPlace->iLine;
	psDest->iColumn = psPlace->iColumn;
	psDest->iLength = (psPlace->iEndLine == psPlace->iLine) ? (psPlace->iEndColumn - psPlace->iColumn) : 0;
	psDest->iContainer = -1;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The results pane's Rerun: what was last looked for is looked for again,
// where it was, in the scope as it was then.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::CanRerunResults(CIDEResultsPane* pcPane)
{
	return mbFound && !mszWhatFile.Empty() && mpDialog.IsNull() && !mbSearching;
}


void CIDEDuplicates::RerunResults(CIDEResultsPane* pcPane)
{
	if (CanRerunResults(pcPane))
	{
		Start(true, false);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Next or Previous Occurrence (CIDEOccurrences) steps through the tree's
// places (CIDEResultsPane::Step).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicates::Step(bool bNext, bool bKeepKeys)
{
	return mpPane->Step(bNext, bKeepKeys);
}


//////////////////////////////////////////////////////////////////////////
//
// The dialog's settings, kept for next time: where it looks and whether
// literals may be others.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SetSettings(EIDEScope eScope, const char* szDirectory, bool bLiteralsFree)
{
	meScope = eScope;
	mszDirectory.Set(szDirectory ? szDirectory : "");
	mbLiteralsFree = bLiteralsFree;
	mpcContent->GetSettings()->WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// The Settings page's: the smallest run of a function's statements that
// is listed (at least one statement and one token).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::SetMinimums(size uiStatements, size uiTokens)
{
	muiMinimumStatements = (uiStatements > 0) ? uiStatements : 1;
	muiMinimumTokens = (uiTokens > 0) ? uiTokens : 1;
	mpcContent->GetSettings()->WriteSettings();
}


//////////////////////////////////////////////////////////////////////////
//
// An edit of the open file szFile (CIDEDocuments::DocumentChanged): the
// places in it move with their text.  The tree's lines say where they
// were until Poll shows them again.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange)
{
	SIDEDuplicatePlace*	psPlace;
	size				uiFile;
	size				i;
	bool				bStart;
	bool				bEnd;

	if (!mbFound)
	{
		return;
	}
	uiFile = maszFiles.GetIndex(szFile);
	if (uiFile == ARRAY_ELEMENT_NOT_FOUND)
	{
		return;
	}
	for (i = 0; i < masPlaces.NumElements(); i++)
	{
		psPlace = masPlaces.Get(i);
		if (psPlace->uiFile != uiFile)
		{
			continue;
		}
		bStart = AdjustIDEPlace(&psPlace->iLine, &psPlace->iColumn, psChange);
		bEnd = AdjustIDEPlace(&psPlace->iEndLine, &psPlace->iEndColumn, psChange);
		if (bStart || bEnd)
		{
			mbMoved = true;
			miMoved = GetDebugTickCount();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Each window tick: the search goes on for a while (the tree saying how
// far it has got); or, once the edits that moved places have stopped for
// IDE_DUPLICATES_REFRESH, the tree is shown again with the lines as they
// are now.  Milliseconds until it should be looked at again, -1 if not
// until something changes.
//
//////////////////////////////////////////////////////////////////////////
int64 CIDEDuplicates::Poll(int64 iNow)
{
	if (mbSearching)
	{
		if (SearchSome(iNow + IDE_DUPLICATES_SLICE))
		{
			ShowResults(false);
			return 0;
		}
		return -1;
	}
	if (!mbMoved)
	{
		return -1;
	}
	if (iNow - miMoved < IDE_DUPLICATES_REFRESH)
	{
		return IDE_DUPLICATES_REFRESH - (iNow - miMoved);
	}
	mbMoved = false;
	ShowResults(true);
	if (mpPane.IsNotNull())
	{
		mpPane->ShowSelected();
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
//
// The solutions have gone: so has what was found in them, and a search
// under way stops.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::Clear(void)
{
	StopSearch();
	maszFiles.ReInit();
	masGroups.ReInit();
	masPlaces.ReInit();
	mbFound = false;
	mbMoved = false;
	if (mpPane.IsNotNull())
	{
		mpPane->Clear();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What is kept of Find Duplicates in the IDE's settings: the dialog's
// scope, directory and tick, the Settings page's minimums, and the tool
// window's preview and share (the results pane's).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicates::ReadSettings(CMarkupTag* pcSettings)
{
	CMarkupTag*	pcTag;
	char*		szValue;

	pcTag = pcSettings->GetTag(IDE_DUPLICATES_SETTINGS_TAG);
	if (pcTag == NULL)
	{
		return;
	}
	CIDEScope::FindScope(pcTag->GetStringAttribute(IDE_DUPLICATES_SCOPE_ATTRIBUTE), &meScope);
	szValue = pcTag->GetStringAttribute(IDE_DUPLICATES_DIRECTORY_ATTRIBUTE);
	if (szValue)
	{
		mszDirectory.Set(szValue);
	}
	szValue = pcTag->GetStringAttribute(IDE_DUPLICATES_LITERALS_ATTRIBUTE);
	mbLiteralsFree = szValue && (StringCompare(szValue, "true") == 0);
	szValue = pcTag->GetStringAttribute(IDE_DUPLICATES_STATEMENTS_ATTRIBUTE);
	if (szValue && (atoi(szValue) > 0))
	{
		muiMinimumStatements = (size)atoi(szValue);
	}
	szValue = pcTag->GetStringAttribute(IDE_DUPLICATES_TOKENS_ATTRIBUTE);
	if (szValue && (atoi(szValue) > 0))
	{
		muiMinimumTokens = (size)atoi(szValue);
	}
	if (mpPane.IsNotNull())
	{
		mpPane->ReadSettings(pcTag);
	}
}


void CIDEDuplicates::WriteSettings(CXMLWriter* pcWriter)
{
	pcWriter->Begin(IDE_DUPLICATES_SETTINGS_TAG);
	pcWriter->Attribute(IDE_DUPLICATES_SCOPE_ATTRIBUTE, CIDEScope::GetScopeName(meScope));
	pcWriter->Attribute(IDE_DUPLICATES_DIRECTORY_ATTRIBUTE, mszDirectory.Text());
	pcWriter->Attribute(IDE_DUPLICATES_LITERALS_ATTRIBUTE, mbLiteralsFree);
	pcWriter->Attribute(IDE_DUPLICATES_STATEMENTS_ATTRIBUTE, (int32)muiMinimumStatements);
	pcWriter->Attribute(IDE_DUPLICATES_TOKENS_ATTRIBUTE, (int32)muiMinimumTokens);
	if (mpPane.IsNotNull())
	{
		mpPane->WriteSettings(pcWriter);
	}
	pcWriter->End();
}


//////////////////////////////////////////////////////////////////////////
//
// Statement uiIndex of block uiBlock of what is looked for.
//
//////////////////////////////////////////////////////////////////////////
SIDEDuplicateStatement* CIDEDuplicates::GetStatement(size uiBlock, size uiIndex)
{
	return masStatements.Get((size)maiBlockStarts.GetValue(uiBlock) + uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
// How many statements block uiBlock of what is looked for has.
//
//////////////////////////////////////////////////////////////////////////
size CIDEDuplicates::NumBlockStatements(size uiBlock)
{
	size	uiEnd;

	uiEnd = (uiBlock + 1 < maiBlockStarts.NumElements()) ? (size)maiBlockStarts.GetValue(uiBlock + 1) : masStatements.NumElements();
	return uiEnd - (size)maiBlockStarts.GetValue(uiBlock);
}


bool CIDEDuplicates::Save(CObjectWriter* pcFile) { return false; }
bool CIDEDuplicates::Load(CObjectReader* pcFile) { return false; }
bool CIDEDuplicates::IsSearching(void) const { return mbSearching; }
EIDEScope CIDEDuplicates::GetScope(void) const { return meScope; }
char* CIDEDuplicates::GetDirectory(void) { return mszDirectory.Text(); }
bool CIDEDuplicates::IsLiteralsFree(void) const { return mbLiteralsFree; }
size CIDEDuplicates::GetMinimumStatements(void) const { return muiMinimumStatements; }
size CIDEDuplicates::GetMinimumTokens(void) const { return muiMinimumTokens; }
Ptr<CTreeContainer> CIDEDuplicates::GetTree(void) { return mpPane.IsNotNull() ? mpPane->GetTree() : Ptr<CTreeContainer>(); }
Ptr<CIDEResultsPane> CIDEDuplicates::GetResultsPane(void) const { return mpPane; }
Ptr<CDockPane> CIDEDuplicates::GetTool(void) const { return mpTool; }
size CIDEDuplicates::NumPlaces(void) const { return masPlaces.NumElements(); }
Ptr<CDockPane> CIDEDuplicates::GetSteppablePane(void) const { return mpTool; }
const char* CIDEDuplicates::GetSteppableName(void) { return "Duplicates"; }
bool CIDEDuplicates::HasSteps(void) { return mbFound && (masPlaces.NumElements() > 0) && mpPane.IsNotNull(); }

