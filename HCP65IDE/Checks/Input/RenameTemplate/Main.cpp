#include "Holder.h"
#include "Foo.h"
#include "Bar.h"
#include "Lone.h"


int main(void)
{
	CHolder<CFoo>	cFoos;
	CHolder<CBar>	cBars;
	CFoo			cFoo;
	CBar			cBar;
	CLone			cLone;

	cFoo.Init();
	cBar.Init();
	cLone.Init();
	cFoos.mp = &cFoo;
	cBars.mp = &cBar;
	cFoos.Start();
	cBars.Start();
	return cFoos.Total() + cBars.Total();
}
