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
#include <string.h>
#include "JSONDocument.h"


//Deeper than this is refused, so a hostile text can't run the stack out.
#define JSON_MAX_DEPTH	256


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CJSONDocument::Init(void)
{
	masNodes.Init();
	mszText.Init();
	mszError.Init();
	mszPos = NULL;
	mszEnd = NULL;
	muiDepth = 0;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CJSONDocument::Kill(void)
{
	mszError.Kill();
	mszText.Kill();
	masNodes.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// No values.
//
//////////////////////////////////////////////////////////////////////////
void CJSONDocument::Clear(void)
{
	masNodes.Kill();
	masNodes.Init();
	mszText.Clear();
	mszError.Clear();
}


//////////////////////////////////////////////////////////////////////////
//
// Reads the uiLength bytes of szText, one value with only whitespace
// around it, as the document's values in place of those it had.  False,
// with no values and GetError saying why, if it isn't JSON.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::Parse(const char* szText, size uiLength)
{
	bool	bResult;

	Clear();
	mszPos = szText;
	mszEnd = szText + uiLength;
	muiDepth = 0;

	SkipWhitespace();
	bResult = ParseValue(JSON_NONE, JSON_NONE);
	if (bResult)
	{
		SkipWhitespace();
		if (mszPos != mszEnd)
		{
			bResult = Error("Expected the end of the text");
		}
	}

	if (!bResult)
	{
		masNodes.Kill();
		masNodes.Init();
		mszText.Clear();
	}
	mszPos = NULL;
	mszEnd = NULL;
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::Parse(const char* szText)
{
	return Parse(szText, strlen(szText));
}


//////////////////////////////////////////////////////////////////////////
//
// The value the text is, JSON_NONE if none was read.
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::GetRoot(void)
{
	return (masNodes.NumElements() > 0) ? 0 : JSON_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// JT_Null for JSON_NONE.
//
//////////////////////////////////////////////////////////////////////////
EJSONType CJSONDocument::GetType(size uiNode)
{
	if (uiNode == JSON_NONE)
	{
		return JT_Null;
	}
	return masNodes.Get(uiNode)->eType;
}


//////////////////////////////////////////////////////////////////////////
//
// An object's member's name, NULL for an array's element or the root.
//
//////////////////////////////////////////////////////////////////////////
char* CJSONDocument::GetName(size uiNode)
{
	if ((uiNode == JSON_NONE) || (masNodes.Get(uiNode)->uiName == JSON_NONE))
	{
		return NULL;
	}
	return mszText.Text(masNodes.Get(uiNode)->uiName);
}


//////////////////////////////////////////////////////////////////////////
//
// A string's text, or a number as it was written.  NULL for anything
// else.
//
//////////////////////////////////////////////////////////////////////////
char* CJSONDocument::GetText(size uiNode)
{
	if ((uiNode == JSON_NONE) || (masNodes.Get(uiNode)->uiText == JSON_NONE))
	{
		return NULL;
	}
	return mszText.Text(masNodes.Get(uiNode)->uiText);
}


//////////////////////////////////////////////////////////////////////////
//
// An object's first member or an array's first element, JSON_NONE if it
// has none.
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::GetFirst(size uiNode)
{
	if (uiNode == JSON_NONE)
	{
		return JSON_NONE;
	}
	return masNodes.Get(uiNode)->uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// The member or element after uiNode, JSON_NONE after the last.
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::GetNext(size uiNode)
{
	if (uiNode == JSON_NONE)
	{
		return JSON_NONE;
	}
	return masNodes.Get(uiNode)->uiNext;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::NumChildren(size uiNode)
{
	size	uiChild;
	size	uiCount;

	uiCount = 0;
	for (uiChild = GetFirst(uiNode); uiChild != JSON_NONE; uiChild = GetNext(uiChild))
	{
		uiCount++;
	}
	return uiCount;
}


//////////////////////////////////////////////////////////////////////////
//
// uiObject's member named szName, JSON_NONE if it has none or isn't an
// object.  With two of the same name the first is found.
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::GetMember(size uiObject, const char* szName)
{
	size	uiChild;

	if (GetType(uiObject) != JT_Object)
	{
		return JSON_NONE;
	}
	for (uiChild = GetFirst(uiObject); uiChild != JSON_NONE; uiChild = GetNext(uiChild))
	{
		if (strcmp(GetName(uiChild), szName) == 0)
		{
			return uiChild;
		}
	}
	return JSON_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// uiArray's element at uiIndex, JSON_NONE past its end.
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::GetElement(size uiArray, size uiIndex)
{
	size	uiChild;

	if (GetType(uiArray) != JT_Array)
	{
		return JSON_NONE;
	}
	for (uiChild = GetFirst(uiArray); uiChild != JSON_NONE; uiChild = GetNext(uiChild))
	{
		if (uiIndex == 0)
		{
			return uiChild;
		}
		uiIndex--;
	}
	return JSON_NONE;
}


//////////////////////////////////////////////////////////////////////////
//
// The text of uiObject's string (or number) member szName, NULL if it
// hasn't one.
//
//////////////////////////////////////////////////////////////////////////
char* CJSONDocument::GetString(size uiObject, const char* szName)
{
	return GetText(GetMember(uiObject, szName));
}


//////////////////////////////////////////////////////////////////////////
//
// uiObject's true or false member szName, bDefault if it hasn't one.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::GetBool(size uiObject, const char* szName, bool bDefault)
{
	EJSONType	eType;

	eType = GetType(GetMember(uiObject, szName));
	if (eType == JT_True)
	{
		return true;
	}
	if (eType == JT_False)
	{
		return false;
	}
	return bDefault;
}


//////////////////////////////////////////////////////////////////////////
//
// A value after uiParent's last child (none for the root).
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::AddNode(EJSONType eType, size uiParent, size uiName)
{
	SJSONNode*	psNode;
	SJSONNode*	psParent;
	size		uiNode;

	uiNode = masNodes.NumElements();
	psNode = masNodes.Add();
	psNode->eType = eType;
	psNode->uiName = uiName;
	psNode->uiText = JSON_NONE;
	psNode->uiFirst = JSON_NONE;
	psNode->uiLast = JSON_NONE;
	psNode->uiNext = JSON_NONE;

	if (uiParent != JSON_NONE)
	{
		psParent = masNodes.Get(uiParent);
		if (psParent->uiLast == JSON_NONE)
		{
			psParent->uiFirst = uiNode;
		}
		else
		{
			masNodes.Get(psParent->uiLast)->uiNext = uiNode;
		}
		psParent->uiLast = uiNode;
	}
	return uiNode;
}


//////////////////////////////////////////////////////////////////////////
//
// szText's uiLength bytes, ended with a '\0', and where they start.
//
//////////////////////////////////////////////////////////////////////////
size CJSONDocument::AddText(const char* szText, size uiLength)
{
	size	uiStart;

	uiStart = mszText.Length();
	mszText.Append(szText, uiLength);
	mszText.Append('\0');
	return uiStart;
}


//////////////////////////////////////////////////////////////////////////
//
// The value at mszPos, as uiParent's next child named uiName.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::ParseValue(size uiParent, size uiName)
{
	size	uiNode;
	size	uiText;
	bool	bResult;

	if (mszPos == mszEnd)
	{
		return Error("Expected a value");
	}

	switch (*mszPos)
	{
	case '{':
	case '[':
		if (muiDepth >= JSON_MAX_DEPTH)
		{
			return Error("Too deep");
		}
		muiDepth++;
		uiNode = AddNode((*mszPos == '{') ? JT_Object : JT_Array, uiParent, uiName);
		mszPos++;
		bResult = (masNodes.Get(uiNode)->eType == JT_Object) ? ParseObject(uiNode) : ParseArray(uiNode);
		muiDepth--;
		return bResult;

	case '"':
		if (!ParseString(&uiText))
		{
			return false;
		}
		uiNode = AddNode(JT_String, uiParent, uiName);
		masNodes.Get(uiNode)->uiText = uiText;
		return true;

	case 't':
		return ParseLiteral("true", JT_True, uiParent, uiName);
	case 'f':
		return ParseLiteral("false", JT_False, uiParent, uiName);
	case 'n':
		return ParseLiteral("null", JT_Null, uiParent, uiName);

	default:
		return ParseNumber(uiParent, uiName);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// After the '{'.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::ParseObject(size uiNode)
{
	size	uiName;

	SkipWhitespace();
	if ((mszPos != mszEnd) && (*mszPos == '}'))
	{
		mszPos++;
		return true;
	}

	for (;;)
	{
		SkipWhitespace();
		if ((mszPos == mszEnd) || (*mszPos != '"'))
		{
			return Error("Expected a member's name");
		}
		if (!ParseString(&uiName))
		{
			return false;
		}
		SkipWhitespace();
		if ((mszPos == mszEnd) || (*mszPos != ':'))
		{
			return Error("Expected ':'");
		}
		mszPos++;
		SkipWhitespace();
		if (!ParseValue(uiNode, uiName))
		{
			return false;
		}
		SkipWhitespace();
		if (mszPos == mszEnd)
		{
			return Error("Expected ',' or '}'");
		}
		if (*mszPos == '}')
		{
			mszPos++;
			return true;
		}
		if (*mszPos != ',')
		{
			return Error("Expected ',' or '}'");
		}
		mszPos++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// After the '['.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::ParseArray(size uiNode)
{
	SkipWhitespace();
	if ((mszPos != mszEnd) && (*mszPos == ']'))
	{
		mszPos++;
		return true;
	}

	for (;;)
	{
		SkipWhitespace();
		if (!ParseValue(uiNode, JSON_NONE))
		{
			return false;
		}
		SkipWhitespace();
		if (mszPos == mszEnd)
		{
			return Error("Expected ',' or ']'");
		}
		if (*mszPos == ']')
		{
			mszPos++;
			return true;
		}
		if (*mszPos != ',')
		{
			return Error("Expected ',' or ']'");
		}
		mszPos++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The string at mszPos, from its opening quote, unescaped into the
// document's text.  \u escapes become UTF-8, a surrogate pair as one
// character.  The text's own bytes are kept as they are.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::ParseString(size* puiText)
{
	CChars		sz;
	char		c;
	uint32		uiCodePoint;
	uint32		uiLow;
	const char*	szRun;

	mszPos++;
	sz.Init();
	for (;;)
	{
		//Characters that need no unescaping are copied together.
		szRun = mszPos;
		while ((mszPos != mszEnd) && (*mszPos != '"') && (*mszPos != '\\') && ((unsigned char)*mszPos >= 0x20))
		{
			mszPos++;
		}
		if (mszPos != szRun)
		{
			sz.Append(szRun, (size)(mszPos - szRun));
		}

		if (mszPos == mszEnd)
		{
			sz.Kill();
			return Error("Expected '\"'");
		}
		if (*mszPos == '"')
		{
			mszPos++;
			break;
		}
		if (*mszPos != '\\')
		{
			sz.Kill();
			return Error("Control character in a string");
		}

		mszPos++;
		if (mszPos == mszEnd)
		{
			sz.Kill();
			return Error("Expected an escape");
		}
		c = *mszPos;
		mszPos++;
		switch (c)
		{
		case '"':	sz.Append('"');		break;
		case '\\':	sz.Append('\\');	break;
		case '/':	sz.Append('/');		break;
		case 'b':	sz.Append('\b');	break;
		case 'f':	sz.Append('\f');	break;
		case 'n':	sz.Append('\n');	break;
		case 'r':	sz.Append('\r');	break;
		case 't':	sz.Append('\t');	break;
		case 'u':
			if (!ParseHex(&uiCodePoint))
			{
				sz.Kill();
				return false;
			}
			if ((uiCodePoint >= 0xD800) && (uiCodePoint <= 0xDBFF))
			{
				//A high surrogate: with a low one after it, the two are one character.
				if (((mszEnd - mszPos) >= 6) && (mszPos[0] == '\\') && (mszPos[1] == 'u'))
				{
					mszPos += 2;
					if (!ParseHex(&uiLow))
					{
						sz.Kill();
						return false;
					}
					if ((uiLow >= 0xDC00) && (uiLow <= 0xDFFF))
					{
						uiCodePoint = 0x10000 + ((uiCodePoint - 0xD800) << 10) + (uiLow - 0xDC00);
					}
					else
					{
						AppendUTF8(&sz, 0xFFFD);
						uiCodePoint = uiLow;
					}
				}
				else
				{
					uiCodePoint = 0xFFFD;
				}
			}
			else if ((uiCodePoint >= 0xDC00) && (uiCodePoint <= 0xDFFF))
			{
				uiCodePoint = 0xFFFD;
			}
			AppendUTF8(&sz, uiCodePoint);
			break;
		default:
			sz.Kill();
			return Error("Unknown escape");
		}
	}

	*puiText = AddText(sz.Text(), sz.Length());
	sz.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// The four hex digits after a \u.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::ParseHex(uint32* puiValue)
{
	size	i;
	char	c;
	uint32	uiValue;

	if ((mszEnd - mszPos) < 4)
	{
		return Error("Expected four hex digits");
	}

	uiValue = 0;
	for (i = 0; i < 4; i++)
	{
		c = mszPos[i];
		uiValue <<= 4;
		if ((c >= '0') && (c <= '9'))
		{
			uiValue |= (uint32)(c - '0');
		}
		else if ((c >= 'a') && (c <= 'f'))
		{
			uiValue |= (uint32)(c - 'a' + 10);
		}
		else if ((c >= 'A') && (c <= 'F'))
		{
			uiValue |= (uint32)(c - 'A' + 10);
		}
		else
		{
			return Error("Expected four hex digits");
		}
	}
	mszPos += 4;
	*puiValue = uiValue;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CJSONDocument::AppendUTF8(CChars* psz, uint32 uiCodePoint)
{
	if (uiCodePoint < 0x80)
	{
		psz->Append((char)uiCodePoint);
	}
	else if (uiCodePoint < 0x800)
	{
		psz->Append((char)(0xC0 | (uiCodePoint >> 6)));
		psz->Append((char)(0x80 | (uiCodePoint & 0x3F)));
	}
	else if (uiCodePoint < 0x10000)
	{
		psz->Append((char)(0xE0 | (uiCodePoint >> 12)));
		psz->Append((char)(0x80 | ((uiCodePoint >> 6) & 0x3F)));
		psz->Append((char)(0x80 | (uiCodePoint & 0x3F)));
	}
	else
	{
		psz->Append((char)(0xF0 | (uiCodePoint >> 18)));
		psz->Append((char)(0x80 | ((uiCodePoint >> 12) & 0x3F)));
		psz->Append((char)(0x80 | ((uiCodePoint >> 6) & 0x3F)));
		psz->Append((char)(0x80 | (uiCodePoint & 0x3F)));
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A number, kept as it was written: -, digits (no leading zeros), a
// fraction and an exponent.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::ParseNumber(size uiParent, size uiName)
{
	const char*	szStart;
	size		uiNode;

	szStart = mszPos;
	if ((mszPos != mszEnd) && (*mszPos == '-'))
	{
		mszPos++;
	}
	if ((mszPos == mszEnd) || (*mszPos < '0') || (*mszPos > '9'))
	{
		mszPos = szStart;
		return Error("Expected a value");
	}
	if (*mszPos == '0')
	{
		mszPos++;
	}
	else
	{
		while ((mszPos != mszEnd) && (*mszPos >= '0') && (*mszPos <= '9'))
		{
			mszPos++;
		}
	}

	if ((mszPos != mszEnd) && (*mszPos == '.'))
	{
		mszPos++;
		if ((mszPos == mszEnd) || (*mszPos < '0') || (*mszPos > '9'))
		{
			return Error("Expected a digit after '.'");
		}
		while ((mszPos != mszEnd) && (*mszPos >= '0') && (*mszPos <= '9'))
		{
			mszPos++;
		}
	}

	if ((mszPos != mszEnd) && ((*mszPos == 'e') || (*mszPos == 'E')))
	{
		mszPos++;
		if ((mszPos != mszEnd) && ((*mszPos == '+') || (*mszPos == '-')))
		{
			mszPos++;
		}
		if ((mszPos == mszEnd) || (*mszPos < '0') || (*mszPos > '9'))
		{
			return Error("Expected an exponent");
		}
		while ((mszPos != mszEnd) && (*mszPos >= '0') && (*mszPos <= '9'))
		{
			mszPos++;
		}
	}

	uiNode = AddNode(JT_Number, uiParent, uiName);
	masNodes.Get(uiNode)->uiText = AddText(szStart, (size)(mszPos - szStart));
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// true, false or null.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::ParseLiteral(const char* szLiteral, EJSONType eType, size uiParent, size uiName)
{
	size	uiLength;

	uiLength = strlen(szLiteral);
	if (((size)(mszEnd - mszPos) < uiLength) || (memcmp(mszPos, szLiteral, uiLength) != 0))
	{
		return Error("Expected a value");
	}
	mszPos += uiLength;
	AddNode(eType, uiParent, uiName);
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CJSONDocument::SkipWhitespace(void)
{
	while ((mszPos != mszEnd) && ((*mszPos == ' ') || (*mszPos == '\t') || (*mszPos == '\n') || (*mszPos == '\r')))
	{
		mszPos++;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Always false.
//
//////////////////////////////////////////////////////////////////////////
bool CJSONDocument::Error(const char* szMessage)
{
	if (mszError.Empty())
	{
		mszError.Append(szMessage);
	}
	return false;
}


char* CJSONDocument::GetError(void) { return mszError.Text(); }

