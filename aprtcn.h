#ifndef _APRTCN_H_
#define _APRTCN_H_
/* Prototypes for the functions in aprtcn.c */

void sendConsCmd(char * CHCmd, char * keyinName);
void doAPRTCN(char * s1);

/* #defines */

#define MAX_RUNID_CHAR_SIZE 6
#define MAX_KEYIN_NAME_CHAR_SIZE 8
#endif /* _APRTCN_H_ */
