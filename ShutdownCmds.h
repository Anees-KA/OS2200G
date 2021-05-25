#ifndef _SHUTDOWN_CMDS_H_
#define _SHUTDOWN_CMDS_H_
/* Prototypes for functions in ShutdownCmds.c */

int processShutdownServerCmd(char * msg);
int processShutdownWorkerCmd(char *msg);
void stopServer(int abort);
#endif /* _SHUTDOWN_CMDS_H_ */
