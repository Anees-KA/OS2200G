/**
 * File: DeregServer.c.
 *
 * Main program file used to Deregister from COMAPI.
 *
 * This program file will utilize the NetworkAPI code files to support the
 * calls to COMAPI.  This program exists to clearly insure that the JDBC
 * Server has been deregistered with COMAPI in case of a JDBC Server failure,
 * e.g. an aborted development testing session.
 *
 * Include all of the normal JDBC Server include files.  Even though only
 * a couple of these files are actually needed, this insures that there are
 * no conflicts within them ( conflicts would appear in the compilation
 * listing).  Also, this routine may be enhanced at a later time and will then
 * require some of these includes.
 */

/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <rsa.h>
#include <ertran.h>
#include <time.h>
#include <tsqueue.h>
#include <task.h>

/* JDBC Project header files */
#include "apidef.p"

/**
 * Procedure main():
 * This procedure deregisters with COMAPI.  This appears needed in the cases where
 * the JDBC Server has terminated in error.  COMAPI will deregister any JDBC Server
 * sockets and/session tables, that may still be assigned to this JDBC Server run.
 */
main(){

    /*
     * First print out a message on STDOUT that we are clearing up our COMAPI
     * connectivity by using a TCP_DEREGISTER. Display this message to STDOUT.
     */
     printf("Attempting to TCP_DEREGISTER with COMAPI.\n");

    /*
     * Now do the TCP deregistration operation. Notice that TCP_DEREGISTER
     * does not return a status value.
     */
    TCP_DEREGISTER();

    /*
     * Now print out a message on STDOUT that we have completed clearing up our COMAPI
     * connectivity. Display this message to STDOUT.
     */
    printf("Completed the deregistration with COMAPI.\n");
    return 0; /* make the compiler happy */
    } /* end of main procedure */
