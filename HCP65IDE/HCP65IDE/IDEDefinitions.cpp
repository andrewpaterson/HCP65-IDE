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
#include <stdlib.h>
#include <string.h>
#include "BaseLib/ConstructorCall.h"
#include "BaseLib/FileUtil.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDEFileRename.h"
#include "IDESafeDelete.h"
#include "IDEIntroduceVariable.h"
#include "IDEDefinitions.h"
#include "IDEContextActions.h"


#define IDE_DEFINITION_BANNER	"//////////////////////////////////////////////////////////////////////////\n//\n//\n//////////////////////////////////////////////////////////////////////////\n"
#define IDE_DEFINITION_ONE_LINE	60	//A body of one statement this long or shorter stays on its declaration's line in a class.

#define IDE_PLACE_AFTER		0	//After the definition of the function declared before it.
#define IDE_PLACE_BEFORE	1	//Before the definition of one declared after it, or before a header's #endif.
#define IDE_PLACE_END		2	//At the end of the file.

#define IDE_FILES_ANY		0	//Where the functions declared with it are defined.
#define IDE_FILES_HEADER	1	//In the file it is declared in.
#define IDE_FILES_SOURCE	2	//Not in the file it is declared in: in that header's source file.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEHeaderFile(const char* szFileName)
{
	CChars	szName;
	bool	bHeader;

	szName.Init(szFileName);
	bHeader = szName.EndsWithIgnoreCase(".h") || szName.EndsWithIgnoreCase(".hpp") || szName.EndsWithIgnoreCase(".hh") || szName.EndsWithIgnoreCase(".inl");
	szName.Kill();
	return bHeader;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFunction::Init(void)
{
	szKey.Init();
	szName.Init();
	bFunction = false;
	sDeclaration.Init();
	bDeclaration = false;
	sDefinition.Init();
	bDefinition = false;
	bUndeclared = false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFunction::Kill(void)
{
	sDefinition.Kill();
	sDeclaration.Kill();
	szName.Kill();
	szKey.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileEdits::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
	maszFiles.Init();
	mapcEdits.Init();
	maszNotes.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// Something an edit is made for once, however often it is asked for (a
// class's forward declaration, that two members added need): true the
// first time it is noted.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEFileEdits::NoteOnce(const char* szNote)
{
	if (maszNotes.GetIndex(szNote) != ARRAY_ELEMENT_NOT_FOUND)
	{
		return false;
	}
	maszNotes.Add(szNote);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileEdits::Kill(void)
{
	size	i;

	for (i = 0; i < mapcEdits.NumElements(); i++)
	{
		mapcEdits.GetPtr(i)->Kill();
		free(mapcEdits.GetPtr(i));
	}
	maszNotes.Kill();
	mapcEdits.Kill();
	maszFiles.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A file's editor, opened (not shown) if it isn't open.  NULL if it
// can't be.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CTextArea> CIDEFileEdits::GetEditor(const char* szFileName)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->FindTab(szFileName);
	if (pTab.IsNull())
	{
		pTab = mpcContent->GetDocuments()->OpenFileTab(szFileName, mpcContent->GetCodeModel()->FindProjectOfFile(szFileName), false);
	}
	return pTab.IsNotNull() ? (Ptr<CTextArea>)pTab->GetEditor() : Ptr<CTextArea>();
}


//////////////////////////////////////////////////////////////////////////
//
// A file's text as its editor has it, NULL if it can't be opened.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEFileEdits::GetText(const char* szFileName)
{
	Ptr<CTextArea>	pEditor;

	pEditor = GetEditor(szFileName);
	return pEditor.IsNotNull() ? pEditor->GetText() : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// The edits of a file, which is one of the files changed from now on.
//
//////////////////////////////////////////////////////////////////////////
CIDETextEdits* CIDEFileEdits::GetEdits(const char* szFileName)
{
	CIDETextEdits*	pcEdits;
	size			i;

	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		if (StringInsensitiveCompare(maszFiles.GetText(i), szFileName) == 0)
		{
			return mapcEdits.GetPtr(i);
		}
	}
	pcEdits = NewMalloc<CIDETextEdits>();
	pcEdits->Init();
	maszFiles.Add(szFileName);
	mapcEdits.Add(pcEdits);
	return pcEdits;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CIDEFileEdits::NumFiles(void)
{
	return maszFiles.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// The errors the checks of the files to be changed have, as they are or
// (bEdited) as they would be with the edits made: a refactoring that
// can't tell by itself whether what it leaves compiles asks before and
// after, and does nothing if there would be more.  Each file is checked
// in its unit; a header's new text is every unit's while they are
// checked, and a file's unit has its new text too if it is one of the
// files.  A file that can't be parsed counts nothing.
//
//////////////////////////////////////////////////////////////////////////
size CIDEFileEdits::CountErrors(bool bEdited)
{
	CIDECodeModel*	pcModel;
	CArrayChars		aszTexts;
	CChars			szNew;
	CChars			szUnit;
	char*			szText;
	size			uiErrors;
	size			uiFile;
	size			uiProject;
	size			uiUnit;
	size			i;
	size			j;

	pcModel = mpcContent->GetCodeModel();
	aszTexts.Init();
	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		szNew.Init();
		szText = GetText(maszFiles.GetText(i));
		if (szText && bEdited)
		{
			mapcEdits.GetPtr(i)->ApplyTo(szText, &szNew);
		}
		else if (szText)
		{
			szNew.Append(szText);
		}
		aszTexts.Add(szNew.Text());
		szNew.Kill();
	}

	pcModel->WaitForIndexing();
	uiErrors = 0;
	szUnit.Init();
	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		//The headers' texts again each time: checking a header gives the code model that header's text.
		for (j = 0; bEdited && (j < maszFiles.NumElements()); j++)
		{
			if (CIDEIcons::GetFileType(maszFiles.GetText(j)) == IFT_Header)
			{
				pcModel->SetHeaderText(maszFiles.GetText(j), aszTexts.GetText(j));
			}
		}
		uiUnit = MAX_SIZE;
		if (bEdited && pcModel->GetIncluder(maszFiles.GetText(i), &uiProject, &szUnit) && (StringInsensitiveCompare(szUnit.Text(), maszFiles.GetText(i)) != 0))
		{
			for (j = 0; j < maszFiles.NumElements(); j++)
			{
				if (StringInsensitiveCompare(maszFiles.GetText(j), szUnit.Text()) == 0)
				{
					uiUnit = j;
				}
			}
		}
		uiFile = CountIDETrialErrors(pcModel, &mpcContent->GetFindInFiles(), maszFiles.GetText(i), bEdited ? aszTexts.GetText(i) : NULL, (uiUnit != MAX_SIZE) ? maszFiles.GetText(uiUnit) : NULL,
									 (uiUnit != MAX_SIZE) ? aszTexts.GetText(uiUnit) : NULL);
		uiErrors += (uiFile == MAX_SIZE) ? 0 : uiFile;
	}
	szUnit.Kill();
	for (i = 0; bEdited && (i < maszFiles.NumElements()); i++)
	{
		szText = GetText(maszFiles.GetText(i));
		if (szText && (CIDEIcons::GetFileType(maszFiles.GetText(i)) == IFT_Header))
		{
			pcModel->SetHeaderText(maszFiles.GetText(i), szText);
		}
	}
	aszTexts.Kill();
	return uiErrors;
}


//////////////////////////////////////////////////////////////////////////
//
// Every file's edits made, as one undo group.
//
//////////////////////////////////////////////////////////////////////////
void CIDEFileEdits::Apply(void)
{
	Ptr<CTextArea>	pEditor;
	size			i;

	mpcContent->GetEdit()->BeginUndoGroup();
	for (i = 0; i < maszFiles.NumElements(); i++)
	{
		pEditor = GetEditor(maszFiles.GetText(i));
		if (pEditor.IsNotNull())
		{
			mapcEdits.GetPtr(i)->Apply(pEditor);
		}
	}
	mpcContent->GetEdit()->EndUndoGroup();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	mapcUnits.Init();
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::Class(void)
{
	U_Pointer(mpcContent);
	U_Unknown(CArrayTemplatePtr<CIDERefactorUnit>, mapcUnits);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::Free(void)
{
	KillUnits();
	mapcUnits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::Save(CObjectWriter* pcFile) { return false; }
bool CIDEDefinitions::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// Generate Definition has CLion's keys; the others have none.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.GenerateDefinition", "Generate Definition", "Control+Shift+D", Ptr<CImageCel>(), this, &CIDEDefinitions::GenerateDefinitionCommand)->SetEnabledCheck(this, &CIDEDefinitions::CanChange);
	pCommands->Add("Refactor.GenerateDeclaration", "Generate Declaration", "", Ptr<CImageCel>(), this, &CIDEDefinitions::GenerateDeclarationCommand)->SetEnabledCheck(this, &CIDEDefinitions::CanChange);
	pCommands->Add("Refactor.MoveDefinitionOut", "Move Definition out of Class", "", Ptr<CImageCel>(), this, &CIDEDefinitions::MoveDefinitionOutCommand)->SetEnabledCheck(this, &CIDEDefinitions::CanChange);
	pCommands->Add("Refactor.MoveDefinitionIn", "Move Definition into Class", "", Ptr<CImageCel>(), this, &CIDEDefinitions::MoveDefinitionInCommand)->SetEnabledCheck(this, &CIDEDefinitions::CanChange);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.GenerateDefinition"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.GenerateDeclaration"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MoveDefinitionOut"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.MoveDefinitionIn"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: what a function has says which of these it can
// have.  One only declared can be defined; one only defined can be
// declared, if it is no class's (a class's is defined in the class, and
// can be moved out), and so can a member defined that its class doesn't
// declare; one defined in its class, or in a header, can be moved out,
// and one declared and defined apart can be moved in.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bFunction)
	{
		if (psContext->bDeclared && !psContext->bDefined)
		{
			psContext->Offer("Refactor.GenerateDefinition");
		}
		if (psContext->bDefined && !psContext->bDeclared && !psContext->bMember)
		{
			psContext->Offer("Refactor.GenerateDeclaration");
		}
		if (psContext->bDefined && !psContext->bDeclared && (psContext->bMember || psContext->bHeaderDefined))
		{
			psContext->Offer("Refactor.MoveDefinitionOut");
		}
		if (psContext->bDefined && psContext->bDeclared)
		{
			psContext->Offer("Refactor.MoveDefinitionIn");
		}
	}
	else if (psContext->bUndeclaredFunction)
	{
		psContext->Offer("Refactor.GenerateDeclaration");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::CanChange(void)
{
	//A member defined that its class doesn't declare is no symbol at all, and Generate Declaration is for it.
	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull();
}


void CIDEDefinitions::GenerateDefinitionCommand(void) { GenerateDefinition(); }
void CIDEDefinitions::GenerateDeclarationCommand(void) { GenerateDeclaration(); }
void CIDEDefinitions::MoveDefinitionOutCommand(void) { MoveDefinitionOut(); }
void CIDEDefinitions::MoveDefinitionInCommand(void) { MoveDefinitionIn(); }


//////////////////////////////////////////////////////////////////////////
//
// The file parsed (its unit, for a header), once for the function being
// looked at.  NULL if it can't be.
//
//////////////////////////////////////////////////////////////////////////
CIDERefactorUnit* CIDEDefinitions::GetUnit(const char* szFileName)
{
	CIDERefactorUnit*	pcUnit;
	size				i;

	for (i = 0; i < mapcUnits.NumElements(); i++)
	{
		pcUnit = mapcUnits.GetPtr(i);
		if (StringInsensitiveCompare(pcUnit->GetFileName(), szFileName) == 0)
		{
			return pcUnit->IsParsed() ? pcUnit : NULL;
		}
	}
	pcUnit = NewMalloc<CIDERefactorUnit>();
	pcUnit->Init();
	mapcUnits.Add(pcUnit);
	return pcUnit->Parse(mpcContent->GetCodeModel(), &mpcContent->GetFindInFiles(), szFileName, false) ? pcUnit : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::KillUnits(void)
{
	size	i;

	for (i = 0; i < mapcUnits.NumElements(); i++)
	{
		mapcUnits.GetPtr(i)->Kill();
		free(mapcUnits.GetPtr(i));
	}
	mapcUnits.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
// The function named at the caret, from its declaration, its definition
// or a use of it: its declaration and its definition as they are
// written.  False, with why, if there is no function there or one of
// them can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::FindFunction(SIDEFunction* psDest, CChars* pszWhyNot)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	CIDERefactorUnit*	pcUnit;
	SIDESignatureSite	sSite;
	const char*			szWhyNot;
	int					iLine;
	int					iColumn;
	uint8				uiRole;
	uint8				uiKind;
	size				i;

	pcModel = mpcContent->GetCodeModel();
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		pszWhyNot->Set("there is no file open");
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) ||
		!pcModel->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &psDest->szKey, &uiKind))
	{
		if (FindUndeclaredFunction(psDest))
		{
			return true;
		}
		pszWhyNot->Set("there is no function at the caret");
		return false;
	}
	GetIDEScopedName(psDest->szKey.Text(), &psDest->szName);
	psDest->bFunction = (uiKind == CPPSK_Function) || (uiKind == CPPSK_FunctionTemplate);
	if (!psDest->bFunction)
	{
		pszWhyNot->Set("it isn't a function");
		return false;
	}

	asLocations.Init();
	pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
	szWhyNot = NULL;
	for (i = 0; !szWhyNot && (i < asLocations.NumElements()); i++)
	{
		psLocation = asLocations.Get(i);
		if (psLocation->uiRole == CPPIR_Reference)
		{
			continue;
		}
		if (pcModel->FindProjectOfFile(psLocation->szFileName.Text()) == MAX_SIZE)
		{
			szWhyNot = "it is declared in a file that isn't in the solution";
			break;
		}
		pcUnit = GetUnit(psLocation->szFileName.Text());
		if (!pcUnit)
		{
			szWhyNot = "a file it is declared in couldn't be parsed";
			break;
		}
		sSite.Init();
		if (!mpcContent->GetChangeSignature()->ReadSite(pcUnit, psLocation, psDest->szKey.Text(), &sSite, &szWhyNot))
		{
			sSite.Kill();
		}
		else if (sSite.bBody && !psDest->bDefinition)
		{
			psDest->sDefinition.Kill();
			psDest->sDefinition = sSite;
			psDest->bDefinition = true;
		}
		else if (!sSite.bBody && !psDest->bDeclaration)
		{
			psDest->sDeclaration.Kill();
			psDest->sDeclaration = sSite;
			psDest->bDeclaration = true;
		}
		else
		{
			sSite.Kill();
		}
	}
	KillIDELocations(&asLocations);
	if (szWhyNot)
	{
		pszWhyNot->Set(szWhyNot);
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The function whose definition's name the caret is in, where that is a
// member defined outside a class that doesn't declare it,
// "void CFoo::Reset(int)": it is nothing the compiler knows, so the index
// has no symbol for it, and it is read from the file as it is written.
// Its key is its name in its class, without parameters.  False if the
// caret isn't in such a name.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::FindUndeclaredFunction(SIDEFunction* psDest)
{
	Ptr<CIDEEditorTab>				pTab;
	CIDERefactorUnit*				pcUnit;
	CCPPSyntaxFunctionDefinition*	pcDefinition;
	CCPPSyntaxName*					pcName;
	SCPPSyntaxRange					sName;
	SCPPSyntaxRange					sLast;
	SIDELocation					sLocation;
	CChars							szScope;
	const char*						szWhyNot;
	size							uiCaret;
	int32							iLine;
	int32							iColumn;
	bool							bRead;

	pTab = mpcContent->GetDocuments()->GetActiveTab();
	pcUnit = GetUnit(pTab->GetFileName());
	if (!pcUnit)
	{
		return false;
	}
	uiCaret = pTab->GetEditor()->GetCaretPosition();
	iLine = (int32)pTab->GetEditor()->FindLine(uiCaret);
	iColumn = (int32)(uiCaret - pTab->GetEditor()->GetLineStart((size)iLine));
	pcDefinition = (CCPPSyntaxFunctionDefinition*)pcUnit->FindAncestor(pcUnit->FindNodeAt(iLine, iColumn), CPPS_FunctionDefinition);
	pcName = (pcDefinition && pcDefinition->mpcDeclarator) ? pcDefinition->mpcDeclarator->GetName() : NULL;
	if (!pcName || pcName->IsSimple() || !pcName->GetLast() || !pcUnit->GetRange(pcName, &sName) ||
		!pcUnit->GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sLast))
	{
		return false;
	}
	if ((iLine < sName.iLine) || ((iLine == sName.iLine) && (iColumn < sName.iColumn)) || (iLine > sName.iEndLine) || ((iLine == sName.iEndLine) && (iColumn > sName.iEndColumn)))
	{
		return false;
	}

	//Its class, as it is written before its name's last ::.
	szScope.Init();
	pcUnit->AppendTokens(pcName->miFirstToken, pcName->GetLast()->miFirstToken - 2, &szScope);
	szScope.Replace(" ", "");

	sLocation.szFileName.Init(pTab->GetFileName());
	sLocation.iLine = sLast.iLine;
	sLocation.iColumn = sLast.iColumn;
	sLocation.iLength = sLast.iEndColumn - sLast.iColumn;
	sLocation.uiRole = CPPIR_Definition;
	sLocation.uiFlags = 0;
	sLocation.uiUsage = 0;
	sLocation.iContainer = -1;
	szWhyNot = NULL;
	bRead = !szScope.Empty() && mpcContent->GetChangeSignature()->ReadSite(pcUnit, &sLocation, "", &psDest->sDefinition, &szWhyNot) && psDest->sDefinition.bBody;
	if (bRead)
	{
		psDest->szKey.Set(szScope);
		psDest->szKey.Append("::");
		psDest->szKey.Append(psDest->sDefinition.szOwnName);
		psDest->szName.Set(psDest->szKey);
		psDest->bFunction = true;
		psDest->bDefinition = true;
		psDest->bUndeclared = true;
	}
	sLocation.szFileName.Kill();
	szScope.Kill();
	return bRead;
}


//////////////////////////////////////////////////////////////////////////
//
// The function at the caret for an operation; if there is none the
// status line says the operation can't be done and why.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::ReadFunction(SIDEFunction* psDest, const char* szOperation)
{
	CChars	szWhyNot;
	bool	bFound;

	szWhyNot.Init();
	bFound = FindFunction(psDest, &szWhyNot);
	if (!bFound)
	{
		Refuse(szOperation, psDest, szWhyNot.Text());
	}
	szWhyNot.Kill();
	return bFound;
}


//////////////////////////////////////////////////////////////////////////
//
// "Can't generate a definition of CFoo::Init: it is defined already."
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::Refuse(const char* szOperation, SIDEFunction* psFunction, const char* szWhyNot)
{
	CChars	szMessage;

	szMessage.Init("Can't ");
	szMessage.Append(szOperation);
	if (psFunction->bFunction)
	{
		szMessage.Append(" of ");
		szMessage.Append(psFunction->szName);
	}
	szMessage.Append(": ");
	szMessage.Append(szWhyNot);
	szMessage.Append(".");
	mpcContent->ShowStatus(szMessage.Text());
	szMessage.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A declaration's or definition's parameters as it writes them, with
// their default values or without, each with one space after its comma:
// what is between its parentheses if it has none ("void", or nothing).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AppendParameters(SIDESignatureSite* psSite, const char* szText, bool bDefaults, CChars* pszDest)
{
	SIDEWrittenParameter*	psWritten;
	SCPPSyntaxRange			sRange;
	CChars					szPart;
	size					i;

	if (psSite->asParameters.NumElements() == 0)
	{
		szPart.Init();
		AppendIDERangeText(szText, &psSite->sParameters, &szPart);
		szPart.StripWhitespace();
		pszDest->Append(szPart);
		szPart.Kill();
		return;
	}
	for (i = 0; i < psSite->asParameters.NumElements(); i++)
	{
		psWritten = psSite->asParameters.Get(i);
		sRange = psWritten->sAll;
		if (!bDefaults && psWritten->bDefault)
		{
			sRange.iEndLine = psWritten->sDefault.iLine;
			sRange.iEndColumn = psWritten->sDefault.iColumn;
		}
		szPart.Init();
		AppendIDERangeText(szText, &sRange, &szPart);
		szPart.StripWhitespace();
		pszDest->Append((i > 0) ? ", " : "");
		pszDest->Append(szPart);
		szPart.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where the comment over a definition starts: the start of the first of
// the // lines directly above the line it starts on; its own line's
// start if there are none.
//
//////////////////////////////////////////////////////////////////////////
size CIDEDefinitions::FindBannerStart(const char* szText, size uiDefinition)
{
	size	uiStart;
	size	uiPrevious;
	size	i;

	for (uiStart = uiDefinition; (uiStart > 0) && (szText[uiStart - 1] != '\n'); uiStart--)
	{
	}
	while (uiStart > 0)
	{
		for (uiPrevious = uiStart - 1; (uiPrevious > 0) && (szText[uiPrevious - 1] != '\n'); uiPrevious--)
		{
		}
		for (i = uiPrevious; (szText[i] == ' ') || (szText[i] == '\t'); i++)
		{
		}
		if ((szText[i] != '/') || (szText[i + 1] != '/'))
		{
			break;
		}
		uiStart = uiPrevious;
	}
	return uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
// The comment to go over a new definition, like the one over the
// definition at uiDefinition of szText: that one's lines, if they say
// nothing (a banner of slashes with nothing written in it, as most of a
// file's are); otherwise the plain banner.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AppendBanner(const char* szText, size uiDefinition, CChars* pszDest)
{
	size	uiStart;
	size	uiEnd;
	size	i;
	bool	bSaysNothing;

	bSaysNothing = false;
	uiStart = 0;
	uiEnd = 0;
	if (szText)
	{
		for (uiEnd = uiDefinition; (uiEnd > 0) && (szText[uiEnd - 1] != '\n'); uiEnd--)
		{
		}
		uiStart = FindBannerStart(szText, uiDefinition);
		bSaysNothing = (uiEnd > uiStart);
		for (i = uiStart; bSaysNothing && (i < uiEnd); i++)
		{
			bSaysNothing = !IsIDENameChar(szText[i]);
		}
	}
	if (bSaysNothing)
	{
		pszDest->AppendSubString(szText, uiStart, uiEnd);
	}
	else
	{
		pszDest->Append(IDE_DEFINITION_BANNER);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A body as it is written outside a class: its braces at the start of
// their lines and what is in it one tab in.  One written on one line,
// "{ return mi; }", is opened out; one of several lines has the
// indentation of its closing brace's line taken from each line.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AppendBodyOutside(const char* szBody, CChars* pszDest)
{
	CChars		szInner;
	CArrayChars	aszLines;
	CChars		szAll;
	CChars*		pszLine;
	size		uiLength;
	size		uiIndent;
	size		i;
	size		j;

	uiLength = strlen(szBody);
	if (strchr(szBody, '\n') == NULL)
	{
		szInner.Init();
		if (uiLength >= 2)
		{
			szInner.AppendSubString(szBody, 1, uiLength - 1);
		}
		szInner.StripWhitespace();
		pszDest->Append("{\n");
		if (!szInner.Empty())
		{
			pszDest->Append('\t')->Append(szInner)->Append('\n');
		}
		pszDest->Append("}");
		szInner.Kill();
		return;
	}

	szAll.Init(szBody);
	szAll.Replace("\r", "");
	aszLines.Init();
	szAll.Split(&aszLines, '\n');
	pszLine = aszLines.Get(aszLines.NumElements() - 1);
	for (uiIndent = 0; (uiIndent < pszLine->Length()) && ((pszLine->Text()[uiIndent] == ' ') || (pszLine->Text()[uiIndent] == '\t')); uiIndent++)
	{
	}
	for (i = 0; i < aszLines.NumElements(); i++)
	{
		pszLine = aszLines.Get(i);
		for (j = 0; (j < uiIndent) && (j < pszLine->Length()) && ((pszLine->Text()[j] == ' ') || (pszLine->Text()[j] == '\t')); j++)
		{
		}
		pszDest->Append((i > 0) ? "\n" : "");
		pszDest->Append(&pszLine->Text()[(i == 0) ? 0 : j]);
	}
	aszLines.Kill();
	szAll.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A body (as it is written outside a class, AppendBodyOutside) as it
// goes after a declaration in a class whose line is indented szIndent:
// one short statement on the declaration's line, " { return mi; }", as
// the house style has a class's short functions, nothing as " {}", and
// anything else under it, each line indented with the declaration.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AppendBodyInClass(const char* szBody, const char* szIndent, CChars* pszDest)
{
	CArrayChars	aszLines;
	CChars		szAll;
	CChars		szStatement;
	size		i;

	szAll.Init(szBody);
	aszLines.Init();
	szAll.Split(&aszLines, '\n');
	if (aszLines.NumElements() == 2)
	{
		pszDest->Append(" {}");
	}
	else if ((aszLines.NumElements() == 3) && (aszLines.Get(1)->Length() <= IDE_DEFINITION_ONE_LINE))
	{
		szStatement.Init(aszLines.GetText(1));
		szStatement.StripWhitespace();
		pszDest->Append(" { ")->Append(szStatement)->Append(" }");
		szStatement.Kill();
	}
	else
	{
		for (i = 0; i < aszLines.NumElements(); i++)
		{
			pszDest->Append('\n');
			if (!aszLines.Get(i)->Empty())
			{
				pszDest->Append(szIndent)->Append(aszLines.GetText(i));
			}
		}
	}
	aszLines.Kill();
	szAll.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Where a function's definition goes outside its class, and the comment
// to go over it: in the file the functions declared with it (its
// class's, or its header's) are defined in, after the definition of the
// nearest one declared before it that has one, or before the nearest
// after it; a class template's member, and a function template, in the
// header they are declared in.  With none of them defined, at the end of
// the header's source file (its stem with .cpp or .c), or for a
// template before the header's #endif.  iFiles (IDE_FILES_) keeps it in
// the file it is declared in, or out of it.  *puiAt is in the file's
// editor's text and *piWhere says how it is put there (IDE_PLACE_).
// False, with why, if there is nowhere.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::FindDefinitionPlace(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, int iFiles, CChars* pszFileName, size* puiAt, int* piWhere, CChars* pszBanner, const char** pszWhyNot)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asDeclared;
	CArrayIDELocations	asDefined;
	CArrayChars			aszRelated;
	SIDESignatureSite*	psDeclared;
	SIDESignatureSite	sSibling;
	SIDELocation*		psDefined;
	CIDERefactorUnit*	pcUnit;
	const char*			szWhyNot;
	char*				szText;
	size				uiOwn;
	size				uiSibling;
	size				uiLength;
	size				uiLine;
	size				i;
	int					iStep;
	bool				bInHeader;
	bool				bFound;
	bool				bSameFile;

	pcModel = mpcContent->GetCodeModel();
	psDeclared = psFunction->bDeclaration ? &psFunction->sDeclaration : &psFunction->sDefinition;
	bInHeader = (iFiles == IDE_FILES_HEADER) || !psDeclared->szOwnTemplate.Empty() || (psDeclared->bInClass && !psDeclared->szClassTemplate.Empty());

	asDeclared.Init();
	asDefined.Init();
	pcModel->FindSiblingFunctions(psFunction->szKey.Text(), psDeclared->szFileName.Text(), &asDeclared, &asDefined);
	uiOwn = MAX_SIZE;
	for (i = 0; i < asDeclared.NumElements(); i++)
	{
		//A function outside a class moved into its header goes last in it: what its body names is all declared by
		//there, as it was for the source file.
		if ((asDeclared.Get(i)->iLine == psDeclared->iLine) && (asDeclared.Get(i)->iColumn == psDeclared->iColumn) && (iFiles != IDE_FILES_HEADER))
		{
			uiOwn = i;
		}
	}

	//The nearest declared before it with a definition outside the class, then the nearest after it.
	bFound = false;
	uiSibling = 0;
	for (iStep = -1; !bFound && (uiOwn != MAX_SIZE) && (iStep <= 1); iStep += 2)
	{
		for (i = uiOwn + (size)iStep; !bFound && (i < asDeclared.NumElements()); i += (size)iStep)
		{
			psDefined = asDefined.Get(i);
			bSameFile = (psDefined->iLine >= 0) && (StringInsensitiveCompare(psDefined->szFileName.Text(), psDeclared->szFileName.Text()) == 0);
			if ((psDefined->iLine >= 0) && (!bInHeader || bSameFile) && ((iFiles != IDE_FILES_SOURCE) || !bSameFile))
			{
				bFound = true;
				uiSibling = i;
				*piWhere = (iStep < 0) ? IDE_PLACE_AFTER : IDE_PLACE_BEFORE;
			}
		}
	}

	*pszWhyNot = NULL;
	if (bFound)
	{
		psDefined = asDefined.Get(uiSibling);
		pszFileName->Set(psDefined->szFileName);
		pcUnit = GetUnit(psDefined->szFileName.Text());
		szText = pcEdits->GetText(psDefined->szFileName.Text());
		sSibling.Init();
		if (!pcUnit || !szText || !mpcContent->GetChangeSignature()->ReadSite(pcUnit, psDefined, "", &sSibling, &szWhyNot))
		{
			*pszWhyNot = "the definition it would go beside couldn't be read";
		}
		else if (*piWhere == IDE_PLACE_AFTER)
		{
			AppendBanner(szText, FindIDETextOffset(szText, sSibling.sAll.iLine, sSibling.sAll.iColumn), pszBanner);
			for (*puiAt = FindIDETextOffset(szText, sSibling.sAll.iEndLine, sSibling.sAll.iEndColumn); (szText[*puiAt] != '\0') && (szText[*puiAt] != '\n'); (*puiAt)++)
			{
			}
			if (szText[*puiAt] == '\n')
			{
				(*puiAt)++;
			}
		}
		else
		{
			AppendBanner(szText, FindIDETextOffset(szText, sSibling.sAll.iLine, sSibling.sAll.iColumn), pszBanner);
			*puiAt = FindBannerStart(szText, FindIDETextOffset(szText, sSibling.sAll.iLine, sSibling.sAll.iColumn));
		}
		sSibling.Kill();
	}
	else if (bInHeader)
	{
		//Before the header's closing #endif, or at its end.
		pszFileName->Set(psDeclared->szFileName);
		szText = pcEdits->GetText(psDeclared->szFileName.Text());
		pszBanner->Append(IDE_DEFINITION_BANNER);
		if (!szText)
		{
			*pszWhyNot = "its header couldn't be opened";
		}
		else
		{
			uiLength = strlen(szText);
			*puiAt = uiLength;
			*piWhere = IDE_PLACE_END;
			for (uiLine = uiLength; uiLine > 0; uiLine--)
			{
				if (((uiLine == 1) || (szText[uiLine - 2] == '\n')) && (strncmp(&szText[uiLine - 1], "#endif", 6) == 0))
				{
					*puiAt = uiLine - 1;
					*piWhere = IDE_PLACE_BEFORE;
					break;
				}
			}
		}
	}
	else
	{
		//The header's source file.
		aszRelated.Init();
		mpcContent->GetFileRename()->FindRelatedFiles(psDeclared->szFileName.Text(), &aszRelated);
		for (i = 0; i < aszRelated.NumElements(); i++)
		{
			if ((aszRelated.Get(i)->EndsWithIgnoreCase(".cpp") || aszRelated.Get(i)->EndsWithIgnoreCase(".c")) && pszFileName->Empty())
			{
				pszFileName->Set(aszRelated.GetText(i));
			}
		}
		aszRelated.Kill();
		szText = pszFileName->Empty() ? NULL : pcEdits->GetText(pszFileName->Text());
		pszBanner->Append(IDE_DEFINITION_BANNER);
		if (!szText)
		{
			*pszWhyNot = psDeclared->bInClass ? "none of its class's functions is defined outside it, and its header has no source file in the solution to define it in" :
												 "its header has no source file in the solution to define it in";
		}
		else
		{
			*puiAt = strlen(szText);
			*piWhere = IDE_PLACE_END;
		}
	}
	KillIDELocations(&asDefined);
	KillIDELocations(&asDeclared);
	return *pszWhyNot == NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// A function's definition as it is written outside its class, from where
// it is declared (szDeclarationText is that file's text): the comment
// over it, its class template's template<...> and its own, szPrefix
// ("inline ", or NULL), what it returns, its name after its class's, its
// parameters without their
// default values, what follows them (const), a constructor's
// initialisers, and its body (an empty one if szBody is NULL).
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AppendDefinition(SIDEFunction* psFunction, const char* szDeclarationText, const char* szBanner, const char* szPrefix, const char* szInitialisers, const char* szBody, CChars* pszDest)
{
	SIDESignatureSite*	psDeclared;
	CChars				szPart;

	psDeclared = psFunction->bDeclaration ? &psFunction->sDeclaration : &psFunction->sDefinition;
	pszDest->Append(szBanner);
	if (psDeclared->bInClass && !psDeclared->szClassTemplate.Empty())
	{
		pszDest->Append(psDeclared->szClassTemplate)->Append('\n');
	}
	if (!psDeclared->szOwnTemplate.Empty())
	{
		pszDest->Append(psDeclared->szOwnTemplate)->Append('\n');
	}
	pszDest->Append(szPrefix ? szPrefix : "");
	if (psDeclared->bReturn)
	{
		szPart.Init();
		AppendIDERangeText(szDeclarationText, &psDeclared->sReturn, &szPart);
		szPart.StripWhitespace();
		pszDest->Append(szPart)->Append(' ');
		szPart.Kill();
	}
	if (psDeclared->bInClass)
	{
		pszDest->Append(psDeclared->szClassQualifier)->Append(psDeclared->szOwnName);
	}
	else
	{
		AppendIDERangeText(szDeclarationText, &psDeclared->sName, pszDest);
	}
	pszDest->Append('(');
	AppendParameters(psDeclared, szDeclarationText, false, pszDest);
	pszDest->Append(')');
	pszDest->Append(psDeclared->szQualifiers);
	if (szInitialisers && (szInitialisers[0] != '\0'))
	{
		pszDest->Append(' ')->Append(szInitialisers);
	}
	pszDest->Append('\n');
	pszDest->Append(szBody ? szBody : "{\n}");
	pszDest->Append('\n');
}


//////////////////////////////////////////////////////////////////////////
//
// A function's definition put outside its class (FindDefinitionPlace,
// AppendDefinition), two blank lines from what is around it, as an edit
// of the file it goes in.  *puiAt is where its text starts in that file
// as it is now.  False, with why, if there is nowhere for it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::PutDefinition(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, int iFiles, const char* szPrefix, const char* szInitialisers, const char* szBody, CChars* pszFileName, size* puiAt, const char** pszWhyNot)
{
	SIDESignatureSite*	psDeclared;
	CChars				szBanner;
	CChars				szDefinition;
	char*				szDeclarationText;
	char*				szText;
	int					iWhere;

	psDeclared = psFunction->bDeclaration ? &psFunction->sDeclaration : &psFunction->sDefinition;
	szBanner.Init();
	iWhere = IDE_PLACE_END;
	if (!FindDefinitionPlace(psFunction, pcEdits, iFiles, pszFileName, puiAt, &iWhere, &szBanner, pszWhyNot))
	{
		szBanner.Kill();
		return false;
	}
	szDeclarationText = pcEdits->GetText(psDeclared->szFileName.Text());
	szText = pcEdits->GetText(pszFileName->Text());
	if (!szDeclarationText || !szText)
	{
		*pszWhyNot = "a file couldn't be opened";
		szBanner.Kill();
		return false;
	}

	szDefinition.Init();
	if (iWhere != IDE_PLACE_BEFORE)
	{
		//After what is before it: on a line of its own, two blank lines down.
		szDefinition.Append(((*puiAt > 0) && (szText[*puiAt - 1] != '\n')) ? "\n\n\n" : "\n\n");
	}
	AppendDefinition(psFunction, szDeclarationText, szBanner.Text(), szPrefix, szInitialisers, szBody, &szDefinition);
	if (iWhere == IDE_PLACE_BEFORE)
	{
		szDefinition.Append("\n\n");
	}
	pcEdits->GetEdits(pszFileName->Text())->Insert(*puiAt, szDefinition.Text());
	szDefinition.Kill();
	szBanner.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The file shown, its caret at a place.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::ShowDefinition(const char* szFileName, size uiAt)
{
	Ptr<CIDEEditorTab>	pTab;

	pTab = mpcContent->GetDocuments()->OpenFileTab(szFileName, mpcContent->GetCodeModel()->FindProjectOfFile(szFileName), true);
	if (pTab.IsNotNull())
	{
		pTab->GetEditor()->SetCaretPosition(uiAt);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A function that is declared and not defined is defined, with an empty
// body, and shown.  False (the status line says why) if it has a
// definition, is pure, or has nowhere to go.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::GenerateDefinition(void)
{
	SIDEFunction		sFunction;
	CIDEFileEdits		cEdits;
	SIDESignatureSite	sStale;
	CChars				szFileName;
	CChars				szMessage;
	const char*			szWhyNot;
	char*				szText;
	size				uiAt;
	bool				bResult;

	sFunction.Init();
	bResult = ReadFunction(&sFunction, "generate a definition");
	if (bResult)
	{
		szWhyNot = NULL;
		if (sFunction.bDefinition)
		{
			szWhyNot = "it is defined already";
		}
		else if (!sFunction.bDeclaration)
		{
			szWhyNot = "it has no declaration";
		}
		else if (sFunction.sDeclaration.bPure)
		{
			szWhyNot = "it is pure virtual";
		}
		cEdits.Init(mpcContent);
		szFileName.Init();
		uiAt = 0;
		sStale.Init();
		if (!szWhyNot && FindUndeclaredDefinition(&sFunction, &cEdits, &sStale))
		{
			//It has a definition that was left behind when one of them was edited: that is changed to match.
			szFileName.Set(sStale.szFileName);
			szText = cEdits.GetText(szFileName.Text());
			MatchSite(&sStale, szText, &sFunction.sDeclaration, cEdits.GetText(sFunction.sDeclaration.szFileName.Text()), false, cEdits.GetEdits(szFileName.Text()));
			uiAt = FindIDETextOffset(szText, sStale.sName.iLine, sStale.sName.iColumn);
			cEdits.Apply();
			ShowDefinition(szFileName.Text(), uiAt);
			szMessage.Init("Changed ");
			szMessage.Append(sFunction.szName);
			szMessage.Append("'s definition in ");
			szMessage.Append(GetIDEFileNamePart(szFileName.Text()));
			szMessage.Append(" to match its declaration.");
			mpcContent->ShowStatus(szMessage.Text());
			szMessage.Kill();
			sStale.Kill();
			szFileName.Kill();
			cEdits.Kill();
			KillUnits();
			sFunction.Kill();
			return true;
		}
		sStale.Kill();
		bResult = !szWhyNot && PutDefinition(&sFunction, &cEdits, IDE_FILES_ANY, NULL, NULL, NULL, &szFileName, &uiAt, &szWhyNot);
		if (bResult)
		{
			cEdits.Apply();
			ShowDefinition(szFileName.Text(), uiAt);
			szMessage.Init("Defined ");
			szMessage.Append(sFunction.szName);
			szMessage.Append(" in ");
			szMessage.Append(GetIDEFileNamePart(szFileName.Text()));
			szMessage.Append(".");
			mpcContent->ShowStatus(szMessage.Text());
			szMessage.Kill();
		}
		else
		{
			Refuse("generate a definition", &sFunction, szWhyNot);
		}
		szFileName.Kill();
		cEdits.Kill();
	}
	KillUnits();
	sFunction.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A function that is defined and not declared is declared: a member in
// its class, after the last function it declares and lined up with it
// (before its closing brace if it declares none); a function outside a
// class in the header of the file that defines it, after the last
// function that declares.  False (the status line says why) if it is
// declared, is defined in its class, or its class or header isn't found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::GenerateDeclaration(void)
{
	CIDECodeModel*		pcModel;
	SIDEFunction		sFunction;
	CIDEFileEdits		cEdits;
	CArrayIDELocations	asDeclared;
	CArrayIDELocations	asDefined;
	CArrayIDELocations	asClass;
	CArrayChars			aszRelated;
	SIDESignatureSite*	psDefinition;
	SIDESignatureSite	sLast;
	SIDESignatureSite	sStale;
	SIDELocation*		psLocation;
	CIDERefactorUnit*	pcUnit;
	CCPPSyntax*			pcClass;
	SCPPSyntaxRange		sRange;
	CChars				szHeader;
	CChars				szScope;
	CChars				szLine;
	CChars				szReturn;
	CChars				szMessage;
	const char*			szWhyNot;
	char*				szText;
	char*				szDefinitionText;
	size				uiAt;
	size				uiLineStart;
	size				uiNameColumn;
	size				uiReturnEnd;
	size				uiIndentEnd;
	size				i;
	uint8				uiKind;
	bool				bResult;
	bool				bMember;
	bool				bLast;
	bool				bStale;

	pcModel = mpcContent->GetCodeModel();
	sFunction.Init();
	sStale.Init();
	bStale = false;
	bResult = ReadFunction(&sFunction, "generate a declaration");
	if (bResult)
	{
		psDefinition = &sFunction.sDefinition;
		szWhyNot = NULL;
		if (sFunction.bDeclaration)
		{
			szWhyNot = "it is declared already";
		}
		else if (!sFunction.bDefinition)
		{
			szWhyNot = "it has no definition";
		}
		else if (psDefinition->bInClass)
		{
			szWhyNot = "it is defined in its class, which declares it";
		}

		cEdits.Init(mpcContent);
		szHeader.Init();
		szLine.Init();
		uiAt = 0;
		uiNameColumn = 0;
		bLast = false;
		szDefinitionText = szWhyNot ? NULL : cEdits.GetText(psDefinition->szFileName.Text());
		szText = NULL;

		//Its scope: a class, whose definition is where it is declared, or none (or a namespace).
		szScope.Init();
		GetIDESymbolScope(sFunction.szKey.Text(), &szScope);
		asClass.Init();
		bMember = !szWhyNot && !szScope.Empty() && pcModel->GetSymbolLocations(szScope.Text(), &asClass, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate));
		sLast.Init();
		if (!szWhyNot && bMember)
		{
			//The class's definition, and in it the last function it declares.
			pcClass = NULL;
			pcUnit = NULL;
			for (i = 0; !pcClass && (i < asClass.NumElements()); i++)
			{
				psLocation = asClass.Get(i);
				if (psLocation->uiRole == CPPIR_Definition)
				{
					szHeader.Set(psLocation->szFileName);
					pcUnit = GetUnit(psLocation->szFileName.Text());
					pcClass = pcUnit ? pcUnit->FindAncestor(pcUnit->FindNodeAt(psLocation->iLine, psLocation->iColumn), CPPS_ClassSpecifier) : NULL;
				}
			}
			szText = pcClass ? cEdits.GetText(szHeader.Text()) : NULL;
			if (!pcClass || !szText || !pcUnit->GetRange(pcClass, &sRange))
			{
				szWhyNot = "its class's definition wasn't found in the solution";
			}
			else
			{
				//Before the class's closing brace, unless a function is found to go after.
				uiAt = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
				for (; (uiAt > 0) && (szText[uiAt - 1] != '\n'); uiAt--)
				{
				}
				asDeclared.Init();
				asDefined.Init();
				pcModel->FindSiblingFunctions(sFunction.szKey.Text(), szHeader.Text(), &asDeclared, &asDefined);
				for (i = asDeclared.NumElements(); !bLast && (i > 0); i--)
				{
					psLocation = asDeclared.Get(i - 1);
					if ((psLocation->iLine >= sRange.iLine) && (psLocation->iLine <= sRange.iEndLine))
					{
						bLast = mpcContent->GetChangeSignature()->ReadSite(pcUnit, psLocation, "", &sLast, &szWhyNot);
						if (!bLast)
						{
							sLast.Kill();
							sLast.Init();
						}
					}
				}
				szWhyNot = NULL;
				//A member with a definition and no declaration: its class may declare it as it was before it was edited.
				bStale = FindUndefinedNamesake(pcUnit, &asDeclared, &asDefined, &sRange, psDefinition->szOwnName.Text(), &sStale);
				KillIDELocations(&asDefined);
				KillIDELocations(&asDeclared);
			}
		}
		else if (!szWhyNot)
		{
			//The header of the file that defines it, and the last function that declares.
			aszRelated.Init();
			mpcContent->GetFileRename()->FindRelatedFiles(psDefinition->szFileName.Text(), &aszRelated);
			for (i = 0; i < aszRelated.NumElements(); i++)
			{
				if ((aszRelated.Get(i)->EndsWithIgnoreCase(".h") || aszRelated.Get(i)->EndsWithIgnoreCase(".hpp")) && szHeader.Empty())
				{
					szHeader.Set(aszRelated.GetText(i));
				}
			}
			aszRelated.Kill();
			szText = szHeader.Empty() ? NULL : cEdits.GetText(szHeader.Text());
			pcUnit = szText ? GetUnit(szHeader.Text()) : NULL;
			if (!szText || !pcUnit)
			{
				szWhyNot = "the file that defines it has no header in the solution to declare it in";
			}
			else
			{
				//Before the header's closing #endif, unless a function is found to go after.
				uiAt = strlen(szText);
				for (i = uiAt; i > 0; i--)
				{
					if (((i == 1) || (szText[i - 2] == '\n')) && (strncmp(&szText[i - 1], "#endif", 6) == 0))
					{
						uiAt = i - 1;
						break;
					}
				}
				asDeclared.Init();
				asDefined.Init();
				pcModel->FindSiblingFunctions(sFunction.szKey.Text(), szHeader.Text(), &asDeclared, &asDefined);
				for (i = asDeclared.NumElements(); !bLast && (i > 0); i--)
				{
					if (mpcContent->GetChangeSignature()->ReadSite(pcUnit, asDeclared.Get(i - 1), "", &sLast, &szWhyNot))
					{
						bLast = true;
					}
				}
				szWhyNot = NULL;
				KillIDELocations(&asDefined);
				KillIDELocations(&asDeclared);
			}
		}

		if (!szWhyNot && szDefinitionText && szText && bStale)
		{
			//The class declares it as it was before its definition was edited: that declaration is changed to match.
			MatchSite(&sStale, szText, psDefinition, szDefinitionText, true, cEdits.GetEdits(szHeader.Text()));
			uiAt = FindIDETextOffset(szText, sStale.sName.iLine, sStale.sName.iColumn);
			cEdits.Apply();
			ShowDefinition(szHeader.Text(), uiAt);
			szMessage.Init("Changed ");
			szMessage.Append(sFunction.szName);
			szMessage.Append("'s declaration in ");
			szMessage.Append(GetIDEFileNamePart(szHeader.Text()));
			szMessage.Append(" to match its definition.");
			mpcContent->ShowStatus(szMessage.Text());
			szMessage.Kill();
		}
		else if (!szWhyNot && szDefinitionText && szText)
		{
			//The declaration's line: indented and lined up as the last function's is, or one tab in a class.
			szReturn.Init();
			if (psDefinition->bReturn)
			{
				AppendIDERangeText(szDefinitionText, &psDefinition->sReturn, &szReturn);
				szReturn.StripWhitespace();
			}
			if (bLast)
			{
				uiLineStart = FindIDETextOffset(szText, sLast.sAll.iLine, 0);
				for (uiIndentEnd = uiLineStart; (szText[uiIndentEnd] == ' ') || (szText[uiIndentEnd] == '\t'); uiIndentEnd++)
				{
				}
				szLine.AppendSubString(szText, uiLineStart, uiIndentEnd);
				uiNameColumn = FindIDEVisualColumn(szText, FindIDETextOffset(szText, sLast.sName.iLine, 0), FindIDETextOffset(szText, sLast.sName.iLine, sLast.sName.iColumn), 4);
				for (uiAt = FindIDETextOffset(szText, sLast.sAll.iEndLine, sLast.sAll.iEndColumn); (szText[uiAt] != '\0') && (szText[uiAt] != '\n'); uiAt++)
				{
				}
				uiAt = (szText[uiAt] == '\n') ? uiAt + 1 : uiAt;
			}
			else if (bMember)
			{
				szLine.Append('\t');
			}
			uiReturnEnd = FindIDEVisualColumn(szLine.Text(), 0, szLine.Length(), 4) + szReturn.Length();
			szLine.Append(szReturn);
			if (!szReturn.Empty())
			{
				if (uiNameColumn > uiReturnEnd)
				{
					AppendIDETabsTo(&szLine, uiReturnEnd, uiNameColumn, 4);
				}
				else
				{
					szLine.Append('\t');
				}
			}
			szLine.Append(psDefinition->szOwnName);
			szLine.Append('(');
			AppendParameters(psDefinition, szDefinitionText, true, &szLine);
			szLine.Append(')');
			szLine.Append(psDefinition->szQualifiers);
			szLine.Append(";\n");
			szReturn.Kill();
			if (!bLast && !bMember)
			{
				szLine.Append("\n\n");
			}
			cEdits.GetEdits(szHeader.Text())->Insert(uiAt, szLine.Text());
			cEdits.Apply();
			ShowDefinition(szHeader.Text(), uiAt);
			szMessage.Init("Declared ");
			szMessage.Append(sFunction.szName);
			szMessage.Append(" in ");
			szMessage.Append(GetIDEFileNamePart(szHeader.Text()));
			szMessage.Append(".");
			mpcContent->ShowStatus(szMessage.Text());
			szMessage.Kill();
		}
		else
		{
			Refuse("generate a declaration", &sFunction, szWhyNot ? szWhyNot : "a file couldn't be opened");
			bResult = false;
		}
		sLast.Kill();
		KillIDELocations(&asClass);
		szScope.Kill();
		szLine.Kill();
		szHeader.Kill();
		cEdits.Kill();
	}
	KillUnits();
	sStale.Kill();
	sFunction.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The one function of a name that a class declares and nothing defines:
// what a member's definition was the definition of before it was edited.
// pasDeclared and pasDefined are the class's file's functions
// (FindSiblingFunctions) and psClass is the class in it.  False if there
// is none, or more than one (overloads: which one it was can't be told).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::FindUndefinedNamesake(CIDERefactorUnit* pcUnit, CArrayIDELocations* pasDeclared, CArrayIDELocations* pasDefined, SCPPSyntaxRange* psClass, const char* szOwnName, SIDESignatureSite* psDest)
{
	SIDESignatureSite	sSite;
	SIDELocation*		psLocation;
	const char*			szWhyNot;
	size				uiFound;
	size				i;

	uiFound = 0;
	for (i = 0; i < pasDeclared->NumElements(); i++)
	{
		psLocation = pasDeclared->Get(i);
		if ((psLocation->iLine < psClass->iLine) || (psLocation->iLine > psClass->iEndLine) || (pasDefined->Get(i)->iLine >= 0))
		{
			continue;
		}
		sSite.Init();
		if (mpcContent->GetChangeSignature()->ReadSite(pcUnit, psLocation, "", &sSite, &szWhyNot) && !sSite.bBody && !sSite.bPure && sSite.szOwnName.Equals(szOwnName))
		{
			uiFound++;
			if (uiFound == 1)
			{
				psDest->Kill();
				memcpy(psDest, &sSite, sizeof(SIDESignatureSite));
				continue;
			}
		}
		sSite.Kill();
	}
	return uiFound == 1;
}


//////////////////////////////////////////////////////////////////////////
//
// The definition a member that is declared and not defined had before it
// or its declaration was edited: the one definition written with its
// class's name and its own, "CFoo::Fill(", in the files its class's
// functions are defined in, that defines nothing the class declares (it
// is no symbol, or one with that definition and nothing else).  False if
// there is none, or more than one.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::FindUndeclaredDefinition(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, SIDESignatureSite* psDest)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asDeclared;
	CArrayIDELocations	asDefined;
	CArrayIDELocations	asLocations;
	CArrayChars			aszFiles;
	CArrayChars			aszRelated;
	SIDESignatureSite*	psDeclaration;
	SIDESignatureSite	sSite;
	SIDELocation		sLocation;
	CIDERefactorUnit*	pcUnit;
	CChars				szWritten;
	CChars				szKey;
	const char*			szWhyNot;
	const char*			szFound;
	char*				szText;
	size				uiFound;
	size				uiName;
	size				uiLineStart;
	size				uiDeclared;
	size				i;
	size				j;
	int32				iLine;
	uint8				uiKind;

	psDeclaration = &psFunction->sDeclaration;
	if (!psFunction->bDeclaration || !psDeclaration->bInClass || psDeclaration->szClassQualifier.Empty())
	{
		return false;
	}
	pcModel = mpcContent->GetCodeModel();

	//The files: where the class's functions are defined, its header's source files, and its header.
	aszFiles.Init();
	asDeclared.Init();
	asDefined.Init();
	pcModel->FindSiblingFunctions(psFunction->szKey.Text(), psDeclaration->szFileName.Text(), &asDeclared, &asDefined);
	for (i = 0; i < asDefined.NumElements(); i++)
	{
		if (asDefined.Get(i)->iLine >= 0)
		{
			aszFiles.AddIfUnique(asDefined.Get(i)->szFileName.Text());
		}
	}
	KillIDELocations(&asDefined);
	KillIDELocations(&asDeclared);
	aszRelated.Init();
	mpcContent->GetFileRename()->FindRelatedFiles(psDeclaration->szFileName.Text(), &aszRelated);
	for (i = 0; i < aszRelated.NumElements(); i++)
	{
		aszFiles.AddIfUnique(aszRelated.GetText(i));
	}
	aszRelated.Kill();
	aszFiles.AddIfUnique(psDeclaration->szFileName.Text());

	szWritten.Init();
	szWritten.Append(psDeclaration->szClassQualifier);
	szWritten.Append(psDeclaration->szOwnName);
	uiFound = 0;
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		szText = pcEdits->GetText(aszFiles.GetText(i));
		pcUnit = szText ? GetUnit(aszFiles.GetText(i)) : NULL;
		if (!pcUnit)
		{
			continue;
		}
		for (szFound = strstr(szText, szWritten.Text()); szFound; szFound = strstr(szFound + 1, szWritten.Text()))
		{
			if (((szFound > szText) && IsIDENameChar(szFound[-1])) || IsIDENameChar(szFound[szWritten.Length()]))
			{
				continue;
			}
			uiName = (size)(szFound - szText) + psDeclaration->szClassQualifier.Length();
			iLine = 0;
			uiLineStart = 0;
			for (j = 0; j < uiName; j++)
			{
				if (szText[j] == '\n')
				{
					iLine++;
					uiLineStart = j + 1;
				}
			}
			//What it defines is declared nowhere else.
			szKey.Init();
			uiDeclared = 0;
			if (pcModel->GetSymbolAt(aszFiles.GetText(i), iLine, (int)(uiName - uiLineStart), &szKey, &uiKind))
			{
				asLocations.Init();
				pcModel->GetSymbolLocations(szKey.Text(), &asLocations, &uiKind);
				for (j = 0; j < asLocations.NumElements(); j++)
				{
					uiDeclared += (asLocations.Get(j)->uiRole != CPPIR_Reference) ? 1 : 0;
				}
				KillIDELocations(&asLocations);
			}
			szKey.Kill();
			if (uiDeclared > 1)
			{
				continue;
			}
			sLocation.szFileName.Init(aszFiles.GetText(i));
			sLocation.iLine = iLine;
			sLocation.iColumn = (int32)(uiName - uiLineStart);
			sLocation.iLength = (int32)psDeclaration->szOwnName.Length();
			sLocation.uiRole = CPPIR_Definition;
			sLocation.uiFlags = 0;
			sLocation.uiUsage = 0;
			sLocation.iContainer = -1;
			sSite.Init();
			if (mpcContent->GetChangeSignature()->ReadSite(pcUnit, &sLocation, "", &sSite, &szWhyNot) && sSite.bBody && !sSite.bInClass)
			{
				uiFound++;
				if (uiFound == 1)
				{
					psDest->Kill();
					memcpy(psDest, &sSite, sizeof(SIDESignatureSite));
					sLocation.szFileName.Kill();
					continue;
				}
			}
			sSite.Kill();
			sLocation.szFileName.Kill();
		}
	}
	szWritten.Kill();
	aszFiles.Kill();
	return uiFound == 1;
}


//////////////////////////////////////////////////////////////////////////
//
// A parameter's type as a declaration or definition writes it, without
// its name, its default value or any space: what tells whether two write
// the same parameter.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::AppendParameterType(SIDESignatureSite* psSite, const char* szText, size uiParameter, CChars* pszDest)
{
	SIDEWrittenParameter*	psWritten;
	SCPPSyntaxRange			sRange;
	CChars					szType;

	psWritten = psSite->asParameters.Get(uiParameter);
	szType.Init();
	sRange = psWritten->sAll;
	if (psWritten->bName)
	{
		sRange.iEndLine = psWritten->sName.iLine;
		sRange.iEndColumn = psWritten->sName.iColumn;
		AppendIDERangeText(szText, &sRange, &szType);
		sRange = psWritten->sAll;
		sRange.iLine = psWritten->sName.iEndLine;
		sRange.iColumn = psWritten->sName.iEndColumn;
	}
	if (psWritten->bDefault)
	{
		sRange.iEndLine = psWritten->sDefault.iLine;
		sRange.iEndColumn = psWritten->sDefault.iColumn;
	}
	AppendIDERangeText(szText, &sRange, &szType);
	szType.Replace(" ", "");
	szType.Replace("\t", "");
	szType.Replace("\n", "");
	szType.Replace("\r", "");
	pszDest->Append(szType);
	szType.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A declaration or a definition changed to match the other (psModel):
// its parameters, what it returns and what follows its parameters (const)
// become the model's, as edits of its file.  A declaration keeps what
// only it writes: its parameters' default values, where each still has
// one after it, and override and final.  A definition keeps the name it
// gave a parameter whose type is as it was, since its body uses it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDefinitions::MatchSite(SIDESignatureSite* psChanged, const char* szChangedText, SIDESignatureSite* psModel, const char* szModelText, bool bDeclaration, CIDETextEdits* pcEdits)
{
	SIDEWrittenParameter*	psWritten;
	SCPPSyntaxRange			sRange;
	CArrayChars				aszParameters;
	CArrayChars				aszDefaults;
	CChars					szNew;
	CChars					szOld;
	CChars					szType;
	CChars					szOtherType;
	size					uiCount;
	size					uiStart;
	size					uiName;
	size					uiLineStart;
	size					uiNameColumn;
	size					uiReturnEnd;
	size					i;
	bool					bDefaults;
	bool					bKeep;
	bool					bTabs;

	//Its parameters.
	uiCount = psModel->asParameters.NumElements();
	aszParameters.Init();
	aszDefaults.Init();
	for (i = 0; i < uiCount; i++)
	{
		bKeep = false;
		if (!bDeclaration && (i < psChanged->asParameters.NumElements()) && psChanged->asParameters.Get(i)->bName)
		{
			szType.Init();
			szOtherType.Init();
			AppendParameterType(psChanged, szChangedText, i, &szType);
			AppendParameterType(psModel, szModelText, i, &szOtherType);
			bKeep = szType.Equals(szOtherType.Text());
			szOtherType.Kill();
			szType.Kill();
		}
		psWritten = bKeep ? psChanged->asParameters.Get(i) : psModel->asParameters.Get(i);
		sRange = psWritten->sAll;
		if (psWritten->bDefault)
		{
			sRange.iEndLine = psWritten->sDefault.iLine;
			sRange.iEndColumn = psWritten->sDefault.iColumn;
		}
		szNew.Init();
		AppendIDERangeText(bKeep ? szChangedText : szModelText, &sRange, &szNew);
		szNew.StripWhitespace();
		aszParameters.Add(szNew.Text());
		szNew.Kill();

		szNew.Init();
		if (bDeclaration && (i < psChanged->asParameters.NumElements()) && psChanged->asParameters.Get(i)->bDefault)
		{
			AppendIDERangeText(szChangedText, &psChanged->asParameters.Get(i)->sDefault, &szNew);
			szNew.StripWhitespace();
		}
		aszDefaults.Add(szNew.Text());
		szNew.Kill();
	}
	szNew.Init();
	if (uiCount == 0)
	{
		AppendIDERangeText(szModelText, &psModel->sParameters, &szNew);
		szNew.StripWhitespace();
	}
	//A default value stays where every parameter after it has one.
	bDefaults = true;
	for (i = uiCount; i > 0; i--)
	{
		bDefaults = bDefaults && !aszDefaults.Get(i - 1)->Empty();
		if (!bDefaults)
		{
			aszDefaults.Get(i - 1)->Clear();
		}
	}
	for (i = 0; i < uiCount; i++)
	{
		szNew.Append((i > 0) ? ", " : "");
		szNew.Append(aszParameters.GetText(i));
		if (!aszDefaults.Get(i)->Empty())
		{
			szNew.Append(' ');
			szNew.Append(aszDefaults.GetText(i));
		}
	}
	pcEdits->Replace(FindIDETextOffset(szChangedText, psChanged->sParameters.iLine, psChanged->sParameters.iColumn),
					 FindIDETextOffset(szChangedText, psChanged->sParameters.iEndLine, psChanged->sParameters.iEndColumn), szNew.Text());
	szNew.Kill();
	aszDefaults.Kill();
	aszParameters.Kill();

	//What it returns.  Its name stays in its column where tabs put it there (a class's declarations are lined up);
	//otherwise the space after what it returns stays as it is.
	if (psChanged->bReturn && psModel->bReturn)
	{
		szOld.Init();
		szNew.Init();
		AppendIDERangeText(szChangedText, &psChanged->sReturn, &szOld);
		AppendIDERangeText(szModelText, &psModel->sReturn, &szNew);
		szOld.StripWhitespace();
		szNew.StripWhitespace();
		if (!szOld.Equals(szNew.Text()))
		{
			uiStart = FindIDETextOffset(szChangedText, psChanged->sReturn.iLine, psChanged->sReturn.iColumn);
			uiName = FindIDETextOffset(szChangedText, psChanged->sName.iLine, psChanged->sName.iColumn);
			bTabs = uiName > uiStart + szOld.Length();
			for (i = uiStart + szOld.Length(); i < uiName; i++)
			{
				bTabs = bTabs && (szChangedText[i] == '\t');
			}
			if (bTabs)
			{
				uiLineStart = FindIDETextOffset(szChangedText, psChanged->sReturn.iLine, 0);
				uiNameColumn = FindIDEVisualColumn(szChangedText, uiLineStart, uiName, 4);
				uiReturnEnd = FindIDEVisualColumn(szChangedText, uiLineStart, uiStart, 4) + szNew.Length();
				if (uiNameColumn > uiReturnEnd)
				{
					AppendIDETabsTo(&szNew, uiReturnEnd, uiNameColumn, 4);
				}
				else
				{
					szNew.Append('\t');
				}
				pcEdits->Replace(uiStart, uiName, szNew.Text());
			}
			else
			{
				pcEdits->Replace(uiStart, uiStart + szOld.Length(), szNew.Text());
			}
		}
		szNew.Kill();
		szOld.Kill();
	}

	//What follows its parameters.
	szOld.Init();
	szNew.Init();
	AppendIDERangeText(szChangedText, &psChanged->sAfter, &szOld);
	szNew.Append(psModel->szQualifiers);
	if (bDeclaration)
	{
		szNew.Append(szOld.Contains("override") ? " override" : "");
		szNew.Append(szOld.Contains("final") ? " final" : "");
	}
	if (!szOld.Equals(szNew.Text()))
	{
		pcEdits->Replace(FindIDETextOffset(szChangedText, psChanged->sAfter.iLine, psChanged->sAfter.iColumn),
						 FindIDETextOffset(szChangedText, psChanged->sAfter.iEndLine, psChanged->sAfter.iEndColumn), szNew.Text());
	}
	szNew.Kill();
	szOld.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A function outside any class that is defined in a header (inline) is
// defined in the header's source file instead, without its inline, where
// PutDefinition puts it.  In the header its definition goes, with the
// comment over it, if the header declares it too; otherwise it becomes
// its declaration, there.  A function template stays where it is.  False,
// with why, if it can't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::MoveFunctionToSource(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, CChars* pszFileName, const char** pszWhyNot)
{
	SIDESignatureSite*	psDefinition;
	CChars				szBody;
	CChars				szOutside;
	CChars				szDeclaration;
	CChars				szPart;
	char*				szText;
	size				uiStart;
	size				uiEnd;
	size				uiAt;
	bool				bResult;

	psDefinition = &psFunction->sDefinition;
	if (!psDefinition->szOwnTemplate.Empty())
	{
		*pszWhyNot = "a function template is defined in its header";
		return false;
	}
	szText = pcEdits->GetText(psDefinition->szFileName.Text());
	if (!szText)
	{
		*pszWhyNot = "its file couldn't be opened";
		return false;
	}
	szBody.Init();
	szOutside.Init();
	AppendIDERangeText(szText, &psDefinition->sBody, &szBody);
	AppendBodyOutside(szBody.Text(), &szOutside);
	uiAt = 0;
	bResult = PutDefinition(psFunction, pcEdits, IDE_FILES_SOURCE, NULL, NULL, szOutside.Text(), pszFileName, &uiAt, pszWhyNot);
	if (bResult)
	{
		uiStart = FindIDETextOffset(szText, psDefinition->sAll.iLine, psDefinition->sAll.iColumn);
		uiEnd = FindIDETextOffset(szText, psDefinition->sAll.iEndLine, psDefinition->sAll.iEndColumn);
		if (psFunction->bDeclaration)
		{
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			pcEdits->GetEdits(psDefinition->szFileName.Text())->Replace(uiStart, uiEnd, "");
		}
		else
		{
			//Its declaration, where its definition and the comment over that were.
			szDeclaration.Init();
			if (psDefinition->bReturn)
			{
				szPart.Init();
				AppendIDERangeText(szText, &psDefinition->sReturn, &szPart);
				szPart.StripWhitespace();
				szDeclaration.Append(szPart)->Append(' ');
				szPart.Kill();
			}
			AppendIDERangeText(szText, &psDefinition->sName, &szDeclaration);
			szDeclaration.Append('(');
			AppendParameters(psDefinition, szText, true, &szDeclaration);
			szDeclaration.Append(')');
			szDeclaration.Append(psDefinition->szQualifiers);
			szDeclaration.Append(';');
			pcEdits->GetEdits(psDefinition->szFileName.Text())->Replace(FindBannerStart(szText, uiStart), uiEnd, szDeclaration.Text());
			szDeclaration.Kill();
		}
	}
	szOutside.Kill();
	szBody.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A function outside any class that a header declares and a source file
// defines is defined in the header, inline: after the header's other
// definitions or before its #endif (PutDefinition), and it stays declared
// where it was, with its default values.  Its definition in the source
// file goes with the comment over it.  The definition is written as the
// source file wrote it: its parameters' names are the ones its body
// uses.  False, with why, if it can't be.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::MoveFunctionToHeader(SIDEFunction* psFunction, CIDEFileEdits* pcEdits, CChars* pszFileName, const char** pszWhyNot)
{
	SIDESignatureSite*	psDefinition;
	SIDEFunction		sWritten;
	CChars				szBanner;
	CChars				szDefinition;
	CChars				szBody;
	CChars				szOutside;
	char*				szText;
	char*				szHeaderText;
	size				uiStart;
	size				uiEnd;
	size				uiAt;
	int					iWhere;

	psDefinition = &psFunction->sDefinition;
	szText = pcEdits->GetText(psDefinition->szFileName.Text());
	szHeaderText = pcEdits->GetText(psFunction->sDeclaration.szFileName.Text());
	if (!szText || !szHeaderText)
	{
		*pszWhyNot = "a file couldn't be opened";
		return false;
	}
	szBanner.Init();
	uiAt = 0;
	iWhere = IDE_PLACE_END;
	if (!FindDefinitionPlace(psFunction, pcEdits, IDE_FILES_HEADER, pszFileName, &uiAt, &iWhere, &szBanner, pszWhyNot))
	{
		szBanner.Kill();
		return false;
	}

	//The definition as its own file writes it (AppendDefinition writes from the declaration, if there is one).
	memcpy(&sWritten, psFunction, sizeof(SIDEFunction));
	sWritten.bDeclaration = false;
	szBody.Init();
	szOutside.Init();
	szDefinition.Init();
	AppendIDERangeText(szText, &psDefinition->sBody, &szBody);
	AppendBodyOutside(szBody.Text(), &szOutside);
	if (iWhere != IDE_PLACE_BEFORE)
	{
		szDefinition.Append(((uiAt > 0) && (szHeaderText[uiAt - 1] != '\n')) ? "\n\n\n" : "\n\n");
	}
	AppendDefinition(&sWritten, szText, szBanner.Text(), "inline ", NULL, szOutside.Text(), &szDefinition);
	if (iWhere == IDE_PLACE_BEFORE)
	{
		szDefinition.Append("\n\n");
	}
	pcEdits->GetEdits(pszFileName->Text())->Insert(uiAt, szDefinition.Text());

	uiStart = FindIDETextOffset(szText, psDefinition->sAll.iLine, psDefinition->sAll.iColumn);
	uiEnd = FindIDETextOffset(szText, psDefinition->sAll.iEndLine, psDefinition->sAll.iEndColumn);
	ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
	pcEdits->GetEdits(psDefinition->szFileName.Text())->Replace(uiStart, uiEnd, "");
	szDefinition.Kill();
	szOutside.Kill();
	szBody.Kill();
	szBanner.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A function defined in its class is left declared there, its body and a
// constructor's initialisers gone from it, and defined outside it with
// them (PutDefinition).  False (the status line says why) if it isn't
// defined in its class or has nowhere to go.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::MoveDefinitionOut(void)
{
	SIDEFunction		sFunction;
	CIDEFileEdits		cEdits;
	SIDESignatureSite*	psDefinition;
	SCPPSyntaxRange		sRange;
	CChars				szFileName;
	CChars				szBody;
	CChars				szOutside;
	CChars				szInitialisers;
	CChars				szMessage;
	const char*			szWhyNot;
	char*				szText;
	size				uiAt;
	bool				bResult;

	sFunction.Init();
	bResult = ReadFunction(&sFunction, "move the definition");
	if (bResult)
	{
		psDefinition = &sFunction.sDefinition;
		szWhyNot = NULL;
		cEdits.Init(mpcContent);
		szFileName.Init();
		if (sFunction.bDefinition && !psDefinition->bInClass && (!sFunction.bDeclaration || !sFunction.sDeclaration.bInClass) && IsIDEHeaderFile(psDefinition->szFileName.Text()))
		{
			//A function outside any class, defined in a header: to the header's source file.
			bResult = MoveFunctionToSource(&sFunction, &cEdits, &szFileName, &szWhyNot);
			if (bResult)
			{
				cEdits.Apply();
				szMessage.Init("Moved ");
				szMessage.Append(sFunction.szName);
				szMessage.Append("'s definition out of its header, to ");
				szMessage.Append(GetIDEFileNamePart(szFileName.Text()));
				szMessage.Append(".");
				mpcContent->ShowStatus(szMessage.Text());
				szMessage.Kill();
			}
			else
			{
				Refuse("move the definition", &sFunction, szWhyNot);
			}
			szFileName.Kill();
			cEdits.Kill();
			KillUnits();
			sFunction.Kill();
			return bResult;
		}
		if (!sFunction.bDefinition || !psDefinition->bInClass)
		{
			szWhyNot = "it isn't defined in its class";
		}
		szBody.Init();
		szOutside.Init();
		szInitialisers.Init();
		szText = szWhyNot ? NULL : cEdits.GetText(psDefinition->szFileName.Text());
		if (!szWhyNot && !szText)
		{
			szWhyNot = "its file couldn't be opened";
		}
		if (!szWhyNot)
		{
			AppendIDERangeText(szText, &psDefinition->sBody, &szBody);
			AppendBodyOutside(szBody.Text(), &szOutside);
			sRange = psDefinition->sBody;
			sRange.iLine = psDefinition->sAfter.iEndLine;
			sRange.iColumn = psDefinition->sAfter.iEndColumn;
			sRange.iEndLine = psDefinition->sBody.iLine;
			sRange.iEndColumn = psDefinition->sBody.iColumn;
			AppendIDERangeText(szText, &sRange, &szInitialisers);
			szInitialisers.StripWhitespace(true);
		}
		uiAt = 0;
		bResult = !szWhyNot && PutDefinition(&sFunction, &cEdits, IDE_FILES_ANY, NULL, szInitialisers.Text(), szOutside.Text(), &szFileName, &uiAt, &szWhyNot);
		if (bResult)
		{
			//In the class: what follows its parameters stays, and a ; is where its initialisers and body were.
			cEdits.GetEdits(psDefinition->szFileName.Text())->Replace(FindIDETextOffset(szText, psDefinition->sAfter.iEndLine, psDefinition->sAfter.iEndColumn), FindIDETextOffset(szText, psDefinition->sBody.iEndLine, psDefinition->sBody.iEndColumn), ";");
			cEdits.Apply();
			szMessage.Init("Moved ");
			szMessage.Append(sFunction.szName);
			szMessage.Append("'s definition out of its class, to ");
			szMessage.Append(GetIDEFileNamePart(szFileName.Text()));
			szMessage.Append(".");
			mpcContent->ShowStatus(szMessage.Text());
			szMessage.Kill();
		}
		else
		{
			Refuse("move the definition", &sFunction, szWhyNot);
		}
		szInitialisers.Kill();
		szOutside.Kill();
		szBody.Kill();
		szFileName.Kill();
		cEdits.Kill();
	}
	KillUnits();
	sFunction.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A function declared in its class and defined outside it is defined in
// the class: its declaration's ; becomes its body (AppendBodyInClass)
// after a constructor's initialisers, its parameters take the names the
// definition gave them (the body uses those), and the definition
// outside is taken out with the comment over it.  False (the status line
// says why) if it isn't declared in a class and defined outside it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDefinitions::MoveDefinitionIn(void)
{
	SIDEFunction			sFunction;
	CIDEFileEdits			cEdits;
	SIDESignatureSite*		psDeclaration;
	SIDESignatureSite*		psDefinition;
	SIDEWrittenParameter*	psDeclared;
	SIDEWrittenParameter*	psDefined;
	SCPPSyntaxRange			sRange;
	CChars					szBody;
	CChars					szOutside;
	CChars					szInClass;
	CChars					szInitialisers;
	CChars					szIndent;
	CChars					szName;
	CChars					szMessage;
	const char*				szWhyNot;
	char*					szText;
	char*					szDefinitionText;
	size					uiStart;
	size					uiEnd;
	size					uiLineStart;
	size					i;
	bool					bResult;

	sFunction.Init();
	bResult = ReadFunction(&sFunction, "move the definition");
	if (bResult)
	{
		psDeclaration = &sFunction.sDeclaration;
		psDefinition = &sFunction.sDefinition;
		szWhyNot = NULL;
		if (sFunction.bDeclaration && sFunction.bDefinition && !psDeclaration->bInClass && !psDefinition->bInClass && IsIDEHeaderFile(psDeclaration->szFileName.Text()) &&
			!IsIDEHeaderFile(psDefinition->szFileName.Text()))
		{
			//A function outside any class, declared in a header and defined in a source file: into the header, inline.
			cEdits.Init(mpcContent);
			szName.Init();
			bResult = MoveFunctionToHeader(&sFunction, &cEdits, &szName, &szWhyNot);
			if (bResult)
			{
				cEdits.Apply();
				szMessage.Init("Moved ");
				szMessage.Append(sFunction.szName);
				szMessage.Append("'s definition into its header, ");
				szMessage.Append(GetIDEFileNamePart(szName.Text()));
				szMessage.Append(", from ");
				szMessage.Append(GetIDEFileNamePart(psDefinition->szFileName.Text()));
				szMessage.Append(".");
				mpcContent->ShowStatus(szMessage.Text());
				szMessage.Kill();
			}
			else
			{
				Refuse("move the definition", &sFunction, szWhyNot);
			}
			szName.Kill();
			cEdits.Kill();
			KillUnits();
			sFunction.Kill();
			return bResult;
		}
		if (!sFunction.bDeclaration || !psDeclaration->bInClass)
		{
			szWhyNot = (sFunction.bDefinition && psDefinition->bInClass) ? "it is defined in its class already" : "it isn't declared in a class";
		}
		else if (!sFunction.bDefinition)
		{
			szWhyNot = "it has no definition";
		}
		cEdits.Init(mpcContent);
		szText = szWhyNot ? NULL : cEdits.GetText(psDeclaration->szFileName.Text());
		szDefinitionText = szWhyNot ? NULL : cEdits.GetText(psDefinition->szFileName.Text());
		if (!szWhyNot && (!szText || !szDefinitionText))
		{
			szWhyNot = "a file couldn't be opened";
		}
		if (!szWhyNot)
		{
			szBody.Init();
			szOutside.Init();
			szInClass.Init();
			szInitialisers.Init();
			szIndent.Init();
			AppendIDERangeText(szDefinitionText, &psDefinition->sBody, &szBody);
			AppendBodyOutside(szBody.Text(), &szOutside);
			sRange = psDefinition->sBody;
			sRange.iLine = psDefinition->sAfter.iEndLine;
			sRange.iColumn = psDefinition->sAfter.iEndColumn;
			sRange.iEndLine = psDefinition->sBody.iLine;
			sRange.iEndColumn = psDefinition->sBody.iColumn;
			AppendIDERangeText(szDefinitionText, &sRange, &szInitialisers);
			szInitialisers.StripWhitespace(true);

			uiLineStart = FindIDETextOffset(szText, psDeclaration->sAll.iLine, 0);
			for (i = uiLineStart; (szText[i] == ' ') || (szText[i] == '\t'); i++)
			{
			}
			szIndent.AppendSubString(szText, uiLineStart, i);
			if (!szInitialisers.Empty())
			{
				szInClass.Append(' ')->Append(szInitialisers);
			}
			AppendBodyInClass(szOutside.Text(), szIndent.Text(), &szInClass);

			//The declaration's ; (and nothing else: it isn't pure) becomes the body.
			uiStart = FindIDETextOffset(szText, psDeclaration->sAfter.iEndLine, psDeclaration->sAfter.iEndColumn);
			uiEnd = FindIDETextOffset(szText, psDeclaration->sAll.iEndLine, psDeclaration->sAll.iEndColumn);
			for (; (szText[uiEnd] == ' ') || (szText[uiEnd] == '\t'); uiEnd++)
			{
			}
			if (szText[uiEnd] == ';')
			{
				uiEnd++;
			}
			cEdits.GetEdits(psDeclaration->szFileName.Text())->Replace(uiStart, uiEnd, szInClass.Text());

			//Its parameters are named as the definition names them.
			for (i = 0; (i < psDeclaration->asParameters.NumElements()) && (i < psDefinition->asParameters.NumElements()); i++)
			{
				psDeclared = psDeclaration->asParameters.Get(i);
				psDefined = psDefinition->asParameters.Get(i);
				if (!psDefined->bName)
				{
					continue;
				}
				szName.Init();
				AppendIDERangeText(szDefinitionText, &psDefined->sName, &szName);
				uiStart = FindIDETextOffset(szText, psDeclared->sName.iLine, psDeclared->sName.iColumn);
				uiEnd = FindIDETextOffset(szText, psDeclared->sName.iEndLine, psDeclared->sName.iEndColumn);
				if ((uiEnd - uiStart != szName.Length()) || (strncmp(&szText[uiStart], szName.Text(), szName.Length()) != 0))
				{
					if (!psDeclared->bName)
					{
						szName.Insert(0, ' ');
					}
					cEdits.GetEdits(psDeclaration->szFileName.Text())->Replace(uiStart, uiEnd, szName.Text());
				}
				szName.Kill();
			}

			//The definition outside goes, with the comment over it.
			uiStart = FindIDETextOffset(szDefinitionText, psDefinition->sAll.iLine, psDefinition->sAll.iColumn);
			uiEnd = FindIDETextOffset(szDefinitionText, psDefinition->sAll.iEndLine, psDefinition->sAll.iEndColumn);
			ExtendIDEDeletion(szDefinitionText, &uiStart, &uiEnd, true);
			cEdits.GetEdits(psDefinition->szFileName.Text())->Replace(uiStart, uiEnd, "");
			cEdits.Apply();

			szMessage.Init("Moved ");
			szMessage.Append(sFunction.szName);
			szMessage.Append("'s definition into its class, from ");
			szMessage.Append(GetIDEFileNamePart(psDefinition->szFileName.Text()));
			szMessage.Append(".");
			mpcContent->ShowStatus(szMessage.Text());
			szMessage.Kill();
			szIndent.Kill();
			szInitialisers.Kill();
			szInClass.Kill();
			szOutside.Kill();
			szBody.Kill();
		}
		else
		{
			Refuse("move the definition", &sFunction, szWhyNot);
			bResult = false;
		}
		cEdits.Kill();
	}
	KillUnits();
	sFunction.Kill();
	return bResult;
}
