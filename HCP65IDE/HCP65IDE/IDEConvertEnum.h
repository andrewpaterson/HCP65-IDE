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

	void	Init(void);
	void	Kill(void);
};


//An enum made an enum class (Refactorings.txt, 22), for the enum the caret is on, or whose enumerator it is on:
//  - "enum EName" becomes "enum class EName" where it is defined and wherever it is declared ahead;
//  - every place one of its enumerators is named gets the enum's name before it ("EN_First" becomes
//    "EName::EN_First", "CTank::EN_First" becomes "CTank::EName::EN_First"), in every file of the solutions that
//    names one.  One that is written with the enum's name already is left, and so are those in the enum's own list.
//An enum class's value is no integer: the files changed are checked as they would be, and nothing is changed if
//they have more errors than they had (a value of it used as an integer or a condition, or an integer given to it).
//Refused: an enum with no name, one that is an enum class already, a declaration or an enumerator's use that a
//macro makes, a file that isn't one of the solutions'.  One undo across the files.
class CIDEConvertEnum : public CObject
{
CONSTRUCTABLE(CIDEConvertEnum);
DESTRUCTABLE(CIDEConvertEnum);
protected:
	CIDEContent*	mpcContent;  //Not held: the content holds it.

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
	bool	Convert(SIDEEnumConversion* psConversion, CChars* pszStatus = NULL);
	bool	ConvertAtCaret(CChars* pszStatus = NULL);
	char*	PrintConversion(SIDEEnumConversion* psConversion, CChars* pszDest);

protected:
	void	ConvertQueued(void);
	bool	Refuse(SIDEEnumConversion* psDest, const char* szWhyNot, const char* szFileName = NULL, int32 iLine = -1);
	bool	FindEnumKeyword(const char* szText, size uiName, size* puiKeywordEnd);
	bool	IsQualified(const char* szText, size uiName, const char* szEnumName);
	void	FindUses(const char* szEnumeratorKey, const char* szEnumerator, const char* szEnumFile, size uiListStart, size uiListEnd, SIDEEnumConversion* psDest);
};


#endif // __IDE_CONVERT_ENUM_H__
