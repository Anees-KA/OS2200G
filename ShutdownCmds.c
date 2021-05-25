/**
 * File: ShutdownCmds.c.
 *
 * Functions that process the JDBC Server's SHUTDOWN
 * console handling commands.
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
#include <universal.h>
#include "marshal.h"

/* The file s$cw.h can be found in SYS$LIB$*PROC$.  This header file is put
 * there by the SLIB installation (SYS$LIB$*SLIB$).  This provides extended
 * mode utility routines.  In our case, we use it for s_cw_set_task_error()
 * and s_cw_clear_task_error().  SYS$LIB$*PROC$ is on the runtime system search
 * chain.  Therefore, you can include the header file and URTS will resolve it.
 */
#include <s$cw.h>

/* JDBC Project Files */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "ConsoleCmds.h"
#include "NetworkAPI.h"
#include "ProcessTask.h"
#include "Server.h"
#include "ServerConsol.h"
#include "ServerLog.h"
#include "ServiceUtils.h"
#include "ShutdownCmds.h"

/* Imported data */

extern workerDescriptionEntry *act_wdePtr;
extern serverGlobalData sgd;
    /* The Server Global Data (SGD), visible to all Server activities. */

/**
 * Function: processShutdownServerCmd
 *
 * Process the server control commands
 *
 *   SHUTDOWN
 *   SHUTDOWN GR[ACEFULLY]
 *   SHUTDOWN IM[MEDIATELY]
 *   ABORT
 *   TERM
 *   TERM GR[ACEFULLY]
 *   TERM IM[MEDIATELY]
 *
 * Only the ABORT command is available for the XA Server.
 *
 * @param msg
 *   The @@CONS command text.
 *
 * @return
 *   A status indiciating whether the reply was send to the @@CONS
 *   command sender.
 *    - 0: The console reply has already been sent.
 *    - 1: The caller calls replyToKeyin to print the reply in
 *         sgd.textMessage.
 */

int processShutdownServerCmd(char * msg) {

    enum serverShutdownStates state;
    int ret;
    int logIndicator;
    int termCmd = FALSE;
    char * msg2;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Set the log indicator for the addServerLogEntry call */
    if (sgd.logConsoleOutput) {
        logIndicator = TO_SERVER_STDOUT;
            /* stdout only, since replyToKeyin() causes console
               output to go to the log file in the flag is TRUE */
    } else {
        logIndicator = SERVER_LOGS; /* log file and stdout */
    }

    /* Clear bit 8 of the Condition Word.  This will indicate to the start
     * runsteam that a JDBC Server normal termination is occurring and not
     * to restart the JDBC Server */
    s_cw_clear_task_error();

    /* Check for ABORT command */
    if (strcmp(msg, "ABORT") == 0) {
        /* We must generate a reply here,
           since we don't return from the stopServer call. */
        msg2 = getLocalizedMessageServer(CH_ABORT_CMD_SHUTDOWN,
               sgd.generatedRunID, NULL, 0, msgBuffer);
            /* 5227 ABORT command causing JDBC Server termination
                    (generated run-id {0}) */

        checkForDigit(sgd.textMessage, msg2);
            /* Eliminate the message number at the start of the message */

        replyToKeyin(sgd.textMessage);

        addServerLogEntry(logIndicator, sgd.textMessage);

        stopServer(TRUE);
            /* Note: Code does not return here, since the task is stopped. */
        return 0;

    } else {
        /* Check SHUTDOWN and TERM command syntax */
        if (strcmp(msg, "TERM") == 0) {
            state = SERVER_SHUTDOWN_IMMEDIATELY; /* Default for TERM command is now Immediately. */
            termCmd = TRUE;
        } else if (strcmp(msg, "TERM GR") == 0) {
            state = SERVER_SHUTDOWN_GRACEFULLY;
            termCmd = TRUE;
        } else if (strcmp(msg, "TERM GRACEFULLY") == 0) {
            state = SERVER_SHUTDOWN_GRACEFULLY;
            termCmd = TRUE;
        } else if (strcmp(msg, "TERM IM") == 0) {
            state = SERVER_SHUTDOWN_IMMEDIATELY;
            termCmd = TRUE;
        } else if (strcmp(msg, "TERM IMMEDIATELY") == 0) {
            state = SERVER_SHUTDOWN_IMMEDIATELY;
            termCmd = TRUE;
        } else if (strcmp(msg, "SHUTDOWN") == 0) {
            state = SERVER_SHUTDOWN_GRACEFULLY;
        } else if (strcmp(msg, "SHUTDOWN GRACEFULLY") == 0) {
            state = SERVER_SHUTDOWN_GRACEFULLY;
        } else if (strcmp(msg, "SHUTDOWN GR") == 0) {
            state = SERVER_SHUTDOWN_GRACEFULLY;
        } else if (strcmp(msg, "SHUTDOWN IMMEDIATELY") == 0) {
            state = SERVER_SHUTDOWN_IMMEDIATELY;
        } else if (strcmp(msg, "SHUTDOWN IM") == 0) {
            state = SERVER_SHUTDOWN_IMMEDIATELY;
        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SHUTDOWN_CMD, NULL,
                NULL, 0, msgBuffer);
                    /* 5228 Invalid SHUTDOWN, ABORT, or TERM command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }
    }

    ret = shutdownServer(state);

    /* Set up a message for a successful command:
         SHUTDOWN command causing server termination
           or
         TERM command causing server termination
       This message is in sgd.textMessage, which is printed by the caller.
       In addition, add an entry to the server log file,
       indicating the shutdown.
       */
    if (termCmd) {
        msg2 = getLocalizedMessageServer(CH_TERM_CMD_SHUTDOWN,
            sgd.generatedRunID, NULL, 0, msgBuffer);
            /* 5229 TERM command causing JDBC Server termination
               (generated run-id {0})*/

    } else {
        msg2 = getLocalizedMessageServer(CH_SHUTDOWN_TERMINATION,
            sgd.generatedRunID, NULL, 0, msgBuffer);
                /* 5230 SHUTDOWN command causing JDBC Server termination
                        (generated run-id {0})*/
    }

    checkForDigit(sgd.textMessage, msg2);
        /* Eliminate the message number at the start of the message */

    addServerLogEntry(logIndicator, sgd.textMessage);

    /* Return 1, which tells the caller to print the message in
       sgd.textMessage
       */
    return 1;

} /* processShutdownServerCmd */

/**
 * Function: shutdownServer
 *
 * Set the server shutdown in motion.
 *
 * @param state
 *   The shutdown state (gracefully or immediately) to place the
 *   server in.
 *
 * @return
 *   Always return 0 for now.
 */

static int shutdownServer(enum serverShutdownStates state) {

#ifndef XABUILD /* JDBC Server */
    int status;

    int i;

    /* Set two shutdown flags, based on the argument */
    sgd.serverShutdownState = state;

   /* printf("In CH - shutdownServer()\n"); */

    /*
     * If we are shutting the server down immediately, then
     * make sure any server workers in graceful shutdown get
     * told just in case a graceful server shutdown was already
     * attempted and the ICL's are not there.
     */
    if (state == SERVER_SHUTDOWN_IMMEDIATELY){
       switchServerWorkerShutDownState();
    }

    /* Now assume there are ICL's. Loop over all the ICL activities we have. */
    for (i = 0; i < sgd.num_COMAPI_modes; i++){
       switch (state) {
           case SERVER_ACTIVE:
               sgd.ICLShutdownState[i] = ICL_ACTIVE;
                   /* This case should not occur */
               break;
           case SERVER_SHUTDOWN_GRACEFULLY:
               sgd.ICLShutdownState[i] = ICL_SHUTDOWN_GRACEFULLY;
               sgd.UASFM_ShutdownState = UASFM_SHUTDOWN;
               break;
           case SERVER_SHUTDOWN_IMMEDIATELY:
               sgd.ICLShutdownState[i] = ICL_SHUTDOWN_IMMEDIATELY;
               sgd.UASFM_ShutdownState = UASFM_SHUTDOWN;
               break;
       }
    }

    /* Notify the ICL's to shutdown.
       We need to do a PassEvent() to the ICL activities since
       they may be waiting on a TCP_Accept - this notifies
       the ICL's right away without them waiting for Client/timeout.
       If no server sockets were active, as the ICL's wake up
       to attempt to get server sockets they will shut down.
       So no need to try to do it any faster (we have to wait
       for the ICL's twaits to expire anyway.)
     */
    status = Pass_Event();

#else /* XA JDBC Server */

    sgd.UASFM_ShutdownState = UASFM_SHUTDOWN; /* tell UASFM to shutdown. */

#endif /* XA and JDBC Servers */

    /* Always return 0 for now */
    return 0;
}

/**
 * Function: stopServer
 *
 * Stop the JDBC server.
 *
 * @param abort
 *   The argument is a flag that if nonzero indicates that
 *   the server is being aborted
 *   (e.g., due to an ABORT SYNTAX command).
 *   If zero, the server is being shutdown due to the
 *   deregistration of the ER KEYIN$ keyin name.
 *
 *   If stopServer is used within an XA Server, no
 *   intial log entry is made since the caller has
 *   already done so.
 */

void stopServer(int abort) {

#ifndef XABUILD /* JDBC Server */

    int i;
    int status2;
#else  /* JDBC XA Server */
    char errstat[5];
    int auxinfo;
    char begin_Thread_Command[50]; /* Space for a "BEGIN THREAD FOR xxxxx;" command */
#endif /* JDBC XA Server */

    int status;
    int logStatus;
    int traceStatus;
    char digits[ITOA_BUFFERSIZE];
    char digits2[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

#ifndef XABUILD /* JDBC Server */

    /* Add a log entry about the server termination */
    if (abort) {
        getMessageWithoutNumber(CH_ABORT_ABNORMAL_SHUTDOWN, NULL, NULL,
            SERVER_LOGS_DEBUG);
                /* 5231 ABORT causing abnormal JDBC Server termination */
    } else {
        getMessageWithoutNumber(CH_KEYIN_CAUSING_NORMAL_SHUTDOWN, NULL, NULL,
            SERVER_LOGS_DEBUG);
                /* 5232 ER KEYIN$ deregistration causing normal JDBC Server
                   termination */
    }

#endif /* JDBC Server */

    /* Set shutdown flags */
    sgd.UASFM_ShutdownState = UASFM_SHUTDOWN;
    sgd.serverShutdownState = SERVER_SHUTDOWN_IMMEDIATELY;
    sgd.consoleHandlerShutdownState = CH_SHUTDOWN;

#ifndef XABUILD /* JDBC Server */

    /*
     * If we are shutting the server down immediately, then
     * make sure any server workers in graceful shutdown get
     * told just in case a graceful server shutdown was already
     * attempted and the ICL's are not there.
     */
    switchServerWorkerShutDownState();

    /* Now assume there are ICL's. Loop over all the ICL activities we have. */
    for (i = 0; i < sgd.num_COMAPI_modes; i++){
       sgd.ICLShutdownState[i] = ICL_SHUTDOWN_IMMEDIATELY;
    }
      /*
       We need to do a PassEvent() to the ICL activity since
       it may be waiting on a TCP_Accept - this notifies
       the ICL right away without it waiting for Client/timeout.
       If no server sockets were active, as the ICL's wake up
       to attempt to get server sockets they will shut down.
       So no need to try to do it any faster (we have to wait
       for the ICL's twaits to expire anyway.)
       */
    status2 = Pass_Event();

#endif /* JDBC Server */

    /* Close log and trace files */
    status = closeServerFiles(&logStatus, &traceStatus);

    if (status != 0) {
        getLocalizedMessageServer(CH_LOG_TRACE_FILE_CLOSE_ERROR,
            itoa(logStatus, digits, 8), itoa(traceStatus, digits2, 8),
            SERVER_LOGS, msgBuffer);
                /* 5233 File close error - log file octal status:{0};
                   trace file octal status:{1} */
    }

#ifndef XABUILD /* JDBC Server */
    /*
     * Send a message to the console indicating that the JDBC Server
     * is shutting down.  This must be done before we deregister from
     * COMAPI and stop all the activities in the Server.
     */
    if (abort) {
        deregComapiAndStopTasks(SERVER_ABORTING, sgd.serverName , NULL);
    }
    else {
        deregComapiAndStopTasks(SERVER_SHUTTING_DOWN, sgd.serverName, NULL);
    }
#else

    /* JDBC XA Server shutting down, probably because XAServerWorkerShutDown()
     * called due to termination initiated by ODTP timing the JDBC XA Server out.
     *
     * We want to avoid the ugly "UDSSRC APT OUTSIDE OF UDS, THREAD ROLLBACK BEING ATTEMPTED",
     * or the "APP 7  DCS RB 50100 First command is an explicit thread control command other
     * than BEGIN THREAD. Try a BEGIN THREAD first.", messages that UDS arbitrarily sends to
     * the PRINT$ file should the JDBC XA Server either just exit or attempt to rollback and
     * end a probable UDS/RDMS thread.
     *
     * The WDE has a open RDMS Thread flag (which is good enough to test if we had a "stateful"
     * JDBC XA Server that ended its database thread correctly, however it would not be enough
     * if the "stateful" JDBC XA Server were timed out in mid-database thread by ODTP), but if
     * the XA Server was in an XA Transaction, the transaction may have been completed and the
     * UDS/RDMS thread closed without the opportunity for the open RDMS Thread flag to be reset
     * (the next transactional usage would just re-open a new UDS/RDMS thread). Also, when we
     * are in an XA transaction, we do not know the actual transactional state of the database
     * connection. For example, the transaction may be in the prepared state, in which case we
     * do NOT want to affect the transaction (clearly we don't want to try a Rollback command).
     * Therefore, in the case of a JDBC XA Server in XA mode (non-stateful) we will not attempt
     * to avoid the ugly messages.
     *
     * Therefore, to accomplish a clean shutdown (without those ugly UDS messages), we must go
     * to extra lengths to determine if there is actually an open RDMS thread held by a stateful
     * (non-XA) JDBC XA Server. If we have determined this, we can try to determine and handle
     * any open database thread that might be present. This is done by issuing a BEGIN THREAD
     * command, and then make a decision based on status returned:
     *
     *    If = 0          - We successfully got a new database thread, so we didn't have one
     *                      open already. Good, just do the END THREAD on it and we're are
     *                      done. No error status means no "message splatter" sent to Print$.
     *    If = 5002       - We already have a database thread open and the attempt to open a
     *                      new one got rejected (5002). So do a ROLLBACK and END THREAD and
     *                      we're done. Getting the simple 5002 error status means no ugly
     *                      "message splatter" will be sent by UDS/RDMS to Print$.
     *    For all others  - Something is not right, we should have gotten either a 0 or 5002.
     *                      Assume that there may still might be a database thread open and
     *                      try the ROLLBACK and END THREAD. At worst, we will see the UDS/RDMS
     *                      messages, but in this error case they are welcome for error handling.
     *
     * So, do we have an open database thread and are we non-transactional (non-XA)?
     */
    if (act_wdePtr->openRdmsThread == 1 && act_wdePtr->txn_flag == 0){
        /*
         * We are non-XA and we may have a UDS/RDMS database thread still
         * open. Find out by issuing an explicit Begin Thread. After this
         * command, we will either have a newly opened RDMS thread or still
         * have the previously opened RDMS thread.
         */
        sprintf(begin_Thread_Command,"BEGIN THREAD FOR %s;", sgd.appGroupName);
        rsa(begin_Thread_Command, errstat,&auxinfo);

        /*
         * If we did not get a 0 status (i.e., we got a 5002 or something else),
         * we still have the previously open database thread, so do the ROLLBACK
         * first (we must make sure no uncommitted changes get applied at END THREAD
         * time - remember RDMS does not operate like the SQL standard JDBC assumes),
         * and then do the END THREAD commands to properly close it. Don't worry about
         * any error statuses since we can't do anything about them and we are going
         * away anyway.
         */
        if (atoi(errstat) !=0){
            /* We have an open database thread to deal with. Do a rollback now, just in case. */
            rsa("ROLLBACK;", errstat, &auxinfo);
        }

        /* Finally, close the open UDS/RDMS database thread. */
        rsa("END THREAD;", errstat, &auxinfo);
    }

    /* JDBC XA Server has no COMAPI usage; just stop all active tasks.
       Note that control does not return to the caller.
       Send the appropriate message to the console. */
    if (abort) {
        stop_all_tasks(XA_SERVER_ABORTING, sgd.serverName , NULL);
    }
    else {
        stop_all_tasks(XA_SERVER_SHUTTING_DOWN, sgd.serverName , NULL);
    }
#endif

}

#ifndef XABUILD /* JDBC Server */
/*
 *  Function switchServerWorkerShutDownState
 *
 * This function goes over the list of active Server Workers (those
 * that have active JDBC clients) looking for workers that have a
 * serverWorkerShutdownState of WORKER_SHUTDOWN_GRACEFULLY. These
 * server workers will have their serverWorkerShutdownState state
 * changed to WORKER_SHUTDOWN_IMMEDIATELY and will be notified via
 * a Pass_Event on their client socket.
 *
 * This handles the case where the customer attempted to shut the
 * server down gracefully and then decides to shut the server down
 * immediately or abort it.  In this case, the ICL's are already
 * shutdown and will not re-notify the gracefully ending Server workers
 * to shutdown immediately. This function handles this case.
 */
void switchServerWorkerShutDownState(){

    workerDescriptionEntry *wdeptr;
    int SW_Client_Socket[TEMPNUMWDE];
    int SW_Client_comapi_bdi[TEMPNUMWDE];
    int i;
    int SW_index;

    /*
     * Now, check the Server Workers that are working on assigned
     * JDBC Clients.  Since we will be holding the assigned Server Worker
     * chain under T/S control, all those Server Workers will be notified
     * before they try to get themselves off the assigned chain and placed
     * on the free Server Worker chain.
     *
     * Do only the minimum work under T/S control to do this:
     * 1) Get T/S cell for the assigned Server Worker chain.
     * 2) Loop over the Server Worker entries on this chain.
     *   2a) Get the WDE pointer to next assigned Server worker under T/S locking.
     *   2b) Test the shutdown status in that Server Workers WDE for graceful shutdown.
     *   2c) Determine if the Server Worker needs to be notified of
     *       immediate shutdown via a Comapi event, and if so put it on
     *       a list to process after the T/S.
     */
    SW_index = 0; /* indicate no SW's on the list regardless of shutdown type. */
    test_set(&(sgd.firstAssigned_WdeTS));  /* Get control of the chains T/S cell */

    /* loop over all Server Workers on the assigned chain. */
    wdeptr=sgd.firstAssigned_wdePtr;
    while(wdeptr != NULL){
       /* A Server Worker was found, so check its shutdown state. */
       if (wdeptr->serverWorkerShutdownState == WORKER_SHUTDOWN_GRACEFULLY){

          /* Tell the Server Worker to shutdown immediately. */
          wdeptr->serverWorkerShutdownState = WORKER_SHUTDOWN_IMMEDIATELY;

          /* Add the SW client info to notify list for later PASS_EVENT_To_SW() */
          SW_Client_Socket[SW_index] = wdeptr->client_socket;
          SW_Client_comapi_bdi[SW_index] = wdeptr->comapi_bdi;
          SW_index++;
       }

       /* Move to next entry on chain */
       wdeptr=wdeptr->assigned_wdePtr_next;
    }

    /* all done processing the assigned SW chain. Unlock the chains T/S cell */
    ts_clear_act(&(sgd.firstAssigned_WdeTS));

    /*
     * Now pass an event to all the Server Workers on the list. If they are
     * not in COMAPI, or the client socket is already closed, there is
     * no problem since they will shut down by themselves.
     */
    if (SW_index > 0){
        /* We have Server Workers to notify */
        for (i = 0; i < SW_index; i++){
             /*
              * Notify the client's Server Worker. There is no
              * need to check the returned status since either
              * the TCP_EVENT succeeded, and if it failed (e.g.
              * the socket is likely closed and the user is not in
              * COMAPI) there is no additional need to notify the
              * client's Server Worker - it will handle itself.
              *
              * If the Server Worker was starting to shutdown, or was
              * finished with the client but hadn't got itself off the
              * assigned chain, it may be possible that the socket and/or
              * the COMAPI bdi was already set to 0.  If so, no need to do
              * a Pass_Event_to_SW.
              */
             if (SW_Client_Socket[i] != 0 & SW_Client_comapi_bdi[i] != 0){
                   Pass_Event_to_SW(SW_Client_Socket[i], SW_Client_comapi_bdi[i]);
             }
        }
    }
}
#endif /* JDBC Server */

/**
 * Function: processShutdownWorkerCmd
 *
 * Process the worker control commands:
 *
 *   SHUTDOWN WORKER id|tid
 *   SHUTDOWN WORKER id|tid GR[ACEFULLY]
 *   SHUTDOWN WORKER id|tid IM[MEDIATELY]
 *   ABORT WORKER id|tid
 *
 * id is the server worker socket ID.
 * This is H1 (the upper 18 bits) in the WDE's socket ID word
 * (client_socket), specified in decimal.
 * This id value is listed under "server work socket id",
 * which is the first line of output for each server worker,
 * as generated by the console command DISPLAY WORKER STATUS.
 *
 * tid (if specified) is the server worker RDMS thread name.
 * This is in the WDE's threadname character array.
 *
 * None of these commands are available for the XA Server.
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

int processShutdownWorkerCmd(char *msg) {

    char tid[80]; /* large enough for thread name (and even the whole CH command). */
    char workerID_or_tid[80];  /* also make large enough */
    int ret;
    int workerID;
    int abortCase = FALSE;
    int logIndicator;
    int socket;
    char insert[TRAILING_STRING_SIZE];
    char * msg2;
    char digits2[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* The following is used to detect garbage following what looks
       like legal command syntax.
       If that case is detected, the command is ignored. */
    char trail[TRAILING_STRING_SIZE];

    enum serverWorkerShutdownStates state;

    /* Scan the string */
    if (sscanf(msg, "SHUTDOWN WORKER %s %s %s", &tid, &insert, &trail)
            == 2) {

        if (strcmp(insert, "GRACEFULLY") == 0) {
            /* SHUTDOWN WORKER id|tid GRACEFULLY */
            state = WORKER_SHUTDOWN_GRACEFULLY;
        } else if (strcmp(insert, "GR") == 0) {
            /* SHUTDOWN WORKER id|tid GR */
            state = WORKER_SHUTDOWN_GRACEFULLY;
        } else if (strcmp(insert, "IMMEDIATELY") == 0) {
            /* SHUTDOWN WORKER id|tid IMMEDIATELY */
            state = WORKER_SHUTDOWN_IMMEDIATELY;
        } else if (strcmp(insert, "IM") == 0) {
            /* SHUTDOWN WORKER id|tid IM */
            state = WORKER_SHUTDOWN_IMMEDIATELY;
        } else {
            msg2 = getLocalizedMessageServer(CH_INVALID_SHUTDOWN_WORKER_CMD,
                NULL, NULL, 0, msgBuffer);
                /* 5234 Invalid SHUTDOWN WORKER command */
            strcpy(sgd.textMessage, msg2);
            return 1;
        }

    } else if (sscanf(msg, "SHUTDOWN WORKER %s %s", &tid, &trail)
            == 1) {
        /* 'SHUTDOWN WORKER id|tid' is the same as
           'SHUTDOWN WORKER id|tid GRACEFULLY' */
        state = WORKER_SHUTDOWN_GRACEFULLY;

    } else if (sscanf(msg, "ABORT WORKER %s %s", &tid, &trail) == 1) {
        /* ABORT WORKER id|tid.
           !!! 'ABORT WORKER id|tid' command not yet handled properly */
        state = WORKER_SHUTDOWN_IMMEDIATELY;
        abortCase = TRUE;

    } else {
        msg2 = getLocalizedMessageServer(CH_INVALID_SHUTDOWN_ABORT_WORKER_CMD,
            NULL, NULL, 0, msgBuffer);
                /* 5235 Invalid SHUTDOWN WORKER or ABORT WORKER command */
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

    /* Call the function to put the worker shutdown into motion */
    ret = shutdownServerWorker(&workerID, state, &socket);

    /* Invalid worker ID */
    if (ret == JDBCSERVER_UNABLE_TO_FIND_SPECIFIED_WORKER) {
        msg2 = getLocalizedMessageServer(CH_INVALID_WORKER_ID,
            workerID_or_tid, tid, 0, msgBuffer);
            /* 5203 Client {0} {1} not found */
        strcpy(sgd.textMessage, msg2);
        return 1;
    }

    /* Set up a message for a successful command:
         Server worker <id|tid> shutting down
       This message is in sgd.textMessage, which is printed by the caller.
       In addition, add an entry to the server log file
       indicating the shutdown,
       */
    msg2 = getLocalizedMessage(CH_WORKER_SHUTTING_DOWN,
        workerID_or_tid, itoa(socket, digits2, 10), 0, msgBuffer);
            /* 5236 The server worker with {0} (socket {1}) is shutting down */


    checkForDigit(sgd.textMessage, msg2);
        /* Eliminate the message number at the start of the message */

    /* Set the log indicator for the addServerLogEntry call */
    if (sgd.logConsoleOutput) {
        logIndicator = TO_SERVER_STDOUT;
            /* stdout only, since replyToKeyin() causes console
               output to go to the log file in the flag is TRUE */
    } else {
        logIndicator = SERVER_LOGS; /* log file and stdout */
    }

    addServerLogEntry(logIndicator, sgd.textMessage);

    /* Return 1, which tells the caller to print the message
       in sgd.textMessage */
    return 1;

} /* processShutdownWorkerCmd */

/**
 * Function: shutdownServerWorker
 *
 * This procedure notifies a specific instance of a Server Worker to shutdown.
 * The Server Worker handles its own shutdown based on the shutdown state
 * requested.
 *
 * If the Server Worker was not found on the Assigned Server Worker chain,
 * then it is assumed to be free (unassigned) and there is no need to have
 * it shutdown.
 * It can be re-assigned by the ICL at a later time for a new JDBC Client.
 *
 * @param clientSocketIDPtr
 *   A pointer to the socket ID (int) of the client's Server Worker
 *   being notified to shutdown.
 *   The int value is the server worker socket ID,
 *   which is in H1 (the upper 18 bits) in the WDE's socket ID word
 *   (client_socket),
 *   The ID is specified in decimal in the console command syntax,
 *   e.g., SHUTDOWN WORKER id, (or looked up from the thread name (tid)).
 *   This id value is listed under "server work socket id",
 *   which is the first line of output for each server worker,
 *   as generated by the console command DISPLAY WORKER STATUS.
 *
 * @param swShutdownState
 *   One of the serverWorkerShutdownStates values.
 *
 * @return
 *   An error status.
 *     - 0 if no error occurred locating the Server Worker on
 *       assigned Server Worker chain and posting the shutdown
 *       notification.
 *     - A non-0 value indicates that the Server Worker
 *       identified by the desired socket ID could not
 *       be found on the assigned Server Worker chain.
 *
 * !!! Note: ABORT WORKER command is not yet implemented properly.
 *           Possibly add a flag argument.
 */

static int shutdownServerWorker(int * clientSocketIDPtr,
    enum serverWorkerShutdownStates swShutdownState, int * socketPtr) {

    workerDescriptionEntry *wdeptr;
    int status;

    *socketPtr = 0; /* zero in case of no-find on socket ID. */
    status = findWorker(clientSocketIDPtr, &wdeptr);

    if (status == 0) {
        /* We found the worker.
           Set its shutdown state.
           Return its socket number.
           Return 0.
           */
        wdeptr->serverWorkerShutdownState = swShutdownState;
        *socketPtr = wdeptr->client_socket;

#ifndef XABUILD /* JDBC Server */

        /* Only need to notify the Server Worker in the JDBC Server case. */
        if (swShutdownState == WORKER_SHUTDOWN_IMMEDIATELY){
            /*
             * Notify the client's Server Worker. There is no
             * returned status since either the TCP_EVENT succeeded,
             * and if it failed (e.g. the socket is now closed and the
             * user is not in COMAPI) there is no additional need to
             * notify the client's Server Worker - it will handle itself.
             */
            Pass_Event_to_SW(wdeptr->client_socket, wdeptr->comapi_bdi);
        }

#endif /* JDBC Server */

        return 0;
    }

    /* We did not find the worker.
       Return a nonzero error status.
       */
    return JDBCSERVER_UNABLE_TO_FIND_SPECIFIED_WORKER;
}
