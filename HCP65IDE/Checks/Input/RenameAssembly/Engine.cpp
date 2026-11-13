#include "Engine.h"


void CEngine::Init(void)
{
	miSpeed = 0;
}


void CEngine::Run(int iSteps)
{
	miSpeed += iSteps;
}


void CEngine::Run(char cStep)
{
	miSpeed += cStep;
}


void CEngineCache::Init(void)
{
}


void CPart::Attach(CEngine* pcEngine)
{
	pcEngine->Init();
}


int EngineCount(CEngine* pcEngine)
{
	return pcEngine->miSpeed;
}
