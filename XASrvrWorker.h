#ifndef _XA_SRVR_WORKER_H_
#define _XA_SRVR_WORKER_H_

/* JDBC Project Files */
#include "xajdbcserver.h"

/* Prototypes for the functions in XASrvrWorker.c */

com_unisys_os2200_rdms_jdbc_byteArray * XAServerWorker(
        CORBA_Object _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag);

void XAServerWorkerShutDown(int who);

extern void _reg4term$c();

extern void XA_SW_term_reg(int userid, int userval);

void start_XAServerConsoleHandler();

#endif /* _XA_SRVR_WORKER_H_ */
