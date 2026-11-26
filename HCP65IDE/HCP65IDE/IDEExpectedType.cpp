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
#include <ctype.h>
#include <string.h>
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDERename.h"
#include "IDESymbolInfo.h"
#include "IDEExpectedType.h"


#define IDE_EXPECTED_STATEMENT	2000	//How far back a statement's start, or a switch, is looked for.


//////////////////////////////////////////////////////////////////////////
//
// A type as it is written, to compare with another: without its spaces,
// its const and volatile and the words that say what kind of type its
// name is ("const struct SPoint *" is "SPoint*").
//
//////////////////////////////////////////////////////////////////////////
void NormaliseIDEType(const char* szType, CChars* pszDest)
{
	static const char*	aszWords[] = { "const", "volatile", "struct", "class", "enum", "union", "typename" };
	size				uiLength;
	size				i;
	size				j;
	bool				bWord;

	for (i = 0; szType[i] != '\0'; )
	{
		if ((szType[i] == ' ') || (szType[i] == '\t') || (szType[i] == '&'))
		{
			i++;
			continue;
		}
		bWord = false;
		if ((i == 0) || !IsIDENameChar(szType[i - 1]))
		{
			for (j = 0; !bWord && (j < sizeof(aszWords) / sizeof(const char*)); j++)
			{
				uiLength = strlen(aszWords[j]);
				if ((strncmp(&szType[i], aszWords[j], uiLength) == 0) && !IsIDENameChar(szType[i + uiLength]))
				{
					i += uiLength;
					bWord = true;
				}
			}
		}
		if (!bWord)
		{
			//Two words keep a space between them: "unsigned int".
			if (IsIDENameChar(szType[i]) && (i > 0) && (szType[i - 1] == ' ') && !pszDest->Empty() && IsIDENameChar(pszDest->Text()[pszDest->Length() - 1]))
			{
				pszDest->Append(' ');
			}
			pszDest->Append(szType[i]);
			i++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpectedTypes::Init(CIDEContent* pcContent, CIDELightTyper* pcTyper)
{
	mpcContent = pcContent;
	mpcTyper = pcTyper;
	masTypes.Init();
}


void CIDEExpectedTypes::Kill(void)
{
	Clear();
	masTypes.Kill();
}


void CIDEExpectedTypes::Clear(void)
{
	size	i;

	for (i = 0; i < masTypes.NumElements(); i++)
	{
		masTypes.Get(i)->szText.Kill();
		masTypes.Get(i)->sType.Kill();
	}
	masTypes.ReInit();
}


size CIDEExpectedTypes::NumTypes(void) const { return masTypes.NumElements(); }


//////////////////////////////////////////////////////////////////////////
//
// A type that fits, once: as the typer has it and as it is written.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpectedTypes::AddType(SIDELightType* psType, const char* szType)
{
	SIDEExpectedType*	psExpected;
	CChars				szText;
	size				i;

	szText.Init();
	NormaliseIDEType(szType, &szText);
	for (i = 0; i < masTypes.NumElements(); i++)
	{
		if (masTypes.Get(i)->szText.Equals(szText))
		{
			szText.Kill();
			return;
		}
	}
	psExpected = masTypes.Add();
	psExpected->sType.Init();
	psExpected->sType.szKey.Set(psType->szKey);
	psExpected->sType.uiKind = psType->uiKind;
	psExpected->sType.iPointers = psType->iPointers;
	psExpected->szText.Init(szText);
	szText.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// A type that fits, as it is written in the scope szDeclaredIn.
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpectedTypes::Add(const char* szType, const char* szDeclaredIn)
{
	SIDELightType	sType;

	sType.Init();
	mpcTyper->TypeOfText(szType, szDeclaredIn, &sType);
	AddType(&sType, szType);
	sType.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The expression is an argument of a call: the parameter it is given to,
// of each function the call could be of, less the parameter's name and
// what it starts as.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExpectedTypes::FindArgument(const char* szText, size uiExpression)
{
	CArrayIDEScopeSymbols	asFunctions;
	SIDEScopeSymbol*		psFunction;
	SIDESymbolInfo			sInfo;
	CChars					szType;
	const char*				szParameter;
	size					uiNameStart;
	size					uiNameEnd;
	size					uiOpen;
	size					uiArgument;
	size					uiChain;
	size					uiEnd;
	size					i;

	if (!FindIDECall(szText, uiExpression, &uiNameStart, &uiNameEnd, &uiOpen, &uiArgument) || !FindIDEChainStart(szText, uiNameEnd, &uiChain))
	{
		return false;
	}
	asFunctions.Init();
	mpcTyper->FindChainSymbols(szText, uiChain, uiNameEnd, &asFunctions);
	for (i = 0; i < asFunctions.NumElements(); i++)
	{
		psFunction = asFunctions.Get(i);
		sInfo.Init();
		if (((psFunction->uiKind == CPPSK_Function) || (psFunction->uiKind == CPPSK_FunctionTemplate)) && ReadIDESymbolInfo(mpcContent, psFunction->szKey.Text(), &sInfo) && (uiArgument < sInfo.aszParameters.NumElements()))
		{
			//"EPaintMode eMode = PM_Fill": to its "=", then less its last name.
			szParameter = sInfo.aszParameters.GetText(uiArgument);
			for (uiEnd = 0; (szParameter[uiEnd] != '\0') && (szParameter[uiEnd] != '='); uiEnd++)
			{
			}
			while ((uiEnd > 0) && (szParameter[uiEnd - 1] == ' '))
			{
				uiEnd--;
			}
			while ((uiEnd > 0) && IsIDENameChar(szParameter[uiEnd - 1]))
			{
				uiEnd--;
			}
			szType.Init();
			szType.Append(szParameter, uiEnd);
			if (!szType.Empty())
			{
				Add(szType.Text(), psFunction->szScope.Text());
			}
			szType.Kill();
		}
		sInfo.Kill();
	}
	KillIDEScopeSymbols(&asFunctions);
	return masTypes.NumElements() > 0;
}


//////////////////////////////////////////////////////////////////////////
//
// The expression is the right of the "=", "==" or "!=" at uiOperator:
// what the left is, by the typer, or failing that what it is declared as;
// or, where the left is a variable being declared, which the index
// hasn't yet, the type it is declared with: what is written before its
// name, from its statement's start.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExpectedTypes::FindAssigned(const char* szText, size uiOperator)
{
	CArrayIDEScopeSymbols	asLeft;
	SIDELightType			sType;
	CChars					szType;
	size					uiChain;
	size					uiStart;
	size					uiEnd;
	bool					bResult;

	for (uiEnd = uiOperator; (uiEnd > 0) && ((szText[uiEnd - 1] == ' ') || (szText[uiEnd - 1] == '\t')); uiEnd--)
	{
	}
	if (!FindIDEChainStart(szText, uiEnd, &uiChain))
	{
		return false;
	}
	sType.Init();
	szType.Init();
	asLeft.Init();
	bResult = mpcTyper->TypeOfChain(szText, uiChain, uiEnd, &sType);
	if (bResult)
	{
		//As it would be written: its class and a star for each level.
		szType.Append(sType.szKey);
		szType.Append('*', (size)sType.iPointers);
		AddType(&sType, szType.Text());
	}
	else if (mpcTyper->FindChainSymbols(szText, uiChain, uiEnd, &asLeft) && mpcTyper->ReadDeclaredType(asLeft.Get(0)->szKey.Text(), &szType))
	{
		//Something the index has that is no class's: an int, say, compared as it is written.
		Add(szType.Text(), asLeft.Get(0)->szScope.Text());
		bResult = true;
	}
	else
	{
		for (uiStart = uiChain; (uiStart > 0) && (uiChain - uiStart < IDE_EXPECTED_STATEMENT) && (szText[uiStart - 1] != ';') && (szText[uiStart - 1] != '{') && (szText[uiStart - 1] != '}') && (szText[uiStart - 1] != '(') && (szText[uiStart - 1] != ','); uiStart--)
		{
		}
		szType.Append(&szText[uiStart], uiChain - uiStart);
		szType.StripWhitespace(true);
		bResult = !szType.Empty();
		if (bResult)
		{
			Add(szType.Text(), "");
		}
	}
	KillIDEScopeSymbols(&asLeft);
	szType.Kill();
	sType.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The expression is a case's, the word at uiCase: what the nearest switch
// before it switches on.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExpectedTypes::FindSwitched(const char* szText, size uiCase)
{
	SIDELightType	sType;
	CChars			szType;
	size			uiSwitch;
	size			uiOpen;
	size			uiClose;
	size			uiChain;
	int				iDepth;
	bool			bResult;

	for (uiSwitch = uiCase; (uiSwitch >= 6) && (uiCase - uiSwitch < IDE_EXPECTED_STATEMENT); uiSwitch--)
	{
		if ((strncmp(&szText[uiSwitch - 6], "switch", 6) == 0) && !IsIDENameChar(szText[uiSwitch]) && ((uiSwitch == 6) || !IsIDENameChar(szText[uiSwitch - 7])))
		{
			break;
		}
	}
	if ((uiSwitch < 6) || (uiCase - uiSwitch >= IDE_EXPECTED_STATEMENT))
	{
		return false;
	}
	for (uiOpen = uiSwitch; (szText[uiOpen] == ' ') || (szText[uiOpen] == '\t'); uiOpen++)
	{
	}
	if (szText[uiOpen] != '(')
	{
		return false;
	}
	iDepth = 0;
	for (uiClose = uiOpen; uiClose < uiCase; uiClose++)
	{
		if (szText[uiClose] == '(')
		{
			iDepth++;
		}
		else if (szText[uiClose] == ')')
		{
			iDepth--;
			if (iDepth == 0)
			{
				break;
			}
		}
	}
	if ((uiClose >= uiCase) || !FindIDEChainStart(szText, uiClose, &uiChain) || (uiChain <= uiOpen))
	{
		return false;
	}
	sType.Init();
	szType.Init();
	bResult = mpcTyper->TypeOfChain(szText, uiChain, uiClose, &sType);
	if (bResult)
	{
		szType.Append(sType.szKey);
		AddType(&sType, szType.Text());
	}
	szType.Kill();
	sType.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The types that would fit an expression that starts at uiExpression of
// szText: by what is before it.  False if nothing there says.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEExpectedTypes::Find(const char* szText, size uiLength, size uiExpression)
{
	SIDELightType	sType;
	CChars			szType;
	size			uiBefore;
	bool			bResult;

	Clear();
	if (uiExpression > uiLength)
	{
		return false;
	}
	for (uiBefore = uiExpression; (uiBefore > 0) && ((szText[uiBefore - 1] == ' ') || (szText[uiBefore - 1] == '\t') || (szText[uiBefore - 1] == '\r') || (szText[uiBefore - 1] == '\n')); uiBefore--)
	{
	}
	if (uiBefore == 0)
	{
		return false;
	}

	if (szText[uiBefore - 1] == '=')
	{
		//"=", "==" and "!=": the operator starts where its other characters do.
		uiBefore--;
		if ((uiBefore > 0) && ((szText[uiBefore - 1] == '=') || (szText[uiBefore - 1] == '!')))
		{
			uiBefore--;
		}
		else if ((uiBefore > 0) && (strchr("<>+-*/%&|^", szText[uiBefore - 1]) != NULL))
		{
			return false;
		}
		return FindAssigned(szText, uiBefore);
	}
	if ((uiBefore >= 6) && (strncmp(&szText[uiBefore - 6], "return", 6) == 0) && ((uiBefore == 6) || !IsIDENameChar(szText[uiBefore - 7])))
	{
		sType.Init();
		szType.Init();
		bResult = mpcTyper->TypeOfFunction(&sType, &szType);
		if (bResult)
		{
			AddType(&sType, szType.Text());
		}
		szType.Kill();
		sType.Kill();
		return bResult;
	}
	if ((uiBefore >= 4) && (strncmp(&szText[uiBefore - 4], "case", 4) == 0) && ((uiBefore == 4) || !IsIDENameChar(szText[uiBefore - 5])))
	{
		return FindSwitched(szText, uiBefore - 4);
	}
	if ((szText[uiBefore - 1] == '(') || (szText[uiBefore - 1] == ','))
	{
		return FindArgument(szText, uiExpression);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// How well what a symbol is fits one of the types: what it is declared
// as (a function, what it returns; an enumerator, its enum) is the same
// type, by the typer or as it is written, or is a pointer to a class
// derived from the class expected.
//
//////////////////////////////////////////////////////////////////////////
EIDETypeFit CIDEExpectedTypes::GetFit(SIDEScopeSymbol* psSymbol)
{
	SIDEExpectedType*	psExpected;
	SIDELightType		sType;
	CArrayChars			aszBases;
	CChars				szDeclared;
	CChars				szText;
	EIDETypeFit			eFit;
	size				i;
	bool				bTyped;

	if ((psSymbol->uiKind != CPPSK_Variable) && (psSymbol->uiKind != CPPSK_Field) && (psSymbol->uiKind != CPPSK_Parameter) && (psSymbol->uiKind != CPPSK_Enumerator) &&
		(psSymbol->uiKind != CPPSK_Function) && (psSymbol->uiKind != CPPSK_FunctionTemplate))
	{
		return ITF_None;
	}
	sType.Init();
	szDeclared.Init();
	szText.Init();
	if (psSymbol->uiKind == CPPSK_Enumerator)
	{
		bTyped = mpcTyper->TypeOfSymbol(psSymbol, &sType);
	}
	else
	{
		//Its declaration is read once, for what the typer makes of it and for how it is written.
		bTyped = mpcTyper->ReadDeclaredType(psSymbol->szKey.Text(), &szDeclared) && mpcTyper->TypeOfText(szDeclared.Text(), psSymbol->szScope.Text(), &sType);
		NormaliseIDEType(szDeclared.Text(), &szText);
	}

	eFit = ITF_None;
	aszBases.Init();
	for (i = 0; (eFit != ITF_Same) && (i < masTypes.NumElements()); i++)
	{
		psExpected = masTypes.Get(i);
		if (bTyped && !psExpected->sType.szKey.Empty())
		{
			if (sType.szKey.Equals(psExpected->sType.szKey) && (sType.iPointers == psExpected->sType.iPointers))
			{
				eFit = ITF_Same;
			}
			else if ((sType.iPointers == psExpected->sType.iPointers) && (sType.iPointers > 0))
			{
				aszBases.ReInit();
				mpcContent->GetCodeModel()->FindClassBases(sType.szKey.Text(), true, &aszBases);
				if (aszBases.GetIndex(psExpected->sType.szKey.Text()) != ARRAY_ELEMENT_NOT_FOUND)
				{
					eFit = ITF_Converts;
				}
			}
		}
		else if (!bTyped && psExpected->sType.szKey.Empty() && !szText.Empty() && szText.Equals(psExpected->szText))
		{
			eFit = ITF_Same;
		}
	}
	aszBases.Kill();
	szText.Kill();
	szDeclared.Kill();
	sType.Kill();
	return eFit;
}


//////////////////////////////////////////////////////////////////////////
//
// For checking: the types, as they are compared: "CGauge*, int".
//
//////////////////////////////////////////////////////////////////////////
void CIDEExpectedTypes::Print(CChars* pszDest)
{
	size	i;

	for (i = 0; i < masTypes.NumElements(); i++)
	{
		pszDest->Append((i > 0) ? ", " : "");
		pszDest->Append(masTypes.Get(i)->szText);
	}
}
