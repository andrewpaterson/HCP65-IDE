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
#ifndef __IDE_CONVERT_ENUM_H__
#define __IDE_CONVERT_ENUM_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayChars.h"
#include "BaseLib/ArrayInt.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/CheckBox.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
struct SIDECaretContext;


//What Convert Enum to Enum Class found for the enum at the caret (on its name, or on one of its enumerators): where
//it is declared, and every place one of its enumerators is named without the enum's name.  Positions are in each
//file's text as it is.
struct SIDEEnumConversion
{
	bool		bPossible;
	CChars		szWhyNot;
	CChars		szKey;				//The enum's symbol.
	CChars		szName;				//Its name, as it goes before an enumerator.
	size		uiEnumerators;
	CArrayChars	aszEnumFiles;		//Each declaration of it (its definition, and any ahead of it):
	CArrayInt	aiEnumAts;			//where its "enum" ends, for " class".
	CArrayChars	aszUseFiles;		//Each place an enumerator is named without the enum's name, a file's together:
	CArrayInt	aiUseAts;			//where the enumerator's name starts, for "EName::".
	CArrayChars	aszCastFiles;		//What is written for the casts an enum class's value needs where it is used as an integer
	CArrayInt	aiCastAts;			//(FindCasts): where,
	CArrayInt	aiCastSpans;		//the length of the expression it is written for (what is written for a longer one is outside),
	CArrayInt	aiCastAfter;		//whether it goes after the expression (a closing bracket) or before it,
	CArrayChars	aszCastTexts;		//and the text.
	CArrayChars	aszReplaceFiles;	//An operator like |= on the enum's value written out ("e = (E)((int)e |"): the file,
	CArrayInt	aiReplaceStarts;	//the operator's place,
	CArrayInt	aiReplaceEnds;
	CArrayChars	aszReplaceTexts;	//and what takes its place.
	CArrayChars	aszCastPlaces;		//Each place a cast is written, for showing: "Use.cpp line 15".

	void	Init(void);
	void	Kill(void);
};


//An enum made an enum class (Refactorings.txt, 22), for the enum the caret is on, or whose enumerator it is on:
//  - "enum EName" becomes "enum class EName" where it is defined and wherever it is declared ahead;
//  - every place one of its enumerators is named gets the enum's name before it ("EN_First" becomes
//    "EName::EN_First", "CTank::EN_First" becomes "CTank::EName::EN_First"), in every file of the solutions that
//    names one.  One that is written with the enum's name already is left, and so are those in the enum's own list.
//An enum class's value is no integer.  Where the enum's value is used as one, the cast is written: every file
//that #includes the enum's is checked with the enum taken as an enum class (CCPPSemanticPass::SetRecordedEnum), and
//each place that refuses is given what it needs: "(int)eLevel + 1", "aiCounts[(int)eLevel]", "if ((int)eMode)",
//"int i = (int)eLevel;", "eFlags = (EFlags)((int)FLAG_A | (int)FLAG_B)", and "eFlags |= FLAG_B" written out as
//"eFlags = (EFlags)((int)eFlags | (int)EFlags::FLAG_B)".  The command shows the places first.  ++ and -- on its
//value have no cast: the enum is refused.  The files are then checked as they would be, and nothing is changed
//if they have more errors than they had.
//
//The command's dialog has a tick, "Write casts where its values are used as integers", on until it is taken off
//(question 58): off, an enum whose values are used as integers is refused, with the places it would need them.
//Refused: an enum with no name, one that is an enum class already, a declaration or an enumerator's use that a
//macro makes, a file that isn't one of the solutions'.  One undo across the files.
class CIDEConvertEnum : public CObject
{
CONSTRUCTABLE(CIDEConvertEnum);
DESTRUCTABLE(CIDEConvertEnum);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>	mpDialog;  //While it is shown.
	bool			mbWriteCasts;  //The dialog's tick, as last chosen: casts are written where its values are used as integers.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanConvert(void);
	void	QueueConvert(void);
	bool	FindConversion(SIDEEnumConversion* psDest);
	void	FindCasts(SIDEEnumConversion* psDest);
	bool	Convert(SIDEEnumConversion* psConversion, CChars* pszStatus = NULL);
	bool	ConvertAtCaret(CChars* pszStatus = NULL, bool bWriteCasts = true);
	bool	ShowConvert(void);
	char*	PrintConversion(SIDEEnumConversion* psConversion, CChars* pszDest);

protected:
	void	ConvertQueued(void);
	bool	Refuse(SIDEEnumConversion* psDest, const char* szWhyNot, const char* szFileName = NULL, int32 iLine = -1);
	bool	FindEnumKeyword(const char* szText, size uiName, size* puiKeywordEnd);
	bool	IsQualified(const char* szText, size uiName, const char* szEnumName);
	void	FindUses(const char* szEnumeratorKey, const char* szEnumerator, const char* szEnumFile, size uiListStart, size uiListEnd, SIDEEnumConversion* psDest);
	void	FindFileCasts(const char* szFileName, SIDEEnumConversion* psDest);
	bool	AddCast(CIDERefactorUnit* pcUnit, const char* szText, const char* szFileName, CCPPSyntax* pcNode, const char* szCast, SIDEEnumConversion* psDest);
	void	AddInsert(SIDEEnumConversion* psDest, const char* szFileName, size uiAt, size uiSpan, bool bAfter, const char* szText);
};


#endif // __IDE_CONVERT_ENUM_H__
