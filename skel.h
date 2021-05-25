#ifndef __xajdbcserver_SKEL_H__
#define __xajdbcserver_SKEL_H__

/* Standard C header files and OS2200 System Library files */
#include <stdlib.h>

/* JDBC Project Files */
#include "xajdbcserver.h"

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer1 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer1;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer1_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer1 com_unisys_os2200_rdms_jdbc_XAJDBCServer1_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer1:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer1_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer1_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer2 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer2;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer2_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer2 com_unisys_os2200_rdms_jdbc_XAJDBCServer2_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer2:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer2_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer2_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer3 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer3;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer3_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer3 com_unisys_os2200_rdms_jdbc_XAJDBCServer3_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer3:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer3_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer3_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer4 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer4;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer4_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer4 com_unisys_os2200_rdms_jdbc_XAJDBCServer4_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer4:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer4_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer4_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer5 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer5;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer5_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer5 com_unisys_os2200_rdms_jdbc_XAJDBCServer5_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer5:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer5_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer5_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer6 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer6;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer6_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer6 com_unisys_os2200_rdms_jdbc_XAJDBCServer6_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer6:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer6_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer6_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer7 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer7;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer7_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer7 com_unisys_os2200_rdms_jdbc_XAJDBCServer7_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer7:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer7_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer7_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer8 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer8;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer8_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer8 com_unisys_os2200_rdms_jdbc_XAJDBCServer8_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer8:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer8_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer8_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer9 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer9;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer9_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer9 com_unisys_os2200_rdms_jdbc_XAJDBCServer9_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer9:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer9_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer9_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer10 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer10;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer10_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer10 com_unisys_os2200_rdms_jdbc_XAJDBCServer10_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer10:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer10_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer10_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer11 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer11;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer11_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer11 com_unisys_os2200_rdms_jdbc_XAJDBCServer11_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer11:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer11_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer11_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer12 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer12;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer12_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer12 com_unisys_os2200_rdms_jdbc_XAJDBCServer12_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer12:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer12_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer12_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer13 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer13;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer13_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer13 com_unisys_os2200_rdms_jdbc_XAJDBCServer13_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer13:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer13_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer13_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer14 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer14;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer14_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer14 com_unisys_os2200_rdms_jdbc_XAJDBCServer14_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer14:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer14_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer14_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer15 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer15;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer15_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer15 com_unisys_os2200_rdms_jdbc_XAJDBCServer15_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer15:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer15_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer15_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

typedef struct POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer16 {
    void *_interface;
    int (*invoke)(PortableServer_Servant, char *, Buffer *, Buffer *);
    void *_private;
} POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer16;

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer16_invoke(
    PortableServer_Servant, char *, Buffer *, Buffer *);

static POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer16 com_unisys_os2200_rdms_jdbc_XAJDBCServer16_servant = {
    "IDL:com/unisys/os2200/rdms/jdbc/XAJDBCServer16:1.0",
    POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer16_invoke, NULL };

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer16_XASendAndReceive (
        PortableServer_Servant _servant,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);


#endif
