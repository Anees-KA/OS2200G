#include <string.h>

/* Standard C header files and OS2200 System Library files */
/* Change to IDLC-generated source.
   The following #include was added to avoid warnings and remarks in UC. */
#include <marshal.h>

/* JDBC Project Files */
#include "xajdbcserver.h"

CORBA_Environment _exc_unk = {
    CORBA_SYSTEM_EXCEPTION,
    "IDL:omg.org/CORBA/UNKNOWN:1.0",
    "\0\0\0\0\0\0\0\1"
};

#define _CHECK(func) \
    _ret = func;     \
    if(_ret) {       \
        goto error;  \
    }

/* added code for CORBA free */
#define _FREE(ptr)                     \
    if (ptr != NULL) {                 \
        if (ptr->_buffer != NULL) {    \
            CORBA_free(ptr->_buffer);  \
        }                              \
        CORBA_free(ptr);               \
    }


static CORBA_Environment _env_CORBA_NO_MEMORY = {
                             CORBA_SYSTEM_EXCEPTION,
                             "IDL:omg.org/CORBA/NO_MEMORY:1.0",
                             NULL};
static CORBA_Environment _env_CORBA_MARSHAL = {
                             CORBA_SYSTEM_EXCEPTION,
                             "IDL:omg.org/CORBA/MARSHAL:1.0",
                             NULL};
static CORBA_Environment _env_CORBA_BAD_PARAM = {
                             CORBA_SYSTEM_EXCEPTION,
                             "IDL:omg.org/CORBA/BAD_PARAM:1.0",
                             NULL};
static CORBA_Environment _env_CORBA_UNKNOWN = {
                             CORBA_SYSTEM_EXCEPTION,
                             "IDL:omg.org/CORBA/UNKNOWN:1.0",
                             NULL};

static long marshal_error(int ret, Buffer *buffer, int body_pos,
                          CORBA_completion_status status)
{
    CORBA_ex_body body = { 0, 0 };
    body.completed = status;

    switch(ret) {
        case IIOP_MALLOC_ERR:
            _env_CORBA_NO_MEMORY.exception_struct = (char *)&body;
            marshal_system_exception(_env_CORBA_NO_MEMORY, buffer, body_pos);
            break;
        case IIOP_DECODE_BUFFER_OVER_ERR:
            _env_CORBA_MARSHAL.exception_struct = (char *)&body;
            marshal_system_exception(_env_CORBA_MARSHAL, buffer, body_pos);
            break;
        case IIOP_BAD_PARAM:
            _env_CORBA_BAD_PARAM.exception_struct = (char *)&body;
            marshal_system_exception(_env_CORBA_BAD_PARAM, buffer, body_pos);
            break;
        default:
            _env_CORBA_UNKNOWN.exception_struct = (char *)&body;
            marshal_system_exception(_env_CORBA_UNKNOWN, buffer, body_pos);
            break;
    }

    return CORBA_SYSTEM_EXCEPTION;
}

/* XAFUNC macro used for defining functions for XA Servers 1-16.
   The macro argument is the XA Server number (1-16).
   The XA Server number is not necessarily the same as the
   app group number. */

#define XAFUNC(XAServerNum)                                                   \
                                                                              \
int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer ##XAServerNum ##_invoke(     \
        PortableServer_Servant _servant,                                      \
        char *_operation,                                                     \
        Buffer *_request,                                                     \
        Buffer *_reply)                                                       \
{                                                                             \
    CORBA_Environment _env;                                                   \
    int _ret;                                                                 \
    CORBA_completion_status _stat = CORBA_COMPLETED_NO;                       \
    int _body_pos = _reply->index;                                            \
    int txn_flag = (int)_request->txn_flag; /* Get request txn_flag (short) as an int */ \
                                                                              \
    _env._major = 0;                                                          \
                                                                              \
    if(strcmp(_operation, "XASendAndReceive") == 0) {                         \
        com_unisys_os2200_rdms_jdbc_byteArray *_retval;                       \
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt;                       \
                                                                              \
        reqPkt = CORBA_sequence_octet__alloc();                               \
        if(reqPkt == NULL) {                                                  \
            _ret = IIOP_MALLOC_ERR;                                           \
            goto error;                                                       \
        }                                                                     \
        _CHECK( unmarshal_CORBA_sequence_octet(reqPkt, 1, _request) );        \
                                                                              \
        _stat = CORBA_COMPLETED_MAYBE;                                        \
                                                                              \
        _retval =                                                             \
        com_unisys_os2200_rdms_jdbc_XAJDBCServer ##XAServerNum ##_XASendAndReceive( \
            _servant,                                                         \
            reqPkt,                                                           \
            &_env,                                                            \
            txn_flag);                                                        \
        /* added code for CORBA free */ _FREE(reqPkt);                        \
        if(_env._major != 0) {                                                \
            if(_env._major == CORBA_SYSTEM_EXCEPTION) {                       \
                _stat = ((CORBA_ex_body *)(_env.exception_struct))->completed;\
                _CHECK( marshal_system_exception(_env, _reply, _body_pos) );  \
            }                                                                 \
            else {                                                            \
                _CHECK( marshal_system_exception(                             \
                    _exc_unk, _reply, _body_pos) );                           \
            }                                                                 \
            /* added code for CORBA free *//* _FREE(_retval); */              \
            return _env._major;                                               \
        }                                                                     \
                                                                              \
        _CHECK( marshal_CORBA_sequence_octet(_retval, 1, _reply) );           \
        /* added code for CORBA free */ /* _FREE(_retval); */                 \
        _stat = CORBA_COMPLETED_YES;                                          \
        /* added code for CORBA free */ return 0;                             \
                                                                              \
/* added code for CORBA free - label and return moved up into if block */     \
error:                                                                        \
    /* added code for CORBA free */ _FREE(reqPkt);                            \
    /* added code for CORBA free */ /* _FREE(_retval); */                     \
    return marshal_error(_ret, _reply, _body_pos, _stat);                     \
    }                                                                         \
                                                                              \
    return 0;                                                                 \
                                                                              \
}

/* Function definitions for XA Servers 1-16, using the macro XAFUNC. */

XAFUNC(1)
XAFUNC(2)
XAFUNC(3)
XAFUNC(4)
XAFUNC(5)
XAFUNC(6)
XAFUNC(7)
XAFUNC(8)
XAFUNC(9)
XAFUNC(10)
XAFUNC(11)
XAFUNC(12)
XAFUNC(13)
XAFUNC(14)
XAFUNC(15)
XAFUNC(16)

/* The edited version of the function for server 1,
   before it was commented out
   (because macro XAFUNC was defined to duplicate it for all 16 XA Servers).

   Note that comments are delimited by "/!" and !/" in the comments below
   so as to be able to use the normal comment delimiters to
   comment out the entire function.

   The IDL-to-C source has been edited,
   since it did not properly generate the CORBA_free calls
   for storage allocated by CORBA_alloc.
   The goal is to ensure that CORBA_free is called for the
   byte array argument and the byte array function return,
   for all possible control flow cases.

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer1_invoke(
        PortableServer_Servant _servant,
        char *_operation,
        Buffer *_request,
        Buffer *_reply)
{
    CORBA_Environment _env;
    int _ret;
    CORBA_completion_status _stat = CORBA_COMPLETED_NO;
    int _body_pos = _reply->index;
    _env._major = 0;

    if(strcmp(_operation, "XASendAndReceive") == 0) {
        com_unisys_os2200_rdms_jdbc_byteArray *_retval;
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt;

        reqPkt = CORBA_sequence_octet__alloc();
        if(reqPkt == NULL) {
            _ret = IIOP_MALLOC_ERR;
            goto error;
        }
        _CHECK( unmarshal_CORBA_sequence_octet(reqPkt, 1, _request) );

        _stat = CORBA_COMPLETED_MAYBE;

        _retval = com_unisys_os2200_rdms_jdbc_XAJDBCServer1_XASendAndReceive(
            _servant,
            reqPkt,
            &_env);
        /! added code for CORBA free !/ _FREE(reqPkt);
        if(_env._major != 0) {
            if(_env._major == CORBA_SYSTEM_EXCEPTION) {
                _stat = ((CORBA_ex_body *)(_env.exception_struct))->completed;
                _CHECK( marshal_system_exception(_env, _reply, _body_pos) );
            }
            else {
                _CHECK( marshal_system_exception(_exc_unk, _reply, _body_pos) );
            }
            /! added code for CORBA free !/ _FREE(_retval);
            return _env._major;
        }

        _CHECK( marshal_CORBA_sequence_octet(_retval, 1, _reply) );
        /! added code for CORBA free !/ _FREE(_retval);
        _stat = CORBA_COMPLETED_YES;
        /! added code for CORBA free !/ return 0;

/! added code for CORBA free - label and return moved up into if block !/
error:
    /! added code for CORBA free !/ _FREE(reqPkt);
    /! added code for CORBA free !/ _FREE(_retval);
    return marshal_error(_ret, _reply, _body_pos, _stat);
    }

    return 0;

}
end of commented out function for server 1 */

/* The original function of the function for server 1,
   generated by the IDL-to-C compiler,
   before editing for the CORBA_free() calls,
   has been commented out below.

int POA_com_unisys_os2200_rdms_jdbc_XAJDBCServer1_invoke(
        PortableServer_Servant _servant,
        char *_operation,
        Buffer *_request,
        Buffer *_reply)
{
    CORBA_Environment _env;
    int _ret;
    CORBA_completion_status _stat = CORBA_COMPLETED_NO;
    int _body_pos = _reply->index;
    _env._major = 0;

    if(strcmp(_operation, "XASendAndReceive") == 0) {
        com_unisys_os2200_rdms_jdbc_byteArray *_retval;
        com_unisys_os2200_rdms_jdbc_byteArray * reqPkt;

        reqPkt = CORBA_sequence_octet__alloc();
        if(reqPkt == NULL) {
            _ret = IIOP_MALLOC_ERR;
            goto error;
        }
        _CHECK( unmarshal_CORBA_sequence_octet(reqPkt, 1, _request) );

        _stat = CORBA_COMPLETED_MAYBE;

        _retval = com_unisys_os2200_rdms_jdbc_XAJDBCServer1_XASendAndReceive(
            _servant,
            reqPkt,
            &_env);
        if(_env._major != 0) {
            if(_env._major == CORBA_SYSTEM_EXCEPTION) {
                _stat = ((CORBA_ex_body *)(_env.exception_struct))->completed;
                _CHECK( marshal_system_exception(_env, _reply, _body_pos) );
            }
            else {
                _CHECK( marshal_system_exception(_exc_unk, _reply, _body_pos) );
            }
            return _env._major;
        }

        _CHECK( marshal_CORBA_sequence_octet(_retval, 1, _reply) );
        _stat = CORBA_COMPLETED_YES;

    }

    return 0;

error:
    return marshal_error(_ret, _reply, _body_pos, _stat);
}

end of commented out original IDL-to-C function for server 1 */
