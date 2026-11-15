#include "Modes.h"


//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
int UseMotor(CMotor* pcMotor)
{
	int	iNext;

	pcMotor->meMode = EM_Fast;
	pcMotor->meState = CMotor::ES_Running;
	pcMotor->meLevel = EL_High;
	iNext = pcMotor->meLevel + 1;
	return (pcMotor->meLevel == EL_High) ? iNext : 0;
}
