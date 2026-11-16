/** ---------------- COPYRIGHT NOTICE, DISCLAIMER, and LICENSE ------------- **

Copyright (c) 2026 Andrew Paterson

This file is part of The Codaphela Project: Codaphela TemplateLib

Codaphela TemplateLib is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Codaphela TemplateLib is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Codaphela TemplateLib.  If not, see <http://www.gnu.org/licenses/>.

** ------------------------------------------------------------------------ **/
#ifndef __TEMPLATE_STRINGS_H__
#define __TEMPLATE_STRINGS_H__
#include "BaseLib/Chars.h"


//What a template's own string methods do, for whoever else needs the same rules (the IDE's include guards).

//"IDEMoveClass" is "IDE_MOVE_CLASS", "CKeg" is "C_KEG": an underscore before a capital after a small letter or a
//digit, and before the last of a run of capitals that a small letter follows; letters in capitals; anything that
//can't be in a name is an underscore.
char* AppendTemplateUpperSnakeCase(const char* szText, CChars* pszDest);

//The same in small letters: "ide_move_class".
char* AppendTemplateLowerSnakeCase(const char* szText, CChars* pszDest);

//Without the prefix the house style gives a class, struct or enum: "CKeg" is "Keg", "SItem" is "Item", "EKind" is
//"Kind"; a name without one ("Kernal", "IDEMoveClass") is as it is.
char* AppendTemplateWithoutPrefix(const char* szText, CChars* pszDest);


#endif // __TEMPLATE_STRINGS_H__
