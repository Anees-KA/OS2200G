/**
 * File: ServerICL.c.
 *
 * Procedures that comprise the JDBC Server's Initial Connection Listener
 * (ICL) function.
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
#include <tsqueue.h>
#include <task.h>

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "NetworkAPI.h"
#include "ProcessTask.h"
#include "Server.h"
#include "ServerActWDE.h"
#include "ServerConsol.h"
#include "ServerICL.h"
#include "ServerLog.h"
#include "ServerWorker.h"
#include "signals.h"
#include "Twait.h"

#define INT_MAX        34359738367  /* Largest 36 bit positive number */

extern serverGlobalData sgd;                /* The Server Global Data (SGD),*/
                                            /* visible to all Server activities. */
/*
 * Now declare extern references to the activity local WDE pointer and the
 * dummy activity local WDE entry.
 *
 * This is necessary since some of the functions and procedures utilized by
 * the JDBCServer/ConsoleHandler activity are also being used by the ICL and Server
 * Worker activities and those functions need specific information to tailor their
 * actions (i.e. localization of messages).  The design of those routines utilized
 * fields in the Server workers WDE to provide this tailoring information, and the
 * WDE pointer required was not passed to those routines but was assumed to be in
 * the activity local (file level) variable.
 */
extern workerDescriptionEntry *act_wdePtr;
extern workerDescriptionEntry anActivityLocalWDE;

 /*
  * Define the interface to use for up to MAX_SERVER_SOCKETS number of Server
  * sockets.  The actual number this ICL can have is held in the SGD variable
  * sgd.num_server_sockets[ICL_num] for this particular ICL (ICL_num).
  *
  * Currently MAX_SERVER_SOCKETS is set to four.  With this, CPCOMM is used for
  * the first and third socket and CMS for the second and fourth socket. Two
  * server sockets are assigned each listen host specified in the JDBC Server
  * configuration.  Usually, the first two sockets will be for external clients
  * (those using networking services from CPCOMM and CMS), while the second two sockets
  * will be for internal clients that want to avoid network service overhead by having
  * COMAPI handle those sockets directly (i.e. by using a host with an IP address of
  * the form 127.xxx.yyy.zzz ).
  *
  * Note: the JDBC Server can be configured to use either two or four server sockets,
  * based on whether one or two host names are provided for the listen_hosts configuration
  * parameter.
  */
 static int comapi_interface_list[MAX_SERVER_SOCKETS] = { CPCOMM, CMS1100, CPCOMM, CMS1100 };

/*
 * Define the macro that checks if the ICL should call initialConnectionListenerShutDown
 * to shut down the Initial Connection Listener and notofy Server Workers to shutdown.
 */
#define testAndCall_ICLShutdown_IfNeeded(shutDownPointInICL,clientsocket)                    \
             if ((sgd.ICLShutdownState[act_wdePtr->ICL_num] == ICL_SHUTDOWN_GRACEFULLY ) ||  \
                 (sgd.ICLShutdownState[act_wdePtr->ICL_num] == ICL_SHUTDOWN_IMMEDIATELY)){   \
                initialConnectionListenerShutDown(shutDownPointInICL,clientsocket);          \
             } /* end of macro */

/*
 * Procedure initialConnectionListenerShutDown()
 *
 * This procedure shuts down the Initial Connection Listener. No new JDBC Clients
 * are accepted and those active in the JDBC Server, being processed by a Server
 * Worker are notified to stop as well.  Once the Server Workers have completed
 * their dialogues with the JDBC Clients, the Initial Connection Listener
 * will terminate the dialogue with COMAPI.  Then the ICL state is set to ICL_CLOSED
 * and the ICL activity will terminate.
 *
 * The two input parameters indicate:
 * ICLshutDownPoint  - where we were in ICL at the time ICL shutdown was requested.
 * socket         - the allocated socket, if we had TCP_Accept'ed a new JDBC client.
 */
static void initialConnectionListenerShutDown(enum ICLShutdownPoints ICLShutdownPoint
                                              , int client_socket){

    int i;
    int error_status;
    int ICL_num=act_wdePtr->ICL_num; /* get an easy to use copy of ICL's number */
    workerDescriptionEntry *wdeptr;
    enum serverWorkerShutdownStates swShutdownState;
    int no_assigned_SW_at_shutdown=BOOL_FALSE; /* assume there are assigned Server Workers */
    char digits[ITOA_BUFFERSIZE];
    char ICLdigits[ITOA_BUFFERSIZE];
    int SW_Client_Socket[TEMPNUMWDE];
    int SW_Client_comapi_bdi[TEMPNUMWDE];
    int SW_index;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Indicate that this ICL will handle the ServerWorker and CH shutdown, if needed. */
    if (sgd.ICL_handling_SW_CH_shutdown == NO_ICL_IS_CURRENTLY_HANDLING_SW_CH_SHUTDOWN){
       sgd.ICL_handling_SW_CH_shutdown = ICL_num; /* Indicate this ICL num bill handle it. */
    }
    /*
     * Turn off the UASFM activity if its active. Since the ICL controls the
     * JDBC Server shutdown process, this is the correct place to inform the
     * UASFM to shutdown.  This is done for any type of ICL shutdown indication.
     * Any JDBC Clients that are attempting to connect will complete their
     * connection process with the current UASFM provided information.
     */
    sgd.UASFM_ShutdownState=UASFM_SHUTDOWN; /* indicate the UASFM should shut down, ASAP. */

    /*
     * Test the ICL shutdown point.  Based on what has been done so far, there
     * are one or more steps that need to be done to clean up. They will be tested
     * in order so they can share common handling where possible. The Case entries
     * are set up so you can flow from one case to the next.  After the switch
     * statement, we simply close down the ICL.
     */
    switch(ICLShutdownPoint)
    {
        case FOUND_NO_FREE_SW: {
            /* Got a new JDBC Client, but it was assigned to a new
             * Server Worker since none were free.  Don't create a
             * new Server Worker for it and assign it this JDBC Client.
             * Disconnect with the JDBC Client right now by
             * TCP_CLOSE'ing the Socket connection now.
             *
             * Then flow thru the AFTER_COMAPI_ACCEPT_TIMEOUT case
             * into the AFTER_ASSIGNING_SW case where we handle all
             * the other Server Workers, both those free and those
             * assigned to JDBC Clients.
             *
             * Clear status before calling COMAPI - makes checking easier.
             */
            error_status = -1;
            error_status=Close_Socket(client_socket);
            if (error_status !=0){
               /*
                * COMAPI error message already logged in the LOG file.
                * Add a log entry indicating that we can't deregister the
                * JDBC Server with COMAPI.
                */
                getLocalizedMessageServer(
                    SERVER_CANT_CLOSE_SOCKET_IN_ICL_SHUTDOWN, itoa(ICL_num, ICLdigits,10), NULL,
                    SERVER_LOGS, msgBuffer);
                        /* 5339 Can't TCP_CLOSE the JDBC CLient socket in ICL({0})
                           function initialConnectionListenerShutDown */
               }
            } /* No break here, flow into the AFTER_ASSIGNING_SW case */

        /*
         * At This point the ICL does not have responsibility for the client socket,
         * the Server Worker does. So we don't need to TCP_CLOSE it, since the SW
         * will do it.
         */
        case BEFORE_INIT_SERVER_SOCKET:
        case INSIDE_INIT_SERVER_SOCKET:
        case BEFORE_AFTER_COMAPI_SELECT:
        case AFTER_COMAPI_ACCEPT_TIMEOUT:
        case AFTER_ASSIGNING_SW:
        case AFTER_COMAPI_ACCEPT_LOOP:{
            /*
             * We have either: a)no new JDBC Client since the TCP_ACCEPT timed out,
             * or 2)its after assigning the last accepted JDBC Client, or 3)its
             * just after the TCP_ACCEPTing loop, or 4)we were just getting ready
             * to re-establish connectivity with COMAPI to get server sockets.
             * We can handle these cases all the same way.
             *
             * So, free the server sockets, and then tell all the Server Workers to
             * shut themselves down.
             *
             * Normally, we will not do have the AFTER_COMAPI_ACCEPT_LOOP case
             * since the ICL will probably have already shut down at one of the
             * other shutdown points. If we do get the AFTER_COMAPI_ACCEPT_LOOP
             * case, we will assume we also have server sockets to close, etc.
             */
            for (i = 0; i < sgd.num_server_sockets; i++){
              /*
               * Check if there is a server socket to close.
               * If so, close it. If not just set error_status
               * to 0 (no socket - no problem).
               */
              if (sgd.server_socket[ICL_num][i] !=0){
                error_status=Close_Socket(sgd.server_socket[ICL_num][i]);
                sgd.server_socket[ICL_num][i]=0; /* mark it closed */
              }else {
                error_status=0;
              }
              if (error_status !=0){
                 /*
                  * COMAPI error message already logged in the LOG file.
                  * Add a log entry indicating that we couldn't deregister
                  * the JDBC Server with COMAPI.
                  */
                  getLocalizedMessageServer(
                        SERVER_CANT_CLOSE_SERVER_SOCKET_IN_ICL,
                        itoa(sgd.server_socket[ICL_num][i], digits,10),
                        itoa(ICL_num, ICLdigits,10), SERVER_LOGS, msgBuffer);
                     /* 5340 Can't TCP_CLOSE the ICL server socket {0} in ICL({1})
                             function initialConnectionListenerShutDown */
                 }
            }

            /*
             * Now, notify the existing Server Workers, both free and those
             * with assigned JDBC Clients, to shutdown. This is done in two steps,
             * in a specific order: assigned SW's first, free SW's second.
             * This insures that no Server Worker can fall through the cracks
             * and get from an assigned state to a free state, i.e. from the
             * assigned chain to the free chain, without detecting and processing
             * the shut down notification.
             */

            /* First, get the appropriate Server Worker shutdown state to use */
            if (sgd.ICLShutdownState[ICL_num] == ICL_SHUTDOWN_GRACEFULLY) {
                /* indicate an graceful shutdown */
                swShutdownState=WORKER_SHUTDOWN_GRACEFULLY;
                }
            else {
                /* indicate an immediate shutdown */
                swShutdownState=WORKER_SHUTDOWN_IMMEDIATELY;
                }

            /*
             * Now, notify the Server Workers that are working on assigned
             * JDBC Clients.  Since we will be holding the assigned Server Worker
             * chain under T/S control, all those Server Workers will be notified
             * before they try to get themselves off the assigned chain and placed
             * on the free Server Worker chain.
             *
             * Do only the minimum work under T/S control to do this:
             * 1) Get T/S cell for the assigned Server Worker chain.
             * 2) Loop over the Server Worker entries on this chain.
             *   2a) Get the WDE pointer to next assigned Server worker under T/S locking.
             *   2b) Set the desired shutdown status in that Server Workers WDE.
             * 3) Clear the T/S cell on the free SW chain.
             * 4) Determine if the Server Worker needs to be notified of
             *    immediate shutdown via a Comapi event, and if so put it on
             *    a list to process after the T/S.
             */
            SW_index = 0; /* indicate no SW's on the list regardless of shutdown type. */
            test_set(&(sgd.firstAssigned_WdeTS));  /* Get control of the chains T/S cell */
            wdeptr=sgd.firstAssigned_wdePtr;
            if (swShutdownState==WORKER_SHUTDOWN_IMMEDIATELY){
            /* Since Shutdown immediately may need Server Worker notification,
             * do these separately (avoid extra IF testing).
             */
            while(wdeptr != NULL){
               /* A Server Worker was found, so notify it of the desired shutdown. */
               wdeptr->serverWorkerShutdownState=swShutdownState;

               /* Add client info to notify list for later PASS_EVENT_To_SW() */
               SW_Client_Socket[SW_index] = wdeptr->client_socket;
               SW_Client_comapi_bdi[SW_index] = wdeptr->comapi_bdi;
               SW_index++;

               /* Move to next entry on chain */
               wdeptr=wdeptr->assigned_wdePtr_next;
               }
            }else {
            /* Not an immediate shutdown, so just handle them */
            while(wdeptr != NULL){
               /* A Server Worker was found, so notify it of the desired shutdown. */
               wdeptr->serverWorkerShutdownState=swShutdownState;

               /* Move to next entry on chain */
               wdeptr=wdeptr->assigned_wdePtr_next;
               }
            }
            /*
             * Set a flag if there were no assigned Server Workers - in this case
             * the ICL must be the activity that will notify the Console Handler
             * to shutdown.
             */
            if (sgd.numberOfAssignedServerWorkers == 0){
                no_assigned_SW_at_shutdown=BOOL_TRUE;
            }

            /* all done processing the assigned SW chain. Unlock the chains T/S cell */
            ts_clear_act(&(sgd.firstAssigned_WdeTS));

            /*
             * Now, walk down the Free Server Worker chain and notify each Server
             * Worker to terminate.  Since they are now asleep, it is most efficent
             * to have them wake up and have them shut themselves down.
             *
             * Do only the minimum work under T/S control to do this:
             * 1) Get T/S cell for the free Server Worker chain.
             * 2) Loop over the Server Worker entries on this chain.
             *   2a) Get the WDE pointer to next free Server worker under T/S locking.
             *   2b) Set the desired shutdown status in that Server Workers WDE.
             *   2c) Wake up the Server Worker, it will handle its own shutdown.
             * 3) Done looping, so clear (set to NULL) the pointer to the first
             *    free Server Worker - they all have been removed from chain. Also,
             *    set the number of free Server Workers on the chain to 0.
             * 4) Clear the T/S cell on the free SW chain.
             */
            test_set(&(sgd.firstFree_WdeTS));  /* Get control of the chains T/S cell */
            wdeptr=sgd.firstFree_wdePtr;
            while(wdeptr != NULL){
               /* A Server Worker was found, so notify it of the desired shutdown. */
               wdeptr->serverWorkerShutdownState=swShutdownState;

               /* Now do a T/S sequence on the Server Workers own T/S cell so
                * it can wake up and shutdown appropriately on its own.
                * (This is the T/S sequence documented in the C Manual to do(!?))
                */
               test_set(&(wdeptr->serverWorkerTS));
               ts_clear(&(wdeptr->serverWorkerTS));
               ts_wake_act(&(wdeptr->serverWorkerTS));

               /* Move to next entry on chain */
               wdeptr=wdeptr->free_wdePtr_next;
               }
            /* Clear the free SW chain header and count */
            sgd.firstFree_wdePtr=NULL;
            sgd.numberOfFreeServerWorkers=0;

            /* all done processing the free SW chain. Unlock the chains T/S cell */
            ts_clear_act(&(sgd.firstFree_WdeTS));

            /* Now check if it was an immediate Server Worker shutdown. In that
             * case, there may be Server Workers inside COMAPI waiting for new
             * request packets. Just in case, notify them via a COMAPI passed
             * event to wake up and process the shutdown request. If they are
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
            break;
            }

        default:{
            /*
             * Bad ICLShutdownPoint value. Signal it with a Log
             * message. Indicate the ICL state as downed due to an error
             * and shut the whole JDBC Server down.
             */
            sgd.ICLState[ICL_num]=ICL_DOWNED_DUE_TO_ERROR;

            getLocalizedMessageServer(SERVER_BAD_SHUTDOWN_POINT_IN_ICL_SHUTDOWN,
                itoa(ICL_num, ICLdigits,10), NULL, SERVER_LOGS, msgBuffer);
                    /* 5341 Bad shutDownPoint value encountered in ICL
                       ({0})function initialConnectionListenerShutDown. */

            getLocalizedMessageServer(SERVER_SHUTDOWN_ICL_AND_SERVER,
                itoa(ICL_num, ICLdigits,10), NULL, SERVER_LOGS, msgBuffer);
                    /* 5342 Initial Connection Listener({0}) and JDBC Server
                       will still be shut down */

            /* tsk_stopall(); */
            /* stop all the remaining JDBC Server tasks. */

            /* Deregister COMAPI from the application,
               and stop all active tasks. */
            deregComapiAndStopTasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);
            }
    }

    /*
     * Now we can just terminate this ICL activity - no more COMAPI usage by the
     * ICL. Add a log entry, and inform the main JDBC Server activity that the
     * ICL activity is shutting down.
     * Before actual shutdown, determine if the ICL has to notify the Console
     * Handler about the Server Shutdown. This is necessary since there were no
     * active Server Workers. Normally the last active Server Worker would
     * handle this task.
     */
    sgd.ICLState[ICL_num]=ICL_CLOSED;

    getMessageWithoutNumber(SERVER_SHUTDOWN_IN_ICL, itoa(ICL_num, ICLdigits,10), NULL,
        SERVER_LOGS_DEBUG);
            /* 5343 Shutting down the JDBC Server ICL({0}) activity in function
               initialConnectionListenerShutDown */
    /*
     * Determine if this ICL needs to handle the Console Handler
     * notification.  If not, then we can just exit this activity.
     */
    if (sgd.ICL_handling_SW_CH_shutdown != ICL_num){
        /* This ICL is not handling the CH shutdown, so just exit. */
        exit(0); /* Just shut down this ICL. */
    }

    /* Immediately before stopping this ICL pseudo Server Worker task, test if we
     * are to notify the Console Handler that we are the Server Worker
     * to complete at shutdown time.
     */
    if (no_assigned_SW_at_shutdown == BOOL_TRUE){
        /*
         * Test if the Console Handler is active (running). If it is
         * then notify the Console Handler that it can complete
         * shutting down the JDBC Server. This is done by setting the
         * CH shutdown state and de-registering its Keyin$ handling.
         * If the Console Handler is already in its CH_DOWNED state,
         * then notification is not needed (its already been done).
         */
        if (sgd.consoleHandlerState == CH_RUNNING){
            /* Notify Console Handler to shutdown */
            error_status=deregisterKeyin();
            if (error_status !=0){
                /* Bad deregister status, log error */
                getLocalizedMessageServer(SERVER_BAD_DEREGISTER_KEYIN_IN_ICL,
                    itoa(ICL_num, ICLdigits,10), NULL, SERVER_LOGS, msgBuffer);
                        /* 5344 Bad deregisterKeyin() status encountered in
                           ICL ({0}) function initialConnectionListenerShutDown */
            }
            else {
                /* Add log entry indicating that the ICL initiated the CH shutdown */
                getMessageWithoutNumber(SERVER_ICL_STARTED_CH_SHUTDOWN,
                    itoa(ICL_num, ICLdigits,10),
                    NULL, SERVER_LOGS_DEBUG);
                        /* 5345 Initial Connection Listener({0}) initiated
                           Console Handler shutdown. */
            }
            sgd.consoleHandlerShutdownState=CH_SHUTDOWN;
        }
     }
     exit(0); /* Now exit just this ICL activity normally. */
}


/*
 * Procedure initialConnectionListener
 *
 * This is the JDBC Server's Initial Connection Listener (ICL) main activity.
 *
 * This procedure has been enhanced to allow multiple ICL activities to exist. Upon
 * invocation of this procedure, the ICL activity will know its ICL number, the
 * COMAPI mode, COMAPI bdi it is to use, and COMAPI IPv6 support flag.
 *
 * After some initialization, this activity will register with COMAPI and then
 * wait for an initial JDBC Client network message ( an initial request packet )
 * to be received by COMAPI on the JDBC Server's network server port (as defined
 * in the JDBC Server's configuration).
 *
 * When COMAPI receives a message, it will waken the ICL and pass it the COMAPI
 * Session Table ID for the received message. Then the ICL will locate a free
 * (unassigned) Server Worker and assign it the COMAPI Session Table ID to
 * process, wherein the Server Worker will continue the COMAPI network dialogue
 * with the JDBC Client via a COMAPI assigned client port.
 *
 * The free Server Worker assigned the COMAPI Session Table ID is taken from the
 * free Server Worker chain. If no free Server Workers are available, a new
 * Server Worker activity is started and it is assigned the COMAPI Session
 * Table ID.
 *
 * After assigning the Server Worker, the ICL will loop back and call COMAPI
 * waiting for the next JDBC Client.  This continues until JDBC Server shutdown,
 * at which time the ICL activity will terminate.
 *
 */
void initialConnectionListener(int ICL_num, int ICL_comapi_bdi, int ICL_comapi_IPv6){

    workerDescriptionEntry *wdePtr;
    tsq_cell *firstFree_WdeTSptr;
    _task_array SW_task_array = {2,0};
    int network_order=1; /* start off first TCP_SELECT with CPComm's socket*/
    int log_tcp_access_error_status;
    int new_client_socket;
    /* Create an IPv6 union to reference the family and zone fields as a full word (word1). */
    union {
        int word1;
        v4v6addr_type full_struc;
    } new_client_IP_addr;
    int new_client_network_interface;
    int error_status;
    char ICLdigits[ITOA_BUFFERSIZE];
    char digits[ITOA_BUFFERSIZE];
    char digits2[ITOA_BUFFERSIZE];
    char *console_msg;
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* printf("ICL_num= %d, ICL_comapi_bdi= %o\n",ICL_num, ICL_comapi_bdi);  */

    /*
     * BEFORE DOING ANY ICL WORK we must provide the dummy WDE pointer reference for
     * the Initial Connection Handler activity's local WDE entry so we can utilize
     * it for messages in case there is an error in this activity of the JDBC Server.
     *
     * This also allows other procedures operating in the ICL activity to use the
     * ICL_num and COMAPI bdi (e.g, other ICL procedure, or NetworkAPI procedures
     * that they call ).
     *
     * This allows us to immediately utilize the localized message handling code
     * in case there is an error. Also, this will be used in handling Network
     * API errors/messages.
     */
    setupActivityLocalWDE(SERVER_ICL, ICL_comapi_bdi, ICL_comapi_IPv6, ICL_num);
    /* addServerLogEntry(SERVER_STDOUT,"Done setting up act_wdePtr in ICL"); */

    /* Register signals for this ICL activity */
#if USE_SIGNAL_HANDLERS
    regSignals(&signalHandlerLevel1);
#endif

    /* Test if we should log procedure entry */
    if (sgd.debug==BOOL_TRUE){
      strcpy(message,"Entering initialConnectionListener(");
      strcat(message,itoa(ICL_num, ICLdigits, 10));
      strcat(message,").");
      addServerLogEntry(SERVER_STDOUT,message);
    }

    /* printf("Inside ICL, ICL_num is %d, server port is: %i\n"
               , ICL_num, sgd.localHostPortNumber); */

    /*
     * The free Server Worker T/S cell (firstFree_WdeTS) does not move in
     * memory, so for performance reasons lets get a pointer pointing directly
     * to the T/S cell for use in the ICL code.
     */
     firstFree_WdeTSptr=&(sgd.firstFree_WdeTS);

    /*
     * The ICL is fully up and running, has set its state
     * accordingly, and is ready to access COMAPI and get
     * JDBC Clients.
     */
    sgd.ICLState[ICL_num] = ICL_CONNECTING;

    /*
     * Initialize the ICL Server worker count.
     */
     sgd.ICLAssignedServerWorkers[ICL_num]=0;

    /*
     * Now loop, attempting to access COMAPI services so that
     * we can obtain JDBC clients. Each time through this loop
     * indicates when we have lost COMAPI and are trying to
     * re-establish COMAPI access.  There is a wait of some seconds
     * between each re-establishment retry.
     *
     * ( Notice that COMAPI will implicitly TCP_REGISTER the ICL
     * using the shared session and no need to do the
     * deregister/register sequence. )
     *
     * This loop ends when the ICL instance is informed that it
     * must shut down.
     *
     * Set log_tcp_access_error_status so we will log an error
     * message the first time through loop.
     */
    log_tcp_access_error_status=0;
    while( (sgd.ICLState[ICL_num] == ICL_RUNNING) ||
           (sgd.ICLState[ICL_num] == ICL_CONNECTING) ) {

        /* Clear error_status (we may test/print it before its actually set/used */
        error_status = 0;

        /* Test if we are forcing an explicit COMAPI error status, if print some trace info */
        if (sgd.forced_COMAPI_error_status[ICL_num] != 0){
          printf("In initialConnectionListener() just inside main while loop, ICL_num= %d, ICL_comapi_bdi= %o\n",ICL_num, ICL_comapi_bdi);
        }

        /* Before interacting with COMAPI, check if we are supposed to shutdown now */
        testAndCall_ICLShutdown_IfNeeded(BEFORE_INIT_SERVER_SOCKET,0);

        /*
         * Call COMAPI to connect this ICL instance to a COMAPI
         * system and give it a set of server sockets to use.
         *
         * This will implicitly register this program activity
         * with the COMAPI system to have shared sessions.
         */

        /*
         printf("IN ICL(%d) before Initialize_ServerSockets we have server sockets %d, %d, %d, %d error status=%d\n",
                 ICL_num, sgd.server_socket[ICL_num][0], sgd.server_socket[ICL_num][1],sgd.server_socket[ICL_num][2],
                 sgd.server_socket[ICL_num][3], error_status);
         */

        error_status = Initialize_ServerSockets();

/* printf("IN ICL(%d) after Initialize_ServerSockets we have server sockets %o and %o, error status=%d\n",
                  ICL_num, sgd.server_socket[ICL_num][0], sgd.server_socket[ICL_num][1], error_status);   */

        /*
         * Test the returned error status.  If it is 0, we got connected
         * to COMAPI succesfully. If it is non-zero, check if it was
         * different then the error status as we got the last time through
         * the loop. If so, then only log it, and set flag to avoid excess
         * logging of the same error status while we loop.
         */
        if (error_status !=0 ){
            if (error_status != log_tcp_access_error_status){
              /*
               * Add a log entry indicating that we can't connect the JDBC
               *  Server with COMAPI. Include the COMAPI error and aux statuses.
               */
               strcpy(message, itoa(error_status & 0777777, digits, 10));
               strcat(message,",0");
               strcat(message, itoa(error_status >> 18, digits, 8));
               getLocalizedMessageServer(SERVER_NO_ACCESS_COMAPI_BACK_RUN,
                     itoa(ICL_num, ICLdigits,10), message, SERVER_LOGS, msgBuffer);
                        /* 5393 JDBC Server's ICL({0}) unable to access COMAPI, or to get server
                                sockets listening on server_listens_on addresses; COMAPI
                                status = ({1}) */
               log_tcp_access_error_status= error_status; /* stop messages for a while */


               /* If previously running, or a access error (first time), then display message */
               if ( (sgd.ICLcomapi_state[ICL_num] == API_NORMAL) ||
                    (sgd.ICLcomapi_state[ICL_num] == SEACCES) )  {

                  /* JDBC Server disconnected to COMAPI mode {0} */
                  console_msg = getLocalizedMessageServerNoErrorNumber(SERVER_COMAPI_DISCONNECT_MESSAGE,
                                                                       sgd.ICLcomapi_mode[ICL_num],
                                                                       NULL,
                                                                       0, message);
                  sendcns(console_msg, strlen(console_msg));
                  sgd.ICLcomapi_state[ICL_num] = error_status;
               }

               sgd.ICLState[ICL_num]=ICL_CONNECTING;
             }
        }else {
             /*
              * We were able to communicate with the COMAPI,
              * so indicate that at later time we can again
              * log connection failure attempts.
              */
             log_tcp_access_error_status=0;

             if (sgd.ICLcomapi_state[ICL_num] != API_NORMAL) {
                sgd.ICLcomapi_state[ICL_num] = API_NORMAL;
             }

             sgd.ICLState[ICL_num]=ICL_RUNNING;
        }

        /* Test if we got at least one server socket.*/
        if (error_status == 0){
            /*
             * Got one or more server sockets and they are listening.
             *
             * Now place the ICL in a loop waiting for COMAPI to wake it up with a new
             * initial JDBC Client request message. When received, we will have a new
             * client socket.
             *
             * The variable sgd.ICLState[ICL_num] indicates what the state of the
             *  ICL is.  When this value is changed to one of its shutdown states
             * (e.g. ICL_SHUTDOWN_GRACEFULLY), this loop should terminate.
             */
            while (sgd.ICLState[ICL_num] == ICL_RUNNING) {
                /*
                 * Request a new JDBC Client.  We may get a new client (
                 * error_status == 0 ), or we may recieve one of a few
                 * selected COMAPI statuses (e.g., a TCP Event status
                 * indicating a change in operation indicator.)
                 *
                 * First, toggle network_order between 0 and 1.
                 */
                network_order = network_order ? 0 : 1;
                error_status = Get_Client_From_ServerSockets(&new_client_socket,
                                                             &new_client_IP_addr.full_struc,
                                                             &new_client_network_interface,
                                                              network_order);

  /* ******************************************************************************************************* */
        if (error_status != 0 ){
           /*
            * If the error is a COMAPI timeout or a user event,
            * then test if the ICL is to shutdown. If so, then
            * initialConnectionListenerShutDown will handle it.
            */
           if (error_status == SETIMEDOUT || (error_status & 0777777) == SEHAVEEVENT ){
              testAndCall_ICLShutdown_IfNeeded(AFTER_COMAPI_ACCEPT_TIMEOUT,0);
            }

           /*
            * We got an error trying to get a new JDBC Client.
            *
            * Notice: If it was just a timeout error, it would have been handled inside
            * Accept_Client. If we got back, then Accept_Client recognized that the
            * ICLShutdownState[ICL_num] != ICL_ACTIVE, and it was caught above by
            * testAndCall_ICLShutdown_IfNeeded check.
            *
            * For all other cases, let the standard COMAPI recovery
            * handling take care of resetting up to resume get JDBC clients.
            */
            break; /* exit JDBC Client processing loop. */
        }

        /*
         * We have a new JDBC Client socket ready to work with !!
         *
         * First, are we at our maximum Server Worker concurrency
         * limit? (We don't need to T/S this access, since we are
         * only making an immediate check - the ServerWorkers also
         * have a max-worker check built-into them). If so, then
         * we will have to reject working on this client.
         */
        if (sgd.numberOfAssignedServerWorkers >= sgd.maxServerWorkers){
            /* We cannot accept any more clients at this time. Close
             * the client socket (no need to check status since we are
             * not continuing with this client), and continue looking
             * for new clients, hopefully there will be free workers
             * at that time.
             */
            getLocalizedMessage(SERVER_MAX_ACT_REACHED,
                itoa(sgd.maxServerWorkers, digits, 10),
                itoa_IP( new_client_socket, digits2, 10, new_client_IP_addr.full_struc,
                        NULL, NULL), SERVER_LOGS, msgBuffer);
                    /* 5351 Configuration concurrency limit of max_activities ({0}) reached,
                       connection refused for JDBC Client on client socket {1} */
            error_status = Close_Socket(new_client_socket);
            continue; /* Continue loop, looking for new clients. */
        }

        /* printf("In ICL: Accepted new_client_socket: %i\n",new_client_socket); */
        /*
         * This code is for testing use only (not in release). It can be turned
         * on to display the client host name in Print$.
         *
            error_status=Get_Hostname(message, new_client_IP_addr, act_wdePtr);
            printf("*** New client being assigned, Hostname retrieval:  IPaddr= %d, status = %d, len= %d, name= %s \n",
            new_client_IP_addr, error_status, strlen(message), message);
         */

        /* Before assigning the new JDBC client to a Server Worker, we should
         * Bequeath the client socket.
         *
         *  If this fails, it will be logged in the Server log as a
         * COMAPI error. There would be no need to then look
         * for Server Worker's, etc.  This could be considered a
         * fatal condition for the JDBC Server since it must be able to
         * have its Server Workers inherit client sockets. However, it could
         * have occurred due to COMAPI going down, etc.  So, we will simply
         * drop the client socket and continue. If it is not a COMAPI problem,
         * the JDBC Server will not accept new clients and the SA will submit
         * a contact or UCF.
         */
        error_status=Bequeath_Socket(new_client_socket);
        if (error_status !=0){
          /*
           * Couldn't bequeath socket, so stop processing this
           * client. Let the Server Worker handle the lost client
           * socket (it will log error and shut down SW activity).
           * If this is consistent problem, then the JDBC Server
           * will not be able to accept clients and will be shut
           * down by the SA most likely.
           */
          continue; /* just continue looking for new clients*/
        }

        /*
         * So an initial message is received from a new JDBC Client.  Find a
         * free Server Worker to pass the new client socket to ( the COMAPI
         * session table id ). The chain of free (unassigned) Server Workers
         * is checked under test/set control.
         *
         * From that point, the Server Worker will handle all further network
         * communications on its local client subport supported by COMAPI.  If
         * no free Server Worker is found on the free (unassigned) worker chain,
         * then create a new Server Worker to do the job.
         *
         * First, get the WDE pointer to next free Server worker under T/S locking.
         * Do only the minimum work to get the free worker and advance the chain.
         * Then clear the T/S and let any other activities access the T/S cell.
         */
        test_set(firstFree_WdeTSptr);  /* Get control of the free chains T/S cell */
        wdePtr=sgd.firstFree_wdePtr;
        if (wdePtr != NULL){
           /*
            * A free Server Worker was found, so remove the Server Worker
            * from the free Server Worker chain that runs through the wde's.
            * Also decrement by one the count of server workers on the free chain.
            */
           sgd.firstFree_wdePtr=wdePtr->free_wdePtr_next;
           sgd.numberOfFreeServerWorkers--;

           }

        /* all done updating the free chain, so unlock T/S cell */
        ts_clear_act(firstFree_WdeTSptr);

        /*
         * Now we are no longer under T/S control. Now test the WDE pointer we
         * got. Does it point to a free Server Worker, or was there none
         * available (wdePtr == NULL)? If there was none free, we need to start
         * a new Server Worker to accept the initial client message.
         */
        if (wdePtr != NULL){
            /* We got a free Server Worker.  Assign it the new JDBC Client
             * and then wake it up so it can handle the client independently.
             *
             * First, set its WDE entry state information to assign it the
             * new client socket.
             */

            if (sgd.debug) {
                sprintf(message,"Free Server Worker %012o being assigned to new client %i",
                    wdePtr->serverWorkerActivityID, new_client_socket);
                addServerLogEntry(TO_SERVER_TRACEFILE,message);
            }

            assignServerWorkerToClient(wdePtr, new_client_socket, ICL_comapi_bdi, ICL_comapi_IPv6,
                                       ICL_num, new_client_IP_addr.full_struc, new_client_network_interface);

            /* Now do a T/S sequence on the Server Workers own T/S cell so we can
             * wake it up and have it start on the new JDBC Client.
             *
             * The test/set sequence used will insure that the Server Worker has
             * put itself to sleep before we attempt to wake it.  This avoids the
             * gap between when the server placed itself on the free Server worker
             * chain and when it put itself to sleep. When we have the test/set
             * cell controlled, the server worker is asleep and ready to be
             * awakened, which we will do.
             */
            test_set(&(wdePtr->serverWorkerTS));  /* wait until server worker is asleep */
            ts_clear(&(wdePtr->serverWorkerTS)); /* free T/S, server worker is still asleep */
            ts_wake_act(&(wdePtr->serverWorkerTS)); /* wake the Server Worker and let it start work */

           /*
            * We have completed assigning a Server Worker to the JDBC Client.
            * Test if ICL Shutdown indicators have been set, if so then
            * we must shutdown the ICL right now before we accept any more JDBC
            * Clients. If not shutting down, just keep going.
            *
            * (Remember, testAndCall_ICLShutdown_IfNeeded calls
            * initialConnectionListenerShutDown if needed, and it does not
            * return to this code).
            */
           testAndCall_ICLShutdown_IfNeeded(AFTER_ASSIGNING_SW,0);
            }
        else {
           /*
            * There was no free Server Worker available at the time.
            * Test if ICL Shutdown indicators have been set, if so then
            * we must shutdown the ICL right now before we accept this JDBC
            * Clients. If not shutting down, just keep going.
            *
            * (Remember, testAndCall_ICLShutdown_IfNeeded calls
            * initialConnectionListenerShutDown if needed, and it does not
            * return to this code).
            */
           testAndCall_ICLShutdown_IfNeeded(FOUND_NO_FREE_SW,new_client_socket);

           /*
            * We need to create a new Server Worker activity and assign it the JDBC client.
            *
            * Pass it a pointer to the JDBC Server's Global Data so it can add
            * itself to the known pool of server workers. Also pass it the COMAPI
            * new client socket so it can utilize the new client connection.
            *
            * We assume that the tskstart will be successful - The URTS runtime system
            * will generate a contingency if the tskstart fails and the JDBC Server
            * would then shutdown abnormally.
            */
 /*   printf("We need to start a new Server Worker for new client socket %i\n",new_client_socket); */

           /*
            * The new_client_IP_addr parameter is a 5 word activity local structure that must be passed
            * to the new serverWorker activity.  Because activity local addresses cannot be passed as
            * parameters to another activity, the 5 word structure is passed by value as 3 separate
            * parameters (1 int and 2 long long values).
            */
           tskstart(SW_task_array,serverWorker,new_client_socket,ICL_comapi_bdi, ICL_comapi_IPv6,
                    ICL_num, new_client_IP_addr.word1, new_client_IP_addr.full_struc.v4v6.v6_dw[0],
                    new_client_IP_addr.full_struc.v4v6.v6_dw[1], new_client_network_interface);
            }

            /*
             * We are done processing that initial JDBC Client network message.
             * So loop back and wait on COMAPI for the next initial JDBC Client
             * message.
             */


  /* ******************************************************************************************************* */
             } /* end client processing while */

             /*
              * JDBC Client processing While loop
              * terminated - we are no longer accepting
              * JDBC clients on the server sockets.
              *
              * Since the ICLState[ICL_num] is ICL_RUNNING
              * until this ICL shuts down (which is done
              * in the ICL shutdown code above), the reason
              * we are here because either Initialize_ServerSockets()
              * returned a summary error status that indicated
              * no server sockets were obtained, or the break statement
              * was executed after the  Get_Client_From_ServerSockets()
              * procedure call (we received a COMAPI
              * error_status that was not acceptable and
              * recovered by Get_Client_From_ServerSockets(),
              * like it does with the network down cases.
              *
              * In this case, recovery will involve a complete
              * re-establishment with the COMAPI system. So shutdown
              * the server sockets now. We will re-request them later
              * on as part of re-establishing connectivity with a
              * working COMAPI system.
              */
             Terminate_Server_Sockets();

        } /* end if */

        /*
         * We are no longer looping looking for JDBC Clients. This
         * occurs for these reasons:
         * 1) The ICL is being told to shutdown. This needs to be
         *    detected and the ICL activity shut down.
         * 2) We had and now have lost COMAPI connectivity. In this
         *    case, we will wait some seconds and attempt to reconnect
         *    to COMAPI and resume processing JDBC Clients.
         * 3) We did not get connected with COMAPI at the beginning
         *    and the code has fallen through to here due to the
         *    error_status being non-zero. Wait and try to reconnect
         *    again.
         *
         * We first check if the ICL Shutdown indicators have been
         * set to a shutdown state, since we won't need COMAPI and
         * the some second wait. If so then we must shutdown the ICL
         * right now.
         *
         * (Remember, testAndCall_ICLShutdown_IfNeeded calls
         * initialConnectionListenerShutDown if needed, and it does not
         * return to this code).
         */
        testAndCall_ICLShutdown_IfNeeded(BEFORE_INIT_SERVER_SOCKET,0);

        /*
         * ICL is still running, but we don't have COMAPI access
         * any longer. Wait some seconds and try to resume COMAPI
         * and accepting of new JDBC Clients.
         */
        twait(sgd.comapi_reconnect_wait_time[ICL_num]);  /* wait some seconds */

    } /* end outer while */

    /*
     * ICLState is no longer ICL_RUNNING, so this ICL's work is completed.
     * At this time, this is true (i.e. ICL's work is done) regardless of the
     * new ICLState value. Shutdown the server sockets and then shutdown the
     * ICL activity.
     */
    initialConnectionListenerShutDown(AFTER_COMAPI_ACCEPT_LOOP, 0);

    /* Shouldn't get here, so shutdown the ICL after logging an error message */
    sgd.ICLState[ICL_num]=ICL_DOWNED_DUE_TO_ERROR;

    getLocalizedMessageServer(SERVER_CANT_SHUTDOWN_ICL,
        itoa(ICL_num, ICLdigits,10), NULL, SERVER_LOGS, msgBuffer);
        /* 5350 Could not shut down Initial Connection Listener({0)} properly */

    getLocalizedMessageServer(SERVER_SHUTTING_DOWN, sgd.serverName, NULL,
        SERVER_LOGS, msgBuffer);
            /* 5421 JDBC Server "{0}" shut down */

    /* Deregister COMAPI from the application,
       and stop all active tasks. */
    deregComapiAndStopTasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);

}

/*
 * Function Get_Client_From_ServerSockets
 *
 * This function attempts to obtain a new JDBC Client connection
 * from one of the Server Sockets associated to the COMAPI mode
 * associated to this ICL instance.
 *
 * If an new JDBC client is available, then the new client socket,
 * their remote address and a good status (0) will be returned.
 *
 * If no new JDBC clients are available, this routine will attempt
 * to re-connect to any network interface that it is currently
 * not connected to for its COMAPI.
 *
 * If any errors occur, an error status is returned to the caller
 * for their processing.
 */
int Get_Client_From_ServerSockets(int * new_client_socket_ptr, v4v6addr_type * remote_ip_addr_ptr,
                                  int * network_interface_ptr, int network_order){

   int error_status;
   int a_server_socket;
   int socket_list[MAX_SERVER_SOCKETS + 1]; /* The first entry has a count */
   int i;
   int ICL_num=act_wdePtr->ICL_num; /* get an easy to use copy of ICL's number */
   int dummy_server_socket=0; /* clearly indicate we do not yet have a socket here. */
   char digits[ITOA_BUFFERSIZE];
   char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
   char *console_msg;

   /* Test if we are forcing an explicit COMAPI error status, if print some trace info */
   if (sgd.forced_COMAPI_error_status[ICL_num] != 0){
     printf("In Get_Client_From_ServerSockets(), ICL_num=%d\n",ICL_num);
   }

   /*
    * While error status indicates no problem, loop
    * looking on any of the Server sockets for a
    * JDBC client requesting a client socket (i.e.,
    * a new JDBC Client to process).
    */
   error_status=0; /* Initially there is no problem */
   while (error_status == 0){
      /* Test if we are to shutdown. If so, we have not gotten a
       * new JDBCClient at this time.
       */
      testAndCall_ICLShutdown_IfNeeded(BEFORE_AFTER_COMAPI_SELECT,0);

      /* Form a list of active Server Sockets for this ICL. */
      socket_list[0] = 0; /* initialize to an empty list, none on list */

     /*
      * Using the network_order value, place the sockets on the socket_list
      * in an alternating order to balance their priority. This prevents
      * favoring one socket and its network over the others in case one
      * network gets a large number of clients in a short time.
      */
      if (network_order == 0){
         /* Favor CPComm first*/
         for (i = 0; i < (sgd.num_server_sockets); i++){
            /* Add the current Server Sockets to the list */
            if (sgd.server_socket[ICL_num][i] != 0)
            {
               socket_list[++socket_list[0]] = sgd.server_socket[ICL_num][i];
            }
         }
      }else {
         /* Favor CMS first */
         for (i = (sgd.num_server_sockets-1); i >= 0; i--){
            /* Add the current Server Sockets to the list */
            if (sgd.server_socket[ICL_num][i] != 0)
            {
               socket_list[++socket_list[0]] = sgd.server_socket[ICL_num][i];
            }
         }
      }

      /*
       * Do a TCP_Select to see if there are any JDBC Client's waiting for
       * service on any of the Server Sockets . After the TCP_Select, also
       * check if its a good time to check/re-connect to a network interface.
       */
      error_status = Select_Socket(socket_list);

      /* If no new clients or we timed out, then check the sockets and networks. */
      if ((error_status == 0 && socket_list[0] == 0) || error_status == SETIMEDOUT)
      {
        /*
         * There were no new clients waiting for service, or we timed out waiting.
         *
         * First check if we are to shut down. Its a good spot to check.
         */
        testAndCall_ICLShutdown_IfNeeded(BEFORE_AFTER_COMAPI_SELECT,0);

        /*
         * Since we have no new Clients, use this opportunity to re-connect
         * to CPcomm or CMS for up to four server sockets if we don't have a
         * Server Socket to one of those networks already. Use the ICL's
         * sgd.num_server_sockets[ICL_num] value to determine how many server
         * sockets we are supposed to have (i.e., don't try to re-connect a
         * server socket if its not supposed to be used).
         */
         for (i = 0; i < sgd.num_server_sockets; i++)
         {
            /* Do we have a Server socket to this network interface? */
            if (sgd.server_socket[ICL_num][i] == 0)
            {
               /*
                * We do not have a Server socket to this network interface,
                * attempt to get one and listen on it. Use the listen IP addresses
                * that is associated to this ICL instance.
                *
                * We have already attempted to use the explicit IP address to help
                * set up the listen ip addresses for this server socket (i.e.
                * looking up the host name IP address).
                *
                * Test explicit_ip_address[ICL_num][i].family, if it is still
                * EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP (-1) then do not attempt
                * to re-connect this server socket because the earlier host name
                * look up done in Initialize_ServerSockets() failed to get an IP
                * address for the host name.  (This is signaled by setting error_status
                * equal to -1).  Notice that error_status is only used in a  0 or non-0
                * manner, its value is not passed back or used to create/define error messages.
                *
                * Note: If the customer decides to change the IP address of a host
                * name after the initial DNS lookup then the JDBC Server will have
                * to be rebooted.  (Making this dynamic would require a code
                * enhancement(s) - something for a future level.)
                *
                */
                error_status = 0; /* assume we have a host IP address. */
                if (sgd.explicit_ip_address[ICL_num][i].family == EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP){
                   /* host name resolution had failed, so set error_status accordingly. */
                   error_status = -1;
                }

               /*
                * If we didn't get an error on the host name look up, if we needed to
                * do that, then we can try to get the server socket.  If an error did
                * occur, the normal error status checking will reject the attempt to get
                * a server socket.
                */
               if (error_status == 0){
                  error_status = Init_COMAPI(&a_server_socket, comapi_interface_list[i],
                                             sgd.listen_ip_addresses[ICL_num][i], sgd.localHostPortNumber);
               }
               if (error_status == 0){
                  /*
                   * We did not get an error so we got a
                   * server socket. Save it in the sgd.
                   */
                  sgd.server_socket[ICL_num][i] = a_server_socket;

                  /*
                   * Now output a message into the JDBC$LOG file that
                   * indicates we have a new server socket.
                   */

                  sprintf(message,"(%d)=%d using %s to listen on %s",
                          i, a_server_socket,
                          (comapi_interface_list[i] == CPCOMM) ? "CPCOMM" : "CMS",
                          sgd.listen_host_names[i]);

                  /* 5412 ICL[{0}] server_socket{1} */
                  getMessageWithoutNumber(SERVER_SOCKET_NOW_LISTENING,
                                          itoa(ICL_num, digits, 10),message,
                                          SERVER_LOGS_DEBUG);

                  /* JDMS Server connected to COMAPI mode {0} using {1} */
                  console_msg = getLocalizedMessageServerNoErrorNumber(SERVER_COMAPI_CONNECT_MESSAGE,
                                                                       sgd.ICLcomapi_mode[ICL_num],
                                                                       ((comapi_interface_list[i] == CPCOMM) ? "CPCOMM" : "CMS"),
                                                                        0, message);
                  sendcns(console_msg, strlen(console_msg));
                }
            }
         } /* end of for loop */

         /*
          * Clear the error_status here, since any non-zero error_status
          * would be left over from the network re-connecting attempt above
          * and not part of the While loop looking for new JDBC clients.
          */
         error_status = 0;
         continue;  /* Continue the While loop (look for new JDBC Clients). */

      } /* end of if */

      /*
       * Display the error status that was returned by Select_Sockets. Test if we are
       * in DEBUG mode (i.e, the status of the TCP_SELECT that was done). The
       * status could either be good (0 - we got a server socket selected) or a bad
       * status (!=0  - error on the server sockets).
       */
#if DEBUG
      printf("In ICL(%d), Select_Socket returned status, aux status %d, %d with list %d, %012o, %012o\n",
                         ICL_num, error_status >> 18, error_status & 0777777, socket_list[0], socket_list[1],
                         socket_list[2]);
#endif
      /*
       * If we received an error status on the Select_Socket() call (i.e, the TCP_SELECT)
       * then we need to stop the JDBC Client accepting while loop.
       */
      if (error_status != 0)
      {
         break; /* Stop the While loop. We have an error to deal with. */
      }

      /*
       * The error status was 0, so we have something to accept on one
       * of the server sockets on the TCP_SELECT list.  So do a TCP_ACCEPT
       * on that Server Socket to accept the new JDBC Client.  Before
       * the TCP_ACCEPT, test if we are to shutdown. If so, we have not gotten a
       * new JDBC Client at this time.
       */
      testAndCall_ICLShutdown_IfNeeded(BEFORE_AFTER_COMAPI_SELECT,0);
      error_status = Accept_Client(socket_list[1], new_client_socket_ptr,
                                   remote_ip_addr_ptr, network_interface_ptr);

      /* Test the returned status in case there was a problem. */
      if (error_status == 0){
        /* Stop the While loop. We have a new JDBC Client. */
        break;
      }
      else {
         /*
          * We got an error attempting to accept the JDBC Client socket.
          * First check if it is an event status or a timeout status. If so,
          * there was no JDBC Client accepted. Either we changed a sgd
          * configuration parameter or we are being informed to shutdown, or
          * we timed out trying to accept something.  In either case, let
          * the while loop continue, so it can either re-check the server
          * sockets for new clients or handle the shutdown flag.
          *
          */
         if (error_status == SEHAVEEVENT || error_status == SETIMEDOUT){
            /* Allow the While loop to continue looking for a new JDBC Client */
            error_status = 0;
            continue;
         }

         /*
          * We have some other error status value than SEHAVEEVENT or SETIMEDOUT.
          * This probably means we have lost the Server Sockets connectivity
          * to the network interface. Terminate this Server Socket right
          * now.  The Server Socket will be re-established as part of the
          * next iteration of the While loop.
          *
          * Loop over the number of Server Sockets we actually should have to find
          * the one used for the TCP_Accept.
          */
         for (i = 0; i < sgd.num_server_sockets; i++)
         {
            /* Terminate the affected Server socket */
            if (sgd.server_socket[ICL_num][i] == socket_list[1])
            {
               /*
                * Terminate the Server socket for this network
                * connection (Rescind and close).
                */
               Rescind_Close_Socket(socket_list[1]);
               sgd.server_socket[ICL_num][i] = 0;
               break;
            }
         }

         /*
          * Allow the While loop to continue so we
          * can hopefully get a new JDBC client and
          * get a new server socket as well.
          */
         error_status = 0;
      }

   } /* end of while loop */

   /*
    * Return the error status that stopped the While loop.
    *
    * If we got a new JDBC Client, the error status will
    * be 0 and the client socket session id will have
    * been returned via new_client_socket_ptr. So go process
    * the  new JDBC client to process.
    *
    * If we had an error with the Select_Socket() call,
    * then the error status is not 0.  Let the caller
    * decide what to do.
    */
   return error_status;
}

/*
 * Function Init_COMAPI
 *
 * This function attempts to obtain a server socket on the
 * the desired network at the designated port number.  If
 * successful, the server socket will have the desired set
 * of COMAPI options set.  At the end, a TCP_LISTEN is started
 * on the designated server socket.
 *
 * If any errors occur, an error status is returned to the caller
 * for their processing and the server socket is closed.
 *
 * Normal execution will return a status value of 0.
*/
int Init_COMAPI(int * new_server_socket, int interface, v4v6addr_type listen_ip_address, int port){

    int ICL_num=act_wdePtr->ICL_num; /* get an easy to use copy of ICL's number */
    int close_error_status;
    int error_status = 0; /* Clear error_status (we may test/print it before its actually set/used */
    int COMAPI_option_name;
    int COMAPI_option_value;
    int COMAPI_option_length;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /*
     printf("In Init_COMAPI at i1: error_status=%d, *new_server_socket=%d, interface=%d, listen_ip_address=%d, port=%d\n",
       error_status, *new_server_socket, interface, listen_ip_address, port);
     */

    /* Test if we are forcing an explicit COMAPI error status, if print some trace info */
    if (sgd.forced_COMAPI_error_status[ICL_num] != 0){
      printf("In Init_COMAPI(), ICL_num=%d\n",ICL_num);
    }

    /*
     * Now get a server socket from COMAPI for this ICL to use.
     */
    error_status=Start_Socket(new_server_socket);


/* printf("In Init_COMAPI at i1: error_status=%d, *new_server_socket=%d, interface=%d, listen_ip_address=%d, port=%d\n",
 error_status, *new_server_socket, interface, listen_ip_address, port); */


    if (error_status !=0){
        /*
         * If a COMAPI error message is supposed to be
         * logged in the LOG file, its already been done.
         */
        *new_server_socket=0; /* No socket to return. */
        return error_status;
    }

    /*
     * Now turn on the COMAPI debug option on the server socket if sgd.COMAPIDebug is set.
     * Remember that COMAPI debugging can be turned on/off via a console command.
     */
   if (sgd.COMAPIDebug != SGD_COMAPI_DEBUG_MODE_OFF){
    COMAPI_option_name=COMAPI_DEBUG_OPT;
    COMAPI_option_value=sgd.COMAPIDebug;
    COMAPI_option_length=4;
    error_status=Set_Socket_Options(*new_server_socket
                                      ,&COMAPI_option_name
                                      ,&COMAPI_option_value
                                      ,&COMAPI_option_length );
    /* test if we have an error status - did we do the set option and fail? */
    if (error_status !=0){
        /*
        * COMAPI error message already logged in the LOG file.
        * Add a log entry indicating that we couldn't set the socket
        * options. Then return an error indication to the caller
        * for their handling.
        */
        getLocalizedMessageServer(SERVER_CANT_SET_COMAPI_DEBUG_OPTION,
            itoa(COMAPI_option_value, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5319 Can't set the debug option on server socket,
                   debug = {0} */
        /*
         * Close the server socket - we don't need to
         * return the close_error_status since the
         * error_status value is what causes this closure.
         */
        close_error_status=Close_Socket(*new_server_socket);
        *new_server_socket=0; /* No socket to return. */
        return error_status;
        }
    }

    /*
     * Now set up the linger and blocking COMAPI options on the server socket.
     * For now this is done with two separate COMAPI calls. It will later
     * be replaced by using the multi-option approach.
     */
    COMAPI_option_name=COMAPI_LINGER_OPT;
    COMAPI_option_value=0;
    COMAPI_option_length=4;
    error_status=Set_Socket_Options(*new_server_socket
                                    ,&COMAPI_option_name
                                    ,&COMAPI_option_value
                                    ,&COMAPI_option_length );
    if (error_status !=0){
        /*
        * COMAPI error message already logged in the LOG file.
        * Add a log entry indicating that we couldn't set the socket
        * options. Then return an error indication to the caller
        * for their handling.
        */
        getLocalizedMessageServer(SERVER_CANT_SET_LINGER_OPTION,
            itoa(COMAPI_option_value, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5320 Can't set the linger option on server socket,
                   linger = {0} */
        /*
         * Close the server socket - we don't need to
         * return the close_error_status since the
         * error_status value is what causes this closure.
         */
        close_error_status=Close_Socket(*new_server_socket);
        *new_server_socket=0; /* No socket to return. */
        return error_status;
        }

    COMAPI_option_name=COMAPI_BLOCKING_OPT;
    COMAPI_option_value=2;
    COMAPI_option_length=4;
    error_status=Set_Socket_Options(*new_server_socket
                                    ,&COMAPI_option_name
                                    ,&COMAPI_option_value
                                    ,&COMAPI_option_length );
    if (error_status !=0){
        /*
        * COMAPI error message already logged in the LOG file.
        * Add a log entry indicating that we couldn't set the socket
        * options. Then return an error indication to the caller
        * for their handling.
        */
        getLocalizedMessageServer(SERVER_CANT_SET_BLOCKING_OPTION,
            itoa(COMAPI_option_value, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5321 Can't set the blocking option on server socket,
                   blocking value = {0} */
        /*
         * Close the server socket - we don't need to
         * return the close_error_status since the
         * error_status value is what causes this closure.
         */
        close_error_status=Close_Socket(*new_server_socket);
        *new_server_socket=0; /* No socket to return. */
        return error_status;
        }

    /*
     * Now set up the receive and send timeout COMAPI options on the server socket.
     * For now this is done with two separate COMAPI calls. It will later
     * be replaced by using the multi-option approach.
     */

    COMAPI_option_name=COMAPI_RECEIVE_OPT;
    COMAPI_option_value=sgd.server_receive_timeout;
    COMAPI_option_length=4;
    error_status=Set_Socket_Options(*new_server_socket
                                    ,&COMAPI_option_name
                                    ,&COMAPI_option_value
                                    ,&COMAPI_option_length );
    if (error_status !=0){
        /*
        * COMAPI error message already logged in the LOG file.
        * Add a log entry indicating that we couldn't set the socket
        * options. Then return an error indication to the caller
        * for their handling.
        */
        getLocalizedMessageServer(SERVER_CANT_SET_RECEIVE_TIMEOUT,
            itoa(COMAPI_option_value, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5322 Can't set the receive timeout option on server socket,
                   receive timeout = {0} */
        /*
         * Close the server socket - we don't need to
         * return the close_error_status since the
         * error_status value is what causes this closure.
         */
        close_error_status=Close_Socket(*new_server_socket);
        *new_server_socket=0; /* No socket to return. */
        return error_status;
        }

    COMAPI_option_name=COMAPI_SEND_OPT;
    COMAPI_option_value=sgd.server_send_timeout;
    COMAPI_option_length=4;
    error_status=Set_Socket_Options(*new_server_socket
                                    ,&COMAPI_option_name
                                    ,&COMAPI_option_value
                                    ,&COMAPI_option_length );
    if (error_status !=0){
        /*
        * COMAPI error message already logged in the LOG file.
        * Add a log entry indicating that we couldn't set the socket
        * options. Then return an error indication to the caller
        * for their handling.
        */
        getLocalizedMessageServer(SERVER_CANT_SET_SEND_TIMEOUT,
            itoa(COMAPI_option_value, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5323 Can't set the send timeout option on server socket,
                   send timeout = {0} */
        /*
         * Close the server socket - we don't need to
         * return the close_error_status since the
         * error_status value is what causes this closure.
         */
        close_error_status=Close_Socket(*new_server_socket);
        *new_server_socket=0; /* No socket to return. */
        return error_status;
        }

    COMAPI_option_name=COMAPI_INTERFACE_OPT;
    COMAPI_option_value=interface;           /* use the procedure parameter value */
    COMAPI_option_length=4;
    error_status=Set_Socket_Options(*new_server_socket
                                    ,&COMAPI_option_name
                                    ,&COMAPI_option_value
                                    ,&COMAPI_option_length );
    if (error_status !=0){
        /*
        * COMAPI error message already logged in the LOG file.
        * Add a log entry indicating that we couldn't set the socket
        * options. Then return an error indication to the caller
        * for their handling.
        */
        getLocalizedMessageServer(SERVER_CANT_SET_SEND_TIMEOUT,
            itoa(COMAPI_option_value, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5323 Can't set the interface option on server socket,
                   interface = {0} */
        /*
         * Close the server socket - we don't need to
         * return the close_error_status since the
         * error_status value is what causes this closure.
         */
        close_error_status=Close_Socket(*new_server_socket);
        *new_server_socket=0; /* No socket to return. */
        return error_status;
        }

    /* If COMAPI supports IPv6, turn on the option to use either IPv4 or IPv6 IP addresses. */
    if (act_wdePtr->comapi_IPv6 == 1) {
        COMAPI_option_name = COMAPI_IP_IPV4V6;
        COMAPI_option_value = 1;           /* use either IPv4 or IPv6 addresses */
        COMAPI_option_length = 4;
        error_status = Set_Socket_Options(*new_server_socket,
                                          &COMAPI_option_name,
                                          &COMAPI_option_value,
                                          &COMAPI_option_length);
        if (error_status != 0) {
            act_wdePtr->comapi_IPv6 = 0;    /* turn off IPv6 to prevent continuous logging */
            /*
            * COMAPI error message already logged in the LOG file.
            * Add a log entry indicating that we couldn't set the socket
            * options. Default will be IPv4.
            */
            getLocalizedMessageServer(SERVER_CANT_SET_IPV4V6_OPTION,
                  NULL, NULL, SERVER_LOGS, msgBuffer);
                  /* 5434 Can't set the IPv4v6 option on server socket, default to IPv4 */
        }
    }

   /* Do a Listen on the server socket. Return the status for caller to process. */
   error_status = Listen_Port(*new_server_socket, sgd.maxCOMAPIQueueSize, interface,
                      listen_ip_address, port);
/*    printf("In Init_COMAPI at i2: error_status=%d, *new_server=%d, interface=%d, listen_ip_address=%d, port=%d\n",
           error_status, *new_server_socket, interface, listen_ip_address, port);  */

   return error_status;
}

/*
 * Function Initialize_ServerSockets
 *
 * This function connects this ICL instance to a COMAPI system. At
 * this point, we have not obtained any server sockets yet.  This
 * code will try to obtain a server socket on each network
 * interface.
 *
 * This function returns a summary error status value to the caller:
 *
 * 1) If at least one network interface is available at the server
 * port requested, this summary error status returned will be 0,
 * indicating that at least one server socket is available, allowing
 * the ICL instance to begin processing JDBC client requestors.
 *
 * 2) If not at least one server socket is available, the summary error
 * status returned will be non-zero ( it will be equal to one of the
 * COMAPI error status values). It is the responsibility of the caller
 * to wait a period of time and to attempt to reconnect with COMAPI and
 * call this procedure to again attempt to obtain at least one server
 * socket.
 *
 *(If all the network interfaces at the desired port are busy, then
 * this function will wait and re-attempt to acquire at least one
 * server socket form those interfaces (COMAPI is still up, so we are
 * just waiting for a port to be available).
 *
 * There are four reasons that a server socket on the desired port
 * cannot be obtained.  Either 1) COMAPI is down, or 2)we cannot get
 * the server socket on the desired network interface because the
 * network interface is down, or 3)both COMAPI and the network interface
 * are up but the server socket on the desired port was already obtained
 * by another ICL instance using another COMAPI mode that is sharing the
 * same network interface, or 4) the IP address that was used was not valid
 * for the particular network interface.
 *
 * It is assumed that the caller has registered (TCP_REGISTER) with
 * COMAPI prior to this function call.
 */
int Initialize_ServerSockets(){

    int ICL_num=act_wdePtr->ICL_num; /* get an easy to use copy of ICL's number */
    int error_status = 0; /* Clear error_status (we may test/print it before its actually set/used */
    int summary_error_status;
    int busy_ports;
    int i;
    int j;
    int a_server_socket;
    int ignore_error_status;
    int dummy_server_socket=0; /* clearly indicate we do not yet have a socket here. */
    char digits[ITOA_BUFFERSIZE];
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char *console_msg;

    /* printf("In Initialize_ServerSockets(), ICL_num=%d\n",ICL_num); */

    /* Test if we are forcing an explicit COMAPI error status, if print some trace info */
    if (sgd.forced_COMAPI_error_status[ICL_num] != 0){
      printf("In Initialize_ServerSockets(), ICL_num=%d\n",ICL_num);
    }

    /*
     * Loop attempting to connect to COMAPI.  Attempt to obtain sgd.num_server_sockets
     * worth of Server Sockets, half on CPComm and the rest on CMS.
     */
    do {
        /* Test if we are to shutdown this ICL. If so, don't mess with COMAPI */
        testAndCall_ICLShutdown_IfNeeded(INSIDE_INIT_SERVER_SOCKET,0);

        /*
         * The summary error status value is basically an or'ing
         * of the Init_COMAPI returned error statuses, indicating if
         * one of the server socket requests succeeded. A 0 value
         * indicating success and a non-zero value (i.e. COMAPI error
         * value) indicating failure. Initially set the summary error
         * status value to a very high value (greater than all the COMAPI
         * error values) as an initial "fail" value, which will later
         * be replaced by the or'ing result from the Init_COMAPI calls.
         *
         * Also set busy_ports to 0 - use later for counting of busy ports.
         */
        summary_error_status = INT_MAX;
        busy_ports = 0;

        /*
         * Now, loop over all the Server sockets we want to obtain based
         * on the number of listen IP addresses that we requested be used,
         * as recorded in the sgd.num_server_sockets value for this ICL.
         *
         * If an explicit IP address is not present, then look up the host
         * name first.  Notice this is done only this one time and in this
         * one place.  If the customer decides to change the IP address of
         * a host name after this DNS lookup then the JDBC Server will have
         * to be rebooted.  (Making this dynamic would require a code
         * enhancement - something for a future level.)
         */
        for (i = 0; i < sgd.num_server_sockets; i++)
        {
           /*
            * Clear error_status here, since we will be testing it
            * before the Init_COMAPI call.  If we have to look up
            * a host name, we will be setting error_status based on
            * that work, but if we don't look up a host error_status
            * would not be cleared before the test preceding the
            * Init_COMAPI call (e.g. 1) an uninitialized error_status
            * would likely be non-zero and Init_COMAPI would not be
            * called and no Server Socket obtained, or 2) the left
            * over value from a previous for loop could be
            * miss-interpreted).
            */
           error_status = 0;

           /* Connect and get a Server socket with the desired Network interface, and listen on it */
           /* First check if we have to look up a host name first. */
           if (sgd.explicit_ip_address[ICL_num][i].family == EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP){

             /*
              * Yes, we must first look up the hosts IP address. To facilitate
              * this step we need a dummy socket since there has to be a socket
              * passed to the Get_Remote_IP_Address() function.  Create it and
              * close it at the end.  This is done to avoid problems between
              * networks.
              *
              * First, obtain the dummy server socket.
              */
             dummy_server_socket = 0; /* we have no socket yet */
             error_status =Start_Socket(&dummy_server_socket);

             /*
              * If we got the dummy socket (error status == 0 ), try to
              * get the host name's IP address and later a server socket.
              */
             if (error_status == 0){
                error_status = Get_Remote_IP_Address(dummy_server_socket,
                                                     sgd.listen_host_names[i],
                                                     &sgd.listen_ip_addresses[ICL_num][i],
                                                     comapi_interface_list[i]);

                /* Close the dummy socket, ignore returned status, not needed. */
                ignore_error_status =Close_Socket(dummy_server_socket);

                /*
                 * If we got a IP address on the host name look up, try to
                 * get a server socket using that IP address.
                 */
                if (error_status == 0){
                  error_status = Init_COMAPI(&a_server_socket, comapi_interface_list[i],
                                      sgd.listen_ip_addresses[ICL_num][i], sgd.localHostPortNumber);
                }

                /*
                 * Test if we got a host name's IP address that got us a listening
                 * server socket, if so we will settle for the host name IP address
                 * we found.
                 * If we couldn't get a TCP_LISTEN on the IP address we found, leave
                 * explicit_ip_address = EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP (-1) so we
                 * will not try again later to re-look up the host name (its bad).
                 *
                 * Note: we will leave the IP address we may have gotten in the listen_ip_addresses
                 * array so that it can be seen with the Console Handler Display Server
                 * Status command even though we cannot use it. (Hopefully we will replace
                 * it on a later try, while the JDBC Server is running, assuming the customer
                 * has the Host name properly (re-)entered into the DNR for the network adapter.)
                 */
                if (error_status == 0){
                   /*
                    * Found a usable IP address on which we got a listening server
                    * socket, so we no longer need to look up host's IP address;
                    * update explicit_ip_address with the hosts IP address (which
                    * also removes the EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP indicator).
                    */
                   sgd.explicit_ip_address[ICL_num][i] = sgd.listen_ip_addresses[ICL_num][i];
                }
             }
           }
           else {
             /*
              * We didn't need to do a host name look up, so we can try to get
              * the server socket if there is no pending non-zero error statuses.
              * If an error occurs, let the normal error handling later reject
              * the attempt to get a server socket.
              */
             if (error_status == 0){
               error_status = Init_COMAPI(&a_server_socket, comapi_interface_list[i],
                                      sgd.listen_ip_addresses[ICL_num][i], sgd.localHostPortNumber);
             }
           }

/* printf("For ICL(%d) in Initialize_ServerSockets(), after the Init_COMAPI call, int=%d, a_server_socket=%o, error_status=%d\n",
                    ICL_num, i, a_server_socket, error_status); */

           /* test the returned status, did we get the server socket? */
           if (error_status == 0)
           {
              /* Yes, we got the server socket as desired. Set
               * the appropriate server socket entry in the sgd.
               * Now Or in the successful error_status with the
               * summary_error_status to indicate we got at
               * least one server socket. (Or'ing the success
               * will mean summary_error_status should now be 0).
               */
              sgd.server_socket[ICL_num][i] = a_server_socket;
              sgd.comapi_reconnect_msg_skip_log_count[i] = 0; /* reset the repetition error 10001 count */
              summary_error_status = 0; /* got at least one server socket */

              /*
               * Now output a message into the JDBC$LOG file that
               * indicates we have a new server socket.
               */

              sprintf(message,"(%d)=%d using %s to listen on %s",
                      i, a_server_socket,
                      (comapi_interface_list[i] == CPCOMM) ? "CPCOMM" : "CMS",
                      sgd.listen_host_names[i]);

              /* 5412 ICL[{0}] server_socket{1} */
              getMessageWithoutNumber(SERVER_SOCKET_NOW_LISTENING,
                                      itoa(ICL_num, digits, 10),message,
                                      SERVER_LOGS_DEBUG);

              /* JDMS Server connected to COMAPI mode {0} using {1} */
              console_msg = getLocalizedMessageServerNoErrorNumber(SERVER_COMAPI_CONNECT_MESSAGE,
                                                                   sgd.ICLcomapi_mode[ICL_num],
                                                                   ((comapi_interface_list[i] == CPCOMM) ? "CPCOMM" : "CMS"),
                                                                   0, message);
              sendcns(console_msg, strlen(console_msg));
           }
           else
           {
              /*
               * No, we did not get the server socket as
               * desired. Clear the server socket session id,
               * update the summary_error_status value,
               * and test the returned status
               * to determine what was the problem.
               */
              sgd.server_socket[ICL_num][i] = 0;  /* we have no socket at this point. */

              /*
               * First test if we have lost the COMAPI background
               * run.  If so, clear all the server socket ids,
               * and return the COMAPI status. It doesn't help
               * to continue in this loop since we don't have any
               * sockets at this time and we don't have COMAPI.
               * There is no need to to a close on the sockets,
               * because they are all lost anyway.
               *
               * Notice that the dummy server socket is also lost,
               * so no special consideration to closing it is needed,
               * but clear its value for later loops.
               */
              dummy_server_socket = 0;
              if (error_status == SEACCES ){
                /* clear out all server socket entries */
                for (j = 0; j < sgd.num_server_sockets; j++){
                   sgd.server_socket[ICL_num][j] = 0;  /* we have no socket at this point. */
                }
                return error_status; /* let caller handle COMAPI background run down. */
              }

              /*
               * Now the or'ing of the error status with the
               * summary error status.
               *
               * If the summary error status value is 0, then
               * it should remain 0 since it indicates we have
               * at least one server socket.
               *
               * If the summary error status value is non-zero,
               * then just set it to the current error status
               * value. This works for two reasons: First, if
               * the summary error status value is INT_MAX, then
               * it was a placeholder value anyway and should be
               * replace by an actual error status. Second, it
               * doesn't really matter which server sockets bad
               * error status value is kept in summary_error_status
               * since the presence of this value at the end means
               * we did not get any server sockets and the caller
               * will have to go into COMAPI re-connect mode.
               */
              if (summary_error_status != 0)
              {
                 summary_error_status = error_status;
              }

              /* test the various network status sent back. */
              if (error_status == SENETDOWN || error_status == (030005 << 18) + SEBADTSAM ||
                  error_status == (030027 << 18) + SEBADTSAM || error_status == (030032 << 18) + SEBADTSAM ||
                  error_status == (030046 << 18) + SEBADTSAM )
              {
                 /*
                  * Bump the busy port count to indicate that
                  * we cannot get the server socket at this
                  * time (i.e.,  its "busy").
                  */
                 busy_ports++;
              }
           }
        } /* end of for */


         /*
          * Are all the server sockets we want to obtain on the
          * network interfaces busy? If so, we will wait a while
          * and try to obtain them again. There is no sense to
          * deregister from COMAPI or to return to the caller
          * since COMAPI is fully active - the ports are just all
          * busy and deregistering from COMAPI and having the
          * caller simply re-execute this function won't help
          * if the ports are still just busy.  Easier to just
          * wait here and try again.
          *
          * Before and after the Twait, test if we are to
          * shutdown this ICL. If so, don't mess with COMAPI.
          * Its a good time to check since we don't have any
          * server sockets to release.
          */
         if (busy_ports == sgd.num_server_sockets)
         {
           testAndCall_ICLShutdown_IfNeeded(INSIDE_INIT_SERVER_SOCKET,0);
           twait(sgd.comapi_reconnect_wait_time[ICL_num]);  /* wait some seconds */
           testAndCall_ICLShutdown_IfNeeded(INSIDE_INIT_SERVER_SOCKET,0);
         }

        /*
         * Else, not all ports on the network interfaces were just busy.
         *
         * The summary error status will tell us if we got at least one
         * server socket.  If not, there were other COMAPI errors. At
         * this point it is easiest to release the server sockets we
         * have.
         */
        else if (summary_error_status != 0)
        {
           /* Yes, there were other problems. Clear
            * out all server socket entries. Close
            * any sockets that may be open. No need
            * to test the error status since we just
            * want the socket closed and can't do any
            * other recovery at this point.
            */
           for (j = 0; j < sgd.num_server_sockets; j++){
              error_status=Close_Socket(sgd.server_socket[ICL_num][j]);
              sgd.server_socket[ICL_num][j] = 0;  /* we have no socket at this point. */
           }
        }
     } while (busy_ports == sgd.num_server_sockets);

     /*
      * Return the COMAPI summary error status to the
      * caller.
      * If 0, we got at least one server socket.
      *
      * If its non-zero, we had problems getting
      * the server sockets (which were freed above).
      */
     return summary_error_status;
}


/*
 * Function Terminate_Server_Sockets
 *
 * This function terminates all the server
 * sockets that are present for this ICL
 * instance.
 */
 void Terminate_Server_Sockets(){

    int i;
    int ICL_num=act_wdePtr->ICL_num; /* get an easy to use copy of ICL's number */

    /* loop over all the possible server sockets */
    for (i = 0; i < sgd.num_server_sockets; i++)
    {
      /* test if a server socket is present */
      if (sgd.server_socket[ICL_num][i] != 0)
      {
         /*
          * Terminate the Server socket for this network
          * connection (Rescind and close).
          */
         Rescind_Close_Socket(sgd.server_socket[ICL_num][i]);
         sgd.server_socket[ICL_num][i] = 0;  /* mark socket as closed */
      }
    }
 }
