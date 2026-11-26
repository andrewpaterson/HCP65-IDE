#include "BaseLib/Chars.h"
#include "TemplateLib/CodeFormatter.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// szSource formatted is szExpected; its edits, made to the source, give
// the same; and formatting the result changes nothing.  How many changes
// were made.
//
//////////////////////////////////////////////////////////////////////////
static size TestCodeFormat(const char* szSource, const char* szExpected, bool bSourceFile)
{
	CCodeFormatter	cFormatter;
	CChars			szText;
	CChars			szEdited;
	SCodeEdit*		psEdit;
	size			uiAt;
	size			uiChanges;
	size			i;

	cFormatter.Init();
	cFormatter.Format(szSource, bSourceFile);
	szText.Init();
	cFormatter.GetText(&szText);
	AssertString(szExpected, szText.Text());
	AssertBool(strcmp(szSource, szExpected) != 0, cFormatter.IsChanged());

	szEdited.Init();
	uiAt = 0;
	for (i = 0; i < cFormatter.NumEdits(); i++)
	{
		psEdit = cFormatter.GetEdit(i);
		AssertTrue(psEdit->uiStart >= uiAt);
		szEdited.Append(szSource + uiAt, psEdit->uiStart - uiAt);
		szEdited.Append(cFormatter.GetEditText(psEdit), psEdit->uiLength);
		uiAt = psEdit->uiEnd;
	}
	szEdited.Append(szSource + uiAt);
	AssertString(szExpected, szEdited.Text());
	for (i = 0; i < cFormatter.NumEdits(); i++)
	{
		//An edit is only what differs: it doesn't start or end with what was there.
		psEdit = cFormatter.GetEdit(i);
		if ((psEdit->uiLength > 0) && (psEdit->uiEnd > psEdit->uiStart))
		{
			AssertTrue(szSource[psEdit->uiStart] != cFormatter.GetEditText(psEdit)[0]);
			AssertTrue(szSource[psEdit->uiEnd - 1] != cFormatter.GetEditText(psEdit)[psEdit->uiLength - 1]);
		}
	}
	szEdited.Kill();
	uiChanges = cFormatter.NumChanges();

	cFormatter.Format(szExpected, bSourceFile);
	AssertFalse(cFormatter.IsChanged());
	AssertSize(0, cFormatter.NumChanges());
	szText.Kill();
	cFormatter.Kill();
	return uiChanges;
}


//////////////////////////////////////////////////////////////////////////
//
// A line that starts a statement is indented by its depth in tabs; a case
// and an access label are level with their switch and class; a directive
// is at the margin; a statement's other lines move with its first.
//
//////////////////////////////////////////////////////////////////////////
void TestCodeFormatterIndent(void)
{
	AssertSize(5, TestCodeFormat(
		"void F(void)\n{\n    int i;\n\n    i = 1;\n  if (i)\n\t{\n\t\t\tG(i,\n\t\t\t  2);\n\t}\n}\n",
		"void F(void)\n{\n\tint i;\n\n\ti = 1;\n\tif (i)\n\t{\n\t\tG(i,\n\t\t  2);\n\t}\n}\n", false));

	TestCodeFormat(
		"void F(int i)\n{\n\tswitch (i)\n\t{\n\t\tcase 1:\n\t\t\tG();\n\t\t\tbreak;\n\t\tdefault:\n\t\t\tbreak;\n\t}\n}\n",
		"void F(int i)\n{\n\tswitch (i)\n\t{\n\tcase 1:\n\t\tG();\n\t\tbreak;\n\tdefault:\n\t\tbreak;\n\t}\n}\n", false);

	//A class: its labels, its friends and macros at the margin, its members a tab in, and members written further in
	//(their types in a column after "virtual") left there.
	TestCodeFormat(
		"class CThing\n{\nfriend class COther;\nCONSTRUCTABLE(CThing);\n\tpublic:\nint\t\tmi;\n\n\tvirtual void\tInit(void);\n\t\t\tvoid\tKill(void);\n};\n",
		"class CThing\n{\nfriend class COther;\nCONSTRUCTABLE(CThing);\npublic:\n\tint\t\tmi;\n\n\tvirtual void\tInit(void);\n\t\t\tvoid\tKill(void);\n};\n", false);

	TestCodeFormat(
		"void F(void)\n{\n\t#ifdef DEBUG\n  //Said.\n\tG();\n\t#endif\n}\n",
		"void F(void)\n{\n#ifdef DEBUG\n\t//Said.\n\tG();\n#endif\n}\n", false);

	//An enum's and an initialiser's elements are a tab in; a body on one line is left.
	TestCodeFormat(
		"enum EKind\n{\nEK_One,\n  EK_Two,\n};\nstatic int gai[] =\n{\n1,\n2,\n};\nint G(void) { return 1; }\n",
		"enum EKind\n{\n\tEK_One,\n\tEK_Two,\n};\nstatic int gai[] =\n{\n\t1,\n\t2,\n};\nint G(void) { return 1; }\n", false);
}


//////////////////////////////////////////////////////////////////////////
//
// A block's opening brace that ends a line goes on a line of its own.
//
//////////////////////////////////////////////////////////////////////////
void TestCodeFormatterBraces(void)
{
	TestCodeFormat(
		"void F(int i) {\n\tif (i) {\n\t\tG();\n\t}\n}\n",
		"void F(int i)\n{\n\tif (i)\n\t{\n\t\tG();\n\t}\n}\n", false);
	TestCodeFormat(
		"void F(int i)\n{ G();\n}\n",
		"void F(int i)\n{\n\tG();\n}\n", false);
	TestCodeFormat("int gai[] = { 1, 2 };\nint F(void) { return 1; }\n", "int gai[] = { 1, 2 };\nint F(void) { return 1; }\n", false);
}


//////////////////////////////////////////////////////////////////////////
//
// A space after a comma and a keyword and beside the operators that are
// always binary; a sign, a star, an ampersand, an angle bracket, a pure
// function's "=0" and white space that is there already are left.
//
//////////////////////////////////////////////////////////////////////////
void TestCodeFormatterSpaces(void)
{
	TestCodeFormat(
		"void F(int a,int b)\n{\n\tif(a==b||a!=1)\n\t{\n\t\ta=b+1;\n\t\tb+=a/2;\n\t\tfor (;;)\n\t\t{\n\t\t}\n\t\tfor (a=0;a<b;a++)\n\t\t{\n\t\t}\n\t\treturn(a);\n\t}\n}\n",
		"void F(int a, int b)\n{\n\tif (a == b || a != 1)\n\t{\n\t\ta = b + 1;\n\t\tb += a / 2;\n\t\tfor (;;)\n\t\t{\n\t\t}\n\t\tfor (a = 0; a<b; a++)\n\t\t{\n\t\t}\n\t\treturn (a);\n\t}\n}\n", false);

	TestCodeFormat(
		"void F(void)\n{\n\ta = -1;\n\tb = (int)-c;\n\td = a*b;\n\te = &a;\n\tf = a<b;\n\tg = 1e-5;\n\th = a   +   b;\n\tx = y,\tz;\n\tsz = \"a,b=c\";\n\tc = 'a'+1;\n}\n",
		"void F(void)\n{\n\ta = -1;\n\tb = (int)-c;\n\td = a*b;\n\te = &a;\n\tf = a<b;\n\tg = 1e-5;\n\th = a   +   b;\n\tx = y,\tz;\n\tsz = \"a,b=c\";\n\tc = 'a' + 1;\n}\n", false);

	TestCodeFormat(
		"class CThing\n{\n\tvirtual void\tF(void) =0;\n\tbool\t\t\toperator==(CThing* pc);\n\tint\t\t\t\tG(int i = -1);\n};\n",
		"class CThing\n{\n\tvirtual void\tF(void) =0;\n\tbool\t\t\toperator==(CThing* pc);\n\tint\t\t\t\tG(int i = -1);\n};\n", false);

	//A directive and a comment are left as they are written.
	TestCodeFormat("#define ADD(a,b) (a+b)\n//a=b,c\n/* if(a) */\n", "#define ADD(a,b) (a+b)\n//a=b,c\n/* if(a) */\n", false);
}


//////////////////////////////////////////////////////////////////////////
//
// A run of declarations has its names in one column, reached with tabs:
// the one most are in, else the first tab stop after the longest type.
//
//////////////////////////////////////////////////////////////////////////
void TestCodeFormatterColumns(void)
{
	//None lined up with tabs: the first tab stop after the longest type, "CChars*" (column 11, so 12).
	TestCodeFormat(
		"void F(void)\n{\n\tint i;\n\tCChars *psz;\n\tunsigned int  uiCount = 0;\n\n\ti = 0;\n}\n",
		"void F(void)\n{\n\tint\t\t\t\ti;\n\tCChars*\t\t\tpsz;\n\tunsigned int\tuiCount = 0;\n\n\ti = 0;\n}\n", false);

	//Most are in a column further right than they need: it is kept, and the others join it.
	TestCodeFormat(
		"class CThing\n{\n\tint\t\t\tmi;\n\tchar\t\tmc;\n\tbool mb;\n\tCArrayTemplate<int, 4> mai;\n};\n",
		"class CThing\n{\n\tint\t\t\tmi;\n\tchar\t\tmc;\n\tbool\t\tmb;\n\tCArrayTemplate<int, 4>\tmai;\n};\n", false);

	//A space and then tabs is tabs; member functions line up as fields do.
	TestCodeFormat(
		"class CThing\n{\n\tvoid \t\tInit(void);\n\tconst char*\tGetName(void) const;\n\tCThing* \tGetNext(void);\n};\n",
		"class CThing\n{\n\tvoid\t\tInit(void);\n\tconst char*\tGetName(void) const;\n\tCThing*\t\tGetNext(void);\n};\n", false);

	//A space and then tabs keeps the column the tabs took it to, though it is further right than it need be.
	TestCodeFormat(
		"class CThing\n{\n\tvoid \t\t\tInit(void);\n\tvoid \t\t\tKill(void);\n\n\tCThing*\t\t\tGetNext(void);\n};\n",
		"class CThing\n{\n\tvoid\t\t\tInit(void);\n\tvoid\t\t\tKill(void);\n\n\tCThing*\t\t\tGetNext(void);\n};\n", false);

	//Short types keep the column they share; a type too long for it has one tab and doesn't move them.  A comment
	//over a member written further in stays over it.
	TestCodeFormat(
		"class CThing\n{\n\tsize\t\t\tNumStops(void);\n\tSTextLayoutStop*\tGetStop(size ui);\n\tsize\t\t\tNumLines(void);\n\tSTextLayoutLine*\tGetLine(size ui);\n};\n",
		"class CThing\n{\n\tsize\t\t\tNumStops(void);\n\tSTextLayoutStop*\tGetStop(size ui);\n\tsize\t\t\tNumLines(void);\n\tSTextLayoutLine*\tGetLine(size ui);\n};\n", false);
	TestCodeFormat(
		"class CThing\n{\n\tvirtual void\tA(void);\n\t\t\t//Over B.\n\t\t\tvoid\tB(void);\n};\n",
		"class CThing\n{\n\tvirtual void\tA(void);\n\t\t\t//Over B.\n\t\t\tvoid\tB(void);\n};\n", false);

	//One on its own written with a space is left; statements that aren't declarations aren't touched; a blank line
	//ends a run.
	TestCodeFormat(
		"void F(void)\n{\n\tint i;\n\n\ti = G(1);\n\treturn i;\n\tdelete p;\n\ta * b;\n\ti = 0;\n\tchar *sz;\n}\n",
		"void F(void)\n{\n\tint i;\n\n\ti = G(1);\n\treturn i;\n\tdelete p;\n\ta* b;\n\ti = 0;\n\tchar* sz;\n}\n", false);
	TestCodeFormat(
		"void F(void)\n{\n\tint\ti;\n\n\tCLongerType\tcLonger;\n}\n",
		"void F(void)\n{\n\tint\ti;\n\n\tCLongerType\tcLonger;\n}\n", false);
}


//////////////////////////////////////////////////////////////////////////
//
// In a source file a function of several lines with no comment over it
// has the banner put over it, two blank lines above; not in a header, and
// not a body on one line or a class.
//
//////////////////////////////////////////////////////////////////////////
void TestCodeFormatterBanners(void)
{
	CCodeFormatter	cFormatter;
	CChars			szText;

	TestCodeFormat(
		"#include \"A.h\"\nvoid F(void)\n{\n}\n\n//Over G.\nvoid G(void)\n{\n}\nint H(void) { return 1; }\nstruct SThing\n{\n\tint i;\n};\nint CThing::I(void) const\n{\n\treturn 1;\n}\n",
		"#include \"A.h\"\n\n\n//////////////////////////////////////////////////////////////////////////\n//\n//\n//////////////////////////////////////////////////////////////////////////\n"
		"void F(void)\n{\n}\n\n//Over G.\nvoid G(void)\n{\n}\nint H(void) { return 1; }\nstruct SThing\n{\n\tint i;\n};\n\n\n"
		"//////////////////////////////////////////////////////////////////////////\n//\n//\n//////////////////////////////////////////////////////////////////////////\n"
		"int CThing::I(void) const\n{\n\treturn 1;\n}\n", true);
	TestCodeFormat("void F(void)\n{\n}\n", "void F(void)\n{\n}\n", false);

	//Another banner, and a file that starts with a function.
	cFormatter.Init();
	cFormatter.SetBanner("//--\n// A function.\n//--\n");
	cFormatter.Format("void F(void)\n{\n}\n", true);
	szText.Init();
	cFormatter.GetText(&szText);
	AssertString("//--\n// A function.\n//--\nvoid F(void)\n{\n}\n", szText.Text());
	AssertSize(1, cFormatter.GetCount(CFC_Banner));
	szText.Kill();
	cFormatter.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// White space at a line's end goes, a comment's too; the line ends stay
// as the file has them, and a new line has the same.
//
//////////////////////////////////////////////////////////////////////////
void TestCodeFormatterLines(void)
{
	TestCodeFormat("int\tgi;  \n\t\n//Said.\t\nint\tgj;", "int\tgi;\n\n//Said.\nint\tgj;", false);
	TestCodeFormat("void F(void) {\r\n\tG();\r\n}\r\n", "void F(void)\r\n{\r\n\tG();\r\n}\r\n", false);
	TestCodeFormat("", "", true);

	//What isn't finished is left: a string and a comment that don't end, a brace that isn't closed.
	TestCodeFormat("void F(void)\n{\n\tsz = \"abc;\n", "void F(void)\n{\n\tsz = \"abc;\n", false);
	TestCodeFormat("/* a=b\n  c=d", "/* a=b\n  c=d", false);
	TestCodeFormat("sz = R\"x(a=b,c\n d)x\";\n", "sz = R\"x(a=b,c\n d)x\";\n", false);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestCodeFormatter(void)
{
	BeginTests();

	TestCodeFormatterIndent();
	TestCodeFormatterBraces();
	TestCodeFormatterSpaces();
	TestCodeFormatterColumns();
	TestCodeFormatterBanners();
	TestCodeFormatterLines();

	TestStatistics();
}

