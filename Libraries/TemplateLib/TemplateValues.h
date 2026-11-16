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
#ifndef __TEMPLATE_VALUES_H__
#define __TEMPLATE_VALUES_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/Chars.h"


enum ETemplateValue
{
	TV_Null,		//No value: a variable nobody set.
	TV_String,
	TV_Number,		//A whole number.
	TV_Bool,
	TV_List,		//Strings, for #foreach.
};


//A value a template's variable or expression has.
struct STemplateValue
{
	ETemplateValue	eKind;
	CChars			szText;
	int64			lliNumber;
	bool			bValue;
	CArrayChars		aszList;

	void	Init(void);
	void	Kill(void);
	void	SetNull(void);
	void	SetString(const char* szString);
	void	SetString(const char* szString, size uiLength);
	void	SetNumber(int64 lliValue);
	void	SetBool(bool bTrue);
	void	SetList(CArrayChars* paszItems);
	void	Copy(STemplateValue* psSource);
	bool	IsNull(void);
	bool	IsTrue(void);
	bool	GetNumber(int64* plliValue);
	char*	AppendText(CChars* pszDest);
};


typedef CArrayTemplate<STemplateValue>	CArrayTemplateValues;


//The variables a template is applied with, by name: those its caller sets (Set, SetList) and those its #set sets.
//A #foreach's variable is pushed over any of its name (Push) and taken off when the #foreach ends (Release to the
//Mark before it), so the newest of a name is the one found.
class CTemplateValues
{
protected:
	CArrayChars				maszNames;
	CArrayTemplateValues	masValues;

public:
	void			Init(void);
	void			Kill(void);

	void			Set(const char* szName, const char* szText);
	void			SetNumber(const char* szName, int64 lliValue);
	void			SetBool(const char* szName, bool bValue);
	void			SetList(const char* szName, CArrayChars* paszItems);
	void			Set(const char* szName, STemplateValue* psValue);
	void			Push(const char* szName, STemplateValue* psValue);
	size			Mark(void);
	void			Release(size uiMark);
	STemplateValue*	Get(const char* szName);
	bool			Has(const char* szName);
	size			NumValues(void);
	char*			GetName(size uiIndex);

protected:
	size			Find(const char* szName);
	STemplateValue*	Add(const char* szName);
};


#endif // __TEMPLATE_VALUES_H__
