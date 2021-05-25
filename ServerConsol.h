#ifndef _SERVER_CONSOL_H_
#define _SERVER_CONSOL_H_
/**
 * File: ServerConsol.h.
 *
 * Header file for the code that establishes and maintains a
 * 2200 console lister for the JDBC Server.
 *
 * Header file corresponding to code file ServerConsol.c.
 *
 */

/* Declarations used in ServerConsol.c */

/* defines */
#define BYTES_PER_WORD 4
#define KEYIN_NAME_FIELD_LEN 8
#define USERID_NAME_FIELD_LEN 12
#define MESSAGE_MAX_SIZE 60
#define KEYIN_PREFIX "JS" /* for JDBC Server */
#define KEYIN_PREFIX_LEN 2
#define TRAILING_STRING_SIZE 100
#define INSERT_NBR_SIZE 20

/* Max configuration parameter name size (with NUL):
     Client <n> trace file
   where <n> is a valid worker ID (12 digits) */
#define MAX_INSERT_CONFIG_PARM_SIZE 40

/* Max configuration parameter string value size
   (e.g., in the command
   SET SERVER LOG FILE=filename,
   the max string size for the filename.
   */
#define MAX_CONFIG_PARM_VALUE_SIZE 80

#define SPACES "        "

/* ER numbers */
#define ER_KEYIN 0244
#define ER_COM 010

/* ER KEYIN$ function codes */
#define KEYIN_REGISTER_FN         1
#define KEYIN_RETRIEVE_FN         2
#define KEYIN_WAIT_FN             3
#define KEYIN_DEREGISTER_FN       4
#define KEYIN_COND_DEREGISTER_FN  5
#define KEYIN_RETRIEVE_USER_FN    6
#define KEYIN_WAIT_USER_FN        7

/* ER KEYIN$ packet */
typedef struct {
    /* Word 0 */
    short status;
    short func;
    /* Word 1 */
    short res1;
    short bufLen;
    /* Word 2 */
    int retPktAddr;
    /* Words 3-4 */
    char keyin[8]; /* not NUL-terminated */
    /* Word 5 */
    short res2;
    short err1;
} keyinPkt;

#define PKT_BYTE_SIZE sizeof(keyinPkt)
#define PKT_WORD_SIZE PKT_BYTE_SIZE/BYTES_PER_WORD

typedef union {
    keyinPkt k1;
    int i1[PKT_WORD_SIZE];
} keyinPkt1;

/* ER KEYIN$ return packet */
typedef struct {
    /* Words 0-1 */
    char keyin[8]; /* not NUL-terminated */
    /* Word 2 */
    int res1        : 12;
    int consoleMode : 6;
      /* charCount is the message size only;
         it does not include the user ID
         if the wait user function is called. */
    short charCount : 18;
    /* Word 3 */
    int keyinRouting;
    /* Word 4 */
      /* Note that this word is treated as
         the second word of a two word routing array,
         even though the ER KEYIN$ doc shows simply a reserved word. */
    int res2;
    /* Words 5-n */
      /* This string starts with a 12 char user ID,
         if the wait user function is called. */
    char msg[MESSAGE_MAX_SIZE];
} keyinRetPkt;

#define RET_PKT_BYTE_SIZE sizeof(keyinRetPkt)
#define RET_PKT_WORD_SIZE RET_PKT_BYTE_SIZE/BYTES_PER_WORD

typedef union {
    keyinRetPkt k1;
    int i1[RET_PKT_WORD_SIZE];
} keyinRetPkt1;

/* ER COM$ packet */
typedef struct {
    /* Word 0 */
    int errorCode           : 6;
    int messageGroupNumber  : 6;
    int controlBits         : 6;
    short inputCount        : 18;
    /* Word 1 */
    int addControlBits      : 6;
    int outputCount         : 12;
    short outputAddress     : 18;
    /* Word 2 */
    int msgSuppressionLevel : 6;
    short expectedInput     : 12;
    short inputAddress      : 18;
    /* Words 3-6 */
    char runID[8];
    int routingInfo1;
    int routingInfo2;
        /* Note: The ER COM$ doc shows only one word of routing info. */
} comDollarPkt;

#define COM_PKT_BYTE_SIZE sizeof(comDollarPkt)
#define COM_PKT_WORD_SIZE COM_PKT_BYTE_SIZE/BYTES_PER_WORD

typedef union {
    comDollarPkt c1;
    int i1[COM_PKT_WORD_SIZE];
} comDollarPkt1;

/* Function prototypes */
int startServerConsoleListener();
int registerKeyin();
int deregisterKeyin();
int replyToKeyin(char * text);
int sendcns(char *omsg, int   olen);

#endif /* _SERVER_CONSOL_H_ */
