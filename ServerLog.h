#ifndef _SERVER_LOG_H_
#define _SERVER_LOG_H_
/**
 * File: ServerLog.h.
 *
 * Header file for the code that establishes and maintains the Log File for the JDBC Server.
 *
 * Header file corresponding to code file ServerLog.c.
 *
 */

/* JDBC Project Files */
#include "JDBCServer.h"

/*
 * These are the #defines that control the location and content of Log File entries
 * that are requested using the addLogEntry procedures. These constants are divided
 * into two groups:
 * logindicator values -used as the first parameter for the addLogEntry or addTraceEntry
 *                      procedures. These values can be AND'ed together to form an
 *                      indicator value that indicates one or more target destinations
 *                      for output (e.g. sending the output image to both STDOUT and
 *                      the Server's Log file).
 *
 * indicator masks   -  used internally by the addLogEntry procedures to decode
 *                      the logindicator value. (The logindicator is first XOR'ed against
 *                      a mask of all 1 bits on, then each target is tested for by an AND
 *                      operation ).
 *
 * Notice that multiple indicators can be passed to the addLogEntry procedures
 * by just bitwise and'ing them together and passing the result as the first
 * parameter (e.g. addLogEntry(TO_LOGFILE & TO_STDOUT & WITH_TIMESTAMP,"Test log entry")
 * will send the log entry suffixed with a timestamp to both the LOG file and to STDOUT )
 *
 * Note: when adding a new indicator value you must also add its indicator mask.
 */
#define LOG_TO_SERVER_LOGFILE   0x00FE     /* Log to JDBC SERVER's Log File */
#define LOG_TO_SERVER_STDOUT    0x00FD     /* Log to JDBC SERVER's STDOUT (PRINT$) file*/
#define LOG_TO_SERVER_TRACEFILE 0x00FB     /* Log to JDBC Server's Trace File, if it exists */
#define LOG_TO_CLIENT_TRACEFILE 0x00F7     /* Log to the Server Workers JDBC Client's trace file */
#define WITH_TIMESTAMP          0x00EF     /* Suffix Log entry with ', at <current-timestamp>' */
#define WITH_SERVER_NAME        0x00DF     /* Prefix Log entry with JDBC Server's name */
#define WITH_CLIENT_NAME        0x00BF     /* Prefix Log entry with JDBC Client name, if known */
#define TO_SERVER_STDOUT_DEBUG  0x007F     /* Log to JDBC Server's stdout file, if DEBUG is on */

/* Logging indicator masks - used only by the addLogEntry procedures internally */
#define REVERSING_MASK           0x00FF   /* Used to turn 0's into 1's for later mask tests */
#define LOG_NOTHING              0x0000   /* Log Nothing to nowhere */
#define TO_SERVER_LOGFILE_MASK   0x0001   /* Log to JDBC SERVER's Log File */
#define TO_SERVER_STDOUT_MASK    0x0002   /* Log to JDBC SERVER's STDOUT (PRINT$) file*/
#define TO_SERVER_TRACEFILE_MASK 0x0004   /* Log to JDBC Server's Trace File, if it exists */
#define TO_CLIENT_TRACEFILE_MASK 0x0008   /* Log to the Server Workers JDBC Client's trace file */
#define WITH_TIMESTAMP_MASK      0x0010   /* Suffix Log entry with ', at <current-timestamp>' */
#define WITH_SERVER_NAME_MASK    0x0020   /* Prefix Log entry with JDBC Server's name */
#define WITH_CLIENT_NAME_MASK    0x0040   /* Prefix Log entry with JDBC Client name, if known */
#define TO_SERVER_STDOUT_DEBUG_MASK 0x0080 /* Log to JDBC Server's stdout file, if DEBUG is on */

/* These Logging indicators are used to as convienence for Server code writers */
/*  Temporarily redefine these tags:
#define SERVER_LOGS     TO_SERVER_LOGFILE & TO_SERVER_STDOUT & WITH_SERVER_NAME & WITH_TIMESTAMP
#define SERVER_STDOUT   TO_SERVER_STDOUT & WITH_SERVER_NAME & WITH_TIMESTAMP
 */
#define TO_SERVER_LOGFILE    LOG_TO_SERVER_LOGFILE & WITH_TIMESTAMP
#define TO_SERVER_STDOUT     LOG_TO_SERVER_STDOUT & WITH_TIMESTAMP
#define TO_SERVER_TRACEFILE  LOG_TO_SERVER_TRACEFILE & WITH_TIMESTAMP
#define TO_CLIENT_TRACEFILE  LOG_TO_CLIENT_TRACEFILE & WITH_TIMESTAMP
#define SERVER_LOGS TO_SERVER_LOGFILE & TO_SERVER_STDOUT & WITH_TIMESTAMP
#define SERVER_LOGS_DEBUG TO_SERVER_LOGFILE & TO_SERVER_STDOUT_DEBUG & WITH_TIMESTAMP
#define SERVER_STDOUT   TO_SERVER_STDOUT & WITH_SERVER_NAME & WITH_TIMESTAMP
#define SERVER_STDOUT_DEBUG TO_SERVER_STDOUT_DEBUG & WITH_SERVER_NAME & WITH_TIMESTAMP
#define SERVER_TRACES TO_SERVER_TRACEFILE & WITH_TIMESTAMP
#define SERVER_LOGTRACES TO_SERVER_LOGFILE & TO_SERVER_TRACEFILE & WITH_TIMESTAMP
#define CLIENT_TRACES TO_CLIENT_TRACEFILE & WITH_SERVER_NAME & WITH_CLIENT_NAME & WITH_TIMESTAMP

#define ITOA_BUFFERSIZE 110 /* itoa() buffer must be 110 (20+90)characters in size to have both digits, IP address, user id, and thread id */

/* assume the log entry prefix is no more than 100 characters in length */
#define MAXLOGPREFIXLENGTH 100

/* Function prototypes */
char * itoa_IP(int value, char * buffer, int base, v4v6addr_type ip_addr, char * user_id, char * thread_id);

/* Function prototypes for use on the Server Log or Trace Files */
int openServerLogFile(char * error_message, int error_len);

int openServerTraceFile(char * error_message, int error_len);

int closeServerFiles(int * logFileStatus, int * traceFileStatus);

int closeServerLogFile(char * error_message, int error_len);

int closeServerTraceFile(char * error_message, int error_len);

void addServerLogEntry(int logIndicator, char *message);

void addServerLogEntry1(int logIndicator, char *log_message, int value1, int value2);

/*
 * Function prototypes for use on the Client Trace File.
 */
int closeClientTraceFile(workerDescriptionEntry *wdePtr
                         , char * error_message, int error_len);

int openClientTraceFile(int eraseFlag, workerDescriptionEntry *wdePtr,
                       char * error_message, int error_len);

void addClientTraceEntry(workerDescriptionEntry *wdePtr,int logIndicator, char *message);

void clientTraceFileInit(workerDescriptionEntry * wdePtr);
#endif /* _SERVER_LOG_H_ */
