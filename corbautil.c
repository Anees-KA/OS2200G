/* Standard C header files and OS2200 System Library files */
#include <CORBA.h>
#include <stdio.h>

/* JDBC Project Files */
#include "xajdbcserver.h"

/* Utility functions for CORBA. */

/*
 * getCORBAByteArray
 *
 * Return the byte array length and pointer for a variable length
 * CORBA byte array.
 *
 * @param byteArrayStructPtr
 *   A pointer to a CORBA variable length byte array structure
 *   (com_unisys_os2200_rdms_jdbc_byteArray, which is the same as
 *   CORBA_sequence_octet).
 *
 * @param lenPtr
 *   A pointer to an int.
 *   This is a reference parameter that returns the length of the byte array.
 *   The caller allocates an int and passes the address of the int.
 *
 * @param byteArrayPtrPtr
 *   A pointer to a pointer to a CORBA variable length byte array
 *   that is allocated by this function.
 *   (reference parameter).
 *   This is a pointer to a char pointer.
 *   A pointer to the CORBA variable length byte array is returned
 *   as a reference parameter.
 *   The caller allocates a pointer to CORBA_octet (i.e., pointer to char)
 *   and passes the address of the pointer.
 */

void getCORBAByteArray(
    com_unisys_os2200_rdms_jdbc_byteArray * byteArrayStructPtr,
    int * len,
    CORBA_octet ** byteArrayPtrPtr) {

    /* Set reference parameters */
    *len = byteArrayStructPtr->_length;
    *byteArrayPtrPtr = byteArrayStructPtr->_buffer;

} /* getCORBAByteArray */

/*
 * SetUpCORBAByteArray
 *
 * Set up a CORBA variable length byte array
 * and the structure containing a pointer to the array.
 * This structure is to be returned to Java client code.
 *
 * @param len
 *   The length in bytes of the byte array.
 *
 * @param byteArrayStructPtrPtr
 *   A pointer to a pointer to a CORBA variable length byte array structure
 *   that is allocated by this function.
 *   (reference parameter).
 *   This is a pointer to pointer to com_unisys_os2200_rdms_jdbc_byteArray.
 *   A pointer to a CORBA variable length byte array structure
 *   is returned as a reference parameter.
 *   The caller allocates a pointer to com_unisys_os2200_rdms_jdbc_byteArray
 *   and passes the address of the pointer.
 *   Note that com_unisys_os2200_rdms_jdbc_byteArray is the same as
 *   CORBA_sequence_octet.

 *
 * @param byteArrayPtrPtr
 *   A pointer to a pointer to a CORBA variable length byte array
 *   that is allocated by this function.
 *   (reference parameter).
 *   This is a pointer to a char pointer.
 *   A pointer to the CORBA variable length byte array is returned
 *   as a reference parameter.
 *   The caller allocates a pointer to CORBA_octet (i.e., pointer to char)
 *   and passes the address of the pointer.
 */

void setUpCORBAByteArray(int len,
    com_unisys_os2200_rdms_jdbc_byteArray ** byteArrayStructPtrPtr,
    CORBA_octet ** byteArrayPtrPtr) {

    com_unisys_os2200_rdms_jdbc_byteArray * byteArrayStructPtr;
        /* This is a struct ptr pointing to a CORBA struct */

    CORBA_octet * byteArrayPtr;
        /* pointer to a byte array (CORBA_octet is char) */

    /* Allocate the structure */
    byteArrayStructPtr = CORBA_sequence_octet__alloc();

    /* Allocate the byte array */
    byteArrayPtr = CORBA_sequence_octet_allocbuf(len);

    /* Fill in fields in the structure */
    byteArrayStructPtr->_maximum = len;
    byteArrayStructPtr->_length = len;
    byteArrayStructPtr->_buffer = byteArrayPtr;

    /* Set the reference parameters */
    *byteArrayStructPtrPtr = byteArrayStructPtr;
    *byteArrayPtrPtr = byteArrayPtr;

} /* setUpCORBAByteArray */

/*
 * formCORBAPacket
 *
 * Given a CORBA variable length byte array and its length,
 * formCORBAPacket gets a CORBA structure and places the reference
 * to that byte array inthe structure. The pointer to this CORBA
 * structure is then returned to the caller.
 *
 * @param byteArrayPtr
 *   The caller has already allocated a CORBA_octet byte array
 *   and passes the address of that array.
 *
 * @param len
 *   The length in bytes of the byte array.
 *
 * @return byteArrayStructPtrPtr
 *   A pointer to  a CORBA variable length byte array structure
 *   that is allocated by this function.
 *   This is a pointer to a com_unisys_os2200_rdms_jdbc_byteArray.
 */

com_unisys_os2200_rdms_jdbc_byteArray * formCORBAPacket(CORBA_octet * byteArrayPtr, int len){

    com_unisys_os2200_rdms_jdbc_byteArray * byteArrayStructPtr;
        /* This is a struct ptr pointing to a CORBA struct */

    /* Allocate the structure */
    byteArrayStructPtr = CORBA_sequence_octet__alloc();

    /* Fill in fields in the structure */
    byteArrayStructPtr->_maximum = len;
    byteArrayStructPtr->_length = len;
    byteArrayStructPtr->_buffer = byteArrayPtr;

    /* printf("In formCORBAPacket, len =%d, byteArrayPtr= %p , byteArrayStructPtr = %p\n", len, byteArrayPtr, byteArrayStructPtr); */

    /* Return the CORBA array reference */
    return byteArrayStructPtr;

} /* formCORBAPacket */

/**
 * printCORBAByteArrayInHex
 *
 * Print a byte array in hex format (10 elements per line).
 *
 * @param ret
 *   A pointer to a char array.
 *
 * @param len
 *   The number of bytes to dump.
 */

void printCORBAByteArrayInHex(char * ret, int len) {

    int i1;

    for (i1=0; i1<len; i1++) {
        printf(" %2x", ret[i1]);

        /* Insert an end of line character if we are at the last
           array element, or if we have inserted 10 elements
           on this line. */
        if ((i1%10 == 9) || (i1 == len-1)) {
            printf("\n");
        }
    }

} /* printCORBAByteArrayInHex */

/*
 * freeCORBAPacket
 *
 * Free the CORBA storage for the byte array structure
 * and the byte array itself.
 *
 * @param byteArrayStructPtr
 *   A pointer to a CORBA byte array structure.
 *   The type is CORBA_sequence_octet, which is the same as
 *   com_unisys_os2200_rdms_jdbc_byteArray.
 *   This structure contains a pointer to a byte array
 *   (i.e., a pointer to CORBA_octet, which is the same as char).
 */

void freeCORBAPacket(
    com_unisys_os2200_rdms_jdbc_byteArray * byteArrayStructPtr) {

    char * byteArrayPtr;

    if (byteArrayStructPtr != NULL) {
        byteArrayPtr = byteArrayStructPtr->_buffer;

        if (byteArrayPtr != NULL) {
            CORBA_free(byteArrayPtr); /* free the byte array */
        }

        CORBA_free(byteArrayStructPtr); /* free the structure */
    }
}
