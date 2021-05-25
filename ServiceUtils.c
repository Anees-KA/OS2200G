/**
 * File: ServiceUtils.c.
 *
 * Utility code for the JDBC Server and XA JDBC Server.
 *
 * Note: Much of the declarations/code in the ServiceUtils.h and
 * ServiceUtils.c files have been extracted from the
 * ServerConsol.h, ConsoleCmds.h and ConsoleCmds.c files.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <ertran.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <ctype.h>
#include <universal.h>
#include <tsqueue.h>  /* needed for T/S cell definitions */
#include "marshal.h"

/* JDBC Project Files */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "ProcessTask.h"
#include "Server.h"
#include "ServerLog.h"
#include "ServiceUtils.h"

/* Imported data */
extern serverGlobalData sgd; /* The Server Global Data (SGD), visible to all Server activities. */

/* Utility functions
   ----------------- */

/**
 * getTrueFalse
 *
 * Utility to return a TRUE or FALSE string from an int argument.
 *
 * @param value
 *   An int input.
 *
 * @return
 *   A TRUE or FALSE string is returned.
 */

char * getTrueFalse(int value) {

    if (value == 0) {
        return "FALSE";
    } else {
        return "TRUE";
    }
}

/**
 * Function doACSF
 *
 * Do an ER ACSF$.
 *
 * @param cmd
 *   The string passed to ER ACSF$.
 *   It is the caller's responsibility to make sure that the ER ACSF$
 *   string is terminated with a ' . ' (before the NUL terminator).
 *   The max length of an ACSF image is 240 characters.
 *
 * @return
 *   The integer from the ER is returned.
 */

int doACSF(char * cmd) {

    int status;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    status = facsf(cmd);

    if (sgd.debug){
     if (status < 0) {
        getLocalizedMessageServer(CH_ACSF_FAILED, itoa(status, digits, 8),
            cmd, SERVER_LOGS, msgBuffer);
                /* 5200 Error return from ER ACSF$ (octal): {0} CSF image= {1} */
     }
    }

    return status;
} /* doACSF */


/**
 * Function toUpperCase
 *
 * Change the characters of a string to all upper case.
 *
 * @param string
 *   The string to traverse.
 */

void toUpperCase(char * string) {

    int len;
    int i1;

    len = strlen(string);

    for (i1 = 0; i1<len; i1++) {
        string[i1] = toupper(string[i1]);
    }
}

/**
 * Function toLowerCase
 *
 * Change the characters of a string to all lower case.
 *
 * @param string
 *   The string to traverse.
 */

void toLowerCase(char * string) {

    int len;
    int i1;

    len = strlen(string);

    for (i1 = 0; i1<len; i1++) {
        string[i1] = tolower(string[i1]);
    }
}

/**
 * Function getFullFileName
 *
 * Convert a USE name to a qualifier*file name.
 * Optionally add an F-cycle to the end of the string.
 *
 * @param internalName
 *   A string containing a USE name.
 *
 * @param externalName
 *   A string returned as in the qualifier*file format.
 *
 * @param addFCycle
 *   A flag that is TRUE if the F-cycle should be added to the end of the
 *   returned file name,
 *   and FALSE if only the qualifier*filename string should be returned.
 */

void getFullFileName(char * internalName, char * externalName, int addFCycle) {

    unionWord6 word6;
    char fileName[MAX_INTERNAL_FILE_NAME_LEN+1];
    char qualifierName[MAX_INTERNAL_FILE_NAME_LEN+1];
    char ACSFString[MAX_ACSF_STRING_SIZE+1];
    int array[7];
    int status;

    /* Initialize the returned string to an empty string */
    if (externalName == NULL) {
        return;
    }

    externalName[0] = '\0';

    /* Assign the file, since ER FITEM$ requires
       that the file be assigned to get the proper F cycle.
       The USE name should be correct,
       so we should not fail the @ASG,Z
       but check anyway.
       */
    if (addFCycle == TRUE) {
        sprintf(ACSFString, "@ASG,Z %s . ", internalName);

        status = doACSF(ACSFString);

        if (status < 0) {
            return; /* return an empty string */
        }
    }

    /* Initialize values before the call to ER FITEM$ */
    array[0] = 0;
    fileName[0] = '\0';
    qualifierName[0] = '\0';

    /* ER FITEM$ returns the qualifier and filename of the
       file, in addition to extra packet words
       (words 6-12).
       */
    ucsfitem(internalName, fileName, qualifierName, array);

    /* Get word 6 of the ER FITEM$ packet.
       It contains the equip code (a status that is
       zero if the file was not assigned or does not exist)
       and the absolute file cycle.
       The absolute file cycle is also zero if the file
       is not assigned or does not exist.
       */
    word6.i1 = array[0];

    if (word6.f6.equipCode == 0) {
        return; /* return an empty string */
    }

    /* Trim the blanks off of the end of the qualifier and filename strings */
    trimTrailingBlanks(qualifierName);
    trimTrailingBlanks(fileName);

    /* Handle the F-cycle in the returned string: qual*file(abs-f-cycle) */
    if (addFCycle == TRUE) {
        sprintf(externalName, "%s*%s(%d)", qualifierName, fileName,
            word6.f6.absFileCycle);

    /* There is no F-cycle in the returned string: qual*file
         */
    } else {
        sprintf(externalName, "%s*%s", qualifierName, fileName);
    }

} /* getFullFileName */

/**
 * Function getFileCycle
 *
 * Get the absolute F cycle for a file.
 *
 * @param internalName
 *   A string containing a USE name for the file.
 *
 * @return
 *   The file's absolute F cycle is returned.
 */

int getFileCycle(char * internalName) {

    unionWord6 word6;

    char fileName[MAX_INTERNAL_FILE_NAME_LEN+1];
    char qualifierName[MAX_INTERNAL_FILE_NAME_LEN+1];
    char ACSFString[MAX_ACSF_STRING_SIZE+1];
    int array[7];
    int status;

    /* Assign the file, since ER FITEM$ requires
       that the file be assigned to get the proper F cycle
       */
    sprintf(ACSFString, "@ASG,Z %s . ", internalName);

    status = doACSF(ACSFString);

    if (status < 0) {
        return 0; /* return a 0 int if the assign failed */
    }

    /* Initialize FITEM word 6 before the call */
    array[0] = 0;

    /* ER FITEM$ returns the qualifier and filename of the
       file, in addition to extra packet words
       (words 6-12).
       */
    ucsfitem(internalName, fileName, qualifierName, array);

    /* Get word 6 of the ER FITEM$ packet.
       It contains the equip code (a status that is zero
       if the file was not assigned or does not exist)
       and the absolute file cycle.
       The absolute file cycle is also zero if the file
       is not assigned or does not exist.
       */
    word6.i1 = array[0];

    /* If the file is not assigned or does not exist, return 0 */
    if (word6.f6.equipCode == 0) {
        return 0;
    }

    /* Get the absolute F cycle from the packet and return it */
    return word6.f6.absFileCycle;

} /* getFileCycle */

/**
 * Function trimTrailingBlanks
 *
 * Trim trailing blanks from the string argument.
 *
 * Algorithm:
 *   Go through the string backwards.
 *   As long as you continue to hit a space (blank) character,
 *   change it to a NUL terminator (\0).
 *   Exit the loop when you hit a nonblank character.
 *
 * @param string
 *   The string that is examined for trailing blanks.
 */

void trimTrailingBlanks(char * string) {

    int len;
    int i1;

    len = strlen(string);

    for (i1 = len-1; i1>=0; i1--) {
        if (string[i1] == ' ') {
            string[i1] = '\0';

        } else {
            break;
        }
    }
} /* trimTrailingBlanks */

/**
 * Function moveToNextNonblank
 *
 * Bump the char pointer to the next nonblank character.
 * On entry or on exit,
 * the argument can point to the NUL terminator.
 *
 * @param ptr
 *   A char pointer reference parameter.
 */

void moveToNextNonblank(char ** ptr) {

    int i1;
    int len;

    len = strlen(*ptr);

    for (i1 = 0; i1 < len; i1++) {
        if (**ptr == ' ') {
            (*ptr)++;
        } else {
            return;
        }
    }
} /* moveToNextNonblank */

/**
 * fopenWithErase
 *
 * Open a file for writing:
 *   - If the file does not exist,
 *     CAT it via fopen() with append ("a") mode.
 *   - If the file exists and contains data,
 *     erase the file by opening it with "w" (write) mode.
 *
 * In summary, always open the file to start writing at the top.
 *
 * Note that the steps to call fopen(), fclose(), and fopen()
 * might sound like extra work.
 * But it's the only way we can handle the operation
 * without parsing 2200 filenames (which is undesireable).
 *
 * @param fileName
 *   A string containing the filename of the file to open.
 *
 * @return
 *   A FILE pointer for the opened file is returned.
 *   NULL is returned if the file can't be opened.
 */

FILE * fopenWithErase(char * fileName) {

    FILE * fptr;

    /* To get the file CAT'ed if is does not exist,
       open in append ("a") mode. */
    fptr = fopen(fileName, "a");

    if (fptr == NULL) {
        /* Maybe the file already exists, but PCIOS could'nt
           position it for appending due to file corruption. Try
           doing an fopen with "w" option, which causes an implicit
           rewind of the file (so that any data in the file is erased).
           If this works, that was the problem and we will have the
           file opened ready to go, if not then we got a NULL handle.
           Either way, return the result to the caller.*/
        fptr = fopen(fileName, "w");
        return fptr;
    }

    /* Close the file, since we want to reopen it in write mode. */
    fclose(fptr);

    /* Open the file in write ("w") mode, which causes an implicit
       rewind of the file (so that any data in the file is erased).
       This should never be NULL, since the file exists.
       Note that a rewind() call does not work on a writeable file
       (it seems to be ignored). */
    fptr = fopen(fileName, "w");

    return fptr;

} /* fopenWithErase */

/**
 * checkForDigit
 *
 * This function checks for a digit as the first character in
 * the input string.
 *
 * If found, it does a string copy of the input string,
 * starting at the 6th character, to the output string.
 * This is because the error number prefix is 4 characters,
 * followed by a blank, followed by the message.
 *
 * If not found, it does a string copy of the entire input
 * string to the output string.
 *
 * @param output
 *   The output string.
 *
 * @param input
 *   The input string.
 */

void checkForDigit(char * output, char * input) {

    if (isdigit(input[0])) {
        strcpy(output, input+ERROR_NUMBER_PREFIX_LEN+1);
            /* remove leading message number from input */
    } else {
        strcpy(output, input);
    }

} /* checkForDigit */
