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
#include "BaseLib/ArrayTemplatePtr.h"
#include "BaseLib/TextDocument.h"
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
	void	AppendLine(const char* szFileName, int iLine, CChars* pszDest, size* puiTrimmed = NULL);

protected:
	void	Read(const char* szFileName);
};


//An edit of an open file, as the places kept in the file are moved by it (AdjustIDEPlace): the edit, the column it
//starts at on its line and, for text put in, how much of it is on its last line.
struct SIDEPlaceChange
{
	STextDocumentChange*	psChange;
	int32					iColumn;
	int32					iLastLength;
};


//Whatever previews a file's code (Find in Files' dialog, a results pane) and wants it coloured as an editor is.
class CIDEPreviewAsker
{
public:
	//The file it asked to be highlighted has been: it is coloured if it is still what is shown.
	virtual void	PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights) =0;
};


//A file a preview shows, to be highlighted by the edits thread (CIDEDocuments::HighlightPreview): who asked, the file
//and its text until it is given to the thread, and what the thread's file is known by after (its change count).
struct SIDEPreviewRequest
{
	CIDEPreviewAsker*	pcAsker;	//NULL once it no longer wants it.
	CChars				szFileName;
	CChars				szText;
	size				uiToken;
	bool				bGiven;
};


typedef CArrayTemplate<SIDEPreviewRequest>	CArrayIDEPreviewRequests;


//Told an open file's edits, so the places kept in it (the navigation history's, Find Usages') stay on the same text.
class CIDEDocumentAnchor : public CTextDocumentListener
{
public:
	CIDEDocuments*	mpcDocuments;
	CChars			mszFile;

	void	Init(CIDEDocuments* pcDocuments, const char* szFile);
	void	Kill(void);
	void	DocumentChanged(STextDocumentChange* psChange) override;
};


typedef CArrayTemplatePtr<CIDEDocumentAnchor>	CArrayIDEDocumentAnchors;


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
	CArrayIDEDocumentAnchors	mapcAnchors;		//One for each open file.
	bool						mbReloading;		//A tab's text is being read again: the places in it keep their lines.
	CArrayIDEPreviewRequests	masPreviews;		//The files previews show that are to be, or are being, highlighted.
	size						muiPreviewToken;	//The last request's.

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
	bool				FocusActiveEditor(void);
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
	void				HighlightPreview(CIDEPreviewAsker* pcAsker, const char* szFileName, const char* szText);
	void				ForgetPreviewAsker(CIDEPreviewAsker* pcAsker);
	bool				IsHighlightingPreview(void);
	void				ReloadTab(Ptr<CIDEEditorTab> pTab, const char* szText);
	void				DocumentChanged(const char* szFileName, STextDocumentChange* psChange);

protected:
	void				AddAnchor(Ptr<CIDEEditorTab> pTab);
	void				RemoveAnchor(Ptr<CIDEEditorTab> pTab);
	int64				GiveEditedTabs(int64 iNow, bool bNow);
	void				AddEditedFile(size uiProject, const char* szFileName, const char* szText, size uiChanges, bool bHighlight, bool bIndex, bool bEdited, CArrayIDEEditedFiles* papsFiles);
	bool				TakeEditedFiles(bool bWait);
	void				GivePreviews(CArrayIDEEditedFiles* papsFiles);
	bool				TakePreview(SIDEEditedFile* psFile);
	void				RemovePreview(size uiIndex);
};


bool AdjustIDEPlace(int32* piLine, int32* piColumn, SIDEPlaceChange* psChange);


#endif // __IDE_DOCUMENTS_H__

