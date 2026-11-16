#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__


template<class M>
M	gmPi = M(3.14159265);


template<>
int	gmPi<int> = 3;


namespace Geometry
{
	template<class M>
	M	gmTau = M(6.2831853);
}


#endif // __CONSTANTS_H__
