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
#include "IDEDuplicateMatcher.h"


//////////////////////////////////////////////////////////////////////////
//
// The run's unit, its free names and whether its literals may be others.
// Neither is held: they last as long as the matcher.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicateMatcher::Init(CIDERefactorUnit* pcUnit, CArrayChars* paszFree, bool bLiteralsFree)
{
	mpcUnit = pcUnit;
	mpaszFree = paszFree;
	mbLiteralsFree = bLiteralsFree;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicateMatcher::Kill(void)
{
	mpcUnit = NULL;
	mpaszFree = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// paszNames (Init'ed) as a comparison starts: an empty name for each
// free name, none met yet.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicateMatcher::StartNames(CArrayChars* paszNames)
{
	size	i;

	paszNames->ReInit();
	for (i = 0; i < mpaszFree->NumElements(); i++)
	{
		paszNames->Add("");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the iTokens tokens from iFirst are written again at pcOther's
// iAt: token for token, but for the free names, each of which is there
// a name of paszFreeThere (any name, if it is NULL) that is always the
// same and is no other free name's, and the literals if they may be
// others.  paszNames has what the other place calls each free name, from
// the runs compared before, and gets what this one calls them; it is
// left part filled in when they aren't the same.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicateMatcher::Match(int32 iFirst, int32 iTokens, CIDERefactorUnit* pcOther, int32 iAt, CArrayChars* paszFreeThere, CArrayChars* paszNames)
{
	SPreprocessedToken*	psSource;
	SPreprocessedToken*	psOther;
	CChars*				pszName;
	size				uiFree;
	size				uiNamed;
	int32				k;
	char				cKind;
	char				cOtherKind;

	//A name alone isn't the end of a member there ("psPoint->iX" isn't "iX").
	if (IsNamedAlone(mpcUnit, iFirst) && !IsNamedAlone(pcOther, iAt))
	{
		return false;
	}
	for (k = 0; k < iTokens; k++)
	{
		psSource = mpcUnit->GetTokeniser()->GetSource(mpcUnit->GetTokeniser()->Get((size)(iFirst + k)));
		psOther = pcOther->GetTokeniser()->GetSource(pcOther->GetTokeniser()->Get((size)(iAt + k)));
		if (!psSource || !psOther)
		{
			return false;
		}

		uiFree = IsNamedAlone(mpcUnit, iFirst + k) ? FindFree(psSource) : MAX_SIZE;
		if (uiFree != MAX_SIZE)
		{
			//One of the free names: a name there that is always the same one, no other's, and free there too.
			if (!pcOther->GetTokeniser()->Get((size)(iAt + k))->IsIdentifier() || !IsNamedAlone(pcOther, iAt + k))
			{
				return false;
			}
			if (paszFreeThere && (paszFreeThere->GetIndex(psOther->szText, psOther->uiLength) == ARRAY_ELEMENT_NOT_FOUND))
			{
				return false;
			}
			pszName = paszNames->Get(uiFree);
			if (pszName->Empty())
			{
				uiNamed = paszNames->GetIndex(psOther->szText, psOther->uiLength);
				if (uiNamed != ARRAY_ELEMENT_NOT_FOUND)
				{
					return false;
				}
				pszName->Append(psOther->szText, psOther->uiLength);
			}
			else if ((pszName->Length() != psOther->uiLength) || (memcmp(pszName->Text(), psOther->szText, psOther->uiLength) != 0))
			{
				return false;
			}
			continue;
		}

		if (mbLiteralsFree && IsLiteral(mpcUnit, iFirst + k, &cKind))
		{
			if (!IsLiteral(pcOther, iAt + k, &cOtherKind) || (cKind != cOtherKind))
			{
				return false;
			}
			continue;
		}

		if ((psSource->uiLength != psOther->uiLength) || (memcmp(psSource->szText, psOther->szText, psSource->uiLength) != 0))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the run's token iToken is one of its free names.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicateMatcher::IsFree(int32 iToken)
{
	SPreprocessedToken*	psSource;

	psSource = mpcUnit->GetTokeniser()->GetSource(mpcUnit->GetTokeniser()->Get((size)iToken));
	return psSource && IsNamedAlone(mpcUnit, iToken) && (FindFree(psSource) != MAX_SIZE);
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the run's token iToken is a name that must be written the same
// at the other place: not a reserved word, and not a free name.  Find
// Duplicates passes over a file that has none of a run's.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicateMatcher::IsFixedIdentifier(int32 iToken)
{
	return mpcUnit->GetTokeniser()->Get((size)iToken)->IsIdentifier() && !IsFree(iToken);
}


//////////////////////////////////////////////////////////////////////////
//
// The free name a token is, MAX_SIZE if none.
//
//////////////////////////////////////////////////////////////////////////
size CIDEDuplicateMatcher::FindFree(SPreprocessedToken* psToken)
{
	size	uiIndex;

	uiIndex = mpaszFree->GetIndex(psToken->szText, psToken->uiLength);
	return (uiIndex == ARRAY_ELEMENT_NOT_FOUND) ? MAX_SIZE : uiIndex;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether pcUnit's token iToken is a name named alone: not a member
// (after . or ->) or a name qualified (after ::).
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicateMatcher::IsNamedAlone(CIDERefactorUnit* pcUnit, int32 iToken)
{
	SPreprocessedToken*	psBefore;

	if (!pcUnit->GetTokeniser()->Get((size)iToken)->IsIdentifier())
	{
		return false;
	}
	if (iToken == 0)
	{
		return true;
	}
	psBefore = pcUnit->GetTokeniser()->GetSource(pcUnit->GetTokeniser()->Get((size)(iToken - 1)));
	if (!psBefore)
	{
		return true;
	}
	return !(((psBefore->uiLength == 1) && (psBefore->szText[0] == '.')) ||
			 ((psBefore->uiLength == 2) && ((memcmp(psBefore->szText, "->", 2) == 0) || (memcmp(psBefore->szText, "::", 2) == 0))));
}


//////////////////////////////////////////////////////////////////////////
//
// Whether pcUnit's token iToken is a literal, and its kind: 'n' a number,
// 's' a string, 'c' a character.  true and false are words, not
// literals: they must be the same.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEDuplicateMatcher::IsLiteral(CIDERefactorUnit* pcUnit, int32 iToken, char* pcKind)
{
	CCPPToken*	pcToken;

	pcToken = pcUnit->GetTokeniser()->Get((size)iToken);
	if (pcToken->IsNumber())
	{
		*pcKind = 'n';
		return true;
	}
	if (pcToken->IsString())
	{
		*pcKind = 's';
		return true;
	}
	if (pcToken->IsChar())
	{
		*pcKind = 'c';
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// Each expression of miTokens tokens (with mbExpression), or each run of
// one block's statements that are miTokens tokens together: its first
// token, once, in mpaiFirsts.
//
//////////////////////////////////////////////////////////////////////////
void CIDEDuplicatePlaceFinder::Visit(CCPPSyntax* pcChild)
{
	CCPPSyntaxCompoundStatement*	pcBlock;
	int32							iLast;
	size							i;
	size							j;

	if (mbExpression)
	{
		if (mpcUnit->IsExpression(pcChild) && (pcChild->miFirstToken >= 0) && (pcChild->miLastToken - pcChild->miFirstToken + 1 == miTokens))
		{
			mpaiFirsts->AddIfUnique(pcChild->miFirstToken);
		}
	}
	else if (pcChild->GetKind() == CPPS_CompoundStatement)
	{
		pcBlock = (CCPPSyntaxCompoundStatement*)pcChild;
		for (i = 0; i < pcBlock->macpStatements.NumElements(); i++)
		{
			iLast = pcBlock->macpStatements.GetPtr(i)->miFirstToken + miTokens - 1;
			for (j = i; (j < pcBlock->macpStatements.NumElements()) && (pcBlock->macpStatements.GetPtr(j)->miLastToken <= iLast); j++)
			{
				if ((pcBlock->macpStatements.GetPtr(i)->miFirstToken >= 0) && (pcBlock->macpStatements.GetPtr(j)->miLastToken == iLast))
				{
					mpaiFirsts->AddIfUnique(pcBlock->macpStatements.GetPtr(i)->miFirstToken);
				}
			}
		}
	}
	pcChild->VisitFields(this);
}


//Collects the names a function declares for itself: its parameters, and the variables its body declares (FindIDEFunctionLocals).
class CIDELocalsFinder : public CCPPSyntaxChildVisitor
{
public:
	CIDERefactorUnit*	mpcUnit;
	CArrayChars*		mpaszDest;

	void	Visit(CCPPSyntax* pcChild) override
	{
		CCPPSyntaxDeclarator*	pcDeclarator;
		CCPPSyntaxName*			pcName;
		SPreprocessedToken*		psName;
		ECPPSyntax				eParent;

		if ((pcChild->GetKind() == CPPS_Declarator) && pcChild->mpcParent)
		{
			eParent = pcChild->mpcParent->GetKind();
			if ((eParent == CPPS_InitDeclarator) || (eParent == CPPS_Parameter) || (eParent == CPPS_RangeForStatement))
			{
				pcDeclarator = (CCPPSyntaxDeclarator*)pcChild;
				pcName = pcDeclarator->GetName();
				if (pcName && pcName->IsSimple() && pcName->GetLast() && (pcName->GetLast()->miName >= 0))
				{
					psName = mpcUnit->GetTokeniser()->GetSource(mpcUnit->GetTokeniser()->Get((size)pcName->GetLast()->miName));
					if (psName && (mpaszDest->GetIndex(psName->szText, psName->uiLength) == ARRAY_ELEMENT_NOT_FOUND))
					{
						mpaszDest->Add(psName->szText, 0, psName->uiLength);
					}
				}
			}
		}
		if (pcChild->GetKind() != CPPS_ClassSpecifier)
		{
			pcChild->VisitFields(this);
		}
	}
};


//////////////////////////////////////////////////////////////////////////
//
// The names pcFunction declares for itself, added to paszDest once each:
// its parameters' and its body's variables' (a class defined in it is
// not looked in).  Find Duplicates lets these be other names elsewhere.
//
//////////////////////////////////////////////////////////////////////////
void FindIDEFunctionLocals(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, CArrayChars* paszDest)
{
	CIDELocalsFinder	cFinder;

	cFinder.mpcUnit = pcUnit;
	cFinder.mpaszDest = paszDest;
	pcFunction->VisitFields(&cFinder);
}


//Counts the statements under a node, CountIDEStatements's.
class CIDEStatementCounter : public CCPPSyntaxChildVisitor
{
public:
	size	muiStatements;

	void	Visit(CCPPSyntax* pcChild) override
	{
		Count(pcChild);
	}

	void	Count(CCPPSyntax* pcNode)
	{
		bool	bInitialiser;

		bInitialiser = pcNode->mpcParent && (pcNode->mpcParent->GetKind() == CPPS_ForStatement) && (((CCPPSyntaxForStatement*)pcNode->mpcParent)->mpcInitialiser == pcNode);
		if ((pcNode->GetKind() > CPPS_CompoundStatement) && (pcNode->GetKind() <= CPPS_SkippedStatement) && !bInitialiser)
		{
			muiStatements++;
		}
		pcNode->VisitFields(this);
	}
};


//////////////////////////////////////////////////////////////////////////
//
// How many statements pcNode is and has inside it, a block's braces and
// a for's first part not counted: "if (a) { b; c; }" is three.  How big
// a run of statements is.
//
//////////////////////////////////////////////////////////////////////////
size CountIDEStatements(CCPPSyntax* pcNode)
{
	CIDEStatementCounter	cCounter;

	cCounter.muiStatements = 0;
	cCounter.Count(pcNode);
	return cCounter.muiStatements;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether pcUnit's token iToken is its file's own (not a header's it
// includes, or the unit's where the file is a header): only those can be
// listed as where something is written.
//
//////////////////////////////////////////////////////////////////////////
bool IsIDETokenOfFile(CIDERefactorUnit* pcUnit, int32 iToken)
{
	SCPPSyntaxRange	sRange;

	return (iToken >= 0) && pcUnit->GetTokensRange(iToken, iToken, &sRange);
}

