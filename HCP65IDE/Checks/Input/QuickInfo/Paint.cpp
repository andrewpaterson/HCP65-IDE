#include "Paint.h"


static CPainter	gcDefaultPainter;


void CPainter::Init(PaintColour uiColour)
{
	muiColour = uiColour;
}


void CPainter::Rect(int iX, int iY)
{
	Rect(iX, iY, 1, 1);
}


void CPainter::Rect(int iX, int iY, int iWidth, int iHeight, EPaintMode eMode)
{
	maiSizes[0] = iWidth * iHeight;
}


int CPainter::Count(void) const
{
	return PAINT_MAXIMUM;
}


CPainter* CPainter::Default(void)
{
	return &gcDefaultPainter;
}


int Blend(int iFirst, int iSecond, float fAmount)
{
	return iFirst + (int)((iSecond - iFirst) * fAmount);
}


int PaintAll(CPainter* pcPainter, int iCount)
{
	int		iTotal;
	int		i;

	iTotal = 0;
	for (i = 0; i < iCount; i++)
	{
		pcPainter->Rect(i, Blend(i, iCount, 0.5f), 2, 2, PM_Outline);
		iTotal += pcPainter->Count();
	}
	pcPainter->Rect(iTotal, 0);
	return iTotal;
}
