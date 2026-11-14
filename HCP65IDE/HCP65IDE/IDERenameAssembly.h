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
#ifndef __IDE_RENAME_ASSEMBLY_H__
#define __IDE_RENAME_ASSEMBLY_H__
#include "BaseLib/Chars.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayChars.h"


//What Rename changes in hand written assembly (RenameFile.txt's assembly step): a C++ name in the compiler's labels
//(CPPUnitLabels.cpp) and in comments.
enum EIDEAssemblyRename
{
	IAR_None,		//Only comments: the symbol has no label of its own (a parameter, a local...).
	IAR_Type,		//A class or enum: a part of a label ("_CFatVolume__Init", "_CBox__T_i__Fill"), a type in a
					//parameter list or a template argument ("PC17CFatInfoSectorCache", its length and the lengths of
					//the names it is in with it), and a generated layout constant
					//("SIZEOF_CFatVolume", "CFatVolume_muiID": BaseLib's FatW65C816.inc).
	IAR_Member,		//A member function or static member: "_Scope__Name" ending there or before "__".
	IAR_Function,	//A function or variable outside a class: "_Scope__Name" or "_Name", the same.
	IAR_Field,		//A data member: only a layout constant, its class and it ("CFatVolume_muiID").
	IAR_Namespace,	//A namespace: a part of a label ("_NGear__Spin") and of a type's name in a parameter list
					//("C12NGear__CGear", its length with it), as a class's.
};


//A rename as labels have it: szScope is the symbol's scopes as a label writes them ("NS__CFatVolume"), empty for none.
struct SIDEAssemblyRename
{
	EIDEAssemblyRename	eKind;
	CChars				szOld;
	CChars				szNew;
	CChars				szScope;
	CChars				szClass;		//szScope's last part: a field's layout constants start with it.
	CChars				szKey;			//An overloaded function's key, as the index has it ("CFat::Open(uint8)").
	CArrayChars			aszOverloads;	//With every overload's key (its own too): a label is renamed if it fits it alone.
	size				uiUnsure;		//Labels that fit it and other overloads too: left, for the status line to say.

	void	Init(EIDEAssemblyRename eKind, const char* szOldName, const char* szNewName, const char* szScopeKey);
	void	Kill(void);
};


//An edit of assembly text: the old text from uiStart, uiLength long, replaced by szReplacement.
struct SIDEAssemblyEdit
{
	size	uiStart;
	size	uiLength;
	CChars	szReplacement;
	bool	bComment;
	bool	bLength;	//A name's length before it in a parameter list: not a place of its own.
};


typedef CArrayTemplate<SIDEAssemblyEdit>	CArrayIDEAssemblyEdits;


void	FindIDEAssemblyEdits(const char* szText, size uiLength, SIDEAssemblyRename* psRename, bool bComments, CArrayIDEAssemblyEdits* pasDest);
void	KillIDEAssemblyEdits(CArrayIDEAssemblyEdits* pasEdits);
bool	IsIDEAssemblyFile(const char* szFileName);
bool	IsIDELayoutPrefix(const char* szToken, size uiLength, const char* szName, size uiNameLength);


#endif // __IDE_RENAME_ASSEMBLY_H__
