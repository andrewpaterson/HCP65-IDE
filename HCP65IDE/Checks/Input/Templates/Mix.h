#ifndef __MIX_H__
#define __MIX_H__


class CGauge
{
public:
	struct SMark
	{
		int		iAt;
	};

	int		miLevel;

	int		Read(void) { return miLevel; }
};


class CMixer
{
public:
	int		miTotal;

	int		Sum(CGauge* pcGauge, int iTimes);
	virtual	int		Blend(CGauge* pcGauge);
};


int		Total(CGauge* pcGauge, int iCount);
int		Make(int iSeed);
int		Marked(CGauge* pcGauge);


#endif // __MIX_H__
