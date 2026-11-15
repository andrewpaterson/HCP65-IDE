#ifndef __CRATE_H__
#define __CRATE_H__


template<class M>
class CBox
{
public:
	M		mcItem;

	M*		Get(void) { return &mcItem; }
};


template<class M>
class CCrate : public CBox<M>
{
public:
	int		Size(void);
};


template<class M>
int CCrate<M>::Size(void)
{
	return sizeof(M);
}


#endif // __CRATE_H__
