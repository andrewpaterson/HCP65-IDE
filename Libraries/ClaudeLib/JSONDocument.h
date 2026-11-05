/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela ClaudeLib

Codaphela ClaudeLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela ClaudeLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela ClaudeLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __JSON_DOCUMENT_H__
#define __JSON_DOCUMENT_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"


//No value: a member or element that isn't there.
#define JSON_NONE	((size)-1)


enum EJSONType
{
	JT_Null,
	JT_False,
	JT_True,
	JT_Number,
	JT_String,
	JT_Array,
	JT_Object,
};


//A value in the document.  Its name (an object's member's) and text (a string's, unescaped as UTF-8, or a number's as
//it was written) are offsets into the document's text, JSON_NONE if it has none.  Its children are a list: its first
//and each one's next.
struct SJSONNode
{
	EJSONType	eType;
	size		uiName;
	size		uiText;
	size		uiFirst;
	size		uiLast;
	size		uiNext;
};


typedef CArrayTemplate<SJSONNode>	CArrayJSONNodes;


//One JSON text (RFC 8259) read into values, which are found by their index: the root, an object's members by name and
//an array's elements in order.  BaseLib's CJSONParser reads JSON into a markup document instead, and doesn't read bare
//true, false and null, or \u escapes, which Claude's output has.
class CJSONDocument
{
protected:
	CArrayJSONNodes	masNodes;
	CChars			mszText;		//Names and texts, each ended with a '\0'.
	CChars			mszError;		//Why the last Parse failed.

	const char*		mszPos;			//While parsing.
	const char*		mszEnd;
	size			muiDepth;

public:
	void		Init(void);
	void		Kill(void);

	bool		Parse(const char* szText, size uiLength);
	bool		Parse(const char* szText);
	void		Clear(void);

	size		GetRoot(void);
	EJSONType	GetType(size uiNode);
	char*		GetName(size uiNode);
	char*		GetText(size uiNode);
	size		GetFirst(size uiNode);
	size		GetNext(size uiNode);
	size		NumChildren(size uiNode);
	size		GetMember(size uiObject, const char* szName);
	size		GetElement(size uiArray, size uiIndex);

	char*		GetString(size uiObject, const char* szName);
	bool		GetBool(size uiObject, const char* szName, bool bDefault);
	char*		GetError(void);

protected:
	size		AddNode(EJSONType eType, size uiParent, size uiName);
	size		AddText(const char* szText, size uiLength);
	bool		ParseValue(size uiParent, size uiName);
	bool		ParseObject(size uiNode);
	bool		ParseArray(size uiNode);
	bool		ParseString(size* puiText);
	bool		ParseNumber(size uiParent, size uiName);
	bool		ParseLiteral(const char* szLiteral, EJSONType eType, size uiParent, size uiName);
	bool		ParseHex(uint32* puiValue);
	void		AppendUTF8(CChars* psz, uint32 uiCodePoint);
	void		SkipWhitespace(void);
	bool		Error(const char* szMessage);
};


#endif // __JSON_DOCUMENT_H__

