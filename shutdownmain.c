/* Standard C header files and OS2200 System Library files */
#include <sysutil.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* JDBC Project Files */
#include "aprtcn.h"

/**
 * Main program for the SHUTDOWN program (to shut down the JDBC server).
 *
 * Format: SHUTDOWN keyin-name
 *
 *   where keyin-name is the keyin name for Console Commands for the server.
 *
 * E.g.: SHUTDOWN jsag3
 */

int main() {
    char keyinName[MAX_KEYIN_NAME_CHAR_SIZE+1];

    getInvocationImageParameters(keyinName);

    if (strlen(keyinName) == 0) {
        printf("The keyin name field was not specified\n");
        return 1;
    }

    /* Execute the Console Handler's SHUTDOWN command */
    sendConsCmd("shutdown", keyinName);

    return 0;
} /* main */

/**
 * getInvocationImageParameters
 *
 * Read the executable program's command line arguments.
 *
 * @param keyinName
 *   A string that is the JDBC server's keyin name (for Console Commands).
 *
 * @return
 *   An int word containing the program's option bits is returned.
 */

static int getInvocationImageParameters(char * keyinName) {

    int option_bits;
    int argc;
    char ** argv;

    /* Get command line arguments */
    _cmd_line(&argc,&argv,&option_bits,0);

    /* Get the field containing the keyin name for the Console Commands
       for the server */
    if (argc > 1) {
        strncpy(keyinName, argv[1], MAX_KEYIN_NAME_CHAR_SIZE);
    } else {
        keyinName[0]=0; /* indicate that no field was specified */
    }

     /* Release the argv space malloc'ed by _cmd_line */
     free(argv);

     /* Return an int word containing the program's option bits */
     return option_bits;

} /* getInvocationImageParameters */
