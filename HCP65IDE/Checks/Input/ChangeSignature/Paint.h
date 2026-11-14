#ifndef __PAINT_H__
#define __PAINT_H__


class CBrush
{
public:
	int		miSize;

	void			Init(int iSize, const char* szName = 0);
	int				Mix(int iRed, int iGreen, int iBlue);
	virtual void	Draw(int iX, int iY);
	void			Clear(void);
};


class CSpray : public CBrush
{
public:
	void	Draw(int iX, int iY) override;
};


int		Blend(int iFirst, int iSecond);
int		Blend(int iFirst, int iSecond, int iThird);
int		Next(void);


#endif // __PAINT_H__
