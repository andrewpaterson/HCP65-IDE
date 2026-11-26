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
#ifndef __IDE_REFACTOR_UNIT_H__
#define __IDE_REFACTOR_UNIT_H__
#include "BaseLib/Chars.h"
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPSyntaxes.h"
#include "CppCompilerLib/CPPSemanticPass.h"
#include "CppCompilerLib/CPPHighlighter.h"
#include "BaseLib/TextNavigation.h"
#include "WindowLib/TextArea.h"
#include "IDECodeModel.h"


//One change to a text: what is from uiStart to uiEnd (nothing, for an insertion) becomes szText.
struct SIDETextEdit
{
	size	uiStart;
	size	uiEnd;
	CChars	szText;
};


typedef CArrayTemplate<SIDETextEdit>	CArrayIDETextEdits;


//The changes a refactoring makes to one file's text, given in any order by where they are in the text as it is now,
//and made as one edit of its editor (Apply), so one undo takes all of them back.  They mustn't overlap; two
//insertions at one place go in the order they were added.
class CIDETextEdits
{
protected:
	CArrayIDETextEdits	masEdits;

public:
	void	Init(void);
	void	Kill(void);

	void	Replace(size uiStart, size uiEnd, const char* szText);
	void	Insert(size uiAt, const char* szText);
	size	NumEdits(void) const;
	bool	Apply(Ptr<CTextArea> pEditor);
	void	ApplyTo(const char* szText, CChars* pszDest);
	size	AdjustPosition(size uiPosition);
};


//A file as a refactoring sees it (Refactorings.txt, F1): parsed as its project builds it, from the text its editor
//has now (a header in a unit that includes it, with what comes before it there), with its syntax tree to find what is
//written at a place and what a node was written from, and, if asked, a kept check of it for the types of its
//expressions and what changes its variables (CCPPSemanticPass).
//
//Places are lines and columns from 0, as the index's; a range's end is exclusive.  A node a macro made, or that is
//another file's, has no range here: what a refactoring can't see the text of, it can't change.
class CIDERefactorUnit
{
protected:
	CIDECodeModel*		mpcModel;
	CParsedUnit			mcUnit;
	CCPPSemanticPass	mcPass;
	CCPPHighlighter		mcHighlighter;	//For its source ranges.
	CSourceFile*		mpcFile;		//The file, in the unit parsed.
	CChars				mszFileName;
	CChars				mszErrors;
	bool				mbParsed;
	bool				mbChecked;

public:
	void			Init(void);
	void			Kill(void);

	bool			Parse(CIDECodeModel* pcModel, CIDEFileTexts* pcTexts, const char* szFileName, bool bCheck, ECPPTarget eTarget = CPPT_Host);
	void			SetRecordedEnum(const char* szQualifiedName) { mcPass.SetRecordedEnum(szQualifiedName); }  //Before Parse: see CCPPSemanticPass.
	bool			IsParsed(void) const;
	bool			IsChecked(void) const;
	bool			IsExceptions(void) const;
	size			NumErrors(void);
	char*			GetFileName(void);
	CCPPTokeniser*	GetTokeniser(void);
	CCPPSyntaxTree*	GetTree(void);
	CCPPSemanticPass*	GetPass(void);

	CSourceFile*	GetFile(void);

	int32			FindTokenAt(int iLine, int iColumn);
	bool			FindTokensIn(int iLine, int iColumn, int iEndLine, int iEndColumn, int32* piFirstToken, int32* piLastToken);
	bool			IsExpression(CCPPSyntax* pcNode);
	void			AppendTokens(int32 iFirstToken, int32 iLastToken, CChars* pszDest);
	bool			AppendDeclaredType(CCPPSyntax* pcExpression, CChars* pszDest);
	bool			AppendTypeKey(CCPPSyntax* pcExpression, CChars* pszDest, int32* piLevels = NULL);
	bool			FindDeclaredTypeName(int iLine, int iColumn, SCPPSyntaxRange* psDest);
	bool			AppendFunctionLabel(int iLine, int iColumn, CChars* pszDest);
	CCPPSyntax*		FindNodeOfTokens(int32 iFirstToken, int32 iLastToken);
	CCPPSyntax*		FindNodeAt(int iLine, int iColumn);
	CCPPSyntax*		FindAncestor(CCPPSyntax* pcNode, ECPPSyntax eKind);
	bool			GetRange(CCPPSyntax* pcNode, SCPPSyntaxRange* psDest);
	bool			GetTokensRange(int32 iFirstToken, int32 iLastToken, SCPPSyntaxRange* psDest);
	bool			IsToken(int32 iToken, const char* szText);
	bool			IsTokenFrom(int32 iToken, int iLine, int iColumn, int iEndLine, int iEndColumn, int32* piLine, int32* piColumn, int32* piBy);
	bool			FindExpansionTokens(int iLine, int iColumn, int iEndLine, int iEndColumn, int32* piFirstToken, int32* piLastToken);
	bool			FindSpelledTokens(int iLine, int iColumn, int iEndLine, int iEndColumn, int32 iFrom, int32 iTo, size uiCopies, int32* piFirstToken, int32* piLastToken);

protected:
	bool			AppendDeclaredTypeOf(CCPPSyntaxDeclSpecifiers* pcSpecifiers, CCPPSyntaxDeclarator* pcDeclarator, CChars* pszDest);
};


//A file's text, and its unit's, as a refactoring would leave them, for checking the file before they are changed
//(CountIDETrialErrors).  A text that is NULL is the file's as it is.
class CIDETrialTexts : public CIDEFileTexts
{
public:
	CIDEFileTexts*	mpcTexts;
	const char*		mszFileName;
	char*			mszText;
	const char*		mszUnit;
	char*			mszUnitText;

	bool	GetFileText(const char* szFileName, CTextFile* pcFile, const char** pszText, size* puiLength) override;
};


size	CountIDETrialErrors(CIDECodeModel* pcModel, CIDEFileTexts* pcTexts, const char* szFileName, char* szText, const char* szUnit, char* szUnitText);


//Finds whether an expression does something: a call, new, delete, throw, ++, --, an assignment.
class CIDEEffectFinder : public CCPPSyntaxChildVisitor
{
public:
	bool	mbEffects;

	void	Look(CCPPSyntax* pcNode)
	{
		ECppTokens	eOperator;

		switch (pcNode->GetKind())
		{
		case CPPS_CallExpression:
		case CPPS_NewExpression:
		case CPPS_DeleteExpression:
		case CPPS_ThrowExpression:
			mbEffects = true;
			break;
		case CPPS_UnaryExpression:
			eOperator = ((CCPPSyntaxUnaryExpression*)pcNode)->meOperator;
			mbEffects |= (eOperator == CT_Increment) || (eOperator == CT_Decrement);
			break;
		case CPPS_BinaryExpression:
			eOperator = ((CCPPSyntaxBinaryExpression*)pcNode)->meOperator;
			mbEffects |= (eOperator == CT_Assignment) || (eOperator == CT_AssignmentAdd) || (eOperator == CT_AssignmentSubtract) || (eOperator == CT_AssignmentMultiply) ||
						 (eOperator == CT_AssignmentDivide) || (eOperator == CT_AssignmentModulus) || (eOperator == CT_AssignmentBitwiseAnd) || (eOperator == CT_AssignmentBitwiseOr) ||
						 (eOperator == CT_AssignmentBitwiseXor) || (eOperator == CT_AssignmentLeftShift) || (eOperator == CT_AssignmentRightShift);
			break;
		default:
			break;
		}
		if (!mbEffects)
		{
			pcNode->VisitFields(this);
		}
	}

	void	Visit(CCPPSyntax* pcChild) override	{ Look(pcChild); }
};


//Whether what is run may throw: a call, a new, or a throw (a call to a function that throws nothing is still counted:
//the syntax doesn't say what a call calls).
class CIDEThrowFinder : public CCPPSyntaxChildVisitor
{
public:
	bool	mbMayThrow;

	void	Look(CCPPSyntax* pcNode)
	{
		ECPPSyntax	eKind;

		eKind = pcNode->GetKind();
		mbMayThrow = mbMayThrow || (eKind == CPPS_CallExpression) || (eKind == CPPS_NewExpression) || (eKind == CPPS_ThrowExpression);
		if (!mbMayThrow)
		{
			pcNode->VisitFields(this);
		}
	}

	void	Visit(CCPPSyntax* pcChild) override	{ Look(pcChild); }
};


//How a run of statements leaves: by return, or some way a function made of them couldn't (a break or continue of a
//loop it isn't all of, a goto, a label something may go to).  A throw leaves a function made of them as it leaves
//them, so it is none of these; a last statement that throws ends the run as a return would (EndsLeaving).
class CIDEExitFinder : public CCPPSyntaxChildVisitor
{
public:
	static bool	EndsLeaving(CCPPSyntax* pcStatement)
	{
		CCPPSyntaxExpression*	pcExpression;

		if (pcStatement->GetKind() == CPPS_ReturnStatement)
		{
			return true;
		}
		if (pcStatement->GetKind() != CPPS_ExpressionStatement)
		{
			return false;
		}
		pcExpression = ((CCPPSyntaxExpressionStatement*)pcStatement)->mpcExpression;
		return pcExpression && (pcExpression->GetKind() == CPPS_ThrowExpression);
	}

	size	muiReturns;
	bool	mbLeaves;
	int		miLoops;
	int		miSwitches;

	void	Look(CCPPSyntax* pcNode)
	{
		ECPPSyntax	eKind;
		bool		bLoop;
		bool		bSwitch;

		eKind = pcNode->GetKind();
		bLoop = (eKind == CPPS_WhileStatement) || (eKind == CPPS_ForStatement) || (eKind == CPPS_DoStatement) || (eKind == CPPS_RangeForStatement);
		bSwitch = (eKind == CPPS_SwitchStatement);
		if (eKind == CPPS_ReturnStatement)
		{
			muiReturns++;
		}
		else if ((eKind == CPPS_GotoStatement) || (eKind == CPPS_LabelStatement))
		{
			mbLeaves = true;
		}
		else if (eKind == CPPS_BreakStatement)
		{
			mbLeaves = mbLeaves || ((miLoops == 0) && (miSwitches == 0));
		}
		else if (eKind == CPPS_ContinueStatement)
		{
			mbLeaves = mbLeaves || (miLoops == 0);
		}
		miLoops += bLoop ? 1 : 0;
		miSwitches += bSwitch ? 1 : 0;
		pcNode->VisitFields(this);
		miLoops -= bLoop ? 1 : 0;
		miSwitches -= bSwitch ? 1 : 0;
	}

	void	Visit(CCPPSyntax* pcChild) override	{ Look(pcChild); }
};


#endif // __IDE_REFACTOR_UNIT_H__

