/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela IndexLib

Codaphela IndexLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela IndexLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela IndexLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __DOCS_SITE_H__
#define __DOCS_SITE_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/MarkupTag.h"


//A folder of pages and where its files land in the website (mkdocs.yml's extra.sources).
struct SDocsSiteSource
{
	CChars	szFolder;	//Full path, without a separator at its end.
	CChars	szSite;		//'/' between folders, "" for the top.
};


typedef CArrayTemplate<SDocsSiteSource>	CArrayDocsSiteSources;


//A website's map of its pages, from its mkdocs.yml (HCP65/Website/DocsPlan.txt E4): each extra.sources folder
//(relative to the folder holding the repositories, two up) and the site folder its files land in, so a link between
//pages ([text](../compiler/building.md)), which is between their places in the site, finds the file it names
//wherever that lives; and the folders a snippet's path (--8<-- "examples/hello.asm") is looked for in
//(pymdownx.snippets' base_path, relative to the website's folder).  Without a map (Read not called, or failed) a
//link is relative to its page's own folder.
class CDocsSite
{
protected:
	CArrayDocsSiteSources	masSources;
	CArrayChars				maszSnippetFolders;	//Full paths.
	bool					mbRead;

public:
	void		Init(void);
	void		Kill(void);

	bool		Read(const char* szMkdocsFile);
	bool		IsRead(void) const;
	size		NumSources(void) const;

	bool		GetSitePath(const char* szFile, CChars* pszDest);
	bool		GetFile(const char* szSitePath, CChars* pszDest);
	bool		ResolveLink(const char* szPage, const char* szTarget, CChars* pszDest);
	bool		ResolveSnippet(const char* szPage, const char* szPath, CChars* pszDest);

	static bool	IsLocalLink(const char* szTarget);
	static void	GetLinkPath(const char* szTarget, CChars* pszDest);

protected:
	void		ReadSnippetFolders(CMarkupTag* pcRoot, const char* szWebsite);
	static bool	Normalise(CChars* pszSitePath);
};


#endif // __DOCS_SITE_H__

