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
class CMarkupTag;
class CXMLWriter;


//Where Claude Code is started for a request, chosen on Settings' Claude page.
enum EIDEClaudeDirectory
{
	ICD_Solution,		//The startup solution's folder (the open file's, with no solution).
	ICD_Repository,		//The repository that folder is in; the folder itself if it is in none.
	ICD_Folder,			//The folder typed in the settings.

	ICD_NumDirectories
};


//The Claude tab under the editor: requests, such as refactorings, sent to Claude Code run headless in the solution's
//folder (see CClaudeCommand), saying where the caret and selection are.  Files are saved before a request, and the
//open files Claude changes are read again as it changes them.
//
//Claude Code refuses to read or search outside the folders it is given, and a solution's projects are mostly outside
//its folder (libraries beside it, and in other repositories).  So it is also given the repository of each solution
//and project in the workspace, or the project's folder if it is in none (AddClaudeDirectories).
//
//The folder it is started in is a setting (EIDEClaudeDirectory, Settings' Claude page): it is where Claude Code looks
//for its CLAUDE.md and keeps its conversations, so a request sent from another folder than the last starts a new
//conversation.
class CIDEAskClaude : public CObject
{
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
	EIDEClaudeDirectory			meDirectory;		//Where Claude is started.
	CChars						mszFolder;			//The folder typed in the settings, for ICD_Folder.
	CChars						mszAskedDirectory;	//Where the conversation's last request was started, "" before the first.
public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	bool					AskClaude(const char* szRequest);
	bool					IsClaudeRunning(void);
	Ptr<CIDEClaudePane>		GetClaudePane(void) const;
	Ptr<CDockPane>			GetTool(void) const;
	void					CreatePane(SDockPlace* psPlace);
	void					ScrollToEnd(void);
	void					PollClaude(Ptr<CWindow> pWindow);

	EIDEClaudeDirectory		GetDirectorySetting(void) const;
	const char*				GetFolderSetting(void) const;
	void					SetDirectorySettings(EIDEClaudeDirectory eDirectory, const char* szFolder);
	void					ReadSettings(CMarkupTag* pcSettings);
	void					WriteSettings(CXMLWriter* pcWriter);
	bool					GetSolutionDirectory(CChars* pszDirectory);

protected:
	bool					GetClaudeDirectory(CChars* pszDirectory);
	void					AddClaudeDirectories(const char* szDirectory);
	void					AddClaudeDirectory(const char* szFolder, const char* szDirectory);
	void					AppendClaudeContext(CChars* pszRequest);
	void					ReloadClaudeFiles(bool bAll);
	bool					ReloadClaudeFile(Ptr<CIDEEditorTab> pTab);
	Ptr<CIDEEditorTab>		FindClaudeTab(const char* szFileName);
	void					ClaudeSendClicked(void);
	void					ClaudeStopClicked(void);
	void					ClaudeNewClicked(void);
	void					ShowClaudeTab(void);
	const CClaudeCommand*	GetClaude(void) const;
};


const char* GetIDEClaudeDirectoryName(EIDEClaudeDirectory eDirectory);
const char* GetIDEClaudeDirectoryText(EIDEClaudeDirectory eDirectory);
bool FindIDEClaudeDirectory(const char* szName, EIDEClaudeDirectory* peDirectory);


#endif // __IDE_ASK_CLAUDE_H__

