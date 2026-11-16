#ifndef __SHAPES_H__
#define __SHAPES_H__


namespace Geometry
{
	typedef int	TLength;


	class CPoint
	{
	public:
		TLength	miX;
		TLength	miY;

		void	Init(TLength iX, TLength iY);
		TLength	Sum(void);
	};


	TLength	Area(CPoint* pcPoint);
	extern int	giCount;

	namespace Detail
	{
		int	Twice(int i);
	}
}


namespace Drawing
{
	typedef long	TLength;

	void	Plot(Geometry::CPoint* pcPoint);
}


#endif // __SHAPES_H__
