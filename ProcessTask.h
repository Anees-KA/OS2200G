#ifndef _PROCESSTASK_H_
#define _PROCESSTASK_H_

/* JDBC Project Files */
#include "JDBCServer.h"

/**
 * File: ProcessTask.h.
 *
 * Header file for the code that processes a JDBC Client's request packet
 * task for a Server Worker.
 *
 * Header file corresponding to code file ProcessTask.c.
 *
 */

/* Function prototypes */

int processClientTask(workerDescriptionEntry *wdePtr, char *requestPacketPtr, char **responsePacketPtrPtr);

int keepAliveTask(workerDescriptionEntry *wdePtr, char *requestPtr, char **responsePtrPtr);

/* Same definition as in ServerLog.h. Placed here to avoid compiling issues. */
#define ITOA_BUFFERSIZE 110 /* itoa() buffer must be 110 (20+90)characters in size to have both digits, IP address, user id, and thread id */

#endif /* _PROCESSTASK_H_ */
