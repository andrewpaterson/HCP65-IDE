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
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "CppCompilerLib/CPPSyntaxMemberExpression.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESafeDelete.h"
#include "IDEDefinitions.h"
#include "IDEMoveMembers.h"
#include "IDEEncapsulateField.h"
#include "IDEContextActions.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFieldUse::Init(void)
{
	szFileName.Init();
	iLine = 0;
	iKind = IDE_FIELD_UNCHANGEABLE;
	bInside = false;
	uiStart = 0;
	uiEnd = 0;
	szObject.Init();
	szValue.Init();
	uiValueEnd = 0;
	szOperator.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEFieldUse::Kill(void)
{
	szOperator.Kill();
	szValue.Kill();
	szObject.Kill();
	szFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEEncapsulation::Init(void)
{
	bPossible = false;
	szWhyNot.Init();
	szKey.Init();
	szName.Init();
	szClassKey.Init();
	szType.Init();
	bObject = false;
	bPublic = false;
	szFileName.Init();
	uiDeclaration = 0;
	uiDeclarationEnd = 0;
	asUses.Init();
	szGetter.Init();
	szSetter.Init();
	szParameter.Init();
	bGetter = true;
	bSetter = true;
	bInside = false;
	bProtect = true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEEncapsulation::Kill(void)
{
	size	i;

	szParameter.Kill();
	szSetter.Kill();
	szGetter.Kill();
	for (i = 0; i < asUses.NumElements(); i++)
	{
		asUses.Get(i)->Kill();
	}
	asUses.Kill();
	szFileName.Kill();
	szType.Kill();
	szClassKey.Kill();
	szName.Kill();
	szKey.Kill();
	szWhyNot.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::Init(CIDEContent* pcContent)
{
	PreInit();
	mpcContent = pcContent;
	PostInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::Class(void)
{
	U_Pointer(mpcContent);
	M_Pointer(mpDialog);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::Free(void)
{
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEncapsulateField::Save(CObjectWriter* pcFile) { return false; }
bool CIDEEncapsulateField::Load(CObjectReader* pcFile) { return false; }


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ gives it no key.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::AddCommands(Ptr<CCommandSet> pCommands)
{
	pCommands->Add("Refactor.EncapsulateField", "Encapsulate Field...", "", Ptr<CImageCel>(), this, &CIDEEncapsulateField::QueueEncapsulate)->SetEnabledCheck(this, &CIDEEncapsulateField::CanEncapsulate);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::AddEditorItems(Ptr<CMenuContainer> pRefactor)
{
	pRefactor->AddItem(mpcContent->GetCommands()->Find("Refactor.EncapsulateField"));
}


//////////////////////////////////////////////////////////////////////////
//
// Show Context Actions: Encapsulate Field is for a field.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::AddContextActions(SIDECaretContext* psContext)
{
	if (psContext->bSymbol && (psContext->uiKind == CPPSK_Field))
	{
		psContext->Offer("Refactor.EncapsulateField");
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEncapsulateField::CanEncapsulate(void)
{
	int		iLine;
	int		iColumn;
	uint8	uiRole;

	return mpcContent->GetDocuments()->GetActiveTab().IsNotNull() && mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole);
}


void CIDEEncapsulateField::QueueEncapsulate(void) { mpcContent->GetWindow()->Post(this, &CIDEEncapsulateField::ShowQueuedEncapsulate); }
void CIDEEncapsulateField::ShowQueuedEncapsulate(void) { ShowEncapsulate(); }


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEncapsulateField::Refuse(SIDEEncapsulation* psDest, const char* szWhyNot)
{
	psDest->bPossible = false;
	psDest->szWhyNot.Set(szWhyNot);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Every place the field is named, and how it is used there: by the
// expression it is in and the compiler's answer to how that changes it.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::ReadUses(SIDEEncapsulation* psDest)
{
	static const ECppTokens			aeCompound[] = { CT_AssignmentAdd, CT_AssignmentSubtract, CT_AssignmentMultiply, CT_AssignmentDivide, CT_AssignmentModulus, CT_AssignmentBitwiseAnd,
													 CT_AssignmentBitwiseOr, CT_AssignmentBitwiseXor, CT_AssignmentLeftShift, CT_AssignmentRightShift };
	static const char*				aszCompound[] = { "+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>" };
	CIDECodeModel*					pcModel;
	CIDERefactorUnit				cUnit;
	CTextFile						cFile;
	CIDEFileTexts*					pcTexts;
	CArrayIDELocations				asLocations;
	CArrayChars						aszFiles;
	SIDELocation*					psLocation;
	SIDEFieldUse*					psUse;
	CCPPSyntax*						pcNode;
	CCPPSyntax*						pcUse;
	CCPPSyntax*						pcTop;
	CCPPSyntax*						pcParent;
	CCPPSyntaxMemberExpression*		pcMember;
	CCPPSyntaxBinaryExpression*		pcBinary;
	CCPPSyntaxUnaryExpression*		pcUnary;
	CCPPSyntaxFunctionDefinition*	pcFunction;
	CCPPSyntaxName*					pcName;
	SCPPSyntaxRange					sRange;
	SCPPSyntaxRange					sName;
	CChars							szKey;
	CChars							szScope;
	char*							szText;
	size							uiLength;
	size							uiAt;
	size							i;
	size							j;
	size							k;
	int32							iToken;
	uint8							uiKind;
	bool							bStatement;

	pcModel = mpcContent->GetCodeModel();
	pcTexts = &mpcContent->GetFindInFiles();
	asLocations.Init();
	pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
	aszFiles.Init();
	for (i = 0; i < asLocations.NumElements(); i++)
	{
		if ((asLocations.Get(i)->uiRole == CPPIR_Reference) && (pcModel->FindProjectOfFile(asLocations.Get(i)->szFileName.Text()) != MAX_SIZE))
		{
			aszFiles.AddIfUnique(asLocations.Get(i)->szFileName.Text());
		}
	}
	for (i = 0; i < aszFiles.NumElements(); i++)
	{
		cFile.Init();
		cUnit.Init();
		szText = NULL;
		if (!pcTexts->GetFileText(aszFiles.GetText(i), &cFile, &szText, &uiLength) || !cUnit.Parse(pcModel, pcTexts, aszFiles.GetText(i), true))
		{
			szText = NULL;
		}
		for (j = 0; j < asLocations.NumElements(); j++)
		{
			psLocation = asLocations.Get(j);
			if ((psLocation->uiRole != CPPIR_Reference) || (StringInsensitiveCompare(psLocation->szFileName.Text(), aszFiles.GetText(i)) != 0))
			{
				continue;
			}
			psUse = psDest->asUses.Add();
			psUse->Init();
			psUse->szFileName.Set(psLocation->szFileName);
			psUse->iLine = psLocation->iLine;
			if (!szText)
			{
				continue;
			}
			iToken = cUnit.FindTokenAt(psLocation->iLine, psLocation->iColumn);
			pcNode = cUnit.FindNodeOfTokens(iToken, iToken);
			if (!pcNode || !cUnit.GetTokensRange(iToken, iToken, &sName))
			{
				continue;
			}
			psUse->uiStart = FindIDETextOffset(szText, sName.iLine, sName.iColumn);
			psUse->uiEnd = FindIDETextOffset(szText, sName.iEndLine, sName.iEndColumn);

			//The expression that names it: "x.miLevel" or "miLevel", and what it is named through.
			pcMember = (CCPPSyntaxMemberExpression*)cUnit.FindAncestor(pcNode, CPPS_MemberExpression);
			pcUse = (pcMember && pcMember->mpcName && (pcMember->mpcName->miFirstToken <= iToken) && (pcMember->mpcName->miLastToken >= iToken)) ? pcMember : cUnit.FindAncestor(pcNode, CPPS_IdExpression);
			if (!pcUse || !cUnit.GetRange(pcUse, &sRange))
			{
				continue;
			}
			if (pcUse == pcMember)
			{
				psUse->szObject.AppendSubString(szText, FindIDETextOffset(szText, sRange.iLine, sRange.iColumn), psUse->uiStart);
			}
			pcTop = pcUse;
			while (pcTop->mpcParent && (pcTop->mpcParent->GetKind() == CPPS_ParenthesisedExpression))
			{
				pcTop = pcTop->mpcParent;
			}
			pcParent = pcTop->mpcParent;

			//In one of its class's own functions?
			pcFunction = (CCPPSyntaxFunctionDefinition*)cUnit.FindAncestor(pcUse, CPPS_FunctionDefinition);
			pcName = (pcFunction && pcFunction->mpcDeclarator) ? pcFunction->mpcDeclarator->GetName() : NULL;
			szKey.Init();
			if (pcName && pcName->GetLast() && cUnit.GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, &sRange) &&
				pcModel->GetSymbolAt(aszFiles.GetText(i), sRange.iLine, sRange.iColumn, &szKey, &uiKind))
			{
				szScope.Init();
				GetIDESymbolScope(szKey.Text(), &szScope);
				psUse->bInside = szScope.Equals(psDest->szClassKey.Text());
				szScope.Kill();
			}
			szKey.Kill();

			pcBinary = (pcParent && (pcParent->GetKind() == CPPS_BinaryExpression)) ? (CCPPSyntaxBinaryExpression*)pcParent : NULL;
			pcUnary = (pcParent && (pcParent->GetKind() == CPPS_UnaryExpression)) ? (CCPPSyntaxUnaryExpression*)pcParent : NULL;
			bStatement = pcParent && pcParent->mpcParent && (pcParent->mpcParent->GetKind() == CPPS_ExpressionStatement);
			if (psDest->bObject)
			{
				//An object: a member of it named (the . after it goes too), or its address taken.
				for (uiAt = psUse->uiEnd; (szText[uiAt] == ' ') || (szText[uiAt] == '\t'); uiAt++)
				{
				}
				if (pcParent && (pcParent->GetKind() == CPPS_MemberExpression) && (((CCPPSyntaxMemberExpression*)pcParent)->mpcObject == pcTop) && (pcTop == pcUse) && (szText[uiAt] == '.'))
				{
					psUse->iKind = IDE_FIELD_OBJECT_MEMBER;
					psUse->uiEnd = uiAt + 1;
				}
				else if (pcUnary && cUnit.IsToken(pcUnary->miFirstToken, "&") && !pcUnary->mbPostfix && cUnit.GetRange(pcUnary, &sRange))
				{
					psUse->iKind = IDE_FIELD_OBJECT_ADDRESS;
					psUse->uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
				}
			}
			else if (pcBinary && (pcBinary->mpcLeft == pcTop) && bStatement && cUnit.GetRange(pcBinary->mpcRight, &sRange))
			{
				//All of what a statement assigns, or a += and the like.
				AppendIDERangeText(szText, &sRange, &psUse->szValue);
				if (pcBinary->meOperator == CT_Assignment)
				{
					psUse->iKind = IDE_FIELD_ASSIGNED;
				}
				for (k = 0; k < sizeof(aeCompound) / sizeof(ECppTokens); k++)
				{
					if (pcBinary->meOperator == aeCompound[k])
					{
						psUse->iKind = IDE_FIELD_COMPOUND;
						psUse->szOperator.Set(aszCompound[k]);
					}
				}
				if (psUse->iKind != IDE_FIELD_UNCHANGEABLE)
				{
					//From its name to what is assigned; the setter's ) goes after that.
					psUse->uiEnd = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
					psUse->uiValueEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
				}
			}
			else if (pcUnary && ((pcUnary->meOperator == CT_Increment) || (pcUnary->meOperator == CT_Decrement)) && bStatement && cUnit.GetRange(pcUnary, &sRange))
			{
				psUse->iKind = IDE_FIELD_STEPPED;
				psUse->szOperator.Set((pcUnary->meOperator == CT_Increment) ? "+" : "-");
				psUse->uiStart = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
				psUse->uiEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
			}
			else if (cUnit.IsChecked() && (cUnit.GetPass()->FindChanges(pcTop) == 0) && !(pcBinary && (pcBinary->mpcLeft == pcTop) && (pcBinary->meOperator == CT_Assignment)))
			{
				psUse->iKind = IDE_FIELD_READ;
			}
		}
		cUnit.Kill();
		cFile.Kill();
	}
	aszFiles.Kill();
	KillIDELocations(&asLocations);
}


//////////////////////////////////////////////////////////////////////////
//
// What Encapsulate Field has to work with, for the field named at the
// caret.  False if there is no field there; psDest->bPossible says
// whether it can be encapsulated, szWhyNot why not.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEncapsulateField::FindEncapsulation(SIDEEncapsulation* psDest)
{
	CIDECodeModel*					pcModel;
	CIDERefactorUnit				cUnit;
	CTextFile						cFile;
	CIDEFileTexts*					pcTexts;
	CArrayIDELocations				asLocations;
	SIDELocation*					psLocation;
	CCPPSyntaxDeclarator*			pcDeclarator;
	CCPPSyntaxSimpleDeclaration*	pcDeclaration;
	CCPPSyntax*						pcParent;
	CCPPSyntaxName*					pcName;
	SPreprocessedToken*				psSource;
	SCPPSyntaxRange					sRange;
	CChars							szType;
	const char*						szName;
	char*							szText;
	size							uiLength;
	size							i;
	int								iLine;
	int								iColumn;
	int32							iToken;
	uint8							uiRole;
	uint8							uiKind;

	pcModel = mpcContent->GetCodeModel();
	pcTexts = &mpcContent->GetFindInFiles();
	if (mpcContent->GetDocuments()->GetActiveTab().IsNull())
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	if (!mpcContent->GetDocuments()->FindSymbolPlace(&iLine, &iColumn, &uiRole) ||
		!pcModel->GetSymbolAt(mpcContent->GetDocuments()->GetActiveTab()->GetFileName(), iLine, iColumn, &psDest->szKey, &uiKind) || (uiKind != CPPSK_Field))
	{
		return false;
	}
	GetIDESymbolName(psDest->szKey.Text(), &psDest->szName);
	GetIDESymbolScope(psDest->szKey.Text(), &psDest->szClassKey);
	psDest->bPossible = true;

	//Its declaration in its class.
	asLocations.Init();
	pcModel->GetSymbolLocations(psDest->szKey.Text(), &asLocations, &uiKind);
	psLocation = NULL;
	for (i = 0; !psLocation && (i < asLocations.NumElements()); i++)
	{
		psLocation = ((asLocations.Get(i)->uiRole != CPPIR_Reference) && (pcModel->FindProjectOfFile(asLocations.Get(i)->szFileName.Text()) != MAX_SIZE)) ? asLocations.Get(i) : NULL;
	}
	cFile.Init();
	cUnit.Init();
	szText = NULL;
	if (!psLocation || !pcTexts->GetFileText(psLocation->szFileName.Text(), &cFile, &szText, &uiLength) || !cUnit.Parse(pcModel, pcTexts, psLocation->szFileName.Text(), false))
	{
		Refuse(psDest, "its declaration isn't in a file of the solution");
	}
	else
	{
		psDest->szFileName.Set(psLocation->szFileName);
		pcDeclarator = (CCPPSyntaxDeclarator*)cUnit.FindAncestor(cUnit.FindNodeAt(psLocation->iLine, psLocation->iColumn), CPPS_Declarator);
		while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
		{
			pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
		}
		pcName = pcDeclarator ? pcDeclarator->GetName() : NULL;
		pcParent = pcDeclarator ? pcDeclarator->mpcParent : NULL;
		pcParent = (pcParent && (pcParent->GetKind() == CPPS_InitDeclarator)) ? pcParent->mpcParent : pcParent;
		pcDeclaration = (pcParent && (pcParent->GetKind() == CPPS_SimpleDeclaration)) ? (CCPPSyntaxSimpleDeclaration*)pcParent : NULL;
		if (!pcName || !pcDeclaration || !pcDeclaration->mpcSpecifiers || (pcDeclaration->mpcSpecifiers->miFirstToken < 0) || !cUnit.GetRange(pcDeclaration, &sRange))
		{
			Refuse(psDest, "its declaration can't be read (a macro makes it)");
		}
		else if (pcDeclaration->macpDeclarators.NumElements() != 1)
		{
			Refuse(psDest, "it is declared with others in one declaration");
		}
		else if (pcDeclarator->miLastToken > pcName->miLastToken)
		{
			Refuse(psDest, "it is an array");
		}
		else
		{
			psDest->uiDeclaration = FindIDETextOffset(szText, sRange.iLine, sRange.iColumn);
			psDest->uiDeclarationEnd = FindIDETextOffset(szText, sRange.iEndLine, sRange.iEndColumn);
			cUnit.GetTokensRange(pcDeclaration->mpcSpecifiers->miFirstToken, pcName->miFirstToken - 1, &sRange);
			AppendIDERangeText(szText, &sRange, &psDest->szType);
			psDest->szType.StripWhitespace();
			if (pcDeclaration->mpcSpecifiers->mpcType && (pcDeclarator->macpPrefix.NumElements() == 0))
			{
				for (iToken = pcDeclaration->mpcSpecifiers->mpcType->miFirstToken; cUnit.IsToken(iToken, "struct") || cUnit.IsToken(iToken, "class"); iToken++)
				{
				}
				psSource = cUnit.GetTokeniser()->GetSource(cUnit.GetTokeniser()->Get((size)iToken));
				szType.Init();
				psDest->bObject = pcModel->GetSymbolAt(psLocation->szFileName.Text(), psSource->iLine, psSource->iColumn, &szType, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate));
				szType.Kill();
			}
		}
	}
	cUnit.Kill();
	cFile.Kill();
	KillIDELocations(&asLocations);

	if (psDest->bPossible)
	{
		psDest->bPublic = (mpcContent->GetMoveMembers()->FindMemberAccess(psDest->szKey.Text()) == IDE_ACCESS_PUBLIC);
		psDest->bSetter = !psDest->bObject;
		psDest->bProtect = psDest->bPublic;

		//Their names: the field's without its m and its type's letters ("miLevel": GetLevel, SetLevel, iLevel).
		szName = psDest->szName.Text();
		szName = ((szName[0] == 'm') && (szName[1] != '\0')) ? &szName[1] : szName;
		psDest->szParameter.Set((szName == psDest->szName.Text()) ? "value" : szName);
		for (i = 0; (szName[i] >= 'a') && (szName[i] <= 'z'); i++)
		{
		}
		szName = ((szName[i] >= 'A') && (szName[i] <= 'Z')) ? &szName[i] : szName;
		psDest->szGetter.Set("Get");
		psDest->szGetter.Append(szName);
		psDest->szSetter.Set("Set");
		psDest->szSetter.Append(szName);
		if ((psDest->szGetter.Text()[3] >= 'a') && (psDest->szGetter.Text()[3] <= 'z'))
		{
			psDest->szGetter.Text()[3] -= 'a' - 'A';
			psDest->szSetter.Text()[3] -= 'a' - 'A';
		}
		ReadUses(psDest);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Why what is asked can't be done: NULL if it can.
//
//////////////////////////////////////////////////////////////////////////
const char* CIDEEncapsulateField::GetProblem(SIDEEncapsulation* psEncapsulation)
{
	CChars	szKey;
	bool	bTaken;

	if (!psEncapsulation->bGetter && !psEncapsulation->bSetter)
	{
		return "Neither a getter nor a setter is asked for.";
	}
	if (psEncapsulation->bSetter && psEncapsulation->bObject)
	{
		return "A field that is an object has no setter: its getter gives a pointer to it.";
	}
	if ((psEncapsulation->bGetter && !IsIDEIdentifier(psEncapsulation->szGetter.Text())) || (psEncapsulation->bSetter && !IsIDEIdentifier(psEncapsulation->szSetter.Text())))
	{
		return "The getter's or the setter's name isn't a name.";
	}
	szKey.Init(psEncapsulation->szClassKey);
	szKey.Append("::");
	szKey.Append(psEncapsulation->szGetter);
	szKey.Append("()");
	bTaken = psEncapsulation->bGetter && mpcContent->GetCodeModel()->HasSymbol(szKey.Text());
	szKey.Append(" const");
	bTaken = bTaken || (psEncapsulation->bGetter && mpcContent->GetCodeModel()->HasSymbol(szKey.Text()));
	szKey.Kill();
	return bTaken ? "The class has a function of the getter's name already." : NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a place is given a getter or setter: the class's own functions
// only if asked, and only where the function that place needs is made.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEncapsulateField::IsReplaced(SIDEEncapsulation* psEncapsulation, SIDEFieldUse* psUse)
{
	if (psUse->bInside && !psEncapsulation->bInside)
	{
		return false;
	}
	switch (psUse->iKind)
	{
	case IDE_FIELD_READ:
	case IDE_FIELD_OBJECT_MEMBER:
	case IDE_FIELD_OBJECT_ADDRESS:
		return psEncapsulation->bGetter;
	case IDE_FIELD_ASSIGNED:
		return psEncapsulation->bSetter;
	case IDE_FIELD_COMPOUND:
	case IDE_FIELD_STEPPED:
		return psEncapsulation->bGetter && psEncapsulation->bSetter;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What stands in the way: the places outside the class that stay naming
// the field, which won't compile once it is protected.
//
//////////////////////////////////////////////////////////////////////////
void CIDEEncapsulateField::FindConflicts(SIDEEncapsulation* psEncapsulation, CArrayChars* paszDest)
{
	SIDEFieldUse*	psUse;
	CChars			szLine;
	size			i;

	for (i = 0; i < psEncapsulation->asUses.NumElements(); i++)
	{
		psUse = psEncapsulation->asUses.Get(i);
		if (psUse->bInside || IsReplaced(psEncapsulation, psUse) || !psEncapsulation->bProtect)
		{
			continue;
		}
		szLine.Init(psEncapsulation->szName);
		szLine.Append(" is used in ");
		szLine.Append(GetIDEFileNamePart(psUse->szFileName.Text()));
		szLine.Append(" at line ");
		szLine.Append((int32)(psUse->iLine + 1));
		szLine.Append((psUse->iKind == IDE_FIELD_UNCHANGEABLE) ? " in a way a getter or a setter can't stand for" : " and the function that would stand for it isn't asked for");
		szLine.Append(": it stays, and won't compile once the field is protected.");
		paszDest->Add(szLine.Text());
		szLine.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The getter and setter written, the places changed, and the field made
// protected, as asked.  One undo across the files.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEncapsulateField::Encapsulate(SIDEEncapsulation* psEncapsulation, CChars* pszStatus)
{
	CIDEFileEdits		cFiles;
	CIDEMoveMembers*	pcMembers;
	SIDEMoveClass		sClass;
	SIDEFieldUse*		psUse;
	CIDERefactorUnit*	pcUnit;
	SCPPSyntaxRange		sRange;
	CChars				szText;
	CChars				szLine;
	CChars				szMessage;
	CChars				szSource;
	CChars				szScoped;
	CChars				szBody;
	char*				szFile;
	size				uiChanged;
	size				uiStart;
	size				uiEnd;
	size				uiAt;
	size				i;
	int32				iBoundary;
	int32				iClose;
	int32				iFirst;
	int32				iLast;
	bool				bResult;
	bool				bTemplates;

	if (!psEncapsulation->bPossible || GetProblem(psEncapsulation))
	{
		return false;
	}
	mpcContent->GetDocuments()->UpdateEditedTabsNow();
	pcMembers = &mpcContent->GetMoveMembers();
	cFiles.Init(mpcContent);

	//The places.
	uiChanged = 0;
	for (i = 0; i < psEncapsulation->asUses.NumElements(); i++)
	{
		psUse = psEncapsulation->asUses.Get(i);
		if (!IsReplaced(psEncapsulation, psUse) || !cFiles.GetText(psUse->szFileName.Text()))
		{
			continue;
		}
		szText.Init();
		switch (psUse->iKind)
		{
		case IDE_FIELD_READ:
			szText.Append(psEncapsulation->szGetter)->Append("()");
			break;
		case IDE_FIELD_OBJECT_MEMBER:
			szText.Append(psEncapsulation->szGetter)->Append("()->");
			break;
		case IDE_FIELD_OBJECT_ADDRESS:
			szText.Append(psUse->szObject)->Append(psEncapsulation->szGetter)->Append("()");
			break;
		case IDE_FIELD_ASSIGNED:
			szText.Append(psEncapsulation->szSetter)->Append('(');
			cFiles.GetEdits(psUse->szFileName.Text())->Insert(psUse->uiValueEnd, ")");
			break;
		case IDE_FIELD_COMPOUND:
			szText.Append(psEncapsulation->szSetter)->Append('(')->Append(psUse->szObject)->Append(psEncapsulation->szGetter)->Append("() ")->Append(psUse->szOperator)->Append(' ');
			cFiles.GetEdits(psUse->szFileName.Text())->Insert(psUse->uiValueEnd, ")");
			break;
		default:
			szText.Append(psUse->szObject)->Append(psEncapsulation->szSetter)->Append('(')->Append(psUse->szObject)->Append(psEncapsulation->szGetter)->Append("() ")->Append(psUse->szOperator)->Append(" 1)");
			break;
		}
		cFiles.GetEdits(psUse->szFileName.Text())->Replace(psUse->uiStart, psUse->uiEnd, szText.Text());
		szText.Kill();
		uiChanged++;
	}

	//The getter and the setter: declared in the class and defined, a line each, at the bottom of the class's source
	//file, where the one-line functions are; in the class, as its short functions are written, if it has no source
	//file or is a template.  A getter of a value is const (one of an object gives a pointer the caller changes it
	//through, and isn't).
	szSource.Init();
	szScoped.Init();
	sClass.Init();
	if (pcMembers->ReadClass(psEncapsulation->szClassKey.Text(), &sClass) && !sClass.bTemplate)
	{
		pcMembers->FindSourceFile(&sClass, &szSource);
	}
	pcMembers->KillUnits();
	sClass.Kill();
	szFile = szSource.Empty() ? NULL : cFiles.GetText(szSource.Text());
	GetIDEScopedName(psEncapsulation->szClassKey.Text(), &szScoped);
	szText.Init();
	szLine.Init();
	bTemplates = true;
	if (psEncapsulation->bGetter)
	{
		szText.Append('\t')->Append(psEncapsulation->szType)->Append(psEncapsulation->bObject ? "*\t" : "\t")->Append(psEncapsulation->szGetter)->Append(psEncapsulation->bObject ? "(void)" : "(void) const");
		szLine.Append(psEncapsulation->szType)->Append(psEncapsulation->bObject ? "* " : " ")->Append(szScoped)->Append("::")->Append(psEncapsulation->szGetter)->Append(psEncapsulation->bObject ? "(void)" : "(void) const");
		szBody.Init(" { ");
		bTemplates = mpcContent->GetFileTemplates()->ApplyCode(IDE_TEMPLATE_GETTER_BODY, &szBody, "FIELD", psEncapsulation->szName.Text(), "IS_OBJECT", psEncapsulation->bObject ? "true" : "", NULL);
		szBody.Append(" }\n");
		szText.Append(szFile ? ";\n" : szBody.Text());
		szLine.Append(szBody);
		szBody.Kill();
	}
	if (psEncapsulation->bSetter)
	{
		szText.Append("\tvoid\t")->Append(psEncapsulation->szSetter)->Append('(')->Append(psEncapsulation->szType)->Append(' ')->Append(psEncapsulation->szParameter)->Append(")");
		szLine.Append("void ")->Append(szScoped)->Append("::")->Append(psEncapsulation->szSetter)->Append('(')->Append(psEncapsulation->szType)->Append(' ')->Append(psEncapsulation->szParameter)->Append(")");
		szBody.Init(" { ");
		bTemplates = bTemplates && mpcContent->GetFileTemplates()->ApplyCode(IDE_TEMPLATE_SETTER_BODY, &szBody, "FIELD", psEncapsulation->szName.Text(), "PARAMETER", psEncapsulation->szParameter.Text(), NULL);
		szBody.Append(" }\n");
		szText.Append(szFile ? ";\n" : szBody.Text());
		szLine.Append(szBody);
		szBody.Kill();
	}
	bResult = bTemplates && pcMembers->AddMember(psEncapsulation->szClassKey.Text(), IDE_ACCESS_PUBLIC, szText.Text(), &cFiles);
	if (bResult && szFile)
	{
		//Under the one-line functions the file ends with, or two lines under whatever else it ends with.
		for (uiEnd = strlen(szFile); (uiEnd > 0) && ((szFile[uiEnd - 1] == '\n') || (szFile[uiEnd - 1] == '\r') || (szFile[uiEnd - 1] == ' ') || (szFile[uiEnd - 1] == '\t')); uiEnd--)
		{
		}
		for (uiStart = uiEnd; (uiStart > 0) && (szFile[uiStart - 1] != '\n'); uiStart--)
		{
		}
		szBody.Init();
		szBody.AppendSubString(szFile, uiStart, uiEnd);
		szLine.Insert(0, (szBody.EndsWith("}") && szBody.Contains("{") && szBody.Contains("::") && (szFile[uiStart] != '\t') && (szFile[uiStart] != '{')) ? "\n" : "\n\n\n");
		szBody.Kill();
		cFiles.GetEdits(szSource.Text())->Replace(uiEnd, strlen(szFile), szLine.Text());
	}
	szLine.Kill();
	szText.Kill();
	szScoped.Kill();
	szSource.Kill();

	//The field, protected: in the class's protected part, or in one made at the class's start.
	szFile = cFiles.GetText(psEncapsulation->szFileName.Text());
	if (bResult && psEncapsulation->bProtect && psEncapsulation->bPublic && szFile)
	{
		//Its declaration's line, as it is written.
		for (uiStart = psEncapsulation->uiDeclaration; (uiStart > 0) && (szFile[uiStart - 1] != '\n'); uiStart--)
		{
		}
		for (uiEnd = psEncapsulation->uiDeclarationEnd; (szFile[uiEnd] != '\0') && (szFile[uiEnd] != '\n'); uiEnd++)
		{
		}
		szLine.Init();
		szLine.AppendSubString(szFile, uiStart, uiEnd);
		szLine.Append('\n');
		sClass.Init();
		iBoundary = -1;
		if (pcMembers->ReadClass(psEncapsulation->szClassKey.Text(), &sClass) && pcMembers->ScanClass(&sClass, -1, IDE_ACCESS_PROTECTED, NULL, &iBoundary, &iClose) && (iBoundary >= 0))
		{
			pcMembers->KillUnits();
			pcMembers->AddMember(psEncapsulation->szClassKey.Text(), IDE_ACCESS_PROTECTED, szLine.Text(), &cFiles);
		}
		else
		{
			//After the class's {.
			pcUnit = pcMembers->GetUnit(psEncapsulation->szFileName.Text());
			uiAt = 0;
			if (pcUnit && pcUnit->FindTokensIn(sClass.sRange.iLine, sClass.sRange.iColumn, sClass.sRange.iEndLine, sClass.sRange.iEndColumn, &iFirst, &iLast))
			{
				for (; (iFirst <= iLast) && !pcUnit->IsToken(iFirst, "{"); iFirst++)
				{
				}
				if (pcUnit->GetTokensRange(iFirst, iFirst, &sRange))
				{
					for (uiAt = FindIDETextOffset(szFile, sRange.iLine, sRange.iColumn); (szFile[uiAt] != '\0') && (szFile[uiAt] != '\n'); uiAt++)
					{
					}
					uiAt = (szFile[uiAt] == '\n') ? uiAt + 1 : uiAt;
				}
			}
			pcMembers->KillUnits();
			if (uiAt > 0)
			{
				szLine.Insert(0, "protected:\n");
				szLine.Append("\n");
				cFiles.GetEdits(psEncapsulation->szFileName.Text())->Insert(uiAt, szLine.Text());
			}
		}
		sClass.Kill();
		szLine.Kill();
		uiStart = psEncapsulation->uiDeclaration;
		uiEnd = psEncapsulation->uiDeclarationEnd;
		ExtendIDEDeletion(szFile, &uiStart, &uiEnd, true);
		cFiles.GetEdits(psEncapsulation->szFileName.Text())->Replace(uiStart, uiEnd, "");
	}
	if (bResult)
	{
		cFiles.Apply();
		szMessage.Init("Encapsulated ");
		GetIDEScopedName(psEncapsulation->szKey.Text(), &szMessage);
		szMessage.Append(": ");
		szMessage.Append(psEncapsulation->bGetter ? psEncapsulation->szGetter.Text() : "");
		szMessage.Append((psEncapsulation->bGetter && psEncapsulation->bSetter) ? " and " : "");
		szMessage.Append(psEncapsulation->bSetter ? psEncapsulation->szSetter.Text() : "");
		szMessage.Append(", ");
		szMessage.Append((uint32)uiChanged);
		szMessage.Append((uiChanged == 1) ? " place changed." : " places changed.");
		if (pszStatus)
		{
			pszStatus->Set(szMessage);
		}
		else
		{
			mpcContent->ShowStatus(szMessage.Text());
		}
		szMessage.Kill();
	}
	else if (!bTemplates)
	{
		szMessage.Init("Encapsulate Field: ");
		szMessage.Append(mpcContent->GetFileTemplates()->GetError())->Append('.');
		if (pszStatus)
		{
			pszStatus->Set(szMessage);
		}
		else
		{
			mpcContent->ShowStatus(szMessage.Text());
		}
		szMessage.Kill();
	}
	cFiles.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What was found, for the checks.
//
//////////////////////////////////////////////////////////////////////////
char* CIDEEncapsulateField::PrintEncapsulation(SIDEEncapsulation* psEncapsulation, CChars* pszDest)
{
	static const char*	aszKinds[] = { "read", "assigned", "changed with an operator", "stepped", "a member of it named", "its address taken", "can't be changed" };
	SIDEFieldUse*		psUse;
	size				i;

	pszDest->Append("Encapsulate Field: ")->Append(psEncapsulation->szKey);
	if (!psEncapsulation->bPossible)
	{
		pszDest->Append(" can't be: ")->Append(psEncapsulation->szWhyNot)->Append('\n');
		return pszDest->Text();
	}
	pszDest->Append(", [")->Append(psEncapsulation->szType)->Append("], ")->Append(psEncapsulation->bPublic ? "public, " : "not public, ");
	pszDest->Append(psEncapsulation->szGetter)->Append(" and ")->Append(psEncapsulation->szSetter)->Append('(')->Append(psEncapsulation->szParameter)->Append(")\n");
	for (i = 0; i < psEncapsulation->asUses.NumElements(); i++)
	{
		psUse = psEncapsulation->asUses.Get(i);
		pszDest->Append("  ")->Append(GetIDEFileNamePart(psUse->szFileName.Text()))->Append(" line ");
		pszDest->Append((int32)(psUse->iLine + 1));
		pszDest->Append(": ")->Append(aszKinds[psUse->iKind])->Append(psUse->bInside ? ", in the class\n" : "\n");
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
// IntelliJ's Encapsulate Fields for the field at the caret: its dialog
// (which functions, their names, whether the class's own functions use
// them, whether the field becomes protected), Problems Detected if
// anything stands in the way, then the change.  False if nothing changed.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEEncapsulateField::ShowEncapsulate(void)
{
	SIDEEncapsulation	sEncapsulation;
	CArrayChars			aszConflicts;
	Ptr<CCheckBox>		pGetter;
	Ptr<CCheckBox>		pSetter;
	Ptr<CCheckBox>		pInside;
	Ptr<CCheckBox>		pProtect;
	Ptr<CTextField>		pGetterName;
	Ptr<CTextField>		pSetterName;
	CChars				szMessage;
	bool				bResult;

	sEncapsulation.Init();
	if (!FindEncapsulation(&sEncapsulation))
	{
		mpcContent->ShowStatus("There is no field at the caret to encapsulate.");
		sEncapsulation.Kill();
		return false;
	}
	szMessage.Init();
	GetIDEScopedName(sEncapsulation.szKey.Text(), &szMessage);
	if (!sEncapsulation.bPossible)
	{
		szMessage.Insert(0, "Can't encapsulate ");
		szMessage.Append(": ");
		szMessage.Append(sEncapsulation.szWhyNot);
		szMessage.Append(".");
		mpcContent->ShowStatus(szMessage.Text());
		szMessage.Kill();
		sEncapsulation.Kill();
		return false;
	}
	szMessage.Insert(0, "Encapsulate ");
	mpDialog = OMalloc<CDialog>(mpcContent->GetWindow(), mpcContent->GetDialogLook(), szMessage.Text());
	szMessage.Kill();
	pGetter = mpDialog->AddCheckBox("Getter", sEncapsulation.bGetter);
	pGetterName = mpDialog->AddTextField("Its name:", sEncapsulation.szGetter.Text());
	pSetter = mpDialog->AddCheckBox("Setter", sEncapsulation.bSetter);
	pSetterName = mpDialog->AddTextField("Its name:", sEncapsulation.szSetter.Text());
	pInside = mpDialog->AddCheckBox("The class's own functions use them too", sEncapsulation.bInside);
	pProtect = mpDialog->AddCheckBox("Make the field protected", sEncapsulation.bProtect);
	mpDialog->SetOKCaption("Refactor");
	mpcContent->GetScript()->WatchDialog(mpDialog);
	bResult = mpDialog->Show(520);
	if (bResult)
	{
		sEncapsulation.bGetter = pGetter->IsTicked();
		sEncapsulation.bSetter = pSetter->IsTicked();
		sEncapsulation.bInside = pInside->IsTicked();
		sEncapsulation.bProtect = pProtect->IsTicked();
		sEncapsulation.szGetter.Set(pGetterName->GetText());
		sEncapsulation.szSetter.Set(pSetterName->GetText());
	}
	pGetter = NULL;
	pSetter = NULL;
	pInside = NULL;
	pProtect = NULL;
	pGetterName = NULL;
	pSetterName = NULL;
	mpDialog = NULL;
	if (bResult && GetProblem(&sEncapsulation))
	{
		mpcContent->ShowStatus(GetProblem(&sEncapsulation));
		bResult = false;
	}
	if (bResult)
	{
		aszConflicts.Init();
		FindConflicts(&sEncapsulation, &aszConflicts);
		bResult = (aszConflicts.NumElements() == 0) || mpcContent->GetChangeSignature()->ShowConflicts(&aszConflicts);
		aszConflicts.Kill();
		bResult = bResult && Encapsulate(&sEncapsulation);
	}
	sEncapsulation.Kill();
	return bResult;
}
