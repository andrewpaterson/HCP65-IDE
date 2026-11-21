#ifndef __PAINT_H__
#define __PAINT_H__


#define PAINT_MAXIMUM	8


enum EPaintMode
{
	PM_Fill,
	PM_Outline = 4,
};


typedef unsigned int	PaintColour;


//Paints rectangles.
class CPainter
{
protected:
	PaintColour	muiColour;
	int			maiSizes[PAINT_MAXIMUM];

public:
	void	Init(PaintColour uiColour);
	void	Rect(int iX, int iY);
	void	Rect(int iX, int iY, int iWidth, int iHeight, EPaintMode eMode = PM_Fill);
	int		Count(void) const;
	static CPainter*	Default(void);
};


int		Blend(int iFirst, int iSecond, float fAmount);


#endif // __PAINT_H__
