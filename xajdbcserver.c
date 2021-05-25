/* Standard C header files and OS2200 System Library files */
#include <CORBA.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <task.h>
#include <convbyte.h>

/* JDBC Project Files */
#include "XASrvrWorker.h"
#include "JDBCServer.h"


extern serverGlobalData sgd;               /* The Server Global Data (SGD),*/
                                           /* visible to all Server activities. */

/* All occurrences of XASendAndReceive for the 16 XA servers
   call the common routine XAServerWorker.
   The arguments are simply passed through from XASendAndReceive
   to XAServerWorker.

   XASendAndReceive is called via RMI-IIOP from the Java client
   via OTM 2200 through one of the 16 configured XA Servers
   (running under OTM control).
   The XA Server number is not necessarily the same as the app group number.
   The Java client in this case is the RDMS-JDBC Java code
   running under control of a J2EE app server.
   */

/* The following is a macro XASEND used to define function XASendAndReceive
   the 16 XA Servers.
   The macro argument is the XA Server number (1-16).
   The XA Server number is not necessarily the same as the
   app group number. */
#define XASEND(XAServerNum)                                                \
                                                                           \
com_unisys_os2200_rdms_jdbc_byteArray *                                    \
  com_unisys_os2200_rdms_jdbc_XAJDBCServer ##XAServerNum ##_XASendAndReceive (   \
        CORBA_Object _obj,                                                 \
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,                    \
        CORBA_Environment *_env,                                           \
        int txn_flag) {                                                    \
                                                                           \
    com_unisys_os2200_rdms_jdbc_byteArray * ret;                           \
                                                                           \
    /* Temporary debug code */                                             \
    if (sgd.debugXA) {                                                     \
        printf("Entering XASendAndReceive() for XAJDBCServer%d\n",         \
            XAServerNum);                                                  \
    }                                                                      \
                                                                           \
    ret = XAServerWorker(_obj, reqPkt, _env, txn_flag);                    \
                                                                           \
    /* Temporary debug code */                                             \
    if (sgd.debugXA) {                                                     \
        printf("Exiting XASendAndReceive() for XAJDBCServer%d\n",          \
            XAServerNum);                                                  \
    }                                                                      \
                                                                           \
    return ret;                                                            \
                                                                           \
}

/* XASendAndReceive functions for the 16 XA Servers. */

XASEND(1)
XASEND(2)
XASEND(3)
XASEND(4)
XASEND(5)
XASEND(6)
XASEND(7)
XASEND(8)
XASEND(9)
XASEND(10)
XASEND(11)
XASEND(12)
XASEND(13)
XASEND(14)
XASEND(15)
XASEND(16)
