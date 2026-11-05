#include <string.h>
#include "ClaudeLib/ClaudeStream.h"
#include "TestLib/Assert.h"


//A request to rename a function, as claude -p --output-format stream-json --verbose writes it (trimmed): a search, a
//read, an edit refused, an edit that worked, a second edit of the same file, a Bash command refused and the answer.
const char* gszTestClaudeRename =
	"{\"type\":\"system\",\"subtype\":\"init\",\"cwd\":\"D:\\\\Code\",\"session_id\":\"d844c748-d234\",\"tools\":[\"Read\",\"Edit\"],\"model\":\"claude-opus-5-5\",\"permissionMode\":\"acceptEdits\"}\n"
	"{\"type\":\"assistant\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"thinking\",\"thinking\":\"Find it.\"},{\"type\":\"text\",\"text\":\"I'll find Add.\"},{\"type\":\"tool_use\",\"id\":\"toolu_1\",\"name\":\"Grep\",\"input\":{\"pattern\":\"\\\\bAdd\\\\b\",\"output_mode\":\"content\"}}]},\"session_id\":\"d844c748-d234\"}\n"
	"{\"type\":\"rate_limit_event\",\"rate_limit_info\":{\"status\":\"allowed\"},\"session_id\":\"d844c748-d234\"}\n"
	"{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":[{\"tool_use_id\":\"toolu_1\",\"type\":\"tool_result\",\"content\":\"Maths.cpp:1:int Add(int a, int b);\"}]}}\n"
	"{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"id\":\"toolu_2\",\"name\":\"Read\",\"input\":{\"file_path\":\"D:\\\\Code\\\\Maths.cpp\"}}]}}\n"
	"{\"type\":\"user\",\"message\":{\"content\":[{\"tool_use_id\":\"toolu_2\",\"type\":\"tool_result\",\"content\":[{\"type\":\"text\",\"text\":\"1\\tint Add(int a, int b);\"},{\"type\":\"text\",\"text\":\"2\\t\"}]}]}}\n"
	"{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"id\":\"toolu_3\",\"name\":\"Edit\",\"input\":{\"file_path\":\"D:\\\\Code\\\\Other.h\",\"old_string\":\"Add\",\"new_string\":\"Sum\"}},"
		"{\"type\":\"tool_use\",\"id\":\"toolu_4\",\"name\":\"Edit\",\"input\":{\"file_path\":\"D:\\\\Code\\\\Maths.cpp\",\"old_string\":\"int Add(\",\"new_string\":\"int Sum(\"}}]}}\n"
	"{\"type\":\"user\",\"message\":{\"content\":[{\"tool_use_id\":\"toolu_3\",\"type\":\"tool_result\",\"content\":\"<tool_use_error>String to replace not found.</tool_use_error>\",\"is_error\":true},"
		"{\"tool_use_id\":\"toolu_4\",\"type\":\"tool_result\",\"content\":\"The file D:\\\\Code\\\\Maths.cpp has been updated successfully.\"}]}}\n"
	"{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"id\":\"toolu_5\",\"name\":\"Write\",\"input\":{\"file_path\":\"D:\\\\Code\\\\Maths.cpp\",\"content\":\"int Sum(int a, int b);\\n\"}},"
		"{\"type\":\"tool_use\",\"id\":\"toolu_6\",\"name\":\"Bash\",\"input\":{\"command\":\"msbuild Maths.vcxproj\",\"description\":\"Build\"}}]}}\n"
	"{\"type\":\"user\",\"message\":{\"content\":[{\"tool_use_id\":\"toolu_5\",\"type\":\"tool_result\",\"content\":\"Written.\"},"
		"{\"tool_use_id\":\"toolu_6\",\"type\":\"tool_result\",\"content\":\"Claude requested permissions to use Bash, but you haven't granted it yet.\",\"is_error\":true}]}}\n"
	"{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"text\",\"text\":\"I renamed `Add` to `Sum` in \\u201cMaths.cpp\\u201d.\"}]}}\n"
	"{\"duration_ms\":1878,\"stop_reason\":\"end_turn\",\"session_id\":\"d844c748-d234\",\"total_cost_usd\":0.0750394,\"permission_denials\":[{\"tool_name\":\"Bash\",\"tool_use_id\":\"toolu_6\"},{\"tool_name\":\"Bash\"},{\"tool_name\":\"WebFetch\"}],"
		"\"is_error\":false,\"num_turns\":6,\"subtype\":\"success\",\"result\":\"I renamed `Add` to `Sum` in \\u201cMaths.cpp\\u201d.\",\"type\":\"result\"}";


//////////////////////////////////////////////////////////////////////////
//
// The rename's events, whatever pieces its output arrived in.
//
//////////////////////////////////////////////////////////////////////////
void AssertTestClaudeRename(CClaudeStream* pcStream, CArrayClaudeEvents* paEvents)
{
	CClaudeEvent*	pcEvent;

	AssertSize(17, paEvents->NumElements());

	pcEvent = paEvents->Get(0);
	AssertInt(CET_Started, pcEvent->GetType());
	AssertString("claude-opus-5-5", pcEvent->GetDetail());

	pcEvent = paEvents->Get(1);
	AssertInt(CET_Text, pcEvent->GetType());
	AssertString("I'll find Add.", pcEvent->GetText());

	pcEvent = paEvents->Get(2);
	AssertInt(CET_ToolUse, pcEvent->GetType());
	AssertString("Grep", pcEvent->GetTool());
	AssertString("\\bAdd\\b", pcEvent->GetDetail());
	AssertString("", pcEvent->GetFile());

	pcEvent = paEvents->Get(3);
	AssertInt(CET_ToolResult, pcEvent->GetType());
	AssertString("Maths.cpp:1:int Add(int a, int b);", pcEvent->GetText());
	AssertFalse(pcEvent->IsError());

	pcEvent = paEvents->Get(4);
	AssertString("Read", pcEvent->GetTool());
	AssertString("D:\\Code\\Maths.cpp", pcEvent->GetDetail());
	AssertString("", pcEvent->GetFile());

	pcEvent = paEvents->Get(5);
	AssertInt(CET_ToolResult, pcEvent->GetType());
	AssertString("1\tint Add(int a, int b);\n2\t", pcEvent->GetText());

	pcEvent = paEvents->Get(6);
	AssertString("Edit", pcEvent->GetTool());
	AssertString("D:\\Code\\Other.h", pcEvent->GetFile());
	pcEvent = paEvents->Get(7);
	AssertString("Edit", pcEvent->GetTool());
	AssertString("D:\\Code\\Maths.cpp", pcEvent->GetFile());

	pcEvent = paEvents->Get(8);
	AssertInt(CET_ToolResult, pcEvent->GetType());
	AssertTrue(pcEvent->IsError());
	pcEvent = paEvents->Get(9);
	AssertFalse(pcEvent->IsError());

	pcEvent = paEvents->Get(10);
	AssertString("Write", pcEvent->GetTool());
	AssertString("D:\\Code\\Maths.cpp", pcEvent->GetFile());
	pcEvent = paEvents->Get(11);
	AssertString("Bash", pcEvent->GetTool());
	AssertString("msbuild Maths.vcxproj", pcEvent->GetDetail());
	AssertString("", pcEvent->GetFile());
	AssertFalse(paEvents->Get(12)->IsError());
	AssertTrue(paEvents->Get(13)->IsError());

	pcEvent = paEvents->Get(14);
	AssertInt(CET_Text, pcEvent->GetType());
	AssertString("I renamed `Add` to `Sum` in \xE2\x80\x9CMaths.cpp\xE2\x80\x9D.", pcEvent->GetText());

	pcEvent = paEvents->Get(15);
	AssertInt(CET_Finished, pcEvent->GetType());
	AssertFalse(pcEvent->IsError());
	AssertString("I renamed `Add` to `Sum` in \xE2\x80\x9CMaths.cpp\xE2\x80\x9D.", pcEvent->GetText());
	AssertString("0.0750394", pcEvent->GetDetail());
	AssertString("Bash, WebFetch", pcEvent->GetDenied());

	//The last line had no line end, so it was read by End.
	pcEvent = paEvents->Get(16);
	AssertInt(CET_Output, pcEvent->GetType());
	AssertString("Done.", pcEvent->GetText());

	AssertString("d844c748-d234", pcStream->GetSessionId());
	AssertTrue(pcStream->IsFinished());
	AssertTrue(pcStream->Succeeded());

	//The refused edit (Other.h) changed nothing; Maths.cpp changed twice is once.
	AssertSize(1, pcStream->GetChangedFiles()->NumElements());
	AssertString("D:\\Code\\Maths.cpp", pcStream->GetChangedFiles()->GetText(0));
}


//////////////////////////////////////////////////////////////////////////
//
// The output arriving whole, in pieces of every size from one byte, and
// with Windows line ends.
//
//////////////////////////////////////////////////////////////////////////
void TestClaudeStreamPieces(void)
{
	CClaudeStream		cStream;
	CArrayClaudeEvents	aEvents;
	CChars				szOutput;
	size				uiPiece;
	size				uiStart;
	size				uiLength;

	szOutput.Init(gszTestClaudeRename);
	szOutput.Append("\nDone.");

	for (uiPiece = 1; uiPiece <= szOutput.Length(); uiPiece += (uiPiece < 64) ? 1 : 997)
	{
		cStream.Init();
		aEvents.Init();
		for (uiStart = 0; uiStart < szOutput.Length(); uiStart += uiPiece)
		{
			uiLength = szOutput.Length() - uiStart;
			if (uiLength > uiPiece)
			{
				uiLength = uiPiece;
			}
			cStream.Append(szOutput.Text() + uiStart, uiLength, &aEvents);
		}
		AssertSize(16, aEvents.NumElements());
		cStream.End(&aEvents);
		AssertTestClaudeRename(&cStream, &aEvents);
		aEvents.Kill();
		cStream.Kill();
	}

	cStream.Init();
	aEvents.Init();
	szOutput.Replace("\n", "\r\n");
	cStream.Append(szOutput.Text(), szOutput.Length(), &aEvents);
	cStream.End(&aEvents);
	AssertTestClaudeRename(&cStream, &aEvents);
	aEvents.Kill();
	cStream.Kill();

	szOutput.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A run that failed: an error result, and lines that aren't JSON shown as
// they are.  A second run keeps the session but nothing else.
//
//////////////////////////////////////////////////////////////////////////
void TestClaudeStreamFailed(void)
{
	CClaudeStream		cStream;
	CArrayClaudeEvents	aEvents;
	const char*			szOutput;
	CClaudeEvent*		pcEvent;

	cStream.Init();
	aEvents.Init();

	szOutput =	"{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"first\"}\n"
				"{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"id\":\"t1\",\"name\":\"Edit\",\"input\":{\"file_path\":\"A.cpp\"}}]}}\n"
				"\n"
				"[1, 2]\n"
				"{\"type\":\"result\",\"subtype\":\"error_max_turns\",\"is_error\":true,\"session_id\":\"second\"}\n";
	cStream.Append(szOutput, strlen(szOutput), &aEvents);
	cStream.End(&aEvents);

	AssertSize(4, aEvents.NumElements());
	AssertInt(CET_Started, aEvents.Get(0)->GetType());
	AssertString("", aEvents.Get(0)->GetDetail());
	AssertInt(CET_ToolUse, aEvents.Get(1)->GetType());
	AssertInt(CET_Output, aEvents.Get(2)->GetType());
	AssertString("[1, 2]", aEvents.Get(2)->GetText());
	pcEvent = aEvents.Get(3);
	AssertInt(CET_Finished, pcEvent->GetType());
	AssertTrue(pcEvent->IsError());
	AssertString("error_max_turns", pcEvent->GetText());
	AssertString("", pcEvent->GetDenied());
	AssertTrue(cStream.IsFinished());
	AssertFalse(cStream.Succeeded());
	AssertString("second", cStream.GetSessionId());

	//The edit was never answered.
	AssertSize(0, cStream.GetChangedFiles()->NumElements());

	cStream.Restart();
	AssertFalse(cStream.IsFinished());
	AssertString("second", cStream.GetSessionId());
	aEvents.Clear();
	szOutput = "Error: Invalid API key";
	cStream.Append(szOutput, strlen(szOutput), &aEvents);
	AssertSize(0, aEvents.NumElements());
	cStream.End(&aEvents);
	AssertSize(1, aEvents.NumElements());
	AssertInt(CET_Output, aEvents.Get(0)->GetType());
	AssertString("Error: Invalid API key", aEvents.Get(0)->GetText());
	AssertFalse(cStream.IsFinished());

	aEvents.Kill();
	cStream.Kill();

	AssertTrue(IsClaudeEditTool("MultiEdit"));
	AssertFalse(IsClaudeEditTool("Read"));
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void TestClaudeStream(void)
{
	BeginTests();

	TestClaudeStreamPieces();
	TestClaudeStreamFailed();

	TestStatistics();
}

