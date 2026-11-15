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
#ifndef __IDE_CHANGE_SIGNATURE_H__
#define __IDE_CHANGE_SIGNATURE_H__
#include "StandardLib/Object.h"
#include "StandardLib/Pointer.h"
#include "BaseLib/ArrayTemplatePtr.h"
#include "WindowLib/Dialog.h"
#include "WindowLib/CommandSet.h"
#include "WindowLib/MenuContainer.h"
#include "CppCompilerLib/CPPHighlighter.h"
#include "IDECodeModel.h"
#include "IDERefactorUnit.h"


class CIDEContent;
class CIDEChangeSignatureDialog;


//A parameter as one declaration or definition of the function writes it.  Ranges are lines and columns from 0 in
//its file, ends exclusive, as the syntax tree gives them.
struct SIDEWrittenParameter
{
	SCPPSyntaxRange	sAll;		//All of it, its default value too.
	SCPPSyntaxRange	sName;		//Its name; empty (its end is its start) where it has none.
	SCPPSyntaxRange	sDefault;	//From its = to the end of its default value; empty if it has none.
	bool			bName;
	bool			bDefault;
	CChars			szKey;		//Its symbol's key in the index, empty if it has no name: its uses in the body are found by it.
};


typedef CArrayTemplate<SIDEWrittenParameter>	CArrayIDEWrittenParameters;
typedef CArrayTemplate<SCPPSyntaxRange>			CArrayIDESyntaxRanges;


//A declaration or a definition of the function, or of one changed with it (an override, a member of its family).
struct SIDESignatureSite
{
	CChars						szFileName;
	CChars						szKey;			//The symbol it declares.
	int32						iLine;			//Of the function's name in it, from 0.
	int32						iColumn;
	SCPPSyntaxRange				sParameters;	//From after its parameters' ( to its ).
	SCPPSyntaxRange				sReturn;		//What it returns, as it writes it: from its type to its name ("const char*	").
	bool						bReturn;		//It writes one (a constructor doesn't).
	bool						bDefinition;
	bool						bFirst;			//It is where its symbol is first declared: a new default value is written here.
	CChars						szLabel;		//Where it is first declared: the label hand written assembly names its symbol by, as it was.
	size						uiNewName;		//Where its name is in its file's text once the change is made.

	//The rest of what it writes, for making a declaration or a definition from another (CIDEDefinitions).
	SCPPSyntaxRange				sAll;			//All of it: the declaration with its ; or the definition, with a template<...> of its own.
	SCPPSyntaxRange				sName;			//Its name as it writes it: "Init", "CFoo::Init".
	SCPPSyntaxRange				sAfter;			//From its parameters' ) to the end of what follows them (const, override).
	SCPPSyntaxRange				sBody;			//A definition's body, from its { to its }.
	bool						bBody;
	bool						bInClass;		//It is written in its class.
	bool						bPure;			//= 0.
	CChars						szOwnName;		//Its own name: "Init", "operator==", "~CFoo".
	CChars						szQualifiers;	//What follows its parameters that a definition outside its class writes too: " const".
	CChars						szOwnTemplate;	//Its own template<...>, if it has one: a function template's, or a class template's member's outside it.
	CChars						szClassQualifier;	//In its class: what its name is written after outside it, "NS::CFoo::", "CBox<M, I>::".
	CChars						szClassTemplate;	//...and the template<...> a class template's member is written under there; empty if it isn't one's.
	CArrayIDEWrittenParameters	asParameters;

	void	Init(void);
	void	Kill(void);
};


//A call of the function (or of one changed with it).
struct SIDESignatureCall
{
	CChars					szFileName;
	int32					iLine;			//Of the function's name in it, from 0.
	int32					iColumn;
	SCPPSyntaxRange			sArguments;		//From after its arguments' ( to its ).
	CArrayIDESyntaxRanges	asArguments;
	CArrayInt				aiEffects;		//Whether each argument does something (a call, an assignment, ++, new): taking it out takes that out.
	bool					bValueUsed;		//What it returns is used: it isn't a statement by itself.

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplatePtr<SIDESignatureSite>	CArrayIDESignatureSites;
typedef CArrayTemplatePtr<SIDESignatureCall>	CArrayIDESignatureCalls;


//A parameter of the function as it is to be.
struct SIDENewParameter
{
	int32	iOld;			//Which parameter it was, from 0; -1 for a new one.
	CChars	szType;			//A new one's type, as it is to be written; an old one's new type, empty to keep each site's.
	CChars	szName;			//A new one's name; an old one's new name, empty to keep each site's.
	CChars	szDefault;		//A new one's default value, written where the function is first declared, empty for none; an
							//old one's, if bDefaultChanged.
	bool	bDefaultChanged;	//An old one's default value is to be szDefault (none, if that is empty).
	CChars	szCallValue;	//A new one's argument in the calls there already are; empty to leave it to the default.

	void	Init(int32 iOldIndex);
	void	Kill(void);
};


typedef CArrayTemplate<SIDENewParameter>	CArrayIDENewParameters;


//What a function's signature is to be changed to.
struct SIDESignatureChange
{
	CArrayIDENewParameters	asParameters;
	CChars					szName;			//Its new name, empty to keep it.
	CChars					szReturn;		//What it is to return, as it is to be written; empty to keep what each site writes.

	void	Init(void);
	void	Kill(void);
};


//What Change Signature found for the function at the caret: every place it is declared, defined and called, and
//where it is named without being called.
struct SIDESignature
{
	CChars					szKey;
	CChars					szName;			//With its scopes: "CFoo::Init".
	bool					bChangeable;
	CChars					szWhyNot;
	size					uiOverrides;	//The functions changed with it: what overrides it and what it overrides,
	size					uiFamily;		//and what a template's body calls with it by one name (Refactorings.txt, T2).
	CChars					szFamily;
	CArrayIDESignatureSites	apsSites;		//The first is the declaration the dialog shows.
	CArrayIDESignatureCalls	apsCalls;
	CArrayIDELocations		asOtherUses;	//Where it is named but not called: its address taken, a pointer to it.
	CArrayChars				aszOldTypes;	//The first declaration's parameters, for showing: each one's type,
	CArrayChars				aszOldNames;	//its name,
	CArrayChars				aszOldDefaults;	//and its default value.
	CChars					szOldReturn;	//What the first declaration says it returns, empty if it says nothing (a constructor).
	CChars					szOldName;		//Its name alone: "Init".
	CArrayChars				aszOverloads;	//The functions of its name in its scope, by their keys (its own too).

	void	Init(void);
	void	Kill(void);
};


typedef CArrayTemplatePtr<CIDERefactorUnit>	CArrayIDESignatureUnits;


//IntelliJ's Change Signature (Ctrl+F6; Refactorings.txt, 2): a function's parameters kept, removed, added, put in
//another order, renamed and given other types and default values, what it returns, and its name, in every declaration
//and definition of it, in the functions that override it and that it overrides, in the members a template's body
//calls with it by one name, and in every call.
//
//Each declaration and definition keeps what it wrote: a parameter kept is the text it has there (its type as that
//file names it, its own name unless it is renamed, its default value where it has one).  A new parameter is its type
//and name, with its default value in the first declaration.  What it returns is changed where each writes it, the
//white space before its name kept.  Its name is changed by Rename, after the rest.  A call's arguments are put in the new order; a
//removed one is taken out; a new one is the value given for it, or nothing if it has a default value and nothing
//follows it.  An argument a call left to a default value stays left to it where it still can be, and is written out
//where an argument now follows it.  A renamed parameter is renamed where each definition's body uses it.
//
//What stands in the way is found first (FindConflicts): a removed parameter a body still uses; the function named
//but not called (its address taken); an argument taken out that does something; a parameter with a default value
//before one without; a new parameter with no value for the calls there are; another function of the same name with
//the new parameters; a call whose value is used when it is to return nothing.
//
//Hand written assembly names a function by the label the compiler gives it, which has its parameters' types in it
//where it is overloaded or isn't a member.  Where the solution has assembly and builds for a processor, each changed
//function's label is asked of the compiler before and after (for that processor: an int is 2 bytes on one and 4 on the
//other), and the assembly's labels are changed from the one to the other.
class CIDEChangeSignature : public CObject
{
CONSTRUCTABLE(CIDEChangeSignature);
DESTRUCTABLE(CIDEChangeSignature);
protected:
	CIDEContent*			mpcContent;  //Not held: the content holds it.
	Ptr<CIDEChangeSignatureDialog>	mpSignatureDialog;  //While it is shown.
	Ptr<CDialog>			mpDialog;  //Problems Detected, while it is shown.
	CArrayIDESignatureUnits	mapcUnits;  //The files parsed for the signature being found.

public:
	void	Init(CIDEContent* pcContent);
	void	Class(void) override;
	void	Free(void) override;
	bool	Save(CObjectWriter* pcFile) override;
	bool	Load(CObjectReader* pcFile) override;

	void	AddCommands(Ptr<CCommandSet> pCommands);
	void	AddEditorItems(Ptr<CMenuContainer> pRefactor);
	bool	CanChangeSignature(void);
	void	QueueChangeSignature(void);
	bool	ShowChangeSignature(void);
	bool	IsUnchanged(SIDESignature* psSignature, SIDESignatureChange* psChange);
	bool	FindSignature(SIDESignature* psDest);
	bool	AppendCaretLabel(CChars* pszDest);
	bool	ParseChange(SIDESignature* psSignature, const char* szSpec, SIDESignatureChange* psDest, CChars* pszProblem);
	bool	FindProblem(SIDESignature* psSignature, SIDESignatureChange* psChange, CChars* pszProblem);
	void	FindConflicts(SIDESignature* psSignature, SIDESignatureChange* psChange, CArrayChars* paszDest);
	bool	Change(SIDESignature* psSignature, SIDESignatureChange* psChange, CChars* pszStatus = NULL);
	void	AppendNewSignature(SIDESignature* psSignature, SIDESignatureChange* psChange, CChars* pszDest);
	char*	PrintSignature(SIDESignature* psSignature, CChars* pszDest);

protected:
	void				ShowQueuedChangeSignature(void);
public:
	bool				ReadSite(CIDERefactorUnit* pcUnit, SIDELocation* psLocation, const char* szKey, SIDESignatureSite* psSite, const char** pszWhyNot);
	bool				ShowConflicts(CArrayChars* paszConflicts);
protected:
	CIDERefactorUnit*	GetUnit(const char* szFileName);
	void				KillUnits(void);
	bool				AddSite(SIDESignature* psSignature, SIDELocation* psLocation, const char* szKey, const char** pszWhyNot);
	bool				AddCall(SIDESignature* psSignature, SIDELocation* psLocation);
	void				FindOldParameters(SIDESignature* psSignature);
	void				AppendPlace(const char* szFileName, int32 iLine, CChars* pszDest);
	void				AppendSiteParameters(SIDESignature* psSignature, SIDESignatureSite* psSite, bool bFirst, CArrayIDENewParameters* pasNew, const char* szText, CChars* pszDest);
	void				AppendCallArguments(SIDESignature* psSignature, SIDESignatureCall* psCall, CArrayIDENewParameters* pasNew, const char* szText, CChars* pszDest);
	void				ShowChangeStatus(CChars* pszMessage, CChars* pszStatus);
	bool				GetAssemblyTarget(ECPPTarget* peTarget);
	void				FindLabels(SIDESignature* psSignature, ECPPTarget eTarget, bool bNew, CArrayChars* paszNew);
	size				ChangeLabels(SIDESignature* psSignature, CArrayChars* paszNew);
};


void KillIDENewParameters(CArrayIDENewParameters* pasParameters);
void AppendIDERangeText(const char* szText, SCPPSyntaxRange* psRange, CChars* pszDest);
void NormaliseIDEParameterType(const char* szType, CChars* pszDest);


#endif // __IDE_CHANGE_SIGNATURE_H__

