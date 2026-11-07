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
#include "WindowLib/JewelColours.h"
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
Ptr<CImageCel> CIDEIcons::CreateFileIcon(EIDEFileType eType, CColourFormatHelper* pcFormat)
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
	Fill(&cDraw, &cPath, JEWEL_ICON);
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
		Fill(&cDraw, &cPath, JEWEL_WHITE);
		cPath.Kill();
	}
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
Ptr<CImageCel> CIDEIcons::CreateSymbolIcon(EIDESymbolType eType, CColourFormatHelper* pcFormat)
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
	Fill(&cDraw, &cPath, JEWEL_WHITE);
	cPath.Kill();
	cDraw.Kill();

	pImage = CImageColourConverter::ConvertColour(pImage, pcFormat->meOrder);
	return OMalloc<CImageCel>(pImage);
}
