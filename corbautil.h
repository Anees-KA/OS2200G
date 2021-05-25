#ifndef _CORBAUTIL_H_
#define _CORBAUTIL_H_

/* JDBC Project Files */
#include "xajdbcserver.h"

void getCORBAByteArray(
    com_unisys_os2200_rdms_jdbc_byteArray * byteArrayStructPtr,
    int * len,
    CORBA_octet ** byteArrayPtrPtr);

 void setUpCORBAByteArray(int len,
    com_unisys_os2200_rdms_jdbc_byteArray ** byteArrayStructPtrPtr,
    CORBA_octet ** byteArrayPtrPtr);

void printCORBAByteArrayInHex(char * ret, int len);

void freeCORBAPacket(
    com_unisys_os2200_rdms_jdbc_byteArray * byteArrayStructPtr);

com_unisys_os2200_rdms_jdbc_byteArray * formCORBAPacket(CORBA_octet * byteArrayPtr, int len);

#endif /* _CORBAUTIL_H_ */
