/* Standard C header files and OS2200 System Library files */
#include <CORBA.h>
#include <stdio.h>
#include <termreg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <task.h>

/* JDBC Project Files */
#include "c-interface.h" /* Include crosses the c-interface/server boundary */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "taskcodes.h" /* Include crosses the c-interface/server boundary */
#include "utilities.h" /* Include crosses the c-interface/server boundary */
#include "corbautil.h"
#include "ProcessTask.h"
#include "ServerActWDE.h"
#include "ServerLog.h"
#include "ServerWorker.h"
#include "ShutdownCmds.h"
#include "signals.h"
#include "XASrvrWorker.h"

extern int termstat[2];                    /* two words for termination handler (termreg) */
extern struct _term_buf XAterm_reg_buffer; /* Term Reg buffer. */

extern serverGlobalData sgd;               /* The Server Global Data (SGD),*/
                                           /* visible to all Server activities. */


/*
 * Now declare an extern reference to the activity local WDE pointer act_wdePtr.
 * This pointer points to the XA Server Workers WDE entry in the SGD (the first
 * and only WDE allocated in the XA JDBC Server ). The XA Server Worker only uses
 * act_wdePtr to point to its wde entry since act_wdePtr is declared at file level
 * and remains unchanged between procedure executions in the XA Server Worker
 * activity and since some of the log functions and procedures that are
 * being utilized by the JDBCServer/ConsoleHandler, ICL and Server Worker
 * activities need specific information to tailor their actions (i.e.
 * localization of messages).  The design of those routines utilized fields
 * in Server workers WDE to provide this tailoring information, and the
 * WDE pointer required is not passed to those routines but is assumed to be in
 * the activity local (file level) variable act_wdePtr.
 */
extern workerDescriptionEntry *act_wdePtr;
extern workerDescriptionEntry anActivityLocalWDE;

#define XASERVER_NOT_INITIALIZED              0     /* Indicates XA JDBC Server is not yet initialized */
#define XASERVER_INITIALIZED                  1     /* Indicates XA JDBC Server did initialize correctly, ready for first client */
#define XASERVER_REINITIALIZE_ON_NEXT_CLIENT  2     /* Indicates XA JDBC Server should re-initialize on next client */
#define XASERVER_INITIALIZATION_ERRORED      -1     /* Indicates XA JDBC Server did not initialize correctly */

static int xa_JDBCServer_initialized = XASERVER_NOT_INITIALIZED;  /* The XA JDBC Server initialization flag, */
                                           /* it can only be set to XASERVER_NOT_INITIALIZED,
                                           /* XASERVER_INITIALIZED, or XASERVER_INITIALIZATION_ERRORED */

static int errMsgNumber = 0;               /* Assume that initialization will work OK - no error messages */
static int enableRecoveryTest = 0;         /* Special recovery testing flag. */
static char * reusable_max_CORBA_octet_ptr;/* Pointer to the reusable response packet CORBA octet byte array of maximal size. */

/*
 * XAServerWorker
 *
 * All occurrences of XASendAndReceive for the 16 XA Servers
 *   call the following common routine.
 *   The arguments are simply passed through from XASendAndReceive.
 *   The 16 XASendAndReceive functions are in xajdbcserver.c.
 *   Note: The XA Server number (1-16) is not necessarily the same
 *   as the app group number.
 *
 * @param _obj
 *   A CORBA_Object.
 *
 * @param CORBAreqPkt
 *   CORBAreqPkt is a CORBA byte array structure that contains a pointer
 *   to the variable length byte array that is the RDMS-JDBC request packet.
 *
 * @param _env
 *   A CORBA_Environment pointer.
 *
 * @param txn_flag
 *   An ODTP provided flag derived from the CORBA request packet indicating
 *   whether this request is inside a transaction. If equal to 0, we are not
 *   in a transaction. If equal to 1, then we are inside a transaction.
 *
 * @return
 *   The function returns a CORBA byte array structure that points to
 *   the variable length byte array that is the RDMS-JDBC response packet.
 */

com_unisys_os2200_rdms_jdbc_byteArray * XAServerWorker(
        CORBA_Object _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * CORBAreqPkt,
        CORBA_Environment *_env,
        int txn_flag) {

    com_unisys_os2200_rdms_jdbc_byteArray * CorbaResPkt;
    CORBA_octet * reqPkt = NULL;  /* Will point to request packet */
    CORBA_octet * resPkt = NULL;  /* Will point to response packet */
    int activity_id;
    int error_status;
    int xind;
    int xtaskcode;
    int index;
    int xtask_status_value;
    int lenReq;
    int actualPacketSize;
    int indexReqPktLen = REQ_LENGTH_OFFSET; /* offset to request packet's byte length */
    int indexResPktLen = RES_LENGTH_OFFSET; /* offset to response packet's byte length */
    char digits[ITOA_BUFFERSIZE];
    char * msg2;
    char errorInsert[100];
    char * errorMessage;
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    int message_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;
    char saveClientTraceFileName[MAX_FILE_NAME_LEN+1];
    int saveClientTraceFileNbr;
    FILE * saveClientTraceFile;
    int saveConsoleSetClientTraceFile;
    c_intDataStructure * pDS;

    /*
     * Allocate large enough space to hold the constructed client/server
     * instance string and anything else that may be added after it.
     */
    char clientsServerInstanceIdent[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char serversServerInstanceIdent[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    /*
     * Determine if this is the first invocation of the XA JDBC
     * server by testing the state of variable xa_JDBCServer_initialized.
     *
     * We must initialize the XA JDBC Server environment before
     * passing the request packet to processClientTask for
     * handling. Also before initializing the SGD, register a
     * term reg handling procedure in case we get a exit termination
     * (either regular or abnormal).
     *
     * The activity local variable xa_JDBCServer_initialized is
     * initially set to XASERVER_NOT_INITIALIZED when the XA JDBC
     * Server begins. After initialization, it will reflect whether
     * the initialization was successful (XASERVER_INITIALIZED) or
     * whether it failed (XASERVER_INITIALIZION_ERRORED).
     */
    if (sgd.debugXA) {
       printf("In XAServerWorker(), xa_JDBCServer_initialized = %d\n", xa_JDBCServer_initialized);
    }

    if (xa_JDBCServer_initialized == XASERVER_INITIALIZATION_ERRORED){
        /*
         * The XA JDBC SERVER did not initialize correctly.
         * Return a response packet with an error message to the caller
         */
        msg2 = getLocalizedMessageServer(XASERVER_INITIALIZATION_ERROR,
                                         itoa(errMsgNumber, digits, 10), NULL, 0, message);
        resPkt = genErrorResponse(JDBC_SERVER_ERROR_TYPE,
                                  XASERVER_INITIALIZATION_ERROR, 0, msg2, BOOL_FALSE);

        /* Form the Corba byte array structure, use actual response packet length. */
        actualPacketSize = getIntFromBytes(resPkt,&indexResPktLen);
        CorbaResPkt = formCORBAPacket(resPkt, actualPacketSize);

        if (sgd.debugXA) {
           /* Debug info about the response packet generated */
           printf("In XaServerWorker() at bad XA Server Initialization Error check, CorbaResPkt= %p, response packet len = %d, act_wdePtr->resPktLen= %d\n",
                  CorbaResPkt, actualPacketSize, act_wdePtr->resPktLen);
        }
        act_wdePtr->in_COMAPI = CALLING_COMAPI; /* use the COMAPI flag for OTM returns (calls) as well */
        return CorbaResPkt;
    }

    else if (xa_JDBCServer_initialized == XASERVER_NOT_INITIALIZED){
        /*
         * The XA JDBC SERVER needs to be initialized.
         * Attempt to initialize the XA Server environment.
         */

        /* Set up the termreg procedure handler. "123456" is an arbitrary value just
           used to easily be seen in the termreg output.*/
        _reg4term$c(XA_SW_term_reg, 2, 123456, &XAterm_reg_buffer, termstat);

        /* Initialize the XAJDBC Server environment. This will set up
        * an activity local wde capable of handling the log/trace files.
        * It also registers signals for this XA server worker activity.
        */
        initialize_XAJDBC_Server(&errMsgNumber);

        /* Note: We will test the errMsgNumber later after we have established the
         * response packet (act_wdePtr->releasedResponsePacketPtr). We cannot generate
         * an error response yet.
         */

        /*
         * Store the RSA BDI, as part of initialization, for the app group being used
         * for later RSA calls. Do this via an RSA call.
         */
        RSA$SETBDI2(sgd.rsaBdi);

        /* Test if we should log procedure entry */
        /* if (sgd.debug){ */
        /*     addServerLogEntry(SERVER_STDOUT,"Inside XAServerWorker just after initialization");  */
        /*   } */

        /* If debug, add a new log entry before processing the new JDBC Client. */
        if (sgd.debug){
           tsk_id(&activity_id);
           getMessageWithoutNumberForWorker(SERVER_NEW_SW,
                     itoa(activity_id, digits, 10), NULL, SERVER_LOGS_DEBUG);
                     /* 5361 New JDBC Server Worker {0} initializing
                        for new JDBC Client */
        }

        /* Now set up the act_wdePtr for this activity.  Make it look
         * like a Server Worker, so assign the first wde entry in the
         * wdeArea to the XA Server Worker.
         */
        act_wdePtr=&sgd.wdeArea[sgd.numWdeAllocatedSoFar]; /* point to next new wde entry */
        /* printf("Allocated space for a new WDE, at wdeArea[%i]\n",sgd.numWdeAllocatedSoFar); */
        sgd.numWdeAllocatedSoFar++;

        /*
         * Now allocate a C_INT data structure so it can
         * be used by the C-Interface routines to hold its
         * state information. Also allocate an initial sized
         * PCA for it now, and clear out the new PCA's packet
         * length, JDBC Section offset, etc.. Its space will
         * be reused as long as the C-INT structure is reused.
         */
        act_wdePtr->c_int_pkt_ptr = malloc(sizeof(c_intDataStructure)); /* Always malloc(), not Tmalloc() */
        ((c_intDataStructure *)act_wdePtr->c_int_pkt_ptr)->pca_ptr = Tmalloc(8, "XASrvrWorker.c", MIN_PCA_SPACE_TO_ALLOCATE_IN_BYTES); /* Always malloc() not Tmalloc() */
        ((c_intDataStructure *)act_wdePtr->c_int_pkt_ptr)->pca_size = MIN_PCA_SPACE_TO_ALLOCATE_IN_WORDS;
        clearPcaSpace(((c_intDataStructure *)act_wdePtr->c_int_pkt_ptr)->pca_ptr);

        /* Minimally initialize the wde entry. */
        minimalInitializeWDE(act_wdePtr,SERVER_WORKER);

        /*
         * Register signals for this XA Server Worker activity (re-registration is
         * done since the signal handling registration done in initialize_XAJDBC_Server()
         * is only for the initialization process, not the Server Worker type operation
         * being done by the XA Server Worker at this point.
         */
#if USE_SIGNAL_HANDLERS
        regSignals(&serverWorkerSignalHandler);
#endif

        /* Now initialize the XA Server Worker.  Since this involves a significant
         * number of assignment statements, use a modified version of the routine
         * found in ServerWorker.c (conditionally compiled for XA Server usage ).
         */
        initializeServerWorkerWDE(act_wdePtr);
        act_wdePtr->serverWorkerState=CLIENT_ASSIGNED;  /* we got a client to process */

        /*
         * Since this is the very first XA Server Worker client, we need to initially
         * allocate this XA Server's reusable CORBA_Octet byte array. It is allocated
         * to the maximum allowed size, so it can accomodate all future request packet
         * requests. The CORBA_Octet byte array will not be free'd during the lifetime
         * of the JDBC XA Server; it will be used for all the response packets generated
         * by this XA Server across all its clients, and will go away automatically when
         * the Server shuts down.
         */
        reusable_max_CORBA_octet_ptr = (char *) CORBA_sequence_octet_allocbuf(MAX_ALLOWED_RESPONSE_PACKET + RES_DEBUG_INFO_MAX_SIZE);
        act_wdePtr->releasedResponsePacketPtr = reusable_max_CORBA_octet_ptr; /* Use the reusable CORBA_Octet byte array. */

        /* Check if there were any problems getting the reusable CORBA_Octet byte array (returned pointer is NULL). */
        if (act_wdePtr->releasedResponsePacketPtr == NULL){
            /*
             * Couldn't allocate space for response packet. Generate error
             * message insert. Output a JDBC$XALOG message and trace image.
             */
            sprintf(errorInsert,"%d for reuse in XAServerWorker().", (MAX_ALLOWED_RESPONSE_PACKET + RES_DEBUG_INFO_MAX_SIZE));
            errorMessage = getLocalizedMessage(SERVER_CANT_ALLOC_RESPONSE_PKT, errorInsert, NULL, SERVER_LOGS, message);
                    /* 5300 Can't allocate a response packet of size {0} */
            if (debugInternal){
                tracef("In XAServerWorker(): %s\n", errorMessage);
            }

            /* Now force a IGDM since we can't handle this error any other
             * way. Use the maximum allowed size - we may be able to easily
             * see it in a register in the dump.
             */
            *(act_wdePtr->releasedResponsePacketPtr) = (MAX_ALLOWED_RESPONSE_PACKET + RES_DEBUG_INFO_MAX_SIZE);
        }

        /* Now we can finally check the status from the call to initialize_XAJDBC_Server() */
        /* We had to wait until after the response packet was created. */
        /* test if an initialization error occurred */
        if (errMsgNumber != 0){
            /*
             * Error message response returned, return an
             * error response packet to the caller. Set initialization
             * error state.
             */
            xa_JDBCServer_initialized = XASERVER_INITIALIZATION_ERRORED; /* indicate error state */

            msg2 = getLocalizedMessageServer(XASERVER_INITIALIZATION_ERROR,
                                             itoa(errMsgNumber, digits, 10), NULL, 0, message);
            resPkt = genErrorResponse(JDBC_SERVER_ERROR_TYPE,
                                      XASERVER_INITIALIZATION_ERROR, 0, msg2, BOOL_FALSE);

            /* Form the Corba byte array structure, use actual response packet length. */
            actualPacketSize = getIntFromBytes(resPkt,&indexResPktLen);
            CorbaResPkt = formCORBAPacket(resPkt, actualPacketSize);

            if (sgd.debugXA) {
                /* Debug info about the response packet generated */
                printf("In XaServerWorker() at bad initialization detected, CorbaResPkt= %p, response packet len = %d, act_wdePtr->resPktLen= %d\n",
                       CorbaResPkt, actualPacketSize, act_wdePtr->resPktLen);
            }
        act_wdePtr->in_COMAPI = CALLING_COMAPI; /* use the COMAPI flag for OTM returns (calls) as well */
        return CorbaResPkt;
        }

        /*
         * The 'H' option on the XA Server invocation indicates whether the JDBC XA Server
         * should have an active console handler. Do we want a console handler?
         */
        if (testOptionBit('H')){
            /*
             * No, no XA JDBC Server Console Handler activity is requested. The XA Server
             * Worker can continue executing properly without a console handler, so just
             * indicate that it is not active.
             */
            sgd.consoleHandlerState = CH_INACTIVE;
            sgd.consoleHandlerShutdownState = CH_SHUTDOWN;
        } else {
            /*
             * Yes, begin the XA JDBC Server Console Handler activity. It logs its startup condition.
             */
            start_XAServerConsoleHandler();
        }

        /* XA Server Worker is initialized. */
        xa_JDBCServer_initialized = XASERVER_INITIALIZED;

        if (sgd.debugXA) {
            printf("In XAServerWorker(), xa_JDBCServer_initialized = %d\n", xa_JDBCServer_initialized);
        }

    } /* end of XA Server Worker's environment initialization */

    /*
     * To get here, the XA JDBC Server Worker's XA JDBC Server's
     * environment must be initialized.  So let's begin the work
     * of processing the request packet.
     */

    /* Indicate we are processing a request packet. This is done so that the Console
     * Handler Display Worker status short command can indicate whether we are in the
     * XASrvrWorker or in OTM. At this time, use the WDE in_COMAPI flag
     * for this. <-- This works because we are not using COMAPI when we use OTM.  This
     * will be replaced with a XA JDBC Server specific flag in a later level.
     */
    act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI; /* use the COMAPI flag to indicate not in OTM calls */

    /* Before processing the request packet, test if the Server Worker
     * shutdown flag is set, indicating the XA Server Worker must
     * terminate immediately.
     */
    if ((act_wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_GRACEFULLY ) ||
        (act_wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_IMMEDIATELY) ||
        (sgd.serverShutdownState == SERVER_SHUTDOWN_GRACEFULLY)            ||
        (sgd.serverShutdownState == SERVER_SHUTDOWN_IMMEDIATELY)              ){
        /* Shutdown the Server Worker */
        XAServerWorkerShutDown(0);
    }

    /* Request packet: Get the length and byte array pointer. */
    getCORBAByteArray(CORBAreqPkt, &lenReq, &reqPkt);

    /* First, dump the request packet */
    if (sgd.debugXA) {
        actualPacketSize = getIntFromBytes(reqPkt,&indexReqPktLen);
        printf("In XAServerWorker, request packet lengths (Corba, internal) = (%d, %d)\n", lenReq, actualPacketSize);
        printf("  Request packet dump in hex:\n");
        printCORBAByteArrayInHex(reqPkt, lenReq);

        xind=REQ_TASK_CODE_OFFSET;
        /* printf("Before calling getShortFromBytes to get task code\n"); */
        xtaskcode = getShortFromBytes(reqPkt, &xind);
        /* printf("After calling getShortFromBytes to get task code\n"); */
        printf("In XAServerWorker, the request packet task code = %d (%s)\n\n",xtaskcode,taskCodeString(xtaskcode));
    }

    /*  hexdump(reqPkt,8);  */
    /*    printf("Request Packet validation byte is=%i\n",reqPkt[REQ_ID_OFFSET]);  */

    /*
     *
     * Now test the one byte request packet id - if it is wrong, we
     * cannot trust the request packet length value.
     * If the id is wrong, treat that as a fatal XA JDBC Client error,
     * place an error message into the log file and return an error
     * response packet.
     */
    if (reqPkt[REQ_ID_OFFSET] != REQUEST_VALIDATION ){
         /*  hexdump(reqPkt,8);
             printf("Request Packet validation byte is=%i\n",reqPkt[REQ_ID_OFFSET]);
          */
         getLocalizedMessage(XASERVER_BAD_REQUEST_PKT_ID,
             itoa(reqPkt[REQ_ID_OFFSET], digits, 10), NULL, SERVER_LOGS, message);
             /* 5401 Bad Request Packet id, id = {0}, shutting client
                down. (Likely mismatch in JDBC client and server levels.) */

         /*
          * Assume the validation id mismatch is because of a JDBC Driver/Server
          * level mismatch. Likely we are looking at the first request packet
          * sent from the client, the begin thread request packet.  Let's assume
          * it is a JDBC Driver of a different product level and let's try to tell
          * that JDBC Driver about the client/server level mismatch.
          *
          * Send back to the JDBC client an error response packet that indicates
          * the probable JDBC client/server level mismatch. That way a JDBC Driver
          * can return a reasonable SQL Exception back to its JDBC client.
          *
          * Notice: This now uses error message 0009 which replaces the error 5401
          * message that previously indicated a client/server mismatch. This was done
          * so there is a consistent error message number for all modes of JDBC client
          * and server (JDBCServer, JDBC XA Server, RDMSJDBC) and so the message (0009)
          * can provide appropriate informational information to assist the user.
          *
          * Generate the special response packet indicating the mismatch client/server
          * level.
          */
         resPkt = generate_ID_level_mismatch_response_packet(reqPkt[REQ_ID_OFFSET]);

         /* Form the Corba byte array structure, use actual response packet length. */
         actualPacketSize = getIntFromBytes(resPkt,&indexResPktLen);
         CorbaResPkt = formCORBAPacket(resPkt, actualPacketSize);

         if (sgd.debugXA) {
             /* Debug info about the response packet generated */
             printf("In XaServerWorker() at bad request packet id detection, CorbaResPkt= %p, response packet len = %d, act_wdePtr->resPktLen= %d\n",
                    CorbaResPkt, actualPacketSize, act_wdePtr->resPktLen);
         }

         /* Return the error response packet. */
         act_wdePtr->in_COMAPI = CALLING_COMAPI; /* use the COMAPI flag for OTM returns (calls) as well */
         return CorbaResPkt;
    }

    /*
     * Get the task code of the request packet.  If it is a USERID_PASSWORD_TASK
     * or XA_BEGIN_THREAD_TASK or BEGIN_THREAD_TASK and we are going to be processing
     * the second or later client, then we need to reset the wde since we have a new
     * client and we need to clean up from the last client (e.g. close client trace
     * file, etc.).
     */
    xind=REQ_TASK_CODE_OFFSET;
    /* printf("Before calling getShortFromBytes to get task code\n"); */
    xtaskcode = getShortFromBytes(reqPkt, &xind);
    if (xtaskcode == USERID_PASSWORD_TASK || xtaskcode == XA_BEGIN_THREAD_TASK ||
        xtaskcode == BEGIN_THREAD_TASK){
        /* Test if this is the first or a subsequent client. */
        if (xa_JDBCServer_initialized == XASERVER_REINITIALIZE_ON_NEXT_CLIENT){
            /* Test for the past client's client Trace file. Close it if there is one. */
            if ((act_wdePtr->clientTraceFile != NULL)
                && (! act_wdePtr-> consoleSetClientTraceFile)) {
                /* A Client Trace file is present, attempt to close it,
                   unless a console SET CLIENT TRACE FILE command
                   changed it.
                   In that case, leave that client trace file open
                   for all subsequent clients for this XA Server run. */
                error_status= closeClientTraceFile(act_wdePtr, message, message_len);
                if (error_status !=0){
                    /*
                     * Error occurred in trying to close past client's trace file. Log
                     * this error into the JDBC Server Log file.
                     */
                    addServerLogEntry(SERVER_LOGS, message);
                }
            }

            /* First, re-minimally initialize the wde entry, and then re-initialize
             * the wde for use as a Server Worker.
             * First save some fields for the client trace file.
             */

            saveConsoleSetClientTraceFile =
                act_wdePtr-> consoleSetClientTraceFile;

            if (saveConsoleSetClientTraceFile) {
                saveClientTraceFileNbr = act_wdePtr-> clientTraceFileNbr;
                saveClientTraceFile = act_wdePtr-> clientTraceFile;
                strcpy(saveClientTraceFileName,
                    act_wdePtr-> clientTraceFileName);
            }

            /*
             * Make sure that all the data structures maintained for
             * the previous client's database connection are now freed.
             *
             * Remember the last client has finished with the database
             * thread (OLTP saw to that), so before the call to
             * freeAllStructures(), get access to the c_intDataStructure
             * and indicate the database connection is " closed". The rest
             * of the fields in the c_intDataStructure will be reset by
             * the initializeC_INT()call done by the following
             * initializeServerWorkerWDE() call.
             *
             * If we are doing an XA_BEGIN_THREAD_TASK and XA database
             * thread reuse is enabled, then we need to determine if we
             * are likely to be reusing the previous still open XA
             * database thread. If so, it is necessary to tell freeAllStructures()
             * to DROP any result set cursors that are still present since
             * we don't want old cursors to still exist for the new client.
             * (If we are not reusing a XA database thread, any cursors
             * still open will be closed or were closed when the database
             * thread ends.
             */
            pDS = get_C_IntPtr();
            pDS->threadIsOpen = 0;

#if XA_THREAD_REUSE_ENABLED
            /*
             * Do the quick check now to see if the previous client left a possible reusable XA
             * database thread still open.
             *
             * Note: Save the get_tx_info() retrieved information (trx_mode, transaction_state)
             * in the SGD for later XA database thread reuse checking in XA reuse test cases 2, 3, 5
             * and 6, in doBeginThread(). This save doing another expensive tx_info() call.
             */
            if (getSaved_beginThreadTaskCode() == XA_BEGIN_THREAD_TASK){
                /* Get previous transactional state. */
                sgd.trx_mode = get_tx_info(&sgd.transaction_state); /* Save the tx_info() information in SGD.*/

                /* Test for possible reusable open XA database thread. */
                if (sgd.transaction_state != TX_ROLLBACK_ONLY && !is_xa_reuse_counter_at_limit()){
                    /* Yes, we need to drop cursors since previous database thread is open and may be reused. */
                    freeAllStructures(1);
                } else {
                    /* XA database thread is either rolled back and closed or will not be reused. */
                    freeAllStructures(0);  /* We don't need to drop cursors since previous database thread will be or is ended. */
                }
            } else {
                /* Was not an XA Begin Thread Task, so no XA database thread reuse possible. */
                freeAllStructures(0);  /* We don't need to drop cursors since previous database thread will be or is ended. */
            }

#else
            /*
             * Not using XA database thread reuse, so don't need to drop
             * cursors since previous database thread will/is ended.
             */
            freeAllStructures(0);

#endif /* XA_THREAD_REUSE_ENABLED */

            /* Now re-initialize the WDE so it can be used by the this new client. */
            minimalInitializeWDE(act_wdePtr,SERVER_WORKER);
            initializeServerWorkerWDE(act_wdePtr);
            act_wdePtr->serverWorkerState=CLIENT_ASSIGNED;  /* we got a client to process */
            act_wdePtr->releasedResponsePacketPtr = reusable_max_CORBA_octet_ptr; /* Use the reusable CORBA_Octet byte array. */

            /* Now restore some fields for the client trace file */
            if (saveConsoleSetClientTraceFile) {
                act_wdePtr-> consoleSetClientTraceFile =
                    saveConsoleSetClientTraceFile;
                act_wdePtr-> clientTraceFileNbr = saveClientTraceFileNbr;
                act_wdePtr-> clientTraceFile = saveClientTraceFile;
                strcpy(act_wdePtr-> clientTraceFileName,
                    saveClientTraceFileName);
            }

        }
        else {
            /*
             * This is the first client, indicate we will
             * need to do re-initialize on next client.
             */
            xa_JDBCServer_initialized = XASERVER_REINITIALIZE_ON_NEXT_CLIENT;
        }
    }

     /*
      * Save the ODTP obtained txn flag for later use. This value is derived
      * from the CORBA request packet and indicates whether this request packet
      * is within a global transaction context (1) or not (0).
      *
      * However, it DOES NOT indicate that we are currently within an active
      * branch of the global transaction (i.e., the XA_BEGIN_THREAD_TASK
      * request packet handling will call _reg_uds_resource() to have ODTP
      * enlist us as an active branch of the global transaction).
      */
    act_wdePtr->txn_flag = txn_flag;

    /*
     * Before processing the task, verify that we've a correct match between
     * client and Server by verifying the Server instance identification
     * information between that provided in the request packet and the
     * information in the Server's SGD. This test is not done on a
     * USERID_PASSWORD_TASK or XA_BEGIN_THREAD_TASK or BEGIN_THREAD_TASK's
     * since the client information is not set until the BEGIN_THREAD_TASK
     * has been processed.
     */
    if (!(xtaskcode == USERID_PASSWORD_TASK || xtaskcode == XA_BEGIN_THREAD_TASK || xtaskcode == BEGIN_THREAD_TASK) &&
        (testServerInstanceInfo(reqPkt + REQ_SERVERINSTANCE_IDENT_OFFSET))){
            /*
             * Client and Server have mismatch on server instance identification
             * values.  Generate an error log entry to place in JDBC$XALOG. This situation
             * should not happen, so it is best to shut down this JDBC Server Worker
             * so we can correct this problem. (Note: Use the DETECTED_BAD_REQUEST_PACKET
             * flag, so serverWorkerShutDown() shuts down in the same manner it does
             * with a bad request packet - in a way a server instance identity mismatch
             * is a reflection of a bad request packet getting to the Server worker.)
             */
            displayableServerInstanceIdent(clientsServerInstanceIdent, reqPkt + REQ_SERVERINSTANCE_IDENT_OFFSET, BOOL_FALSE);
            displayableServerInstanceIdent(serversServerInstanceIdent, NULL, BOOL_FALSE);

            /* Add the required ", on a <taskcode name> (taskcode)" clause at end of Server's instance identification. */
            sprintf(message, ", on a %s (%d)", taskCodeString(xtaskcode), xtaskcode);
            strcat(serversServerInstanceIdent, message);

            msg2 = getLocalizedMessage(JDBCSERVER_SERVER_IDENTIFICATION_MISMATCH, clientsServerInstanceIdent,
                                       serversServerInstanceIdent, SERVER_LOGS, message);
                    /* 5032 Server instance identity mismatch: client driver
                       indicates ({0}), server indicates ({1}) */

            /* Generate a error response packet containing the Client and Server mismatch. */
            act_wdePtr->resPktLen =0; /* Clear out the response packet length. */
            resPkt = genErrorResponse(JDBC_SERVER_ERROR_TYPE, JDBCSERVER_SERVER_IDENTIFICATION_MISMATCH,
                                      0, msg2, BOOL_FALSE);
            error_status = JDBCSERVER_SERVER_IDENTIFICATION_MISMATCH;

    } else {
        /*
         * Either the Client and Server instance identification check passed,
         * or we are processing a USERID_PASSWORD_TASK, XA_BEGIN_THREAD_TASK,
         * or BEGIN_THREAD_TASK, so process it.
         *
         * Note: Since we will be getting a CHAR array from processClientTask() and
         * need a CORBA byte array to return from XAServerWorker(), use a char
         * pointer variable and then cast it after the call.
         */
        act_wdePtr->resPktLen =0; /* Clear out the response packet length. */
        error_status=processClientTask(act_wdePtr, reqPkt, &resPkt);

        /*
         * Test if special recovery testing options are set.
         * The enableRecoveryTest flag ensures that special OLTP commands are
         * only sent once. These commands tell OLTP to delay 30 seconds, which
         * allows time to manually create a failure that results in a recovery. */
        if ( enableRecoveryTest == 0 ) {
            enableRecoveryTest = 1;

            /*  Check for special options that are used when performing
             *  recovery tests.
             */
            if ( testOptionBit('C') ) {
                /* Cause 30 second delay when 2PC Commit is detected */
                _ORB_set_commit_test();
            }
            else if ( testOptionBit('P') ) {
                /* Cause 30 second delay when 2PC Prepare is detected */
                _ORB_set_prepare_test();
            }
        }

    }

    /*
     * Get actual size of response packet from the response packet. The
     * value of act_wdePtr->resPktLen is the allocated buffer size, not the
     * actual amount of that buffer used.
     */
    actualPacketSize = getIntFromBytes(resPkt,&indexResPktLen);

    /* Dump the returned response packet, if desired */
    if (sgd.debugXA) {
        printf("In XAServerWorker, response packet len = %d, act_wdePtr->resPktLen = %d\n", actualPacketSize, act_wdePtr->resPktLen);
        printf("  Response packet dump in hex:\n");
        printCORBAByteArrayInHex(resPkt, actualPacketSize);
    }

    /*
     * Before returning the response packet, test if the Server Worker
     * shutdown flag is set, indicating the XA Server Worker must
     * terminate immediately.
     */
    if ((act_wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_GRACEFULLY ) ||
        (act_wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_IMMEDIATELY) ||
        (sgd.serverShutdownState == SERVER_SHUTDOWN_GRACEFULLY)            ||
        (sgd.serverShutdownState == SERVER_SHUTDOWN_IMMEDIATELY)              ){
      /* Free the Corba response packet and shutdown the Server Worker */
      CORBA_free((com_unisys_os2200_rdms_jdbc_byteArray *)resPkt);
      XAServerWorkerShutDown(0);
    }

    /*
     * If debugInternal or debugSQL, get the task status of the
     * response packet before sending it. If debugInternal or debugSQL,
     * then test the task status of the response packet and if negative,
     * log the fact the response is being sent (we can't do it after).
     * This will produce a trace line as long as there is an open
     * client trace file.  This is probably OK since the trace file is usually
     * kept open until connection is closed, so we would only miss the status
     * on the End Thread operation which is assumed to succeed even if it fails.
     */
    if (debugInternal || debugSQL){
      index = RES_TASK_STATUS_OFFSET;
      xtask_status_value = getByteFromBytes(resPkt,&index);
      if (xtask_status_value < 0){
        tracef("Response packet for thread %s, task %s id returning task status error = %d\n",
               act_wdePtr->threadName, taskCodeString(xtaskcode), xtask_status_value);
      }
    }

    /*
     * The returned error status from processClientTask is not
     * tested at this time. Just return the response packet using
     * the CORBA byte array structure that points to it.
     */
    CorbaResPkt = formCORBAPacket(resPkt, actualPacketSize);

    if (sgd.debugXA) {
            printf("In XaServerWorker() at normal return, CorbaResPkt= %p, response packet len = %d, act_wdePtr->resPktLen= %d\n",
                   CorbaResPkt, actualPacketSize, act_wdePtr->resPktLen);
    }
    act_wdePtr->in_COMAPI = CALLING_COMAPI; /* use the COMAPI flag for OTM returns (calls) as well */

    return CorbaResPkt;

} /* end of XAServerWorker() */

/*
 * Procedure start_XAServerConsoleHandler:
 *
 * This procedure starts the Console handler activity of the XA JDBC
 * Server (XASRVR_CH).
 *
 * If any errors are detected in the start up of the XASRVR_CH activity, an error
 * message is written to the XA JDBC Server Log file.  The XA Server Worker will
 * continue to execute even if there is no Console Handler.
 */
void start_XAServerConsoleHandler(){

    _task_array XASRVR_CH_task_array = {2,0};
    int tskstart_error;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /*
     * Begin the XA JDBC Server's Console Handler (SERVER_CH) activity.
     * The activity will set the Console Handlers state variable if it
     * is indeed actively running.
     */
    tskstart(XASRVR_CH_task_array,XAServerConsoleHandler);

    /*
     * Check if we were successful at starting the Console Handler activity by calling
     * tsktest() which should return 1 ( indicates Console Handler exists and is either
     * executing or suspended). If tsktest() returns 0 then Console Handler is not present
     * (indicates Console Handler is terminated or never existed), so log the error and
     * continue execution.
     */
    tskstart_error=tsktest(XASRVR_CH_task_array);
    if (tskstart_error == 0){
        /*
         * Error occurred in trying to start the Console Handler activity. So place an error
         * message in the LOG file and set the Console Handler's state indicating its not
         * active. The XA Server Worker can continue executing without a console handler.
         */
        getLocalizedMessageServer(SERVER_CANNOT_START_XASERVER_CH_ACTIVITY,
              itoa(tskstart_error, digits, 10), NULL, SERVER_LOGS, msgBuffer);
              /* 5397 Cannot start the Console Handler activity of the
                 XA JDBC Server, error_status = {0}*/
        sgd.consoleHandlerState = CH_INACTIVE;
        sgd.consoleHandlerShutdownState = CH_SHUTDOWN;
    }
    else {
    /*
     * We got the XA JDBC SERVER's Console Handler activity going. Write a log entry
     * to indicate its  activation and return to caller.
     */
    getMessageWithoutNumber(SERVER_XASERVER_CH_STARTED_AND_RUNNING, NULL, NULL, SERVER_LOGS);
         /* 5399 XA JDBC Server's Console Handler activity started and running */
    }
    return;
}

/*
 * Function XAServerWorkerShutDown
 *
 * This function handles the shutdown of the XA Server Worker activity.
 *
 * There is no message sent back via the CORBA calling sequence and control
 * does not return from this procedure.
 *
 * The XA Server Worker termination registration routine uses this routine
 * to shutdown the XA Server Worker in case of an abnormal termination.
 *
 * The input parameter indicates if the call to XAServerWorkerShutDown was
 * due to the shutdown indicator value being set (0), or whether the routine
 * was called as a result of term reg handling (!=0).
 */
 void XAServerWorkerShutDown(int who){

    int error_status;
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    int message_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;

    /* Test if the last client has left an open client Trace file, then close it. */
    if (act_wdePtr->clientTraceFile != NULL) {
        /* A Client Trace file is present, attempt to close it. */
        error_status= closeClientTraceFile(act_wdePtr, message, message_len);
        if (error_status !=0){
            /*
             * Error occurred in trying to close past client's trace file. Log
             * this error into the JDBC Server Log file.
             */
            addServerLogEntry(SERVER_LOGS, message);
        }
    }

    /* Test if a shutdown indicator caused the shutdown or the termination handler. */
    if (who == 0){
        /*
         * Output a log message indicating that detection of the shutdown indicator
         * caused the shutdown - most likely cause is the Console Handler
         * ABORT or TERM commands.
         */
        getMessageWithoutNumber(SERVER_SHUT_DOWN_DUE_TO_INDICATORS, NULL, NULL, SERVER_LOGS);
    } else {
        /*
         * Termination Handler initiated shutdown. Output a log message indicating
         * that term reg caused shutdown.
         */
        getMessageWithoutNumber(SERVER_SHUT_DOWN_DUE_TO_TERMREG, NULL, NULL, SERVER_LOGS);
    }

    /*
     * Use stopServer() to close the log/trace files, signal shutdown
     * to the other activites and exit this activity. Procedure stopServer()
     * will not return.
     */
    stopServer(0); /* parameter is not needed, so use 0. */

    /* Control does not react here. */
 }

/*
 * Function XA_SW_term_reg
 *
 * This function is the termination handler for the XA Server Worker activity.
 *
 * In case of an abnormal termination or other exit$ condition, this procedure
 * will get control and cause the proper shutdown of the XA Server worker.
 *
 * The parameters userid and userval are required by the C runtime term reg handling
 * definition but are not actually used in the XA Server Worker (i.e. they are just
 * placeholders).
 */
void XA_SW_term_reg(int userid, int userval)
{
    /*
     * Invoke the XA Server Worker shutdown procedure to handle
     * the closure/termination of the XA Server Worker activity.
     */

    XAServerWorkerShutDown(1); /* Indicate its the term reg calling. This call does not return. */
}
