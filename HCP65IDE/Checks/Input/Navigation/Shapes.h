#ifndef __SHAPES_H__
#define __SHAPES_H__
#include "Shape.h"


class CCircle : public CShape
{
protected:
	int		miRadius;

public:
	void	Draw(void) override;
	int		Area(void) override;
};


class CSquare : public CShape
{
protected:
	int		miSide;

public:
	void	Draw(void) override;
	int		Area(void) override;
};


class CRoundedSquare : public CSquare
{
public:
	void	Draw(void) override;
};


#endif // __SHAPES_H__
