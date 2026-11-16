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
#include <string.h>
#include "TemplateValues.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::Init(void)
{
	eKind = TV_Null;
	szText.Init();
	lliNumber = 0;
	bValue = false;
	aszList.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::Kill(void)
{
	aszList.Kill();
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::SetNull(void)
{
	eKind = TV_Null;
	szText.Clear();
	lliNumber = 0;
	bValue = false;
	aszList.ReInit();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::SetString(const char* szString)
{
	SetNull();
	eKind = TV_String;
	szText.Set(szString ? szString : "");
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::SetString(const char* szString, size uiLength)
{
	SetNull();
	eKind = TV_String;
	szText.Append(szString, uiLength);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::SetNumber(int64 lliValue)
{
	SetNull();
	eKind = TV_Number;
	lliNumber = lliValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::SetBool(bool bTrue)
{
	SetNull();
	eKind = TV_Bool;
	bValue = bTrue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::SetList(CArrayChars* paszItems)
{
	size	i;

	SetNull();
	eKind = TV_List;
	for (i = 0; paszItems && (i < paszItems->NumElements()); i++)
	{
		aszList.Add(paszItems->GetText(i));
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void STemplateValue::Copy(STemplateValue* psSource)
{
	SetList(&psSource->aszList);
	eKind = psSource->eKind;
	szText.Set(psSource->szText);
	lliNumber = psSource->lliNumber;
	bValue = psSource->bValue;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool STemplateValue::IsNull(void)
{
	return eKind == TV_Null;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether #if takes it as true, as Velocity 2 does (its empty check): not
// nothing, not false, not 0, not an empty string or list.
//
//////////////////////////////////////////////////////////////////////////
bool STemplateValue::IsTrue(void)
{
	switch (eKind)
	{
	case TV_String:
		return !szText.Empty();
	case TV_Number:
		return lliNumber != 0;
	case TV_Bool:
		return bValue;
	case TV_List:
		return aszList.NumElements() > 0;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Its value as a whole number: a number's, or a string's that is one
// ("12", "-3").  False if it isn't one.
//
//////////////////////////////////////////////////////////////////////////
bool STemplateValue::GetNumber(int64* plliValue)
{
	const char*	sz;
	int64		lliValue;
	bool		bNegative;

	if (eKind == TV_Number)
	{
		*plliValue = lliNumber;
		return true;
	}
	if (eKind != TV_String)
	{
		return false;
	}
	sz = szText.Text();
	bNegative = (*sz == '-');
	sz += bNegative ? 1 : 0;
	if ((*sz < '0') || (*sz > '9'))
	{
		return false;
	}
	for (lliValue = 0; (*sz >= '0') && (*sz <= '9'); sz++)
	{
		lliValue = lliValue * 10 + (*sz - '0');
	}
	if (*sz != '\0')
	{
		return false;
	}
	*plliValue = bNegative ? -lliValue : lliValue;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// As it is written into a template's output: a list as Velocity writes
// one ("[a, b]"), nothing as nothing.
//
//////////////////////////////////////////////////////////////////////////
char* STemplateValue::AppendText(CChars* pszDest)
{
	size	i;

	switch (eKind)
	{
	case TV_String:
		pszDest->Append(szText);
		break;
	case TV_Number:
		pszDest->Append(lliNumber);
		break;
	case TV_Bool:
		pszDest->Append(bValue ? "true" : "false");
		break;
	case TV_List:
		pszDest->Append('[');
		for (i = 0; i < aszList.NumElements(); i++)
		{
			pszDest->Append((i > 0) ? ", " : "")->Append(aszList.GetText(i));
		}
		pszDest->Append(']');
		break;
	default:
		break;
	}
	return pszDest->Text();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::Init(void)
{
	maszNames.Init();
	masValues.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::Kill(void)
{
	Release(0);
	masValues.Kill();
	maszNames.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The newest variable of a name, ARRAY_ELEMENT_NOT_FOUND if there is none.
//
//////////////////////////////////////////////////////////////////////////
size CTemplateValues::Find(const char* szName)
{
	size	i;

	for (i = maszNames.NumElements(); i > 0; i--)
	{
		if (maszNames.Get(i - 1)->Equals(szName))
		{
			return i - 1;
		}
	}
	return ARRAY_ELEMENT_NOT_FOUND;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
STemplateValue* CTemplateValues::Add(const char* szName)
{
	STemplateValue*	psValue;

	maszNames.Add(szName);
	psValue = masValues.Add();
	psValue->Init();
	return psValue;
}


//////////////////////////////////////////////////////////////////////////
//
// A variable given a value: the newest of its name if there is one (so a
// #set in a #foreach sets what it can see), otherwise a new one.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::Set(const char* szName, STemplateValue* psValue)
{
	size	uiIndex;

	uiIndex = Find(szName);
	if (uiIndex == ARRAY_ELEMENT_NOT_FOUND)
	{
		Add(szName)->Copy(psValue);
	}
	else
	{
		masValues.Get(uiIndex)->Copy(psValue);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::Set(const char* szName, const char* szText)
{
	STemplateValue	sValue;

	sValue.Init();
	sValue.SetString(szText);
	Set(szName, &sValue);
	sValue.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::SetNumber(const char* szName, int64 lliValue)
{
	STemplateValue	sValue;

	sValue.Init();
	sValue.SetNumber(lliValue);
	Set(szName, &sValue);
	sValue.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::SetBool(const char* szName, bool bValue)
{
	STemplateValue	sValue;

	sValue.Init();
	sValue.SetBool(bValue);
	Set(szName, &sValue);
	sValue.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::SetList(const char* szName, CArrayChars* paszItems)
{
	STemplateValue	sValue;

	sValue.Init();
	sValue.SetList(paszItems);
	Set(szName, &sValue);
	sValue.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A variable of a name over any there is (a #foreach's), until Release.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::Push(const char* szName, STemplateValue* psValue)
{
	Add(szName)->Copy(psValue);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
size CTemplateValues::Mark(void)
{
	return maszNames.NumElements();
}


//////////////////////////////////////////////////////////////////////////
//
// The variables added since a Mark taken off.
//
//////////////////////////////////////////////////////////////////////////
void CTemplateValues::Release(size uiMark)
{
	while (maszNames.NumElements() > uiMark)
	{
		masValues.Tail()->Kill();
		masValues.Pop();
		maszNames.RemoveTail();
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
STemplateValue* CTemplateValues::Get(const char* szName)
{
	size	uiIndex;

	uiIndex = Find(szName);
	return (uiIndex == ARRAY_ELEMENT_NOT_FOUND) ? NULL : masValues.Get(uiIndex);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CTemplateValues::Has(const char* szName) { return Find(szName) != ARRAY_ELEMENT_NOT_FOUND; }
size CTemplateValues::NumValues(void) { return maszNames.NumElements(); }
char* CTemplateValues::GetName(size uiIndex) { return maszNames.GetText(uiIndex); }
