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
#include "BaseLib/FileUtil.h"
#include "BaseLib/StringHelper.h"
#include "IDEIcons.h"
#include "IDECodeModel.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void KillIDEScopeSymbols(CArrayIDEScopeSymbols* pasSymbols)
{
	SIDEScopeSymbol*	psSymbol;
	size				i;

	for (i = 0; i < pasSymbols->NumElements(); i++)
	{
		psSymbol = pasSymbols->Get(i);
		psSymbol->sDeclared.szFileName.Kill();
		psSymbol->sPlace.szFileName.Kill();
		psSymbol->szParameters.Kill();
		psSymbol->szName.Kill();
		psSymbol->szScope.Kill();
		psSymbol->szKey.Kill();
	}
	pasSymbols->Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Whether szKey is of something the scope szScope declares itself
// ("CChars::Append(char const*)" in "CChars", "ClampGauge(int,int)" in
// ""): the key is the scope, "::" and a name with no "::" of its own
// before its parameters or outside its template arguments.  *puiName is
// where the name starts.  Nothing is copied: every key of the index is
// asked this.
//
//////////////////////////////////////////////////////////////////////////
bool FindIDEKeyName(const char* szKey, const char* szScope, size uiScopeLength, size* puiName)
{
	size	i;
	int		iAngles;

	i = 0;
	if (uiScopeLength > 0)
	{
		if ((strncmp(szKey, szScope, uiScopeLength) != 0) || (szKey[uiScopeLength] != ':') || (szKey[uiScopeLength + 1] != ':'))
		{
			return false;
		}
		i = uiScopeLength + 2;
	}
	*puiName = i;
	if (szKey[i] == '\0')
	{
		return false;
	}
	iAngles = 0;
	for (; (szKey[i] != '\0') && (szKey[i] != '('); i++)
	{
		if (szKey[i] == '<')
		{
			iAngles++;
		}
		else if ((szKey[i] == '>') && (iAngles > 0))
		{
			iAngles--;
		}
		else if ((szKey[i] == ':') && (szKey[i + 1] == ':') && (iAngles == 0))
		{
			//operator's own name can't have one; a deeper scope's member has.
			return false;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDEScopeQuery::Init(CArrayChars* paszInScopes)
{
	memset(this, 0, sizeof(SIDEScopeQuery));
	paszScopes = paszInScopes;
}


//////////////////////////////////////////////////////////////////////////
//
// Whether a word of the name szName starts with c, whatever its case:
// its first character, an upper case letter after a lower case one or a
// digit, or a letter or digit after an underscore.
//
//////////////////////////////////////////////////////////////////////////
static bool HasIDEWordStart(const char* szName, size uiLength, char c)
{
	size	i;
	char	cLower;
	char	cName;
	char	cBefore;

	cLower = (char)tolower((unsigned char)c);
	for (i = 0; i < uiLength; i++)
	{
		cName = szName[i];
		if ((char)tolower((unsigned char)cName) != cLower)
		{
			continue;
		}
		if (i == 0)
		{
			return true;
		}
		cBefore = szName[i - 1];
		if ((cBefore == '_') || (isupper((unsigned char)cName) && !isupper((unsigned char)cBefore)) || (isdigit((unsigned char)cName) != isdigit((unsigned char)cBefore)) ||
			(isupper((unsigned char)cName) && (i + 1 < uiLength) && islower((unsigned char)szName[i + 1])))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// What psQuery asks for (SIDEScopeQuery), added to pasDest; the number
// added.  One pass of the index, in which nothing is copied until a
// symbol is known to be wanted: every key is looked at on a key press.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindScopeSymbols(SIDEScopeQuery* psQuery, CArrayIDEScopeSymbols* pasDest)
{
	CCPPIndexSymbol*	pcSymbol;
	CCPPIndexSymbol*	pcFunction;
	SCPPIndexLocation*	psLocation;
	SIDEScopeSymbol*	psDest;
	CArrayInt			aiFound;
	CArrayInt			aiScopeOf;
	CChars				szEnum;
	const char*			szKey;
	const char*			szName;
	size				uiFirst;
	size				uiName;
	size				uiNameLength;
	size				uiEnum;
	size				i;
	size				j;
	int32				iFunction;
	int					iScope;
	int					iFoundIn;
	int					iEnumerators;

	uiFirst = pasDest->NumElements();
	iEnumerators = (int)psQuery->paszScopes->NumElements();
	aiFound.Init();
	aiScopeOf.Init();
	szEnum.Init();
	mcIndexMutex.Lock();
	iFunction = -1;
	if (psQuery->szFunctionFile != NULL)
	{
		pcFunction = mcIndex.FindSymbolAt(psQuery->szFunctionFile, psQuery->iFunctionLine, psQuery->iFunctionColumn);
		for (i = 0; pcFunction && (i < mcIndex.NumSymbols()); i++)
		{
			if (mcIndex.GetSymbol(i) == pcFunction)
			{
				iFunction = (int32)i;
				break;
			}
		}
	}

	//Which symbols, and for each what has it: -1 the function, a scope's index, or after the scopes an enum of theirs.
	for (i = 0; i < mcIndex.NumSymbols(); i++)
	{
		pcSymbol = mcIndex.GetSymbol(i);
		szKey = pcSymbol->mszKey.Text();
		if (pcSymbol->meKind == CPPSK_Label)
		{
			continue;
		}
		iFoundIn = -2;
		uiName = 0;
		if (szKey[0] == '@')
		{
			//A local's name is after its key's last colon.
			psLocation = (iFunction >= 0) ? pcSymbol->GetDeclaration() : NULL;
			if (psLocation == NULL)
			{
				psLocation = (iFunction >= 0) ? pcSymbol->GetDefinition() : NULL;
			}
			if (psLocation && (psLocation->iContainer == iFunction))
			{
				iFoundIn = -1;
				szName = strrchr(szKey, ':');
				uiName = szName ? (size)(szName + 1 - szKey) : 0;
			}
		}
		else
		{
			for (j = 0; (iFoundIn == -2) && (j < psQuery->paszScopes->NumElements()); j++)
			{
				if (FindIDEKeyName(szKey, psQuery->paszScopes->GetText(j), psQuery->paszScopes->Get(j)->Length(), &uiName))
				{
					iFoundIn = (int)j;
				}
			}
			if ((iFoundIn == -2) && psQuery->bEnumerators && (pcSymbol->meKind == CPPSK_Enumerator))
			{
				//Its enum is what is before its name's "::": is that one a scope declares?
				szName = strrchr(szKey, ':');
				uiEnum = (szName && (szName > szKey + 1)) ? (size)(szName - 1 - szKey) : 0;
				szEnum.Set("");
				szEnum.Append(szKey, uiEnum);
				for (j = 0; (uiEnum > 0) && (iFoundIn == -2) && (j < psQuery->paszScopes->NumElements()); j++)
				{
					if (FindIDEKeyName(szEnum.Text(), psQuery->paszScopes->GetText(j), psQuery->paszScopes->Get(j)->Length(), &uiName))
					{
						iFoundIn = iEnumerators;
						uiName = uiEnum + 2;
					}
				}
			}
		}
		if (iFoundIn == -2)
		{
			continue;
		}

		//Its name, to its parameters.
		szName = &szKey[uiName];
		for (uiNameLength = 0; (szName[uiNameLength] != '\0') && (szName[uiNameLength] != '('); uiNameLength++)
		{
		}
		if ((psQuery->szName != NULL) && ((uiNameLength != psQuery->uiNameLength) || (strncmp(szName, psQuery->szName, uiNameLength) != 0)))
		{
			continue;
		}
		if ((psQuery->cWordStart != 0) && !HasIDEWordStart(szName, uiNameLength, psQuery->cWordStart))
		{
			continue;
		}
		aiFound.Add((int)i);
		aiScopeOf.Add(iFoundIn);
	}

	//In the scopes' order: the function's first, the enumerators last.
	for (iScope = -1; iScope <= iEnumerators; iScope++)
	{
		for (i = 0; i < aiFound.NumElements(); i++)
		{
			if (aiScopeOf.GetValue(i) != iScope)
			{
				continue;
			}
			if ((psQuery->uiMaximum != 0) && (pasDest->NumElements() - uiFirst >= psQuery->uiMaximum))
			{
				break;
			}
			pcSymbol = mcIndex.GetSymbol((size)aiFound.GetValue(i));
			psLocation = GetIDESymbolPlace(pcSymbol);
			if (psLocation == NULL)
			{
				continue;
			}
			psDest = pasDest->Add();
			psDest->szKey.Init(pcSymbol->mszKey);
			psDest->uiKind = (uint8)pcSymbol->meKind;
			if (iScope < 0)
			{
				psDest->szScope.Init();
				GetIDESymbolName(pcSymbol->mszKey.Text(), &psDest->szName);
				psDest->szParameters.Init();
			}
			else
			{
				SplitIDESymbolKey(pcSymbol->mszKey.Text(), &psDest->szScope, &psDest->szName, &psDest->szParameters);
			}
			CopyLocation(psLocation, &psDest->sPlace);
			CopyLocation(pcSymbol->GetDeclaration() ? pcSymbol->GetDeclaration() : psLocation, &psDest->sDeclared);
		}
	}
	mcIndexMutex.Unlock();
	szEnum.Kill();
	aiScopeOf.Kill();
	aiFound.Kill();
	return pasDest->NumElements() - uiFirst;
}


//////////////////////////////////////////////////////////////////////////
//
// The type, namespace or template a name written in one of paszScopes
// means: the first of the scopes, in their order (the innermost first,
// "" last for outside any), that declares it.  A name with its own scope
// ("NS::CFoo") is looked for the same way, and then as it is.  False if
// none has it.
//
//////////////////////////////////////////////////////////////////////////
bool CIDECodeModel::FindTypeSymbol(const char* szName, CArrayChars* paszScopes, CChars* pszKey, uint8* puiKind)
{
	CCPPIndexSymbol*	pcSymbol;
	CChars				szKey;
	size				i;

	pcSymbol = NULL;
	szKey.Init();
	mcIndexMutex.Lock();
	for (i = 0; (pcSymbol == NULL) && (i <= paszScopes->NumElements()); i++)
	{
		szKey.Set("");
		if ((i < paszScopes->NumElements()) && !paszScopes->Get(i)->Empty())
		{
			szKey.Append(paszScopes->GetText(i));
			szKey.Append("::");
		}
		szKey.Append(szName);
		pcSymbol = mcIndex.FindSymbol(szKey.Text());
		if (pcSymbol && (pcSymbol->meKind != CPPSK_Namespace) && (pcSymbol->meKind != CPPSK_Class) && (pcSymbol->meKind != CPPSK_Enum) && (pcSymbol->meKind != CPPSK_Typedef) &&
			(pcSymbol->meKind != CPPSK_ClassTemplate) && (pcSymbol->meKind != CPPSK_AliasTemplate))
		{
			pcSymbol = NULL;
		}
	}
	if (pcSymbol)
	{
		pszKey->Set(pcSymbol->mszKey);
		*puiKind = (uint8)pcSymbol->meKind;
	}
	mcIndexMutex.Unlock();
	szKey.Kill();
	return pcSymbol != NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// What an #include in szIncluder could name next, after the folders it
// names so far (szFolder: "BaseLib/", "" for none): the headers and the
// folders that each directory it is looked for in has there (the
// includer's own first unless bSystem, then its project's include
// directories, then the compiler's).  Their names are added to paszFiles
// and paszFolders, each once, in that order; how many.
//
//////////////////////////////////////////////////////////////////////////
size CIDECodeModel::FindIncludeNames(const char* szIncluder, const char* szFolder, bool bSystem, CArrayChars* paszFiles, CArrayChars* paszFolders)
{
	CFileUtil	cFileUtil;
	CArrayChars	aszDirectories;
	CArrayChars	aszFound;
	CChars		szDirectory;
	const char*	szName;
	size		uiFirst;
	size		i;
	size		j;
	int			iKind;

	uiFirst = paszFiles->NumElements() + paszFolders->NumElements();
	aszDirectories.Init();
	GetIncludeDirectories(FindIncludeProject(szIncluder), szIncluder, bSystem, &aszDirectories);
	for (i = 0; i < aszDirectories.NumElements(); i++)
	{
		szDirectory.Init(aszDirectories.GetText(i));
		cFileUtil.AppendToPath(&szDirectory, szFolder);
		for (iKind = 0; iKind < 2; iKind++)
		{
			aszFound.Init();
			if (iKind == 0)
			{
				cFileUtil.FindAllDirectories(szDirectory.Text(), &aszFound);
			}
			else
			{
				cFileUtil.FindAllFiles(szDirectory.Text(), &aszFound);
			}
			for (j = 0; j < aszFound.NumElements(); j++)
			{
				szName = GetIDEFileNamePart(aszFound.GetText(j));
				if ((szName[0] == '\0') || (szName[0] == '.'))
				{
					continue;
				}
				if (iKind == 0)
				{
					if (paszFolders->GetIndex(szName) == ARRAY_ELEMENT_NOT_FOUND)
					{
						paszFolders->Add(szName);
					}
				}
				else if ((CIDEIcons::GetFileType(szName) == IFT_Header) && (paszFiles->GetIndex(szName) == ARRAY_ELEMENT_NOT_FOUND))
				{
					paszFiles->Add(szName);
				}
			}
			aszFound.Kill();
		}
		szDirectory.Kill();
	}
	aszDirectories.Kill();
	return paszFiles->NumElements() + paszFolders->NumElements() - uiFirst;
}
