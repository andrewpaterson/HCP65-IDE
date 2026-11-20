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
#ifndef __IDE_FIND_IN_FILES_H__
#define __IDE_FIND_IN_FILES_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/TextSearch.h"
#include "BaseLib/TextFile.h"
#include "WindowLib/FindInFilesDialog.h"
#include "WindowLib/MessageDialog.h"
#include "BaseLib/ArrayInt.h"
#include "WindowLib/TreeContainer.h"
#include "IDEResultsPane.h"
#include "IDEResultsTabs.h"
#include "WindowLib/FileChooser.h"
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


//One Find in Files, in a tab of the Find tool window: its results pane (CIDEResultsSearch's), the files it found the
//text in and where, its tree's first row, what it was found with (the text, how it had to match, the file mask and
//the scope, for Rerun and for what a result is replaced with), whether it is Replace in Files' and with what, and
//whether edits have moved its results since its tree was made.
class CIDEFindSearch : public CIDEResultsSearch
{
CONSTRUCTABLE(CIDEFindSearch);
DESTRUCTABLE(CIDEFindSearch);
public:
	CArrayChars				maszFoundFiles;  //The files the tree's results are in.
	CArrayIDEFindPlaces		masFound;  //The tree's results, each line's by its node's data less one.
	CChars					mszFoundTitle;  //The tree's first row.
	bool					mbFound;  //Something has been looked for.
	bool					mbMoved;  //Edits have moved results since the tree was shown, the last at miMoved.
	int64					miMoved;
	CTextSearch				mcFoundSearch;  //What the results were found with: what each is replaced with comes from it.
	bool					mbFoundSearch;  //mcFoundSearch is made.
	bool					mbFoundReplacing;  //The results are Replace in Files': each shows what it becomes.
	CChars					mszFoundReplace;  //With this.
	CChars					mszFoundText;  //What was looked for, and how, for Rerun.
	CChars					mszFoundMask;
	CIDEScope				mcFoundScope;
	bool					mbFoundMatchCase;
	bool					mbFoundWholeWords;
	bool					mbFoundRegex;

	void	Init(Ptr<CIDEResultsPane> pPane);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;
};


//IntelliJ IDEA's Find in Files (Control+Shift+F): the text typed is looked for in a scope's files (CIDEScope: the
//solutions', the startup solution's, the active file's project's, the active file, the open files, or those in a
//directory and under it; a tab of the dialog each), whatever its case unless Match case, and as whole words with Words.  Its dialog (CFindInFilesDialog, whose searcher it is) previews the first results as the text is
//typed, each with its file's code under them; enter opens the selected one.  An open file is searched as its editor has
//it.  What was typed, and how, is remembered for the next time; text selected in the editor (on one line) is what is
//looked for instead.
//
//Each Open in Find Window shows what it finds in the Find tool window's tab that is chosen, named for the text, unless
//that tab is pinned (Pin Tab, on the toolbar): then a new tab is made (CIDEResultsTabs, as Find Usages' window has).
//Rerun looks again for a tab's own text, in its own scope.  The button beside the dialog's directory opens a chooser
//of folders.
//
//Open in Find Window lists every result (up to IDE_FIND_RESULTS_MAXIMUM) in the Find tool window beside Find Usages: a
//tree of the files and under each its lines, which opens the place (a results pane, CIDEResultsPane: a line's node's
//data is its result's index, and one).  Their places follow their files' edits, and Next and Previous Occurrence step
//through them.
//
//The preview is coloured as an editor is, by the edits thread (CIDEDocuments::HighlightPreview), a moment after it is
//shown: the dialog's ticks ask for what it made.
//
//The text can be a regular expression (Regex: BaseLib's CRegex; one that isn't is said to be wrong, where the count
//is), and a file mask keeps the search to some of the scope's files: "*.cpp, *.h", names with * and ?, whatever
//their case, parted by commas, a ! before one leaving its files out ("*.h, !Test*").  Both are remembered.
//
//Replace in Files (Control+Shift+R) is the same dialog with what to replace with (a regular expression's $1 is a
//group).  Its Replace All asks, then replaces everything found; its Open in Find Window lists what was found with
//what each becomes after it, and the Find tool window's Replace All and Replace Selected (the selected result, or
//those under the selected file) do it from there.  The files are opened and edited in their editors, all as one undo
//(the refactorings' group), and left for Save All.  A result whose text has changed since it was found is left.
//
//The search is a little at a time (SearchSome): a directory's files are listed, or a file is searched, until the time
//is up, so the dialog is never kept from the keys.
class CIDEFindInFiles : public CObject, public CFindInFilesSearcher, public CIDESteppable, public CIDEResultsSource, public CIDEPreviewAsker, public CIDEFileTexts, public CIDEResultsTabsOwner
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
	bool					mbRegex;
	CChars					mszFileMask;
	CChars					mszReplace;  //What Replace in Files last replaced with.
	bool					mbReplacing;  //The dialog shown is Replace in Files'.

	//The search under way: the dialog's, or Open in Find Window's.
	CTextSearch				mcSearch;
	bool					mbSearch;  //mcSearch is made.
	CChars					mszSearchMask;  //Its file mask.
	CIDEScope				mcScope;  //Where it looks: as the IDE was when it started.
	EIDEScope				maeDialogScopes[IS_Count];  //The scope of each of the dialog's tabs.
	size					muiDialogScopes;
	CArrayChars				maszFiles;  //The files searched and to search.
	CArrayChars				maszDirectories;  //The directories whose files and directories are still to be listed, the next last.
	size					muiNextFile;  //In maszFiles.
	CArrayIDEFindPlaces		masPlaces;  //What was found, a dialog's item's by its data.

	//The Find tool window.
	Ptr<CIDEResultsTabs>	mpTabs;  //A tab for each search.
	Ptr<CIDEFindSearch>		mpSearch;  //The chosen tab's: what the tool window shows.
	Ptr<CDockPane>			mpTool;
	Ptr<CMessageDialog>		mpReplaceDialog;  //While Replace All's question is shown.
	Ptr<CFileChooser>		mpDirectoryChooser;  //While the dialog's directory is being chosen.

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
	void					ReplaceInFiles(void);
	bool					ShowFind(bool bReplace = false);
	bool					CreateFind(bool bReplace = false);
	bool					OpenFindChoice(void);
	size					OpenInFindWindow(bool bRerun = false, bool bReplace = false);
	size					ReplaceAllFound(bool bAsk);
	size					ReplaceSelectedFound(void);
	size					ReplaceFound(CArrayInt* paiResults);
	bool					CanReplaceFound(void);
	Ptr<CFindInFilesDialog>	GetFind(void) const;

	void					FindStart(SFindInFilesQuery* psQuery) override;
	bool					FindStep(CFindInFilesItems* pcDest) override;
	bool					FindPreviewPlace(SFindInFilesItem* psItem, CChars* pszTitle, size* puiStart, size* puiLength) override;
	bool					FindPreviewText(SFindInFilesItem* psItem, CChars* pszText) override;
	void					FindPreviewShown(SFindInFilesItem* psItem, Ptr<CTextArea> pPreview, bool bNewText) override;
	bool					FindProblem(CChars* pszDest) override;
	bool					FindChooseDirectory(const char* szDirectory, CChars* pszDest) override;
	Ptr<CIDEResultsSearch>	CreateResultsSearch(void) override;
	void					ResultsSearchChosen(Ptr<CIDEResultsSearch> pSearch) override;
	Ptr<CIDEResultsTabs>	GetTabs(void) const;
	void					PreviewHighlighted(const char* szFileName, CArrayHighlights* pacHighlights) override;
	void					DialogTick(void);

	void					Clear(void);
	void					AdjustPlaces(const char* szFile, SIDEPlaceChange* psChange);
	void					ReadSettings(CMarkupTag* pcSettings);
	void					WriteSettings(CXMLWriter* pcWriter);
	int64					Poll(int64 iNow);
	Ptr<CTreeContainer>		GetFindTree(void);
	Ptr<CIDEResultsPane>	GetResultsPane(void);
	Ptr<CDockPane>			GetTool(void) const;
	size					NumFound(void);

	Ptr<CDockPane>			GetSteppablePane(void) const override;
	const char*				GetSteppableName(void) override;
	bool					HasSteps(void) override;
	bool					Step(bool bNext, bool bKeepKeys) override;
	bool					GetResultLocation(CIDEResultsPane* pcPane, int64 iData, SIDELocation* psDest) override;
	bool					CanRerunResults(CIDEResultsPane* pcPane) override;
	void					RerunResults(CIDEResultsPane* pcPane) override;
	bool					GetFileText(const char* szFileName, CTextFile* pcFile, const char** pszText, size* puiLength) override;

protected:
	void				ShowQueuedFind(void);
	void				ShowQueuedReplace(void);
	void				ReplaceAllClicked(void);
	void				ReplaceSelectedClicked(void);
	void				AddNodeResults(int32 iNode, CArrayInt* paiResults);
	bool				GetReplacement(const char* szText, size uiLength, SIDEFindPlace* psPlace, CChars* pszDest);
	void				GetInitialText(CChars* pszDest);
	void				GetInitialDirectory(CChars* pszDest);
	size				FindDialogScope(EIDEScope eScope);
	void				StartSearch(const char* szText, bool bMatchCase, bool bWholeWords, bool bRegex, const char* szFileMask);
	bool				SearchSome(CFindInFilesItems* pcDest);
	void				ListDirectory(const char* szDirectory);
	void				SearchFile(size uiFile, CFindInFilesItems* pcDest);
	void				AddItem(CFindInFilesItems* pcDest, size uiFile, SIDEFindPlace* psPlace, const char* szLine, const char* szLineEnd, size uiPlace);
	void				ShowResults(bool bKeepSelected);
	size				GetCommonDirectoryLength(void);
	bool				GetPlaceLocation(CArrayChars* paszFiles, SIDEFindPlace* psPlace, SIDELocation* psDest);
};


bool IsIDEFileMaskMatch(const char* szMask, const char* szFileName);


#endif // __IDE_FIND_IN_FILES_H__

