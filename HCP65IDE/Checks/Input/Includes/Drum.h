#ifndef __DRUM_H__
#define __DRUM_H__


class CDrum
{
public:
	CPipe	mcPipe;
	CGauge*	mpcGauge;

	void	Init(CGauge* pcGauge);
	int		Bore(void);
};


#endif // __DRUM_H__
