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
#ifndef __IDE_FILE_RENAME_H__
#define __IDE_FILE_RENAME_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayTemplate.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"


class CIDEContent;


//A file Rename File renames: its full path, and its new name in the same directory.
struct SIDEFileRename
{
	CChars	szOldPath;
	CChars	szNewName;
	CChars	szNewPath;

	void	Init(const char* szOld, const char* szName);
	void	Kill(void);
};


typedef CArrayTemplate<SIDEFileRename>	CArrayIDEFileRenames;


//Rename File (RenameFile.txt): files renamed in their directories, with every #include that finds them, the items of
//every project that has them, their editor tabs and the undo log, as IntelliJ's rename of a file.  A tracked file is
//renamed with git mv.  The #includes' edits are one undo group, and the files edited are saved.  A file's related
//files are the ones a class's Rename takes with it: the same stem in the same directory, or the stem and an _ part
//(Preprocessor.h's are Preprocessor.cpp, Preprocessor_Linux.cpp and Preprocessor_Windows.cpp).
class CIDEFileRename : public CObject
{
CONSTRUCTABLE(CIDEFileRename);
DESTRUCTABLE(CIDEFileRename);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	FindRelatedFiles(const char* szFileName, CArrayChars* paszDest);
	void	AddRenames(const char* szFileName, const char* szNewName, bool bRelated, CArrayIDEFileRenames* pasDest);
	bool	RenameFiles(CArrayIDEFileRenames* pasRenames);

protected:
	bool	FindProblem(CArrayIDEFileRenames* pasRenames, CChars* pszDest);
	bool	MoveFile(SIDEFileRename* psRename);
};


void	KillIDEFileRenames(CArrayIDEFileRenames* pasRenames);


#endif // __IDE_FILE_RENAME_H__
