#ifndef _SERVER_ACT_WDE_H_
#define _SERVER_ACT_WDE_H_

/* JDBC Project Files */
#include "JDBCServer.h"

/**
 * File: ServerActWde.h.
 *
 * Header file for the code that establishes a pointer for each of the
 * JDBC Servers activities to their WDE entry (e.g. SW's to their wdePtr,
 * or for the ICL and CH activities to their activity local WDE entry). The
 * code also defines the debug/trace variables and file handle for use
 * by the client-side debug/trace handling code throughout the C-Interface
 * Library code.
 *
 * Header file corresponding to code file ServerActWDE.c.
 *
 */

/* Function prototypes */
void setupActivityLocalWDE(enum ownerActivityType ownerActivity,
                           int associated_COMAPI_bdi, int associated_COMAPI_IPv6,
                           int associated_ICL_num);

void minimalInitializeWDE(workerDescriptionEntry *wdeptr,
                          enum ownerActivityType ownerActivity);

void minimalDefaultsSGD();

#endif /* _SERVER_ACT_WDE_H_ */
