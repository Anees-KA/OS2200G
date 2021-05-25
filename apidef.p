#ifndef _APIDEF_P_
#define _APIDEF_P_

/* JDBC Project Files */
#include "apidef.h"

int
getswl();

void
comapi_start(
             int      protocol_type,
             int      tsam_index,
             int      cpcomm_index
            );

void
wait_for_packet(
                struct   if_packet_type   *if_packet,
                struct   socket_type      *socket,
                int      protocol_type
               ) ;

void
comapi_terminate(
                 int      protocol_type
                );

void
return_from_bgrapi(
                   struct  if_packet_type  *if_packet,
                   struct  socket_type     *socket,
                   int     status,
                   int     protocol_type
                  ) ;

void
dumpit(
       int      protocol_type,
       int      parameter,
       char     data_area[]
      );

/* Format of a Queue Bank for UDP QB send or TCP QB send. */

struct qb_user_type
  {
    int             reserved_1[2];
    int             data_bytes_in_this_qb;       /* Range 0    to 1,046,524  */
    int             data_byte_offset_in_this_qb; /* Range 2048 to 1,048,575  */
    int             reserved_2[508];
    char            data[1046524];
  };
#pragma eject
#pragma interface UDP_SESSION
/*
 * This function is called to perform the socket function.
 *
 * CALL 'UDP_SESSION' USING SESSION_ID STATUS.
 *
 */
void
UDP_SESSION(
            int       *socket_value,
            int       *status
           ) ;
#pragma interface UDP_GETREMIP
/*
 * This function is called to get the local and remote IP addresses
 * for the given name
 *
 * CALL 'UDP_GETREMIP' USING SESSION-ID PEER_NAME LRCT STATUS
 */
void
UDP_GETREMIP(
             int     *socket_value,
             char    name[],
             struct  lrct_type  *lrct,
             int     *status
            ) ;
#pragma interface UDP_GETMSGSIZE
/*
 * This function is called to get the maximum message size that can
 * be used for a given remote and local IP address pair
 *
 * CALL 'UDP_GETMSGSIZE' USING LRCT SIZE STATUS
 */
void
UDP_GETMSGSIZE(
               int     *socket_value,
               struct  lrct_type  *lrct,
               int     *msg_size,
               int     *status
              ) ;
#pragma interface UDP_SETOPTS
/*
 * This function is called to perform the set option values.
 *
 * CALL 'UDP_SETOPTS' USING SESSION_ID OPTION_NAME
 *                                   OPTION_VALUE OPTION_LENGTH STATUS.
 *
 */
void
UDP_SETOPTS(
            int       *socket_value,
            int       *opt_name,
            int       *opt_val,
            int       *opt_length,
            int       *status
           ) ;
#pragma interface UDP_GETOPTS
/*
 * This function is called to perform the get the option values.
 *
 * CALL 'UDP_GETOPTS' USING SESSION_ID OPTION_NAME
 *                             OPTION_VALUE OPTION_LENGTH STATUS.
 *
 */
void
UDP_GETOPTS(
            int       *socket_value,
            int       *opt_name,
            int       *opt_val,
            int       *opt_length,
            int       *status
           ) ;
#pragma interface UDP_SENDTO
/*
 * This function performs the sendto function for the UCOB user. The call is
 *
 * CALL 'UDP_SENDTO' USING SESSION-ID DATA-BUFFER LENGTH LRCT STATUS
 *
 */
void
UDP_SENDTO(
           int      *socket_value,
           char     *data_buffer,
           int      *length,
           struct   lrct_type   *lrct,
           int      *status
          ) ;
#pragma interface UDP_REPLY
/*
 * This function performs the sendto function for the UCOB user but without
 * the need to acquire a session table first.
 *
 * CALL 'UDP_REPLY' USING DATA-BUFFER LENGTH LRCT STATUS
 *
 */
void
UDP_REPLY(
          char     *data_buffer,
          int      *length,
          struct   lrct_type   *lrct,
          int      *status
         ) ;
#pragma interface UDP_LISTEN
/*
 * This function allows a user to put up a listen. This call must be made
 * prior to performing the UDP_RECEIVE call.
 *
 * CALL 'UDP_LISTEN' USING SESSION-ID LRCT QUEUE-SIZE STATUS
 *
 */
void
UDP_LISTEN(
           int     *socket_value,
           struct  lrct_type   *lrct,
           int     *queue_size,
           int     *status
          ) ;
#pragma interface UDP_RESCIND
/*
 * This function allows a user to rescind a listen. This call must be made
 * after a listen has been established.    .
 *
 * CALL 'UDP_RESCIND' USING SESSION-ID STATUS
 *
 */
void
UDP_RESCIND(
            int     *socket_value,
            int     *status
           ) ;
#pragma interface UDP_RECEIVE
/*
 * This function performs the receive function for the UCOB user. The call is
 *
 * CALL 'UDP_RECEIVE' USING SESSION-ID BUFFER SIZE LENGTH LRCT STATUS
 *
 */
void
UDP_RECEIVE(
            int      *socket_value,
            char     *buffer,
            int      *size,
            int      *length,
            struct   lrct_type   *lrct,
            int      *status
           );
#pragma interface UDP_CLOSE
/*
 * This function is called to perform the close socket function.
 *
 * CALL 'UDP_CLOSE' USING SESSION_ID STATUS.
 *
 */
void
UDP_CLOSE(
          int       *socket_value,
          int       *status
         ) ;
#pragma interface UDP_PROBLEMS
/*
 * This function is called advise delivery problems.
 *
 * CALL 'UDP_PROBLEMS' USING SESSION_ID LRCT STATUS.
 *
 */
void
UDP_PROBLEMS(
             int       *socket_value,
             struct    lrct_type  *lrct,
             int       *status
            ) ;
#pragma interface UDP_DEREGISTER
/*
 *
 * This function is called to de-register from the UDP API subsystem
 *
 * CALL 'UDP_DEREGISTER'
 *
 */
void
UDP_DEREGISTER() ;

#pragma interface ICMP_SEND
/*
 *
 * This function is called to perform an ICMP echo
 *
 * CALL 'ICMP_SEND' USING SESSION-ID DATA-BUFFER LENGTH LRCT STATUS
 *
 */
void
ICMP_SEND(
          int      *socket_value,
          char     *data_buffer,
          int      *length,
          struct   lrct_type   *lrct,
          int      *status
         ) ;

#pragma interface ICMP_RECEIVE
/*
 *
 * This function is called to perform an ICMP receive
 *
 * CALL 'ICMP_RECEIVE' USING SESSION-ID BUFFER-SIZE LENGTH LRCT FLAGS STATUS
 *
 */
void
ICMP_RECEIVE(
             int      *socket_value,
             char     *buffer,
             int      *size,
             int      *length,
             struct   lrct_type   *lrct,
             int      *wait,
             int      *status
            ) ;

#pragma interface TCP_REGISTER
/*
 * This function is called to register with COMAPI. It is only
 * required if the user does not want the default max sessions
 *
 * CALL 'TCP_REGISTER' USING SESSION_COUNT STATUS.
 *
 */
void
TCP_REGISTER(
             int       *session_count,
             int       *status
            ) ;
#pragma interface UDP_GETHOSTNAME
/*
 * This function is called to get the host name from the DNR. The caller
 * passes the Internet address in the LRCT.
 *
 * CALL 'UDP_GETHOSTNAME' USING LRCT BUFFER COUNT STATUS.
 *
 */
void
UDP_GETHOSTNAME(
                struct    lrct_type    *lrct,
                char      *data_buffer,
                int       *count,
                int       *status
               ) ;
#pragma interface TCP_GETHOSTNAME
/*
 * This function is called to get the host name from the DNR. The caller
 * passes the Internet address in the LRCT.
 *
 * CALL 'TCP_GETHOSTNAME' USING LRCT BUFFER COUNT STATUS.
 *
 */
void
TCP_GETHOSTNAME(
                struct    lrct_type    *lrct,
                char      *data_buffer,
                int       *count,
                int       *status
               ) ;
#pragma interface TCP_SELECT
/*
 * This function is called to request the status of various sockets
 * Three lists are provided for readability, writeability and exception status
 *
 * CALL 'TCP_SELECT' USING READ-LIST WRITE-LIST EXCEPTION-LIST
 *                                 TIMER FORMAT STATUS
 *
 */
void
TCP_SELECT(
            int       read_list[],
            int       write_list[],
            int       exception_list[],
            int       *timer,
            int       *format,
            int       *status
            ) ;
#pragma interface TCP_SESSION
/*
 * This function is called to perform the socket function.
 *
 * CALL 'TCP_SESSION' USING SESSION_ID STATUS.
 *
 */
void
TCP_SESSION(
            int       *socket_value,
            int       *status
           ) ;

#pragma interface TCP_GETREMIP
/*
 * This function is called to get the local and remote IP addresses
 * for the given name
 *
 * CALL 'TCP_GETREMIP' USING SESSION-ID PEER_NAME LRCT STATUS
 */
void
TCP_GETREMIP(
             int     *socket_value,
             char    name[],
             struct  lrct_type  *lrct,
             int     *status
            ) ;

#pragma interface TCP_SETOPTS
/*
 * This function is called to perform the set option values.
 *
 * CALL 'TCP_SETOPTS' USING SESSION_ID OPTION_NAME
 *                                   OPTION_VALUE OPTION_LENGTH STATUS.
 *
 */
void
TCP_SETOPTS(
            int       *socket_value,
            int       *opt_name,
            int       *opt_val,
            int       *opt_length,
            int       *status
           ) ;

#pragma interface TCP_GETOPTS
/*
 * This function is called to perform the get the option values.
 *
 * CALL 'TCP_GETOPTS' USING SESSION_ID OPTION_NAME
 *                             OPTION_VALUE OPTION_LENGTH STATUS.
 *
 */
void
TCP_GETOPTS(
            int       *socket_value,
            int       *opt_name,
            int       *opt_val,
            int       *opt_length,
            int       *status
           ) ;

#pragma interface TCP_ACCEPT
/*
 * This function is called to accept an inbound open session
 *
 * CALL 'TCP_ACCEPT' USING SESSION-ID NEW-SESSION USER-ID LRCT STATUS
 *
 */
void
TCP_ACCEPT(
           int     *socket_value,
           int     *new_socket,
           int     *user_ident,
           struct  lrct_type  *lrct,
           int     *status
          ) ;

#pragma interface TCP_CONNECT
/*
 * This function is called to perform an active open
 *
 * CALL 'TCP_OPEN' USING SESSION_ID LRCT STATUS
 *
 */
void
TCP_CONNECT(
            int     *socket_value,
            struct  lrct_type  *lrct,
            int     *status
           ) ;

#pragma interface TCP_DISCONNECT
/*
 * This function is called to disconnect the TCP session. The disconnect
 * may be an immediate or gracefull close down. The immediate performs
 * an ABORT, the gracefull performs a CLOSE.
 *
 * CALL 'TCP_DISCONNECT' USING SESSION-ID IMMEDIATE-FLAG STATUS
 */
void
TCP_DISCONNECT(
               int       *socket_value,
               int       *disconnect_type,
               int       *status
              ) ;

#pragma interface TCP_CLOSE
/*
 * This function is called to close the socket. If the session is
 * in any state except GROUND_STATE the session should be closed
 *
 * CALL 'TCP_CLOSE' USING SESSION-ID, STATUS
 */
void
TCP_CLOSE(
          int       *socket_value,
          int       *status
         ) ;

#pragma interface TCP_SEND
/*
 * This function is called to send data to the peer
 *
 * CALL 'TCP_SEND' USING SESSION-ID DATA-BUFFER LENGTH STATUS
 *
 */
void
TCP_SEND(
         int      *socket_value,
         char     *data_buffer,
         int      *length,
         int      *status
        ) ;

#pragma interface TCP_RECEIVE
/*
 * This function is called to receive data. The caller can opt to not
 * wait if he so decides.
 *
 * CALL 'TCP_RECEIVE' USING SESSION-ID, BUFFER, LENGTH, BYTE-COUNT, STATUS
 */
void
TCP_RECEIVE(
            int      *socket_value,
            char     *data_buffer,
            int      *length,
            int      *count,
            int      *status
           ) ;

#pragma interface TCP_LISTEN
/*
 * This function allows a user to put up a listen. This call must be made
 * in order to receive inbound opens or for PROXY EVENT handling
 *
 * CALL 'TCP_LISTEN' USING SESSION-ID LRCT QUEUE-SIZE STATUS
 *
 */
void
TCP_LISTEN(
           int     *socket_value,
           struct  lrct_type   *lrct,
           int     *queue_size,
           int     *status
          ) ;

#pragma interface TCP_EVENT
/*
 * This function is called to send a user event indication to a socket
 * that is owned by the same run. It may be teh same or a different activity.
 *
 * CALL 'TCP_EVENT' USING SESSION-ID, STATUS
 */
void
TCP_EVENT(
          int       *socket_value,
          int       *status
         ) ;

#pragma interface TCP_STOP
/*
 * This function is called to stop input on a session. The session must
 * be in CONNECTED_STATE or CLOSING_STATE.
 *
 * CALL 'TCP_STOP' USING SESSION-ID, STATUS
 */
void
TCP_STOP(
         int       *socket_value,
         int       *status
        ) ;

#pragma interface TCP_RESUME
/*
 * This function is called to resume input on a socket. The socket
 * must be in GROUND_STATE or CLOSING_STATE and must be stopped
 * by the user.
 *
 * CALL 'TCP_RESUME' USING SESSION-ID, STATUS
 */
void
TCP_RESUME(
           int       *socket_value,
           int       *status
          ) ;

#pragma interface TCP_RESCIND
/*
 * This fuction is called to rescind an outstanding listen. This calls the
 * background run to close the passive open and closes and releases any
 * oustanding OPENs that have not been picked up.
 *
 *  CALL 'TCP_RESCIND' USING SESSION-ID STATUS
 *
 */
void
TCP_RESCIND(
            int        *socket_value,
            int        *status
           ) ;

#pragma interface TCP_BEQUEATH
/*
 * This function is called to bequeath a session to another activity or run.
 * The activity wishing to inherit the session must perform a TCP_INHERIT in
 * order to take possesion of the session. The session must be in the CONNECTED
 * state.
 *
 *  CALL 'TCP_BEQUEATH' USING SESSION-ID STATUS
 *
 */
void
TCP_BEQUEATH(
             int        *socket_value,
             int        *status
            );

#pragma interface TCP_INHERIT
/*
 * This function is called to inherit a session from another activity or run.
 * The activity wishing to bequeath the session must perform a TCP_BEQUEATH in
 * order to relinquish possesion of the session. The session must be in the
 * CONNECTED state.
 *
 *  CALL 'TCP_INHERIT' USING SESSION-ID STATUS
 *
 */
void
TCP_INHERIT(
            int        *socket_value,
            int        *status
           );

#pragma interface TCP_DEREGISTER
/*
 * This function is called to deegister from the sub-system
 *
 * CALL 'TCP_DEREGISTER'
 *
 */
void
TCP_DEREGISTER() ;


#pragma interface TCP_REPLY
/*
 * This function is called to send data to the peer without owning the session
 *
 * CALL 'TCP_REPLY' USING SESSION-ID DATA-BUFFER LENGTH STATUS
 *
 */
void
TCP_REPLY(
          int      *socket_value,
          char     *data_buffer,
          int      *length,
          int      *status
         );

#pragma eject
#pragma interface TCP_QBSEND
/*
 *  This function is called to use the Q interface for a send.
 *
 *
 */
void
TCP_QBSEND(
           int      *socket_value,
           struct   qb_user_type  **qbank_address,
           int      *status
          );
#pragma interface TCP_QBRECEIVE
/*
 * This function is called to receive a q bank
 *
 *
 */
void
TCP_QBRECEIVE(
              int     *socket_value,
              struct  qb_user_type  **qbank_address,
              int     *status
             );
#pragma interface TCP_QBREPLY
/*
 * This function is called to send qb data to the peer
 *
 *
 *
 */
void
TCP_QBREPLY(
            int      *socket_value,
            struct   qb_user_type  **qb_address,
            int      *status
           );
#pragma interface TCP_BORROWQB
/*
 * This function is called to borrow a q bank
 *
 *
 */
void
TCP_BORROWQB(
             struct  qb_user_type  **qbank_address,
             int     *status
            );

#pragma interface TCP_RETURNQB
/*
 * This function is called to borrow a q bank
 *
 *
 */
void
TCP_RETURNQB(
             struct  qb_user_type  **qbank_address,
             int     *status
            );

#pragma interface UDP_REGISTER
/*
 * This function is called to register with COMAPI. It is only
 * required if the user does not want the default max sessions
 *
 * CALL 'UDP_REGISTER' USING SESSION_COUNT STATUS.
 *
 */
void
UDP_REGISTER(
             int       *session_count,
             int       *status
            );

#pragma interface UDP_SELECT
/*
 * This function is called to return the status of a list of sockets
 * that are checked for readability. Blank lists are allowed for
 * compatibility with the TCP call
 *
 * CALL 'UDP_SELECT' USING READ-LIST WRITE-LIST EXCEPTION-LIST
 *                           TIMER FORMAT STATUS
 *
 */
void
UDP_SELECT(
           int    read_list[],
           int    write_list[],
           int    exception_list[],
           int    *timer,
           int    *format,
           int    *status
          );

#endif /* _APIDEF_P_ */
