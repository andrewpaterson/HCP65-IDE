/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela BuildLib

Codaphela BuildLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela BuildLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela BuildLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/FileUtil.h"
#include "BaseLib/PrimitiveTypes.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "BaseLib/XMLWriter.h"
#include "BuildConfigurations.h"
#include "BuildConfigurationEdit.h"


#define EDIT_CONFIGURATION_TAG		"<Configuration"
#define EDIT_CONFIGURATION_END		"</Configuration>"
#define EDIT_ROOT_TAG				"<BuildConfigurations"
#define EDIT_ROOT_END				"</BuildConfigurations>"
#define EDIT_COMMENT				"<!--"
#define EDIT_COMMENT_END			"-->"
#define EDIT_TEMPORARY_EXTENSION	".tmp"


//Where a <Configuration> is in a file's text.
struct SBuildConfigurationElement
{
	const char*	szStart;
	const char*	szOpenEnd;	//After its opening tag.
	const char*	szEnd;		//After it all.
};
typedef CArrayTemplate<SBuildConfigurationElement>	CArrayBuildConfigurationElements;


//////////////////////////////////////////////////////////////////////////
//
// A new configuration that isn't a copy of another.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurationEdit::Init(const char* szName, EBuildToolchain eToolchain, const char* szSolution)
{
	muiSource = MAX_SIZE;
	mbNew = true;
	mbNeedsHCP65 = false;
	mszName.Init(szName);
	meToolchain = eToolchain;
	mszSolution.Init(szSolution);
	mszBreakpointSpace.Init();
	mbNoFloats = false;
	mbMathTables = true;
	mszCard.Init();
	mszCardPath.Init();
}


//////////////////////////////////////////////////////////////////////////
//
// pcConfiguration, the configuration at uiSource, as it is.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurationEdit::Init(CBuildConfiguration* pcConfiguration, size uiSource)
{
	Init(pcConfiguration->GetName(), pcConfiguration->GetToolchain(), pcConfiguration->GetSolutionConfiguration());
	muiSource = uiSource;
	mbNew = false;
	mbNeedsHCP65 = (pcConfiguration->GetProgram() != BP_None) || (pcConfiguration->NumLoads() > 0);
	mszBreakpointSpace.Set(pcConfiguration->GetBreakpointSpace());
	mbNoFloats = pcConfiguration->IsNoFloats();
	mbMathTables = pcConfiguration->IsMathTables();
	if (pcConfiguration->HasCard())
	{
		mszCard.Set(pcConfiguration->GetCard());
		mszCardPath.Set(pcConfiguration->GetCardPath());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// A new configuration called szName, a copy of pcCopied.
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurationEdit::Init(CBuildConfigurationEdit* pcCopied, const char* szName)
{
	Init(szName, pcCopied->meToolchain, pcCopied->mszSolution.Text());
	muiSource = pcCopied->muiSource;
	mbNeedsHCP65 = pcCopied->mbNeedsHCP65;
	mszBreakpointSpace.Set(pcCopied->mszBreakpointSpace);
	mbNoFloats = pcCopied->mbNoFloats;
	mbMathTables = pcCopied->mbMathTables;
	mszCard.Set(pcCopied->mszCard);
	mszCardPath.Set(pcCopied->mszCardPath);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CBuildConfigurationEdit::Kill(void)
{
	mszCardPath.Kill();
	mszCard.Kill();
	mszBreakpointSpace.Kill();
	mszSolution.Kill();
	mszName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The next szTag at or after sz that isn't in a comment and is the whole
// of a tag's name, NULL if there is none.
//
//////////////////////////////////////////////////////////////////////////
static const char* FindEditTag(const char* sz, const char* szTag)
{
	size	uiLength;
	char	c;

	uiLength = strlen(szTag);
	while (*sz != '\0')
	{
		if (strncmp(sz, EDIT_COMMENT, strlen(EDIT_COMMENT)) == 0)
		{
			sz = strstr(sz, EDIT_COMMENT_END);
			if (sz == NULL)
			{
				return NULL;
			}
			sz += strlen(EDIT_COMMENT_END);
			continue;
		}
		if (strncmp(sz, szTag, uiLength) == 0)
		{
			c = sz[uiLength];
			if ((szTag[uiLength - 1] == '>') || (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '/') || (c == '>'))
			{
				return sz;
			}
		}
		sz++;
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// After the > that ends the tag starting at sz, NULL if none does.  One in
// an attribute's quotes doesn't.
//
//////////////////////////////////////////////////////////////////////////
static const char* FindEditTagEnd(const char* sz)
{
	char	cQuote;

	cQuote = '\0';
	for (; *sz != '\0'; sz++)
	{
		if (cQuote != '\0')
		{
			if (*sz == cQuote)
			{
				cQuote = '\0';
			}
		}
		else if ((*sz == '"') || (*sz == '\'\''))
		{
			cQuote = *sz;
		}
		else if (*sz == '>')
		{
			return sz + 1;
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
//
// Every <Configuration> in szText.  False if one isn't finished.
//
//////////////////////////////////////////////////////////////////////////
static bool FindEditElements(const char* szText, CArrayBuildConfigurationElements* pasElements)
{
	SBuildConfigurationElement	sElement;
	const char*					sz;

	sz = FindEditTag(szText, EDIT_CONFIGURATION_TAG);
	while (sz != NULL)
	{
		sElement.szStart = sz;
		sElement.szOpenEnd = FindEditTagEnd(sz);
		if (sElement.szOpenEnd == NULL)
		{
			return false;
		}
		if (sElement.szOpenEnd[-2] == '/')
		{
			sElement.szEnd = sElement.szOpenEnd;
		}
		else
		{
			sz = FindEditTag(sElement.szOpenEnd, EDIT_CONFIGURATION_END);
			if (sz == NULL)
			{
				return false;
			}
			sElement.szEnd = sz + strlen(EDIT_CONFIGURATION_END);
		}
		pasElements->Add(&sElement);
		sz = FindEditTag(sElement.szEnd, EDIT_CONFIGURATION_TAG);
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// szValue as an attribute's value is written, onto pszDest.
//
//////////////////////////////////////////////////////////////////////////
static void AppendEditEscaped(CChars* pszDest, const char* szValue)
{
	for (; *szValue != '\0'; szValue++)
	{
		switch (*szValue)
		{
		case '&':
			pszDest->Append("&amp;");
			break;
		case '<':
			pszDest->Append("&lt;");
			break;
		case '>':
			pszDest->Append("&gt;");
			break;
		case '"':
			pszDest->Append("&quot;");
			break;
		default:
			pszDest->Append(*szValue);
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Finds the attribute szName in the opening tag szTag: where it starts (at
// the white space before it), its value and where it ends.  False if the
// tag hasn't it.  Then pszEnd is where a new attribute goes.
//
//////////////////////////////////////////////////////////////////////////
static bool FindEditAttribute(const char* szTag, const char* szName, const char** pszStart, const char** pszValue, const char** pszValueEnd, const char** pszEnd)
{
	const char*	sz;
	const char*	szAttribute;
	const char*	szAttributeEnd;
	const char*	szBefore;
	char		cQuote;

	sz = szTag + 1;
	while ((*sz != '\0') && (*sz != ' ') && (*sz != '\t') && (*sz != '\r') && (*sz != '\n') && (*sz != '/') && (*sz != '>'))
	{
		sz++;
	}
	for (;;)
	{
		szBefore = sz;
		while ((*sz == ' ') || (*sz == '\t') || (*sz == '\r') || (*sz == '\n'))
		{
			sz++;
		}
		if ((*sz == '\0') || (*sz == '/') || (*sz == '>'))
		{
			*pszEnd = szBefore;
			return false;
		}
		szAttribute = sz;
		while ((*sz != '\0') && (*sz != '=') && (*sz != ' '))
		{
			sz++;
		}
		szAttributeEnd = sz;
		while ((*sz != '\0') && (*sz != '"') && (*sz != '\'\''))
		{
			sz++;
		}
		if (*sz == '\0')
		{
			*pszEnd = szBefore;
			return false;
		}
		cQuote = *sz;
		sz++;
		*pszValue = sz;
		while ((*sz != '\0') && (*sz != cQuote))
		{
			sz++;
		}
		*pszValueEnd = sz;
		if (*sz != '\0')
		{
			sz++;
		}
		if (((size)(szAttributeEnd - szAttribute) == strlen(szName)) && (strncmp(szAttribute, szName, strlen(szName)) == 0))
		{
			*pszStart = szBefore;
			*pszEnd = sz;
			return true;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The attribute szName of the opening tag in pszTag has the value szValue,
// or isn't there if that is NULL or empty.  The rest of it isn't touched.
//
//////////////////////////////////////////////////////////////////////////
static void SetEditAttribute(CChars* pszTag, const char* szName, const char* szValue)
{
	CChars		szNew;
	const char*	szStart;
	const char*	szValueStart;
	const char*	szValueEnd;
	const char*	szEnd;
	bool		bFound;
	bool		bValue;

	bValue = (szValue != NULL) && (szValue[0] != '\0');
	bFound = FindEditAttribute(pszTag->Text(), szName, &szStart, &szValueStart, &szValueEnd, &szEnd);
	szNew.Init();
	if (bFound && bValue)
	{
		szNew.Append(pszTag->Text(), (int)(szValueStart - pszTag->Text()));
		AppendEditEscaped(&szNew, szValue);
		szNew.Append(szValueEnd);
	}
	else if (bFound)
	{
		szNew.Append(pszTag->Text(), (int)(szStart - pszTag->Text()));
		szNew.Append(szEnd);
	}
	else if (bValue)
	{
		szNew.Append(pszTag->Text(), (int)(szEnd - pszTag->Text()));
		szNew.Append(' ');
		szNew.Append(szName);
		szNew.Append("=\"");
		AppendEditEscaped(&szNew, szValue);
		szNew.Append('"');
		szNew.Append(szEnd);
	}
	else
	{
		szNew.Append(pszTag->Text());
	}
	pszTag->Set(szNew);
	szNew.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// The attributes of the opening tag in pszTag that pcEdit changes from
// pcSource, which is NULL if it is a new tag.  The others stay as they are
// written.
//
//////////////////////////////////////////////////////////////////////////
static void SetEditAttributes(CChars* pszTag, CBuildConfigurationEdit* pcEdit, CBuildConfiguration* pcSource)
{
	if ((pcSource == NULL) || !pcEdit->mszName.Equals(pcSource->GetName()))
	{
		SetEditAttribute(pszTag, "Name", pcEdit->mszName.Text());
	}
	if ((pcSource == NULL) || (pcEdit->meToolchain != pcSource->GetToolchain()))
	{
		SetEditAttribute(pszTag, "Toolchain", CBuildConfiguration::GetToolchainName(pcEdit->meToolchain));
	}
	if ((pcSource == NULL) || !pcEdit->mszSolution.Equals(pcSource->GetSolutionConfiguration()))
	{
		SetEditAttribute(pszTag, "Solution", pcEdit->mszSolution.Text());
	}
	if ((pcSource == NULL) || !pcEdit->mszBreakpointSpace.Equals(pcSource->GetBreakpointSpace()))
	{
		SetEditAttribute(pszTag, "BreakpointSpace", pcEdit->mszBreakpointSpace.Text());
	}
	if ((pcSource == NULL) || (pcEdit->mbNoFloats != pcSource->IsNoFloats()))
	{
		SetEditAttribute(pszTag, "NoFloats", pcEdit->mbNoFloats ? "true" : NULL);
	}
	if ((pcSource == NULL) || (pcEdit->mbMathTables != pcSource->IsMathTables()))
	{
		SetEditAttribute(pszTag, "MathTables", pcEdit->mbMathTables ? NULL : "false");
	}
	if ((pcSource == NULL) || !pcEdit->mszCard.Equals(pcSource->GetCard()))
	{
		SetEditAttribute(pszTag, "Card", pcEdit->mszCard.Text());
	}
	if ((pcSource == NULL) || !pcEdit->mszCardPath.Equals(pcSource->GetCardPath()))
	{
		SetEditAttribute(pszTag, "CardPath", pcEdit->mszCardPath.Text());
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Whether the element's Name is the one its configuration was read with.
//
//////////////////////////////////////////////////////////////////////////
static bool IsEditElementNamed(SBuildConfigurationElement* psElement, const char* szName)
{
	CChars		szTag;
	CChars		szEscaped;
	const char*	szStart;
	const char*	szValue;
	const char*	szValueEnd;
	const char*	szEnd;
	bool		bResult;

	szTag.Init();
	szTag.Append(psElement->szStart, (int)(psElement->szOpenEnd - psElement->szStart));
	szEscaped.Init();
	AppendEditEscaped(&szEscaped, szName);
	bResult = FindEditAttribute(szTag.Text(), "Name", &szStart, &szValue, &szValueEnd, &szEnd);
	bResult = bResult && ((size)(szValueEnd - szValue) == (size)szEscaped.Length()) && (strncmp(szValue, szEscaped.Text(), (size)szEscaped.Length()) == 0);
	szEscaped.Kill();
	szTag.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The file's text pszDest with pasEdits made to it.  Whatever isn't
// changed stays as it was written, to the byte: comments, the order of the
// attributes, and all that is inside each configuration.  A configuration
// that no edit keeps goes, with the line it is on; new ones go last.  False
// if szText isn't as the configurations were read from.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfigurations::MakeEditedText(const char* szText, CArrayBuildConfigurationEdits* pasEdits, CChars* pszDest)
{
	CArrayBuildConfigurationElements	asElements;
	SBuildConfigurationElement*			psElement;
	CBuildConfigurationEdit*			pcEdit;
	CBuildConfigurationEdit*			pcKept;
	CBuildConfiguration*				pcSource;
	CChars								szTag;
	CChars								szIndent;
	const char*							szAt;
	const char*							szStart;
	const char*							szEnd;
	const char*							szRootEnd;
	const char*							szNewLine;
	bool								bSelfClosed;
	size								i;
	size								j;

	asElements.Init();
	if (!FindEditElements(szText, &asElements) || (asElements.NumElements() != masConfigurations.NumElements()))
	{
		asElements.Kill();
		return false;
	}
	for (i = 0; i < asElements.NumElements(); i++)
	{
		if (!IsEditElementNamed(asElements.Get(i), masConfigurations.Get(i)->GetName()))
		{
			asElements.Kill();
			return false;
		}
	}

	bSelfClosed = false;
	szRootEnd = FindEditTag(szText, EDIT_ROOT_END);
	if (szRootEnd == NULL)
	{
		szRootEnd = FindEditTag(szText, EDIT_ROOT_TAG);
		szEnd = (szRootEnd != NULL) ? FindEditTagEnd(szRootEnd) : NULL;
		if ((szEnd == NULL) || (szEnd[-2] != '/'))
		{
			asElements.Kill();
			return false;
		}
		bSelfClosed = true;
		szRootEnd = szEnd - 2;
	}

	szNewLine = (strstr(szText, "\r\n") != NULL) ? "\r\n" : "\n";
	szIndent.Init("\t");
	if (asElements.NumElements() > 0)
	{
		szStart = asElements.Get(0)->szStart;
		while ((szStart > szText) && ((szStart[-1] == ' ') || (szStart[-1] == '\t')))
		{
			szStart--;
		}
		szIndent.Set("");
		szIndent.Append(szStart, (int)(asElements.Get(0)->szStart - szStart));
	}

	szTag.Init();
	szAt = szText;
	for (i = 0; i < asElements.NumElements(); i++)
	{
		psElement = asElements.Get(i);
		pcKept = NULL;
		for (j = 0; j < pasEdits->NumElements(); j++)
		{
			pcEdit = pasEdits->Get(j);
			if (!pcEdit->mbNew && (pcEdit->muiSource == i))
			{
				pcKept = pcEdit;
			}
		}
		if (pcKept != NULL)
		{
			pszDest->Append(szAt, (int)(psElement->szStart - szAt));
			szTag.Set("");
			szTag.Append(psElement->szStart, (int)(psElement->szOpenEnd - psElement->szStart));
			SetEditAttributes(&szTag, pcKept, masConfigurations.Get(i));
			pszDest->Append(szTag);
			szAt = psElement->szOpenEnd;
		}
		else
		{
			szStart = psElement->szStart;
			while ((szStart > szText) && ((szStart[-1] == ' ') || (szStart[-1] == '\t')))
			{
				szStart--;
			}
			szEnd = psElement->szEnd;
			while ((*szEnd == ' ') || (*szEnd == '\t'))
			{
				szEnd++;
			}
			if (*szEnd == '\r')
			{
				szEnd++;
			}
			if (*szEnd == '\n')
			{
				szEnd++;
			}
			pszDest->Append(szAt, (int)(szStart - szAt));
			szAt = szEnd;
		}
	}
	pszDest->Append(szAt, (int)(szRootEnd - szAt));

	if (bSelfClosed)
	{
		pszDest->Append('>');
		pszDest->Append(szNewLine);
	}
	else if ((pszDest->Length() > 0) && (pszDest->Text()[pszDest->Length() - 1] != '\n'))
	{
		j = 0;
		for (i = 0; i < pasEdits->NumElements(); i++)
		{
			j += pasEdits->Get(i)->mbNew ? 1 : 0;
		}
		if (j > 0)
		{
			pszDest->Append(szNewLine);
		}
	}
	for (i = 0; i < pasEdits->NumElements(); i++)
	{
		pcEdit = pasEdits->Get(i);
		if (!pcEdit->mbNew)
		{
			continue;
		}
		pcSource = NULL;
		szTag.Set(EDIT_CONFIGURATION_TAG "/>");
		if (pcEdit->muiSource != MAX_SIZE)
		{
			psElement = asElements.Get(pcEdit->muiSource);
			pcSource = masConfigurations.Get(pcEdit->muiSource);
			szTag.Set("");
			szTag.Append(psElement->szStart, (int)(psElement->szOpenEnd - psElement->szStart));
		}
		SetEditAttributes(&szTag, pcEdit, pcSource);
		pszDest->Append(szIndent);
		pszDest->Append(szTag);
		if (pcEdit->muiSource != MAX_SIZE)
		{
			psElement = asElements.Get(pcEdit->muiSource);
			pszDest->Append(psElement->szOpenEnd, (int)(psElement->szEnd - psElement->szOpenEnd));
		}
		pszDest->Append(szNewLine);
	}
	if (bSelfClosed)
	{
		pszDest->Append(EDIT_ROOT_END);
		pszDest->Append(szRootEnd + 2);
	}
	else
	{
		pszDest->Append(szRootEnd);
	}
	szIndent.Kill();
	szTag.Kill();
	asElements.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
// pasEdits are made to szFileName, the file these configurations were read
// from (it is written if there isn't one yet), and it is read again: every
// configuration pointer goes.  Only what they change in the file changes.
// If the result can't be read the file is left as it was, the
// configurations are as they were and it is false; it is too if the file
// isn't what they were read from.
//
//////////////////////////////////////////////////////////////////////////
bool CBuildConfigurations::ApplyEdits(const char* szFileName, CArrayBuildConfigurationEdits* pasEdits)
{
	CFileUtil	cFileUtil;
	CTextFile	cFile;
	CChars		szTemporary;
	CChars		szOriginal;
	CChars		szEdited;
	bool		bResult;

	szTemporary.Init(szFileName);
	szTemporary.Append(EDIT_TEMPORARY_EXTENSION);
	if (cFileUtil.Exists(szFileName))
	{
		cFile.Init();
		bResult = cFile.Read(szFileName);
	}
	else
	{
		bResult = Write(szTemporary.Text());
		cFile.Init();
		bResult = bResult && cFile.Read(szTemporary.Text());
	}
	szOriginal.Init();
	szEdited.Init();
	if (bResult)
	{
		szOriginal.Append(cFile.Text());
		bResult = MakeEditedText(szOriginal.Text(), pasEdits, &szEdited);
	}
	cFile.Kill();
	if (!bResult)
	{
		cFileUtil.Delete(szTemporary.Text());
		szEdited.Kill();
		szOriginal.Kill();
		szTemporary.Kill();
		return false;
	}

	cFile.Init(szEdited.Text());
	bResult = cFile.Write(szTemporary.Text());
	cFile.Kill();
	bResult = bResult && Read(szTemporary.Text()) && (masConfigurations.NumElements() == pasEdits->NumElements());
	if (bResult)
	{
		cFile.Init(szEdited.Text());
		bResult = cFile.Write(szFileName);
		cFile.Kill();
	}
	if (!bResult)
	{
		cFile.Init(szOriginal.Text());
		cFile.Write(szTemporary.Text());
		cFile.Kill();
		Read(szTemporary.Text());
	}
	cFileUtil.Delete(szTemporary.Text());
	szEdited.Kill();
	szOriginal.Kill();
	szTemporary.Kill();
	return bResult;
}

