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
#ifndef __IDE_DUPLICATE_MATCHER_H__
#define __IDE_DUPLICATE_MATCHER_H__
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "IDERefactorUnit.h"


//Code written again elsewhere, its names apart (Find Duplicates, and Extract Method's duplicates): a run of a unit's
//tokens compared token for token with a run of another's (or of its own).  They are the same but for the run's free
//names (the variables and parameters of the function it is in), each of which may be another name at the other place,
//always the same other and never one another of them is; and, with mbLiteralsFree, but for its literals, each of which
//may be another of the same kind there (a number another number, a string another string, a character another).  A
//free name is one named alone, not a member (after . or ->) or a qualified name (after ::).
//
//The names the other place has for the free names are kept in an array of the free names' order, empty for one not
//met yet: Match fills it in, so runs compared one after another (a function's statements) share it.
class CIDEDuplicateMatcher
{
protected:
	CIDERefactorUnit*	mpcUnit;		//The run's.  Not held.
	CArrayChars*		mpaszFree;		//Its free names.  Not held.
	bool				mbLiteralsFree;

public:
	void	Init(CIDERefactorUnit* pcUnit, CArrayChars* paszFree, bool bLiteralsFree);
	void	Kill(void);

	void	StartNames(CArrayChars* paszNames);
	bool	Match(int32 iFirst, int32 iTokens, CIDERefactorUnit* pcOther, int32 iAt, CArrayChars* paszFreeThere, CArrayChars* paszNames);
	bool	IsFree(int32 iToken);
	bool	IsFixedIdentifier(int32 iToken);

	static bool	IsNamedAlone(CIDERefactorUnit* pcUnit, int32 iToken);
	static bool	IsLiteral(CIDERefactorUnit* pcUnit, int32 iToken, char* pcKind);

protected:
	size	FindFree(SPreprocessedToken* psToken);
};


//Where a run of tokens could be written again in a unit: an expression of as many tokens, or statements of one block
//that are as many tokens together.  The first token of each.
class CIDEDuplicatePlaceFinder : public CCPPSyntaxChildVisitor
{
public:
	CIDERefactorUnit*	mpcUnit;
	CArrayInt*			mpaiFirsts;
	int32				miTokens;
	bool				mbExpression;

	void	Visit(CCPPSyntax* pcChild) override;
};


void	FindIDEFunctionLocals(CIDERefactorUnit* pcUnit, CCPPSyntaxFunctionDefinition* pcFunction, CArrayChars* paszDest);
size	CountIDEStatements(CCPPSyntax* pcNode);
bool	IsIDETokenOfFile(CIDERefactorUnit* pcUnit, int32 iToken);


#endif // __IDE_DUPLICATE_MATCHER_H__

