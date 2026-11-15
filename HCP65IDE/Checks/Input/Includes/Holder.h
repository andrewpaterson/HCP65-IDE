#ifndef __HOLDER_H__
#define __HOLDER_H__


template<class M>
class CHolder
{
public:
	M*		mp;

	void	Start(M* p) { mp = p; mp->Init(); }
};


#endif // __HOLDER_H__
