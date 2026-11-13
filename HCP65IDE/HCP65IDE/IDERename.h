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
#ifndef __IDE_RENAME_H__
#define __IDE_RENAME_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/TextField.h"
#include "WindowLib/Label.h"
#include "WindowLib/CheckBox.h"
#include "BaseLib/MarkupTag.h"
#include "BaseLib/XMLWriter.h"
#include "WindowLib/CommandSet.h"
#include "IDECodeModel.h"


class CIDEContent;
struct SIDEPlace;
typedef CArrayTemplate<SIDEPlace>	CArrayIDEPlaces;


//Where a search found the old name in a file's text.
struct SIDENameOccurrence
{
	size	uiStart;
	int32	iLine;		//0 based.
	int32	iColumn;	//0 based.
	bool	bComment;	//In a comment or a string, else in code (or a text file).
};


typedef CArrayTemplate<SIDENameOccurrence>	CArrayIDENameOccurrences;


//The line and column of offsets into a text, asked for in order.
struct SIDELineCounter
{
	const char*	szText;
	size		uiCounted;
	int32		iLine;
	size		uiLineStart;

	void Init(const char* szSource)
	{
		szText = szSource;
		uiCounted = 0;
		iLine = 0;
		uiLineStart = 0;
	}

	void MoveTo(size uiOffset)
	{
		for (; uiCounted < uiOffset; uiCounted++)
		{
			if (szText[uiCounted] == '\n')
			{
				iLine++;
				uiLineStart = uiCounted + 1;
			}
		}
	}
};


//IntelliJ's Rename (Shift+F6): the symbol at the caret (or the name the selection is in) renamed everywhere the index
//has it, a class's constructors and destructor too, in each file's editor as one edit and across the files as one
//undo group.  The new name is asked for in a dialog; nothing changes if a file changed since it was indexed.  As
//IntelliJ's, it can also search the solution's files for the name in comments and strings, and as text the index
//doesn't have (code a configuration leaves out, macro bodies, the projects' .txt and .md files); each search is
//remembered apart for types and for everything else.
class CIDERename : public CObject
{
CONSTRUCTABLE(CIDERename);
DESTRUCTABLE(CIDERename);
protected:
	CIDEContent*		mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>		mpRenameDialog;  //While Rename is shown.
	Ptr<CTextField>		mpRenameField;
	Ptr<CLabel>			mpRenameError;
	Ptr<CCheckBox>		mpCommentsBox;
	Ptr<CCheckBox>		mpTextBox;
	Ptr<CCheckBox>		mpFilesBox;  //NULL unless the type has files (CIDEFileRename::FindClassFiles).
	CArrayChars			maszClassFiles;  //The files of the type Rename's dialog is renaming.
	SIDERenameTarget	msRenameTarget;  //What Rename's dialog is renaming: its key and name.
	bool				mabComments[2];  //Search in comments and strings, for everything else [0] and for types [1].
	bool				mabText[2];  //Search for text occurrences, the same.
	bool				mbFiles;  //Rename a type's files with it.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pMenu);
	bool	CanRename(void);
	bool	FindRenameTarget(SIDERenameTarget* psDest);
	void	QueueRename(void);
	bool	ShowRename(void);
	bool	RenameSymbol(SIDERenameTarget* psTarget, const char* szNewName, CArrayChars* paszEdited = NULL, CChars* pszStatus = NULL);
	bool	RenameClass(SIDERenameTarget* psTarget, const char* szNewName, bool bFiles);
	void	SetSearches(bool bComments, bool bText);
	void	ReadSettings(CMarkupTag* pcSettings);
	void	WriteSettings(CXMLWriter* pcWriter);

protected:
	void	ShowQueuedRename(void);
	bool	IsExplorerFocussed(void);
	void	ShowRenameStatus(CChars* pszMessage, CChars* pszStatus);
	size	RenameAssembly(SIDERenameTarget* psTarget, const char* szNewName, CArrayChars* paszFiles, size* puiUnsure);
	bool	ApproveRename(void);
	size	FindRenameFile(CArrayChars* paszFiles, const char* szFileName);
	bool	SearchRenameFiles(const char* szName, bool bComments, bool bText, CArrayChars* paszFiles, CArrayIDEPlaces* pasPlaces, CChars* pszChanged);
};


bool IsIDEIdentifier(const char* szName);
size FindIDENameInKey(const char* szKey, const char* szName);


bool	IsIDENameChar(char c);
void	AddIDEOccurrence(SIDELineCounter* psLines, size uiStart, bool bComment, CArrayIDENameOccurrences* pasDest);
void	FindIDEWords(const char* szText, size uiStart, size uiEnd, const char* szName, size uiNameLength, bool bComment, SIDELineCounter* psLines, CArrayIDENameOccurrences* pasDest);
void	FindIDECppOccurrences(const char* szText, size uiLength, const char* szName, bool bCode, bool bComments, SIDELineCounter* psLines, CArrayIDENameOccurrences* pasDest);
bool	IsIDERenameTextFile(const char* szFileName);


#endif // __IDE_RENAME_H__

