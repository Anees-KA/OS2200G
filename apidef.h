#ifndef _APIDEF_H_
#define _APIDEF_H_
/*
 * The following defines JDBC Server created error status.
 */
#define LOST_CLIENT           -100

/*
 * Both apidef.h and apidef.p are originally from the CIFS product.
 * The apidef.h file has been modified to remove the prototypes of the
 * functions TSQ$ACTIVATE(), SLEEP$UL$ACT(), and WAKE$ACT$() due to conflicts
 * with the definitions of these functions that appear in UC 10R2Q1 and higher.
 *
 * Define the commonly used field types
 *
 *
 */
typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned int    u_int;
typedef unsigned long   u_long;

/*
 * Types
 */
#define SOCK_STREAM     1    /* stream socket */
#define SOCK_DGRAM      2    /* datagram socket */
#define SOCK_RAW        3    /* raw protocol interface */
#define SOCK_RDM        4    /* reliably-delivered message */
#define SOCK_SEQPACKET  5    /* sequenced packet stream */
#define SOCK_INTERNAL   6    /* Internal connection */

/*
 * Address families.
 */
#define AF_UNSPEC       0    /* unspecified */
#define AF_UNIX         1    /* local to host */
#define AF_INET         2    /* internetwork UDP, TCP etc */
#define AF_INET6        10   /* addr family internetwork: UDP, TCP, etc. */
#define AF_CCITT        10   /* CCITT includes X.25  */
/*
 * The following is used to keep count of the number of protocol types supported
 */
#define MAX_TYPES       2
#define UDP             0
#define TCP             1
#define ANY             99


/*
 * protocols
 */
#define INET_IP           5   /* For IP   */
#define INET_UDP         10   /* For UDP  */
#define INET_TCP         15   /* For TCP  */
#define CCITTPROTO_X25   20   /* For X25  */
#define X25TIP           30   /* For exclusive use sessions  */

#define SOL_SOCKET      0xfff /* For socket options */

/*
 * Socket level options
 *
 */
#define SO_BROADCAST  0x0001         /*         1       */
#define SO_DEBUG      0x0002         /*         2       */
#define SO_INTERFACE  0x0003         /*         3       */
#define SO_DONTROUTE  0x0004         /*         4       */
#define SO_BEQUEATH   0x0005         /*         5       */
#define SO_AVAILABLE  0x0006         /*         6       */
#define SO_ERROR      0x0008         /*         8       */
#define SO_KEEPALIVE  0x0010         /*        16       */
#define SO_LINGER     0x0020         /*        32       */
#define SO_RCVBUF     0x0040         /*        64       */
#define SO_RCVLOWAT   0x0080         /*       128       */
#define SO_RCVTIMEO   0x0100         /*       256       */
#define SO_REUSEADDR  0x0200         /*       512       */
#define SO_SNDBUF     0x0400         /*      1024       */
#define SO_SNDLOWAT   0x0800         /*      2048       */
#define SO_SNDTIMEO   0x1000         /*      4096       */
#define SO_TYPE       0x2000         /*      8192       */
#define SO_OOBINLINE  0x4000         /*     16384       */
#define SO_NOTIFY     0x8000         /*     32768       */
#define SO_MULTIPLE   0xffff         /*     65535       */
/*
 *
 * TCP options
 *
 */
#define TCP_LEVEL      0x10000       /*     65536       */
#define TCP_KEEPALIVE  0x10001       /*     65537       */
#define TCP_MAXRXT     0x10002       /*     65538       */
#define TCP_NODELAY    0x10004       /*     65540       */
#define TCP_MAXSEG     0x10008       /*     65544       */
#define TCP_STDURG     0x10010       /*     65552       */
#define TCP_RETRANSMIT 0x10020       /*     65568       */
#define TCP_PRECEDENCE 0x10040       /*     65600       */
#define TCP_DISABLE    0x10080       /*     65664       */
#define TCP_PRIORITY   0x10100       /*     65792       */
#define TCP_BLOCKING   0x10200       /*     66048       */
/*
 * IP options
 */
#define IP_LEVEL       0x20000       /*    131072       */
#define IP_TTL         0x20001       /*    131073       */
#define IP_TOS         0x20002       /*    131074       */
#define IP_OPTIONS     0x20004       /*    131076       */
#define IP_PEERNAME    0x20008       /*    131080       */
/*
 * Multicast options
 */
#define IP_ADD_MEMBERSHIP   0x20010  /*    131088       */
#define IP_DROP_MEMBERSHIP  0x20011  /*    131089       */
#define IP_MULTICAST_IF     0x20012  /*    131090       */
#define IP_MULTICAST_LOOP   0x20013  /*    131091       */
#define IP_MULTICAST_TTL    0x20014  /*    131092       */
#define IP_TEST_STATUS      0x20015  /*    131093       */
/*
 * UDP options
 */
#define UDP_LEVEL      0x40000       /*    262144       */
#define UDP_CHECKSUM   0x40001       /*    262145       */
/*
 * Interface options
 */
#define CMS1100        1
#define CPCOMM         2
#define PREFER         4
#define PREFER_CMS1100 PREFER+CMS1100
#define PREFER_CPCOMM  PREFER+CPCOMM
#define CPCOMM_OFFSET  2048

struct  addr_struct
        {
          int     word1;
          int     offset  : 3;
          int     word2   : 33;
        };

union   pointer
        {
          char    *address;
          struct  addr_struct  ptr;
        };

struct  linger
        {
         u_short      linger_on;
         u_short      linger_time;
        } ;
struct  timeval
        {
         int          time_val;
        } ;
#pragma eject
/*
 * Error codes for use in errno
 *
 */
#define API_NORMAL       0
#define SEBASE           10000  /* Not specified in C book  */

#define SEACCES          (SEBASE+1)
#define SEFAULT          (SEBASE+2)
#define SEMFILE          (SEBASE+3)
#define SENFILE          (SEBASE+4)
#define SENOBUFS         (SEBASE+5)
#define SEOPNOSUPPORT    (SEBASE+6)
#define SEPROTONOSUPPORT (SEBASE+7)
#define SESOCKTNOSUPPORT (SEBASE+8)
#define SEBADF           (SEBASE+9)
#define SENOPROTOOPT     (SEBASE+10)
#define SEINVAL          (SEBASE+11)
#define SEISCONN         (SEBASE+12)
#define SEBADREG         (SEBASE+13)
#define SENOTREG         (SEBASE+14)
#define SEBADACT         (SEBASE+15)
#define SENOTCONN        (SEBASE+16)
#define SEHOSTUNREACH    (SEBASE+17)
#define SENONAME         (SEBASE+18)
#define SEOUTWAIT        (SEBASE+19)
#define SETIMEDOUT       (SEBASE+20)
#define SENETDOWN        (SEBASE+21)
#define SEBADTSAM        (SEBASE+22)
#define SEISLIST         (SEBASE+23)
#define SENOTLIST        (SEBASE+24)
#define SEMOREDATA       (SEBASE+25)
#define SEDATALOST       (SEBASE+26)
#define SEBADNOTIFY      (SEBASE+27)
#define SEUDPERRIND      (SEBASE+28)
#define SEBADSTATE       (SEBASE+29)
#define SEWOULDBLOCK     (SEBASE+30)
#define SEOUTSTOPPED     (SEBASE+31)
#define SEHAVEEVENT      (SEBASE+32)
#define SEQUEUELIMIT     (SEBASE+33)
#define SEMAXSOCKETS     (SEBASE+34)
#define SEBADQBREQ       (SEBASE+35)
#define SENOQBANKS       (SEBASE+36)
#define SEISREG          (SEBASE+37)
#define SEMAXLISTENS     (SEBASE+38)
#define SEDUPLISTEN      (SEBASE+39)
/*
 * The following defines the parameter number. It is added to the SEFAULT
 * error code so that the parameter number appears in the top 18 bits of
 * the error code
 */
#define PARAMETER_1      01000000
#define PARAMETER_2      02000000
#define PARAMETER_3      03000000
#define PARAMETER_4      04000000
#define PARAMETER_5      05000000
#define PARAMETER_6      06000000
#define PARAMETER_7      07000000
#define PARAMETER_8     010000000


#pragma eject
/*
 * Socket states
 *
 */
#define NULL_STATE             0
#define GROUND_STATE           1
#define BOUND_STATE            2
#define CONNECTING_STATE       3
#define CONNECTED_STATE        4
#define LISTENING_STATE        5
#define CLOSING_STATE          6
/*
 *
 * This is a list of interface functions
 *
 */
#define SOCKETS_INACTIVE       0
#define GETIPADDS              33
#define UDPRECV                34
#define UDPCLEANUP             35
#define UDPGETLIA              36
#define UDPSEND                37
#define UDPLISTEN              38
#define UDPRESCIND             39
#define UDPSHUTINPUT           40
#define GETMSGSIZE             41
#define UDPADVDELPROB          43
#define ICMPSEND               44
#define ICMPRECV               45
#define TCPRECV                51
#define TCPCLEANUP             52
#define TCPGETLIA              53
#define TCPSEND                54
#define TCPLISTEN              55
#define TCPRESCIND             56
#define TCPSTOPINPUT           57
#define TCPRESUMEINPUT         58
#define TCPCLOSE               59
#define TCPABORT               60
#define TCPOPEN                61
#define TCPNEWOPEN             62
#define TCPCONNECT             63
#define TCPACCEPT              64
#define TCPRESUMEOUTPUT        66
#define TCPSETTRACE            67
#define TCPCLIND               68
#define TCPABIND               69
#define TCPKEEPALIVES          70
#define TCPEVENT               71
#define TIMERTICK              72
#define ICMPERROR              73
#define TCPQRECV               74
#define TCPQSEND               75
#define UDPQRECV               76
#define TCPTRACE               77
#define QTRANSFER              78
#define NEWTSUID               79
#define TCPBEQUEATH            80
#define TCPINHERIT             81
#define TCPREPLY               82
#define TCPQBSEND              83
#define TCPQBRECEIVE           84
#define TCPQBREPLY             85
#define TCPTRANSFERDATA        86
#define TCPRELEASEDATA         87
#define GETHOSTNAME            88
#define TCPTRYINTERNAL         89
#define TCPGETAQUEUEB          90
#define TCPFREEPORT            91
#define REQUEUEDATA            92
#define ADDMEMBERSHIP          93
#define DROPMEMBERSHIP         94
#define ADDMEMBER              95
#define DROPMEMBER             96
#define REMOVESOCKET           97
/*
 * The following defines the events or signals that may be passed
 * to the caller.
 */
#define EVENT_ACCEPT           1
#define EVENT_REJECT           2
#define EVENT_CLOSE            4
#define EVENT_ABORT            8
#define EVENT_RESUME           16
#define EVENT_USER             32
#define EVENT_DATA             64

/*
 * The following is a list of flag parameter settings
 *
 */
#define MSG_WORD_BOUNDARY  1
#define MSG_NOWAIT         2
#define MSG_QBANK          4
#define MSG_ICMP_CALL     32
#define MSG_PEEK          0400000
/*
 * The following are the ICMP request types
 *
 */
#define ECHO_TYPE          8
#define TIMESTAMP         13
/*
 *
 * The following are the shutdown options
 *
 */
#define SHUT_RD             1
#define SHUT_WR             2
#define SHUT_RDWR           3


#pragma eject
#define MAX_INT_TCP_LISTENS 100
#define MAX_INT_UDP_LISTENS 100
#define MAX_TRANSFER_QUEUES 5
#define MAX_USER_QUEUES    2000 /* Must be multiple of 10 for the get QH */
#define MAX_BUFFERS_UDP   200
#define MAX_BUFFERS_TCP   200
#define MAX_DATA_UDP      16384
#define MAX_BUFF_UDP      1500
#define MAX_DATA_TCP      16384
#define MAX_BUFF_TCP      1500
#define MAX_BUFF_LTH      1500
#define WORK_AREA_LENGTH  40        /* Long enough for IP options!  */
#define TRANSACTION_LIST_LENGTH 10
#define BATCH_UDP_LENGTH  100
#define BATCH_TCP_LENGTH  2000
#pragma eject
struct cms_conn_type
                     {
                      int c1;
                      int c2;
                     };
/*
 * UDP local/remote connection table
 *
 */
struct  lrct_type
        {
         char          version;
         char          interface;
         u_short       acquired_port_number;
         int           local_ip_address;
         int           local_port_number;
         int           remote_ip_address;
         int           remote_port_number;
        } ;

/* LRCT Versions */

#define LRCT_VERSION_0  0  /* version for IPv4 addresses (5 words)  */
#define LRCT_VERSION_1  1  /* version for IPv6 addresses (13 words) */

#ifndef __COMM_IADDR46TYPE
#define __COMM_IADDR46TYPE

/* IP Version values */

#define AF_IPV4           2   /* IP Version 4 (1 word)  */
#define AF_IPV6          10   /* IP Version 6 (5 words) */

/* Various ways of looking at IPv6 addresses */

union u_ipv6_host_id
  {
    char                  byte[8];
    int                   word[2];
    long long int         dw;
  };

union u_ipv6_prefix
  {
    char                  byte[8];
    short int             short_word[4];
    int                   word[2];
    long long int         dw;
  };

union u_ipv6_prefix_host
  {
    union u_ipv6_prefix  prefix;
    union u_ipv6_host_id  host_id;
  };

struct s_ipv6_address
  {
    long long int         prefix;
    long long int         host_id;
  };

union u_ipv6_address
  {
    char                     byte[16];
    int                      word[4];
    long long int            dw[2];
    struct s_ipv6_address    addr;
    union u_ipv6_prefix_host u_addr;
  };

union u_addrs_v4v6
  {
    int                   v4;
    char                  v4_b[4];
    struct s_ipv6_address v6;
    long long int         v6_dw[2];
    int                   v6_w[4];
    char                  v6_b[16];
    union u_ipv6_address  u_v6;
  };

/* Define the v4-v6 address structure */

/* This structure is used to contain an IPv4 or an IPV6 address.
     family = AF_IPV4: the v4v6 field contains the 4 byte IPv4 address
            = AF_IPV6: the v4v6 field contains the 16 byte IPv6 address
     zone     is a network interface index supplied by CPComm if an
                IPv6 family address is used
     v4v6     is an IPv4 or IPv6 network address in network byte order.
                For IPv4, bytes 0-3 contain the internet address.
 */

typedef struct v4v6addr
  {
    short int            family;   /* AF_INET or AF_INET6         */
    short int            zone;     /* Qualifier if < global scope */
    union u_addrs_v4v6   v4v6;     /* IPv4 or IPv6 address        */
  } v4v6addr_type;

#endif /* __COMM_IADDR46TYPE */

/* TCP/UDP local/remote connection table for IPv4/IPv6 */

struct  lrctv1_type
  {
   char               version;
   char               interface;
   u_short            acquired_port_number;
   v4v6addr_type      local_ip_address;
   int                local_port_number;
   v4v6addr_type      remote_ip_address;
   int                remote_port_number;
  };

/*
 * Protocol address definitiion
 *
 */
#define MIN_SWL_ADDRESS 0140000
#define SWL_ARRAY_LENGTH  ((01000000 - MIN_SWL_ADDRESS) / 8)

#define MAX_SESSIONS_UDP  200  /* Maximum concurrently in use */
#define MAX_SESSIONS_TCP  50000   /* Maximum concurrently in use */
/*****************************************************************************
 *                                                                           *
 * Note that the following must not exceed 511 or the cells                  *
 *      socket_user and select_user must change to a half word               *
 *                                                                           *
 *****************************************************************************/
#define MAX_UDP_USERS   500  /* Maximum concurrent UDP activities  It
                                MUST BE <= to MAX_TCP_USERS - see the
                                malloc_mem definition in COMAPI/C       */
#define MAX_TCP_USERS   500  /* Maximum concurrent user activities */

#define MAX_QUEUE_SIZE  200  /* MUST BE > MAX_SESSIONS/4  (WHY????)    */
#define MAX_TCP_QUEUE_INPUTS   10  /* MAX input queue buffers for TCP  */

#define SOCKET_ERROR (-1)
#pragma eject
struct  runids
        {
         int     original;
         int     generated;
        } ;
struct  work_area
        {
         int     if_data_length;
         char    work_byte[WORK_AREA_LENGTH];
        } ;

struct  io_buffer_type
        {
         struct  io_buffer_type *link;
         struct  lrct_type  lrct;
         u_short in_use;
         u_short segment;
         u_short count;
         u_short offset;
         char    data[MAX_BUFF_LTH];
         int     options_length;
         char    options_data[40];
        } ;

struct  xfer_buffer_type
        {
         struct  xfer_buffer_type *link;
         struct  lrct_type  lrct;
         u_short in_use;
         u_short segment;
         u_short count;
         u_short offset;
         char    data[MAX_DATA_UDP];
        } ;

struct  user_int_type
        {
         struct  io_buffer_type *link;
         struct  lrct_type  lrct;
         u_short max_entries;
         u_short segment;
         u_short next_in;
         u_short next_out;
         int     user_ident[MAX_BUFF_LTH/4];
        } ;
struct  queue_bank_type
        {
         struct  queue_bank_type *link;
         int     count;
         int     offset;
         int     ip_count;
         int     ip_offset;
         int     segment;
         int     local_ip_address;
         int     local_port_number;
         int     remote_ip_address;
         int     remote_port_number;
         int     local_id;
         int     remote_id;
         char    cpcomm_data[1996];
         char    data[1046528];
        } ;
struct  data_bank_type
        {
         char    data[1048576];
        } ;
struct  q_delete_pkt
        {
         u_short version;
         u_short reserved;
         int     request_count;
         int     status;
         int     q_header_va;
        } ;
#define IO_BUFFER_LTH  (sizeof(struct io_buffer_type)+3)/4
#define USER_BANK 16
#define QBANK     8
#define UDP_ERR_IND   4
#define FIRST     2
#define LAST      1
struct  session_tab
        {
         int    dummy;
        };
struct  if_packet_type
        {
         short   interface;
         short   function;
         int     runid;
         struct  socket_type  *sock_ptr;
         struct  session_tab  *session_table;
         struct  io_buffer_type *buffer;
        } ;
struct  socket_type
        {
         int      socket_lock;
         u_short  slot_index;
         u_short  activity_id;
/*
 *  Keep the LINK entry in this position.  It is used for a BT initialize in
 *  the socket() function.
 */
         struct   socket_type  *link;
         u_short  back_link;
         u_short  forward_link;
         struct   lrct_type  lrct;
         struct   io_buffer_type *send_buffer;
         struct   io_buffer_type *recv_buffer;
         struct   cms_conn_type cms_listen_id;
         int      return_status;
         u_short  id;
         u_short  last_icmp_status;
         u_short  current_timer;
         u_short  queue_hdr_index;
         int      current_timer_pass : 6;
         int      state : 6;
         int      recv_waiting : 6;
         int      send_waiting : 6;
         int      is_linked : 6;
         int      stop_write : 6;
         char     socket_user;
         char     select_user;
         int      input_stop : 6;
         int      notified : 6;
         int      reply_held : 6;
         int      linger_on : 6; /* Also used by UDP if zero to indicate
                                    MULTICAST add_member has been called   */
         int      multicast_loop : 6;
         int      reuseaddr : 6;
         char     max_queue_length;
         char     queue_exists;
         char     function;
         char     locked;
         char     opt_debug;
         char     af;
         char     comm;
         char     protocol;
         char     interface;
         char     opt_ttl;
         char     opt_tos;
         char     opt_ipoptlen;
         char     opt_optlenin;
         char     opt_retransmit;
         char     opt_disablenagle;
         char     opt_precedence;
         char     opt_checksum;
         char     opt_keepalive;
         char     opt_priority;
         char     opt_blocking;
         char     opt_interface;
         char     state_events;
         char     opt_type;
         char     linger_time;
         char     multicast_ttl;
         u_short  opt_rcvbuf;
         u_short  opt_sndbuf;
         int      multicast_if;
         int      transfer_va;
         int      opt_notify;
         struct   timeval opt_rcvtimeo;
         struct   timeval opt_sndtimeo;
         int      user_ident;
         struct   work_area    if_data;
        } ;
#define SOCKET_LTH  (sizeof(struct socket_type)+3)/4

#define DUMP_SOCKET     1
#define DUMP_BASIC      2
struct  basic_info
        {
         int         tsam_index;
         int         cpcomm_index;
         int         preference;
         int         active;
         int         used_st;
         int         used_end;
         int         unused_st;
         int         unused_last;
         int         timer_st;
         int         timer_end;
         int         max_header;
         int         max_hdr_used;
         int         hdrs_in_use;
         struct      join_entry_array  *multicast_buffer;
         int         first_join_entry;
         int         next_join_entry;
         int         last_join_entry;
         int         udp_user[MAX_UDP_USERS + 1];
         int         tcp_user[MAX_TCP_USERS + 1];
         struct      runids       udp_runs[MAX_UDP_USERS + 1];
         struct      runids       tcp_runs[MAX_TCP_USERS + 1];
         } ;
#pragma eject
/*
 * Multicast definitions follow
 *
 */
struct  join_group_type
        {
         short   previous_join;
         short   next_join;
         short   previous_group;
         short   next_group;
         int     group_ip_address;
         int     local_interface;
         char    use_count;
         char    unique_port;
         short   port_number;
         int     socket_id;
        };
#define JOIN_GROUP 1
#define LEAVE_GROUP 2
#define MULTICAST_REASON 255
#define INVALID_SUBREASON 1
#define DISCONNECT_SUBREASON 2
#define INADDR_ANY 0
#define MAX_NUMBER_PORTS   10
#define MIN_MULTICAST_ADDR  ((224 << 27) + (0 << 18) + (0 << 9) + 0)
#define MAX_MULTICAST_ADDR  ((239 << 27) + (255 << 18) + (255 << 9) + 255)
#define MAX_JOIN_ENTRIES  5001
#define JOIN_BUFFER_LENGTH sizeof(struct join_group_type) * MAX_JOIN_ENTRIES
struct  join_entry_array
        {
          struct  join_group_type   join_entry[MAX_JOIN_ENTRIES];
        };

#pragma eject

/* For use to access Test-and-Set cell fields. */

struct ts_fields
  {
    int s1 : 6;
    int s2 : 6;
    int s3 : 6;
    int h2 : 18;
  };

#define fatalerr()                                                             \
  {                                                                            \
    (*pi_fatalerr)++;                                                          \
  }

#pragma eject
/*******************************************************************************
 *
 *  UNLOCK
 *      This macro will unlock a locked test-and-set cell.  If an activity
 *      was deactivated waiting for this lock, it will be activated.
 *
 *    Wait                : None.
 *    Caller requirements : Must define CALLER_ID.  See FATALERR function.
 *    Path                : Main-path.
 *    Activity            : Any activity.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    unlock_ts_cell   : The test-and-set cell to unlock.
 *
 *  RETURNS: None.
 *
 ******************************************************************************/

#define unlock(unlock_ts_cell)                                                 \
    if (((struct ts_fields *)unlock_ts_cell)-> s1 == 0)                        \
      {                                                                        \
        fatalerr();                                                            \
      }                                                                        \
    else                                                                       \
      {                                                                        \
        ((struct ts_fields *)unlock_ts_cell)-> s1 = 0;                         \
        if (((struct ts_fields *)unlock_ts_cell)-> h2 != 0)                    \
          {                                                                    \
            if ((TSQ$ACTIVATE(unlock_ts_cell)) < 0)                            \
              {                                                                \
                fatalerr();                                                    \
              }                                                                \
          }                                                                    \
      }

#pragma eject

/*******************************************************************************
 *
 *  UNLOCK_WAIT
 *      This macro will unlock a locked test-and-set cell, if an activity
 *      was deactivated waiting for this lock, it will be activated.  The
 *      activity making the call will be deactivated.
 *
 *    Wait                : Yes.  Until another activity calls UNLOCK_ACT.
 *    Caller requirements : Must define CALLER_ID.  See FATALERR function.
 *    Path                : Main-path.
 *    Activity            : Any activity.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    unlock_ts_cell   : The test-and-set cell to unlock, and wait on.
 *
 *  RETURNS: None.
 *
 ******************************************************************************/

#define unlock_wait(unlock_ts_cell)                                            \
    if ((SLEEP$UL$ACT(unlock_ts_cell)) < 0)                                    \
      {                                                                        \
        fatalerr();                                                            \
      }

#pragma eject

/*******************************************************************************
 *
 *  WAKEUP_ACT
 *      This macro will fire the first activity that is was deactivated using
 *      using UNLOCK_WAIT.
 *
 *    Wait                : None.
 *    Caller requirements : Must define CALLER_ID.  See FATALERR function.
 *    Path                : Main-path.
 *    Activity            : Any activity.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    unlock_ts_cell   : The test-and-set cell to operate on.
 *
 *  RETURNS: None.
 *
 ******************************************************************************/

#define wakeup_act(wakeup_ts_cell)                                             \
    if (((struct ts_fields *)wakeup_ts_cell)-> h2 != 0)                        \
      {                                                                        \
        if ((WAKE$ACT$(wakeup_ts_cell)) < 0)                                   \
          {                                                                    \
            fatalerr();                                                        \
          }                                                                    \
      }

#pragma eject

/*******************************************************************************
 *
 *  UNLOCK_ACT
 *      This macro will unlock a locked test-and-set cell.  If an activity
 *      was deactivated using UNLOCK_WAIT, it will be activated.
 *
 *    Wait                : None.
 *    Caller requirements : Must define CALLER_ID.  See FATALERR function.
 *    Path                : Main-path.
 *    Activity            : Any activity.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    unlock_ts_cell   : The test-and-set cell to unlock.
 *
 *  RETURNS: None.
 *
 ******************************************************************************/

#define unlock_act(unlock_ts_cell)                                             \
    if (((struct ts_fields *)unlock_ts_cell)-> s1 == 0)                        \
      {                                                                        \
        fatalerr();                                                            \
      }                                                                        \
    else                                                                       \
      {                                                                        \
        ((struct ts_fields *)unlock_ts_cell)-> s1 = 0;                         \
        if (((struct ts_fields *)unlock_ts_cell)-> h2 != 0)                    \
          {                                                                    \
            if ((WAKE$ACT$(unlock_ts_cell)) < 0)                               \
              {                                                                \
                fatalerr();                                                    \
              }                                                                \
          }                                                                    \
      }


#pragma eject

/*******************************************************************************
 *
 *  UL_OR_UL_AND_ACT
 *      This macro will unlock a locked test-and-set cell.  If an activity
 *      activation is required it will optionally perform that.
 *
 *    Wait                : None.
 *    Caller requirements : Must define CALLER_ID.  See FATALERR function.
 *    Path                : Main-path.
 *    Activity            : Any activity.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    unlock_ts_cell   : The test-and-set cell to unlock.
 *
 *  RETURNS: None.
 *
 ******************************************************************************/

#define ul_or_ul_and_act(unlock_ts_cell)                                       \
    if (((struct ts_fields *)unlock_ts_cell)-> s1 == 0)                        \
      {                                                                        \
        fatalerr();                                                            \
      }                                                                        \
    else                                                                       \
      {                                                                        \
        ((struct ts_fields *)unlock_ts_cell)-> s1 = 0;                         \
        if (((struct ts_fields *)unlock_ts_cell)-> h2 != 0)                    \
          {                                                                    \
            if (((struct ts_fields *)unlock_ts_cell)-> s3 == 0)               \
              {                                                                \
                if ((TSQ$ACTIVATE(unlock_ts_cell)) < 0)                        \
                  {                                                            \
                    fatalerr();                                                \
                  }                                                            \
              }                                                                \
            else                                                               \
              {                                                                \
                ((struct ts_fields *)unlock_ts_cell)-> s3 = 0;                 \
                if ((WAKE$ACT$(unlock_ts_cell)) < 0)                           \
                  {                                                            \
                    fatalerr();                                                \
                  }                                                            \
              }                                                                \
          }                                                                    \
      }
#pragma eject

/*******************************************************************************
 *
 *  SET_TO_ACTIVATE
 *      This macro will set S3 of the TS cell to indicate that the call to
 *      UL_OR_UL_AND_ACT should activate the activity that is waiting.
 *
 *    Wait                : None.
 *    Caller requirements : Must define CALLER_ID.  See FATALERR function.
 *    Path                : Main-path.
 *    Activity            : Any activity.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    unlock_ts_cell   : The test-and-set cell to operate on.
 *
 *  RETURNS: None.
 *
 ******************************************************************************/

#define set_to_activate(wakeup_ts_cell)                                        \
    if (((struct ts_fields *)wakeup_ts_cell)-> s1 == 0)                        \
      {                                                                        \
        fatalerr();                                                            \
      }                                                                        \
    else                                                                       \
      {                                                                        \
        ((struct ts_fields *)wakeup_ts_cell)-> s3 = 1;                         \
      }
#pragma eject
/*
 *
 * Define the product and level id fields
 *
 */
extern  char  PROD$NAME[];
extern  char  BLD$ID[];
extern  char  INT$ID[];
extern  char  EXT$ID[];
extern  char  CREATE$DATE[];

#endif /* _APIDEF_H_ */
