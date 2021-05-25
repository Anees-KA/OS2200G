/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <ertran.h>
#include <string.h>

/* JDBC Project Files */
#include "aprtcn.h"

#define ER_APRTCN 074
#define BYTES_PER_WORD 4
#define MAX_WORDS_IN_APRTCN 62
#define MAX_CHARS_IN_APRTCN MAX_WORDS_IN_APRTCN*BYTES_PER_WORD
#define minimum(a,b) ((a) < (b) ? (a) : (b))

/* ER APRTCN$ packet word format */
typedef union {
    struct {
        short wordSize;
        short addr;
    } p1;

    int i1;
} aprtcnWord;

/**
 * sendConsCmd
 *
 * Send an @@CONS command to the JDBC server's Console Handler (CH).
 *
 * @param cmd
 *   The CH command to execute (e.g., "display server status").
 *
 * @param keyinName
 *   A string that is the keyin name for Console Commands for the JDBC server.
 */

void sendConsCmd(char * CHCmd, char * keyinName) {
    char str[MAX_CHARS_IN_APRTCN+1];
    int len;

    len = minimum(strlen(CHCmd), MAX_CHARS_IN_APRTCN);
    strcpy(str, "@@CONS ");
    strncat(str, keyinName, MAX_KEYIN_NAME_CHAR_SIZE);
    strcat(str, " ");
    strncat(str, CHCmd, MAX_CHARS_IN_APRTCN-MAX_KEYIN_NAME_CHAR_SIZE-8);
        /* subtract 8 for "@@CONS " plus a blank before the command */

    /* Perform the ER to send the console command */
    doAPRTCN(str);

} /* sendConsCmd */

/**
 * doAPRTCN
 *
 * Perform an ER APRTCN$ to send an @@CONS Console Handler command.
 *
 * The D format is used for ER APRTCN$.
 * This lets you enter any transparent control statement
 * (starting with "@@").
 *
 * The image sent to ER APRTCN$ contains the following
 * items concatenated together:
 *   - "D,"
 *   - "@@CONS "
 *   - the keyin name for Console Commands for the JDBC server
 *   - a blank
 *   - the CH command
 *
 * All but the first item ("D,") are in the s1 argument.
 *
 * Example of a string passed to ER APRTCN$:
 *   "D,@@CONS JSAG3 display server status"
 *
 * @param s1
 *   The string to send as an @@CONS command
 *   (i.e., the string starts with "@@CONS").
 */

void doAPRTCN(char * s1) {

    int code;
    int in;
    int out;
    int inarr[1];
    int outarr[2];
    int scratch;
    int byteCount;
    int wordCount;
    int index;

     /* Allow for blank fill and NUL */
    char str[MAX_CHARS_IN_APRTCN+BYTES_PER_WORD+1];

    aprtcnWord pkt;

    /* Copy the string (prefixed by "D,")
       into a word aligned, blank filled buffer */
    strcpy(str, "D,");
    byteCount = minimum(strlen(s1)+2, MAX_CHARS_IN_APRTCN);
        /* Add 2 characters for "D," */
    wordCount = (byteCount + (BYTES_PER_WORD - 1)) / BYTES_PER_WORD;
    strncat(str, s1, MAX_CHARS_IN_APRTCN-2);

    for (index=0; index< 4; index++) {
        str[byteCount+index] = ' ';
    }

    /* Set up the packet word for ER APRTCN$*/
    pkt.i1 = (int) str;
    pkt.p1.wordSize = wordCount;

    /* Set up the parameters for ucsgeneral */
    in = 0400000000000;
    out = 0;
    inarr[0] = pkt.i1;
    code = ER_APRTCN;

    /* Perform the ER APRTCN */
    ucsgeneral(&code, &in, &out, inarr, outarr, &scratch, &scratch);

} /* doAPRTCN */
