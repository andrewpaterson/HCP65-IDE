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
#include "BaseLib/ArrayInt.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/XMLWriter.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/TextField.h"
#include "WindowLib/Label.h"
#include "WindowLib/CheckBox.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"


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


//Why Rename File edits a place: an #include that finds a file renamed, or its old name in a comment or string, or in a
//project's text file.
enum EIDEFileRenameEdit
{
	IFRE_Include,
	IFRE_Comment,
	IFRE_Text,
	IFRE_NumKinds
};


struct SIDEFileEdit;
typedef CArrayTemplate<SIDEFileEdit>	CArrayIDEFileEdits;


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
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpDialog;  //While Rename File is shown.
	Ptr<CTextField>		mpNameField;
	Ptr<CCheckBox>		mpRelatedBox;  //NULL if the file has no related files.
	Ptr<CCheckBox>		mpCommentsBox;
	Ptr<CCheckBox>		mpTextBox;
	Ptr<CLabel>			mpError;
	CChars				mszFileName;  //The file Rename File is shown for, or queued for.
	bool				mbRelated;  //Rename File's ticks, remembered.
	bool				mbComments;
	bool				mbText;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddDocumentItems(Ptr<CMenuContainer> pMenu);
	void	QueueRenameFile(const char* szFileName);
	bool	ShowRenameFile(const char* szFileName);
	void	SetSearches(bool bComments, bool bText);
	void	ReadSettings(CMarkupTag* pcSettings);
	void	WriteSettings(CXMLWriter* pcWriter);

	void	FindRelatedFiles(const char* szFileName, CArrayChars* paszDest);
	void	AddRenames(const char* szFileName, const char* szNewName, bool bRelated, CArrayIDEFileRenames* pasDest);
	bool	RenameFiles(CArrayIDEFileRenames* pasRenames, bool bComments, bool bText, CChars* pszStatus = NULL);
	bool	FindProblem(CArrayIDEFileRenames* pasRenames, CChars* pszDest);
	bool	FindClassFiles(SIDERenameTarget* psTarget, CArrayChars* paszDest);
	bool	AddClassRenames(const char* szOldName, const char* szNewName, CArrayChars* paszFiles, CArrayIDEFileRenames* pasDest);
	bool	MoveFile(const char* szFrom, const char* szTo, CArrayInt* paiProjects);
	bool	IsRenamable(const char* szFileName);
	void	RenameExplorerFile(void);
	bool	CanRenameExplorerFile(void);
	bool	GetExplorerFile(CChars* pszDest);

protected:
	size	FindEditedFile(CArrayChars* paszFiles, const char* szFileName);
	void	ShowRenameStatus(CChars* pszMessage, CChars* pszStatus);
	bool	AddEdit(CArrayChars* paszFiles, CArrayIDEFileEdits* pasEdits, const char* szFileName, size uiStart, const char* szOldName, size uiRename, uint8 uiKind, CChars* pszChanged);
	bool	SearchFiles(CArrayChars* paszOldNames, bool bComments, bool bText, CArrayChars* paszFiles, CArrayIDEFileEdits* pasEdits, CChars* pszChanged);
	void	ShowQueuedRenameFile(void);
	bool	ApproveRenameFile(void);
	void	RenameActiveFile(void);
	void	RenameMenuFile(void);
	bool	CanRenameActiveFile(void);
	bool	CanRenameMenuFile(void);
	bool	GetMenuFile(CChars* pszDest);
};


void	KillIDEFileRenames(CArrayIDEFileRenames* pasRenames);


#endif // __IDE_FILE_RENAME_H__
