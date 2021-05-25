/**
 * File: Twait.c.
 *
 *  Code file containing the Twait procedure.
 *
 */
/* JDBC Project Files */
#include "Twait.h"

/*
 * Procedure twait.
 * This procedure does an ER TWAIT$ for a specified number of milliseconds.
 */

void twait(int wait_in_milliseconds){

    int inregs[1];
    int outregs[1];
    int ercode=ER_TWAIT;             /* do an ER TWAIT$ */
    int inputval=0200000000000;      /* indicate to load register a1 */
    int outputval=0;                 /* no output is expected in outregs */

    inregs[0]=wait_in_milliseconds;  /* wait for specified time in milliseconds */
    ucsgeneral(&ercode,&inputval,&outputval,inregs,outregs); /* do the TWAIT$ */
}



