#include "Bases.h"


void CSwitchable::Switch(bool bOn)
{
}


const char* CWidget::Name(void)
{
	return "Widget";
}


void CLamp::Switch(bool bOn)
{
	miCount = bOn ? 1 : 0;
}
