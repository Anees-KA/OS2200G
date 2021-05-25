/**
 * File: ServerConsol.c.
 *
 * Procedures that comprise the JDBC Server's Console Listener function.
 *
 * This code may utilize other code in the C-Interface library to accomplish
 * its required tasks.
 *
 */
/* Standard C header files and OS2200 System Library files */
#include <ertran.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <universal.h>
#include "marshal.h"

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "CycleCmds.h"
#include "DisplayCmds.h"
#include "ProcessTask.h"
#include "Server.h"
#include "ServerConsol.h"
#include "ServerLog.h"
#include "ServiceUtils.h"
#include "SetCmds.h"
#include "ShutdownCmds.h"
#include "TurnCmds.h"
#include "Twait.h"

/* Imported data */

extern serverGlobalData sgd;
    /* The Server Global Data (SGD), visible to all Server activities. */

/**
 * Function: startServerConsoleListener
 *
 * Starts the JDBC Server's Console Listener.
 * The calling activity becomes the Console Listener and will wait
 * for a console message from the operator that
 * is directed to the JDBC Server,
 * and when one appears it initiates its processing.
 *
 * This operation continues until either a error fatal
 * to the JDBC Server occurs,
 * or one of the console commands indicates that the JDBC Server
 * should shut down.
 * In these cases, a status code value is sent to the main procedure
 * so it can take appropriate actions to complete shutdown
 * of the JDBC Server.
 *
 * If any fatal errors are detected in the starting up of the
 * Console Listener,
 * a Log file entry is generated and an error indicator is returned
 * to the caller for its disposition.
 *
 * @return
 *   A non-zero error status is returned is the console
 *   handler can't be properly started up.
 */

int startServerConsoleListener() {

    keyinPkt1 pkt;
    keyinRetPkt1 retPkt;
    int temp;
    int scratchWord;
    int erCode;
    int inputMask;
    int outputMask;
    int emVAOfPkt;
    int emVAOfRetPkt;
    int bmRetPktAddr;
    int bmPktAddr;
    int errStat;
    int inputRegisters[2];
    int outputRegisters[1];
    int done;
    int returnStatus;
    int cmdStatus;
    int keyinDeregistered;
    int stopServerArg;
    char keyinMessage[MESSAGE_MAX_SIZE-USERID_NAME_FIELD_LEN+1];
    char digits[ITOA_BUFFERSIZE];
    int si, di; /* Source and destination index */
    char *console_msg;
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

#if DEBUG
    int int1;
    char keyinName[KEYIN_CHAR_SIZE+1];
    char userID[USERID_NAME_FIELD_LEN+1];
#endif

    /* Initialize the CH (console handler) states. */

    sgd.consoleHandlerState = CH_INITIALIZING;
    sgd.consoleHandlerShutdownState = CH_ACTIVE;

    /* Initialize the return status.
       Assume that no error is going to occur setting up the
       Console Listener.
       */
    returnStatus = 0;

    /* Log that we are starting to listen to the 2200 Console. */
    getMessageWithoutNumber(CH_STARTING_CONSOLE, NULL, NULL,
        SERVER_LOGS_DEBUG);
            /* 5212 Starting the JDBC Server Console Listener */

    /* Convert return packet address to BM */
    emVAOfRetPkt = (int) &retPkt;
    ucsemtobm(&emVAOfRetPkt, &bmRetPktAddr, &errStat);

    if (errStat != 0) {
        getLocalizedMessageServer(CH_BAD_UCSEMTOBM_RETURN_FOR_KEYIN,
            NULL, NULL, SERVER_LOGS, msgBuffer);
                /* 5213 Bad return from ucsemtobm for ER KEYIN$
                   return packet */
        return JDBCSERVER_UNABLE_TO_GET_CONS_CMD;
    } else {
#if DEBUG
        printf("Good return from ucsemtobm for return packet\n");
        printf("    EM addr %012o\n", emVAOfRetPkt);
        printf("    BM addr %012o\n", bmRetPktAddr);
#endif
    }

    /* Convert packet address to BM */
    emVAOfPkt = (int) &pkt;
    ucsemtobm(&emVAOfPkt, &bmPktAddr, &errStat);

    if (errStat != 0) {
        getLocalizedMessageServer(CH_BAD_UCSEMTOBM_RETURN_FOR_KEYIN,
            NULL, NULL, SERVER_LOGS, msgBuffer);
                /* 5213 Bad return from ucsemtobm for ER KEYIN$
                   return packet */
        return JDBCSERVER_UNABLE_TO_GET_CONS_CMD;
    } else {
#if DEBUG
        printf("Good return from ucsemtobm for packet\n");
        printf("    EM addr %012o\n", emVAOfPkt);
        printf("    BM addr %012o\n", bmPktAddr);
#endif
    }

    /*
     * Send a message to the console indicating the (XA) JDBC Server
     * is up and running.  This must be done before we enter the
     * ER KEYIN$ loop for processing Console Commands.
     */
#ifndef XABUILD  /* Local Server */
    console_msg = getLocalizedMessageServerNoErrorNumber(SERVER_IS_RUNNING,
                                                         sgd.serverName,
                                                         NULL,
                                                         0, msgBuffer);
#else   /* XA Server */
    console_msg = getLocalizedMessageServerNoErrorNumber(XA_SERVER_IS_RUNNING,
                                                         sgd.serverName,
                                                         NULL,
                                                         0, msgBuffer);
#endif
    sendcns(console_msg, strlen(console_msg));

    /* The call to register the keyin name for ER KEYIN$
       used to be done here.
       The call below has been commented out and
       moved to the JDBC server main program.

    status = registerKeyin();

    if (status != 0) {
        return status;
    }
    */

    /* Loop until we hit a SHUTDOWN command of the form:
         @@cons keyin-name SHUTDOWN text
       where text does not begin with 'WORKER'.
       */
    done = FALSE;
    sgd.consoleHandlerState = CH_RUNNING;

    while (! done
        && (sgd.serverState == SERVER_RUNNING)) {

        /* Set up ER KEYIN$ packet for the wait function */
        pkt.k1.status = -1; /* initialize to a non-zero value */
        pkt.k1.func = KEYIN_WAIT_USER_FN;
        pkt.k1.res1 = 0;
        pkt.k1.bufLen = RET_PKT_WORD_SIZE;
        pkt.k1.retPktAddr = bmRetPktAddr;
        pkt.k1.res2 = 0;
        pkt.k1.err1 = -1; /* initialize to a non-zero value */


        /* Place the keyin name in the packet. */
        genKeyinName(pkt.k1.keyin);

        /* Set up the return packet */
        memset(retPkt.i1, 0, RET_PKT_BYTE_SIZE); /* zero it out */
        /* retPkt.k1.res1 = 0; */

#if DEBUG
        /* Dump the packet */
        printf("ER KEYIN$ packet:\n");
        for (int1=0; int1<PKT_WORD_SIZE; int1++) {
            printf("  %012o\n", pkt.i1[int1]);
        }
#endif

        /* Set up to call ER for the wait function */
        erCode = ER_KEYIN;
        inputMask = 0600000000000; /* input regs A0-A1 */
        outputMask = 0;
        inputRegisters[0] = bmPktAddr; /* A0 */
        inputRegisters[1] = PKT_WORD_SIZE; /* A1 */

        /* Call ER KEYIN$ for ER KEYIN$ wait function */
#if DEBUG
        printf("Calling ER KEYIN$ for wait user mode\n");
#endif
        keyinDeregistered = FALSE;

        /* ER KEYIN$ call (wait function) */
        ucsgeneral(&erCode, &inputMask, &outputMask, inputRegisters,
            outputRegisters, &temp, &scratchWord);

        /* Check status returned from the ER */
        switch (pkt.k1.status) {
            case 0: /* normal return */
                break;
            case 2:
                getLocalizedMessageServer(CH_KEYIN_PKT_TOO_SMALL, NULL,
                    NULL, SERVER_LOGS, msgBuffer);
                        /* 5214 ER KEYIN$ return buffer is not large enough */
                returnStatus = JDBCSERVER_UNABLE_TO_GET_CONS_CMD;
                goto ret;
            case 02000:
                /* Error:
                   No keyins are queued for this program.

                   This seems to happens when this loop is reentered
                   because of a deregisterKeyin() call on the keyin name.

                   This can occur on a SHUTDOWN command,
                   which causes the server workers to shut down one by one.
                   The last SW (or the ICL, if no SWs are active)
                   that shuts down deregisters the keyin.
                   */
                keyinDeregistered = TRUE;
                break;
            case 010000:
                /* Error:
                   The program is not registered for unsolicited keyins.

                   It is not clear if we can get this error.
                   Treat it like a deregister case above.
                   */
                keyinDeregistered = TRUE;
                break;
            default:
                getLocalizedMessageServer(CH_KEYIN_WAIT_FN_ERROR,
                    itoa(pkt.k1.status, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                        /* 5215 Bad status for ER KEYIN$ wait function: {0} */
                returnStatus = JDBCSERVER_UNABLE_TO_GET_CONS_CMD;
                goto ret;

        } /* end switch */

        if (keyinDeregistered ||
            (sgd.consoleHandlerShutdownState == CH_SHUTDOWN)) {

            /* Set the CH state to downed */
            sgd.consoleHandlerState = CH_DOWNED;

            /* Set the argument for the stopServer call, depending
               on whether we had a deregister or not. */
            if (keyinDeregistered) {
                stopServerArg = FALSE;
            } else {
                /* abort case */
                stopServerArg = TRUE;
                replyToKeyin(
                    "JDBC server shutting down - console commands ignored");
            }

            /* Wait one second to allow the output being sent to the
               log and trace files to complete.  */
            twait(1000);

            /* Shut down the server */
            stopServer(stopServerArg);
                /* Note: Control does not return here,
                   since all server tasks are terminated. */
        }

#if DEBUG
        /* Dump return packet */
        printf("ER KEYIN$ return packet:\n");
        for (int1=0; int1<RET_PKT_WORD_SIZE; int1++) {
            printf("  %012o\n", retPkt.i1[int1]);
        }

        memcpy(keyinName, retPkt.k1.keyin, KEYIN_NAME_FIELD_LEN);
        keyinName[KEYIN_NAME_FIELD_LEN] = '\0';
        printf("keyin name = %s\n", keyinName);

        printf("console mode = %d\n", retPkt.k1.consoleMode);
        printf("char count of keyin = %d\n", retPkt.k1.charCount);
        printf("keyin routing word (octal) = %012o\n", retPkt.k1.keyinRouting);

        /* User ID is returned as the first 12 chars of the message,
           if the ER KEYIN$ function is wait user.
           */
        memcpy(userID, retPkt.k1.msg, USERID_NAME_FIELD_LEN);
        userID[USERID_NAME_FIELD_LEN] = '\0';
        printf("user ID of run that entered @@cons = %s\n", userID);
#endif

        /* Get the keyin message text.
           We automatically get it in upper case from the Exec,
           even if the user keyed in lower case.
           */
        memcpy(keyinMessage, retPkt.k1.msg+USERID_NAME_FIELD_LEN, retPkt.k1.charCount);
        keyinMessage[retPkt.k1.charCount] = '\0';

        /* Strip multiple spaces from the keyinMessage */
        si=0;                                   /* source index */
        di=0;                                   /* destination index */
        while(si <= retPkt.k1.charCount) {       /* loop until end of string */
            if(di != si) {
                keyinMessage[di] = keyinMessage[si]; /* copy one character */
            }
            di++;                               /* incr dest index */
            si++;                               /* incr source index */
            if(keyinMessage[si-1] == ' ') {     /* if last char was a space */
                while(keyinMessage[si] == ' ') si++; /* skip over extra spaces */
            }
        }

#if DEBUG
        printf("message text = '%s'\n", keyinMessage);
#endif

        /* Save the return packet address for use by replyToKeyin */
        sgd.keyinReturnPktAddr = (void *) &retPkt.k1;

        /* Process the @@CONS command in string keyinMessage.
           Notes:
             * All text from @@CONS comes in as upper case.
             * We do not get any leading or trailing blanks from the Exec.
           */
        cmdStatus = processCommand(keyinMessage);

        /* Note:
           We don't need to set done to TRUE to exit the while loop
           when the server is shut down.
           Instead, the code above (after the call to go into wait mode
           on the ER KEYIN$) will get us out of the loop,
           because at some point stopServer will be called,
           and it does not return to this function.
           The conditions to exit the loop are:
             - The ER KEYIN$ keyin name has been deregistered
               (which causes this wait code to be re-entered from
               an inactive state).
               This happens when we have done a SHUTDOWN command
               and the last server worker activity has terminated.
             - The consoleHandlerShutdownState is set to CH_SHUTDOWN.
               This can happen on a server abort.
           */

        /* Send the output back to the run that specified the
           @@CONS command,
           unless the output has already been sent back
           (if cmdStatus is zero).
           */
        if (cmdStatus == 1) {
            replyToKeyin(sgd.textMessage);
        }

    } /* end while */

ret:
    /* The ER KEYIN$ keyin name used to be deregistered here,
       but now it is done from a command's function
       (e.g., SHUTDOWN or ABORT).
    status = deregisterKeyin();

    if (status != 0) {
        return status; -> Unable to deregister keyin
    }
    */

    /* Return the status */
    return returnStatus;

} /* startServerConsoleListener */

/**
 * Function: genKeyinName
 *
 * Place the keyin name in first eight ASCII characters in the packet
 * (pkt, allocated by the caller).
 * Note that this name in the packet is not a NUL-terminated C string.
 * Also fill in the name in sgd.keyinName.
 *
 * Initialize it with ASCII spaces before the keyin name is moved there.
 *
 * The keyin name (case insensitive) is determined as follows:
 *
 *   * If sgd.configKeyinID is an empty string or is equal to "RUNID",
 *     the keyin name is the original run ID of the server run.
 *     Prefix the run ID with "JS" if the run ID begins with a digit.
 *
 *   * Otherwise, the keyin name is sgd.configKeyinID.
 *
 * XA Server case:
 *   The keyin name is the generated run ID.
 *
 * Notes on the keyin name and run ID:
 *
 *   * The keyin name can contain letters, digits, and '*',
 *     but must begin with a letter.
 *     The keyin name is one to eight characters.
 *
 *   * The original run ID contains letters and digits,
 *     and is 1-6 characters.
 *
 * @param pkt
 *   The ER KEYIN$ packet allocated by the caller.
 */

static genKeyinName(char * pkt) {

    int keyinIDLen;

#ifndef XABUILD /* Local Server */

    int configKeyinIDLen;

#endif /* Local Server */


#ifdef XABUILD

    /* XA Server: Use the generated run ID for the keyin name */

    if (isdigit(sgd.generatedRunID[0])) {
        strcpy(sgd.keyinName, KEYIN_PREFIX);
            /* Prefix run ID with "JS" */
        strcpy(sgd.keyinName+KEYIN_PREFIX_LEN, sgd.generatedRunID);
    } else {
        strcpy(sgd.keyinName, sgd.generatedRunID);
    }

#else /* Local JDBC Server */

    toUpperCase(sgd.configKeyinID);

    /* keyin_id=RUNID or no keyin_id config parameter
       imples use the original run ID of the server. */
    if ((strcmp(sgd.configKeyinID, "RUNID") == 0)
        || (strlen(sgd.configKeyinID) == 0)) {

        if (isdigit(sgd.originalRunID[0])) {
            strcpy(sgd.keyinName, KEYIN_PREFIX);
                /* Prefix run ID with "JS" */
            strcpy(sgd.keyinName+KEYIN_PREFIX_LEN, sgd.originalRunID);
        } else {
            strcpy(sgd.keyinName, sgd.originalRunID);
        }

    } else {
        configKeyinIDLen = strlen(sgd.configKeyinID);

        if (configKeyinIDLen > KEYIN_NAME_FIELD_LEN) {
            configKeyinIDLen = KEYIN_NAME_FIELD_LEN;
        }

        strncpy(sgd.keyinName, sgd.configKeyinID, configKeyinIDLen);
    }

#endif

    /* Move sgd.keyinName (a C string) to the packet (eight characters,
       blank filled, not a C string (i.e., not NUL-terminated) */
    memcpy(pkt, SPACES, KEYIN_NAME_FIELD_LEN);
    keyinIDLen = strlen(sgd.keyinName);
    memcpy(pkt, sgd.keyinName, keyinIDLen);

} /* genKeyinName */

/**
 * Function: replyToKeyin
 *
 * Reply to whoever entered the @@CONS keyin.
 * Use ER COM$.
 *
 * Note that there is a limit of 78 characters sent on a reply.
 * Since there is no notion of an end of line (i.e., carriage return
 * or line feed) in the text, we send one line of output at a time.
 *
 * If the flag sgd.logConsoleOutput is TRUE,
 * also add the console message to the server log file.
 *
 * @param text
 *  The text that is sent to the @@CONS sender.
 *
 * @return
 *   0 is returned on a successful reply.
 *   Nonzero implies the reply was not sent.
 */

int replyToKeyin(char * text) {

#define DUMP_BASIC      2  /* ER KEYIN$ console mode: basic keyin mode */
#define DEMAND          4  /* ER KEYIN$ console mode: display & keyin mode */

    comDollarPkt1 comPkt;
    comDollarPkt * bank;
    keyinRetPkt * keyinReturnPktAddr;
    int comER = ER_COM;
    int inMask = 0400000000000; /* A0 only */
    int outMask = 0; /* no output registers */
    int inReg[1];
    int outReg[1];
    int scratch;
    int bmAddr;
    int psPtr;
    int errStat;
    int textCharCount;
    char text1[MAX_COM_OUTPUT_BYTE_SIZE+1];
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

#define BLANKS_8 "        "

#if DEBUG
    int int1;
#endif

    /* Copy the text message to the stack,
       so that the ER COM$ packet and the text message are in the
       same bank for UCSGENERAL. */
    textCharCount = strlen(text);

    if (textCharCount > MAX_COM_OUTPUT_BYTE_SIZE) {
        textCharCount = MAX_COM_OUTPUT_BYTE_SIZE;
        memcpy(text1, text, MAX_COM_OUTPUT_BYTE_SIZE); /* get first 4K bytes */
        text1[MAX_COM_OUTPUT_BYTE_SIZE] = '\0';
#if DEBUG
        printf("Text message for ER COM$ is too large - truncate to 4096\n");
#endif
    } else {
        strcpy(text1, text);
    }

    /* Convert the text address to basic mode */
    psPtr = (int) text1;
    ucsemtobm(&psPtr, &bmAddr, &errStat);

    if (errStat != 0) {
        getLocalizedMessageServer(CH_BAD_UCSEMTOBM_RETURN_FOR_COM_TEXT,
            NULL, NULL, SERVER_LOGS, msgBuffer);
                /* 5216 Bad return from ucsemtobm for ER COM$ text address */
        return JDBCSERVER_UNABLE_TO_PERFORM_ER_COM;
    } else {
#if DEBUG
        printf("Good return from ucsemtobm for text address\n");
        printf("    EM addr %012o\n", psPtr);
        printf("    BM addr %012o\n", bmAddr);
        printf("    text message : '%s'\n", text);
#endif
    }

    /* Convert the packet address to basic mode */
    psPtr = (int) &comPkt;
    ucsemtobm(&psPtr, &inReg[0], &errStat);

    if (errStat != 0) {
        getLocalizedMessageServer(CH_BAD_UCSEMTOBM_RETURN_FOR_COM_PKT,
            NULL, NULL, SERVER_LOGS, msgBuffer);
                /* 5217 Bad return from ucsemtobm for ER COM$ packet address */
        return JDBCSERVER_UNABLE_TO_PERFORM_ER_COM;
    } else {
#if DEBUG
        printf("Good return from ucsemtobm for packet address\n");
        printf("    EM addr %012o\n", psPtr);
        printf("    BM addr %012o\n", inReg[0]);
#endif
    }

    /* Zero out the ER COM$ packet */
    memset(&comPkt, 0, COM_PKT_BYTE_SIZE);

    /* Set up the ER COM$ packet */
    keyinReturnPktAddr = (keyinRetPkt *) sgd.keyinReturnPktAddr;
    if (keyinReturnPktAddr->keyinRouting == 0) {
        if (keyinReturnPktAddr->consoleMode == DEMAND) {
            printf("%s\n", text);
            addServerLogEntry(SERVER_LOGS, text);
            return 0;
        } else {
            comPkt.c1.controlBits = 06;
              /* 040: A run-id is supplied in the packet that should
                      be used for logging purposes.
                      Don't set this bit - it causes too much output
                      in the FIN report.
                 004: Contents of output msg is ASCII or mix of ASCII/Asian.
                      Data input from console is returned in ASCII.
                 002: The Exec does not log the console message answer
                      (because it may contain secure data). */
        }
    } else {
        comPkt.c1.controlBits = 016;
              /* 040: A run-id is supplied in the packet that should
                      be used for logging purposes.
                      Don't set this bit - it causes too much output
                      in the FIN report.
                 010: Valid routing info is present in the pkt.
                 004: Contents of output msg is ASCII or mix of ASCII/Asian.
                      Data input from console is returned in ASCII.
                 002: The Exec does not log the console message answer
                      (because it may contain secure data). */
    }

    comPkt.c1.outputCount = textCharCount;
    comPkt.c1.outputAddress = bmAddr;
    comPkt.c1.expectedInput = 0;
    comPkt.c1.inputAddress = 0;

    /* Insert generated run ID in words 3-4 in the packet.

       The run-id is used when logging the console message.
       The run-id is given as 6 characters, left justified and space-filled.
       If you specify a run-id in this field, you must
       also set 040 (run-id specified) in the control bits field.

       If the run-id is Fieldata, bit value 01 (ASCII run-id)
       of the additional control bits field must be clear.
       In this case, the run-id is located in word 03, and word 04 contains
       routing information.

       If the run-id is in ASCII (our case), bit value 01 of the
       additional control bits field must be set.
       In this case, the run-id is located in words 03 and 04,
       and word 05 contains routing information.

       Note that setting bit 040 and inserting the generated run ID
       does not change the console message that is displayed,
       but it will affect the system log file
       (i.e., the generated run ID will appear in the message in the
       system log file).

       Final note: We are not setting control bit 040,
       since it causes too much output to go to the run's FIN report.
       Therefore, we won't place the generated run-id in words 3-4
       (even though it would be ignored without setting bit 040).
       */
    /* memcpy(comPkt.c1.runID, BLANKS_8, strlen(BLANKS_8)); */
        /* blank fill with 8 blanks */
    /* memcpy(comPkt.c1.runID, sgd.generatedRunID, strlen(sgd.generatedRunID)); */
        /* overwrite as many blanks as necessary with the generated run ID */

    /* Routing info in the packet */
    if (keyinReturnPktAddr->keyinRouting != 0) {
        comPkt.c1.routingInfo1 = keyinReturnPktAddr->keyinRouting;
        comPkt.c1.routingInfo2 = keyinReturnPktAddr->res2;
            /* The ER KEYIN$ packet shows the keyin routing word,
               followed by a reserved word.
               Other users of ER KEYIN$ seem to use the reserved word. */
    }

    comPkt.c1.addControlBits = 1;
        /* The runid supplied with the packet (if any) is in ASCII. */

#if DEBUG
    printf("ER COM$ packet:\n");
    for (int1=0; int1<COM_PKT_WORD_SIZE; int1++) {
        printf("  %012o\n", comPkt.i1[int1]);
    }
#endif

    /* Call ER COM$ to send the reply text to the @@CONS sender */
    bank = &comPkt.c1;

    ucsgeneral(&comER, &inMask, &outMask, inReg, outReg, bank, &scratch);

    /* Check the error status returned in the packet */
    if (comPkt.c1.errorCode != 0) {
        getLocalizedMessageServer(CH_ER_COM_ERROR,
            itoa(comPkt.c1.errorCode, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5218 ER COM$ error status: {0} */
        return JDBCSERVER_UNABLE_TO_PERFORM_ER_COM;
    }

    /* If the flag sgd.logConsoleOutput is TRUE,
       add the console message to the server log file */
    if (sgd.logConsoleOutput) {
        addServerLogEntry(LOG_TO_SERVER_LOGFILE, text);
    }

    /* Successful return */
    return 0;

} /* replyToKeyin */

/**
 * Function: registerKeyin
 *
 * Register the ER KEYIN$ keyin name.
 * Return a status to the caller.
 *
 * @return
 *   An error status.
 *   0 means the operation was successful.
 */

int registerKeyin() {
    keyinPkt1 pkt;
    int erCode;
    int inputMask;
    int outputMask;
    int inputRegisters[2];
    int outputRegisters[1];
    int temp;
    int scratchWord;
    int emVAOfPkt;
    int bmPktAddr;
    int errStat;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */
#if DEBUG
    int int1;
#endif

    /* #defines for ER KEYIN$ error statuses */
#define KEYIN_SECURITY 0100
#define KEYIN_IN_USE 0200

    /* Convert packet address to BM */
    emVAOfPkt = (int) &pkt;
    ucsemtobm(&emVAOfPkt, &bmPktAddr, &errStat);
    if (errStat != 0) {
        getLocalizedMessageServer(CH_BAD_UCSEMTOBM_FOR_KEYIN_PKT, NULL,
            NULL, SERVER_LOGS, msgBuffer);
                /* 5219 Bad return from ucsemtobm for ER KEYIN$ packet */
        return JDBCSERVER_UNABLE_TO_REGISTER_FOR_CONS;
    } else {
#if DEBUG
        printf("Good return from ucsemtobm for packet\n");
        printf("    EM addr %012o\n", emVAOfPkt);
        printf("    BM addr %012o\n", bmPktAddr);
#endif
    }

    /* Set up ER KEYIN$ packet for the register function */
    pkt.k1.status = -1; /* initialize to a non-zero value */
    pkt.k1.func = KEYIN_REGISTER_FN;
    pkt.k1.res1 = 0;
    pkt.k1.bufLen = 0;
    pkt.k1.retPktAddr = 0;
    pkt.k1.res2 = 0;
    pkt.k1.err1 = -1; /* initialize to a non-zero value */

    /* Place the keyin name in the packet. */
    genKeyinName(pkt.k1.keyin);

#if DEBUG
        /* Dump the packet */
        printf("ER KEYIN$ packet:\n");
        for (int1=0; int1<PKT_WORD_SIZE; int1++) {
            printf("  %012o\n", pkt.i1[int1]);
        }
#endif

    /* Set up to call ER for the register function */
    erCode = ER_KEYIN;
    inputMask = 0600000000000; /* input regs A0-A1 */
    outputMask = 0;
    inputRegisters[0] = bmPktAddr; /* A0 */
    inputRegisters[1] = PKT_WORD_SIZE; /* A1 */

    /* Call ER KEYIN$ for the register function */
    ucsgeneral(&erCode, &inputMask, &outputMask, inputRegisters,
            outputRegisters, &temp, &scratchWord);

    if (pkt.k1.status != 0) {
        getLocalizedMessageServer(
            CH_KEYIN_REGISTER_FAILED,  itoa(pkt.k1.status, digits, 8), NULL,
            SERVER_LOGS, msgBuffer);
                /* 5220 ER KEYIN$ register function failed status: 0{0} */

        /* Check the two common error statuses, and issue a second
           error message for these cases */

        if (pkt.k1.status == KEYIN_IN_USE) {
           getLocalizedMessageServer(
                CH_KEYIN_NAME_NOT_AVAIL, sgd.keyinName, NULL, SERVER_LOGS, msgBuffer);
                    /* 5249 Server console keyin name {0} not available
                       (probably in use by another server) */

        } else if (pkt.k1.status == KEYIN_SECURITY) {
            getLocalizedMessageServer(
                CH_KEYIN_NAME_SECURITY, sgd.keyinName, NULL, SERVER_LOGS, msgBuffer);
                    /* 5252 Server console keyin name {0} could not be
                       registered (SSCONSOLE privilege required) */
        }

        return JDBCSERVER_UNABLE_TO_REGISTER_FOR_CONS;
    }

    return 0; /* normal return */

} /* registerKeyin */

/**
 * Function: deregisterKeyin
 *
 * Deregister the ER KEYIN$ keyin name.
 * Return a status to the caller.
 *
 * @return
 *   An error status.
 *   0 means the operation was successful.
 */

int deregisterKeyin() {
    keyinPkt1 pkt;
    int erCode;
    int inputMask;
    int outputMask;
    int inputRegisters[2];
    int outputRegisters[1];
    int temp;
    int scratchWord;
    int emVAOfPkt;
    int bmPktAddr;
    int errStat;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */
#if DEBUG
    int int1;
#endif

    /* Convert packet address to BM */
    emVAOfPkt = (int) &pkt;
    ucsemtobm(&emVAOfPkt, &bmPktAddr, &errStat);
    if (errStat != 0) {
        getLocalizedMessageServer(CH_BAD_UCSEMTOBM_FOR_KEYIN_PKT, NULL,
            NULL, SERVER_LOGS, msgBuffer);
                /* 5219 Bad return from ucsemtobm for ER KEYIN$ packet */
        return JDBCSERVER_UNABLE_TO_DEREGISTER_FOR_CONS;
    } else {
#if DEBUG
        printf("Good return from ucsemtobm for packet\n");
        printf("    EM addr %012o\n", emVAOfPkt);
        printf("    BM addr %012o\n", bmPktAddr);
#endif
    }

    /* Set up ER KEYIN$ packet for the deregister function */
    pkt.k1.status = -1; /* initialize to a non-zero value */
    pkt.k1.func = KEYIN_DEREGISTER_FN;
    pkt.k1.res1 = 0;
    pkt.k1.bufLen = 0;
    pkt.k1.retPktAddr = 0;
    pkt.k1.res2 = 0;
    pkt.k1.err1 = -1; /* initialize to a non-zero value */

    /* Place the keyin name in the packet. */
    genKeyinName(pkt.k1.keyin);

#if DEBUG
        /* Dump the packet */
        printf("ER KEYIN$ packet:\n");
        for (int1=0; int1<PKT_WORD_SIZE; int1++) {
            printf("  %012o\n", pkt.i1[int1]);
        }
#endif

    /* Set up to call ER for the deregister function */
    erCode = ER_KEYIN;
    inputMask = 0600000000000; /* input regs A0-A1 */
    outputMask = 0;
    inputRegisters[0] = bmPktAddr; /* A0 */
    inputRegisters[1] = PKT_WORD_SIZE; /* A1 */

    /* Call ER KEYIN$ for the deregister function */
    ucsgeneral(&erCode, &inputMask, &outputMask, inputRegisters,
            outputRegisters, &temp, &scratchWord);

    if (pkt.k1.status != 0) {
        getLocalizedMessageServer(CH_KEYIN_DEREGISTER_FAILED,
            itoa(pkt.k1.status, digits, 8), NULL, SERVER_LOGS, msgBuffer);
                /* 5221 ER KEYIN$ deregister function failed status: 0{0} */
        return JDBCSERVER_UNABLE_TO_DEREGISTER_FOR_CONS;
    }

    return 0; /* normal return */

} /* deregisterKeyin */

/**
 * processCommand
 *
 * Process the @@CONS command for the JDBC server..
 *
 * @param keyinMessage
 *   The command string.
 *
 * @return
 *   A status that indicates whether the reply has been sent to the @@CONS
 *   command sender.
 *     - 0: The replay has been sent.
 *     - 1: The reply has not been sent.
 *          The caller must send it.
 */

static int processCommand(char * keyinMessage) {

    int cmdStatus;
    char buffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char * msg2;
    int msgNbr;

    /* Echo the console command in the log file.
       We want one blank line before the echoed line,
       so as to separate the console command output. */
    sprintf(buffer, "** CONSOLE COMMAND: %s", keyinMessage);
    addServerLogEntry(LOG_TO_SERVER_LOGFILE, ""); /* causes one blank line */
    addServerLogEntry(TO_SERVER_LOGFILE, buffer);
        /* get timestamp in front of echoed console command */

    /* SHUTDOWN, ABORT, and TERM commands
       ----------------------------------

       You can shutdown the entire server (server control commands)
       or just one server worker (worker control commands).
       */

#ifndef XABUILD /* Local Server */

    /* Note: For the XA Server, all SHUTDOWN, ABORT, and TERM commands have
       been eliminated. */

    if ((strncmp(keyinMessage, "SHUTDOWN WORKER", 15) == 0) ||
        (strncmp(keyinMessage, "ABORT WORKER", 12) == 0)) {
        cmdStatus = processShutdownWorkerCmd(keyinMessage);

    } else if ((strncmp(keyinMessage, "SHUTDOWN", 8) == 0) ||
        (strncmp(keyinMessage, "ABORT", 5) == 0) ||
        (strncmp(keyinMessage, "TERM", 4) == 0)) {
        cmdStatus = processShutdownServerCmd(keyinMessage);

#endif

    /* DISPLAY commands
       ---------------- */

#ifdef XABUILD /* XA Server */

    if (strncmp(keyinMessage, "DISPLAY", 7) == 0) {

#else /* Local Server */

    } else if (strncmp(keyinMessage, "DISPLAY", 7) == 0) {

#endif

        cmdStatus = processDisplayCmd(keyinMessage);

    } else if (strncmp(keyinMessage, "STATUS", 6) == 0) {
        cmdStatus = processDisplayCmd(keyinMessage);

    /* HELP command
       ------------ */

    } else if (strncmp(keyinMessage, "HELP", 4) == 0) {
        cmdStatus = processHelpCmd(keyinMessage);

    /* SET commands
       ------------ */

    } else if (strncmp(keyinMessage, "SET", 3) == 0) {
        cmdStatus = processSetCmd(keyinMessage);

    /* CLEAR command
       ------------- */

    } else if (strncmp(keyinMessage, "CLEAR", 5) == 0) {
        cmdStatus = processClearCmd(keyinMessage);

    /* CYCLE commands
       -------------- */

    } else if (strncmp(keyinMessage, "CYCLE", 5) == 0) {
        cmdStatus = processCycleCmd(keyinMessage);

    /* TURN commands
       ------------- */

    } else if (strncmp(keyinMessage, "TURN", 4) == 0) {
        cmdStatus = processTurnCmd(keyinMessage);

    /* illegal command
       --------------- */

    } else {

#ifdef XABUILD /* XA Server */

        msgNbr = CH_INVALID_XA_CMD;
            /* 5251 Invalid command for the XA Server console */

#else /* Local Server */

        msgNbr = CH_INVALID_CMD;
            /* 5222 Invalid command (use TERM for server shutdown) */

#endif

        msg2 = getLocalizedMessageServer(msgNbr, NULL, NULL, 0, buffer);
        strcpy(sgd.textMessage, msg2);
        cmdStatus = 1;
        }

    return cmdStatus;

} /* processCommand */

/*
 * sendcns()
 *
 * DESCRIPTION: Does ER COM$ to display a message on the console.
 *
 *      RETURNS: number of characters read.
 */
int sendcns(char *omsg, int   olen) {
    comDollarPkt cpkt;
    int code, in, out, inar[1], outar[1], scratch;
        char output[180];
        int *ptr;

        strcpy(output, omsg);               /* Get the output message */

    /* set up packet values */
    cpkt.messageGroupNumber    = 0;     /* general system message console */
    cpkt.controlBits           = 4;     /* ouput message is in ASCII */
    cpkt.addControlBits        = 0;     /* no additional control */
    cpkt.outputCount           = olen;  /* number of characters in display msg */
        ptr                        = (int *)output;
        cpkt.outputAddress         = (int)ptr;
    cpkt.msgSuppressionLevel   = 0;     /* messages not suppressable */
    cpkt.expectedInput         = 0;
    cpkt.inputAddress          = 0;
    inar[0] = (int) &cpkt;
    in = 0400000000000;
    out = 0;
    code = 010;
        /* Issue the ER COM$ */
    ucsgeneral(&code,&in,&out,inar,outar,&scratch,&scratch);
    return(cpkt.inputCount);
} /* end sendcns */

