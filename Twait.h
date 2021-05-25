#ifndef _TWAIT_H_
#define _TWAIT_H_
/*
 * File: Twait.h.
 *
 * Header file for the code that does an ER Twait$ for a specified number of milliseconds.
 *
 * Header file corresponding to code file Twait.c.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <ertran.h>

#define ER_TWAIT 060                      /* ER code for TWAIT$ */

void twait(int wait_in_milliseconds);

#endif /* _TWAIT_H_ */
