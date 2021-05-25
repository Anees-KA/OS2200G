/**
 * File: DisplayCmds.c.
 *
 * Functions that process the JDBC Server's DISPLAY console handling commands.
 *
 * In the process functions in this file:
 *
 *   - A return of 1 means that the textMessage
 *     string (in sgd) contains the message to be returned via ER COM$
 *     to the @@CONS command sender.
 *     The caller must send the reply.
 *
 *   - A return of 0 means that the reply has already been sent.
 */

/* Standard C header files */
#include <ertran.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <universal.h>
#include "marshal.h"

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "taskcodes.h" /* Include crosses the c-interface/server boundary */
#include "ConsoleCmds.h"
#include "DisplayCmds.h"
#include "NetworkAPI.h"
#include "Server.h"
#include "ServerConsol.h"
#include "ServerLog.h"
#include "ServiceUtils.h"

/* Imported data */

extern serverGlobalData sgd;
    /* The Server Global Data (SGD), visible to all Server activities. */

/* File level data */

static char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* a buffer for a generated message */

static char enumDecoding[50];
    /* used to decode an enum value into a string */

static char fullFileName[MAX_FILE_NAME_LEN+1];

static int globalLogIndicator;
static int globalToConsole;
static char * globalBufPtr;

static int displayConfigAllStatus;

#define BLANK_IP_ADDR_CHARS  " "  /* blank string used in display where no IP address is present. */

/**
 * Function processHelpCmd
 *
 * Process the console handler HELP command:
 *
 *   HELP
 *
 * This command displays a list of all of the console handler commands.
 *
 * Note that with the console display, we can only go out to column 72
 * in the C string (the Exec add two blanks to the front of the line).
 *
 * @param msg
 *   The @@CONS command text.
 *
 * @return
 *   A status:
 *     - 0: The console reply has already been sent.
 *     - 1: The caller calls replyToKeyin to print the reply in
 *          sgd.textMessage.
 */

int processHelpCmd(char * msg) {

    char * msg2;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    if (strcmp(msg,"HELP") != 0) {
        msg2 = getLocalizedMessageServer(CH_INVALID_HELP_CMD, NULL, NULL, 0, msgBuffer);
            /* 5248 Invalid HELP command */
        strcpy(sgd.textMessage, msg2);
        return 1;
    }

    strcpy(message,
        "A complete list of the console commands follows.");
    replyToKeyin(message);

    strcpy(message,
        "  braces {}: an optional field.");
    replyToKeyin(message);

    strcpy(message,
        "  vertical bar |: choose one from the list of choices.");
    replyToKeyin(message);

    strcpy(message,
        "  brackets []: an optional part of a keyword.");
    replyToKeyin(message);

#ifdef XABUILD /* XA Server */

    strcpy(message,
        "  lower case: user input.");
    replyToKeyin(message);

    strcpy(message,
        "CYCLE SERVER LOG|TRACE FILE        CYCLE CLIENT TRACE FILE");
    replyToKeyin(message);

    strcpy(message,
        "DISPLAY CONFIG[URATION] {ALL|PART 1|PART 2}");
    replyToKeyin(message);

    strcpy(message,
        "DISPLAY FILENAMES                  DISPLAY SERVER LEVEL");
    replyToKeyin(message);

    strcpy(message,
        "DISPLAY SERVER STATUS              DISPLAY WORKER STATUS {ALL|SHORT}");
    replyToKeyin(message);

    strcpy(message,
        "HELP");
    replyToKeyin(message);

    strcpy(message,
        "SET CLIENT TRACE FILE=filename     SET CLIENT DEBUG=INTERNAL|DETAIL|OFF");
    replyToKeyin(message);

    strcpy(message,
        "SET LOG CONSOLE OUTPUT=ON|OFF");
    replyToKeyin(message);

    strcpy(message,
        "SET SERVER DEBUG=n                 SET SERVER LOG|TRACE FILE=filename");
    replyToKeyin(message);

    strcpy(message,
        "CLEAR SERVER COUNTS");
    replyToKeyin(message);

    strcpy(message,
        "TURN ON|OFF SERVER TRACE           TURN ON|OFF CLIENT TRACE");
    replyToKeyin(message);


#else /* Local Server */

    strcpy(message,
        "  lower case: user input (id: client socket ID, tid: RDMS thread name).");
    replyToKeyin(message);

    strcpy(message,
        "CYCLE SERVER LOG|TRACE FILE        TURN ON|OFF SERVER TRACE");
    replyToKeyin(message);

    strcpy(message,
        "DISPLAY CONFIG[URATION] {ALL|PART 1|PART 2}");
    replyToKeyin(message);

    strcpy(message,
        "DISPLAY FILENAMES                  DISPLAY SERVER LEVEL");
    replyToKeyin(message);

    strcpy(message,
        "DISPLAY SERVER STATUS              DISPLAY WORKER COUNTS ");
    replyToKeyin(message);

    strcpy(message,
        "DISPLAY WORKER {id|tid|RDMS|IN RDMS|COMAPI|IN COMAPI} STATUS {ALL|SHORT}");
    replyToKeyin(message);

    strcpy(message,
        "HELP                               STATUS");
    replyToKeyin(message);

    strcpy(message,
        "SET COMAPI DEBUG=n                 SET LOG CONSOLE OUTPUT=ON|OFF");
    replyToKeyin(message);

    strcpy(message,
        "SET MAX ACTIVITIES=n               SET SERVER ACTIVITY RECEIVE TIMEOUT=n");
    replyToKeyin(message);

    strcpy(message,
        "SET SERVER DEBUG=n                 SET SERVER LOG|TRACE FILE=filename");
    replyToKeyin(message);

    strcpy(message,
        "SET SERVER RECEIVE|SEND TIMEOUT=n  CLEAR SERVER COUNTS");
    replyToKeyin(message);

    strcpy(message,
        "SHUTDOWN {GR[ACEFULLY]|IM[MEDIATELY]}");
    replyToKeyin(message);

    strcpy(message,
        "ABORT                              TERM {GR[ACEFULLY]|IM[MEDIATELY]}");
    replyToKeyin(message);

    strcpy(message,
        "ABORT WORKER id|tid");
    replyToKeyin(message);

    strcpy(message,
        "SHUTDOWN WORKER id|tid {GR[ACEFULLY]|IM[MEDIATELY]}");
    replyToKeyin(message);

#endif

    /* Return a zero, indicating that this routine already sent the
       reply output back to the @@CONS command sender
       via calls to replyToKeyin.
       */
    return 0;

} /* processHelpCmd */

/**
 * Function processDisplayCmd
 *
 * Process the console handler DISPLAY commands.
 *
 *   DISPLAY WORKER COUNTS             (not for XA Server)
 *   DISPLAY WORKER STATUS [ALL|SHORT]
 *   DISPLAY SERVER STATUS
 *   DISPLAY SERVER LEVEL
 *   DISPLAY WORKER [id|tid|[IN ]{RDMS|COMAPI}] STATUS [ALL|SHORT]    (not for XA Server)
 *   DISPLAY FILENAMES
 *
 *   DISPLAY CONFIGURATION
 *   DISPLAY CONFIGURATION ALL
 *   DISPLAY CONFIGURATION PART 1
 *   DISPLAY CONFIGURATION PART 2
 *
 *   DISPLAY CONFIG
 *   DISPLAY CONFIG ALL
 *   DISPLAY CONFIG PART 1
 *   DISPLAY CONFIG PART 2
 *
 * ALL causes all of the WDE or SGD variables to be dumped,
 * not just the variables relevant to the user.
 *
 * For the DISPLAY CONFIGURATION command, the 'PART 1' clause causes
 * the first set of the SGD variable list to printed,
 * while 'PART 2' causes the second set to be printed.
 *
 * @param msg
 *   The @@CONS command text.
 *
 * @return
 *   A status:
 *     - 0: The console reply has already been sent.
 *     - 1: The caller calls replyToKeyin to print the reply in
 *          sgd.textMessage.
 */

int processDisplayCmd(char * msg) {

    int cmdStatus = 0;
    char * msg2;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    if (strcmp(msg,"DISPLAY SERVER STATUS") == 0) {
        cmdStatus = processDisplayServerStatusCmd();
    } else if (strcmp(msg,"STATUS") == 0) {
        cmdStatus = processDisplayServerStatusCmd();
    } else if (strcmp(msg,"DISPLAY SERVER LEVEL") == 0) {
        cmdStatus = processDisplayServerLevelCmd();

#ifndef XABUILD /* Local Server */

    } else if (strcmp(msg,"DISPLAY WORKER COUNTS") == 0) {
        cmdStatus = processDisplayWorkerCountsCmd();

#endif /* Local Server */

    } else if (strcmp(msg,"DISPLAY FILENAMES") == 0) {
        cmdStatus = processDisplayFilenamesCmd();
    } else if (strcmp(msg,"DISPLAY CONFIGURATION ALL") == 0) {
        cmdStatus = processDisplayConfigurationAllCmd(0);
    } else if (strcmp(msg,"DISPLAY CONFIGURATION PART 1") == 0) {
        cmdStatus = processDisplayConfigurationAllCmd(1);
    } else if (strcmp(msg,"DISPLAY CONFIGURATION PART 2") == 0) {
        cmdStatus = processDisplayConfigurationAllCmd(2);
    } else if (strcmp(msg,"DISPLAY CONFIGURATION") == 0) {
        cmdStatus = processDisplayConfigurationCmd(FALSE, 0, TRUE, NULL);
    } else if (strcmp(msg,"DISPLAY CONFIG ALL") == 0) {
        cmdStatus = processDisplayConfigurationAllCmd(0);
    } else if (strcmp(msg,"DISPLAY CONFIG PART 1") == 0) {
        cmdStatus = processDisplayConfigurationAllCmd(1);
    } else if (strcmp(msg,"DISPLAY CONFIG PART 2") == 0) {
        cmdStatus = processDisplayConfigurationAllCmd(2);
    } else if (strcmp(msg,"DISPLAY CONFIG") == 0) {
        cmdStatus = processDisplayConfigurationCmd(FALSE, 0, TRUE, NULL);
    } else if (strncmp(msg,"DISPLAY WORKER", 14) == 0) {
        cmdStatus = processDisplayWorkerCmd(msg);
    } else {
        msg2 = getLocalizedMessageServer(CH_INVALID_DISPLAY_CMD, NULL, NULL,
            0, msgBuffer);
            /* 5210 Invalid DISPLAY command */
        strcpy(sgd.textMessage, msg2);
        cmdStatus = 1;
    }

    return cmdStatus;
} /* processDisplayCmd */

/**
 * Function processDisplayServerLevelCmd
 *
 * Process the command
 *   DISPLAY SERVER LEVEL
 *
 * This command dumps the server level as:
 *   <major level>.<minor level>.<feature ID>.<platform ID>
 *
 * Note that this is the same server level line that is displayed
 * in the DISPLAY CONFIGURATION ALL command.
 *
 * @return
 *   A status:
 *     - 0: The console reply has already been sent.
 *     - 1: The caller calls replyToKeyin to print the reply in
 *          sgd.textMessage.
 */

static int processDisplayServerLevelCmd() {

#ifndef XABUILD /* Local Server */

    if (sgd.server_LevelID[3] !=0){
      /* the platform level is not zero, display it also */
      sprintf(sgd.textMessage,
          "JDBC Server level(build):        %d.%d.%d.%d(%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2], sgd.server_LevelID[3],
          JDBCSERVER_BUILD_LEVEL_ID);
     }
     else {
      /* the platform level is zero, no need to display it */
      sprintf(sgd.textMessage,
          "JDBC Server level(build):        %d.%d.%d(%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2],
          JDBCSERVER_BUILD_LEVEL_ID);
     }

#else /* XA Server */

    if (sgd.server_LevelID[3] !=0){
      /* the platform level is not zero, display it also */
      sprintf(sgd.textMessage,
          "JDBC XA Server level(build):     %d.%d.%d.%d(%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2], sgd.server_LevelID[3],
          JDBCSERVER_BUILD_LEVEL_ID);
     }
     else {
      /* the platform level is zero, no need to display it */
      sprintf(sgd.textMessage,
          "JDBC XA Server level(build):     %d.%d.%d(%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2],
          JDBCSERVER_BUILD_LEVEL_ID);
      }

#endif /* XA Server */

    /* Return a status indicating that the text message has been set up */
    return 1;
}

#ifndef XABUILD /* Local Server */

/**
 * Function processDisplayWorkerCountsCmd
 *
 * Process the command:
 *   DISPLAY WORKER COUNTS             (not for XA Server)
 *
 * This command displays Server Worker counts:
 * free, assigned, and max.
 *
 * Display the following sgd values:
 *   - numberOfFreeServerWorkers
 *   - numberOfAssignedServerWorkers
 *   - maxServerWorkers
 *
 * @return
 *   A status:
 *     - 0: The console reply has already been sent.
 *     - 1: The caller calls replyToKeyin to print the reply in
 *          sgd.textMessage.
 */

static int processDisplayWorkerCountsCmd() {

    /* Get the info and display it */
    sprintf(sgd.textMessage,
        "Server workers: Free %d, Assigned %d, Max %d",
        sgd.numberOfFreeServerWorkers, sgd.numberOfAssignedServerWorkers,
        sgd.maxServerWorkers);

    /* Return a status indicating that the text message has been set up */
    return 1;
} /* processDisplayWorkerCountsCmd */

#endif /* Local Server */

/**
 * Function processDisplayServerStatusCmd
 *
 * Process the command:
 *   DISPLAY SERVER STATUS
 *
 * This command displays the following info:
 *   - server states
 *   - ICL states (not for the XA Server)
 *   - console handler states
 *   - log and trace filenames
 *   - free and active server worker counts (not for the XA Server)
 *
 * @return
 *   A status:
 *     - 0: The console reply has already been sent.
 *     - 1: The caller calls replyToKeyin to print the reply in
 *          sgd.textMessage.
 */

static int processDisplayServerStatusCmd() {

    int i2;
    int totalAlloc;
    char timeBuffer[30];
    char timeString[30];

#ifndef XABUILD /* Local Server */
    int disp_len1;
    int disp_len2;
    char ipInsert0[IP_ADDR_CHAR_LEN+1];
    char ipInsert1[IP_ADDR_CHAR_LEN+1];
    char ipInsert2[IP_ADDR_CHAR_LEN+1];
    char ipInsert3[IP_ADDR_CHAR_LEN+1];

#endif /* Local Server */

    /* Set up the lines of the @@CONS reply, and send them back to the
       @@CONS sender line by line. */

    strcpy(message, " ");
    replyToKeyin(message);
    sprintf(message,"JDBC Server:");
    replyToKeyin(message);

    /* serverStartTimeValue
     *
     * UCSCNVTIM$CC() returns date time in form : YYYYMMDDHHMMSSmmm . So,
     * Form date-time string of format: YYYYMMDD HH:MM:SS.SSS
     */
    UCSCNVTIM$CC(&sgd.serverStartTimeValue, timeBuffer);
    timeBuffer[17]='\0';
    timeString[0]='\0';
    strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
    strcat(timeString," ");
    strncat(timeString, &timeBuffer[8],2); /* copy the HH */
    strcat(timeString,":");
    strncat(timeString, &timeBuffer[10],2); /* copy the MM */
    strcat(timeString,":");
    strncat(timeString, &timeBuffer[12],2); /* copy the SS */
    strcat(timeString,".");
    strncat(timeString, &timeBuffer[14],3); /* copy the mmm */
    sprintf(message, "  start time:                   %s", timeString);
    replyToKeyin(message);

    /* serverLevel */
#ifndef XABUILD /* Local Server */

    if (sgd.server_LevelID[3] !=0){
      /* the platform level is not zero, display it also */
      sprintf(message,
          "  Server level (build):         %d.%d.%d.%d (%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2], sgd.server_LevelID[3],
          JDBCSERVER_BUILD_LEVEL_ID);
     }
     else {
      /* the platform level is zero, no need to display it */
      sprintf(message,
          "  level (build):                %d.%d.%d (%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2],
          JDBCSERVER_BUILD_LEVEL_ID);
     }

#else /* XA Server */

    if (sgd.server_LevelID[3] !=0){
      /* the platform level is not zero, display it also */
      sprintf(message,
          "  XA Server level (build):      %d.%d.%d.%d (%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2], sgd.server_LevelID[3],
          JDBCSERVER_BUILD_LEVEL_ID);
     }
     else {
      /* the platform level is zero, no need to display it */
      sprintf(message,
          "  XA Server level (build):      %d.%d.%d (%s)",
          sgd.server_LevelID[0], sgd.server_LevelID[1],
          sgd.server_LevelID[2],
          JDBCSERVER_BUILD_LEVEL_ID);
     }

#endif /* XA Server */


    /* display server level */
    replyToKeyin(message);

    /* serverState */
    getServerState(sgd.serverState, enumDecoding);
    sprintf(message, "  state, App Group(#):          %s, %s(%d)%s",
            enumDecoding, sgd.appGroupName, sgd.appGroupNumber,
            (sgd.FFlag_SupportsFetchToBuffer == 1) ? "" : " (MRF off)");
    replyToKeyin(message);

#ifdef XABUILD /* XA Server */
    /* clientCount, non-XA (stateful), XA, reuse counts */
    sprintf(message, "  total clients (s,x,r):        %d (%d,%d,%d)", sgd.clientCount,
            sgd.totalStatefulThreads, sgd.totalXAthreads, sgd.totalXAReuses);
    replyToKeyin(message);
#else /* Local Server */
    /* clientCount */
    sprintf(message, "  total clients:                %d", sgd.clientCount);
    replyToKeyin(message);
#endif /* XA Server */

    /* requestCount */
    sprintf(message, "  total requests processed:     %d", sgd.requestCount);
    replyToKeyin(message);

    /* lastRequestTimeStamp */
    strcpy(message, "  last request timestamp:       ");
    if (sgd.lastRequestTimeStamp == 0) {
        strcat(message, "none");
    } else {
        /*
         * UCSCNVTIM$CC() returns date time in form : YYYYMMDDHHMMSSmmm . So,
         * Form date-time string of format: YYYYMMDD HH:MM:SS.SSS
         */
        UCSCNVTIM$CC(&sgd.lastRequestTimeStamp, timeBuffer);
        timeBuffer[17]='\0';
        timeString[0]='\0';
        strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
        strcat(timeString," ");
        strncat(timeString, &timeBuffer[8],2); /* copy the HH */
        strcat(timeString,":");
        strncat(timeString, &timeBuffer[10],2); /* copy the MM */
        strcat(timeString,":");
        strncat(timeString, &timeBuffer[12],2); /* copy the SS */
        strcat(timeString,".");
        strncat(timeString, &timeBuffer[14],3); /* copy the mmm */
        strcat(message, timeString);
    }
    replyToKeyin(message);

    /* lastRequestTaskCode */
    if (sgd.lastRequestTaskCode == 0) {
        sprintf(message, "  last request task code:       none");
    } else {
        sprintf(message, "  last request task code:       %d (%s)",
            sgd.lastRequestTaskCode, taskCodeString(sgd.lastRequestTaskCode));
    }
    replyToKeyin(message);


#ifndef XABUILD /* Local Server */

    /* max worker count */
    sprintf(message, "  max workers (activities):     %d",
        sgd.maxServerWorkers);
    replyToKeyin(message);

    /* free worker count */
    if (sgd.numberOfShutdownServerWorkers == 0){
        sprintf(message, "  total workers free, assigned: %d, %d",
               sgd.numberOfFreeServerWorkers, sgd.numberOfAssignedServerWorkers);
    } else {
        sprintf(message, "  total workers free, assigned: %d, %d, shutdown: %d",
               sgd.numberOfFreeServerWorkers, sgd.numberOfAssignedServerWorkers, sgd.numberOfShutdownServerWorkers);
    }
    replyToKeyin(message);

#endif /* Local Server */

    /* ServerWorker malloc memory usage.
     *
     * Get sum of total malloc memory being currently
     * used by free/assigned ServerWorkers. Loop through
     * the WDE array and find them. Remember, this is
     * just a quick snapshot in time - we are not locking
     * the free or allocated chains to get this value.
     */
    totalAlloc = 0;
    for (i2 = 0; i2 < sgd.numWdeAllocatedSoFar; i2++) {
        if (sgd.wdeArea[i2].serverWorkerState == CLIENT_ASSIGNED ||
            sgd.wdeArea[i2].serverWorkerState == FREE_NO_CLIENT_ASSIGNED) {
            /* We found an active or free worker, add in their current malloc usage. */
            totalAlloc += sgd.wdeArea[i2].totalAllocSpace;
        }
    }
    sprintf(message, "  total workers malloc words:   %d", totalAlloc);
    replyToKeyin(message);

#ifndef XABUILD /* Local Server */

    /* ICLState */
    for (i2=0; i2<sgd.num_COMAPI_modes; i2++) {
        sprintf(message,"COMAPI mode %s connection ICL[%d]:",
                sgd.ICLcomapi_mode[i2], i2);
        replyToKeyin(message);

        getICLState(sgd.ICLState[i2], enumDecoding);
        sprintf(message, "  state:                        %s",
                enumDecoding);
        replyToKeyin(message);

       /* ICLcomapi_state */
        getICLCOMAPIState(sgd.ICLcomapi_state[i2], enumDecoding);
        sprintf(message, "  interface status:             %s",
                enumDecoding);
        replyToKeyin(message);

        /* ICL assigned server workers */
        sprintf(message, "  workers assigned (mode %s):    %d",
                sgd.ICLcomapi_mode[i2],
                sgd.ICLAssignedServerWorkers[i2]);
        replyToKeyin(message);

        /* ICLcomapi_mode and ICLcomapi_bdi */
        sprintf(message, "  port, BDI:                    %d, %07o",
                sgd.localHostPortNumber, sgd.ICLcomapi_bdi[i2]);
        replyToKeyin(message);

        /*
         * Get IP address strings for up to 4 server sockets
         * (MAX_SERVER_SOCKETS). Test if we have a socket, if
         * so, get its IP address.
         */
        if (sgd.server_socket[i2][0] !=0){
            /* Get IP address as a string. */
            IP_to_Char(ipInsert0, sgd.listen_ip_addresses[i2][0]);
        }
        else {
            /* No socket,use blank string instead*/
            strcpy(ipInsert0,BLANK_IP_ADDR_CHARS);
        }
        if (sgd.server_socket[i2][1] !=0){
            /* Get IP address as a string. */
            IP_to_Char(ipInsert1, sgd.listen_ip_addresses[i2][1]);
        }
        else {
            /* No socket,use blank string instead*/
            strcpy(ipInsert1,BLANK_IP_ADDR_CHARS);
        }
        if (sgd.server_socket[i2][2] !=0){
            /* Get IP address as a string. */
            IP_to_Char(ipInsert2, sgd.listen_ip_addresses[i2][2]);}
        else {
            /* No socket,use blank string instead*/
            strcpy(ipInsert2,BLANK_IP_ADDR_CHARS);
        }
        if (sgd.server_socket[i2][3] !=0){
            /* Get IP address as a string. */
            IP_to_Char(ipInsert3, sgd.listen_ip_addresses[i2][3]);}
        else {
            /* No socket,use blank string instead*/
            strcpy(ipInsert3,BLANK_IP_ADDR_CHARS);
        }

        /* server_sockets */
        /* Display the first two server sockets always. */
        sprintf(message, "  socket (CPCOMM,CMS):          (%d, %d)",
                sgd.server_socket[i2][0],  sgd.server_socket[i2][1]);
        replyToKeyin(message);

        /* server_sockets IP addresses Display first two server sockets. */
        disp_len1 = strlen(ipInsert0) + strlen(ipInsert1);
        disp_len2 = strlen(ipInsert2) + strlen(ipInsert3);
        /* If the IP addresses won't fit in the alloted space, display one
         * IP address per line instead of two and shift the field to the left,
         * if necessary, to fit on the line.
         */
        if (disp_len1 > 36 || (disp_len2 > 36 && sgd.num_server_sockets == MAX_SERVER_SOCKETS)) {
            /* Display one IP address per line. */
            if (strlen(ipInsert0) > 38) { /* IP address is too long, move to the left. */
                sprintf(message, "  socket IP address 1:   (%s)", ipInsert0);
                replyToKeyin(message);
            } else {
                sprintf(message, "  socket IP address 1:          (%s)", ipInsert0);
                replyToKeyin(message);
            }
            if (strlen(ipInsert1) > 38) {  /* IP address is too long, move to the left. */
                sprintf(message, "  socket IP address 2:   (%s)", ipInsert1);
                replyToKeyin(message);
            } else {
                sprintf(message, "  socket IP address 2:          (%s)", ipInsert1);
                replyToKeyin(message);
            }
        } else { /* Display two IP addresses per line. */
            sprintf(message, "  socket IP addresses:          (%s, %s)",
                    ipInsert0, ipInsert1);
            replyToKeyin(message);
        }

        if (sgd.num_server_sockets == MAX_SERVER_SOCKETS){
            /*
            * All four (MAX_SERVER_SOCKETS )server sockets to
            * display, so display the third and fourth ones.
            */
            sprintf(message, "  socket (CPCOMM,CMS):          (%d, %d)",
                  sgd.server_socket[i2][2],  sgd.server_socket[i2][3]);
            replyToKeyin(message);
            /* If the IP addresses won't fit in the alloted space, display one per line
             * and shift the field to the left, if necessary, to fit on the line.
             */
            if (disp_len1 > 36 || disp_len2 > 36) {
                if (strlen(ipInsert2) > 38) { /* IP address is too long, move to the left. */
                    sprintf(message, "  socket IP address 3:   (%s)", ipInsert2);
                    replyToKeyin(message);
                } else {
                    sprintf(message, "  socket IP address 3:          (%s)", ipInsert2);
                    replyToKeyin(message);
                }
                if (strlen(ipInsert3) > 38) { /* IP address is too long, move to the left. */
                    sprintf(message, "  socket IP address 4:   (%s)", ipInsert3);
                    replyToKeyin(message);
                } else {
                    sprintf(message, "  socket IP address 4:          (%s)", ipInsert3);
                    replyToKeyin(message);
                }
            } else { /* Display two IP addresses per line. */
                sprintf(message, "  socket IP addresses:          (%s, %s)",
                        ipInsert2,  ipInsert3);
                replyToKeyin(message);
            }
        }
    }

#endif /* Local Server */


    /* Return a zero, indicating that this routine already sent the
       reply output back to the @@CONS command sender
       via calls to replyToKeyin.
       */
    return 0;

} /* processDisplayServerStatusCmd */

/**
 * displayAllActiveWorkersInLog
 *
 * Display info about all active workers in the log file.
 *
 * @param logIndicator
 *   Mask bits that indicate where to place the output
 *   (server log file, stdout, server trace file, client trace file)
 *   and the format of the lines that are output to those sources.
 *   This is the logIndicator used as an argument to function
 *   addServerLogEntry.
 *
 * @param displayAll
 *   If TRUE, display all of the WDE (worker description entry) variables.
 *   If FALSE, display only the WDE variables relevant to the user.
 */

void displayAllActiveWorkersInLog(int logIndicator, int displayAll) {

    int i1;
    int firstTime = TRUE;
    int nbrActiveWorkers = 0;

    /* Loop through the WDE array - look for active workers */
    for (i1 = 0; i1<sgd.numWdeAllocatedSoFar; i1++) {

        if (sgd.wdeArea[i1].serverWorkerState == CLIENT_ASSIGNED) {

            /* We found an active worker. */
            nbrActiveWorkers++;

            if (firstTime) {
                firstTime = FALSE;

#ifndef XABUILD /* Local Server */

                addServerLogEntry(
                    logIndicator, "\nSTATUS OF THE ACTIVE SERVER WORKERS");

#endif /* Local Server */

            }

            /* Display (not in Short format) relevant info about the active worker */
            displayActiveWorkerInfo(logIndicator, FALSE, &sgd.wdeArea[i1],
                displayAll, FALSE);
        }
    }

#ifndef XABUILD /* Local Server */

    /* List number of active workers */
    sprintf(message, "server workers active:           %d", nbrActiveWorkers);
    addServerLogEntry(logIndicator, message);

#endif /* Local Server */

} /* displayAllActiveWorkersInLog */

/**
 * processDisplayConfigurationAllCmd
 *
 * Display all sgd variables.
 *
 * @param allStatus
 *   A status indicating what to display:
 *     0: all sgd variables
 *          DISPLAY CONFIGURATION ALL
 *     1: first part of the sgd variables only
 *          DISPLAY CONFIGURATION PART 1
 *     2: second part of the sgd variables only
 *          DISPLAY CONFIGURATION PART 2
 *
 * @return
 *   0 is currently always returned.
 *   This indicates that the output has already been sent out
 *   (console, log file, etc.),
 *   and that the caller need not send any output.
 */

static int processDisplayConfigurationAllCmd(int allStatus) {

    int cmdStatus;

    displayConfigAllStatus = allStatus;

    cmdStatus = processDisplayConfigurationCmd(TRUE, 0, TRUE, NULL);

    return 0;

} /* processDisplayConfigurationAllCmd */

/**
 * Function processDisplayConfigurationCmd
 *
 * Command: DISPLAY CONFIGURATION
 *
 *   Command to display the configuration values.
 *
 * @param displayAll
 *   If TRUE, display all of the sgd (server global data) variables,
 *   not just the changeable ones.
 *   If FALSE, display only the changeable sgd variables.
 *
 * @param logIndicator
 *   Mask bits that indicate where to place the output
 *   (server log file, stdout, server trace file, client trace file)
 *   and the format of the lines that are output to those sources.
 *   This is the logIndicator used as an argument to function
 *   addServerLogEntry.
 *
 * @param toConsole
 *   This flag is TRUE if the output is for the Console Handler
 *   (i.e., it is returned to the sender of the @@CONS command).
 *
 * @param wdePtr
 *   A pointer to a WDE entry.
 *   If not NULL, and if the logIndicator has the client trace file
 *   mask bit set, output is sent to the proper client trace file.
 *
 * @return
 *   0 is currently always returned.
 *   This indicates that the output has already been sent out
 *   (console, log file, etc.),
 *   and that the caller need not send any output.
 */

/* If we add or subtract lines from the config display,
   modify the following value.
   This is the number of lines that is output for the command:
   DISPLAY CONFIGURATION ALL
   (not including the repeating four lines for COMAPI usage values).
   There is a slack size of 5 added for safety reasons,
   to ensure that we allocate enough storage.
   */
#define CONFIG_LINES 71

/* Make this 80 to ensure enough storage is allocated */
#define CHARS_PER_CONFIG_LINE 80

/* Number of repeating lines displayed for COMAPI usage */
#define COMAPI_USAGE_LINES 4

int processDisplayConfigurationCmd(int displayAll, int logIndicator,
                                   int toConsole,
                                   workerDescriptionEntry * wdePtr) {

    char timeBuffer[30];
    char timeString[30];
    char * bufPtr;
    char msgInsert[12];
    char localLocale[MAX_LOCALE_LEN+1];
    int displayAll1;
    int displayAll2;
    int displayUnlessCase2;
    char optionLetters[27];
    char debugValue[9]; /* OFF, DETAIL, or INTERNAL */

    /*
     * Allocate a buffer large enough to hold all the generated lines
     * to be output to the one or more files if the logIndicator is
     * nonzero, because we don't send one line at a time in that case
     * and we don't want interleaved output in a log or trace file.
     *
     * Since we don't know how many modes of COMAPI will be used at
     * runtime, we assume the worst case of 26 modes of COMAPI. With
     * the current defined constant values, buffer size is 14000 bytes.
     */
    char msgBuffer[CHARS_PER_CONFIG_LINE * (CONFIG_LINES +
                (26 * COMAPI_USAGE_LINES))];

#ifndef XABUILD /* Local Server */

    int i2;

#endif /* Local Server */

#define FILE_NAME_LEN 12

    char UASFMQual[FILE_NAME_LEN+1];
    char UASFMFile[FILE_NAME_LEN+1];

    typedef union {
        struct {
            short sh1;
            short sh2;
        } st1;
        int i1;
    } unionIntShort;

    unionIntShort u1;

    /* Set file level data for the sendOutput function */
    globalLogIndicator = logIndicator;
    globalToConsole = toConsole;

    /*
     * Are lines to be output to one or more files
     * (i.e, if the logIndicator is nonzero)?
     */
    if (logIndicator != 0) {
        bufPtr = msgBuffer;
        bufPtr[0] = '\0';
        globalBufPtr = bufPtr;
    }

    /* Initialize display all flags.
       Each variable should be under a test for
       displayAll1, displayAll2, or displayUnlessCase2.

       Note that for each of the CH commands
       'DISPLAY CONFIGURATION PART 1' and 'DISPLAY CONFIGURATION PART 2',
       we try to dump about half of the SGD variables.

       There are five cases:

       1. If this function is called from another file,
       then we have a non-console command case where all variables are
       displayed in a log or trace file.
       displayAll1, displayAll2, and displayUnlessCase2 are all TRUE
       in that case.

       2. For the 'DISPLAY CONFIGURATION' console handler command,
       displayAll1 and displayAll2 are both FALSE,
       and displayUnlessCase2 is TRUE.

       3. For the 'DISPLAY CONFIGURATION ALL' console handler command,
       displayAll1, displayAll2, and displayUnlessCase2 are all TRUE.

       4. For the 'DISPLAY CONFIGURATION PART 1' console handler command,
       displayAll1 is TRUE, displayAll2 is FALSE,
       and displayUnlessCase2 is TRUE.

       5. For the 'DISPLAY CONFIGURATION PART 2' console handler command,
       displayAll1 is FALSE, displayAll2 is TRUE,
       and displayUnlessCase2 is FALSE.
       */

    displayAll1 = (displayAll
        && (!toConsole
            || (toConsole
                && ((displayConfigAllStatus==0)
                    || (displayConfigAllStatus==1)))));

    displayAll2 = (displayAll
        && (!toConsole
            || (toConsole
                && ((displayConfigAllStatus==0)
                    || (displayConfigAllStatus==2)))));

    displayUnlessCase2 = !toConsole
        || (toConsole
            && (!displayAll
                || (displayAll
                    && (displayConfigAllStatus!=2))));

    /* Heading */
    if (displayAll) {
        strcpy(message, "SERVER GLOBAL DATA VALUES");
    } else {
        strcpy(message, "OPERATIONAL PARAMETER VALUES");
    }

    sendOutput();

    /* server version */
    if (displayAll1) {
      if (sgd.server_LevelID[3] !=0){
        /* the platform level is not zero, display it also */
        sprintf(message,
            "server level(build):             %d.%d.%d.%d(%s)",
            sgd.server_LevelID[0], sgd.server_LevelID[1],
            sgd.server_LevelID[2], sgd.server_LevelID[3],
            JDBCSERVER_BUILD_LEVEL_ID);
      }
      else {
        /* the platform level is zero, no need to display it */
        sprintf(message,
            "server level(build):             %d.%d.%d(%s)",
            sgd.server_LevelID[0], sgd.server_LevelID[1],
            sgd.server_LevelID[2],
            JDBCSERVER_BUILD_LEVEL_ID);
      }
        sendOutput();

    /* rdmsLevelID */
        sprintf(message,
            "RDMS level:                      %s",
            sgd.rdmsLevelID);
        sendOutput();
    }

#ifdef XABUILD /* XA Server */

    /* serverName */
    if (displayUnlessCase2) {
        sprintf(message,
            "server name:                     \"%s\" (generated run-id %s)",
            sgd.serverName, sgd.generatedRunID);
        sendOutput();
    }

#else /* Local Server */

    /* serverName */
    if (displayUnlessCase2) {
        sprintf(message, "server name:                     \"%s\"",
            sgd.serverName);
        sendOutput();
    }

#endif

    /* serverStartTimeValue */
    if (displayAll1) {
        /*
         * UCSCNVTIM$CC() returns date time in form : YYYYMMDDHHMMSSmmm . So,
         * Form date-time string of format: YYYYMMDD HH:MM:SS.SSS
         */
        UCSCNVTIM$CC(&sgd.serverStartTimeValue, timeBuffer);
        timeBuffer[17]='\0';
        timeString[0]='\0';
        strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
        strcat(timeString," ");
        strncat(timeString, &timeBuffer[8],2); /* copy the HH */
        strcat(timeString,":");
        strncat(timeString, &timeBuffer[10],2); /* copy the MM */
        strcat(timeString,":");
        strncat(timeString, &timeBuffer[12],2); /* copy the SS */
        strcat(timeString,".");
        strncat(timeString, &timeBuffer[14],3); /* copy the mmm */
        sprintf(message, "server start time:               %s", timeString);
        sendOutput();

    /* option letters */
        getServerOptionLetters(optionLetters);
        sprintf(message, "processor option letters:        %s", optionLetters);
        sendOutput();
    }

#ifndef XABUILD /* Local Server */

    /* maxServerWorkers */
    if (displayUnlessCase2) {
        sprintf(message, "max activities:                  %d",
            sgd.maxServerWorkers);
        sendOutput();

    /* maxCOMAPIQueueSize */
        sprintf(message, "max queued COMAPI:               %d",
            sgd.maxCOMAPIQueueSize);
        sendOutput();
    }

    /* firstFree_WdeTS */
    if (displayAll1) {
        sprintf(message, "first free WDE T/S:              %012o",
            sgd.firstFree_WdeTS);
        sendOutput();

    /* numberOfFreeServerWorkers */
        sprintf(message, "number of free workers:          %d",
            sgd.numberOfFreeServerWorkers);
        sendOutput();

    /* firstFree_wdePtr */
        sprintf(message, "first free WDE pointer:          %p",
            sgd.firstFree_wdePtr);
        sendOutput();

    /* firstAssigned_WdeTS */
        sprintf(message, "first assigned WDE T/S:          %012o",
            sgd.firstAssigned_WdeTS);
        sendOutput();
    }

    /* numberOfAssignedServerWorkers */
    if (displayAll2) {
        sprintf(message, "number of assigned workers:      %d",
            sgd.numberOfAssignedServerWorkers);
        sendOutput();

    /* firstAssigned_wdePtr */
        sprintf(message, "first assigned WDE pointer:      %p",
            sgd.firstAssigned_wdePtr);
        sendOutput();
    }

#endif /* Local Server */

    /* appGroupName */
    if (displayUnlessCase2) {
        sprintf(message, "app group name (number):         %s (%d)",
            sgd.appGroupName, sgd.appGroupNumber);
        sendOutput();
    }

    /* rdms_threadname_prefix */
    if (displayUnlessCase2) {
        sprintf(message, "rdms_threadname_prefix:          %s",
            sgd.rdms_threadname_prefix);
        sendOutput();
    }

#ifndef XABUILD /* Local Server */

    /* localHostPortNumber */
    if (displayUnlessCase2) {
        sprintf(message, "host port:                       %d",
            sgd.localHostPortNumber);
        sendOutput();
    }

#endif /* Local Server */

    /* serverState */
    if (displayAll2) {
        getServerState(sgd.serverState, enumDecoding);
        sprintf(message, "server state:                    %s", enumDecoding);
        sendOutput();

    /* serverShutdownState */
        getServerShutdownState(sgd.serverShutdownState, enumDecoding);
        sprintf(message, "server shutdown state:           %s", enumDecoding);
        sendOutput();

#ifndef XABUILD /* Local Server */

    /* num_COMAPI_modes */
        sprintf(message, "number of COMAPI modes:          %d",
            sgd.num_COMAPI_modes);
        sendOutput();

    /* ICL_handling_SW_CH_shutdown */
        sprintf(message, "ICL handling SW/CH shutdown:     %d", sgd.ICL_handling_SW_CH_shutdown);
        sendOutput();

    /* ICLState */
        for (i2=0; i2<sgd.num_COMAPI_modes; i2++) {
            getICLState(sgd.ICLState[i2], enumDecoding);
            sprintf(message, "ICL[%d] state:                    %s", i2,
                enumDecoding);
            sendOutput();

    /* ICLShutdownState */
            getICLShutdownState(sgd.ICLShutdownState[i2], enumDecoding);
            sprintf(message, "ICL[%d] shutdown state:           %s", i2,
                enumDecoding);
            sendOutput();

    /* ICLcomapi_mode and ICLcomapi_bdi */
            sprintf(message, "ICL[%d] COMAPI mode and BDI:      %s, %07o",
                i2, sgd.ICLcomapi_mode[i2], sgd.ICLcomapi_bdi[i2]);
            sendOutput();

    /* server_socket */
            if (sgd.num_server_sockets == 2){
              /* Only two server sockets to display. */
              sprintf(message, "ICL[%d] Svr. sockets (CPCOMM,CMS): (%d, %d)",
                    i2, sgd.server_socket[i2][0],  sgd.server_socket[i2][1]);
            }else{
              /* All four server sockets to display. */
              sprintf(message, "ICL[%d] Svr. sockets (CPCOMM,CMS): (%d, %d)",
                    i2, sgd.server_socket[i2][0],  sgd.server_socket[i2][1]);
              sendOutput();
              sprintf(message, "ICL[%d] Svr. socket (CPCOMM,CMS): (%d, %d)",
                    i2, sgd.server_socket[i2][2],  sgd.server_socket[i2][3]);
            }
            sendOutput();
        }

    /* server_listens_on */
            sprintf(message, "server listens on:               %s",
                    sgd.config_host_names);
            sendOutput();

    /* client_keep_alive */
            getClientKeepAlive(sgd.config_client_keepAlive, enumDecoding);
            sprintf(message, "client keep alive:               %s",
                    enumDecoding);
            sendOutput();

#endif /* Local Server */

    /* consoleHandlerState */
        getConsoleHandlerState(sgd.consoleHandlerState, enumDecoding);
        sprintf(message, "console handler state:           %s", enumDecoding);
        sendOutput();

    /* consoleHandlerShutdownState */
        getConsoleHandlerShutdownState(sgd.consoleHandlerShutdownState,
            enumDecoding);
        sprintf(message, "console handler shutdown state:  %s", enumDecoding);
        sendOutput();
    }

#ifndef XABUILD /* Local Server */

    /* server_receive_timeout */
    if (displayUnlessCase2) {
        sprintf(message, "server receive timeout:          %d milliseconds",
            sgd.server_receive_timeout);
        sendOutput();

    /* server_send_timeout */
        sprintf(message, "server send timeout:             %d milliseconds",
            sgd.server_send_timeout);
        sendOutput();

    /* server_activity_receive_timeout */
        sprintf(message, "server activity receive timeout: %d milliseconds",
            sgd.server_activity_receive_timeout);
        sendOutput();
    }

#endif /* Local Server */

    /* fetch_block_size */
    if (displayAll1) {
        sprintf(message, "fetch block size:                %d",
            sgd.fetch_block_size);
        sendOutput();
    }

    /* serverLogFileTS */
    if (displayAll2) {
        sprintf(message, "server log file T/S:             %012o",
            sgd.serverLogFileTS);
        sendOutput();

    /* msgFileTS */
        sprintf(message, "message file T/S:                %012o",
            sgd.msgFileTS);
        sendOutput();

    /* serverLogFile */
        sprintf(message, "server log file pointer:         %p",
            sgd.serverLogFile);
        sendOutput();
    }

    /* serverLogFileName */
    if (displayUnlessCase2) {
        if (sgd.serverLogFile == NULL) {
            strcpy(fullFileName,"none");
        } else {
            getFullFileName(LOG_FILE_USE_NAME, fullFileName, TRUE);
        }

        sprintf(message, "server log file:                 %s", fullFileName);
        sendOutput();
    }

    /* serverTraceFileTS */
    if (displayAll2) {
        sprintf(message, "server trace file T/S:           %012o",
            sgd.serverTraceFileTS);
        sendOutput();

    /* serverTraceFile */
        sprintf(message, "server trace file pointer:       %p",
            sgd.serverTraceFile);
        sendOutput();
    }

    /* serverTraceFileName */
    if (displayUnlessCase2) {
        msgInsert[0] = '\0';

        if (sgd.serverTraceFileName == NULL) {
            strcpy(fullFileName,"none");
        } else if ((strcmp(sgd.serverTraceFileName, PRINT$_FILE) == 0)){
            strcpy(fullFileName, PRINT$_FILE); /* PRINT$ is its own full name */
        } else {
            getFullFileName(TRACE_FILE_USE_NAME, fullFileName, TRUE);

            if (sgd.serverTraceFile == NULL) {
                strcpy(msgInsert,"(trace off)");
            }
        }

        sprintf(message, "server trace file:               %s %s",
            fullFileName, msgInsert);
        sendOutput();

    /* serverMessageFileName */
        sprintf(message, "server message element:          %s",
            sgd.serverMessageFileName);
        sendOutput();
    }

    /* serverMessageFile */
    if (displayAll2) {
        sprintf(message, "server message element FILE ptr.:%p",
            sgd.serverMessageFile);
        sendOutput();
    }

    /* serverLocale */
    if (displayUnlessCase2) {
        if (strlen(sgd.serverLocale) == 0) {
            strcpy(localLocale, "none");
        } else {
            strcpy(localLocale, sgd.serverLocale);
        }

        sprintf(message, "server locale:                   %s", localLocale);
        sendOutput();
    }

    /* debug */
    if (displayAll2) {
        sprintf(message, "server debug:                    %d",
            sgd.debug);
        sendOutput();

    /* debugInternal */
        sprintf(message, "client debug internal:           %s",
            getTrueFalse(sgd.clientDebugInternal));
        sendOutput();

    /* debugDetail */
        sprintf(message, "client debug detail:             %s",
            getTrueFalse(sgd.clientDebugDetail));
        sendOutput();

    /* generatedRunID */
        sprintf(message, "generated run ID:                %s",
            sgd.generatedRunID);
        sendOutput();

    /* originalRunID */
        sprintf(message, "original run ID:                 %s",
            sgd.originalRunID);
        sendOutput();

    /* serverUserID */
        sprintf(message, "server user ID:                  %s",
            sgd.serverUserID);
        sendOutput();

    /* keyinReturnPktAddr */
        sprintf(message, "keyin return packet pointer:     %p",
            sgd.keyinReturnPktAddr);
        sendOutput();

    /* clientTraceFileCounter */
        sprintf(message, "client trace file counter:       %d",
            sgd.clientTraceFileCounter);
        sendOutput();
    }

    /* defaultClientTraceFileQualifier */
    if (displayUnlessCase2) {
        sprintf(message, "def. client trace file qualifier:%s",
            sgd.defaultClientTraceFileQualifier);
        sendOutput();
    }

    /* clientTraceFileTracks */
    if (displayUnlessCase2) {
        sprintf(message, "client trace file max tracks:    %i",
            sgd.clientTraceFileTracks);
        sendOutput();

    /* clientTraceFileCycles */
        sprintf(message, "client trace file max cycles:    %i",
            sgd.clientTraceFileCycles);
        sendOutput();
    }

    if (displayAll2) {

#ifndef XABUILD /* Local Server */

    /* tempMaxNumWde */
        sprintf(message, "max number of WDE entries:       %d",
            sgd.tempMaxNumWde);
        sendOutput();

    /* numWdeAllocatedSoFar */
        sprintf(message, "number WDEs allocated so far:    %d",
            sgd.numWdeAllocatedSoFar);
        sendOutput();

#endif /* Local Server */

    }

    /* rsaBdi */
    if (displayUnlessCase2) {
        sprintf(message, "RSA BDI:                         %07o",
            sgd.rsaBdi);
        sendOutput();

#ifdef XABUILD /* XA Server */

    /* uds_icr_bdi */
        sprintf(message, "UDS ICR BDI:                     %07o",
            sgd.uds_icr_bdi);
        sendOutput();

#endif /* XA Server */

    }

#ifndef XABUILD /* Local Server */

    /* postedServerReceiveTimeout */
    if (displayAll2) {
        sprintf(message, "posted server receive timeout:   %d milliseconds",
            sgd.postedServerReceiveTimeout);
        sendOutput();

    /* postedServerSendTimeout */
        sprintf(message, "posted server send timeout:      %d milliseconds",
            sgd.postedServerSendTimeout);
        sendOutput();
    }

    /* COMAPIDebug */
    if (displayUnlessCase2) {
        sprintf(message, "COMAPI debug:                    %d",
            sgd.COMAPIDebug);
        sendOutput();
    }

#endif /* Local Server */

    /* logConsoleOutput */
    if (displayUnlessCase2) {
        getLogConsoleOutput(sgd.logConsoleOutput, enumDecoding);
        sprintf(message, "log console output:              %s", enumDecoding);
        sendOutput();
    }

#ifndef XABUILD /* Local Server */

    /* configKeyinID */
    if (displayUnlessCase2) {
        sprintf(message, "keyin ID (config. parameter):    %s",
            sgd.configKeyinID);
        sendOutput();
    }

#endif /* Local Server */

#ifdef XABUILD /* XA Server */

    /* keyinName */
    if (displayUnlessCase2) {
        sprintf(message, "keyin name (for console cmds.):  %s (generated run-id)",
            sgd.keyinName);
        sendOutput();
    }

#else /* Local Server */

    /* keyinName */
    if (displayUnlessCase2) {
        sprintf(message, "keyin name (for console cmds.):  %s",
            sgd.keyinName);
        sendOutput();
    }

#endif

    /* user_access_control */
    if (displayUnlessCase2) {
        getUserAccessControl(sgd.user_access_control, enumDecoding);
        sprintf(message, "user access control:             %s", enumDecoding);
        sendOutput();
    }

    /* user_access_file_name
     *
     * The format of the string is a 12 character qualifier (blank filled),
     * followed by a 12 character filename (blank filled). Strip off the
     * blanks, then print as qual*file. If the SGD variable is a null string,
     * don't print a value (leave it blank).
     */
    if (displayUnlessCase2) {
        if (strlen(sgd.user_access_file_name) == 0) {
            sprintf(message, "user access file name:");

        } else {
            UASFMQual[0] = '\0';
            UASFMFile[0] = '\0';
            strncat(UASFMQual, sgd.user_access_file_name, FILE_NAME_LEN);
            strncat(UASFMFile, sgd.user_access_file_name+FILE_NAME_LEN, FILE_NAME_LEN);
            trimTrailingBlanks(UASFMQual);
            trimTrailingBlanks(UASFMFile);

            sprintf(message, "user access file name:           %s*%s",
                UASFMQual, UASFMFile);
        }

        sendOutput();
    }

    /* server_priority */
    if (displayUnlessCase2) {
        sprintf(message, "server priority:                 %s", sgd.server_priority);
        sendOutput();
    }

#ifndef XABUILD /* Local Server */

    /* comapi_modes */
    if (displayUnlessCase2) {
        sprintf(message, "COMAPI modes:                    %s",
            sgd.comapi_modes);
        sendOutput();
    }

#endif /* Local Server */

    /* client debug: This is set by the SET CLIENT DEBUG command */
    if (displayUnlessCase2) {
        if ((sgd.clientDebugInternal == TRUE)
            && (sgd.clientDebugDetail == TRUE)) {

            strcpy(debugValue, "INTERNAL");

        } else if ((sgd.clientDebugInternal == FALSE)
            && (sgd.clientDebugDetail == TRUE)) {

            strcpy(debugValue, "DETAIL");

        } else if ((sgd.clientDebugInternal == FALSE)
            && (sgd.clientDebugDetail == FALSE)) {

            strcpy(debugValue, "OFF");
        }

        sprintf(message, "client debug:                    %s", debugValue);
        sendOutput();
    }

    /* UASFM_State */
    if (displayAll2) {
        getUASFMState(sgd.UASFM_State, enumDecoding);
        sprintf(message, "user access security act. state: %s", enumDecoding);
        sendOutput();

    /* UASFM_ShutdownState */
        getUASFMShutdownState(sgd.UASFM_ShutdownState, enumDecoding);
        sprintf(message, "UA security act. shutdown state: %s", enumDecoding);
        sendOutput();

    /* SUVAL_bad_error_statuses */
        u1.i1 = sgd.SUVAL_bad_error_statuses;
          /* print as two 18 bit octal values */
        sprintf(message, "SUVAL bad error statuses:        %06o %06o",
            u1.st1.sh1, u1.st1.sh2);
        sendOutput();
    }

    /* which_MSCON_li_pkt */
    if (displayAll1) {
        sprintf(message, "MSCON lead item packet number:   %d",
            sgd.which_MSCON_li_pkt);
        sendOutput();

    /* MSCON_li_status */
        sprintf(message, "MSCON lead item packet statuses: %012o %012o",
            sgd.MSCON_li_status[0], sgd.MSCON_li_status[1]);
        sendOutput();
    }

#ifndef XABUILD /* Local Server */

    /* postedCOMAPIDebug */
    if (displayAll2) {
        sprintf(message, "posted COMAPI debug value:       %d",
            sgd.postedCOMAPIDebug);
        sendOutput();
    }

#endif /* Local Server */

    /* Send output buffer as one message,
       for the non-console case.
       If the wdePtr argument is not NULL,
       we call a different function. */
    if (logIndicator != 0) {

        if (wdePtr != NULL) {
            addClientTraceEntry(wdePtr, globalLogIndicator, bufPtr);
        } else {
            addServerLogEntry(globalLogIndicator, bufPtr);
        }
    }

    return 0;

} /* processDisplayConfigurationCmd */

/**
 * Function sendOutput
 *
 * Utility used (by processDisplayConfigurationCmd
 * and displayActiveWorkerInfo) to output lines.
 *
 * Where the text in message is sent depends on the file level variables
 * globalLogIndicator and globalToConsole
 * (set by the caller).
 *
 * If the flag globalToConsole is TRUE,
 * send the one line of output (in message) to the console.
 *
 * If the globalLogIndicator value is nonzero,
 * send the one line of output (in message) to a buffer
 * (pointed to by globalBufPtr).
 * All of the lines in the buffer will eventually be sent as one stream
 * (by the caller) to the proper files (log / trace) or stdout.
 */

static void sendOutput() {

    if (globalToConsole == TRUE) {
        replyToKeyin(message);
    }

    if (globalLogIndicator != 0) {
        strcat(globalBufPtr, message);
        strcat(globalBufPtr, "\n"); /* end of line */
    }
} /* sendOutput */

/**
 * Function processDisplayWorkerCmd
 *
 * Commands:
 *   DISPLAY WORKER [id|tid|[ON ]{RDMS|COMAPI}] STATUS [ALL|SHORT]    (not for XA Server)
 *   DISPLAY WORKER STATUS [ALL|SHORT]
 *
 * Command to display the status of an individual server worker
 * (if id is specified) or all server workers.
 * ALL causes all WDE variables to be dumped
 * (not just variables relevant to the user).
 *
 * id (if specified) is the server worker socket ID.
 * This is in H1 (the upper 18 bits) in the WDE's socket ID word
 * (client_socket), specified in decimal.
 *
 * tid (if specified) is the server worker RDMS thread name.
 * This is in the WDE's threadname character array.
 *
 * To get all active server workers:
 * Just run down the all_wdePtr chain.
 * Use the number of assigned workers to prevent runaway.
 *
 * @param msg
 *   The @@CONS command text.
 *
 * @return
 *   A status:
 *     - 0: The console reply has already been sent.
 *     - 1: The caller calls replyToKeyin to print the reply in
 *          sgd.textMessage.
 */

static int processDisplayWorkerCmd(char * msg) {

#ifndef XABUILD /* Local Server */

    char tid[80]; /* large enough for thread name (and even the whole CH command). */
    char workerID_or_tid[80];  /* also make large enough */
    int workerID;
    int status;
    int count;
    workerDescriptionEntry *wdeptr;
    char insert[TRAILING_STRING_SIZE];
    char insert2[TRAILING_STRING_SIZE];

    /* The following is used to detect garbage following what looks
       like legal command syntax.
       If that case is detected, the command is ignored. */
    char trail[TRAILING_STRING_SIZE];

#endif /* Local Server */

    int displayAll = FALSE;
    int displayShort = FALSE;
    int inSubSys=0; /* assume "IN RDMS" or "IN COMAPI" is not specified */
    int allWorkers = FALSE;
    int nbrActiveWorkers = 0;
    int firstTime = TRUE;
    int stillFirstTime = TRUE;
    int i1;
    char * msg2;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Scan the string */

    if (strcmp(msg, "DISPLAY WORKER STATUS ALL") == 0) {
        displayAll = TRUE;
        allWorkers = TRUE;
        displayShort = FALSE;
        inSubSys = 0;  /* select worker whether in RDMS, COMAPI, or not */

    } else if (strcmp(msg, "DISPLAY WORKER STATUS SHORT") == 0) {
        displayAll = FALSE;
        allWorkers = TRUE;
        displayShort = TRUE;
        inSubSys = 0;  /* select worker whether in RDMS, COMAPI, or not */

    } else if (strcmp(msg, "DISPLAY WORKER STATUS") == 0) {
        displayAll = FALSE;
        allWorkers = TRUE;
        displayShort = FALSE;
        inSubSys = 0;  /* select worker whether in RDMS, COMAPI, or not */

    } else if ((strcmp(msg, "DISPLAY WORKER RDMS STATUS ALL") == 0)||
               (strcmp(msg, "DISPLAY WORKER IN RDMS STATUS ALL") == 0)  ) {
        displayAll = TRUE;
        allWorkers = TRUE;
        displayShort = FALSE;
        inSubSys = 1;  /* select workers if they are currently in RDMS */

    } else if ((strcmp(msg, "DISPLAY WORKER RDMS STATUS SHORT") == 0)||
               (strcmp(msg, "DISPLAY WORKER IN RDMS STATUS SHORT") == 0)  ) {
        displayAll = FALSE;
        allWorkers = TRUE;
        displayShort = TRUE;
        inSubSys = 1;  /* select workers if they are currently in RDMS */

    } else if ((strcmp(msg, "DISPLAY WORKER RDMS STATUS") == 0)||
               (strcmp(msg, "DISPLAY WORKER IN RDMS STATUS") == 0)  ){
        displayAll = FALSE;
        allWorkers = TRUE;
        displayShort = FALSE;
        inSubSys = 1;  /* select workers if they are currently in RDMS */

    } else if ((strcmp(msg, "DISPLAY WORKER COMAPI STATUS ALL") == 0)||
               (strcmp(msg, "DISPLAY WORKER IN COMAPI STATUS ALL") == 0)  ) {
        displayAll = TRUE;
        allWorkers = TRUE;
        displayShort = FALSE;
        inSubSys = 2;  /* select workers if they are currently in COMAPI */

    } else if ((strcmp(msg, "DISPLAY WORKER COMAPI STATUS SHORT") == 0)||
               (strcmp(msg, "DISPLAY WORKER IN COMAPI STATUS SHORT") == 0)  ) {
        displayAll = FALSE;
        allWorkers = TRUE;
        displayShort = TRUE;
        inSubSys = 2;  /* select workers if they are currently in COMAPI */

    } else if ((strcmp(msg, "DISPLAY WORKER COMAPI STATUS") == 0)||
               (strcmp(msg, "DISPLAY WORKER IN COMAPI STATUS") == 0)  ){
        displayAll = FALSE;
        allWorkers = TRUE;
        displayShort = FALSE;
        inSubSys = 2;  /* select workers if they are currently in COMAPI */
    }

    if (allWorkers) {

    /* DISPLAY WORKER STATUS and DISPLAY WORKER STATUS ALL.
       Go through the active workers and send a set of reply lines
       for each one */

        for (i1 = 0; i1<sgd.numWdeAllocatedSoFar; i1++) {

            if (sgd.wdeArea[i1].serverWorkerState == CLIENT_ASSIGNED) {
                if ((inSubSys == 1) && (sgd.wdeArea[i1].in_RDMS == NOT_CALLING_RDMS)){
                     continue; /* this worker is not in RDMS, don't display */
                }
                if ((inSubSys == 2) && (sgd.wdeArea[i1].in_COMAPI == NOT_CALLING_COMAPI)){
                     continue; /* this worker is not in COMAPI, don't display */
                }
                /* We found an active worker to display. */
                nbrActiveWorkers++;

                 /* Check if the first time through loop. Set indicators accordingly. */
                 stillFirstTime = firstTime;
                 if (firstTime) {
                     firstTime = FALSE;

#ifndef XABUILD /* Local Server */

                    strcpy(message, " ");
                    replyToKeyin(message);

                    if (inSubSys == 1){
                        strcpy(message, "STATUS OF THE ACTIVE SERVER WORKERS IN RDMS");
                    } else if (inSubSys == 2){
                        strcpy(message, "STATUS OF THE ACTIVE SERVER WORKERS IN COMAPI");
                    } else {
                        strcpy(message, "STATUS OF THE ACTIVE SERVER WORKERS");
                    }
                    replyToKeyin(message);

#endif /* Local Server */

                }

                /*
                 * If SHORT display form, output the header line once right
                 * here before the displayActiveWorkerInfo() call.
                 */
                if (displayShort && stillFirstTime) {
                  strcpy(message, "SHUT  ID     THREAD IP-ADDRESS      FIRST-PACKET       LAST-PACKET");

                  /*
                   * Set indicators to sendOutput function to match what
                   * the displayActiveWorkerInfo() call will have, and
                   * then send the output.
                   */
                  globalLogIndicator = 0;
                  globalToConsole = TRUE;
                  sendOutput();
                }

                /* Display relevant info about the active worker */
                displayActiveWorkerInfo(0, TRUE, &sgd.wdeArea[i1], displayAll, displayShort);
            }
        }

#ifndef XABUILD /* Local Server */

        /* List the number of active workers */
        sprintf(message, "server workers active:           %d", nbrActiveWorkers);
        replyToKeyin(message);

#endif /* Local Server */

        return 0;
    } /* end of code for DISPLAY WORKER STATUS */

#ifdef XABUILD /* XA Server */

    /* DISPLAY WORKER id|tid STATUS [ALL|SHORT]
       is not allowed for the XA Server */

    msg2 = getLocalizedMessageServer(CH_INVALID_DISPLAY_WORKER_CMD,
        NULL, NULL, 0, msgBuffer);
        /* 5211 Invalid DISPLAY WORKER command */
    strcpy(sgd.textMessage, msg2);
    return 1;

#else /* Local Server */

    count = sscanf(msg, "DISPLAY WORKER %s %s %s %s", &tid, &insert,
        &insert2, &trail);

    if ((count == 2) || (count == 3)) {

        if (strcmp(insert, "STATUS") != 0) {
            msg2 = getLocalizedMessageServer(CH_INVALID_DISPLAY_WORKER_CMD,
                NULL, NULL, 0, msgBuffer);
                /* 5211 Invalid DISPLAY WORKER command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

        if (count == 3) {

            if (strcmp(insert2, "ALL") == 0) {
                displayAll = TRUE; /* DISPLAY WORKER id|tid STATUS ALL */
            } else if (strcmp(insert2, "SHORT") == 0){
                displayShort = TRUE; /* DISPLAY WORKER id|tid STATUS SHORT */
            } else {
                msg2 = getLocalizedMessageServer(CH_INVALID_DISPLAY_WORKER_CMD,
                    NULL, NULL, 0, msgBuffer);
                    /* 5211 Invalid DISPLAY WORKER command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }
        }

    } else {
        msg2 = getLocalizedMessageServer(CH_INVALID_DISPLAY_WORKER_CMD,
            NULL, NULL, 0, msgBuffer);
            /* 5211 Invalid DISPLAY WORKER command */
        strcpy(sgd.textMessage, msg2);
        return 1;
    }

    /* Determine if the tid is a socket id (id) or a thread name (tid) */
    strcpy(workerID_or_tid,"socket ID "); /* assume socket id in message */
    if (sscanf(tid,"%d%s",&workerID, &trail) != 1){
      /* Not a socket id (id), assume its a thread name, find its workerID */
      findWorker_by_RDMS_threadname(tid, &workerID);
      strcpy(workerID_or_tid,"RDMS thread name "); /* assume thread name in message*/
    }

    /* Test the socket id (id) */
    if ((workerID <= 0)
        && (workerID != magicSocketID)) {
            /* Special negative constant causes the first client
               with a non-zero socket ID to be picked.
               This is for development only,
               and is not documented for users.  */
        msg2 = getLocalizedMessageServer(CH_INVALID_WORKER_ID,
            workerID_or_tid, tid, 0, msgBuffer);
            /* 5203 Client {0} {1} not found */
        strcpy(sgd.textMessage, msg2);
        return 1;
    }

    /* We have valid command syntax.
       Now check for a valid worker ID.
       This is in H1 (the upper 18 bits) in the WDE's socket ID word
       (client_socket), specified in decimal. */
    status = findWorker(&workerID, &wdeptr);

    if (status == 0) {
        /* We found the worker.
           Call a function to display its info.
           Return 0.
           */
           /*
            * If SHORT display form, output the header line once right
            * here before the displayActiveWorkerInfo() call.
            */
           if (displayShort) {
             strcpy(message, " ");
             replyToKeyin(message);
             strcpy(message, "SHUT  ID     THREAD IP-ADDRESS      FIRST-PACKET       LAST-PACKET");

             /*
              * Set indicators to sendOutput function to match what
              * the displayActiveWorkerInfo() call will have, and
              * then send the output.
              */
             globalLogIndicator = 0;
             globalToConsole = TRUE;
             sendOutput();
           }
        displayActiveWorkerInfo(0, TRUE, wdeptr, displayAll, displayShort);
        return 0;
    }

    /* We did not find the worker.
       Set up an error and return.
       */
    msg2 = getLocalizedMessageServer(CH_INVALID_WORKER_ID,
        workerID_or_tid, tid, 0, msgBuffer);
        /* 5203 Client {0} {1} not found */
    strcpy(sgd.textMessage, msg2);
    return 1;

#endif

} /* processDisplayWorkerCmd */

/**
 * Function displayActiveWorkerInfo
 *
 * Display info about a server worker.
 * Set up individual strings (each one line of output),
 * and print them via calls to sendOutput.
 *
 * @param logIndicator
 *   Mask bits that indicate where to place the output
 *   (typically the client trace file for this case)
 *   and the format of the lines that are output to those sources.
 *   This is the logIndicator used as an argument to function
 *   addClientTraceEntry.
 *
 * @param toConsole
 *   This flag is TRUE if the output is for the Console Handler
 *   (i.e., it is returned to the sender of the @@CONS command).
 *
 * @param wdePtr
 *   A pointer to a worker description entry.
 *
 * @param displayAll
 *   If TRUE, display all of the WDE (worker description entry) variables.
 *   If FALSE, display only the WDE variables relevant to the user.
 */

/* If we add or subtract lines from the config display,
   modify the following value.
   */
#define WORKER_LINES 41

/* All worker lines are currently 80 chars or less */
#define CHARS_PER_WORKER_LINE 80

void displayActiveWorkerInfo(int logIndicator, int toConsole,
                             workerDescriptionEntry * wdePtr,
                              int displayAll, int displayShort) {

    char msgInsert2[50];
    char msgInsert[12];
    char clientTraceFileInternalName[MAX_INTERNAL_FILE_NAME_LEN+1];
    char * bufPtr;
    char localLocale[MAX_LOCALE_LEN+1];
    char timeString[30];
    char timeBuffer[30];

#ifndef XABUILD /* Local Server */

    char char_ip_address[IP_ADDR_CHAR_LEN+1];
    char hostname[HOST_NAME_MAX_LEN+1];
    int error_status;

#endif /* Local Server */

    /*
     * Allocate a buffer large enough to hold all the generated lines
     * to be output to the one or more files if the logIndicator is
     * nonzero, because we don't send one line at a time in that case
     * and we don't want interleaved output in a log or trace file.
     *
     * With the current defined constant values, buffer size is 3280 bytes.
     */
    char msgBuffer[WORKER_LINES * CHARS_PER_WORKER_LINE];

    /* Check for NULL WDE pointer */
    if (wdePtr == NULL) {
        return;
    }

    /* Set file level data for the sendOutput function */
    globalLogIndicator = logIndicator;
    globalToConsole = toConsole;

    /*
     * Are lines to be output to one or more files
     * (i.e, if the logIndicator is nonzero)?
     */
    if (logIndicator != 0) {
      bufPtr = msgBuffer;
      bufPtr[0] = '\0';
      globalBufPtr = bufPtr;
    }

    /* Initialization */
    msgInsert[0] = '\0';

    /* Set up lines of the reply, and send them one by one. */

    /* If its a SHORT display, compose the single line of output here. */
    if (displayShort){
      /* Form the single display image */
      message[0]='\0';
      if (wdePtr->serverWorkerShutdownState == WORKER_ACTIVE){
        strcat(message,"AC   ");
      }else if (wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_GRACEFULLY){
        strcat(message,"GR   ");
      }else if (wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_IMMEDIATELY){
        strcat(message,"IM   ");
      }else {
        strcat(message,"     "); /* Use this as an unknown default */
      }

      if (wdePtr->in_COMAPI == CALLING_COMAPI){
        /* We are currently inside COMAPI (or OTM in XA JDBC Server case) */
        strcat(message,"*");
      }else {
        /* We are not currently inside COMAPI (or OTM in XA JDBC Server case) */
        strcat(message," ");
      }
      sprintf(msgInsert2, "%d%", getInternalSocketID(wdePtr->client_socket));
      strncat(msgInsert2,"       ", 6-strlen(msgInsert2)); /* pad out to 6 characters */
      strcat(message,msgInsert2);

      if (wdePtr->in_RDMS == CALLING_RDMS){
        /* We are currently inside UDS/RDMS/RSA */
        strcat(message,"*");
      }else {
        /* We are not currently inside UDS/RDMS/RSA */
        strcat(message," ");
      }
      strncat(message, wdePtr->threadName, VISIBLE_RDMS_THREAD_NAME_LEN); /* transfer the visible first 6 characters
                                                                             of RDMS thread name ( 7th and 8th chars are blanks) */
      strcat(message," "); /* pad out to 8 characters */

#ifndef XABUILD /* Local Server */
      IP_to_Char(msgInsert2, wdePtr->client_IP_addr);
      /* If the IP address won't fit in the 15 character column, change the output format. */
      if (strlen(msgInsert2) > 15) {
          /* Output the full IP address on this line, but move to the next line for the
           * two date-time strings in order to maintain their correct column indentation.
           */
          strcat(message,msgInsert2);
          sendOutput();
          strcpy(message,"                                    ");
      } else {
          strncat(msgInsert2,"            ", 16-strlen(msgInsert2)); /* pad out to 16 characters */
          strcat(message,msgInsert2);
      }
#else /* XA Server */
      strcat(message,"<not available> "); /* XA client's IP address is not known */
#endif /* XA Server */

      /* Form two date-time strings in format: MMDD HH:MM:SS.SSS. Use a
       * string "none" if the time value has not been set yet (its still
       * set to 0).
       */
      if (wdePtr->firstRequestTimeStamp == 0){
        strcat(message, "none               "); /* use none and blanks */
      }else{
        UCSCNVTIM$CC(&(wdePtr->firstRequestTimeStamp), timeBuffer);
        timeBuffer[17]='\0';
        strncat(message, &timeBuffer[4],4);   /* copy the MMDD */
        strcat(message," ");
        strncat(message, &timeBuffer[8],2); /* copy the HH */
        strcat(message,":");
        strncat(message, &timeBuffer[10],2); /* copy the MM */
        strcat(message,":");
        strncat(message, &timeBuffer[12],2); /* copy the SS */
        strcat(message,".");
        strncat(message, &timeBuffer[14],3); /* copy the SSS */
        strcat(message," ");
      }

      if (wdePtr->taskCode == CONN_KEEP_ALIVE_TASK){
        /* Client's last Request packet was a keep alive packet */
        strcat(message,"*");
      }else {
        /* Client's last Request packet was not a keep alive packet */
        strcat(message," ");
      }
      if (wdePtr->lastRequestTimeStamp == 0){
        strcat(message, "none             "); /* use none and blanks */
      }else{
        UCSCNVTIM$CC(&(wdePtr->lastRequestTimeStamp), timeBuffer);
        timeBuffer[17]='\0';
        strncat(message, &timeBuffer[4],4);   /* copy the MMDD */
        strcat(message," ");
        strncat(message, &timeBuffer[8],2); /* copy the HH */
        strcat(message,":");
        strncat(message, &timeBuffer[10],2); /* copy the MM */
        strcat(message,":");
        strncat(message, &timeBuffer[12],2); /* copy the SS */
        strcat(message,".");
        strncat(message, &timeBuffer[14],3); /* copy the SSS */
      }
      sendOutput();

      /* Send output buffer as one message to the client trace file,
         for the non-console case.
       */
      if (logIndicator != 0) {
          strcat(bufPtr,"\n");  /* Make sure the header line gets displayed on its own line. */
          addClientTraceEntry(wdePtr, globalLogIndicator, bufPtr);
      }
      return; /* all done */
    }

#ifndef XABUILD /* Local Server */

    /* Header line */
    if (logIndicator != 0) {
        strcpy(message, "SERVER WORKER STATUS");
        sendOutput();
    }

    /* client_socket.
       Note: Always display the internal socket ID,
       since it is used as the ID in various console commands.
       This is in H1 (the upper 18 bits) in the WDE's socket ID word
       (client_socket), printed in decimal. */
    sprintf(message, "  server worker socket ID (id):  %d",
        getInternalSocketID(wdePtr->client_socket));
    sendOutput();

    /* Display the entire client_socket word in octal.
       There are two parts (H1 and H2) in this word;
       that's why we are displaying it in octal. */
    sprintf(message, "  client socket value:           %012o",
        wdePtr->client_socket);
    sendOutput();

    /* serverWorkerActivityID's, both 36-bit and 72-bit */
    sprintf(message, "  server worker activity IDs:    %012o (%024llo)",
        wdePtr->serverWorkerActivityID, wdePtr->serverWorkerUniqueActivityID);
    sendOutput();

#endif /* Local Server */

    /* clientDriverLevel major level, minor level, feature level, platform level(build level)*/
    if (wdePtr->clientDriverLevel[3] !=0){
      /* the platform level is not zero, display it also */
      sprintf(message, "  client JDBC driver level:      %d.%d.%d.%d(%s)",
         wdePtr->clientDriverLevel[0], wdePtr->clientDriverLevel[1], wdePtr->clientDriverLevel[2],
         wdePtr->clientDriverLevel[3], wdePtr->clientDriverBuildLevel);
    }
    else{
      /* the platform level is zero, no need to display it */
      sprintf(message, "  client JDBC driver level:      %d.%d.%d(%s)",
         wdePtr->clientDriverLevel[0], wdePtr->clientDriverLevel[1], wdePtr->clientDriverLevel[2],
         wdePtr->clientDriverBuildLevel);
    }
    sendOutput();

    /* ownerActivity */
    if (displayAll) {
        getOwnerActivityType(wdePtr->ownerActivity, enumDecoding);
        sprintf(message, "  owner activity type:           %s", enumDecoding);
        sendOutput();

#ifndef XABUILD /* Local Server */

    /* serverWorkerTS */
        sprintf(message, "  server worker T/S:             %012o",
            wdePtr->serverWorkerTS);
        sendOutput();

    /* free_wdePtr_next */
        sprintf(message, "  next free WDE pointer:         %p",
            wdePtr->free_wdePtr_next);
        sendOutput();

    /* assigned_wdePtr_back */
        sprintf(message, "  previous assigned WDE pointer: %p",
            wdePtr->assigned_wdePtr_back);
        sendOutput();

    /* assigned_wdePtr_next */
        sprintf(message, "  next assigned WDE pointer:     %p",
            wdePtr->assigned_wdePtr_next);
        sendOutput();

#endif /* Local Server */

    }

    /* serverWorkerState */
    getServerWorkerState(wdePtr->serverWorkerState, enumDecoding);
    sprintf(message, "  server worker state:           %s", enumDecoding);
    sendOutput();

    /* serverWorkerShutdownState */
    getServerWorkerShutdownState(wdePtr->serverWorkerShutdownState,
        enumDecoding);
    sprintf(message, "  server worker shutdown state:  %s", enumDecoding);
    sendOutput();

    /* totalAllocSpace */
    sprintf(message, "  server worker malloc words:    %d", wdePtr->totalAllocSpace);
    sendOutput();

#ifndef XABUILD /* Local Server */

    /* comapi_bdi */
    getNetworkAdapterName(wdePtr->network_interface, enumDecoding);
    sprintf(message, "  COMAPI mode, Network, ICL num: %s, %s, %d",
            sgd.ICLcomapi_mode[wdePtr->ICL_num], enumDecoding, wdePtr->ICL_num);
    sendOutput();

    /* client_IP_addr - IP address display*/
    IP_to_Char(char_ip_address, wdePtr->client_IP_addr);

    /* If IP address is too long for the 72 character limit, move it left so it fits. */
    if (strlen(char_ip_address) < 40) {
        sprintf(message, "  client IP address:             %s", char_ip_address);
    } else {
        sprintf(message, "  client IP address:       %s", char_ip_address);
    }
    sendOutput();

    /* client_IP_addr - client host name display*/
    /*
     *  Get the Client's host name. In this case, ignore the error status
     *  since if host name cannot be found a standard default value will
     *  be displayed.
     */
    error_status = Get_Hostname(hostname, wdePtr->client_IP_addr, wdePtr);
    sprintf(message, "  client IP host name:           %s", hostname);
    sendOutput();

    /* client_keep_alive */
    getClientKeepAlive(wdePtr->client_keepAlive, enumDecoding);
    sprintf(message, "  client keep alive:             %s", enumDecoding);
    sendOutput();

    /* client_receive_timeout */
    sprintf(message, "  client receive timeout:        %d milliseconds",
        wdePtr->client_receive_timeout);
    sendOutput();

#endif /* Local Server */

    /* fetch_block_size */
    sprintf(message, "  fetch block size:              %d",
        wdePtr->fetch_block_size);
    sendOutput();

    /* clientName - removed, its not used*/
    /* sprintf(message, "  client name:                   %s",
        wdePtr->clientName);
    sendOutput(); */

    /* serverCalledVia */
    if (displayAll) {
        getServerCalledViaStates(wdePtr->serverCalledVia, enumDecoding);
        sprintf(message, "  server called via:             %s", enumDecoding);
        sendOutput();
    }

    /* workingOnaClient */
    sprintf(message, "  working on a client:           %s",
        getTrueFalse(wdePtr->workingOnaClient));
    sendOutput();

    /* openRdmsThread */
    sprintf(message, "  RDMS Thread open:              %s",
        getTrueFalse(wdePtr->openRdmsThread));
    sendOutput();

    /* threadName */
    sprintf(message, "  RDMS Thread name (tid):        %s (user id: %s)",
            wdePtr->threadName, wdePtr->client_Userid);
    sendOutput();

    if (displayAll) {

#ifndef XABUILD /* Local Server */

    /* standingRequestPacketPtr */
        sprintf(message, "  standing request packet ptr.:  %p",
            wdePtr->standingRequestPacketPtr);
        sendOutput();

    /* requestPacketPtr */
        sprintf(message, "  request packet pointer:        %p",
            wdePtr->requestPacketPtr);
        sendOutput();

    /* releasedResponsePacketPtr */
        sprintf(message, "  released response packet ptr.: %p",
            wdePtr->releasedResponsePacketPtr);
        sendOutput();

#endif /* Local Server */

    }
    /* firstRequestTimeStamp */
        /* Form two date-time strings in format: YYMMDD HH:MM:SS.SSS. Use a
         * string "none" if the time value has not been set yet (its still
         * set to 0).
         */
        timeString[0]='\0';
        if (wdePtr->firstRequestTimeStamp == 0){
          strcpy(timeString, "none"); /* use none and blanks */
        }else{
          /* Form date-time string of format: YYYYMMDD HH:MM:SS.SSS */
          UCSCNVTIM$CC(&(wdePtr->firstRequestTimeStamp), timeBuffer);
          timeBuffer[17]='\0';
          strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
          strcat(timeString," ");
          strncat(timeString, &timeBuffer[8],2); /* copy the HH */
          strcat(timeString,":");
          strncat(timeString, &timeBuffer[10],2); /* copy the MM */
          strcat(timeString,":");
          strncat(timeString, &timeBuffer[12],2); /* copy the SS */
          strcat(timeString,".");
          strncat(timeString, &timeBuffer[14],3); /* copy the SSS */
        }
        message[0]='\0';
        sprintf(message, "  first request timestamp:       %s", timeString);
        sendOutput();

    /* lastRequestTimeStamp */
        /* Form two date-time strings in format: YYMMDD HH:MM:SS.SSS. Use a
         * string "none" if the time value has not been set yet (its still
         * set to 0).
         */
        timeString[0]='\0';
        if (wdePtr->lastRequestTimeStamp == 0){
          strcpy(timeString, "none"); /* use none and blanks */
        }else{
          UCSCNVTIM$CC(&(wdePtr->lastRequestTimeStamp), timeBuffer);
          timeBuffer[17]='\0';
          strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
          strcat(timeString," ");
          strncat(timeString, &timeBuffer[8],2); /* copy the HH */
          strcat(timeString,":");
          strncat(timeString, &timeBuffer[10],2); /* copy the MM */
          strcat(timeString,":");
          strncat(timeString, &timeBuffer[12],2); /* copy the SS */
          strcat(timeString,".");
          strncat(timeString, &timeBuffer[14],3); /* copy the SSS */
        }
        message[0]='\0';
        sprintf(message, "  last request timestamp:        %s", timeString);
        sendOutput();

    /* taskCode */
        sprintf(message, "  task code:                     %d (%s)",
            wdePtr->taskCode, taskCodeString(wdePtr->taskCode));
        sendOutput();

    if (displayAll) {
    /* debug */
        sprintf(message, "  debug:                         %d", wdePtr->debug);
        sendOutput();

    /* debugFlags */
        sprintf(message, "  debug flags:                   %012o",
            wdePtr->debugFlags);
        sendOutput();

    /* clientTraceFileNbr */
        sprintf(message, "  client trace file number:      %d",
            wdePtr->clientTraceFileNbr);
        sendOutput();
    }

    /* clientTraceFileName.

       Note that the client trace file may or may not exist,
       and if it does exist, it may or may not be open
       (because of the 'TURN OFF CLIENT id TRACE' command
       in the Console Handler).
       */
    if ((strlen(wdePtr->clientTraceFileName) == 0)
           || (wdePtr->clientTraceFileNbr == 0)) {
        strcpy(fullFileName, "none");
    } else if ((strcmp(wdePtr->clientTraceFileName, PRINT$_FILE) == 0)){
        strcpy(fullFileName, PRINT$_FILE); /* PRINT$ is its own full name */
    } else {
        sprintf(clientTraceFileInternalName, "%s%d",
            CLIENT_TRACE_FILE_USE_NAME_PREFIX, wdePtr->clientTraceFileNbr);
        getFullFileName(clientTraceFileInternalName, fullFileName, TRUE);

        if (wdePtr->clientTraceFile == NULL) {
            strcpy(msgInsert,"(trace off)");
        }
    }

    sprintf(message, "  client trace file:             %s %s", fullFileName, msgInsert);
    sendOutput();

    /* clientTraceFile */
    if (displayAll) {
        sprintf(message, "  client trace file pointer:     %p",
            wdePtr->clientTraceFile);
        sendOutput();
    }

    /* workerMessageFileName */
    sprintf(message, "  worker message element:        %s",
        wdePtr->workerMessageFileName);
    sendOutput();

    /* workerMessageFile */
    if (displayAll) {
        sprintf(message, "  worker message file pointer:   %p",
            wdePtr->workerMessageFile);
        sendOutput();
    }

    /* clientLocale */
    if (strlen(wdePtr->clientLocale) == 0) {
        strcpy(localLocale, "none");
    } else {
        strcpy(localLocale, wdePtr->clientLocale);
    }

    sprintf(message, "  client locale:                 %s", localLocale);
    sendOutput();

    /* serverMessageFile */
    if (displayAll) {
        sprintf(message, "  server message file pointer:   %p",
            wdePtr->serverMessageFile);
        sendOutput();

    /* consoleSetClientTraceFile */
        sprintf(message, "  console set client trace file: %s",
            getTrueFalse(wdePtr->consoleSetClientTraceFile));
        sendOutput();
    }

    /* Send output buffer as one message to the client trace file,
       for the non-console case.
       */
    if (logIndicator != 0) {
        addClientTraceEntry(wdePtr, globalLogIndicator, bufPtr);
    }

} /* displayActiveWorkerInfo */

/*
 * Function getClientInfoTraceImages
 *
 * This routine will prepare a trace image containing the current Server Worker
 * data area, aka, the WDE.
 */
void getClientInfoTraceImages(char * messageBuffer,workerDescriptionEntry * wdePtr) {

    char message[500];         /* Buffer used to contruct one trace image */
    char enumDecoding[50];     /* used to decode an enum value into a string */
    char msgInsert[MAX_RDMS_LEVEL_LEN+1]; /* Big enough for a filename or the RDMS Level string */
    char clientTraceFileInternalName[MAX_INTERNAL_FILE_NAME_LEN+1];
    char localLocale[MAX_LOCALE_LEN+1];
    char timeString[30];
    char timeBuffer[30];
    char serverKind[6];
    char serverKindPad[6];

    /*
     * Allocate large enough space to hold the constructed client/server
     * instance string and anything else that may be added after it.
     */
    char messagePiece[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

#ifndef XABUILD /* Local Server */

    char char_ip_address[IP_ADDR_CHAR_LEN+1];
    char hostname[HOST_NAME_MAX_LEN+1];
    int error_status;

    strcpy(serverKind, "");        /* JDBC Server, no insert needed in trace lines */
    strcpy(serverKindPad, "   ");  /* need 3 padding blanks */
#else           /* XA Server */
    strcpy(serverKind, "XA ");     /* XA JDBC Server, so insert is needed in trace lines */
    strcpy(serverKindPad, "");     /* need no padding blanks */
#endif

    /* Generate the first trace line with the header image. */
    strcpy(messageBuffer,"**In JDBC ");
    strcat(messageBuffer,serverKind);
    strcat(messageBuffer,"Server \"");
    strcat(messageBuffer,sgd.serverName );
    strcat(messageBuffer,"\" at ");
    addDateTime(messageBuffer);
    strcat(messageBuffer,"\n  client userid:                 ");
    strcat(messageBuffer,wdePtr->client_Userid);
    strcat(messageBuffer,"\n");

#ifndef XABUILD /* Local Server */
    /* serverExecutableName */
    sprintf(message,   "  JDBC Server's executable name: %s\n", sgd.serverExecutableName);
    strcat(messageBuffer,message);
#else           /* XA Server */
    /* serverExecutableName */
    sprintf(message,   "  JDBC XA Server's executable name: %s\n", sgd.serverExecutableName);
    strcat(messageBuffer,message);

    /* xaServerNumber */
    sprintf(message,   "  JDBC XA Server's server number: %d\n", sgd.xaServerNumber);
    strcat(messageBuffer,message);
#endif

    /* Original and generated runid's */
    sprintf(message,   "  Original/generated runid:      %s/%s\n", sgd.originalRunID, sgd.generatedRunID);
    strcat(messageBuffer,message);

    /* clientDriverLevel major level, minor level, feature level, platform level(build level)*/
    if (wdePtr->clientDriverLevel[3] !=0){
      /* the platform level is not zero, display it also */
      sprintf(message, "  JDBC Driver level:             %d.%d.%d.%d(%s)\n",
         wdePtr->clientDriverLevel[0], wdePtr->clientDriverLevel[1], wdePtr->clientDriverLevel[2],
         wdePtr->clientDriverLevel[3], wdePtr->clientDriverBuildLevel);
    }
    else{
      /* the platform level is zero, no need to display it */
      sprintf(message, "  JDBC Driver level:             %d.%d.%d(%s)\n",
         wdePtr->clientDriverLevel[0], wdePtr->clientDriverLevel[1], wdePtr->clientDriverLevel[2],
         wdePtr->clientDriverBuildLevel);
    }
    strcat(messageBuffer,message);

    /* (XA)JDBC SERVER Library Level major level, minor level, feature level, platform level(build level)*/
    if (sgd.server_LevelID[3] !=0){
      /* the platform level is not zero, display it also */
      sprintf(message,"  JDBC %sServer level:%s          %d.%d.%d.%d(%s)\n", serverKind, serverKindPad,
         sgd.server_LevelID[0], sgd.server_LevelID[1],
         sgd.server_LevelID[2], sgd.server_LevelID[3],
         JDBCSERVER_BUILD_LEVEL_ID);
     }
     else {
      /* the platform level is zero, no need to display it */
      sprintf(message,"  JDBC %sServer level:%s          %d.%d.%d(%s)\n", serverKind, serverKindPad,
         sgd.server_LevelID[0], sgd.server_LevelID[1],
         sgd.server_LevelID[2], JDBCSERVER_BUILD_LEVEL_ID);
     }
    strcat(messageBuffer,message);

    /* Server Instance Identification */
    displayableServerInstanceIdent(messagePiece, NULL, BOOL_FALSE);
    sprintf(message, "  Server Instance Ident.:        (%s)\n", messagePiece);
    strcat(messageBuffer,message);

    /* RDMS level ID  */
    getRdmsLevelID(msgInsert);
    sprintf(message, "  RDMS level:                    %s\n", msgInsert);
    strcat(messageBuffer,message);

/* ownerActivity */
        getOwnerActivityType(wdePtr->ownerActivity, enumDecoding);
        sprintf(message, "  owner activity type:           %s\n", enumDecoding);
        strcat(messageBuffer,message);

#ifndef XABUILD /* Local Server */

    /* serverWorkerTS */
    sprintf(message, "  server worker T/S:             %012o\n",
        wdePtr->serverWorkerTS);
    strcat(messageBuffer,message);

    /* free_wdePtr_next */
    sprintf(message, "  next free WDE pointer:         %p\n",
        wdePtr->free_wdePtr_next);
    strcat(messageBuffer,message);

    /* assigned_wdePtr_back */
    sprintf(message, "  previous assigned WDE pointer: %p\n",
        wdePtr->assigned_wdePtr_back);
    strcat(messageBuffer,message);

    /* assigned_wdePtr_next */
    sprintf(message, "  next assigned WDE pointer:     %p\n",
        wdePtr->assigned_wdePtr_next);
    strcat(messageBuffer,message);

#endif /* Local Server */

    /* activity id, 36 bit and 72 bit */
    sprintf(message, "  server worker activity IDs:    %012o (%024llo)\n",
        wdePtr->serverWorkerActivityID, wdePtr->serverWorkerUniqueActivityID);
    strcat(messageBuffer,message);

    /* serverWorkerState */
    getServerWorkerState(wdePtr->serverWorkerState, enumDecoding);
    sprintf(message, "  server worker state:           %s\n", enumDecoding);
    strcat(messageBuffer,message);

    /* serverWorkerShutdownState */
    getServerWorkerShutdownState(wdePtr->serverWorkerShutdownState,
        enumDecoding);
    sprintf(message, "  server worker shutdown state:  %s\n", enumDecoding);
    strcat(messageBuffer,message);

#ifndef XABUILD /* Local Server */

    /* comapi_bdi */
    getNetworkAdapterName(wdePtr->network_interface, enumDecoding);
    sprintf(message, "  COMAPI mode, Network, ICL num: %s, %s, %d\n",
            sgd.ICLcomapi_mode[wdePtr->ICL_num], enumDecoding, wdePtr->ICL_num);
    strcat(messageBuffer,message);

    /* client_IP_addr - IP address display*/
    IP_to_Char(char_ip_address, wdePtr->client_IP_addr);
    sprintf(message, "  client IP address:             %s\n", char_ip_address);
    strcat(messageBuffer,message);

    /* client_IP_addr - client host name display*/
    /*
     *  Get the Client's host name. In this case, ignore the error status
     *  since if host name cannot be found a standard default value will
     *  be displayed.
     */
    error_status = Get_Hostname(hostname, wdePtr->client_IP_addr, wdePtr);
    sprintf(message, "  client IP host name:           %s\n", hostname);
    strcat(messageBuffer,message);

    /* client_socket ID
       Note: Always display the internal socket ID,
       since it is used as the ID in various console commands.
       This is in H1 (the upper 18 bits) in the WDE's socket ID word
       (client_socket), printed in decimal. */
    sprintf(message, "  server worker socket ID (id):  %d\n",
            getInternalSocketID(wdePtr->client_socket));
    strcat(messageBuffer,message);

    /* client_socket
       Display the entire client_socket word in octal.
       There are two parts (H1 and H2) in this word;
       that's why we are displaying it in octal. */
    sprintf(message, "  client socket value:           %012o\n",
            wdePtr->client_socket);
    strcat(messageBuffer,message);

    /* client_keep_alive */
    getClientKeepAlive(wdePtr->client_keepAlive, enumDecoding);
    sprintf(message, "  client keep alive:             %s\n", enumDecoding);
    strcat(messageBuffer,message);

    /* client_receive_timeout */
    sprintf(message, "  client receive timeout:        %d milliseconds\n",
        wdePtr->client_receive_timeout);
    strcat(messageBuffer,message);

#endif /* Local Server */

    /* fetch_block_size */
    sprintf(message, "  fetch block size:              %d\n",
        wdePtr->fetch_block_size);
    strcat(messageBuffer,message);

    /* serverCalledVia */
    getServerCalledViaStates(wdePtr->serverCalledVia, enumDecoding);
    sprintf(message, "  server called via:             %s\n", enumDecoding);
    strcat(messageBuffer,message);

    /* workingOnaClient */
    sprintf(message, "  working on a client:           %s\n",
        getTrueFalse(wdePtr->workingOnaClient));
    strcat(messageBuffer,message);

    /* openRdmsThread */
    sprintf(message, "  RDMS Thread open:              %s\n",
        getTrueFalse(wdePtr->openRdmsThread));
    strcat(messageBuffer,message);

    /* threadName */
    sprintf(message, "  RDMS Thread name (tid):        %s (user id: %s)\n",
            wdePtr->threadName, wdePtr->client_Userid);
    strcat(messageBuffer,message);

#ifndef XABUILD /* Local Server */

    /* standingRequestPacketPtr */
    sprintf(message, "  standing request packet ptr.:  %p\n",
        wdePtr->standingRequestPacketPtr);
    strcat(messageBuffer,message);

    /* requestPacketPtr */
    sprintf(message, "  request packet pointer:        %p\n",
        wdePtr->requestPacketPtr);
    strcat(messageBuffer,message);

    /* releasedResponsePacketPtr */
    sprintf(message, "  released response packet ptr.: %p\n",
        wdePtr->releasedResponsePacketPtr);
    strcat(messageBuffer,message);

#endif /* Local Server */

    /* firstRequestTimeStamp
     * Form two date-time strings in format: YYMMDD HH:MM:SS.SSS. Use a
     * string "none" if the time value has not been set yet (its still
     * set to 0).
     */
    timeString[0]='\0';
    if (wdePtr->firstRequestTimeStamp == 0){
      strcpy(timeString, "none"); /* use none and blanks */
    }else{
      /* Form date-time string of format: YYYYMMDD HH:MM:SS.SSS */
      UCSCNVTIM$CC(&(wdePtr->firstRequestTimeStamp), timeBuffer);
      timeBuffer[17]='\0';
      strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
      strcat(timeString," ");
      strncat(timeString, &timeBuffer[8],2); /* copy the HH */
      strcat(timeString,":");
      strncat(timeString, &timeBuffer[10],2); /* copy the MM */
      strcat(timeString,":");
      strncat(timeString, &timeBuffer[12],2); /* copy the SS */
      strcat(timeString,".");
      strncat(timeString, &timeBuffer[14],3); /* copy the SSS */
    }
    message[0]='\0';
    sprintf(message, "  first request timestamp:       %s\n", timeString);
    strcat(messageBuffer,message);

    /* lastRequestTimeStamp
     * Form two date-time strings in format: YYMMDD HH:MM:SS.SSS. Use a
     * string "none" if the time value has not been set yet (its still
     * set to 0).
     */
    timeString[0]='\0';
    if (wdePtr->lastRequestTimeStamp == 0){
      strcpy(timeString, "none"); /* use none and blanks */
    }else{
      UCSCNVTIM$CC(&(wdePtr->lastRequestTimeStamp), timeBuffer);
      timeBuffer[17]='\0';
      strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
      strcat(timeString," ");
      strncat(timeString, &timeBuffer[8],2); /* copy the HH */
      strcat(timeString,":");
      strncat(timeString, &timeBuffer[10],2); /* copy the MM */
      strcat(timeString,":");
      strncat(timeString, &timeBuffer[12],2); /* copy the SS */
      strcat(timeString,".");
      strncat(timeString, &timeBuffer[14],3); /* copy the SSS */
    }
    message[0]='\0';
    sprintf(message, "  last request timestamp:        %s\n", timeString);
    strcat(messageBuffer,message);

    /* taskCode */
    sprintf(message, "  task code:                     %d (%s)\n",
            wdePtr->taskCode, taskCodeString(wdePtr->taskCode));
    strcat(messageBuffer,message);

#ifdef XABUILD /* JDBC XA Server */
    /* txn_flag, odtpTokenValue */
    sprintf(message, "  txn_flag, odtpTokenValue:      %d, 0%012o\n",
            wdePtr->txn_flag, wdePtr->odtpTokenValue);
    strcat(messageBuffer,message);
#endif /* JDBC XA Server */

    /* debug */
    sprintf(message, "  debug:                         %d\n", wdePtr->debug);
    strcat(messageBuffer,message);

    /* debugFlags */
    sprintf(message, "  debug flags:                   %012o\n",
        wdePtr->debugFlags);
    strcat(messageBuffer,message);

    /* clientTraceFileNbr */
    sprintf(message, "  client trace file number:      %d\n",
        wdePtr->clientTraceFileNbr);
    strcat(messageBuffer,message);

    /* clientTraceFileName.

       Note that the client trace file may or may not exist,
       and if it does exist, it may or may not be open
       (because of the 'TURN OFF CLIENT id TRACE' command
       in the Console Handler).
       */
    msgInsert[0] = '\0';
    if ((strlen(wdePtr->clientTraceFileName) == 0)
           || (wdePtr->clientTraceFileNbr == 0)) {
        strcpy(fullFileName, "none");
    } else if ((strcmp(wdePtr->clientTraceFileName, PRINT$_FILE) == 0)){
        strcpy(fullFileName, PRINT$_FILE); /* PRINT$ is its own full name */
    } else {
        sprintf(clientTraceFileInternalName, "%s%d",
            CLIENT_TRACE_FILE_USE_NAME_PREFIX, wdePtr->clientTraceFileNbr);
        getFullFileName(clientTraceFileInternalName, fullFileName, TRUE);

        if (wdePtr->clientTraceFile == NULL) {
            strcpy(msgInsert,"(trace off)");
        }
    }

    sprintf(message, "  client trace file:             %s %s\n", fullFileName, msgInsert);
    strcat(messageBuffer,message);

    /* clientTraceFile */
    sprintf(message, "  client trace file pointer:     %p\n",
        wdePtr->clientTraceFile);
    strcat(messageBuffer,message);

    /* workerMessageFileName */
    sprintf(message, "  worker message element:        %s\n",
        wdePtr->workerMessageFileName);
    strcat(messageBuffer,message);

    /* workerMessageFile */
    sprintf(message, "  worker message file pointer:   %p\n",
        wdePtr->workerMessageFile);
    strcat(messageBuffer,message);

    /* clientLocale */
    if (strlen(wdePtr->clientLocale) == 0) {
        strcpy(localLocale, "none");
    } else {
        strcpy(localLocale, wdePtr->clientLocale);
    }

    sprintf(message, "  client locale:                 %s\n", localLocale);
    strcat(messageBuffer,message);

    /* serverMessageFile */
    sprintf(message, "  server message file pointer:   %p\n",
        wdePtr->serverMessageFile);
    strcat(messageBuffer,message);

#ifndef XABUILD /* JDBC XA Server */
    /* consoleSetClientTraceFile - Also add an extra \n since this is last image. */
    sprintf(message, "  console set client trace file: %s\n\n",
        getTrueFalse(wdePtr->consoleSetClientTraceFile));
    strcat(messageBuffer,message);
#else
    /* consoleSetClientTraceFile */
    sprintf(message, "  console set client trace file: %s\n",
        getTrueFalse(wdePtr->consoleSetClientTraceFile));
    strcat(messageBuffer,message);

    /* XA_thread_reuse_limit - Also add an extra \n since this is last image.*/
    sprintf(message, "  XA_thread_reuse_limit:         %d\n\n", sgd.XA_thread_reuse_limit);
    strcat(messageBuffer,message);
#endif /* JDBC XA Server */

    /* Return, messagebuffer has the desired images. */
    return;
}

/**
 * Function processDisplayFilenamesCmd.
 *
 * Process the command: DISPLAY FILENAMES.
 * Display the server log file name and the server trace file name.
 *
 * @return
 *   A status:
 *     - 0: The console reply has already been sent.
 *     - 1: The caller calls replyToKeyin to print the reply in
 *          sgd.textMessage.
 */

static int processDisplayFilenamesCmd() {

    char msgInsert[12];

    /* Initialization */
    msgInsert[0] = '\0';

    /* The server log file should always exist and be open,
       but check for a NULL pointer just in case. */
    if (sgd.serverLogFile == NULL) {
        strcpy(fullFileName,"none");
    } else {
        getFullFileName(LOG_FILE_USE_NAME, fullFileName, TRUE);
    }

    sprintf(sgd.textMessage, "server log file:                 %s",
        fullFileName);
    replyToKeyin(sgd.textMessage);

    /* The server trace file may or may not exist,
       and if it does exist, it may or may not be open
       (because of the TURN OFF SERVER TRACE command
       in the Console Handler).
       */
    if (strlen(sgd.serverTraceFileName) == 0) {
        strcpy(fullFileName,"none");
    } else if ((strcmp(sgd.serverTraceFileName, PRINT$_FILE) == 0)){
        strcpy(fullFileName, PRINT$_FILE); /* PRINT$ is its own full name */
    } else {
        getFullFileName(TRACE_FILE_USE_NAME, fullFileName, TRUE);

        if (sgd.serverTraceFile == NULL) {
            strcpy(msgInsert,"(trace off)");
        }
    }

    sprintf(sgd.textMessage, "server trace file:               %s %s",
        fullFileName, msgInsert);
    replyToKeyin(sgd.textMessage);

    /* Return 0, since the reply has already been sent */
    return 0;

} /* processDisplayFilenamesCmd */

/* Utility functions
   ----------------- */

/* Set the string reference parameter to the decoded value
   of the enum parameter. */

static void getServerWorkerState(enum serverWorkerStates e1, char * s1) {

    switch (e1) {
        case WORKER_CLOSED:
            strcpy(s1, "WORKER_CLOSED");
            break;
        case WORKER_INITIALIZING:
            strcpy(s1, "WORKER_INITIALIZING");
            break;
        case FREE_NO_CLIENT_ASSIGNED:
            strcpy(s1, "FREE_NO_CLIENT_ASSIGNED");
            break;
        case CLIENT_ASSIGNED:
            strcpy(s1, "CLIENT_ASSIGNED");
            break;
        case WORKER_DOWNED_GRACEFULLY:
            strcpy(s1, "WORKER_DOWNED_GRACEFULLY");
            break;
        case WORKER_DOWNED_IMMEDIATELY:
            strcpy(s1, "WORKER_DOWNED_IMMEDIATELY");
            break;
        case WORKER_DOWNED_DUE_TO_ERROR:
            strcpy(s1, "WORKER_DOWNED_DUE_TO_ERROR");
            break;
        default:
            strcpy(s1, "invalid server worker state");
    }
}

static void getServerWorkerShutdownState(enum serverWorkerShutdownStates e1,
                                  char * s1) {

    switch (e1) {
        case WORKER_ACTIVE:
            strcpy(s1, "WORKER_ACTIVE");
            break;
        case WORKER_SHUTDOWN_GRACEFULLY:
            strcpy(s1, "WORKER_SHUTDOWN_GRACEFULLY");
            break;
        case WORKER_SHUTDOWN_IMMEDIATELY:
            strcpy(s1, "WORKER_SHUTDOWN_IMMEDIATELY");
            break;
        default:
            strcpy(s1, "invalid server worker shutdown state");
    }
}

static void getServerState(enum serverStates e1, char * s1) {

    switch (e1) {
        case SERVER_CLOSED:
            strcpy(s1, "SERVER_CLOSED");
            break;
        case SERVER_INITIALIZING_CONFIGURATION:
            strcpy(s1, "SERVER_INITIALIZING_CONFIGURATION");
            break;
        case SERVER_INITIALIZING_SERVERWORKERS:
            strcpy(s1, "SERVER_INITIALIZING_SERVERWORKERS");
            break;
        case SERVER_INITIALIZING_ICL:
            strcpy(s1, "SERVER_INITIALIZING_ICL");
            break;
        case SERVER_RUNNING:
            strcpy(s1, "SERVER_RUNNING");
            break;
        case SERVER_DOWNED_GRACEFULLY:
            strcpy(s1, "SERVER_DOWNED_GRACEFULLY");
            break;
        case SERVER_DOWNED_IMMEDIATELY:
            strcpy(s1, "SERVER_DOWNED_IMMEDIATELY");
            break;
        case SERVER_DOWNED_DUE_TO_ERROR:
            strcpy(s1, "SERVER_DOWNED_DUE_TO_ERROR");
            break;
        default:
            strcpy(s1, "invalid server state");
    }
}

static void getServerShutdownState(enum serverShutdownStates e1, char * s1) {

    switch (e1) {
        case SERVER_ACTIVE:
            strcpy(s1, "SERVER_ACTIVE");
            break;
        case SERVER_SHUTDOWN_GRACEFULLY:
            strcpy(s1, "SERVER_SHUTDOWN_GRACEFULLY");
            break;
        case SERVER_SHUTDOWN_IMMEDIATELY:
            strcpy(s1, "SERVER_SHUTDOWN_IMMEDIATELY");
            break;
        default:
            strcpy(s1, "invalid server shutdown state");
    }
}

#ifndef XABUILD /* Local Server */

static void getICLState(enum ICLStates e1, char * s1) {

    switch (e1) {
        case ICL_CLOSED:
            strcpy(s1, "ICL_CLOSED");
            break;
        case ICL_INITIALIZING:
            strcpy(s1, "ICL_INITIALIZING");
            break;
        case ICL_RUNNING:
            strcpy(s1, "ICL_RUNNING");
            break;
        case ICL_DOWNED_GRACEFULLY:
            strcpy(s1, "ICL_DOWNED_GRACEFULLY");
            break;
        case ICL_DOWNED_IMMEDIATELY:
            strcpy(s1, "ICL_DOWNED_IMMEDIATELY");
            break;
        case ICL_DOWNED_DUE_TO_ERROR:
            strcpy(s1, "ICL_DOWNED_DUE_TO_ERROR");
            break;
        case ICL_CONNECTING:
            strcpy(s1, "ICL_CONNECTING");
            break;
        default:
            strcpy(s1, "invalid ICL state");
    }
}

static void getICLShutdownState(enum ICLShutdownStates e1, char * s1) {

    switch (e1) {
        case ICL_ACTIVE:
            strcpy(s1, "ICL_ACTIVE");
            break;
        case ICL_SHUTDOWN_GRACEFULLY:
            strcpy(s1, "ICL_SHUTDOWN_GRACEFULLY");
            break;
        case ICL_SHUTDOWN_IMMEDIATELY:
            strcpy(s1, "ICL_SHUTDOWN_IMMEDIATELY");
            break;
        default:
            strcpy(s1, "invalid ICL shutdown state");
    }
}

/* The 's1' input buffer must be be large enough to hold the string
 * built here. It must be at least 17 char for the string name plus
 * 11 char for the status display.
 */
static void getICLCOMAPIState(int e1, char * s1) {
    char status[8];

    switch (e1 & 0777777) { /* Look at the lower half word for the status */
        case API_NORMAL:
            strcpy(s1, "API-NORMAL");
            break;
        case SEACCES:
            strcpy(s1, "API-ACCESS-ERROR");
            break;
        case SEFAULT:
            strcpy(s1, "API-BAD-PARAMETER");
            break;
        case SENOBUFS:
            strcpy(s1, "API-NO-BUFFERS");
            break;
        case SEOPNOSUPPORT:
            strcpy(s1, "API-BAD-OPER");
            break;
        case SEBADF:
            strcpy(s1, "API-BAD-SESSION");
            break;
        case SEINVAL:
            strcpy(s1, "API-INVALID=PARA");
            break;
        case SEISCONN:
            strcpy(s1, "API-IS-CONNECTED");
            break;
        case SEBADREG:
            strcpy(s1, "API-BAD_REG");
            break;
        case SENOTREG:
            strcpy(s1, "API-NOT-REG");
            break;
        case SEBADACT:
            strcpy(s1, "API-BAD-ACTIVITY");
            break;
        case SENOTCONN:
            strcpy(s1, "API-NOT-CONNECTED");
            break;
        case SETIMEDOUT:
            strcpy(s1, "API-TIMED-OUT");
            break;
        case SENETDOWN:
            strcpy(s1, "API-NETWORK-DOWN");
            break;
        case SEBADTSAM:
            strcpy(s1, "API-TSAM-STATUS");
            break;
        case SEISLIST:
            strcpy(s1, "API-IS-LISTENING");
            break;
        case SENOTLIST:
            strcpy(s1, "API-NOT-LISTENING");
            break;
        case SEMOREDATA:
            strcpy(s1, "API-MORE-TO-COME");
            break;
        case SEDATALOST:
            strcpy(s1, "API-DATA-LOST");
            break;
        case SEBADNOTIFY:
            strcpy(s1, "API-BAD-NOTIFY");
            break;
        case SEUDPERRIND:
            strcpy(s1, "API-UDP-ERR-IND");
            break;
        case SEBADSTATE:
            strcpy(s1, "API-BAD-STATE");
            break;
        case SEWOULDBLOCK:
            strcpy(s1, "API-WOULD-BLOCK");
            break;
        case SEOUTSTOPPED:
            strcpy(s1, "API-OUTPUT-STOP");
            break;
        case SEHAVEEVENT:
            strcpy(s1, "API-HAVE-EVENT");
            break;
        case SEQUEUELIMIT:
            strcpy(s1, "API-QUEUE-LIMIT");
            break;
        case SEMAXSOCKETS:
            strcpy(s1, "API-MAX-SOCKETS");
            break;
        case SEBADQBREQ:
            strcpy(s1, "API-BAD-QBREQUEST");
            break;
        case SENOQBANKS:
            strcpy(s1, "API-NO-QUEUE-BANK");
            break;
        case SEISREG:
            strcpy(s1, "API-ALREADY-REG");
            break;
        case SEMAXLISTENS:
            strcpy(s1, "API-MAX-LISTENS");
            break;
        case SEDUPLISTEN:
            strcpy(s1, "API-DUP-LISTEN");
            break;
        default:
            /* no match, return code value */
            strcpy(s1, "Unknown");
    }

    /* Append the actual status value (aux status, status) */
    /* The octal/decimal format matches the format in the reference manuals. */
    /* E.g. API-BAD-OPER (3,10006) */
    strcat(s1, " (0");                              /* Add a leading zero for the octal value) */
    itoa(((e1 & 0777777000000) >> 18), status, 8);  /* Get the aux status in octal (upper halfword) */
    strcat(s1, status);
    strcat(s1, ", ");
    itoa((e1 & 0777777), status, 10);               /* Get the status in decimal (lower halfword) */
    strcat(s1, status);
    strcat(s1, ")");
}


#endif /* Local Server */

static void getConsoleHandlerState(enum consoleHandlerStates e1, char * s1) {

    switch (e1) {
        case CH_INACTIVE:
            strcpy(s1, "CH_INACTIVE");
            break;
        case CH_INITIALIZING:
            strcpy(s1, "CH_INITIALIZING");
            break;
        case CH_RUNNING:
            strcpy(s1, "CH_RUNNING");
            break;
        case CH_DOWNED:
            strcpy(s1, "CH_DOWNED");
            break;
        default:
            strcpy(s1, "invalid console handler state");
    }
}

static void getConsoleHandlerShutdownState(
    enum consoleHandlerShutdownStates e1,
    char * s1) {

    switch (e1) {
        case CH_ACTIVE:
            strcpy(s1, "CH_ACTIVE");
            break;
        case CH_SHUTDOWN:
            strcpy(s1, "CH_SHUTDOWN");
            break;
        default:
            strcpy(s1, "invalid console handler shutdown state");
    }
}

static void getOwnerActivityType(enum ownerActivityType e1, char * s1) {

    switch (e1) {
        case SERVER_WORKER:
            strcpy(s1, "SERVER_WORKER");
            break;
        case SERVER_ICL:
            strcpy(s1, "SERVER_ICL");
            break;
        case SERVER_CH:
            strcpy(s1, "SERVER_CH");
            break;
        case SERVER_UASFM:
            strcpy(s1, "SERVER_UASFM");
            break;
        default:
            strcpy(s1, "invalid owner activity type");
    }
}

static void getServerCalledViaStates(enum serverCalledViaStates e1,
                                     char * s1) {

    switch (e1) {
        case CALLED_VIA_COMAPI:
            strcpy(s1, "CALLED_VIA_COMAPI");
            break;
        case CALLED_VIA_JNI:
            strcpy(s1, "CALLED_VIA_JNI");
            break;
        case CALLED_VIA_CORBA:
            strcpy(s1, "CALLED_VIA_CORBA");
            break;
        default:
            strcpy(s1, "invalid server called via state");
    }
}

static void getUserAccessControl(int i1,
                                 char * s1) {

    switch (i1) {
        case USER_ACCESS_CONTROL_OFF:
            strcpy(s1, "OFF");
            break;
        case USER_ACCESS_CONTROL_JDBC:
            strcpy(s1, "JDBC (SECOPT1)");
            break;
        case USER_ACCESS_CONTROL_FUND:
            strcpy(s1, "JDBC (FUNDAMENTAL)");
            break;
        default:
            strcpy(s1, "invalid user access control");
    }
}

static void getUASFMState(enum UASFM_States e1,
                            char * s1) {

    switch (e1) {
        case UASFM_CLOSED:
            strcpy(s1, "UASFM_CLOSED");
            break;
        case UASFM_RUNNING:
            strcpy(s1, "UASFM_RUNNING");
            break;
        default:
            strcpy(s1, "invalid UASFM state");
    }
}

static void getUASFMShutdownState(enum UASFM_ShutdownStates e1,
                            char * s1) {

    switch (e1) {
        case UASFM_ACTIVE:
            strcpy(s1, "UASFM_ACTIVE");
            break;
        case UASFM_SHUTDOWN:
            strcpy(s1, "UASFM_SHUTDOWN");
            break;
        default:
            strcpy(s1, "invalid UASFM shutdown state");
    }
}

static void getLogConsoleOutput(int i1,
                                char * s1) {

    switch (i1) {
        case LOG_CONSOLE_OUTPUT_OFF:
            strcpy(s1, "OFF");
            break;
        case LOG_CONSOLE_OUTPUT_ON:
            strcpy(s1, "ON");
            break;
        default:
            strcpy(s1, "invalid log console output");
    }
}

#ifndef XABUILD /* Local Server */

static void getClientKeepAlive(int i1,
                               char * s1) {

    switch (i1) {
        case CLIENT_KEEPALIVE_OFF:
            strcpy(s1, CONFIG_CLIENT_KEEPALIVE_OFF);
            break;
        case CLIENT_KEEPALIVE_ON:
            strcpy(s1, CONFIG_CLIENT_KEEPALIVE_ON);
            break;
        case CLIENT_KEEPALIVE_ALWAYS_OFF:
            strcpy(s1, CONFIG_CLIENT_KEEPALIVE_ALWAYS_OFF);
            break;
        case CLIENT_KEEPALIVE_ALWAYS_ON:
            strcpy(s1, CONFIG_CLIENT_KEEPALIVE_ALWAYS_ON);
            break;
        default:
            strcpy(s1, "invalid client keep alive");
    }
}
static void getNetworkAdapterName(int i1,
                                  char * s1) {

    switch (i1) {
        case CPCOMM:
            strcpy(s1, "CPCOMM");
            break;
        case CMS1100:
            strcpy(s1, "CMS");
            break;
        default:
            strcpy(s1, "UNKNOWN");
    }
}
#endif /* Local Server */
