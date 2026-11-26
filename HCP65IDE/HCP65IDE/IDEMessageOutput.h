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
#ifndef __IDE_MESSAGE_OUTPUT_H__
#define __IDE_MESSAGE_OUTPUT_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/TextArea.h"
#include "IDEBuildPane.h"
#include "IDEOutputMenu.h"


class CIDEContent;


//What a message says it is.  An error is one that doesn't say it is anything else.
enum EIDEMessageKind
{
	IMK_Error,
	IMK_Warning,
	IMK_Note,
};


//A message in an output tab that says a place in a file.
struct SIDEOutputMessage
{
	size			uiOutputLine;  //Its line in the tab, from 0.
	CChars			szFile;  //The full path of the file it is about.
	int32			iLine;  //From 1.
	int32			iColumn;  //From 1, 1 if it doesn't say.
	EIDEMessageKind	eKind;
};


typedef CArrayTemplate<SIDEOutputMessage>	CArrayIDEOutputMessages;


//An output tab's messages, as IntelliJ IDEA's Build window's: Next and Previous Occurrence (Control+Alt+Down and Up)
//step through them, and Enter, a double click or Jump to Source (F4) on one opens its place in its editor, which has
//the keys.  The tab's context menu (CIDEOutputMenu) has Jump to Source and Clear All after its Copies.  A message is "<file> (<line>, <column>): ...", the HCP65 compiler's and the code model's,
//"<file>(<line>,<column>): ..." or "<file>(<line>): ...", MSVC's, or "<file>:<line>:<column>: error: ...", GCC's.
//Its file is a full path, or a name found in the code model's files (the one next to the unit being compiled, as
//"Kernel.cpp (1/12)" says, if more than one has it).  The Build and Indexing tabs have one each.
class CIDEMessageOutput : public CObject
{
CONSTRUCTABLE(CIDEMessageOutput);
DESTRUCTABLE(CIDEMessageOutput);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CIDEBuildPane>	mpPane;
	Ptr<CInputAction>	mpJumpAction;  //F4 in the tab opens the message the caret is on.
	Ptr<CIDEOutputMenu>	mpMenu;  //The tab's context menu.
	CChars				mszNoun;  //What its messages are called on the status line: "build messages".
	size				muiStepLine;  //The tab's line of the message last gone to, MAX_SIZE for none.

public:
	void			Init(CIDEContent* pcContent, Ptr<CIDEBuildPane> pPane, const char* szNoun);
	void			Class(void) override;
	void			Free(void) override;
	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	void			Clear(void);
	bool			HasMessages(void);
	bool			Step(bool bNext, bool bKeepKeys);
	bool			OpenLine(size uiLine, bool bKeepKeys);
	size			FindMessages(CArrayIDEOutputMessages* pasDest);

	Ptr<CIDEOutputMenu>	GetMenu(void) const;

	static void		KillMessages(CArrayIDEOutputMessages* pasMessages);
	static bool		ParseMessageLine(const char* szLine, CChars* pszFile, int32* piLine, int32* piColumn, EIDEMessageKind* peKind);
	static bool		ParseUnitLine(const char* szLine, CChars* pszName);

protected:
	void			Open(SIDEOutputMessage* psMessage, bool bKeepKeys);
	bool			FindMessageFile(const char* szName, const char* szUnit, CChars* pszDest);
	void			JumpToSource(void);
	bool			IsOnMessage(void);
	void			ClearOutput(void);
	void			EnterPressed(void);
	void			DoubleClicked(CTextArea* pcTextArea, size uiPosition);

	static bool		ParseBracketedPlace(const char* szLine, CChars* pszFile, int32* piLine, int32* piColumn, const char** pszMessage);
	static bool		ParseColonPlace(const char* szLine, CChars* pszFile, int32* piLine, int32* piColumn, const char** pszMessage);
	static bool		ReadNumber(const char** psz, int32* piNumber);
	static EIDEMessageKind	FindKind(const char* szMessage);
};


#endif // __IDE_MESSAGE_OUTPUT_H__

