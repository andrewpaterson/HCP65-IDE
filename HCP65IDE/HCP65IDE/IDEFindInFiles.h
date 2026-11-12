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
#ifndef __IDE_FIND_IN_FILES_H__
#define __IDE_FIND_IN_FILES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/TextSearch.h"
#include "BaseLib/TextFile.h"
#include "WindowLib/FindInFilesDialog.h"
#include "WindowLib/TreeContainer.h"
#include "IDEResultsPane.h"
#include "WindowLib/DockManager.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/CommandSet.h"
#include "IDECodeModel.h"
#include "IDEDocuments.h"
#include "IDESteppable.h"
#include "IDEScope.h"


class CIDEContent;
class CIDEFileLines;
struct SIDEPlaceChange;


//A place the text was found: its file (by index, in the files searched or the files found in), where it is in the
//file's text and its line and column (from 0, the column in bytes).
struct SIDEFindPlace
{
	size	uiFile;
	size	uiPosition;
	int32	iLine;
	int32	iColumn;
	int32	iLength;
};


typedef CArrayTemplate<SIDEFindPlace>	CArrayIDEFindPlaces;


//IntelliJ IDEA's Find in Files (Control+Shift+F): the text typed is looked for in a scope's files (CIDEScope: the
//solutions', the startup solution's, the active file's project's, the active file, the open files, or those in a
//directory and under it; a tab of the dialog each), whatever its case unless Match case, and as whole words with Words.  Its dialog (CFindInFilesDialog, whose searcher it is) previews the first results as the text is
//typed, each with its file's code under them; enter opens the selected one.  An open file is searched as its editor has
//it.  What was typed, and how, is remembered for the next time; text selected in the editor (on one line) is what is
//looked for instead.
//
//Open in Find Window lists every result (up to IDE_FIND_RESULTS_MAXIMUM) in the Find tool window beside Find Usages: a
//tree of the files and under each its lines, which opens the place (a results pane, CIDEResultsPane: a line's node's
//data is its result's index, and one).  Their places follow their files' edits, and Next and Previous Occurrence step
//through them.
//
//The preview is coloured as an editor is, by the edits thread (CIDEDocuments::HighlightPreview), a moment after it is
//shown: the dialog's ticks ask for what it made.
//
//The search is a little at a time (SearchSome): a directory's files are listed, or a file is searched, until the time
//is up, so the dialog is never kept from the keys.
class CIDEFindInFiles : public CObject, public CFindInFilesSearcher, public CIDESteppable, public CIDEResultsSource, public CIDEPreviewAsker
{
friend class CIDEScript;
CONSTRUCTABLE(CIDEFindInFiles);
DESTRUCTABLE(CIDEFindInFiles);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CFindInFilesDialog>	mpDialog;  //While Find in Files is shown.

	//What was typed last, and how.
	CChars					mszText;
	CChars					mszDirectory;
	EIDEScope				meScope;
	bool					mbMatchCase;
	bool					mbWholeWords;

	//The search under way: the dialog's, or Open in Find Window's.
	CTextSearch				mcSearch;
	bool					mbSearch;  //mcSearch is made.
	CIDEScope				mcScope;  //Where it looks: as the IDE was when it started.
	EIDEScope				maeDialogScopes[IS_Count];  //The scope of each of the dialog's tabs.
	size					muiDialogScopes;
	CArrayChars				maszFiles;  //The files searched and to search.
	CArrayChars				maszDirectories;  //The directories whose files and directories are still to be listed, the next last.
	size					muiNextFile;  //In maszFiles.
	CArrayIDEFindPlaces		masPlaces;  //What was found, a dialog's item's by its data.

	//The Find tool window.
	Ptr<CIDEResultsPane>	mpPane;  //The tree of what was found.
	Ptr<CDockPane>			mpTool;
	CArrayChars				maszFoundFiles;  //The files the tree's results are in.
	CArrayIDEFindPlaces		masFound;  //The tree's results, each line's by its node's data less one.
	CChars					mszFoundTitle;  //The tree's first row.
	bool					mbFound;  //Something has been looked for.
	bool					mbMoved;  //Edits have moved results since the tree was shown, the last at miMoved.
	int64					miMoved;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void					AddCommands(Ptr<CCommandSet> pCommands);
	void					AddEditItems(Ptr<CMenuContainer> pMenu);
	void					CreatePane(SDockPlace* psPlace);

	void					FindInFiles(void);
	bool					ShowFind(void);
	bool					CreateFind(void);
	bool					OpenFindChoice(void);
	size					OpenInFindWindow(bool bRerun = false);
	Ptr<CFindInFilesDialog>	GetFind(void);

	void					FindStart(SFindInFilesQuery* psQuery) override;
	bool					FindStep(CFindInFilesItems* pcDest) override;
	bool					FindPreviewPlace(SFindInFilesItem* psItem, CChars* pszTitle, size* puiStart, size* puiLength) override;
	bool					FindPreviewText(SFindInFilesItem* psItem, CChars* pszText) override;
	void					FindPreviewShown(SFindInFilesItem* psItem, Ptr<CTextArea> pPreview, bool bNewText) override;
	void					PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights) override;
	void					DialogTick(void);

	void					Clear(void);
	void					AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	void					ReadSettings(CMarkupTag* pcSettings);
	void					WriteSettings(CXMLWriter* pcWriter);
	int64					Poll(int64 iNow);
	Ptr<CTreeContainer>		GetFindTree(void);
	Ptr<CIDEResultsPane>	GetResultsPane(void);
	Ptr<CDockPane>			GetTool(void);
	size					NumFound(void);

	Ptr<CDockPane>			GetSteppablePane(void) override;
	const char*				GetSteppableName(void) override;
	bool					HasSteps(void) override;
	bool					Step(bool bNext, bool bKeepKeys) override;
	bool					GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) override;
	bool					CanRerunResults(CIDEResultsPane* pcPane) override;
	void					RerunResults(CIDEResultsPane* pcPane) override;

protected:
	void				ShowQueuedFind(void);
	void				GetInitialText(CChars* pszDest);
	void				GetInitialDirectory(CChars* pszDest);
	size				FindDialogScope(EIDEScope eScope);
	void				StartSearch(const char* szText, bool bMatchCase, bool bWholeWords);
	bool				SearchSome(CFindInFilesItems* pcDest);
	void				ListDirectory(const char* szDirectory);
	void				SearchFile(size uiFile, CFindInFilesItems* pcDest);
	bool				GetFileText(const char* szFileName, CTextFile* pcFile, char** pszText, size* puiLength);
	void				AddItem(CFindInFilesItems* pcDest, size uiFile, SIDEFindPlace* psPlace, const char* szLine, const char* szLineEnd, size uiPlace);
	void				ShowResults(bool bKeepSelected);
	size				GetCommonDirectoryLength(void);
	bool				GetPlaceLocation(CArrayChars* paszFiles, SIDEFindPlace* psPlace, SIDELocation* psDest);
};


#endif // __IDE_FIND_IN_FILES_H__

