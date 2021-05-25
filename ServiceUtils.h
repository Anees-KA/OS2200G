#ifndef _SERVICEUTILS_H_
#define _SERVICEUTILS_H_
/**
 * File: ServiceUtils.h.
 *
 * Header file for utility declarations/code for the
 * JDBC Server and XA JDBC Server.
 *
 * Header file corresponding to code file ServiceUtils.c.
 *
 *
 * Note: Much of the declarations/code in the ServiceUtils.h and
 * ServiceUtils.c files have been extracted from the
 * ServerConsol.h, ConsoleCmds.h and ConsoleCmds.c files.
 */

#define MAX_INTERNAL_FILE_NAME_LEN 12 /* Same value as defined in ServerConsol.h */
#define MAX_ACSF_STRING_SIZE 240      /* Same value as defined in ServerConsol.h */

/* typedef for ER FITEM$ packet word 6. */ /* Same value as defined in ConsolCmds.c */
typedef union {
    struct {
        int equipCode    : 6;
        int filler       : 18;
        int absFileCycle : 12;
    } f6;

    int i1;
} unionWord6;

/* Prototypes for utility functions in ServiceUtils.c */

char * getTrueFalse(int value);
void trimTrailingBlanks(char * string);
int doACSF(char * cmd);
void moveToNextNonblank(char ** ptr);
void getFullFileName(char * internalName, char * externalName, int addFCycle);
int getFileCycle(char * internalName);
void toUpperCase(char * string);
void toLowerCase(char * string);
FILE * fopenWithErase(char * fileName);
void checkForDigit(char * output, char * input);

#endif /* _SERVICEUTILS_H_ */
