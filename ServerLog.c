/**
 * File: ServerLog.c.
 *
 * Procedures that establish and modify the JDBC Server's Log File.
 *
 * This code may utilize other code in the C-Interface library to accomplish
 * its required tasks.
 *
 */
/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <universal.h>

/* JDBC Project Files */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "ProcessTask.h"
#include "Server.h"
#include "ServerLog.h"
#include "ServiceUtils.h"
#include "ipv6-parser.h"

extern serverGlobalData sgd;        /* The Server Global Data (SGD),*/
                                             /* visible to all Server activities. */

/*
 * Now declare an extern reference to the activity local WDE pointer.
 *
 * This is necessary since some of the log functions and procedures here are
 * being utilized by the JDBCServer/ConsoleHandler, ICL and Server Worker
 * activities and those functions need specific information to tailor their
 * actions (i.e. localization of messages).  The design of those routines utilized
 * fields in Server workers WDE to provide this tailoring information, and the
 * WDE pointer required is not passed to those routines but is assumed to be in
 * the activity local (file level) variable.
 */
extern workerDescriptionEntry *act_wdePtr;

 /* Check if the following function/procedure is really used. */

 /*
  * Function openServerLogFile
  *
  * This routine opens the JDBC Server's Log file.  Once open,
  * it writes a set of header images into this file describing
  * the initial state, from a log perspective, of the JDBC
  * Server.
  *
  * If there are any errors opening the Log file, an error
  * status and error message is returned.
  */

int openServerLogFile(char * error_message, int error_len){

    int error_status=0; /* initialize to: no error state */
    int useErr;
    char * msg2;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Attempt to open the Log file., if not already done. */
   if ( sgd.serverLogFile == NULL){
       sgd.serverLogFile=fopenWithErase(sgd.serverLogFileName);
   }
   /* Test if we got the file open'ed */
    if (sgd.serverLogFile == NULL){
        /* couldn't open the log file, return an error message
         * and error status - the caller will handle them.
         */
        msg2 = getLocalizedMessageServer(JDBCSERVER_CANT_OPEN_SERVER_LOGFILE,
            sgd.serverLogFileName, NULL, 0, msgBuffer);
                /* 5014 Unable to open the JDBC Server Log File {0} */
        strcpy(error_message, msg2);
        error_status=JDBCSERVER_CANT_OPEN_SERVER_LOGFILE;

    } else {
        /* Do an @USE for the log file */
        sprintf(sgd.textMessage, "@USE %s,%s . ", LOG_FILE_USE_NAME,
            sgd.serverLogFileName);
        useErr = doACSF(sgd.textMessage);

        if (useErr < 0) {
            /*
             * If we can't get @use name it might affect console
             * commands, but the file is open'ed.
             */
            getLocalizedMessageServer(SERVER_LOG_FILE_USE_FAILED,
                itoa(useErr, digits, 8), NULL, SERVER_LOGS, msgBuffer);
                    /* 5332 @USE on server log file name failed; octal status {0} */
        }
    }

   return error_status;
}

 /* Check if the following function/procedure is really used. */

 /*
  * Function openServerTraceFile
  *
  * This routine opens the JDBC Server's Trace file.  Once open,
  * it writes a set of header images into this file describing
  * the initial state, from a trace perspective, of the JDBC
  * Server.
  *
  * If there are any errors opening the trace file, an error
  * status and error message is returned.
  */

int openServerTraceFile(char * error_message, int error_len){

    int error_status=0; /* initialize to: no error state */
    int useErr;
    char * msg2;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Attempt to open the Trace file., if not already done. */
    if ( sgd.serverTraceFile == NULL){
      sgd.serverTraceFile=fopenWithErase(sgd.serverTraceFileName);
    }

   /* Test if we got the file open'ed */
    if (sgd.serverTraceFile == NULL){
        /* couldn't open the trace file, return an error message
         * and error status - the caller will handle them.
         */
        msg2 = getLocalizedMessageServer(JDBCSERVER_CANT_OPEN_SERVER_TRACEFILE,
            sgd.serverTraceFileName, NULL, 0, msgBuffer);
                /* 5015 Unable to open the JDBC Server Trace File {0} */
        strcpy(error_message, msg2);
        error_status=JDBCSERVER_CANT_OPEN_SERVER_TRACEFILE;

    } else {

        /* Do an @USE for the trace file */
        sprintf(sgd.textMessage, "@USE %s,%s . ", TRACE_FILE_USE_NAME,
            sgd.serverTraceFileName);
        useErr = doACSF(sgd.textMessage);

        if (useErr < 0) {
            getLocalizedMessageServer(SERVER_TRACE_FILE_USE_FAILED,
                itoa(useErr, digits, 8), NULL, SERVER_LOGS, msgBuffer);
                    /* 5333 @USE on server trace file name failed; octal status {0} */
        }
    }
   return error_status;
}

 /* Check if the following function/procedure is really used. */

 /*
  * Function closeServerFiles
  *
  * This routine closes both the JDBC Server's Log and Trace files.  Once
  * closed, no more log or trace entries can be written until other log
  * and trace files are opened.
  *
  * If there are any errors closing either the Log or Trace file, an error
  * status code is returned.  The close status for both the Log and Trace
  * files is always returned.
  */

int closeServerFiles(int * logFileStatus, int * traceFileStatus){

    /* Is there a Log file, if not, nothing to close, else close it */
    if (sgd.serverLogFile == NULL){
        *logFileStatus=0;
    }
    else{
        /*
         * Close file under T/S control in case other activities are
         * trying to write to file.
         */
        test_set(&(sgd.serverLogFileTS));  /* Get control of the Log file's T/S cell */
        *logFileStatus=fclose(sgd.serverLogFile);
        if (*logFileStatus ==0){
            /* File closed, clear sgd file handle */
            sgd.serverLogFile=NULL;
            }
        ts_clear_act(&(sgd.serverLogFileTS));
    }

   /* Is there a Trace file, if not, nothing to close, else close it */
    if (sgd.serverTraceFile == NULL){
        *traceFileStatus=0;
    }
    else{
        /*
         * Close file under T/S control in case other activities are
         * trying to write to file.
         */
        test_set(&(sgd.serverTraceFileTS));  /* Get control of the Trace file's T/S cell */
        *traceFileStatus=fclose(sgd.serverTraceFile);
        if (*traceFileStatus ==0){
            /* File closed, clear sgd file handle */
            sgd.serverTraceFile=NULL;
            }
        ts_clear_act(&(sgd.serverTraceFileTS));
    }

    /* Determine if an error occurred closing either file - if so return error code */
    if (*logFileStatus !=0 || *traceFileStatus !=0) {
        return CH_LOG_TRACE_FILE_CLOSE_ERROR;
    }
    else{
        return 0; /* all file properly closed */
    }

}

 /* Check if the following function/procedure is really used. */

 /*
  * Function closeServerLogFile
  *
  * This routine closes the JDBC Server's Log file.  Once closed,
  * no more log entries can be written until another log file is
  * opened.
  *
  * If there are any errors closing the Log file, an error
  * status and error message is returned.
  */

int closeServerLogFile(char * error_message, int error_len){

    int error_status=0; /* initialize to: no error state */
    char * msg2;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* First, is there a Log file, if not we're done */
    if (sgd.serverLogFile == NULL){
        return 0; /* no file to close */
    }

    /* Attempt to close the Log file, do it under T/S control.*/
    test_set(&(sgd.serverLogFileTS));  /* Get control of the Log file's T/S cell */
    error_status=fclose(sgd.serverLogFile);
    if (error_status ==0){
       /* File closed, clear sgd file handle */
       sgd.serverLogFile=NULL;
       }
    ts_clear_act(&(sgd.serverLogFileTS));

    if (error_status != 0){
        /* couldn't close the log file, return an error message
         * and error status - the caller will handle them.
         */
        msg2 = getLocalizedMessageServer(JDBCSERVER_CANT_CLOSE_SERVER_LOGFILE,
            sgd.serverLogFileName, itoa(error_status, digits, 8), 0, msgBuffer);
                /* 5016 Unable to close the JDBC Server Log File {0},
                   octal status {1} */
        strcpy(error_message, msg2);
        return JDBCSERVER_CANT_CLOSE_SERVER_LOGFILE;
    }
   return error_status;
}

 /*
  * Function closeServerTraceFile
  *
  * This routine closes the JDBC Server's Trace file.  Once closed,
  * no more Trace entries can be written until another Trace file is
  * opened.
  *
  * If there are any errors closing the Trace file, an error
  * status and error message is returned.
  */

int closeServerTraceFile(char * error_message, int error_len){

    int error_status=0; /* initialize to: no error state */
    char * msg2;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* First, is there a Trace file, if not we're done */
    if (sgd.serverTraceFile == NULL){
        return 0; /* no file to close */
    }
    /* Attempt to close the Trace file, do it under T/S control. */
    test_set(&(sgd.serverTraceFileTS));  /* Get control of the Trace file's T/S cell */
    error_status=fclose(sgd.serverTraceFile);
    if (error_status ==0){
       /* File closed, clear sgd file handle */
       sgd.serverTraceFile=NULL;
       }
    ts_clear_act(&(sgd.serverTraceFileTS));

    if (error_status != 0){
        /* couldn't close the log file, return an error message
         * and error status - the caller will handle them.
         */
        msg2 = getLocalizedMessageServer(JDBCSERVER_CANT_CLOSE_SERVER_TRACEFILE,
            sgd.serverTraceFileName, itoa(error_status, digits, 8), 0, msgBuffer);
                /* 5017 Unable to close the JDBC Server Trace File {0},
                   octal status {1} */
        strcpy(error_message, msg2);
        return JDBCSERVER_CANT_CLOSE_SERVER_TRACEFILE;
    }
    return error_status;
}

/*
 * Function closeClientTraceFile
 *
 * This routine closes the JDBC Client's Trace file.  Once closed,
 * no more Trace entries can be written until another Trace file is
 * opened.
 *
 * This routine will decrement the file usage counter by one. If the counter
 * goes to zero, the file is closed. and removed from the table.  Any errors
 * are logged, but no error status is returned.
 *
 * If there are any errors closing the Trace file, an error
 * status and error message is returned.
 *
 * The following steps describe the execution flow:
 * Step  1: Write close message to trace and log files.
 * Step  2: Set the ClientTraceFile Test&Set cell.  We need to prevent another
 *          activity from trying to use the same trace file at the same time and
 *          messing up the table and usage counter. This whole routine is under
 *          Test&Set control.
 * Step  3: Search the 'clientTraceTable' for a matching filename.
 *          If found, continue with Step 3.
 *          If not found, log an error, release the T/S and return. We should
 *          never fail this search.
 * Step  4: Decrement the usage count.
 * Step  5: Check if usage count has gone to zero. If the trace file name is
 *          "PRINT$" then go straight to step 5c, otherwise do steps 5a, 5b, and 5c.
 *       5a: Do a fclose on the filename.
 *       5b: Do a @FREE of the file.
 *       5c: Clear the entry in the clientTraceTable
 * Step  6: Release the T/S lock
 */
int closeClientTraceFile(workerDescriptionEntry *wdePtr
                         , char * error_message, int error_len){

    int error_status=0; /* initialize to: no error state */
    char * msg2;
    char digits[ITOA_BUFFERSIZE];
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char msgInsert[500];
    char ACSFString[MAX_ACSF_STRING_SIZE+1];
    int index;

    /* First, is there a Trace file, if not we're done */
    if (wdePtr->clientTraceFile == NULL){
        return 0; /* no file to close */
    }

    /*
     * Step  1: Write close message to trace and log files.
     *          Place a trace file closing message into the Server logs
     *          and the client's trace file. Generate a message with the
     *          client trace file name and the socket ID.
     *          Note: We can't call getLocalizedMessage, since it allows
     *          only two inserts, and this message has three inserts.
     */
    sprintf(message, "<-Closing Client trace to file %s at ", wdePtr->clientTraceFileName);
    addDateTime(message);
    sprintf(msgInsert, "for JDBC client on socket %d", wdePtr->client_socket);
    strcat(message, msgInsert);

    /* Add the message to the server log file and client trace file */
    addServerLogEntry(TO_SERVER_LOGFILE, message);
    tracef("%s\n\n", message);

    /*
     * Step  2: Set the ClientTraceFile Test&Set cell.  We need to prevent
     *          another activity from trying to use the same trace file at
     *          the same time and messing up the table and usage counter.
     *          The rest of the routine is under Test&Set control.
     */
    test_set(&(sgd.clientTraceFileTS));  /* global Client Trace file T/S cell */

    /*
     * Step  3: Search the 'clientTraceTable' for a matching filename.
     *          We should always find a match. If not Log it and continue.
     */
    index = 0;
    while (index < sgd.maxServerWorkers) {
        if (strcmp(sgd.clientTraceTable[index].fileName, wdePtr->clientTraceFileName) == 0) {
            /* Match found - exit loop */
            break;
        }
        index = index +1;
    }
    /* Check if we found a match */
    if (index >= sgd.maxServerWorkers) {
        /* couldn't find entry in clientTraceTable, return an error
         * message and error status - the caller will handle them.
         * This should never happen.
         */
        msg2 = getLocalizedMessageServer(JDBCSERVER_CLOSE_CLIENT_TRACETABLE,
                wdePtr->clientTraceFileName, NULL, 0, message);
            /*  5031 Server internal error - cannot find table entry for {0} */
        strcpy(error_message, msg2);
        ts_clear_act(&(sgd.clientTraceFileTS)); /* Release client trace file T/S */
        return JDBCSERVER_CLOSE_CLIENT_TRACETABLE;
    }

    /*
     * Step  4: Decrement the usage count.
     */
    sgd.clientTraceTable[index].usageCount = sgd.clientTraceTable[index].usageCount -1;

    /*
     * Step  5: Check if usage count has gone to zero. If the trace file name is
     *          "PRINT$" then go straight to step 5c, otherwise do steps 5a, 5b, and 5c.
     *       5a: Do a fclose on the filename.
     *       5b: Do a @FREE of the file.
     *       5c: Clear the entry in the clientTraceTable & WDE table
     */
    if (sgd.clientTraceTable[index].usageCount == 0) {
        /* Is the trace file name "PRINT$"? If so, go to step 5c.*/
        if (strcmp(wdePtr->clientTraceFileName, PRINT$_FILE) == 0){
            goto Step5c;
        }

        /* Step 5a: No one else is using the trace file, so lets close it. */
        error_status=fclose(wdePtr->clientTraceFile);
        if (error_status != 0){
            /* couldn't close the client trace file, return an error
             * message and error status - the caller will handle them.
             */
            msg2 = getLocalizedMessageServer(JDBCSERVER_CANT_CLOSE_CLIENT_TRACEFILE,
                    wdePtr->clientTraceFileName, itoa(error_status, digits, 8), 0, message);
                /*  5019 Unable to close JDBC Client's server-side
                    trace file {0}, octal status {1} */
            strcpy(error_message, msg2);
            ts_clear_act(&(sgd.clientTraceFileTS)); /* Release client trace file T/S */
            return JDBCSERVER_CANT_CLOSE_CLIENT_TRACEFILE;
        }
        /* Step 5b: We need to free the file assignment.  @FREE <filename>    */
        sprintf(ACSFString, "@FREE %s . ", wdePtr->clientTraceFileName);
        error_status = doACSF(ACSFString);      /* ignore status on the free */
        /* Step 5c: Clear the entry in the clientTraceTable & the WDE */
Step5c:
        sgd.clientTraceTable[index].file = NULL;
        sgd.clientTraceTable[index].fileName[0] = '\0';
        sgd.clientTraceTable[index].fileNbr = 0;
        wdePtr->clientTraceFile=NULL;
    }
    /*
     * Step  6: Release the T/S lock & return
     */
    ts_clear_act(&(sgd.clientTraceFileTS)); /* Release client trace file T/S */
    return 0;
}

/*
 * Function openClientTraceFile
 *
 * This routine will check if the requested fileName is already open. If it is,
 * then the file handle and file number is returned and the file usage counter
 * is bumped by one.  If the file is not open, then it is cataloged and opened
 * using the erase flag. The file usage counter is also bumped by one.
 * If there is an error, a non-zero error number is returned and the error is
 * logged.
 *
 * Parameters:
 *      eraseFlag     IN  = 0 - open file in 'append' mode;
 *                        <>0 - open file with erase.
 *      wdePtr        IN  pointer to WDE struture for this client.
 *      error_message OUT generated error message
 *      error_len     IN  maximum size of the error_message buffer.
 * Return value: error code value (0 if no errors)
 *
 * The following steps show the execution flow:
 * Step  1: Set the ClientTraceFile Test&Set cell.  We need to prevent another
 *          activity from trying to open the same trace file at the same time
 *          and messing up the table and usage counter. This whole routine is
 *          under Test&Set control.
 * Step 1a: If the trace file name is "PRINT$" then skip to step 6 since the
 *          PRINT$ file is always open and stdout's FILE reference  will be used.
 * Step  2: Proceed with trying to ASG the trace file for exclusive use.
 *       2a: Try @ASG,AXZ <fileName>
 *           If found, go to step 3.
 *           If not failed, continue with step 3b.
 *       2b: Try @CAT,PZ <fileName>,F///<#Tracks>
 *           If failed, go to error exit.
 *           If okay, continue with step 2c.
 *       2c: Try again @ASG,AXZ <fileName>
 *           This time if failed, go to error exit.
 *           Otherwise, contine with step 2d.
 *       2d: Release the exclusive assign, but keep it assigned.
 *           Do @FREE,X <filename>
 * Step  3: Determine the absolute cycle number of the trace file.
 *          If the provide file already has cycle info, then skip this step.
 *       3a: Bump the clientTraceFileCounter under T/S control.
 *       3b: Do an @USE JDBC$Cnnn,<filename>.  The 'nnn' is a unique number
 *           obtained from the clientTraceFileCounter bumped under T/S.
 *       3c: Get the F-cycle number from a call to ER FITEM$ and add it
 * Step  4: Search the 'clientTraceTable' for a matching filename.
 *          If found, contine with Step 7.
 *          If not found, continue with Step 5
 * Step  5: Open the trace file for writing. If the erase flag is set,
 *          open the the file for 'w'riting at the start of the file.
 *          If the erase flag is not set, open the file for 'a'ppend mode.
 * Step  6: Find the next free slot in the 'clientTraceTable' and add an entry.
 * Step  7: Valid entry - Bump the usage counter.
 * Step  8: Set the trace file info (File handle, file name, and file 'Nbr') in
 *          the client WDE structure.
 * Step  9: Release the ClientTraceFile Test&Set cell and return
 *
 */
int openClientTraceFile(int eraseFlag, workerDescriptionEntry *wdePtr,
                       char * error_message, int error_len) {

    int status=0; /* initialize to: no error state */
    int absFCycle;
    FILE * tempFile;
    char * msg2;
    char * filenamePtr;
    char ACSFString[MAX_ACSF_STRING_SIZE+1];
    char useName[MAX_FILE_NAME_LEN+1];
    char traceFileName[MAX_FILE_NAME_LEN+1];    /* Contains the trace file name */
    char insert1[100];  /* big enough for two integers enclosed in parenthesis and separated by a comma */
    char step[3];   /* 2-char step indicator plus a null terminator */
    int tempFileNbr;
    int index;
    int found;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Save a copy of the trace filename */
    strcpy(traceFileName, wdePtr->clientTraceFileName);

    /* Check if we already have one opened. If so, skip everything and return. */
    if ( wdePtr->clientTraceFile != NULL){
        return 0;
    }

    /* There is a client trace file name. Remove
       any file cycle information it may have. */
    filenamePtr=strtok(traceFileName,"("); /* find the file-cycle spec. */
    if (filenamePtr != NULL){
      /* Reset to only the filename part. */
      strcpy(traceFileName, filenamePtr);
    }

    /*
     * Step  1: Set the ClientTraceFile Test&Set cell.  We need to prevent another
     *          activity from trying to open the same trace file at the same time and
     *          messing up the table and usage counter. This whole routine is under
     *          Test&Set control.
     */
    test_set(&(sgd.clientTraceFileTS));  /* Get global Client Trace file T/S cell */

    /*
     * Step 1a: Is the trace file "PRINT$"? If so, then it is already open,
     *          so set the tempFile reference to stdout and jump to step 6.
     */
    if (strcmp(traceFileName, PRINT$_FILE) == 0){
    	tempFile = stdout;
    	goto Step6;
    }
    /*
     * Step  2: Proceed with trying to ASG the trace file.
     *          This step should result in an assigned file or an error exit.
     *          Note: We do not try to strip any cycle info from the filename.
     *          It's okay for the requested filename to have cycle info.
     *       2a: First try to see if the file exists using:
     *           @ASG,AZ <fileName>
     *           If found, go to step 3.
     *           If not failed, continue with step 2b.
     *       2b: Try @CAT,PZ <fileName>,F///<#Tracks>
     *           If failed, go to error exit.
     *           If okay, continue with step 3c.
     *       2c: Try again @ASG,AZ <fileName>
     *           This time if failed, go to error exit.
     *           Otherwise, contine with step 2d.
     *       2d: Release the exclusive assign, but keep it assigned.
     *           Do @FREE,X <filename>
     */
    sprintf(ACSFString, "@ASG,AXZ %s . ", traceFileName);
    status = doACSF(ACSFString);
    /* Test if @ASG,AXZ was rejected for any reason. */
    if (status < 0) {
        /* Step 2b:
         * Okay, let's try to catalog the file.  The assign may have been
         * rejected for many reasons, but it is easier to try creating it,
         * then to decode the various reasons for failure. If the catalog
         * fails, then we will error exit any way.
         */
        sprintf(ACSFString, "@CAT,PZ %s,F///%i . ", traceFileName,
                sgd.clientTraceFileTracks);
        status = doACSF(ACSFString);
        /* Test if @CAT,PZ was rejected for any reason. */
        if (status < 0) {
            strcpy(step,"2b");
            goto errorExit;
        }
        /* Step 2c:
         * Okay the catalog worked, so let's assign the file. This
         * should work, since we just cataloged it.
         */
        sprintf(ACSFString, "@ASG,AXZ %s . ", traceFileName);
        status = doACSF(ACSFString);
        /* Test if @ASG,AXZ was rejected for any reason. */
        if (status < 0) {
            strcpy(step,"2c");
            goto errorExit;
        }
        /* Step 2d:
         * Okay we have the file assigned exclusively to us. We
         * can release the exclusive part.
         */
        sprintf(ACSFString, "@FREE,X %s . ", traceFileName);
        status = doACSF(ACSFString);
        /* Test if @FREE,X was rejected for any reason. */
        if (status < 0) {
            strcpy(step,"2d");
            goto errorExit;
        }
    }

    /*
     * Step  3: Determine the absolute cycle number of the trace file.
     *          If the provide file already has cycle info, then skip this step.
     *       3a: Bump the clientTraceFileCounter under T/S control.
     *       3b: Do an @USE JDBC$Cnnn,<filename>.  The 'nnn' is a unique number
     *           obtained from the clientTraceFileCounter bumped under T/S.
     *       3c: Get the F-cycle number from a call to ER FITEM$ and add it
     *           to the filename.
     */
    if (strchr(traceFileName,'(')== NULL) {
        /* Step 3a:
         * Bump the counter under T/S control. Get T/S cell for server log file,
         * since the client trace file does not have its own T/S cell.
         */
        test_set(&(sgd.serverLogFileTS));
          sgd.clientTraceFileCounter++;
          tempFileNbr = sgd.clientTraceFileCounter;
        ts_clear_act(&(sgd.serverLogFileTS)); /* Clear T/S cell */
        /* Step 3b: Form the usename */
        sprintf(useName, "%s%d",CLIENT_TRACE_FILE_USE_NAME_PREFIX, tempFileNbr);
        sprintf(ACSFString, "@USE %s,%s . ",useName, traceFileName);
        status = doACSF(ACSFString);
        /* Test if @USE worked. It should always work, but check status anyway. */
        if (status < 0) {
            strcpy(step,"3b");
            goto errorExit;
        }
        /* Step 3c: Get the file cycle number and add it to the filename. */
        absFCycle = getFileCycle(useName);
        sprintf(insert1,"(%i)", absFCycle);
        strcat(traceFileName, insert1);
    }

    /*
     * Step  4: Search the 'clientTraceTable' for a matching filename.
     *          If found, contine with Step 7.
     *          If not found, continue with Step 5
     */
    index = 0;
    found = 0;
    while (index < sgd.maxServerWorkers) {
        if (strcmp(sgd.clientTraceTable[index].fileName, traceFileName) == 0) {
            /* Match found - exit loop */
            found = -1; /* flag as found */
            goto matchFound;    /* go to Step 7 */
        }
        index = index +1;
    }

    /*
     * Step  5: Open the trace file for writing. If the erase flag is set,
     *          open the the file for 'w'riting at the start of the file.
     *          If the erase flag is not set, open the file for 'a'ppend mode.
     */
    if (eraseFlag != 0) {
        tempFile = fopenWithErase(traceFileName);
    } else {
        tempFile = fopen(traceFileName, "a");
    }
    status = errno;             /* save the error status for the last fopen() */
    /* Test if we got the file open'ed */
    if (tempFile == NULL){
        /* couldn't open the trace file, return an error message
         * and error status - the caller will handle them.
         */
        strcpy(step,"5");
        goto errorExitDec;
    }

    /*
     * Step  6: Find the next free slot in the 'clientTraceTable' and add an entry.
     *          Look at the file handle field for a NULL.
     */
Step6:
    index = 0;
    while (index < sgd.maxServerWorkers) {
        if (sgd.clientTraceTable[index].file == NULL) {
            /* Empty entry found - add entry */
            strcpy(sgd.clientTraceTable[index].fileName, traceFileName);    /* file name */
            sgd.clientTraceTable[index].file = tempFile;                    /* file handle */
            sgd.clientTraceTable[index].fileNbr = tempFileNbr;              /* usename number */
            sgd.clientTraceTable[index].usageCount = 0;     /* it will be bumpped later */
            break;    /* exit while loop */
        }
        index = index +1;
    }

    /*
     * Step  7: Valid entry - Bump the usage counter.
     *          Note: index points to either the recently added entry or the original
     *          found entry from step 4.
     */
matchFound:
    sgd.clientTraceTable[index].usageCount = sgd.clientTraceTable[index].usageCount +1;
    /*
     * Step  8: Set the trace file info (File handle, file name, and file 'Nbr') in
     *          the client WDE structure.
     *          Note: Values are copied from the SGD table because they already exist
     *          and the file was not created this time.
     */
    strcpy(wdePtr->clientTraceFileName, sgd.clientTraceTable[index].fileName);
    wdePtr->clientTraceFile = sgd.clientTraceTable[index].file;
    wdePtr->clientTraceFileNbr = sgd.clientTraceTable[index].fileNbr;

    /*
     * Step  9: Release the ClientTraceFile Test&Set cell and return
     */
    ts_clear_act(&(sgd.clientTraceFileTS)); /* Release client trace file T/S */
    return 0;

/* We can not open the trace file for one reason or the other, so let's exit. */
errorExitDec:
    sprintf(insert1,"(%i, step %s)", status, step);
    goto errorExit2;
errorExit:
    sprintf(insert1,"(%o12, step %s)", status, step);
errorExit2:
    msg2 = getLocalizedMessageServer(JDBCSERVER_CANT_OPEN_CLIENT_TRACEFILE2,
        traceFileName, insert1, 0, msgBuffer); /* use the local trace file name */
            /* 5030 Unable to open JDBC
                    Client's server-side trace file {0}, status = (000000000000, step x) */
    if (strlen(msg2) > error_len-1) {
        strncpy(error_message, msg2, error_len-1);
        error_message[error_len-1] = '\0';
    } else {
        strcpy(error_message, msg2);
    }

    /* Make sure the T/S is released */
    ts_clear_act(&(sgd.clientTraceFileTS)); /* Release client trace file T/S */
    return JDBCSERVER_CANT_OPEN_CLIENT_TRACEFILE2;
}

/*
 * Procedure addServerLogEntry1
 *
 * This procedure adds a new log entry into Server Log and/or Trace Files. It
 * uses addServerLogEntry to do the actual logging.  Two additional parameters
 * provide decimal integer inserts into the message text provided. This is done
 * by using sprintf.  The incoming message must have %d's for the inserts that
 * are actually passed. The incoming message, with the inserts added, must be less
 * than SERVER_ERROR_MESSAGE_BUFFER_LENGTH in number of characters.
 *
 * Parameters:
 *    logIndicator  - an integer value containing a set of indicator bits
 *                    that control the location and content of the log/trace
 *                    file message thats added. If LOG_NOTHING (0), no
 *                    logging is done.
 *    message       - character buffer containing the base message (\0 terminated)
 *
 *    value1        - the first integer to be inserted into the message.
 *
 *    value2        - the second integer to be inserted into the message.
 *
 */
void addServerLogEntry1(int logIndicator, char *log_message, int value1, int value2){

    int message_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;   /* maximum length message can be */
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* a buffer for a message */

    /* Check if logging needs to be done. */
    if (logIndicator != LOG_NOTHING){
        /* create the output message and send it to addServerLogEntry() */
        sprintf(message, log_message, value1, value2);

        /* output message with inserts */
        addServerLogEntry(logIndicator, message);
    }

    /* All done logging entry if needed, return to caller */
    return;
}


/*
 * Procedure addServerLogEntry
 *
 * This procedure adds a new log entry into Server Log and/or Trace Files.  The
 * Log entry can be tailored.  It can be just the message text provided as the
 * procedures parameter, or it can be prefixed with the JDBC Server's and/or by
 * the current time stamp.  This is controlled by the logIndicator parameter.
 *
 *
 * Note: There are some combinations of logIndicator values that do produce log
 * entries when using addServerLogEntry:
 *       - This routine does not log to the JDBC Client's trace file even if
 *         the logIndicator value indicates this, since this routine does not have
 *         a pointer to the Server Workers WDE. The procedure addClientLogEntry
 *         must be used for logging to the JDBC Client's trace file. (Also note
 *         that the routine addClientLogEntry can add log entries to both the
 *         JDBC Client's trace file and the JDBC Server's log file since access
 *         to the SGD is always available.)
 *       - This routine cannot obtain the JDBC Client's name since this routine
 *         does not have a pointer to the appropriate Server Workers WDE.
 *
 * Logging action:
 * If the logging file requested does not exist, no log entry is made.  If a
 * logging entry cannot be added to the designated Log and/or Trace Files,
 * no error message is generated. At a later time we may choose to design in
 * a way to handle the file full case, e.g. we may also want to raise some
 * kind of signal when the log entry cannot be written to the designated Log
 * and/or Trace files.  For now, it is assumed there is enough space in the
 * log files.
 *
 * Concurrency control:
 * All the printfs, puts, etc. are done under T/S control to synchronize the
 * I/O going to the Server log/trace files.  This is needed since there are
 * multiple Server activities (e.g. SW's, ICL, CH) that could try to write to
 * the log/trace files at the same time. Concurrency control for output sent
 * to STDOUT will be done using the serverLogTS cell since this is the usual
 * type of output that is being diverted to STDOUT, and the log file output
 * level is usually lower than the level of trace output should tracing be turned
 * on.
 *
 * Parameters:
 *    logIndicator  - an integer value containing a set of indicator bits
 *                    file message that's added. If LOG_NOTHING (0), no
 *                    logging is done.
 *    message       - character buffer containing the message (\0 terminated)
 *
 */
void addServerLogEntry(int logIndicator, char *message){

    int headerInfo;
    char header[MAXLOGPREFIXLENGTH];

    /* printf("logIndicator=%x\n",logIndicator); */

    /* Check if logging needs to be done. */
    if (logIndicator != LOG_NOTHING){
        /* Reverse the bits in the log indicator ( turn 1"s to 0's and vice versa */
        logIndicator=logIndicator ^ REVERSING_MASK;

        /* Initially assume no header needed */
        headerInfo=BOOL_FALSE;
        header[0]='\0';

        /* Now determine if we will need the JDBC Server's name as message prefix */
        if (logIndicator & WITH_SERVER_NAME_MASK ){
            /* Yes, we need the JDBC Server's name */
           strcat(header,"\"");
           strcat(header,sgd.serverName );
           strcat(header,"\": ");
           headerInfo=BOOL_TRUE;
        }

        /* Now determine if we need a timestamp value since this must be obtained. */
        if (logIndicator & WITH_TIMESTAMP_MASK ){
           /*
            * Yes, we need the timestamp. Call addDateTime() to
            * construct the date/timestamp and add it to header.
            */
           addDateTime(header);
           headerInfo=BOOL_TRUE;
        }

        /*
         * Each I/O operation will be done under a T/S control to force
         * synchronization of attempted concurrent I/O's to the file
         */

        /* Now determine to which files a log entry should be made. */
        if (logIndicator & TO_SERVER_LOGFILE_MASK){
              test_set(&(sgd.serverLogFileTS));  /* Get control of the Log file's T/S cell */
         /*   fprintf(sgd.serverLogFile,"%s%s\n", header, message); */
              /* If there is a header, print it first */
              if (headerInfo == BOOL_TRUE){
                 fputs(header,sgd.serverLogFile);
                 }
              fputs(message,sgd.serverLogFile);
              fputs("\n",sgd.serverLogFile);

              ts_clear_act(&(sgd.serverLogFileTS));
            }
        if (logIndicator & TO_SERVER_STDOUT_MASK){
              test_set(&(sgd.serverLogFileTS));  /* Get control using the Log file's T/S cell */
         /*    printf("%s%s\n", header, message); */
              /* If there is a header, print it first */
              if (headerInfo == BOOL_TRUE){
                    printf("%s",header);
                 }
               puts(message);
              ts_clear_act(&(sgd.serverLogFileTS));
            }

        if (logIndicator & TO_SERVER_STDOUT_DEBUG_MASK){
            if (sgd.debug) {
                test_set(&(sgd.serverLogFileTS));
                    /* Get control using the Log file's T/S cell */

                /* If there is a header, print it first */
                if (headerInfo == BOOL_TRUE){
                    printf("%s",header);
                }
                puts(message);
                ts_clear_act(&(sgd.serverLogFileTS));
            } /* DEBUG */
        } /* stdout debug case */

        if (logIndicator & TO_SERVER_TRACEFILE_MASK){
              test_set(&(sgd.serverTraceFileTS));  /* Get control of the Trace file's T/S cell */
         /*    fprintf(sgd.serverTraceFile,"%s%s\n", header, message); */
              /* If there is a header, print it first */
              if (headerInfo == BOOL_TRUE){
                 fputs(header,sgd.serverTraceFile);
                 }
               fputs(message,sgd.serverTraceFile);
               fputs("\n",sgd.serverTraceFile);
              ts_clear_act(&(sgd.serverTraceFileTS));
            }
    }

    /* All done logging entry if needed, return to caller */
    return;
}

/*
 * Procedure addClientTraceEntry
 *
 * This procedure adds a new log entry into the Client Trace Files.  The Log
 * entry can be tailored.  It can be just the message text provided as the
 * procedures parameter, or it can be prefixed with the JDBC Client's name,
 * and/or suffixed by the current time stamp.  This is controlled by the
 * logIndicator parameter.
 *
 *
 * Note: There are some combinations of logIndicator values that also produce
 * log entries into the JDBCServer's Log and Trace files.  This is particularly
 * important when a log and trace entry needs to be made in both the Server and
 * Client trace files: each log entry must have the same timestamp value.  This
 * might not be the case if separate calls to addServerLogEntry and
 * addClientTraceEntry were done.
 *
 * Logging action:
 * If the logging file requested does not exist, no log entry is made.  If a
 * logging entry cannot be added to the designated Log and/or Trace Files, no
 * error message is generated. At a later time we may choose to design in a way
 * to handle the file full case, e.g. we may also want to raise some kind of
 * signal when the log entry cannot be written to the designated Log and/or Trace
 * files.  For now, it is assumed there is enough space in the log files.
 *
 *
 * Parameters:
 *    wdePtr        - pointer to the workerDescriptionEntry for this JDBC Client.
 *                    This will be used to obtain the JDBC Client Trace file
 *                    reference.
 *    logIndicator  - an integer value containing a set of indicator bits
 *                    that control the location and content of the log/trace
 *                    file message that's added. If LOG_NOTHING (0), no
 *                    logging is done.
 *    message       - character buffer containing the message (\0 terminated)
 *
 * Concurrency control:
 * Since we can have only one Server worker activity handling a given JDBC client,
 * there is no concurrent usage of this routine if the output is sent only to the
 * Clients own trace file - in this case no T/S synchronization is not
 * required. If output is diverted to the Server's Log or Trace files, or STDOUT,
 * then the Server's Log/Trace file T/S cells are used to synchronize output.
 */
void addClientTraceEntry(workerDescriptionEntry *wdePtr,int logIndicator
                       , char *message){

    int headerInfo;
    char header[MAXLOGPREFIXLENGTH]="\0";  /* initially cleared for copying */

    /* Check if logging needs to be done. */
    if (logIndicator != LOG_NOTHING){
        /* Reverse the bits in the log indicator ( turn 1"s to 0's and vice versa */
        logIndicator=logIndicator ^ REVERSING_MASK;

        /* Initially assume no header needed */
        headerInfo=BOOL_FALSE;
        header[0]='\0';

        /* Now determine if we need the JDBC Server's name to prefix the message */
        if (logIndicator & WITH_SERVER_NAME_MASK){
            /* Yes, we need the JDBC Server's name */
            strcpy(header,"\n**JDBC Server \"");
            strcat(header,sgd.serverName );
            strcat(header,"\", ");

            /* Do we also need the JDBC Client's user id? */
            if (logIndicator & WITH_CLIENT_NAME_MASK){
               /* Yes, we need the JDBC CLIENT's name */
               strcat(header,"JDBC Client '");
               strcat(header,wdePtr->client_Userid);
               strcat(header,"': ");
            }
           headerInfo=BOOL_TRUE;
        }
        else {
            /* No, determine if we need the JDBC Client's name as message prefix */
            if (logIndicator & WITH_CLIENT_NAME_MASK){
               /* Yes, we need the JDBC CLIENT's name */
               strcat(header,"\n**JDBC Client '");
               strcat(header,wdePtr->client_Userid);
               strcat(header,"': ");
               headerInfo=BOOL_TRUE;
            }
        }

        /* Now determine if we need a timestamp value since this must be obtained. */
        if (logIndicator & WITH_TIMESTAMP_MASK ){
           /*
            * Yes, we need the timestamp. Call addDateTime() to
            * construct the date/timestamp and add it to header.
            */
           addDateTime(header);
           headerInfo=BOOL_TRUE;
        }

        /* Now determine to which files a log entry should be made.  For the
        * Server Log and Trace files, any name and timestamp information is in
        * the header variable so just write the entry to the Log or Trace file.
         */
        if (logIndicator & TO_CLIENT_TRACEFILE_MASK){
           test_set(&(sgd.clientTraceFileTS));  /* Get global Client Trace file T/S cell */
           /* If there is a header, print it first */
           if (headerInfo == BOOL_TRUE){
              fprintf(wdePtr->clientTraceFile,"%s", header);
              }
           fprintf(wdePtr->clientTraceFile,"%s\n", message);
           ts_clear_act(&(sgd.clientTraceFileTS)); /* Release client trace file T/S */
        }
        if (logIndicator & TO_SERVER_LOGFILE_MASK){
           test_set(&(sgd.serverLogFileTS));  /* Get control of the Trace file's T/S cell */
           /* If there is a header, print it first */
           if (headerInfo == BOOL_TRUE){
              fprintf(sgd.serverLogFile,"%s", header);
              }
           fprintf(sgd.serverLogFile,"%s\n", message);
           ts_clear_act(&(sgd.serverLogFileTS));
        }
        if (logIndicator & TO_SERVER_STDOUT_MASK){
           test_set(&(sgd.serverLogFileTS));  /* Get control of the Trace file's T/S cell */
           /* If there is a header, print it first */
           if (headerInfo == BOOL_TRUE){
              printf("%s", header);
              }
           printf("%s\n", message);
           ts_clear_act(&(sgd.serverLogFileTS));
        }

        if (logIndicator & TO_SERVER_STDOUT_DEBUG_MASK){
            if (sgd.debug) {
                test_set(&(sgd.serverLogFileTS));
                    /* Get control using the Log file's T/S cell */

                /* If there is a header, print it first */
                if (headerInfo == BOOL_TRUE){
                  printf("%s", header);
                }
                puts(message);
                ts_clear_act(&(sgd.serverLogFileTS));
            } /* DEBUG */
        } /* stdout debug case */

        if (logIndicator & TO_SERVER_TRACEFILE_MASK){
           test_set(&(sgd.serverTraceFileTS));  /* Get control of the Trace file's T/S cell */
           /* If there is a header, print it first */
           if (headerInfo == BOOL_TRUE){
             fprintf(sgd.serverTraceFile,"%s", header);
             }
           fprintf(sgd.serverTraceFile,"%s\n", message);
           ts_clear_act(&(sgd.serverTraceFileTS));
        }
    }

    /* All done logging entry if needed, return to caller */
    return;
}

/*
 * Function itoa_IP
 *
 * This function converts an integer value into the character array provided
 * and appends an IP address, user id, and thread id in either the format:
 * " (for IP addr: xxx.xxx.xxx.xxx)"
 * or
 * " (for IP addr: xxx.xxx.xxx.xxx, user id: xxxxxxxx, thread id: xxxxxxx)"
 *
 * Which insert is returned depends on whether there is a user id available
 * or not.
 *
 * Parameters:
 *
 * value    - integer value to convert to base 10 or base 16, or base 8
 * buffer   - a character buffer sufficently large to hold the
 *            converted 2200 integer value into characters plus
 *            sign, null terminator, and the IP address string in
 *            the format " (for IP addr. xxx.xxx.xxx.xxx)".
 * base     - base to convert to: either 8, 10, or 16
 *
 * ip_addr  - IP address to convert
 *
 * user_id  - Points to the user id.  If pointer is NULL, then there is
 *            no user id or thread name to insert into the message.
 *
 * thread_id - Points to the thread name to insert. If user_id is NULL,
 *             the thread_id (present or not) is not copied into the insert.
 *
 * Warning:   A buffer of 110 characters is sufficent and REQUIRED (
 *            always nice to have a bit of extra room).
 */
char * itoa_IP(int value, char * buffer, int base, v4v6addr_type ip_addr,
               char * user_id, char * thread_id){

    char digits[ITOA_BUFFERSIZE];
    char ipaddr_buff[ITOA_BUFFERSIZE];

    /* Convert the value to a string.*/
    itoa(value, digits, base);

    /* copy strings into the buffer provided */
    strcpy(buffer, digits);
    strcat(buffer, " (for IP addr: ");

    /* Convert the IP address to an IPv4 or IPv6 format string */
    inet_ntop((int)ip_addr.family, (const unsigned char *)&ip_addr.v4v6,
              ipaddr_buff, IP_ADDR_CHAR_LEN + 1);

    /* Form additional information based on whether a userid is present. */
    if (user_id == NULL){
        /* User id field is not present, just use the IP address insert. */
        strcat(ipaddr_buff,")");
    } else {
        /* User id field present, copy it and thread name into insert. */
        strcat(ipaddr_buff, ", user id: ");
        strncat(ipaddr_buff, user_id, CHARS_IN_USER_ID);
        strcat(ipaddr_buff, ", thread id: ");
        strncat(ipaddr_buff, thread_id, RDMS_THREADNAME_LEN);
        strcat(ipaddr_buff, ")");
    }

    /* copy string into the buffer provided, and return buffer pointer */
    strcat(buffer, ipaddr_buff);
    return buffer;
}

/**
 * clientTraceFileInit
 *
 * Add output to the current position in the client trace file:
 *   - an initial text message listing the socket id and the filename
 *   - the current sgd (like the console command 'DISPLAY CONFIGURATION ALL')
 *   - the current wde (like the console command
 *     'DISPLAY WORKER id STATUS ALL' for the current worker)
 *
 * In addition, add a text message to the server log file
 * listing the socket id and the filename.
 *
 * @param wdePtr
 *   A pointer to the current WDE entry.
 */

void clientTraceFileInit(workerDescriptionEntry * wdePtr) {

    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char msgInsert[500];

    /* Generate a message with the client trace file name and the socket ID.
       Note: We can't call getLocalizedMessage,
       since it allows only two inserts,
       and this message has three inserts. */
    sprintf(message, "->Opening Client trace to file %s at ", wdePtr->clientTraceFileName);
    addDateTime(message);
    sprintf(msgInsert, "for JDBC client on socket %d", wdePtr->client_socket);
    strcat(message, msgInsert);

    /* Add the message to the server log file and client trace file */
    addServerLogEntry(TO_SERVER_LOGFILE, message);
    tracef("%s\n", message);

    /* Note: we used to dump the sgd and the wde info to the trace file
     * here, but now this has been moved to the BeginThread code in
     * connection.c and only dumped for a new connection.
     */
} /* clientTraceFileInit */
