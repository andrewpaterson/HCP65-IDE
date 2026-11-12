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
#include <math.h>
#include "BaseLib/Chars.h"
#include "StandardLib/Objects.h"
#include "SupportLib/ImageColour.h"
#include "SupportLib/ImageColourConverter.h"
#include "SupportLib/VectorShapes.h"
#include "IDEIcons.h"


//The badges' colours: C++'s logo blue, C's slate and a header purple, as Visual Studio's are.
#define IDE_ICON_CPP_COLOUR		Set32BitColour((uint8)0x55, (uint8)0x8C, (uint8)0xD6)
#define IDE_ICON_C_COLOUR		Set32BitColour((uint8)0x6B, (uint8)0x8F, (uint8)0xB5)
#define IDE_ICON_HEADER_COLOUR	Set32BitColour((uint8)0xA0, (uint8)0x74, (uint8)0xC4)

//The symbols' circles, by EIDESymbolType: IntelliJ's class blue, enum green, typedef teal, namespace grey, function
//pink and variable purple.
static uint8 gauiIDESymbolColours[IST_Count][3] =
{
	{ 0x35, 0x92, 0xC4 },
	{ 0x4D, 0x9C, 0x5A },
	{ 0x3A, 0x9E, 0x9A },
	{ 0x7A, 0x7E, 0x85 },
	{ 0xC7, 0x5A, 0x8E },
	{ 0x96, 0x6C, 0xC2 },
};


//////////////////////////////////////////////////////////////////////////
//
// The kind of file by its extension, whatever its case.
//
//////////////////////////////////////////////////////////////////////////
EIDEFileType CIDEIcons::GetFileType(const char* szFileName)
{
	CChars			szName;
	EIDEFileType	eType;

	szName.Fake(szFileName);
	if (szName.EndsWithIgnoreCase(".cpp") || szName.EndsWithIgnoreCase(".cc") || szName.EndsWithIgnoreCase(".cxx"))
	{
		eType = IFT_CPP;
	}
	else if (szName.EndsWithIgnoreCase(".c"))
	{
		eType = IFT_C;
	}
	else if (szName.EndsWithIgnoreCase(".h") || szName.EndsWithIgnoreCase(".hpp") || szName.EndsWithIgnoreCase(".hxx") || szName.EndsWithIgnoreCase(".inl"))
	{
		eType = IFT_Header;
	}
	else
	{
		eType = IFT_Other;
	}
	return eType;
}


//////////////////////////////////////////////////////////////////////////
//
// A page with its top right corner folded, and for source files a
// rounded badge over its bottom left with the file's mark in white.
// Points on pixel centres keep the strokes sharp.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CImageCel> CIDEIcons::CreateFileIcon(EIDEFileType eType, Ptr<CWindowTheme> pTheme, CColourFormatHelper* pcFormat)
{
	Ptr<CImage>		pImage;
	CImageDraw		cDraw;
	CVectorPath		cPath;
	SFloat64Vec2	asPage[5];
	SFloat64Vec2	asBadge[8];
	ARGB32			sBadge;
	size			i;

	pImage = OMalloc<CImage>(16, 16, PT_uint8, IMAGE_DIFFUSE_RED, IMAGE_DIFFUSE_GREEN, IMAGE_DIFFUSE_BLUE, IMAGE_OPACITY, CHANNEL_STOP);
	pImage->Black();
	cDraw.Init(&pImage);

	//The page, its fold and, for a file that isn't source, two lines of text.
	asPage[0].Init(3.5, 1.5);
	asPage[1].Init(9.5, 1.5);
	asPage[2].Init(12.5, 4.5);
	asPage[3].Init(12.5, 14.5);
	asPage[4].Init(3.5, 14.5);
	cPath.Init();
	cPath.BeginPart(true);
	for (i = 0; i < 5; i++)
	{
		AddVectorStroke(&cPath, &asPage[i], &asPage[(i + 1) % 5], 0.5);
	}
	AddLine(&cPath, 9.5, 1.5, 9.5, 4.5);
	AddLine(&cPath, 9.5, 4.5, 12.5, 4.5);
	if (eType == IFT_Other)
	{
		AddLine(&cPath, 5.5, 8.5, 10.5, 8.5);
		AddLine(&cPath, 5.5, 11.5, 10.5, 11.5);
	}
	Fill(&cDraw, &cPath, pTheme->GetColour(TC_Icon));
	cPath.Kill();

	if (eType != IFT_Other)
	{
		sBadge = (eType == IFT_CPP) ? IDE_ICON_CPP_COLOUR : (eType == IFT_C) ? IDE_ICON_C_COLOUR : IDE_ICON_HEADER_COLOUR;

		//A rounded rectangle from (0, 7) to (12, 16), its corners cut one pixel.
		asBadge[0].Init(1.0, 7.0);
		asBadge[1].Init(11.0, 7.0);
		asBadge[2].Init(12.0, 8.0);
		asBadge[3].Init(12.0, 15.0);
		asBadge[4].Init(11.0, 16.0);
		asBadge[5].Init(1.0, 16.0);
		asBadge[6].Init(0.0, 15.0);
		asBadge[7].Init(0.0, 8.0);
		cPath.Init();
		cPath.BeginPart(true);
		cPath.AddPolygon(asBadge, 8, false);
		Fill(&cDraw, &cPath, sBadge);
		cPath.Kill();

		cPath.Init();
		cPath.BeginPart(true);
		if (eType == IFT_CPP)
		{
			//++
			AddLine(&cPath, 1.5, 11.5, 5.5, 11.5);
			AddLine(&cPath, 3.5, 9.5, 3.5, 13.5);
			AddLine(&cPath, 6.5, 11.5, 10.5, 11.5);
			AddLine(&cPath, 8.5, 9.5, 8.5, 13.5);
		}
		else if (eType == IFT_C)
		{
			//C
			AddLine(&cPath, 8.5, 9.5, 5.5, 9.5);
			AddLine(&cPath, 5.5, 9.5, 4.0, 11.0);
			AddLine(&cPath, 4.0, 11.0, 4.0, 12.0);
			AddLine(&cPath, 4.0, 12.0, 5.5, 13.5);
			AddLine(&cPath, 5.5, 13.5, 8.5, 13.5);
		}
		else
		{
			//h
			AddLine(&cPath, 4.5, 8.5, 4.5, 14.5);
			AddLine(&cPath, 4.5, 11.0, 6.0, 10.5);
			AddLine(&cPath, 6.0, 10.5, 7.0, 10.5);
			AddLine(&cPath, 7.0, 10.5, 7.5, 11.5);
			AddLine(&cPath, 7.5, 11.5, 7.5, 14.5);
		}
		Fill(&cDraw, &cPath, pTheme->GetColour(TC_BrightText));
		cPath.Kill();
	}
	cDraw.Kill();

	pImage = CImageColourConverter::ConvertColour(pImage, pcFormat->meOrder);
	return OMalloc<CImageCel>(pImage);
}


//The tool icons' lines, by EIDEToolIcon: each line is x1, y1, x2, y2 in the 16 x 16 icon.
static const float64 gafRerun[] =
{
	11.21, 4.17, 12.53, 5.89,
	12.53, 5.89, 13.0, 8.0,
	13.0, 8.0, 12.53, 10.11,
	12.53, 10.11, 11.21, 11.83,
	11.21, 11.83, 9.29, 12.83,
	9.29, 12.83, 7.13, 12.92,
	7.13, 12.92, 5.13, 12.1,
	5.13, 12.1, 3.67, 10.5,
	3.67, 10.5, 3.02, 8.44,
	3.02, 8.44, 3.3, 6.29,
	3.3, 6.29, 4.46, 4.46,
	4.46, 4.46, 6.29, 3.3,
	11.2, 1.2, 11.2, 4.2,
	11.2, 4.2, 8.2, 4.2,
};
static const float64 gafPrevious[] =
{
	8.0, 13.0, 8.0, 3.0,
	4.0, 7.0, 8.0, 3.0,
	8.0, 3.0, 12.0, 7.0,
};
static const float64 gafNext[] =
{
	8.0, 3.0, 8.0, 13.0,
	4.0, 9.0, 8.0, 13.0,
	8.0, 13.0, 12.0, 9.0,
};
static const float64 gafExpandAll[] =
{
	4.0, 6.0, 8.0, 2.5,
	8.0, 2.5, 12.0, 6.0,
	4.0, 10.0, 8.0, 13.5,
	8.0, 13.5, 12.0, 10.0,
};
static const float64 gafCollapseAll[] =
{
	4.0, 2.5, 8.0, 6.0,
	8.0, 6.0, 12.0, 2.5,
	4.0, 13.5, 8.0, 10.0,
	8.0, 10.0, 12.0, 13.5,
};
static const float64 gafGroupByType[] =
{
	4.5, 3.0, 2.5, 3.0,
	2.5, 3.0, 2.5, 13.0,
	2.5, 13.0, 4.5, 13.0,
	7.0, 4.5, 13.0, 4.5,
	10.0, 4.5, 10.0, 11.5,
};
static const float64 gafGroupByProject[] =
{
	4.5, 3.0, 2.5, 3.0,
	2.5, 3.0, 2.5, 13.0,
	2.5, 13.0, 4.5, 13.0,
	7.5, 11.5, 7.5, 4.5,
	7.5, 4.5, 11.5, 4.5,
	11.5, 4.5, 12.5, 5.5,
	12.5, 5.5, 12.5, 7.0,
	12.5, 7.0, 11.5, 8.0,
	11.5, 8.0, 7.5, 8.0,
};
static const float64 gafGroupByClass[] =
{
	4.5, 3.0, 2.5, 3.0,
	2.5, 3.0, 2.5, 13.0,
	2.5, 13.0, 4.5, 13.0,
	12.5, 5.5, 11.5, 4.5,
	11.5, 4.5, 8.5, 4.5,
	8.5, 4.5, 7.5, 5.5,
	7.5, 5.5, 7.5, 10.5,
	7.5, 10.5, 8.5, 11.5,
	8.5, 11.5, 11.5, 11.5,
	11.5, 11.5, 12.5, 10.5,
};
static const float64 gafPreview[] =
{
	2.5, 3.5, 13.5, 3.5,
	13.5, 3.5, 13.5, 12.5,
	13.5, 12.5, 2.5, 12.5,
	2.5, 12.5, 2.5, 3.5,
	7.0, 3.5, 7.0, 12.5,
	9.0, 6.0, 11.5, 6.0,
	9.0, 8.0, 11.5, 8.0,
	9.0, 10.0, 11.5, 10.0,
};
static const float64 gafSingleClick[] =
{
	4.5, 2.5, 4.5, 12.5,
	4.5, 12.5, 7.0, 10.0,
	7.0, 10.0, 9.0, 13.5,
	4.5, 2.5, 11.5, 9.5,
	11.5, 9.5, 7.5, 9.8,
};
static const float64 gafReadAccess[] =
{
	5.5, 12.5, 5.5, 3.5,
	5.5, 3.5, 9.5, 3.5,
	9.5, 3.5, 10.5, 4.5,
	10.5, 4.5, 10.5, 7.0,
	10.5, 7.0, 9.5, 8.0,
	9.5, 8.0, 5.5, 8.0,
	8.0, 8.0, 10.5, 12.5,
};
static const float64 gafWriteAccess[] =
{
	2.5, 3.5, 5.0, 12.5,
	5.0, 12.5, 8.0, 6.5,
	8.0, 6.5, 11.0, 12.5,
	11.0, 12.5, 13.5, 3.5,
};
static const float64 gafPin[] =
{
	5.0, 2.5, 11.0, 2.5,
	6.0, 2.5, 6.0, 8.0,
	10.0, 2.5, 10.0, 8.0,
	3.5, 8.0, 12.5, 8.0,
	8.0, 8.0, 8.0, 13.5,
};
static const float64 gafSettings[] =
{
	11.0, 8.0, 10.43, 9.76,
	10.43, 9.76, 8.93, 10.85,
	8.93, 10.85, 7.07, 10.85,
	7.07, 10.85, 5.57, 9.76,
	5.57, 9.76, 5.0, 8.0,
	5.0, 8.0, 5.57, 6.24,
	5.57, 6.24, 7.07, 5.15,
	7.07, 5.15, 8.93, 5.15,
	8.93, 5.15, 10.43, 6.24,
	10.43, 6.24, 11.0, 8.0,
	8.0, 2.0, 8.0, 3.5,
	8.0, 12.5, 8.0, 14.0,
	2.0, 8.0, 3.5, 8.0,
	12.5, 8.0, 14.0, 8.0,
	3.8, 3.8, 4.9, 4.9,
	11.1, 11.1, 12.2, 12.2,
	3.8, 12.2, 4.9, 11.1,
	11.1, 4.9, 12.2, 3.8,
};


struct SIDEToolIconLines
{
	const float64*	pafLines;
	size			uiNumLines;
};


#define IDE_TOOL_ICON(name)	{ gaf##name, sizeof(gaf##name) / (4 * sizeof(float64)) }


static const SIDEToolIconLines	gasIDEToolIcons[] =
{
	IDE_TOOL_ICON(Rerun),
	IDE_TOOL_ICON(Previous),
	IDE_TOOL_ICON(Next),
	IDE_TOOL_ICON(ExpandAll),
	IDE_TOOL_ICON(CollapseAll),
	IDE_TOOL_ICON(GroupByType),
	IDE_TOOL_ICON(GroupByProject),
	IDE_TOOL_ICON(GroupByClass),
	IDE_TOOL_ICON(Preview),
	IDE_TOOL_ICON(SingleClick),
	IDE_TOOL_ICON(ReadAccess),
	IDE_TOOL_ICON(WriteAccess),
	IDE_TOOL_ICON(Pin),
	IDE_TOOL_ICON(Settings),
};
static_assert(sizeof(gasIDEToolIcons) / sizeof(SIDEToolIconLines) == ITI_Count, "Each EIDEToolIcon needs its lines in gasIDEToolIcons.");


//////////////////////////////////////////////////////////////////////////
//
// A tool window's toolbar's icon, its lines in sColour.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CImageCel> CIDEIcons::CreateToolIcon(EIDEToolIcon eIcon, ARGB32 sColour, CColourFormatHelper* pcFormat)
{
	Ptr<CImage>		pImage;
	CImageDraw		cDraw;
	CVectorPath		cPath;
	const float64*	pafLine;
	size			i;

	pImage = OMalloc<CImage>(16, 16, PT_uint8, IMAGE_DIFFUSE_RED, IMAGE_DIFFUSE_GREEN, IMAGE_DIFFUSE_BLUE, IMAGE_OPACITY, CHANNEL_STOP);
	pImage->Black();
	cDraw.Init(&pImage);
	cPath.Init();
	cPath.BeginPart(true);
	for (i = 0; i < gasIDEToolIcons[eIcon].uiNumLines; i++)
	{
		pafLine = &gasIDEToolIcons[eIcon].pafLines[i * 4];
		AddLine(&cPath, pafLine[0], pafLine[1], pafLine[2], pafLine[3]);
	}
	Fill(&cDraw, &cPath, sColour);
	cPath.Kill();
	cDraw.Kill();

	pImage = CImageColourConverter::ConvertColour(pImage, pcFormat->meOrder);
	return OMalloc<CImageCel>(pImage);
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CIDEIcons::Fill(CImageDraw* pcDraw, CVectorPath* pcPath, ARGB32 sColour)
{

	pcDraw->SetColour(sColour);
	pcDraw->DrawPath(pcPath);
}


//////////////////////////////////////////////////////////////////////////
//
// A one pixel wide stroke.
//
//////////////////////////////////////////////////////////////////////////
void CIDEIcons::AddLine(CVectorPath* pcPath, float64 x1, float64 y1, float64 x2, float64 y2)
{
	SFloat64Vec2	sStart;
	SFloat64Vec2	sEnd;

	sStart.Init(x1, y1);
	sEnd.Init(x2, y2);
	AddVectorStroke(pcPath, &sStart, &sEnd, 0.5);
}


//////////////////////////////////////////////////////////////////////////
//
// A circle of the kind's colour with its letter in white.
//
//////////////////////////////////////////////////////////////////////////
Ptr<CImageCel> CIDEIcons::CreateSymbolIcon(EIDESymbolType eType, Ptr<CWindowTheme> pTheme, CColourFormatHelper* pcFormat)
{
	Ptr<CImage>		pImage;
	CImageDraw		cDraw;
	CVectorPath		cPath;
	SFloat64Vec2	asCircle[24];
	size			i;
	float64			fAngle;

	pImage = OMalloc<CImage>(16, 16, PT_uint8, IMAGE_DIFFUSE_RED, IMAGE_DIFFUSE_GREEN, IMAGE_DIFFUSE_BLUE, IMAGE_OPACITY, CHANNEL_STOP);
	pImage->Black();
	cDraw.Init(&pImage);

	for (i = 0; i < 24; i++)
	{
		fAngle = (float64)i * 2.0 * 3.14159265358979 / 24.0;
		asCircle[i].Init(8.0 + 7.0 * cos(fAngle), 8.0 + 7.0 * sin(fAngle));
	}
	cPath.Init();
	cPath.BeginPart(true);
	cPath.AddPolygon(asCircle, 24, false);
	Fill(&cDraw, &cPath, Set32BitColour(gauiIDESymbolColours[eType][0], gauiIDESymbolColours[eType][1], gauiIDESymbolColours[eType][2]));
	cPath.Kill();

	cPath.Init();
	cPath.BeginPart(true);
	switch (eType)
	{
	case IST_Class:
		AddLine(&cPath, 10.5, 5.5, 6.5, 5.5);
		AddLine(&cPath, 6.5, 5.5, 5.5, 6.5);
		AddLine(&cPath, 5.5, 6.5, 5.5, 9.5);
		AddLine(&cPath, 5.5, 9.5, 6.5, 10.5);
		AddLine(&cPath, 6.5, 10.5, 10.5, 10.5);
		break;
	case IST_Enum:
		AddLine(&cPath, 10.5, 5.5, 5.5, 5.5);
		AddLine(&cPath, 5.5, 5.5, 5.5, 10.5);
		AddLine(&cPath, 5.5, 10.5, 10.5, 10.5);
		AddLine(&cPath, 5.5, 8.0, 9.5, 8.0);
		break;
	case IST_Typedef:
		AddLine(&cPath, 5.0, 5.5, 11.0, 5.5);
		AddLine(&cPath, 8.0, 5.5, 8.0, 11.0);
		break;
	case IST_Namespace:
		AddLine(&cPath, 5.5, 11.0, 5.5, 5.0);
		AddLine(&cPath, 5.5, 5.0, 10.5, 11.0);
		AddLine(&cPath, 10.5, 11.0, 10.5, 5.0);
		break;
	case IST_Function:
		AddLine(&cPath, 10.0, 5.0, 8.5, 5.0);
		AddLine(&cPath, 8.5, 5.0, 7.5, 6.0);
		AddLine(&cPath, 7.5, 6.0, 7.5, 11.0);
		AddLine(&cPath, 6.0, 7.5, 10.0, 7.5);
		break;
	default:
		AddLine(&cPath, 5.5, 6.5, 8.0, 10.5);
		AddLine(&cPath, 8.0, 10.5, 10.5, 6.5);
		break;
	}
	Fill(&cDraw, &cPath, pTheme->GetColour(TC_BrightText));
	cPath.Kill();
	cDraw.Kill();

	pImage = CImageColourConverter::ConvertColour(pImage, pcFormat->meOrder);
	return OMalloc<CImageCel>(pImage);
}
