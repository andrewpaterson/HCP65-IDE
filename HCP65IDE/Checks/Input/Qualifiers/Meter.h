#ifndef __METER_H__
#define __METER_H__


struct SMark
{
	int		iAt;
};


class CMeter
{
public:
	int		miLevel;
	int		maiMarks[4];
	SMark	msMark;
	CMeter*	mpcNext;

			void	Init(void);
	virtual	int		Level(void);
			int		Top(void) { return maiMarks[3]; }
			int		Zero(void) const { return 0; }
			int		Raise(int iBy);
			int		Mark(void);
			int		Both(void);
			int		Next(void);
			int		Scale(int iValue);
			int		Twice(int iValue);
			CMeter*	Find(void);
};


class CFineMeter : public CMeter
{
public:
	int		Level(void) override;
};


#endif // __METER_H__
