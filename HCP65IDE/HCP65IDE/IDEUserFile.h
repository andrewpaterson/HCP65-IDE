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
#ifndef __IDE_USER_FILE_H__
#define __IDE_USER_FILE_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "StandardLib/Object.h"


class CIDEContent;
class CMarkupTag;
class CXMLWriter;


//Something kept in the workspace's per-user file (CIDEUserFile): it writes its own tags and reads them back.
class CIDEUserPart
{
public:
	//Its tags, into the file's root.
	virtual void	WriteUser(CXMLWriter* pcWriter) =0;

	//What it wrote is taken from the root's tags, in place of what it had; pcRoot is NULL when there is no file, and
	//it then has nothing.
	virtual void	ReadUser(CMarkupTag* pcRoot) =0;
};


typedef CArrayTemplate<CIDEUserPart*>	CArrayIDEUserParts;


//The per-user file beside the workspace file (or the solution, if there isn't one), <workspace>.hcp65user: what a
//user leaves in a workspace that isn't the workspace's own, as Visual Studio's .suo.  Several features each keep a
//part of it (AddPart): the breakpoints, the bookmarks, the recent files.  It is read when a solution opens (Read:
//each part takes its own) and written whole whenever a part has changed (Write: each part writes its own).
//
//	<UserSettings>
//		<Breakpoint File="D:\...\Start.asm" Line="42" Enabled="true"/>
//		<Bookmark File="D:\...\Shapes.cpp" Line="12" Mnemonic="1"/>
//	</UserSettings>
class CIDEUserFile : public CObject
{
CONSTRUCTABLE(CIDEUserFile);
DESTRUCTABLE(CIDEUserFile);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	CArrayIDEUserParts	mapcParts;  //Not held: the content holds them too.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddPart(CIDEUserPart* pcPart);
	void	GetFileName(CChars* pszDest);
	bool	Read(void);
	bool	Write(void);
};


#endif // __IDE_USER_FILE_H__

