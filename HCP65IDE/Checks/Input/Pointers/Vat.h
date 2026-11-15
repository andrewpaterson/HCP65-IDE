#ifndef __VAT_H__
#define __VAT_H__


struct SLevel
{
	int		miDepth;
	int		miMark;
};


class CVat
{
public:
	SLevel	msLevel;
	int		miCount;

			void	Init(void);
	virtual	int		Fill(SLevel* psLevel, int iAmount);
			int		Drain(const SLevel& sLevel);
			int		Check(SLevel* psLevel);
			void	Step(SLevel* psLevel);
			int		Sum(SLevel* psLevel, int iTimes);
};


class CDeepVat : public CVat
{
public:
	int		Fill(SLevel* psLevel, int iAmount) override;
};


int		Depth(SLevel* psLevel);


#endif // __VAT_H__
