#ifndef __DIESEL_H__
#define __DIESEL_H__


class CDiesel
{
public:
	int		miRuns;

	void			Start(int iSpeed) { miRuns = iSpeed; }
	virtual void	Stop(void) { miRuns = 0; }
	int				Count(void) const { return miRuns; }
};


#endif // __DIESEL_H__
