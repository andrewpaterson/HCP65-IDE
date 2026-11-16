#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"
#include "TemplateLib/TemplateText.h"
#include "TemplateLib/TemplateStrings.h"
#include "TestLib/Assert.h"


//The includes a test's templates #parse, by name.
class CTestTemplateIncludes : public CTemplateIncludes
{
public:
	CArrayChars	maszNames;
	CArrayChars	maszTexts;

	void Init(void)
	{
		maszNames.Init();
		maszTexts.Init();
	}

	void Kill(void)
	{
		maszTexts.Kill();
		maszNames.Kill();
	}

	void Add(const char* szName, const char* szText)
	{
		maszNames.Add(szName);
		maszTexts.Add(szText);
	}

	bool GetInclude(const char* szName, CChars* pszDest)
	{
		size	i;

		for (i = 0; i < maszNames.NumElements(); i++)
		{
			if (strcmp(maszNames.GetText(i), szName) == 0)
			{
				pszDest->Append(maszTexts.GetText(i));
				return true;
			}
		}
		return false;
	}
};


//////////////////////////////////////////////////////////////////////////
//
// A template applied with values: its output, or "Error: " and the error.
//
//////////////////////////////////////////////////////////////////////////
void AssertTemplate(const char* szExpected, const char* szTemplate, CTemplateValues* pcValues, CTemplateIncludes* pcIncludes)
{
	CTemplateText	cTemplate;
	CChars			szOutput;

	cTemplate.Init();
	szOutput.Init();
	if (!cTemplate.Parse(szTemplate) || !cTemplate.Evaluate(pcValues, pcIncludes, &szOutput))
	{
		szOutput.Clear();
		szOutput.Append("Error: ")->Append(cTemplate.GetError());
	}
	AssertString(szExpected, szOutput.Text());
	szOutput.Kill();
	cTemplate.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateTextReferences(void)
{
	CTemplateValues	cValues;

	cValues.Init();
	cValues.Set("NAME", "CBoardView");
	cValues.Set("EMPTY", "");
	cValues.SetNumber("COUNT", 3);

	AssertTemplate("No references at all.\n", "No references at all.\n", &cValues, NULL);
	AssertTemplate("class CBoardView", "class $NAME", &cValues, NULL);
	AssertTemplate("class CBoardView;", "class ${NAME};", &cValues, NULL);
	AssertTemplate("CBoardView.h", "$NAME.h", &cValues, NULL);
	AssertTemplate("CBoardViewX", "${NAME}X", &cValues, NULL);
	AssertTemplate("3 parts", "$COUNT parts", &cValues, NULL);
	AssertTemplate("[] []", "[$EMPTY] [$!EMPTY]", &cValues, NULL);

	//With no value: written as it is, or nothing for $!.
	AssertTemplate("$MISSING and ${MISSING} and .", "$MISSING and ${MISSING} and $!MISSING$!{MISSING}.", &cValues, NULL);
	AssertTemplate("${MISSING.toUpperCase()}|", "${MISSING.toUpperCase()}|$!{MISSING.toUpperCase()}", &cValues, NULL);

	//$ and # that start nothing, and escaped ones.
	AssertTemplate("$5 costs $ 5, #1 and # and $NAME and #if", "$5 costs $ 5, #1 and # and \\$NAME and \\#if", &cValues, NULL);
	AssertTemplate("a@b.com $", "a@b.com $", &cValues, NULL);

	cValues.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateTextMethods(void)
{
	CTemplateValues	cValues;

	cValues.Init();
	cValues.Set("NAME", "CBoardView");
	cValues.Set("PADDED", "  Keg \t");
	cValues.Set("FILE_NAME", "BoardView.h");

	AssertTemplate("CBOARDVIEW cboardview", "${NAME.toUpperCase()} ${NAME.toLowerCase()}", &cValues, NULL);
	AssertTemplate("[Keg]", "[${PADDED.trim()}]", &cValues, NULL);
	AssertTemplate("10 false", "${NAME.length()} ${NAME.isEmpty()}", &cValues, NULL);
	AssertTemplate("BoardView Board", "${NAME.substring(1)} ${NAME.substring(1, 6)}", &cValues, NULL);
	AssertTemplate("CBoardPanel", "${NAME.replace('View', 'Panel')}", &cValues, NULL);
	AssertTemplate("true true false true", "${NAME.startsWith('CB')} ${NAME.endsWith(\"View\")} ${NAME.contains('Keg')} ${NAME.equals('CBoardView')}", &cValues, NULL);
	AssertTemplate("true 6 -1 1", "${NAME.equalsIgnoreCase('cboardview')} ${NAME.indexOf('View')} ${NAME.indexOf('Keg')} ${NAME.lastIndexOf('B')}", &cValues, NULL);
	AssertTemplate("BOARD_VIEW_H board_view_h", "${FILE_NAME.toUpperSnakeCase()} ${FILE_NAME.toLowerSnakeCase()}", &cValues, NULL);
	AssertTemplate("BoardView", "${NAME.withoutPrefix()}", &cValues, NULL);
	AssertTemplate("__BOARD_VIEW_H__", "__${NAME.withoutPrefix().toUpperSnakeCase()}_H__", &cValues, NULL);
	AssertTemplate("CBOARDVIEW.h", "$NAME.toUpperCase().h", &cValues, NULL);

	AssertTemplate("Error: Line 1, column 7: There is no method called toSnakeCase()", "Guard ${NAME.toSnakeCase()}", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 1: substring()'s begin and end aren't in the string", "${NAME.substring(4, 20)}", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 1: replace() takes 2 arguments", "${NAME.replace('a')}", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 1: substring() takes 1 or 2 arguments", "${NAME.substring(1, 2, 3)}", &cValues, NULL);

	cValues.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateTextDirectives(void)
{
	CTemplateValues	cValues;

	cValues.Init();
	cValues.Set("NAME", "CBoardView");
	cValues.Set("LICENCE", "LGPL");
	cValues.Set("EMPTY", "");
	cValues.SetNumber("COUNT", 3);
	cValues.SetBool("YES", true);

	//#if, #elseif and #else, on lines of their own: the lines go.
	AssertTemplate("Lesser\nEnd\n", "#if ($LICENCE == \"AGPL\")\nAffero\n#elseif ($LICENCE == 'LGPL')\nLesser\n#else\nNone\n#end\nEnd\n", &cValues, NULL);
	AssertTemplate("None\n", "#if ($LICENCE == \"AGPL\")\nAffero\n#elseif($LICENCE eq 'GPL')\nGeneral\n#else\nNone\n#end\n", &cValues, NULL);
	AssertTemplate("  Indented\n", "  #if ($YES)\n  Indented\n  #end\n", &cValues, NULL);

	//Against text.
	AssertTemplate("[yes]", "[#if($YES)yes#{else}no#end]", &cValues, NULL);
	AssertTemplate("[no]", "[#if($EMPTY)yes#{else}no#{end}]", &cValues, NULL);

	//Velocity 2's empty check.
	AssertTemplate("a c e g", "#if($NAME)a#end #if(!$EMPTY)c#end #if($COUNT)e#end #if(!$MISSING)g#end", &cValues, NULL);
	AssertTemplate("1 2 3 4", "#if($COUNT > 2)1#end #if($COUNT <= 3 && $YES)2#end #if($COUNT == 3 || $MISSING)3#end #if(not $EMPTY and $COUNT ne 4)4#end", &cValues, NULL);
	AssertTemplate("4 2 CBoardView!", "#set($x = $COUNT + 1)$x #set($y = $COUNT - 1)$y #set($z = $NAME + '!')$z", &cValues, NULL);

	//#set.
	AssertTemplate("CBoardView.cpp\n", "#set($FILE = \"${NAME}.cpp\")\n$FILE\n", &cValues, NULL);
	AssertTemplate("ab\n", "#set( $a = 'a' )\n#set($b = \"b\") ## a comment\n$a$b\n", &cValues, NULL);

	//C++'s preprocessor is text.
	AssertTemplate("#ifndef __X_H__\n#define __X_H__\n#include \"Y.h\"\n#endif // __X_H__\n", "#ifndef __X_H__\n#define __X_H__\n#include \"Y.h\"\n#endif // __X_H__\n", &cValues, NULL);
	AssertTemplate("#include <string.h>\n#if 0\n#else\n", "#include <string.h>\n#if 0\n\\#else\n", &cValues, NULL);
	AssertTemplate("Error: Line 2, column 1: #elseif and #else need an #if", "#if 0\n#else\n", &cValues, NULL);

	//Comments.
	AssertTemplate("a\nb\n", "a\n## all of this line goes\n#* and\nthese *#\nb\n", &cValues, NULL);
	AssertTemplate("a b", "a #* inline *#b", &cValues, NULL);

	//Errors.
	AssertTemplate("Error: Line 2, column 1: #if has no #end", "a\n#if($YES)\nb\n", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 3: #end has no #if or #foreach", "a #end", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 15: Nothing but #end can come after #else", "#if($YES)#else#else#elseif($YES)#end", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 9: #set needs an =: #set($name = value)", "#set($a 'x')", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 1: #* has no *#", "#* never ends", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 1: ${ has no }", "${NAME and", &cValues, NULL);

	cValues.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateTextForeach(void)
{
	CTemplateValues	cValues;
	CArrayChars		aszBases;

	cValues.Init();
	aszBases.Init();
	aszBases.Add("CComponent");
	aszBases.Add("CListener");
	aszBases.Add("CPointer");
	cValues.SetList("BASES", &aszBases);

	AssertTemplate("class CView : public CComponent, public CListener, public CPointer\n",
		"class CView#foreach($BASE in $BASES)#if($foreach.first) :#else,#end public $BASE#end\n", &cValues, NULL);
	AssertTemplate("0 1 CComponent\n1 2 CListener\n2 3 CPointer\n",
		"#foreach ($BASE in $BASES)\n$foreach.index $foreach.count $BASE\n#end\n", &cValues, NULL);
	AssertTemplate("CComponent, CListener, CPointer.", "#foreach($b in $BASES)$b#if($foreach.hasNext), #end#end.", &cValues, NULL);
	AssertTemplate("CComponent CListener ", "#foreach($b in $BASES)#if($b == 'CPointer')#break#end$b#if(!$foreach.last) #end#end", &cValues, NULL);
	AssertTemplate("a1b1 a2b2 ", "#foreach($a in ['a1', 'a2'])#foreach($b in [\"b${foreach.count}\"])$a$b #end#end", &cValues, NULL);
	AssertTemplate("3 true CListener", "${BASES.size()} ${BASES.contains('CPointer')} ${BASES.get(1)}", &cValues, NULL);
	AssertTemplate(".", "#foreach($b in $MISSING)$b#end.", &cValues, NULL);

	//The item goes when the #foreach ends.
	AssertTemplate("$b", "#foreach($b in $BASES)#end$b", &cValues, NULL);
	AssertTemplate("Error: Line 1, column 1: #foreach has no #end", "#foreach($b in $BASES)", &cValues, NULL);

	aszBases.Kill();
	cValues.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateTextParse(void)
{
	CTemplateValues			cValues;
	CTestTemplateIncludes	cIncludes;
	CChars					szExpected;
	int						i;

	cValues.Init();
	cValues.Set("NAME", "CBoardView");
	cValues.Set("PROJECT_NAME", "LogicimLib");
	cIncludes.Init();
	cIncludes.Add("File Header", "/** $PROJECT_NAME **/\n#set($HEADER = 'done')\n");
	cIncludes.Add("Raw", "$NAME #if\n");
	cIncludes.Add("Self", "x#parse('Self')");

	AssertTemplate("/** LogicimLib **/\nclass CBoardView; done\n", "#parse(\"File Header\")\nclass $NAME; $HEADER\n", &cValues, &cIncludes);
	AssertTemplate("$NAME #if\n", "#include('Raw')", &cValues, &cIncludes);
	AssertTemplate("Error: Line 1, column 1: There is no include called \"Missing\"", "#parse('Missing')", &cValues, &cIncludes);
	//Each include's error is inside the one that #parsed it.
	szExpected.Init();
	szExpected.Append("Error: Line 1, column 1: In \"Self\": ");
	for (i = 1; i < TEMPLATE_MAXIMUM_PARSE_DEPTH; i++)
	{
		szExpected.Append("Line 1, column 2: In \"Self\": ");
	}
	szExpected.Append("Line 1, column 2: \"Self\" is #parsed inside itself, or inside more than 20 includes");
	AssertTemplate(szExpected.Text(), "#parse('Self')", &cValues, &cIncludes);
	szExpected.Kill();

	cIncludes.Kill();
	cValues.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateTextFindMissing(void)
{
	CTemplateValues			cValues;
	CTestTemplateIncludes	cIncludes;
	CTemplateText			cTemplate;
	CArrayChars				aszMissing;

	cValues.Init();
	cValues.Set("NAME", "CBoardView");
	cIncludes.Init();
	cIncludes.Add("File Header", "/** $PROJECT_NAME $NAME $DESCRIPTION **/\n");
	aszMissing.Init();
	cTemplate.Init();

	AssertTrue(cTemplate.Parse("#parse('File Header')\n#set($GUARD = $NAME.toUpperCase())\n${DESCRIPTION} $GUARD $BASE.trim() #foreach($b in $BASES)$b $foreach.count#end $!AUTHOR"));
	cTemplate.FindMissing(&cValues, &cIncludes, &aszMissing);
	AssertSize(5, aszMissing.NumElements());
	AssertString("DESCRIPTION", aszMissing.GetText(0));
	AssertString("BASE", aszMissing.GetText(1));
	AssertString("BASES", aszMissing.GetText(2));
	AssertString("AUTHOR", aszMissing.GetText(3));
	AssertString("PROJECT_NAME", aszMissing.GetText(4));

	cTemplate.Kill();
	aszMissing.Kill();
	cIncludes.Kill();
	cValues.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateStrings(void)
{
	CChars	sz;

	sz.Init();
	AssertString("BOARD_VIEW", AppendTemplateUpperSnakeCase("BoardView", &sz));
	sz.Clear();
	AssertString("CPP65_TYPE_CHECKER_H", AppendTemplateUpperSnakeCase("CPP65TypeChecker.h", &sz));
	sz.Clear();
	AssertString("board_view", AppendTemplateLowerSnakeCase("BoardView", &sz));
	sz.Clear();
	AssertString("BoardView", AppendTemplateWithoutPrefix("CBoardView", &sz));
	sz.Clear();
	AssertString("TemplateNode", AppendTemplateWithoutPrefix("STemplateNode", &sz));
	sz.Clear();
	AssertString("Chars", AppendTemplateWithoutPrefix("Chars", &sz));
	sz.Clear();
	AssertString("CPU", AppendTemplateWithoutPrefix("CCPU", &sz));
	sz.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestTemplateText(void)
{
	BeginTests();

	TestTemplateStrings();
	TestTemplateTextReferences();
	TestTemplateTextMethods();
	TestTemplateTextDirectives();
	TestTemplateTextForeach();
	TestTemplateTextParse();
	TestTemplateTextFindMissing();

	TestStatistics();
}
