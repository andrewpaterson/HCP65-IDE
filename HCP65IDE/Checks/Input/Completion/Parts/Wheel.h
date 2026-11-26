#ifndef __WHEEL_H__
#define __WHEEL_H__


class CWheel
{
public:
	int		miPressure;

	void	Inflate(int iPressure);
	int		GetPressure(void);
};


#endif // __WHEEL_H__
