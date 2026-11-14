#ifndef __EXTRA_H__
#define __EXTRA_H__


namespace NSpare
{
	int		Twice(int iValue);

	class CInside
	{
	public:
		int		mi;
	};
}


template<class M>
M Largest(M a, M b)
{
	return (a > b) ? a : b;
}


template<class M>
class CBox
{
public:
	M		mItem;

	M		Get(void) { return mItem; }
	void	Set(M item);
};


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
template<class M>
void CBox<M>::Set(M item)
{
	mItem = item;
}


using TWord = unsigned short;


#endif // __EXTRA_H__
