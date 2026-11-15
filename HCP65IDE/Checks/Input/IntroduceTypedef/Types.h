#ifndef __TYPES_H__
#define __TYPES_H__


template<class M>
class CList
{
public:
	M*		mpFirst;
	int		miCount;

	M*		Get(int iIndex) { return mpFirst; }
	M*		Last(void);
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
template<class M>
M* CList<M>::Last(void)
{
	M*	pLast;

	pLast = mpFirst;
	return pLast;
}


struct SItem
{
	int	iValue;
};


//The items of a thing.
class CItems
{
public:
	CList<SItem>		mcItems;
	unsigned long long	mulTotal;

	CList<SItem>*		GetItems(void);
	unsigned long long	Total(CList<SItem>* pcOther);
};


#endif // __TYPES_H__
