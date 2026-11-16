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
#ifndef __IDE_MOVE_MEMBERS_H__
#define __IDE_MOVE_MEMBERS_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/MenuContainer.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"
#include "IDEChangeSignature.h"
#include "IDEDefinitions.h"


class CIDEContent;
struct SIDECaretContext;


#define IDE_ACCESS_PUBLIC		0
#define IDE_ACCESS_PROTECTED	1
#define IDE_ACCESS_PRIVATE		2


//A class a member is moved from or to: where it is defined.
struct SIDEMoveClass
{
	CChars			szKey;
	CChars			szFileName;
	SCPPSyntaxRange	sRange;		//Its class specifier, from class to its closing brace.
	bool			bTemplate;
	bool			bStruct;	//Its members are public until it says otherwise.
	bool			bChosen;	//Push Down: the member goes to it.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplate<SIDEMoveClass>	CArrayIDEMoveClasses;


//A member of the class: a function or a field (a static one too), as the class declares it and, where it is defined
//outside the class, as that is written.
struct SIDEMovedMember
{
	CChars				szKey;
	CChars				szName;			//As it is shown and asked for: "Fill(int, int)", "miLevel".
	CChars				szOwnName;		//"Fill".
	bool				bFunction;
	bool				bStatic;
	int32				iAccess;		//IDE_ACCESS_.
	SCPPSyntaxRange		sDeclared;		//In the class: all of its declaration (a function's body too, if it is there).
	SIDESignatureSite	sDeclaration;	//A function's, in the class.
	bool				bOutside;		//It is defined outside the class:
	CChars				szOutsideFile;
	SCPPSyntaxRange		sOutside;		//all of that definition,
	SCPPSyntaxRange		sOutsideName;	//and its name there, "CFoo::Fill".
	SIDESignatureSite	sDefinition;	//A function's, outside the class.
	CChars				szWhyNot;		//Why it can't be moved, empty if it can.
	bool				bChosen;		//It is to be moved.
	bool				bAbstract;		//Pull Up: only its declaration goes up, pure.  Push Down: it stays declared, pure.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplatePtr<SIDEMovedMember>	CArrayIDEMovedMembers;


//What Pull Members Up or Push Members Down found for the class at the caret.
struct SIDEMembersMove
{
	bool					bUp;
	bool					bMovable;
	CChars					szWhyNot;
	SIDEMoveClass			sClass;
	CArrayIDEMoveClasses	asTargets;	//Up: its base classes, nearest first.  Down: the classes that derive from it.
	size					uiTarget;	//Up: the one the members go to.
	CArrayIDEMovedMembers	apsMembers;	//In the order the class declares them.

	void	Init(void);
	void	Kill(void);
};


//IntelliJ's Pull Members Up and Push Members Down (Refactorings.txt, 3), for the class the caret is in (on one of
//its members, which is the one offered, or on its name).
//
//Pull Members Up moves members to one of the class's base classes: each one's declaration, with the comment over it,
//goes to the end of the part of the base class with its access (public, protected, private: the part is made if the
//class has none), and out of the class.  A function defined outside the class has that definition renamed to the
//base class's and moved to the file the base class's functions are defined in.  A function can go up abstract
//instead: the base class gets it pure virtual and the class keeps it.
//
//Push Members Down moves members to the classes that derive from the class (all of them, or those chosen): each gets
//the declaration, and a copy of a definition outside the class under its own name, unless it has that function
//already (it overrides it).  A function can be kept abstract: the class keeps it pure virtual.
//
//Where the class or the one a member goes to is a class template, a function defined outside goes with its body in
//the class, which is right whatever the template's parameters are called.
//
//What stands in the way is found first and shown (Problems Detected): a member that uses one of the class's that
//isn't going with it; one the class to has already; a private one the class's others still use; pushed down, one
//that is used from outside the classes it goes to (a use through an object of one of those classes, x.Seal() where
//x is a CDrum, is no use of the class's: the object's type is asked of a kept check of the file).
class CIDEMoveMembers : public CObject
{
friend class CIDEExtractClass;
friend class CIDETemplateInterface;
friend class CIDEEncapsulateField;
friend class CIDEIntroduceVariable;
CONSTRUCTABLE(CIDEMoveMembers);
DESTRUCTABLE(CIDEMoveMembers);
protected:
	CIDEContent*						mpcContent;  //Not held: the content holds it.
	Ptr<CDialog>						mpDialog;  //While it is shown.
	CArrayTemplatePtr<CIDERefactorUnit>	mapcUnits;  //The files parsed for the move being found.
	CArrayTemplatePtr<CIDERefactorUnit>	mapcChecked;  //Those parsed and checked, for what an expression's type is.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	void	AddContextActions(SIDECaretContext* psContext);
	bool	CanMove(void);
	void	QueuePullUp(void);
	void	QueuePushDown(void);
	bool	ShowMove(bool bUp);
	bool	FindMove(bool bUp, SIDEMembersMove* psDest);
	bool	ParseSpec(SIDEMembersMove* psMove, const char* szSpec, CChars* pszProblem);
	void	FindConflicts(SIDEMembersMove* psMove, CArrayChars* paszDest);
	bool	Move(SIDEMembersMove* psMove, CChars* pszStatus = NULL);
	char*	PrintMove(SIDEMembersMove* psMove, CChars* pszDest);
	bool	AddMember(const char* szClassKey, int32 iAccess, const char* szDeclaration, CIDEFileEdits* pcEdits, bool bLastPart = false);
	int32	FindMemberAccess(const char* szMemberKey);
	bool	MarkOverride(const char* szFunctionKey, CIDEFileEdits* pcEdits);
	bool	IsDeclaredVirtual(SIDEMovedMember* psMember, const char* szText);
	void	MarkOverriders(SIDEMovedMember* psMember, const char* szBaseKey, const char* szSkipClassKey, CIDEFileEdits* pcEdits);

protected:
	void				ShowQueuedPullUp(void);
	void				ShowQueuedPushDown(void);
	bool				ShowDialog(SIDEMembersMove* psMove);
	CIDERefactorUnit*	GetUnit(const char* szFileName);
	void				KillUnits(void);
	CIDERefactorUnit*	GetCheckedUnit(const char* szFileName);
	bool				IsUsedThrough(SIDELocation* psLocation, CArrayChars* paszClasses);
	bool				ReadClass(const char* szKey, SIDEMoveClass* psDest);
	void				ReadMembers(SIDEMembersMove* psMove, const char* szCaretKey);
	bool				ReadMember(SIDEMembersMove* psMove, const char* szKey, SIDEMovedMember* psDest);
	bool				ScanClass(SIDEMoveClass* psClass, int32 iAtToken, int32 iAccess, int32* piAccessAt, int32* piBoundaryToken, int32* piCloseToken);
	void				AddForwardDeclarations(SIDEMoveClass* psClass, const char* szText, const char* szDeclaration, CIDEFileEdits* pcEdits);
	bool				FindLastFieldPlace(SIDEMoveClass* psClass, const char* szText, int32 iAccess, size* puiAt);
	bool				FindInsertPlace(SIDEMoveClass* psClass, const char* szText, int32 iAccess, size* puiAt, CChars* pszBefore, bool bLastPart = false);
	void				FindSourceFile(SIDEMoveClass* psClass, CChars* pszDest);
	bool				IsChosen(SIDEMembersMove* psMove, const char* szKey, bool bAbstractToo);
	void				AppendMemberKeyIn(SIDEMovedMember* psMember, const char* szClassKey, CChars* pszDest);
	void				AppendDeclarationText(SIDEMembersMove* psMove, SIDEMovedMember* psMember, SIDEMoveClass* psTo, CIDEFileEdits* pcEdits, CChars* pszDest);
	void				AppendPureText(SIDEMovedMember* psMember, const char* szText, CChars* pszDest);
	void				AppendOutsideText(SIDEMovedMember* psMember, SIDEMoveClass* psTo, const char* szText, CChars* pszDest);
	void				ShowMoveStatus(CChars* pszMessage, CChars* pszStatus);
};


size FindIDECommentStart(const char* szText, size uiAt);
size FindIDENameColumn(const char* szText, size uiAt);
void RealignIDEDeclaration(CChars* pszLines, size uiNameColumn);
void RetypeIDEDeclaration(CChars* pszLines, const char* szText, size uiAt);


#endif // __IDE_MOVE_MEMBERS_H__

