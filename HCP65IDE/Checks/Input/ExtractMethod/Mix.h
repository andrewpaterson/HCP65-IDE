#ifndef __MIX_H__
#define __MIX_H__


struct SPoint
{
	int	iX;
	int	iY;
};


class CMix
{
protected:
	int		miBase;
	int		miCount;

public:
	void	Init(int iBase);
	int		Blend(int iA, int iB) const;
	int		Sum(int iLimit);
	void	Both(int iA);
	int		Early(int iA);
	void	Move(SPoint* psPoint);
	int		Twice(int iA) { return iA * 2 + miBase; }
};


template<class M>
class CPile
{
public:
	M		mcTop;
	int		miDepth;

	int		Weigh(int iScale);
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
template<class M>
int CPile<M>::Weigh(int iScale)
{
	int	iWeight;

	iWeight = miDepth * iScale;
	iWeight += sizeof(M);
	return iWeight;
}


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
template<class M>
M Larger(M a, M b)
{
	M	cResult;

	cResult = a;
	if (b > a)
	{
		cResult = b;
	}
	return cResult;
}


#endif // __MIX_H__
