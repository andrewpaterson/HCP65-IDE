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
#include <string.h>
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "CppCompilerLib/CPPIndex.h"
#include "IDEContent.h"
#include "IDELocationPreview.h"
#include "IDERename.h"
#include "IDEClassAccess.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEClassAccess::Init(void)
{
	mszClass.Init();
	mszFileName.Init();
	muiBodyStart = 0;
	muiBodyEnd = 0;
	masChanges.Init();
	maiLineStarts.Init();
}


void CIDEClassAccess::Kill(void)
{
	maiLineStarts.Kill();
	masChanges.Kill();
	mszFileName.Kill();
	mszClass.Kill();
}


const char* CIDEClassAccess::GetClass(void) { return mszClass.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// The word of szText that ends at uiEnd is szWord.
//
//////////////////////////////////////////////////////////////////////////
static bool IsIDEWordBefore(const char* szText, size uiEnd, const char* szWord)
{
	size	uiLength;

	uiLength = strlen(szWord);
	return (uiEnd >= uiLength) && (strncmp(&szText[uiEnd - uiLength], szWord, uiLength) == 0) && ((uiEnd == uiLength) || !IsIDENameChar(szText[uiEnd - uiLength - 1]));
}


//////////////////////////////////////////////////////////////////////////
//
// The class szClass's access read from where the index has it defined.
// False if it has it defined nowhere, its file can't be read or its body
// isn't found: then everything of it is taken to be public.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEClassAccess::Read(CIDEContent* pcContent, const char* szClass)
{
	static const char*	aszAccess[] = { "public", "protected", "private" };
	CArrayIDELocations	asLocations;
	SIDELocation*		psDefinition;
	SIDEAccessChange*	psChange;
	CTextFile			cFile;
	const char*			szText;
	size				uiLength;
	size				uiName;
	size				i;
	size				j;
	size				uiWord;
	int					iDepth;
	uint8				uiKind;
	char				cQuote;

	mszClass.Set(szClass);
	mszFileName.Set("");
	masChanges.ReInit();
	maiLineStarts.ReInit();
	asLocations.Init();
	psDefinition = NULL;
	if (pcContent->GetCodeModel()->GetSymbolLocations(szClass, &asLocations, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
	{
		for (i = 0; (psDefinition == NULL) && (i < asLocations.NumElements()); i++)
		{
			if (asLocations.Get(i)->uiRole == CPPIR_Definition)
			{
				psDefinition = asLocations.Get(i);
			}
		}
	}
	cFile.Init();
	if (psDefinition && pcContent->GetFindInFiles()->GetFileText(psDefinition->szFileName.Text(), &cFile, &szText, &uiLength))
	{
		//From its name to its body's "{": what it starts as is by the word before its name.
		uiName = FindIDETextPosition(szText, uiLength, psDefinition->iLine, psDefinition->iColumn);
		for (i = uiName; (i > 0) && ((szText[i - 1] == ' ') || (szText[i - 1] == '\t')); i--)
		{
		}
		psChange = masChanges.Add();
		psChange->eAccess = IsIDEWordBefore(szText, i, "class") ? IA_Private : IA_Public;
		for (i = uiName; (i < uiLength) && (szText[i] != '{') && (szText[i] != ';'); i++)
		{
		}
		if ((i < uiLength) && (szText[i] == '{'))
		{
			muiBodyStart = i + 1;
			psChange->uiFrom = muiBodyStart;

			//Its body: the access words of its own level, a string's and a comment's text left alone.
			iDepth = 1;
			cQuote = 0;
			for (i = muiBodyStart; (i < uiLength) && (iDepth > 0); i++)
			{
				if (cQuote != 0)
				{
					if ((szText[i] == '\\') && (i + 1 < uiLength))
					{
						i++;
					}
					else if ((szText[i] == cQuote) || (szText[i] == '\n'))
					{
						cQuote = 0;
					}
				}
				else if ((szText[i] == '"') || (szText[i] == '\''))
				{
					cQuote = szText[i];
				}
				else if ((szText[i] == '/') && (i + 1 < uiLength) && (szText[i + 1] == '/'))
				{
					for (; (i < uiLength) && (szText[i] != '\n'); i++)
					{
					}
				}
				else if ((szText[i] == '/') && (i + 1 < uiLength) && (szText[i + 1] == '*'))
				{
					for (i += 2; (i + 1 < uiLength) && !((szText[i] == '*') && (szText[i + 1] == '/')); i++)
					{
					}
				}
				else if (szText[i] == '{')
				{
					iDepth++;
				}
				else if (szText[i] == '}')
				{
					iDepth--;
				}
				else if ((szText[i] == ':') && (iDepth == 1) && (i + 1 < uiLength) && (szText[i + 1] != ':') && (szText[i - 1] != ':'))
				{
					for (uiWord = i; (uiWord > muiBodyStart) && ((szText[uiWord - 1] == ' ') || (szText[uiWord - 1] == '\t')); uiWord--)
					{
					}
					for (j = 0; j < 3; j++)
					{
						if (IsIDEWordBefore(szText, uiWord, aszAccess[j]))
						{
							psChange = masChanges.Add();
							psChange->uiFrom = i;
							psChange->eAccess = (EIDEAccess)j;
						}
					}
				}
			}
			muiBodyEnd = i;
			mszFileName.Set(psDefinition->szFileName);
			maiLineStarts.Add(0);
			for (i = 0; i < muiBodyEnd; i++)
			{
				if (szText[i] == '\n')
				{
					maiLineStarts.Add((int)(i + 1));
				}
			}
		}
	}
	cFile.Kill();
	KillIDELocations(&asLocations);
	return !mszFileName.Empty();
}


//////////////////////////////////////////////////////////////////////////
//
// The access of what the class declares at psDeclared: by the last change
// of access before it in the class's body.  Public for a place that isn't
// in the body.
//
//////////////////////////////////////////////////////////////////////////
EIDEAccess CIDEClassAccess::GetAccess(SIDELocation* psDeclared)
{
	EIDEAccess	eAccess;
	size		uiPosition;
	size		i;

	if (mszFileName.Empty() || (StringInsensitiveCompare(mszFileName.Text(), psDeclared->szFileName.Text()) != 0) || (psDeclared->iLine < 0) || ((size)psDeclared->iLine >= maiLineStarts.NumElements()))
	{
		return IA_Public;
	}
	uiPosition = (size)maiLineStarts.GetValue((size)psDeclared->iLine) + (size)psDeclared->iColumn;
	if ((uiPosition < muiBodyStart) || (uiPosition >= muiBodyEnd))
	{
		return IA_Public;
	}
	eAccess = IA_Public;
	for (i = 0; (i < masChanges.NumElements()) && (masChanges.Get(i)->uiFrom <= uiPosition); i++)
	{
		eAccess = masChanges.Get(i)->eAccess;
	}
	return eAccess;
}
