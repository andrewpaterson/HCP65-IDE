#ifndef __SHAPE_H__
#define __SHAPE_H__


//A CShape is drawn with a CPen.
class CPen
{
public:
	void	Draw(void);
};


class CShape
{
public:
	int		miSides;

	void	Init(int iSides);
	void	Draw(CPen* pcPen);
	void	InitPlatform(void);
};


#endif // __SHAPE_H__
