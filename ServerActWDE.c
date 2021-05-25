/**
 * File: ServerActWde.c.
 *
 * Procedures that comprise the JDBC Server's Activity local WDE definition
 * and set routines. This code module provides the file level (activity level)
 * declaration of the act_wdePtr and anActivityLocalWDE variable and structure
 * that will be used to reference WDE entries throughout the JDBC Server's
 * Initial Connection Listener, Startup/Console Handler, and the numerous
 * Server Worker activities.
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
#include "cintstruct.h" /* Include crosses the c-interface/server boundary */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "NetworkAPI.h"
#include "ServerActWDE.h"

/*
 * Provide the Server Worker activity with an indicator that tells
 * whether we're executing in the C-Interface code (1 or true), or in the
 * Server Worker code (0 or false). This indicator is used in the signal
 * handling routine as part of its determination on what to do with the
 * Server Worker activity.
 */
int insideClibProcessing;

/*
 * Provide the Server Worker activity and C-Interface code
 * visible debugging/trace variables and a trace file handle.
 *
 * The C-Interface library will gain access to these variables
 * through extern's defined in the <server.h> copy element that
 * each C-Interface code module includes.
 *
 * The debugXXXX variables are established in processClientTask prior
 * to calling the desired task indicated in the request packet. The
 * Client's trace file handle is established by the processing of
 * binding a Server Worker instance to a client connection request.
 */
int debugInternal;     /* indicates internal level of debug info is desired */
int debugDetail;       /* indicates detailed level of debug info is desired */
int debugBrief;        /* indicates brief level of debug info is desired */
int debugTstampFlag;   /* indicates whether we need to add the timestamp prefix during tracef processing. */
int debugPrefixFlag;   /* indicates whether we need to add the debugPrefix during tracef processing. */
int debugPrefixOrTstampFlag;   /* indicates whether we need to add either a debugPrefix or a timestamp during tracef processing. */
int debugSQL;          /* indicates whether SQL debug info is desired */
int debugSQLE;         /* indicates whether SQL debug info, if produced, should include EXPLAIN text */
int debugSQLP;         /* indicates whether SQL debug info, if produced, should include $Pi parameter info */
char debugPrefix[REQ_DEBUG_PREFIX_STRING_LENGTH+1]; /* trace object and & parent's object instance id's */


/* Define the Server Global Data (SGD) that's visible to all Server activities. */
extern serverGlobalData sgd;

/*
 * The activity visible pointer, act_wdePtr, will be extern'ed in each of the files
 * that needed it.  Each activity (e.g. ICL, CH, SW's) will set act_wdePtr to an
 * appropriate WDE instance.  The JDBCServer/ConsoleHandler and ICL will use an
 * activity local instance, anActivityLocalWDE, whereas the Server Workers will
 * set act_wdePtr to their actual wdePtr value provided them (which points into
 * the wdeArea of the sgd which is in program level memory.
 *
 * This is necessary since some of the functions and procedures utilized by
 * the JDBCServer/ConsoleHandler activity are also being used by the ICL and Server
 * Worker activities and those functions need specific information to tailor their
 * actions (i.e. localization of messages).  The design of those routines utilized
 * fields in the Server workers WDE to provide this tailoring information, and the
 * WDE pointer required was not passed to those routines but was assumed to be in
 * the activity local (file level) variable.
 */
workerDescriptionEntry *act_wdePtr;         /* Pointer to the ICL activities */
                                            /* own local WDE entry */
workerDescriptionEntry anActivityLocalWDE; /* activity local storage for a */
                                           /* local activities WDE entry. */

/*
 * Procedure setupActivityLocalWDE
 *
 * This procedure sets up the act_wdePtr, and a dummy WDE entry for this
 * calling JDBC Server activity. Upon return, the activity will have its
 * act_wdePtr pointer pointing to a WDE entry that is usable throughout the
 * activity with the various functions and procedures that implicitly
 * require a WDE pointer, i.e. act_wdePtr, to do their work.
 *
 * For the JDBC Server startup/Console Handler activity and the ICL activity,
 * the act_wdePtr will point to a dummy WDE called anActivityLocalWDE available
 * for this purpose.
 *
 * For a Server Worker activity, a WDE entry will be allocated later
 * and pointed to by wdePtr, so act_wdePtr will then be assigned this wdePtr
 * value ( i.e. wdePtr and act_wdePtr will point to the same entry) and
 * anActivityLocalWDE is not used. Until then, the Server Worker will have
 * the dummy WDE entry available for error handling in case the real WDE entry
 * cannot be allocated.
 *
 * Although the Server Worker will have a valid wdePtr, it and the other
 * activities that comprise the JDBC Server (e.g. ICL, CH ) MUST call this
 * routine immediately upon their activities creation ( i.e. an SW must call
 * first thing after getting a valid wdePtr) since calling this code insures
 * that the total JDBC Server code has the same reference location for
 * act_wdePtr in all of its activities.
 *
 * Parameters:
 *
 * ownerActivity  - the ownerActivityType value to set into the anActivityLocalWDE
 *                  if the anActivityLocalWDE is being utilized.
 * associated_COMAPI_bdi - the initial COMAPI bdi associated to this activities work.
 * associated_COMAPI_IPv6 - indicates whether COMAPI supports IPv6 (0 - no, 1 - yes).
 * associated_ICL_num    - the initial ICL's number associated to this activities work.
 */
void setupActivityLocalWDE(enum ownerActivityType ownerActivity,
                           int associated_COMAPI_bdi, int associated_COMAPI_IPv6,
                           int associated_ICL_num){

    /*
     * So set up the available dummy WDE entry (anActivityLocalWDE),
     * for this activity to use and have it minimally initialized.
     */
    act_wdePtr=&anActivityLocalWDE;
    minimalInitializeWDE(act_wdePtr,ownerActivity);

    /* save the initial COMAPI bdi, IPv6 flag, ICL_num to use in the dummy WDE entry, just in case. */
    act_wdePtr->comapi_bdi=associated_COMAPI_bdi;
    act_wdePtr->comapi_IPv6=associated_COMAPI_IPv6;
    act_wdePtr->ICL_num=associated_ICL_num;
    act_wdePtr->in_COMAPI=NOT_CALLING_COMAPI; /* Initial COMAPI subsystem calling flag */
    act_wdePtr->in_RDMS=NOT_CALLING_RDMS;     /* Initial UDS/RDMS/RSA subsystem calling flag */

    /*
     * Now allocate a C_INT data structure so it can initially be
     * used by the C-Interface message routines if we need to
     * create/display internal error messages. If this activity is
     * for a ServerWorker, the C_INT data structure will be switched
     * to the actual WDE and used by the C-Interface routines to
     * hold their state information. Note: the C_INT data structure
     * allocation is done directly using a malloc() call since it
     * is not part of the Tmalloc/Tfree allocations. The malloc for
     * the c_intDataStructure must be rounded to a multiple of 8 bytes
     * in size to avoid the malloc "orphan" issue.
     */
    act_wdePtr->c_int_pkt_ptr = malloc( ((sizeof(c_intDataStructure)+7)/8)*8 ); /* Always malloc(), not Tmalloc(), and round size to multiple of 8 bytes. */

    return;  /* all done */
}

void minimalInitializeWDE(workerDescriptionEntry *wdeptr,
                          enum ownerActivityType ownerActivity){
    int i;

    /* indicate that we are not in C-Interface code of course */
    insideClibProcessing=BOOL_FALSE;

    /* (Re)-initial COMAPI, UDS/RDMS/RSA subsystems calling flags */
    wdeptr->in_COMAPI=NOT_CALLING_COMAPI;
    wdeptr->in_RDMS=NOT_CALLING_RDMS;

    /*
     * Set up the WDE entry for this activity to use.
     */
    wdeptr->ownerActivity=ownerActivity;              /* The activity type that owns this WDE */
    tsk_id(&(wdeptr->serverWorkerActivityID));             /* Exec 2200 36-bit activity ID of this activity. */
    tsk_retactid(&(wdeptr->serverWorkerUniqueActivityID)); /* Exec 2200 72-bit activity ID of this activity. */

    /* Indicate that we do not have a userid or an open RDMS Thread at this time. */
    wdeptr->openRdmsThread=BOOL_FALSE;
    strcpy(wdeptr->client_Userid,"\0");                /* the JDBC Client's userid */
    strcpy(wdeptr->threadName,"\0");                   /* Clear the thread name passed to RDMS at Begin Thread time. */

    /*
     * Indicate this WDE is not on the WDE reallocation chain by clearing
     * its realloc_wdePtr_next pointer. Remember, this entry is not on the chain,
     * so its OK to clear this cell.
     */
    wdeptr->realloc_wdePtr_next=NULL;

    /*
     * For all the JDBC Server's activities, except the initial
     * CH and XAServer Worker at server startup, the Server's message file locale
     * is already set.  For the CH and XA Server Worker activities WDE at startup,
     * the message file locale assigned here will be updated as part of the
     * configuration processing ( value used here will probably be the products
     * initial default value ).
     */
    wdeptr->serverMessageFile=sgd.serverMessageFile;  /* Use the Server's locale message file */
    wdeptr->workerMessageFile=sgd.serverMessageFile;  /* Use the Server's locale message file */
    strcpy(wdeptr->workerMessageFileName,sgd.serverMessageFileName); /* the Server's locale message filename */
    strcpy(wdeptr->clientLocale,sgd.serverLocale);

    /*
     * Initially indicate we have no response packet waiting for reallocation - we
     * are not holding a COMAPI q_bank at this time.
     */
    wdeptr->releasedResponsePacketPtr=NULL;

    /* For debug usage, set the debugIndent string to blanks */
    for (i=0; i < REQ_DEBUG_PREFIX_STRING_LENGTH; i++) {
       debugPrefix[i]=' ';
    }
    debugPrefix[REQ_DEBUG_PREFIX_STRING_LENGTH]='\0'; /* add a terminating null */
    debugPrefixOrTstampFlag=BOOL_TRUE; /* set it to indicate that we will add either prefix or timestamp on first tracef */

    /* Indicate that we have no Relational Driver level information at this time. */
    strcpy(act_wdePtr->clientDriverBuildLevel,"\0");
    act_wdePtr->clientDriverLevel[0]=0;
    act_wdePtr->clientDriverLevel[1]=0;
    act_wdePtr->clientDriverLevel[2]=0;
    act_wdePtr->clientDriverLevel[3]=0;

    return;  /* all done */
}

void minimalDefaultsSGD(){

    int error_code;
    char * ptr1;

    /*
     * Set up the minimal SGD default values to support the use of the
     * activity level WDE entries.  The SGD values that need to be initially
     * present can be seen in the minimalInitializeWDE() procedure code.
     *
     * First indicate there are no free WDE's on the reallocation chain.
     */
    sgd.WdeReAllocTS=TSQ_NULL;   /* be ready to allocate a WDE entry */
    sgd.realloc_wdePtr=NULL;   /* currently no WDE's on chain */

    /* Initialize the global T/S cells */

    sgd.serverLogFileTS=TSQ_NULL;
    sgd.serverTraceFileTS=TSQ_NULL;
    sgd.clientTraceFileTS=TSQ_NULL;
    sgd.msgFileTS = TSQ_NULL;

    sgd.serverLocale[0] = '\0'; /* initially cause locale to be default value */
    ptr1 = sgd.serverMessageFileName;

    error_code=openMessageFile(NULL,&sgd.serverMessageFile,&ptr1);

   /* Test for fatal problems with message file. */
   if (error_code < 0){
      /*
       * Couldn't open even the default message file. This
       * is fatal since we can only send messages to STDOUT.
       * Terminate the server execution after displaying a
       * error message on STDOUT.
       */
      printf("Fatal error - JDBC Server could not open the product's default locale message file.\n");

      /* tsk_stopall(); */
      /* terminate JDBC Server now */

#ifndef XABUILD /* JDBC Server */

     /* Deregister COMAPI from the application,
        and stop all active tasks.
        Note that control does not return to the caller. */
     deregComapiAndStopTasks(SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT, sgd.serverName, NULL);

#else          /* XA JDBC Server */

     /* No COMAPI usage; just stop all active tasks.
        Note that control does not return to the caller. */
     stop_all_tasks(XA_SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT, sgd.serverName, NULL);

#endif /* XA and JDBC Servers */
   }

   /* We do have a default message file to use, so return to caller. */
   return;
}
