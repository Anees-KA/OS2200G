#ifndef __xajdbcserver_H__
#define __xajdbcserver_H__

/* Standard C header files and OS2200 System Library files */
#include "corba.h"

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer1;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer2;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer3;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer4;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer5;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer6;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer7;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer8;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer9;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer10;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer11;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer12;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer13;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer14;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer15;

typedef CORBA_Object com_unisys_os2200_rdms_jdbc_XAJDBCServer16;

#ifndef _CORBA_sequence_octet_defined
#define _CORBA_sequence_octet_defined
typedef struct {
    CORBA_unsigned_long _maximum;
    CORBA_unsigned_long _length;
    CORBA_octet *_buffer;
} CORBA_sequence_octet;

/* Change to IDLC-generated source.
   The following line was moved up, in order to avoid warnings and remarks
   in UC.
   IDLC generated it after the unmarshal_CORBA_sequence_octet prototype. */
#endif /* _CORBA_sequence_octet_defined */

CORBA_sequence_octet *CORBA_sequence_octet__alloc();
CORBA_octet *CORBA_sequence_octet_allocbuf(CORBA_unsigned_long len);

int marshal_CORBA_sequence_octet(CORBA_sequence_octet *, int, Buffer *);
int unmarshal_CORBA_sequence_octet(CORBA_sequence_octet *, int, Buffer *);

typedef CORBA_sequence_octet com_unisys_os2200_rdms_jdbc_byteArray;
#define com_unisys_os2200_rdms_jdbc_byteArray__alloc CORBA_sequence_octet__alloc
#define marshal_com_unisys_os2200_rdms_jdbc_byteArray   marshal_CORBA_sequence_octet
#define unmarshal_com_unisys_os2200_rdms_jdbc_byteArray unmarshal_CORBA_sequence_octet

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer1_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer1 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer2_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer2 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer3_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer3 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer4_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer4 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer5_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer5 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer6_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer6 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer7_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer7 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer8_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer8 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer9_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer9 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer10_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer10 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer11_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer11 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer12_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer12 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer13_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer13 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer14_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer14 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer15_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer15 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);

com_unisys_os2200_rdms_jdbc_byteArray *com_unisys_os2200_rdms_jdbc_XAJDBCServer16_XASendAndReceive (
        com_unisys_os2200_rdms_jdbc_XAJDBCServer16 _obj,
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt,
        CORBA_Environment *_env,
        int txn_flag
);


#endif
