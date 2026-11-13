#ifndef __ENGINE_H__
#define __ENGINE_H__


class CEngine
{
public:
	int		miSpeed;

	void	Init(void);
	void	Run(int iSteps);
	void	Run(char cStep);
	void	Mix(int iSpeed);
	void	Mix(long lSpeed);
};


class CEngineCache
{
public:
	void	Init(void);
};


class CPart
{
public:
	void	Attach(CEngine* pcEngine);
};


int EngineCount(CEngine* pcEngine);


#endif // __ENGINE_H__
