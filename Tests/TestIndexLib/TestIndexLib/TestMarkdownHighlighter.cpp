#include "IndexLib/MarkdownHighlighter.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// Each highlight as "<line>:<column>+<length> <kind>", one a line.
//
//////////////////////////////////////////////////////////////////////////
static void TestMarkdownHighlighterPrint(CArrayHighlights* pacHighlights, CChars* pszDest)
{
	size			i;
	SCPPHighlight*	psHighlight;

	for (i = 0; i < pacHighlights->NumElements(); i++)
	{
		psHighlight = pacHighlights->Get(i);
		pszDest->Append(psHighlight->iLine);
		pszDest->Append(':');
		pszDest->Append(psHighlight->iColumn);
		pszDest->Append('+');
		pszDest->Append(psHighlight->iLength);
		pszDest->Append(' ');
		pszDest->Append(CCPPHighlighter::GetKindName((ECPPHighlight)psHighlight->uiKind));
		pszDest->AppendNewLine();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A heading (its code span shown as the heading), a code span, a
// reference to a class and one that names nothing, an HTML comment, and a
// ```cpp block coloured lexically.
//
//////////////////////////////////////////////////////////////////////////
void TestMarkdownHighlighterPage(void)
{
	CArrayPageReferences	asReferences;
	SPageReference*			psReference;
	CArrayHighlights		acHighlights;
	CChars					szPrinted;

	asReferences.Init();
	psReference = asReferences.Add();
	memset(psReference, 0, sizeof(SPageReference));
	psReference->eKind = PRK_Link;
	psReference->iLine = 1;
	psReference->iColumn = 16;
	psReference->iSymbolKind = CPPSK_Class;
	psReference = asReferences.Add();
	memset(psReference, 0, sizeof(SPageReference));
	psReference->eKind = PRK_Error;
	psReference->iLine = 1;
	psReference->iColumn = 31;
	psReference->iSymbolKind = -1;

	acHighlights.Init();
	CMarkdownHighlighter::Highlight(
		"# Title `x`\n"
		"Text `code` and [`CShape`] and [`Nope`].\n"
		"<!-- a comment -->\n"
		"```cpp\n"
		"int i = 0;  // count\n"
		"```\n", &asReferences, &acHighlights);
	szPrinted.Init();
	TestMarkdownHighlighterPrint(&acHighlights, &szPrinted);
	AssertString(
		"0:0+11 keyword\n"
		"1:5+6 string\n"
		"1:16+10 class\n"
		"1:31+8 unresolved\n"
		"2:0+18 comment\n"
		"3:0+6 directive\n"
		"4:0+3 keyword\n"
		"4:8+1 number\n"
		"4:12+8 comment\n"
		"5:0+3 directive\n", szPrinted.Text());
	szPrinted.Kill();
	acHighlights.Kill();
	asReferences.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A fence that isn't C++ is left alone inside; a page not resolved yet
// shows its references as code spans.
//
//////////////////////////////////////////////////////////////////////////
void TestMarkdownHighlighterUnresolved(void)
{
	CArrayHighlights	acHighlights;
	CChars				szPrinted;

	acHighlights.Init();
	CMarkdownHighlighter::Highlight("```asm\nlda #0  // not C++\n```\nSee [`CShape`].", NULL, &acHighlights);
	szPrinted.Init();
	TestMarkdownHighlighterPrint(&acHighlights, &szPrinted);
	AssertString(
		"0:0+6 directive\n"
		"2:0+3 directive\n"
		"3:4+10 string\n", szPrinted.Text());
	szPrinted.Kill();
	acHighlights.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestMarkdownHighlighter(void)
{
	BeginTests();

	TestMarkdownHighlighterPage();
	TestMarkdownHighlighterUnresolved();

	TestStatistics();
}

