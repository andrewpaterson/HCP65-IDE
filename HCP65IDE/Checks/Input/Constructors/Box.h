#ifndef __BOX_H__
#define __BOX_H__


class CBox
{
public:
	int		miWidth;
	int		miHeight;

	CBox(int iWidth, int iHeight);
};


class CCrate : public CBox
{
public:
	CBox	mcInner;

	CCrate(int iSize);
};


#endif // __BOX_H__
