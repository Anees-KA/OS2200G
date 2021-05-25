/**
 * File: ServerWorker.c.
 *
 * Procedures that comprise the JDBC Server's Server Worker function.
 *
 * This code may utilize other code in the C-Interface library to accomplish
 * its required tasks.
 *
 */
/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <rsa.h>
#include <time.h>
#include <tsqueue.h>
#include <task.h>
#include <universal.h>
#include "marshal.h"

/* JDBC Project Files */
#include "cintstruct.h" /* Include crosses the c-interface/server boundary */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "taskcodes.h" /* Include crosses the c-interface/server boundary */
#include "NetworkAPI.h"
#include "ProcessTask.h"
#include "ServerActWDE.h"
#include "ServerConsol.h"
#include "ServerLog.h"
#include "ServerWorker.h"
#include "signals.h"
#include "ipv6-parser.h"

extern int insideClibProcessing;    /* variable indicates whether executing in Clib (1) or not (0) */



extern serverGlobalData sgd;        /* The Server Global Data (SGD),*/
                                    /* visible to all Server activities. */
/*
 * Now declare an extern reference to the activity local WDE pointer act_wdePtr.
 * Normally this pointer will have the same value as wdePtr, unless a WDE entry
 * could not be allocated then anActivityLocalWDE will be used and act_wdePtr
 * will point to it.
 *
 * This is necessary since some of the log functions and procedures that are
 * being utilized by the JDBCServer/ConsoleHandler, ICL and Server Worker
 * activities need specific information to tailor their actions (i.e.
 * localization of messages).  The design of those routines utilized fields
 * in Server workers WDE to provide this tailoring information, and the
 * WDE pointer required is not passed to those routines but is assumed to be in
 * the activity local (file level) variable act_wdePtr.
 */
extern workerDescriptionEntry *act_wdePtr;
extern workerDescriptionEntry anActivityLocalWDE;

#ifndef XABUILD /* JDBC Server */
/*
 * These Macros are coded like procedures and could have been
 * implemented that way. As macros we get inline performance.
 *
 * These macros add and remove a WDE from the assigned
 * Server Worker WDE chain.
 */
#define add_to_assigned_wde_chain(ts_cell_ptr,wdeptr)                    \
                                                                         \
     /* lock the assigned chain T/S cell */                              \
     test_set(ts_cell_ptr);                                              \
                                                                         \
     /* Now handle the back pointer */                                   \
     wdeptr->assigned_wdePtr_back=NULL;                                  \
     if (sgd.firstAssigned_wdePtr !=NULL){                               \
       (sgd.firstAssigned_wdePtr)->assigned_wdePtr_back=wdeptr;          \
     }                                                                   \
                                                                         \
     /* Now handle the next pointer */                                   \
     wdeptr->assigned_wdePtr_next=sgd.firstAssigned_wdePtr;              \
     sgd.firstAssigned_wdePtr=wdeptr;                                    \
                                                                         \
     /* increment the counter of number on assigned SW chain */          \
     sgd.numberOfAssignedServerWorkers++;                                \
                                                                         \
     /* increment the counter of number on assigned SW for this ICL */   \
     sgd.ICLAssignedServerWorkers[wdeptr->ICL_num]++;                    \
                                                                         \
     /* increment the cumulative total of assigned workers, also */      \
     /* save this number in the current Server Worker's wde entry */     \
     sgd.cumulative_total_AssignedServerWorkers++;                       \
     wdeptr->assigned_client_number=sgd.cumulative_total_AssignedServerWorkers; \
                                                                         \
     /* unlock the assigned chain T/S cell */                            \
    ts_clear_act(ts_cell_ptr);

#define remove_from_assigned_wde_chain(ts_cell_ptr,wdeptr)               \
                                                                         \
     /* lock the assigned chain T/S cell */                              \
     test_set(ts_cell_ptr);                                              \
                                                                         \
     /* Now handle the back pointer */                                   \
     if (wdeptr->assigned_wdePtr_next !=NULL){                           \
       (wdeptr->assigned_wdePtr_next)->assigned_wdePtr_back              \
                              = wdeptr->assigned_wdePtr_back;            \
     }                                                                   \
                                                                         \
     /* Now handle the next pointer */                                   \
     if (wdeptr->assigned_wdePtr_back !=NULL){                           \
       (wdeptr->assigned_wdePtr_back)->assigned_wdePtr_next              \
                              = wdeptr->assigned_wdePtr_next;            \
     }                                                                   \
     else{                                                               \
        /* removing first entry on chain, so get new first of chain */   \
        sgd.firstAssigned_wdePtr=wdeptr->assigned_wdePtr_next;           \
     }                                                                   \
                                                                         \
     /* decrement the counter of number on assigned SW chain */          \
     sgd.numberOfAssignedServerWorkers--;                                \
                                                                         \
     /* decrement the counter of number on assigned SW for this ICL */   \
     sgd.ICLAssignedServerWorkers[wdeptr->ICL_num]--;                    \
                                                                         \
                                                                         \
                                                                         \
     /* unlock the assigned chain T/S cell */                            \
     ts_clear_act(ts_cell_ptr);

/*
 * This macro gets called when the Server Worker detects a global
 * Server shutdown is occurring.  It removes a WDE from the assigned
 * Server Worker WDE chain and checks if it is the last Server Worker
 * to shut down at the time the Server is shutting down ( by testing if
 * the sgd.ICLshutdownState is not ICL_ACTIVE). If so, it sets the
 * last_SW_flag to BOOL_TRUE, otherwise it sets it to BOOL_FALSE.
 */

#define remove_from_assigned_wde_chain_at_SW_shutdown(ts_cell_ptr,wdeptr,last_SW_flag) \
                                                                         \
     last_SW_flag=BOOL_FALSE; /* assume we're not the last Server Worker \
                                                                         \
     /* lock the assigned chain T/S cell */                              \
     test_set(ts_cell_ptr);                                              \
                                                                         \
     /* Now handle the back pointer */                                   \
     if (wdeptr->assigned_wdePtr_next !=NULL){                           \
       (wdeptr->assigned_wdePtr_next)->assigned_wdePtr_back              \
                              = wdeptr->assigned_wdePtr_back;            \
     }                                                                   \
                                                                         \
     /* Now handle the next pointer */                                   \
     if (wdeptr->assigned_wdePtr_back !=NULL){                           \
       (wdeptr->assigned_wdePtr_back)->assigned_wdePtr_next              \
                              = wdeptr->assigned_wdePtr_next;            \
     }                                                                   \
     else{                                                               \
        /* removing first entry on chain, so get new first of chain */   \
        sgd.firstAssigned_wdePtr=wdeptr->assigned_wdePtr_next;           \
     }                                                                   \
                                                                         \
     /* decrement the counter of number on assigned SW chain, and */     \
     /* test if this was the last Server Worker at Server shutdown */    \
     sgd.numberOfAssignedServerWorkers--;                                \
                                                                         \
     /* decrement the counter of number on assigned SW for this ICL */   \
     sgd.ICLAssignedServerWorkers[wdeptr->ICL_num]--;                    \
                                                                         \
     if (sgd.numberOfAssignedServerWorkers==0 &&                         \
         sgd.ICLShutdownState[wdeptr->ICL_num] != ICL_ACTIVE ) {         \
       last_SW_flag=BOOL_TRUE; /* indicate we were the last*/            \
     }                                                                   \
                                                                         \
     /* unlock the assigned chain T/S cell */                            \
     ts_clear_act(ts_cell_ptr);





/*
 * Define the macro that checks if the ServerWorker should call
 * serverWorkerShutDown to shut down the current Server Worker.
 */
#define testAndCall_SW_Shutdown_IfNeeded(wdeptr, shutDownPointInSW,clientsocket)    \
          if ((wdeptr->serverWorkerShutdownState == WORKER_SHUTDOWN_GRACEFULLY ) || \
              (wdeptr->serverWorkerShutdownState == WORKER_SHUTDOWN_IMMEDIATELY)){  \
            serverWorkerShutDown(wdeptr,shutDownPointInSW,clientsocket);            \
          } /* end of macro */

#define testAndCall_SW_ShutdownImmediately_IfNeeded(wdeptr, shutDownPointInSW,clientsocket) \
          if (wdeptr->serverWorkerShutdownState == WORKER_SHUTDOWN_IMMEDIATELY){            \
            serverWorkerShutDown(wdeptr, shutDownPointInSW, clientsocket);                  \
          } /* end of macro */


/*
 * Procedure createServerWorkerWDE
 *
 * This routine creates a new Worker Description Entry (WDE) and
 * returns a pointer to this new WDE.  The WDE created will be
 * pre-initialized to a default set of values and then linked
 * into the chain of all WDE entries.  Upon return from
 * createServerWorkerWDE(), the new Server Worker will have a WDE
 * ready to use.
 *
 * The only caller of this routine is the Server Worker's main
 * procedure, serverWorker().
 *
 * Returned value:
 *
 * *workerDescriptionEntry  - pointer to the newly created and
 *                            initialized WDE for the Server Worker.
 */

 static workerDescriptionEntry * createServerWorkerWDE(int new_client_socket
                                                       , int new_client_COMAPI_bdi
                                                       , int new_client_COMAPI_IPv6
                                                       , int new_client_ICL_num
                                                       , v4v6addr_type new_client_IP_addr
                                                       , int new_client_network_interface){
    int reallocedWDE;
    int activity_id;
    workerDescriptionEntry *wdePtr;
    char digits[ITOA_BUFFERSIZE];
    char digits2[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Test if we should log procedure entry */
    /* printf("Entering createServerWorkerWDE\n"); */

    /*
     * First check if we have reached our concurrency limit. If so, do
     * not allocate a WDE for this client (it will be shutdown later
     * without service).
     */
     if (sgd.numberOfAssignedServerWorkers >= sgd.maxServerWorkers) {
        tsk_id(&activity_id);
        getLocalizedMessage(SERVER_MAX_ACT_REACHED,
            itoa(sgd.maxServerWorkers, digits, 10),
            itoa_IP( new_client_socket, digits2, 10, new_client_IP_addr,
                    NULL, NULL), SERVER_LOGS, msgBuffer);
                /* 5351 Configuration concurrency limit of max_activities ({0}) reached,
                   connection refused for JDBC Client on client socket {1} */
        return NULL; /* signals no WDE creation */
     }

    /*
     * Next, allocate a new piece of program level memory
     * for this Worker Description Entry (WDE).  Once allocated,
     * save its location into both wdePtr and act_wdePtr.
     *
     * Temporary Patch:  Use the memory provided via the sgdPtr.
     *                   This will allow some creating of Server Worker
     *                   description entries until the real code
     *                   solution is available.  This code will detect
     *                   when this memory is used up and error in the
     *                   same manner as the actual code will, when its
     *                   available.
     */

     /*
      * First look to see if there is a available WDE on the reuse chain.
      * By taking one from that chain first, we keep the real memory usage
      * down since we will allocate new WDE entries from the wdeArea only as needed.
      * If no free reusable WDE is available, allocate one from the wdeArea.
      * This has to be under T/S control since we are afecting chains and/or offsets.
      */

     reallocedWDE=BOOL_FALSE; /* assume we will not get a realloc'ed WDE entry */
     /* lock the WDE reallocation chain chain T/S cell */
     test_set(&(sgd.WdeReAllocTS));

      /* test if there is an available WDE to reuse */
      if (sgd.realloc_wdePtr != NULL){
        /*
         * Yes, there is a WDE entry that can be reused.
         * So use it and pop it off the chain.
         */
        wdePtr=sgd.realloc_wdePtr;
        sgd.realloc_wdePtr= wdePtr->realloc_wdePtr_next;

        /* unlock the WDE reallocation chain T/S cell */
        ts_clear_act(&(sgd.WdeReAllocTS));

        reallocedWDE=BOOL_TRUE; /* indicate we got a realloc'ed WDE entry */
      }
      else {
       /* we have to get a new one from new WDE area space. */
       if (sgd.numWdeAllocatedSoFar < sgd.tempMaxNumWde){
        /*
         * we still have memory, so allocate an entry there. Note
         * that the array is 0 based so get the ith+1 entry first
         * and bump counter after.
         */
        wdePtr=&sgd.wdeArea[sgd.numWdeAllocatedSoFar]; /* point to next new wde entry */
/* printf("Allocated space for a new WDE, at wdeArea[%i]\n",sgd.numWdeAllocatedSoFar); */
        sgd.numWdeAllocatedSoFar++;

        /* unlock the WDE reallocation chain T/S cell */
        ts_clear_act(&(sgd.WdeReAllocTS));

        /*
         * Initialize to 0 the field containing the total malloc'ed words of allocated
         * space for this new WDE entry. This is done before any space is Tmalloc'ed or
         * Tcalloc'ed.
         */
        wdePtr->totalAllocSpace = 0;
        }
       else {

        /* unlock the WDE reallocation chain T/S cell */
        ts_clear_act(&(sgd.WdeReAllocTS));

        /*
         * We have attempted to allocate the memory for a Worker
         * Description Entry and no memory was available. Use the
         * activity local WDE entry for logging purposes since some
         * routines expect a WDE to get message file, logfile info, etc.
         * Then return a NULL pointer for wdePtr as
         * an error indicator to the calling routine to handle.
         */
        getLocalizedMessage(SERVER_CANT_GET_WDE_MEMORY,
            itoa_IP(new_client_socket, digits, 10, new_client_IP_addr, NULL, NULL),
            NULL, SERVER_STDOUT, msgBuffer);
                /* 5354 Couldn't allocate the Server Worker Description Entry
                   memory for JDBC Client on client socket {0} */
        getLocalizedMessage(SERVER_ACT_COUNT,
            itoa(sgd.maxServerWorkers, digits, 10),
            itoa(sgd.numWdeAllocatedSoFar, digits2, 10), SERVER_STDOUT, msgBuffer);
                /* 5355 JDBC Server has max_activities set to {0},
                   and number allocated so far = {1} */

        return NULL; /* signals bad WDE creation attempt */
        }
      }

      /*
       * A WDE entry was obtained - either a reused one or a new one.
       * So, do three things:
       *  1) Initialize this WDE entry minimally.
       *  2) Switch the allocated C_INT data structure from the activity
       *     local WDE to the actual WDE. The activity local WDE will no
       *     longer be used and there is no reason to abandon allocated
       *     malloc space or to allocate that data structure again.
       *  3) Reset the ServerWorker's activity local WDE pointer to point
       *     to this WDE.
       */
      minimalInitializeWDE(wdePtr,SERVER_WORKER);

      /* Move over the C_INT structure buffer. */
      wdePtr->c_int_pkt_ptr = act_wdePtr->c_int_pkt_ptr;

      /* Switch activity local WDE pointer to point to actual WDE. */
      act_wdePtr=wdePtr;

    /* addServerLogEntry(SERVER_STDOUT,"Done setting up act_wdePtr with actual WDE in SW"); */

    /*
     * We have a WDE entry, next assign it to the JDBC Client that
     * was provided on the Server Worker's activity start up.
     */
    assignServerWorkerToClient(wdePtr, new_client_socket, new_client_COMAPI_bdi, new_client_COMAPI_IPv6,
                               new_client_ICL_num, new_client_IP_addr, new_client_network_interface);

    /*
     * Now complete the initialization of the WDE.
     *
     * First, allocate the standingRequestPacket for this WDE. The allocation
     * is done directly using a malloc() call since it is not part of the
     * Tmalloc/Tfree allocations. This malloc'ed area, like the malloc'ed
     * c_intDataStructure space is acquired only when a reused or new WDE
     * is obtained for a new ServerWorker activity.
     *
     * Note: A free ServerWorker activity that is woken up to handle a new
     * JDBC client already has these malloc'ed areas. Only when the ServerWorker
     * activity is stopped and it's WDE is placed on the reuse list will the
     * standingRequestPacket and the c_intDataStructure malloc's be free'ed,
     * and then later redone at the appropriate times when the WDE is reused.
     */
    wdePtr->standingRequestPacketPtr = malloc(STANDING_REQUEST_PACKET_SIZE); /* Always malloc(), not Tmalloc() */
    initializeServerWorkerWDE(wdePtr);

/* printf("New Server Worker %i created and set up for new client %%012o (%024llo)\n",wdePtr->serverWorkerActivityID,wdePtr->serverWorkerUniqueActivityID,new_client_socket); */

    /*
     * Now, link the WDE into the chain of all the Server Workers
     * WDE entries. This step is done under Test and Set control.
     *
     * Once added to the assigned Server Workers WDE chain, this WDE is visible
     * to the other main activities of the JDBC Server, Initial Connection
     * Listener and the Console Handler. They can post a shutdown directive
     * (e.g. serverWorkerShutDownState = WORKER_SHUTDOWN_GRACEFULLY) that this
     * Server Worker will have to obey which is not part of the Server
     * Worker codes normal flow.
     *
     * Do only the minimum work to add the newly created WDE to the assigned
     * Server Worker chain. (Remember the chain is a doubly linked list and we
     * are adding a WDE at the front (top) end.)
     */
/* printf("Just before add_to_assigned_wde_chain in createServerWorkerWDE\n");  */
    add_to_assigned_wde_chain(&sgd.firstAssigned_WdeTS,wdePtr);
/* printf("Just after add_to_assigned_wde_chain in createServerWorkerWDE\n"); */

    /*
     * All done creating the WDE. It is now ready to use, and is in fact
     * now visible to the other activities via the assigned WDE chain.
     *
     * All done, so return to caller.
     */
     return wdePtr;
  }

/*
 * Procedure assignServerWorkerToClient
 *
 * This routine assigns a new JDBC Client to a particular Server Worker.
 * This is done by recording network access information of the new JDBC
 * Client in the Server Worker's WDE and setting the WDE state indicators
 * to an initializing for a new JDBC Client state. This routine will be
 * called by the Initial Connection Listener (ICL) when it assigns a free
 * unassigned) Server Worker, or as part of the start up process a brand
 * new Server Worker does when it is started by the ICL when no free Server
 * Workers were found.
 *
 * Important Note: The rest of the WDE's variables MUST be (re-)initialized to
 * their default settings by the calling the initializeServerWorkerWDE()
 * procedure.  For performance reasons, initializeServerWorkerWDE() is always
 * called under the Server Workers activity (i.e. the ICL spends only enough
 * time in assignServerWorkertoClient() to assign a new JDBC Client to the
 * Server Worker - the Server Worker must complete WDE initialization on its
 * own time.)
 *
 * Parameters:
 *
 * wdePtr                - pointer to the Server Workers WDE.
 * new_client_socket     - the COMAPI socket assigned for the network dialogue
 *                         with the new JDBC Client assigned to this Server Worker.
 * new_client_COMAPI_bdi - the COMAPI bdi for the new JDBC Client's COMAPI socket.
 * new_client_ICL_num    - the ICL num of the ICL activity for the new JDBC Client socket.
 * new_client_IP_addr    - the IP address of the new JDBC Client.
 * new_client_network_interface - the network adapter for the client connection (CPCOMM or CMS)
 */
 void assignServerWorkerToClient(workerDescriptionEntry *wdePtr
                                 , int new_client_socket
                                 , int new_client_COMAPI_bdi
                                 , int new_client_COMAPI_IPv6
                                 , int new_client_ICL_num
                                 , v4v6addr_type new_client_IP_addr
                                 , int new_client_network_interface){

    /* Test if we should log procedure entry */
   /* if (sgd.debug==BOOL_TRUE){
      addServerLogEntry(SERVER_STDOUT,"Entering assignServerWorkerToClient");
    }*/

#if SIMPLE_MALLOC_TRACING == 2
     /* When simple malloc tracing to print$, include new client's starting "ballast". */
     printf("\nIn assignServerWorkerToClient(): New client (%p)'s initial totalAllocSpace = %d\n", wdePtr, wdePtr->totalAllocSpace);
#endif
    /*
     * Assign the new JDBC Client to the Server Worker by saving
     * its socket, COMAPI bdi, ICL number, client IP address, and
     * network interface (CPCOMM or CMS).
     */
    wdePtr->client_socket=new_client_socket;
    wdePtr->comapi_bdi=new_client_COMAPI_bdi;
    wdePtr->comapi_IPv6=new_client_COMAPI_IPv6;
    wdePtr->ICL_num=new_client_ICL_num;
    wdePtr->client_IP_addr=new_client_IP_addr;
    wdePtr->network_interface=new_client_network_interface;

    /*
     * Now set the state of the WDE to show that the
     * Server Worker is initializing its WDE.
     */
    wdePtr->serverWorkerState=WORKER_INITIALIZING;

    /* all done */
  }
#endif          /* JDBC Server */

/*
 * Procedure initializeServerWorkerWDE
 *
 * This routine initializes the WDE of this Server Worker to initial
 * default values with the exceptions of the new JDBC Client socket
 * and IP address information which must be set first through a call
 * to the assignServerWorkertoClient() procedure and the sufficently sized
 * request packet buffer that is created in createServerWorkerWDE.
 *
 * This routine is always called by the Server Worker as part of its
 * initial JDBC Client handling process, i.e. this is done either
 * immediately after the Server Worker awakes from having been on
 * the free worker chain (since it now has a new JDBC Client assigned
 * to it by the ICL), or as part of a newly created ( task started)
 * Server Worker activity that is setting up to process its first
 * JDBC Client.
 *
 * The WDE that's initialized is not placed on the assigned Server
 * Worker's WDE entry chain. That task,if needed, is the responsibility
 * of the caller.
 *
 * Important Note:
 * This routine must be called after assignServerWorkertoClient and before
 * the WDE can used to service that JDBC Client (e.g. before receiving the
 * first Request Packet from COMAPI ).
 *
 * XA JDBC Server Note:
 * This routine is the only ServerWorker.c routine used by the XA Server. It
 * is the responsibility of the XASRVRWorker.c code to properly set up its
 * wde entry up to the initializeServerWorkerWDE() method call in the same
 * manner as performed by the Server Worker code when it needs to create
 * and initialize a new wde entry. The initializeServerWorkerWDE() procedure
 * is conditionally compiled so that it can be used by both the JDBC Server
 * and the XA JDBC Server to complete the initialization of the wde entry
 * used by the Server Worker or XA Server Worker respectively.
 *
 * Parameters:
 *
 * wdePtr             - pointer to the Server Workers WDE.
 *
 */
void initializeServerWorkerWDE(workerDescriptionEntry *wdePtr){

    /*
     * Do these assignments first so we can produce messages immediately.
     */
    wdePtr->ownerActivity=SERVER_WORKER;              /* This WDE is owned by the SW */
    wdePtr->serverMessageFile=sgd.serverMessageFile;  /* Initially, use the Server's locale message file */
    wdePtr->workerMessageFile=sgd.serverMessageFile;  /* Initially, use the Server's locale message file */
    strcpy(wdePtr->workerMessageFileName,sgd.serverMessageFileName); /* the Server's locale message filename */

    /* Test if we should log procedure entry */
  /*  if (sgd.debug==BOOL_TRUE){
      addServerLogEntry(SERVER_STDOUT,"Entering initializeServerWorkerWDE");
    }*/

    /*
     * Assigns or re-assigns the initial default values to this WDE
     * with the exception of the following variables that are set in
     * assignServerWorkertoClient prior to calling this procedure:
     *
     * client_socket     - already set to new JDBC Clients socket id.
     * comapi_bdi        - already set to the COMAPI bdi for client socket.
     * ICL_num           - already set to the ICL instance for this client socket.
     * client_IP_addr    - already set to new JDBC Clients IP address.
     * network_interface - already set to new JDBC Clients network adapter (CPCOMM or CMS).
     * serverWorkerState - already set to WORKER_INITIALIZING.
     * standingRequestPacketPtr - space already malloc'ed and pointer set.
     *
     * For a JDBC Server Worker only:
     * When we receive request packet data from the client, we first
     * have to ask for its length in bytes and then the
     * request COMAPI to receive the rest of this request packet.
     * Pre-allocate a standing request packet buffer that's usually
     * sufficent in size to place most of our typical request packets
     * into.  Hopefully we can reuse this buffer for the vast majority
     * of the user's request packets so we don't get into an
     * allocate/deallocate scenario - that is not good for memory management.
     */
 /* printf("got here 1 in initializeServerWorkerWDE\n");    */
    tsk_id(&(wdePtr->serverWorkerActivityID));             /* Exec 2200 36-bit activity ID. */
    tsk_retactid(&(wdePtr->serverWorkerUniqueActivityID)); /* Exec 2200 72-bit unique activity ID. */
    wdePtr->serverWorkerTS=TSQ_NULL;                   /* Initialize the T/S cell for this Server Worker's WDE */
    wdePtr->free_wdePtr_next=NULL;                     /* NULL pointer - end of chain. */
    wdePtr->assigned_wdePtr_back=NULL;                 /*  NULL pointer - end of chain. */
    wdePtr->assigned_wdePtr_next=NULL;                 /*  NULL pointer - end of chain. */
    /* wdePtr->serverWorkerState */                    /* set already to WORKER_INITIALIZING */
    wdePtr->serverWorkerShutdownState=WORKER_ACTIVE;   /* Initially, don't plan on a shutdown */
    /* wdePtr->client_socket */                        /* set already to new JDBC Clients socket id. */
    /* wdePtr->comapi_bdi */                           /* set already to new the COMAP bdi for Clients socket. */
    /* wdePtr->ICL_num */                              /* set already to the ICL number for this Clients socket. */
    /* wdePtr->client_IP_addr */                       /* set already to new JDBC Client's IP address. */
    /* wdePtr->network_interface */                    /* set already to new JDBC Client's network adapter. */
    wdePtr->client_receive_timeout=sgd.server_activity_receive_timeout; /* use sgd's value for client receive timeout. */
    wdePtr->fetch_block_size=sgd.fetch_block_size;     /* use sgd's value for client fetch block size. */
    strcpy(wdePtr->client_Userid,"\0");                /* the JDBC Client's userid */
    wdePtr->firstRequestTimeStamp = 0;                 /* Timestamp of the client's first request packet. */
    wdePtr->lastRequestTimeStamp = 0;                  /* Timestamp of the client's last request packet. */
    wdePtr->client_keepAlive = sgd.config_client_keepAlive; /* Initially set to the Server's configuration default, this will be  */
                                                       /* changed when the client's requested connection properties are processed. */
    wdePtr->txn_flag = 0;                              /* Initialize to 0 to indicate this request packet is not in a transaction. */
                                                       /* Otherwise a value of 1 indicates a transaction. Used by JDBC XA Server   */
                                                       /* only. Set from ODTP info obtained from CORBA request packet.            */
    wdePtr->odtpTokenValue = 0;                        /* Initialize to 0 to indicate this request packet is not in stateful      */
                                                       /* dialogue with the JDBC XA Server instance. Otherwise, the non-0 value   */
                                                       /* indicates a stateful dialogue is in progress with a specifc JDBC XA     */
                                                       /* Server instance. Used by JDBC XA Server only. Set from info obtained    */
                                                       /* from the JDBC request packet header.                                    */
    wdePtr->in_COMAPI=NOT_CALLING_COMAPI;              /* Initialize COMAPI subsystem calling flag */
                                                       /* In XA JDBC Server, in_COMAPI flag indicates we are not in OTM instead of COMAPI */
    wdePtr->in_RDMS=NOT_CALLING_RDMS;                  /* Initialize UDS/RDMS/RSA subsystem calling flag */

#ifndef XABUILD /* JDBC Server */

    wdePtr->serverCalledVia=CALLED_VIA_COMAPI;         /* called via network - COMAPI. */

#else /* JDBC Server */

    wdePtr->serverCalledVia=CALLED_VIA_CORBA;          /* called via CORBA and RMI-IIOP. */
    wdePtr->resPktLen = 0;                             /* No CORBA response packet yet, reset its length */

#endif /* XA and JDBC Servers */

    wdePtr->workingOnaClient=BOOL_TRUE;                /* Indicates we will do tasks for client */
    wdePtr->openRdmsThread=BOOL_FALSE;                 /* Indicates we do not have an open Rdms thread */
    /* wdePtr->standingRequestPacketPtr */             /* Set already to an appropriate sized buffer */
    wdePtr->requestPacketPtr=NULL;                     /* No request packet memory at this time. */
    wdePtr->releasedResponsePacketPtr = NULL;          /* No response packet waiting for reallocation. */
    wdePtr->taskCode=0;                                /* No Request Packet task code */
    wdePtr->debug=BOOL_FALSE;                          /* Indicate debug/tracing is off. */
    wdePtr->debugFlags=0;                              /* No debug flags are set. */
    strcpy(wdePtr->clientTraceFileName,"\0");          /* No trace file name. */
    wdePtr->clientTraceFile=NULL;                      /* No trace file handle. */
    wdePtr-> clientTraceFileNbr = 0;                   /* Clear client trace file nbr, used for console @USE cmds */
    wdePtr->consoleSetClientTraceFile = FALSE;         /* TRUE if a console SET cmd sets the client trace file. */
    strcpy(wdePtr->threadName,"\0");                   /* Clear the thread name passed to RDMS at Begin Thread time. */
    strcpy(wdePtr->clientDriverBuildLevel,"\0");       /* Clear the client Driver Build id string. */
    wdePtr->clientDriverLevel[0] = 0;                  /* Client Driver level - major */
    wdePtr->clientDriverLevel[1] = 0;                  /* Client Driver level - minor */
    wdePtr->clientDriverLevel[2] = 0;                  /* Client Driver level - feature */
    wdePtr->clientDriverLevel[3] = 0;                  /* Client Driver level - platform */

    /*
     * Now (re)-initialize the C_INT data structure
     * so it can be used by the C-Interface routines to hold
     * its state information.
     */
    initializeC_INT((c_intDataStructure *)wdePtr->c_int_pkt_ptr);

    /* All Done setting up the WDE */
 }

#ifndef XABUILD /* JDBC Server */
/*
 * Procedure serverWorkerShutDown
 *
 * This routine shuts down the Server Worker activity.  It can
 * perform the following tasks, dependent on where in the main
 * Server Worker code it is called from:
 *
 * 1 ) Closes the Client Trace file, if open ( file handle != NULL)
 *
 * 2 ) Removes the Server Workers WDE entry from the assigned
 *     WDE chain.
 *
 * 3 ) Releases the memory for the WDE.
 *
 * 4 ) Release the standing Request Packet buffer pointed to by
 *     wdePtr->standingRequestPacketPtr.
 *
 * 5 ) Closes the client socket, if not already closed.
 *
 * 6 ) Stops the particular Server Worker activity.
 *
 * This routine will be called as a result of the serverWorkerState
 * being set to WORKER_SHUTDOWN_GRACEFULLY or WORKER_SHUTDOWN_IMMEDIATELY,
 * and the state value was tested and processed by this Server Worker,
 * at one of the appropriate places in the code flow of the Server Worker.
 */
void serverWorkerShutDown(workerDescriptionEntry *wdePtr
                          , enum serverWorkerShutdownPoints serverWorkerShutdownPoint
                          , int client_socket){

    int error_status;
    char * ccc_message_ptr = NULL;
    int last_SW_at_shutdown=BOOL_FALSE; /* initially assume we are not last SW */
    int error_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;   /* maximum length error message can be */
    char error_message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* a buffer for a returned error message */
    char digits[ITOA_BUFFERSIZE];
    char digits2[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Test if we should log procedure entry */
    if (sgd.debug==BOOL_TRUE){
        getMessageWithoutNumberForWorker(SERVER_WORKER_ACT_SHUTTING_DOWN,
            itoa(wdePtr->serverWorkerActivityID, digits, 8),
            itoa_IP(client_socket, digits2, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
            SERVER_STDOUT);
                /* 5356 Entering function serverWorkerShutDown
                   for SW activity {0}, client socket = {1} */
    }

    /*
     * Test the Server Worker shutdown point.  Based on what has been done so
     * far, there are one or more steps that need to be done to clean up. They
     * will be tested in order so they can share common handling where possible.
     * The Case entries are set up so you can flow from one case to the next.
     * At the end of the last case done, the Server Worker activity is stopped.
     */
    switch(serverWorkerShutdownPoint)
    {
        /*
         * For these shutdown points, we have to remove the SW from
         * the SW assigned chain.  Once we've done that, control does
         * flow into the next case set, where the rest of the Server
         * Worker shutdown operations take place. This sequence of
         * operations is dependent on the current form of the Server
         * Worker code; if code organization changes, then we need to
         * review the operation taken here and in what order.
         *
         * Close the Client Trace file, if the file handle is != NULL.
         *
         * Remove the Server Worker from the SW assigned chain. This
         * must be done under T/S control using the assigned_WdeTS
         * T/S cell. Also find out if we are the last Server Worker at
         * JDBC Server shutdown time and set last_SW_at_shutdown
         * accordingly for testing later.
         */
        case SW_CONTINGENCY_CASE:
        case NEW_WORKER_JUST_INITIALIZED:
        case CANT_RECEIVE_REQUEST_PACKET:
        case DETECTED_BAD_REQUEST_PACKET:
        case CANT_SEND_RESPONSE_PACKET:
        case AFTER_SENDING_RESPONSE_PACKET:
        case AFTER_RECEIVING_DATA:
        case DONE_PROCESSING_CLIENT:
        case AFTER_GIVEN_NEXT_CLIENT:{
            if (wdePtr->clientTraceFile != NULL){
                /* Clear the debug flags since we are done with the client and will be
                 * closing the trace file.
                 */
                debugBrief =    BOOL_FALSE;
                debugDetail =   BOOL_FALSE;
                debugInternal = BOOL_FALSE;
                debugSQL =      BOOL_FALSE;
                debugSQLP =     BOOL_FALSE;
                debugSQLE =     BOOL_FALSE;

                /* A Client Trace file is present, attempt to close it */
                error_status= closeClientTraceFile(wdePtr, error_message, error_len);
                if (error_status !=0){
                    /*
                     * Error occurred in trying to close client trace file. Log
                     * this error into the JDBC Server Log file and continue
                     * Server Worker shutdown steps.
                     */
                    addServerLogEntry(SERVER_LOGS,error_message);
                }
            }

            /* now remove the Server Worker from assigned chain */
            remove_from_assigned_wde_chain_at_SW_shutdown(&sgd.firstAssigned_WdeTS,wdePtr,last_SW_at_shutdown);
	
            if (debugInternal)
                tracef("\nsgd.numberOfAssignedServerWorkers=%d\n, last_SW_at_shutdown=%d\n",sgd.numberOfAssignedServerWorkers,last_SW_at_shutdown);

            /* Now flow into next case handling code and do that code as well. */
            }
        /*
         * For these shutdown points, we have already removed the SW from
         * the SW assigned chain or the SW was not on the SW assigned chain.
         * There is no open JDBC Client trace file at this point.
         * Now just complete the rest of the Server Worker shutdown operations
         * that are needed. Test last_SW_at_shutdown to determine if we need to
         * notify the Console Handler activity.
         *
         * Notice we may flow into these cases from the case set above.
         */
        case WOKEN_UP_TO_JUST_SHUTDOWN:
        case COULDNT_INHERIT_CLIENT_SOCKET:
        case END_OF_MORE_CLIENTS_LOOP:{
            /*
             * The Server Worker is not now on the assigned chain
             * and should have no JDBC Client assigned to it.
             *
             * Now close the client socket, if one is still present.
             * Remember, a client socket value of 0 indicates there is
             * no socket to close.
             */
            if (client_socket != 0){
                error_status =Close_Socket(client_socket);
                if (error_status != 0 ){
                   /*
                    * We got an error trying to close the socket from COMAPI.
                    * The COMAPI error is already in the Log file. The socket
                    * will error off automatically, so continue shutting down
                    * this JDBC Server Worker instance.
                    */
                    getLocalizedMessage(SERVER_CANT_CLOSE_CLIENT_SOCKET,
                        itoa_IP(client_socket, digits, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
                        itoa(error_status, digits2, 10), SERVER_LOGS, msgBuffer);
                            /* 5357 Can't close client socket {0} in function
                               serverWorkerShutDown, status = {1}*/
                  }
                }

            /*
             * Now release the standing request packet buffer. Double
             * check that it exists before attempting to free it. Remember,
             * it was malloc()'ed not TMalloc()'ed so it must be just free()'ed.
             */
            if (wdePtr->standingRequestPacketPtr != NULL){
               free(wdePtr->standingRequestPacketPtr);
               wdePtr->standingRequestPacketPtr=NULL; /* indicate we have released it */
            }

            /* Immediately before stopping this Server Worker task, test if we
             * are to notify the Console Handler that we are the last Server Worker
             * to complete at shutdown time.
             */
            if (last_SW_at_shutdown == BOOL_TRUE){
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
                    sgd.consoleHandlerShutdownState=CH_SHUTDOWN;
                    error_status=deregisterKeyin();
                    if (error_status !=0){
                        /* Bad deregister status, log error at stop JDBCServer */
                        getLocalizedMessage(SERVER_BAD_DEREG_KEYIN,
                            itoa(error_status, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                                /* 5358 Bad deregisterKeyin() status {0}
                                   encountered in function
                                   serverWorkerShutDown */
                        getLocalizedMessage(
                            SERVER_SHUT_DOWN_SERVER_AND_WORKER, NULL, NULL,
                            SERVER_LOGS, msgBuffer);
                                /* 5359 Server Worker and JDBC Server will
                                   be shut down due to detected error */
                        wdePtr->serverWorkerState=WORKER_DOWNED_DUE_TO_ERROR;

                        /* Deregister COMAPI from the application,
                           and stop all active tasks. */
                        deregComapiAndStopTasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);
                    }
                }
            }

            /*
             * Since we are shutting down the Server Worker and stopping its
             * activity and possibly terminating a still open Rdms thread.
             * So check if the Server Worker currently has an open RDMS Thread.
             * To avoid a UDS APT capture (which does a rollback and a end thread)
             * when the activity stops, we will need to do an explicit ROLLBACK
             * and END THREAD command sequence in that case.
             */
            if (wdePtr->openRdmsThread == BOOL_TRUE){
                /*
                 * Yes a thread is open. Call closeRdmsConnection to
                 * do the rollback and end thread.  We do not need to do
                 * any additional clean up, since the activity is going away.
                 */
                closeRdmsConnection(0); /* set cleanup argument to false */
            }

            /* Indicate in the wde entry that this Server Worker is
             *  shut down and closed.
             */
            wdePtr->serverWorkerState=WORKER_CLOSED;
            wdePtr->serverWorkerState=WORKER_DOWNED_DUE_TO_ERROR;

            /*
             * Now shut down this Server Worker activity right now.
             * Issue the message if the shutdown point was not either
             * WOKEN_UP_TO_JUST_SHUTDOWN, DONE_PROCESSING_CLIENT,
             * AFTER_RECEIVING_DATA ( passed event notifying immediate shutdown),
             * or END_OF_MORE_CLIENTS_LOOP, since these cases have
             * no more client involvement.
             */
            if (!(serverWorkerShutdownPoint == WOKEN_UP_TO_JUST_SHUTDOWN ||
                  serverWorkerShutdownPoint == DONE_PROCESSING_CLIENT    ||
                  serverWorkerShutdownPoint == AFTER_RECEIVING_DATA      ||
                  serverWorkerShutdownPoint == END_OF_MORE_CLIENTS_LOOP)){
            sprintf(error_message,"%s, status %d",
                    itoa_IP(client_socket, digits,10,wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
                    (int)serverWorkerShutdownPoint);
            getLocalizedMessage(
                  SERVER_SHUT_DOWN_SERVERWORKER, itoa(wdePtr->serverWorkerActivityID, digits,8),
                   error_message, TO_SERVER_LOGFILE, msgBuffer);
                  /* 5377 Server Worker activity {0}, client socket {1}, shut down
                     due to detected error, any open RDMS Thread was rolled back
                     and ended */
/* printf("In SW shutdown, shutdown indicator is %d\n",serverWorkerShutdownPoint); */
            }

            /*
             * Clear the c_intDataStructure pointer for this WDE entry.
             * Note: Since the ServerWorker activity is exiting, all it's
             * malloc'ed memory (C_INT structure, PCA, stmtInfo entries,
             * etc) will go away, so we don't need to explicitly free it.
             */
            wdePtr->c_int_pkt_ptr = NULL;

            /*
             * Clear out the Server Worker activity id's as well
             * since the activity is being stopped.
             */
            wdePtr->serverWorkerActivityID=0;
            wdePtr->serverWorkerUniqueActivityID = 0;

            /* Place the Server Worker's WDE on the WDE reuse chain.
             * Lock the assigned chain T/S cell to do this safely.
             */
            test_set(&(sgd.WdeReAllocTS));
            wdePtr->realloc_wdePtr_next=sgd.realloc_wdePtr;
            sgd.realloc_wdePtr= wdePtr;

            /* Indicate we shut down the Server Worker activity. (It's malloc space all goes away.) */
            sgd.numberOfShutdownServerWorkers++;

            /* unlock the assigned chain T/S cell */
            ts_clear_act(&(sgd.WdeReAllocTS));
/* printf("WDE entry %p placed on WDE reallocation chain.\n",wdePtr); */

            exit(0); /* exit task normally */
            break;
            }
        default:{
            /*
             * Bad serverWorkerShutdownPoint value. Signal it with a Log
             * message. Indicate the SW state as downed due to an error
             * and shut the whole JDBC Server down, as is (don't worry about
             * open Rdms Threads, UDS/RDMS APT will handle that.
             */
            getLocalizedMessage(SERVER_BAD_SW_SHUTDOWN_POINT, NULL, NULL,
                SERVER_LOGS, msgBuffer);
                    /* 5360 Bad Server Worker shutDownPoint value encountered
                       in function serverWorkerShutDown */

            getLocalizedMessage(SERVER_SHUT_DOWN_SERVER_AND_WORKER, NULL,
                NULL, SERVER_LOGS, msgBuffer);
                    /* 5359 Server Worker and JDBC Server will be shut down */
            wdePtr->serverWorkerState=WORKER_DOWNED_DUE_TO_ERROR;

            /* Deregister COMAPI from the application,
               and stop all active tasks. */
            deregComapiAndStopTasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);
            }
    }
}


#define DEFAULT_REQUEST_PKT_SIZE = 2500 /*from Driver Java code*/

/*
 * Func moveByteArrayArg_1
 *
 * This function is a copy of the function moveByteArrayArg present in Packet.c
 * Additional Boundary checks have been placed to ensure: 
 * - we are working or pointing within the packet size.
 * - Whether we get a decoded string length value len which is valid
 *
 * ToDo: instead of using STANDING_REQUEST_PACKET_SIZE we could use DEFAULT_REQUEST_PKT_SIZE 
 * Or pass the actual packet size value present in the packet i.e. variable actualRequestPacketSize. 
 *
 * @brief: copies len number of bytes from the buffer
 * starting at offset bufferIndex into retBuffer.
 * The value len is an int which is 4 bytes long.
 * which is also extarcted from the packet.
 *
 * @params1: pointer to the destination buffer
 * @params2: pointer to the source buffer
 * @params3: offset into the source buffer
 * @params4: sizeof destination buffer
 * @returns: 0 = not error, other value = error 
 *
 */
int moveByteArrayArg_1(char * retBuffer, char * buffer, int * bufferIndex, int retBufSize)
{
    int len = 0;
    int ind = 0;
	
	if(*bufferIndex >= STANDING_REQUEST_PACKET_SIZE || *bufferIndex < 0 
		|| retBufSize >= STANDING_REQUEST_PACKET_SIZE)
	{
		printf("\n[moveByteArrayArg_1], Detected invalid offset value in packet!\n");
		return -1;
	}

    /* Skip over the descriptor type and get the byte array length */
    ind = *bufferIndex + BYTE_LEN;
    len = getDescriptorLength(buffer, &ind);

	if(len > retBufSize || len < 0)
	{
		printf("\n[moveByteArrayArg_1], Detected invalid param data value in packet!");
		return -1;
	}

    /* Get the bytes of the byte array */
    memcpy(retBuffer, buffer+ind, len);

    /* Bump the index by the size of a descriptor and the byte array */
    *bufferIndex += DESCRIPTOR_LEN + len;

    return 0;

} /* moveByteArrayArg_1 */


/*
 * Func moveUnicodeStringArg_1
 *
 * This function is a copy of the function moveUnicodeStringArg present in Packet.c
 * Additional Boundary checks have been placed to ensure: 
 * - we are working or pointing within the packet size.
 * - Whether we get a decoded string length value len which is valid
 *
 * ToDo: instead of using STANDING_REQUEST_PACKET_SIZE we could use DEFAULT_REQUEST_PKT_SIZE 
 * Or pass the actual packet size value present in the packet i.e. variable actualRequestPacketSize. 
 *
 * @brief: copies len number of bytes from the buffer
 * starting at offset bufferIndex into retBuffer.
 * The value len is an int which is 4 bytes long.
 * which is also extarcted from the packet.
 *
 * @params1: pointer to the destination buffer
 * @params2: pointer to the source buffer
 * @params3: offset into the source buffer
 * @params4: sizeof destination buffer
 * @returns: 0 = not error, other value = error 
 *
 */
int moveUnicodeStringArg_1(char * retBuffer, char * buffer, int * bufferIndex, int retBufSize)
{
    int len = 0;
    int ind = 0;

	if(*bufferIndex >= STANDING_REQUEST_PACKET_SIZE || *bufferIndex < 0 
		|| retBufSize >= STANDING_REQUEST_PACKET_SIZE)
	{
		printf("\n[moveUnicodeStringArg_1], Detected invalid offset value in packet!\n");
		return -1;
	}

    /* Skip over the descriptor type and get the Unicode string length in bytes */
    ind = *bufferIndex + BYTE_LEN;
    len = getDescriptorLength(buffer, &ind);

	if(len >= retBufSize-2 || len < 0) /* need 2 bytes for NULL char & length can't be -ve */
	{
		printf("\n[moveUnicodeStringArg_1], Detected invalid param data value in packet!");
		return -1;
	}

    /* Get the bytes of the string */
    memcpy(retBuffer, buffer+ind, len);

    retBuffer[len] = '\0';   /* Add a two-byte Unicode NUL terminator */
    retBuffer[len+1] = '\0';

    /* Bump the index by the size of a descriptor and the Unicode string */
    *bufferIndex += DESCRIPTOR_LEN + len;

    return 0;

} /* moveUnicodeStringArg_1 */

/*
 * Function moveStringArg_1
 *
 * This function is a copy of the function moveStringArg present in Packet.c
 * Additional Boundary checks have been placed to ensure: 
 * - we are working or pointing within the packet size.
 * - Whether we get a decoded string length value len which is valid
 *
 * ToDo: instead of using STANDING_REQUEST_PACKET_SIZE we could use DEFAULT_REQUEST_PKT_SIZE 
 * Or pass the actual packet size value present in the packet i.e. variable actualRequestPacketSize. 
 *
 * @brief: copies len number of bytes from the buffer
 * starting at offset bufferIndex into retBuffer.
 * The value len is an int which is 4 bytes long.
 * which is also extarcted from the packet.
 *
 * @params1: pointer to the destination buffer
 * @params2: pointer to the source buffer
 * @params3: offset into the source buffer
 * @params4: sizeof destination buffer
 * @returns: 0 = not error, other value = error 
 *
 */
int moveStringArg_1(char * retBuffer, char * buffer, int * bufferIndex, int retBufSize)
{
    int len = 0;
    int ind = 0;

	if(*bufferIndex >= STANDING_REQUEST_PACKET_SIZE || *bufferIndex < 0 
		|| retBufSize >= STANDING_REQUEST_PACKET_SIZE)
	{
		printf("\n[moveStringArg_1], Detected invalid offset value in packet!\n");
		return -1;
	}

    /* Skip over the descriptor type and get the string length */
    ind = *bufferIndex + BYTE_LEN;
    len = getDescriptorLength(buffer, &ind);

	if(len >= retBufSize-1 || len < 0) /* need 1 byte for NULL char & length can't be -ve */
	{
		printf("\n[moveStringArg_1], Detected invalid param data value in packet!\n");
		return -1;
	}

    /* Get the bytes of the string */
    memcpy(retBuffer, buffer+ind, len);

    retBuffer[len] = '\0'; /* add NUL terminator */

    /* Bump the index by the size of a descriptor and the string */
    *bufferIndex += DESCRIPTOR_LEN + len;

    return 0;

} /* moveStringArg_1 */



/*
 * Procedure Validate_RequestPacket
 *
 * @brief
 * This function validates the Request Packet data
 * It verifies if the byte data in the packet are valid. 
 * It calls few utility functions to check if the data 
 * read from the buffer is within the limits of the packet size
 * 
 * This function also compares the RDMS Driver and Server version to
 * verify the compatibility. Else it returns error.
 *
 * ToDo: If we have a list of valid values for example for:
 * accessType, charSet etc.  
 *
 * @param1: takes pointer to the request packet.
 * @param2: task code from the same request packet 
 * Output: if the functions returns value other than zero 
 * the serverWorker thread would be shutdown.
 * @return: 0 = not error , other value = error 
 *
 */
int Validate_RequestPacket(char *requestPtr, int task_code)
{
	/*int reqHdrStatus;
    requestHeader reqHdr;*/
    char schema[MAX_CHARSET_LEN*2 +2]; /* Unicode string up to 30 characters followed by a double NUL. */
    char charSet[MAX_CHARSET_LEN+1];   /* String up to 30 characters followed by a NUL. Note: value may be truncated later. */
    char ncharSet[MAX_CHARSET_LEN+1];  /* String up to 30 characters followed by a NUL. Note: value may be truncated later.  */
	char user[CHARS_IN_USER_ID+1]; /* String up to CHARS_IN_USER_ID (12) characters followed by a NUL.*/
    char accessType[8];    /* Access types can be from 4 to 6 characters, followed by a NUL. */
    char recovery[16];     /* Recover options can be from 4 to 12 characters, followed by a NUL. */
    char version[MAX_SCHEMA_NAME_LEN+1]; /* String up to 30 characters followed by a NUL. */
    char locale[MAXIMAL_LOCALE_LEN+1]; /* Maximal String up to 400 characters (see RdmsConnection's definition) followed by a NUL. */
    char timeOut[16];      /* Timeout value, at most 2147483647, so up to 10 characters, followed by a NUL. */
    char varChar[12];      /* VarChar value can be from 7 to 10 characters, followed by a NUL. */
    char storageArea[MAX_STORAGEAREA_NAME_LEN+1]; /* String up to 70 characters followed by a NUL. */
    char clientLevel[4];   /* Driver (client) level is 4 bytes in length. */
    char driverBuildLevel[JDBC_DRIVER_BUILD_LEVEL_MAX_SIZE+1]; /* Build level strings maximum length, followed by a NUL. */
    char serverLevel[SERVER_JDBC_SERVER_LEVEL_SIZE];
    char serverBuildLevel[SERVER_BUILD_LEVEL_MAX_SIZE];
    char threadPrefix[8]; /* Thread Prefix can be from 0 to 5 characters followed by a NUL. */
	char skipGenerated[8]; /* Skip Generated can be from either "ON" or "OFF" followed by a NUL. */
	
	long long encryptedCaseInsensitive;  /* The 64 bit DES encrypted userid/password */
    long long encryptedCaseSensitive;    /* The 64 bit DES encrypted userid/password */
	set_value_type emptyStringProp;
	int autoCommitProp;
	int clientKeepAlive;
	int  ncharSetInt;
	int index = 0;
	int connId;
	int schemaLength;
	int status = 0;
	int retVal = 0;
	char validate_userid_password;       /* 0 (False)- validation of userid and password Not needed */
                                         /* 1 (True) - validation of userid and password is needed */


	/* not interested in header as its already validated before it gets here */
	skipRequestHeader(&index);
	/*printf("\n[Validate_RequestPacket], index=%d\n",index);*/
	
	if(task_code == USERID_PASSWORD_TASK)
	{
		/* The next three arguments are needed for handling the userid and password. */
		/* Get the userid from the request packet. */
		if(moveStringArg_1(user, requestPtr, &index, sizeof(user)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		printf("[Validate_RequestPacket] user=%s, index=%d\n",user,index);
		
		/* Get the case insensitive encrypted password from the request packet. */
		index += DESCRIPTOR_LEN;
		encryptedCaseInsensitive = get8BytesFromBytes(requestPtr, &index);
		/*printf("[Validate_RequestPacket] encryptedCaseInsensitive=%o, index=%d\n",encryptedCaseInsensitive,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		
		/* Get the case sensitive encrypted password from the request packet. */
		index += DESCRIPTOR_LEN;
		encryptedCaseSensitive = get8BytesFromBytes(requestPtr, &index);
		/*printf("[Validate_RequestPacket] encryptedCaseSensitive=%o, index=%d\n",encryptedCaseSensitive,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
	}
	else
	{
		/* Get the schema name descriptor type from the request packet. */
		if(moveUnicodeStringArg_1(schema, requestPtr, &index, sizeof(schema)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		schemaLength = strlenu(schema);
		/*printf("[Validate_RequestPacket], index=%d, schemaLength=%d\n",index, schemaLength);	*/
		
		/* Get the connection ID from the request packet. */
		connId = getIntArg(requestPtr, &index);
		/*printf("[Validate_RequestPacket] connId=%d, index=%d\n",connId,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;

		/* Get the charSet from the request packet. */
		if(moveStringArg_1(charSet, requestPtr, &index, sizeof(charSet)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] charSet=%s, index=%d\n",charSet,index);*/
		
		/* Get the ncharSet and ncharSetInt from the request packet. */
		if(moveStringArg_1(ncharSet, requestPtr, &index, sizeof(ncharSet)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] ncharSet=%s, index=%d\n",ncharSet,index);*/

		ncharSetInt = getIntArg(requestPtr, &index);
		/*printf("[Validate_RequestPacket] ncharSetInt=%d, index=%d\n",ncharSetInt,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		
		/* The next four arguments are needed for handling the userid and password. */
		/* Get the userid from the request packet. */
		if(moveStringArg_1(user, requestPtr, &index, sizeof(user)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] user=%s, index=%d\n",user,index);*/

		/* Get the case insensitive encrypted password from the request packet. */
		index += DESCRIPTOR_LEN;
		encryptedCaseInsensitive = get8BytesFromBytes(requestPtr, &index);
		/*printf("[Validate_RequestPacket] encryptedCaseInsensitive=%o, index=%d\n",encryptedCaseInsensitive,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		
		/* Get the case sensitive encrypted password from the request packet. */
		index += DESCRIPTOR_LEN;
		encryptedCaseSensitive = get8BytesFromBytes(requestPtr, &index);
		/*printf("[Validate_RequestPacket] encryptedCaseSensitive=%o, index=%d\n",encryptedCaseSensitive,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;

		/* Get the "validate userid and password now" indicator. */
		validate_userid_password = getBooleanArg(requestPtr, &index);
		/*printf("[Validate_RequestPacket] validate_userid_password=%d, index=%d\n",validate_userid_password,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;

		/* Get the access type (READ, UPDATE, ACCESS) from the request packet. */
		if(moveStringArg_1(accessType, requestPtr, &index, sizeof(accessType)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] accessType=%s, index=%d\n",accessType,index);*/

		/* Get the recovery option (deferred, quicklooks, commandlooks, none). */
		if(moveStringArg_1(recovery, requestPtr, &index, sizeof(recovery)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] recovery=%s, index=%d\n",recovery,index);*/
		
		/* Get the version name from the request packet. */
		if(moveStringArg_1(version, requestPtr, &index, sizeof(version)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] version=%s, index=%d\n",version,index);*/
		
		/* Get the locale from the request packet. */
		if(moveStringArg_1(locale, requestPtr, &index, sizeof(locale)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] locale=%s, index=%d\n",locale,index);*/
		
		/* Get the timeOut value from the request packet. */
		if(moveStringArg_1(timeOut, requestPtr, &index, sizeof(timeOut)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] timeOut=%s, index=%d\n",timeOut,index);*/
		
		/* Get the varChar value from the request packet. */
		if(moveStringArg_1(varChar, requestPtr, &index, sizeof(varChar)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] varChar=%s, index=%d\n",varChar,index);*/
		
		/* Get the storageArea value from the request packet. */
		if(moveStringArg_1(storageArea, requestPtr, &index, sizeof(storageArea)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] storageArea=%s, index=%d\n",storageArea,index);*/
		
		/* Get the Drivers build level value from the request packet. */
		if(moveStringArg_1(driverBuildLevel, requestPtr, &index, sizeof(driverBuildLevel)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] driverBuildLevel=%s, index=%d\n",driverBuildLevel,index);*/
		
		/* Get the clientLevel (Major version, Minor version, Feature, Platform) */
		if(moveByteArrayArg_1(clientLevel, requestPtr, &index, sizeof(clientLevel)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] index=%d, Client Level = %d.%d.%d.%d\n",
        index, clientLevel[0],clientLevel[1],clientLevel[2],clientLevel[3]);*/

		/* Get the JDBC Server's level and build level */
		getServerLevel(serverLevel, serverBuildLevel);
		/*printf("JDBC Server level(build):  %d.%d.%d.%d(%s)\n",
            serverLevel[0], serverLevel[1],
            serverLevel[2], serverLevel[3],
            serverBuildLevel);*/

		/*
		 * Check if the client's product level is within the server's allowed range.
		 *
		 * If the client's product level doesn't fit within the range, we can't proceed
		 * with begin thread task.
		 */
		if (validate_client_driver_level_compatibility(clientLevel, serverLevel) != 0)
		{
			printf("[Validate_RequestPacket] validate_client_driver_level_compatibility FAILED! Cannot process task_code=%d\n",task_code);
			return -5001;
		}

		/* Get the keepalive option (4=DEFAULT, 1=ON, 0=OFF). */
		clientKeepAlive = getIntArg(requestPtr, &index);
		/*printf("[Validate_RequestPacket] clientKeepAlive=%d, index=%d\n",clientKeepAlive,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		
		/* Get the EmptyString property (9=Return_error, 10=null_value, 11=single_space). */
		emptyStringProp = getIntArg(requestPtr, &index);
		/*printf("[Validate_RequestPacket] emptyStringProp=%d, index=%d\n",emptyStringProp,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		
		/* Get the initial autocommit value from the request packet(0= off, 1= on) */
		autoCommitProp = (int)getBooleanArg(requestPtr, &index);
		/*printf("[Validate_RequestPacket] autoCommitProp=%d, index=%d\n",autoCommitProp,index);*/
		if(index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		
		/* Get the skipGenerated value from the request packet. */
		if(moveStringArg_1(skipGenerated, requestPtr, &index, sizeof(skipGenerated)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] skipGenerated=%s, index=%d\n",skipGenerated,index);*/
		
		/* Get the threadPrefix value from the request packet. */
		if(moveStringArg_1(threadPrefix, requestPtr, &index, sizeof(threadPrefix)) || index >= STANDING_REQUEST_PACKET_SIZE)
			return -5001;
		/*printf("[Validate_RequestPacket] threadPrefix=%s, index=%d\n",threadPrefix,index);*/
	}

	return status;
}

/*
 * Procedure serverWorker
 *
 * This is the JDBC Server's Server-Worker main procedure.
 * Each Server Worker is an independent activity running a copy
 * of this serverWorker procedure.  Each Server worker handles a
 * specific JDBC Client for the life time of its connection to RDMS.
 *
 * Once the JDBC clients work is completed, the given instance of the
 * Server Worker (its activity) will add itself to a chain of free
 * (unassigned) server Workers waiting for activation by the Initial
 * Connection Listener when a new JDBC Client has occurred.
 *
 * There may be many copies of Server Workers in various states (e.g.
 * waiting for clients, processing a client, etc.).  Test and set
 * queuing is used to synchronize the various chains and queues involved.
 */
void serverWorker(int new_client_socket, int new_client_COMAPI_bdi, int new_client_COMAPI_IPv6,
                  int new_client_ICL_num, int new_client_IP_addr1, long long new_client_IP_addr2,
                  long long new_client_IP_addr3, int new_client_network_interface){

    int activity_id;
    int error_status;
    int client_socket;
    v4v6addr_type client_IP_addr;
    int buffer_count;
    int buffer_length;
    int actualResponsePacketSize;
    int actualRequestPacketSize;
    char *requestPacketPtr; /* contains a pointer to the request packet */
    char *readToPtr;
    char *qbankPtr;
    int task_status_value;
    int task_code;
    char digits[ITOA_BUFFERSIZE];
    char digits2[ITOA_BUFFERSIZE];
    int index;
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    int message_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;
    char * msg2;

    workerDescriptionEntry *wdePtr;

    tsq_cell *firstFree_WdeTSptr;
    tsq_cell *firstAssigned_WdeTSptr;
    tsq_cell *serverWorkerTSptr;

    int COMAPI_option_name;
    int COMAPI_option_value;
    int COMAPI_option_length;

    int setClientTimeoutNow;
    int processNewClient;
    int workingOnaClient;

    /*
     * Allocate large enough space to hold the constructed client/server
     * instance string and anything else that may be added after it.
     */
    char clientsServerInstanceIdent[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char serversServerInstanceIdent[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    /*
     * The VERY FIRST THING to do is provide the dummy WDE pointer reference for
     * the Server Worker activities local WDE entry so we can utilize it for
     * messages in case there is an error in this activity of the JDBC Server.
     *
     * This allows us to immediately utilize the localized message handling code
     * in case there is an error. Also, this will be used in handling Network
     * API errors/messages.
     */
    setupActivityLocalWDE(SERVER_WORKER, new_client_COMAPI_bdi, new_client_COMAPI_IPv6,
                          new_client_ICL_num);
    /* addServerLogEntry(SERVER_STDOUT,"Done setting up act_wdePtr to activity WDE in SW"); */

    /* Register signals for this server worker activity */
#if USE_SIGNAL_HANDLERS
    regSignals(&serverWorkerSignalHandler);
#endif

    /* Test if we should log procedure entry */
    /* if (sgd.debug==BOOL_TRUE){
      addServerLogEntry(SERVER_STDOUT,"Entering serverWorker");
    }*/

    /* Store the RSA BDI for the app group for later RSA calls.
       Do this via an RSA call.*/
    RSA$SETBDI2(sgd.rsaBdi);

    /*
     * Save the new client socket and IP addresses
     * for later local code usage. Note: The COMAPI
     * bdi, ICL number, and network interface will
     * automatically be taken
     * from the WDE if needed, so a local code copy
     * is not needed.
     * The new_client_IP_addr parameters represent the 5 word
     * IP address that is passed by value as 3 separate parameters
     * (1 int and 2 long long values). The full structure could not
     * be passed by address because it is an activity local address
     * in another activity.
     */
    client_socket = new_client_socket;
    client_IP_addr.family = new_client_IP_addr1 >> 18;
    client_IP_addr.zone = new_client_IP_addr1 & 0777777;
    client_IP_addr.v4v6.v6_dw[0] = new_client_IP_addr2;
    client_IP_addr.v4v6.v6_dw[1] = new_client_IP_addr3;

    /*
     * Inherit the Client socket now, since the ICL bequeathed it before
     * starting this new Server Worker task and we may need to use it if
     * we cannot get a WDE entry for this Server Worker.
     *
     * A Server Worker that is being reused will inherit its client socket
     * just after being awoke by the ICL with a new JDBC Client.
     *
     * We must use the  Bequeath/Inherit mechanism (i.e., we can't use
     * shared sockets) since we do two TCP_Receives to get the request
     * packet. Doing this under shared sockets causes COMAPI to preform
     * poorly (it copies the request packet information in/out of queue
     * banks, etc.). Test the error status right away since we cannot
     * do any processing (including sending an error packet to the
     * client) if we couldn't inherit the client socket.
     */
     error_status = Inherit_Socket(client_socket);
     if (error_status != 0 ){
           /*
            * We got an error trying to inherit the socket from COMAPI.
            * The COMAPI error is already in the Log file. The socket
            * will error off automatically, so continue shutting down
            * this JDBC Server Worker instance.
            *
            * Indicate we shut down the Server Worker activity; it's
            * malloc space also goes away. Lock the assigned chain
            * T/S cell to do this safely. (Other SW's may be shutting
            * down normally also.)
            */
           test_set(&(sgd.WdeReAllocTS));
           sgd.numberOfShutdownServerWorkers++;
           ts_clear_act(&(sgd.WdeReAllocTS));

            exit(0); /* stop Server Worker activity - can't continue. */
    }

    /* If debug, add a new log entry for accepting the new JDBC Client. */
    if (sgd.debug){
	  tracef("accepting a new JDBC Client\n");
      tsk_id(&activity_id);

      getMessageWithoutNumberForWorker(SERVER_NEW_SW,
          itoa(activity_id, digits, 10), NULL, SERVER_LOGS_DEBUG);
              /* 5361 New JDBC Server Worker {0} initializing
                 for new JDBC Client */

      getMessageWithoutNumberForWorker(SERVER_CLIENT_ACCEPTED,
          itoa(client_socket, digits, 10),
          (char *)inet_ntop((int)client_IP_addr.family, (const unsigned char *)&client_IP_addr.v4v6,
                             digits2, ITOA_BUFFERSIZE),
          SERVER_LOGS_DEBUG);
              /* 5362 Client accepted from client socket {0}
                 at IP address {1} */
    }

    /*
     * Get, Set, and link in a new WDE for this Server Worker.
     *
     * A returned WDE pointer of NULL means we failed to acquire
     * a new WDE. In that case, terminate the Server Worker now after
     * logging the activity shutdown.
     *
     * Note: createServerWorkerWDE() will reset act_wdePtr to point to
     * the new WDE entry that was allocated, or if a new WDE could not
     * be allocated ( wdePtr was returned NULL) it will still point to
     * the minimally initilaized activity local WDE (anActivityLocalWDE).
     */
    wdePtr=createServerWorkerWDE(client_socket, new_client_COMAPI_bdi, new_client_COMAPI_IPv6,
                                 new_client_ICL_num, client_IP_addr,
                                 new_client_network_interface);
    if (wdePtr == NULL){
       /*
        * Unable to acquire a WDE, so we can't support the JDBC Client.
        * Send the JDBC Client an error Response Packet and then
        * terminate the Server Worker and close the network connection.
        * Notice, there is no WDE so the assigned Server Worker's WDE chain
        * was not affected, hence this Server Worker only has one resource
        * assigned to it that must be handled before this Server Worker
        * activity can go away: a JDBC Client socket.
        */

       /*
        * A Server Log Entry was produced so send an error response packet
        * and close the client socket down.  Then shut down this activity
        * (this procedure call does not return).
        */
        msg2 = getLocalizedMessage(SERVER_CANT_ACCEPT_CONNECTION, NULL, NULL,
                                   0, message);
                /* 5363 JDBC SERVER cannot accept this connection since it
                   has reached its configured resource limit for concurrent
                   JDBC Clients */
        qbankPtr = genErrorResponse(
            JDBC_SERVER_ERROR_TYPE, SERVER_CANT_ACCEPT_CONNECTION, 0, msg2,
            FALSE);

        /* Send the error response packet */
        index=RES_LENGTH_OFFSET;
        actualResponsePacketSize = getIntFromBytes(qbankPtr,&index);
        error_status=Send_DataQB(client_socket, qbankPtr, actualResponsePacketSize);
            if (error_status != 0 ){
               /*
                * We could not send the data and return the QB that was used
                * for the request packet.  The COMAPI error is already in the
                * Log file.
                * This should not happen, so just proceed to the shutdown of the
                * Server Worker activity.  We will review and change our
                * approach for this situation at a later time.
                */
                getLocalizedMessage(SERVER_CANT_SEND_ERROR_RESPONSE_PKT,
                    itoa_IP(client_socket, digits, 10, client_IP_addr, NULL, NULL),
                    itoa(error_status, digits2, 10), SERVER_LOGS, message);
                        /* 5364 Can't send the error response packet data for JDBC Client
                           on client socket {0}, error status {1}, shutting client down */
            }

        /* Now shut the Server Worker's client socket */
        error_status =Close_Socket(client_socket);
        if (error_status != 0 ){
           /*
            * We got an error trying to close the socket from COMAPI.
            * The COMAPI error is already in the Log file. The socket
            * will error off automatically, so continue shutting down
            * this JDBC Server Worker instance.
            */
            getLocalizedMessage(SERVER_CANT_CLOSE_SOCKET_MAX_ACT,
                itoa_IP(client_socket, digits, 10, client_IP_addr, NULL, NULL),
                itoa(activity_id, digits2, 10), SERVER_LOGS, message);
                    /* 5366 Can't close client socket {0} in activity {1}
                       when we hit max_activities limit */
        }

        /*
         * Indicate we shut down the Server Worker activity; it's
         * malloc space also goes away. Lock the assigned chain
         * T/S cell to do this safely. (Other SW's may be shutting
         * down normally also.)
         */
        test_set(&(sgd.WdeReAllocTS));
        sgd.numberOfShutdownServerWorkers++;
        ts_clear_act(&(sgd.WdeReAllocTS));

        exit(0); /* stop Server Worker activity */
       }

    /*
     * If we are here, we did get a WDE for this Server Worker.
     *
     * We have a valid initialized WDE linked into the assigned Server Worker
     * WDE chain, so indicate that this Server Worker is done initializing
     * and is waiting to handle its first JDBC Clients request.
     */
    wdePtr->serverWorkerState=CLIENT_ASSIGNED;

    /*
     * The first free Server Worker T/S cell (firstFree_WdeTS) and the
     * first assigned Server Worker T/S cell (firstAssigned_WdeTS)in the
     * SGD and this Server Workers T/S cell (serverWorkerTS) in its WDE
     * do not move in memory, so for performance reasons lets get pointers
     * that point directly to those T/S cells for use later in the Server
     * Worker code.
     */
  /* printf("got here 1 in serverWorker\n");   */
     firstFree_WdeTSptr=&(sgd.firstFree_WdeTS);
     firstAssigned_WdeTSptr=&(sgd.firstAssigned_WdeTS);
     serverWorkerTSptr=&(wdePtr->serverWorkerTS);

    /*
     * Now enter the main while loop of the Server Worker.  Each
     * iteration through this loop handles one JDBC CLient and all of
     * the Request/Response packet dialogue between that JDBC Client
     * and this Server Worker.
     *
     * At initial entry into this while loop, the Server Worker is awakened
     * with an assigned JDBC Client given by the ICL.  In the bottom of
     * this while loop, when the Server Worker has completed all this JDBC
     * Client's requests (e.g. RDMS thread has been closed), the Server
     * Worker will enter a sleep state waiting for the ICL to awaken it
     * with a new JDBC Client at which time a new loop is done.
     *
     * This main while loop remains active until one of four conditions
     * occurs that will stop the while loop:
     *
     * 1 ) This Server Worker is told to shut down while inside this loop.
     *     If serverWorkerShutdownState == WORKER_SHUTDOWN_IMMEDIATELY,
     *     occurs then within the inner while loop (see below), the inner
     *     loop will terminate and control passes to routine
     *     serverWorkerShutdown() which will shut down the Server Worker.
     *
     * 2)  This Server Worker is told to shut down while inside the inner while
     *     loop and serverWorkerShutdownState == WORKER_SHUTDOWN_GRACEFULLY.
     *     Then the currently active JDBC Client is allowed to complete its
     *     connection usage of the Server Worker and C-Interface library.
     *     When the JDBC Client has finished (connection is closed), then the
     *     Server Worker will shut itself down.
     *
     * 3)  This Server Worker is told to shut down just after completing
     *     processing of the JDBC Client (inner loop completed).  In this case,
     *     the Server Worker will shut down without processing another JDBC
     *     Client, even if it was woken up with a new JDBC Client (which will
     *     be closed down before working on it).
     *
     * 4 ) The loop control is via the WDE value serverWorkerShutdownState
     *     being set to WORKER_ACTIVE. This variable is set before
     *     the while loop. The loop control variable will not be set to another
     *     value by normal operation.  If it's changed, then the loop ends and
     *     a call to serverWorkerShutdown() will be made.

     *
     * Note:
     * It is possible that the JDBC Server has entered a WORKER_SHUTDOWN_GRACEFULLY
     * state just prior to this code.  In this case, we assume that this JDBC
     * Client got in under the wire and we will process this JDBC Client.
     * Notice that when it is done with the database connection, the Server Worker
     * will then shut down. If the Server Worker has entered a
     * WORKER_SHUTDOWN_IMMEDIATELY state, this will be recognized and processed
     * immediately ( Client will not get first request packet processed).
     *
     */

     /*
      * First, test if we have been told to shut down the Server Worker.  At
      * this point, we can easily avoid beginning the new JDBC Client. Routine
      * serverWorkerShutDown() does not return control.  If not a shutdown,
      * execution will continue normally.
      */
     testAndCall_SW_Shutdown_IfNeeded(wdePtr, NEW_WORKER_JUST_INITIALIZED,client_socket);

     /* Not shutting down, so set up for a loop over all incoming JDBC Clients */
     processNewClient=BOOL_TRUE;
     while (processNewClient == BOOL_TRUE){

        /*
         * Set flag so we test the client socket receive timeout after the first
         * request packet is processed.  The first packet has the connection
         * properties that could change the timeout. After the response packet
         * is sent, test if the JDBC client has chosen a different socket receive
         *  timeout then the Server's.  If so, then change the client socket value
         * after their first response packet is sent (this saves the COMAPI cost since
         * it will overlap the network transfer time).
         */
        setClientTimeoutNow=BOOL_TRUE;

        /*
         * Now we need to loop, receiving request packets for the JDBC
         * Client, processing the request and producing a response packet
         * and returning the response packet to the JDBC Client, until
         * the shut down flag stops this loop.
         *
         * This looping will continue until one of two conditions is met,
         * either:
         *
         * 1) The last request packet processed indicates that we are done
         *    with this JDBC Client (e.g. an END THREAD task is indicated in
         *    the request packet) which sets the WDE serverWorkerState to
         *    FREE_NO_CLIENT_ASSIGNED and also sets the flag workingOnaClient
         *    to BOOL_FALSE (0).
         *
         * 2) The JDBC Server Worker's serverWorkerShutdownState is set to
         *    either WORKER_SHUTDOWN_IMMEDIATELY or WORKER_SHUTDOWN_GRACEFULLY.
         *    After finishing the requested task operation and sending the
         *    client the resulting response packet, the Server Worker
         *    checks the flag, and calls serverWorkerShutDown() to shut
         *    the Server Worker.
         *
         * If the loop ends because of case 1, a normal completion of the
         * JDBC CLient, then this Server Worker will try to place itself on
         * the free chain and place itself in a sleep state waiting for the
         * ICL to assign it a new JDBC Client. This is assuming there is no
         * shutdown indicator set.
         *
         */
         workingOnaClient=BOOL_TRUE;
         while (workingOnaClient == BOOL_TRUE){
            /*
             * We are still processing this client.
             *
             * Now read the request packet.  This is done by two calls
             * to Receive_Data.  This is necessary to get the request packet
             * into one continuous piece of memory that is sufficiently large
             * to hold the entire request packet.
             *
             * (Note: If the transmission of keepAlive request packet is
             * abnormally terminated, i.e. program terminates without close()'ing
             * the connection, there could be only a partial request packet
             * so note that this only shuts down the individual Server Worker
             * thread when a bad (incomplete) request packet is received.)
             *
             * The request packet is usually read into a standing buffer that
             * is set up to be sufficient for most of the request packets that
             * will be received. This will minimize the amount of memory being
             * being used so there will be little need to allocate/release
             * request packets.  However, if the length indicated is too large
             * for the standing buffer, we will allocate a response buffer as
             * large as needed.
             *
             * First, receive the  length and id of the next request packet from
             * the JDBC Client using COMAPI.
             */
            requestPacketPtr=wdePtr->standingRequestPacketPtr; /* use standing buffer */
            buffer_count=-1;
            buffer_length=INITIAL_REQUESTPACKET_READSIZE; /* get id and packet length */

            error_status=Receive_Data(client_socket, requestPacketPtr
                                      , buffer_length,&buffer_count);

            /*
             * Test if we have been told to immediately shut down the Server
             * Worker. If its an immediate shutdown, stop the Server
             * Worker right away. If it is a graceful shutdown, then we
             * can still process this request packet data.
             *
             * Note: The Server Worker may have been notified by a passed
             * event. This is done only for an immediate shutdown, in which
             * case there is no data to process anyway.
             */
            testAndCall_SW_ShutdownImmediately_IfNeeded(wdePtr
                                       ,AFTER_RECEIVING_DATA, client_socket);

            /* Test for closed connection COMAPI returned status values. */
            if (error_status == LOST_CLIENT){
               /* client has closed connection, so skip rest of the checks */
               if (debugInternal) {
                    tracef("In serverWorker(): Receive_Data() got LOST_CLIENT status (%d)\n", error_status);
               }
               workingOnaClient = BOOL_FALSE;
               wdePtr->workingOnaClient = BOOL_FALSE;
               /* We also need to shut down any RDMS activity if a thread is still open. */
               closeRdmsConnection(1);  /* Close and cleanup */
               goto NOTWOC;
            }

            /* Test for any other COMAPI returned status values. */
            if (( error_status != 0 )||
                (buffer_count != INITIAL_REQUESTPACKET_READSIZE)){
               /*
                * We could not receive the desired amount of data from the
                * JDBC Client's socket. For errors other than COMAPI error
                * SETIMEDOUT (socket timeout), the COMAPI error is already
                * in the Log file. We will log the SETIMEDOUT error here.
                *
                * In any case, an error situation should not happen, so it
                * is best to just shut down this JDBC Client connection. In
                * later code < add code here xxxx > we can add more sophistication
                * to our error handling/recovery here.
                */
                getLocalizedMessage(SERVER_CANT_RECEIVE_SOCKET_DATA,
                    itoa_IP(client_socket, digits, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
                    itoa(error_status, digits2, 10), TO_SERVER_LOGFILE, message);
                        /* 5367 Can't receive data on JDBC Client's socket {0},
                           error status = {1}, shutting client down */
               serverWorkerShutDown(wdePtr,CANT_RECEIVE_REQUEST_PACKET,client_socket);
            }

            /*
             * Now test the one byte request packet id -if it is wrong, we
             * cannot trust the request packet length value.
             * If the id is wrong, treat that as a fatal JDBC Client error,
             * log it and shut down this Server Worker ( this causes the
             * JDBC Client to get a communication failure error at this
             * time - we may want to send a nice response packet with a
             * fatal error message instead).
             * Notice that serverWorkerShutDown does not return since the
             * Server Worker activity is terminated.
             */

/* printf("Request Packet Header:\n"); */
/* hexdump(requestPacketPtr,8); */
/* printf("Request Packet validation byte is=%i\n",requestPacketPtr[REQ_ID_OFFSET]); */
            if (requestPacketPtr[REQ_ID_OFFSET] != REQUEST_VALIDATION ){
                /* bad request Packet id, shut down this JDBC Client */
                getLocalizedMessage(SERVER_BAD_REQUEST_PKT_ID,
                    itoa(requestPacketPtr[REQ_ID_OFFSET], digits, 10),
                    itoa_IP(client_socket, digits2, 10, wdePtr->client_IP_addr,
                    wdePtr->client_Userid, wdePtr->threadName), TO_SERVER_LOGFILE, message);
                        /* 5368 Bad Request Packet id {0} in JDBC Client's
                           Request Packet on client socket {1},
                           shutting client down */

                /*
                 *  Assume the validation id mismatch is because of a JDBC Driver/Server
                 * level mismatch. In this case, we are looking at the first request packet
                 * sent from the client, the begin thread request packet.  If this is the
                 * case, the request packet will be significantly smaller than 5000 bytes
                 * in length.  If this is true, then let's assume it is a JDBC Driver of
                 * a different product level and let's try to tell that JDBC Driver about
                 * the client/server level mismatch.
                 *
                 * So let's do two things before shutting this server worker down:
                 *
                 * 1) We need to recieve all of the incoming request packet bytes (otherwise
                 * the closing of the socket by the Server worker activity shutdown will
                 * cause the JDBC client to get an I/O error for a lost socket and not the
                 * nice client/server level mismatch SQLException response packet).
                 * 2)Before shutting down this Server Worker, attempt to send back
                 * to the JDBC client an error response packet that informs the client
                 * that probably a JDBC client/server level mismatch has occurred.  That
                 * way a JDBC Driver can return a reasonable SQL Exception back to its
                 * JDBC client.
                 *
                 *
                 * So, is the request packet length less than the minimum size (5 byte header),
                 * or greater than 5000 bytes? If so, just set error flag for now.
                 */
                index=REQ_LENGTH_OFFSET ;
                actualRequestPacketSize = getIntFromBytes(requestPacketPtr
                                                      ,&index);
                if ((actualRequestPacketSize > 5000) ||
                    (actualRequestPacketSize < INITIAL_REQUESTPACKET_READSIZE)){
                    /* indicate the error situation so we will log error and shutdown SW */
                    error_status=-5001; /* use a value that's an indicator and will not look like a bad buffer count value */
                }
                else {
                    /* OK, receive it into a malloc'ed buffer to be released afterward */
                    readToPtr = Tmalloc(4, "ServerWorker.c", actualRequestPacketSize);

                    /*
                     * Now read of the rest of the request packet (set buffer_length to the
                     * size of the remaining bytes left).
                     */
                    buffer_count=-1;
                    buffer_length=actualRequestPacketSize-INITIAL_REQUESTPACKET_READSIZE;
                    error_status=Receive_Data(client_socket, readToPtr,
                                              buffer_length,&buffer_count);

                    /* Did we get the desired number of bytes? */
                    if (buffer_count != buffer_length){
                        /*
                         * Bad number of returned bytes.  Signal the error via
                         * error_status by using the negative of the buffer_count.
                         */
                        error_status = -buffer_length;
                    }
                    /* Always release the buffer since we don't need the data. */
                    Tfree(5, "ServerWorker.c", readToPtr);
                }

                /* Did we received the rest of the request packet? Generate the error response, if so. */
                if (error_status == 0 ){
                    /*
                     * Generate a special response packet indicating the mismatch
                     * client/server level.
                     */
                    qbankPtr = generate_ID_level_mismatch_response_packet(requestPacketPtr[REQ_ID_OFFSET]);

                    /*
                     * Try to send out the error response.  We will not continue
                     * to comunicate with the JDBC client (if it is one). We will
                     * be shutting down the Server worker and the client socket,
                     * so we don't need to check the error_status.
                     */
                    index=RES_LENGTH_OFFSET;
                    actualResponsePacketSize = getIntFromBytes(qbankPtr,&index);
                    error_status=Send_DataQB(client_socket, qbankPtr, actualResponsePacketSize);
                }

                /*
                 * Was there an error in the recieving of the request packet data
                 * or in the sending of the mismatched client/server error
                 * response packet? Upon any of these errors, forget the mismatch
                 * level notification and log the error and quit.
                 */
                if (error_status != 0 ){
                   /*
                    * For a variety of reasons, we could/can not send a client/server
                    * mismatch response packet. Any COMAPI error or other information
                    * message is already in the Log file, so just proceed to the shutdown
                    * of the Server Worker activity. If it was a JDBC client, they will
                    * have to make due with a possible I/O error indication on their side.
                    */
                    getLocalizedMessage(SERVER_CANT_SEND_ERROR_RESPONSE_PKT,
                        itoa_IP(client_socket, digits, 10, client_IP_addr, NULL, NULL),
                        itoa(error_status, digits2, 10), SERVER_LOGS, message);
                            /* 5364 Can't send the error response packet data for JDBC Client
                               on client socket {0}, error status {1}, shutting client down */
                }

                /* Now shut down the Server Worker */
                serverWorkerShutDown(wdePtr,DETECTED_BAD_REQUEST_PACKET,client_socket);
            }

            /*
             * Now test the length of the actual request packet coming in.
             * Can we hold it in the standing buffer? If so use it, if not
             * then allocate a buffer of appropriate size.  Record which
             * buffer has the request packet int he requestPacketPtr.
             */
            index=REQ_LENGTH_OFFSET ;
            actualRequestPacketSize = getIntFromBytes(requestPacketPtr
                                                      ,&index);
			printf("\n[ServerWorker.c] activity id= %012o, Request Packet length= %d\n", 
			wdePtr->serverWorkerActivityID, actualRequestPacketSize);

            if(actualRequestPacketSize > STANDING_REQUEST_PACKET_SIZE){
                /*
                 * requestPacketPtr points to the standing buffer
                 * which is too small, allocate one of required
                 * size and use it. Transfer the stuff already read.
                 * (Is it faster to do a few assignment statements
                 * instead of a memcpy?).
                 */
                requestPacketPtr = Tmalloc(6, "ServerWorker.c", actualRequestPacketSize);
                memcpy(requestPacketPtr,wdePtr->standingRequestPacketPtr
                       ,INITIAL_REQUESTPACKET_READSIZE);
            }

            /*
             * Now complete the read of the rest of the request packet (set
             * buffer_length to the size of the remaining bytes left).
             *
             */
            buffer_count=-1;
            buffer_length=actualRequestPacketSize-INITIAL_REQUESTPACKET_READSIZE;
            readToPtr=requestPacketPtr+INITIAL_REQUESTPACKET_READSIZE;
            error_status=Receive_Data(client_socket, readToPtr
                                      ,buffer_length,&buffer_count);
            /* tracef("\n[ServerWorker.c] error_status=%d, buffer_length=%d, buffer_count=%d\n",error_status,buffer_length, buffer_count);
			printf("Request Packet's all of the data:\n");
			hexdump(requestPacketPtr,actualRequestPacketSize, 80); */

            /*
             * Test if we have been told to immediately shut down the Server
             * Worker. If its an immediate shutdown, stop the Server
             * Worker right away. If it is a graceful shutdown, then we
             * can still process this request packet data.
             *
             * Note: The Server Worker may have been notified by a passed
             * event. This is done only for an immediate shutdown, in which
             * case there is no data to process anyway.
             */
            testAndCall_SW_ShutdownImmediately_IfNeeded(wdePtr
                                       ,AFTER_RECEIVING_DATA, client_socket);

            /* Test for any other COMAPI returned status values. */
            if ((error_status != 0 ) ||
                (buffer_count != buffer_length)){
               /*
                * We could not receive the desired amount of data from the
                * JDBC Client's socket.
                * The COMAPI error is already in the Log file. This should
                * not happen, so it is best to shut down this JDBC Server Worker
                * so we can correct this problem.
                */
                getLocalizedMessage(SERVER_CANT_RECEIVE_REQUEST_PKT,
                    itoa_IP(client_socket, digits, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
                    itoa(error_status, digits2, 10), SERVER_LOGS, message);
                        /* 5369 Can't receive rest of JDBC Client's Request
                           Packet on client socket {0},
                           error status = {1}, shutting client down */
               serverWorkerShutDown(wdePtr,DETECTED_BAD_REQUEST_PACKET,client_socket);
            }

            /*
             * We now have the Request packet in a memory buffer.  So
             * lets process it.
             *
             * Routine processClientTask will process the task and
             * return an adjusted Q-bank pointer(qbankPtr) which points
             * to the part of the Q-Bank containing the response
             * packet for the task completed. Also, it will set workingOnaClient
             * to BOOL_FALSE if the completed task indicates we are done
             * processing this JDBC Client (e.g. End Thread task).
             *
             * In either case, there is a response packet to return.
             * Also, the task may have indicated its done with the
             * JDBC Client. So set workingOnaClient to the value of
             * workingOnaClient in the WDE so the processing loop
             * can end correctly.
             */
  /*   printf("In ServerWorker before ProcessClientTask, activity id= %012o,workingOnaClient= %i\n",wdePtr->serverWorkerActivityID,workingOnaClient);
   */
            /*
             * Get the task code of the request packet before processing the task,
             * since we do not know if the processing task routine will affect the
             * packet.  It may be traced AFTER the Response packet is sent (so we
             * know it was/is on its way to client.
             * Remember that processClientTask may set the debugXXXX flags, etc. so we
             * must do any debugging after the processClientTask and before the next
             * request packet is received.
             */
            index = REQ_TASK_CODE_OFFSET;
            task_code = getShortFromBytes(requestPacketPtr,&index);
			/* printf("[serverWorker] task_code=[%s]\n", taskCodeString(task_code)); */
			
            /* Before processing, verify that we have a correct match between the
             * client and the Server by verifying the Server instance identification
             * information between that provided in the request packet and the
             * information in the Server's SGD. This test is not done on a
             * USERID_PASSWORD_TASK or BEGIN_THREAD_TASK's since the client
             * information is not set until the BEGIN_THREAD_TASK has been
             * processed.
             */
            if (!(task_code == USERID_PASSWORD_TASK || task_code == BEGIN_THREAD_TASK) &&
                (testServerInstanceInfo(requestPacketPtr + REQ_SERVERINSTANCE_IDENT_OFFSET))){
                    /*
                     * Client and Server have mismatch on server instance identification
                     * values.  Generate an error log entry to place in JDBC$LOG. This situation
                     * should not happen, so it is best to shut down this JDBC Server Worker
                     * so we can correct this problem. (Note: Use the DETECTED_BAD_REQUEST_PACKET
                     * flag, so serverWorkerShutDown() shuts down in the same manner it does
                     * with a bad request packet - in a way a server instance identity mismatch
                     * is a reflection of a bad request packet getting to the Server worker.)
                     */
                    displayableServerInstanceIdent(clientsServerInstanceIdent, requestPacketPtr + REQ_SERVERINSTANCE_IDENT_OFFSET, BOOL_FALSE);
                    displayableServerInstanceIdent(serversServerInstanceIdent, NULL, BOOL_FALSE);

                    /* Add the required ", on a <taskcode name> (taskcode)" clause at end of Server's instance identification. */
                    sprintf(message, ", on a %s (%d)", taskCodeString(task_code), task_code);
                    strcat(serversServerInstanceIdent, message);

                    msg2 = getLocalizedMessage(JDBCSERVER_SERVER_IDENTIFICATION_MISMATCH, clientsServerInstanceIdent,
                                               serversServerInstanceIdent, SERVER_LOGS, message);
                            /* 5032 Server instance identity mismatch: client driver
                               indicates {0}, server indicates {1} */

                    /* Generate a error response packet containing the Client and Server mismatch. */
                    qbankPtr = genErrorResponse(JDBC_SERVER_ERROR_TYPE, JDBCSERVER_SERVER_IDENTIFICATION_MISMATCH,
                                                0, msg2, BOOL_FALSE);
                    error_status = JDBCSERVER_SERVER_IDENTIFICATION_MISMATCH;

            } else {
					/*
					 * A USERID_PASSWORD_TASK or BEGIN_THREAD_TASK, or Client and
					 * Server's instance identification checked, so process it.
					 *
					 * Check if the packet has valid data before trusting/processing it.
					 * Validate_RequestPacket is needed for USERID_PASSWORD_TASK and BEGIN_THREAD_TASK
					 * Server's instance identification testServerInstanceInfo() should take
                     * care of the other tasks.
					 */
					if (task_code == USERID_PASSWORD_TASK || task_code == BEGIN_THREAD_TASK)
					{
						error_status = Validate_RequestPacket(requestPacketPtr, task_code);				
						if(error_status != 0)
						{
							printf("In ServerWorker Validate_RequestPacket FAILED!, activity id= %012o, workingOnaClient= %i will be shutdown\n",wdePtr->serverWorkerActivityID,workingOnaClient);
							/* setting workingOnaClient to FALSE to stop working on this client */
							workingOnaClient = BOOL_FALSE;
							wdePtr->workingOnaClient = BOOL_FALSE;
						   /*
							* temp handling with some error log for McAfee bad request packet.
							*/
							getLocalizedMessage(SERVER_BAD_REQUEST_PKT_ID,
							itoa_IP(client_socket, digits, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
							itoa(error_status, digits2, 10), SERVER_LOGS, message);
							/* 5368 Bad Request Packet id {0} on client socket {1}, shutting client down. */
							/* Shut down the Server Worker */
							serverWorkerShutDown(wdePtr,DETECTED_BAD_REQUEST_PACKET,client_socket);
						}
					} 
				    error_status=processClientTask(wdePtr, requestPacketPtr, &qbankPtr);
            }


            /* Get the wde's indicator telling us if we are done with this client */
            workingOnaClient=wdePtr->workingOnaClient;

  /*   printf("In ServerWorker after ProcessClientTask, activity id= %012o, workingOnaClient= %i\n",wdePtr->serverWorkerActivityID,workingOnaClient);
   */

            /*
             * Done with the individual request.  Now send the response
             * packet in the Q-bank back to the JDBC Client. Get the actual size
             * of the response packet to pass in as a parameter to Send_DataQB.
             */
            index=RES_LENGTH_OFFSET;
            actualResponsePacketSize = getIntFromBytes(qbankPtr,&index);

            /* printf("In ServerWorker(), sending response packet of %d bytes, for task %s(%d)\n",
                      actualResponsePacketSize, taskCodeString(task_code), task_code); */
            /* hexdump(qbankPtr,60);  */

            /*
             * If debugInternal, get the task status of the
             * response packet before sending it.  It will be traced AFTER the
             * Response packet is sent (so we know it was/is on its way to client.
             */
            if (debugInternal && (wdePtr->clientTraceFile != NULL)){
            index = RES_TASK_STATUS_OFFSET;
            task_status_value = getByteFromBytes(qbankPtr,&index);
            }

            /* Send response packet back to client. */
            error_status=Send_DataQB(client_socket, qbankPtr, actualResponsePacketSize);

            /*
             * If debugInternal, then test the task status of the just
             * sent response packet and if negative, log the fact the response was
             * sent. This will produce a trace line as long as there is an open
             * client trace file.
             */
            if (debugInternal && (wdePtr->clientTraceFile != NULL)){
              if (task_status_value < 0){
              tracef("Response packet for task %s id returning task status error = %d\n",
                     taskCodeString(task_code), task_status_value);
              }
            }

            if (error_status != 0 ){
               /*
                * We could not send the data and return the QB that was used
                * for the request packet.  The COMAPI error is already in the
                * Log file.
                * This should not happen, so it is best to shut down the
                * Server Worker activity.  We will review and change our
                * approach for this situation at a later time.
                */
                getLocalizedMessage(SERVER_CANT_SEND_RESPONSE_PKT,
                    itoa_IP(client_socket, digits, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
                    itoa(error_status, digits2, 10), SERVER_LOGS, message);
                        /* 5370 Can't send response packet data for JDBC Client on
                           client socket {0}, error status {1}, shutting client down*/
               serverWorkerShutDown(wdePtr,CANT_SEND_RESPONSE_PACKET,client_socket);
            }

            /*
             * Now, on the first request packet/response packet pair only,
             * check if we if the client has a different receive
             * timeout value specified than the client socket got from the ICL
             * when it was first created (at TCP_ACCEPT time it gets the server
             * socket value as its initial default value). We do this test now
             * since the COMAPI command, if done, can overlap the network transmit.
             * Remember that the first response task - Begin Thread - can provide a new
             * value for this client to use that may be different.  If its the same
             * value, then there is no need to do the option setting.
             */
            if (setClientTimeoutNow){
                if (wdePtr->client_receive_timeout != sgd.server_receive_timeout){
                    /* Not the same, so set the recieve timeout on the client socket */
                    COMAPI_option_name=COMAPI_RECEIVE_OPT;
                    COMAPI_option_value=wdePtr->client_receive_timeout;
                    COMAPI_option_length=4;
                    error_status=Set_Socket_Options( client_socket
                                                   ,&COMAPI_option_name
                                                   ,&COMAPI_option_value
                                                   ,&COMAPI_option_length );
                    if (error_status !=0){
                        /*
                         * COMAPI error message already logged in the LOG file.
                         * Add a log entry indicating that we couldn't set the socket
                         * options. Then continue client processing using the receive timeout
                         * that is in effect (either the server's or the client's).
                         */
                        getLocalizedMessage(
                            SERVER_CANT_SET_RECEIVE_TIMEOUT_OPTION,
                            itoa(COMAPI_option_value, digits, 10),
                            itoa_IP(client_socket, digits2, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
                            SERVER_LOGS, message);
                                /* 5371 Can't set the receive timeout option
                                   value of {0} on client socket {1} */
                     }

                     /* Indicate we don't need to do this test again. */
                     setClientTimeoutNow=BOOL_FALSE;
                }
            }

            /*
             * We've completed the work for processing a request packet and
             * response packet pair.
             *
             * Test if we used the standing request packet.  If not,
             * then we need to return the memory used for the request
             * packet we just processed.
             */
            if ( requestPacketPtr != (wdePtr->standingRequestPacketPtr)){
                /* we used a separate buffer, release it */
                Tfree(7, "ServerWorker.c", requestPacketPtr);
                }

            /*
             * If we have finished with this client, (workingOnaClient is equal
             * to BOOL_FALSE), then close the client Trace file if present and
             * also close the client socket since we have sent the last response
             * packet.
             */
NOTWOC:
            if (workingOnaClient == BOOL_FALSE){

                /* Clear the debug flags since we are done with the client and will be
                 * closing the trace file.
                 */
                debugBrief =    BOOL_FALSE;
                debugDetail =   BOOL_FALSE;
                debugInternal = BOOL_FALSE;
                debugSQL =      BOOL_FALSE;
                debugSQLP =     BOOL_FALSE;
                debugSQLE =     BOOL_FALSE;

                /* Test for a client Trace file */
                if (wdePtr->clientTraceFile != NULL){
                   /* A Client Trace file is present, attempt to close it */
                   error_status= closeClientTraceFile(wdePtr, message, message_len);
                   if (error_status !=0){
                       /*
                        * Error occurred in trying to close client trace file. Log
                        * this error into the JDBC Server Log file and continue
                        * Server Worker client close down steps.
                        */
                       addServerLogEntry(SERVER_LOGS, message);
                   }
                }

                /* Close down the Socket connection to the JDBC Client.
                 * If there are any errors doing that, they will have
                 * been logged in the Server's Log file.
                 */
                error_status=Close_Socket(client_socket);
                if (error_status !=0){
                   /*
                    * Couldn't even close the the Client Socket, log another
                    * message indicating this fact. Do a SW shutdown, indicating
                    * their is no client socket (0) to close.
                    */
                    getLocalizedMessage(SERVER_SW_CANT_CLOSE_CLIENT_SOCKET,
                        itoa(wdePtr->serverWorkerActivityID, digits2, 8),
                        itoa_IP(client_socket, digits, 10, wdePtr->client_IP_addr, wdePtr->client_Userid, wdePtr->threadName),
                        SERVER_LOGS, message);
                            /* 5372 Server Worker activity {0} couldn't close client socket {1},
                               shutting Server Worker activity down */
                   serverWorkerShutDown(wdePtr, DONE_PROCESSING_CLIENT,0); /* stop the Server Worker activty. */
                }
                /* Socket is closed, no client, so clear client_socket */
                wdePtr->client_socket=0; /* first clear the WDE entry, */
                wdePtr->comapi_bdi=0;    /* and its COMAPI bdi, */
                client_socket=0;         /* then clear the convienence local copy */
            }

            /*
             * Before looping back, test if we have been told to shut
             * down the Server Worker. If its an immediate shutdown,
             * stop the Server Worker right away, otherwise let the
             * Client finish using the connection .
             */
            testAndCall_SW_ShutdownImmediately_IfNeeded(wdePtr
                                       ,AFTER_SENDING_RESPONSE_PACKET
                                       ,client_socket);
            /*
             * If there will be more requests from this JDBC Client,
             * then loop back to handle the next one.
             */
            } /* end of inner while loop - one per request/response handling */

            /*
             * Before placing this Server Worker back on the free chain, test
             * if we have been told to shut down the Server Worker. If its
             * an immediate or graceful shutdown, stop the Server Worker
             * right away ( the shutdown code will remove the Server Worker
             * from the assigned chain), otherwise let the code continue.
             */
            testAndCall_SW_Shutdown_IfNeeded(wdePtr, DONE_PROCESSING_CLIENT
                                             ,client_socket);

            /*
             * Not shutting down, so we just need to remove the Server Worker
             * from the assigned WDE chain since it has completed its work for
             * its JDBC Client.
             */
            remove_from_assigned_wde_chain(firstAssigned_WdeTSptr,wdePtr);

            /* ICL_num is initialized to 0 after the call to the remove_from_assigned_wde_chain()
             * macro. (The macro needed the ICL number to decrement the ICLAssignedServerWorkers
             * count for that ICL. After that, the ICL number should be set to 0).
             */
            wdePtr->ICL_num=0;

            /*
             * Now we need to place the Server Worker on the free (unassigned)
             * Server Worker chain and place it asleep until a new JDBC Client
             * is assigned to it. This involves Test and Set queuing.
             * This operation will done as quickly as possible, since part
             * of the time this operation while be holding the free (unassigned)
             * Server Worker chain, and then the ICL will not be able
             * to obtain a free Server Worker for any incoming JDBC
             * Clients until that sequence is completed.
             *
             * In addition to the sequence noted above, there is also
             * the need to check if the Server Worker has been told
             * to shutdown.  Normally this would be handled by the
             * Initial Connection Listener when the Server Worker
             * is still on either the assigned Server Worker chain
             * or on the free Server Worker chain.
             *
             * However, it is possible that this Server Worker was in
             * the middle of switching; already off the assigned Server
             * Worker chain and not yet on the free Server Worker chain
             * when the Initial Connection Listener posted the shutdown
             * notifications.  In this case, the Server Worker would not
             * informed of the shutdown via its WDE ( the ICL couldn't
             * see the Server Worker on either chain).
             *
             * To prevent a Server Worker from falling into this hole,
             * it must check the ICL's shutdown state while trying to
             * place itself on the free Server Worker chain. (Checking
             * the ICL's shutdown state makes sense since if the ICL
             * is shutting down, or is shut down, then the Server
             * Worker would never come off the free Server Worker chain!)
             * If it's to shut down, the Server Worker will not add
             * itself to the free chain and not go to sleep.  It then
             * checks the client socket value, which is 0 (no ICL=no client),
             * which inform the Server Worker to shutdown.
             *
             * So, there are two test and set sequences done, in nested
             * fashion, here:
             *  1)  Test and set on this Server Worker's WDE T/S cell.
             *      This allows us to complete our work without the ICL
             *      attempting to wake this SW before it can go to sleep.
             *
             *  2) Test and set on the free Server Worker WDE chain T/S
             *     cell.
             *
             *  3) Test for possible Server Worker shutdown notifications
             *     that might be missed by checking the ICL's shutdown state.
             *     3a) If the ICL is not shutting down, proceed to step 4.
             *     3b) If ICL is shutting down, then:
             *         3b1) Clear the free Server Worker WDE chain T/S
             *              cell. This allows the ICL to look at the free
             *              Server Worker chain, if it hasn't already.
             *         3b2) Clear the Server Worker's WDE T/S cell and
             *              leave the Server Worker awake so it can
             *              detect its to shutdown Proceed to step 7.
             *
             *  4) Place this SW on the free Server Worker WDE chain for
             *     later re-use.
             *
             *  5) Clear the global (sgd)free Server Worker WDE chain T/S cell.
             *     This allows the ICL to once again look for free
             *     Server Workers.
             *
             *  6) Clear this (wde) Server Worker's WDE T/S cell and place
             *     it asleep until it is woken up.
             *
             *  7) The Server Worker is now awake. It either woke up
             *     with a new JDBC Client (client socket !=0) or its
             *     awake to shut down (client socket=0).
             */

            test_set(serverWorkerTSptr);   /* Lock this SW WDE T/S cell */
            test_set(firstFree_WdeTSptr);  /* Lock the free WDE chain T/S cell */

            /* Test if we fell into the shutdown notification gap */
            if(sgd.ICLShutdownState[wdePtr->ICL_num] != ICL_ACTIVE ) {
                /* Server Worker should have shut down because ICL is not
                 * active - either it is shut down already or it is in the
                 * process of shutting down. So no new JDBC Clients will be
                 * forthcoming.  Set the Server Worker shutdown flag
                 * to shut down immediately (quick is better) and let it
                 * resume execution.
                 */
                wdePtr->serverWorkerShutdownState = WORKER_SHUTDOWN_IMMEDIATELY;

                /* release the T/S cells in proper order. */
                ts_clear_act(firstFree_WdeTSptr);     /* free the free WDE chain T/S cell*/
                ts_clear_act(serverWorkerTSptr);      /* Unlock this SW's T/S cell */
            }
            else {
                /*
                 * Server Worker is not shutting down, so place this
                 * Server Worker on free (unassigned) SW WDE chain
                 */
                wdePtr->free_wdePtr_next=sgd.firstFree_wdePtr; /* point WDE to WDE at top of free chain */
                sgd.firstFree_wdePtr=wdePtr;         /* now point top of free chain to this WDE */
                sgd.numberOfFreeServerWorkers++;     /* bump count of free WDE's (i.e. num of SW's)*/

                wdePtr->serverWorkerState=FREE_NO_CLIENT_ASSIGNED; /* set worker state to free */
                ts_clear_act(firstFree_WdeTSptr);            /* free the free WDE chain T/S cell*/

                /* Now clear the T/S cell on this Server Workers
                 * WDE and at the same time place this worker into
                 * a sleep state waiting for a new JDBC Client.
                 */
                /*   printf("The Server Worker is putting itself to sleep waiting for a new JDBC Client\n"); */
                ts_sleep_ul_act(serverWorkerTSptr); /* Unlock T/S cell and sleep */
                }

            /*
             * We are now outside T/S control, and the SW was either asleep
             * on the free Server Worker chain, or is now active. It has been
             * woken, either to detect it was told to shut down, or it has a
             * new JDBC Client to process.
             *
             * If the Server Worker was asleep, when control reaches the
             * next statement, the Server Worker has been awoken by the ICL
             * or possibly by another main JDBC Server activity.  Check why
             * we have been woken and begin processing:
             *
             * Either we have a new JDBC Client, or we have been woken
             * up so we can shut down.  Test both the client_socket to quickly
             * determine which.
             */
    /*    printf("Free Server Worker %012o has just woken up and may have a new JDBC Client\n",wdePtr->serverWorkerActivityID);
     */
            if (wdePtr->client_socket == 0 ||
                wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_GRACEFULLY ||
                wdePtr->serverWorkerShutdownState == WORKER_SHUTDOWN_IMMEDIATELY){
                /*
                 * We were woken to shut ourselves down, so do so.
                 * This can either be because there is no client socket (0)
                 * and hence no work to do, or its because the Server Worker's
                 * shut down state is no longer active indicating its either
                 * shutdown immediate or shutdown graceful - in either case
                 * the Server Worker can shutdown right away.
                 *
                 * Note: the Server Worker should not under normal conditions
                 * ever be woken up with a client socket value of 0 - this is
                 * interpreted always as indicating the Server Worker should
                 * shut down; i.e. no client socket = no JDBC client = no need
                 * for Server Worker.
                 *
                 * The call to the procedure serverWorkerShutDown will
                 * not return since the activity will be terminated.
                 *
                 */
                serverWorkerShutDown(wdePtr,WOKEN_UP_TO_JUST_SHUTDOWN,wdePtr->client_socket);
                }

            /*
             * We've accepted a new JDBC Client.
             * If debug on, add a new log entry for accepting the new JDBC Client.
             */
            if (sgd.debug){
              strcpy(message,"Free JDBC Server Worker ");
              strcat(message,itoa(wdePtr->serverWorkerActivityID,digits,8));
              strcat(message," assigned new JDBC Client accepted from IP address ");
              strcat(message,inet_ntop((int)wdePtr->client_IP_addr.family,
                     (const unsigned char *)&(wdePtr->client_IP_addr.v4v6),
                     digits, IP_ADDR_CHAR_LEN + 1));
              strcat(message," on client socket: ");
              strcat(message,itoa(wdePtr->client_socket,digits,10));
              addServerLogEntry(TO_SERVER_TRACEFILE,message);
             }

            /*
             * There are three steps that must be done before we loop back to process
             * this user:
             * 1) Get a local copy of the new client socket and IP addresses from the
             *    WDE for later usage in the task processing loop, etc. (Remember the
             *    the COMAPI bdi value is automatically gotten from the WDE if needed.
             *    Then inherit the client socket from the ICL activity.
             * 2) Re-initialize the rest of the WDE for this Server Worker. This will
             *    set up the appropriate initial values in the WDE.
             * 3) Add the WDE to the assigned Server Worker WDE chain.
             */

            /* Get the local copies of the client socket and IP address information. */
            client_socket=wdePtr->client_socket;
            client_IP_addr=wdePtr->client_IP_addr;

            /*
             * Inherit the Client socket now, since the ICL bequeathed it before
             * waking up this Server Worker task with a new client.
             *
             * When the Server Worker is first started, it will inherit its client
             * socket just before attempting to acquire a WDE.
             *
             * We must use the  Bequeath/Inherit mechanism (i.e., we can't use
             * shared sockets) since we do two TCP_Receives to get the request
             * packet. Doing this under shared sockets causes COMAPI to preform
             * poorly (it copies the request packet information in/out of queue
             * banks, etc.). Test the error status right away since we cannot
             * do any processing (including sending an error packet to the
             * client) if we couldn't inherit the client socket.
             */
            error_status = Inherit_Socket(client_socket);
            if (error_status != 0 ){
               /*
                * We got an error trying to inherit the socket from COMAPI.
                * The COMAPI error is already in the Log file. The socket
                * will error off automatically. We can't process the client,
                * so shut down this JDBC Server Worker instance.
                */
               serverWorkerShutDown(wdePtr, COULDNT_INHERIT_CLIENT_SOCKET, client_socket);
            }

            /*
             * Now complete the re-initialization of the WDE and indicate that
             * Server Worker has a new (another one) JDBC Client assigned.
             */
            initializeServerWorkerWDE(wdePtr);
            wdePtr->serverWorkerState=CLIENT_ASSIGNED;

            /*
             * Now, link the WDE into the chain of assigned Server Workers
             * WDE entries. This step is done under Test and Set control.
             *
             * Once added to the assigned Server Workers WDE chain, this WDE is visible
             * to the other main activities of the JDBC Server, Initial Connection
             * Listener and the Console Handler. They can post a shutdown directive
             * (e.g. serverWorkerShutDownState = WORKER_SHUTDOWN_GRACEFULLY) that this
             * Server Worker will have to obey which is not part of the Server
             * Worker codes normal flow.
             *
             * Do only the minimum work to add the newly created WDE to the assigned
             * Server Worker chain. (Remember the chain is a doubly linked list and we
             * are adding a WDE at the front (top) end.)
             */
/* printf("Just before add_to_assigned_wde_chain in serverWorkerWDE\n");  */
            add_to_assigned_wde_chain(firstAssigned_WdeTSptr,wdePtr);
/* printf("Just after add_to_assigned_wde_chain in serverWorkerWDE\n");  */

            /*
             * Before looping back, test if we have been told to shut
             * down the Server Worker. If a shutdown is indicated,
             * stop the Server Worker right away.  The JDBC Client will
             * be notified due to the socket closing.
             */
            testAndCall_SW_Shutdown_IfNeeded(wdePtr,AFTER_GIVEN_NEXT_CLIENT
                                             ,client_socket);

     } /* end of outer while loop - one per JDBC Client */

     /*
      * We have ended the main while loop because the shutdown flag was
      * set, so we need to close done this Server Worker.
      *
      * Note: We have a client on the assigned Server Worker chain ( because
      * we either tried to enter at the top of the while loop or we are looping
      * back with a new client obtained at the bottom of the while loop.  Either
      * way, we have to remove the client from the assign chain.  Then this
      * Server Worker can shut down without having to check if it is the last
      * Server Worker in a shutdown scenario.
      *
      * So, close the last client's socket if it exists, and free
      * this Server Worker's resources. The call to the procedure
      * serverWorkerShutDown will not return, and the Server Worker's
      * activity will be terminated.
      */
     serverWorkerShutDown(wdePtr,END_OF_MORE_CLIENTS_LOOP,client_socket);
     return; /* return not needed, but here anyway */
}
#endif /* JDBC Server */
