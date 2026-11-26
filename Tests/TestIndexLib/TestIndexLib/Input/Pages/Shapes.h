#ifndef __SHAPES_H__
#define __SHAPES_H__


enum EKind
{
	EK_Square,
	EK_Circle,
};


class CShape
{
protected:
	EKind	meKind;

public:
	void	Init(EKind eKind);
	EKind	GetKind(void);
};


#endif // __SHAPES_H__
