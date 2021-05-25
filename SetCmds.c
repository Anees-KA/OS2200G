/**
 * File: SetCmds.c.
 *
 * Functions that process the JDBC Server's console handling SET commands.
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

/* Standard C header files and OS2200 System Library files */
#include <ertran.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <ctype.h>
#include <universal.h>
#include <performance.h>
#include "marshal.h"

/* JDBC Project Files */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "ConsoleCmds.h"
#include "DisplayCmds.h"
#include "NetworkAPI.h"
#include "ServerConfig.h"
#include "ServerConsol.h"
#include "ServerLog.h"
#include "ServiceUtils.h"
#include "SetCmds.h"

/* Imported data */

extern serverGlobalData sgd;
    /* The Server Global Data (SGD), visible to all Server activities. */

/* SET command
   ----------- */

/**
 * Function processSetCmd.
 *
 * Process the console handler SET commands.
 *
 * Note that we allows any number of blanks (including none)
 * on either side of the equal.
 * However, the keywords must have only one blank between them.
 *
 *   configuration management
 *
 *     SET MAX ACTIVITIES=n                    **
 *
 *     SET SERVER RECEIVE TIMEOUT=n            **
 *     SET SERVER SEND TIMEOUT=n               **
 *     SET SERVER ACTIVITY RECEIVE TIMEOUT=n   **
 *
 *     SET COMAPI DEBUG=n                      **
 *
 *       The following four commands are for internal use only
 *       (i.e., they are not documented in the user doc)
 *
 *     SET SERVER DEBUG=n
 *     SET CLIENT DEBUG=INTERNAL
 *     SET CLIENT DEBUG=DETAIL
 *     SET CLIENT DEBUG=OFF
 *
 *   log and trace file control
 *
 *     SET SERVER LOG FILE=filename
 *     SET SERVER TRACE FILE=filename
 *     SET CLIENT id|tid TRACE FILE=filename   **
 *     SET CLIENT TRACE FILE=filename          ***
 *
 *     SET LOG CONSOLE OUTPUT=ON
 *     SET LOG CONSOLE OUTPUT=OFF
 *
 *   internal (not documented)
 *
 *     SET ICL m COMAPI STATE=n                **
 *
 * The commands marked with "**" are available only for the Local Server.
 * The command marked with "***" is available only for the XA Server.
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

int processSetCmd(char *msg) {

    char tid[80]; /* large enough for thread name (and even the whole CH command). */
    char workerID_or_tid[80];  /* also make large enough */
    int value;
    int errorStatus;
    int logIndicator;
    char fileName[MAX_FILE_NAME_LEN+1];
    char valueSpecified[MAX_CONFIG_PARM_VALUE_SIZE];
    char insertParm[MAX_INSERT_CONFIG_PARM_SIZE];
    char * errMsg;
    char * msg2;
    workerDescriptionEntry * wdePtr = NULL;
    char tempMsg[MESSAGE_MAX_SIZE+10]; /* allow some slack bytes */
    char * msg3;
    char keyword1[MESSAGE_MAX_SIZE+10]; /* allow some slack bytes */
    int status;

    /* Local variable used to pick up the "=" in the SET command */
    char equalSign[TRAILING_STRING_SIZE];

    /* The following is used to detect garbage following what looks
       like legal command syntax.
       If that case is detected, the command is ignored. */
    char trail[TRAILING_STRING_SIZE];

    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

#ifndef XABUILD /* Local Server */

    int iclnum;
    int eventStatus;

#endif /* Local Server */

    /* Handle space around '=' in SET command.
       Take the console command message (msg),
       and replace the '=' with " = ".
       The allows us to parse with any number of blanks of either side
       of the '=' (to allow more flexible syntax).
       The result in msg3 (a char *)
       (the actual storage for the updated text is in tempMsg).

       If there is no '=' in the SET console command,
       return NULL from addSpaceAroundEqual.
       This is an error case,
       since each legal SET command has an '='.
       */

    msg3 = addSpaceAroundEqual(msg, tempMsg);

    if (msg3 == NULL) {
        /* No '=' in SET command */
        msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL, 0, msgBuffer);
            /* 5223 Invalid SET command */
        strcpy(sgd.textMessage, msg2);
        return 1;
    }

#ifndef XABUILD /* Local Server: SET MAX ... and SET ICL ... */

    /* Check command syntax */
    if (strncmp(msg3, "SET MAX", 7) == 0) {

        /* SET MAX ACTIVITIES=n */
        if (sscanf(msg3, "SET MAX ACTIVITIES %s %d %s", &equalSign, &value,
            &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "MAX ACTIVITIES");
            sprintf(valueSpecified, "%d", value);

            /* Call the config validation routine to either
               use the value specified if valid (0 returned),
               or reject the value (positive value returned).
               */
            errorStatus = validate_config_int(
                value,
                VALIDATE_AND_UPDATE,
                CONFIG_MAX_ACTIVITIES,
                CONFIG_LIMIT_MINSERVERWORKERS,
                CONFIG_LIMIT_MAXSERVERWORKERS,
                TRUE,
                CONFIG_DEFAULT_MAXSERVERWORKERS,
                JDBCSERVER_VALIDATE_MAXACTIVITIES_ERROR,
                JDBCSERVER_VALIDATE_MAXACTIVITIES_WARNING,
                NULL);

        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL,
                0, msgBuffer);
                /* 5223 Invalid SET command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

    } else if (strncmp(msg3, "SET ICL", 7) == 0) {

        /* SET ICL m COMAPI STATE=n
           This command is internal and is not documented.
           It is used to test COMAPI.
           */
        errorStatus = 0; /* always assume it works */

        if (sscanf(msg3, "SET ICL %d COMAPI STATE %s %d %s", &iclnum,
            &equalSign, &value, &trail) == 3) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            /* Set up the display info */
            sprintf(insertParm, "ICL %d COMAPI STATE", iclnum);
            sprintf(valueSpecified, "%d", value);

            /* check if the ICL number is legal */
            if (iclnum >= 0 && iclnum < MAX_COMAPI_MODES){
                /*
                 * Legal, Test if a COMAPI wait time or a forced
                 * COMAPI error status.
                 */
                if (value < 0){
                    /* Store the new COMAPI wait time for twaits */
                    sgd.comapi_reconnect_wait_time[iclnum]= - value; /* time is in milliseconds */
                }
                else {
                    /* Store the forced error status for the COMAPI for the specified ICL instance */
                    sgd.forced_COMAPI_error_status[iclnum]=value;
                }
            }

        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL,
                0, msgBuffer);
                /* 5223 Invalid SET command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

    } else if (strncmp(msg3, "SET SERVER", 10) == 0) {

#else /* XA Server */

    if (strncmp(msg3, "SET SERVER", 10) == 0) {

#endif

        /* SET SERVER DEBUG=n */
        if (sscanf(msg3, "SET SERVER DEBUG %s %d %s", &equalSign, &value, &trail)
            == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "SERVER DEBUG");
            sprintf(valueSpecified, "%d", value);

            /* For now, allow any integer value for n */
            sgd.debug = value;
            errorStatus = 0;

#ifndef XABUILD /* Local Server:
                   SET SERVER RECEIVE TIMEOUT
                   SET SERVER SEND TIMEOUT
                   SET SERVER ACTIVITY RECEIVE TIMEOUT
                */

        /* SET SERVER RECEIVE TIMEOUT=n */
        } else if (sscanf(msg3, "SET SERVER RECEIVE TIMEOUT %s %d %s",
            &equalSign, &value, &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "SERVER RECEIVE TIMEOUT");
            sprintf(valueSpecified, "%d", value);

            /* Call the config validation routine to either
               use the value specified if valid (0 returned),
               or reject the value (positive value returned).
               */
            errorStatus = validate_config_int(
                value,
                VALIDATE_AND_UPDATE,
                CONFIG_SERVER_RECEIVE_TIMEOUT,
                CONFIG_LIMIT_MINSERVER_RECEIVE_TIMEOUT,
                CONFIG_LIMIT_MAXSERVER_RECEIVE_TIMEOUT,
                TRUE,
                CONFIG_DEFAULT_SERVER_RECEIVE_TIMEOUT,
                JDBCSERVER_VALIDATE_SERVER_RECEIVE_TIMEOUT_ERROR,
                JDBCSERVER_VALIDATE_SERVER_RECEIVE_TIMEOUT_WARNING,
                NULL);

        /* SET SERVER SEND TIMEOUT=n */
        } else if (sscanf(msg3, "SET SERVER SEND TIMEOUT %s %d %s",
            &equalSign, &value, &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "SERVER SEND TIMEOUT");
            sprintf(valueSpecified, "%d", value);

            /* Call the config validation routine to either
               use the value specified if valid (0 returned),
               or reject the value (positive value returned).
               */
            errorStatus = validate_config_int(
                value,
                VALIDATE_AND_UPDATE,
                CONFIG_SERVER_SEND_TIMEOUT,
                CONFIG_LIMIT_MINSERVER_SEND_TIMEOUT,
                CONFIG_LIMIT_MAXSERVER_SEND_TIMEOUT,
                TRUE,
                CONFIG_DEFAULT_SERVER_SEND_TIMEOUT,
                JDBCSERVER_VALIDATE_SERVER_SEND_TIMEOUT_ERROR,
                JDBCSERVER_VALIDATE_SERVER_SEND_TIMEOUT_WARNING,
                NULL);

        /* SET SERVER ACTIVITY RECEIVE TIMEOUT=n */
        } else if (sscanf(msg3, "SET SERVER ACTIVITY RECEIVE TIMEOUT %s %d %s",
                &equalSign, &value, &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "SERVER ACTIVITY RECEIVE TIMEOUT");
            sprintf(valueSpecified, "%d", value);

            /* Call the config validation routine to either
               use the value specified if valid (0 returned),
               or reject the value (positive value returned).
               */
            errorStatus = validate_config_int(
                value,
                VALIDATE_AND_UPDATE,
                CONFIG_CLIENT_RECEIVE_TIMEOUT,
                CONFIG_LIMIT_MINCLIENT_RECEIVE_TIMEOUT,
                CONFIG_LIMIT_MAXCLIENT_RECEIVE_TIMEOUT,
                TRUE,
                CONFIG_DEFAULT_CLIENT_RECEIVE_TIMEOUT,
                JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_ERROR,
                JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_WARNING,
                NULL);

#endif /* Local Server */

        /* SET SERVER LOG FILE=filename */
        } else if (sscanf(msg3, "SET SERVER LOG FILE %s %s %s", &equalSign,
            &fileName, &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "SERVER LOG FILE");
            strcpy(valueSpecified, fileName);

            /* Call the config validation routine to either
               use the value specified if valid (0 returned),
               or reject the value (positive value returned).

               Note that if successful, this call sets the string
               sgd.serverLogFileName,
               and does an @USE LOG_FILE_USE_NAME on that string.
               */
            errorStatus = validate_config_filename(
                fileName,
                VALIDATE_AND_UPDATE,
                CONFIG_SERVERLOGFILENAME,
                CONFIG_LIMIT_MINFILENAMELENGTH,
                CONFIG_LIMIT_MAXFILENAMELENGTH,
                TRUE,
                CONFIG_DEFAULT_SERVER_LOGFILENAME,
                JDBCSERVER_VALIDATE_LOGFILENAME_ERROR,
                JDBCSERVER_VALIDATE_LOGFILENAME_WARNING,
                NULL);

            /* Write the config info the log file */
            if (errorStatus == 0) {
                processDisplayConfigurationCmd(TRUE, TO_SERVER_LOGFILE, FALSE,
                    NULL);
            }

        /* SET SERVER TRACE FILE=filename */
        } else if (sscanf(msg3, "SET SERVER TRACE FILE %s %s %s", &equalSign,
            &fileName, &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "SERVER TRACE FILE");
            strcpy(valueSpecified, fileName);

            /* Call the config validation routine to either
               use the value specified if valid (0 returned),
               or reject the value (positive value returned).

               Note that if successful, this call sets the string
               sgd.serverTraceFileName,
               and does an @USE TRACE_FILE_USE_NAME on that string.
               */
            errorStatus = validate_config_filename(
                fileName,
                VALIDATE_AND_UPDATE,
                CONFIG_SERVERTRACEFILENAME,
                CONFIG_LIMIT_MINFILENAMELENGTH,
                CONFIG_LIMIT_MAXFILENAMELENGTH,
                TRUE,
                CONFIG_DEFAULT_SERVER_TRACEFILENAME,
                JDBCSERVER_VALIDATE_TRACEFILENAME_ERROR,
                JDBCSERVER_VALIDATE_TRACEFILENAME_WARNING,
                NULL);

            /* Write the config info the trace file */
            if (errorStatus == 0) {
                processDisplayConfigurationCmd(TRUE, TO_SERVER_TRACEFILE,
                    FALSE, NULL);
            }

        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL,
                0, msgBuffer);
                /* 5223 Invalid SET command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

#ifndef XABUILD /* Local Server: SET COMAPI ... */

    } else if (strncmp(msg3, "SET COMAPI", 10) == 0) {

        /* SET COMAPI DEBUG=n */
        if (sscanf(msg3, "SET COMAPI DEBUG %s %d %s", &equalSign, &value,
            &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "COMAPI DEBUG");
            sprintf(valueSpecified, "%d", value);
            errorStatus = 0;

            /* Post the new COMAPI debug value
               and do a PassEvent() to the ICL activity.
               This allows the COMAPI to be called (via Set_Socket_Options)
               with the new value when the socket is free.
               If the value is 0, post a -1
               (since nonzero indicates a value was posted).
               */
            if (sgd.COMAPIDebug != value) {
                if (value == 0) {
                    sgd.postedCOMAPIDebug = -1;
                } else {
                    sgd.postedCOMAPIDebug = value;
                }

                eventStatus = Pass_Event();

                if (eventStatus == NO_SERVER_SOCKETS_ACTIVE){
                    /* Just change the SGD value - the
                       server sockets will pick up this
                       value later as they are initialized.
                       */
                    sgd.COMAPIDebug = value;
                    sgd.postedCOMAPIDebug = 0; /* no need to post it. */
                }
            }

        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL,
                0, msgBuffer);
                /* 5223 Invalid SET command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

#endif /* Local Server */

    } else if (strncmp(msg3, "SET LOG", 7) == 0) {

        if (sscanf(msg3, "SET LOG CONSOLE OUTPUT %s %s %s", &equalSign,
            &keyword1, &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "LOG CONSOLE OUTPUT");
            strcpy(valueSpecified, keyword1);
            errorStatus = 0;

            /* SET LOG CONSOLE OUTPUT=ON */
            if (strcmp(keyword1, "ON") == 0) {
                sgd.logConsoleOutput = TRUE;

            /* SET LOG CONSOLE OUTPUT=OFF */
            } else if (strcmp(keyword1, "OFF") == 0) {
                sgd.logConsoleOutput = FALSE;

            } else {
                errorStatus = 1;
            }

        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL,
                0, msgBuffer);
                    /* 5223 Invalid SET command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

    } else if (strncmp(msg3, "XSET CLIENT", 11) == 0) {

        if (sscanf(msg3, "SET CLIENT DEBUG %s %s %s", &equalSign,
            &keyword1, &trail) == 2) {

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(insertParm, "CLIENT DEBUG");
            strcpy(valueSpecified, keyword1);
            errorStatus = 0;

            /* SET CLIENT DEBUG=INTERNAL */
            if (strcmp(keyword1, "INTERNAL") == 0) {
                sgd.clientDebugInternal = TRUE;
                sgd.clientDebugDetail = TRUE;

#ifdef XABUILD /* XA JDBC Server */

                sgd.debugXA=TRUE; /* turn on XA debugging if we are in XA JDBC Server */

#endif         /* XA JDBC Server */


            /* SET CLIENT DEBUG=DETAIL */
            } else if (strcmp(keyword1, "DETAIL") == 0) {
                sgd.clientDebugInternal = FALSE;
                sgd.clientDebugDetail = TRUE;

#ifdef XABUILD /* XA JDBC Server */

                sgd.debugXA=FALSE; /* turn off XA debugging if we are in XA JDBC Server */

#endif         /* XA JDBC Server */

            /* SET CLIENT DEBUG=OFF */
            } else if (strcmp(keyword1, "OFF") == 0) {
                sgd.clientDebugInternal = FALSE;
                sgd.clientDebugDetail = FALSE;

#ifdef XABUILD /* XA JDBC Server */

                sgd.debugXA=FALSE; /* turn off XA debugging if we are in XA JDBC Server */

#endif         /* XA JDBC Server */

            } else {
                errorStatus = 1;
            }

#ifdef XABUILD /* XA Server */

        /* SET CLIENT TRACE FILE=filename */

        } else if (sscanf(msg3, "SET CLIENT TRACE FILE %s %s %s",
                &equalSign, &fileName, &trail) == 2) {

            value = magicSocketID;
            strcpy(tid, magicSocketID_as_string);
                /* Set this value for the findWorker call below.
                   This causes us to get the first (only) WDE entry for
                   the XA Server.
                   This is needed since there is no ID field in the
                   command for the XA case.  */

#else /* Local Server */

        /* SET CLIENT id|tid TRACE FILE=filename

           id is the server worker socket ID.
           This is in H1 (the upper 18 bits) in the WDE's socket ID word
           (client_socket), specified in decimal.
           This id value is listed under "server work socket id",
           which is the first line of output for each server worker,
           as generated by the console command DISPLAY WORKER STATUS.

           tid (if specified) is the server worker RDMS thread name.
           This is in the WDE's threadname character array.
           */

        } else if (sscanf(msg3, "SET CLIENT %s TRACE FILE %s %s %s", &tid,
                &equalSign, &fileName, &trail) == 3) {

#endif

            /* Check for '=' sign.
               Remember that any number of spaces (including none)
               may appear around each side of the '=',
               because of the addSpaceAroundEqual call above.
               */
            if (strcmp(equalSign, "=") != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL,
                    NULL, 0, msgBuffer);
                    /* 5223 Invalid SET command */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            strcpy(valueSpecified, fileName);

            /* Determine if the tid is a socket id (id) or a thread name (tid) */
            strcpy(workerID_or_tid,"socket ID "); /* assume socket id in message */
            if (sscanf(tid,"%d%s",&value, &trail) != 1){
               /* Not a socket id (id), assume its a thread name, find its workerID */
               findWorker_by_RDMS_threadname(tid, &value);
               strcpy(workerID_or_tid,"RDMS thread name "); /* assume thread name in message*/
            }

            if ((value <= 0)
                && (value != magicSocketID)) {

                    /* Special negative constant causes the first client
                       with a non-zero socket ID to be picked.
                       This is for development only,
                       and is not documented for users.

                       For the XA Server case,
                       the command has no ID field,
                       but we substituted the magicSocketID value above,
                       so as to get the first (i.e., only) WDE entry.
                       */

                msg2 = getLocalizedMessageServer(CH_INVALID_WORKER_ID,
                    workerID_or_tid, tid, 0, msgBuffer);
                    /* 5203 Client {0} {1} not found */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

            /* We have valid command syntax.
               Now check for a valid worker ID.
               This is in H1 (the upper 18 bits) in the WDE's socket ID word
               (client_socket), specified in decimal.
               This id value is listed under "server work socket id",
               which is the first line of output for each server worker,
               as generated by the console command DISPLAY WORKER STATUS.

               For the XA case, simply return the only WDE entry.
               */

            status = findWorker(&value, &wdePtr);

            if (status != 0) {
                msg2 = getLocalizedMessageServer(CH_INVALID_WORKER_ID,
                    workerID_or_tid, tid, 0, msgBuffer);
                    /* 5203 Client {0} {1} not found */
                strcpy(sgd.textMessage, msg2);
                return 1;
            }

#ifdef XABUILD /* XA Server */

            strcpy(insertParm, "CLIENT TRACE FILE");

#else /* Local Server */

            sprintf(insertParm, "CLIENT %s TRACE FILE", tid);

#endif

            /* Close the old trace file if one is open */
            if (wdePtr->clientTraceFile != NULL) {
                closeClientTraceFile(wdePtr, tempMsg, MESSAGE_MAX_SIZE);
            }

            /* Set the new Client trace file to use. */
            strcpy(wdePtr->clientTraceFileName, fileName);

            /* Open the specified trace file for this client.
             * The will not be cycled and will be erased.
             */
            openClientTraceFile(TRUE, wdePtr, tempMsg, MESSAGE_MAX_SIZE);
            /* set flag to indicate trace file set by console commands */
            wdePtr->consoleSetClientTraceFile = TRUE;

            /* Call a utility to place the initial output in the
               client trace file */
            clientTraceFileInit(wdePtr);

        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL,
                0, msgBuffer);
                /* 5223 Invalid SET command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

    } else {
        msg2 = getLocalizedMessageServer(CH_INVALID_SET_CMD, NULL, NULL, 0, msgBuffer);
            /* 5223 Invalid SET command */
        strcpy(sgd.textMessage, msg2);
        errorStatus = 1;
        return 1;
    }

    /* Generate a reply message.

       - Always generate a reply message which will be printed by the caller
         (in the run which sends the @@CONS command).

       - If the command was successful, generate the following messages:

       - Generate a reply message to stdout (in the server run).
       - Generate a reply message in the log file
         (unless flag sgd.logConsoleOutput is TRUE,
         in which case the console output is
         already being sent to the log file).
         addServerLogEntry's logIndicator argument controls the
         server log file and stdout.
       - If we have the command 'SET CLIENT {id|tid} TRACE FILE=filename',
         add an entry to the client trace file.

       This message is in sgd.textMessage, which is printed by the caller.

       Case 1: Generate a console message for the case where the value was
       valid (i.e., the sgd value was changed):
          <config-param-name> was changed to <new-value>
       */
    if (errorStatus == 0) {
        errMsg = getLocalizedMessage(CH_CONFIG_PARAM_CHANGED, insertParm,
            valueSpecified, 0, msgBuffer);
                /* 5226 {0} parameter was changed to {1} */

        /* Eliminate the message number at the start of the message */
        checkForDigit(sgd.textMessage, errMsg);

        if (sgd.logConsoleOutput) {
            logIndicator = TO_SERVER_STDOUT; /* stdout only */
        } else {
            logIndicator = SERVER_LOGS; /* log file and stdout */
        }

        addServerLogEntry(logIndicator, sgd.textMessage);

        if (wdePtr != NULL) {
            addClientTraceEntry(wdePtr, TO_CLIENT_TRACEFILE, sgd.textMessage);
        }

    /* Case 2: Generate a console message for the case where the value was
       invalid (i.e., the sgd value was not changed).
        */
    } else {
        errMsg = getLocalizedMessage(JDBCSERVER_VALIDATE_GENERIC_ERROR,
            valueSpecified, insertParm, 0, msgBuffer);
                /* 5141 Invalid value {0} specified for {1} */
        strcpy(sgd.textMessage, errMsg);
    }

    /* Return 1, which tells the caller to print the message in
       sgd.textMessage
       */
    return 1;

} /* processSetCmd */

/**
 * Function processClearCmd
 *
 * Process the console handler CLEAR command:
 *
 *   CLEAR SERVER COUNTS
 *
 * This command clears the following sgd variables:
 *   clientCount
 *   requestCount
 *   lastRequestTimeStamp
 *   lastRequestTaskCode
 *
 * These values are displayed by DISPLAY SERVER STATUS.
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

int processClearCmd(char * msg) {

    char * msg2;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    if (strcmp(msg,"CLEAR SERVER COUNTS") != 0) {
        msg2 = getLocalizedMessageServer(CH_INVALID_CLEAR_CMD, NULL, NULL, 0, msgBuffer);
            /* 5250 Invalid CLEAR command */
        strcpy(sgd.textMessage, msg2);
        return 1;
    }

    /* Clear the counts */
    sgd.clientCount = 0;
    sgd.requestCount = 0;
    sgd.lastRequestTimeStamp = 0;
    sgd.lastRequestTaskCode = 0;

    /* Set up message to send back */
    sprintf(sgd.textMessage, "Counts have been cleared");

    /* Return a status indicating that the text message has been set up */
    return 1;

} /* processClearCmd */

/**
 * addSpaceAroundEqual
 *
 * This function adds a space on each side of the '=' in the input string.
 * The output string returned is the input string with the above substitution.
 * If the input string has no '=', NULL is returned.
 *
 * @param input
 *   The input string (i.e., the console command).
 *
 * @param output
 *   A char array where the output string is placed.
 *   This is the input string with the substitution.
 *
 * @return
 *   The output string is returned, unless the input has no '='.
 *   NULL is returned in that case.
 */

char * addSpaceAroundEqual(char * input, char * output) {
    int len;
    char * ret;
    int pos;

    len = strlen(input);

    /* __memccpy(void * s1, const void * s2, int c, size_t num_bytes)
       does the following:
       Copies bytes from s2 into s1, stopping after the first occurence of c
       has been copied, or after num_bytes have been copied
       (whicheven comes first).
       It must be called as an inline __memccpy (i.e., memccpy does not exist).
       It returns a pointer to the first byte after the copy of c in s1.
       NULL is returned is c is not found in the furst num_byte bytes.
       */

    ret = __memccpy(output, input, '=', len);

    if (ret == NULL) {
        return ret; /* No '=' found in the input string */
    }

    /* Calculate the position in the input string of the
       first character after the '=' */
    pos = ret - output;

    /* Replace "=" in the output with " = " */
    strcpy(ret-1, " = ");

    /* Move the string after the '=' in input to the output */
    strcpy(ret+2, input+pos);

    return output;
} /* addSpaceAroundEqual */
