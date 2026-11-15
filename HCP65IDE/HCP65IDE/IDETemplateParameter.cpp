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
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDETemplateExtraction::Kill(void)
{
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
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETemplateParameter::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.ExtractTemplateParameter"));
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
	}
	cUnit.Kill();
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
	CChars					szMessage;
	CChars					szName;
	bool					bResult;

	sExtraction.Init();
	if (!FindExtraction(&sExtraction))
	{
		mpcContent->ShowStatus("The caret isn't on a type in a function's declaration or body.");
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
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(480);
	szName.Init(pName->GetText());
	pName = NULL;
	mpDialog = NULL;
	if (bResult && GetProblem(&sExtraction, szName.Text()))
	{
		mpcContent->ShowStatus(GetProblem(&sExtraction, szName.Text()));
		bResult = false;
	}
	bResult = bResult && Extract(&sExtraction, szName.Text());
	szName.Kill();
	sExtraction.Kill();
	return bResult;
}
