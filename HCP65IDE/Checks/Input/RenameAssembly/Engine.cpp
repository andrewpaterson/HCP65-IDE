#include "Engine.h"


const char* gszEngineSpeed = "CEngine_miSpeed";


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


void CEngine::Mix(int iSpeed)
{
	miSpeed = iSpeed;
}


void CEngine::Mix(long lSpeed)
{
	miSpeed = (int)lSpeed;
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
