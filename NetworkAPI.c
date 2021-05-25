/**
 * File: NetworkAPI.c.
 *
 * Procedures that comprise the JDBC Server's network API.
 *
 * For remote JDBC Clients the code interfaces to the network
 * services of COMAPI are used to support the client/server dialogue.
 *
 * For local JDBC Clients utilizing JNI, the code interfaces to the Java
 * Virtual Machine environment are used.
 *
 * Note: Currently only the remote JDBC Clients utilizing COMAPI are
 * supported.
 *
 * This code may utilize other code in the C-Interface library to accomplish
 * its required tasks.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <task.h>

/* JDBC Project Files */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "apidef.p"
#include "NetworkAPI.h"
#include "Server.h"
#include "ServerLog.h"
#include "ipv6-parser.h"

/* The following items are for use with COMAPI Q-Banks:
 *    1) Offset to User Data Area inside a Q-bank
 *    2) a typedef for the qb_user_type structure*/
#define OFFSET_TO_USER_DATA_AREA 04000    /* 04000 bytes or Word 01000 */\

extern serverGlobalData sgd;        /* The Server Global Data (SGD),*/
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
 * Note:
 * These functions clear status and aux_status to -1 before the
 * COMAPI call.  This is to make it very clear that these values
 * are being set by the COMAPI and NetworkAPI routines ( also a
 * value of -1 is an error indicator in BSD sockets on which
 * COMAPI is modelled, thats another reason a clearing value of -1
 * is used.
 */

/*
 * Function Register_Application
 *
 * Registers this application program with COMAPI.  This allows
 * each activity access to the sockets created by the other activities
 * in the JDBC Server, without having to use the bequeath and
 * inherit approach (TCP_BEQUEATH and TCP_INHERIT).
 *
 * Use of the routine is appropriate in the JDBC Server due to
 * way it utilizes COMAPI sockets.  This will provide some
 * COMAPI performance since two COMAPI calls are avoided each
 * time a new JDBC Client is accepted by ICL and the Server Worker.
 */
int Register_Application(){

   /* int session_parameter=02000000;  This value would use shared sessions */

   /* Set parameter so COMAPI knows sessions are not shared across activities. */
   int session_parameter=00000000; /* This value indicates sessions are not shared */

   int status;
   int aux_status;
   int socket=0;

   #pragma interface tcp_register_with_bdi
   void (*tcp_register_with_bdi)(int *, int *);
   union{
      void (* rregister)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_REGISTER;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_register_with_bdi= tcp_call.rregister;

   /*
    * Before registering with COMAPI, first perform a deregistration
    * with COMAPI to insure that we have not left ourselves registered
    * from a previous JDBC Server execution that terminated in an
    * abnormal manner, e.g. it was killed with an @@x tio.
    *
    */
   Deregister_Application();

   /* Perform the COMAPI Register and check the returned status */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;

   /* Perform the COMAPI Register and check the returned status */
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_register_with_bdi(&session_parameter, &status); /* TCP_REGISTER(&session_parameter, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_REGISTER", socket, &status, &aux_status);
   return status;                    /* return status to caller */
}


/*
 * Procedure Deregister_Application
 *
 * De-registers this application program with COMAPI.  This is done to
 * gracefully disconnect the JDBC Server (e.g. ICL) from COMAPI.
 *
 * COMAPI will disconnect the JDBC Server from the COMAPI subsystem
 * and automatically TCP_CLOSE any sessions (sockets) that the JDBC
 * Server may have had open.
 *
 * Note: there is no returned status from TCP_DEREGISTER, so fake it for
 * the check_COMAPI_status call that follows, so it can log call if needed.
 */
void Deregister_Application(){

   int status=0;
   int aux_status=0;
   int socket=0;

   #pragma interface tcp_deregister_with_bdi
   void (*tcp_deregister_with_bdi)();
   union{
      void (* deregister)();
      unsigned short packet[16];
   } tcp_call = TCP_DEREGISTER;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_deregister_with_bdi= tcp_call.deregister;

   /* Perform the COMAPI Deregister and "check the returned status" */
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_deregister_with_bdi(); /* TCP_DEREGISTER() - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_DEREGISTER", socket, &status, &aux_status);
}

/**
 * deregComapiAndStopTasks
 *
 * This function deregisters this ICL instance's activity with COMAPI
 * and then stops all active tasks in the application
 * (thus shutting down the JDBC server).
 *  
 * @param   errNumber         The server messageNumber.
 *
 * @param   insert1           An optional insert string to replace '{0}'
 *                            in the message line.
 *
 * @param   insert2           An optional insert string to replace '{1}'
 *                            in the message line.
 */

void deregComapiAndStopTasks(int messageNumber, char* insert1, char* insert2) {

    Deregister_Application();

    stop_all_tasks(messageNumber, insert1, insert2);

    /* Control does not return here,
       since all tasks are shut down. */
}

/*
 * Function Start_Socket
 *
 * Sets up the initial COMAPI session and returns a socket number ( a COMAPI
 * session table id) that is used as the JDBC Server socket that the Initial
 * Connection Listener will use.
 *
 * After making the TCP_SESSION call, this routine will have the status checked.
 * If a fatal error occurs, an error status and message is returned.
 *
 * If the status indicates that the connection was not made due to the COMAPI
 * system not being available, this code will try up to num_connection_tries times
 * to connect, waiting wait_time (in milliseconds)in between tries. If this is not
 * successful, then an error status and message is returned.
 *
 * Normal execution will return a status value of 0.
 */
int Start_Socket(int *socket_ptr){

   /* int socket = *socket_ptr;  - this was the original call */
   int socket = 0; /* Just get the session, do not associate another session to it. */
   int status;
   int aux_status;

  #pragma interface tcp_session_with_bdi
   void (*tcp_session_with_bdi)(int *, int *);
   union{
      void (* session)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_SESSION;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   /* printf("In Start_Socket, current COMAPI bdi is %o\n",tcp_call.packet[2]); */

   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   /* printf("In Start_Socket, new COMAPI bdi is %o\n",tcp_call.packet[2]); */

   tcp_session_with_bdi= tcp_call.session;

   /*
    * get the socket number from the caller's code and clear the status field so
    * we can easily detect that COMAPI has changed it during its code.
    */
   /* Try to set up a new session with COMAPI */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_session_with_bdi(&socket, &status); /* TCP_SESSION(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_SESSION", socket, &status, &aux_status);

   /*
    * Return the session table id as the socket value.  If status is 0, then we
    * were successful at getting a COMAPI session, otherwise it indicates we could
    * not establish the COMAPI session.
    */
   if (status == 0) {
      /* Everything went fine, so return the socket number (COMAPI session table id)*/
      *socket_ptr = socket;
   } else {
      /* Could not get a socket, so make sure
       * that socket = 0 to indicate that no socket
       * is present.
       */
      *socket_ptr=0;
   }
   return status;  /* socket value set above. */
}

/*
 * Function Bequeath_Socket.
 *
 * Routine informs COMAPI that control of the socket will be passed to another
 * activity.
 *
 * In the case of the JDBC Server's server socket, its going to belong to the ICL.
 * In the case of a JDBC Client socket, its going to belong to one of the Server
 * Workers.
 *
 * Normal execution will return a status value of 0.
 */
int Bequeath_Socket(int socket){

   int status;
   int aux_status;

   #pragma interface tcp_bequeath_with_bdi
   void (*tcp_bequeath_with_bdi)(int *, int *);
   union{
      void (* bequeath)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_BEQUEATH;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_bequeath_with_bdi= tcp_call.bequeath;

   /* Perform the COMAPI Bequeath and check the returned status */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_bequeath_with_bdi(&socket, &status); /* TCP_BEQUEATH(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_BEQUEATH", socket, &status, &aux_status);
   return status;                    /* return status to caller */
}

/*
 * Function Inherit_Socket
 *
 * This procedure tells COMAPI that control of the socket is to be passed to
 * this calling activity.
 *
 * In the case of the JDBC Server's server socket, its going to belong to the
 * ICL activity. In the case of a JDBC Client socket, its going to belong to one
 * of the Server Workers.
 *
 * Normal execution will return a status value of 0.
 */
int Inherit_Socket(int socket)
{
   int status;
   int aux_status;

   #pragma interface tcp_inherit_with_bdi
   void (*tcp_inherit_with_bdi)(int *, int *);
   union{
      void (* inherit)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_INHERIT;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_inherit_with_bdi= tcp_call.inherit;

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_inherit_with_bdi(&socket, &status); /* TCP_INHERIT(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_INHERIT", socket, &status, &aux_status);
   return status;                      /* return status to caller */
}


/*
 * Function Listen_Port
 *
 * This routine informs COMAPI that the calling activity will listening on the
 * socket indicated at the specified local port.  The main caller of this routine
 * is the Initial Connection Listener which is telling COMAPI that it will be
 * listening to the server socket waiting for new JDBC Client requests.
 *
 * The TCP_LISTEN command is told to listen on port port_number and is given the
 * maximum queue size max_queue_size.
 *
 * If the TCP_LISTEN command fails, then the server socket is TCP_CLOSE'ed and
 * the error status is returned to the caller for processing.
 *
 * Normal execution will return a status value of 0.
 */
int Listen_Port(int socket, int max_queue_size, int interface,
                v4v6addr_type listen_ip_address, int listen_port){

   int close_status;
   int status;
   int aux_status;
   int listen_socket;
   int qsize = max_queue_size; /* no more than max_queue_size clients to queue up */
   /* Create a union of version0 (IPv4) and version1 (IPv4/IPv6) lrct tables */
   union {
      struct lrct_type   packet; /* Used for referencing only */
      struct lrct_type   v0;     /* Version 0 of packet format */
      struct lrctv1_type v1;     /* Version 1 of packet format */
   } lrct;

   #pragma interface tcp_listen_with_bdi
   void (*tcp_listen_with_bdi)(int *, struct lrct_type *, int *, int *);
   union{
      void (* listen)(int *, struct lrct_type *, int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_LISTEN;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_listen_with_bdi= tcp_call.listen;

   /*
    * Allow clients to accessing us on the listen IP address and port specified.
    *
    * If the listen IP address is 0, then clients can access listen socket through any
    * host IP address that's made accessible through the network adapter (e.g. CPCOMM).
    * If the listen IP address is not 0, then clients can access listen socket only
    * through that specified IP address assuming its accessible through the network
    * adapter.  If the listen IP address is of the form 127.xxx.yyy.zzz COMAPI assumes
    * this local host IP address is for by-passing the network adapter, i.e., COMAPI
    * will handle data send and recieves internally without using the network adapters,
    * thus providing a performance gain for JDBC clients accessing the JDBC Server on the
    * same 2200 host.
    */

   /* Test if COMAPI supports IPv6 */
   if (act_wdePtr->comapi_IPv6 == 0) { /* Supports IPv4 only, use version 0 lrct table */
       lrct.v0.version = 0;
       lrct.v0.interface = interface;             /* use interface provided */
       lrct.v0.acquired_port_number = 0;
       lrct.v0.local_port_number = listen_port;
       lrct.v0.local_ip_address = listen_ip_address.v4v6.v4;
       lrct.v0.remote_port_number = 0;
       lrct.v0.remote_ip_address = 0;
   } else { /* Supports IPv6, use version 1 lrct table */
       lrct.v1.version = 1;
       lrct.v1.interface = interface;             /* use interface provided */
       lrct.v1.acquired_port_number = 0;
       lrct.v1.local_port_number = listen_port;
       lrct.v1.local_ip_address = listen_ip_address;
       lrct.v1.remote_port_number = 0;
       lrct.v1.remote_ip_address.family = 0;
       lrct.v1.remote_ip_address.zone = 0;
       lrct.v1.remote_ip_address.v4v6.v6_dw[0] = 0;
       lrct.v1.remote_ip_address.v4v6.v6_dw[1] = 0;
   }

   listen_socket=socket;
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_listen_with_bdi(&listen_socket, &lrct.packet, &qsize, &status); /* TCP_LISTEN(&listen_socket, &lrct.packet, &qsize, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

   /* Test if sgd.debug says to trace the COMAPI calls or to force a COMAPI status. */
   if (TEST_SGD_DEBUG_TRACE_COMAPI || sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
         printf("In Listen_Port(), ICL_num=%d, func_name=%s, listen_ip_address=%d, port=%d, socket=%d, actual status=%d,%d forced status=%d\n",
             act_wdePtr->ICL_num, "TCP_LISTEN", listen_ip_address.v4v6.v4, listen_port, socket, (status & 0000000777777),((status & 0777777000000) >> 18),
             sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num]);

         /* Test if we are forcing an explicit COMAPI error status */
         if (sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
              status=sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num];
         }
   }

   /*
    * Test if we got a network down status (SENETDOWN) or the appropriate
    * TSAM errors on a CMS connection that indicate we do not have a
    * server socket with CMS.  Since we are handling these network cases
    * specially in the ICL Initialize_ServerSockets() code, do not check/log
    * these statuses unless Server COMAPI debugging is turned on. (Note: when
    * COMAPI debug is on, the TCP_LISTEN and is tested statuses will be traced.)
    *
    * Check status in all other cases as normal.
    */

   if (!(  (status == SENETDOWN || status == (030005 << 18) + SEBADTSAM ||
            status == (030027 << 18) + SEBADTSAM || status == (030032 << 18) + SEBADTSAM ||
            status == (030046 << 18) + SEBADTSAM || status == (030000 << 18) + SEBADTSAM)
         )) {
     if (!TEST_SGD_DEBUG_TRACE_COMAPI){
       check_COMAPI_status("TCP_LISTEN", listen_socket, &status, &aux_status);
     }
   }
   if (status != 0)
     {
      close_status = -1;  /* clear close_status, aux_status so checking is easier */
      aux_status=-1;
      act_wdePtr->in_COMAPI = CALLING_COMAPI;
      close_status = Close_Socket(listen_socket);
      act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
      check_COMAPI_status("TCP_CLOSE", listen_socket, &close_status, &aux_status);
     }
   return status;    /* return TCP_LISTEN status to caller */
}

/*
 * Function Close_Socket
 *
 * This procedure shuts down a COMAPI socket (session table id).  This is used
 * either when an error has occurred manipulating an existing socket, or as part
 * of a normal shutdown with COMAPI. The socket is disconnected and then closed
 * so that the client is handled correctly.
 *
 * Normal execution will return a status value of 0.
 */
int Close_Socket(int socket)
{
   int status;
   int aux_status;

   #pragma interface tcp_close_with_bdi
   void (*tcp_close_with_bdi)(int *, int *);
   union{
      void (* close)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_CLOSE;

   /*
    * Test if there is no session table id (either an
    * socket id of 0 or -1). If so, there is no
    * work to do.  Return a status of 0 as would
    * be expected.  This avoids the possible
    * 10009 error that COMAPI would return, and we
    * would log, when there is no actual socket.
    */
   if (socket ==0 || socket == -1){
    return 0; /* no socket, no error */
   }

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_close_with_bdi= tcp_call.close;

/* printf("We are TCP_CLOSEing socket %d ICL_num=%d\n", socket, act_wdePtr->ICL_num); */

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_close_with_bdi(&socket, &status); /* TCP_CLOSE(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_CLOSE", socket, &status, &aux_status);
   return status;                       /* return status to caller */
}

/*
 * Function Inherit_Close_Socket  -  Not currently used.
 *
 * This procedure tells COMAPI that control of the socket is to be passed to this
 * calling activity and then it closes the socket.  This routine is only used by
 * a Server Worker that cannot properly start up, but has already accepted the
 * responsibility of inheriting a new JDBC Client connection (i.e. when a new
 * Server Worker activity had to be started and it cannot acquire a WDE).
 *
 * Normal execution will return a status value of 0.
 */
 int Inherit_Close_Socket(int socket)
 {

   int status;
   int aux_status;

   #pragma interface tcp_iinherit_with_bdi
   void (*tcp_iinherit_with_bdi)(int *, int *);
   union{
      void (* inherit)(int *, int *);
      unsigned short packet[16];
   } tcp_callI = TCP_INHERIT;

   #pragma interface tcp_cclose_with_bdi
   void (*tcp_cclose_with_bdi)(int *, int *);
   union{
      void (* close)(int *, int *);
      unsigned short packet[16];
   } tcp_callC = TCP_CLOSE;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_callI.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_iinherit_with_bdi= tcp_callI.inherit;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_callC.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_cclose_with_bdi= tcp_callC.close;

  /* First, inherit the socket */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_iinherit_with_bdi(&socket, &status); /* TCP_INHERIT(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_INHERIT", socket, &status, &aux_status);
   if (status != 0) {
      /* Couldn't inherit the socket, return error status */
      return status;
      }

   /* Now Close the socket */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_cclose_with_bdi(&socket, &status); /* TCP_CLOSE(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_CLOSE", socket, &status, &aux_status);
   return status;                       /* return status to caller */
}

/*
 * Function Set_Socket_Options
 * This function calls COMAPI to set an option for the
 * specified socket. It is possible to set multiple options
 * on a single call using the COMAPI-MULTIPLE-OPT option.
 *
 * Normal execution will return a status value of 0.
 */
 int Set_Socket_Options(int socket, int *option_name_ptr, int *option_value_ptr
                        , int *option_length_ptr)
 {

   int status;
   int aux_status;

   #pragma interface tcp_setopts_with_bdi
   void (*tcp_setopts_with_bdi)(int *, int *, int *, int *, int *);
   union{
      void (* setopts)(int *, int *, int *, int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_SETOPTS;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_setopts_with_bdi= tcp_call.setopts;

   /* Now set the option for the socket */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_setopts_with_bdi(&socket, option_name_ptr, option_value_ptr, option_length_ptr,
                      &status); /* TCP_SETOPTS(&socket, option_name_ptr, option_value_ptr
                                               , option_length_ptr, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

   /* Test if sgd.debug says to trace the COMAPI calls or to force a COMAPI status. */
   if (TEST_SGD_DEBUG_TRACE_COMAPI || sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
         printf("In Set_Socket_Options(), ICL_num=%d, func_name=%s, option_name=%d, option_value= %d, socket=%d, actual status=%d,%d forced status=%d\n",
             act_wdePtr->ICL_num, "TCP_SETOPTS", *option_name_ptr,*option_value_ptr, socket, (status & 0000000777777),((status & 0777777000000) >> 18),
             sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num]);

         /* Test if we are forcing an explicit COMAPI error status */
         if (sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
              status=sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num];
         }
   }

   check_COMAPI_status("TCP_SETOPTS", socket, &status, &aux_status);
   return status;
 }


/*
 * Function Pass_Event
 *
 * This procedure tells COMAPI that an event should be passed to the
 * socket (session table id) indicated.  The socket must be in either a
 * connected or listening state.
 *
 * This function is used to notify the Initial Connection Listener, which
 * is normally waiting in Accept_Client on a COMAPI TCP_ACCEPT call, that
 * a shutdown of some type is requested/or in progress (e.g.
 * ICLShutdownState = ICL_SHUTDOWN_IMMEDIATELY).
 *
 * Normal execution will return a status value of 0.
 */
int Pass_Event()
{
   int i;
   int j;
   int num_server_sockets=0;
   int a_status=0;
   int status;
   int aux_status;

   #pragma interface tcp_event_with_bdi
   void (*tcp_event_with_bdi)(int *, int *);
   union{
      void (* event)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_EVENT;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_event_with_bdi= tcp_call.event;


   /*
    * We must pass the event to all active Server Sockets
    * in all the ICL's.
    */
    for (i = 0; i < sgd.num_COMAPI_modes; i++){
        for (j = 0; j < MAX_SERVER_SOCKETS; j++){

            /* Is there a server socket active? */
            if (sgd.server_socket[i][j] != 0){
                /*
                 * Yes, do a TCP_EVENT on this server socket.
                 * Keep a count of the number of active server
                 * sockets.
                 */
                num_server_sockets++; /* count the number of active server sockets*/
                tcp_call.packet[2]= sgd.ICLcomapi_bdi[i]; /* get the bdi for this COMAPI system. */
                status = -1;     /* clear status, aux_status - makes checking easier */
                aux_status=-1;
                act_wdePtr->in_COMAPI = CALLING_COMAPI;
                tcp_event_with_bdi(&sgd.server_socket[i][j], &status); /* TCP_EVENT(&sgd.server_socket[i][j],
                                                                                       &status) - with current COMAPI bdi */
                act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

                /* printf("In Pass_event(), ICL_num=%d, i=%d, j=%d, comapi_bdi=%o, status=%d\n",
                           act_wdePtr->ICL_num, i, j, sgd.server_socket[i][j], sgd.ICLcomapi_bdi[i], status); */

                /* Test if sgd.debug says to trace the COMAPI calls or to force a COMAPI status. */
                if (TEST_SGD_DEBUG_TRACE_COMAPI || sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
                      printf("In Pass_Event(), ICL_num=%d, func_name=%s, i=%d, j=%d, socket=%d, actual status=%d,%d forced status=%d\n",
                             act_wdePtr->ICL_num, "TCP_EVENT", i, j, sgd.server_socket[i][j], (status & 0000000777777),
                             ((status & 0777777000000) >> 18), sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num]);

                      /* Test if we are forcing an explicit COMAPI error status */
                      if (sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
                            status=sgd.forced_COMAPI_error_status[i];
                      }
                }

                /*
                 * If the server socket is no longer present (ICL is already shutting
                 * down), then do not check/log that status unless Server debugging
                 * is turned on. Check all other non-zero statuses in all other cases
                 * as normal.
                 */
                if ( status !=0 && !((status == SEBADF) &&  (sgd.debug == 0)) ){
                  check_COMAPI_status("TCP_EVENT", sgd.server_socket[i][j], &status, &aux_status);
                }
                if (status !=0){
                    /* Not 0, keep one of the statuses to return to caller */
                    a_status=status;
                }
            }
        } /* end for j */
    } /* end for i */

    /* Were there any active Server Sockets? */
    if (num_server_sockets > 0){
        /* all done, return status. */
        return a_status; /* return a status to caller */
    }

    /* No server sockets, return NO_SERVER_SOCKETS_ACTIVE as a flag value to caller. */
    return NO_SERVER_SOCKETS_ACTIVE;
}

/*
 * Function Pass_Event_to_SW
 *
 * This procedure tells COMAPI that an event should be passed to the Server Worker
 * socket (session table id) indicated.  The socket must be in either a connected
 * or listening state.
 *
 * This function is used to notify a Server Worker waiting on a Receive_Data
 * (a COMAPI TCP_RECEIVE call), that an immediate shutdown is requested or in
 * progress (i.e. serverWorkerShutdownState = WORKER_SHUTDOWN_IMMEDIATELY).
 *
 * The first parameter is the client socket to send the event notifcation.
 * The second parameter is the COMAPI bdi for the COMAPI mode that supports
 * the client socket.
 *
 * If the Server Worker has already processed the immediate shutdown, the client
 * socket may be closed.  This is OK, and any ComAPI status will be ignored.
 *
 * Execution does not return a status. Any abnormal statuses are logged.
 */
void Pass_Event_to_SW(int socket, int comapi_bdi)
{
   int status;
   int aux_status;

   #pragma interface tcp_event1_with_bdi
   void (*tcp_event1_with_bdi)(int *, int *);
   union{
      void (* event)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_EVENT;

   /* Place appropriate COMAPI bdi for the socket into a usable procedure call. */
   tcp_call.packet[2]= comapi_bdi; /* use the COMAPI bdi for the socket */
   tcp_event1_with_bdi= tcp_call.event;

   /* We must pass a user event to the provided Server Worker's client socket */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_event1_with_bdi(&socket, &status); /* TCP_EVENT(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

   /*
    * Test for an OK status or an indication that the socket is already closed.
    * Both statuses are acceptable and need no further checking or logging.
    */
   if (status !=0 ){
     /*
      *  We got a COMAPI status.  Check if its just "session (socket) not found",
      *  which we can ignore.
      */
      if ( status != SEBADACT &
           status != SEBADF &
           status != SENOTCONN   ){
         /* Some other status.  Check it and log it as appropriate. */
         check_COMAPI_status("TCP_EVENT", socket, &status, &aux_status);
      }
   }
}

/*
 * Function Accept_Client
 *
 * Accepts a new JDBC Client initial incoming message on the ICL's server socket.
 * COMAPI will assign a new socket (session table id) for the socket conection
 * it will maintain for the network dialogue with this new JDBC Client.
 *
 * This routine returns the client socket, the clients IP address, and a maximum
 * size for the Response packets that can be generated (based on whether CPCOMM
 * or CMS is being used as the underlying network service.
 *
 * If any errors occur, an error status is returned to the caller for their
 * processing. The server socket is not shut down and remains in a listen state.
 *
 * Normal execution will return a status value of 0.
 */
int Accept_Client(int server_socket, int * new_client_socket_ptr
                  , v4v6addr_type * remote_ip_address, int * network_interface){


   int status;
   int errorStatus;
   int aux_status;
   int new_client_socket;
   int socket;
   int COMAPI_option_name;
   int COMAPI_option_value;
   int COMAPI_option_length;
   char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

   int user_ident = 0;   /* Always set to 0, because JDBC Server ICL is not using
                             the PROXY listener feature.                */
   /* Create a union of version0 (IPv4) and version1 (IPv4/IPv6) lrct tables */
   union {
      struct lrct_type   packet; /* Used for referencing only */
      struct lrct_type   v0;     /* Version 0 of packet format */
      struct lrctv1_type v1;     /* Version 1 of packet format */
   } lrct;

   #pragma interface tcp_accept_with_bdi
   void (*tcp_accept_with_bdi)(int *, int *, int *, struct lrct_type *, int *);
   union{
      void (* accept)(int *, int *, int *, struct lrct_type *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_ACCEPT;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_accept_with_bdi= tcp_call.accept;


   /*
    * Attempt to receive a new JDBC client from a server socket.
    *
    * Note: We know which ICL activity instance by its ICL_num
    * value in its local WDE.
    */
        /* structure lrct will be filled in upon return
        * from TCP_ACCEPT, but clear it now so later
        * debugging would be easier.
        */

       /* Test if COMAPI supports IPv6 */
       if (act_wdePtr->comapi_IPv6 == 0) { /* Supports IPv4 only, use version 0 lrct table */
           lrct.v0.version = 0;
           lrct.v0.interface = 0;
           lrct.v0.acquired_port_number = 0;
           lrct.v0.local_port_number = 0;
           lrct.v0.local_ip_address = 0;
           lrct.v0.remote_port_number = 0;
           lrct.v0.remote_ip_address = 0;
       } else { /* Supports IPv6, use version 1 lrct table */
           lrct.v1.version = 1;
           lrct.v1.interface = 0;
           lrct.v1.acquired_port_number = 0;
           lrct.v1.local_port_number = 0;
           lrct.v1.local_ip_address.family = 0;
           lrct.v1.local_ip_address.zone = 0;
           lrct.v1.local_ip_address.v4v6.v6_dw[0] = 0;
           lrct.v1.local_ip_address.v4v6.v6_dw[1] = 0;
           lrct.v1.remote_port_number = 0;
           lrct.v1.remote_ip_address.family = AF_INET6;
           lrct.v1.remote_ip_address.zone = 0;
           lrct.v1.remote_ip_address.v4v6.v6_dw[0] = 0;
           lrct.v1.remote_ip_address.v4v6.v6_dw[1] = 0;
       }

       socket=server_socket;
       status = -1;     /* clear status, aux_status - makes checking easier */
       aux_status=-1;

       act_wdePtr->in_COMAPI = CALLING_COMAPI;
       tcp_accept_with_bdi(&socket, &new_client_socket, &user_ident, &lrct.packet,
                        &status); /* TCP_ACCEPT(&socket, &new_client_socket, &user_ident,
                                               &lrct.packet, &status) - with current COMAPI bdi */
       act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
       check_COMAPI_status("TCP_ACCEPT", socket, &status, &aux_status);

       /*  printf("In Accept_client(), server_socket=%d, comapi_bdi=%o, status=%d\n",
                   socket, act_wdePtr->comapi_bdi, status); */

       if (status == 0){
       /*
        * We have a new client, return the client socket and IP location to caller.
        */

       /* Test if sgd.debug says to trace the COMAPI calls or to force a COMAPI status. */
       if (TEST_SGD_DEBUG_TRACE_COMAPI || sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
         printf("In Accept_Client() after TCP_ACCEPT, ICL_num=%d, new client socket= %d\n",
                act_wdePtr->ICL_num, new_client_socket);
       }

       *new_client_socket_ptr = new_client_socket;

       /* Test if COMAPI supports IPv6 */
       if (act_wdePtr->comapi_IPv6 == 0) {
           (*remote_ip_address).v4v6.v4 = lrct.v0.remote_ip_address;
           (*remote_ip_address).family = AF_INET;
           *network_interface = lrct.v0.interface;
       } else {
           *remote_ip_address = lrct.v1.remote_ip_address;
           *network_interface = lrct.v1.interface;
       }

       return status;
       }
       else {
           /*
            * Status was not 0, check if it's a timeout or an event
            * (i.e. a waking up of the TCP_ACCEPT without a new client),
            * or it is an unexpected error status.
            */
           if (status == SETIMEDOUT){
             /*
              * It's a timeout condition.
              */
              /* log the timeout because it was unexpected ( normally they are not logged) */
              log_COMAPI_error("TCP_ACCEPT", socket, status, aux_status);
              *new_client_socket_ptr = 0;   /* indicate no new client socket */
              return status; /* return timeout status */
           }
           else if ((status & 0000000777777) == SEHAVEEVENT){
             /*
              * USER EVENT.
              *
              * It's a user event.  This is only done by the Console Handler
              * or other JDBC Server activity to wake up the ICL from the
              * TCP_ACCEPT.
              *
              * First test for a posted value for the server receive timeout
              * value,the server send timeout value, or the debug value.
              *
              * Note: Since there may be multiple ICL's active, each ICL
              * will perform these steps so the SGD values will reflect
              * the successful assignment by any of the active server sockets
              * across the multiple ICL's ( i.e., one server socket success
              * means the sgd value will be updated. Also the posting variable
              * is not cleared so that other server sockets can apply the posted
              * value as well.  This means that an event will cause all the
              * earlier posted values to be applied as well as the new value that
              * this event is requesting to be changed.
              *
              */
                if ((sgd.postedServerReceiveTimeout != 0)
                        || (sgd.postedServerSendTimeout != 0)
                        || (sgd.postedCOMAPIDebug != 0)) {
                    COMAPI_option_length=4;

                    /* Clear the status (makes checking easier) */
                    errorStatus = -1;

                    /* Call COMAPI to change the server receive timeout value */
                    if (sgd.postedServerReceiveTimeout != 0) {
                        COMAPI_option_name=COMAPI_RECEIVE_OPT;
                        COMAPI_option_value=sgd.postedServerReceiveTimeout;

                        act_wdePtr->in_COMAPI = CALLING_COMAPI;
                        errorStatus=Set_Socket_Options(server_socket,
                            &COMAPI_option_name,
                            &COMAPI_option_value,
                            &COMAPI_option_length);
                        act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

                        /* If the value was accepted by COMAPI,
                           change the sgd value.
                           */
                        if (errorStatus == 0) {
                            sgd.server_receive_timeout =
                                sgd.postedServerReceiveTimeout;
                        } else {
                            getLocalizedMessageServer(
                                SERVER_COMAPI_RECEIVE_TIMEOUT_ERROR, NULL,
                                NULL, SERVER_LOGS, msgBuffer);
                                    /* 5327 COMAPI rejected receive timeout
                                            value on server socket */
                        }
                    }

                    /* Call COMAPI to change the server send timeout value */
                    if (sgd.postedServerSendTimeout != 0) {
                        COMAPI_option_name=COMAPI_SEND_OPT;
                        COMAPI_option_value=sgd.postedServerSendTimeout;

                        act_wdePtr->in_COMAPI = CALLING_COMAPI;
                        errorStatus=Set_Socket_Options(server_socket,
                            &COMAPI_option_name,
                            &COMAPI_option_value,
                            &COMAPI_option_length);
                        act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

                        /* If the value was accepted by COMAPI,
                           change the sgd value.
                           */
                        if (errorStatus == 0) {
                            sgd.server_send_timeout =
                                sgd.postedServerSendTimeout;
                        } else {
                            getLocalizedMessageServer(
                                SERVER_COMAPI_SEND_TIMEOUT_ERROR, NULL,
                                NULL, SERVER_LOGS, msgBuffer);
                                    /* 5328 COMAPI rejected send timeout
                                            value on server socket */
                        }
                    }

                    /* Call COMAPI to change the debug value */
                    if (sgd.postedCOMAPIDebug != 0) {
                        COMAPI_option_name=COMAPI_DEBUG_OPT;

                        if (sgd.postedCOMAPIDebug == -1) {
                            COMAPI_option_value = 0;
                        } else {
                            COMAPI_option_value = sgd.postedCOMAPIDebug;
                        }

                        act_wdePtr->in_COMAPI = CALLING_COMAPI;
                        errorStatus=Set_Socket_Options(server_socket,
                            &COMAPI_option_name,
                            &COMAPI_option_value,
                            &COMAPI_option_length);
                        act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

                        /* If the value was accepted by COMAPI,
                           change the sgd value.
                           */
                        if (errorStatus == 0) {
                            if (sgd.postedCOMAPIDebug == -1) {
                                sgd.COMAPIDebug = 0;
                            } else {
                                sgd.COMAPIDebug = sgd.postedCOMAPIDebug;
                            }
                        } else {
                            getLocalizedMessageServer(
                                SERVER_COMAPI_DEBUG_ERROR, NULL, NULL,
                                SERVER_LOGS, msgBuffer);
                                    /* 5329 COMAPI rejected debug value on
                                       server socket */
                        }
                    }

                /* End of the code to handle the server receive timeout,
                   server send timeout, or COMAPI debug.
                   Don't return.
                   Control passes back to the TCP_ACCEPT in the while loop.
                   */

                } else {
                    /*
                     * Currently, just send the status back to the ICL
                     * code for handling.
                     */
                    *new_client_socket_ptr = 0;   /* indicate no new client socket */
                    return status; /* return user event status */
                }
           } /* end of user event code */

           else {
             /*
              * An unexpected error status, not a timeout or a passed event. So
              * just send it back to caller.
              */
             *new_client_socket_ptr = 0;   /* indicate no new client socket */
             return status;  /* return status to caller */
           }
       }

       /* Code should not reach here. Indicate that we didn't get a client.
          Let the ICL handle the error. */
       return SEBASE; /* Use error 10000, Its not specified in COMAPI book  */
}

/*
 * Function Select_Socket
 *
 * This function checks if there are any incoming client socket
 * requests against any of the server sockets on the socket list
 * passed to this function as the input parameter.
 *
 * If any errors occur, an error status is returned to the caller
 * for their processing. The server sockets being tested are not
 * shut down and remain in a listen state.
 *
 * Normal execution will return a status value of 0.
 */
int Select_Socket(int * socket_list){

   int status;
   int aux_status;

   int no_list = 0;
   int wait_time = DEFAULT_COMAPI_RECONNECT_WAIT_TIME; /* Wait no more than the re-connect time for the list,
                             matches the other COMAPI (re)- establishment twait times */
   int id_format = 0;     /* indicate return session identifiers */

   #pragma interface tcp_select_with_bdi
   void (*tcp_select_with_bdi)(int [],int [],int [], int *, int *, int *);
   union{
      void (* select)(int [],int [],int [],int *, int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_SELECT;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_select_with_bdi= tcp_call.select;

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_select_with_bdi(socket_list, &no_list, &no_list, &wait_time,
                       &id_format, &status); /* TCP_SELECT(socket_list, &no_list, &no_list,&wait_time,
                                                           &id_format, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_SELECT", socket_list[1], &status, &aux_status);
   return status;
}

/*
 * Function Rescind_Close_Socket
 *
 * This function rescinds and closes the server socket
 * passed as an argument to the function.  There is
 * no status value returned. Any COMAPI errors are logged.
 */
void Rescind_Close_Socket(int socket){

   int status;
   int aux_status;

   #pragma interface tcp_rescind_with_bdi
   void (*tcp_rescind_with_bdi)(int *, int *);
   union{
      void (* rescind)(int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_RESCIND;

   #pragma interface tcp_ccclose_with_bdi
   void (*tcp_ccclose_with_bdi)(int *, int *);
   union{
      void (* close)(int *, int *);
      unsigned short packet[16];
   } tcp_callCC = TCP_CLOSE;

   /*
    * Test if there is no session table id (either an
    * socket id of 0 or -1). If so, there is no
    * work to do.  Return a status of 0 as would
    * be expected.  This avoids the possible
    * 10009 error that COMAPI would return, and we
    * would log, when there is no actual socket.
    */
   if (socket ==0 || socket == -1){
    return; /* no socket, no error */
   }

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_rescind_with_bdi= tcp_call.rescind;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_callCC.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_ccclose_with_bdi= tcp_callCC.close;

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_rescind_with_bdi(&socket, &status); /* TCP_RESCIND(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_RESCIND", socket, &status, &aux_status);

   /* Now Close the socket */
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_ccclose_with_bdi(&socket, &status); /* TCP_CLOSE(&socket, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   check_COMAPI_status("TCP_CLOSE", socket, &status, &aux_status);
}

/*
 * Function Receive_Data
 *
 * Routine receives a buffer of bytes from COMAPI from the socket specified.
 *
 * The location where to put the received bytes, and the number of bytes requested
 * are provided as parameters.  The caller should insure that the buffer is large
 * enough to contain the number of bytes requested. Also note, COMAPI will wait
 * for incoming data on the socket until the requested number of bytes are received.
 * Therefore it is possible to get control back if all the desired bytes are not
 * received within the timeout window. If that occurs, the caller will be given
 * the status of the last TCP_RECEIVE.  The argument values returned by Receive_Data
 * will in all cases reflect the aggregate of all the TCP_RECEIVE's used (i.e.
 * the total bytes recieved up to that point will be returned).
 *
 * The parameter buffer_length indicates how many bytes to request from TCP_RECEIVE.
 * If buffer_length is =< 0 then no bytes are requested and the procedure returns
 * with no error status (0) - caller must deal with this case.
 *
 * The parameter buffer_count_ptr points to where the actual received byte count
 * is stored. Since multiple TCP_RECEIVEs may be needed, since each TCP_RECEIVE can
 * only read MAX_DATA_TCP bytes, this count will be the total number of bytes
 * returned by all of the TCP_RECEIVE's needed to satisfy the buffer_length value.
 *
 * Normal execution will return a status value of 0.
 */
int Receive_Data(int socket, char * bufferPtr, int buffer_length
                 ,int *buffer_count_ptr){

   int status=0;  /* clear it in case buffer_length comes in <= 0 (see note above) */
   int aux_status;
   int receive_socket=socket; /* get a local procedure reference to socket id */

   int num_bytes_to_request_this_call;
   int num_bytes_actually_received_this_call;
   int num_of_bytes_received_so_far;

   #pragma interface tcp_receive_with_bdi
   void (*tcp_receive_with_bdi)(int *, char *, int *, int *, int *);
   union{
      void (* receive)(int *, char *, int *, int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_RECEIVE;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_receive_with_bdi= tcp_call.receive;

   /* Initially no bytes are received, set counts accordingly */
   num_of_bytes_received_so_far=0;
   *buffer_count_ptr = 0;

   /*
    * Loop performing TCP_RECEIVE calls until all the requested
    * bytes are received. Notice, the loop ends when the remaining
    * buffer_length is 0 or less.  Hence, an original request of
    * 0 or less bytes will return to the caller without performing
    * any TCP_RECEIVES.  The caller must handle that situation BEFORE
    * calling Receive_Data.
    *
    * Each loop attempts to read the maximum number of bytes possible,
    * based on the maximum number of bytes per TCP_RECEIVE (i.e.MAX_DATA_TCP).
    *
    * If a given TCP_RECEIVE call receives less data then requested due to
    * receiving a timeout (e.g. part of the data came in and then there was a
    * long wait), then an error status is returned along with the current
    * values for the return parameters.
    *
    * As the loop progresses, the counts and pointers being used will be
    * updated accordingly.
    */
   while (buffer_length > 0){

      /* Determine how big to make this request */
      if (buffer_length <= MAX_DATA_TCP){
        num_bytes_to_request_this_call = buffer_length; /* get whats needed/left */
      } else {
        num_bytes_to_request_this_call = MAX_DATA_TCP;  /* get a MAX_DATA_TCP chunk */
      }

      /*
       * Perform the COMAPI TCP_RECEIVE to get data.
       */
      status = -1;     /* clear status, aux_status, returned byte count - makes checking easier */
      aux_status=-1;
      num_bytes_actually_received_this_call=0;
      act_wdePtr->in_COMAPI = CALLING_COMAPI;
      tcp_receive_with_bdi(&receive_socket, bufferPtr, &num_bytes_to_request_this_call,
                         &num_bytes_actually_received_this_call, &status); /* TCP_RECEIVE(&receive_socket, bufferPtr,
                                                                                        &num_bytes_to_request_this_call,
                                                                                        &num_bytes_actually_received_this_call,
                                                                                        &status) - with current COMAPI bdi */
      act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

      /* Test for closed connection COMAPI returned status values. */
      if ((status >> 18) == EVENT_CLOSE && (status & 0777777) == SEHAVEEVENT || \
         (status & 0777777) == SENOTCONN){
        /* We have a client event (closing of the socket) or the socket is not connected.
         * In either case, return a JDBC defined status code of LOST_CLIENT.
         */
          status = LOST_CLIENT;
          return status;
      }

      /* Test if some other User Event was sent. In this case, we did not get data. */
      if ((status & 0000000777777) == SEHAVEEVENT){
        /* We have a User event.  Likely, the JDBC Server has been given
         * a shutdown command.  Return status to the caller, indicating that
         * no bytes were read.
         */
        check_COMAPI_status("TCP_RECEIVE", receive_socket, &status, &aux_status);
        return status;
      }

      /*
       * Update returned parameter information right now.
       */
      num_of_bytes_received_so_far = num_of_bytes_received_so_far + num_bytes_actually_received_this_call;
      *buffer_count_ptr = num_of_bytes_received_so_far;

      /*
       * Prepare counts and pointers for next possible loop iteration.
       */
      buffer_length=buffer_length - num_bytes_actually_received_this_call;
      bufferPtr = bufferPtr + num_bytes_actually_received_this_call;

      /*
       * Now check the TCP_RECEIVE returned status. If we didn't get all
       * the bytes we requested, this will be caught here in a COMAPI error.
       */
      check_COMAPI_status("TCP_RECEIVE", receive_socket, &status, &aux_status);
      if (status !=0){
      return status;                    /* return status to caller */
      }

   }
   /*
    * All TCP_RECEIVEs done successfully.  The final counts are
    * already in place, so just return to caller. Notice the final
    * status value is 0.
    */
   return status;
}

/*
 * Function Borrow_Qbank
 *
 * This routine borrows a Queue Bank from COMAPI for use in forming
 * the response packet that will be later sent via Send_Data (using
 * COMAPI's TCP_SEND). The address returned to the caller is not
 * the start of the Q-bank, but the start of the User Data Area
 * within the Queue bank.
 *
 * The byte count of the User Data Area and the byte
 * offset to the User Data Area fields are pre-set for the
 * caller.  Also, the Q-Bank link field located
 * at offset 0 of the Queue bank is cleared to avoid problems
 * as indicated by COMAPI staff.
 *
 * Normal execution will return a status value of 0.
 */
int Borrow_Qbank(char **borrowed_qbankPtrPtr, int response_packet_size) {

   int status;
   int aux_status;
   struct qb_user_type *qbankPtr;

   #pragma interface tcp_borrowqb_with_bdi
   void (*tcp_borrowqb_with_bdi)(struct qb_user_type **, int *);
   union{
      void (* borrowqb)(struct qb_user_type **, int *);
      unsigned short packet[16];
   } tcp_call = TCP_BORROWQB;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_borrowqb_with_bdi= tcp_call.borrowqb;

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_borrowqb_with_bdi(&qbankPtr, &status); /* TCP_BORROWQB(&qbankPtr, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

   /* Check status ( use 0 for socket id since no socket was involved */
   check_COMAPI_status("TCP_BORROWQB", 0, &status, &aux_status);
   if (status != 0)
      {
      /* Error occurred, return a NULL Q-bank pointer */
      *borrowed_qbankPtrPtr = NULL;
      }
   else {
      /*
       * Set the byte count and the offset fields of the
       * Q-bank to the User Data Area within the Q-bank
       * and set the first two words of the Q-bank to NULL
       * to clear out the Q-bank chaining link.
       *
       * Note: Setting the byte count now is only an estimate.  The
       * actual byte count to be sent will be determined by Send_DataQB
       * from the response packet that's built in the Q-bank User Data Area.
       */
       qbankPtr->data_bytes_in_this_qb=response_packet_size;
       qbankPtr->data_byte_offset_in_this_qb=OFFSET_TO_USER_DATA_AREA;
       *(char **)qbankPtr=NULL;

      /* return a q-bank pointer that is positioned to the
       * the user data area in the q-bank.  This is where the
       * response packet will be built.
       */
      *borrowed_qbankPtrPtr = ((char *)qbankPtr)+OFFSET_TO_USER_DATA_AREA;
      }
   return status;  /* return the status of the request */
}


/*
 * Function Return_Qbank
 *
 * This routine returns a borrowed Queue Bank from COMAPI.
 * Remember, the pointer provided actually points to the
 * user data area within the Q-bank, so we have to
 * subtract OFFSET_TO_USER_DATA_AREA bytes from the pointer
 * that was passed.
 *
 * Normal execution will return a status value of 0.
 */
int Return_Qbank(char *borrowed_qbankPtr){

   int status;
   int aux_status;
   struct qb_user_type *qbankPtr;

   #pragma interface tcp_returnqb_with_bdi
   void (*tcp_returnqb_with_bdi)(struct qb_user_type **, int *);
   union{
      void (* returnqb)(struct qb_user_type **, int *);
      unsigned short packet[16];
   } tcp_call = TCP_RETURNQB;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_returnqb_with_bdi= tcp_call.returnqb;

   /* Position to start of Q-bank */
   qbankPtr=(void *)(borrowed_qbankPtr - OFFSET_TO_USER_DATA_AREA);

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_returnqb_with_bdi(&qbankPtr, &status); /* TCP_RETURNQB(&qbankPtr, &status) - with current COMAPI bdi */

   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

   /* Check status ( use 0 for socket id since no socket was involved */
   check_COMAPI_status("TCP_RETURNQB", 0, &status, &aux_status);
   return status;
}


/*
 * Function Send_DataQB
 *
 * This routine sends the response packet data in the borrowed
 * Queue Bank to the designated socket. The amount of data to send, in
 * bytes, is provided via the actual_response_packet_size parameter. This
 * parameter is passed by the caller and must be the same as in the response
 * packets length field in the Queue Banks User Data Area.
 *
 * The caller actually provides a Request packet pointer which points
 * to a location in the Q-bank OFFSET_TO_USER_DATA_AREA bytes from the
 * beginning.  To do the COMAPI TCP_QBSEND we have to pass the actual
 * beginning of the Q-bank.
 *
 * Normal execution will return a status value of 0.
 */
int Send_DataQB(int socket, char * borrowed_qbankPtr
                , int actual_response_packet_size){

   int status;
   int aux_status;
   struct  qb_user_type *qbankPtr;

   #pragma interface tcp_qbsend_with_bdi
   void (*tcp_qbsend_with_bdi)(int *, struct qb_user_type **, int *);
   union{
      void (* qbsend)(int *, struct qb_user_type **, int *);
      unsigned short packet[16];
   } tcp_call = TCP_QBSEND;

   /* Place appropriate COMAPI bdi into a usable procedure call. */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use COMAPI bdi in the activities local WDE */
   tcp_qbsend_with_bdi= tcp_call.qbsend;

   /* Position to start of Q-bank */
   qbankPtr=(void *)(borrowed_qbankPtr - OFFSET_TO_USER_DATA_AREA);

   /* now set the number of actual bytes to send. */
   qbankPtr->data_bytes_in_this_qb=actual_response_packet_size;
   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_qbsend_with_bdi(&socket, &qbankPtr, &status); /* TCP_QBSEND(&socket, &qbankPtr, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;
   /*
    * Possible enhancement or requirement -we may need to check
    * if another Qbank pointer has been returned (partial transmission).
    */
   check_COMAPI_status("TCP_QBSEND", socket, &status, &aux_status);
   return status; /* caller can tell if data was sent */
}

/*
 * Function Get_Hostname
 *
 * This routine retrieves the name of the remote host given by the IP
 * address agrument.
 *
 * The first parameter, remote_namePtr, is a pointer to a character buffer
 * sufficent to hold a 256 character host name. The second parameter,
 * remote_ip_address, is the IP address whose name is requested (probably
 * provided from an earlier TCP_Accept call). The third parameter is the WDE
 * pointer to the WDE, either a client's ServerWorker's WDE, or
 * the activity WDE for a JDBC Server activity that has the socket for which
 * the host name is being looked up.
 *
 * If the remote host is not identifiable, a string of characters is returned
 * (only the terminating '\0' is present). Normal execution will return a
 *  status value of 0.
 */
int Get_Hostname(char * remote_namePtr, v4v6addr_type remote_ip_address,
                 workerDescriptionEntry *wdePtr){
   int status;
   int aux_status;
   int name_len;
   /* Create a union of version0 (IPv4) and version1 (IPv4/IPv6) lrct tables */
   union {
      struct lrct_type   packet; /* Used for referencing only */
      struct lrct_type   v0;     /* Version 0 of packet format */
      struct lrctv1_type v1;     /* Version 1 of packet format */
   } lrct;

   #pragma interface tcp_gethostname_with_bdi
   void (*tcp_gethostname_with_bdi)(struct lrct_type *, char *, int *, int *);
   union{
      void (* gethostname)(struct lrct_type *, char *, int *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_GETHOSTNAME;

   /*
    * Place appropriate COMAPI bdi into a usable procedure call. Use the
    *  wdePtr provided to access the correct COMAPI network adapter.
    */
   tcp_call.packet[2]= wdePtr->comapi_bdi; /* use the appropriate COMAPI bdi */
   tcp_gethostname_with_bdi= tcp_call.gethostname;

   /* Fill in an LRCT with required parameters */

   /* Test if COMAPI supports IPv6 */
   if (wdePtr->comapi_IPv6 == 0) { /* Supports IPv4 only, use version 0 lrct table */
       lrct.v0.version = 0;
       lrct.v0.interface = wdePtr->network_interface; /* use the appropriate network */
       lrct.v0.acquired_port_number = 0;
       lrct.v0.local_port_number = 0;
       lrct.v0.local_ip_address = 0;
       lrct.v0.remote_port_number = 0;
       lrct.v0.remote_ip_address = remote_ip_address.v4v6.v4;
   } else { /* Supports IPv6, use version 1 lrct table */
       lrct.v1.version = 1;
       lrct.v1.interface = wdePtr->network_interface; /* use the appropriate network */
       lrct.v1.acquired_port_number = 0;
       lrct.v1.local_port_number = 0;
       lrct.v1.local_ip_address.family = 0;
       lrct.v1.local_ip_address.zone = 0;
       lrct.v1.local_ip_address.v4v6.v6_dw[0] = 0;
       lrct.v1.local_ip_address.v4v6.v6_dw[1] = 0;
       lrct.v1.remote_port_number = 0;
       lrct.v1.remote_ip_address = remote_ip_address;
   }

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_gethostname_with_bdi(&lrct.packet, remote_namePtr, &name_len, &status); /* TCP_GETHOSTNAME(&lrct.packet, remote_namePtr, &name_len, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

   /*
    * Skip checking the COMAPI status and logging - if
    * the status indicates the case where host name is not
    * found, we will just use an unknown host string.
    */
    /* check_COMAPI_status("TCP_GETHOSTNAME", 0, &status, &aux_status); */
   if (status != 0){
    /* Couldn't find host name for IP address, use unknown host name */
    strcpy(remote_namePtr,UNKNOWN_HOST_NAME);
   } else {
    /* Name found, add trailing null to string using the returned name length */
    remote_namePtr[name_len]='\0';
   }
   return status;
}

/*
 * Function Get_Remote_IP_Address
 *
 * This routine retrieves the IP address of the remote host name provided by
 *  using the Domain Name resolver (DNR) of the provided network adapter.
 *
 * The first parameter, remote_namePtr, is a pointer to a character buffer
 * containing the  host name. The second parameter, remote_ip_address, will
 * contain the returned IP address for the host name specified. The third
 * is the network adapter to utilize in the look up process.
 *
 * If the remote host is not identifiable, a COMAPI error status is returned,
 * and the remote_ip_address value is unchanged. Normal execution will
 * return a status value of 0.
 */
int Get_Remote_IP_Address(int socket, char * remote_namePtr, v4v6addr_type *remote_ip_address,
                          int interface){
   int status;
   int aux_status;
   char remote_host_name[HOST_NAME_MAX_LEN+2]; /* give extra room for additional trailing blank */
   /* Create a union of version0 (IPv4) and version1 (IPv4/IPv6) lrct tables */
   union {
      struct lrct_type   packet; /* Used for referencing only */
      struct lrct_type   v0;     /* Version 0 of packet format */
      struct lrctv1_type v1;     /* Version 1 of packet format */
   } lrct;

   #pragma interface tcp_getremote_ip_address_with_bdi
   void (*tcp_getremote_ip_address_with_bdi)(int *, char *, struct lrct_type *, int *);
   union{
      void (* getremote_ip_address)(int *, char *, struct lrct_type *, int *);
      unsigned short packet[16];
   } tcp_call = TCP_GETREMIP;

   /*
    * Place appropriate COMAPI bdi into a usable procedure call. Use the
    *  wdePtr provided to access the correct COMAPI mode.
    */
   tcp_call.packet[2]= act_wdePtr->comapi_bdi; /* use the appropriate COMAPI bdi */
   tcp_getremote_ip_address_with_bdi= tcp_call.getremote_ip_address;

   /* Fill in an LRCT with required parameters */

   /* Test if COMAPI supports IPv6 */
   if (act_wdePtr->comapi_IPv6 == 0) { /* Supports IPv4 only, use version 0 lrct table */
       lrct.v0.version = 0;
       lrct.v0.interface = interface;             /* use interface provided */
       lrct.v0.acquired_port_number = 0;
       lrct.v0.local_port_number = 0;
       lrct.v0.local_ip_address = 0;
       lrct.v0.remote_port_number = 0;
       lrct.v0.remote_ip_address = 0;
   } else { /* Supports IPv6, use version 1 lrct table */
       lrct.v1.version = 1;
       lrct.v1.interface = interface;             /* use interface provided */
       lrct.v1.acquired_port_number = 0;
       lrct.v1.local_port_number = 0;
       lrct.v1.local_ip_address.family = 0;
       lrct.v1.local_ip_address.zone = 0;
       lrct.v1.local_ip_address.v4v6.v6_dw[0] = 0;
       lrct.v1.local_ip_address.v4v6.v6_dw[1] = 0;
       lrct.v1.remote_port_number = 0;
       lrct.v1.remote_ip_address.family = AF_INET6;
       lrct.v1.remote_ip_address.zone = 0;
       lrct.v1.remote_ip_address.v4v6.v6_dw[0] = 0;
       lrct.v1.remote_ip_address.v4v6.v6_dw[1] = 0;
   }

   /* copy the requested host name followed by an additional blank character */
   strcpy(remote_host_name, remote_namePtr);
   strcat(remote_host_name," ");

   status = -1;     /* clear status, aux_status - makes checking easier */
   aux_status=-1;
   act_wdePtr->in_COMAPI = CALLING_COMAPI;
   tcp_getremote_ip_address_with_bdi(&socket, remote_host_name, &lrct.packet, &status); /* TCP_GETREMIP(&socket, remote_host_name, &lrct.packet, &status) - with current COMAPI bdi */
   act_wdePtr->in_COMAPI = NOT_CALLING_COMAPI;

   /*
    printf("In Get_Remote_IP_Address(): status=%d, remote_host_name=""%s"", local_ip_address=%d\n",
           status, remote_host_name, lrct.remote_ip_address);
    */

   /* Check COMAPI status */
   check_COMAPI_status("TCP_GETREMIP", socket, &status, &aux_status);

   /* Check if call returned an IP address */
   if (status == 0){
    /* Name found, return the hosts IP address to use*/

        /* Test if COMAPI supports IPv6 */
        if (act_wdePtr->comapi_IPv6 == 0) {
           (*remote_ip_address).v4v6.v4 = lrct.v0.remote_ip_address;
           (*remote_ip_address).family = AF_INET;
        } else {
           *remote_ip_address = lrct.v1.remote_ip_address;
        }

   }

   return status;
}

/*
 * Procedure IP_to_Char
 *
 * This routine returns an IP address as a string in either the IPv4 format
 * ("xxx.xxx.xxx.xxx") or the IPv6 format ("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx").
 *
 * The first parameter,charPtr, is a pointer to a character buffer
 * sufficent to hold a 45 character string. The second parameter,
 * remote_ip_address, is the IP address to convert. If the IP listen address
 * is 0, the family value is also 0, so use the IPv4 value (AF_INET) as the
 * family.
 */
void IP_to_Char(char * charPtr, v4v6addr_type remote_ip_address) {

    /* AF_UNSPEC (0) may have been used as the IP listen address, use an IPv4 format. */
    if (remote_ip_address.family == AF_UNSPEC) {
        inet_ntop(AF_INET, (const unsigned char *)&remote_ip_address.v4v6,
                  charPtr, IP_ADDR_CHAR_LEN + 1);
    } else {
        inet_ntop((int)remote_ip_address.family, (const unsigned char *)&remote_ip_address.v4v6,
                  charPtr, IP_ADDR_CHAR_LEN + 1);
    }
}

/*
 * Procedure log_COMAPI_error
 *
 * Since COMAPI only returns an error status code, this procedure creates a
 * character string containing the COMAPI message and sends it to the JDBC
 * Server's LOG file.  The message text can be up to error_len number of
 * characters.
 */
static void log_COMAPI_error(char * func_name, int socket, int error_status
                             , int aux_status){

    char digits[20];
    char error_message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    int error_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH ;

    /* Initially, just return the COMAPI error code and a simple header
     * message.  This code will later be replaced by a more complete routine
     * that has COMAPI error message text.
     */

    /* Don't prefix with 'Error' (i.e., always use 'Status'),
       since many COMAPI nonzero statuses are really just warning cases */
    strcpy(error_message,"Status - ");
    strcat(error_message,func_name);
    strcat(error_message," on socket ");
    strcat(error_message,itoa(socket,digits,10));
    strcat(error_message," received a COMAPI error status, aux_status: ");
    strcat(error_message,itoa(error_status,digits,10));
    strcat(error_message,", 0"); /* leading 0 for octal */
    strcat(error_message,itoa(aux_status,digits,8)); /* octal aux info status */

    /* Now log the COMAPI error.  -  This code may have to be modified later. */
    addServerLogEntry(SERVER_LOGS_DEBUG, error_message);
}

/*
 * Procedure check_COMAPI_status
 *
 * This procedure checks the COMAPI status value returned from a COMAPI
 * function call. The error status is adjusted so it can be easily used by
 * the caller in producing any desired error mesages, etc.
 *
 * In addition, if the error status is not 0, a Log File entry is made.
 *
 * Later versions of this routine may be enhanced to also add trace lines to
 * the trace file under certain conditions, flags, etc.
 *
 */
static void check_COMAPI_status(char * func_name, int socket, int * status
                                , int * aux_status)
{
   int comapi_status_code_only = *status & 0000000777777;
   int i;

   /* Test if sgd.debug says to trace the COMAPI calls or to force a COMAPI status. */
   if (TEST_SGD_DEBUG_TRACE_COMAPI || sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
         printf("In check_COMAPI_status(), ICL_num=%d, func_name=%s, socket=%d, actual status=%d,%d forced status=%d\n",
             act_wdePtr->ICL_num, func_name, socket, (*status & 0000000777777),((*status & 0777777000000) >> 18),
             sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num]);

         /* Test if we are forcing an explicit COMAPI error status */
         if (sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num] != 0){
           *status=sgd.forced_COMAPI_error_status[act_wdePtr->ICL_num];
           comapi_status_code_only = *status & 0000000777777;
         }
   }

   *aux_status = (*status & 0777777000000) >> 18;

   /*
    * Test the error/status value based on whether we are looking at the
    * server socket or a client socket.
    *
    * For the server socket, test if there was an error/status (status value !=0)
    * that was not a COMAPI error SETIMEDOUT (the caller will handle timeout
    * themselves). If so, have the COMAPI function and the error status logged.
    *
    * For the client sockets, if there was an error/status (status value !=0)
    * log it, even if it is a timeout, since the client socket should work
    * without error in normal operation.  A timeout status means we will have
    * lost the client connection and this should be logged.
    */
    if (*status !=0) {
    	
       /* update the COMAPI interface state for this ICL */
       sgd.ICLcomapi_state[act_wdePtr->ICL_num] = *status;
       
       /*
        * Operation did not execute cleanly. Check the socket
        * type. Notice we only need to check against the server
        * sockets from the same ICL number.
        *
        *  Test if its a failed server socket reconnect attempt to COMAPI when
        * COMAPI is down (returned socket id is -1, returned status code = 10001)
        */
       if ((socket == -1) && (comapi_status_code_only == SEACCES)){
            /*
             * Its the Server socket that got a 10001 status on reconnect to COMAPI.
             * Log the first few error/status's of a possible number of repetitive
             * 10001 error that could occur during the COMAPI reconnect loop. (Note:
             * the first NUMBER_OF_COMAPI_RECONNECT_TRY_FAILED_MSGS_TO_LOG are
             * logged, the rest are skipped).
             *
             * First, count how many SEACCES (10001) have we seen in a row.
             */
             sgd.comapi_reconnect_msg_skip_log_count[act_wdePtr->ICL_num]++;   /* bump count of 10001's seen */
              
             /* Log message, skipping repetitive 10001's */
             if (sgd.comapi_reconnect_msg_skip_log_count[act_wdePtr->ICL_num] <= NUMBER_OF_COMAPI_RECONNECT_TRY_FAILED_MSGS_TO_LOG){
               log_COMAPI_error(func_name, socket, comapi_status_code_only,*aux_status);
             }
             return;
       }
       
       /* Not a SEACCES (10001), so clear the reptition count */
       sgd.comapi_reconnect_msg_skip_log_count[act_wdePtr->ICL_num] = 0;  /* it's not a 10001, so clear count */
        
       for (i = 0; i < MAX_SERVER_SOCKETS; i++){
        /* printf("socket = %d, sgd.server_socket[act_wdePtr->ICL_num][i] = %d, status = %d\n", socket,
                   sgd.server_socket[act_wdePtr->ICL_num][i],*status); */

        if ((socket != 0) &&  (socket == sgd.server_socket[act_wdePtr->ICL_num][i])) {
            /* Its the Server socket, log error/status if its not a timeout */
            if (*status != SETIMEDOUT){
              log_COMAPI_error(func_name, socket, comapi_status_code_only,*aux_status);
              return;
            }
        }
       }
       /* Must be a client socket (its not the server socket), always log error/status */
       log_COMAPI_error(func_name, socket, comapi_status_code_only,*aux_status);
   }
}
