#ifndef __HOLDER_H__
#define __HOLDER_H__


template<class M>
class CHolder
{
public:
	M*		mp;

	void	Start(void) { mp->Init(); }
	int		Total(void) { return mp->miCount; }
};


#endif // __HOLDER_H__
