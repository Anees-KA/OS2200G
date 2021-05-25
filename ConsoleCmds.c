/**
 * File: ConsolCmds.c.
 *
 * Utility functions to help process the JDBC Server's
 * console handling commands.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <ertran.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>
#include <ctype.h>
#include <universal.h>
#include <tsqueue.h>  /* needed for T/S cell definitions */

/* JDBC Project Files */
#include "ConsoleCmds.h"

/* Imported data */

extern serverGlobalData sgd;
    /* The Server Global Data (SGD), visible to all Server activities. */

/**
 * Function findWorker
 *
 * This function scans the WDE entries for a given socket ID.
 * If found, it returns the pointer to that WDE (worker description entry)
 * entry for that worker and returns 0.
 * If not found, it returns an error code.
 *
 * For the XA Server case,
 * this function is only called when the first argument
 * points to an int containing the magicSocketID.
 * In this case, we always return the first (and only) WDE entry.
 * There is always exactly one WDE entry for the XA case.
 *
 * @param clientSocketIDPtr
 *   A pointer to an int representing the socket ID of a client.
 *   This is H1 (the upper 18 bits) in the WDE's socket ID word
 *   (client_socket).
 *   The ID is specified in decimal in a console command.
 *
 *   If the value is magicSocketID (-9),
 *   return the function value as the first client with a non-zero socket ID.
 *   This reference parameter causes the socket ID that is picked
 *   from the table to be returned to the caller.
 *   It's a reference parameter for the magicSocketID case.
 *
 *   For other cases, the reference parameter value is the socket ID (int)
 *   pointed to from the caller.
 *
 * @param retPtr
 *   A pointer to a pointer to a WDE entry.
 *   This is a reference parameter that is set to point to the
 *   WDE entry requested.
 *   If not found, a NULL pointer is returned.
 *
 * @return
 *   A status:
 *     - 0 is returned if the worker is found.
 *     - An error code is returned if the worker is not found.
 */

int findWorker(int * clientSocketIDPtr, workerDescriptionEntry ** retPtr) {

    int i1;
    int socketIDArg;

#ifndef XABUILD /* Local Server */

    int tableSocketID;

#endif

    socketIDArg = *clientSocketIDPtr;

    for (i1 = 0; i1<sgd.numWdeAllocatedSoFar; i1++) {
        if (sgd.wdeArea[i1].serverWorkerState == CLIENT_ASSIGNED) {
            /* We found an active worker.
               This is the same check used by the DISPLAY commands
               when they check for active workers. */

#ifdef XABUILD /* XA Server */

            if (socketIDArg == magicSocketID) {
                /* We automatically return the first WDE entry
                   for the XA case */

#else /* Local Server */

            tableSocketID = getInternalSocketID(sgd.wdeArea[i1].client_socket);

            if ((tableSocketID == socketIDArg)
                    /* the socket ID arg matched the one in the WDE entry */

                || ((socketIDArg == magicSocketID)
                    && (tableSocketID != 0))) {
                        /* If the client socket ID arg is magicSocketID,
                           simply return the first client with a
                           non-zero socket ID */

#endif

                /* We found the correct worker.
                   Get a pointer to its WDE and the reference parameter.
                   Return 0 as the function result.
                   */

#ifdef XABUILD /* XA Server */

                *clientSocketIDPtr = 0;
                    /* This should not be used, but change the value to 0,
                       for any possible error messages by the caller . */


#else /* Local Server */

                *clientSocketIDPtr = tableSocketID;

#endif

                *retPtr = &sgd.wdeArea[i1];
                return 0;
            }
        }
    }

    /*
     * Loop has ended with no find.
     * Return a nonzero error status.
     * There is no WDE ptr to return as a reference parameter.
     */
    *retPtr = NULL;
    return JDBCSERVER_UNABLE_TO_FIND_SPECIFIED_WORKER;

} /* findWorker */

/**
 * Function findWorker_by_RDMS_threadname
 *
 * This function scans the WDE entries for a RDMS thread name.
 * If found, it returns an int representing the socket ID of a client.
 *   (This is H1 (the upper 18 bits) in the WDE's socket ID word
 *   (client_socket).
 * If not found, it returns a socket ID value of -1 (considered an error
 * indicator by the caller since it is NOT the magicSocketID (-9) and
 * a value not to be found by findWorker().
 *
 * @param threadnamePtr
 *   A pointer to a character string containing the RDMS thread name.
 *
 * @return
 *
 * @param clientSocketIDPtr
 *   A pointer to an int representing the socket ID of the client
 * with the desired RDMS thread name.
 *   This is H1 (the upper 18 bits) in the WDE's socket ID word
 *   (client_socket).
 *   The ID is specified in decimal in a console command.
 */

void findWorker_by_RDMS_threadname(char * threadnamePtr, int * clientSocketIDPtr) {

    int i1;

    for (i1 = 0; i1<sgd.numWdeAllocatedSoFar; i1++) {
        if (sgd.wdeArea[i1].serverWorkerState == CLIENT_ASSIGNED) {
            /* We found an active worker.
               This is the same check used by the DISPLAY commands
               when they check for active workers. */

            if (strncmp(threadnamePtr,sgd.wdeArea[i1].threadName, VISIBLE_RDMS_THREAD_NAME_LEN) == 0){
                /* We found the correct worker.
                   Get its client socket ID value.
                   Return 0 as the function result.
                   */
                *clientSocketIDPtr = getInternalSocketID(sgd.wdeArea[i1].client_socket);
                return;
            }
        }
    }

    /*
     * Loop has ended with no find.
     * Return the error indicator (-1) as the client socket ID value.
     */
    *clientSocketIDPtr = NOFIND_THREADNAME_SOCKETID;
    return;

} /* findWorker_by_RDMS_threadname */

/**
 * getInternalSocketID
 *
 * Return the internal socket ID,
 * which is in H1 (the upper 18 bits) of the WDE's client_socket field
 * (which is a word).
 *
 * @param client_socket
 *   A client_socket word from a WDE entry.
 *
 * @return
 *   Return an int with the 18 bit internal socket ID.
 */

int getInternalSocketID(int client_socket) {
    int value;

    value = client_socket;
    value >>= 18; /* eliminate the lower 18 bits */

    return value;

} /* getInternalSocketID */
