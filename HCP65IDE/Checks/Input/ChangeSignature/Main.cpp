#include "Paint.h"


int (*gpfNext)(void) = Next;


int main(void)
{
	CBrush		cBrush;
	CSpray		cSpray;
	CBrush*		pcBrush;
	int			iMixed;

	cBrush.Init(4);
	cSpray.Init(2, "wide");
	iMixed = cBrush.Mix(1, Next(), 3);
	pcBrush = &cSpray;
	pcBrush->Draw(iMixed, 2);
	cSpray.Draw(3, 4);
	cBrush.Clear();
	return Blend(1, 2) + Blend(1, 2, 3);
}
