/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela HCP65IDE

Codaphela HCP65IDE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela HCP65IDE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela HCP65IDE.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#include "BaseLib/FileUtil.h"
#include "IDEListingSource.h"


#define IDE_LISTING_PIECES_GAP	"\t\t...\n"		//Between a line's pieces.


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::Init(void)
{
	mpcListings = NULL;
	mszFileName.Init();
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::Kill(void)
{
	mszFileName.Kill();
}


//////////////////////////////////////////////////////////////////////////
//
// szFileName's lines' code in pcListings.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::Set(CCPP65ListingIndex* pcListings, const char* szFileName)
{
	mpcListings = pcListings;
	mszFileName.Set(szFileName);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::GetLines(CArrayLineZoneLines* pauiDest)
{
	mpcListings->GetLines(mszFileName.Text(), pauiDest);
}


//////////////////////////////////////////////////////////////////////////
//
// The places the line's code is in.
//
//////////////////////////////////////////////////////////////////////////
size CIDEListingSource::NumPages(size uiLine)
{
	CArrayCPP65ListingPlaces	asPlaces;
	size						uiPlaces;

	asPlaces.Init();
	mpcListings->GetPlaces(mszFileName.Text(), uiLine, &asPlaces);
	uiPlaces = asPlaces.NumElements();
	asPlaces.Kill();
	return uiPlaces;
}


//////////////////////////////////////////////////////////////////////////
//
// "Main.lst - _main": the place's listing and function.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::GetTitle(size uiLine, size uiPage, CChars* pszDest)
{
	SCPP65ListingPlace	sPlace;
	CFileUtil			cFileUtil;
	CChars				szListing;

	if (GetPlace(uiLine, uiPage, &sPlace))
	{
		szListing.Init();
		cFileUtil.GetFileName(&szListing, mpcListings->GetListingName(sPlace.uiListing));
		pszDest->Append(szListing);
		pszDest->Append(" - ");
		pszDest->Append(mpcListings->GetFunction(sPlace.uiFunction));
		szListing.Kill();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The place's pieces, "..." between them, without a line end after the
// last.  The comments are left out: they are mostly the C++ line, which is
// right above.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::GetText(size uiLine, size uiPage, CChars* pszDest)
{
	SCPP65ListingPlace	sPlace;
	SCPP65ListingRange*	psRange;
	size				i;
	CChars				szPiece;

	if (!GetPlace(uiLine, uiPage, &sPlace))
	{
		return;
	}
	for (i = 0; i < sPlace.uiNumRanges; i++)
	{
		if (i > 0)
		{
			pszDest->Append(IDE_LISTING_PIECES_GAP);
		}
		psRange = mpcListings->GetRange(sPlace.uiFirstRange + i);
		szPiece.Init();
		mpcListings->AppendRange(psRange, &szPiece);
		AppendWithoutComments(szPiece.Text(), pszDest);
		szPiece.Kill();
	}
	if (pszDest->EndsWith('\n'))
	{
		pszDest->RemoveLastCharacter();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// szLines' lines without their ; comments (a ; in quotes isn't one) or the
// white space before them.  A line that was only a comment, or blank, is
// left out.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::AppendWithoutComments(const char* szLines, CChars* pszDest)
{
	const char*		szLine;
	const char*		szEnd;
	char			cQuote;

	for (szLine = szLines; *szLine != '\0'; szLine = (*szEnd == '\n') ? szEnd + 1 : szEnd)
	{
		//The line's code ends at its end, or at a ; outside quotes.
		cQuote = '\0';
		for (szEnd = szLine; (*szEnd != '\0') && (*szEnd != '\n') && ((*szEnd != ';') || (cQuote != '\0')); szEnd++)
		{
			if (cQuote == '\0')
			{
				if ((*szEnd == '"') || (*szEnd == '\''))
				{
					cQuote = *szEnd;
				}
			}
			else if ((*szEnd == '\\') && (szEnd[1] != '\0') && (szEnd[1] != '\n'))
			{
				szEnd++;
			}
			else if (*szEnd == cQuote)
			{
				cQuote = '\0';
			}
		}
		AppendCode(szLine, szEnd, pszDest);

		//On to the line's end, past its comment.
		while ((*szEnd != '\0') && (*szEnd != '\n'))
		{
			szEnd++;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// The code from szStart to szEnd as a line, without the white space after
// it; nothing if it's all white space.
//
//////////////////////////////////////////////////////////////////////////
void CIDEListingSource::AppendCode(const char* szStart, const char* szEnd, CChars* pszDest)
{
	while ((szEnd > szStart) && ((szEnd[-1] == ' ') || (szEnd[-1] == '\t') || (szEnd[-1] == '\r')))
	{
		szEnd--;
	}
	if (szEnd > szStart)
	{
		pszDest->AppendSubString(szStart, szEnd);
		pszDest->AppendNewLine();
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Place uiPage of line uiLine's.  False if it hasn't that many.
//
//////////////////////////////////////////////////////////////////////////
bool CIDEListingSource::GetPlace(size uiLine, size uiPage, SCPP65ListingPlace* psDest)
{
	CArrayCPP65ListingPlaces	asPlaces;
	bool						bResult;

	asPlaces.Init();
	mpcListings->GetPlaces(mszFileName.Text(), uiLine, &asPlaces);
	bResult = uiPage < asPlaces.NumElements();
	if (bResult)
	{
		*psDest = *asPlaces.Get(uiPage);
	}
	asPlaces.Kill();
	return bResult;
}

