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
#ifndef __IDE_CLAUDE_PANE_H__
#define __IDE_CLAUDE_PANE_H__
#include "StandardLib/Object.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/GridContainer.h"
#include "WindowLib/Label.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/Button.h"
#include "ClaudeLib/ClaudeEvent.h"


//The Claude tab under the editor: the conversation with Claude Code over the box a request is typed in, with Send,
//Stop and New Conversation beside it, and a line saying how the request is going.  What Claude writes is shown whole;
//its tools are a line each (the tool and what it was used on, a file in Claude's folder named from there), and their
//results only when they fail.
//
//The IDE's fonts only have ASCII, so the punctuation Claude writes in Unicode (quotes, dashes, arrows) is shown as the
//ASCII nearest it, and other characters as '?'.
class CIDEClaudePane : public CObject
{
CONSTRUCTABLE(CIDEClaudePane);
DESTRUCTABLE(CIDEClaudePane);
protected:
	Ptr<CWindow>		mpWindow;
	ARGB32				msText;  //The conversation's and request's colour.
	CChars				mszDirectory;	//The folder Claude works in, with a '\' after it.
	bool				mbScrollToEnd;	//Text was added since the conversation was last scrolled to its end.
	Ptr<CGridContainer>	mpPanel;
	Ptr<CLabel>			mpStatus;
	Ptr<CTextArea>		mpConversation;
	Ptr<CScrollPane>	mpConversationPane;
	Ptr<CTextArea>		mpRequest;
	Ptr<CScrollPane>	mpRequestPane;
	Ptr<CButton>		mpSendButton;
	Ptr<CButton>		mpStopButton;
	Ptr<CButton>		mpNewButton;

public:
	void				Init(Ptr<CWindow> pWindow, Ptr<CWindowTheme> pTheme, Ptr<CFont> pStatusFont, Ptr<CFontFamily> pFamily);
	void				Class(void) override;
	void				Free(void) override;

	bool				Save(CObjectWriter* pcFile) override;
	bool				Load(CObjectReader* pcFile) override;

	void				Clear(void);
	void				AppendRequest(const char* szRequest);
	void				AppendEvent(CClaudeEvent* pcEvent);
	void				AppendNote(const char* szNote, bool bError);
	void				SetStatus(const char* szStatus);
	void				SetDirectory(const char* szDirectory);
	void				ScrollToEnd(void);

	char*				GetRequest(void);
	void				SetRequest(const char* szRequest);

	Ptr<CComponent>		GetComponent(void);
	Ptr<CTextArea>		GetRequestArea(void);
	Ptr<CButton>		GetSendButton(void);
	Ptr<CButton>		GetStopButton(void);
	Ptr<CButton>		GetNewButton(void);
	char*				GetConversation(void);
	char*				GetStatus(void);

protected:
	void				Append(const char* szText, ARGB32 sColour);
	void				AppendDetail(const char* szDetail, CChars* pszDest);
};


void ToIDEText(const char* szUTF8, CChars* pszDest);


#endif // __IDE_CLAUDE_PANE_H__

