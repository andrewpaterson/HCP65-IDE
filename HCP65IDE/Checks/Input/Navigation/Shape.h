#ifndef __SHAPE_H__
#define __SHAPE_H__


//A shape that can be drawn.
class CShape
{
protected:
	int		miX;
	int		miY;

public:
	void			Init(int iX, int iY);
	virtual void	Draw(void);
	virtual int		Area(void) = 0;
	void			Move(int iX, int iY);
};


#endif // __SHAPE_H__
