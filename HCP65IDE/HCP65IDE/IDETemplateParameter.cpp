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
#include "BaseLib/StringHelper.h"
#include "BaseLib/PointerFunctions.h"
#include "CppCompilerLib/CPPSyntaxes.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEMemberQualifier.h"
#include "IDETemplateParameter.h"
#include "IDEContextActions.h"


//The language's own types that can be a parameter: one word, written by itself.
static const char*	gaszIDEOwnTypes[] = { "int", "char", "float", "double", "bool", "long", "short" };
static const char*	gaszIDETypeWords[] = { "unsigned", "signed", "long", "short", "int", "char", "double" };


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETemplateExtraction::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szType.Init();
	szTypeKey.Init();
	sSignature.Init();
	szSuggested.Init();
	bDeduced = false;
	bClass = false;
	szClassKey.Init();
	szClassFile.Init();
	memset(&sClass, 0, sizeof(SCPPSyntaxRange));
	szSuggestedClass.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETemplateExtraction::Kill(void)
{
	szSuggestedClass.Kill();
	szClassFile.Kill();
	szClassKey.Kill();
	szSuggested.Kill();
	sSignature.Kill();
	szTypeKey.Kill();
	szType.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::Save(CObjectWriter* pcFile) { return false; }
bool CIDETemplateParameter::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's nearest is Extract Type Parameter... no: it has none for a
// method's.  No key.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.ExtractTemplateParameter", "Extract Template Parameter...", "", Ptr<CImageCel>(), this, &CIDETemplateParameter::QueueExtract)->SetEnabledCheck(this, &CIDETemplateParameter::CanExtract);
	pCommands->Add("Refactor.RemoveTemplateParameter", "Remove Template Parameter", "", Ptr<CImageCel>(), this, &CIDETemplateParameter::QueueRemove)->SetEnabledCheck(this, &CIDETemplateParameter::CanExtract);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ExtractTemplateParameter"));
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.RemoveTemplateParameter"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: a type where it is used can be made a template
// parameter, and a template's parameter taken away.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bType)
	{
		psContext->Offer("Refactor.ExtractTemplateParameter");
	}
	if (psContext->bSymbol && ((psContext->uiKind == CPPSK_TypeParameter) || (psContext->uiKind == CPPSK_ValueParameter)))
	{
		psContext->Offer("Refactor.RemoveTemplateParameter");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The editor has a C++ file: what the caret is on is found when asked.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::CanExtract(void)
{
	EIDEFileType	eType;

	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	eType = CIDEIcons::GetFileType(mpcContent->GetDocuments()->GetActiveTab()->GetFileName());
	return (eType == IFT_CPP) || (eType == IFT_Header);
}


void CIDETemplateParameter::QueueExtract(void) { mpcContent->GetWindow()->Post(this, &CIDETemplateParameter::ShowQueuedExtract); }
void CIDETemplateParameter::ShowQueuedExtract(void) { ShowExtract(); }
void CIDETemplateParameter::QueueRemove(void) { mpcContent->GetWindow()->Post(this, &CIDETemplateParameter::RemoveQueued); }
void CIDETemplateParameter::RemoveQueued(void) { RemoveParameter(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::Refuse(SIDETemplateExtraction* psDest, const char* szWhyNot)
{
	if (psDest->bPossible)
	{
		psDest->bPossible = false;
		psDest->szWhyNot.Set(szWhyNot);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Where a range of a file names the type, in the file's text and in
// order; with each, whether typename must go before it there (a type of
// its own is named through it: "CGauge::SMark" is "typename M::SMark").
// A type of the solution's is found by the index; one of the language's
// own by its word, where it is the whole of a type ("int", not "unsigned
// int").
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::FindTypePlaces(SIDETemplateExtraction* psExtraction, CIDERefactorUnit* pcUnit, const char* szFileName, const char* szText, SCPPSyntaxRange* psRange, CArrayInt* paiStarts, CArrayInt* paiTypenames)
{
	CIDECodeModel*		pcModel;
	CArrayIDELocations	asLocations;
	SIDELocation*		psLocation;
	SCPPSyntaxRange		sToken;
	CChars				szKey;
	size				i;
	size				j;
	size				k;
	int32				iToken;
	int32				iFirst;
	int32				iLast;
	int					iStart;
	int					iTypename;
	uint8				uiKind;
	bool				bPart;

	pcModel = mpcContent->GetCodeModel();
	if (!psExtraction->szTypeKey.Empty())
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(psExtraction->szTypeKey.Text(), &asLocations, &uiKind);
		for (i = 0; i < asLocations.NumElements(); i++)
		{
			psLocation = asLocations.Get(i);
			if ((psLocation->uiRole != CPPIR_Reference) || (StringInsensitiveCompare(psLocation->szFileName.Text(), szFileName) != 0) ||
				(psLocation->iLine < psRange->iLine) || ((psLocation->iLine == psRange->iLine) && (psLocation->iColumn < psRange->iColumn)) ||
				(psLocation->iLine > psRange->iEndLine) || ((psLocation->iLine == psRange->iEndLine) && (psLocation->iColumn >= psRange->iEndColumn)))
			{
				continue;
			}
			iToken = pcUnit->FindTokenAt(psLocation->iLine, psLocation->iColumn);
			iTypename = 0;
			szKey.Init();
			if ((iToken != -1) && pcUnit->IsToken(iToken + 1, "::") && pcUnit->GetTokensRange(iToken + 2, iToken + 2, &sToken) &&
				pcModel->GetSymbolAt(szFileName, sToken.iLine, sToken.iColumn, &szKey, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_Enum) || (uiKind == CPPSK_Typedef)))
			{
				iTypename = 1;
			}
			szKey.Kill();
			paiStarts->Add((int)FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn));
			paiTypenames->Add(iTypename);
		}
		KillIDELocations(&asLocations);
	}
	else if (pcUnit->FindTokensIn(psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &iFirst, &iLast))
	{
		for (iToken = iFirst; iToken <= iLast; iToken++)
		{
			if (!pcUnit->IsToken(iToken, psExtraction->szType.Text()) || !pcUnit->GetTokensRange(iToken, iToken, &sToken))
			{
				continue;
			}
			bPart = false;
			for (k = 0; k < sizeof(gaszIDETypeWords) / sizeof(char*); k++)
			{
				bPart |= pcUnit->IsToken(iToken - 1, gaszIDETypeWords[k]) || pcUnit->IsToken(iToken + 1, gaszIDETypeWords[k]);
			}
			if (!bPart)
			{
				paiStarts->Add((int)FindIDETextOffset(szText, sToken.iLine, sToken.iColumn));
				paiTypenames->Add(0);
			}
		}
	}

	//In the text's order.
	for (i = 1; i < paiStarts->NumElements(); i++)
	{
		iStart = paiStarts->GetValue(i);
		iTypename = paiTypenames->GetValue(i);
		for (j = i; (j > 0) && (paiStarts->GetValue(j - 1) > iStart); j--)
		{
			paiStarts->SetValue(j, paiStarts->GetValue(j - 1));
			paiTypenames->SetValue(j, paiTypenames->GetValue(j - 1));
		}
		paiStarts->SetValue(j, iStart);
		paiTypenames->SetValue(j, iTypename);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A piece of a text with the type made the parameter szName at each of
// its places in the piece, added to pszDest.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::AppendReplaced(SIDETemplateExtraction* psExtraction, const char* szName, const char* szText, size uiStart, size uiEnd, CArrayInt* paiStarts, CArrayInt* paiTypenames, CChars* pszDest)
{
	size	uiAt;
	size	uiPlace;
	size	i;

	uiAt = uiStart;
	for (i = 0; i < paiStarts->NumElements(); i++)
	{
		uiPlace = (size)paiStarts->GetValue(i);
		if ((uiPlace < uiAt) || (uiPlace >= uiEnd))
		{
			continue;
		}
		pszDest->AppendSubString(szText, uiAt, uiPlace);
		pszDest->Append(paiTypenames->GetValue(i) ? "typename " : "")->Append(szName);
		uiAt = uiPlace + psExtraction->szType.Length();
	}
	pszDest->AppendSubString(szText, uiAt, uiEnd);
}


//////////////////////////////////////////////////////////////////////////
//
// What Extract Template Parameter would work on: the type the caret is
// on and the function it is named in, with why it can't be if it can't.
// False if the caret isn't on a type in a function.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::FindExtraction(SIDETemplateExtraction* psDest)
{
	CIDECodeModel*					pcModel;
	CIDERefactorUnit				cUnit;
	Ptr<CTextArea>					pEditor;
	CArrayInt						aiStarts;
	CArrayInt						aiTypenames;
	SIDESignatureSite*				psSite;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcWalk;
	CCPPSyntaxDeclarator*			pcDeclarator;
	CCPPSyntaxSimpleDeclaration*	pcDeclaration;
	CCPPSyntaxName*					pcName;
	SCPPSyntaxRange					sRange;
	CChars							szScope;
	CChars							szClassName;
	CTextFile						cFile;
	const char*						szFileName;
	char*							szText;
	size							uiLength;
	size							uiStart;
	size							uiEnd;
	size							uiLine;
	size							i;
	int								iLine;
	int								iColumn;
	int32							iToken;
	uint8							uiKind;
	bool							bFound;

	if (!CanExtract())
	{
		return false;
	}
	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szFileName = mpcContent->GetDocuments()->GetActiveTab()->GetFileName();
	pEditor = mpcContent->GetDocuments()->GetActiveTab()->GetEditor();
	szText = pEditor->GetText();
	if (!pEditor->GetSelection(&uiStart, &uiEnd))
	{
		uiStart = pEditor->GetCaretPosition();
	}
	if (!IsIDENameChar(szText[uiStart]) && (uiStart > 0) && IsIDENameChar(szText[uiStart - 1]))
	{
		uiStart--;
	}
	for (; (uiStart > 0) && IsIDENameChar(szText[uiStart - 1]); uiStart--)
	{
	}
	for (uiEnd = uiStart; IsIDENameChar(szText[uiEnd]); uiEnd++)
	{
	}
	psDest->szType.AppendSubString(szText, uiStart, uiEnd);
	uiLine = pEditor->FindLine(uiStart);
	iLine = (int)uiLine;
	iColumn = (int)(uiStart - pEditor->GetLineStart(uiLine));

	//A type of the solution's, or one of the language's own.
	bFound = pcModel->GetSymbolAt(szFileName, iLine, iColumn, &psDest->szTypeKey, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_Enum) || (uiKind == CPPSK_Typedef));
	if (!bFound)
	{
		psDest->szTypeKey.Clear();
		for (i = 0; i < sizeof(gaszIDEOwnTypes) / sizeof(char*); i++)
		{
			bFound |= psDest->szType.Equals(gaszIDEOwnTypes[i]);
		}
	}
	if (!bFound)
	{
		return false;
	}

	//The function it is named in: its definition, or a declaration of it.
	cUnit.Init();
	bFound = false;
	if (cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), szFileName, false))
	{
		iToken = cUnit.FindTokenAt(iLine, iColumn);
		pcNode = (iToken != -1) ? cUnit.FindNodeOfTokens(iToken, iToken) : NULL;
		pcDeclarator = NULL;
		pcWalk = cUnit.FindAncestor(pcNode, CPPS_FunctionDefinition);
		if (pcWalk)
		{
			pcDeclarator = ((CCPPSyntaxFunctionDefinition*)pcWalk)->mpcDeclarator;
		}
		for (pcWalk = pcNode; pcWalk && !pcDeclarator; pcWalk = pcWalk->mpcParent)
		{
			if ((pcWalk->GetKind() == CPPS_Declarator) && ((CCPPSyntaxDeclarator*)pcWalk)->IsFunction())
			{
				pcDeclarator = (CCPPSyntaxDeclarator*)pcWalk;
			}
			else if (pcWalk->GetKind() == CPPS_SimpleDeclaration)
			{
				pcDeclaration = (CCPPSyntaxSimpleDeclaration*)pcWalk;
				if ((pcDeclaration->macpDeclarators.NumElements() == 1) && (pcDeclaration->macpDeclarators.GetPtr(0)->GetKind() == CPPS_InitDeclarator) &&
					((CCPPSyntaxInitDeclarator*)pcDeclaration->macpDeclarators.GetPtr(0))->mpcDeclarator && ((CCPPSyntaxInitDeclarator*)pcDeclaration->macpDeclarators.GetPtr(0))->mpcDeclarator->IsFunction())
				{
					pcDeclarator = ((CCPPSyntaxInitDeclarator*)pcDeclaration->macpDeclarators.GetPtr(0))->mpcDeclarator;
				}
				break;
			}
		}
		pcName = pcDeclarator ? pcDeclarator->GetName() : NULL;
		bFound = pcName && pcName->GetLast() && cUnit.GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sRange) &&
				 mpcContent->GetChangeSignature()->FindSignature(&psDest->sSignature, szFileName, sRange.iLine, sRange.iColumn);
		if (!bFound && !pcDeclarator)
		{
			//Not in a function: in a class, then, which is what becomes a template.
			FindClass(psDest, &cUnit, pcNode, szFileName);
		}
	}
	cUnit.Kill();
	if (psDest->bClass)
	{
		psDest->szSuggested.Set("M");
		return true;
	}
	if (!bFound)
	{
		return false;
	}

	psDest->bPossible = true;
	szScope.Init();
	szClassName.Init();
	GetIDESymbolScope(psDest->sSignature.szKey.Text(), &szScope);
	GetIDESymbolName(szScope.Text(), &szClassName);
	if (!psDest->sSignature.bChangeable)
	{
		Refuse(psDest, psDest->sSignature.szWhyNot.Text());
	}
	else if ((psDest->sSignature.uiOverrides > 0) || (psDest->sSignature.uiFamily > 0) || mpcContent->GetMemberQualifier()->IsDeclaredWith(psDest->sSignature.szKey.Text(), "virtual"))
	{
		Refuse(psDest, "its function is virtual, or a template's body calls it with other classes' functions: a virtual function can't be a template");
	}
	else if (psDest->sSignature.asOtherUses.NumElements() > 0)
	{
		Refuse(psDest, "its function is named without being called");
	}
	else if (!szScope.Empty() && (psDest->sSignature.szOldName.Equals(szClassName.Text()) || psDest->sSignature.szOldName.StartsWith("~")))
	{
		Refuse(psDest, "a constructor or a destructor can't be a template here");
	}
	szClassName.Kill();
	szScope.Kill();
	for (i = 0; psDest->bPossible && (i < psDest->sSignature.apsSites.NumElements()); i++)
	{
		psSite = psDest->sSignature.apsSites.GetPtr(i);
		if (!psSite->szOwnTemplate.Empty() || !psSite->szClassTemplate.Empty())
		{
			Refuse(psDest, "its function is a template already, or a class template's");
		}
	}

	//Whether a call's arguments say what the type is: it is in the first declaration's parameters.
	if (psDest->bPossible)
	{
		psSite = psDest->sSignature.apsSites.GetPtr(0);
		cUnit.Init();
		aiStarts.Init();
		aiTypenames.Init();
		cFile.Init();
		if (mpcContent->GetFindInFiles()->GetFileText(psSite->szFileName.Text(), &cFile, &szText, &uiLength) && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psSite->szFileName.Text(), false))
		{
			FindTypePlaces(psDest, &cUnit, psSite->szFileName.Text(), szText, &psSite->sParameters, &aiStarts, &aiTypenames);
		}
		psDest->bDeduced = aiStarts.NumElements() > 0;
		cFile.Kill();
		aiTypenames.Kill();
		aiStarts.Kill();
		cUnit.Kill();
	}
	psDest->szSuggested.Set("M");
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Why the parameter can't be named szName, NULL if it can: it must be a
// name, and one the function doesn't write already.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDETemplateParameter::GetProblem(SIDETemplateExtraction* psExtraction, const char* szName)
{
	CIDEFileEdits		cFiles;
	SIDESignatureSite*	psSite;
	CChars				szSite;
	const char*			szFound;
	const char*			szProblem;
	char*				szText;
	size				uiLength;
	size				i;

	if (!IsIDEIdentifier(szName))
	{
		return "The parameter's name isn't a name.";
	}
	uiLength = strlen(szName);
	szProblem = NULL;
	cFiles.Init(mpcContent);
	for (i = 0; (i < psExtraction->sSignature.apsSites.NumElements()) && !szProblem; i++)
	{
		psSite = psExtraction->sSignature.apsSites.GetPtr(i);
		szText = cFiles.GetText(psSite->szFileName.Text());
		if (!szText)
		{
			continue;
		}
		szSite.Init();
		AppendIDERangeText(szText, &psSite->sAll, &szSite);
		for (szFound = strstr(szSite.Text(), szName); szFound && !szProblem; szFound = strstr(szFound + 1, szName))
		{
			if (((szFound == szSite.Text()) || !IsIDENameChar(szFound[-1])) && !IsIDENameChar(szFound[uiLength]))
			{
				szProblem = "The function has something of the parameter's name already.";
			}
		}
		szSite.Kill();
	}
	cFiles.Kill();
	return szProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// The function made a template with the type its parameter szName, as
// one undo.  A definition in a source file goes to the header that
// declares the function.  The files changed are checked as they would be
// first.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::Extract(SIDETemplateExtraction* psExtraction, const char* szName, CChars* pszStatus)
{
	CIDECodeModel*		pcModel;
	CIDEFileEdits		cFiles;
	CIDERefactorUnit	cUnit;
	CIDERefactorUnit	cClassUnit;
	CArrayIDELocations	asLocations;
	CArrayInt			aiStarts;
	CArrayInt			aiTypenames;
	SIDESignatureSite*	psSite;
	SIDESignatureSite*	psDefinition;
	SIDESignatureSite*	psHeader;
	SIDESignatureCall*	psCall;
	SIDELocation*		psLocation;
	CCPPSyntax*			pcNode;
	SCPPSyntaxRange		sClass;
	CChars				szTemplate;
	CChars				szMoved;
	CChars				szBanner;
	CChars				szClassKey;
	CChars				szMessage;
	CChars				szArgument;
	char*				szText;
	size				uiStart;
	size				uiEnd;
	size				uiAllStart;
	size				uiAllEnd;
	size				uiLine;
	size				uiCalls;
	size				uiBefore;
	size				i;
	size				j;
	int32				iToken;
	uint8				uiKind;
	bool				bResult;
	bool				bMove;
	bool				bPlaced;

	if (!psExtraction->bPossible || GetProblem(psExtraction, szName))
	{
		return false;
	}
	pcModel = mpcContent->GetCodeModel();
	szTemplate.Init("template<class ");
	szTemplate.Append(szName)->Append(">\n");
	szClassKey.Init();
	GetIDESymbolScope(psExtraction->sSignature.szKey.Text(), &szClassKey);

	//Its definition goes to the header that declares it, if it is in a source file.
	psDefinition = NULL;
	psHeader = NULL;
	for (i = 0; i < psExtraction->sSignature.apsSites.NumElements(); i++)
	{
		psSite = psExtraction->sSignature.apsSites.GetPtr(i);
		if (psSite->bBody)
		{
			psDefinition = psSite;
		}
		else if (!psHeader && (CIDEIcons::GetFileType(psSite->szFileName.Text()) == IFT_Header))
		{
			psHeader = psSite;
		}
	}
	bMove = psDefinition && psHeader && (CIDEIcons::GetFileType(psDefinition->szFileName.Text()) != IFT_Header);

	cFiles.Init(mpcContent);
	bResult = true;
	szMoved.Init();
	szBanner.Init();
	if (bMove)
	{
		szText = cFiles.GetText(psDefinition->szFileName.Text());
		cUnit.Init();
		aiStarts.Init();
		aiTypenames.Init();
		bResult = szText && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psDefinition->szFileName.Text(), false);
		if (bResult)
		{
			FindTypePlaces(psExtraction, &cUnit, psDefinition->szFileName.Text(), szText, &psDefinition->sAll, &aiStarts, &aiTypenames);
			uiAllStart = FindIDETextOffset(szText, psDefinition->sAll.iLine, psDefinition->sAll.iColumn);
			uiAllEnd = FindIDETextOffset(szText, psDefinition->sAll.iEndLine, psDefinition->sAll.iEndColumn);
			AppendReplaced(psExtraction, szName, szText, uiAllStart, uiAllEnd, &aiStarts, &aiTypenames, &szMoved);
			uiStart = uiAllStart;
			uiEnd = uiAllEnd;
			ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
			if ((uiStart < uiAllStart) && (szText[uiStart] == '/'))
			{
				//The banner over it goes with it.
				szBanner.AppendSubString(szText, uiStart, uiAllStart);
			}
			cFiles.GetEdits(psDefinition->szFileName.Text())->Replace(uiStart, uiEnd, "");
		}
		aiTypenames.Kill();
		aiStarts.Kill();
		cUnit.Kill();
	}

	for (i = 0; bResult && (i < psExtraction->sSignature.apsSites.NumElements()); i++)
	{
		psSite = psExtraction->sSignature.apsSites.GetPtr(i);
		if (bMove && (psSite == psDefinition))
		{
			continue;
		}
		szText = cFiles.GetText(psSite->szFileName.Text());
		cUnit.Init();
		aiStarts.Init();
		aiTypenames.Init();
		bResult = szText && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psSite->szFileName.Text(), false);
		if (bResult)
		{
			FindTypePlaces(psExtraction, &cUnit, psSite->szFileName.Text(), szText, &psSite->sAll, &aiStarts, &aiTypenames);
			uiAllStart = FindIDETextOffset(szText, psSite->sAll.iLine, psSite->sAll.iColumn);
			uiAllEnd = FindIDETextOffset(szText, psSite->sAll.iEndLine, psSite->sAll.iEndColumn);
			if (bMove && (psSite == psHeader) && !psSite->bInClass)
			{
				//The definition, in the declaration's place, two lines from what is above and below it.
				for (uiLine = uiAllStart; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
				{
				}
				szArgument.Init();
				if ((uiLine >= 1) && !((uiLine >= 2) && (szText[uiLine - 2] == '\n')))
				{
					szArgument.Append("\n\n");
				}
				else if ((uiLine >= 3) && (szText[uiLine - 3] != '\n'))
				{
					szArgument.Append('\n');
				}
				szArgument.Append(szTemplate)->Append(szMoved);
				for (j = uiAllEnd; (szText[j] == ' ') || (szText[j] == '\t') || (szText[j] == '\r'); j++)
				{
				}
				if ((szText[j] == '\n') && (szText[j + 1] != '\n') && (szText[j + 1] != '\0'))
				{
					szArgument.Append("\n\n");
				}
				else if ((szText[j] == '\n') && (szText[j + 1] == '\n') && (szText[j + 2] != '\n') && (szText[j + 2] != '\0'))
				{
					szArgument.Append('\n');
				}
				cFiles.GetEdits(psSite->szFileName.Text())->Replace(uiAllStart, uiAllEnd, szArgument.Text());
				szArgument.Kill();
			}
			else
			{
				//template<class M> over it, at its own indent, and the type made M.
				for (uiLine = uiAllStart; (uiLine > 0) && (szText[uiLine - 1] != '\n'); uiLine--)
				{
				}
				szArgument.Init(szTemplate);
				for (j = uiLine; (j < uiAllStart) && ((szText[j] == '\t') || (szText[j] == ' ')); j++)
				{
					szArgument.Append(szText[j]);
				}
				cFiles.GetEdits(psSite->szFileName.Text())->Insert(uiAllStart, szArgument.Text());
				szArgument.Kill();
				for (j = 0; j < aiStarts.NumElements(); j++)
				{
					szArgument.Init(aiTypenames.GetValue(j) ? "typename " : "");
					szArgument.Append(szName);
					cFiles.GetEdits(psSite->szFileName.Text())->Replace((size)aiStarts.GetValue(j), (size)aiStarts.GetValue(j) + psExtraction->szType.Length(), szArgument.Text());
					szArgument.Kill();
				}
			}
		}
		aiTypenames.Kill();
		aiStarts.Kill();
		cUnit.Kill();
	}

	//A member function's definition: after its class, in the class's header.
	if (bResult && bMove && psHeader->bInClass)
	{
		szText = cFiles.GetText(psHeader->szFileName.Text());
		asLocations.Init();
		pcModel->GetSymbolLocations(szClassKey.Text(), &asLocations, &uiKind);
		bPlaced = false;
		cClassUnit.Init();
		for (i = 0; (i < asLocations.NumElements()) && !bPlaced; i++)
		{
			psLocation = asLocations.Get(i);
			if ((psLocation->uiRole != CPPIR_Definition) || (StringInsensitiveCompare(psLocation->szFileName.Text(), psHeader->szFileName.Text()) != 0) ||
				!cClassUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psLocation->szFileName.Text(), false))
			{
				continue;
			}
			iToken = cClassUnit.FindTokenAt(psLocation->iLine, psLocation->iColumn);
			pcNode = (iToken != -1) ? cClassUnit.FindAncestor(cClassUnit.FindNodeOfTokens(iToken, iToken), CPPS_SimpleDeclaration) : NULL;
			if (pcNode && cClassUnit.GetRange(pcNode, &sClass))
			{
				uiEnd = FindIDETextOffset(szText, sClass.iEndLine, sClass.iEndColumn);
				for (; (szText[uiEnd] != '\0') && (szText[uiEnd] != '\n'); uiEnd++)
				{
				}
				szArgument.Init("\n\n");
				szArgument.Append(szBanner)->Append(szTemplate)->Append(szMoved)->Append('\n');
				cFiles.GetEdits(psHeader->szFileName.Text())->Insert((szText[uiEnd] == '\n') ? uiEnd + 1 : uiEnd, szArgument.Text());
				szArgument.Kill();
				bPlaced = true;
			}
		}
		cClassUnit.Kill();
		KillIDELocations(&asLocations);
		bResult = bPlaced;
	}

	//The calls name the type where their arguments don't say it.
	uiCalls = 0;
	for (i = 0; bResult && !psExtraction->bDeduced && (i < psExtraction->sSignature.apsCalls.NumElements()); i++)
	{
		psCall = psExtraction->sSignature.apsCalls.GetPtr(i);
		szText = cFiles.GetText(psCall->szFileName.Text());
		if (szText)
		{
			szArgument.Init("<");
			szArgument.Append(psExtraction->szType)->Append('>');
			cFiles.GetEdits(psCall->szFileName.Text())->Insert(FindIDETextOffset(szText, psCall->iLine, psCall->iColumn) + psExtraction->sSignature.szOldName.Length(), szArgument.Text());
			szArgument.Kill();
			uiCalls++;
		}
	}

	szMessage.Init();
	if (bResult)
	{
		uiBefore = cFiles.CountErrors(false);
		bResult = cFiles.CountErrors(true) <= uiBefore;
	}
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Extracted the template parameter ")->Append(szName)->Append(" from ")->Append(psExtraction->szType)->Append(" in ")->Append(psExtraction->sSignature.szName);
		if (bMove)
		{
			szMessage.Append(": its definition is in ")->Append(GetIDEFileNamePart(psHeader->szFileName.Text()))->Append(" now");
		}
		if (uiCalls > 0)
		{
			szMessage.Append(bMove ? ", and " : ": ")->Append((uint32)uiCalls)->Append((uiCalls == 1) ? " call names the type" : " calls name the type");
		}
		szMessage.Append('.');
	}
	else
	{
		szMessage.Append("Can't extract a template parameter from ")->Append(psExtraction->szType)->Append(" in ")->Append(psExtraction->sSignature.szName)->Append(": the code doesn't compile with it a template.");
	}
	if (pszStatus)
	{
		pszStatus->Set(szMessage);
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	szBanner.Kill();
	szMoved.Kill();
	cFiles.Kill();
	szClassKey.Kill();
	szTemplate.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Extract Template Parameter's dialog, for the type at the caret: the
// parameter's name.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::ShowExtract(void)
{
	SIDETemplateExtraction	sExtraction;
	Ptr<CTextField>			pName;
	Ptr<CTextField>			pTemplateName;
	CChars					szMessage;
	CChars					szName;
	CChars					szTemplateName;
	bool					bResult;

	sExtraction.Init();
	if (!FindExtraction(&sExtraction))
	{
		mpcContent->ShowStatus("The caret isn't on a type in a function or a class.");
		sExtraction.Kill();
		return false;
	}
	szMessage.Init();
	if (!sExtraction.bPossible)
	{
		szMessage.Append("Can't extract a template parameter from ")->Append(sExtraction.szType)->Append(": ")->Append(sExtraction.szWhyNot)->Append('.');
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sExtraction.Kill();
		return false;
	}
	szMessage.Append("Extract Template Parameter from ")->Append(sExtraction.szType);
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	pName = mpDialog->AddTextField("The parameter's name:", sExtraction.szSuggested.Text());
	if (sExtraction.bClass)
	{
		//Its class becomes the template, and the class's name a typedef of it.
		pTemplateName = mpDialog->AddTextField("The class template's name:", sExtraction.szSuggestedClass.Text());
	}
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(480);
	szName.Init(pName->GetText());
	szTemplateName.Init(pTemplateName.IsNotNull() ? pTemplateName->GetText() : "");
	pName = NULL;
	pTemplateName = NULL;
	mpDialog = NULL;
	if (bResult && sExtraction.bClass)
	{
		if (GetClassProblem(&sExtraction, szName.Text(), szTemplateName.Text()))
		{
			mpcContent->ShowStatus(GetClassProblem(&sExtraction, szName.Text(), szTemplateName.Text()));
			bResult = false;
		}
		bResult = bResult && ExtractClass(&sExtraction, szName.Text(), szTemplateName.Text());
	}
	else if (bResult)
	{
		if (GetProblem(&sExtraction, szName.Text()))
		{
			mpcContent->ShowStatus(GetProblem(&sExtraction, szName.Text()));
			bResult = false;
		}
		bResult = bResult && Extract(&sExtraction, szName.Text());
	}
	szTemplateName.Kill();
	szName.Kill();
	sExtraction.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The class a node is in, for a type named in it outside its functions'
// declarations: psDest is a class's extraction (bClass), with why the
// class can't become a template if it can't.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::FindClass(SIDETemplateExtraction* psDest, CIDERefactorUnit* pcUnit, CCPPSyntax* pcNode, const char* szFileName)
{
	CIDECodeModel*				pcModel;
	CCPPSyntaxClassSpecifier*	pcClass;
	CCPPSyntax*					pcDeclaration;
	SCPPSyntaxRange				sName;
	uint8						uiKind;

	pcModel = mpcContent->GetCodeModel();
	pcClass = (CCPPSyntaxClassSpecifier*)pcUnit->FindAncestor(pcNode, CPPS_ClassSpecifier);
	pcDeclaration = pcClass ? pcUnit->FindAncestor(pcClass, CPPS_SimpleDeclaration) : NULL;
	if (!pcDeclaration || !pcClass->mpcName || !pcUnit->GetRange(pcDeclaration, &psDest->sClass) || !pcUnit->GetTokensRange(pcClass->mpcName->miLastToken, pcClass->mpcName->miLastToken, &sName) ||
		!pcModel->GetSymbolAt(szFileName, sName.iLine, sName.iColumn, &psDest->szClassKey, &uiKind))
	{
		return;
	}
	psDest->bClass = true;
	psDest->bPossible = true;
	psDest->szClassFile.Set(szFileName);
	psDest->szSuggestedClass.Set(psDest->szClassKey);
	psDest->szSuggestedClass.Append("Template");
	if (uiKind != CPPSK_Class)
	{
		Refuse(psDest, "its class is a template already");
	}
	else if (strchr(psDest->szClassKey.Text(), ':'))
	{
		Refuse(psDest, "its class is in a class or a namespace");
	}
	else if (psDest->szClassKey.Equals(psDest->szTypeKey.Text()))
	{
		Refuse(psDest, "it is its class itself");
	}
	else if (pcUnit->FindAncestor(pcDeclaration, CPPS_TemplateDeclaration))
	{
		Refuse(psDest, "its class is a template already");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Where a range of a file writes the name szName as a token, in the
// file's text and in order: a class's own name, in itself and its
// members.
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::FindNamePlaces(CIDERefactorUnit* pcUnit, const char* szText, SCPPSyntaxRange* psRange, const char* szName, CArrayInt* paiStarts)
{
	SCPPSyntaxRange	sToken;
	int32			iToken;
	int32			iFirst;
	int32			iLast;

	if (!pcUnit->FindTokensIn(psRange->iLine, psRange->iColumn, psRange->iEndLine, psRange->iEndColumn, &iFirst, &iLast))
	{
		return;
	}
	for (iToken = iFirst; iToken <= iLast; iToken++)
	{
		if (pcUnit->IsToken(iToken, szName) && pcUnit->GetTokensRange(iToken, iToken, &sToken))
		{
			paiStarts->Add((int)FindIDETextOffset(szText, sToken.iLine, sToken.iColumn));
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A piece of the class (all of it, or one of its members' definitions) as
// the template has it, added to pszDest: the type made the parameter
// szName, the class's own name made szTemplateName, and, for a member
// defined outside the class, the name it is defined by (psQualified,
// "CTank::Fill") made "CTankTemplate<M>::Fill".
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::AppendClassText(SIDETemplateExtraction* psExtraction, CIDERefactorUnit* pcUnit, const char* szFileName, const char* szText, SCPPSyntaxRange* psRange, SCPPSyntaxRange* psQualified,
											 const char* szOwnName, const char* szName, const char* szTemplateName, CChars* pszDest)
{
	CArrayInt	aiTypes;
	CArrayInt	aiTypenames;
	CArrayInt	aiNames;
	CChars		szClassName;
	CChars		szQualified;
	size		uiStart;
	size		uiEnd;
	size		uiQualified;
	size		uiQualifiedEnd;
	size		uiAt;
	size		uiType;
	size		uiNamePlace;
	size		uiNext;
	size		i;
	size		j;

	szClassName.Init();
	GetIDESymbolName(psExtraction->szClassKey.Text(), &szClassName);
	uiStart = FindIDETextOffset(szText, psRange->iLine, psRange->iColumn);
	uiEnd = FindIDETextOffset(szText, psRange->iEndLine, psRange->iEndColumn);
	uiQualified = psQualified ? FindIDETextOffset(szText, psQualified->iLine, psQualified->iColumn) : MAX_SIZE;
	uiQualifiedEnd = psQualified ? FindIDETextOffset(szText, psQualified->iEndLine, psQualified->iEndColumn) : MAX_SIZE;
	aiTypes.Init();
	aiTypenames.Init();
	aiNames.Init();
	FindTypePlaces(psExtraction, pcUnit, szFileName, szText, psRange, &aiTypes, &aiTypenames);
	FindNamePlaces(pcUnit, szText, psRange, szClassName.Text(), &aiNames);

	//Whichever comes next: the type, the class's name, or the name the member is defined by.
	uiAt = uiStart;
	i = 0;
	j = 0;
	while (uiAt < uiEnd)
	{
		for (; (i < aiTypes.NumElements()) && ((size)aiTypes.GetValue(i) < uiAt); i++)
		{
		}
		for (; (j < aiNames.NumElements()) && ((size)aiNames.GetValue(j) < uiAt); j++)
		{
		}
		uiType = (i < aiTypes.NumElements()) ? (size)aiTypes.GetValue(i) : MAX_SIZE;
		uiNamePlace = (j < aiNames.NumElements()) ? (size)aiNames.GetValue(j) : MAX_SIZE;
		uiNext = (uiType < uiNamePlace) ? uiType : uiNamePlace;
		if ((uiQualified != MAX_SIZE) && (uiQualified >= uiAt) && (uiQualified <= uiNext))
		{
			pszDest->AppendSubString(szText, uiAt, uiQualified);
			szQualified.Init(szOwnName);
			if (szQualified.Equals(szClassName.Text()))
			{
				szQualified.Set(szTemplateName);
			}
			else if (szQualified.StartsWith("~") && (strcmp(&szQualified.Text()[1], szClassName.Text()) == 0))
			{
				szQualified.Set("~");
				szQualified.Append(szTemplateName);
			}
			pszDest->Append(szTemplateName)->Append('<')->Append(szName)->Append(">::")->Append(szQualified);
			szQualified.Kill();
			uiAt = uiQualifiedEnd;
			uiQualified = MAX_SIZE;
		}
		else if ((uiNext == MAX_SIZE) || (uiNext >= uiEnd))
		{
			break;
		}
		else if (uiNext == uiType)
		{
			pszDest->AppendSubString(szText, uiAt, uiType);
			pszDest->Append(aiTypenames.GetValue(i) ? "typename " : "")->Append(szName);
			uiAt = uiType + psExtraction->szType.Length();
		}
		else
		{
			pszDest->AppendSubString(szText, uiAt, uiNamePlace);
			pszDest->Append(szTemplateName);
			uiAt = uiNamePlace + szClassName.Length();
		}
	}
	pszDest->AppendSubString(szText, uiAt, uiEnd);
	aiNames.Kill();
	aiTypenames.Kill();
	aiTypes.Kill();
	szClassName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Why the class can't become the template szTemplateName with the
// parameter szName, NULL if it can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDETemplateParameter::GetClassProblem(SIDETemplateExtraction* psExtraction, const char* szName, const char* szTemplateName)
{
	CTextFile	cFile;
	CChars		szClass;
	const char*	szFound;
	const char*	szProblem;
	char*		szText;
	size		uiLength;
	size		uiName;

	if (!IsIDEIdentifier(szName))
	{
		return "The parameter's name isn't a name.";
	}
	if (!IsIDEIdentifier(szTemplateName))
	{
		return "The template's name isn't a name.";
	}
	if (mpcContent->GetCodeModel()->HasSymbol(szTemplateName))
	{
		return "There is something of the template's name already.";
	}
	szProblem = NULL;
	uiName = strlen(szName);
	cFile.Init();
	if (mpcContent->GetFindInFiles()->GetFileText(psExtraction->szClassFile.Text(), &cFile, &szText, &uiLength))
	{
		szClass.Init();
		AppendIDERangeText(szText, &psExtraction->sClass, &szClass);
		for (szFound = strstr(szClass.Text(), szName); szFound && !szProblem; szFound = strstr(szFound + 1, szName))
		{
			if (((szFound == szClass.Text()) || !IsIDENameChar(szFound[-1])) && !IsIDENameChar(szFound[uiName]))
			{
				szProblem = "The class has something of the parameter's name already.";
			}
		}
		szClass.Kill();
	}
	cFile.Kill();
	return szProblem;
}


//////////////////////////////////////////////////////////////////////////
//
// The class made a class template named szTemplateName with the type its
// parameter szName, as one undo: the class, then its members' definitions
// (those outside it, brought to its header from wherever they are, in
// their order), then the class that is its old name: one that derives
// from the template with the type it had and has nothing of its own but
// the class's constructors, each handing what it is given to the
// template's ("class CTank : public CTankTemplate<CGauge>").  Unlike a
// typedef it is a type of its own, so the class can stay declared ahead
// of its definition wherever it is; and the compiler warns where the
// template's instance is written out under it.  The files changed are
// checked as they would be first.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::ExtractClass(SIDETemplateExtraction* psExtraction, const char* szName, const char* szTemplateName, CChars* pszStatus)
{
	CIDECodeModel*		pcModel;
	CIDEFileEdits		cFiles;
	CIDERefactorUnit	cUnit;
	CIDERefactorUnit	cOther;
	CArrayIDEFileRanges	asRanges;
	CArrayIDELocations	asLocations;
	CArrayIDELocations	asDefinitions;
	CArrayChars			aszKeys;
	CArrayChars			aszDefinitionKeys;
	SIDELocation*		psLocation;
	SIDELocation*		psDefinition;
	SIDESignatureSite	sSite;
	CChars				szScope;
	CChars				szTemplate;
	CChars				szClass;
	CChars				szAfter;
	CChars				szPiece;
	CChars				szMessage;
	CChars				szClassName;
	CChars				szNameClass;
	const char*			szWhyNot;
	const char*			szProblem;
	char*				szText;
	char*				szOther;
	size				uiStart;
	size				uiEnd;
	size				uiAllStart;
	size				uiMoved;
	size				uiBefore;
	size				i;
	size				j;
	size				k;
	int32				iAccess;
	int32				iLastAccess;
	uint8				uiKind;
	bool				bResult;
	bool				bStruct;

	if (!psExtraction->bPossible || !psExtraction->bClass || GetClassProblem(psExtraction, szName, szTemplateName))
	{
		return false;
	}
	uiBefore = 0;
	pcModel = mpcContent->GetCodeModel();
	szTemplate.Init("template<class ");
	szTemplate.Append(szName)->Append(">\n");
	szClassName.Init();
	GetIDESymbolName(psExtraction->szClassKey.Text(), &szClassName);
	szProblem = NULL;
	cFiles.Init(mpcContent);
	szText = cFiles.GetText(psExtraction->szClassFile.Text());
	cUnit.Init();
	bResult = szText && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), psExtraction->szClassFile.Text(), false);

	//Its members' definitions outside it, in their files' order.
	asRanges.Init();
	asRanges.Add()->Init(psExtraction->szClassFile.Text(), psExtraction->sClass.iLine, psExtraction->sClass.iColumn, psExtraction->sClass.iEndLine, psExtraction->sClass.iEndColumn);
	aszKeys.Init();
	pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
	asDefinitions.Init();
	aszDefinitionKeys.Init();
	for (i = 0; bResult && (i < aszKeys.NumElements()); i++)
	{
		szScope.Init();
		GetIDESymbolScope(aszKeys.GetText(i), &szScope);
		asLocations.Init();
		if (szScope.Equals(psExtraction->szClassKey.Text()) && pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind))
		{
			for (j = 0; j < asLocations.NumElements(); j++)
			{
				psLocation = asLocations.Get(j);
				if ((psLocation->uiRole != CPPIR_Definition) || asRanges.Get(0)->Has(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn))
				{
					continue;
				}
				if ((uiKind != CPPSK_Function) && (uiKind != CPPSK_FunctionTemplate))
				{
					szProblem = "one of its fields is static and defined outside it";
					bResult = false;
					break;
				}
				psDefinition = asDefinitions.Add();
				memcpy(psDefinition, psLocation, sizeof(SIDELocation));
				psDefinition->szFileName.Init(psLocation->szFileName);
				aszDefinitionKeys.Add(aszKeys.GetText(i));
			}
		}
		KillIDELocations(&asLocations);
		szScope.Kill();
	}

	//Its destructor's definition: the index has the destructor's name as its class's, so it is found by how it is
	//written, "CTank::~CTank".
	asLocations.Init();
	pcModel->GetSymbolLocations(psExtraction->szClassKey.Text(), &asLocations, &uiKind);
	for (j = 0; bResult && (j < asLocations.NumElements()); j++)
	{
		psLocation = asLocations.Get(j);
		szOther = asRanges.Get(0)->Has(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn) ? NULL : cFiles.GetText(psLocation->szFileName.Text());
		uiStart = szOther ? FindIDETextOffset(szOther, psLocation->iLine, psLocation->iColumn) : 0;
		if ((uiStart >= 3) && (szOther[uiStart - 1] == '~') && (szOther[uiStart - 2] == ':') && (szOther[uiStart - 3] == ':'))
		{
			psDefinition = asDefinitions.Add();
			memcpy(psDefinition, psLocation, sizeof(SIDELocation));
			psDefinition->szFileName.Init(psLocation->szFileName);
			psDefinition->uiRole = CPPIR_Definition;
			psDefinition->iLength = (int32)szClassName.Length();
			aszDefinitionKeys.Add(psExtraction->szClassKey.Text());
		}
	}
	KillIDELocations(&asLocations);

	//The class as the template has it.
	szClass.Init(szTemplate);
	szAfter.Init();
	uiMoved = 0;
	if (bResult)
	{
		AppendClassText(psExtraction, &cUnit, psExtraction->szClassFile.Text(), szText, &psExtraction->sClass, NULL, "", szName, szTemplateName, &szClass);
	}
	for (i = 0; bResult && (i < asDefinitions.NumElements()); i++)
	{
		//The first, by file and line, that isn't done.
		psDefinition = NULL;
		for (j = 0; j < asDefinitions.NumElements(); j++)
		{
			psLocation = asDefinitions.Get(j);
			if ((psLocation->iLength >= 0) && (!psDefinition || (StringInsensitiveCompare(psLocation->szFileName.Text(), psDefinition->szFileName.Text()) < 0) ||
				((StringInsensitiveCompare(psLocation->szFileName.Text(), psDefinition->szFileName.Text()) == 0) && (psLocation->iLine < psDefinition->iLine))))
			{
				psDefinition = psLocation;
				uiBefore = j;
			}
		}
		psDefinition->iLength = -1;
		szOther = cFiles.GetText(psDefinition->szFileName.Text());
		cOther.Init();
		sSite.Init();
		szWhyNot = NULL;
		if (!szOther || !cOther.Parse(pcModel, &mpcContent->GetFindInFiles(), psDefinition->szFileName.Text(), false) ||
			!mpcContent->GetChangeSignature()->ReadSite(&cOther, psDefinition, aszDefinitionKeys.GetText(uiBefore), &sSite, &szWhyNot) || !sSite.bBody)
		{
			szProblem = "one of its members' definitions can't be read (a macro makes it)";
			bResult = false;
		}
		else if (!sSite.szOwnTemplate.Empty())
		{
			szProblem = "one of its members is a template";
			bResult = false;
		}
		else
		{
			uiAllStart = FindIDETextOffset(szOther, sSite.sAll.iLine, sSite.sAll.iColumn);
			uiStart = uiAllStart;
			uiEnd = FindIDETextOffset(szOther, sSite.sAll.iEndLine, sSite.sAll.iEndColumn);
			ExtendIDEDeletion(szOther, &uiStart, &uiEnd, true);
			szPiece.Init();
			if ((uiStart < uiAllStart) && (szOther[uiStart] == '/'))
			{
				szPiece.AppendSubString(szOther, uiStart, uiAllStart);
			}
			szPiece.Append(szTemplate);
			AppendClassText(psExtraction, &cOther, psDefinition->szFileName.Text(), szOther, &sSite.sAll, &sSite.sName, sSite.szOwnName.Text(), szName, szTemplateName, &szPiece);
			szAfter.Append("\n\n\n")->Append(szPiece);
			szPiece.Kill();
			cFiles.GetEdits(psDefinition->szFileName.Text())->Replace(uiStart, uiEnd, "");
			uiMoved++;
		}
		sSite.Kill();
		cOther.Kill();
	}

	//The class that is its old name: each constructor the class declares, with its access, its parameters as they
	//are written and their names given to the template's.
	szNameClass.Init();
	bStruct = false;
	iLastAccess = IDE_ACCESS_PRIVATE;
	if (bResult)
	{
		bStruct = (strncmp(&szText[FindIDETextOffset(szText, psExtraction->sClass.iLine, psExtraction->sClass.iColumn)], "struct", 6) == 0);
		szNameClass.Append(bStruct ? "struct " : "class ")->Append(szClassName)->Append(" : public ")->Append(szTemplateName)->Append('<')->Append(psExtraction->szType)->Append(">\n{\n");
		iLastAccess = bStruct ? IDE_ACCESS_PUBLIC : IDE_ACCESS_PRIVATE;
	}
	for (i = 0; bResult && (i < aszKeys.NumElements()); i++)
	{
		szScope.Init();
		szPiece.Init();
		GetIDESymbolScope(aszKeys.GetText(i), &szScope);
		GetIDESymbolName(aszKeys.GetText(i), &szPiece);
		asLocations.Init();
		if (szScope.Equals(psExtraction->szClassKey.Text()) && szPiece.Equals(szClassName.Text()) && pcModel->GetSymbolLocations(aszKeys.GetText(i), &asLocations, &uiKind) && (uiKind == CPPSK_Function))
		{
			for (j = 0; j < asLocations.NumElements(); j++)
			{
				psLocation = asLocations.Get(j);
				if ((psLocation->uiRole == CPPIR_Reference) || !asRanges.Get(0)->Has(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn))
				{
					continue;
				}
				sSite.Init();
				szWhyNot = NULL;
				if (!mpcContent->GetChangeSignature()->ReadSite(&cUnit, psLocation, aszKeys.GetText(i), &sSite, &szWhyNot))
				{
					szProblem = "one of its constructors can't be read (a macro makes it)";
					bResult = false;
				}
				else
				{
					iAccess = mpcContent->GetMoveMembers()->FindMemberAccess(aszKeys.GetText(i));
					if (iAccess != iLastAccess)
					{
						szNameClass.Append((iAccess == IDE_ACCESS_PUBLIC) ? "public:\n" : ((iAccess == IDE_ACCESS_PROTECTED) ? "protected:\n" : "private:\n"));
						iLastAccess = iAccess;
					}
					szNameClass.Append('\t')->Append(szClassName)->Append('(');
					szNameClass.AppendSubString(szText, FindIDETextOffset(szText, sSite.sParameters.iLine, sSite.sParameters.iColumn), FindIDETextOffset(szText, sSite.sParameters.iEndLine, sSite.sParameters.iEndColumn));
					szNameClass.Append(") : ")->Append(szTemplateName)->Append('<')->Append(psExtraction->szType)->Append(">(");
					for (k = 0; k < sSite.asParameters.NumElements(); k++)
					{
						if (!sSite.asParameters.Get(k)->bName)
						{
							szProblem = "one of its constructors has a parameter with no name to hand on";
							bResult = false;
							break;
						}
						szNameClass.Append((k > 0) ? ", " : "");
						szNameClass.AppendSubString(szText, FindIDETextOffset(szText, sSite.asParameters.Get(k)->sName.iLine, sSite.asParameters.Get(k)->sName.iColumn),
													FindIDETextOffset(szText, sSite.asParameters.Get(k)->sName.iEndLine, sSite.asParameters.Get(k)->sName.iEndColumn));
					}
					szNameClass.Append(") {}\n");
				}
				sSite.Kill();
				break;
			}
		}
		KillIDELocations(&asLocations);
		szPiece.Kill();
		szScope.Kill();
	}
	szNameClass.Append("};");

	szMessage.Init();
	if (bResult)
	{
		//The class, its members' definitions, and the class that is its old name.
		szClass.Append(szAfter);
		szClass.Append("\n\n\n")->Append(szNameClass);
		cFiles.GetEdits(psExtraction->szClassFile.Text())->Replace(FindIDETextOffset(szText, psExtraction->sClass.iLine, psExtraction->sClass.iColumn),
																 FindIDETextOffset(szText, psExtraction->sClass.iEndLine, psExtraction->sClass.iEndColumn), szClass.Text());
		uiBefore = cFiles.CountErrors(false);
		if (cFiles.CountErrors(true) > uiBefore)
		{
			szProblem = "the code doesn't compile with it a template";
			bResult = false;
		}
	}
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Append("Extracted the template parameter ")->Append(szName)->Append(" from ")->Append(psExtraction->szType)->Append(" in ")->Append(szClassName)->Append(": ")->Append(szClassName);
		szMessage.Append(" is a class that derives from ")->Append(szTemplateName)->Append('<')->Append(psExtraction->szType)->Append('>');
		if (uiMoved > 0)
		{
			szMessage.Append(", and ")->Append((uint32)uiMoved)->Append((uiMoved == 1) ? " definition is in " : " definitions are in ")->Append(GetIDEFileNamePart(psExtraction->szClassFile.Text()))->Append(" now");
		}
		szMessage.Append('.');
	}
	else
	{
		szMessage.Append("Can't extract a template parameter from ")->Append(psExtraction->szType)->Append(" in ")->Append(szClassName)->Append(": ")->Append(szProblem ? szProblem : "its class can't be read")->Append('.');
	}
	if (pszStatus)
	{
		pszStatus->Set(szMessage);
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	szNameClass.Kill();
	szAfter.Kill();
	szClass.Kill();
	aszDefinitionKeys.Kill();
	KillIDELocations(&asDefinitions);
	aszKeys.Kill();
	KillIDEFileRanges(&asRanges);
	cUnit.Kill();
	cFiles.Kill();
	szClassName.Kill();
	szTemplate.Kill();
	return bResult;
}


//A declaration of a class template or of one of its members outside it, for Remove Template Parameter.
struct SIDEOwnTemplate
{
	CChars	szFileName;
	size	uiStart;			//Its template<...>,
	size	uiHeaderEnd;		//what it declares,
	size	uiEnd;				//and its end.
	size	uiParameterStart;	//The parameter that goes, with the comma it is written with if it isn't the only one.
	size	uiParameterEnd;
	CChars	szParameter;		//Its name here.
	bool	bMember;			//A member's definition, not the class.
};


typedef CArrayTemplate<SIDEOwnTemplate>	CArrayIDEOwnTemplates;


//////////////////////////////////////////////////////////////////////////
//
// The template arguments written after a name that ends at uiAt in a
// text: where each starts and ends, and where the > that closes them is.
// False if no < follows the name.
//
//////////////////////////////////////////////////////////////////////////
static bool FindIDETemplateArguments(const char* szText, size uiAt, CArrayInt* paiStarts, CArrayInt* paiEnds, size* puiOpen, size* puiClose)
{
	size	uiStart;
	int		iDepth;
	int		iBrackets;

	for (; (szText[uiAt] == ' ') || (szText[uiAt] == '\t'); uiAt++)
	{
	}
	if (szText[uiAt] != '<')
	{
		return false;
	}
	*puiOpen = uiAt;
	iDepth = 1;
	iBrackets = 0;
	uiStart = uiAt + 1;
	for (uiAt++; (szText[uiAt] != '\0') && (iDepth > 0); uiAt++)
	{
		if (szText[uiAt] == '(')
		{
			iBrackets++;
		}
		else if (szText[uiAt] == ')')
		{
			iBrackets--;
		}
		else if ((szText[uiAt] == '<') && (iBrackets == 0))
		{
			iDepth++;
		}
		else if ((szText[uiAt] == '>') && (iBrackets == 0))
		{
			iDepth--;
		}
		else if ((szText[uiAt] == ';') || (szText[uiAt] == '{'))
		{
			//Not arguments: a comparison.
			return false;
		}
		if ((iDepth == 0) || ((szText[uiAt] == ',') && (iDepth == 1) && (iBrackets == 0)))
		{
			paiStarts->Add((int)uiStart);
			paiEnds->Add((int)uiAt);
			uiStart = uiAt + 1;
		}
	}
	*puiClose = uiAt - 1;
	return iDepth == 0;
}


//////////////////////////////////////////////////////////////////////////
//
// A class that was a template until its last parameter went may have a
// class that was its name ("class CTank : public CTankTemplate", what
// Extract Template Parameter left where the class was): one class
// derives from it, from it alone, and has nothing of its own but
// constructors written in it.  Then the class takes the name again: the
// name's class goes, and the class is called by the name wherever it is
// named, its constructors and destructor too.  As edits of their own,
// checked as they would be: false, with nothing changed, if there is no
// such class or the code wouldn't compile so.  *pszName is the name.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::FoldNameClass(const char* szClassKey, CChars* pszName)
{
	CIDECodeModel*				pcModel;
	CIDEFileEdits				cFiles;
	CIDERefactorUnit			cUnit;
	CArrayChars					aszDerived;
	CArrayIDELocations			asLocations;
	SIDERenameTarget			sTarget;
	SIDELocation*				psLocation;
	CCPPSyntaxClassSpecifier*	pcClass;
	CCPPSyntaxDeclarator*		pcDeclarator;
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcDeclaration;
	CCPPSyntax*					pcMember;
	SCPPSyntaxRange				sRange;
	CChars						szFileName;
	CChars						szName;
	CChars						szOldName;
	char*						szText;
	size						uiStart;
	size						uiEnd;
	size						uiAt;
	size						uiBefore;
	size						i;
	int32						iToken;
	int32						iLine;
	int32						iColumn;
	uint8						uiKind;
	bool						bResult;

	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	aszDerived.Init();
	pcModel->FindClassDerived(szClassKey, false, &aszDerived);
	szFileName.Init();
	szName.Init();
	szOldName.Init();
	GetIDESymbolName(szClassKey, &szOldName);
	iLine = 0;
	iColumn = 0;
	bResult = false;
	if (aszDerived.NumElements() == 1)
	{
		GetIDESymbolName(aszDerived.GetText(0), &szName);
		asLocations.Init();
		if (pcModel->GetSymbolLocations(aszDerived.GetText(0), &asLocations, &uiKind) && (uiKind == CPPSK_Class))
		{
			for (i = 0; !bResult && (i < asLocations.NumElements()); i++)
			{
				if (asLocations.Get(i)->uiRole == CPPIR_Definition)
				{
					szFileName.Set(asLocations.Get(i)->szFileName);
					iLine = asLocations.Get(i)->iLine;
					iColumn = asLocations.Get(i)->iColumn;
					bResult = true;
				}
			}
		}
		KillIDELocations(&asLocations);
	}
	aszDerived.Kill();

	//The name's class: one base, and constructors written in it.
	memset(&sRange, 0, sizeof(SCPPSyntaxRange));
	cUnit.Init();
	pcClass = NULL;
	if (bResult && cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), szFileName.Text(), false))
	{
		iToken = cUnit.FindTokenAt(iLine, iColumn);
		pcNode = (iToken != -1) ? cUnit.FindNodeOfTokens(iToken, iToken) : NULL;
		pcClass = pcNode ? (CCPPSyntaxClassSpecifier*)cUnit.FindAncestor(pcNode, CPPS_ClassSpecifier) : NULL;
	}
	pcDeclaration = pcClass ? cUnit.FindAncestor(pcClass, CPPS_SimpleDeclaration) : NULL;
	bResult = bResult && pcClass && pcDeclaration && (pcClass->macpBases.NumElements() == 1) && cUnit.GetRange(pcDeclaration, &sRange);
	for (i = 0; bResult && (i < pcClass->macpMembers.NumElements()); i++)
	{
		pcMember = pcClass->macpMembers.GetPtr(i);
		if (pcMember->GetKind() == CPPS_AccessSpecifier)
		{
			continue;
		}
		pcDeclarator = (pcMember->GetKind() == CPPS_FunctionDefinition) ? ((CCPPSyntaxFunctionDefinition*)pcMember)->mpcDeclarator : NULL;
		bResult = pcDeclarator && pcDeclarator->mpcName && (pcDeclarator->mpcName->miFirstToken == pcDeclarator->mpcName->miLastToken) && cUnit.IsToken(pcDeclarator->mpcName->miLastToken, szName.Text());
	}
	cUnit.Kill();

	//Every place the class is named, for the name it takes.
	sTarget.Init();
	if (bResult)
	{
		bResult = false;
		asLocations.Init();
		pcModel->GetSymbolLocations(szClassKey, &asLocations, &uiKind);
		for (i = 0; !bResult && (i < asLocations.NumElements()); i++)
		{
			psLocation = asLocations.Get(i);
			bResult = (psLocation->uiRole == CPPIR_Definition) && pcModel->FindRenameTarget(psLocation->szFileName.Text(), psLocation->iLine, psLocation->iColumn, &sTarget) &&
					  (sTarget.asLocations.NumElements() > 0);
		}
		KillIDELocations(&asLocations);
	}
	cFiles.Init(mpcContent);
	szText = bResult ? cFiles.GetText(szFileName.Text()) : NULL;
	bResult = bResult && szText;
	uiStart = 0;
	uiEnd = 0;
	if (bResult)
	{
		uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
		uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
		ExtendIDEDeletion(szText, &uiStart, &uiEnd, true);
		cFiles.GetEdits(szFileName.Text())->Replace(uiStart, uiEnd, "");
	}
	for (i = 0; bResult && (i < sTarget.asLocations.NumElements()); i++)
	{
		psLocation = sTarget.asLocations.Get(i);
		szText = cFiles.GetText(psLocation->szFileName.Text());
		if (!szText)
		{
			bResult = false;
			break;
		}
		uiAt = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
		if ((StringInsensitiveCompare(psLocation->szFileName.Text(), szFileName.Text()) == 0) && (uiAt >= uiStart) && (uiAt < uiEnd))
		{
			continue;
		}
		if (strncmp(&szText[uiAt], szOldName.Text(), szOldName.Length()) != 0)
		{
			bResult = false;
			break;
		}
		cFiles.GetEdits(psLocation->szFileName.Text())->Replace(uiAt, uiAt + szOldName.Length(), szName.Text());
	}
	if (bResult)
	{
		uiBefore = cFiles.CountErrors(false);
		bResult = cFiles.CountErrors(true) <= uiBefore;
	}
	if (bResult)
	{
		cFiles.Apply();
		pszName->Set(szName);
	}
	cFiles.Kill();
	sTarget.Kill();
	szOldName.Kill();
	szName.Kill();
	szFileName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// Remove Template Parameter (Refactorings.txt, 20, the inverse of Extract
// Template Parameter), for the class template's parameter the caret is
// on: if every place the template is used gives the parameter the same
// argument, the argument takes the parameter's place throughout the
// template and its members' definitions, the parameter goes from each
// template<...> (the whole of it, if it was the only parameter: the class
// is a template no longer, and a member defined outside it in a header
// becomes inline), and the argument goes from every use.  The files
// changed are checked as they would be first.  Where the class is a
// template no longer and a class was its name, it takes the name again
// (FoldNameClass), in the same undo.  The status says what was done, or
// why nothing was.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETemplateParameter::RemoveParameter(CChars* pszStatus)
{
	CIDECodeModel*					pcModel;
	CIDEFileEdits					cFiles;
	CIDERefactorUnit				cUnit;
	CIDERefactorUnit*				pcOther;
	Ptr<CTextArea>					pEditor;
	CArrayIDEOwnTemplates			asOwn;
	CArrayIDEFileRanges				asRanges;
	CArrayIDELocations				asLocations;
	CArrayIDELocations				asDeclared;
	CArrayChars						aszKeys;
	CArrayChars						aszRemovedFiles;
	CArrayInt						aiRemovedStarts;
	CArrayInt						aiRemovedEnds;
	CArrayInt						aiStarts;
	CArrayInt						aiEnds;
	SIDEOwnTemplate*				psOwn;
	SIDELocation*					psLocation;
	CCPPSyntax*						pcNode;
	CCPPSyntaxTemplateDeclaration*	pcTemplate;
	CCPPSyntaxTypeParameter*		pcParameter;
	SCPPSyntaxRange					sRange;
	SCPPSyntaxRange					sToken;
	CChars							szWord;
	CChars							szClassKey;
	CChars							szKey;
	CChars							szScope;
	CChars							szArgument;
	CChars							szOne;
	CChars							szProblem;
	CChars							szMessage;
	const char*						szFileName;
	char*							szText;
	size							uiStart;
	size							uiEnd;
	size							uiLine;
	size							uiParameter;
	size							uiParameters;
	size							uiOpen;
	size							uiClose;
	size							uiUses;
	size							uiBefore;
	size							i;
	size							j;
	size							k;
	int32							iToken;
	int32							iFirst;
	int32							iLast;
	uint8							uiKind;
	bool							bResult;
	bool							bOwn;
	bool							bRemoved;

	if (!CanExtract())
	{
		return false;
	}
	pcModel = mpcContent->GetCodeModel();
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	szFileName = mpcContent->GetDocuments()->GetActiveTab()->GetFileName();
	pEditor = mpcContent->GetDocuments()->GetActiveTab()->GetEditor();
	szText = pEditor->GetText();
	if (!pEditor->GetSelection(&uiStart, &uiEnd))
	{
		uiStart = pEditor->GetCaretPosition();
	}
	if (!IsIDENameChar(szText[uiStart]) && (uiStart > 0) && IsIDENameChar(szText[uiStart - 1]))
	{
		uiStart--;
	}
	for (; (uiStart > 0) && IsIDENameChar(szText[uiStart - 1]); uiStart--)
	{
	}
	for (uiEnd = uiStart; IsIDENameChar(szText[uiEnd]); uiEnd++)
	{
	}
	szWord.Init();
	szWord.AppendSubString(szText, uiStart, uiEnd);
	uiLine = pEditor->FindLine(uiStart);
	szProblem.Init();
	szClassKey.Init();
	szArgument.Init();
	uiParameter = MAX_SIZE;
	uiParameters = 0;
	uiUses = 0;

	//The template the caret's name is a parameter of, and the class template that is, or that its member is of.
	cUnit.Init();
	bResult = false;
	if (cUnit.Parse(pcModel, &mpcContent->GetFindInFiles(), szFileName, false))
	{
		iToken = cUnit.FindTokenAt((int)uiLine, (int)(uiStart - pEditor->GetLineStart(uiLine)));
		pcNode = (iToken != -1) ? cUnit.FindNodeOfTokens(iToken, iToken) : NULL;
		pcTemplate = (CCPPSyntaxTemplateDeclaration*)cUnit.FindAncestor(pcNode, CPPS_TemplateDeclaration);
		while (pcTemplate)
		{
			for (i = 0; i < pcTemplate->macpParameters.NumElements(); i++)
			{
				pcParameter = (CCPPSyntaxTypeParameter*)pcTemplate->macpParameters.GetPtr(i);
				if ((pcParameter->GetKind() == CPPS_TypeParameter) && cUnit.IsToken(pcParameter->miName, szWord.Text()))
				{
					uiParameter = i;
					uiParameters = pcTemplate->macpParameters.NumElements();
				}
			}
			if (uiParameter != MAX_SIZE)
			{
				break;
			}
			pcTemplate = (CCPPSyntaxTemplateDeclaration*)cUnit.FindAncestor(pcTemplate->mpcParent, CPPS_TemplateDeclaration);
		}
		if (pcTemplate && (uiParameter != MAX_SIZE) && pcTemplate->mpcDeclaration)
		{
			//What it declares: "class CName", or a member "... CName<M>::Name(".
			iToken = pcTemplate->mpcDeclaration->miFirstToken;
			if ((cUnit.IsToken(iToken, "class") || cUnit.IsToken(iToken, "struct")) && cUnit.GetTokensRange(iToken + 1, iToken + 1, &sToken))
			{
				bResult = pcModel->GetSymbolAt(szFileName, sToken.iLine, sToken.iColumn, &szClassKey, &uiKind) && (uiKind == CPPSK_ClassTemplate);
			}
			else
			{
				for (; (iToken < pcTemplate->mpcDeclaration->miLastToken) && !cUnit.IsToken(iToken, "(") && !bResult; iToken++)
				{
					bResult = cUnit.GetTokensRange(iToken, iToken, &sToken) && pcModel->GetSymbolAt(szFileName, sToken.iLine, sToken.iColumn, &szClassKey, &uiKind) && (uiKind == CPPSK_ClassTemplate);
				}
			}
		}
	}
	cUnit.Kill();
	if (!bResult)
	{
		szProblem.Set((uiParameter == MAX_SIZE) ? "The caret isn't on a template's parameter." :
					  "Only a class template's parameter can be removed: a function template's arguments aren't written at its calls.");
	}

	//Every declaration that is the template's: the class, and its members' definitions outside it.
	asOwn.Init();
	asDeclared.Init();
	cFiles.Init(mpcContent);
	if (bResult)
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(szClassKey.Text(), &asLocations, &uiKind);
		for (i = 0; i < asLocations.NumElements(); i++)
		{
			psLocation = asLocations.Get(i);
			szText = cFiles.GetText(psLocation->szFileName.Text());
			uiStart = szText ? FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn) : 0;
			if ((psLocation->uiRole != CPPIR_Reference) || ((uiStart >= 3) && (szText[uiStart - 1] == '~') && (szText[uiStart - 2] == ':')))
			{
				//The class, a declaration of it ahead, or its destructor's definition.
				memcpy(asDeclared.Add(), psLocation, sizeof(SIDELocation));
				asDeclared.Get(asDeclared.NumElements() - 1)->szFileName.Init(psLocation->szFileName);
			}
			if (psLocation->uiRole == CPPIR_Definition)
			{
				//Its members: what is declared from there on that is the template's.
				asRanges.Init();
				asRanges.Add()->Init(psLocation->szFileName.Text(), psLocation->iLine, 0, 0x7fffffff, 0);
				aszKeys.Init();
				pcModel->FindSymbolsDeclaredIn(&asRanges, &aszKeys);
				for (j = 0; j < aszKeys.NumElements(); j++)
				{
					szScope.Init();
					GetIDESymbolScope(aszKeys.GetText(j), &szScope);
					if (szScope.Equals(szClassKey.Text()))
					{
						pcModel->GetSymbolLocations(aszKeys.GetText(j), &asDeclared, &uiKind);
					}
					szScope.Kill();
				}
				aszKeys.Kill();
				KillIDEFileRanges(&asRanges);
			}
		}
		KillIDELocations(&asLocations);

		for (i = 0; bResult && (i < asDeclared.NumElements()); i++)
		{
			psLocation = asDeclared.Get(i);
			if (psLocation->uiRole == CPPIR_Reference)
			{
				continue;
			}
			szText = cFiles.GetText(psLocation->szFileName.Text());
			pcOther = NewMalloc<CIDERefactorUnit>();
			pcOther->Init();
			pcTemplate = NULL;
			if (szText && pcOther->Parse(pcModel, &mpcContent->GetFindInFiles(), psLocation->szFileName.Text(), false))
			{
				iToken = pcOther->FindTokenAt(psLocation->iLine, psLocation->iColumn);
				pcNode = (iToken != -1) ? pcOther->FindNodeOfTokens(iToken, iToken) : NULL;
				pcTemplate = (CCPPSyntaxTemplateDeclaration*)pcOther->FindAncestor(pcNode, CPPS_TemplateDeclaration);
				//The outermost that has the template's number of parameters: a member's own template<...> is inside it.
				for (pcNode = pcTemplate ? pcTemplate->mpcParent : NULL; pcNode; pcNode = pcNode->mpcParent)
				{
					if (pcNode->GetKind() == CPPS_TemplateDeclaration)
					{
						pcTemplate = (CCPPSyntaxTemplateDeclaration*)pcNode;
					}
				}
			}
			if (pcTemplate && (pcTemplate->macpParameters.NumElements() == uiParameters) && pcTemplate->mpcDeclaration &&
				pcOther->GetRange(pcTemplate, &sRange) && pcOther->GetTokensRange(pcTemplate->mpcDeclaration->miFirstToken, pcTemplate->mpcDeclaration->miFirstToken, &sToken))
			{
				uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
				bOwn = false;
				for (j = 0; j < asOwn.NumElements(); j++)
				{
					bOwn |= (asOwn.Get(j)->uiStart == uiStart) && (StringInsensitiveCompare(asOwn.Get(j)->szFileName.Text(), psLocation->szFileName.Text()) == 0);
				}
				if (!bOwn)
				{
					psOwn = asOwn.Add();
					psOwn->szFileName.Init(psLocation->szFileName);
					psOwn->uiStart = uiStart;
					psOwn->uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
					psOwn->uiHeaderEnd = FindIDETextOffset(szText, sToken.iLine, sToken.iColumn);
					psOwn->bMember = !pcOther->IsToken(pcTemplate->mpcDeclaration->miFirstToken, "class") && !pcOther->IsToken(pcTemplate->mpcDeclaration->miFirstToken, "struct");
					pcParameter = (CCPPSyntaxTypeParameter*)pcTemplate->macpParameters.GetPtr(uiParameter);
					psOwn->szParameter.Init();
					pcOther->AppendTokens(pcParameter->miName, pcParameter->miName, &psOwn->szParameter);
					iFirst = pcParameter->miFirstToken;
					iLast = pcParameter->miLastToken;
					if (uiParameter + 1 < uiParameters)
					{
						//To the next parameter: its comma with it.
						iLast = pcTemplate->macpParameters.GetPtr(uiParameter + 1)->miFirstToken - 1;
					}
					else if (uiParameter > 0)
					{
						iFirst = pcTemplate->macpParameters.GetPtr(uiParameter - 1)->miLastToken + 1;
					}
					pcOther->GetTokensRange(iFirst, iFirst, &sToken);
					psOwn->uiParameterStart = FindIDETextOffset(szText, sToken.iLine, sToken.iColumn);
					if (uiParameter + 1 < uiParameters)
					{
						pcOther->GetTokensRange(iLast + 1, iLast + 1, &sToken);
						psOwn->uiParameterEnd = FindIDETextOffset(szText, sToken.iLine, sToken.iColumn);
					}
					else
					{
						pcOther->GetTokensRange(iLast, iLast, &sToken);
						psOwn->uiParameterEnd = FindIDETextOffset(szText, sToken.iEndLine, sToken.iEndColumn);
					}
				}
			}
			pcOther->Kill();
			free(pcOther);
		}
	}

	//Every place the template is named: its argument for the parameter, which goes, and outside its own
	//declarations must be one argument.
	aszRemovedFiles.Init();
	aiRemovedStarts.Init();
	aiRemovedEnds.Init();
	if (bResult)
	{
		asLocations.Init();
		pcModel->GetSymbolLocations(szClassKey.Text(), &asLocations, &uiKind);
		szOne.Init();
		GetIDESymbolName(szClassKey.Text(), &szOne);
		for (i = 0; bResult && (i < asLocations.NumElements()); i++)
		{
			psLocation = asLocations.Get(i);
			szText = cFiles.GetText(psLocation->szFileName.Text());
			if (!szText)
			{
				continue;
			}
			uiStart = FindIDETextOffset(szText, psLocation->iLine, psLocation->iColumn);
			bOwn = false;
			for (j = 0; j < asOwn.NumElements(); j++)
			{
				psOwn = asOwn.Get(j);
				bOwn |= (uiStart >= psOwn->uiStart) && (uiStart < psOwn->uiEnd) && (StringInsensitiveCompare(psOwn->szFileName.Text(), psLocation->szFileName.Text()) == 0);
			}
			aiStarts.Init();
			aiEnds.Init();
			if (!FindIDETemplateArguments(szText, uiStart + szOne.Length(), &aiStarts, &aiEnds, &uiOpen, &uiClose) || (aiStarts.NumElements() != uiParameters))
			{
				if (!bOwn)
				{
					szProblem.Set("It is named without its arguments in ");
					szProblem.Append(GetIDEFileNamePart(psLocation->szFileName.Text()))->Append(" at line ")->Append((int32)(psLocation->iLine + 1))->Append('.');
					bResult = false;
				}
				aiEnds.Kill();
				aiStarts.Kill();
				continue;
			}
			if (!bOwn)
			{
				szKey.Init();
				szKey.AppendSubString(szText, (size)aiStarts.GetValue(uiParameter), (size)aiEnds.GetValue(uiParameter));
				szKey.StripWhitespace();
				if (uiUses == 0)
				{
					szArgument.Set(szKey);
				}
				else if (!szArgument.Equals(szKey.Text()))
				{
					szProblem.Set("It isn't given one argument everywhere: ");
					szProblem.Append(szArgument)->Append(" and ")->Append(szKey)->Append(" (")->Append(GetIDEFileNamePart(psLocation->szFileName.Text()))->Append(" line ")->Append((int32)(psLocation->iLine + 1))->Append(").");
					bResult = false;
				}
				szKey.Kill();
				uiUses++;
			}
			//The argument out: all of <...> if it is the only one, else with its comma.
			if (uiParameters == 1)
			{
				uiStart = uiOpen;
				uiEnd = uiClose + 1;
			}
			else if (uiParameter + 1 < uiParameters)
			{
				uiStart = (size)aiStarts.GetValue(uiParameter);
				uiEnd = (size)aiStarts.GetValue(uiParameter + 1);
				for (; (szText[uiEnd] == ' ') || (szText[uiEnd] == '\t'); uiEnd++)
				{
				}
			}
			else
			{
				uiStart = (size)aiEnds.GetValue(uiParameter - 1);
				uiEnd = (size)aiEnds.GetValue(uiParameter);
			}
			aszRemovedFiles.Add(psLocation->szFileName.Text());
			aiRemovedStarts.Add((int)uiStart);
			aiRemovedEnds.Add((int)uiEnd);
			aiEnds.Kill();
			aiStarts.Kill();
		}
		szOne.Kill();
		KillIDELocations(&asLocations);
		if (bResult && (uiUses == 0))
		{
			szProblem.Set("Nothing uses the template: there is no argument to put in the parameter's place.");
			bResult = false;
		}
	}

	if (bResult)
	{
		for (i = 0; i < aiRemovedStarts.NumElements(); i++)
		{
			cFiles.GetEdits(aszRemovedFiles.GetText(i))->Replace((size)aiRemovedStarts.GetValue(i), (size)aiRemovedEnds.GetValue(i), "");
		}
		for (i = 0; i < asOwn.NumElements(); i++)
		{
			psOwn = asOwn.Get(i);
			szText = cFiles.GetText(psOwn->szFileName.Text());
			if (uiParameters == 1)
			{
				//No template any more; a member defined in a header is inline, as every file that has it defines it.
				cFiles.GetEdits(psOwn->szFileName.Text())->Replace(psOwn->uiStart, psOwn->uiHeaderEnd, (psOwn->bMember && (CIDEIcons::GetFileType(psOwn->szFileName.Text()) == IFT_Header)) ? "inline " : "");
			}
			else
			{
				cFiles.GetEdits(psOwn->szFileName.Text())->Replace(psOwn->uiParameterStart, psOwn->uiParameterEnd, "");
			}
			//The parameter's name, where it isn't in an argument that goes.
			for (j = psOwn->uiHeaderEnd; j < psOwn->uiEnd; j++)
			{
				if (!IsIDENameChar(szText[j]) || IsIDENameChar(szText[j - 1]))
				{
					continue;
				}
				for (uiEnd = j; IsIDENameChar(szText[uiEnd]); uiEnd++)
				{
				}
				if ((uiEnd - j == psOwn->szParameter.Length()) && (strncmp(&szText[j], psOwn->szParameter.Text(), uiEnd - j) == 0))
				{
					bRemoved = false;
					for (k = 0; k < aiRemovedStarts.NumElements(); k++)
					{
						bRemoved |= (j >= (size)aiRemovedStarts.GetValue(k)) && (j < (size)aiRemovedEnds.GetValue(k)) && (StringInsensitiveCompare(aszRemovedFiles.GetText(k), psOwn->szFileName.Text()) == 0);
					}
					if (!bRemoved)
					{
						cFiles.GetEdits(psOwn->szFileName.Text())->Replace(j, uiEnd, szArgument.Text());
					}
				}
				j = uiEnd;
			}
		}
		uiBefore = cFiles.CountErrors(false);
		if (cFiles.CountErrors(true) > uiBefore)
		{
			szProblem.Set("The code doesn't compile without it.");
			bResult = false;
		}
	}

	szMessage.Init();
	if (bResult)
	{
		mpcContent->GetEdit()->BeginUndoGroup();
		cFiles.Apply();
		szOne.Init();
		GetIDESymbolName(szClassKey.Text(), &szOne);
		szMessage.Append("Removed the template parameter ")->Append(szWord)->Append(" from ")->Append(szOne)->Append(": it is ")->Append(szArgument);
		if (uiUses == 1)
		{
			szMessage.Append(" in the one place the template is used.");
		}
		else
		{
			szMessage.Append(" in all ")->Append((uint32)uiUses)->Append(" places the template is used.");
		}
		szOne.Kill();

		//A template no longer: the class that was its name gives it the name back.
		szOne.Init();
		if ((uiParameters == 1) && FoldNameClass(szClassKey.Text(), &szOne))
		{
			szMessage.Append("  It is ")->Append(szOne)->Append(" again, which was the class that was its name.");
		}
		szOne.Kill();
		mpcContent->GetEdit()->EndUndoGroup();
	}
	else
	{
		szMessage.Append("Can't remove the template parameter ")->Append(szWord)->Append(": ")->Append(szProblem);
	}
	if (pszStatus)
	{
		pszStatus->Set(szMessage);
	}
	else
	{
		mpcContent->ShowStatus(szMessage.Text());
	}
	szMessage.Kill();
	aiRemovedEnds.Kill();
	aiRemovedStarts.Kill();
	aszRemovedFiles.Kill();
	cFiles.Kill();
	KillIDELocations(&asDeclared);
	for (i = 0; i < asOwn.NumElements(); i++)
	{
		asOwn.Get(i)->szParameter.Kill();
		asOwn.Get(i)->szFileName.Kill();
	}
	asOwn.Kill();
	szArgument.Kill();
	szClassKey.Kill();
	szProblem.Kill();
	szWord.Kill();
	return bResult;
}
