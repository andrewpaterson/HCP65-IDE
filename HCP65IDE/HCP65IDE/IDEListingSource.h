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
#ifndef __IDE_LISTING_SOURCE_H__
#define __IDE_LISTING_SOURCE_H__
#include "BaseLib/Chars.h"
#include "CppCompilerLib/CPP65ListingIndex.h"
#include "WindowLib/LineZoneSource.h"


//A file's lines' code in an HCP65 build's listings, for CLineZones: a page for each place a line's code is in, titled
//"Main.lst - _main", its text the place's assembly (a line in pieces in its function, like a for's parts, with "..."
//between them).
class CIDEListingSource : public CLineZoneSource
{
protected:
	CCPP65ListingIndex*	mpcListings;	//Whatever set it keeps it.
	CChars				mszFileName;

public:
	void	Init(void);
	void	Kill(void);

	void	Set(CCPP65ListingIndex* pcListings, const char* szFileName);

	void	GetLines(CArrayLineZoneLines* pauiDest) override;
	size	NumPages(size uiLine) override;
	void	GetTitle(size uiLine, size uiPage, CChars* pszDest) override;
	void	GetText(size uiLine, size uiPage, CChars* pszDest) override;

protected:
	bool	GetPlace(size uiLine, size uiPage, SCPP65ListingPlace* psDest);
	void	AppendWithoutComments(const char* szLines, CChars* pszDest);
	void	AppendCode(const char* szStart, const char* szEnd, CChars* pszDest);
};


#endif // __IDE_LISTING_SOURCE_H__

