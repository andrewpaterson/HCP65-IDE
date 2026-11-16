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
#ifndef __IDE_CONVERT_POINTER_H__
#define __IDE_CONVERT_POINTER_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplate.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEDefinitions.h"


class CIDEContent;
struct SIDECaretContext;


typedef CArrayTemplatePtr<CIDERefactorUnit>	CArrayIDEPointerUnits;


//A pointer made a reference, or a reference a pointer (Refactorings.txt, 17), for the parameter or the local
//variable the caret is on (its declaration or any use):
//  - its declaration's * becomes & (or its & a *), in every declaration and definition of a parameter's function
//    and of the functions that override it or that it overrides (CIDEChangeSignature::FindSignature finds them);
//  - where it is used, "p->x" becomes "p.x" and "*p" becomes "p", and p alone (given on, assigned from) becomes
//    "&p"; the other way, "r.x" becomes "r->x", "&r" becomes "r" and r alone becomes "*r";
//  - every call's argument for it changes to match: "&c" becomes "c" and anything else "*p" (the other way, "*p"
//    becomes "p" and anything else "&c"); a local variable's initialiser changes the same way.
//Refused, to a reference: a pointer that is assigned to, stepped, compared, tested (a reference is never NULL and
//never changes what it refers to), used as an array, or deleted; one with a default value; a call that gives it
//NULL; a local variable with no initialiser.  Either way: a function that is named without being called (a pointer
//to it wouldn't fit), a use or an argument a macro makes.  The files changed are checked as they would be, and
//nothing is changed if they have more errors than they had.
class CIDEConvertPointer : public CObject
{
CONSTRUCTABLE(CIDEConvertPointer);
DESTRUCTABLE(CIDEConvertPointer);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	CArrayIDEPointerUnits	mapcUnits;  //The files parsed for one conversion.
	CArrayChars				maszPendingFiles;  //Arguments that are a name alone, to be given a * or an &: one that is the
	CArrayInt				maiPendingStarts;  //parameter itself (the function calls itself) is left as it is.
	CChars					mszProblem;

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
	void	QueueToReference(void);
	void	QueueToPointer(void);
	bool	Convert(bool bToReference, CChars* pszStatus = NULL);
	bool	ConvertUsesIn(const char* szKey, const char* szFileName, size uiStart, size uiEnd, bool bToReference, CIDEFileEdits* pcFiles, CChars* pszWhyNot);

protected:
	void				ToReferenceQueued(void);
	void				ToPointerQueued(void);
	CIDERefactorUnit*	GetUnit(const char* szFileName);
	void				KillUnits(void);
	bool				Refuse(const char* szWhyNot, const char* szFileName = NULL, int32 iLine = -1);
	bool				GetTokenOffsets(CIDERefactorUnit* pcUnit, const char* szText, int32 iFirstToken, int32 iLastToken, size* puiStart, size* puiEnd);
	bool				RemoveToken(CIDERefactorUnit* pcUnit, const char* szText, int32 iToken, CIDETextEdits* pcEdits);
	void				RemoveBrackets(CIDERefactorUnit* pcUnit, const char* szText, CCPPSyntax* pcNode, CIDETextEdits* pcEdits);
	bool				ConvertDeclarator(CIDERefactorUnit* pcUnit, const char* szFileName, CCPPSyntaxDeclarator* pcDeclarator, bool bToReference, CIDEFileEdits* pcFiles);
	bool				ConvertUse(const char* szFileName, int32 iLine, int32 iColumn, bool bToReference, CIDEFileEdits* pcFiles);
	bool				ConvertValue(CIDERefactorUnit* pcUnit, const char* szFileName, int32 iFirstToken, int32 iLastToken, bool bToReference, CIDEFileEdits* pcFiles);
	bool				ConvertParameter(CIDERefactorUnit* pcUnit, CCPPSyntax* pcParameter, int32 iLine, int32 iColumn, bool bToReference, CIDEFileEdits* pcFiles, CChars* pszWhat, size* puiUses, size* puiCalls);
	bool				ConvertLocal(CIDERefactorUnit* pcUnit, CCPPSyntax* pcDeclarator, const char* szKey, bool bToReference, CIDEFileEdits* pcFiles, CChars* pszWhat, size* puiUses);
	size				ConvertUses(const char* szKey, bool bToReference, CIDEFileEdits* pcFiles);
	bool				ConvertField(CIDERefactorUnit* pcUnit, CCPPSyntax* pcDeclarator, const char* szKey, bool bToReference, CIDEFileEdits* pcFiles, CChars* pszWhat, size* puiUses);
	void				FlushPending(bool bToReference, CIDEFileEdits* pcFiles);
};


#endif // __IDE_CONVERT_POINTER_H__
