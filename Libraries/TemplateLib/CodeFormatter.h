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
#ifndef __CODE_FORMATTER_H__
#define __CODE_FORMATTER_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "BaseLib/Chars.h"


#define CODE_FORMAT_TAB_WIDTH	4
#define CODE_FORMAT_NONE		(-1)


enum ECodeTokenKind
{
	CTK_WhiteSpace,		//Spaces and tabs.
	CTK_LineEnd,
	CTK_Comment,		//From // to its line's end.
	CTK_BlockComment,	//From /* to */, whatever lines it is over.
	CTK_Directive,		//A preprocessor line, with the lines it carries on over.
	CTK_String,
	CTK_Character,
	CTK_Number,
	CTK_Identifier,		//A name or a keyword.
	CTK_Punctuator,
};


//What a '{' opens: a block (a function's, a statement's, a class's), in which a ';' ends a statement, or a list (an
//enum's, an initialiser's), in which a ',' ends an element.
enum ECodeBraceKind
{
	CBK_None,
	CBK_Block,
	CBK_List,
};


//What the formatter changed, counted (CCodeFormatter::GetCount).
enum ECodeFormatChange
{
	CFC_Indent,			//A line indented again.
	CFC_Brace,			//A brace put on a line of its own.
	CFC_AfterBrace,		//Code after a brace put on the next line.
	CFC_Comma,			//A space put after a comma or a for's ';'.
	CFC_Keyword,		//A space put after if, for, while, switch or return.
	CFC_Operator,		//A space put beside an operator.
	CFC_Column,			//A declaration's name put in its run's column.
	CFC_Star,			//A star or ampersand put against its type.
	CFC_Banner,			//A banner put over a function.
	CFC_Trailing,		//White space taken off a line's end.

	CFC_NumChanges
};


//A piece of the text: its kind, where its text is (in the source, or in what the formatter added) and, once the
//structure is worked out (CCodeFormatter::FindStructure), how deep in braces and brackets it is and whether it starts
//a statement; a brace's kind, the first token of the statement it is in (its head) and the brace that matches it.
struct SCodeToken
{
	ECodeTokenKind	eKind;
	bool			bAdded;
	size			uiStart;
	size			uiLength;

	int32			iDepth;
	int32			iRounds;
	bool			bStarts;
	ECodeBraceKind	eBrace;
	int32			iHead;
	int32			iMatch;
};


typedef CArrayTemplate<SCodeToken>	CArrayCodeTokens;


//A change to the source that makes it the formatted text: [uiStart, uiEnd) of the source becomes the edit's text
//(CCodeFormatter::GetEditText).  They are in order and don't overlap.
struct SCodeEdit
{
	size	uiStart;
	size	uiEnd;
	size	uiText;
	size	uiLength;
};


typedef CArrayTemplate<SCodeEdit>	CArrayCodeEdits;


//A declaration on a line of its own: its first token, the last of its type, where the stars before its name start
//(its name, if it has none) and its name.
struct SCodeDeclaration
{
	int32	iFirst;
	int32	iTypeEnd;
	int32	iStars;
	int32	iName;
};


typedef CArrayTemplate<SCodeDeclaration>	CArrayCodeDeclarations;


//Lays C and C++ out in the house style, changing only the white space between its tokens (and putting a banner over
//a function that has no comment over it).  It reads the text itself, not what the preprocessor makes of it, so the
//code an #if leaves out is laid out too; and it knows no types, so what can't be told from the tokens is left as it
//is written.
//
//  - Each line that starts a statement is indented by a tab for each brace it is in; a case and an access label
//    (public:) are level with their switch and class; a preprocessor line is at the margin.  A statement's other
//    lines move as its first did.  In a class, a friend and a macro (CONSTRUCTABLE) at the margin stay there, and a
//    member indented further stays so (its type in a column after "virtual").
//  - A block's opening brace that ends a line of code goes on a line of its own.  A body on one line ({ return x; })
//    and an initialiser stay as they are.
//  - A space is put after a comma and a for's ';', between if, for, while, switch or return and a bracket, and each
//    side of
//    = == != <= >= || += -= *= /= %= &= |= ^= <<= >>= and of a binary + - / % | ^ written with none.  The * & < >
//    are left: a pointer or a template's can't be told from an operator's.  White space that is there is never
//    taken out or made one space, as code laid out in a table has more.
//  - A run of declaration lines (a function's locals, a class's fields and its member functions) has its names in
//    one column, reached with tabs: the column most of them are in already, else the first tab stop after the
//    longest type.  A declaration too long for it has one tab.  A star is put against its type (char* sz).  A
//    declaration on its own, written with one space, is left.
//  - In a source file (bSourceFile) a function defined over several lines at the top level, with no comment over
//    it, has the banner put over it (SetBanner), two blank lines above.
//  - White space at a line's end goes.
//
//Formatting what it has formatted changes nothing.
class CCodeFormatter
{
protected:
	const char*			mszSource;
	size				muiLength;
	bool				mbSourceFile;
	CChars				mszAdded;		//The text of the tokens it added.
	CChars				mszLineEnd;		//The source's: "\r\n" if most of its line ends are.
	CArrayChars			maszBanner;		//The banner's lines.
	CArrayCodeTokens	masTokens;
	CArrayCodeEdits		masEdits;
	CChars				mszEdits;		//The edits' texts.
	size				mauiCounts[CFC_NumChanges];

public:
	void		Init(void);
	void		Kill(void);

	void		SetBanner(const char* szBanner);
	void		Format(const char* szSource, bool bSourceFile);

	void		GetText(CChars* pszDest);
	bool		IsChanged(void);
	size		NumEdits(void);
	SCodeEdit*	GetEdit(size uiIndex);
	const char*	GetEditText(SCodeEdit* psEdit);
	size		GetCount(ECodeFormatChange eChange) const;
	size		NumChanges(void) const;

protected:
	void		Tokenise(void);
	void		AddToken(ECodeTokenKind eKind, size uiStart, size uiEnd);
	void		InsertToken(int32 iIndex, ECodeTokenKind eKind, const char* szText, size uiLength);
	void		InsertLineEnd(int32 iIndex);
	const char*	GetTokenText(int32 iToken);
	bool		Is(int32 iToken, const char* szText);
	bool		IsPunctuator(int32 iToken, const char* szText);
	bool		IsWord(int32 iToken, const char* szText);
	bool		IsOneOf(int32 iToken, const char* szWords);
	bool		IsSignificant(int32 iToken);
	bool		HasLineEnd(int32 iToken);
	int32		PreviousSignificant(int32 iToken);
	int32		NextSignificant(int32 iToken);
	bool		IsSameLine(int32 iFirst, int32 iSecond);
	bool		IsFirstOnLine(int32 iToken);
	int32		FindLineStart(int32 iToken);
	int32		FindLineEnd(int32 iToken);
	int32		FindWidth(int32 iFirst, int32 iEnd, int32 iColumn);
	void		AppendTokens(int32 iFirst, int32 iEnd, bool bWhiteSpaceOnly, CChars* pszDest);

	void		FindStructure(void);
	void		MoveBraces(void);
	void		Indent(void);
	bool		SetLead(int32 iLineStart, const char* szLead);
	bool		IsLabel(int32 iToken);
	bool		IsLabelWord(int32 iToken);
	bool		IsAtMargin(int32 iToken);
	bool		IsInRecord(int32 iToken);
	bool		HasRecordWord(int32 iFirst, int32 iEnd);
	void		AddSpaces(void);
	bool		IsValue(int32 iToken);
	bool		FindDeclaration(int32 iLineStart, int32 iLineEnd, SCodeDeclaration* psDest);
	void		AlignDeclarations(void);
	void		AlignRun(CArrayCodeDeclarations* pasRun, CArrayInt* paiStarts, CArrayInt* paiEnds, CArrayChars* paszTexts);
	void		AddBanners(void);
	void		RemoveTrailing(void);
	void		MakeEdits(void);
};


#endif // __CODE_FORMATTER_H__

