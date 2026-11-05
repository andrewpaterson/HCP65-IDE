#include "ClaudeLib/JSONDocument.h"
#include "TestLib/Assert.h"


//////////////////////////////////////////////////////////////////////////
//
// Every kind of value, members found by name and elements in order.
//
//////////////////////////////////////////////////////////////////////////
void TestJSONDocumentValues(void)
{
	CJSONDocument	cDocument;
	size			uiRoot;
	size			uiArray;
	size			uiElement;

	cDocument.Init();
	AssertTrue(cDocument.Parse(" {\"name\": \"Maths.cpp\", \"lines\": 12, \"cost\": -0.075e-2, \"open\": true, \"saved\": false, \"owner\": null,"
							   " \"list\": [1, \"two\", [], {}], \"empty\": {}} \r\n"));

	uiRoot = cDocument.GetRoot();
	AssertInt(JT_Object, cDocument.GetType(uiRoot));
	AssertNull(cDocument.GetName(uiRoot));
	AssertSize(8, cDocument.NumChildren(uiRoot));

	AssertString("Maths.cpp", cDocument.GetString(uiRoot, "name"));
	AssertInt(JT_Number, cDocument.GetType(cDocument.GetMember(uiRoot, "lines")));
	AssertString("12", cDocument.GetString(uiRoot, "lines"));
	AssertString("-0.075e-2", cDocument.GetString(uiRoot, "cost"));
	AssertTrue(cDocument.GetBool(uiRoot, "open", false));
	AssertFalse(cDocument.GetBool(uiRoot, "saved", true));
	AssertInt(JT_Null, cDocument.GetType(cDocument.GetMember(uiRoot, "owner")));
	AssertTrue(cDocument.GetMember(uiRoot, "owner") != JSON_NONE);
	AssertNull(cDocument.GetString(uiRoot, "owner"));
	AssertTrue(cDocument.GetBool(uiRoot, "missing", true));
	AssertTrue(cDocument.GetMember(uiRoot, "missing") == JSON_NONE);
	AssertNull(cDocument.GetString(uiRoot, "missing"));

	uiArray = cDocument.GetMember(uiRoot, "list");
	AssertInt(JT_Array, cDocument.GetType(uiArray));
	AssertString("list", cDocument.GetName(uiArray));
	AssertSize(4, cDocument.NumChildren(uiArray));
	uiElement = cDocument.GetFirst(uiArray);
	AssertString("1", cDocument.GetText(uiElement));
	AssertNull(cDocument.GetName(uiElement));
	uiElement = cDocument.GetNext(uiElement);
	AssertString("two", cDocument.GetText(uiElement));
	AssertInt(JT_Array, cDocument.GetType(cDocument.GetElement(uiArray, 2)));
	AssertSize(0, cDocument.NumChildren(cDocument.GetElement(uiArray, 2)));
	AssertInt(JT_Object, cDocument.GetType(cDocument.GetElement(uiArray, 3)));
	AssertTrue(cDocument.GetElement(uiArray, 4) == JSON_NONE);
	AssertTrue(cDocument.GetMember(uiArray, "name") == JSON_NONE);

	AssertInt(JT_Object, cDocument.GetType(cDocument.GetMember(uiRoot, "empty")));
	AssertTrue(cDocument.GetFirst(cDocument.GetMember(uiRoot, "empty")) == JSON_NONE);

	//Read again, the old values go.
	AssertTrue(cDocument.Parse("\"alone\""));
	AssertInt(JT_String, cDocument.GetType(cDocument.GetRoot()));
	AssertString("alone", cDocument.GetText(cDocument.GetRoot()));
	AssertTrue(cDocument.Parse("0"));
	AssertString("0", cDocument.GetText(cDocument.GetRoot()));

	cDocument.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Escapes, \u as UTF-8, surrogate pairs as one character and lone
// surrogates replaced.
//
//////////////////////////////////////////////////////////////////////////
void TestJSONDocumentEscapes(void)
{
	CJSONDocument	cDocument;
	size			uiRoot;

	cDocument.Init();
	AssertTrue(cDocument.Parse("{\"a\":\"say \\\"hi\\\"\\n\\tC:\\\\Code\\/x\", \"b\":\"\\u0041\\u00e9\\u201c\", \"c\":\"\\ud83d\\ude00!\", \"d\":\"\\ud83dx\", \"e\":\"\\u00\"}") == false);
	AssertTrue(cDocument.GetRoot() == JSON_NONE);

	AssertTrue(cDocument.Parse("{\"a\":\"say \\\"hi\\\"\\n\\tC:\\\\Code\\/x\", \"b\":\"\\u0041\\u00e9\\u201c\", \"c\":\"\\ud83d\\ude00!\", \"d\":\"\\ud83dx\", \"\\u006eame\":\"\\b\\f\\r\"}"));
	uiRoot = cDocument.GetRoot();
	AssertString("say \"hi\"\n\tC:\\Code/x", cDocument.GetString(uiRoot, "a"));
	AssertString("A\xC3\xA9\xE2\x80\x9C", cDocument.GetString(uiRoot, "b"));
	AssertString("\xF0\x9F\x98\x80!", cDocument.GetString(uiRoot, "c"));
	AssertString("\xEF\xBF\xBDx", cDocument.GetString(uiRoot, "d"));
	AssertString("\b\f\r", cDocument.GetString(uiRoot, "name"));

	//UTF-8 in the text is kept.
	AssertTrue(cDocument.Parse("\"caf\xC3\xA9\""));
	AssertString("caf\xC3\xA9", cDocument.GetText(cDocument.GetRoot()));

	cDocument.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Texts that aren't JSON are refused, with no values left.
//
//////////////////////////////////////////////////////////////////////////
void TestJSONDocumentErrors(void)
{
	CJSONDocument	cDocument;
	CChars			sz;

	cDocument.Init();
	AssertFalse(cDocument.Parse(""));
	AssertFalse(cDocument.Parse("   "));
	AssertFalse(cDocument.Parse("{"));
	AssertFalse(cDocument.Parse("{\"a\" 1}"));
	AssertFalse(cDocument.Parse("{\"a\":1,}"));
	AssertFalse(cDocument.Parse("{a:1}"));
	AssertFalse(cDocument.Parse("[1 2]"));
	AssertFalse(cDocument.Parse("[1,]"));
	AssertFalse(cDocument.Parse("tru"));
	AssertFalse(cDocument.Parse("\"open"));
	AssertFalse(cDocument.Parse("\"tab\there\""));
	AssertFalse(cDocument.Parse("\"\\x\""));
	AssertFalse(cDocument.Parse("01"));
	AssertFalse(cDocument.Parse("1."));
	AssertFalse(cDocument.Parse("1e"));
	AssertFalse(cDocument.Parse("-"));
	AssertFalse(cDocument.Parse("{} {}"));
	AssertFalse(cDocument.Parse("Error: not signed in"));
	AssertString("Expected a value", cDocument.GetError());
	AssertTrue(cDocument.GetRoot() == JSON_NONE);

	//256 deep is read, 257 is refused.
	sz.Init();
	sz.Append('[', 256);
	sz.Append('1');
	sz.Append(']', 256);
	AssertTrue(cDocument.Parse(sz.Text()));
	sz.Clear();
	sz.Append('[', 257);
	sz.Append('1');
	sz.Append(']', 257);
	AssertFalse(cDocument.Parse(sz.Text()));
	AssertString("Too deep", cDocument.GetError());
	sz.Kill();
	cDocument.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestJSONDocument(void)
{
	BeginTests();

	TestJSONDocumentValues();
	TestJSONDocumentEscapes();
	TestJSONDocumentErrors();

	TestStatistics();
}

