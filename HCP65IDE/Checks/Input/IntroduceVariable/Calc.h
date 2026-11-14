#ifndef __CALC_H__
#define __CALC_H__


typedef unsigned int	uint32;


class CCalc
{
public:
	uint32	muiCount;
	int		miScale;
	char*	mszName;

	void				Init(void);
	uint32				GetCount(void);
	int					Scaled(int iValue);
	char*				GetName(void);
	unsigned long long	BigValue(void);
	int					Total(int iExtra);
	void				Reset(void);
	int					Big(void);
};


template<class M>
class CStack
{
public:
	M*		mpTop;
	int		miDepth;

	M*		Top(void) { return mpTop; }
	int		Depth(void);
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
template<class M>
int CStack<M>::Depth(void)
{
	int	iResult;

	iResult = miDepth * 2 + Top()->miScale;
	return iResult;
}


#endif // __CALC_H__
