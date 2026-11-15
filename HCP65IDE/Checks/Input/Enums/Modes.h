#ifndef __MODES_H__
#define __MODES_H__


enum EMode
{
	EM_Off,
	EM_Slow = 4,
	EM_Fast = EM_Slow * 2,
};


enum ELevel
{
	EL_Low,
	EL_High,
};


class CMotor
{
public:
	enum EState
	{
		ES_Idle,
		ES_Running,
	};

	EMode	meMode;
	EState	meState;
	ELevel	meLevel;

	void	Init(void);
	bool	IsFast(void) { return meMode == EM_Fast; }
	int		Speed(void);
};


#endif // __MODES_H__
