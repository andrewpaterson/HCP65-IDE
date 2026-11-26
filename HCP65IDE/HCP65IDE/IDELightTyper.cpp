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
#include "IDELightTyper.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SIDELightType::Init(void)
{
	szKey.Init();
	uiKind = 0;
	iPointers = 0;
	szArgument.Init();
}


void SIDELightType::Kill(void)
{
	szArgument.Kill();
	szKey.Kill();
}


void SIDELightType::Clear(void)
{
	szKey.Set("");
	uiKind = 0;
	iPointers = 0;
	szArgument.Set("");
}


bool SIDELightType::Equals(SIDELightType* psOther)
{
	return szKey.Equals(psOther->szKey) && (iPointers == psOther->iPointers) && szArgument.Equals(psOther->szArgument);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDELightTyper::Init(CIDEContent* pcContent)
{
	mpcContent = pcContent;
	maszScopes.Init();
	mszClass.Init();
	mszFunctionFile.Init();
	miFunctionLine = 0;
	miFunctionColumn = 0;
	mbFunction = false;
}


void CIDELightTyper::Kill(void)
{
	mszFunctionFile.Kill();
	mszClass.Kill();
	maszScopes.Kill();
}


void CIDELightTyper::Forget(void)
{
	maszScopes.ReInit();
	mszClass.Set("");
	mszFunctionFile.Set("");
	mbFunction = false;
}


//////////////////////////////////////////////////////////////////////////
//
// szScope ("NS::CGauge") and each scope it is in ("NS") added, the
// innermost first: not the scope outside any.
//
//////////////////////////////////////////////////////////////////////////
void CIDELightTyper::AddScopesOf(const char* szScope, CArrayChars* paszDest)
{
	CChars	szOuter;
	size	uiEnd;
	size	i;
	int		iAngles;

	szOuter.Init(szScope);
	while (!szOuter.Empty())
	{
		if (paszDest->GetIndex(szOuter.Text()) == ARRAY_ELEMENT_NOT_FOUND)
		{
			paszDest->Add(szOuter.Text());
		}
		//Less its last name: after the last "::" outside a template's arguments.
		uiEnd = 0;
		iAngles = 0;
		for (i = 0; i + 1 < szOuter.Length(); i++)
		{
			if (szOuter.Text()[i] == '<')
			{
				iAngles++;
			}
			else if (szOuter.Text()[i] == '>')
			{
				iAngles--;
			}
			else if ((szOuter.Text()[i] == ':') && (szOuter.Text()[i + 1] == ':') && (iAngles == 0))
			{
				uiEnd = i;
			}
		}
		szOuter.SetLength(uiEnd);
	}
	szOuter.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// Where it is asked from: uiPosition of pTab's text.  The file's outline
// as it was last worked out says what that is in: the namespaces and the
// class round it, and the function, whose name may have its class before
// it ("CGauge::Reset").  The outline may be a few keys old: what the place
// was in then, it is taken to be in now.
//
//////////////////////////////////////////////////////////////////////////
void CIDELightTyper::SetPlace(Ptr<CIDEEditorTab> pTab, size uiPosition)
{
	Ptr<CTextArea>		pEditor;
	CIDEOutline*		pcOutline;
	SIDEOutlineEntry*	psEntry;
	CArrayChars			aszBases;
	CChars				szScope;
	CChars				szPart;
	CChars				szKey;
	const char*			szLast;
	size				uiLine;
	size				i;
	int32				iEntry;
	int32				iFunction;
	uint8				uiKind;

	Forget();
	pEditor = pTab->GetEditor();
	pcOutline = pTab->GetLastOutline();
	uiLine = pEditor->FindLine(uiPosition);
	iEntry = pcOutline->FindEntryAt((int)uiLine, (int)(uiPosition - pEditor->GetLineStart(uiLine)));
	if ((iEntry < 0) && !pTab->IsOutlineCurrent())
	{
		//Lines typed since the outline was worked out have pushed the place past the end of what it is in: it is
		//taken to be in the last thing that starts before it.
		for (i = 0; (i < pcOutline->NumEntries()) && (pcOutline->Get(i)->sRange.iLine <= (int32)uiLine); i++)
		{
			iEntry = (int32)i;
		}
	}
	iFunction = iEntry;
	while ((iFunction >= 0) && (pcOutline->Get((size)iFunction)->eKind != IOK_Function))
	{
		iFunction = pcOutline->Get((size)iFunction)->iParent;
	}

	//The scope the place is in: each entry round it that is a namespace or a class, outermost first, then what the
	//function's own name has before its last name.
	szScope.Init();
	for (; iEntry >= 0; iEntry = pcOutline->Get((size)iEntry)->iParent)
	{
		psEntry = pcOutline->Get((size)iEntry);
		szPart.Init();
		if ((psEntry->eKind == IOK_Namespace) || (psEntry->eKind == IOK_Class))
		{
			szPart.Append(psEntry->szName);
		}
		else if (iEntry == iFunction)
		{
			szLast = strrchr(psEntry->szName.Text(), ':');
			if (szLast && (szLast > psEntry->szName.Text()) && (szLast[-1] == ':'))
			{
				szPart.Append(psEntry->szName.Text(), (size)(szLast - 1 - psEntry->szName.Text()));
			}
		}
		if (!szPart.Empty())
		{
			if (!szScope.Empty())
			{
				szPart.Append("::");
			}
			szScope.Insert(0, szPart.Text());
		}
		szPart.Kill();
	}

	if (iFunction >= 0)
	{
		//The index has the function at its last name.
		psEntry = pcOutline->Get((size)iFunction);
		szLast = strrchr(psEntry->szName.Text(), ':');
		szLast = szLast ? szLast + 1 : psEntry->szName.Text();
		mszFunctionFile.Set(pTab->GetFileName());
		miFunctionLine = psEntry->sName.iEndLine;
		miFunctionColumn = psEntry->sName.iEndColumn - (int32)strlen(szLast);
		mbFunction = true;
	}

	//A class's members and its bases' come before what is round it.
	szKey.Init();
	maszScopes.Add("");
	if (!szScope.Empty() && mpcContent->GetCodeModel()->FindTypeSymbol(szScope.Text(), &maszScopes, &szKey, &uiKind) && ((uiKind == CPPSK_Class) || (uiKind == CPPSK_ClassTemplate)))
	{
		mszClass.Set(szKey);
	}
	maszScopes.ReInit();
	if (!mszClass.Empty())
	{
		maszScopes.Add(mszClass.Text());
		aszBases.Init();
		mpcContent->GetCodeModel()->FindClassBases(mszClass.Text(), true, &aszBases);
		for (i = 0; i < aszBases.NumElements(); i++)
		{
			maszScopes.Add(aszBases.GetText(i));
		}
		aszBases.Kill();
	}
	AddScopesOf(szScope.Text(), &maszScopes);
	maszScopes.Add("");
	szKey.Kill();
	szScope.Kill();
}


CArrayChars* CIDELightTyper::GetScopes(void) { return &maszScopes; }
const char* CIDELightTyper::GetClass(void) { return mszClass.Text(); }


//////////////////////////////////////////////////////////////////////////
//
// What is in scope at the place, added: the function's parameters and
// locals, what each scope declares itself, the innermost first, and then
// the enumerators of the enums among those (an enum's are written without
// it).  Only those named szName if it is given, only those with a word
// starting with cWordStart if that is, and no more than uiMaximum if that
// is.  The number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDELightTyper::FindVisible(CArrayIDEScopeSymbols* pasDest, const char* szName, size uiNameLength, char cWordStart, size uiMaximum)
{
	SIDEScopeQuery	sQuery;

	sQuery.Init(&maszScopes);
	if (mbFunction)
	{
		sQuery.szFunctionFile = mszFunctionFile.Text();
		sQuery.iFunctionLine = miFunctionLine;
		sQuery.iFunctionColumn = miFunctionColumn;
	}
	sQuery.bEnumerators = true;
	sQuery.szName = szName;
	sQuery.uiNameLength = uiNameLength;
	sQuery.cWordStart = cWordStart;
	sQuery.uiMaximum = uiMaximum;
	return mpcContent->GetCodeModel()->FindScopeSymbols(&sQuery, pasDest);
}


//////////////////////////////////////////////////////////////////////////
//
// What the class, enum or namespace szKey has, added: its own, then for a
// class its bases', nearest first; "" is what is outside any scope.
// Fewer as FindVisible's.  The number added.
//
//////////////////////////////////////////////////////////////////////////
size CIDELightTyper::FindMembers(const char* szKey, CArrayIDEScopeSymbols* pasDest, const char* szName, size uiNameLength, char cWordStart, size uiMaximum)
{
	SIDEScopeQuery	sQuery;
	CArrayChars		aszScopes;
	size			uiFound;

	aszScopes.Init();
	aszScopes.Add(szKey);
	mpcContent->GetCodeModel()->FindClassBases(szKey, true, &aszScopes);
	sQuery.Init(&aszScopes);
	sQuery.szName = szName;
	sQuery.uiNameLength = uiNameLength;
	sQuery.cWordStart = cWordStart;
	sQuery.uiMaximum = uiMaximum;
	uiFound = mpcContent->GetCodeModel()->FindScopeSymbols(&sQuery, pasDest);
	aszScopes.Kill();
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// The type a symbol is declared with, as it is written, appended: a
// variable's, a field's or a parameter's, or what a function returns.
// False if its declaration can't be read.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::ReadDeclaredType(const char* szKey, CChars* pszDest)
{
	SIDESymbolInfo	sInfo;
	bool			bResult;

	sInfo.Init();
	bResult = ReadIDESymbolInfo(mpcContent, szKey, &sInfo);
	if (bResult)
	{
		AppendIDEDeclaredType(&sInfo, pszDest);
	}
	sInfo.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What a symbol is: a class, an enum or a namespace is itself; an
// enumerator is its enum; a typedef is what it names; anything else is
// what it is declared as (a function, what it returns).  False if that
// isn't a class, an enum or a namespace the index has.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfSymbol(SIDEScopeSymbol* psSymbol, SIDELightType* psDest)
{
	CChars	szType;
	bool	bResult;

	psDest->Clear();
	switch (psSymbol->uiKind)
	{
	case CPPSK_Namespace:
	case CPPSK_Class:
	case CPPSK_ClassTemplate:
	case CPPSK_Enum:
		psDest->szKey.Set(psSymbol->szKey);
		psDest->uiKind = psSymbol->uiKind;
		return true;
	case CPPSK_Enumerator:
		psDest->szKey.Set(psSymbol->szScope);
		psDest->uiKind = CPPSK_Enum;
		return !psSymbol->szScope.Empty();
	case CPPSK_Typedef:
	case CPPSK_AliasTemplate:
		return TypeOfText(psSymbol->szName.Text(), psSymbol->szScope.Text(), psDest);
	case CPPSK_Variable:
	case CPPSK_Field:
	case CPPSK_Parameter:
	case CPPSK_Function:
	case CPPSK_FunctionTemplate:
		szType.Init();
		bResult = ReadDeclaredType(psSymbol->szKey.Text(), &szType) && TypeOfText(szType.Text(), psSymbol->szScope.Text(), psDest);
		szType.Kill();
		return bResult;
	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// What a type written in the scope szDeclaredIn means ("const CGauge*",
// "NS::CGauge&", "Ptr<CGauge>", "CGauge[4]"): its name looked for from
// that scope outwards, then from the place's scopes; its pointers and
// array bounds counted; a typedef or an alias followed to what it names.
// False if the name is none the index has as a class, an enum or a
// namespace (a built in type, a template's parameter).
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfText(const char* szType, const char* szDeclaredIn, SIDELightType* psDest, int iDepth)
{
	static const char*	aszWords[] = { "const", "volatile", "struct", "class", "enum", "union", "typename", "typedef", "using" };
	CArrayChars			aszScopes;
	SIDESymbolInfo		sInfo;
	CChars				szName;
	CChars				szKey;
	CChars				szNamed;
	const char*			szEquals;
	size				uiLength;
	size				uiEnd;
	size				i;
	size				j;
	int32				iPointers;
	int					iAngles;
	uint8				uiKind;
	bool				bFound;
	bool				bResult;

	psDest->Clear();
	if (iDepth > IDE_LIGHT_TYPE_DEPTH)
	{
		return false;
	}

	//The words before its name that say nothing of what it is.
	i = 0;
	bFound = true;
	while (bFound)
	{
		while (szType[i] == ' ')
		{
			i++;
		}
		bFound = false;
		for (j = 0; j < sizeof(aszWords) / sizeof(const char*); j++)
		{
			uiLength = strlen(aszWords[j]);
			if ((strncmp(&szType[i], aszWords[j], uiLength) == 0) && !IsIDENameChar(szType[i + uiLength]))
			{
				i += uiLength;
				bFound = true;
			}
		}
	}

	//Its name, with its scopes, to its template arguments (the first is kept) or what follows it.
	szName.Init();
	for (; IsIDENameChar(szType[i]) || ((szType[i] == ':') && (szType[i + 1] == ':')); i++)
	{
		szName.Append(szType[i]);
		if (szType[i] == ':')
		{
			szName.Append(':');
			i++;
		}
	}
	if (szType[i] == '<')
	{
		iAngles = 0;
		uiEnd = 0;
		for (j = i; szType[j] != '\0'; j++)
		{
			if (szType[j] == '<')
			{
				iAngles++;
			}
			else if (szType[j] == '>')
			{
				iAngles--;
				if (iAngles == 0)
				{
					break;
				}
			}
			else if ((szType[j] == ',') && (iAngles == 1) && (uiEnd == 0))
			{
				uiEnd = j;
			}
		}
		if (szType[j] != '>')
		{
			szName.Kill();
			return false;
		}
		psDest->szArgument.Append(&szType[i + 1], ((uiEnd != 0) ? uiEnd : j) - (i + 1));
		psDest->szArgument.StripWhitespace();
		i = j + 1;
	}

	//After it: a level for each pointer and each array bound; a reference is what it refers to.
	iPointers = 0;
	for (; szType[i] != '\0'; i++)
	{
		if ((szType[i] == '*') || (szType[i] == '['))
		{
			iPointers++;
		}
	}

	szKey.Init();
	aszScopes.Init();
	AddScopesOf(szDeclaredIn, &aszScopes);
	for (i = 0; i < maszScopes.NumElements(); i++)
	{
		if (aszScopes.GetIndex(maszScopes.GetText(i)) == ARRAY_ELEMENT_NOT_FOUND)
		{
			aszScopes.Add(maszScopes.GetText(i));
		}
	}
	bResult = !szName.Empty() && mpcContent->GetCodeModel()->FindTypeSymbol(szName.Text(), &aszScopes, &szKey, &uiKind);
	aszScopes.Kill();
	szName.Kill();

	if (bResult && ((uiKind == CPPSK_Typedef) || (uiKind == CPPSK_AliasTemplate)))
	{
		//What it names: "typedef unsigned int PaintColour" less its own name, or what is after an alias's "=".
		sInfo.Init();
		szNamed.Init();
		bResult = ReadIDESymbolInfo(mpcContent, szKey.Text(), &sInfo);
		if (bResult)
		{
			szEquals = strchr(sInfo.szDeclaration.Text(), '=');
			if (szEquals)
			{
				szNamed.Append(szEquals + 1);
			}
			else if (sInfo.uiNameLength > 0)
			{
				szNamed.Append(sInfo.szDeclaration.Text(), sInfo.uiName);
				szNamed.Append(&sInfo.szDeclaration.Text()[sInfo.uiName + sInfo.uiNameLength]);
			}
			GetIDESymbolScope(szKey.Text(), &szName);
			bResult = !szNamed.Empty() && TypeOfText(szNamed.Text(), szName.Text(), psDest, iDepth + 1);
			szName.Kill();
		}
		szNamed.Kill();
		sInfo.Kill();
		psDest->iPointers += iPointers;
	}
	else if (bResult)
	{
		psDest->szKey.Set(szKey);
		psDest->uiKind = uiKind;
		psDest->iPointers = iPointers;
	}
	szKey.Kill();
	if (!bResult)
	{
		psDest->Clear();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What the symbols of pasSymbols named so are, if they agree: a function
// (bCall) is every function of the name, which must all return the same;
// anything else is the first of the name that isn't a function.  False
// if there is none, or they differ.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfNamed(CArrayIDEScopeSymbols* pasSymbols, const char* szName, size uiLength, bool bCall, SIDELightType* psDest)
{
	SIDEScopeSymbol*	psSymbol;
	SIDELightType		sOther;
	size				i;
	bool				bFunction;
	bool				bFound;
	bool				bResult;

	bFound = false;
	bResult = true;
	sOther.Init();
	for (i = 0; bResult && (i < pasSymbols->NumElements()); i++)
	{
		psSymbol = pasSymbols->Get(i);
		bFunction = (psSymbol->uiKind == CPPSK_Function) || (psSymbol->uiKind == CPPSK_FunctionTemplate);
		if ((bFunction != bCall) || (psSymbol->szName.Length() != uiLength) || (strncmp(psSymbol->szName.Text(), szName, uiLength) != 0))
		{
			continue;
		}
		if (!bFound)
		{
			bFound = true;
			bResult = TypeOfSymbol(psSymbol, psDest);
			if (!bCall)
			{
				break;
			}
		}
		else
		{
			bResult = TypeOfSymbol(psSymbol, &sOther) && sOther.Equals(psDest);
		}
	}
	sOther.Kill();
	if (!bFound || !bResult)
	{
		psDest->Clear();
		return false;
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What a name on its own is, at the place: "this" is a pointer to the
// class; otherwise whatever in scope has the name (TypeOfNamed).
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfName(const char* szName, size uiLength, bool bCall, SIDELightType* psDest)
{
	CArrayIDEScopeSymbols	asNamed;
	bool					bResult;

	if ((uiLength == 4) && (strncmp(szName, "this", 4) == 0) && !bCall)
	{
		psDest->Clear();
		psDest->szKey.Set(mszClass);
		psDest->uiKind = CPPSK_Class;
		psDest->iPointers = 1;
		return !mszClass.Empty();
	}
	asNamed.Init();
	FindVisible(&asNamed, szName, uiLength);
	bResult = TypeOfNamed(&asNamed, szName, uiLength, bCall, psDest);
	KillIDEScopeSymbols(&asNamed);
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// A group in brackets is typed only as a cast is written:
// "((CGauge*)pvThing)" and "(CGauge*)pvThing" are a CGauge*.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfGroup(const char* szText, size uiStart, size uiEnd, SIDELightType* psDest)
{
	CChars	szType;
	size	i;
	size	uiClose;
	bool	bResult;

	//Inside the group's own brackets, and any more round all of it.
	i = uiStart;
	while ((i < uiEnd) && ((szText[i] == '(') || (szText[i] == ' ')))
	{
		i++;
	}
	if ((i == uiStart) || (i >= uiEnd) || (szText[i - 1] != '('))
	{
		return false;
	}
	for (uiClose = i; (uiClose < uiEnd) && (szText[uiClose] != ')') && (szText[uiClose] != '('); uiClose++)
	{
	}
	if ((uiClose >= uiEnd) || (szText[uiClose] != ')') || (uiClose + 1 >= uiEnd) || (szText[uiClose + 1] == ')'))
	{
		//No type in brackets with something after it: not a cast.
		return false;
	}
	szType.Init();
	szType.Append(&szText[i], uiClose - i);
	bResult = TypeOfText(szType.Text(), "", psDest);
	szType.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What "." or "->" after something of psType reaches: a pointer's or an
// array's is what it points at.  "->" on a class template that isn't a
// pointer ("Ptr<CGauge>") is taken to reach its first argument, as a
// smart pointer's does.  False if that can't be found.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::Dereference(SIDELightType* psType, bool bArrow)
{
	CChars	szArgument;
	bool	bResult;

	if (bArrow && (psType->iPointers == 0) && !psType->szArgument.Empty())
	{
		szArgument.Init(psType->szArgument);
		bResult = TypeOfText(szArgument.Text(), "", psType);
		szArgument.Kill();
		psType->iPointers = 0;
		return bResult;
	}
	psType->iPointers = 0;
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// What the first uiNumLinks parts of an expression are, one after the
// other: the first by what is in scope (or "this", or a cast in
// brackets, or a type's or a namespace's name), each part after it as a
// member of what the part before it is.  A call is what its function
// returns; an indexing takes a level of pointer off.  False if any part
// can't be said for sure.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfLinks(const char* szText, CArrayIDEChainLinks* pasLinks, size uiNumLinks, SIDELightType* psDest)
{
	CArrayIDEScopeSymbols	asMembers;
	SIDEChainLink*			psLink;
	CChars					szKey;
	size					i;
	bool					bResult;

	psDest->Clear();
	bResult = true;
	for (i = 0; bResult && (i < uiNumLinks); i++)
	{
		psLink = pasLinks->Get(i);
		if (psLink->bGroup)
		{
			bResult = TypeOfGroup(szText, psLink->uiStart, psLink->uiEnd, psDest);
		}
		else if (psLink->uiEnd == psLink->uiStart)
		{
			//"::" with nothing before it: outside any scope, which the next part is looked for in.
			psDest->Clear();
		}
		else if (psLink->cJoin == '\0')
		{
			bResult = TypeOfName(&szText[psLink->uiStart], psLink->uiEnd - psLink->uiStart, psLink->bCall, psDest);
		}
		else
		{
			//A member of what there is so far: what a class, an enum or a namespace has, or after a first "::" what
			//is outside any.
			bResult = (psLink->cJoin == ':') || Dereference(psDest, psLink->cJoin == '>');
			if (bResult)
			{
				szKey.Init(psDest->szKey);
				asMembers.Init();
				FindMembers(szKey.Text(), &asMembers, &szText[psLink->uiStart], psLink->uiEnd - psLink->uiStart);
				bResult = TypeOfNamed(&asMembers, &szText[psLink->uiStart], psLink->uiEnd - psLink->uiStart, psLink->bCall, psDest);
				KillIDEScopeSymbols(&asMembers);
				szKey.Kill();
			}
		}
		if (bResult && (psLink->iIndexes > 0))
		{
			psDest->iPointers -= psLink->iIndexes;
			bResult = psDest->iPointers >= 0;
		}
	}
	if (!bResult)
	{
		psDest->Clear();
	}
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What the expression szText[uiStart, uiEnd) is (FindIDEChainStart found
// it): all of its parts (TypeOfLinks).
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfChain(const char* szText, size uiStart, size uiEnd, SIDELightType* psDest)
{
	CArrayIDEChainLinks	asLinks;
	bool				bResult;

	psDest->Clear();
	asLinks.Init();
	bResult = ParseIDEChain(szText, uiStart, uiEnd, &asLinks) && TypeOfLinks(szText, &asLinks, asLinks.NumElements(), psDest);
	asLinks.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// What the last name of the expression szText[uiStart, uiEnd) could be,
// added: everything of that name in scope, or that what is before it
// has (every function of the name, for a call of one of them).  False if
// what is before it can't be typed, or nothing has the name.
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::FindChainSymbols(const char* szText, size uiStart, size uiEnd, CArrayIDEScopeSymbols* pasDest)
{
	CArrayIDEChainLinks	asLinks;
	SIDEChainLink*		psLast;
	SIDELightType		sType;
	size				uiFirst;
	bool				bResult;

	uiFirst = pasDest->NumElements();
	asLinks.Init();
	sType.Init();
	bResult = ParseIDEChain(szText, uiStart, uiEnd, &asLinks) && (asLinks.NumElements() > 0);
	if (bResult)
	{
		psLast = asLinks.Get(asLinks.NumElements() - 1);
		if (psLast->bGroup || (psLast->uiEnd == psLast->uiStart))
		{
			bResult = false;
		}
		else if (psLast->cJoin == '\0')
		{
			FindVisible(pasDest, &szText[psLast->uiStart], psLast->uiEnd - psLast->uiStart);
		}
		else
		{
			bResult = TypeOfLinks(szText, &asLinks, asLinks.NumElements() - 1, &sType) && ((psLast->cJoin == ':') || Dereference(&sType, psLast->cJoin == '>'));
			if (bResult)
			{
				FindMembers(sType.szKey.Text(), pasDest, &szText[psLast->uiStart], psLast->uiEnd - psLast->uiStart);
			}
		}
	}
	sType.Kill();
	asLinks.Kill();
	return bResult && (pasDest->NumElements() > uiFirst);
}


//////////////////////////////////////////////////////////////////////////
//
// What the function the place is in returns: its type, and that as it is
// written.  False if the place is in no function the index has, or its
// declaration can't be read; the type is empty where it is none the
// index has (an int).
//
//////////////////////////////////////////////////////////////////////////
bool CIDELightTyper::TypeOfFunction(SIDELightType* psDest, CChars* pszText)
{
	CChars	szKey;
	CChars	szScope;
	uint8	uiKind;
	bool	bResult;

	psDest->Clear();
	szKey.Init();
	bResult = mbFunction && mpcContent->GetCodeModel()->GetSymbolAt(mszFunctionFile.Text(), miFunctionLine, miFunctionColumn, &szKey, &uiKind) && ReadDeclaredType(szKey.Text(), pszText);
	if (bResult)
	{
		GetIDESymbolScope(szKey.Text(), &szScope);
		TypeOfText(pszText->Text(), szScope.Text(), psDest);
		szScope.Kill();
	}
	szKey.Kill();
	return bResult;
}
