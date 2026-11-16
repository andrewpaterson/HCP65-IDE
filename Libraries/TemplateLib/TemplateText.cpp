/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela TemplateLib

Codaphela TemplateLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela TemplateLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela TemplateLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/CPPIdentifier.h"
#include "BaseLib/CPPString.h"
#include "BaseLib/JSONWhitespace.h"
#include "BaseLib/TextParserConfig.h"
#include "TemplateStrings.h"
#include "TemplateText.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::Init(void)
{
	mszSource.Init();
	mszPool.Init();
	masNodes.Init();
	masExpressions.Init();
	muiRoot = TEMPLATE_NONE;
	mszError.Init();
	mpcParser = NULL;
	mbParsed = false;
	miDepth = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::Kill(void)
{
	mszError.Kill();
	masExpressions.Kill();
	masNodes.Kill();
	mszPool.Kill();
	mszSource.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Why the last Parse or Evaluate failed: "Line 3, column 5: #if has no
// #end".
//
//////////////////////////////////////////////////////////////////////////
char* CTemplateText::GetError(void)
{
	return mszError.Text();
}


//////////////////////////////////////////////////////////////////////////
//
// The line and column (from 1) of a place in the source.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::FindLineColumn(size uiPos, size* puiLine, size* puiColumn)
{
	const char*	sz;
	size		i;

	sz = mszSource.Text();
	*puiLine = 1;
	*puiColumn = 1;
	for (i = 0; (i < uiPos) && (sz[i] != '\0'); i++)
	{
		if (sz[i] == '\n')
		{
			(*puiLine)++;
			*puiColumn = 1;
		}
		else
		{
			(*puiColumn)++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The error, at a place in the source.  False always.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::Error(size uiPos, const char* szMessage)
{
	size	uiLine;
	size	uiColumn;

	if (!mszError.Empty())
	{
		return false;
	}
	FindLineColumn(uiPos, &uiLine, &uiColumn);
	mszError.Append("Line ")->Append((uint32)uiLine)->Append(", column ")->Append((uint32)uiColumn)->Append(": ")->Append(szMessage);
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CTemplateText::AddName(const char* szName, size uiLength)
{
	size	uiName;

	uiName = mszPool.Length();
	mszPool.Append(szName, uiLength);
	mszPool.Append('\0');
	return uiName;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
char* CTemplateText::GetName(size uiName)
{
	return &mszPool.Text()[uiName];
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CTemplateText::AddNode(ETemplateNode eKind, size uiStart, size uiEnd)
{
	STemplateNode*	psNode;
	size			uiNode;

	uiNode = masNodes.NumElements();
	psNode = masNodes.Add();
	memset(psNode, 0, sizeof(STemplateNode));
	psNode->eKind = eKind;
	psNode->uiStart = uiStart;
	psNode->uiEnd = uiEnd;
	psNode->uiExpression = TEMPLATE_NONE;
	psNode->uiValue = TEMPLATE_NONE;
	psNode->uiName = TEMPLATE_NONE;
	psNode->uiFirst = TEMPLATE_NONE;
	psNode->uiNext = TEMPLATE_NONE;
	psNode->uiElse = TEMPLATE_NONE;
	return uiNode;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CTemplateText::AddExpression(ETemplateExpression eKind, size uiStart)
{
	STemplateExpression*	psExpression;
	size					uiExpression;

	uiExpression = masExpressions.NumElements();
	psExpression = masExpressions.Add();
	memset(psExpression, 0, sizeof(STemplateExpression));
	psExpression->eKind = eKind;
	psExpression->uiName = TEMPLATE_NONE;
	psExpression->uiLeft = TEMPLATE_NONE;
	psExpression->uiRight = TEMPLATE_NONE;
	psExpression->uiFirst = TEMPLATE_NONE;
	psExpression->uiNext = TEMPLATE_NONE;
	psExpression->uiStart = uiStart;
	psExpression->uiEnd = uiStart;
	return uiExpression;
}


//////////////////////////////////////////////////////////////////////////
//
// Where the parser is, in the source (the parser of a string's part reads
// the same text).
//
//////////////////////////////////////////////////////////////////////////
size CTemplateText::Offset(void)
{
	return (size)(mpcParser->Position() - mszSource.Text());
}


//////////////////////////////////////////////////////////////////////////
//
// The character at the parser, or iAhead after it; '\0' past the end.
//
//////////////////////////////////////////////////////////////////////////
char CTemplateText::Peek(void)
{
	return mpcParser->IsOutside() ? '\0' : mpcParser->Current();
}


char CTemplateText::Peek(int iAhead)
{
	const char*	sz;

	sz = mpcParser->Position() + iAhead;
	return (mpcParser->IsOutside() || (sz > mpcParser->End())) ? '\0' : *sz;
}


//////////////////////////////////////////////////////////////////////////
//
// A name at the parser (an identifier), into the pool.  False if there is
// none there.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ReadName(size* puiName)
{
	const char*	szStart;
	size		uiLength;

	if (mpcParser->IsOutside())
	{
		return false;
	}
	szStart = mpcParser->Position();
	uiLength = 0;
	if (mpcParser->GetIdentifier(NULL, &uiLength, false) != TRITRUE)
	{
		return false;
	}
	*puiName = AddName(szStart, uiLength);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A word at the parser, taken if it is there (and not the start of a
// longer name).
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::IsWord(const char* szWord)
{
	return !mpcParser->IsOutside() && (mpcParser->GetExactIdentifier(szWord, false) == TRITRUE);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::SkipSpace(void)
{
	if (!mpcParser->IsOutside())
	{
		mpcParser->SkipWhitespace();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A template's text read into its nodes.  False, with the error, if it
// can't be.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::Parse(const char* szText)
{
	CArrayTemplateTokens	asTokens;
	ETemplateDirective		eStop;
	size					uiToken;
	size					uiStopToken;
	bool					bResult;

	mszSource.Set(szText ? szText : "");
	mszPool.Clear();
	masNodes.ReInit();
	masExpressions.ReInit();
	mszError.Clear();
	muiRoot = TEMPLATE_NONE;
	asTokens.Init();
	bResult = Lex(0, mszSource.Length(), false, &asTokens);
	if (bResult)
	{
		RemoveDirectiveLines(&asTokens);
		uiToken = 0;
		bResult = Build(&asTokens, &uiToken, 0, &muiRoot, &eStop, &uiStopToken);
	}
	asTokens.Kill();
	mbParsed = bResult;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A text token of the source, if it has any characters.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::AddText(size uiStart, size uiEnd, CArrayTemplateTokens* pasTokens)
{
	STemplateToken*	psToken;

	if (uiEnd <= uiStart)
	{
		return;
	}
	psToken = pasTokens->Add();
	memset(psToken, 0, sizeof(STemplateToken));
	psToken->eKind = TT_Text;
	psToken->uiStart = uiStart;
	psToken->uiEnd = uiEnd;
	psToken->uiExpression = TEMPLATE_NONE;
	psToken->uiValue = TEMPLATE_NONE;
	psToken->uiName = TEMPLATE_NONE;
	psToken->bKeep = true;
}


//////////////////////////////////////////////////////////////////////////
//
// The source from uiStart to uiEnd as tokens, read by a parser of its
// own: text (each ending at the end of its line, or before a token that
// isn't text), references, directives and comments.  In a string, \" and
// \\ are a " and a \ too.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::Lex(size uiStart, size uiEnd, bool bInString, CArrayTemplateTokens* pasTokens)
{
	CTextParser			cParser;
	STextParserConfig	sConfig;
	CTextParser*		pcOldParser;
	STemplateToken*	psToken;
	STemplateToken	sDirective;
	size			uiText;
	size			uiPos;
	size			uiExpression;
	bool			bDirective;
	bool			bResult;
	char			c;
	char			cNext;

	if (uiEnd <= uiStart)
	{
		return true;
	}
	pcOldParser = mpcParser;
	//White space is only white space: // and /* aren't comments in a template.
	sConfig.Init(SkipJSONWhitespace, ParseCPPString, ParseCPPExactIdentifier, ParseCPPIdentifier);
	cParser.Init(&mszSource.Text()[uiStart], (lsize)(uiEnd - uiStart), &sConfig);
	mpcParser = &cParser;
	uiText = uiStart;
	bResult = true;
	while (bResult && !mpcParser->IsOutside())
	{
		uiPos = Offset();
		c = Peek();
		cNext = Peek(1);
		if ((c == '\\') && ((cNext == '$') || (cNext == '#') || (bInString && ((cNext == '"') || (cNext == '\\')))))
		{
			//The character after it, as it is.
			AddText(uiText, uiPos, pasTokens);
			AddText(uiPos + 1, uiPos + 2, pasTokens);
			mpcParser->StepRight();
			mpcParser->StepRight();
			uiText = Offset();
		}
		else if (c == '$')
		{
			mpcParser->PushPosition();
			if (ParseReference(false, &uiExpression))
			{
				mpcParser->PassPosition();
				AddText(uiText, uiPos, pasTokens);
				psToken = pasTokens->Add();
				memset(psToken, 0, sizeof(STemplateToken));
				psToken->eKind = TT_Reference;
				psToken->uiStart = uiPos;
				psToken->uiEnd = Offset();
				psToken->uiExpression = uiExpression;
				psToken->uiValue = TEMPLATE_NONE;
				psToken->uiName = TEMPLATE_NONE;
				uiText = Offset();
			}
			else
			{
				mpcParser->PopPosition();
				bResult = mszError.Empty();
				mpcParser->StepRight();
			}
		}
		else if (c == '#')
		{
			mpcParser->PushPosition();
			bDirective = false;
			bResult = LexDirective(&sDirective, &bDirective);
			if (bResult && bDirective)
			{
				mpcParser->PassPosition();
				AddText(uiText, uiPos, pasTokens);
				memcpy(pasTokens->Add(), &sDirective, sizeof(STemplateToken));
				uiText = Offset();
			}
			else
			{
				mpcParser->PopPosition();
				mpcParser->StepRight();
			}
		}
		else
		{
			mpcParser->StepRight();
			if (c == '\n')
			{
				AddText(uiText, Offset(), pasTokens);
				uiText = Offset();
			}
		}
	}
	if (bResult)
	{
		AddText(uiText, uiEnd, pasTokens);
	}
	cParser.Kill();
	mpcParser = pcOldParser;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A directive or a comment at the parser (its '#'), into psDest, the
// parser after it; *pbDirective false if what is there isn't one (a C++
// #include "x.h", #ifndef, a # of nothing), which is text.  False only for
// an error.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::LexDirective(STemplateToken* psDest, bool* pbDirective)
{
	static const char*				aszWords[] = { "set", "if", "elseif", "else", "end", "foreach", "parse", "include", "break" };
	static const ETemplateDirective	aeWords[] = { TD_Set, TD_If, TD_ElseIf, TD_Else, TD_End, TD_Foreach, TD_Parse, TD_Include, TD_Break };
	ETemplateDirective	eDirective;
	size				uiStart;
	size				i;
	bool				bBraced;

	*pbDirective = false;
	uiStart = Offset();
	memset(psDest, 0, sizeof(STemplateToken));
	psDest->uiStart = uiStart;
	psDest->uiExpression = TEMPLATE_NONE;
	psDest->uiValue = TEMPLATE_NONE;
	psDest->uiName = TEMPLATE_NONE;
	mpcParser->StepRight();

	//Comments.
	if (Peek() == '#')
	{
		while (!mpcParser->IsOutside() && (Peek() != '\n'))
		{
			mpcParser->StepRight();
		}
		psDest->eKind = TT_Comment;
		psDest->uiEnd = Offset();
		*pbDirective = true;
		return true;
	}
	if (Peek() == '*')
	{
		mpcParser->StepRight();
		if (mpcParser->FindExactCharacterSequence("*#") != TRITRUE)
		{
			return Error(uiStart, "#* has no *#");
		}
		mpcParser->StepRight();
		mpcParser->StepRight();
		psDest->eKind = TT_Comment;
		psDest->uiEnd = Offset();
		*pbDirective = true;
		return true;
	}

	//#word or #{word}.
	bBraced = (mpcParser->GetExactCharacter('{', false) == TRITRUE);
	eDirective = TD_None;
	for (i = 0; (eDirective == TD_None) && (i < sizeof(aszWords) / sizeof(char*)); i++)
	{
		if (IsWord(aszWords[i]))
		{
			eDirective = aeWords[i];
		}
	}
	if ((eDirective == TD_None) || (bBraced && (mpcParser->GetExactCharacter('}', false) != TRITRUE)))
	{
		return true;
	}
	psDest->eKind = TT_Directive;
	psDest->eDirective = eDirective;
	if ((eDirective == TD_Else) || (eDirective == TD_End) || (eDirective == TD_Break))
	{
		psDest->uiEnd = Offset();
		*pbDirective = true;
		return true;
	}

	//The others take brackets; without them it is text, as Velocity has it.
	while ((Peek() == ' ') || (Peek() == '\t'))
	{
		mpcParser->StepRight();
	}
	if (mpcParser->GetExactCharacter('(', false) != TRITRUE)
	{
		return true;
	}
	SkipSpace();
	if ((eDirective == TD_Set) || (eDirective == TD_Foreach))
	{
		//$name = value, $name in list.
		if ((mpcParser->GetExactCharacter('$', false) != TRITRUE) || !ReadName(&psDest->uiName))
		{
			return Error(Offset(), (eDirective == TD_Set) ? "#set needs a variable: #set($name = value)" : "#foreach needs a variable: #foreach($item in $list)");
		}
		SkipSpace();
		if (eDirective == TD_Set)
		{
			if (mpcParser->GetExactCharacter('=', false) != TRITRUE)
			{
				return Error(Offset(), "#set needs an =: #set($name = value)");
			}
		}
		else if (!IsWord("in"))
		{
			return Error(Offset(), "#foreach needs an in: #foreach($item in $list)");
		}
		if (!ParseExpression(&psDest->uiValue))
		{
			return false;
		}
	}
	else if (!ParseExpression(&psDest->uiExpression))
	{
		return false;
	}
	SkipSpace();
	if (mpcParser->GetExactCharacter(')', false) != TRITRUE)
	{
		return Error(Offset(), "A directive's ( has no )");
	}
	psDest->uiEnd = Offset();
	*pbDirective = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Velocity 2's "lines": a line that has nothing but directives, comments
// and white space on it is taken out of the output, its end of line too.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::RemoveDirectiveLines(CArrayTemplateTokens* pasTokens)
{
	STemplateToken*	psToken;
	const char*		sz;
	size			uiFirst;
	size			uiLast;
	size			i;
	size			j;
	bool			bDirectives;
	bool			bOther;
	bool			bLineEnd;

	sz = mszSource.Text();
	uiFirst = 0;
	while (uiFirst < pasTokens->NumElements())
	{
		//The tokens of one line: to a text that ends its line, or the last.
		bDirectives = false;
		bOther = false;
		bLineEnd = false;
		for (uiLast = uiFirst; uiLast < pasTokens->NumElements(); uiLast++)
		{
			psToken = pasTokens->Get(uiLast);
			if ((psToken->eKind == TT_Directive) || (psToken->eKind == TT_Comment))
			{
				bDirectives = true;
			}
			else if (psToken->eKind == TT_Reference)
			{
				bOther = true;
			}
			else
			{
				for (j = psToken->uiStart; j < psToken->uiEnd; j++)
				{
					bOther = bOther || ((sz[j] != ' ') && (sz[j] != '\t') && (sz[j] != '\r') && (sz[j] != '\n'));
				}
				bLineEnd = (psToken->uiEnd > psToken->uiStart) && (sz[psToken->uiEnd - 1] == '\n');
			}
			if (bLineEnd)
			{
				break;
			}
		}
		if (uiLast == pasTokens->NumElements())
		{
			uiLast--;
		}
		if (bDirectives && !bOther)
		{
			for (i = uiFirst; i <= uiLast; i++)
			{
				psToken = pasTokens->Get(i);
				if (psToken->eKind == TT_Text)
				{
					psToken->bKeep = false;
				}
			}
		}
		uiFirst = uiLast + 1;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The tokens from *puiToken made into a list of nodes, to the end or to
// an #elseif, #else or #end (*peStop, and its token), which ends the
// block it is in.  iDepth is how many blocks the list is in.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::Build(CArrayTemplateTokens* pasTokens, size* puiToken, int iDepth, size* puiFirst, ETemplateDirective* peStop, size* puiStopToken)
{
	STemplateToken*		psToken;
	STemplateToken*		psBranch;
	ETemplateDirective	eStop;
	size				uiNode;
	size				uiLast;
	size				uiBranch;
	size				uiOpen;
	size				uiStopToken;
	size				uiChildren;

	*puiFirst = TEMPLATE_NONE;
	*peStop = TD_None;
	uiLast = TEMPLATE_NONE;
	while (*puiToken < pasTokens->NumElements())
	{
		psToken = pasTokens->Get(*puiToken);
		uiNode = TEMPLATE_NONE;
		if ((psToken->eKind == TT_Text) && psToken->bKeep)
		{
			uiNode = AddNode(TN_Text, psToken->uiStart, psToken->uiEnd);
		}
		else if (psToken->eKind == TT_Reference)
		{
			uiNode = AddNode(TN_Reference, psToken->uiStart, psToken->uiEnd);
			masNodes.Get(uiNode)->uiExpression = psToken->uiExpression;
		}
		else if (psToken->eKind == TT_Directive)
		{
			switch (psToken->eDirective)
			{
			case TD_ElseIf:
			case TD_Else:
			case TD_End:
				if (iDepth == 0)
				{
					return Error(psToken->uiStart, (psToken->eDirective == TD_End) ? "#end has no #if or #foreach" : "#elseif and #else need an #if");
				}
				*peStop = psToken->eDirective;
				*puiStopToken = *puiToken;
				(*puiToken)++;
				return true;
			case TD_Set:
				uiNode = AddNode(TN_Set, psToken->uiStart, psToken->uiEnd);
				masNodes.Get(uiNode)->uiName = psToken->uiName;
				masNodes.Get(uiNode)->uiValue = psToken->uiValue;
				break;
			case TD_Parse:
			case TD_Include:
				uiNode = AddNode((psToken->eDirective == TD_Parse) ? TN_Parse : TN_Include, psToken->uiStart, psToken->uiEnd);
				masNodes.Get(uiNode)->uiExpression = psToken->uiExpression;
				break;
			case TD_Break:
				uiNode = AddNode(TN_Break, psToken->uiStart, psToken->uiEnd);
				break;
			case TD_Foreach:
				uiOpen = psToken->uiStart;
				uiNode = AddNode(TN_Foreach, psToken->uiStart, psToken->uiEnd);
				masNodes.Get(uiNode)->uiName = psToken->uiName;
				masNodes.Get(uiNode)->uiValue = psToken->uiValue;
				(*puiToken)++;
				if (!Build(pasTokens, puiToken, iDepth + 1, &uiChildren, &eStop, &uiStopToken))
				{
					return false;
				}
				masNodes.Get(uiNode)->uiFirst = uiChildren;
				if (eStop != TD_End)
				{
					return Error((eStop == TD_None) ? uiOpen : pasTokens->Get(uiStopToken)->uiStart, (eStop == TD_None) ? "#foreach has no #end" : "#foreach has no #elseif or #else");
				}
				(*puiToken)--;
				break;
			case TD_If:
				uiOpen = psToken->uiStart;
				uiNode = AddNode(TN_If, psToken->uiStart, psToken->uiEnd);
				masNodes.Get(uiNode)->uiExpression = psToken->uiExpression;
				uiBranch = uiNode;
				(*puiToken)++;
				for (;;)
				{
					if (!Build(pasTokens, puiToken, iDepth + 1, &uiChildren, &eStop, &uiStopToken))
					{
						return false;
					}
					masNodes.Get(uiBranch)->uiFirst = uiChildren;
					if (eStop == TD_None)
					{
						return Error(uiOpen, "#if has no #end");
					}
					if (eStop == TD_End)
					{
						break;
					}
					if (masNodes.Get(uiBranch)->eKind == TN_Else)
					{
						return Error(pasTokens->Get(uiStopToken)->uiStart, "Nothing but #end can come after #else");
					}
					psBranch = pasTokens->Get(uiStopToken);
					uiChildren = AddNode((eStop == TD_ElseIf) ? TN_If : TN_Else, psBranch->uiStart, psBranch->uiEnd);
					masNodes.Get(uiChildren)->uiExpression = psBranch->uiExpression;
					masNodes.Get(uiBranch)->uiElse = uiChildren;
					uiBranch = uiChildren;
				}
				(*puiToken)--;
				break;
			default:
				break;
			}
		}
		if (uiNode != TEMPLATE_NONE)
		{
			if (uiLast == TEMPLATE_NONE)
			{
				*puiFirst = uiNode;
			}
			else
			{
				masNodes.Get(uiLast)->uiNext = uiNode;
			}
			uiLast = uiNode;
		}
		(*puiToken)++;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A reference at the parser (its '$'), the parser after it: $name,
// ${name}, $!name, $!{name}, each with methods called on it
// (.name(...)).  Outside braces in text a .name with no brackets ends it
// (it is text), but for $foreach's; in an expression it is read.  False
// if what is there isn't a reference (a $ of nothing), or for an error
// (GetError says).
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseReference(bool bExpression, size* puiExpression)
{
	size	uiStart;
	size	uiName;
	size	uiExpression;
	size	uiCall;
	size	uiRoot;
	size	uiFirst;
	bool	bQuiet;
	bool	bBraced;
	bool	bForeach;

	uiStart = Offset();
	if (mpcParser->GetExactCharacter('$', false) != TRITRUE)
	{
		return false;
	}
	bQuiet = (mpcParser->GetExactCharacter('!', false) == TRITRUE);
	bBraced = (mpcParser->GetExactCharacter('{', false) == TRITRUE);
	if (!ReadName(&uiName))
	{
		return false;
	}
	uiExpression = AddExpression(TE_Reference, uiStart);
	masExpressions.Get(uiExpression)->uiName = uiName;
	masExpressions.Get(uiExpression)->bQuiet = bQuiet;
	uiRoot = uiExpression;
	bForeach = (strcmp(GetName(uiName), "foreach") == 0);

	//.name and .name(...).
	while (Peek() == '.')
	{
		mpcParser->PushPosition();
		mpcParser->StepRight();
		if (!ReadName(&uiName))
		{
			mpcParser->PopPosition();
			break;
		}
		if (mpcParser->GetExactCharacter('(', false) == TRITRUE)
		{
			mpcParser->PassPosition();
			uiCall = AddExpression(TE_Call, uiStart);
			masExpressions.Get(uiCall)->uiLeft = uiExpression;
			masExpressions.Get(uiCall)->uiName = uiName;
			if (!ParseArguments(')', &uiFirst))
			{
				return false;
			}
			masExpressions.Get(uiCall)->uiFirst = uiFirst;
			uiExpression = uiCall;
		}
		else if (bBraced || bExpression || bForeach)
		{
			mpcParser->PassPosition();
			uiCall = AddExpression(TE_Property, uiStart);
			masExpressions.Get(uiCall)->uiLeft = uiExpression;
			masExpressions.Get(uiCall)->uiName = uiName;
			uiExpression = uiCall;
		}
		else
		{
			mpcParser->PopPosition();
			break;
		}
	}
	if (bBraced && (mpcParser->GetExactCharacter('}', false) != TRITRUE))
	{
		return Error(uiStart, "${ has no }");
	}
	masExpressions.Get(uiExpression)->uiEnd = Offset();
	masExpressions.Get(uiRoot)->uiEnd = Offset();
	*puiExpression = uiExpression;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// Expressions at the parser: ||, then &&, then !, then the comparisons,
// then + and -.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseExpression(size* puiExpression)
{
	SkipSpace();
	return ParseOr(puiExpression);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseOr(size* puiExpression)
{
	size	uiRight;
	size	uiNode;

	if (!ParseAnd(puiExpression))
	{
		return false;
	}
	for (;;)
	{
		SkipSpace();
		if ((mpcParser->GetExactCharacterSequence("||", false) != TRITRUE) && !IsWord("or"))
		{
			return true;
		}
		uiNode = AddExpression(TE_Or, Offset());
		if (!ParseAnd(&uiRight))
		{
			return false;
		}
		masExpressions.Get(uiNode)->uiLeft = *puiExpression;
		masExpressions.Get(uiNode)->uiRight = uiRight;
		*puiExpression = uiNode;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseAnd(size* puiExpression)
{
	size	uiRight;
	size	uiNode;

	if (!ParseNot(puiExpression))
	{
		return false;
	}
	for (;;)
	{
		SkipSpace();
		if ((mpcParser->GetExactCharacterSequence("&&", false) != TRITRUE) && !IsWord("and"))
		{
			return true;
		}
		uiNode = AddExpression(TE_And, Offset());
		if (!ParseNot(&uiRight))
		{
			return false;
		}
		masExpressions.Get(uiNode)->uiLeft = *puiExpression;
		masExpressions.Get(uiNode)->uiRight = uiRight;
		*puiExpression = uiNode;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseNot(size* puiExpression)
{
	size	uiNode;
	size	uiOperand;

	SkipSpace();
	if (((Peek() == '!') && (Peek(1) != '=')) || IsWord("not"))
	{
		if (Peek() == '!')
		{
			mpcParser->StepRight();
		}
		uiNode = AddExpression(TE_Not, Offset());
		if (!ParseNot(&uiOperand))
		{
			return false;
		}
		masExpressions.Get(uiNode)->uiLeft = uiOperand;
		*puiExpression = uiNode;
		return true;
	}
	return ParseCompare(puiExpression);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseCompare(size* puiExpression)
{
	static const char*					aszOperators[] = { "==", "!=", "<=", ">=", "<", ">" };
	static const char*					aszWords[] = { "eq", "ne", "le", "ge", "lt", "gt" };
	static const ETemplateExpression	aeKinds[] = { TE_Equal, TE_NotEqual, TE_LessEqual, TE_GreaterEqual, TE_Less, TE_Greater };
	size								uiRight;
	size								uiNode;
	size								i;

	if (!ParseAdditive(puiExpression))
	{
		return false;
	}
	SkipSpace();
	for (i = 0; i < sizeof(aszOperators) / sizeof(char*); i++)
	{
		if ((!mpcParser->IsOutside() && (mpcParser->GetExactCharacterSequence(aszOperators[i], false) == TRITRUE)) || IsWord(aszWords[i]))
		{
			uiNode = AddExpression(aeKinds[i], Offset());
			if (!ParseAdditive(&uiRight))
			{
				return false;
			}
			masExpressions.Get(uiNode)->uiLeft = *puiExpression;
			masExpressions.Get(uiNode)->uiRight = uiRight;
			*puiExpression = uiNode;
			return true;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseAdditive(size* puiExpression)
{
	size				uiRight;
	size				uiNode;
	ETemplateExpression	eKind;

	if (!ParsePrimary(puiExpression))
	{
		return false;
	}
	for (;;)
	{
		SkipSpace();
		if ((Peek() != '+') && (Peek() != '-'))
		{
			return true;
		}
		eKind = (Peek() == '+') ? TE_Add : TE_Subtract;
		mpcParser->StepRight();
		uiNode = AddExpression(eKind, Offset());
		if (!ParsePrimary(&uiRight))
		{
			return false;
		}
		masExpressions.Get(uiNode)->uiLeft = *puiExpression;
		masExpressions.Get(uiNode)->uiRight = uiRight;
		*puiExpression = uiNode;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A value: ( ), a string, a number, true, false, a list, a reference.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParsePrimary(size* puiExpression)
{
	const char*	szStart;
	size		uiStart;
	size		uiFirst;
	int64		lliNumber;
	char		c;

	SkipSpace();
	uiStart = Offset();
	c = Peek();
	if (mpcParser->IsOutside())
	{
		return Error(uiStart, "A value is needed");
	}
	if (c == '(')
	{
		mpcParser->StepRight();
		if (!ParseExpression(puiExpression))
		{
			return false;
		}
		SkipSpace();
		if (mpcParser->GetExactCharacter(')', false) != TRITRUE)
		{
			return Error(Offset(), "( has no )");
		}
		return true;
	}
	if (c == '"')
	{
		return ParseString(puiExpression);
	}
	if (c == '\'')
	{
		mpcParser->StepRight();
		szStart = mpcParser->Position();
		while (!mpcParser->IsOutside() && (Peek() != '\''))
		{
			mpcParser->StepRight();
		}
		if (mpcParser->IsOutside())
		{
			return Error(uiStart, "' has no '");
		}
		*puiExpression = AddExpression(TE_Literal, uiStart);
		masExpressions.Get(*puiExpression)->uiName = AddName(szStart, (size)(mpcParser->Position() - szStart));
		mpcParser->StepRight();
		return true;
	}
	if (((c >= '0') && (c <= '9')) || ((c == '-') && (Peek(1) >= '0') && (Peek(1) <= '9')))
	{
		if (mpcParser->GetInteger(&lliNumber, NULL, false) != TRITRUE)
		{
			return Error(uiStart, "The number can't be read");
		}
		*puiExpression = AddExpression(TE_Number, uiStart);
		masExpressions.Get(*puiExpression)->lliNumber = lliNumber;
		return true;
	}
	if (c == '[')
	{
		mpcParser->StepRight();
		*puiExpression = AddExpression(TE_List, uiStart);
		if (!ParseArguments(']', &uiFirst))
		{
			return false;
		}
		masExpressions.Get(*puiExpression)->uiFirst = uiFirst;
		return true;
	}
	if (IsWord("true"))
	{
		*puiExpression = AddExpression(TE_True, uiStart);
		return true;
	}
	if (IsWord("false"))
	{
		*puiExpression = AddExpression(TE_False, uiStart);
		return true;
	}
	if ((c == '$') && ParseReference(true, puiExpression))
	{
		return true;
	}
	return Error(uiStart, "A value is needed: a string, a number, true, false, a list or a $reference");
}


//////////////////////////////////////////////////////////////////////////
//
// Expressions separated by commas to cClose, at the parser (after the
// opening bracket): a call's arguments, a list's items.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseArguments(char cClose, size* puiFirst)
{
	size	uiLast;
	size	uiItem;

	*puiFirst = TEMPLATE_NONE;
	uiLast = TEMPLATE_NONE;
	SkipSpace();
	if (mpcParser->GetExactCharacter(cClose, false) == TRITRUE)
	{
		return true;
	}
	for (;;)
	{
		if (!ParseExpression(&uiItem))
		{
			return false;
		}
		if (uiLast == TEMPLATE_NONE)
		{
			*puiFirst = uiItem;
		}
		else
		{
			masExpressions.Get(uiLast)->uiNext = uiItem;
		}
		uiLast = uiItem;
		SkipSpace();
		if (mpcParser->GetExactCharacter(',', false) == TRITRUE)
		{
			continue;
		}
		if (mpcParser->GetExactCharacter(cClose, false) == TRITRUE)
		{
			return true;
		}
		return Error(Offset(), (cClose == ')') ? "( has no )" : "[ has no ]");
	}
}


//////////////////////////////////////////////////////////////////////////
//
// "...": its text as a template of its own (read by a parser of its own),
// references in it.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::ParseString(size* puiExpression)
{
	CArrayTemplateTokens	asTokens;
	ETemplateDirective		eStop;
	size					uiStart;
	size					uiContent;
	size					uiClose;
	size					uiToken;
	size					uiStopToken;
	size					uiFirst;
	bool					bResult;

	uiStart = Offset();
	mpcParser->StepRight();
	uiContent = Offset();
	while (!mpcParser->IsOutside() && (Peek() != '"'))
	{
		if ((Peek() == '\\') && ((Peek(1) == '"') || (Peek(1) == '\\')))
		{
			mpcParser->StepRight();
		}
		mpcParser->StepRight();
	}
	if (mpcParser->IsOutside())
	{
		return Error(uiStart, "\" has no \"");
	}
	uiClose = Offset();
	mpcParser->StepRight();

	*puiExpression = AddExpression(TE_String, uiStart);
	asTokens.Init();
	bResult = Lex(uiContent, uiClose, true, &asTokens);
	uiToken = 0;
	uiFirst = TEMPLATE_NONE;
	bResult = bResult && Build(&asTokens, &uiToken, 0, &uiFirst, &eStop, &uiStopToken);
	asTokens.Kill();
	masExpressions.Get(*puiExpression)->uiFirst = uiFirst;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The template applied with its values, appended to pszDest.  False, with
// the error, if it can't be (it wasn't parsed, a method that isn't one,
// an include there is none of).
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::Evaluate(CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest)
{
	bool	bBreak;

	if (!mbParsed)
	{
		if (mszError.Empty())
		{
			mszError.Set("The template hasn't been parsed");
		}
		return false;
	}
	mszError.Clear();
	bBreak = false;
	return EvaluateList(muiRoot, pcValues, pcIncludes, pszDest, &bBreak);
}


//////////////////////////////////////////////////////////////////////////
//
// A list of nodes, to its end or a #break (*pbBreak).
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::EvaluateList(size uiFirst, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest, bool* pbBreak)
{
	size	uiNode;

	for (uiNode = uiFirst; (uiNode != TEMPLATE_NONE) && !*pbBreak; uiNode = masNodes.Get(uiNode)->uiNext)
	{
		if (!EvaluateNode(uiNode, pcValues, pcIncludes, pszDest, pbBreak))
		{
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::EvaluateNode(size uiNode, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest, bool* pbBreak)
{
	STemplateNode*			psNode;
	STemplateNode*			psBranch;
	STemplateExpression*	psRoot;
	STemplateValue			sValue;
	size					uiBranch;
	bool					bResult;
	bool					bTrue;

	psNode = masNodes.Get(uiNode);
	bResult = true;
	sValue.Init();
	switch (psNode->eKind)
	{
	case TN_Text:
		pszDest->Append(&mszSource.Text()[psNode->uiStart], psNode->uiEnd - psNode->uiStart);
		break;
	case TN_Reference:
		bResult = EvaluateExpression(psNode->uiExpression, pcValues, pcIncludes, &sValue);
		if (bResult)
		{
			if (!sValue.IsNull())
			{
				sValue.AppendText(pszDest);
			}
			else
			{
				//$!'s quiet is the reference's, at the bottom of its calls.
				psRoot = masExpressions.Get(psNode->uiExpression);
				while (psRoot->uiLeft != TEMPLATE_NONE)
				{
					psRoot = masExpressions.Get(psRoot->uiLeft);
				}
				if (!psRoot->bQuiet)
				{
					pszDest->Append(&mszSource.Text()[psNode->uiStart], psNode->uiEnd - psNode->uiStart);
				}
			}
		}
		break;
	case TN_Set:
		bResult = EvaluateExpression(psNode->uiValue, pcValues, pcIncludes, &sValue);
		if (bResult)
		{
			pcValues->Set(GetName(psNode->uiName), &sValue);
		}
		break;
	case TN_If:
		for (uiBranch = uiNode; bResult && (uiBranch != TEMPLATE_NONE); uiBranch = psBranch->uiElse)
		{
			psBranch = masNodes.Get(uiBranch);
			bTrue = true;
			if (psBranch->eKind == TN_If)
			{
				bResult = EvaluateExpression(psBranch->uiExpression, pcValues, pcIncludes, &sValue);
				bTrue = sValue.IsTrue();
			}
			if (bResult && bTrue)
			{
				bResult = EvaluateList(psBranch->uiFirst, pcValues, pcIncludes, pszDest, pbBreak);
				break;
			}
		}
		break;
	case TN_Foreach:
		bResult = EvaluateForeach(psNode, pcValues, pcIncludes, pszDest);
		break;
	case TN_Parse:
	case TN_Include:
		bResult = EvaluateParse(psNode, pcValues, pcIncludes, pszDest);
		break;
	case TN_Break:
		*pbBreak = true;
		break;
	default:
		break;
	}
	sValue.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// #foreach($item in $list): its nodes for each of the list's items, the
// item in $item and $foreach's own (index from 0, count from 1, hasNext,
// first, last) pushed over any of their names, taken off at the end.  A
// value that isn't a list goes round no times, as Velocity's null does.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::EvaluateForeach(STemplateNode* psNode, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest)
{
	STemplateValue	sList;
	STemplateValue	sItem;
	size			uiMark;
	size			uiNumItems;
	size			i;
	size			uiFirst;
	size			uiName;
	bool			bResult;
	bool			bBreak;

	uiFirst = psNode->uiFirst;
	uiName = psNode->uiName;
	sList.Init();
	sItem.Init();
	bResult = EvaluateExpression(psNode->uiValue, pcValues, pcIncludes, &sList);
	uiNumItems = (sList.eKind == TV_List) ? sList.aszList.NumElements() : 0;
	if (bResult && (uiNumItems > TEMPLATE_MAXIMUM_FOREACH))
	{
		bResult = Error(psNode->uiStart, "#foreach's list has more than 10,000 items");
	}
	bBreak = false;
	for (i = 0; bResult && !bBreak && (i < uiNumItems); i++)
	{
		uiMark = pcValues->Mark();
		sItem.SetString(sList.aszList.GetText(i));
		pcValues->Push(GetName(uiName), &sItem);
		sItem.SetNumber((int64)i);
		pcValues->Push("foreach.index", &sItem);
		sItem.SetNumber((int64)i + 1);
		pcValues->Push("foreach.count", &sItem);
		sItem.SetBool(i + 1 < uiNumItems);
		pcValues->Push("foreach.hasNext", &sItem);
		sItem.SetBool(i == 0);
		pcValues->Push("foreach.first", &sItem);
		sItem.SetBool(i + 1 == uiNumItems);
		pcValues->Push("foreach.last", &sItem);
		bResult = EvaluateList(uiFirst, pcValues, pcIncludes, pszDest, &bBreak);
		pcValues->Release(uiMark);
	}
	sItem.Kill();
	sList.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// #parse("Name"): the include applied as a template, with these values
// (its #set sets them, as Velocity's does); #include("Name"): the include
// as it is written.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::EvaluateParse(STemplateNode* psNode, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest)
{
	CTemplateText	cInclude;
	STemplateValue	sName;
	CChars			szText;
	CChars			szName;
	CChars			szMessage;
	size			uiStart;
	bool			bResult;
	bool			bParse;

	uiStart = psNode->uiStart;
	bParse = (psNode->eKind == TN_Parse);
	sName.Init();
	szText.Init();
	szName.Init();
	szMessage.Init();
	bResult = EvaluateExpression(psNode->uiExpression, pcValues, pcIncludes, &sName);
	if (bResult)
	{
		sName.AppendText(&szName);
		if (!pcIncludes || !pcIncludes->GetInclude(szName.Text(), &szText))
		{
			szMessage.Append("There is no include called \"")->Append(&szName)->Append("\"");
			bResult = Error(uiStart, szMessage.Text());
		}
		else if (!bParse)
		{
			pszDest->Append(&szText);
		}
		else if (miDepth + 1 > TEMPLATE_MAXIMUM_PARSE_DEPTH)
		{
			szMessage.Append("\"")->Append(&szName)->Append("\" is #parsed inside itself, or inside more than 20 includes");
			bResult = Error(uiStart, szMessage.Text());
		}
		else
		{
			cInclude.Init();
			cInclude.miDepth = miDepth + 1;
			bResult = cInclude.Parse(szText.Text()) && cInclude.Evaluate(pcValues, pcIncludes, pszDest);
			if (!bResult)
			{
				szMessage.Append("In \"")->Append(&szName)->Append("\": ")->Append(cInclude.GetError());
				Error(uiStart, szMessage.Text());
			}
			cInclude.Kill();
		}
	}
	szMessage.Kill();
	szName.Kill();
	szText.Kill();
	sName.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// An expression's value, into psDest (Init'ed by the caller).  A
// reference with no value is null (TV_Null), as is a method called on
// one.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::EvaluateExpression(size uiExpression, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, STemplateValue* psDest)
{
	STemplateExpression*	psExpression;
	STemplateExpression*	psLeft;
	STemplateValue*			psFound;
	STemplateValue			sLeft;
	STemplateValue			sRight;
	CArrayChars				aszItems;
	CChars					szText;
	ETemplateExpression		eKind;
	size					uiItem;
	size					uiLeft;
	size					uiRight;
	size					uiName;
	size					uiStart;
	int64					lliLeft;
	int64					lliRight;
	bool					bResult;
	bool					bBreak;

	psExpression = masExpressions.Get(uiExpression);
	eKind = psExpression->eKind;
	uiLeft = psExpression->uiLeft;
	uiRight = psExpression->uiRight;
	uiName = psExpression->uiName;
	uiStart = psExpression->uiStart;
	bResult = true;
	switch (eKind)
	{
	case TE_String:
		szText.Init();
		bBreak = false;
		bResult = EvaluateList(psExpression->uiFirst, pcValues, pcIncludes, &szText, &bBreak);
		psDest->SetString(szText.Text());
		szText.Kill();
		return bResult;
	case TE_Literal:
		psDest->SetString(GetName(uiName));
		return true;
	case TE_Number:
		psDest->SetNumber(psExpression->lliNumber);
		return true;
	case TE_True:
	case TE_False:
		psDest->SetBool(eKind == TE_True);
		return true;
	case TE_List:
		aszItems.Init();
		sLeft.Init();
		szText.Init();
		for (uiItem = psExpression->uiFirst; bResult && (uiItem != TEMPLATE_NONE); uiItem = masExpressions.Get(uiItem)->uiNext)
		{
			sLeft.SetNull();
			bResult = EvaluateExpression(uiItem, pcValues, pcIncludes, &sLeft);
			szText.Clear();
			sLeft.AppendText(&szText);
			aszItems.Add(szText.Text());
		}
		psDest->SetList(&aszItems);
		szText.Kill();
		sLeft.Kill();
		aszItems.Kill();
		return bResult;
	case TE_Reference:
		psFound = pcValues->Get(GetName(uiName));
		if (psFound)
		{
			psDest->Copy(psFound);
		}
		else
		{
			psDest->SetNull();
		}
		return true;
	case TE_Property:
		//$a.b with no brackets is the variable called "a.b" ($foreach.count is one).
		psLeft = masExpressions.Get(uiLeft);
		psDest->SetNull();
		if (psLeft->eKind == TE_Reference)
		{
			szText.Init();
			szText.Append(GetName(psLeft->uiName))->Append('.')->Append(GetName(uiName));
			psFound = pcValues->Get(szText.Text());
			if (psFound)
			{
				psDest->Copy(psFound);
			}
			szText.Kill();
		}
		return true;
	case TE_Call:
		return EvaluateCall(psExpression, pcValues, pcIncludes, psDest);
	default:
		break;
	}

	//Operators.
	sLeft.Init();
	sRight.Init();
	bResult = EvaluateExpression(uiLeft, pcValues, pcIncludes, &sLeft);
	if (bResult && (eKind == TE_Not))
	{
		psDest->SetBool(!sLeft.IsTrue());
	}
	else if (bResult && ((eKind == TE_And) || (eKind == TE_Or)))
	{
		//Only as much as is needed, as C's.
		if ((eKind == TE_And) ? !sLeft.IsTrue() : sLeft.IsTrue())
		{
			psDest->SetBool(eKind == TE_Or);
		}
		else
		{
			bResult = EvaluateExpression(uiRight, pcValues, pcIncludes, &sRight);
			psDest->SetBool(sRight.IsTrue());
		}
	}
	else if (bResult)
	{
		bResult = EvaluateExpression(uiRight, pcValues, pcIncludes, &sRight);
		if (bResult && ((eKind == TE_Add) || (eKind == TE_Subtract)))
		{
			if (sLeft.GetNumber(&lliLeft) && sRight.GetNumber(&lliRight))
			{
				psDest->SetNumber((eKind == TE_Add) ? lliLeft + lliRight : lliLeft - lliRight);
			}
			else if (eKind == TE_Add)
			{
				//Strings are joined.
				szText.Init();
				sLeft.AppendText(&szText);
				sRight.AppendText(&szText);
				psDest->SetString(szText.Text());
				szText.Kill();
			}
			else
			{
				bResult = Error(uiStart, "- needs two numbers");
			}
		}
		else if (bResult)
		{
			bResult = Compare(psExpression, &sLeft, &sRight, psDest);
		}
	}
	sRight.Kill();
	sLeft.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// ==, !=, <, <=, > and >=: as numbers if both are, otherwise as their
// text.  A null is equal only to a null.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::Compare(STemplateExpression* psExpression, STemplateValue* psLeft, STemplateValue* psRight, STemplateValue* psDest)
{
	CChars	szLeft;
	CChars	szRight;
	int64	lliLeft;
	int64	lliRight;
	int		iCompare;

	if (psLeft->IsNull() || psRight->IsNull())
	{
		iCompare = (psLeft->IsNull() && psRight->IsNull()) ? 0 : 1;
		if ((psExpression->eKind != TE_Equal) && (psExpression->eKind != TE_NotEqual))
		{
			return Error(psExpression->uiStart, "A value with nothing in it can't be compared but with == and !=");
		}
	}
	else if (psLeft->GetNumber(&lliLeft) && psRight->GetNumber(&lliRight))
	{
		iCompare = (lliLeft < lliRight) ? -1 : ((lliLeft > lliRight) ? 1 : 0);
	}
	else if ((psLeft->eKind == TV_Bool) && (psRight->eKind == TV_Bool))
	{
		iCompare = (psLeft->bValue == psRight->bValue) ? 0 : 1;
	}
	else
	{
		szLeft.Init();
		szRight.Init();
		psLeft->AppendText(&szLeft);
		psRight->AppendText(&szRight);
		iCompare = strcmp(szLeft.Text(), szRight.Text());
		szRight.Kill();
		szLeft.Kill();
	}

	switch (psExpression->eKind)
	{
	case TE_Equal:
		psDest->SetBool(iCompare == 0);
		break;
	case TE_NotEqual:
		psDest->SetBool(iCompare != 0);
		break;
	case TE_Less:
		psDest->SetBool(iCompare < 0);
		break;
	case TE_LessEqual:
		psDest->SetBool(iCompare <= 0);
		break;
	case TE_Greater:
		psDest->SetBool(iCompare > 0);
		break;
	case TE_GreaterEqual:
		psDest->SetBool(iCompare >= 0);
		break;
	default:
		break;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// A method called on a value: Java's String's (toUpperCase(),
// substring(i, j), ...), the IDE's (toUpperSnakeCase(), toLowerSnakeCase(),
// withoutPrefix()), and on a list size(), isEmpty(), get(i) and
// contains(s).  A method called on null is null; one that isn't one, or
// with the wrong arguments, is an error.
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateText::EvaluateCall(STemplateExpression* psCall, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, STemplateValue* psDest)
{
	static const char*	aszMethods[] = { "toUpperCase", "toLowerCase", "trim", "length", "isEmpty", "substring", "replace", "startsWith", "endsWith", "contains", "equals", "equalsIgnoreCase", "indexOf", "lastIndexOf", "toUpperSnakeCase", "toLowerSnakeCase", "withoutPrefix", "size", "get" };
	static const int	aiMinimum[] = { 0, 0, 0, 0, 0, 1, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1 };
	static const int	aiMaximum[] = { 0, 0, 0, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1 };
	STemplateValue		asArguments[TEMPLATE_MAXIMUM_ARGUMENTS];
	STemplateValue		sLeft;
	CChars				szText;
	CChars				aszArguments[TEMPLATE_MAXIMUM_ARGUMENTS];
	CChars				szMessage;
	const char*			szMethod;
	size				uiArgument;
	size				uiStart;
	size				uiFound;
	size				uiLength;
	int					iNumArguments;
	int					iMethod;
	int					i;
	int64				lliBegin;
	int64				lliEnd;
	bool				bResult;
	bool				bList;

	szMethod = GetName(psCall->uiName);
	uiStart = psCall->uiStart;
	iMethod = -1;
	for (i = 0; i < (int)(sizeof(aszMethods) / sizeof(char*)); i++)
	{
		if (strcmp(aszMethods[i], szMethod) == 0)
		{
			iMethod = i;
		}
	}
	szMessage.Init();
	if (iMethod == -1)
	{
		szMessage.Append("There is no method called ")->Append(szMethod)->Append("()");
		bResult = Error(uiStart, szMessage.Text());
		szMessage.Kill();
		return bResult;
	}

	sLeft.Init();
	szText.Init();
	for (i = 0; i < TEMPLATE_MAXIMUM_ARGUMENTS; i++)
	{
		asArguments[i].Init();
		aszArguments[i].Init();
	}
	bResult = EvaluateExpression(psCall->uiLeft, pcValues, pcIncludes, &sLeft);
	iNumArguments = 0;
	for (uiArgument = psCall->uiFirst; bResult && (uiArgument != TEMPLATE_NONE); uiArgument = masExpressions.Get(uiArgument)->uiNext)
	{
		if (iNumArguments == TEMPLATE_MAXIMUM_ARGUMENTS)
		{
			iNumArguments++;
			break;
		}
		bResult = EvaluateExpression(uiArgument, pcValues, pcIncludes, &asArguments[iNumArguments]);
		asArguments[iNumArguments].AppendText(&aszArguments[iNumArguments]);
		iNumArguments++;
	}
	if (bResult && ((iNumArguments < aiMinimum[iMethod]) || (iNumArguments > aiMaximum[iMethod])))
	{
		szMessage.Append(szMethod)->Append("() takes ");
		if (aiMinimum[iMethod] != aiMaximum[iMethod])
		{
			szMessage.Append(aiMinimum[iMethod])->Append(" or ");
		}
		szMessage.Append(aiMaximum[iMethod])->Append((aiMaximum[iMethod] == 1) ? " argument" : " arguments");
		bResult = Error(uiStart, szMessage.Text());
	}

	bList = (sLeft.eKind == TV_List);
	if (!bResult || sLeft.IsNull())
	{
		psDest->SetNull();
	}
	else if (bList)
	{
		//A list's.
		if (strcmp(szMethod, "size") == 0)
		{
			psDest->SetNumber((int64)sLeft.aszList.NumElements());
		}
		else if (strcmp(szMethod, "isEmpty") == 0)
		{
			psDest->SetBool(sLeft.aszList.NumElements() == 0);
		}
		else if (strcmp(szMethod, "contains") == 0)
		{
			psDest->SetBool(sLeft.aszList.Contains(aszArguments[0].Text()));
		}
		else if (strcmp(szMethod, "get") == 0)
		{
			if (!asArguments[0].GetNumber(&lliBegin) || (lliBegin < 0) || (lliBegin >= (int64)sLeft.aszList.NumElements()))
			{
				bResult = Error(uiStart, "get()'s index isn't one of the list's");
			}
			else
			{
				psDest->SetString(sLeft.aszList.GetText((size)lliBegin));
			}
		}
		else
		{
			szMessage.Append("A list has no method called ")->Append(szMethod)->Append("()");
			bResult = Error(uiStart, szMessage.Text());
		}
	}
	else
	{
		//A string's (a number's or a bool's as its text).
		sLeft.AppendText(&szText);
		uiLength = szText.Length();
		if ((strcmp(szMethod, "toUpperCase") == 0) || (strcmp(szMethod, "toLowerCase") == 0) || (strcmp(szMethod, "trim") == 0))
		{
			if (szMethod[0] == 't' && szMethod[1] == 'r')
			{
				szText.StripWhitespace(true);
			}
			else if (szMethod[2] == 'U')
			{
				szText.UpperCase();
			}
			else
			{
				szText.LowerCase();
			}
			psDest->SetString(szText.Text());
		}
		else if (strcmp(szMethod, "length") == 0)
		{
			psDest->SetNumber((int64)uiLength);
		}
		else if (strcmp(szMethod, "isEmpty") == 0)
		{
			psDest->SetBool(uiLength == 0);
		}
		else if (strcmp(szMethod, "substring") == 0)
		{
			lliEnd = (int64)uiLength;
			if (!asArguments[0].GetNumber(&lliBegin) || ((iNumArguments == 2) && !asArguments[1].GetNumber(&lliEnd)))
			{
				bResult = Error(uiStart, "substring() takes numbers");
			}
			else if ((lliBegin < 0) || (lliEnd > (int64)uiLength) || (lliBegin > lliEnd))
			{
				bResult = Error(uiStart, "substring()'s begin and end aren't in the string");
			}
			else
			{
				psDest->SetString(&szText.Text()[lliBegin], (size)(lliEnd - lliBegin));
			}
		}
		else if (strcmp(szMethod, "replace") == 0)
		{
			if (!aszArguments[0].Empty())
			{
				szText.Replace(aszArguments[0].Text(), aszArguments[1].Text());
			}
			psDest->SetString(szText.Text());
		}
		else if (strcmp(szMethod, "startsWith") == 0)
		{
			psDest->SetBool(szText.StartsWith(aszArguments[0].Text()));
		}
		else if (strcmp(szMethod, "endsWith") == 0)
		{
			psDest->SetBool(szText.EndsWith(aszArguments[0].Text()));
		}
		else if (strcmp(szMethod, "contains") == 0)
		{
			psDest->SetBool(szText.Contains(aszArguments[0].Text()));
		}
		else if (strcmp(szMethod, "equals") == 0)
		{
			psDest->SetBool(szText.Equals(aszArguments[0].Text()));
		}
		else if (strcmp(szMethod, "equalsIgnoreCase") == 0)
		{
			psDest->SetBool(szText.EqualsIgnoreCase(aszArguments[0].Text()));
		}
		else if ((strcmp(szMethod, "indexOf") == 0) || (strcmp(szMethod, "lastIndexOf") == 0))
		{
			uiFound = (szMethod[0] == 'i') ? szText.Find(aszArguments[0].Text()) : szText.FindFromEnd(aszArguments[0].Text());
			psDest->SetNumber((uiFound > uiLength) ? -1 : (int64)uiFound);
		}
		else if (strcmp(szMethod, "toUpperSnakeCase") == 0)
		{
			szMessage.Clear();
			AppendTemplateUpperSnakeCase(szText.Text(), &szMessage);
			psDest->SetString(szMessage.Text());
		}
		else if (strcmp(szMethod, "toLowerSnakeCase") == 0)
		{
			szMessage.Clear();
			AppendTemplateLowerSnakeCase(szText.Text(), &szMessage);
			psDest->SetString(szMessage.Text());
		}
		else if (strcmp(szMethod, "withoutPrefix") == 0)
		{
			szMessage.Clear();
			AppendTemplateWithoutPrefix(szText.Text(), &szMessage);
			psDest->SetString(szMessage.Text());
		}
		else
		{
			szMessage.Append("A string has no method called ")->Append(szMethod)->Append("()");
			bResult = Error(uiStart, szMessage.Text());
		}
	}

	for (i = 0; i < TEMPLATE_MAXIMUM_ARGUMENTS; i++)
	{
		aszArguments[i].Kill();
		asArguments[i].Kill();
	}
	szText.Kill();
	sLeft.Kill();
	szMessage.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The variables the template uses that have no value, nor are set by its
// #set or #foreach, nor are $foreach's, in the order they are first used:
// those the New dialog asks for.  The includes it #parses by a name
// written in it are looked in too.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::FindMissing(CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CArrayChars* paszDest)
{
	CArrayChars	aszSet;

	aszSet.Init();
	FindMissing(pcValues, pcIncludes, &aszSet, paszDest);
	aszSet.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// FindMissing with the names set so far (an include's parent's #set's
// and #foreach's), which are added to.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateText::FindMissing(CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CArrayChars* paszSet, CArrayChars* paszDest)
{
	CTemplateText			cInclude;
	STemplateNode*			psNode;
	STemplateExpression*	psExpression;
	STemplateExpression*	psName;
	CChars					szText;
	CChars					szInclude;
	const char*				szName;
	size					i;

	if (!mbParsed)
	{
		return;
	}
	for (i = 0; i < masNodes.NumElements(); i++)
	{
		psNode = masNodes.Get(i);
		if (((psNode->eKind == TN_Set) || (psNode->eKind == TN_Foreach)) && !paszSet->Contains(GetName(psNode->uiName)))
		{
			paszSet->Add(GetName(psNode->uiName));
		}
	}
	for (i = 0; i < masExpressions.NumElements(); i++)
	{
		psExpression = masExpressions.Get(i);
		if (psExpression->eKind == TE_Reference)
		{
			szName = GetName(psExpression->uiName);
			if ((strcmp(szName, "foreach") != 0) && !pcValues->Has(szName) && !paszSet->Contains(szName) && !paszDest->Contains(szName))
			{
				paszDest->Add(szName);
			}
		}
	}

	//The includes #parse'd by a name written in the template.
	if (!pcIncludes || (miDepth + 1 > TEMPLATE_MAXIMUM_PARSE_DEPTH))
	{
		return;
	}
	for (i = 0; i < masNodes.NumElements(); i++)
	{
		psNode = masNodes.Get(i);
		if (psNode->eKind != TN_Parse)
		{
			continue;
		}
		psName = masExpressions.Get(psNode->uiExpression);
		szText.Init();
		if ((psName->eKind == TE_Literal) || ((psName->eKind == TE_String) && ((psName->uiFirst == TEMPLATE_NONE) || ((masNodes.Get(psName->uiFirst)->eKind == TN_Text) && (masNodes.Get(psName->uiFirst)->uiNext == TEMPLATE_NONE)))))
		{
			if (psName->eKind == TE_Literal)
			{
				szText.Append(GetName(psName->uiName));
			}
			else if (psName->uiFirst != TEMPLATE_NONE)
			{
				szText.Append(&mszSource.Text()[masNodes.Get(psName->uiFirst)->uiStart], masNodes.Get(psName->uiFirst)->uiEnd - masNodes.Get(psName->uiFirst)->uiStart);
			}
			cInclude.Init();
			cInclude.miDepth = miDepth + 1;
			szInclude.Init();
			if (pcIncludes->GetInclude(szText.Text(), &szInclude) && cInclude.Parse(szInclude.Text()))
			{
				cInclude.FindMissing(pcValues, pcIncludes, paszSet, paszDest);
			}
			szInclude.Kill();
			cInclude.Kill();
		}
		szText.Kill();
	}
}
