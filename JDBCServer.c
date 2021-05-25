/*
 * Copyright (c) 2021 Unisys Corporation.
 *
 *          All rights reserved.
 *
 *          UNISYS CONFIDENTIAL
 */

/**
 * File: JDBCServer.c.
 *
 * Main program file used for UC JDBC Server.
 *
 * This program file will utilize other code files in the C-Interface
 * library to provide support for JDBC Clients accessing RDMS.
 *
 */
/* Standard C header files and OS2200 System Library files */
#include <termreg.h>
#include "rsa.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ertran.h>
#include <time.h>
#include <tsqueue.h>
#include <task.h>
#include <sysutil.h>
#include <universal.h>
#include <ctype.h>
#include "marshal.h"

/* The file s$cw.h can be found in SYS$LIB$*PROC$.  This header file is put
 * there by the SLIB installation (SYS$LIB$*SLIB$).  This provides extended
 * mode utility routines.  In our case, we use it for s_cw_set_task_error()
 * and s_cw_clear_task_error().  SYS$LIB$*PROC$ is on the runtime system search
 * chain.  Therefore, you can include the header file and URTS will resolve it.
 */
#include <s$cw.h>

#ifdef XABUILD  /* XA JDBC Server */
#include <CORBA.h>
#include <stdio.h>
#include <termreg.h>
#endif          /* XA JDBC Server */

/* JDBC Project Files */
#include "crdmdefs.h" /* Include crosses the c-interface/server boundary */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "DisplayCmds.h"
#include "NetworkAPI.h"
#include "ProcessTask.h"
#include "sec$ident$.h"
#include "Server.h"
#include "ServerActWDE.h"
#include "ServerConfig.h"
#include "ServerConsol.h"
#include "ServerICL.h"
#include "ServerLog.h"
#include "ServerUASFM.h"
#include "signals.h"

static char Copyright[] =
                         "Copyright (c) 2021 Unisys Corporation. "
                         " "
                         "          All rights reserved. "
                         " "
                         "          UNISYS CONFIDENTIAL";

/*
 * Define the console contingency message as static so it will always be available.
 * The size should large enough to hold the JDBC_CONTINGENCY_MESSAGE or
 * XA_JDBC_CONTINGENCY_MESSAGE, plus the defined server name and xa server number.
 * e.g. sgd.xaServerNumber, sgd.serverName
 */
static char console_cgy_msg[sizeof(XA_JDBC_CONTINGENCY_MESSAGE) + 50];

/*
 * Declare the data structures that are global to the JDBC Server and the
 * C-Interface library routines and must be declared at program level.
 * References to these data structures must be tightly controlled.  The
 * SGD is a JDBC Server structure used by its main activities.  The
 * C-Interface library code may access the Server Worker activities WDE
 * instance, but is not allowed access to the SGD.
 *
 */

#pragma shared begin program

/*
 * First, declare the Server's Global Data (SGD)structure.  For easy code
 * development and review, the SGD structure items can be referenced via
 * sgd.xxx where xxx is the item desired. All other code modules that need
 * the SGD must include an extern for it.
 */
serverGlobalData sgd;                        /* The Server Global Data (SGD),*/
                                             /* visible to all Server activities. */
serverGlobalData *sgdPtr;                    /* Pointer to the Server Global Data (SGD) */

#ifdef XABUILD  /* XA JDBC Server */
int termstat[2];                             /* two words for termination handler (termreg) */
struct _term_buf XAterm_reg_buffer;          /* Term Reg buffer. */
#endif          /* XA JDBC Server */

#pragma shared end

/*
 * Now declare extern references to the activity local WDE pointer and the
 * dummy activity local WDE entry.
 *
 * This is necessary since some of the functions and procedures utilized by
 * the JDBCServer/ConsoleHandler activity are also being used by the ICL and Server
 * Worker activities and those functions need specific information to tailor their
 * actions (i.e. localization of messages).  The design of those routines utilized
 * fields in the Server workers WDE to provide this tailoring information, and the
 * WDE pointer required was not passed to those routines but was assumed to be in
 * the activity local (file level) variable.
 */
extern workerDescriptionEntry *act_wdePtr;
extern workerDescriptionEntry anActivityLocalWDE;


/**
 * Procedure main():
 * This procedure will initialize the JDBC Server.  A set of configuration
 * parameters will be applied to condition this JDBC Server.  Once the
 * configuration information is applied, the main procedure will initiate
 * the following program activities that will remain active to handle the
 * JDBC Client requests:
 *
 *      Once the JDBC Server is initialized, the initial activity will then become
 *      the console handler. When it is receives a shutdown request, either via a
 *      2200 console keyin or via a JDBC Server Maintainance program shutdown directive,
 *      it will notify the other activites. Once all the Server Workers have completed
 *      their dialogues with JDBC Clients, the JDBC Server will shutdown and exit.
 *
 *
 * Procedure initialize_XAJDBC_Server()  ( XA JDBC Server only ):
 * In the J2EE environment, this procedure initializes the XA JDBC Server in a manner
 * similar to that done for the JDBC Server. Once the modified configuration information
 * is applied, this procedure will initiate the following program activities that will
 * remain active to handle the XA JDBC Client requests:
 *
 *      Once the XA JDBC Server is initialized, the initialize_XAJDBC_Server will return
 *      to the caller - the console handler is later invoked in a separate activity. There
 *      is no usage of COMAPI, so the setup/startup of the COMAPI parameters and the ICL
 *      activity and is not performed.  Also, when it is receives a shutdown request, either
 *      via a 2200 console keyin or via a JDBC Server Maintainance program shutdown directive,
 *      the Console handler activity will notify the other activites to shutdown. Since there
 *      is only one server worker, the XASrvrWorker, it is that server worker that must
 *      notify the Console Handler to shut down.
 *
 *
 * Note: The define XABUILD, if defined, indicates we are building the XA JDBC Server. Code that
 * is part of the XA JDBC Server only is indicated by code surrounded by an #ifdef XABUILD and
 * #endif pair. Code that is part of the local JDBC Server only is indicated by code surrounded
 * by an #ifndef XABUILD and #endif pair.  Where there are two conditional compile sections
 * next to each other, whenever possible the code that is part of the local JDBC Server is
 * provided first and the XA JDBC Server code is provided second. This should make the code
 * easier to read and understand.
 *
 */
#ifdef XABUILD  /* XA JDBC Server */

void initialize_XAJDBC_Server(int * errNum){

#else            /* JDBC Server */

main(){

#endif          /* XA and JDBC Servers */

    int option_bits;
    char optionLetters[27];

#ifndef XABUILD /* JDBC Server */
    int regs[12];
    int len;
    int i;
    int j;
    int actual_num;
    char a_mode[4];
    char mode_spec[IMAGE_MAX_CHARS];
    FILE *comapiBdiFile;
    char error_message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* a buffer for a returned error message */
    char ICLdigits[ITOA_BUFFERSIZE];
#endif          /* JDBC Server */

    #define isoctal(c) ((c) >= '0' && (c) < '8')
    #define OCT_CONVERT (('0' << 12) + ('0' << 9) + ('0' << 6) + ('0' << 3) + '0')
    int comapi_bdi[MAX_COMAPI_MODES] = { 0 };
    /* Flag indicating COMAPI support of IPv6 (0 - no, 1 - yes). */
    int comapi_IPv6[MAX_COMAPI_MODES] = { 0 };
    int error_code;
    int status_code;
    exec_status_t  exec_status;
    sec_ident_packet ident_pkt;
    sec_ident_buffer ident_buffer;
    struct sec_userid_record userid_record;
    struct userid_call_pkt userid_pkt;
    int message_len = SERVER_ERROR_MESSAGE_BUFFER_LENGTH; /* maximum length message can be */
    int error_len = SERVER_ERROR_MESSAGE_BUFFER_LENGTH;   /* maximum length error message can be */
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];   /* a buffer for a message */
    char digits[ITOA_BUFFERSIZE];
    int warning_status = 0;
    char *console_msg;
    char timeString[30];
    char timeBuffer[30];
    int message_number;
    unsigned int elms_word_as_uint;

    /*
     * IMPORTANT THINGS THAT MUST BE DONE FIRST:
     * Assign some initial configuration values into the sgd.  These are not
     * part of the configuration values set by processing the JDBC$CONFIG$ file.
     *
     * The first assignments must not cause any log/trace messages to be generated
     * until the product's default locale for the message file is established and
     * the JDBCServer-CH's "dummy" WDE is set up, since we have to be able to generate
     * messages as soon as possible.
     *
     * We need to provide the dummy WDE pointer reference for the
     * JDBCServer/ConsoleHandler activity's local WDE entry so we can utilize
     * it for messages in case there is an error in this activity of the JDBC Server.
     *
     * Since we are just starting the JDBC Server, we have not yet retrieved the
     * Server's locale from the configuration file, so we need to assume the product
     * default locale.  Call the routine that sets up the SGD message file info first,
     * followed by a call to the routine that sets up the activity's local WDE entry.
     *
     * This allows us to immediately utilize the localized message handling
     * code in case there is an error, even before processing the JDBC$CONFIG file
     * file for this possible execution of the JDBC Server. (Also, this will be
     * in handling Network API errors/messages.) When the configuration file is
     * processed, the message file name and file handle will be updated.
     *
     * This must be done as the operational first things in the operation and startup
     * of the JDBC Server:
     *
     * A) Define some basic sgd parameters at this point:
     *      1) Get the time this JDBC Server was started.
     *      2) The JDBC Server's level Id and Release Id.
     *      2a) Get the command line invocation parameters.
     *      2b) Print on STDOUT a JDBC Server signon line.
     *      3) Set a dummy Server name until the configuration file is
     *         processed, in case there is a configuration problem.
     *      4) Set parameters indicating the Server, the Initial Connection
     *         Listener, and the Console Handler are closed - not yet up and
     *         running.
     *  B) Now, set up the JDBCServer/CH's dummy WDE entry and its initial values.
     *
     */
    /* Get the basics into the sgd that are needed for Server sign-on  */
    sgdPtr = &sgd;                                        /* Set pointer to SGD, for use in Server.c */
    getRunIDs();
    getServerUserID();
    setServerInstanceDateandTime();                       /* Set the Server's instance indentification date/time information. */
    sgd.server_LevelID[0] = JDBCSERVER_MAJOR_LEVEL_ID;    /* The numeric major release level ID of this JDBC Server code.     */
    sgd.server_LevelID[1] = JDBCSERVER_MINOR_LEVEL_ID;    /* The numeric minor release level ID of this JDBC Server code.     */
    sgd.server_LevelID[2] = JDBCSERVER_FEATURE_LEVEL_ID;  /* The numeric stability level ID of this JDBC Server code release. */
    sgd.server_LevelID[3] = JDBCSERVER_PLATFORM_LEVEL_ID; /* The numeric level ID of this JDBC Server code release.           */

    /*
     * UCSCNVTIM$CC() returns the Server's start date and time in form : YYYYMMDDHHMMSSmmm . So,
     * Form date-time string of format: YYYYMMDD HH:MM:SS.SSS
     */
    UCSCNVTIM$CC(&sgd.serverStartTimeValue, timeBuffer);
    timeBuffer[17]='\0';
    timeString[0]='\0';
    strncat(timeString, &timeBuffer[0],8);   /* copy the YYYYMMDD */
    strcat(timeString," ");
    strncat(timeString, &timeBuffer[8],2); /* copy the HH */
    strcat(timeString,":");
    strncat(timeString, &timeBuffer[10],2); /* copy the MM */
    strcat(timeString,":");
    strncat(timeString, &timeBuffer[12],2); /* copy the SS */
    strcat(timeString,".");
    strncat(timeString, &timeBuffer[14],3); /* copy the mmm */

    /* Print a JDBC Server sign-on line*/
    if (sgd.server_LevelID[3] !=0){
      /* the platform level is not zero, display it also */
      printf("%s %d.%d.%d.%d (%s), at %s\n  with userid: %s, runid: %s, generated runid: %s\n",
           SERVERTYPENAME, sgd.server_LevelID[0],
           sgd.server_LevelID[1], sgd.server_LevelID[2],
           sgd.server_LevelID[3], JDBCSERVER_BUILD_LEVEL_ID,
           timeString,
           sgd.serverUserID, sgd.originalRunID, sgd.generatedRunID
           );
     }
     else {
      /* the platform level is zero, no need to display it */
      printf("%s %d.%d.%d (%s), at %s\n  with userid: %s, runid: %s, generated runid: %s\n",
           SERVERTYPENAME, sgd.server_LevelID[0],
           sgd.server_LevelID[1], sgd.server_LevelID[2],
           JDBCSERVER_BUILD_LEVEL_ID,
           timeString,
           sgd.serverUserID, sgd.originalRunID, sgd.generatedRunID
           );
     }

#ifndef XABUILD /* JDBC Server */

    /*
     * Get JDBC Server invocation parameters and configuration file name.
     */
    getInvocationImageParameters();

#endif          /* JDBC Server */

    /* Get a string of the processor option letters */
    getServerOptionLetters(optionLetters);
    option_bits = sgd.option_bits; /* get the value for local processing here */

    /* Print a JDBC Server invocation parameter definition line.
       Note: We can't yet call getLocalizedMessageServer,
       since the environment is not set up.*/
    if (strlen(optionLetters) == 0){
       printf("Invocation parameters: Configuration Filename = %s\n\n", sgd.configFileName);
    }
    else {
       printf("Invocation parameters: Configuration Filename = %s, Options = %s\n\n",
              sgd.configFileName, optionLetters);
    }

    /* Continue setting up the sgd values. (ICL information is set up later). */
    strcpy(sgd.serverName,"Unknown-Server");
    sgd.serverState=SERVER_CLOSED;
    sgd.serverShutdownState=SERVER_ACTIVE;     /* it is not shutting down at this time */
    sgd.consoleHandlerState = CH_INACTIVE;
    sgd.consoleHandlerShutdownState=CH_ACTIVE; /* it is not shutting down at this time */
    sgd.UASFM_State =UASFM_CLOSED;
    sgd.UASFM_ShutdownState = UASFM_SHUTDOWN;  /* Its only activated, if user access security is enabled */

    sgd.tempMaxNumWde=TEMPNUMWDE;              /* temp max. number of workers allowed  */
    sgd.numWdeAllocatedSoFar=0;                /* No allocated Server Workers so far.  */

    /* Set up the JDBCServer/CH, or XA Server Worker, activity and then the local WDE entry */
    minimalDefaultsSGD();

#ifndef XABUILD /* JDBC Server */

    sgd.debugXA = 0; /* Not in an XA Server, so clear the XA debug to 0. */
    setupActivityLocalWDE(SERVER_CH, 0, 0, 0); /* Note: we must have at least one ICL,
                                                  so 0 is used for ICL_num, we do not have its
                                                  COMAPI bdi info yet so use 0 (real bdi value
                                                  will be filled in later). */

#else          /* XA JDBC Server */

    setupActivityLocalWDE(SERVER_WORKER, 0, 0, 0); /* Note: we do not use the ICL's,
                                                      so 0 is passed in for ICL_num.*/
    /* Assume the XA JDBC Server initialization process will work correctly. */
    *errNum = 0;
    /* The XA JDBC Server has already registered its termreg procedure handler. */

#endif          /* XA and JDBC Servers */

    /*
     * At this point we have a minimal JDBC Server ready to configure and go, able
     * to log error/status messages using the standard JDBC Server log and trace code
     * (although the output file may be STDOUT).
     *
     */

    /*
     * Test if a JDBC$CONFIG file was specified. If none specified, we have
     * no configuration file to verify and could not start the JDBC Server,
     * so give caller an error message and stop the Server.
     */
    if (strlen(sgd.configFileName)==0){
        /* Were we doing a verification only? If yes, give nice messages */
        getLocalizedMessageServer(JDBCSERVER_CONFIG_MISSING_CONFIG_FILENAME,NULL,NULL,LOG_TO_SERVER_STDOUT, message);
            /* 5099 Configuration file name not provided on JDBC Server
               invocation */
#ifndef XABUILD  /* JDBC Server */

        if (option_bits & (1<<('Z'-'V'))){
            getLocalizedMessageServer(SERVER_V_OPTION_CONFIG_ERROR,
                sgd.configFileName, NULL, LOG_TO_SERVER_STDOUT, message);
                    /* 5302 Verification (V-option) could not be completed on
                       JDBC Server configuration file {0},
                       error encountered. */
            stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT, CONFIG_DEFAULT_SERVERNAME, NULL); /* only verifying configuration, terminate JDBC Server now */
        } else {
            /* We were trying to bring up the JDBC Server, indicate server termination */
            getLocalizedMessageServer(SERVER_TERMINATION, "", NULL,
                LOG_TO_SERVER_STDOUT, message);
                    /* 5303 JDBC {0}Server execution will now terminate. */

            stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT, CONFIG_DEFAULT_SERVERNAME, NULL); /* no configuration file provided, so terminate JDBC Server now */
        }

#else          /* XA JDBC Server */
        /* Return an error message number back to the caller. */
        *errNum = JDBCSERVER_CONFIG_MISSING_CONFIG_FILENAME;
        return;
#endif          /* XA and JDBC Servers */
    }

    /*
     * We have a configuration file name, so now get the global configuration
     * parameters that will control the operation of the JDBC Server by calling
     * the procedure getConfigurationParameters. The results of this operation
     * will be utilized in the rest of this JDBC Servers initialization and
     * operation.
     *
     * Upon return to here, it is assumed that we have a valid configuration
     * in the SGD and that a JDBC Server Log File log entry indicating that
     * was made.
     *
     * Note: If the Server Log File configuration parameter is not valid,
     * then getConfigurationParameters will return an error and the JDBC
     * Server will have to shut down. If getConfigurationParameters returns
     * a warning code, then we can bring up the JDBC Server using the modified
     * configuration information that was initialized.
     *
     * Before getting the configuration parameters, print a nice
     * configuration processing notification image in STDOUT.
     */
    sgd.serverState=SERVER_INITIALIZING_CONFIGURATION;

    getMessageWithoutNumber(SERVER_BEGIN_CONFIG, NULL, NULL, LOG_TO_SERVER_STDOUT);
        /* 5304 Begin processing the JDBC Server configuration file. */
    error_code=getConfigurationParameters(sgd.configFileName);

    /* If negative, then a warning was encountered.  Save value for later use at end of function. */
    if (error_code < 0) {
        warning_status = error_code;
    }

#ifndef XABUILD  /* JDBC Server */
    /*
     * Test for the V-option on the product invocation which indicates that we
     * were just verifying a configuration file. If so, errors or not, just
     * exit (stop) Server we're done.
     */
    if (option_bits & (1<<('Z'-'V'))){

        /*
         * Since this was a V-option verification, display the current
         * configuration at the beginning of the log file.
         * This will consist of a description
         * of the current JDBCServer Configuration.
         */
        processDisplayConfigurationCmd(TRUE, TO_SERVER_LOGFILE, FALSE, NULL);

        /* decide on the message to display, then shut down */
        if (error_code == 0){
            /* 5305 Verification only processing of JDBC Server configuration file {0} completed. */
            message_number = SERVER_V_OPTION_DONE;
        } else if (error_code < 0){
            /* 5306 Verification only processing of JDBC Server configuration file {0} completed,
               warnings encountered. */
            message_number = SERVER_V_OPTION_WARNINGS;
        } else if (error_code > 0){
            /* 5307 Verification only processing of JDBC Server configuration file {0} completed,
               errors encountered. */
            message_number = SERVER_V_OPTION_ERRORS;
        }
        getMessageWithoutNumber(message_number, sgd.configFileName, NULL, LOG_TO_SERVER_STDOUT);
        stop_all_tasks(0, NULL, NULL); /* only verifying configuration, terminate JDBC Server now */

    } else {
        /* Not V-option, let user know we are done processing configuration
         * file. This message indirectly implies we will check for errors and
         * warnings and possibly bring up the JDBC Server.
         * 5308 Processing of JDBC Server configuration file {0} completed.
         */
        getMessageWithoutNumber(SERVER_CONFIG_DONE, sgd.configFileName, NULL,
            LOG_TO_SERVER_STDOUT);
        printf("\n");
    }

#else            /* XA JDBC Server */
        getMessageWithoutNumber(SERVER_CONFIG_DONE, sgd.configFileName, NULL,
                                LOG_TO_SERVER_STDOUT);
        /* 5308 Processing of JDBC Server configuration file {0} completed. */
        printf("\n");

#endif          /* XA and JDBC Servers */
    /*
     * We are not just verifying a JDBC$Config file, but are attempting to
     * bring up an executing version of the JDBC Server.  How did the
     * configuration file processing go? We can continue if we error_code
     * indicates we had either no errors (0) or only warnings (<0).
     */
    if (error_code == 0) {
        /* 5309 JDBC Server will begin operation using the configuration file just processed. */
        message_number = SERVER_USING_CONFIG;
    } else if (error_code < 0){
        /*
         * error_code < 0, so we got one or more warning processing
         * configuration parameters.  Let the caller know that we will
         * start up the JDBCServer with configuration modifications.
         * 5310 JDBC Server will begin operation using the configuration file just processed, as modified.
         */
        message_number = SERVER_USING_MODIFIED_CONFIG;
     } else {
         /*
          * error_code > 0, so one or more errors occurred in trying to get the
          * configuration parameters. We cannot assume there is a JDBC Server Log
          * File available at this time, so display the error on STDOUT and
          * terminate the start up of the JDBC Server.
          * 5303 JDBC Server execution will now terminate.
          */
         getLocalizedMessageServer(SERVER_TERMINATION, NULL, NULL,
                 LOG_TO_SERVER_STDOUT, message);

#ifndef XABUILD  /* JDBC Server */
         /* only verifying configuration, terminate JDBC Server now */
         stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT, CONFIG_DEFAULT_SERVERNAME, NULL);
#else            /* XA JDBC Server */

         /*
          * Return the error code returned by the configuration processing, it
          * was the last fatal error in the processing and should be in the log file.
          */
         *errNum = error_code;
         return;

#endif          /* XA and JDBC Servers */
     }
     /* Display the message */
     getMessageWithoutNumber(message_number, NULL, NULL,LOG_TO_SERVER_STDOUT);


     /* Call runtime routine to register the contingency message that the runtime
      * system will send to the console if a contingency is encountered.  The runtime
      * contingency routine will send the console message on behalf of the server.
      */
#ifndef XABUILD  /* JDBC Server */
     sprintf(console_cgy_msg, JDBC_CONTINGENCY_MESSAGE, sgd.serverName);
#else
     sprintf(console_cgy_msg, XA_JDBC_CONTINGENCY_MESSAGE, sgd.xaServerNumber, sgd.serverName);
#endif
     __rtscgymsg(console_cgy_msg, strlen(console_cgy_msg));

    /*
     * SGD Configuration is fine and is ready for use by the multiple activities,
     * i.e. the two global Test/Set cells: firstFree_WdeTS, firstAll_WdeTS, used
     * for the Worker Description Entry chains are TSQ_NULL'ed.
     *
     * Now set the Server's ( local or XA) dispatch level or switching priority
     * as indicated by the server_priority configuration parameter.
     */
    ER_Level();  /* This function sets the Server run's switching priority indicated. */

    /*
     * Before registering the Console Handler, starting the ICL, etc., we
     * need to check that we have a couple of required hardware/software
     * capabilities. What specific capabilities need checking depends on
     * whether we are running a JDBC Server or JDBC XA Server.
     */

#ifndef XABUILD /* JDBC Server */
    /*
     * Check that the JDBC Server is running on the correct hardware for
     * COMAPI (we have to have queuing instructions on hardware). Since the
     * JDBC XA Server uses ODTP for packet handling, this test is not needed
     * for JDBC XA Server executions.
     */
    ucsinitreg(regs); /* get the initial program register contents */
    if (!(regs[1] & 040)){
        /* Hardware doesn't support queuing, error off JDBC Server */
        getLocalizedMessageServer(SERVER_HARDWARE_DOESNT_HAVE_QUEUING,
            NULL, NULL, SERVER_LOGS, message);
                /* 5373 JDBC Server cannot operate because the hardware
                   does not support queuing instructions. */
        strcpy(message, "JDBC Server execution will now terminate");
        addServerLogEntry(SERVER_STDOUT, message);
        stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);
    }
#endif          /* JDBC Server */

    /*
     * Verify that the RDMS environment is setup for JDBC access.  The verify routine performs multiple
     * checks on the database each time. Minimal checks are performed for XA Servers or if the "K" option
     * is set.  If any fatal errors are detected, the returned 'status_code' will be non-zero.
     */
    status_code = verifyRdmsEnv();

    /* If we have any fatal errors, then shut down the Server. */
    if (status_code > 0) {
        /* Log the Server termination, then shut down. */
        /* 5303 {0} execution will now terminate. */
        getLocalizedMessageServer(SERVER_TERMINATION, SERVERTYPENAME, NULL, SERVER_LOGS, message);
        stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName, sgd.fullLogFileName);
        /* FYI - We never return to here. */
    }

    /* Display the RDMS Level that we are running with. */
    sprintf(message, "Running with RDMS level: %s, AppGroup: %s, Server UserID: %s",
            sgd.rdmsLevelID, sgd.appGroupName, sgd.serverUserID);
    addServerLogEntry(SERVER_LOGS, message);

    /*
     * Now determine if the userID that started the JDBC Server or JDBC
     * XA Server has the privileges needed to read a userID. This is
     * needed so that client access credentials can be validated.
     */
    sgd.serverState = SERVER_CHECKING_AUTHENTICATION_PRIVILEGE;

    ident_pkt.version = ident_version_1;
    ident_pkt.request_type = run_ident;
    memset(&exec_status, 0, sizeof exec_status);
    memset(&ident_buffer, 0, sizeof ident_buffer);
    exec_status = SEC$IDENT$(&ident_pkt , &ident_buffer);

    elms_word_as_uint = exec_status.elms_word.severity;
    elms_word_as_uint = elms_word_as_uint << 15;
    elms_word_as_uint += exec_status.elms_word.product_id;
    elms_word_as_uint = elms_word_as_uint << 18;
    elms_word_as_uint += exec_status.elms_word.message_id;

    if (exec_status.elms_word.severity != 0) {

#ifndef XABUILD
        getLocalizedMessageServer(SERVER_SSAUTHNTICAT_FAILURE,
              "Server shutting down", itoa(elms_word_as_uint, digits, 8), SERVER_LOGS, message);
                   /* 5433 JDBC {0}, must have SSAUTHNTICAT
                      privilege to start; status ({1}) */
        tsk_stopall(); /*  terminate JDBC (XA) Server now */
#else
        getLocalizedMessageServer(SERVER_SSAUTHNTICAT_FAILURE,
              "XA Server error", itoa(elms_word_as_uint, digits, 8), SERVER_LOGS, message);
              /* 5433 JDBC {0}, must have SSAUTHNTICAT
                 privilege to start; status ({1}) */
        *errNum = SERVER_SSAUTHNTICAT_FAILURE;
        return;
#endif
    }

    __wmemset(&userid_record, 0, (sizeof userid_record)/4);
    memset(&exec_status, 0, sizeof exec_status);

    memcpy(userid_pkt.userid, ident_buffer.ident_userid, MAX_USERID_LEN) ;
    userid_pkt.version = version_1;
    userid_pkt.reserved = 0;
    userid_pkt.record_type = userid;
    userid_pkt.request_type = retrieve;

    exec_status = SEC$USERID$(&userid_pkt, &userid_record);

    elms_word_as_uint = exec_status.elms_word.severity;
    elms_word_as_uint = elms_word_as_uint << 15;
    elms_word_as_uint += exec_status.elms_word.product_id;
    elms_word_as_uint = elms_word_as_uint << 18;
    elms_word_as_uint += exec_status.elms_word.message_id;

    if (exec_status.elms_word.severity != 0) {

#ifndef XABUILD
        getLocalizedMessageServer(SERVER_SSAUTHNTICAT_FAILURE,
              "Server shutting down", itoa(elms_word_as_uint, digits, 8), SERVER_LOGS, message);
                   /* 5433 JDBC {0}, must have SSAUTHNTICAT
                      privilege to start; status ({1}) */
        tsk_stopall(); /*  terminate JDBC (XA) Server now */
#else
        getLocalizedMessageServer(SERVER_SSAUTHNTICAT_FAILURE,
              "XA Server error", itoa(elms_word_as_uint, digits, 8), SERVER_LOGS, message);
                   /* 5433 {0}, must have SSAUTHNTICAT
                      privilege to start; status ({1}) */
        *errNum = SERVER_SSAUTHNTICAT_FAILURE;
        return;
#endif
    }

    /* The privilege bits reside in the privilege_er_masks words in consecutive bit
     * positions, but numbered from right to left. (e.g. Word[0] = [36,35,...2,1],
     * Word[1] = [72,71,...38,37], Word[2] = [108,107,...74,73]).
     * The SSAUTHNTICAT privilege is bit number 73.
     */
    if ((userid_record.user_execution_info.privilege_er_masks[(SSAUTHNTICAT - 1) / 36] &
        1 << (SSAUTHNTICAT % 36) - 1) == 0) {
        /*  if (SERVER CAN NOT AUTHENTICATE) */
#ifndef XABUILD
        getLocalizedMessageServer(SERVER_SSAUTHNTICAT_FAILURE,
              "Server shutting down",
              itoa(userid_record.user_execution_info.privilege_er_masks[1], digits, 8), SERVER_LOGS, message);
                   /* 5433 JDBC {0}, must have SSAUTHNTICAT
                      privilege to start; status ({1}) */
        tsk_stopall(); /*  terminate JDBC (XA) Server now */
#else
        getLocalizedMessageServer(SERVER_SSAUTHNTICAT_FAILURE,
              "XA Server error",
              itoa(userid_record.user_execution_info.privilege_er_masks[1], digits, 8), SERVER_LOGS, message);
                   /* 5433 JDBC {0}, must have SSAUTHNTICAT
                      privilege to start; status ({1}) */
        *errNum = SERVER_SSAUTHNTICAT_FAILURE;
        return;
#endif
    }

#ifndef XABUILD /* JDBC Server */
    /*
     * Now for the JDBC Server, obtain a list of the installed COMAPI modes, their bdi's,
     * and the COMAPI level to determine IPv6 support (levels 6R3 and beyond support IPv6).
     * The COMAPI information was produced by an SSG skeleton just before the execution
     * of the JDBC Server, with the results left in the file TPF$.comapi/bdi.
     *
     * Open the file COMAPI_MODE_AND_BDI_FILENAME (TPF$.comapi/bdi.)  for
     * read access only.
     */
    comapiBdiFile=fopen(COMAPI_MODE_AND_BDI_FILENAME,"r");
    if (comapiBdiFile == NULL){
        /* Can't open file TPF$.comapi/bdi.  Without it, the JDBC
         * Server cannot connect to COMAPI and operate correctly,
         * so signal the error and stop the server.
         */
        getLocalizedMessageServer(SERVER_CANT_OPEN_COMAPIBDI_FILE,
              COMAPI_MODE_AND_BDI_FILENAME, NULL, SERVER_LOGS, message);
              /* 5388 Cannot open file {0} to obtain COMAPI mode and bdi information,
                      JDBC Server is shutting down */
        stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName); /* stop all remaining JDBC Server tasks. */
    }

    /* Now loop over the COMAPI mode entries */
    while (fgets(mode_spec, IMAGE_MAX_CHARS, comapiBdiFile) != NULL){
        /* printf(" Value of COMAPI mode_spec: %s\n",mode_spec); */

        if (mode_spec[0] == '-' && isalpha(mode_spec[1]) && mode_spec[2] == ':' &&
            memcmp(&mode_spec[3], "04", 2) == 0 && isoctal(mode_spec[5]) &&
            isoctal(mode_spec[6]) && isoctal(mode_spec[7]) && isoctal(mode_spec[8]) &&
            isoctal(mode_spec[9]) && mode_spec[10] == ':'){
            /* We got an entry, get the bdi value */
            comapi_bdi[toupper(mode_spec[1]) - 'A'] = (mode_spec[5] << 12) + (mode_spec[6] << 9) +
                                                      (mode_spec[7] << 6) + (mode_spec[8] << 3) +
                                                       mode_spec[9] - OCT_CONVERT + 0200000;
            /* Check if the COMAPI level is 6R3 or beyond for IPv6 support. */
            sscanf(&mode_spec[11], "%dR%d", &i, &j);
            if (i > 6 || (i == 6 && j > 2)) {
                comapi_IPv6[toupper(mode_spec[1]) - 'A'] = 1;
            }
        }
        else {
            /* Retrieved information is not in the correct format to
            *  provide the COMAPI mode and bdi information.  Without it,
             * the JDBC Server cannot operate correctly, so signal the
             * error and stop the server.
             */
            getLocalizedMessageServer(SERVER_BAD_COMAPI_MODE_BDI_INFO,
                  mode_spec, NULL, SERVER_LOGS, message);
                       /* 5389 Incorrect COMAPI mode and bdi specification: {0},
                               JDBC Server is shutting down*/
             stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName); /* stop all remaining JDBC Server tasks. */
        }
    }

    /* Now close the COMAPI mode and bdi file. */
    if (fclose(comapiBdiFile)){
        /* Can't close file TPF$.comapi/bdi.  This is not considered
         * fatal, just log the error and continue.
         */
        getMessageWithoutNumber(SERVER_CANT_CLOSE_BDI_FILE,
                                COMAPI_MODE_AND_BDI_FILENAME, "", SERVER_LOGS);
              /* 5390 Cannot close file {0}, {1}JDBC Server
                      will continue operation */
    }

   /*
    * Set up the SGD information used by the ICL's to access COMAPI.
    * The number of COMAPI modes and hence the number of ICL's is kept
    * in sgd.num_COMAPI_modes. The COMAPI modes that should be
    * used by each ICL is kept in sgd.comapi_modes.
    *
    * Use the information on what COMAPI modes are available (installed)
    * and what COMAPI modes are being requested for this JDBC Server to
    * construct the ICL COMAPI mode array.
    */
    actual_num=0;                 /* number of accepted modes */
    len=strlen(sgd.comapi_modes); /* get number of requested COMAPI modes */

    for (i = 0; i < len; i++) {

        if (comapi_bdi[sgd.comapi_modes[i]-'A'] == 0){
            /* COMAPI mode not installed, signal with an error and continue */
            a_mode[0]=sgd.comapi_modes[i];
            a_mode[1]='\0';
            getLocalizedMessageServer(SERVER_THIS_COMAPI_MODES_NOT_AVAILABLE,
                  a_mode, NULL, SERVER_LOGS, message);
                  /* 5391 Requested COMAPI mode {0} is not installed,
                     this COMAPI mode will not be used.*/
        }
        else{
            /* set up the next ICL's COMAPI info. */
            sgd.comapi_reconnect_msg_skip_log_count[i]=0;
            sgd.comapi_reconnect_wait_time[i]=DEFAULT_COMAPI_RECONNECT_WAIT_TIME;
            sgd.forced_COMAPI_error_status[i]=0;
            sgd.ICLState[actual_num]=ICL_CLOSED;
            sgd.ICLShutdownState[actual_num]=ICL_ACTIVE;     /* it is not shutting down at this time */
            sgd.ICLcomapi_bdi[actual_num]=comapi_bdi[sgd.comapi_modes[i]-'A'];
            sgd.ICLcomapi_IPv6[actual_num]=comapi_IPv6[sgd.comapi_modes[i]-'A'];
            sgd.ICLcomapi_mode[actual_num][0]=sgd.comapi_modes[i];
            sgd.ICLcomapi_mode[actual_num][1]='\0';
            sgd.ICLcomapi_state[actual_num]=SEACCES; /* default to API access error (10001) */
            for (j = 0; j < sgd.num_server_sockets; j++){

               /*
                * Set this ICL's known IP addresses. Use the explicit_ip_address values
                * from the first array entries (sgd.explicit_ip_address[0][j]) since
                * each ICL will start with the same state ( the first array entry is
                * setup by the configuration processing). A EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP
                * (-1) value for the explicit_ip_address.family means that the IP address will
                * be set via a DNS lookup when the server sockets are initialized. Right
                * now, just clear the server_socket value.
                *
                * Remember that the IP address, if it was found in configuration
                * processing, is stored in the explicit_ip_address entry. So copy
                * it into the listen_ip_address entry. If the explicit_ip_address.family
                * is EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP (-1), that's OK since the
                * real listen_ip_address will be determined by the ICL code via
                * a host name lookup before attempting to get a server socket.
                */
               sgd.listen_ip_addresses[actual_num][j] = sgd.explicit_ip_address[0][j];
               sgd.explicit_ip_address[actual_num][j] = sgd.explicit_ip_address[0][j];
               sgd.server_socket[actual_num][j]=0;

               /* printf("In JDBCServer: sgd.explicit_ip_address[%d][%d]=%d, sgd.listen_host_names[%d]=%s, sgd.listen_ip_addresses[%d][%d]=%d\n",
                          actual_num, j, sgd.explicit_ip_address[actual_num][j], j, sgd.listen_host_names[j], actual_num,
                          j, sgd.listen_ip_addresses[actual_num][j]);
                */
            }

            actual_num++; /* now bump the count */

            /* printf("ICL COMAPI values actual_num=%d, ICLcomapi_bdi=%o, ICLcomapi_mode=%s\n",
                      (actual_num-1), sgd.ICLcomapi_bdi[actual_num-1],sgd.ICLcomapi_mode[actual_num-1]); */
        }
    }

    sgd.num_COMAPI_modes=actual_num;                   /* save number of COMAPI modes we will attempt to use. */

    /* Clear out all the unused COMAPI usage information in the SGD. */
     for (i = actual_num; i < MAX_COMAPI_MODES; i++){
        sgd.comapi_reconnect_msg_skip_log_count[i]=0;
        sgd.comapi_reconnect_wait_time[i]=0;
        sgd.forced_COMAPI_error_status[i]=0;
        sgd.ICLState[i]=ICL_CLOSED;
        sgd.ICLShutdownState[i]=ICL_ACTIVE;     /* it is not shutting down at this time */
        sgd.ICLcomapi_bdi[i]=0;
        sgd.ICLcomapi_IPv6[i]=0;
        sgd.ICLcomapi_state[i]=SEACCES; /* default to API access error (10001) */
        sgd.ICLcomapi_mode[i][0]='\0';
        for (j = 0; j < MAX_SERVER_SOCKETS; j++){
           sgd.server_socket[i][j]=0;
           sgd.listen_ip_addresses[i][j].family = 0;
           sgd.listen_ip_addresses[i][j].zone = 0;
           sgd.listen_ip_addresses[i][j].v4v6.v6_dw[0] = 0;
           sgd.listen_ip_addresses[i][j].v4v6.v6_dw[1] = 0;
           sgd.explicit_ip_address[i][j].family = 0;
           sgd.explicit_ip_address[i][j].zone = 0;
           sgd.explicit_ip_address[i][j].v4v6.v6_dw[0] = 0;
           sgd.explicit_ip_address[i][j].v4v6.v6_dw[1] = 0;
        }
     }

    /*
     * Check if any of the requested COMAPI modes are installed. If
     * there are none, signal situation and shut down the JDBC Server.
     */
    if (sgd.num_COMAPI_modes == 0){
        /* No requested COMAPI modes are installed. */
        getLocalizedMessageServer(SERVER_NO_COMAPI_MODES_AVAILABLE,
              sgd.comapi_modes, NULL, SERVER_LOGS, message);
              /* 5392 None of requested COMAPI modes {0} is available,
                 JDBC Server is shutting down.*/
        stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);; /* stop all remaining JDBC Server tasks. */
    }

    /*
     * COMAPI access is needed by the JDBCServer (e.g., to register
     * all the the JDBC Server activities for socket sharing, and by
     * CH ), save the first ICL's COMAPI bdi in the activity local WDE
     * for this use.
     */
    act_wdePtr->comapi_bdi=sgd.ICLcomapi_bdi[0];

    /*
     * Test the JDBC Server invocation options for the A-option.  This
     * option will set sgd.debug = SGD_DEBUG_TRACE_COMAPI, so we can
     * trace all of the COMAPI function calls from the very beginning.
     */
    if (option_bits & (1<<('Z'-'A'))){
        /* The A-option was set. */
        sgd.debug=SGD_DEBUG_TRACE_COMAPI;
    }

    /*
     * Test the JDBC Server invocation options for the B-option.  This
     * option will set sgd.COMAPIDebug = SGD_COMAPI_DEBUG_MODE_CALL_TRACE,
     * so we can have COMAPI trace calls from the very beginning.
     */
    if (option_bits & (1<<('Z'-'B'))){
        /* The B-option was set. */
        sgd.COMAPIDebug=SGD_COMAPI_DEBUG_MODE_CALL_TRACE;
    }

    /*
     * Test the JDBC Server invocation options for the C-option.  This
     * option will set sgd.COMAPIDebug = SGD_COMAPI_DEBUG_MODE_CALL_AND_DATA_TRACE,
     * so we can have COMAPI trace calls and data from the very beginning.
     */
    if (option_bits & (1<<('Z'-'C'))){
        /* The C-option was set. */
        sgd.COMAPIDebug=SGD_COMAPI_DEBUG_MODE_CALL_AND_DATA_TRACE;
    }

    /*
     * Test the JDBC Server invocation options for the E-option.  This
     * option will set sgd.debug = SGD_DEBUG_TRACE_SUVAL_AND_MSCON,
     * so we can have COMAPI trace calls and data from the very beginning.
     */
    if (option_bits & (1<<('Z'-'E'))){
        /* The C-option was set. */
        sgd.debug=SGD_DEBUG_TRACE_SUVAL_AND_MSCON;
    }

#endif          /* JDBC Server */

    /*
     * Test what type of JDBC user access control is wanted, if
     * any. Currently there are three choices for JDBC user access
     * security: OFF, JDBC, FUND. We need to set the SGD variable
     * sgd.user_access_file_name correctly for each choice.
     */
    if (sgd.user_access_control == USER_ACCESS_CONTROL_JDBC){
        /*
         * JDBC user access control under SECOPT1 is turned on. So we
         * need to add the application group number to the JDBC user
         * access security file name, forming the actual name that will
         * be append application group number to the JDBC user access
         * security file name.
         * Make sure if the app group number is a single digit that
         * there is a trailing blank added to the filename. Make sure
         * to pad out filename part to a full 12 characters.
         */
         strcpy(sgd.user_access_file_name,JDBC_USER_ACCESS_FILENAME);
         strcat(sgd.user_access_file_name,itoa(sgd.appGroupNumber,digits,10));
         /* make sure we have the needed trailing blanks */
         strcat(sgd.user_access_file_name," ");
         if (sgd.appGroupNumber < 10){
            strcat(sgd.user_access_file_name," "); /* makes sure filename part is full 12 characters */
         }

         /* printf("The JDBC user access security file name is now %s.\n",sgd.user_access_file_name); */
    }
    else {
        /*
         * Either JDBC user access security is set OFF or set for JDBC user access
         * security using Fundamental security is requested. For either of these
         * cases, the JDBC user access security file is not needed, so clear its
         * name string so the SGD data appears consistent when displayed.
         *
         * The FUND setting indicates the Fundamental security approach is to be
         * used.  For this mode of operation we need to verfiy that the table
         * table JDBC$CATALOG2.JDBC_USER_ACCESS exists. This is done when the
         * RDMS data base thread is opened to determine the RDMS level, etc.
         *
         * The OFF setting is the product default, consistent with the operation
         * of earlier levels of the RDMS JDBC Driver Product.
         */
        sgd.user_access_file_name[0] = '\0'; /* set the user security file to null - its not used. */
    }

    /* Register the keyin name for the Console Handler (CH) */
     error_code = registerKeyin();

    if (error_code != 0) {
        /* We could not register the keyin name.
           Therefore, the CH can't run.
           Terminate the server.
           */
        getLocalizedMessageServer(SERVER_KEYIN_FAILED,
            NULL, NULL, SERVER_LOGS, message);
                /* 5312 JDBC Server being shut down because ER KEYIN$
                   registration failed */

#ifndef XABUILD  /* JDBC Server */

               stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName); /* only verifying configuration, terminate JDBC Server now */

#else  /* XA JDBC Server */

               /*
                * Return an error message number back to the caller.
                */
               *errNum = SERVER_KEYIN_FAILED;
               return;

#endif /* XA and JDBC Servers */
    }

    /* Register signals for this server main program activity */
#if USE_SIGNAL_HANDLERS
    regSignals(&signalHandlerLevel1);
#endif

    /*
     * Test what type of JDBC user access control is wanted, if
     * any. Currently there are three choices for JDBC user access
     * security: OFF, JDBC, and FUND. If JDBC, we need to start
     * the UASFM activity.
     */
    if (sgd.user_access_control == USER_ACCESS_CONTROL_JDBC){
        /*
         * JDBC user access control is configured, so do the work to start
         * up the componentry to support user access control at the JDBC
         * Server level. This is done before starting the ICL activity so
         * we are ready to process access permission validation on the
         * first JDBC Client connection request that comes in.
         *
         * Start up the User Access Security File MSCON (UASFM) activity.
         *
         * The User Access Security File MSCON (UASFM) activity is only
         * started if JDBC user access security is turned on by the
         * user_access_control configuration parameter. If it is turned on,
         * the UASFM activity must be running BEFORE the ICL is started, so
         * the UASFM can have accurate MSCON$ data available for each new JDBC
         * client before they are accepted by the ICL and before any attempt
         * to do a BEGIN THREAD within a Server Worker activity.
         *
         * Since it might take time for the just starting UASFM activity
         * to actually call MSCON$, we will do the first MSCON$ call here. This
         * call is done BEFORE the UASFM activity is started, there is no
         * concurrency problem, hence no special locking is needed to protect
         * MSCON$ packet switching operation.
         */
        set_sgd_MSCON_information();

        /* Now start the UASFM activity */
        error_code = start_user_Access_Security_File_MSCON();
        if (error_code !=0){
            /* Could not get the activity started.  A message is already
             * in the JDBCServer Log file.  Since the customer wants
             * user access security, we have no choice but to stop the
             * Server since the only other action would be that
             * ALL user BEGIN Thread requests must be responded
             * with the permission denied response. So either way, there
             * would be no JDBC service provided.
             */
            getLocalizedMessageServer(SERVER_UASFM_FAILURE,
                  itoa(error_code, digits, 10), NULL, SERVER_LOGS, message);
                       /* 5380 Shutting the JDBC Server down,
                          startup of UASFM status = {0} */
#ifndef XABUILD  /* JDBC Server */

               stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);

#else            /* XA JDBC Server */

               /*
                * Return an error message number back to the caller.
                */
               *errNum = SERVER_UASFM_FAILURE;
               return;

#endif          /* XA and JDBC Servers */
        }
    }


#ifndef XABUILD /* JDBC Server */
/* printf("Stopping just before ICL set up\n");twait(19000);stop_all_tasks();    */

    /*
     * Next start up the JDBC Server's Initial Connection Listeners. The ICLs will
     * call COMAPI indicating they will wait and listen on a Server Socket at
     * the JDBC Server's configuration-defined port number, waiting for incoming
     * new JDBC Clients.
     *
     * Upon return to here, it is assumed that we will have properly started the JDBC
     * Server's Initial Connection Listener activities and that a JDBC Server Log File
     * log entry indicating that will be made. It is the responsibility of each ICL
     * activity to do this work.  This procedure only initiates the ICL procedures.
     *
     * Note: If the Initial Connection Listener activities could not be started
     * then an error count will be returned and the JDBC Server will have
     * to shut down.
     */
    sgd.serverState=SERVER_INITIALIZING_ICL;

    error_code=startInitialConnectionListener();
    if (error_code !=0){
            /*
             * We couldn't start all of the ICL's. If so, then we have to stop the
             * JDBC Server. A log file entry for the failed ICL initializations was
             * already made, but add a log entry that we are shutting down the
             * JDBC Server.
             */
        getLocalizedMessageServer(SERVER_ICL_FAILURE,
            itoa(sgd.num_COMAPI_modes, ICLdigits, 10), itoa(error_code, digits, 10), SERVER_LOGS, message);
                /* 5313 Shutting the JDBC Server down, startup of {0} ICLs returned status = {1} */
            stop_all_tasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName); /* stop all remaining JDBC Server tasks. */
            }
#endif          /* JDBC Server */

    /*
     * JDBC Server is now up and running.  Indicate its server state as
     * SERVER_RUNNING, set its initial Server usage type and start date
     * and time (based on type of server),  and write a log entry
     * indicating that fact that the server is up and running.
     */
    sgd.serverState = SERVER_RUNNING;
#ifndef XABUILD /* JDBC Server */
    sgd.usageTypeOfServer_serverStartDate[0] = SERVER_USAGETYPE_JDBCSERVER;
    addServerLogEntry(SERVER_LOGS,"JDBC Server is now up and running, starting up the Console Handler");
#else            /* XA JDBC Server */
    sgd.usageTypeOfServer_serverStartDate[0] = SERVER_USAGETYPE_JDBCXASERVER_UNASSIGNED;
    addServerLogEntry(SERVER_LOGS,"XA JDBC Server is now up and running, starting up the Console Handler");
#endif          /* XA and JDBC Servers */

     /*
      * Now write the header information we want to place at the
      * beginning of the log file.
      * This will consist of a description
      * of the current JDBCServer Configuration.
      */
     processDisplayConfigurationCmd(TRUE, TO_SERVER_LOGFILE, FALSE, NULL);

   /* Send message to console indicating that the JDBC Server is up. */
    if (warning_status < 0) {
#ifndef XABUILD
       console_msg = getLocalizedMessageServerNoErrorNumber(SERVER_IS_RUNNING_WITH_WARNINGS, sgd.serverName , NULL, 0, message);
#else
       console_msg = getLocalizedMessageServerNoErrorNumber(XA_SERVER_IS_RUNNING_WITH_WARNINGS, sgd.serverName , NULL, 0, message);
#endif
    }
    else {
#ifndef XABUILD
       console_msg = getLocalizedMessageServerNoErrorNumber(SERVER_IS_RUNNING, sgd.serverName , NULL, 0, message);
#else
       console_msg = getLocalizedMessageServerNoErrorNumber(XA_SERVER_IS_RUNNING, sgd.serverName , NULL, 0, message);
#endif
    }

#ifdef XABUILD  /* XA JDBC Server */

    } /* end of initialize_XAJDBC_Server procedure */

/*
 * Procedure XAServerConsoleHandler:
 *
 * This procedure provides the XA JDBC Server with its Console Handler
 * functionality.  This procedure is part of a new activity that specifically
 * handles the console commands for the XA JDBC Server.
 */
void XAServerConsoleHandler(){

    int error_code;
    int status_code;
    char digits[ITOA_BUFFERSIZE];
    int error_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;   /* maximum length error message can be */
    char error_message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* a buffer for a returned error message */

    /*
     * The VERY FIRST THING to do is provide the dummy WDE pointer reference for
     * the XA JDBC Server's Console Handler activities local WDE entry so
     * we can utilize it for messages in case there is an error in this activity
     * of the JDBC Server.
     *
     * This allows us to immediately utilize the localized message handling code
     * in case there is an error. Also, this will be used in handling Network
     * API errors/messages.
     *
     * Note: This activity does not use COMAPI, so its WDE entries for COMAPI
     * are set to zeroes.
     */
    setupActivityLocalWDE(SERVER_CH, 0, 0, 0);
   /* addServerLogEntry(SERVER_STDOUT,"Done setting up act_wdePtr in XA Server Console Handler"); */

    /* Register signals for this activity */
#if USE_SIGNAL_HANDLERS
    regSignals(&signalHandlerLevel1);
#endif

    /* Test if we should log procedure entry */
    if (sgd.debug==BOOL_TRUE){
      addServerLogEntry(SERVER_STDOUT,"Entering the XA JDBC Server's Console Handler");
    }

#endif          /* XA JDBC Server */

    /* Set Condition Word bits 7 and 8 (0 is leftmost).  This indicates to the
     * start runstream to restart the JDBC Server if an abnormal termination occurs.
     * If normal termination, then a call to s_cw_clear_task_error() is made to
     * clear these bits, and the JDBC Server will not be restarted.
     * The runstream uses S3 of the Condition Word to optionally indicate if a
     * restart is to occur on error. */
    s_cw_set_task_error();

    /*
     * Now start up the JDBC Server's 2200 Console Listener (CL).  This
     * procedure will wait for any control commands for the JDBC Server.
     * Processing of those commands will be performed by the Console Handler
     * process.  When control is returned, it is either due to a detected
     * error in the Console Handler or it is time to shut down the JDBC Server.
     */

    status_code=startServerConsoleListener();

    /* Control has returned, so we must be shutting down the JDBC Server.  How
     * the JDBC Server is shut down depends on the status code returned. A
     * status_code value of 0 means normal shutdown, while a status_code value
     * !=0 means an error has occurred, so stop the JDBC Server immediately.
     * (The actions taken will evolve as enhancements to the JDBC Server and
     * Console Handler are made).
     *
     * In whichever case, it is the job of this main activity to take appropriate
     * actions to allow the JDBC Server to shut down.
     */
    if (status_code !=0){
        /*
         * Determine the nature of the status_code.  If it indicates an error
         * condition in the Console Listener, then we cannot continue. The CL
         * has logged the error in the LOG file.  So we need to terminate the
         * JDBC Server.
         *
         * This code needs to be enhanced to notify and wait until all the Server
         * Workers have immediately shutdown their connections with JDBC Clients.
         */
        sgd.serverState=SERVER_DOWNED_IMMEDIATELY;
        getLocalizedMessageServer(SERVER_CONSOLE_LISTENER_FAILURE,
            itoa(status_code, digits, 10), NULL, SERVER_LOGS, error_message);
                /* 5314 Shutting the JDBC Server down immediately,
                   Console Listener returned status = {0} */

#ifndef XABUILD /* JDBC Server */

        /* Deregister COMAPI from the application,
           and stop all active tasks.
           Note that control does not return to the caller. */
        deregComapiAndStopTasks(SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);

#else /* XA JDBC Server */

        /* No COMAPI usage; just stop all active tasks.
           Note that control does not return to the caller. */
        stop_all_tasks(XA_SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);

#endif /* XA and JDBC Servers */

        }
    else {
        /*
         * Normal return, so shut down the main JDBC Server gracefully.
         * Write a log entry to indicate the JDBC Server's shutdown almost
         * complete. Then close the server log and trace files. After this,
         * all the JDBC Server's remaining activities will be stopped,
         * completing shutdown.
         *
         * This code needs to be enhanced to notify and wait until all the Server
         * Workers have gracefully completed their connections with JDBC Clients.
         */
        sgd.serverState=SERVER_DOWNED_GRACEFULLY;

        getMessageWithoutNumber(SERVER_GRACEFUL_SHUTDOWN, NULL, NULL,
            SERVER_LOGS);
                /* 5315 JDBC Server is shutting down gracefully */

        /* Now close the Server Trace and Log files */
        error_code= closeServerTraceFile(error_message, error_len);
        if (error_code !=0){
           /*
            * Error occurred in trying to close the Server Trace File. So,
            * try to print error message on the Log and STDOUT file.
            */
           addServerLogEntry(SERVER_STDOUT,error_message);
           }
        error_code= closeServerLogFile(error_message, error_len);
        if (error_code !=0){
           /*
            * Error occurred in trying to close the Server Log File. So,
            * try to print error message on the STDOUT file.
            */
           addServerLogEntry(SERVER_STDOUT,error_message);
           }
        }

    /*
     * The Console Handler has returned and we have closed the JDBC Server
     * Log and JDBC Server Trace files, so we must be shutting down. At this
     * point all normal shutdown operations were handled by the Console
     * Handler calling serverShutDown, so just terminate this main activity and
     * any outstanding activities left (although they should have been shutdown
     * already).
     * Before final termination, display a final message on STDOUT and then
     * deregister with COMAPI.
     */
    getMessageWithoutNumber(SERVER_ACTIVITY_SHUTDOWN, sgd.serverName, NULL,
        TO_SERVER_STDOUT);
            /* 5316 JDBC Server {0} is shutting down its main and any other
                remaining activities. */

#ifndef XABUILD /* JDBC Server */

    /* Deregister COMAPI from the application,
       and stop all active tasks.
       Note that control does not return to the caller. */
    deregComapiAndStopTasks(SERVER_SHUTTING_DOWN_WITH_ERRORS_STDOUT, sgd.serverName, NULL);
    return 0; /* make the compiler happy - this statement is not executed */

    } /* end of main procedure */

#else    /* XA JDBC Server */

    /* No COMAPI usage; just stop all active tasks.
       Note that control does not return to the caller. */
    stop_all_tasks(XA_SERVER_SHUTTING_DOWN_WITH_ERRORS, sgd.serverName , sgd.fullLogFileName);
    } /* end of XAServerConsoleHandler procedure */

#endif /* XA and JDBC Servers */


#ifndef XABUILD /* JDBC Server */
/*
 * Procedure startInitialConnectionListener
 *
 * Starts the JDBC Server's Initial Connection Listener (ICL) activities.
 *
 * If any errors are detected in the start up of any ICL activity, an error
 * indicator and error message is returned to the caller for its disposition.
 * If there are any errors in the proper establishment of the server sockets to
 * be used by an ICL, that fact will be logged by the ICL itself.
 */
int startInitialConnectionListener(){

    int num_ICLs_not_started=0;
    int icl_num;
    _task_array ICL_task_array = {2,0};
    int tskstart_error;

    char digits[ITOA_BUFFERSIZE];
    char ICLdigits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Test if we should log procedure entry */
      addServerLogEntry(SERVER_STDOUT_DEBUG,"Entering startInitialConnectionListener");

    /*
     * For each COMAPI mode we are using, start an ICL activity to support it.
     */
    for (icl_num = 0; icl_num < sgd.num_COMAPI_modes; icl_num++){
        /* Indicate that we are initializing the Initial Connection Listener instance */
        sgd.ICLState[icl_num] = ICL_INITIALIZING;
        sgd.ICLShutdownState[icl_num] = ICL_ACTIVE;

        /*
         * Begin an JDBC Server Initial Connection Listener activity.
         */
        tskstart(ICL_task_array, initialConnectionListener, icl_num,
                 sgd.ICLcomapi_bdi[icl_num], sgd.ICLcomapi_IPv6[icl_num]);

        /*
         * Check if we were successful at starting the ICL activity by calling
         * tsktest() which should return 1 ( indicates ICL exists and is either
         * executing or suspended). If tsktest() returns 0 then ICL is not present
         * (indicates ICL is terminated or never existed), so log an error and
         * indicate that ICL is not active.
         */
        tskstart_error=tsktest(ICL_task_array);
        if (tskstart_error == 0){
            /*
             * Error occurred in trying to start the ICL activity. So place an error
             * message in the LOG file and send back an error indicator to caller.
             */
            getLocalizedMessageServer(SERVER_CANT_START_ICL_ACTIVITY,
                  itoa(icl_num, ICLdigits,10), itoa(tskstart_error, digits, 10), SERVER_LOGS, msgBuffer);
                       /* 5324 Couldn't properly start Initial Connection Listener({0})
                               activity, error status = {1} */

            /* indicate that the ICL is not started */
            num_ICLs_not_started++; /* bump the can't start count */
            sgd.ICLState[icl_num]=ICL_CLOSED;
        }
        else {
            /*
             * We started the ICL activity, write a log entry to indicate the
             *  JDBC Server's ICL task activation.
             */
            getMessageWithoutNumber(SERVER_ICL_STARTED, itoa(icl_num, ICLdigits,10), NULL, SERVER_LOGS_DEBUG);
                                    /* 5325 Initial Connection Listener({0}) started and activated */
        }
    } /* end for */

    /*
     * Return the number of ICL activities that were
     * not started. A value of 0 indicates we started
     * all the ICL activities.
     */
    return num_ICLs_not_started;
}
#endif          /* JDBC Server */

/*
 * Procedure start_user_Access_Security_File_MSCON
 *
 * Starts the JDBC Server's User Access Security File MSCON (UASFM) activity.
 *
 * If any errors are detected in the start up of the UASFM activity, an error
 * message is written to the JDBC Server Log file and the error message code
 * is returned to the caller for its disposition. A return value of 0 indicates
 * the UASFM activity is up and running.
 */
int start_user_Access_Security_File_MSCON(){

    _task_array UASFM_task_array = {2,0};
    int tskstart_error;
    char digits[ITOA_BUFFERSIZE];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /*
     * Begin the JDBC Server's User Access Security File MSCON (UASFM) activity.
     * The activity will set the UASFM_State value to UASFM_RUNNING if it is
     * indeed actively running.)
     */
    tskstart(UASFM_task_array,user_Access_Security_File_MSCON);

    /*
     * Check if we were successful at starting the UASFM activity by calling
     * tsktest() which should return 1 ( indicates UASFM exists and is either
     * executing or suspended). If tsktest() returns 0 then UASFM is not present
     * (indicates UASFM is terminated or never existed), so return an error to
     * the calling routine to handle.
     */
    tskstart_error=tsktest(UASFM_task_array);
    if (tskstart_error == 0){
        /*
         * Error occurred in trying to start the UASFM activity. So place an error
         * message in the LOG file and send back the error indicator to caller.
         */
        getLocalizedMessageServer(SERVER_CANT_START_UASFM_ACTIVITY,
              itoa(tskstart_error, digits, 10), NULL, SERVER_LOGS, msgBuffer);
                /* 5379 Couldn't properly start the
                   User Access Security File MSCON activity, error status {0}*/
        return SERVER_CANT_START_UASFM_ACTIVITY; /* indicate error */
    }

    /*
     * We got the UASFM activity going. Write a log entry to
     * indicate the JDBC Server UASFM activation and return to caller.
     */
    getMessageWithoutNumber(SERVER_UASFM_STARTED, NULL, NULL, SERVER_LOGS_DEBUG);
        /* 5387 JDBC Server UASFM activity started and activated */
    return 0;
}

/*
 * Procedure getInvocationImageParameters
 *
 * This procedure gets some of the SGD values from the JDBC
 * SERVER's exec invocation images:
 * @file.jdbcserver,<options>  <jdbc$config>
 *
 * where:
 *      <options>  - acceptable options for the JDBC Server
 *  <jdbc$config>  - the JDBC$CONFIG file name (file containing configuration info)
 *
 * Example:
 * @AG$*server.jdbcserver,v    AG4*JDBC$CONFIG.
 *
 * In the above example, the v option indicates that the configuation
 * information in file AG4*JDBC$CONFIG. should be verified only - the
 * JDBC Server will not continue execution after configuration verification.
 *
 * Upon return the sgd.option_bits and sgd.configFileName values will be set.
 *
 */
 void getInvocationImageParameters() {

     int message_len=SERVER_ERROR_MESSAGE_BUFFER_LENGTH;   /* maximum length message can be */
     int option_bits;
     int argc;
     char **argv;
#ifdef XABUILD /* XA JDBC Server */
     int i;
     int len;
     int powerTen = 1; /* 10 to the 0 power */
#endif

     /* Initially clear the SGD's server executable name, server number, option bits, and configuration filename values. */
     sgd.serverExecutableName[0] ='\0'; /* indicate no server executable name was found */
     sgd.xaServerNumber = 0;            /* 0 indicates either: a JDBC Server, or no XA server number at end of XA Server executables name. */
     sgd.option_bits = 0;               /* indicate no options letters were given */
     sgd.configFileName[0]='\0';        /* indicate no configuration file name was given */

     /* get command line arguments */
     _cmd_line(&argc,&argv,&option_bits,0);

     /* Produce a warning if there are too many command line arguments. */
     if (argc > 2){
#ifdef XABUILD /* XA JDBC Server */
        printf("5326 Too many parameters (%d) on JDBC XA SERVER invocation, first parameter accepted as Configuration filename.\n",(argc-1));
#else          /* JDBC Server */
        printf("5326 Too many parameters (%d) on JDBC SERVER invocation, first parameter accepted as Configuration filename.\n",(argc-1));
#endif
     }

     if (argc > 1){
        /* get JDBC Configuration filename, don't copy more than you have space*/
        strncpy(sgd.configFileName,argv[1],MAX_FILE_NAME_LEN-1);
     }
     else {
        sgd.configFileName[0]=0;
     }

     /* Save the JDBC Server or JDBC XA Server's executable name. */
     strncpy(sgd.serverExecutableName, argv[0], MAX_FILE_NAME_LEN-1);

#ifdef XABUILD /* JDBC XA Server */
     /*
      * Get the XA server number from the trailing digits at the end of the
      * XA Server's executable name. A result value of 0 means none was found.
      */
     len = strlen(sgd.serverExecutableName);

     /* Loop backwards across the executable name, looking only at digits. */
     for (i = len-1; i >= 0; i--){
        if (isdigit(sgd.serverExecutableName[i])) {
            /* Add in the new digit to the correct power of 10. */
            sgd.xaServerNumber = sgd.xaServerNumber + ((sgd.serverExecutableName[i] - '0') * powerTen);
            powerTen = powerTen * 10; /* get next power of ten for next loop. */
        } else {
            break;
        }
     }
#endif        /* XA JDBC Server */

     /* Save the executable's invocation option bits. */
     sgd.option_bits = option_bits;
     return;
 }

/**
 * getServerOptionLetters
 *
 * Pick up the option bits for the executing processor
 * (the Local Server or the XA Server),
 * and convert it to a string of option letters.
 *
 * @param optionLetters
 *   A string of 27 characters.
 *   This is a reference parameter returned as the option letters.
 *   An empty string is returned if there are no option letters.
 *
 */

 void getServerOptionLetters(char * optionLetters) {

    int option_bits;

    optionLetters[0] = '\0'; /* initialize string */

    option_bits = sgd.option_bits;

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
 * Procedure ER_level.
 * This procedure does an ER LEVEL$ to set a specific dispatch level or priority.
 * The level is determined by the Server's server_priority configuration parameter.
 *
 * There will be a Log message indicating how the ER went, in case there were any
 * problems (e.g. the runs userid does not have SSLEVEL privilege.)
 */

void ER_Level(){

    int inregs[1];
    int outregs[1];
    int ercode=ER_LEVEL;             /* do an ER Level$ */
    int inputval=0400000000000;      /* input is expected in A0  */
    int outputval=0400000000000;     /*  output is expected in A0 */
    int original_level;
    int set_level;
    char statuses[50]; /* a buffer for the two status values */
    char request_level[60];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* get the Server's new switching level argument to set it's new priority*/
    inregs[0]=sgd.er_level_value;

    /* printf("In ER_Level(), er_level_value= %o, running priority = %s\n", sgd.er_level_value, sgd.running priority); */

    /*
     * Do the ER Level$ twice.
     * The first time will cause the desired change in dispatch level
     * and return the original dispatch level, which probably is batch
     * (since we start the server's via a batch run).
     * The second time will reset the same desired dispatch level, and
     * the returned value should indicate this dispatch level.
     * Both values will be displayed in a log message.
     */
    ucsgeneral(&ercode,&inputval,&outputval,inregs,outregs);   /* do the first LEVEL$ */
    original_level=outregs[0]; /* get the original switching priority */

    ucsgeneral(&ercode,&inputval,&outputval,inregs,outregs);   /* do the second LEVEL$ */
    set_level=outregs[0];      /* get the set switching priority */

    /* Get the requested level and value being set ready for the message insert */
    sprintf(request_level,"%s (0%o)", sgd.server_priority, sgd.er_level_value);

    /* Get the statuses ready for the message insert */
    sprintf(statuses,"0%o/0%o", original_level, set_level);

    /*
     * Check if there was a problem with getting the desired level. Do
     * this by checking the S1 portion of the returned status.
     *
     * First check if the level we want is what we actually got.
     */
    if ( (sgd.er_level_value & 0770000000000) == (set_level & 0770000000000)){
        /*
         * We got the level of service that we asked for.  Log it, and display
         * the two status values as well.
         */

#ifndef XABUILD /* JDBC Server */

        /* Output a log message indicating the running priority of the server and the statuses. */
        getMessageWithoutNumber(SERVER_SERVER_PRIORITY_STATUSES, request_level,
                                statuses, SERVER_LOGS);

#else    /* XA JDBC Server */

        /* Output a log message indicating the running priority of the server and the statuses. */
        getMessageWithoutNumber(XASERVER_SERVER_PRIORITY_STATUSES, request_level,
                                statuses, SERVER_LOGS);

#endif /* XA and JDBC Servers */

    }
    else {
        /*
         * The requested level was not obtained.  Check if we got
         * sent to the "depths". In this case, H1 will be set to 0.
         */
         if ((set_level & 0777777000000)== 0){
           /*
            * Yes, we did not get desired level at all - we got demoted
            * to the lowest (least) level of service.  Probably the run's
            * user id does have the SSLEVEL privilege.  Output a warning
            * and continue.
            */

#ifndef XABUILD /* JDBC Server */

           /* Output a log message indicating the running priority of the server and the statuses. */
           getLocalizedMessageServer(SERVER_SERVER_WASNOT_PRIORITY_STATUSES, sgd.server_priority,
                                   statuses, SERVER_LOGS, msgBuffer);

#else    /* XA JDBC Server */

           /* Output a log message indicating the running priority of the server and the statuses. */
           getLocalizedMessageServer(XASERVER_SERVER_WASNOT_PRIORITY_STATUSES, sgd.server_priority,
                                   statuses, SERVER_LOGS, msgBuffer);

#endif /* XA and JDBC Servers */

         }
         else {
           /*
            * We did not get desired level at all, but we did get assigned
            * to a viable level of service.  For example, the configuration
            * may have requested USER level but the system has split USER
            * into DEMAND and BATCH, and therefore we got BATCH. Output a
            * warning and continue.
            */

#ifndef XABUILD /* JDBC Server */

           /* Output a log message indicating the running priority of the server and the statuses. */
           getLocalizedMessageServer(SERVER_SERVER_ALT_PRIORITY_STATUSES, sgd.server_priority,
                                   statuses, SERVER_LOGS, msgBuffer);

#else    /* XA JDBC Server */

           /* Output a log message indicating the running priority of the server and the statuses. */
           getLocalizedMessageServer(XASERVER_SERVER_ALT_PRIORITY_STATUSES, sgd.server_priority,
                                   statuses, SERVER_LOGS, msgBuffer);

#endif /* XA and JDBC Servers */

         }
    }
}

/* verifyRdmsEnv()
 *
 * Verify that the RDMS environment is setup for JDBC access.  The following conditions are checked.
 * Status will be logged to the LOG file and the BRKPT file.  Steps 1, 2 and 7 can cause a fatal
 * error. Other steps only affect Database Metadata operations and will be logged.
 *  1 - Verify that the RDMS database and application group exist.
 *      We will open a database thread.  This will also establish a relationship with this app grp.
 *      Verify that userid that started this Server is registered in UDS as a MASTER-RUN-USERID.
 *      We will get an 8006 error if not.
 *  2 - Verify that the level of RDMS is supported.
 *      The call to retrieve_RDMS_level_info() will check this.
 *
 *  Steps 3-10 only performed for the JDBC Server - not the JDBC XA Server
 *  3 - Verify the presence of old RDMS_CATALOG views. (at least to the server's userid).
 *      * Try to do a SELECT on the SCHEMAS view.
 *  4 - Verify that RDMS_CATALOG views can be read.
 *      The RDMS_CATALOG views need to visible to each user who want to retrieve database metadata.
 *      Normally, 'select' access is granted to PUBLIC, but could be given to just some users.
 *      * Try to FETCH a record to make sure we have GRANTS on this view.
 *  5 - Verify the presence of new (CP 16) RDMS_CATALOG views.
 *      * Try to do a SELECT on the SCHEMAS view.
 *      * Then try to FETCH a record to make sure we have GARNTS on this view.
 *  6 - Verify that newer RDMS_CATALOG views can be read.
 *      The RDMS_CATALOG views need to visible to each user who want to retrieve database metadata.
 *      Normally, 'select' access is granted to PUBLIC, but could be given to just some users.
 *      * Try to FETCH a record to make sure we have GARNTS on this view.
 *  7 - Verify the presence of JDBC$CATALOG tables.
 *      * Try to do a SELECT on one of the tables.
 *      We don't need to check the GRANTS, since the create and GRANTS are all done in the same stream.
 *  8 - Verify that CP 16 changes for the JDBC$CATALOG2 have been performed.
 *      Check the RDMS-CATALOG.INDEXES to see if the primary key is missing from the JDBC_GPC table.
 *  9 - Verify that the JDBC$ACCESS table exists if JDBC Fundamental security is enabled.
 *      This table is required for this mode to work.
 *  10 - Verify that 'OPTIONS RDMS' does not have the VIEW-PROCESSING attribute set incorrectly.
 *       This tries to create a new table and verify that it is visible in the RDMS_CATALOG.
 *
 *  The routine returns the following information.
 *      jdbc_error_code - Overall status code (if set, means a fatal error)
 *      The 'jdbc_error_code' is set and used to control the flow through each step of this routine.
 */
int verifyRdmsEnv() {
    int auxinfo;
    rsa_err_code_type errstatPtr;   /* RDMS Error Status Ptr */
    int rdms_status_code;           /* RDMS return status code */
    int jdbc_error_code;            /* JDBC error step code */
    int jdbc_warning;               /* JDBC Warning flag */
    int step;                       /* Step Started */
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */
    char rdmsErrorMessage[RSA_MSG_VARS * (MAX_RSA_MSG_LEN + 2)/2];
    char *errorMessage;
    access_type_code access_type;
    rec_option_code rec_option;
#ifndef XABUILD /* JDBC Server */
    rsa_error_code_type errorCode;  /* RDMS Error Status Code value */
    int pv_count;
    pv_array_type pv_array[2];
    char dataBuffer[36];
    int indBuffer;
#endif          /* JDBC Server */

    addServerLogEntry(SERVER_LOGS,"Verifying the RDMS environment:");

    /*
     * STEP 1:
     *      Verify that the RDMS database and application group exist.
     *      Verify that userid that started this Server is registered in UDS as a MASTER-RUN-USERID.
     *      Open a database thread.
     */
    step = 1;               /* Current step number. */
    jdbc_error_code = 0;    /* Contains the returned error code defined and set by this routine. */
    jdbc_warning = 0;
    access_type = update;   /* = 1 */
    rec_option = deferred;  /* = 3 */
    /* Do the begin thread. */
    errstatPtr = begin_thread(sgd.generatedRunID, sgd.appGroupName, sgd.serverUserID, 1, 1, access_type, rec_option, 1, &auxinfo);
    rdms_status_code = atoi(errstatPtr);
    sprintf(msgBuffer,"  Step 1:  Opening RDMS thread, rdms_status_code = %i, auxinfo = %i", rdms_status_code, auxinfo);
    addServerLogEntry(SERVER_LOGS, msgBuffer);

    switch (rdms_status_code) {
        case 0: {       /* No errors */
            break;
        }
        case 8006: {    /* Not a master run userid */
            addServerLogEntry(SERVER_LOGS,"**Error:");
            /* 5001 Userid ({0}) which started the {1} is not a MASTER_RUN_USERID in the UDS configuration (RDMS error 8006). */
            getLocalizedMessageServer(JDBCSERVER_MASTER_RUN_USERID_ERROR, sgd.serverUserID, SERVERTYPENAME, SERVER_LOGS, msgBuffer);
            jdbc_error_code = step;
            break;
        }
        default: {      /* Unknown error */
            addServerLogEntry(SERVER_LOGS,"**Error:");
            /* 5378 {0} received error code {1} on the BEGIN THREAD to RDMS during initial system verification. */
            getLocalizedMessageServer(SERVER_CANT_DO_BEGIN_THREAD, SERVERTYPENAME, errstatPtr, SERVER_LOGS, msgBuffer);
            /* Get more details from RDMS with a GET_ERROR call. */
            errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
            addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
            jdbc_error_code = step;
        }
    }

    /*
     * STEP 2:
     *      Verify that the level of RDMS is supported.
     *      Also retrieves the RDMS level and set the Feature Flags in the SGD.
     *      Any errors are logged in the retrieve_RDMS_level_info() routine.
     */
    if (jdbc_error_code == 0) {                  /* Continue if no previous errors. */
        step = 2;
        addServerLogEntry(SERVER_LOGS, "  Step 2:  Retrieving RDMS Level.");

        jdbc_error_code = retrieve_RDMS_level_info();
        if (jdbc_error_code != 0) {
            sprintf(msgBuffer,"**Error: Retrieving RDMS Level, jdbc_error_code = %i", jdbc_error_code);
            addServerLogEntry(SERVER_LOGS, msgBuffer);
            jdbc_error_code = step; /* reset error code to step number */
        }
    }

    /* Steps 3-10 are not needed for the XA Server, so are skipped. */
#ifndef XABUILD /* JDBC Server */

    /* Check for the 'K' option being set.  If so, skip the additional verification checks. */
    if (sgd.option_bits & (1<<('Z'-'K'))){
        addServerLogEntry(SERVER_LOGS, "  K-option set, skipping the rest of the verify steps.");
        goto kOptSkip;       /* Skip the rest of the checks */
    }

    /*
     * STEP 3:
     *      Verify that RDMS_CATALOG views are visible.
     */
    if (jdbc_error_code == 0) {                  /* Continue if no previous fatal errors. */
        step = 3;
        /* We only need to check for one of the catalog views and assume that the rest are there. */
        /* We will check that the view RDMS_CATALOG.TABLE_PRIVILEGES exists. */
        rsa("DECLARE RDMS__CD3 CURSOR SELECT * FROM RDMS_CATALOG.TABLE_PRIVILEGES;", errorCode, &auxinfo);
        rdms_status_code = atoi(errorCode);
        sprintf(msgBuffer,"  Step 3:  Checking for RDMS_CATALOG views, rdms_status_code = %i", rdms_status_code);
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        switch (rdms_status_code) {
            case 0: {       /* No errors */
                break;
            }
            case 6006: {     /* Views are missing. */
                /* Could not access RDMS_CATALOG.TABLE_PRIVILEGES. */
                addServerLogEntry(SERVER_LOGS, "**Warning: Could not access some RDMS_CATALOG views.");
                addServerLogEntry(SERVER_LOGS, "           See your DBA and the UREP*UTILITY.RDMS-CATALOG script.");
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_warning = step;   /* Non fatal error, just log it. */
                break;
            }
            default: {      /* Declare Cursor: Unknown fatal error.  Log it. */
                /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "3", SERVER_LOGS, msgBuffer);
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_error_code = step;     /* Fatal error */
            }
        }
    }

    /*
     * STEP 4:
     *      If the view was found, check if we can access it. May not have performed the GRANTS.
     */
    if (jdbc_error_code == 0) {                 /* Continue if no previous errors. */
        /* Skip step 4 if we could find the views in step 3 */
        if (jdbc_warning == 3) {
            addServerLogEntry(SERVER_LOGS, "  Step 4:  Skipped.");
        } else {
            rsa("OPEN CURSOR RDMS__CD3;", errorCode, &auxinfo);
            rdms_status_code = atoi(errorCode);
            sprintf(msgBuffer,"  Step 4:  Checking access to RDMS_CATALOG views, rdms_status_code = %i", rdms_status_code);
            addServerLogEntry(SERVER_LOGS, msgBuffer);

            switch (rdms_status_code) {
                case 0: {       /* No errors. */
                    break;
                }
                case 6002: {    /* User not authorized */
                    /* 6002 error mean that the user is not authorized to read the values. */
                    /* This is most likely due to the fact that GRANTS were not performed on the tables. */
                    /* Non-fatal error - do not set 'jdbc_error_code'  */
                    /* It could be that GRANTS were only set for a few users, not to PUBLIC. */
                    sprintf(msgBuffer,"**WARNING: SELECT access has not been granted some RDMS_CATALOG views for userid %s.",
                            sgd.serverUserID);
                    addServerLogEntry(SERVER_LOGS, msgBuffer);
                    addServerLogEntry(SERVER_LOGS, "           See your DBA and the UREP*UTILITY.RDMS-CATPRIV script.");
                    /* Get more details from RDMS with a GET_ERROR call. */
                    errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    jdbc_warning = 1;   /* Non fatal error, just log it. */
                    break;
                }
                default: {      /* Open Cursor: Unknown fatal error. Log it. */
                    /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                    getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "4", SERVER_LOGS, msgBuffer);
                    /* Get more details from RDMS with a GET_ERROR call. */
                    errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    jdbc_error_code = step;     /* Fatal error */
                }
            }
        }
    }

    /*
     *  5 - Verify the presence of new (CP 16) RDMS_CATALOG views.
     *      * Try to do a SELECT on the RDMS_STATS view.
     *      * Then try to FETCH a record to make sure we have GARNTS on this view.
     */
    if (jdbc_error_code == 0) {                  /* Continue if no previous fatal errors. */
        step = 5;
        /* We only need to check for one of the catalog views and assume that the rest are there. */
        /* We will check that the view RDMS_CATALOG.RDMS_STATS exists. */
        rsa("DECLARE RDMS__CD5 CURSOR SELECT * FROM RDMS_CATALOG.RDMS_STATS;", errorCode, &auxinfo);
        rdms_status_code = atoi(errorCode);
        sprintf(msgBuffer,"  Step 5:  Checking for newer RDMS_CATALOG views, rdms_status_code = %i", rdms_status_code);
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        switch (rdms_status_code) {
            case 0: {       /* No errors */
                break;
            }
            case 6006: {     /* Views are missing. */
                /* Could not access RDMS_CATALOG.RDMS_STATS. */
                addServerLogEntry(SERVER_LOGS, "**Warning: Some of the new (CP 16) RDMS_CATALOG views are not present.");
                addServerLogEntry(SERVER_LOGS, "           See your DBA and the UREP*UTILITY.RDMS-CATALOG script.");
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_warning = step;   /* Non fatal error, just log it. */
                break;
            }
            default: {      /* Declare Cursor: Unknown fatal error.  Log it. */
                /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "5", SERVER_LOGS, msgBuffer);
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_error_code = step;     /* Fatal error */
            }
        }
    }

    /*
     * STEP 6:
     *      If the view was found, check if we can access it. May not have performed the GRANTS.
     */
    if (jdbc_error_code == 0) {                 /* Continue if no previous errors. */
        /* Skip step 6 if we could find the views in step 5 */
        if (jdbc_warning == 5) {
            addServerLogEntry(SERVER_LOGS, "  Step 6:  Skipped.");
        } else {
            rsa("OPEN CURSOR RDMS__CD5;", errorCode, &auxinfo);
            rdms_status_code = atoi(errorCode);
            sprintf(msgBuffer,"  Step 6:  Checking access to newer RDMS_CATALOG views, rdms_status_code = %i", rdms_status_code);
            addServerLogEntry(SERVER_LOGS, msgBuffer);

            switch (rdms_status_code) {
                case 0: {       /* No errors. */
                    break;
                }
                case 6002: {    /* User not authorized */
                    /* 6002 error mean that the user is not authorized to read the values. */
                    /* This is most likely due to the fact that GRANTS were not performed on the tables. */
                    /* Non-fatal error - do not set 'jdbc_error_code'  */
                    /* It could be that GRANTS were only set for a few users, not to PUBLIC. */
                    sprintf(msgBuffer,"**WARNING: SELECT access has not been granted some new (CP 16) RDMS_CATALOG views for userid %s.",
                            sgd.serverUserID);
                    addServerLogEntry(SERVER_LOGS, msgBuffer);
                    addServerLogEntry(SERVER_LOGS, "           See your DBA and the UREP*UTILITY.RDMS-CATPRIV script.");
                    errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    jdbc_warning = 1;   /* Non fatal error, just log it. */
                    break;
                }
                default: {      /* Open Cursor: Unknown fatal error. Log it. */
                    /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                    getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "6", SERVER_LOGS, msgBuffer);
                    /* Get more details from RDMS with a GET_ERROR call. */
                    errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    jdbc_error_code = step;     /* Fatal error */
                }
            }
        }
    }

    /*
     * STEP 7:
     *      Check for the presence of JDBC_CATALOG2 tables.
     *      These are requested for Database Metadata operations.
     *      We don't need to check the GRANTS, since the create and GRANTS are all done in the same stream.
     */
    if (jdbc_error_code == 0) {                  /* Continue if no previous errors. */
        step = 7;
        /* We only need to check for one of the catalog views and assume that the rest are there. */
        /* We will check that the JDBC$CATALOG2.JDBC_GSTY table exists. */
        rsa("DECLARE RDMS__CD7 CURSOR SELECT * FROM \"JDBC$CATALOG2\".JDBC_GSTY;", errorCode, &auxinfo);
        rdms_status_code = atoi(errorCode);
        sprintf(msgBuffer,"  Step 7:  Checking for JDBC$CATALOG2 tables, rdms_status_code = %i", rdms_status_code);
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        switch (rdms_status_code) {
            case 0: {       /* No errors */
                break;
            }
            case 6006: {    /* Could not access JDBC$CATALOG2.JDBC_GSTY. */
                addServerLogEntry(SERVER_LOGS, "**WARNING: Some of the JDBC$CATALOG2 tables do not exist.");
                addServerLogEntry(SERVER_LOGS, "           See the RDMS-JDBC User Guide section 2 for options.");
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_warning = 1;   /* Non fatal error, just log it. */
                break;
            }
            default: {      /* Declare Cursor: Unknown fatal error.  Log it. */
                /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "7", SERVER_LOGS, msgBuffer);
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_error_code = step;     /* Fatal error. */
            }
        }
    }

    /*
     * STEP 8:
     *      Verify that CP 16 changes for the JDBC$CATALOG2 have been performed.
     *      Check the RDMS_CATALOG.INDEXES to verify that the primary key is missing from the JDBC_GPC table.
     */
    if (jdbc_error_code == 0) {                  /* Continue if no previous errors. */
        step = 8;
        /* We will check for changes by looking to see if the primary key index was removed from */
        /* the JDBC$GPC (getProcedureColumns).  We will look at the RDMS_CATALOG.INDEXES view. */
        /* If the table is current, the select will return no records. */

        /* Setup the pv_array for fetching */
        pv_count = 2;
        pv_array[0].pv_addr = &dataBuffer;
        pv_array[0].pv_type = ucs_char;
        pv_array[0].pv_length = 32;
        pv_array[0].pv_precision = 9;
        pv_array[0].pv_scale = 0;
        pv_array[0].pv_flags = 0400;
        pv_array[1].pv_addr = &indBuffer;
        pv_array[1].pv_type = ucs_int;
        pv_array[1].pv_length = 4;
        pv_array[1].pv_precision = 36;
        pv_array[1].pv_scale = 0;
        pv_array[1].pv_flags = 0400;


        rsa("DECLARE RDMS__CD8 CURSOR SELECT INDEX_TYPE FROM RDMS_CATALOG.INDEXES WHERE schema_name='JDBC$CATALOG2' AND table_name='JDBC_GPC';",
                errorCode, &auxinfo);
        rdms_status_code = atoi(errorCode);
        sprintf(msgBuffer,"  Step 8:  Checking for updated (CP 16) JDBC$CATALOG2 tables, rdms_status_code = %i", rdms_status_code);
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        switch (rdms_status_code) {
            case 0: {       /* No errors */
                break;
            }
            default: {      /* Declare Cursor: Unknown fatal error.  Log it. */
                /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "8", SERVER_LOGS, msgBuffer);
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_error_code = step;     /* Fatal error. */
            }
        }

        if (jdbc_error_code == 0) {                  /* Continue if no previous errors. */
            /* Now check if there are any records to fetch. */
            /* There should be no records if the table has been updated and there is no primary key. */
            strcpy(msgBuffer, "FETCH NEXT RDMS__CD8 INTO $p1;");
            set_params(&pv_count, pv_array);
            rsa(msgBuffer, errorCode, &auxinfo);
            rdms_status_code = atoi(errorCode);

            switch (rdms_status_code) {
                case 0: {       /* No errors. We must have found a primary key, which means the table has not been updated. */
                    addServerLogEntry(SERVER_LOGS, "**Warning: Changes to JDBC$CATALOG2 have not been performed.");
                    jdbc_warning = 1;
                    break;
                }
                case 6001: {    /* EOF detected.  This is good and means the table is current. */
                    break;
                }
                default: {      /* Declare Cursor: Unknown fatal error.  Log it. */
                    /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                    getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "8", SERVER_LOGS, msgBuffer);
                    /* Get more details from RDMS with a GET_ERROR call. */
                    errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    jdbc_error_code = step;     /* Fatal error. */
                }
            }
        }
    }

    /*
     * STEP 9:
     *      Verify that the JDBC$ACCESS table exists in case JDBC Fundamental security is enabled.
     */
    if (jdbc_error_code == 0) {                  /* Continue if no previous errors. */
        step = 9;
        /* Check that the table JDBC$CATALOG2.JDBC$ACCESS exists. */
        rsa("DECLARE RDMS__CD9 CURSOR SELECT * FROM \"JDBC$CATALOG2\".\"JDBC$ACCESS\";", errorCode, &auxinfo);
        rdms_status_code = atoi(errorCode);
        sprintf(msgBuffer,"  Step 9:  Checking for JDBC$ACCESS, rdms_status_code = %i", rdms_status_code);
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        switch (rdms_status_code) {
            case 0: {           /* No errors. */
                break;
            }
            case 6002: {        /* Could not access JDBC$CATALOG2.JDBC$ACCESS, log it. */
                if (sgd.user_access_control == USER_ACCESS_CONTROL_FUND){
                    /* If Fundamental Security is enabled, then this is a fatal error, set the error code, else, just log it. */
                    addServerLogEntry(SERVER_LOGS, "**Error: Could not access JDBC$CATALOG2.JDBC$ACCESS. Required for Fundamental Security.");
                    jdbc_error_code = step;
                } else {
                    /* Just a warning if the server is not setup for Fundamental Security. */
                    addServerLogEntry(SERVER_LOGS, "**Warning: Could not access JDBC$CATALOG2.JDBC$ACCESS. Required for Fundamental Security.");
                    jdbc_warning = 1;
                }
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                break;
            }
            default: {          /* Declare Cursor: Unknown fatal error.  Log it. */
                /* 5394 Unknown RDMS error ({0}) during verification of CATALOGs. Step {1}. */
                getLocalizedMessageServer(CATALOG_VERIFY_ERROR, errorCode, "9", SERVER_LOGS, msgBuffer);
                /* Get more details from RDMS with a GET_ERROR call. */
                errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                jdbc_error_code = step;
            }
        }
    }

    /*
     * STEP 10:
     *      Verify that 'OPTIONS RDMS' does not have the RDMS-LEVEL and VIEW-PROCESSING attribute set incorrectly.
     *      To do this, we will create a new global temporary table and view Then try to view the table and view in
     *      the catalog.  The table will not have any storage areas, since it is a temporary table and it will go away
     *      when we do a rollback of the current thread.  The table and view will be created in the JDBC$CATALOGX schema,
     *      which is created just for this process. The schema will implicitly be created as an unowned schema.
     *      It will be deleted on the rollback.
     *
     *      To verify the RDMS-LEVEL attribute:
     *        CREATE GLOBAL TEMPORARY TABLE JDBC$CATALOGX.JDBCTEST COLUMNS ARE TESTID: CHAR(11) PRIMARY PK IS TESTID;
     *        SELECT COLUMN_NAME FROM RDMS_CATALOG.TABLE_COLUMNS WHERE
     *          SCHEMA_NAME = 'JDBC$CATALOGX' AND TABLE_NAME = 'JDBCTEST' AND COLUMN_NAME = 'TESTID';
     *      Verify that I get one row back.
     *
     *      To verify the VIEW-PROCESSING attribute:
     *        CREATE VIEW JDBC$CATALOGX.V01 AS SELECT * FROM JDBC$CATALOGX.JDBCTEST;
     *        SELECT SCHEMA_NAME FROM RDMS_CATALOG.VIEW_RELATION_NAMES WHERE
     *          SCHEMA_NAME = 'JDBC$CATALOGX' AND VIEW_NAME = 'V01';
     *      Verify that I get one row back.
     *
     */
    if (jdbc_error_code == 0) {                  /* Continue if no previous errors. */
        step = 10;
        sprintf(msgBuffer,"  Step 10: Checking RDMS configuration attributes.");
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        /* Setup the pv_array for fetching */
        pv_count = 2;
        pv_array[0].pv_addr = &dataBuffer;
        pv_array[0].pv_type = ucs_char;
        pv_array[0].pv_length = 32;
        pv_array[0].pv_precision = 9;
        pv_array[0].pv_scale = 0;
        pv_array[0].pv_flags = 0400;
        pv_array[1].pv_addr = &indBuffer;
        pv_array[1].pv_type = ucs_int;
        pv_array[1].pv_length = 4;
        pv_array[1].pv_precision = 36;
        pv_array[1].pv_scale = 0;
        pv_array[1].pv_flags = 0400;

        /* Part 1 - create a dummy table and try to fetch the metadata from the RDMS Catalog. */
        if (jdbc_error_code == 0) {
            strcpy(msgBuffer,"CREATE GLOBAL TEMPORARY TABLE \"JDBC$CATALOGX\".JDBCTEST COLUMNS ARE TESTID: CHAR(11) PRIMARY PK IS TESTID;");
            rsa(msgBuffer, errorCode, &auxinfo);
            rdms_status_code = atoi(errorCode);
            if (rdms_status_code != 0) {
                jdbc_error_code = 100;
            } else {
                strcpy(msgBuffer,"DECLARE RDMS__CD10 CURSOR SELECT COLUMN_NAME FROM RDMS_CATALOG.TABLE_COLUMNS WHERE SCHEMA_NAME = 'JDBC$CATALOGX' AND TABLE_NAME = 'JDBCTEST' AND COLUMN_NAME = 'TESTID';");
                rsa(msgBuffer, errorCode, &auxinfo);
                rdms_status_code = atoi(errorCode);
                if (rdms_status_code != 0) {
                    jdbc_error_code = 101;
                } else {
                    /* Now verify that we can fetch a record. */
                    strcpy(msgBuffer, "FETCH NEXT RDMS__CD10 INTO $p1;");
                    set_params(&pv_count, pv_array);
                    rsa(msgBuffer, errorCode, &auxinfo);
                    rdms_status_code = atoi(errorCode);
                    if (rdms_status_code == 0) {
                        jdbc_error_code = 0;    /* Record found - Good. */
                    } else if  (rdms_status_code == 6001) {
                        jdbc_error_code = 102;  /* EOF detected */
                    } else {
                        jdbc_error_code = 103;  /* Unknown error */
                    }
               }
            }

            /* Check for any errors during step 10, part 1*/
            if (jdbc_error_code != 0) {
                /* First, check if everything worked, but just no record found. */
                if (jdbc_error_code == 102) {
                    addServerLogEntry(SERVER_LOGS, "**Warning: New dummy table not visible in the RDMS_CATALOG.");
                    addServerLogEntry(SERVER_LOGS, "  This can be caused by not setting RDMS-LEVEL COMPLETE in UREP.");
                    /* Reset the jdbc_error_code, not fatal. */
                    jdbc_error_code = 0;
                    jdbc_warning = 1;
                } else {
                    addServerLogEntry(SERVER_LOGS, "**Warning: Unable to complete verification of this step.");
                    sprintf(rdmsErrorMessage,"  While executing: %s", msgBuffer);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    /* Get more details from RDMS with a GET_ERROR call. */
                    errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    /* Reset the jdbc_error_code, not fatal. */
                    jdbc_error_code = 0;
                    jdbc_warning = 1;
                }
            }
        }

        /*  Part 2 - Create a dummy view and try to fetch the metadata from the RDMS Catalog */
        if (jdbc_error_code == 0) {
            strcpy(msgBuffer,"CREATE VIEW \"JDBC$CATALOGX\".V01 AS SELECT * FROM \"JDBC$CATALOGX\".JDBCTEST;");
            rsa(msgBuffer, errorCode, &auxinfo);
            rdms_status_code = atoi(errorCode);
            if (rdms_status_code != 0) {
                jdbc_error_code = 200;
            } else {
                strcpy(msgBuffer,"DECLARE RDMS__CD10A CURSOR SELECT SCHEMA_NAME FROM RDMS_CATALOG.VIEW_RELATION_NAMES WHERE SCHEMA_NAME = 'JDBC$CATALOGX' AND VIEW_NAME = 'V01';");
                rsa(msgBuffer, errorCode, &auxinfo);
                rdms_status_code = atoi(errorCode);
                if (rdms_status_code != 0) {
                    jdbc_error_code = 201;
                } else {
                    /* Now verify that we can fetch a record. */
                    strcpy(msgBuffer, "FETCH NEXT RDMS__CD10A INTO $p1;");
                    set_params(&pv_count, pv_array);
                    rsa(msgBuffer, errorCode, &auxinfo);
                    rdms_status_code = atoi(errorCode);
                    if (rdms_status_code == 0) {
                        jdbc_error_code = 0;    /* Record found - Good. */
                    } else if  (rdms_status_code == 6001) {
                        jdbc_error_code = 202;  /* EOF detected */
                    } else {
                        jdbc_error_code = 203;  /* Unknown error */
                    }
                }
            }

            /* Check for any errors during step 10, part 2*/
            if (jdbc_error_code != 0) {
                /* First, check if everything worked, but just no record found. */
                if (jdbc_error_code == 202) {
                    addServerLogEntry(SERVER_LOGS, "**Warning: New dummy view not visible in the RDMS_CATALOG.");
                    addServerLogEntry(SERVER_LOGS, "  This can be caused by not setting VIEW-PROCESSING IMMEDIATE in UREP.");
                    /* Reset the jdbc_error_code, not fatal. */
                    jdbc_error_code = 0;
                    jdbc_warning = 1;
                } else {
                    addServerLogEntry(SERVER_LOGS, "**Warning: Unable to complete verification of this step.");
                    sprintf(rdmsErrorMessage,"  While executing: %s", msgBuffer);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    /* Get more details from RDMS with a GET_ERROR call. */
                    errorMessage = getAsciiRsaErrorMessage(rdmsErrorMessage);
                    addServerLogEntry(SERVER_LOGS, rdmsErrorMessage);
                    /* Reset the jdbc_error_code, not fatal. */
                    jdbc_error_code = 0;
                    jdbc_warning = 1;
                }
            }
        }
    }

kOptSkip:       /* end of K-option branch */

#endif          /* JDBC Server */

    /*
     * CLEANUP:
     *      Now close down the thread used to verify the environment, if we got past step 1 and have an open thread. */
    if (step > 1) {

        /* Do a rollback on the thread, which is required to get rid of the schema. */
        errstatPtr = rollback_thread(rbdefault, &auxinfo);
        rdms_status_code = atoi(errstatPtr);
        sprintf(msgBuffer,"  Cleanup: Rollback Thread, rdms_status_code = %i", rdms_status_code);
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        if (rdms_status_code != 0 ) {
            /* could not rollback the thread, should be able to so stop server */
            /* 5376 JDBC Server could not {0} the thread used for system verification. RDMS error = {1}. */
            getLocalizedMessageServer(SERVER_CANT_END_RDMS_LEVEL_THREAD, "rollback", errstatPtr, SERVER_LOGS, msgBuffer);
            jdbc_error_code = SERVER_CANT_END_RDMS_LEVEL_THREAD;
        }

        errstatPtr = end_thread(nop, &auxinfo);
        rdms_status_code = atoi(errstatPtr);
        sprintf(msgBuffer,"           End Thread, rdms_status_code = %i", rdms_status_code);
        addServerLogEntry(SERVER_LOGS, msgBuffer);

        if (rdms_status_code != 0 ) {
            /* could'nt end the thread, should be able to so stop server */
            /* 5376 JDBC Server could not {0} the thread used for system verification. RDMS error = {1}. */
            getLocalizedMessageServer(SERVER_CANT_END_RDMS_LEVEL_THREAD, "end", errstatPtr, SERVER_LOGS, msgBuffer);
            jdbc_error_code = SERVER_CANT_END_RDMS_LEVEL_THREAD;
        }
    }

    if (jdbc_error_code == 0) {
        addServerLogEntry(SERVER_LOGS, "Successful verification.");
        /* Check if we had any warnings. */
        if (jdbc_warning != 0) {
            addServerLogEntry(SERVER_LOGS, "... but there were warnings that need to be corrected.");
            addServerLogEntry(SERVER_LOGS, "    Some JDBC DatabaseMetadata APIs may not operate correctly.");
        }
    } else {
        addServerLogEntry(SERVER_LOGS, "Verification Failed - JDBC Server will be shutdown.");
        if (step > 2) {
            /* We have an error and we got past the required checks. Add an extra info message.*/
            addServerLogEntry(SERVER_LOGS, "  If you continue to fail after step 2, the RDMS System Verify");
            addServerLogEntry(SERVER_LOGS, "  checks can be bypassed by using the K-option to start the JDBC Server.");
        }
    }

    return jdbc_error_code;
}
