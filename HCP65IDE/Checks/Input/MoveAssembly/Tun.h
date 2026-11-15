#ifndef __TUN_H__
#define __TUN_H__


class CVat
{
public:
	int		miSize;

	void	Init(void);
};


class CTun : public CVat
{
public:
	int		miLevel;

	void	Fill(void);
	void	Drain(int iBy);
	void	Drain(long lBy);
	void	Drain(char cBy);
	void	Stir(void);
};


#endif // __TUN_H__
