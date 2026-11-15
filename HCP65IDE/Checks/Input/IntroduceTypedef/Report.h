#ifndef __REPORT_H__
#define __REPORT_H__
#include "Types.h"


#define REPORT_LIST	CList<SItem>


int		CountItems(CList<SItem>* pcList);
int		CountBoth(CList<SItem>* pcFirst, REPORT_LIST* pcSecond);


#endif // __REPORT_H__
