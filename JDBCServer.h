#ifndef _JDBCSERVER_H_
#define _JDBCSERVER_H_
/**
 * File: JDBCServer.h.
 *
 * Header file used for UC JDBC Server code for Server data structures.
 *
 * Header file corresponding to code file JDBCServer.c and other JDBC Server
 * worker modules.
 *
 * Data structure descriptions
 *
 * serverGlobalData - Global Server Data table. One instance of this
 *     structure is defined, named sgd, per JDBC Server. This structure
 *     is defined in program level memory and can be made visible in any
 *     of the JDBC Server components by utilizing a "extern sgd" construct.
 *
 *
 * workerDescriptionEntry - individual Server Worker's control information.
 *     One instance of this structure is defined for each Server Worker
 *     activity that exists. Each instance of this structure is defined in
 *     program level memory and a particular Server Worker's instance is made
 *     visible to the Server Worker through a pointer named wdePtr.
 *     Most code modules can/will see only see its own Server Worker's WDE
 *     entry.  This behavior is enforced via coding discipline.
 *
 *     Only selected code modules within the JDBC Server can access all the
 *     Server Workers WDEs - usually this is done in conjunction with the
 *     maintenance of either the free or assigned Server Worker chains, or
 *     this is done in response to a Console Listener command (e.g. a request
 *     for a list of assigned Server Workers would have to traverse all the WDE's
 *     on the assigned Server Worker chain.
 */

/* Standard C header files and OS2200 System Library files */
#include <tsqueue.h>  /* needed for T/S cell definitions */
#include <time.h>
#include <ertran.h>
#include <stdio.h>

/* JDBC Project Files */
#include "apidef.h"
#include "suval.h"

/*
 * Defines for simulating the XA Thread reuse connection property value. This value
 * indicates whether an XA Server can reuse an already open RDMS 2PC (XA) database thread.
 */
#define XA_THREAD_REUSE_ENABLED 1 /* Simulated "XAThread" connection property value. 0 = No (false), 1 = Yes (true). */
#define XA_THREAD_REUSE_TESTING 0 /* When XA_THREAD_REUSE_ENABLED is 1, this flag turns on special test code (1) or not (0). */
/*
 * Define local constants used to allocate a temporary set of WDE's.
 * WARNING: See define MONITOR_TMALLOC_USAGE regarding reducing this
 * value when Tmalloc usage tracing is turned on.
 */
#define TEMPNUMWDE 2000       /* temporarily allow up to 2000 worker description entries */

/* Product release levels.  Normally, these numbers will match the numbers in RdmsDriver
   when we release a new product level. */
#define JDBCSERVER_MAJOR_LEVEL_ID    2                         /* This is the major level of the current release of the Server */
#define JDBCSERVER_MINOR_LEVEL_ID    13                        /* This is the minor level of the current release of the Server */
#define JDBCSERVER_FEATURE_LEVEL_ID  0                         /* This is the feature level of the current release of the Server */
#define JDBCSERVER_PLATFORM_LEVEL_ID 0                         /* This is the platform level of the current release of the Server */
                                                               /* Update this level for each new customer release */
#define JDBCSERVER_MINIMUM_MAJOR_LEVEL_ID       2              /* This is the major level of the minimum release acceptable by the Server */
#define JDBCSERVER_MINIMUM_MINOR_LEVEL_ID       12             /* This is the minor level of the minimum release acceptable by the Server */
#define JDBCSERVER_MINIMUM_FEATURE_LEVEL_ID     0              /* This is the feature level of the minimum release acceptable by the Server */
#define JDBCSERVER_MINIMUM_PLATFORM_LEVEL_ID    0              /* This is the platform level of the minimum release acceptable by the Server */
#define JDBCSERVER_MAXIMUM_PLATFORM_LEVEL_ID    256
                                                               /* Update this level for each new customer release */
#define JDBC_DRIVER_BUILD_LEVEL_MAX_SIZE  30                   /* Maximum size in characters of the JDBCSERVER_BUILD_LEVEL_ID string */
#define JDBC_SERVER_BUILD_LEVEL_MAX_SIZE  30                   /* Maximum size in characters of the JDBCSERVER_BUILD_LEVEL_ID string */
                                                               /* Note: both driver and server build strings MUST have same maximum size. */
#define JDBCSERVER_BUILD_LEVEL_ID    "20210225-DST1"           /* This is the build level id string of the current build of the Server */
                                                               /* build level id is in the format "<date>-<id>", where <date> is in    */
                                                               /* format YYYYMMDD and <id> is a string of characters with no embedded  */
                                                               /* blanks, e.g. "20050507-FIN" . Update the build level id for each     */
                                                               /* tested mainline build.                                               */
#define JDBC_SERVER_LEVEL_SIZE 4                               /* Size in bytes of array containing JDBC Server level in SGD */
#define JDBC_DRIVER_LEVEL_SIZE 4                               /* Size in bytes of array containing JDBC driver level in WDE */

/* Server contingency messages. */
#define JDBC_CONTINGENCY_MESSAGE "  JDBC Server \"%s\" contingency, server shut down"  /* Contingency message */
#define XA_JDBC_CONTINGENCY_MESSAGE "  JDBC XA Server %d \"%s\" contingency, server shut down"  /* Contingency message */

/* Minimal RDMS/RSA levels.  To use this level of the RDMS JDBC product, RDMS 17R1 (17-1-nnnn) or higher is needed. */
#define MINIMAL_RDMS_BASE_LEVEL    17                          /* Need at least a 15 level base */
#define MINIMAL_RDMS_FEATURE_LEVEL  1                          /* With a feature level of at least 1 */
#define MAX_RDMS_LEVEL_LEN         36                          /* Maximum length of the RDMS level string */
#define RDMS_LEVEL_INDENT          11                          /* The RDMS level starts after the 11th character */
                                                               /* - it follows the "RDMS LEVEL " prefix returned */
/* Define specific RDMS levels used to set the Feature Flags */
#define RDMS_BASE_LEVEL_21         21                          /* Base level value for 21Rx */
#define RDMS_BASE_LEVEL_20         20                          /* Base level value for 20Rx */
#define RDMS_BASE_LEVEL_19         19                          /* Base level value for 19Rx */
#define RDMS_BASE_LEVEL_18         18                          /* Base level value for 18Rx */
#define RDMS_BASE_LEVEL_17         17                          /* Base level value for 17Rx */
#define RDMS_FEATURE_LEVEL_1       1                           /* Feature level value for xxR1 */
#define RDMS_FEATURE_LEVEL_2       2                           /* Feature level value for xxR2 */

/* ****************************************************************************************** */
/* Defines for special compile (or run-time) testing. See Work instructions for more details. */
#define SIMPLE_MALLOC_TRACING 0  /* Define for turning on/off one line tracing of Tmalloc(), Tcalloc() and Tfree() calls. */
                                 /* 0 - tracing is off, 1 - JDBC tracing on, 2 - JDBC tracing and PRINT$ trace turned on. */

#define JUNIT_NAME_TRACING 0     /* Define to control displaying JUNIT name in JDBC trace and/or PRINT$ file. */
                                 /* If 0 , then normal threadprefix connection property behavior. If 1, then  */
                                 /* display JUNIT name in JDBC trace only. If 2, then display JUNIT name in   */
                                 /* both JDBC trace file and in PRINT$ file.                                  */
/*
 * Defines for the Tmalloc(), Tcalloc(), and Tfree() Usage Tracking routines for performing a
 * diagnostic analysis of their memory allocation and release handling. Also indicate whether
 * a Usage Report should be produced and the level of detail of that report. Tag MONITOR_TMALLOC_USAGE
 * turns on the whole feature, and MONITOR_TMALLOC_USAGE_DISPLAY_CALLS and MONITOR_TMALLOC_USAGE_REPORT
 * control what is displayed when the feature is used.
 *
 * Warning:  When using this feature, you MUST reduce the value of TEMPNUMWDE to a value under 55. A
 * value of 20 is probably enough.  Since there is a tmalloc_table for each allocated WDE, and a given
 * tmalloc_table has TMALLOC_TRACEID_MAX entries, the amount of space allocated for the total WDE's
 * can exceed the amount allowed by the UCS compile time/run time definitions.  A value of 20 is
 * recommended for TEMPNUMWDE, but a value of 55 has been used, if more client concurrency is needed.
 */
#define MONITOR_TMALLOC_USAGE               0  /* Indicates whether compile-time code for Tmalloc()/Tcalloc()/Tfree() usage tracking should be added.  (1 = on, 0 = off).  */
#define MONITOR_TMALLOC_USAGE_DISPLAY_CALLS 0  /* Indicates if a Tmalloc()/Tcalloc()/Tfree() call should be displayed at call time. (1 = on, 0 = off).      */
#define MONITOR_TMALLOC_USAGE_REPORT        3  /* Indicates if a Tmalloc/Tfree Usage Tracking report is desired, and at what level of detail:               */
                                               /* 0 - Indicates that no tracking report is desired.                                                         */
                                               /* 1 - Indicates a tracking report with only summary level information (total's) is desired.                 */
                                               /* 2 - Indicates a tracking report including all the used traceid's Tmalloc/Tfree general entry information. */
                                               /* 3 - Indicates a tracking report including all the used traceid's entries dLink list information.          */
/* ****************************************************************************************** */

/*
 * Define for use by the Tmalloc(), Tcalloc(), and Tfree() routines. This is the current maximum
 * Tmalloc/Tcalloc/Tfree traceid value used by JDBC (XA) Server, or  C-interface code.
 *
 * NOTE:  All traceid numbers, regardless of type of call, are assigned in order from 1 to
 *        TMALLOC_TRACEID_MAX. When new Tmalloc(), Tcalloc(), or Tfree() calls are added, they
 *        get the next higher traceid integer value and the TMALLOC_TRACEID_MAX define must be
 *        adjusted accordingly.
 *
 *        There are 75 Tmalloc()'s, 5 Tcalloc()'s, and 89 Tfree()'s being used at this time.
 *        These traceid values are unused: 38, 39, 40, 41, 149, 150, 151,152, 153.
 *
 */
#define TMALLOC_TRACEID_MAX 173

/* Define the struct, size, and functions that provide a doubly linked list capability. */
struct dLinkListEntry{
    struct dLinkListEntry * priorDLinkListPtr;
    struct dLinkListEntry * nextDLinkListPtr;
};
typedef struct dLinkListEntry dLinkListEntry;
#define SIZE_DLINKLIST_ENTRY sizeof(dLinkListEntry) /* Size of the dLinkListEntry's link pointers. */

typedef struct {
    dLinkListEntry * firstDLinkListPtr;  /* Pointer to first entry on the dLinkList. */
    int              num_entries;        /* Number of entries on the dLinkList.      */
} dLinkListHeader;

typedef struct {
    dLinkListEntry dLink;          /* Doubly linked list pointers for this entry. */
    int            traceid;        /* The traceid for this Tmalloc/Tcalloc entry */
    int            malloc_size;    /* Size of the requested malloc/calloc space. */
    long long      requestDWTime;  /* The DWTime value when the malloc/calloc was requested. */
    int            tagValue;       /* Tag value to insure we have valid Tmalloc_space entry. */
                                   /* The above fields are the tmalloc_entry header, what follows is malloc/calloc space for caller to use. */
    int            malloc_area;    /* The start of the malloc'ed space to be used by JDBC (XA) Server, or C-interface code. */
} tmalloc_entry;
#define TMALLOC_ENTRY_HEADER_SIZE (sizeof(tmalloc_entry)- sizeof(int)) /* Size of the tmalloc_entry header in bytes */
#define TMALLOC_ENTRY_HEADER_SIZE_WORDS (TMALLOC_ENTRY_HEADER_SIZE/4) /* Size of the tmalloc_entry header in words */
#define TMALLOC_ENTRY_TAGVALUE 0525252525252 /* Identify or validation tag value placed in every tmalloc_entry */
#define WORD_OFFSET_TO_TMALLOC_ENTRY_HEADER_TAG_VALUE 1 /* Tag value in tmalloc_entry is one word back from user's "malloc" pointer. */
#define TMALLOC_TABLE_ENTRY_ELTNAME_SIZE 16 /* Space for a 15 character eltname or label and a trailing null. */
typedef union {
    /* Note: The traceid for a tmalloc_table_entry is the array subscript used when an array of this structure is declared. */
    struct{
        int   type_of_entry;            /* Indicates if its a "empty" (0), "total" (1), "Tmalloc/Tcalloc" (2) , or "Tfree" (2) entry. */
        char  label[TMALLOC_TABLE_ENTRY_ELTNAME_SIZE];/* Space for a 15 character label and a trailing null. */
        int   total_tmalloc_calls;      /* Total number of times all Tmalloc/Tcalloc's were called. */
        int   total_malloced_memory;    /* Total amount of malloced memory for all traceid's. */
        int   total_tfree_calls;        /* Total number of times all Tfree's were called. */
        int   total_freed_memory;       /* Total freed memory for all the Tfree's. */
        int   total_dlink_entries;      /* Total number (current) of Tmalloc/Tcalloc entry's on all traceid's doubly linked lists. */
    } total;
    struct{
        int   type_of_entry;            /* Indicates if its a "empty" (0), "total" (1), "Tmalloc/Tcalloc" (2) , or "Tfree" (2) entry. */
        char  eltName[TMALLOC_TABLE_ENTRY_ELTNAME_SIZE];/* Space for a 15 character element name (12 for element name plus 2 for ".c" version and a trailing null. */
        int   sum_tmalloc_calls;        /* Times this traceid's Tmalloc/Tcalloc was called. */
        int   sum_malloced_memory;      /* Sum total malloced memory from this traceid. */
        int   sum_tfree_calls;          /* Times a Tfree was called that freed this Tmalloc/Tcalloc'ed memory. */
        int   sum_freed_memory;         /* Sum total freed memory for all of this traceid's Tmalloc/Tcalloc's. */
        dLinkListHeader firstdLink;     /* Header pointing to first Tmalloc/Tcalloc entry on the doubly linked list and the number of entries. */

    } tmalloc;
    struct{
        int  type_of_entry;             /* Indicates if its a "empty" (0), "total" (1), "Tmalloc/Tcalloc" (2) , or "Tfree" (2) entry. */
        char eltName[TMALLOC_TABLE_ENTRY_ELTNAME_SIZE];/* Space for a 15 character element name (12 for element name plus 2 for ".c" version and a trailing null. */
        int  sum_tfree_calls;           /* Times this traceid's Tfree was called. */
        int  sum_freed_memory;          /* Sum total freed memory using this Tfree traceid. */
                                        /* Keep history of up to 3 Tmalloc/Tcalloc's whose memory this Tfree released. */
        int  tmalloc_traceid1;          /* Traceid 1 for the Tmalloc/Tcalloc. */
        int  traceid1_tfree_calls;      /* Times this traceid's Tfree was called for Traceid 1 for the Tmalloc/Tcalloc. */
        int  sum_traceid1_freed_memory; /* Sum total freed memory for traceid 1's Tmalloc/Tcalloc. */
        int  tmalloc_traceid2;          /* Traceid 2 for the Tmalloc/Tcalloc. */
        int  traceid2_tfree_calls;      /* Times this traceid's Tfree was called for Traceid 2 for the Tmalloc/Tcalloc. */
        int  sum_traceid2_freed_memory; /* Sum total freed memory for traceid 1's Tmalloc/Tcalloc. */
        int  untraceid_tfree_calls;     /* Times all other traceid's Tfree was called for Traceid 2 for the Tmalloc/Tcalloc. */
        int  sum_untraceid_freed_memory;/* Sum total freed memory for all other traceid's Tmalloc/Tcalloc. */
    } tfree;
} tmalloc_table_entry;

#define TMALLOC_TABLE_ENTRY_SIZE sizeof(tmalloc_table_entry) /* Size of one tmalloc_table_entry */
#define TMALLOC_TABLE_SIZE_BYTES ((TMALLOC_TRACEID_MAX + 1) * sizeof(tmalloc_table_entry)) /* Size in bytes of table with (TMALLOC_TRACEID_MAX + 1) tmalloc_table_entry's */
#define TMALLOC_TABLE_ENTRY_TYPE_UNUSED  0  /* type_of_entry value for all the unused entries. */
#define TMALLOC_TABLE_ENTRY_TYPE_TOTAL   1  /* type_of_entry value for the "total" entry.      */
#define TMALLOC_TABLE_ENTRY_TYPE_TMALLOC 2  /* type_of_entry value for the "tmalloc" entries.  */
#define TMALLOC_TABLE_ENTRY_TYPE_TFREE   3  /* type_of_entry value for the "tfree" entries.    */

/* These Tmalloc tracking prototypes are defined here, if needed, to avoid #include ordering problems */
#if MONITOR_TMALLOC_USAGE /* Tmalloc/Tcalloc/Tfree tracking */
void output_malloc_tracking_summary();
void addDLinkListEntry(dLinkListHeader * dListHeaderPtr, dLinkListEntry * dLinkListEntryPtr);
void removeDLinkListEntry(dLinkListHeader * dListHeaderPtr, dLinkListEntry * dLinkListEntryPtr);
void dumpAllDlinkedEntries(dLinkListHeader * dListHeaderPtr);
#endif /* Tmalloc/Tcalloc/Tfree tracking */

/*
 * Server Type Information for Message Inserts
 * Example Usage:
 *      getLocalizedMessageServer(...,SERVERTYPENAME, ...);
 *      or
 *      if (SERVERTYPE == NONXA) {...}
 */
#define NONXA 0     /* Normal JDBC Server */
#define XA 1        /* JDBC XA Server */

#ifndef XABUILD
#define SERVERTYPE 0 /* 0 indicates a JDBC Server */
#define SERVERTYPENAME "JDBC SERVER"
#else

#define SERVERTYPE 1 /* 1 indicates a JDBC XA Server */
#define SERVERTYPENAME "JDBC XA SERVER"
#endif

/* Server Usage types. */
#define SERVER_USAGETYPE_NOTUSED                  0  /*  NOT USED - 0 is not used as a server usage type. */
#define SERVER_USAGETYPE_NOTUSED1                 1  /*  NOT USED - 1 was used for Direct mode. */
#define SERVER_USAGETYPE_JDBCSERVER               2  /* Indicates a JDBC Server. */
#define SERVER_USAGETYPE_JDBCXASERVER_UNASSIGNED  3  /* Indicates a JDBC XA Server that is not now stateful (XA) or stateless (non-XA). */
#define SERVER_USAGETYPE_JDBCXASERVER_XA          4  /* Indicates a JDBC XA Server that is now stateful (XA). */
#define SERVER_USAGETYPE_JDBCXASERVER_NONXA       5  /* Indicates a JDBC XA Server that is now stateless (non-XA). */

/* define constants */
#define DEFAULT_SERVER_RECEIVE_TIMEOUT 60000                   /* Initial timeout 60 sec., overridden by configuration    */
#define DEFAULT_SERVER_SEND_TIMEOUT 60000                      /* Initial timeout 60 sec., overridden by configuration    */
#define DEFAULT_CLIENT_RECEIVE_TIMEOUT 900000                  /* Initial timeout is 15 minutes, overridden by configuration/connection */
#define INITIAL_REQUESTPACKET_READSIZE 5                       /* 5 bytes: 4 byte request packet length, 1 byte id field  */
#define STANDING_REQUEST_PACKET_SIZE 1000                      /* Byte size of the standing response packet - used for    */
                                                               /* for the vast majority of our request packet tasks. The  */
                                                               /* size must be a multiple of 8 to avoid malloc "orphan" issue. */
#define MAX_APPGROUP_NAMELEN    12                             /* Restrict the appgroup name to 12 chars.                 */
#define MAX_HOST_IPADDRLEN      30                             /* Maximum length of the Host IP address.                  */
#define MAX_USERID_LEN          12                             /* Maximum length of a 2200 user id                        */
#define MAX_USERID_PASS_LEN      6                             /* Maximum length of the JDBC Adminstrator's 2200 password */
#define MAX_EXCERPT_IMAGELEN    60                             /* Excerpt (up to first 60 chars) of this/last SQL command */
#define MAX_FILE_NAME_LEN      100                             /* Enough space in characters for a 2200 file name         */
#define MAX_SERVERNAME_LEN      24                             /* Maximum length in characters of the JDBC Server's name  */
#define MAX_CHARSET_LEN         30                             /* Maximum length in characters of the charset name        */
#define MAX_SCHEMA_NAME_LEN     30                             /* Maximum length in characters of a schema or version name*/
#define MAX_STORAGEAREA_NAME_LEN 70                            /* Maximum length in characters for a full storageArea     */
                                                               /* connection property string, e.g. "schema_name"."storage_area_name" */
#define SERVER_ERROR_MESSAGE_BUFFER_LENGTH 500                 /* Max. size for the error message buffers inside the JDBC */
                                                               /* Server; give enough room for multiple lines if needed   */
#define RUNID_CHAR_SIZE         6                              /* Number of character in a 2200 run ID                    */
#define MIN_KEYIN_CHAR_SIZE     1                              /* Minimum number of characters allowed in the keyin name  */
#define KEYIN_CHAR_SIZE         8                              /* Maximum number of characters in the keyin name for console cmds */

#define ASG_FILE_TRACK_SIZE 9999                               /* Default file size in tracks for cycled @ASG,CPZ files   */
#define MAX_COM_OUTPUT_BYTE_SIZE 78                            /* ER COM$ buffer for sending output back to @@CONS sender */
#define BRK_FILE_USE_NAME "JDBC$BRK"                           /* 2200 USE name for Server's breakpoint file                              */
#define LOG_FILE_USE_NAME "JDBC$LOG"                           /* 2200 USE name for log file                              */
#define TRACE_FILE_USE_NAME "JDBC$TRACE"                       /* 2200 USE name for trace file                            */
#define CLIENT_TRACE_FILE_USE_NAME_PREFIX "JDBC$C"             /* Prefix for the USE name for a client trace file         */
#define CLIENT_TRACEFILE_DEFAULT_FILENAME_PART "TRC"           /* The file name part of a generated client tracefile name using timestamp */
#define ELT_PREFIX_LEN 5                                       /* smgs element name prefix                                */
#define MAX_2200_ELT_NAME_SIZE 12
#define MAX_LOCALE_LEN MAX_2200_ELT_NAME_SIZE-ELT_PREFIX_LEN-1 /* Maximum length for local part of smsg-xxxx              */
    /* 6 characters (since we need 6 of the 12 characters in an element name
       for "SMSGS-" as a prefix before the locale name. */
#define MAXIMAL_LOCALE_LEN 400                                 /* A sufficiently large length for the locale string argument sent to doBeginThread().*/
#define CLIENT_TRACEFILE_TRACKS_MAX     262143                 /* Upper limit for 'client_tracefile_max_trks' parameter */
#define CLIENT_TRACEFILE_TRACKS_MIN       9999                 /* Lower limit for 'client_tracefile_max_trks' parameter */
#define CLIENT_TRACEFILE_TRACKS_DEFAULT 262143                 /* Default value for 'client_tracefile_max_trks' parameter */
#define CLIENT_TRACEFILE_CYCLES_MAX         32                 /* Upper limit for 'client_tracefile_max_cycles' parameter */
#define CLIENT_TRACEFILE_CYCLES_MIN          1                 /* Lower limit for 'client_tracefile_max_cycles' parameter */
#define CLIENT_TRACEFILE_CYCLES_DEFAULT      5                 /* Default value for 'client_tracefile_max_cycles' parameter */
#define PRINT$_FILE                     "PRINT$"               /* PRINT$ in uppercase, used for client trace file name testing. */

 /*
  * Special sfile filename indicators
  * for indicating usage of the default sfile name while in JDBC
  * Server, or a JDBC XA Server, etc..
  * NOTE: The defined name indicators must always be specified in lowercase
  * since sendAndRecieve() and ProcessClientTask() will
  * send/utilize them in lowercase.
  */
#define RDMSTRACE_DEFAULT_SERVER_FILE_NAME        "*jdbctrc-"
#define RDMSTRACE_DEFAULT_SERVER_FILE_NAME_LEN    9
#define RDMSTRACE_DEFAULT_XASERVER_FILE_NAME      "*jdbct-"
#define RDMSTRACE_DEFAULT_XASERVER_FILE_NAME_LEN  7
#define RDMSTRACE_SFILE_TO_FILE_ARGUMENT          "[file]"
#define RDMSTRACE_SFILE_TO_FILE_ARGUMENT_LEN      6
#define RDMSTRACE_SFILE_DEFAULT_ARGUMENT          "[default]"
#define RDMSTRACE_SFILE_DEFAULT_ARGUMENT_LEN      9

/* defines for creating RDMS Thread Names */
#define RDMS_THREADNAME_LEN                        8           /* length of an RDMS Thread name                           */
#define VISIBLE_RDMS_THREAD_NAME_LEN               6           /* length in chars of the visible (leading non-blanks, i.e. as seen in UDSMON) RDMS thread name */
#define CONFIG_DEFAULT_RDMS_THREADNAME_PREFIX_LEN  2           /* Thread name prefix is 2 characters long. */
#define CONFIG_DEFAULT_RDMS_THREADNAME_PREFIX      "JS"        /* 2 character prefix indicates a JDBC Server thread name  */

/* defines for UDS/RDMS/RSA usage detection */
#define CALLING_RDMS                      1                    /* indicates we are calling UDS/RDMS/RSA subsystem */
#define NOT_CALLING_RDMS                  0                    /* indicates we are not calling UDS/RDMS/RSA subsystem */

/* defines for the client keep alive thread configuration parameter */
#define CLIENT_KEEPALIVE_OFF              0        /* Indicates keep alive thread not be used. (Config value that can be overrided by client) */
#define CLIENT_KEEPALIVE_ON               1        /* Indicates keep alive thread be used. (Config value that can be overrided by client)*/
#define CLIENT_KEEPALIVE_ALWAYS_OFF       2        /* Indicates keep alive thread must never be used. (Config value cannot be overrided by client)*/
#define CLIENT_KEEPALIVE_ALWAYS_ON        3        /* Indicates a keep alive thread must always be used. (Config value cannot be overrided by client)*/
#define CLIENT_KEEPALIVE_DEFAULT          4        /* Client indicates the Server's Config default value for the keep alive thread be used. */
#define CONFIG_CLIENT_KEEPALIVE_OFF       "OFF"    /* Config indicator - keep alive thread not be used. (Config value that can be overrided by client) */
#define CONFIG_CLIENT_KEEPALIVE_ON        "ON"     /* Config indicator -  keep alive thread be used. (Config value that can be overrided by client)*/
#define CONFIG_CLIENT_KEEPALIVE_ALWAYS_OFF "ALWAYS_OFF" /* Config indicator -  keep alive thread must never be used. (Config value cannot be overrided by client)*/
#define CONFIG_CLIENT_KEEPALIVE_ALWAYS_ON "ALWAYS_ON"   /* Config indicator -  a keep alive thread must always be used. (Config value cannot be overrided by client)*/

/* defines for octal character detection and conversion. */
#define isoctal(c) ((c) >= '0' && (c) < '8')
#define OCT_CONVERT (('0' << 12) + ('0' << 9) + ('0' << 6) + ('0' << 3) + '0')

#ifdef XABUILD
/* defines for JDBC XA Server usage */
#define UDSICR_QUAL_BDI_FILENAME         "TPF$.UDSICR/BDI"            /* Location of file with extracted UDS ICR qual/bdi information */
#endif

/* defines for the COMAPI mode and usage changes */
#define COMAPI_MODE_AND_BDI_FILENAME     "TPF$.COMAPI/BDI"            /* Location of file with extracted COMAPI mode/bdi information */
#define COMAPI_MODE_NAMES                "ABCDEFGHIJKLMNOPQRSTUVWXYZ" /* used to get the COMAPI mode name */
#define MAX_COMAPI_MODES                  26                          /* Maximum number of COMAPI modes */
#define MAX_SERVER_SOCKETS                4                           /* Maximum number of Server sockets per ICL (two for CPComm, CMS) */
#define DEFAULT_LISTEN_IP_ADDRESS         0                           /* Default local host listen IP address. */
#define DEFAULT_LISTEN_IP_ADDRESS_STR     "0"                         /* Default local host listen IP address. */
#define FOUR_CHARS                        4
#define NO_ICL_IS_CURRENTLY_HANDLING_SW_CH_SHUTDOWN -1                /* Indicates no ICL_num value is present. */
#define DEFAULT_COMAPI_RECONNECT_WAIT_TIME        6000                /* Time to wait (6 sec.) between COMAPI re-connect attempts */
#define CALLING_COMAPI                    1                           /* indicates we are calling COMAPI subsystem */
#define NOT_CALLING_COMAPI                0                           /* indicates we are not calling COMAPI subsystem */
#define HOST_NAME_MAX_LEN               256                           /* Maximum size of a host name in characters */
#define IP_ADDR_CHAR_LEN                 45                           /* Size of string with IP address in form "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255" */
#define UNKNOWN_HOST_NAME               "<unknown host>"              /* return value for IP address that can't be found */
#define EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP -1                         /* Indicates we do not have an explicit IP address (need to look host name's IP address) */
#define NUMBER_OF_COMAPI_RECONNECT_TRY_FAILED_MSGS_TO_LOG 5           /* Log this many of the COMAPI 10001 before skipping them (cuts down on extraneous log messages) */

/* defines for the JDBC user access security file checking */
#define MSCON_TWAIT_TIME 10000                                 /* wait 10 seconds between MSCON$ calls */
#define UDS_APP_GROUP_INFO_FILENAME "UDS$$SRC*UREP$BDI"        /* Name of file containing App group numbers, BDI's, etc. */
#define USER_ACCESS_FILENAME_LEN 24                            /* Length of full file qualifier and full file name */
#define JDBC_USER_ACCESS_FILENAME "sys$lib$    jdbc$rdms"      /* Initial JDBC user access security file name (12 char. qualifier and 9 char. filename)  */
#define SSAUTHNTICAT 73                                        /* Bit number of the SSAUTHNTICAT privilege in the userid_record */
                                                               /* the app number (n or nn ) will be appended at the end of the nn file name when used. */
#define USER_ACCESS_CONTROL_OFF  0                             /* JDBC Server user id access control is turned off (default) */
#define USER_ACCESS_CONTROL_JDBC 1                             /* JDBC Server user id access control, using SECOPT-1 security, is turned on. */
#define USER_ACCESS_CONTROL_FUND 2                             /* JDBC Server user id access control, using fundamental security, is turned on. */
#define CONFIG_JDBC_USER_ACCESS_CONTROL_OFF  "OFF"             /* Config string value indicating JDBC user access security is off */
#define CONFIG_JDBC_USER_ACCESS_CONTROL_JDBC "JDBC"            /* Config string value indicating JDBC user access security, using SECOPT-1 security, to be on */
#define CONFIG_JDBC_USER_ACCESS_CONTROL_JDBC_SECOPT1 "JDBC_SECOPT1"   /* Config string value indicating JDBC user access security, using SECOPT-1 security, to be on */
#define CONFIG_JDBC_USER_ACCESS_CONTROL_FUND "FUND"                   /* Config string value indicating JDBC user access security, using fundamental security, to be on */
#define CONFIG_JDBC_USER_ACCESS_CONTROL_FUNDAMENTAL "JDBC_FUNDAMENTAL"/* Config string value indicating JDBC user access security, using fundamental security, to be on */

#define LOG_CONSOLE_OUTPUT_OFF  0                              /* JDBC Server log console output is off */
#define LOG_CONSOLE_OUTPUT_ON 1                                /* JDBC Server log console output is on (console output also goes to log file) */
#define CONFIG_JDBC_LOG_CONSOLE_OUTPUT_OFF  "OFF"              /* Config string value indicating JDBC log console output is off */
#define CONFIG_JDBC_LOG_CONSOLE_OUTPUT_ON "ON"                 /* Config string value indicating JDBC log console output is on */

/* defines for the server_priority configuration parameter */
#define ER_LEVEL   0207                                        /* The ER Level$ index number */
#define MAX_SERVER_PRIORITY_LEN               6                /* Maximum length of switching priority name string. */

#define CONFIG_JDBC_SERVER_PRIORITY_TXN      "TXN"             /* Transaction Priority */
#define CONFIG_JDBC_SERVER_PRIORITY_USER     "USER"            /* User Priority */
#define CONFIG_JDBC_SERVER_PRIORITY_BATCH    "BATCH"           /* Batch Priority */
#define CONFIG_JDBC_SERVER_PRIORITY_DEMAND   "DEMAND"          /* Demand Priority */
#define CONFIG_JDBC_SERVER_PRIORITY_LEVEL$   "LEVEL$"          /* FORMAT 4 A0 user specified */


#define CONFIG_JDBC_ER_LEVEL_ARG_TXN       0600001000002       /* LEVEL$ A0 value for setting TXN priority */
#define CONFIG_JDBC_ER_LEVEL_ARG_USER      0620001000002       /* LEVEL$ A0 value for setting USER priority */
#define CONFIG_JDBC_ER_LEVEL_ARG_BATCH     0630001000002       /* LEVEL$ A0 value for setting BATCH priority */
#define CONFIG_JDBC_ER_LEVEL_ARG_DEMAND    0640001000002       /* LEVEL$ A0 value for setting DEMAND priority */

/* This server priority is not currently defined for JDBC Server usage*/
/* #define CONFIG_JDBC_SERVER_PRIORITY_DEADLINE "DEDLIN"    */       /* Deadline Priority */
/* #define CONFIG_JDBC_ER_LEVEL_ARG_DEADLINE  0610001000002 */      /* LEVEL$ A0 value for setting DEADLINE priority */

#define DEFAULT_SERVER_PRIORITY  CONFIG_JDBC_SERVER_PRIORITY_TXN /* Default is transactional priority */
#define DEFAULT_ER_LEVEL_ARG    CONFIG_JDBC_ER_LEVEL_ARG_TXN   /* LEVEL$ A0 value for setting default TXN priority */

/*
 * RDMS has allocated the error number range 4000-4049 for use by RDMS JDBC. These psuedo-RDMS error
 * numbers are used within the JDBC Server and the C-Interface code to handle special situations.
 * Error responses for these psuedo-RDMS errors may or may not be returned to the JDBC client.
 *
 * Definitions of psuedo-RDMS errors 4000-4049:
 */
/* define RDMSJDBC_INVALID_VERIFICATION_ID        4025         Incorrect RDMS SQL section or metadata verification id value. (used in statement.h) */

/* Define JDBC Server error codes */
#define JDBCSERVER_MASTER_RUN_USERID_ERROR        5001         /* RDMS error 8006 - not running under master run user id. */
#define JDBCSERVER_CANT_OPEN_LOGFILE              5002         /* Can't open the JDBCServer Log File                      */
#define JDBCSERVER_BAD_CONSOLE_LISTENER_READ      5007         /* Received a bad read by the Console Listener             */
#define JDBCSERVER_EXIT_SERVER                    5008         /* Console Listener got an "Exit Server" command           */
#define JDBCSERVER_CANT_ALLOCATEWORKER_WDE_MEMORY 5009         /* Can't allocate memory for a Server Worker entry         */
#define JDBCSERVER_CANT_START_ICL                 5010         /* Can't start the ICL activity                            */
#define JDBCSERVER_ICL_CANT_REGISTER_WITH_COMAPI  5011         /* Can't register with COMAPI                              */
#define JDBCSERVER_ICL_CANT_GET_COMAPI_SOCKET     5012         /* Can't get ICL a COMAPI server socket (session table id) */
#define JDBCSERVER_ICL_CANT_SET_COMAPI_SOCKETOPTS 5013         /* ICL set the COMAPI options on server socket             */
#define JDBCSERVER_CANT_OPEN_SERVER_LOGFILE       5014         /* Can't open the Server Log file                          */
#define JDBCSERVER_CANT_OPEN_SERVER_TRACEFILE     5015         /* Can't open the Server Trace file                        */
#define JDBCSERVER_CANT_CLOSE_SERVER_LOGFILE      5016         /* Can't close Server Log file                             */
#define JDBCSERVER_CANT_CLOSE_SERVER_TRACEFILE    5017         /* Can't close Server Trace file                           */
#define JDBCSERVER_CANT_OPEN_CLIENT_TRACEFILE     5018         /* Can't cycle and open JDBC Client Trace file             */
#define JDBCSERVER_CANT_CLOSE_CLIENT_TRACEFILE    5019         /* Can't close JDBC Client Trace file                      */
#define JDBCSERVER_PROCESSTASK_RETURNED_ERROR     5020         /* Process task returned a error response packet           */
#define JDBCSERVER_UNABLE_TO_REGISTER_FOR_CONS    5021         /* Can't set up ER KEYIN$ registration for @@CONS commands */
#define JDBCSERVER_UNABLE_TO_GET_CONS_CMD         5022         /* Can't get the @@CONS command                            */
#define JDBCSERVER_UNABLE_TO_DEREGISTER_FOR_CONS  5023         /* ER KEYIN$ deregistration function failed for @@CONS     */
#define JDBCSERVER_UNABLE_TO_PERFORM_ER_COM       5024         /* ER COM$ failed for @@CONS console handling              */
#define JDBCSERVER_CANT_CLOSE_SERVER_LOG_TRACEFILES 5025       /* Can't close both the Server's Log and Trace files       */
#define JDBCSERVER_UNABLE_TO_FIND_SPECIFIED_WORKER 5026        /* Can't locate Server Worker on assigned chain            */
#define JDBDSERVER_UNABLE_TO_OPEN_DEFAULT_MSG_FILE 5027        /* Can't find message element JDBC$MSGFILE.SMSGS/PROPERTIES */
#define JDBDSERVER_UNABLE_TO_OPEN_MSG_FILE_USE_DEFAULT 5028    /* Can't find message elt for locale, default msg elt used */
#define JDBCSERVER_UNABLE_TO_ALLOCATE_NEW_WDE     5029         /* Can't allocate a new WDE entry, out of WDEAREA space */
#define JDBCSERVER_CANT_OPEN_CLIENT_TRACEFILE2    5030         /* Can't open JDBC Client Trace file                       */
#define JDBCSERVER_CLOSE_CLIENT_TRACETABLE        5031         /* Can't find entry in ClientTraceTable                    */
#define JDBCSERVER_SERVER_IDENTIFICATION_MISMATCH 5032         /* Server's information identification descriptor does not client's */
#define JDBCSERVER_CLIENT_SERVER_INCOMPATIBIlITY  5033          /* Client level is incompatible with the server level */

#define JDBCSERVER_OBSOLETE_CONFIG_PARAMETER                    5097
#define JDBCSERVER_UNKNOWN_CONFIG_PARAMETER                     5098
#define JDBCSERVER_CONFIG_MISSING_CONFIG_FILENAME               5099
#define JDBCSERVER_CONFIG_CANT_OPEN_CONFIG_FILE                 5100
#define JDBCSERVER_CONFIG_INVALID_PARAMETER_FORMAT              5101
#define JDBCSERVER_CONFIG_SERVERNAME_LENGTH_ERROR               5102
#define JDBCSERVER_CONFIG_MAX_ACTIVITIES_ERROR                  5103
#define JDBCSERVER_CONFIG_MAX_QUEUED_COMAPI_ERROR               5104
#define JDBCSERVER_CONFIG_APPGROUPNAME_LENGTH_ERROR             5105
#define JDBCSERVER_CONFIG_HOSTPORTNUMBER_ERROR                  5107
#define JDBCSERVER_CONFIG_SERVER_RECEIVE_TIMEOUT_ERROR          5112
#define JDBCSERVER_CONFIG_SERVER_SEND_TIMEOUT_ERROR             5113
#define JDBCSERVER_CONFIG_CLIENT_RECEIVE_TIMEOUT_ERROR          5114
#define JDBCSERVER_CONFIG_LOGFILENAME_LENGTH_ERROR              5115
#define JDBCSERVER_CONFIG_TRACEFILENAME_LENGTH_ERROR            5116
#define JDBCSERVER_CONFIG_FETCHBLOCK_SIZE_ERROR                 5142
#define JDBCSERVER_CONFIG_SW_CLIENT_TRACEFILENAME_LENGTH_ERROR  5149
#define JDBCSERVER_CONFIG_TRACEFILEQUAL_LENGTH_ERROR            5152
#define JDBCSERVER_CONFIG_SERVER_LOCALE_LENGTH_ERROR            5155
#define JDBCSERVER_CONFIG_APPGROUPNUMBER_ERROR                  5163
#define JDBCSERVER_CONFIG_UDS_ICR_BDI_ERROR                     5164
#define JDBCSERVER_CONFIG_UDS_ICR_BDI_MISSING_ERROR             5165
#define JDBCSERVER_CONFIG_RSA_BDI_ERROR                         5166
#define JDBCSERVER_CONFIG_RSA_BDI_DEFAULT_RETRIEVED             5167
#define JDBCSERVER_CONFIG_APP_GROUP_NAME_RSA_BDI_ERROR          5168
#define JDBCSERVER_CONFIG_APP_GROUP_NUMBER_DEFAULT_RETRIEVED    5169
#define JDBCSERVER_CONFIG_LOGFILENAME_TRACEFILENAME_NOT_USED    5170
#define JDBCSERVER_VALIDATE_SERVER_PRIORITY_ERROR               5171
#define JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED        5172
#define JDBCSERVER_CONFIG_LISTENHOSTS_ERROR                     5173
#define JDBCSERVER_CONFIG_BAD_LISTEN_HOSTNAME_FORMAT            5174
#define JDBCSERVER_CONFIG_RDMS_THREADNAME_PREFIX_ERROR          5175
#define JDBCSERVER_CONFIG_CLIENT_KEEP_ALIVE_ERROR               5176
#define JDBCSERVER_CONFIG_TRACEFILE_TRACKS_ERROR                5177
#define JDBCSERVER_CONFIG_TRACEFILE_CYCLES_ERROR                5178
#define JDBCSERVER_CONFIG_INCORRECT_APPGROUPNUMBER_ERROR        5179
#define JDBCSERVER_CONFIG_UDS_ICR_BDI_DEFAULT_RETRIEVED         5180
#define JDBCSERVER_CONFIG_KEYIN_ID_XA_GENERATED_VALUE           5181
#define JDBCSERVER_CONFIG_XA_THREAD_REUSE_LIMIT_ERROR           5182

#define JDBCSERVER_CONFIG_MISSING_HOSTPORT_ERROR                5118

#define JDBCSERVER_VALIDATE_MAXACTIVITIES_ERROR                 5121
#define JDBCSERVER_VALIDATE_MAXACTIVITIES_WARNING              -5122
#define JDBCSERVER_VALIDATE_MAXQUEUEDCOMAPI_ERROR               5123
#define JDBCSERVER_VALIDATE_MAXQUEUEDCOMAPI_WARNING            -5124
#define JDBCSERVER_VALIDATE_LOGFILENAME_ERROR                   5125
#define JDBCSERVER_VALIDATE_LOGFILENAME_WARNING                -5126
#define JDBCSERVER_VALIDATE_TRACEFILENAME_ERROR                 5127
#define JDBCSERVER_VALIDATE_TRACEFILENAME_WARNING              -5128
#define JDBCSERVER_VALIDATE_SERVER_RECEIVE_TIMEOUT_ERROR              5133
#define JDBCSERVER_VALIDATE_SERVER_RECEIVE_TIMEOUT_WARNING           -5134
#define JDBCSERVER_VALIDATE_SERVER_SEND_TIMEOUT_ERROR                 5135
#define JDBCSERVER_VALIDATE_SERVER_SEND_TIMEOUT_WARNING              -5136
#define JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_ERROR              5137
#define JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_WARNING           -5138
#define JDBCSERVER_VALIDATE_GENERIC_ERROR                             5141
#define JDBCSERVER_VALIDATE_SW_CLIENT_RECEIVE_TIMEOUT_ERROR           5143
#define JDBCSERVER_VALIDATE_SW_CLIENT_RECEIVE_TIMEOUT_WARNING        -5144
#define JDBCSERVER_VALIDATE_FETCHBLOCK_SIZE_ERROR                     5145
#define JDBCSERVER_VALIDATE_FETCHBLOCK_SIZE_WARNING                  -5146
#define JDBCSERVER_VALIDATE_SW_FETCHBLOCK_SIZE_ERROR                  5147
#define JDBCSERVER_VALIDATE_SW_FETCHBLOCK_SIZE_WARNING               -5148
#define JDBCSERVER_VALIDATE_SW_CLIENT_TRACEFILENAME_ERROR             5150
#define JDBCSERVER_VALIDATE_SW_CLIENT_TRACEFILENAME_WARNING          -5151
#define JDBCSERVER_VALIDATE_TRACEFILEQUAL_LENGTH_ERROR                5153
#define JDBCSERVER_VALIDATE_TRACEFILEQUAL_LENGTH_WARNING             -5154
#define JDBCSERVER_VALIDATE_SERVER_LOCALE_ERROR                       5156
#define JDBCSERVER_VALIDATE_SERVER_LOCALE_WARNING                    -5157
#define JDBCSERVER_VALIDATE_KEYIN_ID_ERROR                            5158
#define JDBCSERVER_VALIDATE_USER_ACCESS_CONTROL_ERROR                 5159
#define JDBCSERVER_VALIDATE_COMAPI_MODES_ERROR                        5160
#define JDBCSERVER_VALIDATE_COMAPI_MODES_DUP_MODE_VALUE_IGNORED       5161
#define JDBCSERVER_VALIDATE_LOG_CONSOLE_OUTPUT_WARNING               -5162

/* Console handler error numbers */

#define CH_ACSF_FAILED                          5200
#define CH_INVALID_CYCLE_CMD                    5201
#define CH_INVALID_CYCLE_CLIENT_CMD             5202
#define CH_INVALID_WORKER_ID                    5203
#define CH_NO_CYCLING_POSSIBLE                  5204
#define CH_NO_NEW_CYCLE_CREATED                 5205
#define CH_NEW_CYCLE_NOT_OPENED                 5206
#define CH_TRACE_FILE_USE_FAILED                5207
#define CH_FCLOSE_FAILED_FOR_F_CYCLE            5208
#define CH_F_CYCLE_CREATED                      5209
#define CH_INVALID_DISPLAY_CMD                  5210
#define CH_INVALID_DISPLAY_WORKER_CMD           5211
#define CH_STARTING_CONSOLE                     5212
#define CH_BAD_UCSEMTOBM_RETURN_FOR_KEYIN       5213
#define CH_KEYIN_PKT_TOO_SMALL                  5214
#define CH_KEYIN_WAIT_FN_ERROR                  5215
#define CH_BAD_UCSEMTOBM_RETURN_FOR_COM_TEXT    5216
#define CH_BAD_UCSEMTOBM_RETURN_FOR_COM_PKT     5217
#define CH_ER_COM_ERROR                         5218
#define CH_BAD_UCSEMTOBM_FOR_KEYIN_PKT          5219
#define CH_KEYIN_REGISTER_FAILED                5220
#define CH_KEYIN_DEREGISTER_FAILED              5221
#define CH_INVALID_CMD                          5222
#define CH_INVALID_SET_CMD                      5223
#define CH_CLIENT_TRACE_FILE_USE_FAILED         5224
#define CH_CLOSE_CLIENT_TRACE_FILE_FAILED       5225
#define CH_CONFIG_PARAM_CHANGED                 5226
#define CH_ABORT_CMD_SHUTDOWN                   5227
#define CH_INVALID_SHUTDOWN_CMD                 5228
#define CH_TERM_CMD_SHUTDOWN                    5229
#define CH_SHUTDOWN_TERMINATION                 5230
#define CH_ABORT_ABNORMAL_SHUTDOWN              5231
#define CH_KEYIN_CAUSING_NORMAL_SHUTDOWN        5232
#define CH_LOG_TRACE_FILE_CLOSE_ERROR           5233
#define CH_INVALID_SHUTDOWN_WORKER_CMD          5234
#define CH_INVALID_SHUTDOWN_ABORT_WORKER_CMD    5235
#define CH_WORKER_SHUTTING_DOWN                 5236
#define CH_INVALID_TURN_CMD                     5237
#define CH_INVALID_TURN_ON_CLIENT_CMD           5238
#define CH_INVALID_TURN_OFF_CLIENT_CMD          5239
#define CH_INVALID_TURN_ON_OFF_CLIENT_CMD       5240
#define CH_TRACE_ALREADY_OFF                    5241
#define CH_FCLOSE_FAILED_ON_TRACE               5242
#define CH_TRACE_TURNED_OFF                     5243
#define CH_TRACE_ALREADY_ON                     5244
#define CH_NO_TRACE_FILE_SPECIFIED              5245
#define CH_TRACE_FILE_NOT_OPENED                5246
#define CH_TRACE_TURNED_ON                      5247
#define CH_INVALID_HELP_CMD                     5248
#define CH_KEYIN_NAME_NOT_AVAIL                 5249
#define CH_INVALID_CLEAR_CMD                    5250
#define CH_INVALID_XA_CMD                       5251
#define CH_KEYIN_NAME_SECURITY                  5252
#define CH_CYCLING_PRINT$_NOT_ALLOWED           5253

/* Server error messages */
#define SERVER_CANT_ALLOC_RESPONSE_PKT          5300
/* #define ?????                                5301 */
#define SERVER_V_OPTION_CONFIG_ERROR            5302
#define SERVER_TERMINATION                      5303
#define SERVER_BEGIN_CONFIG                     5304
#define SERVER_V_OPTION_DONE                    5305
#define SERVER_V_OPTION_WARNINGS                5306
#define SERVER_V_OPTION_ERRORS                  5307
#define SERVER_CONFIG_DONE                      5308
#define SERVER_USING_CONFIG                     5309
#define SERVER_USING_MODIFIED_CONFIG            5310
#define SERVER_BAD_RSA_BDI_CALL                 5311
#define SERVER_KEYIN_FAILED                     5312
#define SERVER_ICL_FAILURE                      5313
#define SERVER_CONSOLE_LISTENER_FAILURE         5314
#define SERVER_GRACEFUL_SHUTDOWN                5315
#define SERVER_ACTIVITY_SHUTDOWN                5316
#define SERVER_COMAPI_REGISTER_FAILURE          5317
#define SERVER_CANT_START_ICL                   5318
#define SERVER_CANT_SET_COMAPI_DEBUG_OPTION     5319
#define SERVER_CANT_SET_LINGER_OPTION           5320
#define SERVER_CANT_SET_BLOCKING_OPTION         5321
#define SERVER_CANT_SET_RECEIVE_TIMEOUT         5322
#define SERVER_CANT_SET_SEND_TIMEOUT            5323
#define SERVER_CANT_START_ICL_ACTIVITY          5324
#define SERVER_ICL_STARTED                      5325
#define SERVER_TOO_MANY_PARMS                   5326
#define SERVER_COMAPI_RECEIVE_TIMEOUT_ERROR     5327
#define SERVER_COMAPI_SEND_TIMEOUT_ERROR        5328
#define SERVER_COMAPI_DEBUG_ERROR               5329
#define SERVER_INVALID_REQUEST_ID               5330
#define SERVER_NO_RESPONSE_PKT_GENERATED        5331
#define SERVER_LOG_FILE_USE_FAILED              5332
#define SERVER_TRACE_FILE_USE_FAILED            5333
#define SERVER_LOG_FILE_CLOSE_FAILED            5334
#define SERVER_TRACE_FILE_CLOSE_FAILED          5335
#define SERVER_CONFIG_MESSAGE_LINE_1            5336
#define SERVER_CONFIG_MESSAGE_LINE_2            5337
#define SERVER_CONFIG_MESSAGE_LINE_3            5338
#define SERVER_CANT_CLOSE_SOCKET_IN_ICL_SHUTDOWN  5339
#define SERVER_CANT_CLOSE_SERVER_SOCKET_IN_ICL  5340
#define SERVER_BAD_SHUTDOWN_POINT_IN_ICL_SHUTDOWN 5341
#define SERVER_SHUTDOWN_ICL_AND_SERVER          5342
#define SERVER_SHUTDOWN_IN_ICL                  5343
#define SERVER_BAD_DEREGISTER_KEYIN_IN_ICL      5344
#define SERVER_ICL_STARTED_CH_SHUTDOWN          5345
#define SERVER_ICL_CANT_LISTEN_ON_SERVER_SOCKET 5346
#define SERVER_ICL_LISTENING_ON_SERVER_SOCKET   5347
#define SERVER_ICL_CANT_ACCEPT_ON_SERVER_SOCKET 5348
/* #define ???                                  5349 */
#define SERVER_CANT_SHUTDOWN_ICL                5350
#define SERVER_MAX_ACT_REACHED                  5351
#define SERVER_CANT_GET_WDE_MEMORY              5354
#define SERVER_ACT_COUNT                        5355
#define SERVER_WORKER_ACT_SHUTTING_DOWN         5356
#define SERVER_CANT_CLOSE_CLIENT_SOCKET         5357
#define SERVER_BAD_DEREG_KEYIN                  5358
#define SERVER_SHUT_DOWN_SERVER_AND_WORKER      5359
#define SERVER_BAD_SW_SHUTDOWN_POINT            5360
#define SERVER_NEW_SW                           5361
#define SERVER_CLIENT_ACCEPTED                  5362
#define SERVER_CANT_ACCEPT_CONNECTION           5363
#define SERVER_CANT_SEND_ERROR_RESPONSE_PKT     5364
#define SERVER_CANT_CLOSE_SOCKET_MAX_ACT        5366
#define SERVER_CANT_RECEIVE_SOCKET_DATA         5367
#define SERVER_BAD_REQUEST_PKT_ID               5368
#define SERVER_CANT_RECEIVE_REQUEST_PKT         5369
#define SERVER_CANT_SEND_RESPONSE_PKT           5370
#define SERVER_CANT_SET_RECEIVE_TIMEOUT_OPTION  5371
#define SERVER_SW_CANT_CLOSE_CLIENT_SOCKET      5372
#define SERVER_HARDWARE_DOESNT_HAVE_QUEUING     5373
#define SERVER_CANT_RETRIEVE_RDMS_LEVEL         5374
#define SERVER_DOESNT_HAVE_ADEQUATE_RDMS_LEVEL  5375
#define SERVER_CANT_END_RDMS_LEVEL_THREAD       5376
#define SERVER_SHUT_DOWN_SERVERWORKER           5377
#define SERVER_CANT_DO_BEGIN_THREAD             5378
#define SERVER_CANT_START_UASFM_ACTIVITY        5379
#define SERVER_UASFM_FAILURE                    5380
#define SERVER_UASFM_SHUTDOWN                   5381
#define SERVER_UASFM_ABNORMAL_SHUTDOWN          5382
#define SERVER_UASFM_MSCON_ERROR                5383
#define SERVER_APP_NUMBER_NOT_FOUND_ERROR       5384
#define SERVER_CANT_OPENCLOSEFILE_UREPBDI_ERROR 5385
#define SERVER_SUVAL_ERROR_DETECTED             5386
#define SERVER_UASFM_STARTED                    5387
#define SERVER_CANT_OPEN_COMAPIBDI_FILE         5388
#define SERVER_BAD_COMAPI_MODE_BDI_INFO         5389
#define SERVER_CANT_CLOSE_BDI_FILE              5390
#define SERVER_THIS_COMAPI_MODES_NOT_AVAILABLE  5391
#define SERVER_NO_COMAPI_MODES_AVAILABLE        5392
#define SERVER_NO_ACCESS_COMAPI_BACK_RUN        5393
#define CATALOG_VERIFY_ERROR                    5394
#define SERVER_SHUT_DOWN_DUE_TO_INDICATORS      5395
#define SERVER_SHUT_DOWN_DUE_TO_TERMREG         5396
#define SERVER_CANNOT_START_XASERVER_CH_ACTIVITY 5397
#define SERVER_XASERVER_INITIALIZED_AND_WITH_CH 5398
#define SERVER_XASERVER_CH_STARTED_AND_RUNNING  5399
#define XASERVER_INITIALIZATION_ERROR           5400
#define XASERVER_BAD_REQUEST_PKT_ID             5401
#define XASERVER_BAD_UDS_REQ_RESOURCE_STATUS    5402
#define SERVER_SERVER_PRIORITY_STATUSES         5403
#define XASERVER_SERVER_PRIORITY_STATUSES       5404
#define SERVER_SERVER_ALT_PRIORITY_STATUSES     5405
#define XASERVER_SERVER_ALT_PRIORITY_STATUSES   5406
#define SERVER_SERVER_WASNOT_PRIORITY_STATUSES  5407
#define XASERVER_SERVER_WASNOT_PRIORITY_STATUSES 5408
#define SERVER_BAD_DEBUG_INFO_FORMAT_IN_REQPKT  5409
#define SERVER_BAD_DEBUG_INFO_FORMAT_IN_RESPKT  5410
#define SERVER_SOCKET_NOW_LISTENING             5412
#define XASERVER_CANT_OPEN_UDSICRBDI_FILE       5415
#define XASERVER_BAD_UDSICR_BDI_INFO            5416
#define SERVER_IS_RUNNING                       5417
#define XA_SERVER_IS_RUNNING                    5418
#define SERVER_IS_RUNNING_WITH_WARNINGS         5419
#define XA_SERVER_IS_RUNNING_WITH_WARNINGS      5420
#define SERVER_SHUTTING_DOWN                    5421
#define XA_SERVER_SHUTTING_DOWN                 5422
#define SERVER_SHUTTING_DOWN_WITH_ERRORS        5423
#define XA_SERVER_SHUTTING_DOWN_WITH_ERRORS     5424
#define SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT 5425
#define XA_SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT 5426
#define SERVER_ABORTING                         5427
#define XA_SERVER_ABORTING                      5428
#define SERVER_COMAPI_CONNECT_MESSAGE           5429
#define SERVER_COMAPI_DISCONNECT_MESSAGE        5430
#define XASERVER_BAD_BT_TASKCODE_AND_TRAN_STATE 5431
#define XASERVER_STATEFUL_CLIENT_ERROR          5432
#define SERVER_SSAUTHNTICAT_FAILURE             5433
#define SERVER_CANT_SET_IPV4V6_OPTION           5434
/* Note: RDMS SQL Section, Get Description, and PCA error numbers 6024-6029 are defined/described in statement.h */

/* New JDBC Server defines for JDBC$Config validation routines.*/
#define VALIDATE_ONLY                     1
#define VALIDATE_AND_UPDATE               2
#define VALIDATE_AND_UPDATE_ALLOW_DEFAULT 3

#define testOptionBit(letter) (sgd.option_bits & (1<<('Z'-letter)))
/*
 * These are the defines for the legal sgd.debug values.  Each
 * defined value causes a selected set of trace output to be
 * generated.  Each #define has a comment describing what trace
 * output is generated for that sgd.debug value.
 */
 #define SGD_DEBUG_MALLOCS 10 /* Produce a malloc(), calloc(), free() trace */
 #define SGD_DEBUG_TRACE_SUVAL                      64  /* Produce a JDBC Server SUVAL$ usage trace */
 #define SGD_DEBUG_TRACE_MSCON                      32  /* Produce a JDBC Server MSCON$ usage trace */
 #define SGD_DEBUG_TRACE_SUVAL_AND_MSCON            96  /* Produce a JDBC Server SUVAL$ and MSCON$ usage trace */
 #define SGD_DEBUG_TRACE_COMAPI                     16  /* Produce a JDBC Server COMAPI function trace */
 #define SGD_COMAPI_DEBUG_MODE_OFF                   0  /* TCP_SETOPTS value indicating no COMAPI debug mode */
 #define SGD_COMAPI_DEBUG_MODE_CALL_TRACE            1  /* TCP_SETOPTS value for COMAPI call trace */
 #define SGD_COMAPI_DEBUG_MODE_CALL_AND_DATA_TRACE  65  /* TCP_SETOPTS value for COMAPI call trace and partial data trace */

 #define TEST_SGD_DEBUG_TRACE_SUVAL     (sgd.debug & SGD_DEBUG_TRACE_SUVAL)
 #define TEST_SGD_DEBUG_TRACE_MSCON     (sgd.debug & SGD_DEBUG_TRACE_MSCON)
 #define TEST_SGD_DEBUG_TRACE_COMAPI    (sgd.debug & SGD_DEBUG_TRACE_COMAPI)


/*
 * Debug/trace defines to support debugFlags, debugPrefix, sfilename, etc.
 * These match with their counterparts on the JDBC Client side in RdmsObject.
 */
#define DEBUG_RSA_DUMPALLMASK     0x100
#define DEBUG_TSTAMPMASK          0x80
#define DEBUG_PREFIXMASK          0x40
#define DEBUG_SQLPMASK            0x20
#define DEBUG_SQLEMASK            0x10
#define DEBUG_SQLMASK             0x8
#define DEBUG_INTERNALMASK        0x4
#define DEBUG_DETAILMASK          0x2
#define DEBUG_BRIEFMASK           0x1

#define MAX_QUALIFIER_LEN          12
#define MAX_SFILE_FILELEN_WITHQUAL 30  /* this matches size of reqpacket debug info */

/* Indicators as to whether parameter is in JDBC$Config. */
#define GOT_NO_CONFIG_VALUE               0
#define GOT_CONFIG_VALUE                  1
#define HAVE_CONFIG_DEFAULT               2

/* Image length maximum for images from files JDBC$CONFIG., UREP$BDI., and TPF$.COMAPI/BDI */
#define IMAGE_MAX_CHARS 256

/* Product limits and defaults for Configuration parameters */
#define CONFIG_LIMIT_MINSERVERWORKERS                              10
#define CONFIG_LIMIT_MAXSERVERWORKERS                             800 /* RDMS 7.1 max_threads limit */
#define CONFIG_LIMIT_MINQUEUEDCLIENTS                              10
#define CONFIG_LIMIT_MINFILENAMELENGTH                              1
#define CONFIG_LIMIT_MAXFILENAMELENGTH              MAX_FILE_NAME_LEN
#define CONFIG_LIMIT_MINSERVER_RECEIVE_TIMEOUT                  60000
#define CONFIG_LIMIT_MAXSERVER_RECEIVE_TIMEOUT                 300000
#define CONFIG_LIMIT_MINSERVER_SEND_TIMEOUT                     60000
#define CONFIG_LIMIT_MAXSERVER_SEND_TIMEOUT                    120000
#define CONFIG_LIMIT_MINCLIENT_RECEIVE_TIMEOUT                  60000
#define CONFIG_LIMIT_MAXCLIENT_RECEIVE_TIMEOUT               172800000 /* two days - in milliseconds */
#define CONFIG_LIMIT_MINFETCHBLOCK_SIZE                          4096
#define CONFIG_LIMIT_MAXFETCHBLOCK_SIZE                        131072
#define CONFIG_LIMIT_MINCLIENT_DEFAULT_TRACEFILE_QUAL               1
#define CONFIG_LIMIT_MAXCLIENT_DEFAULT_TRACEFILE_QUAL              12
#define CONFIG_LIMIT_MINSERVER_LOCALE                               1
#define CONFIG_LIMIT_MAXSERVER_LOCALE                  MAX_LOCALE_LEN
#define CONFIG_LIMIT_MIN_APPGROUPNUMBER                             1
#define CONFIG_LIMIT_MAX_APPGROUPNUMBER                            16
#define CONFIG_LIMIT_MIN_BDI_VALUE                            0200001
#define CONFIG_LIMIT_MAX_BDI_VALUE                            0207777

#define CONFIG_DEFAULT_SERVERNAME                        "JDBCSERVER"
#define CONFIG_DEFAULT_MAXSERVERWORKERS                            10
#define CONFIG_DEFAULT_COMAPI_QUEUESIZE                            10
#define CONFIG_DEFAULT_APPGROUPNAME                          "UDSSRC"
#define CONFIG_DEFAULT_NUM_INITIAL_SERVER_SESSION_TRIES             2
#define CONFIG_DEFAULT_WAIT_SERVER_SESSION_RETRY                10000
#define CONFIG_DEFAULT_SERVER_RECEIVE_TIMEOUT                   60000
#define CONFIG_DEFAULT_SERVER_SEND_TIMEOUT                      60000
#define CONFIG_DEFAULT_CLIENT_RECEIVE_TIMEOUT                  900000
#define CONFIG_DEFAULT_SERVER_LOGFILENAME                  "JDBC$LOG"
#define CONFIG_DEFAULT_SERVER_TRACEFILENAME              "JDBC$TRACE"
#define CONFIG_DEFAULT_FETCHBLOCK_SIZE                          16384
#define CONFIG_DEFAULT_SW_CLIENT_TRACEFILENAME           "Client$TRACE" /* define this xxxx */
#define CONFIG_DEFAULT_CLIENT_DEFAULT_TRACEFILE_QUAL_P1  "JDBC-"
#define CONFIG_DEFAULT_SERVER_LOCALE                     "en_US"
#define CONFIG_DEFAULT_KEYIN_ID                          "RUNID"
#define CONFIG_DEFAULT_USER_ACCESS_CONTROL_OFF       USER_ACCESS_CONTROL_OFF
#define CONFIG_DEFAULT_COMAPI_MODE           "A"
#define CONFIG_DEFAULT_LOG_CONSOLE_OUTPUT_ON         LOG_CONSOLE_OUTPUT_ON   /* 1 */
#define CONFIG_DEFAULT_LOG_CONSOLE_OUTPUT_ON_STRING  CONFIG_JDBC_LOG_CONSOLE_OUTPUT_ON   /* "ON" */
#define CONFIG_DEFAULT_RSA_BDI                                0200000
#define CONFIG_DEFAULT_RSA_BDI_STR                            "0200000"
#define CONFIG_DEFAULT_UDS_ICR_BDI                            0200000
#define CONFIG_DEFAULT_UDS_ICR_BDI_STR                        "0200000"
#define CONFIG_DEFAULT_XA_THREAD_REUSE_LIMIT                      100  /* The default of 100 was chosen by the RDMS group. */

#define CONFIG_SERVERNAME                       1
#define CONFIG_MAX_ACTIVITIES                   2
#define CONFIG_MAX_QUEUED_COMAPI                3
#define CONFIG_APPGROUPNAME                     4
#define CONFIG_HOSTPORTNUMBER                   6
#define CONFIG_SERVER_RECEIVE_TIMEOUT          11
#define CONFIG_SERVER_SEND_TIMEOUT             12
#define CONFIG_CLIENT_RECEIVE_TIMEOUT          13
#define CONFIG_SERVERLOGFILENAME               14
#define CONFIG_SERVERTRACEFILENAME             15
#define CONFIG_SW_CLIENT_RECEIVE_TIMEOUT       16
#define CONFIG_FETCHBLOCK_SIZE                 17
#define CONFIG_SW_FETCHBLOCK_SIZE              18
#define CONFIG_SW_CLIENT_TRACEFILENAME         19
#define CONFIG_CLIENT_DEFAULT_TRACEFILE_QUAL   20
#define CONFIG_SERVER_LOCALE                   21
#define CONFIG_USER_ACCESS_CONTROL             22
#define CONFIG_XA_THREAD_REUSE_LIMIT           23

/* enums and typedefs */
enum serverCalledViaStates {  CALLED_VIA_COMAPI=0              /* Client accessed JDBC Server via COMAPI (network socket) */
                             ,CALLED_VIA_JNI                   /* Client accessed JDBC Server via JNI form 2200 Java JVM  */
                             ,CALLED_VIA_CORBA                 /* Client accessed XA JDBC Server via CORBA and RMI-IIOP   */
    };
enum serverStates     {      SERVER_CLOSED=0                   /* Server is not up and running.                           */
                            ,SERVER_INITIALIZING_CONFIGURATION /* Server is in process of initializing                    */
                                                               /* its configuration and basic data structures.            */
                            ,SERVER_INITIALIZING_SERVERWORKERS /* Server is in process of initializing                    */
                                                               /* the Server Workers the first time.                      */
                            ,SERVER_CHECKING_AUTHENTICATION_PRIVILEGE /* Server calls SEC$IDENT$                          */
                            ,SERVER_INITIALIZING_ICL           /* Server is in process of initializing                    */
                                                               /* the Initial Connection Listener with COMAPI             */
                            ,SERVER_RUNNING                    /* Server is active, waiting for/or has JDBC clients.      */
                            ,SERVER_DOWNED_GRACEFULLY          /* Server has been shut down gracefully.                   */
                            ,SERVER_DOWNED_IMMEDIATELY         /* Server has been shut down immediately.                  */
                            ,SERVER_DOWNED_DUE_TO_ERROR        /* Server has been shut down due to an error.              */
    };
enum serverShutdownStates {  SERVER_ACTIVE=0                   /* Server is assumed up and running (its active).          */
                            ,SERVER_SHUTDOWN_GRACEFULLY        /* Server is told/in process of shutting down gracefully.  */
                            ,SERVER_SHUTDOWN_IMMEDIATELY       /* Server is told/in process of shutting down immediately. */
    };
enum serverShutdownPoints {  SERVER_ICL_CLOSED=0               /* Server is shutdown by return from ICL activity.         */
    };
enum serverWorkerStates {    WORKER_CLOSED=0                   /* Worker is shutdown; just before activity terminates.    */
                            ,WORKER_INITIALIZING               /* Worker activity is in process of initializing itself.   */
                            ,FREE_NO_CLIENT_ASSIGNED           /* Worker activity is free; has no assigned JDBC Client.   */
                            ,CLIENT_ASSIGNED                   /* Worker activity has an assigned JDBC Client.            */
                            ,WORKER_DOWNED_GRACEFULLY          /* Worker has been shut down gracefully.                   */
                            ,WORKER_DOWNED_IMMEDIATELY         /* Worker has been shut down immediately.                  */
                            ,WORKER_DOWNED_DUE_TO_ERROR        /* Worker was shut down due to an error.                   */
    };
enum serverWorkerShutdownStates { WORKER_ACTIVE=0              /* Worker is assumed up and running (its active).          */
                            ,WORKER_SHUTDOWN_GRACEFULLY        /* Worker is told/in process of shutting down gracefully.  */
                            ,WORKER_SHUTDOWN_IMMEDIATELY       /* Worker is told/in process of shutting down immediately. */
    };
enum serverWorkerShutdownPoints {NEW_WORKER_JUST_INITIALIZED=0 /* New Worker has initialized, not yet processing Client.  */
                                ,CANT_RECEIVE_REQUEST_PACKET   /* The TCP_RECEIVE for request packet failed or timed out. */
                                ,DETECTED_BAD_REQUEST_PACKET   /* Detected a bad request packet, shutting down SW.        */
                                ,CANT_SEND_RESPONSE_PACKET     /* The TCP_SENDQB and QB release failed or timedout.       */
                                ,AFTER_SENDING_RESPONSE_PACKET /* Done sending response packet,likely immediate shutdown. */
                                ,AFTER_RECEIVING_DATA          /* Just after receiving part of the request packet.        */
                                ,DONE_PROCESSING_CLIENT        /* Done processing the Client, not on free/assigned chain. */
                                ,WOKEN_UP_TO_JUST_SHUTDOWN     /* Server Worker woken up so it can shut itself down.      */
                                ,AFTER_GIVEN_NEXT_CLIENT       /* SW woken and given a new Client to process.             */
                                ,COULDNT_INHERIT_CLIENT_SOCKET /* SW could not inherit the client's socket. */
                                ,END_OF_MORE_CLIENTS_LOOP      /* Just after the process more clients While loop.         */
                                ,SW_CONTINGENCY_CASE           /* Shut down a worker because of a contingency */
    };
enum ICLStates {             ICL_CLOSED=0                      /* Initial Connection Listener is shutdown, not running.   */
                            ,ICL_INITIALIZING                  /* ICL is initializing (e.g. doing COMAPI registration).   */
                            ,ICL_RUNNING                       /* Initial Connection Listener is running normally.        */
                            ,ICL_DOWNED_GRACEFULLY             /* ICL is told/in process of shutting down gracefully.     */
                            ,ICL_DOWNED_IMMEDIATELY            /* ICL is told/in process of shutting down immediately.    */
                            ,ICL_DOWNED_DUE_TO_ERROR           /* ICL was shut down due to an error.                      */
                            ,ICL_CONNECTING                    /* ICL attempting to connect with COMAPI                   */
    };
enum ICLShutdownStates {     ICL_ACTIVE=0                      /* ICL is assumed up and running (its active).             */
                            ,ICL_SHUTDOWN_GRACEFULLY           /* ICL is told/in process of shutting down gracefully.     */
                            ,ICL_SHUTDOWN_IMMEDIATELY          /* ICL is told/in process of shutting down immediately.    */
    };
enum ICLShutdownPoints    { BEFORE_INIT_SERVER_SOCKET=0        /* Server/ICL initializing, haven't gotten the server sockets yet. */
                           ,AFTER_COMAPI_ACCEPT_TIMEOUT        /* We just returned from TCP_ACCEPT, no new JDBC client.   */
                           ,FOUND_NO_FREE_SW                   /* We have JDBC Client, but found no free SW to assign.    */
                           ,AFTER_ASSIGNING_SW                 /* We just assigned a SW to the JDBC Client.               */
                           ,BEFORE_AFTER_COMAPI_SELECT         /* Just before we do the TCP_SELECT for a new JDBC client. */
                           ,AFTER_COMAPI_ACCEPT_LOOP           /* We are done looping waiting for JDBC Clients.           */
                           ,INSIDE_INIT_SERVER_SOCKET          /* We were trying to set up the ICL's server sockets.      */
    };
enum consoleHandlerStates { CH_INACTIVE=0                      /* Initial Console Handler state (before CH listen mode */
                           ,CH_INITIALIZING                    /* CH is in init. mode (before going into wait mode */
                           ,CH_RUNNING                         /* CH state while we are in the CH listen mode */
                           ,CH_DOWNED                          /* CH has been marked for shut down */
    };
enum consoleHandlerShutdownStates { CH_ACTIVE=0                /* Console Handler is assumed to be up and running */
                            ,CH_SHUTDOWN                       /* CH is in the process of shutting down */
    };
enum UASFM_States         { UASFM_CLOSED=0                      /* User Access Security File MSCON activity is not active. */
                           ,UASFM_RUNNING                       /* User Access Security File MSCON activity is running (active). */
    };
enum UASFM_ShutdownStates { UASFM_ACTIVE=0                      /* UASFM is assumed to be up and running */
                           ,UASFM_SHUTDOWN                      /* UASFM is shut down, or is in the process of shutting down */
    };
enum ownerActivityType     { SERVER_WORKER=0                   /* The WDE entry belongs to a Server Worker activity */
                            ,SERVER_ICL                        /* The WDE entry belongs to the ICL activity */
                            ,SERVER_CH                         /* The WDE entry belongs to the JDBCServer-CH activity */
                            ,SERVER_UASFM                      /* The WDE entry belongs to the User access security MSCON activity */
    };

/* -- Client Trace File Table Entry --
 * Defines one entry into this table.  Each entry defines a client trace file that can be shared.
 * Each client (a.k.a., WDE) can have a trace file open.  If the file is the same name, it can only be
 * opened once and each activity can use the same file handle to write to the file under test&set control.
 * There must be a common place to manage these shared file handles - i.e., this table.
 * The usage counter determines how many activities are using the same file.
 */
typedef struct {
    char fileName[MAX_FILE_NAME_LEN+1];                        /* fully qualified file name including the cycle number) */
    FILE *file;                                                /* pointer to file handle */
    int fileNbr;                                               /* file number used for the USE name */
    int usageCount;                                            /* number of activities using this file */
} clientTraceTableEntry;

/* definition of the Server Workers data structure workerDescriptionEntry */
/* Add display in DisplayCmds.c for additions to this data structure. */
typedef struct workerDescriptionEntry {
    enum ownerActivityType ownerActivity;                      /* The type of Server activity that owns this WDE Entry.   */
    int serverWorkerActivityID;                                /* Exec 2200 36-bit activity ID for this Server Worker.    */
    long long serverWorkerUniqueActivityID;                    /* Exec 2200 72-bit unique activity ID for this Server Worker. */
    tsq_cell serverWorkerTS;                                   /* T/S cell used to lock this Server Worker's WDE entry.   */
    struct workerDescriptionEntry *free_wdePtr_next;           /* Pointer to next WDE on the free chain. NULL marks end.  */
    struct workerDescriptionEntry *assigned_wdePtr_back;       /* Points to previous WDE on assigned chain. NULL marks top.*/
    struct workerDescriptionEntry *assigned_wdePtr_next;       /* Points to next WDE on the assigned chain. NULL marks end.*/
    struct workerDescriptionEntry *realloc_wdePtr_next;        /* Points to next WDE on the WDE reallocation chain. NULL marks end.*/
    enum serverWorkerStates serverWorkerState;                 /* Current state of this Server Worker (what we are doing).*/
    enum serverWorkerShutdownStates serverWorkerShutdownState; /* Indicator used to notify Server Worker to shut down.    */

    /* For ICL activities the ICL_num and comapi_bdi are associated
       to the ICL's server sockets (located in the sgd). For a Server Worker
       the ICL_num and comapi_bdi are associated to the JDBC client
       socket in this WDE entry. For all other activities, these entries
       are not needed and just have the values from the first ICL activity(0).
     */
    int ICL_num;                                               /* The ICL num associated to the COMAPI system that socket(s) are from. */
    int comapi_bdi;                                            /* The bdi for the COMAPI system associated to the socket(s). */
    int comapi_IPv6;                                           /* Indicates whether COMAPI supports IPv6 (0 - no, 1 - yes). */
    int in_COMAPI;                                             /* Indicates whether we are in a COMAPI call (0 - no, 1 - yes)
                                                                  Also, in_COMAPI indicates whether we are in OTM (0 - no, 1 - yes). <- This
                                                                  will be changed later so OTM has its own flag.  This overload works now
                                                                  since we can either be using COMAPI (JDBC Server) or OTM (XA JDBC Server)
                                                                  and not both. */
    int in_RDMS;                                               /* Indicates whether we are in a UDS/RDMS/RSA call (0 - no, 1 - yes) */
    int client_socket;                                         /* COMAPI socket for access to JDBC Client (0=no socket).  */
    int network_interface;                                     /* The network interface (CPCOMM (2) or CMS (1)) of the client socket */
    v4v6addr_type client_IP_addr;                              /* The client's IP address as returned by COMAPI.          */
    int client_receive_timeout;                                /* The client's socket receive timeout value in milliseconds. */
    int client_keepAlive;                                      /* The client's keep alive value they will use with their client socket. */
    int fetch_block_size;                                      /* The client's fetch block size value.                    */
    char client_Userid[CHARS_IN_USER_ID+1];                     /* the userid of the JDBC client.                          */
    int assigned_client_number;                                /* The sgd.cumulative_total_AssignedServerWorkers value for this SWorker's wde.*/
                                                               /* for this Server Worker's client.                        */
    char threadName[RDMS_THREADNAME_LEN+1];                    /* The thread name passed to RDMS at Begin Thread time.    */
    char clientDriverLevel[JDBC_DRIVER_LEVEL_SIZE];            /* JDBC driver levels: 4 bytes:                            */
                                                               /*   major, minor, stability, release                      */
    char clientDriverBuildLevel[JDBC_DRIVER_BUILD_LEVEL_MAX_SIZE+1]; /* JDBC driver build id level string.                      */
    enum serverCalledViaStates serverCalledVia;                /* How Client accessed JDBC Server, either JNI or COMAPI.  */
    int workingOnaClient;                                      /* Are we done with Client(BOOL_FALSE) or not (BOOL_TRUE). */
    int openRdmsThread;                                        /* Do we have an open RDMS Thread (BOOL_TRUE) or not (BOOL_TRUE). */
    int txn_flag;                                              /* XA Server usage only - ODTP txn flag (0=no txn, 1=in txn),*/
                                                               /* derived from CORBA request packet. (Unused value is 0)  */
    int odtpTokenValue;                                        /* XA Server usage only - ODTP token value from Request packet */
                                                               /* header. A formatted int value, defined as 4-byte value, */
                                                               /* with each byte's 9th bit set to 0. (Unused value is 0)  */
    int resPktLen;                                             /* XA Server usage only -Length of allocated CORBA response packet in bytes */
    char *standingRequestPacketPtr;                            /* Pointer to standing buffer sized for most request packets*/
    char *requestPacketPtr;                                    /* Pointer to the actual memory used for request packet    */
    char *releasedResponsePacketPtr;                           /* Pointer to a response packet that will be reallocated.  */
    int taskCode;                                              /* The last/current Request task code processed by Server. */
    int debug;                                                 /* Indicates whether debug/tracing active (0=off, non-0=on)*/
    int debugFlags;                                            /* Provides the debugging flags used during tracing. At    */
                                                               /* time only the display detail indicators are present.    */
    int totalAllocSpace;                                       /* current total number of words alloc'ed by the activity  */
    int clientTraceFileNbr;                                    /* Integer value associated with the client trace file     */
    char clientTraceFileName[MAX_FILE_NAME_LEN+1];             /* Name of the client trace file to send trace output.     */
    FILE *clientTraceFile;                                     /* File handle to the client trace file.                   */
    char workerMessageFileName[MAX_FILE_NAME_LEN+1];           /* Name of the client's locale message file.               */
    FILE *workerMessageFile;                                   /* File handle to the client's locale message file.        */
    char clientLocale[MAX_LOCALE_LEN+1];                       /* client message file locale name */
    FILE *serverMessageFile;                                   /* File handle to the server's locale message file.        */
    int consoleSetClientTraceFile;                             /* TRUE if a console SET cmd sets the client trace file.   */
    long long firstRequestTimeStamp;                           /* DWTIME$ timestamp when first request packet arrived for this client */
    long long lastRequestTimeStamp;                            /* DWTIME$ timestamp when the last (latest) request packet arrived for this client */
    void *c_int_pkt_ptr;                                       /* pointer to the c_intDataStructure data structure for this client */

#if MONITOR_TMALLOC_USAGE /* Tmalloc/Tcalloc/Tfree tracking */
    tmalloc_table_entry  tmalloc_table[TMALLOC_TRACEID_MAX + 1]; /* The Tmalloc/Tcalloc/Tfree tracking table (room for TMALLOC_TRACEID_MAX + 1 entries. */
#endif                       /* Tmalloc/Tcalloc/Tfree tracking */
    }workerDescriptionEntry; /* WDE */


/* Definition of the global data structure serverGlobalData */
/* Add display in DisplayCmds.c for additions to this data structure. */
typedef struct {
    char    server_LevelID[JDBC_SERVER_LEVEL_SIZE];            /* The JDBC Server level ID (Major version, Minor version, Stability, Release). */
    char    serverName[MAX_SERVERNAME_LEN+1];                  /* The name of this JDBC Server, null terminated.          */
    char    serverExecutableName[MAX_FILE_NAME_LEN+1];         /* The actual executable name of this (XA) JDBC Server (in q*f.e/v format) null terminated. */
    int     xaServerNumber;                                    /* The XA Server's executables server number, 0 if not present or not an XA Server. */
                                                               /* Provides XA Server's executables server number, 0 if not present or not an XA Server. */
    char    rdmsLevelID[MAX_RDMS_LEVEL_LEN+1];                 /* The RDMS level ID for the RDMS used by this JDBC Server */
    int     rdmsBaseLevel;                                     /* The RDMS base level. */
    int     rdmsFeatureLevel;                                  /* The RDMS feature level. */
    int     er_level_value;                                    /* The ER Level$ A0 value for Server's switching priority. */
    char    server_priority[MAX_SERVER_PRIORITY_LEN+1];        /* The switching priority of the Server.                   */
    long long  serverStartTimeValue;                           /* The DWTIME$ value for the Server Start up.              */
    int     maxServerWorkers;                                  /* Limit on number of Server Workers, free or assigned.    */
    int     maxCOMAPIQueueSize;                                /* Limit of JDBC Clients allowed to wait on COMAPI queue.  */
    tsq_cell WdeReAllocTS;                                     /* T/S cell used to lock the WDE re-allocation code.        */
    workerDescriptionEntry *realloc_wdePtr;                    /* Pointer to first WDE entry on reuasble chain.           */
    tsq_cell firstFree_WdeTS;                                  /* T/S cell used to lock the free Server Worker chain.     */
    int     numberOfFreeServerWorkers;                         /* Number of free(unassigned) Server Workers on free chain.*/
    int     numberOfShutdownServerWorkers;                     /* Number of shutdown (exited) Server Workers by this Server.*/
    workerDescriptionEntry *firstFree_wdePtr;                  /* Pointer to first Server Worker WDE in free chain.       */
    tsq_cell firstAssigned_WdeTS;                              /* T/S cell used to lock the assigned Server Worker chain. */
    int     numberOfAssignedServerWorkers;                     /* Number of assigned Server Workers on assigned chain.    */
    int     cumulative_total_AssignedServerWorkers;            /* Within this JDBC Server, the cumulative total number of assigned client SWs. */
    workerDescriptionEntry *firstAssigned_wdePtr;              /* Pointer to first Server Worker WDE in assigned chain.   */
    char    appGroupName[MAX_APPGROUP_NAMELEN+1];              /* Name of the UDS/RDMS Application Group.                 */
    char    appGroupQualifier[MAX_QUALIFIER_LEN+1];            /* Qualifier of the UDS/RDMS Application Group. Only set if needed */
    int     appGroupNumber;                                    /* Application Group number of the named UDS/RDMS Application Group. */
    int     uds_icr_bdi;                                       /* The bdi of the UDS ICR bank for the Application Group   */
    int     localHostPortNumber;                               /* Port number for the JDBC Server's server socket         */
    enum serverStates serverState;                             /* Current state of the JDBC Server (what we are doing).   */
    enum serverShutdownStates serverShutdownState;             /* Indicator used to notify JDBC Server to shut down.      */
    enum consoleHandlerStates consoleHandlerState;             /* Current state of the CH (what is it doing). */
    enum consoleHandlerShutdownStates consoleHandlerShutdownState; /* Indicator used to notify the CH to shut down. */
    int debugXA;                                               /* XA Server only - Internal XA debug flag. Explicitly clear/set in server$corba */
    int config_client_keepAlive;                               /* The Server's config value for a client's keep alive thread. */
    int server_receive_timeout;                                /* The ICL's server socket receive timeout value in milliseconds. */
    int server_send_timeout;                                   /* The ICL's server socket send timeout value in milliseconds. */
    int server_activity_receive_timeout;                       /* The ICL's client socket receive timeout value in milliseconds. */
    int fetch_block_size;                                      /* The configuration's client fetch block size.            */
    tsq_cell serverLogFileTS;                                  /* T/S cell used to synchronize the Server Log file I/O.   */
    tsq_cell msgFileTS;                                        /* T/S cell used to synchronize the server message file I/O. */
    char    serverLogFileName[MAX_FILE_NAME_LEN+1];            /* JDBC Server's Log file name.                            */
    FILE    *serverLogFile;                                    /* JDBC Server's Log file handle.                          */
    tsq_cell serverTraceFileTS;                                /* T/S cell used to synchronize the Server Trace file I/O. */
    char    serverTraceFileName[MAX_FILE_NAME_LEN+1];          /* JDBC Server's Trace file name.                          */
    FILE    *serverTraceFile;                                  /* JDBC Server's Trace file handle.                        */
    char serverMessageFileName[MAX_FILE_NAME_LEN+1];           /* Name of the JDBC Server's current locale message file.  */
    FILE *serverMessageFile;                                   /* File handle to JDBC Server's current locale message file.*/
    char serverLocale[MAX_LOCALE_LEN+1];                       /* server message file locale name */
    int     debug;                                             /* indicates whether we are debugging the JDBC Server.     */
    int clientDebugInternal;                                   /* client flag for debug internal */
    int clientDebugDetail;                                     /* client flag for debug detail */
                                                               /* IMPORTANT: The three fields (usageTypeOfServer_serverStartDate,*/
                                                               /* serverStartMilliseconds, generatedRunID) comprise the        */
                                                               /* Server's instance identification. Notice, order of these    */
                                                               /* fields is FIXED, it is 16 bytes in size, and the last two   */
                                                               /* bytes must be trailing nulls (0) at the end of the grunid.  */
                                                               /* Server.c method addServerInstanceInfoArg() adds             */
                                                               /* the first 14 bytes (i.e., minus extra two nulls) to the     */
                                                               /* begin thread response packet.                               */
    char    usageTypeOfServer_serverStartDate[4];              /* Byte 0 has usage type this Server is providing (see defines */
                                                               /* SERVER_USAGETYPE_XXXXX). Bytes 1-3 have Server's start date */
                                                               /* as three byte values right-justified. Byte 1 is the year    */
                                                               /* modulo 2000, byte 2 is 2-digit Month, byte 3 is 2-digit Day.*/
    char    serverStartMilliseconds[4];                        /* Server's start time (in day) as a Java 32-bit millisecond   */
                                                               /* value placed into the chars by putIntIntoBytes() (i.e., as  */
                                                               /* 8-bit bytes in the word's 9-bit byte spaces.                */
    char    generatedRunID[RUNID_CHAR_SIZE+2];                 /* NUL-terminated string with the run's generated run ID. The  */
                                                               /* field is rounded up to word boundary (8 bytes total).       */
    char    originalRunID[RUNID_CHAR_SIZE+1];                  /* NUL-terminated string with the run's original run ID   */
    char    serverUserID[MAX_USERID_LEN+1];                    /* NUL-terminated string with the user-id used to start the Server */
    int  rdms_threadname_prefix_len;                           /* Length (either 1 or 2) of the rdms_threadname_prefix string. */
    char rdms_threadname_prefix[CONFIG_DEFAULT_RDMS_THREADNAME_PREFIX_LEN+1]; /* Two prefix characters for RDMS thread name. */
    void * keyinReturnPktAddr;                                 /* ER KEYIN$ return packet address */
    tsq_cell clientTraceFileTS;                                /* T/S cell used to synchronize the client Trace file I/O. */
    int     clientTraceFileCounter;                            /* Global counter of the number of client trace files */
    char defaultClientTraceFileQualifier[MAX_QUALIFIER_LEN+1]; /* Default qualifier for client trace files.               */
    char textMessage[MAX_COM_OUTPUT_BYTE_SIZE+150];            /* Buffer used for ER COM$ messages.  Allow overflow. */
    int tempMaxNumWde;                                         /* holds max number of wde's we can create */
    int numWdeAllocatedSoFar;                                  /* holds the number of wde's we have allocated so far */
    int rsaBdi;                                                /* Save the BDI for RSA, so we can restore it for each worker */
    int postedServerReceiveTimeout;                            /* Posted value (in milliseconds) from the CH cmd. SET SERVER RECEIVE TIMEOUT */
    int postedServerSendTimeout;                               /* Posted value (in milliseconds) from the CH cmd. SET SERVER SEND TIMEOUT */
    int COMAPIDebug;                                           /* Value for COMAPI debug (64 for partial debug) */
    int postedCOMAPIDebug;                                     /* Posted value for COMAPI debug (set to 64 if from SET COMAPI DEBUG=ON) */
    int logConsoleOutput;                                      /* If 1, console output is also sent to the log file */
    char configKeyinID[KEYIN_CHAR_SIZE+1];                     /* The configuration keyin_id parameter */
    char keyinName[KEYIN_CHAR_SIZE+1];                         /* The keyin name used for console commands for the server */
    int clientCount;                                           /* Total number of clients attached to the server since it started */
                                                               /*   or since the count was cleared by CLEAR console command */
    int requestCount;                                          /* Total number of request packets through the server since it started */
                                                               /*   or since the count was cleared by CLEAR console command */
    long long lastRequestTimeStamp;                            /* DWTIME$ timestamp of the last request packet.  Cleared by CLEAR console command. */
    int lastRequestTaskCode;                                   /* Task code of the last request packet.  Cleared by CLEAR console command. */
    int  option_bits;                                          /* Option bits corresponding to  option letters on @jdbcserver image */
    char configFileName[MAX_FILE_NAME_LEN+1];                  /* XA or JDBC Server configuration file name. */
    int clientTraceFileTracks;                                 /* Size of Client Trace file in tracks (TRK) */
    int clientTraceFileCycles;                                 /* Number of Client trace file cycles to maintain */
    clientTraceTableEntry clientTraceTable[CONFIG_LIMIT_MAXSERVERWORKERS]; /* Table of shared client trace files */


    /*
     * Now comes the COMAPI usage information. There is one array
     * entry reserved for each COMAPI system indicated by the
     * configuration parameter comapi_modes. Note that there is a
     * limit on the maximum number of COMAPI modes that can be utilized.
     */
    char comapi_modes[MAX_COMAPI_MODES+1];                     /* Contains the configuration parameter's string value */
    int num_COMAPI_modes;                                      /* The actual number of COMAPI systems being used by this JDBC Server. */
    int ICL_handling_SW_CH_shutdown;                           /* The ICL number that is handling ServerWorker and CH shutdown
                                                                  notification ( set to NO_ICL_IS_CURRENTLY_HANDLING_SW_CH_SHUTDOWN
                                                                  if no ICL has accepted the task yet).*/
    int  comapi_reconnect_msg_skip_log_count[MAX_COMAPI_MODES]; /* Current number of consecutive SEACCES (10001) messages (note: not all are logged). */
    int  comapi_reconnect_wait_time[MAX_COMAPI_MODES];          /* COMAPI reconnect wait time. */
    int  forced_COMAPI_error_status[MAX_COMAPI_MODES];         /* for debug use only - forces a specific COMAPI error status */
    enum ICLStates ICLState[MAX_COMAPI_MODES];                 /* Current states of each of the ICL Activities (what is it doing).   */
    enum ICLShutdownStates ICLShutdownState[MAX_COMAPI_MODES]; /* Indicator used to notify each ICL activity to shut down.        */
    char ICLcomapi_mode[MAX_COMAPI_MODES][FOUR_CHARS];         /* Character representation of the COMAPI mode for this ICL activity. */
    int ICLcomapi_state[MAX_COMAPI_MODES];                     /* Current state of COMAPI for this ICL. */
    int ICLcomapi_bdi[MAX_COMAPI_MODES];                       /* The bdi's for each COMAPI's being used. */
    int ICLcomapi_IPv6[MAX_COMAPI_MODES];                      /* Indicator for each COMAPI's support of IPv6 (0 - no, 1 - yes). */
    int ICLAssignedServerWorkers[MAX_COMAPI_MODES];            /* Number of assigned server workers for this ICL */
    int num_server_sockets;                                    /* The number of actual server sockets needed, based on the configurations listen_hosts information. */
    int server_socket[MAX_COMAPI_MODES][MAX_SERVER_SOCKETS];   /* The server sockets (CPComm & CMS) for each ICL activity, where 0= no server socket. */
    v4v6addr_type listen_ip_addresses[MAX_COMAPI_MODES][MAX_SERVER_SOCKETS];/* The IP addresses for the specific server socket, network, and COMAPI mode. */
    v4v6addr_type explicit_ip_address[MAX_COMAPI_MODES][MAX_SERVER_SOCKETS];/* If listen host name string is an explicit IP address then it contains that IP address, else -1 indicator */
    char listen_host_names[MAX_SERVER_SOCKETS][((HOST_NAME_MAX_LEN+1 +3)/4)*4]; /* The actual listen host name string (name or IP address) from the configuration */

    char config_host_names[IMAGE_MAX_CHARS];                   /* hold the actual listen_hosts configuration parameter value. */
    char fullLogFileName[MAX_FILE_NAME_LEN+1];                 /* Contains either the default server_name in JDMS$CONFIG if log file not initialized, or
                                                                  contains the fully qualified log file name with cycle number. */


    /* Now come the JDBC user access security information */
    int user_access_control;                                   /* Indicates if/how userid access security is done */
    enum UASFM_States UASFM_State;                             /* Current state of the UASFM Activity (what is it doing).   */
    enum UASFM_ShutdownStates UASFM_ShutdownState;             /* Indicator used to notify UASFM to shut down.              */
    int SUVAL_bad_error_statuses;                              /* Saved bad SUVAL$ error status (H1) and function return status (H2)*/
    char user_access_file_name[USER_ACCESS_FILENAME_LEN+1];    /* The JDBC user access security filename for this appgroup */
    int which_MSCON_li_pkt;                                    /* Indicates which MSCON$ lead item packet is current one. */
    int MSCON_li_status[2];                                    /* Error status value returned for each MSCON$ lead item packets */
    mfd_lead_item_struct MSCON_li_pkt[2];                      /* Declare two MSCON$ lead item packets */

    /* Define the Feature Flags that determine which features are available.  These flags are used to support multiple levels of RDMS   */
    /* These flags are generally considered boolean and set to 1 (true) or 0 (false)                                                    */
    /* A set of defines are created in server.h to reference these flags.                                                */
    int FFlag_SupportsSQLSection;                               /* Indicates if the enhanced RDMS SQL sections are available in RDMS. */
    int FFlag_SupportsFetchToBuffer;                            /* Indicates if the "fetch_to_buffer()" API is available in RDMS. */
#ifdef XABUILD  /* XA JDBC Server */
    int totalStatefulThreads;                                   /* The total number of XA Server's non-XA (stateful) client database threads opened. */
    int totalXAthreads;                                         /* The total number of XA Server's XA transactional client database threads opened.  */
    int totalXAReuses;                                          /* The total number of XA Server's XA database thread reuses.                        */
    int XA_thread_reuse_limit;                                  /* The XA Server's XA database thread reuse limit configuration parameter value.     */
    int trx_mode;                                               /* The XA Server obtained tx_info() returned value (used by XA reuse checking only).      */
    int transaction_state;                                      /* The XA Server obtained transaction_state value from tx_info() (used by XA reuse checking only). */
    int XA_thread_reuses_so_far;                                /* The number of XA Server's XA database thread reuses in a row so far.              */
    int saved_beginThreadTaskCode;                              /* The saved BT task code for the current type of XA Server's opened database thread (non-XA, XA). */
    int saved_XAaccessType;                                     /* The saved access type for XA Server's database thread (for XA reuse use only)   */
    int saved_XArecovery;                                       /* The saved recovery mode for XA Server's database thread (for XA reuse use only) */
    int saved_rsaDump;                                          /* The saved rdmstrace rsadump flag for XA Server's database thread (for XA reuse use only) */
    int saved_debugSQLE;                                        /* The saved debugSQLE flag for XA Server's database thread (for XA reuse use only) */
    int saved_processedUseCommand;                              /* The saved processed USE command indicator. Its set to 1 if a USE command is processed. */
    int saved_processedSetCommand;                              /* The saved processed SET command indicator. Its set to 1 if a SET command is processed. */
    int saved_varcharInt;                                       /* The saved varchar setting for XA Server's database thread (for XA reuse use only). Set to 1 if property varchar=varchar is set. */
    int saved_emptyStringProp;                                  /* The saved emptyStringProp setting for XA Server's database thread (for XA reuse use only). */
    int saved_skipGeneratedType;                                /* The saved current skipGeneratedType setting for XA Server's database thread.                */
    char saved_XAappGroupName[MAX_APPGROUP_NAMELEN + 1];        /* The saved appgroup name for XA Server's database thread (for XA reuse use only) */
    char saved_client_Userid[CHARS_IN_USER_ID+1];               /* The saved userid of the XA Server's JDBC client.                                */
    char saved_schemaName[2*(MAX_SCHEMA_NAME_LEN+1)];           /* The saved connection property schema name (Unicode) of the XA Server's JDBC client.       */
    char saved_versionName[MAX_SCHEMA_NAME_LEN+1];              /* The saved connection property version name of the XA Server's JDBC client.      */
    char saved_charSet[MAX_CHARSET_LEN+1];                      /* The saved charSet setting for XA Server's database thread (for XA reuse use only). An empty string if no charSet was specified. */
    char saved_storageAreaName[MAX_STORAGEAREA_NAME_LEN+1];     /* The saved storageArea setting for XA Server's database thread (for XA reuse use only). An empty string if no storageArea was specified. */
#endif          /* XA JDBC Server */

    /* The WDE array that follows MUST BE LAST in the SGD.
       Reason: It is huge, but the Exec will only allocate memory
       in 4K chunks when it is referenced.
       In general, we won't have many WDE array elements referenced,
       so the amount of allocated memory will be small.
       */
    workerDescriptionEntry wdeArea[TEMPNUMWDE];                /* Allocate space for TEMPNUMWDE workers */

    }serverGlobalData; /* SGD */


void RSA$SETBDI2(int rsaBdi);

void getServerOptionLetters(char * optionLetters);

void ER_Level();

int verifyRdmsEnv();

#ifdef XABUILD  /* XA JDBC Server */
void initialize_XAJDBC_Server(int * errMsg);

void XAServerConsoleHandler();

void getInvocationImageParameters();

#endif          /* XA JDBC Server */

#endif /* _JDBCSERVER_H_ */
