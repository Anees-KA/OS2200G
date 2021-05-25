/**
 * File: AllocRespPkt.c.
 *
 * Contains the procedure that provide storage for the Response Packet.
 *
 * For remote JDBC Clients this code interfaces to the network
 * services of COMAPI to acquire Q-bank storage space.
 *
 * For local JDBC Clients utilizing JNI, the code interfaces to the Java
 * Virtual Machine environment to acquire a byte array for the Response Packet.
 *
 * Note: Currently only the remote JDBC Clients utilizing COMAPI are
 * supported.
 *
 * Note: The define XABUILD, if defined, indicates we are building the XA JDBC Server. Code that
 * is part of the XA JDBC Server only is indicated by code surrounded by an #ifdef XABUILD and
 * #endif pair. Code that is part of the local JDBC Server only is indicated by code surrounded
 * by an #ifndef XABUILD and #endif pair.  Where there are two conditional compile sections
 * next to each other, whenever possible the code that is part of the local JDBC Server is
 * provided first and the XA JDBC Server code is provided second. This should make the code
 * easier to read and understand.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <jni.h>
#include <task.h>

#ifdef XABUILD  /* XA JDBC Server */
#include <CORBA.h>
#endif          /* XA JDBC Server */

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "AllocRespPkt.h"
#include "NetworkAPI.h"
#include "ServerLog.h"

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


/*
 * Function allocateResponsePacket
 *
 * In the JDBC Server case:
 * This function allocates a byte (char) array,that will contain the
 * response packet. How the array is provided depends on whether the
 * JDBC Client is using the local JNI interface or the remote network
 * interface.
 *
 * For a local JDBC client using JNI to access the JDBC Server, the byte
 * array needed will be allocated in Java and passed to the JDBC Client
 * via the local JDBC Client JNI calling sequence.
 *
 * For a remote JDBC client (one using the socket interface to the JDBC Server),
 * a COMAPI Q-bank is allocated, initialized, and a pointer reference to the
 * User Data Area within that Q-Bank is passed back to the caller - this is
 * the location of the response packet that is to be used by the C-Interface
 * code. Only the network routines that handle the Q-bank know where the actual
 * start of the Q-bank is located.
 * The actual length of the response packet will be calculated just before it is
 * sent via COMAPI (e.g., the debug info area can easily be added to the end of
 * a typical response packet. The exception is response packets for Fetch tasks
 * need to compute maximum number of fetchable records.  Their allocation requests
 * will already assume the space for the debug info area type 0 is available in the
 * allocated q-bank space.
 *
 * In the XA JDBC Server case:
 * The response packet will be fully allocated, and the length
 * of the packet is saved in the wde.  The additional space for a debug
 * info area type 0 is added to the requested allocation size obtained
 * from function CORBA_sequence_octet_allocbuf(), in case the debug info
 * area is needed later on.
 */
char * allocateResponsePacket(int response_packet_size)
 {

#ifndef XABUILD /* JDBC Server */

    int error_status;
    char * msgPtr;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */
    char errorText[100];
    char * borrowed_qbankPtr;
/*
 if (wdePtr->serverCalledVia==CALLED_VIA_COMAPI){
       */   /* A local JDBC Client, allocate space from JVM. */
/*
          newByteArrayObj = (*localEnv)->NewByteArray(localEnv, (jint)numberOfBytes);
          resPkt_Ptr = (jbyte *)(*localEnv)->GetBooleanArrayElements(localEnv,newByteArrayObj,NULL);
          return (char *)resPkt_Ptr;
      }  */
/* else { */
          /* A remote Jdbc Client, networking will be used, so
           * allocate a COMAPI Q-Bank that set up ready for
           * our use. Use a previously released response packet if
           * available to save borrowing a q_bank since we already would
           * have it allocated to this activity.
           */
          borrowed_qbankPtr=act_wdePtr->releasedResponsePacketPtr;
          /* is there a saved one? */
          if (borrowed_qbankPtr != NULL){
            /* yes, so use it. */
            act_wdePtr->releasedResponsePacketPtr=NULL; /* clear it */

            /* Assume no debug info information information, so set debug info area offset to 0 */
            setDebugInfoOffset(borrowed_qbankPtr, 0);

            /*
             * Save the requested size plus the debug header space. We can later
             * compare this to actual space used to form the response packet.
             */
            act_wdePtr->resPktLen = response_packet_size + RES_DEBUG_INFO_MAX_SIZE;

            return borrowed_qbankPtr;
          }

          /* No released response packet was available, so borrow a q_bank */
          error_status=Borrow_Qbank(&borrowed_qbankPtr, response_packet_size);
          if ((error_status !=0) || (response_packet_size > MAX_ALLOWED_RESPONSE_PACKET)){
           /*
             * An error was detected.  This should not happen, so
             * log the error and shutdown the JDBC Server.
             *
             *  *** We will want to enhance this later xxxx *****
             */
        	sprintf(errorText, "%d, error status = %d", response_packet_size, error_status);
            msgPtr = getLocalizedMessageServer(SERVER_CANT_ALLOC_RESPONSE_PKT,
                                               errorText, NULL, SERVER_LOGS, msgBuffer);
                    /* 5300 Can't allocate a response packet of size {0}, error status = {1} */

            msgPtr = getLocalizedMessageServer(SERVER_SHUTTING_DOWN, sgd.serverName, NULL,
                                               SERVER_LOGS, msgBuffer);
                    /* 5421 JDBC Server "{0}" shut down */

            /* tsk_stopall(); */
            /* stop all remaining JDBC Server tasks. */

           /* Deregister COMAPI from the application,
              and stop all active tasks.
              Note that control does not return to the caller. */
           deregComapiAndStopTasks(0, NULL, NULL);
          }

          /* Assume no debug info information information, so set debug info area offset to 0 */
          setDebugInfoOffset(borrowed_qbankPtr, 0);

          /*
           * Got a Q-bank for the response packet, return it. First, save
           * the requested size plus the debug header space. We can later
           * compare this to actual space used to form the response packet.
           */
          act_wdePtr->resPktLen = response_packet_size + RES_DEBUG_INFO_MAX_SIZE;

          return borrowed_qbankPtr;
   /* } */

#else          /* XA JDBC Server */

    /*
     * Now "allocate" the response packet the size requested plus the extra
     * space needed for a possible debug info entry (currently type 0) to be
     * added (should debugging be turned on and a client trace file opened).
     * If the extra debug info space is not used, the extra bytes will not be
     * sent back. Always return the pointer to the response packet byte array.
     * Save the size of the packet in the wde.
     *
     * The XA Server Worker will be using the reusable CORBA_Octet byte array
     * for the response packet (pointed to by reusable_max_CORBA_octet_ptr in
     * XASrvrWorker.c and by the WDE's releasedResponsePacketPtr). This array
     * is actually bigger than what is being requested, so the act_wdePtr->resPktLen
     * indicates the amount of space actually needed (requested) by the C-Interface
     * code.
     *
     * IMPORTANT: Do not clear the released byte array reference (releasedResponsePacketPtr),
     * since when this task has completed it will be needed foruse by the next task.
     */
    act_wdePtr->resPktLen = response_packet_size + RES_DEBUG_INFO_MAX_SIZE;

    /* Assume no debug info information information, so set debug info area offset to 0 */
    setDebugInfoOffset(act_wdePtr->releasedResponsePacketPtr, 0);

    /* got a CORBA buffer for the response packet, return it */
    return act_wdePtr->releasedResponsePacketPtr;

#endif          /* XA and JDBC Servers */

 }

/*
 * Function releaseResPktForReallocation
 *
 * In the JDBC Server case:
 * This procedure temporarily allows a q_bank request packet to
 * be returned and later reallocated without actually releasing
 * the q_bank back to COMAPI.  This routine exists to handle
 * error handling cases in the C-Interface library where code has
 * already allocated a request packet ( i.e. gotten a q_bank )
 * and then determine that there is an error and want to use the
 * error message routine that produces a request packet with
 * the error message.  Without releaseResPktForReallocation, there
 * could be too many COMAPI checkout q_banks.
 *
 * The activity global reference to this activities WDE will be used
 * since a wdePtr to the wde entry is not provided as a calling parameter.
 *
 * In the JNI calling case, this routine would handle the Java
 * byte array being alocated in a similar fashion ( it might be
 * that the buffer could be release and later a new one allocated -
 * this will be determined when this code is actually developed).
 *
 * In the XA JDBC Server case:
 * This procedure temporarily allows a CORBA_octet request packet to
 * be returned and later reallocated without actually releasing
 * the CORBA_octet byte array.  This routine exists to handle
 * error handling cases in the C-Interface library where code has
 * already allocated a request packet ( i.e. gotten a CORBA_octet )
 * and then determine that there is an error and want to use the
 * error message routine that produces a request packet with
 * the error message.  Without releaseResPktForReallocation, the
 * usage of a single large CORBA_octet byte array to handle multiple
 * request tasks would not work as desired.
 */

void releaseResPktForReallocation(char * resPkt){

    /*
     * Save the pointer to the response packet (for JDBC Server it's the
     * q_bank, for the JDBC XA Server we always use the same buffer so don't
     * change it.
     */
#ifndef XABUILD /* JDBC Server */
    act_wdePtr->releasedResponsePacketPtr= resPkt;
#endif          /* JDBC Server */
    act_wdePtr->resPktLen = 0;  /* Indicate that we have no space allocated at this time. */

}
