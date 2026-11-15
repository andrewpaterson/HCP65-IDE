#ifndef __PAIR_H__
#define __PAIR_H__
#include "Mix.h"


template<class A, class B>
class CPair
{
public:
	A*		mpFirst;
	B		mSecond;

	void	Set(A* pFirst, B second);
	int		Read(void) { return mpFirst->Read(); }
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
template<class A, class B>
void CPair<A, B>::Set(A* pFirst, B second)
{
	mpFirst = pFirst;
	mSecond = second;
}


template<class M>
class CLone
{
public:
	M*		mp;

	void	Hold(M* p);
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
template<class M>
void CLone<M>::Hold(M* p)
{
	mp = p;
}


#endif // __PAIR_H__
