/**
 * File: ProcessTask.c.
 *
 * Procedures that handle processing of a JDBC Client's request packet task.
 * This code is utilized by the JDBC Server's Server Worker function.
 *
 * This code may utilize other code in the C-Interface library to accomplish
 * its required tasks.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <universal.h>
#include "marshal.h"

/* JDBC Project Files */
#include "c-interface.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "taskcodes.h" /* Include crosses the c-interface/server boundary */
#include "utilities.h" /* Include crosses the c-interface/server boundary */
#include "ProcessTask.h"
#include "ServerLog.h"

extern insideClibProcessing;
extern serverGlobalData *sgdPtr;     /* The Server Global Data (SGD) Pointer */

/*
 * Function processTask
 *
 * This routine calls the appropriate C-Interface code to process the
 * task indicated by the task code inside the request packet argument.
 *
 * Upon return from the called C-Interface routine, there is always a
 * response packet that will be sent back to the JDBC Client by the caller
 * of this function.
 *
 * For purposes of debugging, this routine is considered part of the JDBC
 * Clients connection trace.
 * For the JDBC Sever mode, the output is stored in the clients server-side
 * trace file.
 *
 * The returned function value indicates whether the task executed
 * correctly or whether an error occurred.
 *
 * Input parameters:
 *
 *     wdePtr - For JDBC Server or JDBC XA Server, this points to the
 *              WDE entry for the current JDBC client's Server Worker
 *              activity.
 *
 * request_ptr - Input pointer to the request packet byte array.
 *
 * response_ptr - Output pointer to the response packet byte array.
 *
 */
int processClientTask(workerDescriptionEntry *wdePtr, char *request_ptr
                      , char **response_ptr_ptr){

   c_intDataStructure * pDS;
   char *response_ptr;
   short taskCode;
   int returnStatus;
   int ind;
   int reqHdrStatus;
   requestHeader reqHdr;
   int taskOpenedClientTracefile=BOOL_FALSE;
                                /* assume this task initially doesn't open the client trace file */
   char dateTimeString[GETDATETIME_STRINGLEN];
   int debugUserPassValidation_inXAmode = BOOL_FALSE; /* Assume we'll not be tracing a USERID_PASSWORD_TASK. */
   char generatedRunID[RUNID_CHAR_SIZE+1];
   int debugFlags;
   int eraseTraceFileFlag;
   int sfileNameLen;
   char sfileName[MAX_FILE_NAME_LEN+1];
   char *sfileNamePtr=&sfileName[0];
   int error_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;   /* maximum length error message can be */
   char error_message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* a buffer for a returned error message */
   char * msg2;
   int debugInfoID;
   int error_status;
   char digits[ITOA_BUFFERSIZE];
#ifdef XABUILD  /* XA JDBC Server */
   int filelen;
   char digits2[ITOA_BUFFERSIZE];
#endif

   pDS = get_C_IntPtr();
   /* Get the request packet header */
   reqHdrStatus = getRequestHeader(request_ptr, &ind, &reqHdr);

   /* Return an error response if the request hdr was not valid */
   if (reqHdrStatus == 1) {
       msg2 = getLocalizedMessageServer(SERVER_INVALID_REQUEST_ID, NULL, NULL, 0, error_message);
           /* 5330 Invalid ID in request packet */
       response_ptr = genErrorResponse(C_INTERFACE_ERROR_TYPE,
           SERVER_INVALID_REQUEST_ID, 0, msg2, FALSE);
       *response_ptr_ptr=response_ptr;
       return JDBCSERVER_PROCESSTASK_RETURNED_ERROR;
   }

   /*
    * We have a valid request packet.
    * Get the request packet's task code, save it
    * in the wde and the C_int data structures.
    */
   taskCode = reqHdr.taskCode;
   wdePtr->taskCode = taskCode;
   pDS->taskCode = taskCode;
#ifdef XABUILD
   /*
    * In the JDBC XA Server: Get the odtpTokenValue from
    * the request header, save it in the wde. At this point,
    * the WDE should now have both the odtpTokenValue value
    * and the txn_flag value (saved in XAServerWorker()
    * just before the processTask() call).
    */
   wdePtr->odtpTokenValue = reqHdr.odtpTokenValue;
#endif

   /* Bump the counter for the number of request packets.
      Set the timestamp for the last request packet,
      and save the task code. */
   sgdPtr->requestCount ++;
   sgdPtr->lastRequestTaskCode = taskCode;
   UCSDWTIME$(&sgdPtr->lastRequestTimeStamp);

   /* Save as the timestamp of the last (latest) request packet in this clients WDE. */
   wdePtr->lastRequestTimeStamp = sgdPtr->lastRequestTimeStamp;

   /*
    * Now process any debugging info
    * that might be included in the packet.
    *
    * First, get the debugging flag information from the request packet.
    * The debug indicators sent in the request header are those set in
    * the RdmsXXXX objects whose request packet we are processing.
    */
   debugFlags=reqHdr.debugFlags;
#ifdef XABUILD  /* JDBC XA Server */
   /*
    * Test for the case of a non-transactional XAConnection client that has
    * lost their JDBC XA Server copy due to the stateful_timeout (i.e., the
    * JDBC XAConnection client took too long between requests).  We have to
    * separate this possiblility from the normal (correct) assignment of this
    * JDBC XA Server to a new JDBC XAConnection client.
    *
    * Test two things: 1) Is the odtpTokenValue in the request packet different
    *                     from the one we have been operating under.  If different,
    *                     then we either have a non-transactional client trying
    *                     to use a previously XA-transactional JDBC XA Server
    *                     or visa versa. If the same, then we have either a
    *                     non-transactional JDBC client returning to its assigned
    *                     JDBC XA Server stetfully or we have a new XA client accessing
    *                     a currently XA-transactional JDBC XA Server. This same case
    *                     is OK. The different case indicates a possible lost stateful
    *                     JDBC XA Server to the stateful_timeout condition or a
    *                     truly new client condition - for this we need the second test.
    *                  2) If the odtpTokenValues are not the same, then the only legal
    *                     situation is when we are either testing user id and passwords
    *                     or when we are doing a begin thread task (either XA or
    *                     non-transactional). All other request tasks indicate an error.
    */
   if (reqHdr.odtpTokenValue != wdePtr->odtpTokenValue &&
       !(taskCode == USERID_PASSWORD_TASK || taskCode == XA_BEGIN_THREAD_TASK || taskCode == BEGIN_THREAD_TASK) ){
        /* Non-transactional client timed out as is trying to continue with a new JDBC XA Server. */
        msg2 = getLocalizedMessageServer(XASERVER_STATEFUL_CLIENT_ERROR,
                                         itoa(reqHdr.odtpTokenValue,digits,10),
                                         itoa(wdePtr->odtpTokenValue,digits2,10), 0, error_message);
            /* 5432 Non-transactional XAConnection accessing transactional JDBC XA
                    Server ({0}, {1}), increase JDBC XA Server's ODTP stateful_timeout value. */
        response_ptr = genErrorResponse(C_INTERFACE_ERROR_TYPE,
            XASERVER_STATEFUL_CLIENT_ERROR, 0, msg2, FALSE);
        *response_ptr_ptr=response_ptr;
        return JDBCSERVER_PROCESSTASK_RETURNED_ERROR;
   }

   /*
    * In the JDBC XA Server case, check if we have an USERID_PASSWORD_TASK task.
    * If so, it may have debug flags but will have no sfile specification (by
    * design). We need to set debugUserPassValidation_inXAmode so the task is
    * traced and to turn off the regular debug flags for this one task. This way
    * we get USERID_PASSWORD_TASK task tracing sent to the JDBC XA Server's
    * stdout (PRINT$) file, but won't get a default client trace file created
    * (which we don't want since many USERID_PASSWORD_TASK tasks can get sent
    * in a row to be processed and they could go to this one copy of the JDBC
    * XA Server resulting in lots of small client trace files and fragmented
    * trace output when in a connection pool environment).  So, the trace output
    * will be sent to the JDBC XA Server's stdout PRINT$ file (e.g. *jdbcxa3brk.).
    *
    * For all other tasks, we will continue to do the regular debug flag and trace
    * file name handling, like the other Server modes.
    */
   if (taskCode == USERID_PASSWORD_TASK){
    debugUserPassValidation_inXAmode = BOOL_TRUE;
    debugFlags=0; /* Debugging in that task will be handled by other flag */
   }
#endif
   wdePtr->debugFlags=debugFlags;

   /*
    * Set the debugXXXX flags from the debugFlags retrieved from request packet.
    * If the debugBrief bit is off, then tracing is not currently active.
    * This is a special indicator sent from the client side which is valid even though
    * the debugClassIndex mask and the debugFlags indicator word might indicate
    * otherwise - this mechanism passes the isTraceOn value from the client side
    * RdmsXXXX creator object without requiring additional packet space.
    *
    * This works because we do not use the debugBrief indicator in the Server side
    * code.  The weakest tracing will occur when debugDetail is set. We will set
    * debugBrief to the debugFlags value if tracing is turned on.
    */
   debugBrief=((debugFlags & DEBUG_BRIEFMASK)>0)?BOOL_TRUE:BOOL_FALSE;
   if (debugBrief == BOOL_TRUE) {
     /*
      * Tracing is turned on at this time, so set up the other
      * debugXXXX indicators as dictated by the debugFlags. These debugXXXX
      * variables will actually be used to determine if debug calls are
      * to be executed (e.g. if (debugBrief){traceMethod(....)} ).
      */
     debugInternal=((debugFlags & DEBUG_INTERNALMASK)>0)?BOOL_TRUE:BOOL_FALSE;
     if (debugInternal == BOOL_TRUE) {
       debugDetail=BOOL_TRUE;
     }
     else {
       debugDetail=((debugFlags & DEBUG_DETAILMASK)>0)?BOOL_TRUE:BOOL_FALSE;
     }

     /*
      * Set the SQL debug indicator flag. If set, SQL commands are to be
      * traced. Notice, the SQL traces may also occur as part of the
      * debugDetail or debugInternal controlled trace output.
      *
      * Also, if debugSQL is set, then check for possibility that debugSQLE
      * and/or debugSQLP need to be also set.
      */
     debugSQLE=BOOL_FALSE;  /* clear them first, in case debugSQL is not set */
     debugSQLP=BOOL_FALSE;
     if ((debugSQL=((debugFlags & DEBUG_SQLMASK)>0)?BOOL_TRUE:BOOL_FALSE) == BOOL_TRUE){
        /* debug SQL is set, set debugSQLE and debugSQLP accordingly */
        debugSQLE=((debugFlags & DEBUG_SQLEMASK)>0)?BOOL_TRUE:BOOL_FALSE;
        debugSQLP=((debugFlags & DEBUG_SQLPMASK)>0)?BOOL_TRUE:BOOL_FALSE;
     }
   }
   else {
    /*
     * Tracing is not on at this time, so just set the other debugXXXX
     * indicators to false.
     */
     debugInternal=BOOL_FALSE;
     debugDetail=BOOL_FALSE;
     debugSQL=BOOL_FALSE;
     debugSQLE=BOOL_FALSE;
     debugSQLP=BOOL_FALSE;
   }

   /*
    * In a JDBC Server or JDBC XA Server, tracing is done for a Java client's
    * through the rdmsTrace sfile(), so set up the necessary trace stuff and
    * get access to the appropriate client trace file.
    */
   /* Get the debug prefix string.  Note: this string could be an empty string. */
   strcpy(debugPrefix,reqHdr.debugPrefixString); /* always get the debugPrefix */

   /* test if debug prefix is non-empty.  If empty, we don't need to display it! */
   if (strlen(debugPrefix) !=0){
      debugPrefixFlag=BOOL_TRUE; /* indicate that we will be adding a prefix */
   }else{
      /* No prefix string, so set indicator flag to false */
      debugPrefixFlag=BOOL_FALSE; /* indicate that we will not be adding a prefix */
   }

   /* Set the time stamp prefix indicator. It may be used in trace messages. */
   debugTstampFlag=((debugFlags & DEBUG_TSTAMPMASK)>0)?BOOL_TRUE:BOOL_FALSE;

   /* Set local debug flags from the sgd.  In Server mode, the sgd values can
    * override the JDBC Client's values and turn debugging on.
    * In these cases, the debugTstamp value is always set to true.
    */
   if (sgdPtr->clientDebugInternal) {
       debugInternal = TRUE;
       debugTstampFlag = TRUE;  /* also set timestamp on. */
       debugPrefixFlag=BOOL_TRUE; /* indicate that we will be adding prefix */
   }

   if (sgdPtr->clientDebugDetail) {
       debugDetail = TRUE;
       debugTstampFlag = TRUE;  /* also set timestamp on. */
       debugPrefixFlag=BOOL_TRUE; /* indicate that we will be adding prefix */
   }

   /*
    * Now: If the DEBUG compile time value is set to true, set the
    * debugBrief, debugDetail, and debugInternal to true (this may override
    * the values these variables received from the request packet. Note
    * that the trace file used will be the default one associated to the
    * Server Worker working for this client.
    */
    #if (DEBUG)
      debugInternal=BOOL_TRUE;  /* turn on debug */
      debugDetail=BOOL_TRUE;
      debugBrief=BOOL_TRUE;
      debugTstampFlag = TRUE;       /* also set timestamp on. */
      debugPrefixFlag=BOOL_TRUE;    /* indicate that we will be adding prefix */
    #endif

   /* Indicate whether we will either add a prefix or timestamp on first tracef() */
   if(debugPrefixFlag == BOOL_TRUE || debugTstampFlag == BOOL_TRUE){
        debugPrefixOrTstampFlag=BOOL_TRUE;
   }else{
        debugPrefixOrTstampFlag=BOOL_FALSE;
   }

   /* Now we can setup the debug trace file.
    *
    * JDBC (XA) Server - Trace File Setup::
    * - If trace file set by console, then skip file setup
    *   - If reqPkt passed a trace file name, get it.
    *     - Close existing client trace file if it was open
    *     - If no filename passed, generate a default filename.
    *     - Add qualifer to name if needed.
    *     - Save fully qualified name to WDE.
    *   - If debugging on and trace file not opened yet.
    *     - Open the trace file and write initial log entries.
    *
    * Just in case there is no debugInfoOffset, clear
    * fileNameLen and sfileName in case debugging tries to print them.
    */
   sfileNameLen=0;
   sfileName[0]='\0';
   eraseTraceFileFlag = BOOL_FALSE;

   /* In JDBC Server or JDBC XA Server only:
      Don't do anything with the client trace file,
      if the console command
          SET CLIENT id TRACE FILE   (Local Server)
          SET CLIENT TRACE FILE      (XA Server)
      was specified.
      This console command overrides any subsequent
      client sfile debug file specifications from request packets
      from the client (Local Server) or the XA Server run (XA Server). */
   if (! wdePtr-> consoleSetClientTraceFile) {
   /*
    * Now test if a debug info area is present in the request packet.
    *
    * If debug info is present, determine what format the debug information is
    * in. Currently, only debug info in format 0 is being used (debugInfo_ID=0).
    * In debug info format 0, the debug info area contains a file name for
    * the client-side trace file. The debug info area is usually present only
    * in the Userid validation or Begin Thread tasks request packet since these
    * are the first request packets from the client.
    *
    * Note: In the XA_TRANSACTION case, the userid password validation task will
    * be sent without any client debug filename information (since it will appear
    * later on the begin thread task). Since there may be multiple userid
    * validation tasks processed in a row, their trace output should go to the
    * XA Server's PRINT$ file (stdout) so it is available, but doesn't mess up
    * normal debugging.
    */
   if (reqHdr.debugInfoOffset != 0){
      /* get the client debug filename from the debug entry in the request packet */
      debugInfoID = getRequestDebugInfo_0(&sfileNameLen, sfileNamePtr, &eraseTraceFileFlag,
                                          request_ptr, &reqHdr.debugInfoOffset);

      /* Test that debug info is in format 0 - only one  supported at this time */
      if (debugInfoID !=0){
        /* Debug info not in format 0, generate an error message and return it to client */
        msg2 = getLocalizedMessageServer(SERVER_BAD_DEBUG_INFO_FORMAT_IN_REQPKT,
                                         itoa(debugInfoID,digits,10), NULL, 0, error_message);
                /* 5409 Invalid debug information id ({0}) in request packet. */
        response_ptr = genErrorResponse(JDBC_SERVER_ERROR_TYPE,
                                        SERVER_BAD_DEBUG_INFO_FORMAT_IN_REQPKT,
                                        0, msg2, FALSE);
        *response_ptr_ptr=response_ptr;
        return SERVER_BAD_DEBUG_INFO_FORMAT_IN_REQPKT;
      }

      #if (DEBUG)
      printf("Request Packet debug info area has debugFlags = %i, sfileNameLen= %i, sfileName= %s, debugPrefix= %s, debugInfoID= %i\n",
            debugFlags, sfileNameLen, sfileName, debugPrefix, debugInfoID);
      printf("                                   eraseTraceFileFlag= %i\n",
            eraseTraceFileFlag);
      #endif

      /* First, close the existing client trace file if it is open. */
      error_status=closeClientTraceFile(wdePtr,error_message,error_len);

      /* Now test the status on the trace file closing. */
      if (error_status !=0) {
        /* generate an error message and return it to client */
        response_ptr = genErrorResponse(JDBC_SERVER_ERROR_TYPE,
            JDBCSERVER_CANT_CLOSE_CLIENT_TRACEFILE, 0, error_message, FALSE);
                /* Unable to close JDBC Client's server-side trace file */
        *response_ptr_ptr=response_ptr;
        return JDBCSERVER_PROCESSTASK_RETURNED_ERROR;
      }

      /*
       * We successfully closed the previous client trace file.
       *
       * So now construct and save the new client trace file name, based on the server
       * mode (JDBC Server or XA Server) and what base sfileName that has
       * been provided.
       *
       * There are four types of sfile name specifications we can encounter. We can test
       * for them, and process them in order from first type to last. (Although we may
       * do an extra string comparison or two, no extra or incorrect work will be done
       * processing them in this straightforward manner.)
       *
       * Type-1:  The sfileName is the default sfile indicator "[file]nn". This indicator
       *          is specifically for use in Direct mode. It indicates that the service-side
       *          trace should be sent back to Java to be added to the client-side trace
       *          file as defined by the rdmstrace file() trace property. If we are using
       *          a JDBC Server or JDBC XA Server, then the "[file]nn" indicator is replaced
       *          with "[default]nn" and handled as a type 2 specification (as follows).
       *
       *          For a JDBC Server or JDBC XA Server:
       *            The "[file]nn" indicator is replaced with "[default]nn" and
       *            handled as a type 2 specification (as follows).
       *
       * Type-2:  The sfileName is the default sfile indicator "[default]nn",
       *          where nn is the connection number formed on the client-side by
       *          # substitution on the "[default]#" default sfile trace file
       *          indicator. If so, use the nn portion and construct the default
       *          sfile name based on the server mode.
       *
       *          For a JDBC Server:
       *            Use a trace file name of "<qual>*jdbctrc-#", where # is the nn
       *            value and <qual> is this JDBC Server's configuration default
       *            client trace file qualifier.
       *
       *          For a JDBC XA Server:
       *            Use a trace file name of "<qual>*jdbct-<grunid>", where <grunid>
       *            is the JDBC XA Server's generated runid value and <qual> is this
       *            JDBC XA Server's configuration default client trace file qualifier.
       *            Notice that the nn value is not used, since the generated runid
       *            provides uniqueness and it would be possible for two JDBC XA
       *            Server's to access the same file (bad) if the nn value was used
       *            in the filename (like done in the JDBC Server case). The 'generated
       *            runid' for XA Servers will be appended to the trace filename.
       *
       * Type 2.5: The sfileName is "PRINT$". The trace will be placed into the JDBC
       *           Server or JDBC XA Server's standard PRINT$ file (which is already
       *           breakpointed to a *jdbcrunbrk or *JDBC$XABRK file, respectively.
       *           So no other adjustments are needed here - openClientTraceFile()
       *           and closeClientTraceFile() will handle the special details later.
       *
       * Type-3:  The sfileName is a regular filename, with or without a qualifier. This
       *          filename must undergo the needed steps to form a fully qualified (q*f)
       *          file name based on how complete the filename is and what type of server
       *          is being used.
       *
       *          For a JDBC Server:
       *            If there is no explicit qualifier present, the JDBC Server's
       *            configuration default client trace file qualifier is added. The
       *            file name is then complete.
       *
       *          For a JDBC XA Server:
       *            If there is no explicit qualifier present, the JDBC Server's
       *            configuration default client trace file qualifier is added. The
       *            filename portion is modified by truncating it to a maximum of
       *            5 characters and adding the string "-<grunid>" to the end, where
       *            <grunid> is the JDBC XA Server's generated runid value. The
       *            file name is then complete.
       *            Note: Using the 'generated runid' will insure that a different
       *            trace file name is used when multiple XA connections are created
       *            from a managed connection pool and the specified 'sfile' name is
       *            the same for each connection. The 'generated runid' will be unique
       *            for each connection.
       *
       * Note: The supplied sfilename never has a trailing period. It is stripped on
       * the Java side. File assignment code will add it back, if needed.
       *
       * Test for a Type-1 sfilename (e.g., "[file]nn").
       */
      if (strncmp(sfileName, RDMSTRACE_SFILE_TO_FILE_ARGUMENT, RDMSTRACE_SFILE_TO_FILE_ARGUMENT_LEN) == 0){
          /*
           * For JDBC Server and JDBC XA Server replace the Type-1 sfileName
           * with the sfile default argument specification, and then handle
           * it as a Type-2 sfilename. Don't forget to save and add back the
           * nn connection number.
           */
          strcpy(digits, &sfileName[RDMSTRACE_SFILE_TO_FILE_ARGUMENT_LEN]); /* Save the nn value */
          strcpy(sfileName, RDMSTRACE_SFILE_DEFAULT_ARGUMENT);
          strcat(sfileName, digits);

      } /* Done with Type-1 sfileName testing/processing */

      /* Now test for a Type 2 sfilename (e.g., "[default]nn").  */
      if (strncmp(sfileName, RDMSTRACE_SFILE_DEFAULT_ARGUMENT, RDMSTRACE_SFILE_DEFAULT_ARGUMENT_LEN) == 0){
#ifndef XABUILD        /* JDBC Server */
          /*
           * For JDBC Server, use the default trace file base name and then handle
           * it as a Type-3 sfilename. Don't forget to add the nn connection number.
           */
          strcpy(digits, &sfileName[RDMSTRACE_SFILE_DEFAULT_ARGUMENT_LEN]); /* Save the nn value */
          strcpy(sfileName, sgdPtr->defaultClientTraceFileQualifier);
          strcat(sfileName, RDMSTRACE_DEFAULT_SERVER_FILE_NAME);
          strcat(sfileName, digits);
#else                  /* JDBC XA Server */
          /*
           * For JDBC XA Server, use the default trace file base name with the
           * generated runid appended at the end, then handle it as a Type-3
           * sfilename. (Remember, we use the generated runid for uniqueness
           * and not use the nn connection number).
           */
          strcpy(sfileName, sgdPtr->defaultClientTraceFileQualifier);
          strcat(sfileName, RDMSTRACE_DEFAULT_XASERVER_FILE_NAME);
          strcat(sfileName, sgdPtr->generatedRunID);
#endif                 /* JDBC XA Server */
      } /* Done with Type-2 sfileName testing/processing */

      /*
       * Now handle the sfileName as a Type 2.5 or Type 3 (e.g., "PRINT$", Q*F, *F, or F).
       * For the JDBC Server and JDBC
       * XA Server, proceed with the code to set up and open the Type-3 sfileName.
       *
       * Now check for the Type 2.5 case ("PRINT$"). If so, no other
       * adjusting of the "filename" is needed, so we only need to save
       * the trace file name and proceed past Type 3 case handling.
       */
      if (strcmp(sfileName, PRINT$_FILE) == 0){
          /*
           * Type 2.5 case, "PRINT$" found. Leave sfileName file name as is.
           */
          strcpy(wdePtr->clientTraceFileName,sfileName);
      }

      /*
       * For all Server modes, modify the Type-3 sfileName and
       * open the file appropriately.
       *
       * First, test if the qualifier was provided by looking for a leading
       * '*' (i.e., the file was specified in the *fn. format).  If found in
       * that form, add or not add the configurations default qualifier value
       * to the sfileName based on the server mode. If a leading "*"
       * was not found, leave the base filename alone except in the JDBC XA
       * Server case. In that case, we will change the filename portion to
       * include the generated runid.
       */
      else if (strchr(sfileName,'*')== NULL) {
          /*
           * In JDBC Server and JDBC XA Server: There is no qualifier,
           * so use the configuration's default qualifier followed by
           * the file name to form the clients trace file name.
           */
          strcpy(wdePtr->clientTraceFileName, sgdPtr->defaultClientTraceFileQualifier);
          strcat(wdePtr->clientTraceFileName,"*");
          #ifdef XABUILD   /* JDBC XA Server */
              /* For XA, only use up to 5 characters from the name and append -<grunid>. */
              strncat(wdePtr->clientTraceFileName,sfileName,5);
              strcat(wdePtr->clientTraceFileName, "-");
              strcat(wdePtr->clientTraceFileName,sgdPtr->generatedRunID);
          #else            /* JDBC Server */
              strcat(wdePtr->clientTraceFileName,sfileName);
          #endif           /* JDBC Server and JDBC XA Server */
      }
      else {
              /* In JDBC Server and JDBC XA Server: the supplied filename has a qualifier. */
          #ifdef XABUILD   /* JDBC XA Server */
              /* For XA, only use up to 5 characters from the name and append -<grunid>. */
              /* Note: the length for the strncpy is computed as follows:
               *       Ptr2(location of '*' - Ptr1(location of sfilename)  ==> length of the qualifier
               *       + 1 (for the '*'
               *       + 5 (for the length of filename to use
               * Also make sure the strncpy is NULL terminated.
               * If the filename part is 5 or greater, the strncpy will not null terminate.
               */
              filelen = strchr(sfileName,'*') - sfileName + 1 + 5;
              strncpy(wdePtr->clientTraceFileName, sfileName, filelen);
              wdePtr->clientTraceFileName[filelen] = '\0';
              strcat(wdePtr->clientTraceFileName, "-");
              strcat(wdePtr->clientTraceFileName,sgdPtr->generatedRunID);
          #else            /* JDBC Server */
              strcpy(wdePtr->clientTraceFileName,sfileName);
          #endif           /* JDBC Server */
      }

      #if (DEBUG)
      tracef("Requested client trace file name= %s\n",wdePtr->clientTraceFileName);
      #endif

     /* Done with Type-3 sfileName testing/processing */
   } /* Done obtaining and processing the Debug Info Area's sfileName value */

   /*
    * Now that we've handled the setting up of any server-side trace files:
    *
    * For each request packet, before processing the task we need to check
    * if we have active server-side client debugging occurring ( debugDetail is
    * true, or debugSQL is true ).
    *
    * If so, test if we need to open the client trace file. If debugDetail is
    * set to true and the client trace file is not open, then open the client
    * trace file.  If neither of those conditions are ever met, there is no need
    * to open the client trace file - improves performance.  The C-Interface code
    * will test one of the debugXXXX indicators to determine when to trace, so
    * it is alright to delay this action until then. The clientTraceFile activity
    * visible variable is defined so the trace code can easily reference the file
    * handle in their tracef ( or fprintf() ) function calls.
    */
   if (debugDetail == BOOL_TRUE || debugSQL == BOOL_TRUE){

     /* Test if 2200 trace file is open, for those cases using a 2200 file. */
     if (wdePtr->clientTraceFile == NULL){
        /*
         * We have to open the trace file. The candidate filename was
         * placed into the WDE (clientTraceFileName). The openClientTraceFile()
         * routine will update this value with the fully qualified filename
         * including the absolute cycle number.
         *
         *
         * The client trace file cycling routine will use it to form the actual
         * file (with cycle). It may change the type of file if necessary.
         * When completed, sfileName will still have original packet's
         * file name and the WDE and generatedTracefileName will both have
         * the actual file name used including the file cycle. The variable
         * generatedTracefileNameType will indicate the type of file
         * actually opened. The requested filename after any qualifications
         * were made, etc., will kept in requestedFileName.
         *
         * Return an error response if a client trace file cannot
         * be opened.  In this case, we cannot continue with the task's
         * execution.
         */
        error_status = openClientTraceFile(eraseTraceFileFlag,
                            wdePtr, error_message, error_len);
        if (error_status !=0) {
           /* Log the generated error message and return it to client */
           addServerLogMessage(error_message);
            response_ptr = genErrorResponse(C_INTERFACE_ERROR_TYPE,
                JDBCSERVER_CANT_OPEN_CLIENT_TRACEFILE, 0, error_message, FALSE);
                    /* Unable to open JDBC Client's server-side trace file */
           *response_ptr_ptr=response_ptr;
           return JDBCSERVER_PROCESSTASK_RETURNED_ERROR;
        }

        /* We opened a trace file.  Set indicator to return file name
         * as part of the response packet.
         */
        taskOpenedClientTracefile = BOOL_TRUE;

        /* Check if we need to initialize the Trace file output. */
        if (wdePtr->clientTraceFile != NULL) {
            clientTraceFileInit(wdePtr);
        }
     }
   }
  /* end of client trace file code:
      if (! act_wdePtr-> consoleSetClientTraceFile) ... */
  }

   /* Test if we should trace the fact that we are processing a request packet */
   if (debugDetail){
     /* Yes, get timestamp for trace image. */
     tracef("** Within processClientTask, starting a %s, at %s\n",
            taskCodeString(taskCode), getDateTime(NULL, dateTimeString));
   }

   /* If DEBUG or debugInternal is set, trace the debug indicators themselves */
   if (DEBUG || debugInternal){
      tracef("In processClientTask(): debugFlags = %i, sfileName= %s, wdePtr->clientTraceFileName= %s, debugPrefix= %s\n",
             debugFlags, sfileName, wdePtr->clientTraceFileName,debugPrefix);
   }

   /* Set response_ptr to NULL before making the call to the C-Interface library function,
    * the C function will set it to a pointer to a response packet.
    */
   response_ptr =NULL;

   #if (DEBUG)
   tracef("*** In Procedure processClientTask, task code being processed is = %d \n",taskCode);
   #endif

   returnStatus =0;

   /*
    * Do a switch statement to find the right function to call. Immediately
    * before the switch statement set the flag indicating we are doing
    * C-Interface code processing.  This is used by the signal handler in
    * deciding what type of recovery handling that can be done for the
    * Server Worker activity. (Don't quibble about the fact that the indicator
    * is set one statement before the switch operation. Logically, we are doing
    * Clib processing at this point.
    */
   insideClibProcessing=BOOL_TRUE;
   switch (taskCode) {
       /*
        * Each case entry has the call to the C Interface Library code to
        * process the task specified by the taskCode, e.g. func_name. All the
        * functions that are called here should have two parameters at least:
        * the request packet and a response packet which are referenced via
        * pointers and are the first two parameters, in that order, to the
        * function. (Note: The one exception is the keepAlive task which is
        * provided within theJDBC Server code inside ProcessTask.c.)
        *
        * Once the called function gets control, there is normally no access
        * to any of the JDBC Server's global data (via sgd) or the Server Workers
        * description data (via wdePtr). So, if any of that information is needed
        * by the function it should be placed into either activity visible storage
        * before the function call, or added as an additional third parameter, or
        * more to the function call.
        *
        * A typical task case would have the following format:
        *             case DUMMYTASK1:{
        *
        *                 <get any global info needed from JDBC Server>
        *
        *                 stat=func_name(request_ptr,&response_ptr, extra_params...);
        *
        *                 <return any global info needed by the JDBC Server>
        *
        *                 break;
        *                 }
        * where:
        *  func_name    -   The name of the C function to call.
        *  request_ptr  -   An char pointer to the input request packet.
        *  response_ptr -   An char pointer to the response packet. It is
        *                   set to NULL before the switch statement.The
        *                   space for the response packet normally will be
        *                   allocated by the C code in the task calling the
        *                   function allocateResponsePacket().
        *  extra_params -   Any additional parameters that may be needed.
        *  stat         -   The returned status, 0=no error, != 0 is an
        *                   error indicator (e.g. allocation error).
        *
        */

       case COMMIT_THREAD_TASK: {
           returnStatus = commitRdmsThreadTask(request_ptr,&response_ptr);
           break;
       }
       case STMT_EXECUTE_QUERY_TASK:
       case STMT_EXECUTE_UPDATE_TASK:
       case STMT_EXECUTE_TASK: {
           /* Does this level of RDMS support RDMS SQL section usage? */
           if (!FF_SupportsSQLSection){
               /* No, Just execute the SQL command from the request packet, as it is. */
               returnStatus = executeRdmsStatement(request_ptr,&response_ptr, FALSE, NULL, FALSE, TRUE);
           } else {
               /*
                * Yes, adjust the executeRdmsStatement() call based on JDBC Section availability.
                *
                * We want to utilize RDMS SQL sections if available to accelerate the SQL
                * command execution, so tell executeRdmsStatement() to:
                *     1) Use the JDBC Section in the request packet if present.
                *     2) Indicate we don't have an JDBC Section already in the PCA.
                *     3) We want any obtained JDBC Section returned.
                *     4) Do any cursor dropping as indicated in the request packet.
                */
               returnStatus = executeRdmsStatement(request_ptr, &response_ptr, TRUE, NULL, TRUE, TRUE);

               #if (DEBUG)
                    tracef("Returned from executeRdmsStatement with status = %d\n", returnStatus);
               #endif

               /*
                * Did we receive an invalid RDMS SQL section error (6025 or 4025)? (Remember these errors don't
                * ever occur with a pre-RDMS 17R2.)
                *
                * RDMS error        6025 - Indicates that the RDMS SQL section that was utilized is no longer valid.
                *                          This occurs because an ALTER TABLE command (not necessarily on this
                *                          RDMS database thread) has been successfully executed that changed
                *                          the table definitions sufficiently that the RDMS SQL section is no
                *                          longer valid.
                * Psuedo-RDMS error 4025 - Indicates that the JDBC execute code has detected that the RDMS SQL section
                *                          does not have a valid section verify id value.  This occurs when a
                *                          USE or ALTER command (of any type) has been executed since the SQL
                *                          section was obtained by JDBC from RDMS.
                *
                * In either of these two cases, the requested task must be re-executed using the actual SQL
                * command string. Since the request packet was already executed, tell executeRdmsStatement()
                * the following on the second call:
                *     1) Do not use the JDBC Section (and its RDMS SQL section) from the request packet if
                *        one is present. Instead use the SQL command string that is in the request packet.
                *     2) Indicate we will not use an JDBC Section and it's RDMS SQL section that may be in the PCA.
                *     3) Do not do any cursor dropping operation indicated in the request packet since
                *        the previous executeRdmsStatement() has already handled any cursor dropping.
                *
                * If the execute retry produces a new section/metadata, this information will be sent
                * back to the client-side to form a new sharedInfo object to be used on later executions.
                *
                */
               if (returnStatus == RDMS_INVALID_SQL_SECTION || returnStatus == RDMSJDBC_INVALID_VERIFICATION_ID){
                   /* Yes, release the original response packet and re-try using the SQL command string. */
                   releaseResPktForReallocation(response_ptr);
                   returnStatus = executeRdmsStatement(request_ptr,&response_ptr, FALSE, NULL, TRUE, FALSE);
               }
               #if (DEBUG)
                    tracef("Returned from retry executeRdmsStatement with status = %d\n", returnStatus);
               #endif
           }
           break;
       }
       case PREPARED_SUBSEQUENT_EXECUTE: {
           /* Does this level of RDMS support RDMS SQL section usage? */
           if (!FF_SupportsSQLSection){
               /* No, Just re-execute the SQL command from the request packet, as it is. */
               returnStatus = executeAgain(request_ptr,&response_ptr, FALSE, NULL, FALSE, TRUE);
           } else {
               /*
                * Yes, adjust the executeRdmsStatement() call based on JDBC Section availability. Use the
                * same initial settings for executeAgain() used on the initial executeRdmsStatement() call.
                */
               returnStatus = executeAgain(request_ptr,&response_ptr, TRUE, NULL, TRUE, TRUE);
               #if (DEBUG)
                    tracef("Returned from executeAgain with status = %d\n", returnStatus);
               #endif

                /*
                * Did we receive an invalid RDMS SQL section error (6025 or 4025)? (Remember these errors don't
                * ever occur with a pre-RDMS 17R2.)
                *
                * In either of these two cases, the requested task must be re-executed using the actual SQL
                * command string. Use the same settings for the retry of executeAgain() that we used on the
                * retry of the executeRdmsStatement() call above.
                */
               if (returnStatus == RDMS_INVALID_SQL_SECTION || returnStatus == RDMSJDBC_INVALID_VERIFICATION_ID){
                   /* Yes, release the original response packet and re-try using the SQL command string. */
                   releaseResPktForReallocation(response_ptr);
                   returnStatus = executeAgain(request_ptr,&response_ptr, FALSE, NULL, TRUE, FALSE);
               }
               #if (DEBUG)
                    tracef("Returned from retry executeAgain with status = %d\n", returnStatus);
               #endif
           }
           break;
       }
       case STMT_EXECUTE_BATCH_TASK: {
           returnStatus = executeBatchTask(request_ptr,&response_ptr);
           #if (DEBUG)
               tracef("Returned from executeBatchTask with status = %d\n", returnStatus);
           #endif
        break;
       }
       case STMT_COMPLETE_TASK: {
           returnStatus = completeStatementTask(request_ptr,&response_ptr);
           #if (DEBUG)
                tracef("Returned from completeStatement with status = %d\n", returnStatus);
           #endif
           break;
       }
       case STMT_DROP_CURSOR_TASK:
       case STMT_DROP_CURSOR_AND_MD_TASK: {
           returnStatus = dropCursorTask(request_ptr,&response_ptr);
           break;
       }
       case NEXT_TASK: {
           returnStatus = getRowsOfData(request_ptr,&response_ptr);
           break;
       }
       case ASYNCH_NEXT_N_TASK:
       case NEXT_N_TASK: {
           returnStatus = getNRowsOfData(request_ptr,&response_ptr);
           break;
       }
       case POSITIONED_FETCH_TASK: {
           returnStatus = positionedFetchRow(request_ptr,&response_ptr);
           break;
       }
       case GET_BLOB_DATA_TASK: {
           returnStatus = getBlobData(request_ptr,&response_ptr);
           break;
       }
       case SET_BLOB_BYTES_TASK: {
           returnStatus = setBlobBytes(request_ptr,&response_ptr);
           break;
       }
       case TRUNCATE_BLOB_TASK: {
           returnStatus = truncateBlob(request_ptr,&response_ptr);
           break;
       }
       case GET_LOB_HANDLE_TASK: {
           returnStatus = getBlobHandle(request_ptr,&response_ptr);
           break;
       }
       case DBMD_BRI_TASK: {
           returnStatus = getBestRowId(request_ptr,&response_ptr);
           break;
       }
       case DBMD_CP_TASK: {
           returnStatus = getColPriv(request_ptr,&response_ptr);
           break;
       }
       case DBMD_C_TASK: {
           returnStatus = getCols(request_ptr,&response_ptr);
           break;
       }
       case DBMD_CR_TASK: {
           returnStatus = getCrossRef(request_ptr,&response_ptr);
           break;
       }
       case DBMD_EK_TASK: {
           returnStatus = getExpKeys(request_ptr,&response_ptr);
           break;
       }
       case DBMD_IK_TASK: {
           returnStatus = getImpKeys(request_ptr,&response_ptr);
           break;
       }
       case DBMD_II_TASK: {
           returnStatus = getIdxInfo(request_ptr,&response_ptr);
           break;
       }
       case DBMD_PK_TASK: {
           returnStatus = getPrimKeys(request_ptr,&response_ptr);
           break;
       }
       case DBMD_PC_TASK: {
           returnStatus = getProcCols(request_ptr,&response_ptr);
           break;
       }
       case DBMD_P_TASK: {
           returnStatus = getProc(request_ptr,&response_ptr);
           break;
       }
       case DBMD_TP_TASK: {
           returnStatus = getTabPriv(request_ptr,&response_ptr);
           break;
       }
       case DBMD_VER_TASK: {
           returnStatus = getRsaVersion(request_ptr,&response_ptr);
           break;
       }
       case DBMD_T_TASK: {
           returnStatus = getTables(request_ptr,&response_ptr);
           break;
       }
       case DBMD_TT_TASK: {
           returnStatus = insertTableTypes(request_ptr,&response_ptr);
           break;
       }
       case DBMD_TI_TASK: {
           returnStatus = insertTypeInfo(request_ptr,&response_ptr);
           break;
       }

       case RELEASE_METADATA_BUFFERS_TASK: {
           returnStatus = releaseMetaDataBuffersTask(request_ptr,&response_ptr);
           break;
       }

       case BEGIN_THREAD_TASK:
       case XA_BEGIN_THREAD_TASK: {

           /*
            * Use the timestamp of this first, and latest (last) request
            * packet as the first request packet timestamp for this client.
            *
            * This is true since each connection must begin with a Begin Thread
            * task packet.
            */
           wdePtr->firstRequestTimeStamp = wdePtr->lastRequestTimeStamp;

           returnStatus = doBeginThread(request_ptr,&response_ptr);

           /*
            * According to information from the RSA group, either
            * the RDMS  Begin Thread worked ( returnStatus == 0),
            * or we do not have a RDMS Thread (returnStatus !=0).
            * So test the returnStatus and set workingOnaClient
            * in the wde to false if no thread is there ( we will
            * not be continuing the Server Worker dialogue on this
            * Connection. Also set openRdmsThread to indicate if a
            * Rdms thread is open.
            *
            */
           if (returnStatus ==0){
               wdePtr->openRdmsThread = BOOL_TRUE;   /* indicate an open Rdms thread */
           }
           else {
               /* non-0, signal that we are done with this JDBC Client after this packet */
               wdePtr->workingOnaClient=BOOL_FALSE;
               wdePtr->openRdmsThread = BOOL_FALSE;   /* make sure we indicate no open Rdms thread */
           }
           break;
       }
       case ROLLBACK_THREAD_TASK: {
           returnStatus = rollbackRdmsThreadTask(request_ptr,&response_ptr);
           break;
       }
       case CONN_AUTOCOMMIT_TASK: {
           returnStatus = doAutoCommit(request_ptr,&response_ptr);
           break;
       }
       case USERID_PASSWORD_TASK: {
           returnStatus = useridPasswordTask(request_ptr,&response_ptr, debugUserPassValidation_inXAmode);

           /*
            * Test returnStatus to see if the userid and password validation
            * did not succeed. If so, the client will get an error and we
            * are done with this JDBC Client after this packet. (Remember,
            * no RDMS thread was opened by this task, either way).
            */
            if (returnStatus !=0){
               wdePtr->workingOnaClient=BOOL_FALSE;
            }
           break;
       }
       case END_THREAD_TASK: {
           returnStatus = doEndThread(request_ptr,&response_ptr);

           /*
            * According to information from the RSA group, either
            * the RDMS thread was closed ( returnStatus == 0),
            * or we do not have a RDMS Thread to close (returnStatus !=0).
            * Either way, we no longer will ned to work on the JDBC Client
            * since there is no active thread (i.e. the JDBC Connection
            * is/will no longer need the Server Worker). Set workingOnaClient
            * and openRdmsThread in the wde accordingly.
            */
           wdePtr->openRdmsThread = BOOL_FALSE;
           wdePtr->workingOnaClient=BOOL_FALSE;

           /*
            * Now indicate we are back from the C-Interface Library. We
            * won't wait until the end of the Switch command since we are
            * doing Server Worker code already.
            */
            insideClibProcessing=BOOL_FALSE;

#if MONITOR_TMALLOC_USAGE /* Tmalloc/Tcalloc/Tfree tracking */
        /*
         * Before returning from END_THREAD_TASK, produce a
         * malloc'ed memory tracking summary.
         */
        output_malloc_tracking_summary();
#endif                       /* Tmalloc/Tcalloc/Tfree tracking */

           /*
            * Insure RSA has free'd its URTS$TABLE space - we really
            * should not have to do this, but oink, oink ...
            */
           if (debugInternal) {
               tracef("Issuing: rsa$free()\n");
           }
           rsa$free();
           break;
       }
       case NEXT_RS_UPDATECOUNT_ENTRY_TASK: {
           returnStatus = nextResultSetUpdateCountTask(request_ptr,&response_ptr);
           break;
       }
       case NEXT_RS_CURSOR_ENTRY_TASK: {
           returnStatus = nextResultSetCursorTask(request_ptr,&response_ptr);
           break;
       }
       case UPDATER_ROW_TASK: {
           returnStatus = updaterRowTask(request_ptr,&response_ptr);
           break;
       }

       case CONN_KEEP_ALIVE_TASK: {
           returnStatus= keepAliveTask(wdePtr, request_ptr,&response_ptr);
           break;
       }
       default: {
           /*
            * Unrecognized task code.  Return an error response
            * indicating an invalid task code in the request packet.
            */
            sprintf(error_message,"ProcessTask(), Task Code %d",taskCode);
            response_ptr = generateExceptionResponse(C_INTERFACE_ERROR_TYPE, SE_INTERNAL_ERROR,
                                                     error_message, taskCodeString(taskCode), NULL, 0);
            returnStatus = JDBCSERVER_PROCESSTASK_RETURNED_ERROR;
            break;
       }
   } /* end of switch on taskcode */

   /* Now indicate we are back from the C-Interface Library */
   insideClibProcessing=BOOL_FALSE;

   /*
    * Conditionally test if the task worked correctly. Normally returnStatus
    * always returns 0, so this check is for debugging purposes only.
    */
   #if (DEBUG)
      if (returnStatus !=0 ){
        tracef("\n**In processTask(), task %d indicated a error status =  %d \n", taskCode, returnStatus);
      }
   #endif

   /*
    * Test if we should trace the fact the we have complete processing
    * request packet.
    */
   if (debugDetail){
      /* Yes, get timestamp for trace image. */
      tracef("** Within processClientTask, completed %s, status = %d, at %s\n",
             taskCodeString(taskCode), returnStatus, getDateTime(NULL, dateTimeString));
   }

   /* Return the response packet generated, if one is present */
   if (response_ptr == NULL){
       sprintf(error_message,"%d (%s)",taskCode, taskCodeString(taskCode));
       msg2 = getLocalizedMessageServer(SERVER_NO_RESPONSE_PKT_GENERATED,
               error_message, NULL, 0, error_message);
               /* 5331 Internal error. Response packet not generated:
                * taskCode {0}) */
       response_ptr = genErrorResponse(C_INTERFACE_ERROR_TYPE,
           SERVER_NO_RESPONSE_PKT_GENERATED, 0, msg2, FALSE);
       *response_ptr_ptr=response_ptr;
       return returnStatus;
   }
   else {
         /* There is a response packet to return. */

         /*
          * Did we open the Client Trace file before processing
          * this task? If so, return the trace file names as part
          * of response packet.
          */
         if (taskOpenedClientTracefile == BOOL_TRUE){

         #if (DEBUG)
            printf("Before adding debug info to response packet:\n");
            hexdump(response_ptr,RESPONSE_HEADER_LEN);
         #endif

            /*
             * Add the debug info information. For debug info
             * format 1 we also send the generated runid.
             */
            getGeneratedRunID(generatedRunID);
            addDebugInfo_1(wdePtr->clientTraceFileName, generatedRunID, response_ptr);

         #if (DEBUG)
            printf("After adding debug info to response packet:\n");
            hexdump(response_ptr,RESPONSE_HEADER_LEN);
         #endif

         }
         else {
            /* No debug info information, so set/reset debug info area offset to 0 */
            setDebugInfoOffset(response_ptr, 0);
         }


         #if (DEBUG)
         {
            int taskStatus;
            int ind = RES_TASK_STATUS_OFFSET;

            taskStatus = getByteFromBytes(response_ptr, &ind);
            tracef("processClientTask:  Returning response packet with taskStatus = %d\n", taskStatus);
         }
         #endif
         *response_ptr_ptr=response_ptr;
         return returnStatus;
   }
}

/*
 * Function keepAliveTask
 *
 * This function provides a keep alive action for the JDBC socket connection
 * between the JDBC Client and its supporting Server Worker activity.  This task
 * is basically a NOP, no information will be returned in the response packet.
 * The response packet is solely needed for retransmission back to the client
 * so both directions of the socket are exercised on a periodic basis.
 */
int keepAliveTask(workerDescriptionEntry *wdePtr, char *requestPtr, char **responsePtrPtr){

    /* The Response Packet Definitions */
    int index;

    /* Allocate a response packet */
    *responsePtrPtr = allocateResponsePacket(RESPONSE_HEADER_LEN);

    /* Fill in the response packet header */
    addResponseHeader(*responsePtrPtr, &index, RESPONSE_HEADER_LEN,
                      CONN_KEEP_ALIVE_TASK, 0, 0, 0);

    if (debugInternal)
        tracef("keepAliveTask()in Server Worker activity: %012o (%024llo) for client socket: %d\n",
            wdePtr->serverWorkerActivityID, wdePtr->serverWorkerUniqueActivityID, wdePtr->client_socket);

    /* Always return a good status */
    return (0);
}
