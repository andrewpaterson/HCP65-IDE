#ifndef __SHAPES_H__
#define __SHAPES_H__


class CShape
{
protected:
	int		miSides;

public:
	void	Init(int iSides);
	int		GetSides(void);
};


#endif // __SHAPES_H__
