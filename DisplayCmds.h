#ifndef _DISPLAY_CMDS_H_
#define _DISPLAY_CMDS_H_

/* JDBC Project Files */
#include "JDBCServer.h"

/* Prototypes for functions in DisplayCmds.c */

int processDisplayCmd(char * msg);

int processDisplayConfigurationCmd(int displayAll, int logIndicator,
                                   int toConsole,
                                   workerDescriptionEntry * wdePtr);

void displayActiveWorkerInfo(int logIndicator, int toConsole,
                             workerDescriptionEntry * wdePtr,
                             int displayAll,  int displayShort);

void getClientInfoTraceImages(char * messageBuffer, workerDescriptionEntry * wdePtr);

void displayAllActiveWorkersInLog(int logIndicator, int displayAll);

int processHelpCmd(char * msg);

#endif /* _DISPLAY_CMDS_H_ */
