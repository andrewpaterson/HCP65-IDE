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
#ifndef __IDE_OUTLINE_H__
#define __IDE_OUTLINE_H__
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/Chars.h"
#include "CppCompilerLib/CPPHighlighter.h"


class CSourceFile;
class CCPPTokeniser;
class CCPPSyntaxTree;
class CCPPSyntax;
class CCPPSyntaxDeclarator;


//What an entry of a file's outline is.
enum EIDEOutlineKind
{
	IOK_Namespace,
	IOK_Class,
	IOK_Enum,
	IOK_Function,
	IOK_Field,
	IOK_Variable,
	IOK_Typedef,

	IOK_NumKinds
};


//Something a file declares: its kind, its name as it is written ("CShape::Draw" outside its class), what follows the
//name (a function's parameters and qualifiers, "(int iX, int iY) const"), whether it is a definition (a function with
//its body, a class or an enum with its members), where its name is and where all of it is (lines and columns from 0,
//an end exclusive), and the entry it is in (a class's member in the class, what a namespace has in the namespace; -1
//at the file's top) with how deep that makes it.
struct SIDEOutlineEntry
{
	EIDEOutlineKind		eKind;
	CChars				szName;
	CChars				szDetail;
	bool				bDefinition;
	SCPPSyntaxRange		sName;
	SCPPSyntaxRange		sRange;
	int32				iParent;
	int32				iDepth;
};


typedef CArrayTemplate<SIDEOutlineEntry>	CArrayIDEOutlineEntries;


//A file's outline (NavigationPlan.txt, S4): what it declares, in the order it declares it, each entry after the one
//it is in: namespaces, classes, enums, functions (declared and defined), fields, variables and typedefs.  What is
//declared inside a function's body isn't in it, nor what a macro made or another file declares.
//
//It is gathered from the syntax tree of a parse of the file (Gather: the edits thread's one parse of an edited file,
//which makes its highlights too), and kept by the file's tab with the change count it was parsed at.  File Structure,
//the Structure tool window, Next and Previous Method and whatever else asks what a file declares, or what a place is
//in (FindEntryAt), read it.
class CIDEOutline
{
protected:
	CArrayIDEOutlineEntries	masEntries;

public:
	void				Init(void);
	void				Kill(void);
	void				Clear(void);

	void				Gather(CSourceFile* pcFile, CCPPTokeniser* pcTokeniser, CCPPSyntaxTree* pcTree);
	void				TakeFrom(CIDEOutline* pcSource);

	size				NumEntries(void) const;
	SIDEOutlineEntry*	Get(size uiIndex);
	int32				FindEntryAt(int iLine, int iColumn);
	int32				FindFunctionAt(int iLine, int iColumn);
	void				Print(CChars* pszDest);

	SIDEOutlineEntry*	Add(EIDEOutlineKind eKind, int32 iParent);
};


void AppendIDETokens(CCPPTokeniser* pcTokeniser, int32 iFirstToken, int32 iLastToken, CChars* pszDest, bool bDeclaration = false);
const char* GetIDEOutlineKindName(EIDEOutlineKind eKind);


#endif // __IDE_OUTLINE_H__

