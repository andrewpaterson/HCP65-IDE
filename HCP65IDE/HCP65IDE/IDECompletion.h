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
#ifndef __IDE_COMPLETION_H__
#define __IDE_COMPLETION_H__
#include "StandardLib/Object.h"
#include "WindowLib/ChooserPopup.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDEIcons.h"
#include "IDEClassAccess.h"
#include "IDELightTyper.h"
#include "IDEExpressionTyper.h"
#include "IDEExpectedType.h"


class CIDEContent;
class CIDEEditorTab;


#define IDE_COMPLETION_TYPED	60		//The most of a list's entries whose declarations are read, for their types.
#define IDE_COMPLETION_UNIQUE	2000	//The most entries looked through for one written already.
#define IDE_COMPLETION_UNTYPED	500		//The most symbols listed before any of the name is typed: more than are shown.
#define IDE_COMPLETION_SMART	250		//The most symbols in scope whose types Complete Code by Type reads, the nearest first.
#define IDE_COMPLETION_POLL		30		//Milliseconds between looks for the checked parse's answer.
#define IDE_COMPLETION_WORKING	"Working out what this is..."


//How an entry is written when it is taken.
enum EIDECompletionWrite
{
	ICW_Name,			//Its name.
	ICW_Call,			//A function without parameters: its name and "()", the caret after them.
	ICW_CallArguments,	//A function with parameters: its name and "()", the caret between them, and Parameter Info.
	ICW_Folder,			//An #include's folder: its name and "/", and completion again for what is in it.
};


//An entry of the list: what is written for it, how, and the symbol it is (its key; empty for a keyword or a file).
struct SIDECompletionEntry
{
	CChars				szText;
	EIDECompletionWrite	eWrite;
	CChars				szKey;
};


typedef CArrayTemplate<SIDECompletionEntry>	CArrayIDECompletionEntries;


//Code completion, as IntelliJ IDEA's (NextIDEWork.txt's item 5): a list under the caret of what can be written
//there, which goes on being filtered as the name is typed (its capitals match too: "GSN" finds GetSelectedNode).
//
//It opens by itself straight after "." "->" and "::" are typed, and otherwise on Complete Code (Control+Space).
//What it lists is by where the caret is (FindIDECompletionContext):
//
//  - after "x." or "x->", the fields and functions of what x is, its base classes' too;
//  - after "N::", what the class, enum or namespace N declares; after "::" alone, what is outside any;
//  - on its own, what is in scope: the function's parameters and locals, the class's members, what the namespaces
//    round the caret and the scope outside any declare, and the language's words;
//  - in an #include's name, the headers and folders of the include directories.
//
//Complete Code by Type (Control+Shift+Space) is the same list with only what has the type wanted where the caret is
//(CIDEExpectedTypes: an argument's parameter, the left of an "=", what the function returns, what a switch switches
//on), what is exactly that type first, then pointers to classes derived from it.  Of what is in scope it looks at
//the nearest IDE_COMPLETION_SMART, as each one's declaration is read for its type; the checked parse isn't asked.
//
//After "." and "->" the compiler's own type checker is asked too (CIDEExpressionTyper: a checked parse of the file on
//the edits thread, which takes as long as the file's check does).  The list opens at once with what the light typer
//gives, or with a note that it is working where that is nothing, and what the parse says the expression has is
//added to it when it comes: the list only grows, and the selected row stays selected.  If neither knows, it closes.
//
//A class's member is listed only where it can be used: a private one inside its own class's members, a protected one
//there and in the classes derived from it (CIDEClassAccess; friends aren't known).
//
//What x is comes from the light typer (CIDELightTyper), which reads the index and the text and parses nothing, so
//the list is there on the key.  So that it is in a large workspace too, a list of what is in scope, or of what a
//namespace has, is of the symbols with a word of their name starting with the first character typed (the only ones
//what is typed can match), and is made again when that character changes; before anything is typed it is the first
//IDE_COMPLETION_UNTYPED, the nearest in scope.
//
//The editor keeps the keys.  While the list is open Up, Down, Page Up and Page Down move through it; Enter writes
//the selected entry in place of what was typed of it, and Tab in place of the whole name the caret is in; a function
//is written with its brackets, the caret between them if it has parameters, and Parameter Info shown; Escape, a press
//elsewhere, or the caret leaving the name closes it.  Complete Code with one entry left writes it at once.
class CIDECompletion : public CObject
{
CONSTRUCTABLE(CIDECompletion);
DESTRUCTABLE(CIDECompletion);
protected:
	CIDEContent*				mpcContent;  //Not held: the content holds this.
	Ptr<CChooserPopup>			mpPopup;  //Made when first opened.
	int32						miFileIcon;
	int32						maiSymbolIcons[IST_Count];
	CIDELightTyper				mcTyper;
	CIDEExpressionTyper			mcParse;  //The checked parse, for what is before a "." or "->".
	size						muiAsked;  //The question it was last asked for this list, 0 for none.
	CIDEExpectedTypes			mcExpected;  //The types wanted at the caret, for a list by type.
	bool						mbByType;  //The list is of what has one of them.

	Ptr<CIDEEditorTab>			mpTab;  //The tab it is open in, NULL while it is closed.
	SIDECompletionContext		msContext;  //What is being completed.
	CArrayIDECompletionEntries	masEntries;  //A row's item's data is its entry's index.
	bool						mbReplaceWord;  //The entry chosen replaces the whole name the caret is in (Tab).
	char						mcFilledFor;  //The first character of the name the entries were found for, in lower case; 0 for none typed.

	Ptr<CIDEEditorTab>			mpSeenTab;  //The active editor, its caret and its change count at the last tick:
	size						muiSeenCaret;  //a character typed since may open the list.
	size						muiSeenChanges;

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditItems(Ptr<CMenuContainer> pMenu);
	void	AddEditorActions(Ptr<CTextArea> pEditor);
	int64	Poll(void);
	void	TabClosed(Ptr<CIDEEditorTab> pTab);
	void	Detach(void);

	bool	Complete(void);
	bool	CompleteByType(void);
	bool	HasEditor(void);
	bool	Open(bool bAutomatic, bool bByType = false);
	void	Close(void);
	bool	IsOpen(void);
	bool	Choose(bool bReplaceWord);
	bool	GetSelectedSymbol(CChars* pszKey, SInt32Vec2* psBeside);
	void	Print(CChars* pszDest);

protected:
	void	KeyAction(SComponentInputEvent* psEvent);
	void	EntryChosen(void);
	void	ChooseOnlyEntry(void);
	void	PopupClosed(void);
	void	MakePopup(void);
	void	KillEntries(void);
	bool	Fill(void);
	size	FillSymbols(CArrayIDEScopeSymbols* pasSymbols, bool bMembersOnly);
	size	AddSymbols(CArrayIDEScopeSymbols* pasSymbols, bool bMembersOnly, CArrayInt* paiFits, EIDETypeFit eFit);
	bool	HasEntry(const char* szKey);
	void	FillKeywords(void);
	void	FillIncludes(void);
	bool	IsAccessible(SIDEScopeSymbol* psSymbol, CArrayIDEClassAccesses* pacAccesses);
	void	AddEntry(const char* szName, const char* szDetail, int32 iIcon, const char* szText, EIDECompletionWrite eWrite, const char* szKey);
	void	GetPrefix(CChars* pszDest);
	char	GetFirstCharacter(void);
	void	ShowPrefix(void);
	bool	IsWorking(void);
	void	AskParse(void);
	void	TakeParse(void);
	void	SeeEditor(void);
};


#endif // __IDE_COMPLETION_H__

