#ifndef _CONSOLE_CMDS_H_
#define _CONSOLE_CMDS_H_

/* JDBC Project Files */
#include "JDBCServer.h"

/* Prototypes for utility functions in ConsoleCmds.c */

int findWorker(int * clientSocketIDPtr, workerDescriptionEntry ** retPtr);
void findWorker_by_RDMS_threadname(char * threadnamePtr, int * clientSocketIDPtr);
int getInternalSocketID(int client_socket);

/* #define for magic socket ID constant.
   This value is used in console command syntax,
   where a client ID or worker ID is specified,
   to get the first active client
   (i.e., the first one with a non-zero socket ID).
   This magic constant is not documented for users
   (only available in development mode,
   for console testing). */
#define magicSocketID -9
#define magicSocketID_as_string "-9"

/* #define for the client socket ID value to use
   for a failed thread name lookup.  This value
   must be negative and not the same as the value
   defined by magicSocketID, since it will be
   part of a value verification test which must
   fail (indicating an unfound client Server Worker. */
#define NOFIND_THREADNAME_SOCKETID -1

#endif /* _CONSOLE_CMDS_H_ */
