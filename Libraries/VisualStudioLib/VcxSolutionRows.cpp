/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela VisualStudioLib

Codaphela VisualStudioLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela VisualStudioLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela VisualStudioLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include <string.h>
#include "BaseLib/ArrayChars.h"
#include "BaseLib/StringHelper.h"
#include "BaseLib/TextFile.h"
#include "VcxSolutionRows.h"


#define VCX_ROWS_CONFIGURATIONS	"GlobalSection(SolutionConfigurationPlatforms)"
#define VCX_ROWS_PROJECTS		"GlobalSection(ProjectConfigurationPlatforms)"
#define VCX_ROWS_SECTION_END	"EndGlobalSection"
#define VCX_ROWS_INDENT			"\t\t"


//A solution's text as its lines, each without its line ending, which is kept to write them with.
class CVcxSolutionLines
{
public:
	CArrayChars	maszLines;
	bool		mbCarriageReturns;
	bool		mbLastLineEnded;

	bool	Read(const char* szFileName);
	bool	Write(const char* szFileName);
	void	Kill(void);

	bool	FindSection(const char* szSection, size* puiFirst, size* puiEnd);
	void	Insert(size uiAt, const char* szLine);
};


//A row: {GUID}.Configuration.Kind = Value.
struct SVcxSolutionRow
{
	CChars	szGuid;
	CChars	szConfiguration;
	CChars	szKind;
	CChars	szValue;

	void	Init(void);
	void	Kill(void);
	bool	Parse(const char* szLine);
	void	Write(CChars* pszDest);
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolutionLines::Read(const char* szFileName)
{
	CTextFile	cFile;
	CChars		szText;
	size		i;

	maszLines.Init();
	mbCarriageReturns = false;
	mbLastLineEnded = false;
	cFile.Init();
	if (!cFile.Read(szFileName))
	{
		cFile.Kill();
		return false;
	}
	szText.Init();
	szText.AppendSubString(cFile.Text(), (size)cFile.Length());
	cFile.Kill();
	mbCarriageReturns = szText.Contains("\r\n");
	mbLastLineEnded = szText.EndsWith('\n');
	if (mbLastLineEnded)
	{
		szText.RemoveLastCharacter();
	}
	maszLines.Split(szText.Text(), '\n');
	for (i = 0; mbCarriageReturns && (i < maszLines.NumElements()); i++)
	{
		if (maszLines.Get(i)->EndsWith('\r'))
		{
			maszLines.Get(i)->RemoveLastCharacter();
		}
	}
	szText.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolutionLines::Write(const char* szFileName)
{
	CTextFile	cFile;
	CChars		szText;
	size		i;
	bool		bResult;

	szText.Init();
	for (i = 0; i < maszLines.NumElements(); i++)
	{
		szText.Append(maszLines.GetText(i));
		if ((i + 1 < maszLines.NumElements()) || mbLastLineEnded)
		{
			szText.Append(mbCarriageReturns ? "\r\n" : "\n");
		}
	}
	cFile.Init(szText.Text());
	bResult = cFile.Write(szFileName);
	cFile.Kill();
	szText.Kill();
	return bResult;
}


void CVcxSolutionLines::Kill(void) { maszLines.Kill(); }


//////////////////////////////////////////////////////////////////////////
//
// A global section's lines: its first, and the line that ends it.
//
//////////////////////////////////////////////////////////////////////////
bool CVcxSolutionLines::FindSection(const char* szSection, size* puiFirst, size* puiEnd)
{
	size	i;
	size	j;

	for (i = 0; i < maszLines.NumElements(); i++)
	{
		if (maszLines.Get(i)->Contains(szSection))
		{
			for (j = i + 1; j < maszLines.NumElements(); j++)
			{
				if (maszLines.Get(j)->Contains(VCX_ROWS_SECTION_END))
				{
					*puiFirst = i + 1;
					*puiEnd = j;
					return true;
				}
			}
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// A line put before the one at uiAt.
//
//////////////////////////////////////////////////////////////////////////
void CVcxSolutionLines::Insert(size uiAt, const char* szLine)
{
	size	i;

	maszLines.Add("");
	for (i = maszLines.NumElements() - 1; i > uiAt; i--)
	{
		maszLines.Get(i)->Set(maszLines.GetText(i - 1));
	}
	maszLines.Get(uiAt)->Set(szLine);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void SVcxSolutionRow::Init(void)
{
	szGuid.Init();
	szConfiguration.Init();
	szKind.Init();
	szValue.Init();
}


void SVcxSolutionRow::Kill(void)
{
	szValue.Kill();
	szKind.Kill();
	szConfiguration.Kill();
	szGuid.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// False for a line that isn't a row.
//
//////////////////////////////////////////////////////////////////////////
bool SVcxSolutionRow::Parse(const char* szLine)
{
	static const char*	aszKinds[] = { ".ActiveCfg", ".Build.0", ".Deploy.0", NULL };
	const char*			szStart;
	const char*			szBrace;
	const char*			szEquals;
	CChars				szKey;
	int					i;

	szStart = szLine;
	while ((*szStart == ' ') || (*szStart == '\t'))
	{
		szStart++;
	}
	szBrace = (*szStart == '{') ? strchr(szStart, '}') : NULL;
	szEquals = strstr(szStart, " = ");
	if (!szBrace || (szBrace[1] != '.') || !szEquals || (szEquals < szBrace))
	{
		return false;
	}
	szKey.Init();
	szKey.AppendSubString(szBrace + 2, szEquals);
	for (i = 0; aszKinds[i] != NULL; i++)
	{
		if (szKey.EndsWith(aszKinds[i]))
		{
			szKind.Set(aszKinds[i] + 1);
			szKey.RemoveFromEnd(strlen(aszKinds[i]));
			szConfiguration.Set(szKey.Text());
			szGuid.Set("");
			szGuid.AppendSubString(szStart, szBrace + 1);
			szValue.Set(szEquals + 3);
			szKey.Kill();
			return true;
		}
	}
	szKey.Kill();
	return false;
}


void SVcxSolutionRow::Write(CChars* pszDest)
{
	pszDest->Set(VCX_ROWS_INDENT);
	pszDest->Append(szGuid)->Append('.')->Append(szConfiguration)->Append('.')->Append(szKind)->Append(" = ")->Append(szValue);
}


//////////////////////////////////////////////////////////////////////////
//
// A configuration line's name: what is left of its " = ".  Empty for a
// line that isn't one.
//
//////////////////////////////////////////////////////////////////////////
static void GetVcxListedConfiguration(const char* szLine, CChars* pszDest)
{
	const char*	szEquals;

	pszDest->Set("");
	while ((*szLine == ' ') || (*szLine == '\t'))
	{
		szLine++;
	}
	szEquals = strstr(szLine, " = ");
	if (szEquals != NULL)
	{
		pszDest->AppendSubString(szLine, szEquals);
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The line of the configurations' section that lists szName; MAX_SIZE if
// none does.
//
//////////////////////////////////////////////////////////////////////////
static size FindVcxListedConfiguration(CVcxSolutionLines* pcLines, const char* szName)
{
	CChars	szListed;
	size	uiFirst;
	size	uiEnd;
	size	i;
	size	uiFound;

	uiFound = MAX_SIZE;
	if (pcLines->FindSection(VCX_ROWS_CONFIGURATIONS, &uiFirst, &uiEnd))
	{
		szListed.Init();
		for (i = uiFirst; (i < uiEnd) && (uiFound == MAX_SIZE); i++)
		{
			GetVcxListedConfiguration(pcLines->maszLines.GetText(i), &szListed);
			if (szListed.Equals(szName))
			{
				uiFound = i;
			}
		}
		szListed.Kill();
	}
	return uiFound;
}


//////////////////////////////////////////////////////////////////////////
//
// A row put among its project's: before the first of them that sorts
// after it by configuration and then kind, else after the last of them, or
// last in the section for a project with none.
//
//////////////////////////////////////////////////////////////////////////
static void InsertVcxSolutionRow(CVcxSolutionLines* pcLines, SVcxSolutionRow* psRow)
{
	SVcxSolutionRow	sOther;
	CChars			szLine;
	size			uiFirst;
	size			uiEnd;
	size			uiAt;
	size			i;
	int				iCompare;

	if (!pcLines->FindSection(VCX_ROWS_PROJECTS, &uiFirst, &uiEnd))
	{
		return;
	}
	uiAt = uiEnd;
	sOther.Init();
	for (i = uiFirst; i < uiEnd; i++)
	{
		if (sOther.Parse(pcLines->maszLines.GetText(i)) && sOther.szGuid.EqualsIgnoreCase(psRow->szGuid.Text()))
		{
			iCompare = sOther.szConfiguration.Compare(psRow->szConfiguration.Text());
			if (iCompare == 0)
			{
				iCompare = sOther.szKind.Compare(psRow->szKind.Text());
			}
			if (iCompare > 0)
			{
				uiAt = i;
				break;
			}
			uiAt = i + 1;
		}
	}
	sOther.Kill();
	szLine.Init();
	psRow->Write(&szLine);
	pcLines->Insert(uiAt, szLine.Text());
	szLine.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szNew listed and given szCopied's rows, in the lines.
//
//////////////////////////////////////////////////////////////////////////
static bool AddVcxSolutionConfiguration(CVcxSolutionLines* pcLines, const char* szNew, const char* szCopied)
{
	CArrayChars		aszCopied;
	SVcxSolutionRow	sRow;
	CChars			szLine;
	CChars			szListed;
	size			uiFirst;
	size			uiEnd;
	size			uiAt;
	size			i;

	if ((FindVcxListedConfiguration(pcLines, szCopied) == MAX_SIZE) || (FindVcxListedConfiguration(pcLines, szNew) != MAX_SIZE) || !pcLines->FindSection(VCX_ROWS_CONFIGURATIONS, &uiFirst, &uiEnd))
	{
		return false;
	}

	szLine.Init(VCX_ROWS_INDENT);
	szLine.Append(szNew)->Append(" = ")->Append(szNew);
	szListed.Init();
	uiAt = uiEnd;
	for (i = uiFirst; i < uiEnd; i++)
	{
		GetVcxListedConfiguration(pcLines->maszLines.GetText(i), &szListed);
		if (szListed.Compare(szNew) > 0)
		{
			uiAt = i;
			break;
		}
	}
	szListed.Kill();
	pcLines->Insert(uiAt, szLine.Text());
	szLine.Kill();

	//The copied rows are found before any is added: an added row moves the ones after it.
	aszCopied.Init();
	sRow.Init();
	if (pcLines->FindSection(VCX_ROWS_PROJECTS, &uiFirst, &uiEnd))
	{
		for (i = uiFirst; i < uiEnd; i++)
		{
			if (sRow.Parse(pcLines->maszLines.GetText(i)) && sRow.szConfiguration.Equals(szCopied))
			{
				aszCopied.Add(pcLines->maszLines.GetText(i));
			}
		}
	}
	for (i = 0; i < aszCopied.NumElements(); i++)
	{
		sRow.Parse(aszCopied.GetText(i));
		sRow.szConfiguration.Set(szNew);
		InsertVcxSolutionRow(pcLines, &sRow);
	}
	sRow.Kill();
	aszCopied.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
static bool RemoveVcxSolutionConfiguration(CVcxSolutionLines* pcLines, const char* szName)
{
	SVcxSolutionRow	sRow;
	size			uiFirst;
	size			uiEnd;
	size			uiListed;
	size			i;

	uiListed = FindVcxListedConfiguration(pcLines, szName);
	if (uiListed == MAX_SIZE)
	{
		return false;
	}
	pcLines->maszLines.Remove(uiListed);
	sRow.Init();
	if (pcLines->FindSection(VCX_ROWS_PROJECTS, &uiFirst, &uiEnd))
	{
		for (i = uiEnd; i > uiFirst; i--)
		{
			if (sRow.Parse(pcLines->maszLines.GetText(i - 1)) && sRow.szConfiguration.Equals(szName))
			{
				pcLines->maszLines.Remove(i - 1);
			}
		}
	}
	sRow.Kill();
	return true;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool AddVcxSolutionConfiguration(const char* szFileName, const char* szNew, const char* szCopied)
{
	CVcxSolutionLines	cLines;
	bool				bResult;

	bResult = cLines.Read(szFileName) && AddVcxSolutionConfiguration(&cLines, szNew, szCopied) && cLines.Write(szFileName);
	cLines.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool RemoveVcxSolutionConfiguration(const char* szFileName, const char* szName)
{
	CVcxSolutionLines	cLines;
	bool				bResult;

	bResult = cLines.Read(szFileName) && RemoveVcxSolutionConfiguration(&cLines, szName) && cLines.Write(szFileName);
	cLines.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
// The rows are added under the new name, where they sort, and those of
// the old one taken away.
//
//////////////////////////////////////////////////////////////////////////
bool RenameVcxSolutionConfiguration(const char* szFileName, const char* szOld, const char* szNew)
{
	CVcxSolutionLines	cLines;
	bool				bResult;

	bResult = cLines.Read(szFileName) && AddVcxSolutionConfiguration(&cLines, szNew, szOld) && RemoveVcxSolutionConfiguration(&cLines, szOld) && cLines.Write(szFileName);
	cLines.Kill();
	return bResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
bool SetVcxSolutionProjectConfiguration(const char* szFileName, const char* szGuid, const char* szSolutionConfiguration, const char* szProjectConfiguration)
{
	CVcxSolutionLines	cLines;
	SVcxSolutionRow		sRow;
	size				uiFirst;
	size				uiEnd;
	size				i;
	bool				bFound;
	bool				bChanged;
	bool				bResult;

	if (!cLines.Read(szFileName))
	{
		cLines.Kill();
		return false;
	}
	bFound = false;
	bChanged = false;
	sRow.Init();
	if (cLines.FindSection(VCX_ROWS_PROJECTS, &uiFirst, &uiEnd))
	{
		for (i = uiFirst; i < uiEnd; i++)
		{
			if (sRow.Parse(cLines.maszLines.GetText(i)) && sRow.szGuid.EqualsIgnoreCase(szGuid) && sRow.szConfiguration.Equals(szSolutionConfiguration))
			{
				bFound = true;
				if (!sRow.szValue.Equals(szProjectConfiguration))
				{
					sRow.szValue.Set(szProjectConfiguration);
					sRow.Write(cLines.maszLines.Get(i));
					bChanged = true;
				}
			}
		}
	}
	sRow.Kill();
	bResult = bFound && (!bChanged || cLines.Write(szFileName));
	cLines.Kill();
	return bResult;
}
