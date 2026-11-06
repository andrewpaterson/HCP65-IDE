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
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "StandardLib/Array.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/DockManager.h"
#include "IDECodeModel.h"
#include "IDEEditorTab.h"
#include "SolutionExplorer.h"


class CIDEContent;


//The open files: each in its editor tab (CIDEEditorTab), opened from Solution Explorer, a place found or a script,
//saved, closed, coloured and indexed again from the editor as it changes; and the caret's place in the active one.
class CIDEDocuments : public CObject
{
friend class CIDEContent;
friend class CIDEScript;
CONSTRUCTABLE(CIDEDocuments);
DESTRUCTABLE(CIDEDocuments);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	CArray<CIDEEditorTab>		maTabs;
	Ptr<CIDEEditorTab>			mpActive;			//The selected tab's file, NULL if none is open.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void				SetActiveTab(Ptr<CIDEEditorTab> pTab);
	void				RemoveTab(Ptr<CIDEEditorTab> pTab);
	bool			OpenFile(const char* szFileName, size uiProject = SOLUTION_EXPLORER_NO_PROJECT);
	Ptr<CIDEEditorTab>	OpenFileTab(const char* szFileName, size uiProject, bool bShow);
	void			CloseAllFiles(void);
	bool			SaveFile(Ptr<CIDEEditorTab> pTab);
	bool			SaveActiveFile(void);
	bool			SaveAllFiles(void);
	bool			SetCaretPlace(int iLine, int iColumn);
	void			ReindexStaleTabs(bool bNow);
	void			ReindexAll(void);
	void			AddReindexFile(size uiProject, const char* szFileName, const char* szContents, const char* szHeader, CArrayIDEReindexFiles* papsFiles);
	void			AddReindexHeader(const char* szFileName, CArrayIDEReindexFiles* papsFiles);
	void			ReindexDiscarded(Ptr<CIDEEditorTab> pTab);
	bool			CloseFile(const char* szFileName);
	Ptr<CTextArea>		GetEditor(void);
	Ptr<CScrollPane>	GetEditorPane(void);
	Ptr<CIDEEditorTab>	GetActiveTab(void);
	bool				FindSymbolAtCaret(CArrayIDELocations* pasDest, CChars* pszKey);
	void				GetFileLine(const char* szFileName, int iLine, CChars* pszDest);
	size				NumTabs(void);
	Ptr<CIDEEditorTab>	GetTab(size uiIndex);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				OpenLocation(SIDELocation* psLocation);
	Ptr<CIDEEditorTab>	FindTab(const char* szFileName);
	Ptr<CIDEEditorTab>	FindTab(CDockPane* pcPane);
	Ptr<CIDEEditorTab>	FindTab(CCodeEditor* pcEditor);
	Ptr<CIDEEditorTab>	FindTab(CTextArea* pcEditor);
	void				HighlightTab(Ptr<CIDEEditorTab> pTab);

protected:
};


#endif // __IDE_DOCUMENTS_H__

