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
#ifndef __IDE_DOCUMENTS_H__
#define __IDE_DOCUMENTS_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/TextFile.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "StandardLib/Array.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/CommandSet.h"
#include "IDECodeModel.h"
#include "IDEEditorTab.h"
#include "SolutionExplorer.h"


class CIDEContent;
class CIDEDocuments;


//The text of files' lines, for a list of places to show (Find Usages' tree, Control+B's popup): as its editor has it if
//the file is open, else as it is on the disk.  A file that isn't open is read, and its lines found, once for all the
//lines asked of it in a row: places come a file at a time, and reading the file for each made thousands of them slow.
class CIDEFileLines
{
protected:
	CIDEDocuments*			mpcDocuments;
	CChars					mszFileName;	//The file last asked for that isn't open.
	CTextFile				mcFile;			//Its text, if mbRead.
	CArrayTemplate<size>	mauiLineStarts;
	bool					mbRead;			//False if it couldn't be read.

public:
	void	Init(CIDEDocuments* pcDocuments);
	void	Kill(void);
	void	AppendLine(const char* szFileName, int iLine, CChars* pszDest);

protected:
	void	Read(const char* szFileName);
};


//The open files: each in its editor tab (CIDEEditorTab), opened from Solution Explorer, a place found or a script,
//saved, closed, coloured and indexed again from the editor as it changes; and the caret's place in the active one.
class CIDEDocuments : public CObject
{
friend class CIDEScript;
CONSTRUCTABLE(CIDEDocuments);
DESTRUCTABLE(CIDEDocuments);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds it.
	CArray<CIDEEditorTab>		maTabs;
	Ptr<CIDEEditorTab>			mpActive;			//The selected tab's file, NULL if none is open.
	size						muiIndexings;		//The code model's indexings the tabs have seen (NumIndexings).

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				AddCommands(Ptr<CCommandSet> pCommands);
	void				AddFileItems(Ptr<CMenuContainer> pFile);
	void				AddWindowItems(Ptr<CMenuContainer> pWindowMenu);
	void				SetActiveTab(Ptr<CIDEEditorTab> pTab);
	void				RemoveTab(Ptr<CIDEEditorTab> pTab);
	bool				OpenFile(const char* szFileName, size uiProject = SOLUTION_EXPLORER_NO_PROJECT);
	Ptr<CIDEEditorTab>	OpenFileTab(const char* szFileName, size uiProject, bool bShow);
	void				CloseAllFiles(void);
	bool				SaveFile(Ptr<CIDEEditorTab> pTab);
	bool				SaveActiveFile(void);
	bool				SaveAllFiles(void);
	bool				SetCaretPlace(int iLine, int iColumn);
	int64				UpdateEditedTabs(int64 iNow);
	void				UpdateEditedTabsNow(void);
	void				ReindexAll(void);
	void				ReindexDiscarded(Ptr<CIDEEditorTab> pTab);
	bool				CloseFile(const char* szFileName);
	Ptr<CTextArea>		GetEditor(void);
	Ptr<CScrollPane>	GetEditorPane(void);
	Ptr<CIDEEditorTab>	GetActiveTab(void);
	bool				FindSymbolPlace(int* piLine, int* piColumn, uint8* puiRole);
	bool				FindSymbolAtCaret(CArrayIDELocations* pasDest, CChars* pszKey);
	void				GetFileLine(const char* szFileName, int iLine, CChars* pszDest);
	size				NumTabs(void);
	Ptr<CIDEEditorTab>	GetTab(size uiIndex);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				OpenLocation(SIDELocation* psLocation, bool bSelectName = false);
	void				SelectAtCaret(size uiLength);
	void				SelectWordAtCaret(void);
	Ptr<CIDEEditorTab>	FindTab(const char* szFileName);
	Ptr<CIDEEditorTab>	FindTab(CDockPane* pcPane);
	Ptr<CIDEEditorTab>	FindTab(CCodeEditor* pcEditor);
	Ptr<CIDEEditorTab>	FindTab(CTextArea* pcEditor);
	void				RequestHighlight(Ptr<CIDEEditorTab> pTab);

protected:
	int64				GiveEditedTabs(int64 iNow, bool bNow);
	void				AddEditedFile(size uiProject, const char* szFileName, const char* szText, size uiChanges, bool bHighlight, bool bIndex, bool bEdited, CArrayIDEEditedFiles* papsFiles);
	bool				TakeEditedFiles(bool bWait);
};


#endif // __IDE_DOCUMENTS_H__

