/*
 * Copyright (c) 2019 Unisys Corporation.
 *
 *          All rights reserved.
 *
 *          UNISYS CONFIDENTIAL
 */

/*
 * File: xaoutFiles.c.
 *
 * Main program file used for output file handling during JDBC XA Server runstream execution.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <tsqueue.h>
#include <task.h>
#include <sysutil.h>
#include <ctype.h>
#include <universal.h>
#include <ertran.h>
#include <fp$defs.h>
#include <fp$file$id.h>
#include <fp$generic.h>
#include <fp$delfil.h>
#include <fp$cpyrafraf.h>
#include <fp$chgfilcyc.h>
#include "marshal.h"

/* Global debug indicator */
#define DEBUG                   0   /* 0 - Debug off, 1 - Debug on */

/* Static global defines */
#define TWELVE_BLANKS           "            "
#define RUNID_CHAR_SIZE         6
#define IMAGE_MAX_CHARS         256
#define MESSAGE_MAX_CHARS       300
#define XASERVER_NUMBER_LEN     2
#define CYCLES_UPPERCASE        "CYCLES"

/* Program exit returned condition values */
#define NORMAL_EXECUTION_NO_ERRORS                            0
#define OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR  1
#define COULD_NOT_CATALOG_INTERMEDIARY_FILES                  2
#define COULD_NOT_DELETE_INTERMEDIARY_FILES                   3
#define COULD_NOT_CATALOG_RETAINED_FILES                      4
#define COULD_NOT_CYCLE_RETAINED_FILES                        5
#define COULD_NOT_COPY_INTERMEDIARY_TO_RETAINED_FILES         6
#define COULD_NOT_FREE_RETAINED_FILES                         7

/* Defines and declarations for ACSF$ calls. */
#define ACFS_IMAGE_MAX_CHARS    120

/* Debugging and IGDM testing flags*/
static int debug;     /* 0 - Debug off, 1 - Debug on. Normally 0 (off). */
static int igdmFlag;  /* Used to indicate an IGDM is desired. Normally 0 (off). */

/* Declarations for program invocation information. */
static int option_bits;
static char optionLetters[27];
static int xaServerNumber;
static char cataloging_size_loc[IMAGE_MAX_CHARS];
static char diag_cataloging_size_loc[IMAGE_MAX_CHARS];
static int  maxFileCycles;
static char runid[RUNID_CHAR_SIZE+1];
static char grunid[RUNID_CHAR_SIZE+1];

/* Error message construction and handling. */
static char message[MESSAGE_MAX_CHARS];

/* Space and definitions for intermediary and retained output files. */
#define MAX_FILENAME_SIZE       60
#define JXBNAME_PART1           "JXB"
#define JXLNAME_PART1           "JXL"
#define JXTNAME_PART1           "JXT"
#define JXDNAME_PART1           "JXD"

#define XABRKNAME_PART1         "JDBC$XABRK"
#define XALOGNAME_PART1         "JDBC$XALOG"
#define XATRCNAME_PART1         "JDBC$XATRC"

#define XDBRKNAME_PART1         "JDBC$XDBRK"
#define XDLOGNAME_PART1         "JDBC$XDLOG"
#define XDTRCNAME_PART1         "JDBC$XDTRC"
#define XDIAGNAME_PART1         "JDBC$XDIAG"

#define JDBCBRK_USENAME         "JDBC$BRK"
#define JDBCLOG_USENAME         "JDBC$LOG"
#define JDBCTRC_USENAME         "JDBC$TRACE"
#define JDBCDIAG_USENAME        "JDBC$DIAG"

static char intermediaryBrkFileName[MAX_FILENAME_SIZE];
static char intermediaryLogFileName[MAX_FILENAME_SIZE];
static char intermediaryTrcFileName[MAX_FILENAME_SIZE];
static char intermediaryDiagFileName[MAX_FILENAME_SIZE];

static char retainedBrkFileName[MAX_FILENAME_SIZE];
static char retainedLogFileName[MAX_FILENAME_SIZE];
static char retainedTrcFileName[MAX_FILENAME_SIZE];
static char retainedDiagFileName[MAX_FILENAME_SIZE];

/*
 * Procedure main
 *
 * This program will be executed before and after the JDBC XA Server execution to handle file
 * cataloging, cycling of intermediary output files and later the and copying of this files
 * into a cataloged and cycled retained set of normal or abnormal execution output files based
 * on how the JDBC XA Server execution terminates.
 *
 * Program callable FURPUR (PCFP) is utilized to handle file copying, file deleting, and file
 * cycling operations, allowing the program to handle any error situations programmatically,
 * keeping the actual runxasrvrnn runstream as straightforward in design as possible.
 *
 * Operation Overview:
 * The xaoutFiles program is invoked before the JDBC XA Server invocation as indicated via the
 * B option, and after the JDBC XA Server invocation as indicated via the A option. A set of
 * three input images is provides to each xaoutFiles execution.
 *
 * IMPORTANT: The three images provided to the before and after executions of xaoutFiles must
 * have the same cataloging options and cycle number specification. This is required since the
 * before execution (B option) of xaoutFiles may also need to catalog, cycle, and copy into the
 * retained abnormal set of output files just like the after execution (A option) of xaoutFiles
 * may need to do. In that case, these cataloged files must have the save cataloged size,
 * placement, and cycle characteristics.
 *
 * Special program invocation in debugging or IGDM diagnostic mode
 * -------------------------------------------------------------------------------
 * If there is the need to debug the xaoutFiles program itself, or have it IGDM as
 * part of a functional test, the following option sequences can be added to the
 * program invocation:
 *   DT  - These options together indicate that the debugging flag, debug, should be
 *         set to 1 enabling debug tracing during xaoutFiles execution. Once detected,
 *         the two option letters are removed from the option_bits value so normal
 *         option letter testing/processing can be debugged.
 *   DI  - These options together indicate that the xaoutFiles program should abort
 *         with an IGDM at the end of a successful execution instead of just returning
 *         a termination code of 0 (flag igdmFlag will indicate this). This is used by
 *         the functional test to determine proper runstream operation in case the JDBC
 *         XA Server or xaoutFiles program were to experience a IGDM after having a
 *         working set of intermediary output files. Once detected, the option letters
 *         are removed from the option_bits value so normal option letter testing/processing
 *         can be performed.
 *   DN  - These options together indicate that the xaoutFiles program should abort
 *         NOW (immediately) causing an IGDM.  This is used by the functional test to determine
 *         proper runstream operation in case the JDBC XA Server or xaoutFiles program
 *         were to experience a IGDM at the beginning of their executions before they
 *         could access or create the intermediary set of output files. Since the program
 *         immediately IGDM's, there is no need to remove the option letters from the
 *         option_bits value.
 *
 *   The DT and DI sequences can be combined, i.e. DIT, so that both program debug tracing
 *   and a terminating IGDM can occur.
 *
 *
 * Normal program invocation/operation done before the JDBC XA Server
 * -------------------------------------------------------------------------------
 * The xaoutFiles program is invoked using the sequence (where # is replaced with
 * the number in the JDBC XA Server being executed, e.g., xajdbcsrvr#):
 *
 * @*JDBC$ABS.XAOUTFILES,B #
 * ///262000   . (for capturing PRINT$, LOG, TRACE files)
 * ///262000   . (for capturing DIAG$ file, if needed)
 * CYCLES 32   . Number of file cycles to keep (from 1 to 32).
 *
 *
 * This xaoutFiles invocation does the following file related operations
 * programmatically (where <grunid> is the batch run's generated runid, # is
 * the XA Server number, <size-loc> is the file cataloging information provided
 * on the first two input images, and <n> is the nuumeric value on the third
 * cycles image):
 *
 * 1) Tests if file *JXB#-<grunid> already exists.
 *    A) File does exist. (The previous runstream execution under this
 *       same generated runid was abnormally aborted immediately before the
 *       intermediary files could be saved (likely caused by a contingency
 *       17 that occurred because of an internal abort in UDS/RDMS). Then
 *       do the following steps programmatically:
 *
 *         1) @ASG,CP  *JDBC$XDBRK#(+1).,<size-loc>  . Retained name for abnormal execution's breakpoint file (JDBC$BRK)
 *         2) @ASG,CP *JDBC$XDLOG#(+1).,<size-loc>   . Retained name for abnormal execution's log file (JDBC$LOG)
 *         3) @ASG,CP *JDBC$XDTRC#(+1).,<size-loc>   . Retained name for abnormal execution's trace file (JDBC$TRACE)
 *         4) @ASG,CP *JDBC$XDIAG#(+1).,<size-loc>   . Retained name for abnormal execution's diag$ file (JDBC$DIAG)
 *
 *         5) @CYCLE *JDBC$XDBRK#.,<n>               . Indicate number of cycles to save
 *         6) @CYCLE *JDBC$XDLOG#.,<n>               . Indicate number of cycles to save
 *         7) @CYCLE *JDBC$XDTRC#.,<n>               . Indicate number of cycles to save
 *         8) @CYCLE *JDBC$XDIAG#.,<n>               . Indicate number of cycles to save
 *
 *         9) @COPY  *JXB#-<grunid>.,*JDBC$XDBRK#(+1).  . Retain previous PRINT$ file
 *        10) @COPY  *JXL#-<grunid>.,*JDBC$XDLOG#(+1).  . Retain previous JDBC$LOG file
 *        11) @COPY  *JXT#-<grunid>.,*JDBC$XDTRC#(+1).  . Retain previous JDBC$TRACE file
 *        12) @COPY  *JXD#-<grunid>.,*JDBC$XDIAG#(+1).  . Retain previous JDBC$DIAG file
 *
 *        13) @FREE  *JDBC$XDTRC#(+1).               . Free retained trace file
 *        14) @FREE  *JDBC$XDIAG#(+1).               . Free retained diag$ file
 *        15) @FREE  *JDBC$XDLOG#(+1).               . Free retained log file
 *        16) @FREE  *JDBC$XDBRK#.                   . Retain previous PRINT$ file
 *
 *        17) @DELETE,C *JXL#-<grunid>.              . Now delete intermediary file, not needed any more
 *        18) @DELETE,C *JXT#-<grunid>.              . Now delete intermediary file, not needed any more
 *        19) @DELETE,C *JXD#-<grunid>.              . Now delete intermediary file, not needed any more
 *        20) @DELETE,C *JXB#-<grunid>.              . Now delete intermediary file, not needed any more
 *
 *
 *       The above sequence works because step 1's @ASG,CP statement causes a hold on any other runstreams
 *       that attempt to catalog a new cycle of *JDBC$XDBRK#; the hold remains until step 16 completes,
 *       at which time another runstream could perform the sequence. By that time, the four new cycles of
 *       *JDBC$XDBRK, *JDBC$XDLOG, *JDBC$XDTRC, and JDBC$XDIAG$ have been cataloged, have the same cycle
 *       number (which is VERY important), and the file copying has been completed.
 *
 *       After step 16, there is no more need to have access to the four *JDBC$XD... files, and they may
 *       in fact be later replaced when enough file save steps occur to wrap around the number of saved
 *       file cycles.
 *
 *       Once the saving of the earlier intermediary file set is completed, they're deleted in steps 17-20.
 *
 *       Should any of the above groups of four steps fail (e.g., could not catalog one of the retained files,
 *       or a copy operation fails) then the xaoutFiles program will terminate with an error indication (see
 *       code list below for code returned). If they do operate correctly, the code flows into case B that follows.
 *
 *    B) File does not exist. (The previous runstream execution under this same generated runid either
 *       terminated normally, or terminated abnormally (e.g., JDBC XA Server IGDM). Either way, it was
 *       able to save the intermediary files into the appropriate set of retained output files. So form
 *       a new set of intermediary output files by doing these steps programmatically:
 *
 *         1) @CAT,P *JXB#-<grunid>.,<size-loc>   . Intermediary file for execution's breakpoint file (JDBC$BRK)
 *         2) @USE JDBC$BRK,*JXB#-<grunid>.       . Place use name JDBC$BRK
 *         3) @CAT,P *JXL#-<grunid>).,<size-loc>  . Intermediary file for execution's log file (JDBC$LOG)
 *         4) @USE JDBC$LOG,*JXL#-<grunid>).      . Place use name JDBC$LOG
 *         5) @CAT,P *JXT#-<grunid>.,<size-loc>   . Intermediary file for execution's trace file (JDBC$TRACE)
 *         6) @USE JDBC$TRACE,*JXT#-<grunid>.     . Place use name  JDBC$TRACE
 *         7) @CAT,P *JXD#-<grunid>.,<size-loc>   . Intermediary file for execution's diag$ file (JDBC$DIAG)
 *         8) @USE JDBC$DIAG,*JXD#-<grunid>.      . Place use name  JDBC$DIAG
 *
 *       Should any of the @CAT's fail the xaoutFiles program will terminate with an error indication (see code
 *       list below for code returned) and the runstream will be terminated in error. When the four intermediary
 *       output files are successfully cataloged and have their appropriate use names applied, xaoutFiles will
 *       exit normally.
 *
 *
 *
 * Program invocation/operation done after the JDBC XA Server
 * -------------------------------------------------------------------------------
 * The xaoutFiles program is invoked using the sequence (where # is replaced with
 * the number in the JDBC XA Server being executed, e.g., xajdbcsrvr#):
 *
 * @@*JDBC$ABS.XAOUTFILES,A #
 * ///262000   . (for retained PRINT$, LOG, TRACE files)
 * ///262000   . (for retained DIAG$ file)
 * CYCLES 32   . Number of file cycles to keep (from 1 to 32).
 *
 *
 * This xaoutFiles invocation does the following file related operations
 * programmatically (# is the XA Server number, <size-loc> is the file cataloging
 * information provided on the first two input images, and <n> is the nuumeric
 * value on the third cycles image):
 *
 * 1) Tests if the use name ABNORMAL is defined (if so, its placed on DIAG$ file):
 *
 *    A) Use name does not exist, the JDBC XA Server execution terminated normally; there is no need to
 *       save any DIAG$ file since the server terminated normally. The intermediary output files should
 *       be copied into a new set of the retained normal output files. So do the following steps programmatically:
 *
 *         1) @ASG,CP  *JDBC$XABRK#(+1).,<size-loc>  . Retained name for normal execution's breakpoint file (JDBC$BRK)
 *         2) @USE BRKPTFILE,*JDBC$XABRK#.           . JDBC$BRK copying happens later, so identify the file.
 *         3) @ASG,CP *JDBC$XALOG#(+1).,<size-loc>   . Retained name for normal execution's log file (JDBC$LOG)
 *         4) @ASG,CP *JDBC$XATRC#(+1).,<size-loc>   . Retained name for normal execution's trace file (JDBC$TRACE)

 *         5) @CYCLE *JDBC$XABRK#.,<n>               . Indicate number of cycles to save
 *         6) @CYCLE *JDBC$XALOG#.,<n>               . Indicate number of cycles to save
 *         7) @CYCLE *JDBC$XATRC#.,<n>               . Indicate number of cycles to save
 *
 *         8) @COPY  JDBC$LOG,*JDBC$XALOG#(+1).      . Retain previous JDBC$LOG file
 *         9) @COPY  JDBC$TRACE,*JDBC$XATRC#(+1).    . Retain previous JDBC$TRACE file
 *
 *        10) @FREE,R *JDBC$XATRC#(+1).              . Free retained trace file
 *        11) @FREE,R *JDBC$XALOG#(+1).              . Free retained log file
 *
 *        12) @DELETE,C *JXL#-<grunid>.              . Now delete intermediary file, not needed any more
 *        13) @DELETE,C *JXT#-<grunid>.              . Now delete intermediary file, not needed any more
 *        14) @DELETE,C *JXD#-<grunid>.              . Now delete intermediary file, not needed any more
 *
 *
 *       IMPORTANT: This sequence is slightly different than the one that may be done by the xaoutFiles
 *       before the JDBC XA Server execution because the retention copying, freeing, and deletion of the
 *       *JXB#-<grunid> file (JDBC$BRK) after the JDBC XA Server execution is delayed until the end of
 *       the actual runstream's work to maximize the breakpoint files contents. The runstream will perform
 *       the @COPY, @FREE, @DELETE operations explicitly from the *JXB#-<grunid> file to the *JDBC$XABRK#
 *       file using the BRKPTFILE use name. Also, since the JDBC XA Server terminated normally there is no
 *       reason to retain the DIAG$ file. So those steps related to retaining this file are not present or
 *       performed.
 *
 *       The above sequence works because step 1's @ASG,CP statement causes a hold on any other runstreams
 *       that attempt to catalog a new cycle of *JDBC$XABRK#; the hold remains until the runstream copies
 *       and releases the *JDBC$XABRK# file, at which time another runstream could perform the sequence.
 *       At that time, the three new cycles of *JDBC$XALOG, *JDBC$XATRC, and *JDBC$XABRK are again cataloged
 *       again with the same cycle numbers (which is VERY important).
 *
 *       After step 9, xaoutFiles no longer needs to have access to the two *JDBC$XA... files.
 *
 *       Once the saving/need of the three intermediary file set is completed, they're deleted in steps 12-14.
 *
 *       Should any of the above groups of four steps fail (e.g., could not catalog one of the retained files,
 *       or a copy operation fails) then the xaoutFiles program will terminate with an error indication (see
 *       code list below for code returned). If they do operate correctly, the xaoutFiles program exits normally.
 *
 *    B) Use name does exist. (The JDBC XA Server execution terminated in error, likely due to a IGDM or storage
 *       limits violation in the JDBC XA Server). The intermediary output files should be copied into a new set
 *       of the retained abnormal output files. So do the following steps programmatically:
 *
 *         1) @ASG,CP *JDBC$XDBRK#(+1).,<size-loc>   . Retained name for abnormal execution's breakpoint file (JDBC$BRK)
 *         2) @USE BRKPTFILE,*JDBC$XDBRK#.           . JDBC$BRK copying happens later, so identify the file.
 *         3) @ASG,CP *JDBC$XDLOG#(+1).,<size-loc>   . Retained name for abnormal execution's log file (JDBC$LOG)
 *         4) @ASG,CP *JDBC$XDTRC#(+1).,<size-loc>   . Retained name for abnormal execution's trace file (JDBC$TRACE)
 *         5) @ASG,CP *JDBC$XDIAG#(+1).,<size-loc>   . Retained name for abnormal execution's diag$ file (JDBC$DIAG)

 *         6) @CYCLE *JDBC$XDBRK#.,<n>               . Indicate number of cycles to save
 *         7) @CYCLE *JDBC$XDLOG#.,<n>               . Indicate number of cycles to save
 *         8) @CYCLE *JDBC$XDTRC#.,<n>               . Indicate number of cycles to save
 *         9) @CYCLE *JDBC$XDIAG#.,<n>               . Indicate number of cycles to save
 *
 *        10) @COPY  *JXL#-<grunid>.,*JDBC$XDLOG#(+1).  . Retain previous JDBC$LOG file
 *        11) @COPY  *JXT#-<grunid>.,*JDBC$XDTRC#(+1).  . Retain previous JDBC$TRACE file
 *        12) @COPY  *JXD#-<grunid>.,*JDBC$XDIAG#(+1).  . Retain previous JDBC$DIAG file
 *
 *        13) @FREE,R *JDBC$XDTRC#(+1).              . Free retained trace file
 *        14) @FREE,R *JDBC$XDIAG#(+1).              . Free retained diag$ file
 *        15) @FREE,R *JDBC$XDLOG#(+1).              . Free retained log file
 *
 *        16) @DELETE,C *JXL#-<grunid>.              . Now delete intermediary file, not needed any more
 *        17) @DELETE,C *JXT#-<grunid>.              . Now delete intermediary file, not needed any more
 *        18) @DELETE,C *JXD#-<grunid>.              . Now delete intermediary file, not needed any more
 *
 *
 *       IMPORTANT: This sequence is slightly different than the one that may be done by the xaoutFiles
 *       before the JDBC XA Server execution because the retention copying, freeing, and deletion of the
 *       *JXB#-<grunid> file (JDBC$BRK) at this time (after the JDBC XA Server execution) is delayed until
 *       the end of the actual runstream's work to maximize the breakpoint files contents. The runstream
 *       will perform the @COPY, @FREE, @DELETE operations explicitly from the *JXB#-<grunid> file to the
 *       *JDBC$XDBRK# file using the BRKPTFILE use name.
 *
 *       The above sequence works because step 1's @ASG,CP statement causes a hold on any other runstreams
 *       that attempt to catalog a new cycle of *JDBC$XDBRK#; the hold remains until the runstream copies
 *       and releases the *JDBC$XDBRK# file, at which time another runstream could perform the sequence.
 *       At that time, the three new cycles of *JDBC$XDLOG, *JDBC$XDTRC, and *JDBC$XDBRK are again cataloged
 *       again with the same cycle numbers (which is VERY important).
 *
 *       After step 12, there is no more xaoutFiles need to have access to the four *JDBC$XD... files.
 *
 *       Once the saving of the three intermediary file set is completed, they're deleted in steps 16-18.
 *
 *       Should any of the above groups of four steps fail (e.g., could not catalog one of the retained files,
 *       or a copy operation fails) then the xaoutFiles program will terminate with an error indication (see
 *       code list below for code returned). If they do operate correctly, the xaoutFiles program exits normally.
 *
 * Program xaoutFiles error termination codes:
 *
 * Error code  Reason ("set" means one or more of the type of files mentioned)
 * ----------  --------------------------------------------------------------------------------------------
 *      0      Normal program termination. it did what it was supposed to do.
 *      1      Bad input argument (option or catalog size/loc specification, or cycles number).
 *      2      Could not catalog the full set of intermediary output files using input size/loc information.
 *      3      Could not  successfully delete the intermediary files using PCFP.
 *      4      Could not catalog the set of retained output files (either normal or abnormal set).
 *      5      Could not apply the cycle maximum value to the set of retained output files.
 *      6      Could not successfully copy the set of intermediary files to the retained files.
 *      7      Could not free the retained set of output files.
 *
 */
main(){
    int argc;
    char **argv;
    int status = 0;      /* Initially assume no error occurs. */
    char maxFileCyclesChars[IMAGE_MAX_CHARS];
    char xaServerNumberChars[5];
    char cyclesKeyword[IMAGE_MAX_CHARS];
    char tempBuffer[IMAGE_MAX_CHARS];
    char * igdmptr;

    /* Clear fields before usage. */
    debug = 0;        /* Assume production mode - initially assume debug is off. */
    igdmFlag = 0;     /* Assume production mode - initially assume IGDM testing is off. */
    option_bits = 0;
    optionLetters[0] = '\0';
    xaServerNumber = 0;
    cataloging_size_loc[0] = '\0';
    diag_cataloging_size_loc[0] = '\0';
    cyclesKeyword[0] = '\0';
    maxFileCycles = 0;
    runid[0] = '\0';
    grunid[0] = '\0';

    /* get command line arguments */
    _cmd_line(&argc,&argv,&option_bits,0);

    /* Test if we are in debugging mode and/or IGDM testing mode. */
    if (option_bits & ((1<<('Z'-'D')))){
        /*
         * Yes, in a diagnostic mode. What type of debugging/testing are we doing?
         *
         * Do we want an immediate IGDM right now?
         */
        if (option_bits & ((1<<('Z'-'N')))){
            /* Yes, IGDM testing mode - force an IGDM right now.*/
            igdmptr = 0;
            *igdmptr = 'a'; /* This will cause a failure and IGDM */
        }

        /* Are we doing a IGDM test at an of successful execution? */
        if (option_bits & ((1<<('Z'-'I')))){
            /* Yes, we want a IGDM at the end of successful execution. Set indicator flag. */
            igdmFlag = 1;

            /*  Clear the I option bit setting. */
            option_bits = option_bits ^ ((1<<('Z'-'I')));
        }

        if (option_bits & ((1<<('Z'-'T')))){
            /* Diagnostic debugging mode, just set debug to true (1). */
            debug = 1;

            /*  Clear the T option bit setting. */
            option_bits = option_bits ^ ((1<<('Z'-'T')));
        }

        /*  Clear the D option bit setting. */
        option_bits = option_bits ^ ((1<<('Z'-'D')));
    }

    /* If we are debugging, display the program start up parameters. */
    if (debug){
        getOptionLetters(optionLetters, option_bits);
        printf("Starting xaoutFiles: argc = %d, option_bits = %o12, optionLetters = '%s'\n", argc, option_bits, optionLetters);
    }

    /* Obtain the JDBC XA Server number. Produce an error if there are wrong number of parameters. */
    if (argc ==2){
       strncpy(xaServerNumberChars, argv[1], 5);
       xaServerNumber = atoi(xaServerNumberChars);

       if (debug){
           printf("Starting xaoutFiles: xaServerNumber = %d, xaServerNumberChars = '%s'\n", xaServerNumber, xaServerNumberChars);
       }

       /* Is the JDBC XA Server number in the legal range? */
       if (xaServerNumber > 16 || xaServerNumber < 1){
           printf("Program xaoutFiles given an invalid JDBC XA Server number %s\n", xaServerNumberChars);
           exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
       }
    } else {
        printf("Program xaoutFiles has the wrong number of invocation parameters (%d); only required parameter is JDBC XA Server number\n",(argc-1));
        exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
    }

    /* Obtain the information from the three images that follow the xaoutFiles invocation in the runstream. */
    if (fscanf(stdin, "%s\0", cataloging_size_loc) != 1){
        /* We couldn't read the breakpoint, log, trace file cataloging size, location information. Produce an error message and exit. */

        if (debug){
            printf("Starting xaoutFiles: missing or bad breakpoint, log, trace file cataloging size, location information image\n");
        }

        printf(message, "Program xaoutFiles could not read the first line with cataloging information: %s\n", cataloging_size_loc);
        exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
    }

    if (debug){
        printf("Starting xaoutFiles: the breakpoint, log, trace file cataloging size, location information image = '%s'\n", cataloging_size_loc);
    }

    if (fscanf(stdin, "%s\0", diag_cataloging_size_loc) != 1){
        /* We couldn't read the DIAG$ file cataloging size, location information. Produce an error message and exit. */

        if (debug){
            printf("Starting xaoutFiles: missing or bad diag$ cataloging size, location information image\n");
        }

        printf(message, "Program xaoutFiles could not read the second line with diag$ cataloging information: %s\n", diag_cataloging_size_loc);
        exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
    }

    if (debug){
        printf("Starting xaoutFiles: the diag$ cataloging size, location information image = '%s'\n", diag_cataloging_size_loc);
    }

    /* Get CYCLES information. Allow Cycles keyword to be mixed case. */
    if (fscanf(stdin, "%s %s\0", cyclesKeyword, maxFileCyclesChars) != 2 || strcmp(toUpperCase(cyclesKeyword, tempBuffer), CYCLES_UPPERCASE) != 0 ){
        /* We couldn't read the cycles number image. Produce an error message and exit. */

        if (debug){
            printf("Starting xaoutFiles: missing or bad CYCLES information image = %s %s \n", cyclesKeyword, maxFileCyclesChars);
        }

        sprintf(message, "Program xaoutFiles could not read the third line with CYCLES information: %s %s\n", cyclesKeyword, maxFileCyclesChars);
        exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
    }

    if (debug){
        printf("Starting xaoutFiles: the CYCLES information image = '%s %s'\n", cyclesKeyword, maxFileCyclesChars);
    }

    /* Is the cycles maximum in the legal range? */
    maxFileCycles = atoi(maxFileCyclesChars);
    if (maxFileCycles > 32 || maxFileCycles < 1){
        printf("Program xaoutFiles given an invalid maximum file cycles number of %s\n", maxFileCyclesChars);
        exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
    }

    /* Get the original runid and the generated runid. */
    getRunIDs(runid, grunid);

    if (debug){
        printf("Starting xaoutFiles: runid = %s, grunid = %s\n", runid, grunid);
    }

    /* Determine if this execution is before or after the JDBC XA Server execution. */
    if (option_bits & ((1<<('Z'-'B'))) && (option_bits & (1<<('Z'-'A')))){
        /* The options B and A cannot be specified together. Produce an error message and exit. */
        printf("Program xaoutFiles invocation must specify either an A or B option, but not both.\n");
        exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
    }

    /* Construct the intermediary file names: JXB#-<grunid>, JXL#-<grunid>, JXT#-<grunid>, JXD#-<grunid>  */
    sprintf(intermediaryBrkFileName,  "%s%d-%s", JXBNAME_PART1, xaServerNumber, grunid); /* For intermediary breakpoint file*/
    sprintf(intermediaryLogFileName,  "%s%d-%s", JXLNAME_PART1, xaServerNumber, grunid); /* For intermediary log file*/
    sprintf(intermediaryTrcFileName,  "%s%d-%s", JXTNAME_PART1, xaServerNumber, grunid); /* For intermediary trace file*/
    sprintf(intermediaryDiagFileName, "%s%d-%s", JXDNAME_PART1, xaServerNumber, grunid); /* For intermediary diag$ file*/

    if (debug){
        printf("intermediaryBrkFileName = %s\n", intermediaryBrkFileName);
        printf("intermediaryLogFileName = %s\n", intermediaryLogFileName);
        printf("intermediaryTrcBrkFileName = %s\n", intermediaryTrcFileName);
        printf("intermediaryDiagFileName = %s\n", intermediaryDiagFileName);
    }

    if (option_bits & (1<<('Z'-'B'))){
        /* This is the program invocation before the JDBC XA Server invocation. */

        if (debug){
            printf("Just before option_B_invocation() call\n");
        }

        status = option_B_invocation();
        if (status != NORMAL_EXECUTION_NO_ERRORS){
            printf("Program xaoutFiles has detected an error situation, exiting with status code %d\n", status);
            exit(status);
        }

    } else if (option_bits & (1<<('Z'-'A'))){
        /* This is the program invocation after the JDBC XA Server invocation. */

        if (debug){
            printf("Just before option_A_invocation() call\n");
        }

        status = option_A_invocation();
        if (status != NORMAL_EXECUTION_NO_ERRORS){
            printf("Program xaoutFiles has detected an error situation, exiting with status code %d\n", status);
            exit(status);
        }
    } else {

        /* The option letter(s), if any, on the xaoutFiles invocation are not allowed. */
        getOptionLetters(optionLetters, option_bits);

        if (strlen(optionLetters) == 0){
            /* There were no option letters specified. There must be an B or an A. Produce an error message. */
            printf("Program xaoutFiles invocation must specify either a B option or an A option.\n");
        } else {
            /* Produce an error message. */
            printf("Program xaoutFiles invocation does not allow these options: %s\n", optionLetters);
        }

        /* Exit with error code. */
        exit(OPTION_LETTER_ERROR_OR_CATALOG_OR_CYCLING_INFO_ERROR);
    }

    /* Program completed its work successfully. Are we supposed to force an IGDM now? */
    if (igdmFlag){
        /* Yes, force an IGDM. */
        igdmptr = 0;
        *igdmptr = 'a'; /* This will cause a failure and IGDM */
    }

    /* Normal execution and a normal return. Exit and set condition word. */
    return(NORMAL_EXECUTION_NO_ERRORS);
}

/*
 * Function option_B_invocation
 *
 * This function is executed before the JDBC XA Server invocation.
 * It typically just catalogs the required intermediary set of
 * output files, places the desired use names on them and returns
 * normally. If in this process it detects that file *JXB#-<grunid>
 * already exists, then it attempts to save the earlier set of
 * intermediary files in the retained abnormal set of output files
 * before providing the required intermediary set of files.
 *
 * Returned value:
 *  NORMAL_EXECUTION_NO_ERRORS (0)
 *  COULD_NOT_CATALOG_INTERMEDIARY_FILES (1)
 *  COULD_NOT_CATALOG_RETAINED_FILES_OR_COPY_TO_THEM_OR_FREE_IT (2)
 *
 */
int option_B_invocation(){
    
    int status = 0;
    int overallStatus = NORMAL_EXECUTION_NO_ERRORS; /* Assume all operations will succeed. */
    char acsfString[ACFS_IMAGE_MAX_CHARS];

    /* Test if intermediary file *JXB#-<grunid> already exists by trying to assign it to the run. */
    sprintf(acsfString, "@ASG,AXZ *%s. . ", intermediaryBrkFileName);
    status = facsf(acsfString);

    if (debug){
        printf("In option_B_invocation(): '%s' got status = %o\n", acsfString, status);
    }

    if (status >= 0){
        /*
         * File *JXB#-<grunid> already exists, and likely the other intermediary
         * files as well from an earlier previous runstream execution.
         *
         * So construct the retained abnormal output file names: JDBC$XDBRK#, JDBC$XDLOG#, JDBC$XDTRC#, JDBC$XDIAG#
         */
        sprintf(retainedBrkFileName,  "%s%d", XDBRKNAME_PART1, xaServerNumber); /* For retained abnormal breakpoint file*/
        sprintf(retainedLogFileName,  "%s%d", XDLOGNAME_PART1, xaServerNumber); /* For retained abnormal log file*/
        sprintf(retainedTrcFileName,  "%s%d", XDTRCNAME_PART1, xaServerNumber); /* For retained abnormal trace file*/
        sprintf(retainedDiagFileName, "%s%d", XDIAGNAME_PART1, xaServerNumber); /* For retained abnormal diag$ file*/

        if (debug){
            printf("retainedBrkFileName = %s\n", retainedBrkFileName);
            printf("retainedLogFileName = %s\n", retainedLogFileName);
            printf("retainedTrcFileName = %s\n", retainedTrcFileName);
            printf("retainedDiagFileName = %s\n", retainedDiagFileName);
        }
        /*
         * Now catalog the new set of retained abnormal output files. Cataloging the
         * retained abnormal breakpoint file first using @ASG,CP will make any other
         * runstream trying to catalog its own set of files wait until this runstream
         * has copied and freed its retained abnormal breakpoint file.
         */
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedBrkFileName, cataloging_size_loc, NULL, "retained abnormal breakpoint");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedLogFileName, cataloging_size_loc, NULL, "retained abnormal log");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedTrcFileName, cataloging_size_loc, NULL, "retained abnormal trace");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedDiagFileName, diag_cataloging_size_loc, NULL, "retained abnormal diag$");

        /* Were we successful at getting a new set of retained files to work with? If so,
         * save the existing intermediary output files into the retained abnormal output set.
         */
        if (overallStatus == 0){
            /* Yes, we cataloged the retained set. Copy previous intermediary files to them.*/
            copyToRetainedFile(&overallStatus, intermediaryBrkFileName, retainedBrkFileName, "previous intermediary breakpoint");
            copyToRetainedFile(&overallStatus, intermediaryLogFileName, retainedLogFileName, "previous intermediary log");
            copyToRetainedFile(&overallStatus, intermediaryTrcFileName, retainedTrcFileName, "previous intermediary trc");
            copyToRetainedFile(&overallStatus, intermediaryDiagFileName, retainedDiagFileName, "previous intermediary diag$");
        } else {
            /*
             * No, then leave the intermediary files and any retained files
             * copied alone and exit the program in error.
             */
            exit(overallStatus);
        }

        /*
         * Did all the copies work? If so, then free the retained abnormal
         * output files. Free breakpoint file last (see notes above).
         */
        if (overallStatus == 0){
            /* Yes, now free all the retained files now. */
            freeCP_File(&overallStatus, retainedLogFileName, "retained abnormal log");
            freeCP_File(&overallStatus, retainedTrcFileName, "retained abnormal trc");
            freeCP_File(&overallStatus, retainedDiagFileName, "retained abnormal diag$");
            freeCP_File(&overallStatus, retainedBrkFileName, "retained abnormal breakpoint");
        } else {
            /* No, then leave the retained files alone and exit the program in error. */
            exit(overallStatus);
        }

        /*
         * Did all the copies and free's work? If so, then delete the intermediary files
         * since we have successfully saved them.
         */
        if (overallStatus == 0){
            /* Yes, now delete all the current intermediary output files here; they will be cleanly recreated later */
            deleteFile(&overallStatus, intermediaryBrkFileName, "intermediary breakpoint");
            deleteFile(&overallStatus, intermediaryLogFileName, "intermediary log");
            deleteFile(&overallStatus, intermediaryTrcFileName, "intermediary trc");
            deleteFile(&overallStatus, intermediaryDiagFileName, "intermediary diag$");
        } else {
            /* No, then leave the intermediary files alone and exit the program in error. */
            exit(overallStatus);
        }
    }

    /*
     * File *JXB#-<grunid> didn't exist. Or it did and we were able to successfully retain
     * the previous set of intermediary files into a retained set of abnormal output files
     *  and then we were able to delete them (so there would be no "residual" left over from the
     * previous runstream execution).
     *
     * So catalog a new set of intermediary output files and place use names on them.
     */
    catalogFile_PlaceUsename(&overallStatus, intermediaryBrkFileName, cataloging_size_loc, JDBCBRK_USENAME, "intermediary breakpoint");
    catalogFile_PlaceUsename(&overallStatus, intermediaryLogFileName, cataloging_size_loc, JDBCLOG_USENAME, "intermediary log");
    catalogFile_PlaceUsename(&overallStatus, intermediaryTrcFileName, cataloging_size_loc, JDBCTRC_USENAME, "intermediary trace");
    catalogFile_PlaceUsename(&overallStatus, intermediaryDiagFileName, diag_cataloging_size_loc, JDBCDIAG_USENAME, "intermediary diag$");

    /* Return the overall status for the function. */
    return overallStatus;
}

/*
 * Function option_A_invocation
 *
 * This function is executed after the JDBC XA Server invocation.
 * It just catalogs the required set of retained output files
 * (normal or abnormal), copies the set of intermediary output
 * files into them and then deletes the set of intermediary files.
 * The handling of the intermediary output breakpoint file is done
 * after this program exits.
 * This function detects an abnormal JDBC XA Server execution by
 * checking for the use name ABNORM placed on the DIAG$ file.
 *
 * Returned value:
 *  NORMAL_EXECUTION_NO_ERRORS (0)
 *  COULD_NOT_CATALOG_RETAINED_FILES_OR_COPY_TO_THEM_OR_FREE_IT (2)
 */
int option_A_invocation(){

    int status = 0;
    int overallStatus = NORMAL_EXECUTION_NO_ERRORS; /* Assume all operations will succeed. */
    char acsfString[ACFS_IMAGE_MAX_CHARS];

    /* Test if the use name ABNORM is present by attempting to just release the use name. */
    sprintf(acsfString, "@FREE,A ABNORM. . ");
    status = facsf(acsfString);

    if (debug){
        printf("Free on ABNORM got status = %o\n", status);
    }
    /*
     * We need to shift the status value logically one bit to the right for testing because:
     *   1) If the use name was not present, a status of 000000000000 is returned.
     *   2) If the use name was present, a status of 100000000000 is returned.
     *   3) Both 1000000000000 and 0000000000000 compare equal to 0.
     * So we need to move the sign bit a little to the right to "see" it in the test,
     */
    if (!((status >> 1) == 0)){
        /* The use name ABNORM was not present, so we have a normal JDBC XA Server termination. */

        if (debug){
            printf("Use name ABNORM not present, normal JDBC XA Server termination.\n");
        }

        /*
         * So construct the retained normal output file names: JDBC$XABRK#, JDBC$XALOG#, JDBC$XATRC#
         * Remember we don't need to save the DIAG$ file on a normal termination.
         */
        sprintf(retainedBrkFileName,  "%s%d", XABRKNAME_PART1, xaServerNumber); /* For retained normal breakpoint file*/
        sprintf(retainedLogFileName,  "%s%d", XALOGNAME_PART1, xaServerNumber); /* For retained normal log file*/
        sprintf(retainedTrcFileName,  "%s%d", XATRCNAME_PART1, xaServerNumber); /* For retained normal trace file*/

        if (debug){
            printf("retainedBrkFileName = %s\n", retainedBrkFileName);
            printf("retainedLogFileName = %s\n", retainedLogFileName);
            printf("retainedTrcFileName = %s\n", retainedTrcFileName);
        }

        /*
         * Now catalog the set of retained normal output file. Cataloging the
         * retained normal breakpoint file first using @ASG,CP will make any other
         * runstream trying to catalog its own set of files wait until this runstream
         * has copied and freed its retained normal breakpoint file. Place the use name
         * BRKPTFILE on the breakpoint file so it can be copied to later in the runstream.
         */
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedBrkFileName, cataloging_size_loc, "BRKPTFILE", "retained normal breakpoint");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedLogFileName, cataloging_size_loc, NULL, "retained normal log");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedTrcFileName, cataloging_size_loc, NULL, "retained normal trace");

        /* Were we successful at getting a new set of retained files to work with? If so,
         * save the existing intermediary output files into the retained normal output set.
         */
        if (overallStatus == 0){
             /* Yes, we cataloged the retained set. Copy previous intermediary log and trace files to them.*/
            copyToRetainedFile(&overallStatus, intermediaryLogFileName, retainedLogFileName, "previous intermediary log");
            copyToRetainedFile(&overallStatus, intermediaryTrcFileName, retainedTrcFileName, "previous intermediary trc");
        } else {
            /*
             * No, then leave the intermediary files and any retained files
             * copied alone and exit the program in error.
             */
            exit(overallStatus);
        }

        /*
         * Did all the copies work? If so, then free the retained
         * files except the breakpoint file (done later).
         */
        if (overallStatus == 0){
        /* Yes, now free all the retained files now. */
            freeCP_File(&overallStatus, retainedLogFileName, "retained normal log");
            freeCP_File(&overallStatus, retainedTrcFileName, "retained normal trc");
        } else {
            /* No, then leave the retained files alone and exit the program in error. */
            exit(overallStatus);
        }

    } else {
        /*
         * The use name ABNORM was present, so we have an abnormal JDBC XA Server termination.
         */

        if (debug){
            printf("Use name ABNORM was present, abnormal JDBC XA Server termination.\n");
        }

        /*
         * So construct the retained abnormal output file names: JDBC$XDBRK#, JDBC$XDLOG#, JDBC$XDTRC#, JDBC$XDIAG#
         */
        sprintf(retainedBrkFileName,  "%s%d", XDBRKNAME_PART1, xaServerNumber); /* For retained abnormal breakpoint file*/
        sprintf(retainedLogFileName,  "%s%d", XDLOGNAME_PART1, xaServerNumber); /* For retained abnormal log file*/
        sprintf(retainedTrcFileName,  "%s%d", XDTRCNAME_PART1, xaServerNumber); /* For retained abnormal trace file*/
        sprintf(retainedDiagFileName, "%s%d", XDIAGNAME_PART1, xaServerNumber); /* For retained abnormal diag$ file*/

        if (debug){
            printf("retainedBrkFileName = %s\n", retainedBrkFileName);
            printf("retainedLogFileName = %s\n", retainedLogFileName);
            printf("retainedTrcFileName = %s\n", retainedTrcFileName);
            printf("retainedDiagFileName = %s\n", retainedDiagFileName);
        }

        /*
         * Now catalog the set of retained abnormal output file. Cataloging the
         * retained abnormal breakpoint file first using @ASG,CP will make any other
         * runstream trying to catalog its own set of files wait until this runstream
         * has copied and freed its retained abnormal breakpoint file. Place the use name
         * BRKPTFILE on the breakpoint file so it can be copied to later in the runstream.
         */
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedBrkFileName, cataloging_size_loc, "BRKPTFILE", "retained abnormal breakpoint");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedLogFileName, cataloging_size_loc, NULL, "retained abnormal log");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedTrcFileName, cataloging_size_loc, NULL, "retained abnormal trace");
        assignCP_retainedFile_PlaceUsename(&overallStatus, retainedDiagFileName, diag_cataloging_size_loc, NULL, "retained abnormal diag$");

        /*
         * Were we successful at getting a new set of retained files to work with? If so,
         * save the existing intermediary output files into the retained abnormal output set.
         */
        if (overallStatus == 0){
            /* Yes, we cataloged the retained set. Copy previous intermediary files to them.*/
            copyToRetainedFile(&overallStatus, intermediaryLogFileName, retainedLogFileName, "previous intermediary log");
            copyToRetainedFile(&overallStatus, intermediaryTrcFileName, retainedTrcFileName, "previous intermediary trc");
            copyToRetainedFile(&overallStatus, intermediaryDiagFileName, retainedDiagFileName, "previous intermediary diag$");
        } else {
            /*
             * No, then leave the intermediary files and any retained files
             * copied alone and exit the program in error.
             */
            exit(overallStatus);
        }

        /*
         * Did all the copies work? If so, then free the retained
         * files except the breakpoint file (done later).
         */
        if (overallStatus == 0){
            /* Yes, now free all the retained files now. */
            freeCP_File(&overallStatus, retainedLogFileName, "retained abnormal log");
            freeCP_File(&overallStatus, retainedTrcFileName, "retained abnormal trc");
            freeCP_File(&overallStatus, retainedDiagFileName, "retained abnormal diag$");
        } else {
            /* No, then leave the retained files alone and exit the program in error. */
            exit(overallStatus);
        }
    }

    /*
     * Did all the copies and free's work? If so, then delete the intermediary files except
     * the breakpoint file (done later), since we have successfully saved them.
     */
    if (overallStatus == 0){
        /* Yes, now delete all the current intermediary output files here; they will be cleanly recreated later */
        deleteFile(&overallStatus, intermediaryLogFileName, "intermediary log");
        deleteFile(&overallStatus, intermediaryTrcFileName, "intermediary trc");
        deleteFile(&overallStatus, intermediaryDiagFileName, "intermediary diag$");
    } else {
        /* No, then leave the intermediary files alone and exit the program in error. */
        exit(overallStatus);
    }

    /* Return the overall status for the function. */
    return overallStatus;

}

/*
 * Procedure catalogFile_PlaceUsename
 *
 * This procedure catalogs an intermediary output file, using @CAT,P a 2200 file,
 * and optionally places a use name on that file.
 *
 * Parameters:
 * overallStatus       - contains the accumulated overall status value. If
 *                       the catalog/use operations work without error, then
 *                       overallStatus is left untouched, otherwise it is set
 *                       to an error status value.
 * filename            - a filename string (e.g., "*JXB11-jtest") to be cataloged
 *                       using an @CAT,P.
 * cataloging_size_loc - the cataloging size and location information (e.g.,"///262000")
 * usename             - The use name to place on the filename once its cataloged. If
 *                       NULL, adding of a use name is not desired.
 * error_insert        - an insert string to use in the printed error message if
 *                       an error occurs on either the catalog or use process.
 */
void catalogFile_PlaceUsename(int * overallStatus, char * filename, char * cataloging_size_loc, char * usename, char * error_insert){

    int status = 0;
    char acsfString[ACFS_IMAGE_MAX_CHARS];

    /* Now catalog the intermediary files. */
    sprintf(acsfString, "@CAT,P *%s.,%s . ", filename, cataloging_size_loc);
    status = facsf(acsfString);

    if (debug){
        printf("In catalogFile_PlaceUsename(): '%s' got status = %o\n", acsfString, status);
    }

    if (status < 0){
        *overallStatus = COULD_NOT_CATALOG_INTERMEDIARY_FILES;
        printf("Can't catalog %s file %s using %s  got status %o\n", error_insert, filename, acsfString, status);
    } else {
        /* Catalog worked, now put use name on it. */
        sprintf(acsfString, "@USE %s,*%s. . ", usename, filename);
        status = facsf(acsfString);

        if (debug){
            printf("In catalogFile_PlaceUsename(): '%s' got status = %o\n", acsfString, status);
        }

        if (status < 0){
            *overallStatus = COULD_NOT_CATALOG_INTERMEDIARY_FILES;
            printf("Can't add USE name %s on %s file %s using %s  got status %o\n", usename, error_insert, filename, acsfString, status);
        }
    }
}

/*
 * Procedure assignCP_retainedFile_PlaceUsename
 *
 * This procedure catalogs using @ASG,CP a retained output 2200 file,
 * sets its maximum allowed number of cycles, and optionally places a
 * use name on the file.
 *
 * Parameters:
 * overallStatus       - contains the accumulated overall status value. If
 *                       the catalog/use operations work without error, then
 *                       overallStatus is left untouched, otherwise it is set
 *                       to an error status value.
 * filename            - a filename string (e.g., "*JXB11-jtest") to be cataloged
 *                       using an @ASG,CP.
 * cataloging_size_loc - the cataloging size and location information (e.g.,"///262000")
 * usename             - The use name to place on the filename once its cataloged. If
  *                      NULL, adding of a use name is not desired.
 * error_insert        - an insert string to use in the printed error message if
 *                       an error occurs on either the catalog or use process.
 */
 void assignCP_retainedFile_PlaceUsename(int * overallStatus, char * filename, char * cataloging_size_loc, char * usename, char * error_insert){
    int status = 0;
    char acsfString[ACFS_IMAGE_MAX_CHARS];

    /* Now catalog the desired file. */
    sprintf(acsfString, "@ASG,CP *%s(+1).,%s . ", filename, cataloging_size_loc);
    status = facsf(acsfString);

    if (debug){
        printf("In assignCP_retainedFile_PlaceUsename(): '%s' got status = %o\n", acsfString, status);
    }

    /* Test the status to see if we've got the file cataloged and assigned. */
    if (status < 0){
        /* The @ASG,CP failed. */
        *overallStatus = COULD_NOT_CATALOG_RETAINED_FILES;
        printf("Can't assign and catalog %s file %s using %s  got status %o\n", error_insert, filename, acsfString, status);
    } else {
        /*
         * The @ASG,CP worked, so indicate the maximum number of allowed cycles for this file.
         * Use the maxFileCycles value provided on the program invocations third input image.
         */
        status = set_file_maxcycles(TWELVE_BLANKS, filename, maxFileCycles);
        if (status < 0){
            *overallStatus = COULD_NOT_CYCLE_RETAINED_FILES;
            printf("Can't set maximum cycles of %d for %s file %s  got status %o\n", maxFileCycles, error_insert, filename, status);
        } else {
            /*
             * We got the file created and the maximum file cycles set
             * properly, so check if a use name should be put on the file.
             */
            if (usename != NULL){
                /* Put use name on the file. */
                sprintf(acsfString, "@USE %s,*%s(+1). . ", usename, filename);
                status = facsf(acsfString);

                if (debug){
                    printf("In assignCP_retainedFile_PlaceUsename(): '%s' got status = %o\n", acsfString, status);
                }

                if (status < 0){
                    *overallStatus = COULD_NOT_CATALOG_RETAINED_FILES;
                    printf("Can't add USE name %s on %s file %s using %s  got status %o\n", usename, error_insert, filename, acsfString, status);
                }
            }
        }
    }
}

 /*
  * Procedure copyToRetainedFile
  *
  * This procedure copies an intermediary output file to the specified retained
  * output file. This procedure calls copy_file() to do the actual file copying.
  *
  * Parameters:
  * overallStatus       - contains the accumulated overall status value. If
  *                       the copy operation works without error, then
  *                       overallStatus is left untouched, otherwise it is set
  *                       to an error status value.
  * filename            - the intermediary output filename (e.g., "*JXLOG11-JTEST")
  *                       that is to be copied.
  * filename            - the retained output filename (e.g., "*JDBC$XALOG11")
  *                       that will recieve the copy.
  * error_insert        - an insert string to use in the printed error message if
  *                       an error occurs on the copying process.
  */
 void copyToRetainedFile(int * overallStatus, char * intermediaryFilename, char * retainedFilename, char * error_insert){

     int status;

     /* Now copy the desired files. */
     status = copy_file(TWELVE_BLANKS, intermediaryFilename, 1, TWELVE_BLANKS, retainedFilename, 1);

     /* Test for a error in the copy attempt. */
     if (status != 0) {
         /* Got an error status indicating a failure happened, return error code. */
         *overallStatus = COULD_NOT_COPY_INTERMEDIARY_TO_RETAINED_FILES;
         printf("Can't copy %s file %s to %s status(0%o), try increasing cataloging size specification.\n", error_insert, intermediaryFilename, retainedFilename, status);
         return;
     }
 }

 /*
  * Function copy_file
  *
  * This is a slightly modified version of the C- program file copy
  * example in the Programmable FURPUR (PCFP) PRM. The only changes are
  * in the definitions and usage of the from_cycle and dest_cycle values.
  *
  * Parameters:
  * from_qualifier - qualifier of from file
  * from_filename  - filename of from file
  * from_cycle     - absolute cycle number of the from file  (e.g., the
  *                  file is currently assigned using an @ASG,AZ filename1(1). )
  * dest_qualifier - qualifier of destination file
  * dest_filename  - filename of destination file
  * dest_cycle     - relative cycle number of the destination file (e.g., the
  *                  file is currently assigned using an @ASG,CP filename2(+1). )
  *
  * Returned value:
  * status         - 0 indicates successful copy operation. Non-0 indicates
  *                  a failed copy operation (as indicated by the PCFP code)
  */
 int copy_file(char * from_qualifier, char * from_filename, int from_cycle,
               char * dest_qualifier, char * dest_filename, int dest_cycle  ){

     /* Define the function packet */
     fp_copy_raf_raf_type cpy_pkt;

     /* Define the file identifier structures. */
     fp_file_id_type source_file_id;
     fp_file_id_type dest_file_id;

     /* Zero-fill the function packet. */
     memset (&cpy_pkt, 0, sizeof (fp_copy_raf_raf_type));

     /* Set items in the function packet. */
     cpy_pkt.gen.interface_level = fp_interface_level_1;
     cpy_pkt.gen.null_term_strings = TRUE;
     cpy_pkt.gen.work_area_size = fp_max_work_area_copy_raf_raf;

     /* Zero-fill the source file identifier. */
     memset (&source_file_id, 0, sizeof (fp_file_id_type));

     /* Set items in the source file id. */
     source_file_id.interface_level = fp_interface_level_1;
     strncpy (source_file_id.qualifier, from_qualifier, sizeof (source_file_id.qualifier));
     strncpy (source_file_id.filename, from_filename, sizeof (source_file_id.filename));
     source_file_id.f_cycle_type = fp_cycle_abs;
     source_file_id.f_cycle = from_cycle;

     /* Zero-fill the destination file identifier. */
     memset (&dest_file_id, 0, sizeof (fp_file_id_type));

     /* Set items in the destination file id. */
     dest_file_id.interface_level = fp_interface_level_1;
     strncpy (dest_file_id.qualifier, dest_qualifier, sizeof (dest_file_id.qualifier));
     strncpy (dest_file_id.filename, dest_filename, sizeof (dest_file_id.filename));
     dest_file_id.f_cycle_type = fp_cycle_rel;
     dest_file_id.f_cycle = dest_cycle;

     /* Do the file copy now. */
     _fp_copy_raf_raf (&cpy_pkt, &source_file_id, &dest_file_id, NULL);
     if ( cpy_pkt.gen.error_class != fp_error_class_none ) {

         /* Got an error status indicating a failure happened, return error code. */

         if (debug){
             printf("In copy_file(): PCFP _fp_copy_raf_raf(%s, %s) failed, received statuses (%o, %o)\n",
                     from_filename, dest_filename, cpy_pkt.gen.error_code, cpy_pkt.gen.error_code % 01000000);
         }

         return cpy_pkt.gen.error_code % 01000000;
     } else {

         if (debug){
             printf("In copy_file(): PCFP _fp_copy_raf_raf(%s, %s) was successful\n", from_filename, dest_filename);
         }

         /* Got a successful error status indicating the file was copied, return success code. */
         return 0;
     }
 }

/*
 * Procedure deleteFile
 *
 * This procedure deletes an 2200 intermediary output file. This procedure calls
 * delete_file() to do the actual file deletion.
 *
 * Parameters:
 * overallStatus       - contains the accumulated overall status value. If
 *                       the delete operation works without error, then
 *                       overallStatus is left untouched, otherwise it is set
 *                       to an error status value.
 * filename            - a filename string (e.g., "*JXLOG11-JTEST") to be deleted.
 * error_insert        - an insert string to use in the printed error message if
 *                       an error occurs on the deletion process.
 */
void deleteFile(int * overallStatus, char * filename, char * error_insert){

    int status;

    /* Now delete the desired files. */
    status = delete_file(TWELVE_BLANKS, filename);

    /* Test for a error in the deletion attempt. */
    if (status != 0) {
        /* Got an error status indicating a failure happened, return error code. */
        *overallStatus = COULD_NOT_DELETE_INTERMEDIARY_FILES;
        printf("Can't delete %s file %s got PCFP status %o\n", error_insert, filename, status);
        return;
    }
}

/*
 * Function delete_file
 *
 * This function deletes (removes) the current cycle of the indicated file
 * using the Program callable FURPUR (PCFP) package. This is a slightly
 * modified version of the C- program file delete example in the
 * Programmable FURPUR (PCFP) PRM.
 *
 * Parameters:
 * qualifier - qualifier for the 2200 file name. A qualifier
 *             made up of all blanks indicates the file is of
 *             the form "*File".
 * filename  - filename portion of the 2200 file name.
 *
 * Returns:
 */
int delete_file(char * qualifier, char * filename){
    /* Define the function packet */
    fp_delete_file_type df_pkt;

    /* Define the file identifier structure. */
    fp_file_id_type file_id;

    /* Zero-fill the function packet. */
    memset (&df_pkt, 0, sizeof (fp_delete_file_type));
    /* Set items in the function packet. */

    df_pkt.gen.interface_level = fp_interface_level_1;
    df_pkt.gen.null_term_strings = TRUE;
    df_pkt.gen.work_area_size = fp_max_work_area_delete_file;

    /* Zero-fill the file identifier. */
    memset (&file_id, 0, sizeof (fp_file_id_type));

    /* Set items in the file id. */
    file_id.interface_level = fp_interface_level_1;
    strncpy (file_id.qualifier,qualifier,sizeof (file_id.qualifier));
    strncpy (file_id.filename,filename,sizeof (file_id.filename));

    /* Now delete the file. */
    _fp_delete_file (&df_pkt,&file_id,NULL);

    /* test for a error in the deletion attempt. */
    if ( df_pkt.gen.error_class != fp_error_class_none ) {
        /* Got an error status indicating a failure happened, return error code. */

        if (debug){
            printf("In delete_file(): PCFP _fp_delete_file(%s, %s) failed, received statuses (%o, %o)\n",
                   qualifier, filename, df_pkt.gen.error_code, df_pkt.gen.error_code % 01000000);
        }

        return df_pkt.gen.error_code % 01000000;
    } else {
        /* Got a successful error status indicating file was deleted, return success code. */

        if (debug){
            printf("In delete_file(): PCFP _fp_delete_file(%s, %s) was successful\n", TWELVE_BLANKS, filename);
        }

        return 0;
    }
}

/*
 * Function set_file_maxcycles
 *
 * This function set the maximum file cycles for the indicated
 * file using the Program callable FURPUR (PCFP) package.
 *
 * Parameters:
 * qualifier - qualifier for the 2200 file name. A qualifier
 *             made up of all blanks indicates the file is of
 *             the form "*File".
 * filename  - filename portion of the 2200 file name.
 * maxcycles - the maximum number of allowed cycles for the
 *             indicated file.
 *
 * Returns:
 */
int set_file_maxcycles(char * qualifier, char * filename, int maxcycles){
    /* Define the function packet */
    fp_chg_file_cyc_type cycle_pkt;

    /* Define the file identifier structure. */
    fp_file_id_type file_id;

    /* Zero-fill the function packet. */
    memset (&cycle_pkt, 0, sizeof (fp_chg_file_cyc_type));
    /* Set items in the function packet. */

    cycle_pkt.gen.interface_level = fp_interface_level_1;
    cycle_pkt.gen.null_term_strings = TRUE;
    cycle_pkt.gen.work_area_size = fp_max_work_area_delete_file;

    /* Zero-fill the file identifier. */
    memset (&file_id, 0, sizeof (fp_file_id_type));

    /* Set items in the file id. */
    file_id.interface_level = fp_interface_level_1;
    strncpy (file_id.qualifier,qualifier,sizeof (file_id.qualifier));
    strncpy (file_id.filename,filename,sizeof (file_id.filename));

    /* Now place the maximum cycle limit in the packet. */
    cycle_pkt.max_f_cycles = maxcycles;

    /* Now change the file's maximum file cycle limit. */
    _fp_chg_file_cyc (&cycle_pkt,&file_id,NULL);

    /* test for a error in the cycle limit attempt. */
    if ( cycle_pkt.gen.error_class != fp_error_class_none ) {
        /* Got an error status indicating a failure happened, return error code. */

        if (debug){
            printf("In set_file_maxcycles(): PCFP _fp_chg_file_cyc(%s, %s, %d) failed, received statuses (%o, %o)\n",
                   qualifier, filename, maxcycles, cycle_pkt.gen.error_code, cycle_pkt.gen.error_code % 01000000);
        }

        return cycle_pkt.gen.error_code % 01000000;
    } else {
        /* Got a successful error status indicating file's maximum file cycle limit was changed, return success code. */

        if (debug){
            printf("In set_file_maxcycles(): PCFP _fp_chg_file_cyc(%s, %s, %d) was successful\n", TWELVE_BLANKS, filename, maxcycles);
        }

        return 0;
    }
}

/*
 * Procedure freeCP_File
 *
 * This procedure frees (releases) a relatively cycled 2200 file that was
 * cataloged using an @ASG,CP. It retains the use names that were placed on
 * it so you can see the file reference an the actual cycle later using a @PRT,I
 *
 * Parameters:
 * overallStatus       - contains the accumulated overall status value. If
 *                       the free operation works without error, then
 *                       overallStatus is left untouched, otherwise it is set
 *                       to an error status value.
 * filename            - a filename string (e.g., "*JDBC$XALOG11") to be @FREE'd
 *                       using its relative cycle number.
 * error_insert        - an insert string to use in the printed error message if
 *                       an error occurs on the free'ing process.
 */
void freeCP_File(int * overallStatus, char * filename, char * error_insert){

    int status = 0;
    char acsfString[ACFS_IMAGE_MAX_CHARS];

    /* Now free the assigned CP'ed file. */
    sprintf(acsfString, "@free,r *%s(+1). . ", filename);
    status = facsf(acsfString);

    if (debug){
        printf("In freeCP_File(): '%s' got status = %o\n", acsfString, status);
    }

    if (status < 0){
        *overallStatus = COULD_NOT_FREE_RETAINED_FILES;
        printf("Can't free %s file %s using %s  got status %o\n", error_insert, filename, acsfString, status);
    }
}

 /*
  * getRunIDs
  *
  * Returns the run's original run ID and the run's generated run ID.
  * Parameters:
  *  runid  - pointer to character array to hold run's original run ID
  *  grunid - pointer to character array to hold run's generated run ID
  */
 #define PCT_ORIG_RUN_ID_POS 0
 #define PCT_GEN_RUN_ID_POS  1

 void getRunIDs(char * runid, char * grunid) {

  int nbrWords = PCT_GEN_RUN_ID_POS + 1;
     int start = 0;
     int buffer[PCT_GEN_RUN_ID_POS+1];
     int fieldataOrigRunID;
     int fieldataGenRunID;
     char * ptr1;

     /* ER PCT$ to get the first two words of the PCT */
     ucspct(&nbrWords, &start, buffer);

     /* Original run ID */

     /* Get the original run ID in fieldata (6 chars) */
     fieldataOrigRunID = buffer[PCT_ORIG_RUN_ID_POS];

     /* printf("fd orig run id = %12o\n", fieldataOrigRunID); */

     /* Convert the ID from fieldata to ASCII */
     fdasc(runid, &fieldataOrigRunID, RUNID_CHAR_SIZE);

     /* If the ID is less than 6 chars, find the first space */
     ptr1 = (char*) memchr(runid, ' ', RUNID_CHAR_SIZE);

     /* NUL-terminate the string */
     if (ptr1 == NULL) {
         runid[RUNID_CHAR_SIZE] = '\0';
     } else {
         *ptr1 = '\0';
     }

     /* Generated run ID */

     /* Get the generated run ID in fieldata (6 chars) */
     fieldataGenRunID = buffer[PCT_GEN_RUN_ID_POS];

     /* printf("fd generated run id = %12o\n", fieldataGenRunID); */

     /* Convert the ID from fieldata to ASCII */
     fdasc(grunid, &fieldataGenRunID, RUNID_CHAR_SIZE);

     /* If the ID is less than 6 chars, find the first space */
     ptr1 = (char*) memchr(grunid, ' ', RUNID_CHAR_SIZE);

     /* NUL-terminate the string */
     if (ptr1 == NULL) {
         grunid[RUNID_CHAR_SIZE] = '\0';
     } else {
         *ptr1 = '\0';
     }

 } /* getRunIDs */

 /*
  * Function getOptionLetters
  *
  * Picks up the option bits for the executing processor
  * and converts them to a string of option letters.
  *
  * Parameters:
  * option_bits   - The program invocation option bits.
  *
  * optionLetters - On input a pointer to a character array
  *                 of 27. On output this character array will
  *                 contain a string of up to 26 characters
  *                 (A-Z) representing the option letters
  *                 obtained from the option bits.
  *                 An empty string is returned if there are
  *                 no option letters.
  *
  */
  void getOptionLetters(char * optionLetters, int option_bits) {

      /* Get an option letter for each option bit indicated. */
      optionLetters[0] = '\0'; /* initialize string */
      if (option_bits & (1<<('Z'-'A'))) strcat(optionLetters,"A");
      if (option_bits & (1<<('Z'-'B'))) strcat(optionLetters,"B");
      if (option_bits & (1<<('Z'-'C'))) strcat(optionLetters,"C");
      if (option_bits & (1<<('Z'-'D'))) strcat(optionLetters,"D");
      if (option_bits & (1<<('Z'-'E'))) strcat(optionLetters,"E");
      if (option_bits & (1<<('Z'-'F'))) strcat(optionLetters,"F");
      if (option_bits & (1<<('Z'-'G'))) strcat(optionLetters,"G");
      if (option_bits & (1<<('Z'-'H'))) strcat(optionLetters,"H");
      if (option_bits & (1<<('Z'-'I'))) strcat(optionLetters,"I");
      if (option_bits & (1<<('Z'-'J'))) strcat(optionLetters,"J");
      if (option_bits & (1<<('Z'-'K'))) strcat(optionLetters,"K");
      if (option_bits & (1<<('Z'-'L'))) strcat(optionLetters,"L");
      if (option_bits & (1<<('Z'-'M'))) strcat(optionLetters,"M");
      if (option_bits & (1<<('Z'-'N'))) strcat(optionLetters,"N");
      if (option_bits & (1<<('Z'-'O'))) strcat(optionLetters,"O");
      if (option_bits & (1<<('Z'-'P'))) strcat(optionLetters,"P");
      if (option_bits & (1<<('Z'-'Q'))) strcat(optionLetters,"Q");
      if (option_bits & (1<<('Z'-'R'))) strcat(optionLetters,"R");
      if (option_bits & (1<<('Z'-'S'))) strcat(optionLetters,"S");
      if (option_bits & (1<<('Z'-'T'))) strcat(optionLetters,"T");
      if (option_bits & (1<<('Z'-'U'))) strcat(optionLetters,"U");
      if (option_bits & (1<<('Z'-'V'))) strcat(optionLetters,"V");
      if (option_bits & (1<<('Z'-'W'))) strcat(optionLetters,"W");
      if (option_bits & (1<<('Z'-'X'))) strcat(optionLetters,"X");
      if (option_bits & (1<<('Z'-'Y'))) strcat(optionLetters,"Y");
      if (option_bits & (1<<('Z'-'Z'))) strcat(optionLetters,"Z");
      return;
  } /* getServerOptionLetters */

  /*
   * Function toUpperCase
   *
   * This function takes two arguments. The first, a input character string
   * argument is copied into the second output character array with each
   * character being uppercased. The second character array must be as
   * long or longer than the first character array, and the first character
   * array must end with a null. The function returns the pointer to the
   * second character array.
   */
  char * toUpperCase(char * inPtr, char * outPtr){
      int i;
      int strLen = strlen(inPtr); /* get input string length */

      /* Copy the input string uppercased to the output string. */
      for (i = 0; i < strLen; i++) {
          outPtr[i] = toupper(inPtr[i]);
      }

      /* null terminate output string. */
      outPtr[i] = '\0';

      return outPtr;
  }
