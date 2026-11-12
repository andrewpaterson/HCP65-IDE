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
#ifndef __IDE_ICONS_H__
#define __IDE_ICONS_H__
#include "SupportLib/ImageCel.h"
#include "SupportLib/ColourFormat.h"
#include "SupportLib/ImageDraw.h"
#include "SupportLib/VectorPath.h"
#include "WindowLib/WindowTheme.h"


enum EIDEFileType
{
	IFT_CPP,		//.cpp .cc .cxx
	IFT_C,			//.c
	IFT_Header,		//.h .hpp .hxx .inl
	IFT_Other,

	IFT_Count
};


//The kinds of C++ symbol Go to Class and Go to Symbol show, each its letter in a coloured circle as CLion's.
enum EIDESymbolType
{
	IST_Class,		//C
	IST_Enum,		//E
	IST_Typedef,	//T
	IST_Namespace,	//N
	IST_Function,	//f
	IST_Variable,	//v, a variable, field or enumerator.

	IST_Count
};


//A tool window's toolbar's icons (CIDEResultsPane's), each a few lines.
enum EIDEToolIcon
{
	ITI_Rerun,			//An arrow round a circle.
	ITI_Previous,		//An arrow up.
	ITI_Next,			//An arrow down.
	ITI_ExpandAll,		//Arrow heads up and down, apart.
	ITI_CollapseAll,	//Arrow heads down and up, together.
	ITI_GroupByType,	//A bracket and a T.
	ITI_GroupByProject,	//A bracket and a P.
	ITI_GroupByClass,	//A bracket and a C.
	ITI_Preview,		//A pane with lines of text in its right half.
	ITI_SingleClick,	//A pointer.
	ITI_ReadAccess,		//An R.
	ITI_WriteAccess,	//A W.
	ITI_Pin,			//A pin.
	ITI_Settings,		//A cog.
	ITI_Count
};


//The IDE's 16 x 16 icons of the kinds of file: an outlined page, as the Jewel icons are, with a coloured badge on
//its bottom left that says what the file is: ++ for C++, C for C and h for a header.  And a tool window's toolbar's
//icons, drawn in one colour (the theme's icon colour, or its dim text's for a button that can't be used).
class CIDEIcons
{
public:
	static EIDEFileType		GetFileType(const char* szFileName);
	static Ptr<CImageCel>	CreateFileIcon(EIDEFileType eType, Ptr<CWindowTheme> pTheme, CColourFormatHelper* pcFormat);
	static Ptr<CImageCel>	CreateSymbolIcon(EIDESymbolType eType, Ptr<CWindowTheme> pTheme, CColourFormatHelper* pcFormat);
	static Ptr<CImageCel>	CreateToolIcon(EIDEToolIcon eIcon, ARGB32 sColour, CColourFormatHelper* pcFormat);

protected:
	static void				Fill(CImageDraw* pcDraw, CVectorPath* pcPath, ARGB32 sColour);
	static void				AddLine(CVectorPath* pcPath, float64 x1, float64 y1, float64 x2, float64 y2);
};


#endif // __IDE_ICONS_H__

