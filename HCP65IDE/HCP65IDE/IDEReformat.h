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
#ifndef __IDE_REFORMAT_H__
#define __IDE_REFORMAT_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayChars.h"
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/CommandSet.h"
#include "TemplateLib/CodeFormatter.h"


class CIDEContent;
class CIDEFileEdits;


//IntelliJ IDEA's Reformat Code (Control+Alt+L): the active file's C or C++ laid out in the house style
//(CCodeFormatter, which says what that is), or only the lines its selection is on; and, from Solution Explorer's
//menu, the selected file's, or every C and C++ file's of the selected folder, project or solution.  A function with
//no comment over it gets the Definition Banner template's.  Each file is one edit and all of them one undo; the
//files are left changed, not saved, as a refactoring leaves them.  The status line says what was changed.
class CIDEReformat : public CObject
{
CONSTRUCTABLE(CIDEReformat);
DESTRUCTABLE(CIDEReformat);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	size			mauiCounts[CFC_NumChanges];  //What the last reformat changed, in all its files.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditItems(Ptr<CMenuContainer> pMenu);
	void	AddExplorerItems(Ptr<CMenuContainer> pMenu);

	bool	ReformatActiveFile(void);
	size	ReformatExplorerSelection(void);
	bool	CanReformatActiveFile(void);
	bool	CanReformatExplorerSelection(void);

protected:
	void	QueueReformat(void);
	void	QueueExplorerReformat(void);
	void	ReformatQueued(void);
	void	ExplorerReformatQueued(void);
	bool	IsReformatted(const char* szFileName);
	void	SetBanner(CCodeFormatter* pcFormatter);
	bool	AddFileEdits(CIDEFileEdits* pcEdits, const char* szFileName, size uiFrom, size uiTo);
	void	FindExplorerFiles(int32 iNode, CArrayChars* paszFiles);
	void	AppendCounts(CChars* pszDest);
};


#endif // __IDE_REFORMAT_H__

