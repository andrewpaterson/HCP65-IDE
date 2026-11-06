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
#ifndef __IDE_ASK_CLAUDE_H__
#define __IDE_ASK_CLAUDE_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "WindowLib/Window.h"
#include "ClaudeLib/ClaudeCommand.h"
#include "IDEEditorTab.h"
#include "IDEClaudePane.h"
#include "WindowLib/DockManager.h"


class CIDEContent;


//The Claude tab under the editor: requests, such as refactorings, sent to Claude Code run headless in the solution's
//folder (see CClaudeCommand), saying where the caret and selection are.  Files are saved before a request, and the
//open files Claude changes are read again as it changes them.
class CIDEAskClaude : public CObject
{
friend class CIDEContent;
friend class CIDEScript;
CONSTRUCTABLE(CIDEAskClaude);
DESTRUCTABLE(CIDEAskClaude);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CIDEClaudePane>	mpClaudePane;  //The Claude tab,
	Ptr<CDockPane>		mpClaudeTool;  //and its tool window.
	CClaudeCommand				mcClaude;			//The request Claude is working on, or the last.
	size						muiClaudeReloaded;	//How many of the files Claude changed have been looked at.
	size						muiClaudeTools;		//How many tools Claude has used on the request.
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool				AskClaude(const char* szRequest);
	void				ClaudeSendClicked(void);
	void				ClaudeStopClicked(void);
	void				ClaudeNewClicked(void);
	void				ShowClaudeTab(void);
	bool				IsClaudeRunning(void);
	CClaudeCommand*		GetClaude(void);
	Ptr<CIDEClaudePane>	GetClaudePane(void);
	Ptr<CDockPane>		GetTool(void);
	void				CreatePane(SDockPlace* psPlace);
	void				ScrollToEnd(void);

protected:
	bool				GetClaudeDirectory(CChars* pszDirectory);
	void				AppendClaudeContext(CChars* pszRequest);
	void				PollClaude(Ptr<CWindow> pWindow);
	void				ReloadClaudeFiles(bool bAll);
	bool				ReloadClaudeFile(Ptr<CIDEEditorTab> pTab);
	Ptr<CIDEEditorTab>	FindClaudeTab(const char* szFileName);
};


#endif // __IDE_ASK_CLAUDE_H__

