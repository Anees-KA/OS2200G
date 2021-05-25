/* This module contains functions for signal handling in the
   JDBC server */

/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <universal.h>
#include <task.h>
#include <ertran.h>
#include "marshal.h"

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "DisplayCmds.h"
#include "NetworkAPI.h"
#include "ProcessTask.h"
#include "Server.h"
#include "ServerLog.h"
#include "ServerWorker.h"
#include "ShutdownCmds.h"
#include "signals.h"

/* Imported data */
extern insideClibProcessing;
extern workerDescriptionEntry *act_wdePtr;
extern serverGlobalData sgd;

#define SGD_SIZE sizeof(serverGlobalData);
#define WDE_SIZE sizeof(workerDescriptionEntry);

/* Prototypes for static functions in this file */
static void xcHandler(int sig);
static void signalHandlerLevel2(int sig);
static void signalHandlerLevel3(int sig);
static void signalHandlerLevel4(int sig);
static void getContingencyInfo(int sig, char * cgyString,
                               char * signalString, int * cgyType,
                               char * errType);

static char cgyStr[40];
static char sigStr[12];
static int cgyTypeInt;
static char errTypeStr[10];

#define ER_APRINT 070
#define BYTES_PER_WORD 4
#define OCTAL_WORDS_DUMPED_PER_LINE 5
#define MAX_CHARS_IN_APRINT 132
#define CGY_PKT_BYTE_SIZE sizeof(cgy_pkt)
#define minimum(a,b) ((a) < (b) ? (a) : (b))

#define IN_SERVERWORKERSIGNALHANDLER 0
#define IN_SIGNALHANDLERLEVEL1       1
#define WAS_NOT_IN_COMAPI            0
#define WAS_IN_COMAPI                1

/**
 * regSignals
 *
 * This function is called from the server initialization to register
 * all signals to a specified signal handler.
 * All signals except the remote break (@@X C interrupt) get the same handler
 * (the function passed in as a parameter).
 *
 * @param func
 *   A pointer to a function (the signal handler).
 *   The function has one int argument.
 *
 */
void regSignals(void (*func)(int)) {

    /* In the table below:

         CT: contingency type (octal)
         ET: error type (octal)

         Notes:

         We need to capture SIGRBK (@@X C remote break, CT=10, ET=2),
         with a separate handler,
         so we can't just use SIGINT_C (CT=10, ET=any) and skip the
         rest of the CT=10 cases.
         Therefore, SIGINT_C is not registered.

         On the other hand, for CT=12 and CT=13, we simply register the
         general cases (i.e., ET=any).
         These cases are:
           SIGERR_: CT=12, ET=any
           SIGICGY: CT=13, ET=any

                                  CT   ET
                                  --   --                                   */

    signal(SIGOPR, *func);     /*   1   0 IOPR (invalid operation)          */
    signal(SIGGDM, *func);     /*   2   0 IGDM (guard mode)                 */
    signal(SIGFOF, *func);     /*   3   0 floating overflow                 */
    signal(SIGFUF, *func);     /*   4   0 floating underflow                */
    signal(SIGDOF, *func);     /*   5   0 divide fault (divide by 0)        */
    signal(SIGRST, *func);     /*   6 any restart contingency               */
    signal(SIGIABT, *func);    /*   7   0 abort: ER ABORT$ or EABT$         */

    /* signal(SIGINT_C, *func);    10 any console interrupt                 */

    signal(SIGII, *func);      /*  10   1 II command by system console      */
    signal(SIGRBK, xcHandler); /*  10   2 remote break @@X C                */
    signal(SIGX_CT, *func);    /*  10   3 @@X CT interactive signal         */
    signal(SIGTS, *func);      /*  11   0 T/S interrupt in R/T programs     */
    signal(SIGERR_, *func);    /*  12 any error mode                        */
    signal(SIGICGY, *func);    /*  13 any interactivity interrupt           */
    signal(SIGBRKPT, *func);   /*  14   0 breakpoint mode                   */
    signal(SIGHWFLT, *func);   /*  15   0 hardware fault                    */
    signal(SIGMSUPS, *func);   /*  16   0 max SUPS or PCT overflow          */
    signal(SIGABTRM, *func);   /*  17   0 abnormal program termination      */
    signal(SIGRTFLT, *func);   /*  17   1 storage fault in R/T program      */
    signal(SIGTIMOU, *func);   /*  20   0 timeout warning                   */
    signal(SIGELINK, *func);   /*  21   1 Linking System error              */
    signal(SIGSGNL, *func);    /*  21   2 SGNL instr. interrupt or EM ER    */
    signal(SIGSTACK, *func);   /*  21   3 stack overflow or underflow       */
    signal(SIGVFY, *func);     /*  21   4 call or return verification error */
    signal(SIGCGY, *func);     /*  21   5 contingency handler fault         */
    signal(SIGDEACT, *func);   /*  21   6 subsystem deactivated             */

} /* regSignals */

/**
 * This is the signal handler for the @@X C (remote break) contingency.
 * It prints a message (via ER APRINT$),
 * then returns to the code where the interrupt occurred.
 *
 * When a signal handler is entered, the specific signal that was caught
 * (by the runtime) has been deregistered by the runtime system.
 * Therefore, the first item performed in a handler is typically
 * to register the signal (and possibly other signals as well).
 *
 * @param sig
 *   The signal being handled.
 *
 */

static void xcHandler(int sig) {

    char itoaBuffer[ITOA_BUFFERSIZE];
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    /* Re-register this @@X C contingency with this handler */
    signal(SIGRBK, xcHandler);

    if (DEBUG) {
        printf("in xcHandler: sig=%o, SIGRBK=%o, in CLIB=%d\n",
            sig, SIGRBK, insideClibProcessing);
    }

    /* Log a message.
       Don't use UC I/O, since we want to simply return to the code.
       (The UC PRM suggests avoiding UC I/O in signal handlers.)
       */
    if (insideClibProcessing) {
        strcpy(message, "@@X C (remote break) occurred during server worker ");
        strcat(message, itoa(act_wdePtr->serverWorkerActivityID, itoaBuffer,
            8));
        strcat(message, " execution.");
        printOneLineViaAprint(message);
        printOneLineViaAprint("  Control returned to worker.");

    } else {
        printOneLineViaAprint(
            "@@X C interrupt (remote break) occurred during server execution.");
        printOneLineViaAprint("  Control returned to server.");
    }

    /* Return to the code where the interrupt occurred, and resume execution */
    return;

} /* xcHandler */

/**
 * This is the signal handler code for checking a subsystem deactivation contingency.
 * It checks whether the deactivation contingency was in COMAPI and then prints a
 * message (via ER APRINT$). It returns to the caller either a TRUE (COMAPI deact)
 * or FALSE (some other subsystem deact). It also re-registers the correct signal
 * handler for SIGDEACT before return.
 *
 * When a signal handler is entered, the specific signal that was caught
 * (by the runtime) has been deregistered by the runtime system.
 * Therefore, the first item performed in a handler is typically
 * to register the signal (and possibly other signals as well). The second
 * parameter indicates whether it was serverWorkerSignalHandler or
 * signalHandlerLevel1 that called so that the correct re-registration of
 * signal handler is done.
 *
 * @param whichHandler
 *   Indicates which handler called:
 *   0 = serverWorkerSignalHandler, 1 = signalHandlerLevel1
 *
 * @returns
 *   0 (FALSE) - Subsystem deactivation contingency was not COMAPI.
 *   1 (TRUE) -  Subsystem deactivation contingency was COMAPI.
 */

static int comapiDeactCheck(int whichHandler) {

    char itoaBuffer[ITOA_BUFFERSIZE];
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    int wasInCOMAPI;

    /* Re-register the correct SIGDEACT signal handler */
    if (whichHandler == IN_SIGNALHANDLERLEVEL1){
        signal(SIGDEACT,signalHandlerLevel1); /* General handler */
    }
    else {
       signal(SIGDEACT, serverWorkerSignalHandler); /* SW handler */
    }

    if (DEBUG) {
        printf("in comapiDeactCheck checking SIGDEACT, in COMAPI=%d\n",
             act_wdePtr->in_COMAPI);
    }

    /*
     * Test if we were in COMAPI and log appropriate message and
     * determine the appropriate return value.
     * Don't use UC I/O, since we want to simply return to the code.
     * (The UC PRM suggests avoiding UC I/O in signal handlers.)
     */
    wasInCOMAPI = WAS_NOT_IN_COMAPI; /* assume contingency was not while in COMAPI */
    if (act_wdePtr->in_COMAPI == CALLING_COMAPI) {
        if (whichHandler == IN_SIGNALHANDLERLEVEL1){
          /* Not in a Server Worker activity, just display activity id */
          strcpy(message, "Subsystem Deactivation occurred while in COMAPI,  activity id = ");
        } else {
          /* In a Server Worker activity, display activity id as a worker id */
          strcpy(message, "Subsystem Deactivation occurred while in COMAPI,  worker id = ");
        }
        strcat(message, itoa(act_wdePtr->serverWorkerActivityID, itoaBuffer,
            8));
        printOneLineViaAprint(message);
        wasInCOMAPI = WAS_IN_COMAPI; /* contingency while in COMAPI */
    }

    /* Return whether we were in COMAPI when subsystem deactivation occurred */
    return wasInCOMAPI;

} /* comapiDeactCheck */

/**
 * serverWorkerSignalHandler
 *
 * This function handles signals for a server worker activity.
 *
 * If we are in CLIB code (i.e., C code in the c-interface file is executing,
 * not C code in the server file), mark the server worker
 * to be shut down.
 * (Flag insideClibProcessing is true in this case.)
 * Print out some contingency information,
 * and terminate the task.
 *
 * Otherwise, go thru normal server termination by calling
 * signalHandlerLevel1.
 *
 * @param sig
 *   The signal being processed (passed from the C runtime routine
 *   handling the contingency).
 */

void serverWorkerSignalHandler(int sig) {

    /* Allow for 30 lines (times 80 bytes per line) of a contingency message,
       plus an addition 2000 bytes (for use by the LS$DECODE
       Linking System utility)
       */

#ifdef XABUILD  /* XA JDBC Server */

    int error_status;
    int error_len;
    char error_message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* a buffer for a returned error message */

#endif          /* XA JDBC Server */

    char message[5000];

    /* With spacing, allow 4 character per octal byte */
    char cgyPkt[CGY_PKT_BYTE_SIZE*4];

    /* Register signal handlers */
    regSignals(&signalHandlerLevel1);

    if (DEBUG) {
        printf("in serverWorkerSignalHandler: sig=%o, in CLIB=%d\n",
            sig, insideClibProcessing);
    }

    /* Get info about the contingency */
    getContingencyInfo(sig, cgyStr, sigStr, &cgyTypeInt, errTypeStr);
    getOctalBytes(cgyPkt, (char *) &_sg_data.contingency_packet,
        CGY_PKT_BYTE_SIZE);

    /* Test if it was a subsystem deactivation (SIGDEACT) */
    if (sig == SIGDEACT){
        /* Yes, call comapiDeactCheck to check if it was in COMAPI subsystem */
       if (comapiDeactCheck(IN_SERVERWORKERSIGNALHANDLER)){
           return; /* It was, just return since our COMAPI calling code can handle recovery */
       }
       /* If not in COMAPI, handle like any other fatal contingency. */
    }

    /* Check if we are in CLIB code */
    if (insideClibProcessing) {
        /* Mark the worker for shutdown */
        act_wdePtr->serverWorkerShutdownState = WORKER_SHUTDOWN_IMMEDIATELY;

        /* Generate a contingency message for worker shutdown.
           Note: String literal continued across lines - do not adjust.
           */
        sprintf(message,
            "\nUnrecoverable contingency occurred during server worker %012o (%024llo) execution.\n\
  Signal name:%s, Contingency:%s.\n\
  Signal number:%o, Contingency type:%02o, Error type:%s.\n\
  Reentry address:%012o.\n\
  Server worker is being shut down.\n\n\
  Contingency packet:\n%s",
            act_wdePtr->serverWorkerActivityID, act_wdePtr->serverWorkerUniqueActivityID,
            sigStr, cgyStr, sig, cgyTypeInt, errTypeStr, _sg_data.reentry_address,
            cgyPkt);

        /* Decode the reentry address */
        decodeAddress(_sg_data.reentry_address,
            _sg_data.contingency_packet.modifiable_execution_data.
            originating_ss_va, message+strlen(message));

        /* The contingency message goes in the client trace file
           and the server log file
           */
        addClientTraceEntry(act_wdePtr, TO_CLIENT_TRACEFILE, message);
        addServerLogEntry(SERVER_LOGS, message);

        /* Display the configuration and worker status in the log file */
        processDisplayConfigurationCmd(TRUE, TO_SERVER_LOGFILE, FALSE, NULL);
        displayAllActiveWorkersInLog(TO_SERVER_LOGFILE, TRUE);

        /* Display the worker info in the client trace file */
        displayActiveWorkerInfo(TO_CLIENT_TRACEFILE, FALSE, act_wdePtr, TRUE, FALSE);

        /* Shut down the server worker or XA Server Worker.
           We should not return from this call,
           but do an exit(0) just in case (to exit the task).
           */
#ifndef XABUILD  /* JDBC Server */

        serverWorkerShutDown(act_wdePtr, SW_CONTINGENCY_CASE, act_wdePtr->client_socket);

#else            /* XA JDBC Server */

       if (act_wdePtr->clientTraceFile != NULL){
           /* A Client Trace file is present, attempt to close it */
           error_status= closeClientTraceFile(act_wdePtr, error_message, error_len);
           if (error_status !=0){
               /*
                * Error occurred in trying to close client trace file. Log
                * this error into the JDBC Server Log file and continue
                * to stop the XA Server.
                */
               addServerLogEntry(SERVER_LOGS,error_message);
           }
        }
        stopServer(0); /* Indicate its the term reg calling. This call does not return. */

#endif          /* XA and JDBC Servers */

        exit(0);

    } else {
        signalHandlerLevel1(sig);
    }

} /* serverWorkerSignalHandler */

/**
 * decodeAddress
 *
 * Decode a virtual address into symbolic format.
 *
 * @param address1
 *   The extended mode virtual address to decode.
 *   This is the contingency reetry address.
 *
 * @param address 2
 *   A second address to decode, if different from the first address.
 *
 * @param stringBuffer
 *   This is the decoded string that is returned.
 *   It is allocated by the caller.
 *   Assume that you could get up to 2000 bytes in the buffer
 *   (LS_DECODE_MAX_BUFFER_SIZE = 1000 bytes
 *   for each potential call to the LS$DECODE Linking System utility).
 */

#define LS_DECODE_MAX_BUFFER_SIZE 1000

void decodeAddress(int address1, int address2, char * stringBuffer) {

    int charOffset = 0;
    char buffer[LS_DECODE_MAX_BUFFER_SIZE];
    int bufSize = LS_DECODE_MAX_BUFFER_SIZE;
    int status = 0;
    int useSSInfo = FALSE;

    buffer[0] = '\0';

    sprintf(stringBuffer,
        "\n  Decoding of the contingency reentry address %012o:\n", address1);

    status = LS$DECODE(&address1, &useSSInfo, buffer, &bufSize, &charOffset);

    if (status != 0) {
        sprintf(stringBuffer+strlen(stringBuffer),
            "   Error status from LS$DECODE: %012o\n", status);
    }

    massageDecodeBuffer(buffer, stringBuffer);

    /* Dump the second address, if different from the first address.
       Note: Instead of checking for an exact match,
       check if they are different by more than 1.
       (This allows a contingency address to be one instruction off.)
       */
    if (abs(address1 - address2) > 1) {
        buffer[0] = '\0';

        sprintf(stringBuffer+strlen(stringBuffer),
            "  Decoding of the original contingency address %012o:\n", address2);

        status = LS$DECODE(&address2, &useSSInfo, buffer, &bufSize, &charOffset);

        if (status != 0) {
            sprintf(stringBuffer+strlen(stringBuffer),
                "   Error status from LS$DECODE: %012o\n", status);
        }

        massageDecodeBuffer(buffer, stringBuffer);
    }

} /* decodeAddress */

/**
 * massageDecodeBuffer
 *
 * Add the info from the input (LS$DECODE) buffer into the output buffer.
 *
 * The input buffer has an unknown number of NUL-terminated strings that we
 * want to handle.
 * Two NULs in a row signify the end of the strings in the buffer.
 *
 * In the output buffer, add three blanks before each string,
 * and add a '\n' after each string.
 *
 * @param inputBuffer
 *   The input buffer (returned from LS$DECODE).
 *
 * @param outputBuffer
 *   The massaged output buffer returned by this function.
 *   It has already been initialized (i.e., it contains a string).
 */

static void massageDecodeBuffer(char * inputBuffer, char * outputBuffer) {

    while (*inputBuffer != '\0') {
        strcat(outputBuffer, "   ");
        strcat(outputBuffer, inputBuffer);
        strcat(outputBuffer, "\n");
        inputBuffer =  inputBuffer + strlen(inputBuffer) + 1;
    }

    strcat(outputBuffer, "\n");

} /* massageDecodeBuffer */

#if FALSE

/**
 * printDecodeBuffer
 *
 * Print the lines of a buffer (from LS$DECODE).
 * The input buffer has an unknown number of NUL-terminated strings that we
 * want to print.
 * Two NULs in a row signify the end of the strings in the buffer.
 *
 * Note: This function is no longer called,
 * since it was replaced by massageDecodeBuffer.
 * But we are leaving it here in case it's needed in the future.
 *
 * @param bufin
 *   The input buffer (returned from LS$DECODE).
 */

void printDecodeBuffer(char * bufin) {

    char * buf = bufin;

    while (*buf != '\0') {
        printf("%s\n",buf);
        buf = buf + strlen(buf) + 1;
    }

    printf("\n");

} /* printDecodeBuffer */

#endif

/**
 * signalHandlerLevel<n>
 *
 * This is the signal handler for the nth level of cascading
 * signals handlers.
 *
 * @param sig
 *   The signal being processed (passed from the C runtime routine
 *   handling the contingency).
 */

void signalHandlerLevel1(int sig) {

    char message[5000];

    /* With spacing, allow 4 character per octal byte */
    char cgyPkt[CGY_PKT_BYTE_SIZE*4];

    /* Register signals for level 2.
       This will cause the level 2 handler to be called
       if the step 1 code below causes a contingency.
       */
    regSignals(&signalHandlerLevel2);

    if (DEBUG) {
        printf("in signalHandlerLevel1: sig=%o\n", sig);
    }

    /* Test if it was a subsystem deactivation (SIGDEACT) */
    if (sig == SIGDEACT){
        /* Yes, call comapiDeactCheck to check if it was in COMAPI */
       if (comapiDeactCheck(IN_SIGNALHANDLERLEVEL1)){
           return; /* It was, return since our COMAPI calling code can handle recovery */
       }
       /* If not in COMAPI, handle like any other fatal contingency. */
    }

    /* Get info about the contingency */
    getContingencyInfo(sig, cgyStr, sigStr, &cgyTypeInt, errTypeStr);
    getOctalBytes(cgyPkt, (char *) &_sg_data.contingency_packet,
        CGY_PKT_BYTE_SIZE);

    /* Server signal processing step 1:
       Insert contingency message in log file.
       Note: String literal continued across lines - do not adjust.
       */
        sprintf(message,
        "\nUnrecoverable contingency occurred during server execution.\n\
  Signal name:%s, Contingency:%s.\n\
  Signal number:%o, Contingency type:%02o, Error type:%s.\n\
  Reentry address:%012o.\n\
  Server is being shut down.\n\n\
  Contingency packet:\n%s",
        sigStr, cgyStr, sig, cgyTypeInt, errTypeStr, _sg_data.reentry_address,
        cgyPkt);

    /* Decode the reentry address */
    decodeAddress(_sg_data.reentry_address,
        _sg_data.contingency_packet.modifiable_execution_data.
        originating_ss_va, message+strlen(message));

    addServerLogEntry(SERVER_LOGS, message);

    /* Call the next cascading signal handler */
    signalHandlerLevel2(sig);
}

static void signalHandlerLevel2(int sig) {

    /* Register signals for level 3.
       This will cause the level 3 handler to be called
       if the step 2 code below causes a contingency.
       */
    regSignals(&signalHandlerLevel3);

    if (DEBUG) {
        printf("in signalHandlerLevel2: sig=%o\n", sig);
    }

    /* Server signal processing step 2:
       Dump the SGD and the workers in symbolic format.
       I.e., display, in the server log file, info from the configuration
       (sgd variables) and info for each server worker
       */
    processDisplayConfigurationCmd(TRUE, TO_SERVER_LOGFILE, FALSE, NULL);
    displayAllActiveWorkersInLog(TO_SERVER_LOGFILE, TRUE);

    /* Call the next cascading signal handler */
    signalHandlerLevel3(sig);
}

static void signalHandlerLevel3(int sig) {

    int status;
    int logStatus;
    int traceStatus;

    /* Register signals for level 4.
       This will cause the level 4 handler to be called
       if the step 3 code below causes a contingency.
       */
    regSignals(&signalHandlerLevel4);

    if (DEBUG) {
        printf("in signalHandlerLevel3: sig=%o\n", sig);
    }

    /* Server signal processing step 3:
       Close server files.
       */
    status = closeServerFiles(&logStatus, &traceStatus);


    /* Call the next cascading signal handler */
    signalHandlerLevel4(sig);
}

static void signalHandlerLevel4(int sig) {

    /* Server signal processing step 4 (the last one):
       Shut down all JDBC server tasks,
       after deregistering COMAPI.
       Note that control does not return to the caller.
       */

    if (DEBUG) {
        printf("in signalHandlerLevel4: sig=%o\n", sig);
    }

#ifndef XABUILD  /* JDBC Server */

    /* Deregister COMAPI from the application,
       and stop all active tasks. */
    deregComapiAndStopTasks(0, NULL, NULL);

#else          /* XA JDBC Server */

    /* No COMAPI usage; just stop all active tasks. */
    tsk_stopall();

#endif          /* XA and JDBC Servers */

}

/**
 * getContingencyInfo
 *
 * Return info about a contingency, based on the signal type.
 *
 * @param sig
 *   The signal type (int).
 *
 * @param cgyString
 *   A string (allocated by the caller) containing the contingency condition.
 *
 * @param signalString
 *   A string (allocated by the caller) containing the signal macro name.
 *
 * @param cgyType
 *   The contingency type (int).
 *
 * @param errType
 *   The error type (as an octal number) in string format.
 *   The string is allocated by the caller.
 *   The string may be 'any'.
 *
 */

static void getContingencyInfo(int sig, char * cgyString,
                                 char * signalString, int * cgyType,
                                 char * errType) {

    switch (sig) {
        case SIGOPR:
            strcpy(cgyString, "invalid operation");
            strcpy(signalString,"SIGOPR");
            *cgyType = 1;
            strcpy(errType, "0");
            break;
        case SIGGDM:
            strcpy(cgyString, "guard mode");
            strcpy(signalString,"SIGGDM");
            *cgyType = 2;
            strcpy(errType, "0");
            break;
        case SIGFOF:
            strcpy(cgyString, "floating overflow");
            strcpy(signalString,"SIGFOF");
            *cgyType = 3;
            strcpy(errType, "0");
            break;
        case SIGFUF:
            strcpy(cgyString, "floating underflow");
            strcpy(signalString,"SIGFUF");
            *cgyType = 4;
            strcpy(errType, "0");
            break;
        case SIGDOF:
            strcpy(cgyString, "divide fault");
            strcpy(signalString,"DOF");
            *cgyType = 5;
            strcpy(errType, "0");
            break;
        case SIGRST:
            strcpy(cgyString, "restart contingency");
            strcpy(signalString,"SIGRST");
            *cgyType = 6;
            strcpy(errType, "any");
            break;
        case SIGIABT:
            strcpy(cgyString, "ER ABORT$ or EABT$");
            strcpy(signalString,"SIGIABT");
            *cgyType = 7;
            strcpy(errType, "0");
            break;
        case SIGINT_C:
            strcpy(cgyString, "console interrupt");
            strcpy(signalString,"SIGINT_C");
            *cgyType = 010;
            strcpy(errType, "any");
            break;
        case SIGII:
            strcpy(cgyString, "II command by system console");
            strcpy(signalString,"SIGII");
            *cgyType = 010;
            strcpy(errType, "1");
            break;
        case SIGRBK:
            strcpy(cgyString, "remote break @@X C");
            strcpy(signalString,"SIGRBK");
            *cgyType = 010;
            strcpy(errType, "2");
            break;
        case SIGX_CT:
            strcpy(cgyString, "@@X CT interactive signal");
            strcpy(signalString,"SIGX_CT");
            *cgyType = 010;
            strcpy(errType, "3");
            break;
        case SIGTS:
            strcpy(cgyString, "test and set interrupt");
            strcpy(signalString,"SIGTS");
            *cgyType = 011;
            strcpy(errType, "0");
            break;
        case SIGERR_:
            strcpy(cgyString, "error mode");
            strcpy(signalString,"SIGERR");
            *cgyType = 012;
            strcpy(errType, "any");
            break;
        case SIGICGY:
            strcpy(cgyString, "interactivity interrupt");
            strcpy(signalString,"SIGICGY");
            *cgyType = 013;
            strcpy(errType, "any");
            break;
        case SIGBRKPT:
            strcpy(cgyString, "breakpoint mode");
            strcpy(signalString,"SIGBRKPT");
            *cgyType = 014;
            strcpy(errType, "0");
            break;
        case SIGHWFLT:
            strcpy(cgyString, "hardware fault");
            strcpy(signalString,"SIGHWFLT");
            *cgyType = 015;
            strcpy(errType, "any");
            break;
        case SIGMSUPS:
            strcpy(cgyString, "max SUPS or PCT overflow");
            strcpy(signalString,"SIGMSUPS");
            *cgyType = 016;
            strcpy(errType, "any");
            break;
        case SIGABTRM:
            strcpy(cgyString, "abnormal program termination");
            strcpy(signalString,"SIGABTRM");
            *cgyType = 017;
            strcpy(errType, "0");
            break;
        case SIGRTFLT:
            strcpy(cgyString, "storage fault in R/T program");
            strcpy(signalString,"SIGRTFLT");
            *cgyType = 017;
            strcpy(errType, "1");
            break;
        case SIGTIMOU:
            strcpy(cgyString, "timeout warning");
            strcpy(signalString,"SIGTIMOU");
            *cgyType = 020;
            strcpy(errType, "0");
            break;
        case SIGELINK:
            strcpy(cgyString, "linking System error");
            strcpy(signalString,"SIGELINK");
            *cgyType = 021;
            strcpy(errType, "1");
            break;
        case SIGSGNL:
            strcpy(cgyString, "SGNL instr. interrupt or EM ER");
            strcpy(signalString,"SIGSGNL");
            *cgyType = 021;
            strcpy(errType, "2");
            break;
        case SIGSTACK:
            strcpy(cgyString, "stack overflow or underflow");
            strcpy(signalString,"SIGSTACK");
            *cgyType = 021;
            strcpy(errType, "3");
            break;
        case SIGVFY:
            strcpy(cgyString, "call or return verification error");
            strcpy(signalString,"SIGVFY");
            *cgyType = 021;
            strcpy(errType, "4");
            break;
        case SIGCGY:
            strcpy(cgyString, "contingency handler fault");
            strcpy(signalString,"SIGCGY");
            *cgyType = 021;
            strcpy(errType, "5");
            break;
        case SIGDEACT:
            strcpy(cgyString, "subsystem deactivated");
            strcpy(signalString,"SIGDEACT");
            *cgyType = 021;
            strcpy(errType, "6");
            break;
        default:
            strcpy(cgyString, "unknown");
            strcpy(signalString, "unknown");
            *cgyType = 0;
            strcpy(errType, "0");
    }

} /* getContingencyInfo */

/**
 * printOneLineViaAprint
 *
 * Use ER APRINT$ to print one image (one line, max 132 characters).
 * Avoid using UC I/O.
 *
 * @param s1
 *   A char pointer to a string.
 */

void printOneLineViaAprint(char * s1) {

    int code;
    int in;
    int out;
    int inarr[1];
    int outarr[2];
    int scratch;
    int byteCount;
    int wordCount;
    int index;

     /* Allow for blank fill and NUL */
    char str[MAX_CHARS_IN_APRINT+BYTES_PER_WORD+1];

    typedef union {
        /* ER APRINT$ packet word format */
        struct {
            int lineSpacing : 12;
            int wordSize : 6;
            int addr : 18;
        } p1;

        int i1;
    } aprintWord;

    aprintWord pkt;

    /* Copy the string into a word aligned, blank filled buffer */
    byteCount = minimum(strlen(s1), MAX_CHARS_IN_APRINT);
    wordCount = (byteCount + (BYTES_PER_WORD - 1)) / BYTES_PER_WORD;
    strncpy(str, s1, MAX_CHARS_IN_APRINT);

    for (index=0; index< 4; index++) {
        str[byteCount+index] = ' ';
    }

    /* Set up the packet word for ER APRINT$*/
    pkt.i1 = (int) str;
    pkt.p1.lineSpacing = 1;
    pkt.p1.wordSize = wordCount;

    /* Set up the parameters for ucsgeneral */
    in = 0400000000000;
    out = 0;
    inarr[0] = pkt.i1;
    code = ER_APRINT;

    /* Perform the ER APRINT$ */
    ucsgeneral(&code, &in, &out, inarr, outarr, &scratch, &scratch);

} /* printOneLineViaAprint */

#if FALSE

/**
 * printBytesInOctal
 *
 * Print a sequence of words in octal (5 words per line).
 *
 * Note: This function is no longer called,
 * so we have a #if around it to turn it off for now.
 *
 * @param data
 *   A pointer to the data to dump.
 *
 * @param byteCount
 *   The number of bytes to dump.
 *
 */

void printBytesInOctal(char * ret, int byteCount) {

    int i1 = 0;
    int i2;
    int wordCount;

    wordCount = (byteCount + (BYTES_PER_WORD - 1)) / BYTES_PER_WORD;

    for (i2=0; i2<wordCount; i2++, i1+=BYTES_PER_WORD) {
        /* Print one word */
        printf(" %03o%03o%03o%03o", ret[i1], ret[i1+1], ret[i1+2], ret[i1+3]);

        /* Insert an end of line character if we are at the last
           array element, or if we have inserted 5 words
           on this line. */
        if ((i2%OCTAL_WORDS_DUMPED_PER_LINE == OCTAL_WORDS_DUMPED_PER_LINE-1)
                || (i2 == wordCount-1)) {
            printf("\n");
        }
    }

} /* printBytesInOctal */

#endif

/**
 * getOctalBytes
 *
 * Return a string containing an octal dump of the data.
 * Five words per lines are dumped.
 *
 * @param retBuf
 *   A caller allocated buffer which contains the returned string.
 *
 * @param data
 *   A pointer to the data to dump.
 *
 * @param byteCount
 *   The number of bytes to dump.
 *   (The function always dumps full words,
 *   so if the byte count is not a multiple of 4,
 *   extra bytes are dumped.)
 */

static void getOctalBytes(char * retBuf, char * data, int byteCount) {

    int i1 = 0;
    int i2;
    int i3 = 0;
    int wordCount;

    wordCount = (byteCount + (BYTES_PER_WORD - 1)) / BYTES_PER_WORD;

    strcpy(retBuf, "  ");
    i3 = 2; /* index into retBuf */

    for (i2=0; i2<wordCount; i2++, i1+=BYTES_PER_WORD) {
        /* Print one word */
        sprintf(retBuf+i3, " %03o%03o%03o%03o", data[i1], data[i1+1],
            data[i1+2], data[i1+3]);

        /* Increment count by 13 (since we have 12 octal digits and a
           leading space). */
        i3 += 13;

        /* Insert an end of line character if we are at the last
           array element, or if we have inserted 5 words
           on this line. */
        if ((i2%OCTAL_WORDS_DUMPED_PER_LINE == OCTAL_WORDS_DUMPED_PER_LINE-1)
                || (i2 == wordCount-1)) {
            strcpy(retBuf+i3, "\n  ");
            i3 += 3;
        }
    }

    /* NUL terminate the returned string */
    retBuf[i3] = '\0';
}
