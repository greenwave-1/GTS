//
// Created on 10/29/25.
//

#ifndef GTS_DATETIME_H
#define GTS_DATETIME_H

char *getDateTimeStr();

enum DATE_CHECK_LIST { DATE_NONE, DATE_AF, DATE_PM, DATE_CMAS };

void forceDate(enum DATE_CHECK_LIST dateToForce);

enum DATE_CHECK_LIST checkDate();

#endif //GTS_DATETIME_H
