/**
 * File: ServerUASFM.c.
 *
 * Procedures that comprise the JDBC Server's User Access
 * Security File MSCON$ (UASFM) information retrieval
 * functions supporting the JDBC user access security.
 *
 * This code works with the JDBC Server's SUVAL$ code (e.g. code
 * in file Server.c) to provide the permission validation on the JDBC
 * user access security file associated to the application group
 * underwhich the JDBC Server is operating at execution time.
 *
 */
/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <tsqueue.h>
#include <task.h>
#include <sysutil.h>
#include <universal.h>

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "ProcessTask.h"
#include "Server.h"
#include "ServerActWDE.h"
#include "ServerLog.h"
#include "ServerUASFM.h"
#include "signals.h"
#include "Twait.h"

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
 * Procedure user_Access_Security_File_MSCON
 *
 * This is the activity that obtains the latest lead item information
 * on the JDBC user access security file.  This information is obtained
 * using ER MSCON$ and is stored in the SGD for later userid permission
 * validation.
 *
 * This activity is not initiated if JDBC user access security is not
 * activated.
 */
void user_Access_Security_File_MSCON(){

    int loop;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /*
     * The VERY FIRST THING to do is provide the dummy WDE pointer reference for
     * the user_Access_Security_File_MSCON (UASFM) activity's local WDE entry so
     * we can utilize it for messages in case there is an error in this activity
     * of the JDBC Server.
     *
     * This allows us to immediately utilize the localized message handling code
     * in case there is an error. Also, this will be used in handling Network
     * API errors/messages.
     *
     * Note: This activity does not use COMAPI, so its WDE entries will be set as
     * a default to the COMAPI values that will be used by ICL(0).
     */
    setupActivityLocalWDE(SERVER_UASFM, sgd.ICLcomapi_bdi[0], 0, 0);
   /* addServerLogEntry(SERVER_STDOUT,"Done setting up act_wdePtr in UASFM"); */

    /* Register signals for this UASFM activity */
#if USE_SIGNAL_HANDLERS
    regSignals(&signalHandlerLevel1);
#endif

    /* Test if we should log procedure entry */
    if (sgd.debug==BOOL_TRUE){
      addServerLogEntry(SERVER_STDOUT,"Entering user_Access_Security_File_MSCON");
    }

    /*
     * Now the UASFM activity is up and running, so set its global
     * indicators in SGD. The indicator values will be changed if
     * the UASFM activity is told to shut down.
     */
    sgd.UASFM_State = UASFM_RUNNING;
    sgd.UASFM_ShutdownState = UASFM_ACTIVE;

    /*
     * Loop retrieving the JDBC user access security file MSCON$
     * lead item. This code toggles between two MSCON$ packets,
     * so there is always one packet available for ne JDBC client
     * SUVAL$ testing while we are retrieving any possible updates
     * to the MSCON$ information (e.g., security administrator may
     * switch the owned ACR associated to the JDBC user access security
     * file. A wait of MSCON_TWAIT_TIME seconds occurs between each
     * MSCON$ call. The loop ends when the code detects one of a set
     * of test conditions on the state of the other Server components.
     */

    /*
     * If we are tracing the SUVAL$ and MSCON$ operation using
     * TEST_SGD_DEBUG_TRACE_MSCON, then adjust the initial setting
     * of the sgd.MSCON_li_status[1] value so the first MSCON$ is
     * logged and traced.
     */
     if (TEST_SGD_DEBUG_TRACE_MSCON){
     sgd.MSCON_li_status[1]=0; /* forces a miss-compare on first MSCON$ status test */
     }

    /*
     * Loop indefinitely while the JDBC Server is up and running.
     */
    loop = BOOL_TRUE;
    while (loop){
        /*
         * FIRST, test if we are to shutdown. Since we have
         * no other tasks to perform then the MSCON$, check
         * the main state of the JDBC Server, the ICL activity state
         * (only in the JDBC Server not in the XA JDBCServer),
         * and the state of the UASFM activity to determine if we
         * should shutdown this task.
         */
        if ((sgd.UASFM_State != UASFM_RUNNING) || (sgd.UASFM_ShutdownState != UASFM_ACTIVE )||
            (sgd.serverShutdownState != SERVER_ACTIVE)
#ifndef XABUILD /* JDBC Server */
             || (sgd.ICLShutdownState[act_wdePtr->ICL_num] !=ICL_ACTIVE)
#endif          /* JDBC Server */
                                                                       ){
          /*
           * We are shutting down, so exit the UASFM activity, after
           * setting the global indicators and entering a Log file message.
           */
          sgd.UASFM_ShutdownState = UASFM_SHUTDOWN;
          sgd.UASFM_State = UASFM_CLOSED;
          getMessageWithoutNumber(SERVER_UASFM_SHUTDOWN, NULL, NULL,
                                  SERVER_LOGS_DEBUG);
          exit(0);
        }
        else {
            /*
             * We are not shutting down so do the MSCON$ operation.
             */
            set_sgd_MSCON_information();
            /* printf("In UASFM activity, after set_sgd_MSCON_information(), sgd.which_MSCON_li_pkt = %d \n", sgd.which_MSCON_li_pkt); */
        }

        /*
         * Now, before we sleep for a while, lets recheck
         * if we are to shutdown. Since we have no other
         * tasks to perform then the MSCON$, check the main
         * state of the JDBC Server, the ICL activity state,
         * and the state of the UASFM activity to determine
         * if we should shutdown this task.
         */
        if ((sgd.UASFM_State != UASFM_RUNNING) || (sgd.UASFM_ShutdownState != UASFM_ACTIVE )||
            (sgd.serverShutdownState != SERVER_ACTIVE) ||
             sgd.ICLShutdownState[act_wdePtr->ICL_num] !=ICL_ACTIVE){

          /*
           * We are shutting down, so exit the UASFM activity, after
           * setting the global indicators and entering a Log file message.
           */
          sgd.UASFM_ShutdownState = UASFM_SHUTDOWN;
          sgd.UASFM_State = UASFM_CLOSED;
          getMessageWithoutNumber(SERVER_UASFM_SHUTDOWN, NULL, NULL,
                                  SERVER_LOGS_DEBUG);
          exit(0);
        }
        /*
         * OK, let's sleep some time before we loop back do the MSCON$ again.
         */
        twait(MSCON_TWAIT_TIME);

     } /* end of while */

    /*
     * Loop exited abnormally so just shut down, and exit the
     * UASFM activity, after setting the global indicators.
     */
    sgd.UASFM_ShutdownState = UASFM_SHUTDOWN;
    sgd.UASFM_State = UASFM_CLOSED;
    getLocalizedMessageServer(SERVER_UASFM_ABNORMAL_SHUTDOWN,
                          itoa(loop, digits, 10), NULL, SERVER_LOGTRACES, msgBuffer);
    exit(0); /* terminates activity */
}

/*
 * Function get_MSCON_exist_lead_item
 *
 * This function uses MSCON$ to retrieve information on the JDBC user access
 * security file. Remember the security file is application group specific and
 * is part of the filename. The file's master file directory lead item is retrieved
 * and returned. This packet contains the owner userid and compartment information
 * for the JDBC user access security file, which is needed for the JDBC userid
 * access security check. If the required file is defined and available, then this
 * function worked OK and returns MSCON_GOT_LEAD_ITEM (01000000). If there is no file
 * or an error occurs, then the MSCON$ A0 returned status (full error info) is returned.
 *
 * Input Parameters:
 *      filename_ptr        - Points to the filename as 24 characters. The first
 *                            12 characters are the qualifier and the next 12
 *                            characters are the filename.  This must be adhered
 *                            to exactly, because both the qualifier and filename
 *                            are each converted into 12 fieldata strings, so the
 *                            input file name be in the desired for to be found.
 *                            NOTE: The filename checked is always the current cycle.
 *
 *      mscon_pkt_ptr       - MSCON packet pointer to where the MSCON$ packet
 *                            information is to be stored.
 *
 *      li_ptr              - lead item pointer to where the returned MSCON$
 *                            mfd lead item packet information is to be stored.
 *
 * Returned Function Value:
 *    (01000000, pkt addr in H2)  - the JDBC user access rights file was found and the
 *                                  desired information was stored in the SGD.
 *  !=(01000000, pkt addr in H2), - If an error occurred on the MSCON$ call, the whole MSCON$
 *    or = -1,                      status value (36 bits) is returned as the function value
 *                                  (this includes the packet address in H2 of the value). If
 *    or = -2,                      an error occured on the EM VA to BM VA conversion, then a
 *                                  function value of -1 or -2 is returned. Note: the -1 or -2
 *                                  values will not conflict with the MSCON$ error returned
 *                                  value so the two types of errors can be easily separated
 *                                  by the caller.
 *
 */
int get_MSCON_exist_lead_item(char * filename_ptr, mscon_exist_pkt_struct * mscon_pkt_ptr,
                              mfd_lead_item_struct * li_ptr){

    mscon_exist_pkt_struct mscon_pkt;     /* declare the MSCON$ packet */
    mfd_lead_item_struct mscon_lead_item; /* declare space for lead item */
    int em_va;                            /* will hold EM VA to be converted to BM VA*/
    int error_status;
    int inregs[1];  /* input register values */
    int outregs[1]; /* output register values */
    int ercode=ER_MSCON$;     /* the ER to do is MSCON$*/
    int inputval=0400000000000;   /* input is expected in A0  */
    int outputval=0400000000000;  /*  output is expected in A0 */
    int scratch;                  /* dummy item so ucsgeneral can obtain right L,BDI */

    /*
     * This routine builds an MSCON$ packet and retrieves information on the
     * JDBC user access security file.
     *
     * This operation consists of four steps:
     *
     * Step 1) Build the MSCON$ packet, placing in the JDBC access
     *         security file name passed into this procedure.
     *
     * Step 2) Do an ER MSCON$ to retrieve the owner and compartment
     *         information on the JDBC access security file.
     *
     * Step 3) Copy the lead item packet back to the caller's specified
     *         lead item packet buffer.  This is done regardless of the
     *         MSCON$ status returned to all the caller full control over
     *         any error status handling.
     *
     * Step 4) Test the MSCON$ status (A0 with H2 mask to 0's). If it
     *         is equal to MSCON_GOT_LEAD_ITEM (01000000), this is a
     *         special MSCON$ status indicating that you've received
     *         the lead packet as requested and that there are other
     *         directory items available. This is the status we expect
     *         since we only wanted the lead item. This status value
     *         will be sent back in this case.  In all other cases
     *         (status is not MSCON_GOT_LEAD_ITEM ) the full A0 value
     *         is returned as the function's value.
     */

    /* Clear out the Lead item buffer */
    memset(&mscon_lead_item, 0, sizeof(mscon_lead_item)); /* clear out buffer to 0's */

    /* Build the MSCON$ packet */
    memset(&mscon_pkt, 0, sizeof(mscon_pkt)); /* first clear out packet to 0's */
    mscon_pkt.dir_index= 0 ; /* standard directory */
    mscon_pkt.func_code =     MSCON_EXIST_FUNC; /* do an exist$ function */
    mscon_pkt.buffer_length = MSCON_BUFFER_LENGTH;
    mscon_pkt.f_cycle =       MSCON_FILE_CYCLE;
    mscon_pkt.start_sector =  MSCON_START_SECTOR;
    mscon_pkt.sector_count =  MSCON_SECTOR_COUNT;
    mscon_pkt.start_item =    MSCON_START_ITEM;

    /*
     * Convert to fieldata the full 12 characters of each of the
     * file name's qualifier and the filename's filename part.  Place
     * the fieldata into the MSCON$ packet.  Since the filename is given
     * as a fully adjacent 24 characters (with appropriate blanks in between the
     * qualifier and the file name parts), this conversion is done in one step.
     * It is assumed that the filename checked is always file-cycle 1.
     */
    ascfd(&mscon_pkt.qualifier, filename_ptr, MSCON_FULL_FILENAME_LEN); /* get the filename (24 chars) in fieldata */

    /* Get a BM VA to the lead item buffer */
    em_va=(int)&mscon_lead_item;  /* get em va to lead item buffer */
    ucsemtobm(&em_va,&mscon_pkt.buffer_address,&error_status);
    if (error_status !=0){
        /* Internal error, this should not happen. Return internal error flag, caller will log it. */
        return JDBC_INTERNAL_CODE_ERROR_EMTOBM1_MSCON_EXIST_FAILED;
    }
    /* Get a BM VA to the MSCON$ packet */
    em_va=(int)&mscon_pkt;  /* get em va to MSCON$ packet */
    ucsemtobm(&em_va,&inregs[0],&error_status); /* save BM VA for ucsgeneral */
    if (error_status !=0){
        /* Internal error, this should not happen. Return internal error flag, caller will log it. */
        return JDBC_INTERNAL_CODE_ERROR_EMTOBM2_MSCON_EXIST_FAILED;
    }

    /*
     * We are ready to perform the MSCON$, first test if we are to
     * display packet on debug.
     */

    /* Do the MSCON$ to retrieve information on access security file. */
    ucsgeneral(&ercode,&inputval,&outputval,inregs,outregs,&scratch,&scratch); /* do the MSCON$ */
    error_status= outregs[0] & MSCON_ERROR_STATUS_MASK; /* get error status part (H1) only. */


    /* Before testing status, copy the mscon_exist and mscon_lead_item packets to callers buffers */
    memcpy(mscon_pkt_ptr, &mscon_pkt, sizeof(mscon_pkt));
    memcpy(li_ptr, &mscon_lead_item, sizeof(mscon_lead_item));

    /*
     * The error status should be MSCON_GOT_LEAD_ITEM (01000000), a special
     * status code indicating there are more directory items available.
     * If the error_status is not MSCON_GOT_LEAD_ITEM, then we have an
     * error. Either way, give the caller the status (H1) and pkt addr (H2).
     */
    return outregs[0]; /* error, return 36 bit status value from A0 */
}



/*
 * Function set_sgd_MSCON_information
 *
 * This function calls ER MSCON$ and places the retrieved
 * information into the appropriate SGD fields for later
 * use by the SUVAL$ user access validation check.
 *
 * Note: The following code was coded inline in the while loop
 * of the user_Access_Security_File_MSCON() function in earlier
 * JDBC Server levels (that function now references the
 * set_sgd_MSCON_information() function). For comparison purposes,
 * the following code is left at the same amount of indentation as
 * it was when it was in user_Access_Security_File_MSCON().
 */
void set_sgd_MSCON_information(){

    int current_index;
    int new_index;
    mscon_exist_pkt_struct mscon_pkt;     /* declare the MSCON$ packet */

    /* used to dump MSCON Pkt */
    int * dump;
    char adigits[ITOA_BUFFERSIZE];
    char digits[ITOA_BUFFERSIZE];
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

            /*
             * Alternate which of the two MSCON$ lead item packets to
             * use so there is always a usable MSCON$ lead item packet
             * for SUVAL$ testing.
             */
            current_index=sgd.which_MSCON_li_pkt; /* get which pkt is current */
            new_index = (current_index == 0) ? 1:0; /* toggle between array index 0 and 1 */

            /* Now do the MSCON$ call and save the returned error status */
            sgd.MSCON_li_status[new_index] = get_MSCON_exist_lead_item(sgd.user_access_file_name,
                                                                       &mscon_pkt,
                                                                       &sgd.MSCON_li_pkt[new_index]);

            /* printf("In UASFM: new_index = %d, sgd.MSCON_li_status[new_index] = %012o\n",new_index,sgd.MSCON_li_status[new_index]); */
            /*
             * Test error status. If its different then the other lead item packets error
             * status, then place a log entry indicating this.  Note: using this logging
             * approach, we only log a bad status one time, and we log the switch
             * in that status (even back to a good status ) only one time! Since the
             * MSCON$ status indicators are initialized to 1's at JDBC Server
             * start up, we will not log an initial normal status MSCON_GOT_LEAD_ITEM on
             * the very first MSCON$ call.
             *
             * Do the error testing only on the error status (in H1), not on the packet address
             * part of the returned MSCON$ value (in H2).
             */
            if ((sgd.MSCON_li_status[new_index]& MSCON_ERROR_STATUS_MASK) !=
                (sgd.MSCON_li_status[current_index] & MSCON_ERROR_STATUS_MASK)){

               /* change in error status part between MSCON$ calls, Log the change. */
               getMessageWithoutNumber(SERVER_UASFM_MSCON_ERROR,
                                       itoa((sgd.MSCON_li_status[new_index] >> 18), digits, 8),
                                       itoa((sgd.MSCON_li_status[new_index] & MSCON_PACKET_ADDRESS_MASK), adigits, 8), TO_SERVER_LOGFILE);
                     /* 5383 Server UASFM activity now receives the following MSCON octal status = {0}, packet address = {1} */

               if (TEST_SGD_DEBUG_TRACE_MSCON){
                    /*
                     * In debug mode, trace the full error code, MSCON$ packet and lead item
                     * buffer. Update this code if the defines for MSCON_EXIST_PKT_LEN
                     * and MFD_LEAD_ITEM_LEN are changed.
                     *
                     */
                     getMessageWithoutNumber(SERVER_UASFM_MSCON_ERROR,
                                             itoa((sgd.MSCON_li_status[new_index] >> 18), digits, 8),
                                             itoa((sgd.MSCON_li_status[new_index] & MSCON_PACKET_ADDRESS_MASK), adigits, 8), SERVER_TRACES);
                           /* 5383 Server UASFM activity now receives the following MSCON octal status = {0}, packet address = {1} */

                    /* Now dump the MSCON$ packet in octal in the JDBC Server Trace file. */
                    dump = (int *)&mscon_pkt;
                    sprintf(message,"MSCON$ status: %012o, MSCON$ packet:\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o\n",
                            sgd.MSCON_li_status[new_index], dump[0], dump[1], dump[2],
                            dump[3], dump[4], dump[5], dump[6], dump[7]);
                    addServerLogEntry(SERVER_TRACES, message);

                    /* Now dump first part (first 18 words) of Lead Item buffer in
                     * octal in the JDBC Server Trace file.
                     */
                    dump = (int *)&sgd.MSCON_li_pkt[new_index];
                    sprintf(message,"MSCON$ status: %012o, MSCON_li_pkt[%d] :\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o %012o %012o %012o\n",
                            sgd.MSCON_li_status[new_index], new_index, dump[0], dump[1], dump[2],
                            dump[3], dump[4], dump[5], dump[6], dump[7], dump[8], dump[9], dump[10],
                            dump[11], dump[12], dump[13], dump[14], dump[15], dump[16], dump[17]);
                    addServerLogEntry(SERVER_TRACES, message);
               }
            }

            /* Switch the current MSCON packet indicator */
            sgd.which_MSCON_li_pkt = new_index;
}
