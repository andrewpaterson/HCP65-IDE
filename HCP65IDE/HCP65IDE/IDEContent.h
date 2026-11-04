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
#include "BuildLib/BuildConfigurations.h"
#include "BuildLib/SolutionBuild.h"
#include "CppParserLib/CPP65ListingIndex.h"
#include "WindowLib/Button.h"
#include "WindowLib/ButtonListener.h"
#include "WindowLib/TabbedContainer.h"
#include "WindowLib/TabbedContainerListener.h"
#include "IDECodeModel.h"
#include "IDEEditorTab.h"
#include "IDEBuildPane.h"
#include "IDEIcons.h"
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


//Hears the Build button being clicked.
class CIDEBuildListener : public CButtonListener
{
CONSTRUCTABLE(CIDEBuildListener);
public:
	CIDEContent*	mpcContent;

	void	Init(CIDEContent* pcContent);
	void	ButtonClicked(CUnknown* pcSource, void* pvContext) override;
};


//Hears the editor's tabs being selected and closed.
class CIDETabsListener : public CTabbedContainerListener
{
CONSTRUCTABLE(CIDETabsListener);
public:
	CIDEContent*	mpcContent;

	void	Init(CIDEContent* pcContent);
	void	TabSelected(CUnknown* pcSource, void* pvContext) override;
	void	TabRemoved(CUnknown* pcSource, void* pvContext) override;
};


//The IDE's window's content: Solution Explorer on the left, the editor's tabs of open files on the right with the
//references found under them.  F12 goes to the definition of what is at the caret and Shift+F12 finds its references.  It is an object; keep it
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
	Ptr<CTabbedContainer>		mpEditors;			//A tab for each open file.
	CArray<CIDEEditorTab>		maTabs;
	Ptr<CIDEEditorTab>			mpActive;			//The selected tab's file, NULL if none is open.
	CIDETabsListener			mcTabsListener;
	CArray<CImageCel>			maFileIcons;		//By EIDEFileType.
	Ptr<CSolutionExplorer>		mpExplorer;
	CIDEActivationListener		mcExplorerListener;
	Ptr<CComboBox>				mpConfigurations;	//The build configuration's configuration; the code is parsed as its solution configuration.
	Ptr<CComboBox>				mpPlatforms;		//And its platform.
	CIDEConfigurationListener	mcConfigurationsListener;
	CBuildConfigurations		mcConfigurations;	//The solution's, from beside it or the defaults.
	size						muiConfiguration;	//The chosen one, MAX_SIZE if none.
	Ptr<CButton>				mpBuildButton;
	CIDEBuildListener			mcBuildListener;
	CSolutionBuild				mcBuild;			//The build running, or the last.
	CChars						mszLibc65;			//The HCP65 compiler's system headers.
	CCPP65ListingIndex			mcListings;			//The last HCP65 build's listings, given to the open files.
	size						muiListingToShow;	//The line to show the code of after the next build, MAX_SIZE for none.
	Ptr<CTabbedContainer>		mpBottomTabs;		//Under the editor: Find All References and Build.
	Ptr<CTabComponent>			mpReferencesTab;
	Ptr<CTabComponent>			mpBuildTab;
	Ptr<CIDEBuildPane>			mpBuildPane;
	Ptr<CTreeContainer>			mpReferences;		//What Find All References found.
	CIDEActivationListener		mcReferencesListener;
	CArrayIDELocations			masReferences;		//The references' places; a node's data is its index plus one.
	CVcxSolution				mcSolution;
	bool						mbSolution;
	CIDECodeModel				mcCodeModel;		//Uses mcSolution; initialised when a solution is opened.

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
	void			SetComboBoxItems(Ptr<CComboBox> pComboBox, bool bPlatforms);
	void			ShowConfiguration(void);
	void			SelectComboBoxItem(Ptr<CComboBox> pComboBox, const char* szText);
	void			Tick(Ptr<CWindow> pWindow);
	void			TabSelected(CUnknown* pcTab);
	void			TabRemoved(CUnknown* pcTab);
	bool			CloseFile(const char* szFileName);
	CIDECodeModel*	GetCodeModel(void);
	CBuildConfiguration*	GetConfiguration(void);

	Ptr<CTextArea>		GetEditor(void);
	Ptr<CScrollPane>	GetEditorPane(void);
	Ptr<CIDEEditorTab>	GetActiveTab(void);
	Ptr<CTabbedContainer>	GetEditors(void);
	CSolutionExplorer*	GetExplorer(void);
	Ptr<CTreeContainer>	GetReferences(void);
	Ptr<CIDEBuildPane>	GetBuildPane(void);
	void				ShowBuildTab(void);
	void				SetLibc65Directory(const char* szDirectory);
	bool				Build(void);
	bool				IsBuilding(void);
	void				ShowListingAfterBuild(size uiLine);
	CCPP65ListingIndex*	GetListings(void);

protected:
	void				HighlightTab(Ptr<CIDEEditorTab> pTab);
	Ptr<CIDEEditorTab>	FindTab(const char* szFileName);
	Ptr<CIDEEditorTab>	FindTab(CUnknown* pcTab);
	void				CodeCommandAction(SComponentInputEvent* psEvent);
	bool				GetCaretPlace(int* piLine, int* piColumn);
	bool				OpenLocation(SIDELocation* psLocation);
	void				AddReference(int32 iParent, SIDELocation* psLocation, size uiIndex);
	void				ShowReferencesStatus(const char* szText);
	void				PollBuild(Ptr<CWindow> pWindow);
	void				ReadListings(CArrayChars* paszLines);
	void				SetListings(bool bListings);
};


Ptr<CFont> GenerateIDEFont(const char* szFontFilename, const char* szName, float32 fPixelSize, EColourOrder eOrder);
ARGB32     GetHighlightColour(ECPPHighlight eKind);


#endif // __IDE_CONTENT_H__

