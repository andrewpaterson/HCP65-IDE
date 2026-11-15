#ifndef __RUNNER_H__
#define __RUNNER_H__


//Runs what it is given.
template<class M>
class CRunner
{
public:
	M*		mp;

	void	Go(void) { mp->Start(3); mp->Stop(); }
	void	Again(void) { this->mp->Start(4); }
	int		Count(M* pcItem);
};


template<class T>
int CRunner<T>::Count(T* pcItem)
{
	return pcItem->Count();
}


template<class M>
void Poke(M* pcItem)
{
	pcItem->Wake(1, 2);
}


#endif // __RUNNER_H__
