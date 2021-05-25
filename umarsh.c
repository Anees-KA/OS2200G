/* Standard C header files and OS2200 System Library files */
#include <stdlib.h>
#include <string.h>
#include "marshal.h"

/* JDBC Project Files */
#include "xajdbcserver.h"

#define _CHECK(func) \
    _ret = func;     \
    if(_ret) {       \
        return _ret; \
    }

#ifndef _CORBA_sequence_octet_api_defined
#define _CORBA_sequence_octet_api_defined
CORBA_sequence_octet *CORBA_sequence_octet__alloc(void)
{
    return (CORBA_sequence_octet *)CORBA_alloc(sizeof(CORBA_sequence_octet));
}

CORBA_octet *CORBA_sequence_octet_allocbuf(CORBA_unsigned_long len)
{
    return (CORBA_octet *)CORBA_alloc(sizeof(CORBA_octet) * len);
}

int marshal_CORBA_sequence_octet(CORBA_sequence_octet *args, int count, Buffer *buffer)
{
    CORBA_sequence_octet _val;
    int _idx, _ret;

    for(_idx = 0; _idx < count; _idx++) {
        _val = args[_idx];
        _CHECK( marshal_ulong(&(_val._length), 1, buffer) );
        _CHECK( marshal_octet((_val._buffer), (_val._length), buffer) );
    }

    return 0;
}

int unmarshal_CORBA_sequence_octet(CORBA_sequence_octet *args, int count, Buffer *buffer)
{
    CORBA_sequence_octet _val;
    int _idx, _ret;

    for(_idx = 0; _idx < count; _idx++) {
        _CHECK( unmarshal_ulong(&(_val._length), 1, buffer) );
        _val._buffer = CORBA_sequence_octet_allocbuf(_val._length);
        if(_val._buffer == NULL) {
            return IIOP_MALLOC_ERR;
        }
        _CHECK( unmarshal_octet((_val._buffer), (_val._length), buffer) );
        args[_idx] = _val;
    }

    return 0;
}
#endif /* _CORBA_sequence_octet_api_defined */

