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
#ifndef __IDE_CONTENT_H__
#define __IDE_CONTENT_H__
#include "SupportLib/FontColours.h"
#include "SupportLib/FontFamily.h"
#include "WindowLib/Window.h"
#include "WindowLib/WindowTheme.h"
#include "WindowLib/TextArea.h"
#include "WindowLib/ScrollPane.h"
#include "WindowLib/ListListener.h"
#include "WindowLib/TreeContainer.h"
#include "WindowLib/ComboBox.h"
#include "WindowLib/ComboBoxListener.h"
#include "VisualStudioLib/VcxSolution.h"
#include "IDECodeModel.h"
#include "SolutionExplorer.h"


class CIDEContent;


//Hears a tree's items being opened (a double click or enter) and tells the content, with one of its methods.
class CIDEActivationListener : public CListListener
{
CONSTRUCTABLE(CIDEActivationListener);
public:
	CIDEContent*	mpcContent;
	void			(CIDEContent::*mfActivated)(void);

	void	Init(CIDEContent* pcContent, void (CIDEContent::*fActivated)(void));
	void	ItemActivated(CUnknown* pcSource, void* pvContext) override;
};


//Hears the solution configuration being chosen.
class CIDEConfigurationListener : public CComboBoxListener
{
CONSTRUCTABLE(CIDEConfigurationListener);
public:
	CIDEContent*	mpcContent;

	void	Init(CIDEContent* pcContent);
	void	SelectionChanged(CUnknown* pcSource, void* pvContext) override;
};


//The IDE's window's content: Solution Explorer on the left, the editor on the right with the references found under
//it.  F12 goes to the definition of what is at the caret and Shift+F12 finds its references.  It is an object; keep it
//in the root and call Setup.
class CIDEContent : public CObject
{
CONSTRUCTABLE(CIDEContent);
DESTRUCTABLE(CIDEContent);
protected:
	Ptr<CWindow>				mpWindow;
	Ptr<CFont>					mpWhite;
	Ptr<CFontColours>			mpFontColours;
	Ptr<CWindowTheme>			mpTheme;
	Ptr<CFontFamily>			mpCodeFamily;
	Ptr<CTextArea>				mpEditor;
	Ptr<CScrollPane>			mpEditorPane;
	Ptr<CSolutionExplorer>		mpExplorer;
	CIDEActivationListener		mcExplorerListener;
	Ptr<CComboBox>				mpConfigurations;	//The solution configuration the code is parsed as.
	CIDEConfigurationListener	mcConfigurationsListener;
	Ptr<CTreeContainer>			mpReferences;		//What Find All References found.
	CIDEActivationListener		mcReferencesListener;
	CArrayIDELocations			masReferences;		//The references' places; a node's data is its index plus one.
	CVcxSolution				mcSolution;
	bool						mbSolution;
	CIDECodeModel				mcCodeModel;		//Uses mcSolution; initialised when a solution is opened.
	CChars						mszFileName;		//The file in the editor.
	size						muiProject;			//The solution's project it is in, SOLUTION_EXPLORER_NO_PROJECT if none.
	size						muiSeenChanges;		//The editor's change count when it was last looked at.
	int64						miChangedTime;		//When the editor's text last changed (GetDebugTickCount).
	bool						mbRecolour;			//The text changed and hasn't been highlighted since.
	bool						mbEdited;			//The text was changed since the file was opened.

public:
	void			Init(void);
	void			Class(void) override;
	void			Free(void) override;

	bool			Save(CObjectWriter* pcFile) override;
	bool			Load(CObjectReader* pcFile) override;

	bool			Setup(Ptr<CWindow> pWindow, const char* szUIFont, const char* szCodeFont);

	bool			OpenSolution(const char* szFileName);
	bool			OpenFile(const char* szFileName, size uiProject = SOLUTION_EXPLORER_NO_PROJECT);
	void			ExplorerActivated(void);
	void			ReferenceActivated(void);
	bool			GoToDefinition(void);
	bool			FindReferences(void);
	bool			SetCaretPlace(int iLine, int iColumn);
	bool			SetConfiguration(const char* szConfiguration);
	void			ConfigurationChosen(void);
	void			Tick(Ptr<CWindow> pWindow);
	CIDECodeModel*	GetCodeModel(void);

	Ptr<CTextArea>		GetEditor(void);
	Ptr<CScrollPane>	GetEditorPane(void);
	CSolutionExplorer*	GetExplorer(void);
	Ptr<CTreeContainer>	GetReferences(void);

protected:
	void				HighlightFile(const char* szFileName, const char* szText, size uiProject, bool bEdited);
	void				CodeCommandAction(SComponentInputEvent* psEvent);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				OpenLocation(SIDELocation* psLocation);
	void				AddReference(int32 iParent, SIDELocation* psLocation, size uiIndex);
	void				ShowReferencesStatus(const char* szText);
};


Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder);
ARGB32     GetHighlightColour(ECPPHighlight eKind);


#endif // __IDE_CONTENT_H__

