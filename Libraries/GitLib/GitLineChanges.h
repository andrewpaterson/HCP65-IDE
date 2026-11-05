/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela GitLib

Codaphela GitLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela GitLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with Codaphela GitLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __GIT_LINE_CHANGES_H__
#define __GIT_LINE_CHANGES_H__
#include "BaseLib/ArrayTemplate.h"
#include "StandardLib/SimpleTextDiff.h"
#include "StandardLib/TextDiffProducer.h"


enum EGitLineChange
{
	GLC_Added,
	GLC_Modified,
	GLC_Removed,
};


//Lines of a file's text that differ from its last commit.  Lines are numbered from 0 in the text as it is now; removed
//lines were above line uiStartLine, and uiStartLine equals uiEndLine.  The base lines are those in the last commit.
struct SGitLineChange
{
	EGitLineChange	eType;
	size			uiStartLine;
	size			uiEndLine;
	size			uiBaseStartLine;
	size			uiBaseEndLine;
};


typedef CArrayTemplate<SGitLineChange>	CArrayGitLineChange;


//The changes IntelliJ IDEA marks in an editor's gutter: a file's text as it is now, edited or not, compared line by line
//with its text in the last commit (CGitRepository::ReadHeadText).  The comparison is CTextDiffProducer's, lines only,
//whitespace counted.  Lines end at line feeds, carriage return and line feed pairs, or carriage returns, as
//CTextContent splits them; the last line needn't end.
class CGitLineChanges
{
protected:
	CArrayGitLineChange	masChanges;
	CSimpleTextDiff		mcDiff;
	CTextDiffProducer	mcProducer;

public:
	void			Init(void);
	void			Kill(void);

	void			Compare(const char* szBase, size uiBaseLength, const char* szText, size uiTextLength);
	void			Clear(void);

	size			NumChanges(void);
	SGitLineChange*	GetChange(size uiIndex);
};


#endif // __GIT_LINE_CHANGES_H__

