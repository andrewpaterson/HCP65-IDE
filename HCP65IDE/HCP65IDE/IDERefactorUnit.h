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
	size	NumEdits(void);
	bool	Apply(Ptr<CTextArea> pEditor);
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
	CIDEParsedUnit		mcUnit;
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

	bool			Parse(CIDECodeModel* pcModel, CIDEFileTexts* pcTexts, const char* szFileName, bool bCheck);
	bool			IsParsed(void) { return mbParsed; }
	bool			IsChecked(void) { return mbChecked; }
	char*			GetFileName(void) { return mszFileName.Text(); }
	CCPPTokeniser*	GetTokeniser(void) { return &mcUnit.mcTokeniser; }
	CCPPSyntaxTree*	GetTree(void) { return &mcUnit.mcTree; }
	CCPPSemanticPass*	GetPass(void) { return &mcPass; }

	CSourceFile*	GetFile(void) { return mpcFile; }

	int32			FindTokenAt(int iLine, int iColumn);
	bool			FindTokensIn(int iLine, int iColumn, int iEndLine, int iEndColumn, int32* piFirstToken, int32* piLastToken);
	bool			IsExpression(CCPPSyntax* pcNode);
	void			AppendTokens(int32 iFirstToken, int32 iLastToken, CChars* pszDest);
	bool			AppendDeclaredType(CCPPSyntax* pcExpression, CChars* pszDest);
	CCPPSyntax*		FindNodeOfTokens(int32 iFirstToken, int32 iLastToken);
	CCPPSyntax*		FindNodeAt(int iLine, int iColumn);
	CCPPSyntax*		FindAncestor(CCPPSyntax* pcNode, ECPPSyntax eKind);
	bool			GetRange(CCPPSyntax* pcNode, SCPPSyntaxRange* psDest);
	bool			GetTokensRange(int32 iFirstToken, int32 iLastToken, SCPPSyntaxRange* psDest);
	bool			IsToken(int32 iToken, const char* szText);

protected:
	bool			AppendDeclaredTypeOf(CCPPSyntaxDeclSpecifiers* pcSpecifiers, CCPPSyntaxDeclarator* pcDeclarator, CChars* pszDest);
};


#endif // __IDE_REFACTOR_UNIT_H__

