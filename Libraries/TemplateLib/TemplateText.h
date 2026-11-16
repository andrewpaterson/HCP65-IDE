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
#ifndef __TEMPLATE_TEXT_H__
#define __TEMPLATE_TEXT_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"
#include "BaseLib/TextParser.h"
#include "TemplateValues.h"


//No node or expression.
#define TEMPLATE_NONE					((size)-1)

//How many includes can #parse each other, one inside the next, and how many times one #foreach can go round.
#define TEMPLATE_MAXIMUM_PARSE_DEPTH	20
#define TEMPLATE_MAXIMUM_FOREACH		10000

//The most arguments a method takes (substring(i, j), replace(a, b)).
#define TEMPLATE_MAXIMUM_ARGUMENTS		2


//What #parse "File Header" and #include("Notice") find: whoever applies a template gives it.
class CTemplateIncludes
{
public:
	virtual bool	GetInclude(const char* szName, CChars* pszDest) =0;
};


enum ETemplateNode
{
	TN_Text,		//uiStart to uiEnd of the source, written as it is.
	TN_Reference,	//uiExpression's value; uiStart to uiEnd as it is written, for when it has none.
	TN_Set,			//#set($uiName = uiValue).
	TN_If,			//#if(uiExpression) or #elseif: uiFirst if it is true, otherwise uiElse, the next branch.
	TN_Else,		//#else: uiFirst.
	TN_Foreach,		//#foreach($uiName in uiValue): uiFirst for each.
	TN_Parse,		//#parse(uiExpression): the include, applied as a template.
	TN_Include,		//#include(uiExpression): the include as it is written.
	TN_Break,		//#break: out of the #foreach it is in.
};


//A part of a template: a list of them is a first and each one's next.
struct STemplateNode
{
	ETemplateNode	eKind;
	size			uiStart;
	size			uiEnd;
	size			uiExpression;
	size			uiValue;
	size			uiName;			//In the pool.
	size			uiFirst;
	size			uiNext;
	size			uiElse;
};


enum ETemplateExpression
{
	TE_String,			//"...": uiFirst is the nodes it is made of, references in it.
	TE_Literal,			//'...': uiName, as it is.
	TE_Number,			//lliNumber.
	TE_True,
	TE_False,
	TE_List,			//[a, b]: uiFirst and each one's uiNext.
	TE_Reference,		//$uiName; bQuiet for $!.
	TE_Property,		//uiLeft.uiName, with no brackets: $foreach.count.
	TE_Call,			//uiLeft.uiName(uiFirst, ...).
	TE_Not,				//!uiLeft.
	TE_And,
	TE_Or,
	TE_Equal,
	TE_NotEqual,
	TE_Less,
	TE_LessEqual,
	TE_Greater,
	TE_GreaterEqual,
	TE_Add,
	TE_Subtract,
};


struct STemplateExpression
{
	ETemplateExpression	eKind;
	size				uiName;
	size				uiLeft;
	size				uiRight;
	size				uiFirst;
	size				uiNext;
	int64				lliNumber;
	bool				bQuiet;
	size				uiStart;
	size				uiEnd;
};


enum ETemplateToken
{
	TT_Text,
	TT_Reference,
	TT_Directive,
	TT_Comment,
};


enum ETemplateDirective
{
	TD_None,
	TD_Set,
	TD_If,
	TD_ElseIf,
	TD_Else,
	TD_End,
	TD_Foreach,
	TD_Parse,
	TD_Include,
	TD_Break,
};


//A piece of the source as it is read, before the nodes are made of them.
struct STemplateToken
{
	ETemplateToken		eKind;
	ETemplateDirective	eDirective;
	size				uiStart;
	size				uiEnd;
	size				uiExpression;
	size				uiValue;
	size				uiName;
	bool				bKeep;			//A text's: false where its line is only directives (Velocity 2's "lines").
};


typedef CArrayTemplate<STemplateNode>		CArrayTemplateNodes;
typedef CArrayTemplate<STemplateExpression>	CArrayTemplateExpressions;
typedef CArrayTemplate<STemplateToken>		CArrayTemplateTokens;


//A template's text, as IntelliJ IDEA's File and Code Templates have them: Apache Velocity's language, what of it a
//template needs (CodeTemplates.txt says which), parsed once (Parse) and applied with values (Evaluate).
//
//  $NAME and ${NAME} are a variable's value; $!NAME is nothing where it has none, and $NAME is written as it is.
//  A method of a string is called on it as Java's: ${NAME.toUpperCase()}, ${NAME.substring(1)}, and the IDE's own
//  toUpperSnakeCase(), toLowerSnakeCase() and withoutPrefix().  Outside braces, $NAME.h is NAME's value and ".h":
//  only $foreach's own (count, index, hasNext, first, last) are read without brackets there.
//  #set($x = ...), #if(...) #elseif(...) #else #end, #foreach($x in $list) #end with #break, #parse("Include"),
//  #include("Include"), ## to the end of the line and #* ... *# are Velocity's.  #{if}, #{else} and the like, with
//  braces, can be written against text.  A word after # that isn't one of them ("#include \"x.h\"", "#ifndef") is
//  text, as it is in Velocity; \$ and \# are a $ or a # that starts nothing.
//  A line that has only directives and comments on it is taken out of the output, its end too, so a template can
//  be laid out with its #if on lines of their own (Velocity 2's "lines").
//  An expression has strings ("..." with references in it, '...' as it is), whole numbers, true, false, lists
//  ([a, b]), references, ( ), !, &&, ||, ==, !=, <, <=, >, >=, + and -, and Velocity's words for them (not, and,
//  or, eq, ne, lt, le, gt, ge).  #if takes a value as true unless it is nothing, false, 0, or an empty string or
//  list (Velocity 2's empty check).
class CTemplateText
{
protected:
	CChars						mszSource;
	CChars						mszPool;			//Names, each ended with a '\0'.
	CArrayTemplateNodes			masNodes;
	CArrayTemplateExpressions	masExpressions;
	size						muiRoot;
	CChars						mszError;
	CTextParser*				mpcParser;			//While parsing: the source, or the part of it (a string's) being read.
	bool						mbParsed;
	int							miDepth;			//#parse's: how many includes this one is inside.

public:
	void	Init(void);
	void	Kill(void);

	bool	Parse(const char* szText);
	bool	Evaluate(CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest);
	void	FindMissing(CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CArrayChars* paszDest);
	char*	GetError(void);

protected:
	bool	Error(size uiPos, const char* szMessage);
	void	FindLineColumn(size uiPos, size* puiLine, size* puiColumn);
	size	AddName(const char* szName, size uiLength);
	char*	GetName(size uiName);
	size	AddNode(ETemplateNode eKind, size uiStart, size uiEnd);
	size	AddExpression(ETemplateExpression eKind, size uiStart);
	size	Offset(void);
	char	Peek(void);
	char	Peek(int iAhead);
	bool	ReadName(size* puiName);
	bool	IsWord(const char* szWord);
	void	SkipSpace(void);

	bool	Lex(size uiStart, size uiEnd, bool bInString, CArrayTemplateTokens* pasTokens);
	void	AddText(size uiStart, size uiEnd, CArrayTemplateTokens* pasTokens);
	bool	LexDirective(STemplateToken* psDest, bool* pbDirective);
	void	RemoveDirectiveLines(CArrayTemplateTokens* pasTokens);
	bool	Build(CArrayTemplateTokens* pasTokens, size* puiToken, int iDepth, size* puiFirst, ETemplateDirective* peStop, size* puiStopToken);

	bool	ParseReference(bool bExpression, size* puiExpression);
	bool	ParseExpression(size* puiExpression);
	bool	ParseOr(size* puiExpression);
	bool	ParseAnd(size* puiExpression);
	bool	ParseNot(size* puiExpression);
	bool	ParseCompare(size* puiExpression);
	bool	ParseAdditive(size* puiExpression);
	bool	ParsePrimary(size* puiExpression);
	bool	ParseArguments(char cClose, size* puiFirst);
	bool	ParseString(size* puiExpression);

	bool	EvaluateList(size uiFirst, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest, bool* pbBreak);
	bool	EvaluateNode(size uiNode, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest, bool* pbBreak);
	bool	EvaluateForeach(STemplateNode* psNode, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest);
	bool	EvaluateParse(STemplateNode* psNode, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CChars* pszDest);
	bool	EvaluateExpression(size uiExpression, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, STemplateValue* psDest);
	bool	EvaluateCall(STemplateExpression* psCall, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, STemplateValue* psDest);
	bool	Compare(STemplateExpression* psExpression, STemplateValue* psLeft, STemplateValue* psRight, STemplateValue* psDest);
	void	FindMissing(CTemplateValues* pcValues, CTemplateIncludes* pcIncludes, CArrayChars* paszSet, CArrayChars* paszDest);
};


#endif // __TEMPLATE_TEXT_H__
