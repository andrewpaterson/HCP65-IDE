#ifndef __GEAR_H__
#define __GEAR_H__


namespace NGear
{
	class CGear
	{
	public:
		int		miTeeth;

		void	Turn(int iSteps);
	};

	void Spin(CGear* pcGear);
}


template<class T>
class CBox
{
public:
	T		mtItem;

	void	Fill(T tItem) { mtItem = tItem; }
};


void Pack(CBox<NGear::CGear>* pcBox);
void Load(CBox<int>* pcBox);


namespace NGear
{
	extern "C"
	{
		int GearCount(int iGears);
	}
}


#endif // __GEAR_H__
