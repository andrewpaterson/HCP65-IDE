#ifndef __SHAPE_H__
#define __SHAPE_H__


class CUnused;


enum EShapeKind
{
	SK_Circle,
	SK_Square,
	SK_Spare,
};


//A shape.
class CShape
{
public:
	int		miWidth, miHeight;
	int		miSpare;

	void	Init(void);
	void	Kill(void);
	int		Area(void);
	int		Unused(int iScale);
	virtual void	Draw(void);
};


class CCircle : public CShape
{
public:
	void	Draw(void) override;
};


//Nothing uses it but itself.
class CLonely
{
public:
	int		miValue;

	void	Init(void);
	int		Get(void);
};


typedef int	TSpare;
int		UnusedFunction(int iValue);
int		UsedFunction(int iValue);
extern int	giUnused;


#endif // __SHAPE_H__
