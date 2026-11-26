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
#include <string.h>
#include "CppCompilerLib/SourceFile.h"
#include "CppCompilerLib/TranslationUnit.h"
#include "CppCompilerLib/CPPScope.h"
#include "CppCompilerLib/CPPSymbol.h"
#include "CppCompilerLib/CPPSyntaxNamedTypeSpecifier.h"
#include "CppCompilerLib/CPPSyntaxElaboratedTypeSpecifier.h"
#include "CppCompilerLib/CPPSyntaxParameter.h"
#include "CppCompilerLib/CPPSyntaxSimpleDeclaration.h"
#include "CppCompilerLib/CPPSyntaxFunctionDefinition.h"
#include "CppCompilerLib/CPPSyntaxDeclSpecifiers.h"
#include "CppCompilerLib/CPPSyntaxDeclarator.h"
#include "CppCompilerLib/CPPSyntaxName.h"
#include "IDERename.h"
#include "CppCompilerLib/CPP65Type.h"
#include "IDERefactorUnit.h"
#include "IDEOutline.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETextEdits::Init(void)
{
	masEdits.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETextEdits::Kill(void)
{
	size	i;

	for (i = 0; i < masEdits.NumElements(); i++)
	{
		masEdits.Get(i)->szText.Kill();
	}
	masEdits.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Kept in the order of where they start, one added later after those
// that start at the same place.
//
//////////////////////////////////////////////////////////////////////////
void CIDETextEdits::Replace(size uiStart, size uiEnd, const char* szText)
{
	SIDETextEdit	sEdit;
	size			i;

	for (i = 0; (i < masEdits.NumElements()) && (masEdits.Get(i)->uiStart <= uiStart); i++)
	{
	}
	sEdit.uiStart = uiStart;
	sEdit.uiEnd = uiEnd;
	sEdit.szText.Init(szText);
	masEdits.InsertAt(&sEdit, i);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDETextEdits::Insert(size uiAt, const char* szText)
{
	Replace(uiAt, uiAt, szText);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CIDETextEdits::NumEdits(void) const
{
	return masEdits.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// The edits made in pEditor as one: the text from the first's start to
// the last's end is replaced by what it is with them all made.  False if
// there are none or the editor wouldn't take it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDETextEdits::Apply(Ptr<CTextArea> pEditor)
{
	CChars			szNew;
	STextRange		sRange;
	SIDETextEdit*	psEdit;
	const char*		szText;
	size			uiAt;
	size			i;
	bool			bResult;

	if (masEdits.NumElements() == 0)
	{
		return false;
	}
	szText = pEditor->GetText();
	sRange.uiStart = masEdits.Get(0)->uiStart;
	sRange.uiEnd = sRange.uiStart;
	szNew.Init();
	uiAt = sRange.uiStart;
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		psEdit = masEdits.Get(i);
		if (psEdit->uiStart > uiAt)
		{
			szNew.AppendSubString(szText, uiAt, psEdit->uiStart);
		}
		szNew.Append(psEdit->szText);
		if (psEdit->uiEnd > uiAt)
		{
			uiAt = psEdit->uiEnd;
		}
		if (uiAt > sRange.uiEnd)
		{
			sRange.uiEnd = uiAt;
		}
	}
	bResult = pEditor->ReplaceRanges(&sRange, 1, szNew.Text());
	szNew.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A text with the edits made, added to pszDest: what a refactoring would
// leave, for checking it before the editor is changed.
//
//////////////////////////////////////////////////////////////////////////
void CIDETextEdits::ApplyTo(const char* szText, CChars* pszDest)
{
	SIDETextEdit*	psEdit;
	size			uiAt;
	size			i;

	uiAt = 0;
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		psEdit = masEdits.Get(i);
		if (psEdit->uiStart > uiAt)
		{
			pszDest->AppendSubString(szText, uiAt, psEdit->uiStart);
		}
		pszDest->Append(psEdit->szText);
		if (psEdit->uiEnd > uiAt)
		{
			uiAt = psEdit->uiEnd;
		}
	}
	pszDest->Append(&szText[uiAt]);
}


//////////////////////////////////////////////////////////////////////////
//
// Where a position of the text as it was is once the edits are made: a
// position in what an edit replaces is the start of what replaces it.
//
//////////////////////////////////////////////////////////////////////////
size CIDETextEdits::AdjustPosition(size uiPosition)
{
	SIDETextEdit*	psEdit;
	size			uiAdjusted;
	size			i;

	uiAdjusted = uiPosition;
	for (i = 0; i < masEdits.NumElements(); i++)
	{
		psEdit = masEdits.Get(i);
		if (psEdit->uiEnd <= uiPosition)
		{
			uiAdjusted = uiAdjusted + psEdit->szText.Length() - (psEdit->uiEnd - psEdit->uiStart);
		}
		else if (psEdit->uiStart < uiPosition)
		{
			uiAdjusted = uiAdjusted - (uiPosition - psEdit->uiStart);
		}
	}
	return uiAdjusted;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDERefactorUnit::Init(void)
{
	mpcModel = NULL;
	mcUnit.Init();
	mcPass.Init();
	mcHighlighter.Init();
	mpcFile = NULL;
	mszFileName.Init();
	mszErrors.Init();
	mbParsed = false;
	mbChecked = false;
}


//////////////////////////////////////////////////////////////////////////
//
// The check is killed before the tree its types are of.
//
//////////////////////////////////////////////////////////////////////////
void CIDERefactorUnit::Kill(void)
{
	mszErrors.Kill();
	mszFileName.Kill();
	mcHighlighter.Kill();
	mcPass.Kill();
	mcUnit.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName (a full path, one of the workspace's projects' files)
// parsed from the text pcTexts has for it: its editor's if it is open.
// A header is parsed in the unit that included it when it was indexed,
// that unit's text from pcTexts too.  With bCheck its expressions are
// typed, in a check that is kept.  False if it isn't in a project, its
// text can't be had, or nothing of it is in the unit (the unit no longer
// includes it).  The check is for eTarget: the host's sizes, or a
// processor's for the labels its functions have there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::Parse(CIDECodeModel* pcModel, CIDEFileTexts* pcTexts, const char* szFileName, bool bCheck, ECPPTarget eTarget)
{
	CTextFile	cFile;
	CTextFile	cUnitFile;
	const char*	szText;
	const char*	szUnitText;
	size		uiLength;
	size		uiProject;
	size		uiUnitProject;
	CChars		szUnit;
	EIDEFileType	eType;
	bool		bResult;

	mpcModel = pcModel;
	mszFileName.Set(szFileName);
	uiProject = pcModel->FindProjectOfFile(szFileName);
	if (uiProject == MAX_SIZE)
	{
		return false;
	}

	//Nothing else is preprocessing while the files' texts are given and read.
	pcModel->WaitForIndexing();
	cFile.Init();
	cUnitFile.Init();
	szUnit.Init();
	bResult = pcTexts->GetFileText(szFileName, &cFile, &szText, &uiLength);
	eType = CIDEIcons::GetFileType(szFileName);
	if (bResult && (eType == IFT_Header) && pcModel->GetIncluder(szFileName, &uiUnitProject, &szUnit) && (StringInsensitiveCompare(szUnit.Text(), szFileName) != 0))
	{
		pcModel->SetHeaderText(szFileName, szText);
		bResult = pcTexts->GetFileText(szUnit.Text(), &cUnitFile, &szUnitText, &uiLength) && pcModel->ParseUnit(uiUnitProject, szUnit.Text(), szUnitText, &mcUnit, &mszErrors);
		mpcFile = bResult ? pcModel->FindFileInUnit(&mcUnit, szFileName) : NULL;
	}
	else if (bResult)
	{
		bResult = pcModel->ParseUnit(uiProject, szFileName, szText, &mcUnit, &mszErrors);
		mpcFile = bResult ? mcUnit.mpcUnit : NULL;
	}
	szUnit.Kill();
	cUnitFile.Kill();
	cFile.Kill();

	mbParsed = bResult && (mpcFile != NULL);
	if (mbParsed && bCheck)
	{
		mcPass.SetOptions(&mcUnit.mcOptions);
		mcPass.Check(&mcUnit.mcTree, eTarget, true);
		mbChecked = true;
	}
	return mbParsed;
}


//////////////////////////////////////////////////////////////////////////
//
// The errors of the unit the file was parsed in: its preprocessing's (a
// line each), its parse's and, if it was checked, its check's.  A
// refactoring that checks what it would leave compares this with what
// there was.
//
//////////////////////////////////////////////////////////////////////////
size CIDERefactorUnit::NumErrors(void)
{
	size	uiErrors;
	size	i;

	uiErrors = mcUnit.mcTree.NumErrors() + (mbChecked ? mcPass.GetErrors()->NumErrors() : 0);
	for (i = 0; i < mszErrors.Length(); i++)
	{
		if ((mszErrors.Text()[i] == '\n') || ((i + 1 == mszErrors.Length()) && (mszErrors.Text()[i] != '\n')))
		{
			uiErrors++;
		}
	}
	return uiErrors;
}


//////////////////////////////////////////////////////////////////////////
//
// The token written at a place in the file: the one the place is in, or
// that ends there if none starts there (a caret after a name is on it).
// -1 if there is none: the place is in a comment, in white space, in code
// the configuration leaves out, or in what a macro's use was replaced by.
//
//////////////////////////////////////////////////////////////////////////
int32 CIDERefactorUnit::FindTokenAt(int iLine, int iColumn)
{
	size				i;
	SPreprocessedToken*	psSource;
	int32				iEnding;

	iEnding = -1;
	for (i = 0; mbParsed && (i < mcUnit.mcTokeniser.NumTokens()); i++)
	{
		psSource = mcUnit.mcTokeniser.GetSource(mcUnit.mcTokeniser.Get(i));
		if ((psSource->pcFile != mpcFile) || (psSource->iExpansion != -1) || (psSource->iLine != iLine))
		{
			continue;
		}
		if ((iColumn >= psSource->iColumn) && (iColumn < psSource->iColumn + (int32)psSource->uiLength))
		{
			return (int32)i;
		}
		if (iColumn == psSource->iColumn + (int32)psSource->uiLength)
		{
			iEnding = (int32)i;
		}
	}
	return iEnding;
}


//Finds the innermost node that has a run of tokens (CIDERefactorUnit::FindNodeOfTokens).
class CIDERefactorNodeFinder : public CCPPSyntaxChildVisitor
{
public:
	int32		miFirstToken;
	int32		miLastToken;
	CCPPSyntax*	mpcFound;

	void	Visit(CCPPSyntax* pcChild) override
	{
		if ((pcChild->miFirstToken >= 0) && (pcChild->miFirstToken <= miFirstToken) && (pcChild->miLastToken >= miLastToken))
		{
			mpcFound = pcChild;
			pcChild->VisitFields(this);
		}
	}
};


//////////////////////////////////////////////////////////////////////////
//
// The innermost node of the tree that all of the tokens from iFirstToken
// to iLastToken are in, NULL if none is.
//
//////////////////////////////////////////////////////////////////////////
CCPPSyntax* CIDERefactorUnit::FindNodeOfTokens(int32 iFirstToken, int32 iLastToken)
{
	CIDERefactorNodeFinder	cFinder;

	if (!mbParsed || (iFirstToken < 0) || (iLastToken < iFirstToken) || !mcUnit.mcTree.GetRoot())
	{
		return NULL;
	}
	cFinder.miFirstToken = iFirstToken;
	cFinder.miLastToken = iLastToken;
	cFinder.mpcFound = NULL;
	mcUnit.mcTree.GetRoot()->VisitFields(&cFinder);
	return cFinder.mpcFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The innermost node the token at a place is in (FindTokenAt).
//
//////////////////////////////////////////////////////////////////////////
CCPPSyntax* CIDERefactorUnit::FindNodeAt(int iLine, int iColumn)
{
	int32	iToken;

	iToken = FindTokenAt(iLine, iColumn);
	return FindNodeOfTokens(iToken, iToken);
}


//////////////////////////////////////////////////////////////////////////
//
// pcNode, or the nearest node it is in, of eKind; NULL if none is.
//
//////////////////////////////////////////////////////////////////////////
CCPPSyntax* CIDERefactorUnit::FindAncestor(CCPPSyntax* pcNode, ECPPSyntax eKind)
{
	while (pcNode && (pcNode->GetKind() != eKind))
	{
		pcNode = pcNode->mpcParent;
	}
	return pcNode;
}


//////////////////////////////////////////////////////////////////////////
//
// What a node, or a run of tokens, was written from in the file: false
// if its first or last token isn't written there.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::GetRange(CCPPSyntax* pcNode, SCPPSyntaxRange* psDest)
{
	return mbParsed && pcNode && mcHighlighter.GetSyntaxRange(mpcFile, &mcUnit.mcTokeniser, pcNode, psDest);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::GetTokensRange(int32 iFirstToken, int32 iLastToken, SCPPSyntaxRange* psDest)
{
	return mbParsed && mcHighlighter.GetTokensRange(mpcFile, &mcUnit.mcTokeniser, iFirstToken, iLastToken, psDest);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether token iToken is szText (a punctuator or a keyword, say).
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::IsToken(int32 iToken, const char* szText)
{
	SPreprocessedToken*	psSource;
	size				uiLength;

	if (!mbParsed || (iToken < 0) || ((size)iToken >= mcUnit.mcTokeniser.NumTokens()))
	{
		return false;
	}
	psSource = mcUnit.mcTokeniser.GetSource(mcUnit.mcTokeniser.Get((size)iToken));
	uiLength = strlen(szText);
	return (psSource->uiLength == uiLength) && (memcmp(psSource->szText, szText, uiLength) == 0);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a token is from a range of the file: written there (a macro's
// argument is, wherever the macro's body put it), or made by a macro
// whose name is written there.  Where that is in *piLine and *piColumn,
// and the macro's use in *piBy (-1 if the token is written there itself).
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::IsTokenFrom(int32 iToken, int iLine, int iColumn, int iEndLine, int iEndColumn, int32* piLine, int32* piColumn, int32* piBy)
{
	CPreprocessedTokens*	pcTokens;
	SMacroExpansion*		psExpansion;
	SPreprocessedToken*		psSource;
	int32					iExpansion;

	psSource = mcUnit.mcTokeniser.GetSource(mcUnit.mcTokeniser.Get((size)iToken));
	if (psSource->iExpansion == -1)
	{
		*piLine = psSource->iLine;
		*piColumn = psSource->iColumn;
		*piBy = -1;
		return (psSource->pcFile == mpcFile) && ((psSource->iLine > iLine) || ((psSource->iLine == iLine) && (psSource->iColumn >= iColumn))) &&
			   ((psSource->iLine < iEndLine) || ((psSource->iLine == iEndLine) && (psSource->iColumn < iEndColumn)));
	}
	pcTokens = mcUnit.mpcUnit->GetTokens();
	for (iExpansion = psSource->iExpansion; iExpansion != -1; iExpansion = psExpansion->iParent)
	{
		psExpansion = pcTokens->GetExpansion((size)iExpansion);
		if ((psExpansion->pcFile == mpcFile) && ((psExpansion->iLine > iLine) || ((psExpansion->iLine == iLine) && (psExpansion->iColumn >= iColumn))) &&
			((psExpansion->iLine < iEndLine) || ((psExpansion->iLine == iEndLine) && (psExpansion->iColumn < iEndColumn))))
		{
			*piLine = psExpansion->iLine;
			*piColumn = psExpansion->iColumn;
			*piBy = iExpansion;
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The tokens a macro's use is in the unit: the use written in a range of
// the file (from its name to the end of its arguments), every token it
// was replaced by, its arguments' among them.  The first run of them: a
// use that is another macro's argument is there as often as that macro's
// body has the argument.  False if no macro is used there or it made
// nothing.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::FindExpansionTokens(int iLine, int iColumn, int iEndLine, int iEndColumn, int32* piFirstToken, int32* piLastToken)
{
	CPreprocessedTokens*	pcTokens;
	SMacroExpansion*		psExpansion;
	size					i;
	int32					iAtLine;
	int32					iAtColumn;
	int32					iBy;
	bool					bFound;

	*piFirstToken = -1;
	*piLastToken = -1;
	if (!mbParsed)
	{
		return false;
	}
	pcTokens = mcUnit.mpcUnit->GetTokens();
	bFound = false;
	for (i = 0; (i < pcTokens->NumExpansions()) && !bFound; i++)
	{
		psExpansion = pcTokens->GetExpansion(i);
		bFound = (psExpansion->pcFile == mpcFile) && (psExpansion->iLine == iLine) && (psExpansion->iColumn == iColumn);
	}
	for (i = 0; bFound && (i < mcUnit.mcTokeniser.NumTokens()); i++)
	{
		if (IsTokenFrom((int32)i, iLine, iColumn, iEndLine, iEndColumn, &iAtLine, &iAtColumn, &iBy))
		{
			if (*piFirstToken < 0)
			{
				*piFirstToken = (int32)i;
			}
			*piLastToken = (int32)i;
		}
		else if (*piFirstToken >= 0)
		{
			break;
		}
	}
	return *piFirstToken >= 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The tokens, from iFrom to iTo, that are from a range of the file
// (IsTokenFrom): an argument of a macro's use, the first of the uiCopies
// times the macro's body has it.  False if there are none, or they aren't
// one run.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::FindSpelledTokens(int iLine, int iColumn, int iEndLine, int iEndColumn, int32 iFrom, int32 iTo, size uiCopies, int32* piFirstToken, int32* piLastToken)
{
	size	uiTokens;
	size	uiTaken;
	int32	i;
	int32	iAtLine;
	int32	iAtColumn;
	int32	iBy;

	*piFirstToken = -1;
	*piLastToken = -1;
	uiTokens = 0;
	for (i = iFrom; mbParsed && (i <= iTo) && ((size)i < mcUnit.mcTokeniser.NumTokens()); i++)
	{
		uiTokens += IsTokenFrom(i, iLine, iColumn, iEndLine, iEndColumn, &iAtLine, &iAtColumn, &iBy) ? 1 : 0;
	}
	if ((uiTokens == 0) || (uiCopies == 0) || ((uiTokens % uiCopies) != 0))
	{
		return false;
	}
	uiTaken = 0;
	for (i = iFrom; (i <= iTo) && (uiTaken < uiTokens / uiCopies); i++)
	{
		if (IsTokenFrom(i, iLine, iColumn, iEndLine, iEndColumn, &iAtLine, &iAtColumn, &iBy))
		{
			if (*piFirstToken < 0)
			{
				*piFirstToken = i;
			}
			*piLastToken = i;
			uiTaken++;
		}
		else if (*piFirstToken >= 0)
		{
			//Something else is in among them.
			*piFirstToken = -1;
			*piLastToken = -1;
			return false;
		}
	}
	return *piFirstToken >= 0;
}


//////////////////////////////////////////////////////////////////////////
//
// What an expression's type is of, appended as the index keys it: the
// class or enum a reference, a pointer or an array of it is to, however
// many levels of pointer and array (*piLevels, if asked for).  The file
// must have been checked.  False if the expression has no type, or one
// with no name of its own (an int).
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::AppendTypeKey(CCPPSyntax* pcExpression, CChars* pszDest, int32* piLevels)
{
	SCPP65Type*	psType;
	CChars		szKey;
	int32		iLevels;
	bool		bResult;

	iLevels = 0;
	psType = mbChecked ? mcPass.GetType(pcExpression) : NULL;
	while (psType && (psType->IsReference() || psType->IsPointer() || psType->IsArray()) && psType->psBase)
	{
		iLevels += psType->IsReference() ? 0 : 1;
		psType = psType->psBase;
	}
	szKey.Init();
	bResult = psType && (psType->IsClass() || (psType->pcEnum != NULL)) && mcPass.AppendType(psType->Unqualified(), &szKey);
	if (bResult)
	{
		szKey.StripWhitespace();
		pszDest->Append(szKey);
		if (piLevels)
		{
			*piLevels = iLevels;
		}
	}
	szKey.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CIDETrialTexts::GetFileText(const char* szFileName, CTextFile* pcFile, const char** pszText, size* puiLength)
{
	if (mszText && (StringInsensitiveCompare(szFileName, mszFileName) == 0))
	{
		*pszText = mszText;
		*puiLength = strlen(mszText);
		return true;
	}
	if (mszUnitText && mszUnit && (StringInsensitiveCompare(szFileName, mszUnit) == 0))
	{
		*pszText = mszUnitText;
		*puiLength = strlen(mszUnitText);
		return true;
	}
	return mpcTexts->GetFileText(szFileName, pcFile, pszText, puiLength);
}


//////////////////////////////////////////////////////////////////////////
//
// The errors a file's check has (its preprocessing's and its parse's
// too), with szText as its text (as it is, if NULL) and szUnitText as the
// text of szUnit, the unit it is parsed in.  MAX_SIZE if it can't be
// parsed.  A header checked with a text is left with that text in the
// code model: the caller gives it back its own
// (CIDECodeModel::SetHeaderText).
//
//////////////////////////////////////////////////////////////////////////
size CountIDETrialErrors(CIDECodeModel* pcModel, CIDEFileTexts* pcTexts, const char* szFileName, char* szText, const char* szUnit, char* szUnitText)
{
	CIDETrialTexts		cTexts;
	CIDERefactorUnit	cUnit;
	size				uiErrors;

	cTexts.mpcTexts = pcTexts;
	cTexts.mszFileName = szFileName;
	cTexts.mszText = szText;
	cTexts.mszUnit = szUnit;
	cTexts.mszUnitText = szUnitText;
	cUnit.Init();
	uiErrors = cUnit.Parse(pcModel, &cTexts, szFileName, true) ? cUnit.NumErrors() : MAX_SIZE;
	cUnit.Kill();
	return uiErrors;
}


//////////////////////////////////////////////////////////////////////////
//
// The tokens written in a range of the file (a selection): the first
// that starts in it and the last that ends in it.  False if there are
// none, or one of them is only partly in it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::FindTokensIn(int iLine, int iColumn, int iEndLine, int iEndColumn, int32* piFirstToken, int32* piLastToken)
{
	size				i;
	SPreprocessedToken*	psSource;
	int32				iTokenEnd;
	bool				bStartsIn;
	bool				bEndsIn;
	bool				bStartsBeforeEnd;
	bool				bEndsAfterStart;

	*piFirstToken = -1;
	*piLastToken = -1;
	for (i = 0; mbParsed && (i < mcUnit.mcTokeniser.NumTokens()); i++)
	{
		psSource = mcUnit.mcTokeniser.GetSource(mcUnit.mcTokeniser.Get(i));
		if ((psSource->pcFile != mpcFile) || (psSource->iExpansion != -1))
		{
			continue;
		}
		iTokenEnd = psSource->iColumn + (int32)psSource->uiLength;
		bStartsIn = ((psSource->iLine > iLine) || ((psSource->iLine == iLine) && (psSource->iColumn >= iColumn))) &&
					((psSource->iLine < iEndLine) || ((psSource->iLine == iEndLine) && (psSource->iColumn < iEndColumn)));
		bEndsIn = ((psSource->iLine > iLine) || ((psSource->iLine == iLine) && (iTokenEnd > iColumn))) &&
				  ((psSource->iLine < iEndLine) || ((psSource->iLine == iEndLine) && (iTokenEnd <= iEndColumn)));
		bStartsBeforeEnd = (psSource->iLine < iEndLine) || ((psSource->iLine == iEndLine) && (psSource->iColumn < iEndColumn));
		bEndsAfterStart = (psSource->iLine > iLine) || ((psSource->iLine == iLine) && (iTokenEnd > iColumn));
		if (bStartsBeforeEnd && bEndsAfterStart && (!bStartsIn || !bEndsIn))
		{
			//Cut by an end of the range.
			return false;
		}
		if (bStartsIn && bEndsIn)
		{
			if (*piFirstToken < 0)
			{
				*piFirstToken = (int32)i;
			}
			*piLastToken = (int32)i;
		}
	}
	return *piFirstToken >= 0;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a node is an expression that has a value of its own: not a
// list of initialisers.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::IsExpression(CCPPSyntax* pcNode)
{
	ECPPSyntax	eKind;

	eKind = pcNode->GetKind();
	return (eKind >= CPPS_LiteralExpression) && (eKind <= CPPS_NoexceptExpression) && (eKind != CPPS_InitializerList) && (eKind != CPPS_ParenthesisedInitializer);
}


//////////////////////////////////////////////////////////////////////////
//
// A run of tokens' text as it would be written on one line: a space
// between two tokens where leaving it out would join them (two words),
// and after a comma.  For comparing what is written in two places, and
// for writing a type that was written elsewhere.
//
//////////////////////////////////////////////////////////////////////////
void CIDERefactorUnit::AppendTokens(int32 iFirstToken, int32 iLastToken, CChars* pszDest)
{
	if (mbParsed)
	{
		AppendIDETokens(&mcUnit.mcTokeniser, iFirstToken, iLastToken, pszDest);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A type as a declaration wrote it (Refactorings.txt, F2), for a new
// variable that is to hold a value of it: the specifiers ("const char",
// "uint32", "CArray<CFoo>") without those that aren't of the type
// (static, extern, typedef and the like), then the declarator's pointers
// ("*", "* const*").  A reference is what it refers to.  Without a
// pointer, const and volatile are left out: the new variable is assigned
// to.  False if it can't be written that way: it is an array or a
// function, or one of its names is a template's parameter or is declared
// in a class (it may not mean the same where it is to be written).
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::AppendDeclaredTypeOf(CCPPSyntaxDeclSpecifiers* pcSpecifiers, CCPPSyntaxDeclarator* pcDeclarator, CChars* pszDest)
{
	static const char*		aszNotType[] = { "static", "extern", "typedef", "mutable", "inline", "virtual", "explicit", "friend", "constexpr", "register", "thread_local", "__inline", "__forceinline" };
	CCPPSyntax*				pcPrefix;
	SPreprocessedToken*		psSource;
	SCPPTokenSymbol*		psSymbol;
	CChars					szPointers;
	CChars					szSpecifiers;
	int32					i;
	size					j;
	bool					bLeftOut;
	bool					bPointer;

	if (!pcSpecifiers || (pcSpecifiers->miFirstToken < 0))
	{
		return false;
	}

	//The declarator: only pointers, and a reference, before its name; nothing after it but a function's parameters.
	szPointers.Init();
	bPointer = false;
	for (j = 0; pcDeclarator && (j < pcDeclarator->macpPrefix.NumElements()); j++)
	{
		pcPrefix = pcDeclarator->macpPrefix.GetPtr(j);
		if (pcPrefix->GetKind() != CPPS_PointerOperator)
		{
			continue;
		}
		if (IsToken(pcPrefix->miFirstToken, "&") || IsToken(pcPrefix->miFirstToken, "&&"))
		{
			continue;
		}
		bPointer = true;
		AppendTokens(pcPrefix->miFirstToken, pcPrefix->miLastToken, &szPointers);
	}
	if (pcDeclarator && (pcDeclarator->mpcNested || ((pcDeclarator->macpSuffixes.NumElements() > 0) && !pcDeclarator->IsFunction())))
	{
		szPointers.Kill();
		return false;
	}

	szSpecifiers.Init();
	for (i = pcSpecifiers->miFirstToken; i <= pcSpecifiers->miLastToken; i++)
	{
		psSource = mcUnit.mcTokeniser.GetSource(mcUnit.mcTokeniser.Get((size)i));
		bLeftOut = false;
		for (j = 0; j < sizeof(aszNotType) / sizeof(char*); j++)
		{
			bLeftOut |= IsToken(i, aszNotType[j]);
		}
		if (!bPointer)
		{
			bLeftOut |= IsToken(i, "const") || IsToken(i, "volatile");
		}
		if (bLeftOut)
		{
			continue;
		}
		psSymbol = mcUnit.mcTree.GetTokenSymbol(i);
		if (psSymbol && psSymbol->pcSymbol)
		{
			if ((psSymbol->pcSymbol->meKind == CPPSK_TypeParameter) || (psSymbol->pcSymbol->meKind == CPPSK_ValueParameter) || (psSymbol->pcSymbol->meKind == CPPSK_TemplateTemplateParameter) ||
				(psSymbol->pcSymbol->mpcScope && (psSymbol->pcSymbol->mpcScope->meKind == CPPSC_Class)))
			{
				szSpecifiers.Kill();
				szPointers.Kill();
				return false;
			}
		}
		if (!szSpecifiers.Empty() && (IsIDENameChar(szSpecifiers.Text()[szSpecifiers.Length() - 1]) && IsIDENameChar(psSource->szText[0])))
		{
			szSpecifiers.Append(' ');
		}
		else if (!szSpecifiers.Empty() && (szSpecifiers.Text()[szSpecifiers.Length() - 1] == ','))
		{
			szSpecifiers.Append(' ');
		}
		szSpecifiers.Append(psSource->szText, psSource->uiLength);
	}
	if (szSpecifiers.Empty())
	{
		szSpecifiers.Kill();
		szPointers.Kill();
		return false;
	}
	pszDest->Append(szSpecifiers);
	pszDest->Append(szPointers);
	szSpecifiers.Kill();
	szPointers.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the type is named that what is declared at a place (a variable's,
// a field's, a parameter's or a function's name, in a declaration of it)
// is declared with: the name its declaration's specifiers have, its last
// part if it is qualified ("CTank" of "const Plant::CTank*").  False if
// the place isn't a declared name or its type has no name of its own (a
// built in type, auto, decltype, a class written there).
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::FindDeclaredTypeName(int iLine, int iColumn, SCPPSyntaxRange* psDest)
{
	CCPPSyntaxDeclarator*		pcDeclarator;
	CCPPSyntaxDeclSpecifiers*	pcSpecifiers;
	CCPPSyntaxName*				pcName;
	CCPPSyntax*					pcNode;
	CCPPSyntax*					pcParent;
	int32						iToken;

	iToken = FindTokenAt(iLine, iColumn);
	pcNode = (iToken != -1) ? FindNodeOfTokens(iToken, iToken) : NULL;
	pcDeclarator = pcNode ? (CCPPSyntaxDeclarator*)FindAncestor(pcNode, CPPS_Declarator) : NULL;
	while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
	{
		pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
	}
	pcName = pcDeclarator ? pcDeclarator->GetName() : NULL;
	if (!pcName || (iToken < pcName->miFirstToken) || (iToken > pcName->miLastToken))
	{
		return false;
	}

	pcParent = pcDeclarator->mpcParent;
	if (pcParent && (pcParent->GetKind() == CPPS_InitDeclarator))
	{
		pcParent = pcParent->mpcParent;
	}
	pcSpecifiers = NULL;
	if (pcParent && (pcParent->GetKind() == CPPS_SimpleDeclaration))
	{
		pcSpecifiers = ((CCPPSyntaxSimpleDeclaration*)pcParent)->mpcSpecifiers;
	}
	else if (pcParent && (pcParent->GetKind() == CPPS_Parameter))
	{
		pcSpecifiers = ((CCPPSyntaxParameter*)pcParent)->mpcSpecifiers;
	}
	else if (pcParent && (pcParent->GetKind() == CPPS_FunctionDefinition))
	{
		pcSpecifiers = ((CCPPSyntaxFunctionDefinition*)pcParent)->mpcSpecifiers;
	}

	pcName = NULL;
	if (pcSpecifiers && pcSpecifiers->mpcType && (pcSpecifiers->mpcType->GetKind() == CPPS_NamedTypeSpecifier))
	{
		pcName = ((CCPPSyntaxNamedTypeSpecifier*)pcSpecifiers->mpcType)->mpcName;
	}
	else if (pcSpecifiers && pcSpecifiers->mpcType && (pcSpecifiers->mpcType->GetKind() == CPPS_ElaboratedTypeSpecifier))
	{
		pcName = ((CCPPSyntaxElaboratedTypeSpecifier*)pcSpecifiers->mpcType)->mpcName;
	}
	return pcName && pcName->GetLast() && GetTokensRange(pcName->GetLast()->miFirstToken, pcName->GetLast()->miFirstToken, psDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The type of an expression as its declaration wrote it, where the
// expression is one thing with a declared type: a variable, a parameter
// or a field named (x, o.m, p->m), or a call of a function, whose
// declared type is what it returns.  So a typedef the declaration used
// (uint32, size) is the type, where the compiler's own type has none.
// Parentheses around it are looked through.  False if it isn't such an
// expression or its declaration isn't in the unit parsed; the compiler's
// type is written then (CCPPSemanticPass::AppendType).  It needs a kept
// check, for what a member or a call was resolved to.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::AppendDeclaredType(CCPPSyntax* pcExpression, CChars* pszDest)
{
	CCPPSyntaxName*				pcName;
	CCPPSymbol*					pcSymbol;
	CCPPSyntaxDeclarator*		pcDeclarator;
	CCPPSyntax*					pcDeclaration;
	CCPPSyntax*					pcParent;
	SCPPResolvedName*			psResolved;
	bool						bCall;

	if (!mbChecked || !pcExpression)
	{
		return false;
	}
	pcExpression = pcExpression->WithoutParentheses();
	bCall = false;
	if (pcExpression && (pcExpression->GetKind() == CPPS_CallExpression))
	{
		bCall = true;
		pcExpression = ((CCPPSyntaxCallExpression*)pcExpression)->mpcCallee;
		pcExpression = pcExpression ? pcExpression->WithoutParentheses() : NULL;
	}
	pcName = NULL;
	if (pcExpression && (pcExpression->GetKind() == CPPS_IdExpression))
	{
		pcName = ((CCPPSyntaxIdExpression*)pcExpression)->mpcName;
	}
	else if (pcExpression && (pcExpression->GetKind() == CPPS_MemberExpression))
	{
		pcName = ((CCPPSyntaxMemberExpression*)pcExpression)->mpcName;
	}
	if (!pcName || !pcName->GetLast())
	{
		return false;
	}

	psResolved = mcPass.GetResolved(pcName->GetLast()->miName);
	pcSymbol = psResolved ? psResolved->pcSymbol : pcName->mpcSymbol;
	pcDeclarator = psResolved ? psResolved->pcDeclarator : NULL;
	if (psResolved && mcPass.GetNextResolved(psResolved))
	{
		//It is different things in a template's instances.
		return false;
	}
	if (bCall)
	{
		//The function's declarator: its specifiers are its declaration's, or its definition's.
		if (!pcDeclarator)
		{
			return false;
		}
		pcParent = pcDeclarator->mpcParent;
		if (pcParent && (pcParent->GetKind() == CPPS_InitDeclarator))
		{
			pcParent = pcParent->mpcParent;
		}
		if (pcParent && (pcParent->GetKind() == CPPS_SimpleDeclaration))
		{
			return AppendDeclaredTypeOf(((CCPPSyntaxSimpleDeclaration*)pcParent)->mpcSpecifiers, pcDeclarator, pszDest);
		}
		if (pcParent && (pcParent->GetKind() == CPPS_FunctionDefinition))
		{
			return AppendDeclaredTypeOf(((CCPPSyntaxFunctionDefinition*)pcParent)->mpcSpecifiers, pcDeclarator, pszDest);
		}
		return false;
	}

	if (!pcSymbol || ((pcSymbol->meKind != CPPSK_Variable) && (pcSymbol->meKind != CPPSK_Field) && (pcSymbol->meKind != CPPSK_Parameter)))
	{
		return false;
	}
	pcDeclaration = pcSymbol->mpcDeclaration;
	if (pcDeclaration && (pcDeclaration->GetKind() == CPPS_Declarator))
	{
		pcDeclaration = pcDeclaration->mpcParent;
	}
	if (pcDeclaration && (pcDeclaration->GetKind() == CPPS_InitDeclarator) && pcDeclaration->mpcParent && (pcDeclaration->mpcParent->GetKind() == CPPS_SimpleDeclaration))
	{
		return AppendDeclaredTypeOf(((CCPPSyntaxSimpleDeclaration*)pcDeclaration->mpcParent)->mpcSpecifiers, ((CCPPSyntaxInitDeclarator*)pcDeclaration)->mpcDeclarator, pszDest);
	}
	if (pcDeclaration && (pcDeclaration->GetKind() == CPPS_Parameter))
	{
		return AppendDeclaredTypeOf(((CCPPSyntaxParameter*)pcDeclaration)->mpcSpecifiers, ((CCPPSyntaxParameter*)pcDeclaration)->mpcDeclarator, pszDest);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// The label the compiler gives the function whose name is written at a
// place in the file (in a declaration or a definition of it), for the
// target the unit was checked for (CCPPSemanticPass::AppendFunctionLabel).
// False if there is no function's declarator there, or the unit wasn't
// checked.
//
//////////////////////////////////////////////////////////////////////////
bool CIDERefactorUnit::AppendFunctionLabel(int iLine, int iColumn, CChars* pszDest)
{
	CCPPSyntaxDeclarator*	pcDeclarator;
	CCPPSyntax*				pcNode;

	if (!mbChecked)
	{
		return false;
	}
	pcNode = FindNodeAt(iLine, iColumn);
	pcDeclarator = (CCPPSyntaxDeclarator*)FindAncestor(pcNode, CPPS_Declarator);
	while (pcDeclarator && pcDeclarator->mpcParent && (pcDeclarator->mpcParent->GetKind() == CPPS_Declarator))
	{
		pcDeclarator = (CCPPSyntaxDeclarator*)pcDeclarator->mpcParent;
	}
	return pcDeclarator && mcPass.AppendFunctionLabel(pcDeclarator, pszDest);
}


bool CIDERefactorUnit::IsParsed(void) const { return mbParsed; }
bool CIDERefactorUnit::IsChecked(void) const { return mbChecked; }
char* CIDERefactorUnit::GetFileName(void) { return mszFileName.Text(); }
CCPPTokeniser* CIDERefactorUnit::GetTokeniser(void) { return &mcUnit.mcTokeniser; }
CCPPSyntaxTree* CIDERefactorUnit::GetTree(void) { return &mcUnit.mcTree; }
CCPPSemanticPass* CIDERefactorUnit::GetPass(void) { return &mcPass; }
//Whether the file's build configuration compiles it with exceptions (CCPPCompileOptions::mbExceptions).
bool CIDERefactorUnit::IsExceptions(void) const { return mcUnit.mcOptions.mbExceptions; }
CSourceFile* CIDERefactorUnit::GetFile(void) { return mpcFile; }
