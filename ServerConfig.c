/**
 * File: ServerConfig.c.
 *
 * Procedures that establish and modify the runtime configuration of the
 * JDBC Server.
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
#include <tsqueue.h>
#include <task.h>
#include <sysutil.h>
#include <ertran.h>
#include <ctype.h>
#include <universal.h>
#include <server-msg.h>
#include "marshal.h"

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "mscon.h"
#include "NetworkAPI.h"
#include "ProcessTask.h"
#include "Server.h"
#include "ServerConfig.h"
#include "ServerLog.h"
#include "ServiceUtils.h"
#include "ipv6-parser.h"

extern serverGlobalData sgd;        /* The Server Global Data (SGD),*/
                                             /* visible to all Server activities. */
extern workerDescriptionEntry *act_wdePtr;

static FILE *configFile;            /* used by multiple procedures during configuration processing. */

/*
 * Procedure getConfigurationParameters
 *
 * Retrieves and sets the Server configuration information.
 * This process will involve reading a configuration file analyzing the contents
 * and setting up the appropriate configuration information in the Server's
 * global data area.  Then it will do an fopen on the JDBC Server Log file
 * and write a log entry indicating the initialization of the JDBC Server that
 * contains the configuration parameters that should be logged at that time. The
 * corresponding thing is done to the Server Trace file, if present.
 *
 * If any errors are detected in the configuration parameters, an error indicator
 * and error message is returned to the caller for its disposition.  At the time
 * of an error, there may already be some values set into the Server's global
 * data area.  If the Log File configuration value is in error, no log file
 * entry is possible and the the calling procedure is responsible for handling
 * the error (likely it will terminate the JDBC Server in error).
 *
 * Parameters:
 *
 * error_message   - a character array that will contain returned message
 *                   should an error or warning occur during processing of
 *                   the configuration file.
 *
 * error_len       - length of the error_message array in bytes (chars)
 *
 * configFileName  - a character array that contains the file name ( null
 *                   terminated) of the configration file to process.
 *
 * Returned value:
 *
 * error_code      - 0, if no errors or warnings occurred processing the
 *                   configuration file.
 *                   < 0, if a warning message was returned.  In this case,
 *                   one or more of the parameter values were modified to
 *                   produce a working configuration set (e.g. a default
 *                   value was supplied due to a parameter out of acceptable
 *                   range). The error_code value returned is the negative
 *                   value of the warning message number.
 *                   >0, if an error occurred attempting to process one or
 *                   more of the configuration parameters. The error_code
 *                   value returned is the error message number.
 */
int getConfigurationParameters(char * configFileName) {

    int configRes;
    int fcloseStatus;
    int freeStatus;
    char freeString[100];

    /*
     * First initialize the part of the Server Global Data information
     * not being provided by the JDBC$CONFIG file or its default values.
     *
     * We will then retrieve the rest of the configuration information
     * from the JDBC$CONFIG file or use appropriate defaults.
     *
     * Note: for documentation purposes, all the fields in the sgd are
     * here: either as a comment indicating where the actual initial
     * value comes from, or being initialized as noted above.
     */
    /* sgd.server_LevelID */                            /* set by JDBCServer.c - The numeric (major, minor, feature, platform levels) ID of this JDBC Server code. */
    sgd.rdmsLevelID[0]='\0';                            /* No RDMS level ID has been retrieved yet by this JDBC Server. */
    sgd.rdmsLevelID[MAX_RDMS_LEVEL_LEN]='\0';           /* Make sure a NULL will always be at end of string (handles concurrent string copies) */
    sgd.rdmsBaseLevel=0;                                /* Don't know yet - the RDMS base level of the RDMS used by this JDBC Server. */
    sgd.rdmsFeatureLevel=0;                             /* Don't know yet - the RDMS feature level of the RDMS used by this JDBC Server. */
    sgd.er_level_value=0;                               /* set by JDBC$CONFIG$ - The ER Level$ A0 value for Server's switching priority. */
    sgd.server_priority[0]='\0';                        /* set by JDBC$CONFIG$ - The switching priority of the Server.                   */
    /* sgd.serverStartTimeValue */                      /* set by JDBCServer.c - The raw time of the Server Start up */
    /* sgd.option_bits */                               /* set by JDBCServer.c - The processor invocation option letters as bits */
    /* sgd.maxServerWorkers */                          /* set by JDBC$CONFIG$ - max. total number of Server Workers*/
    /* sgd.maxCOMAPIQueueSize */                        /* set by JDBC$CONFIG$ -maximum number of JDBC Clients allowed waiting on COMAPI queue */
    sgd.firstFree_WdeTS = TSQ_NULL;                     /* Initialize the T/S cell for the Free Worker Description Entry chain */
    sgd.numberOfFreeServerWorkers=0;                    /* No free Server Workers exist yet on the free Server Worker chain */
    sgd.numberOfShutdownServerWorkers=0;                /* No Server Workers have been shutdown yet by this Server.*/
    sgd.firstFree_wdePtr = NULL;                        /* No first entry yet on the chain of free (unassigned) Server Worker entries.*/
    sgd.firstAssigned_WdeTS = TSQ_NULL;                 /* Initialize the T/S cell for the all Worker Description Entry chain */
    sgd.numberOfAssignedServerWorkers=0;                /* No Server Workers exist yet on the assigned Server Worker chain */
    sgd.firstAssigned_wdePtr = NULL;                    /* No first entry yet on the chain of assigned Server Worker entries.*/
    /* sgd.appGroupName */                              /* set by JDBC$CONFIG$ - Name of Application Group */
    sgd.appGroupQualifier[0] = '\0';                    /* Qualifier for Application Group. If needed, set in get_UDS_ICR_bdi(). */
    /* sgd.appGroupNumber */                            /* set by JDBCServer.c - the Application Group's number */
    sgd.cumulative_total_AssignedServerWorkers=0;       /* No Server Workers have been assigned clients yet. */
    sgd.uds_icr_bdi = 0;                                /* set by JDBC$CONFIG$ - the Application Group's UDS ICR bdi */
    /* sgd.serverState */                               /* set by JDBCServer.c - Current state of the main JDBC Server as a whole.*/
    /* sgd.serverShutDownState */                       /* set by JDBCServer.c - Current shutdown state for the JDBC Server */
    /* sgd.consoleHandlerState */                       /* set by JDBCServer.c - Console Handler is not active at configuration time */
    /* sgd.consoleHandlerShutDownState */               /* set by JDBCServer.c - Current shutdown state for the Console Handler */
    /* sgd.debugXA */                                   /* set by server$corba.c or by a SET command- XA Server only, internal XA debug flag. Initially clear/set in server$corba. */
    /* sgd.server_receive_timeout */                    /* set by JDBC$CONFIG$ - Server socket receive timeout value. */
    /* sgd.server_send_timeout */                       /* set by JDBC$CONFIG$ - Server socket send timeout value. */
    /* sgd.server_activity_receive_timeout */           /* set by JDBC$CONFIG$ - Initial Client Server socket receive timeout value. */
    sgd.fetch_block_size=CONFIG_DEFAULT_FETCHBLOCK_SIZE; /* not used, this is current configuration default value */
    /* sgd.serverLogFileTS=TSQ_NULL; */                 /* set by ServerActWDE - part of minimalDefaultsSGD() */
    /* sgd.serverTraceFileTS=TSQ_NULL; */               /* set by ServerActWDE - part of minimalDefaultsSGD() */
    sgd.debug=0;                                        /* turn off debugging */
    sgd.clientDebugInternal=FALSE;                      /* turn off client debug internal */
    sgd.clientDebugDetail=FALSE;                        /* turn off client debug detail */
    /* sgd.generatedRunID[0] = '\0'; */                 /* set by JDBCServer.c - getRunIDs() */
    /* sgd.originalRunID[0] = '\0'; */                  /* set by JDBCServer.c - getRunIDs() */
    /* sgd.serverUserID[0] = '\0';                      /* set by JDBCServer.c - getServerUserID() */
    /* sgd.rdms_threadname_prefix_len */                /* set by JDBC$CONFIG$ - length (either 1 or 2) of the rdms_threadname_prefix string. */
    /* sgd.rdms_threadname_prefix */                    /* set by JDBC$CONFIG$ - two prefix characters for RDMS thread name. */
    /* sgd.clientTraceFileTS=TSQ_NULL; */               /* set by ServerActWDE - part of minimalDefaultsSGD() */
    sgd.clientTraceFileCounter = 0;                     /* Counter for the number of client trace files. */
    /* sgd.defaultClientTraceFileQualifier */           /* set by JDBC$CONFIG$ - Default qualifier to use for client trace files. */
    /* sgd.tempMaxNumWde */                             /* set by JDBCServer.c - max. number of Server Worker wde's in wdeArea (TEMPNUMWDE) */
    /* sgd.numWdeAllocatedSoFar */                      /* set by JDBCServer.c - indicate number allocated Server Worker wde's so far. */
    /* sgd.rsaBdi; */                                   /* Save the BDI for RSA, so we can restore it for each worker */
    sgd.postedServerReceiveTimeout = 0;                 /* Posted value (in milliseconds) from the CH cmd. SET SERVER RECEIVE TIMEOUT */
    sgd.postedServerSendTimeout = 0;                    /* Posted value (in milliseconds) from the CH cmd. SET SERVER SEND TIMEOUT */
    sgd.COMAPIDebug = SGD_COMAPI_DEBUG_MODE_OFF;        /* Value for no COMAPI debug */
    sgd.postedCOMAPIDebug = 0;                          /* Posted value for COMAPI debug (set to 64 if from SET COMAPI DEBUG=ON) */
    /* sgd.logConsoleOutput = 0;                        /* set by JDBC$CONFIG$ - If 1, console output is also sent to the log file */
    sgd.configKeyinID[0] = '\0';                        /* The configuration keyin_id parameter */
    sgd.keyinName[0] = '\0';                            /* The keyin name used for console commands for the server */
    sgd.clientCount = 0;                                /* Total number of clients attached to the server since it started */
                                                        /*   or since the count was cleared by CLEAR console command */
    sgd.requestCount = 0;                               /* Total number of request packets through the server since it started */
                                                        /*   or since the count was cleared  by CLEAR console command */
    sgd.lastRequestTimeStamp = 0;                       /* Timestamp of the last request packet.  Cleared by CLEAR console command. */
    sgd.lastRequestTaskCode = 0;                        /* Task code of the last request packet.  Cleared by CLEAR console command. */
    /* sgd.configFileName */                            /* set by server$corba.c or JDBCServer.c - XA or JDBC Server configuration file name. */
    sgd.clientTraceFileTracks = CLIENT_TRACEFILE_TRACKS_DEFAULT;    /* Size of Client Trace file in tracks (TRK) */
    sgd.clientTraceFileCycles = CLIENT_TRACEFILE_CYCLES_DEFAULT;    /* Number of Client trace file cycles to maintain */
    memset(sgd.clientTraceTable,0,sizeof(sgd.clientTraceTable));    /* Clear clientTraceTable */
    sgd.comapi_modes[0]= '\0';                          /* set up by JDBC$CONFIG$ - The COMAPI modes to be utilized.*/
    /* sgd.user_access_control;*/                       /* set by JDBC$CONFIG$ - Indicates if/how userid access security is done */
    /* sgd.UASFM_State; */                              /* set by ServerUASFM.c - Current state of the UASFM Activity (what is it doing).   */
    /* sgd.UASFM_ShutdownStates UASFM_ShutdownState; */ /* set by ServerUASFM.c - Indicator used to notify UASFM to shut down.              */
    sgd.SUVAL_bad_error_statuses=0;                     /* Set it so first bad SUVAL$ error status (H1) and function return status (H2) values are saved. */
    /* sgd.user_access_file_name[USER_ACCESS_FILENAME_LEN+1];*/ /* set by JDBCServer.c - The JDBC user access security filename for this appgroup */
    sgd.which_MSCON_li_pkt = 1;                         /* Set it so the UASFM activities first real lead item packet retrieved will be into 1st array element. */
    sgd.MSCON_li_status[0] = MSCON_GOT_LEAD_ITEM;       /* First MSCON$ will set this returned error status value. */
    sgd.MSCON_li_status[1] = MSCON_GOT_LEAD_ITEM;       /* Set so the first MSCON$, hopefully good, will not cause a log message. */
    sgd.ICL_handling_SW_CH_shutdown= NO_ICL_IS_CURRENTLY_HANDLING_SW_CH_SHUTDOWN; /* clear the ICL number for SW and CH shutdown handling. */
    sgd.fullLogFileName[0] = '\0';
    /*  sgd.FFlag_SupportsSQLSection */                 /* Set in the retrieve_RDMS_level_info() routine that is called by the Server on startup */
    /*  sgd.FFlag_SupportsFetchToBuffer */              /* Set in the retrieve_RDMS_level_info() routine that is called by the Server on startup */

#ifdef XABUILD  /* XA JDBC Server */
    sgd.totalStatefulThreads = 0;                       /* The total number of XA Server's non-XA (stateful) client database threads opened.     */
    sgd.totalXAthreads = 0;                             /* The total number of XA Server's XA transactional client database threads opened.      */
    sgd.totalXAReuses = 0;                              /* The total number of XA Server's XA database thread reuses.                            */
    /* sgd.XA_thread_reuse_limit */                     /* set by JDBC$CONFIG$ - XA database thread reuse limit.                                 */
    /* sgd. trx_mode */                                 /* set by XAServerWorker() - The XA Server obtained tx_info() returned value.            */
    /* sgd.transaction_state */                         /* set by XAServerWorker() - The XA Server obtained transaction_state value from tx_info(). */
    sgd.saved_beginThreadTaskCode = 0;                  /* This will force clearSavedXA_threadInfo() to initially clear all the relevant SGD     */
                                                        /* fields XA_thread_reuses_so_far to saved_versionName.                                  */
    clearSavedXA_threadInfo();                          /* Initially clear saved User id, database thread access type, recovery type fields, etc */
#endif          /* XA JDBC Server */

    /*
     * Retrieve and store configuration information for the serverGlobalData
     * structure sgd from the configuration file. Return to caller the
     * error_code that was returned.
     */
    configRes = processConfigFile(configFileName);

#if DEBUG
    printf("*** config result %d\n", configRes);
#endif

    fcloseStatus = fclose(configFile);

#if DEBUG
    printf("*** fclose result %d\n", fcloseStatus);
#endif

    sprintf(freeString, "@free,r %s . ", configFileName);
    freeStatus = doACSF(freeString);

#if DEBUG
    printf("*** free result %d\n", freeStatus);
#endif

    /* return processConfigFile(configFileName); */
    return configRes;
 }

/*
 * The following routines process validate and set
 * JDBC Server Configuration parameters.  Each routine
 * handles part of the task and uses various defines and
 * values available within the JDBC Server program files.
 *
 * This section describes the instructions on adding new
 * Configuration Parameters to the JDBC$Config file and
 * having them processed by these routines. (For these
 * steps, an example of a new config parameter
 * called max_free will be used).
 *
 * There are 3 major steps that have to be done:
 * a) Define the configuration parameter xxxx:
 *     1) define the parameter name for use in the
 *        JDBC$Config file, and what type of value it
 *        can have assigned (e.g. integer, string, file,
 *        userid). Example: max_free, an integer
 *        indicating maximum number of free Server Workers
 *        that exist.
 *
 *     2) define the sgd or wde parameter that will hold
 *        the configruation parameter. Keep Server values
 *        in the sgd and individual client values in the
 *        wde (i.e. their Server Workers operating data).
 *        Remember that the current Server Worker's wde
 *        is pointed to by a wdePtr in this code.
 *        Example: sgd.max_free_SW
 *
 *     3) define and add appropriate define constants for
 *        this config parameter in the areas of JDBCServer/h
 *        (or wherever these define constants are kept).
 *
 *        To define the parameter and its bounds:
 *        a) CONFIG_xxxx - use next higher integer
 *        b) CONFIG_DEFAULT_xxxx - default product value,
 *                                 if one is defined
 *        c) CONFIG_LIMIT_MINxxxx  - the allowed minimum value,
 *                                 if a fixed one is defined
 *        d) CONFIG_LIMIT_MAXxxxx  - the allowed maximum value,
 *                                 if a fixed one is defined
 *
 *        To define error/warning handling:
 *        e) JDBCSERVER_CONFIG_xxxx_ERROR    - error number used for
 *                                             basic bad parameter
 *                                             syntax or general error
 *        f) JDBCSERVER_CONFIG_MISSING_xxxx_ERROR - error number used
 *                                                  when a required
 *                                                  parameter is
 *                                                  missing from JDBC$Config
 *        g) JDBCSERVER_VALIDATE_xxxx_ERROR  - error number,
 *                                             if parameter value
 *                                             is unacceptable/bad
 *        h) JDBCSERVER_VALIDATE_xxxx_WARNING  - warning number,
 *                                             if bad/specified
 *                                             parameter value is
 *                                             replaced by a system
 *                                             determined value. This
 *                                             number is always <0 and
 *                                             is usually the error
 *                                             number negated.
 *        Example: CONFIG_FREE_SERVERWORKERS,
 *        CONFIG_DEFAULT_FREE_SERVERWORKERS,
 *        CONFIG_LIMIT_MINFREE_SERVERWORKERS,
 *        CONFIG_LIMIT_MAXFREE_SERVERWORKERS,
 *        JDBCSERVER_CONFIG_FREE_SERVERWORKERS_ERROR,
 *        JDBCSERVER_CONFIG_MISSING_FREE_SERVERWORKERS_ERROR
 *        JDBCSERVER_VALIDATE_FREE_SERVERWORKERS_ERROR,
 *        JDBCSERVER_VALIDATE_FREE_SERVERWORKERS_WARNING
 *
 * b) Add code to processConfigFile() to process xxxx, the
 *    new configuration parameter. (See the code that handles
 *    other configuration parameters for code placement, etc.)
 *    Function processConfigFile() assumes all parameters will
 *    be validated and will be placed into the sgd as the
 *    accepted initial operational configuration.
 *
 *    a) Add a variable called got_server_name and assign it
 *       the initial value GOT_NO_CONFIG_VALUE.
 *    b) Add an else if piece of code to the "Initial Syntactic Check"
 *       area to detect and retrieve the value of the parameter.
 *       Initially store the retrieved value into the appropriate
 *       sgd location ( it will be validated at a later step),
 *       for example sgd.max_free.
 *       Remember, we do not have a running system yet, so we are
 *       using the sgd since it is the place when the final accepted
 *       values need to end up anyway. Remember to set got_xxxx to
 *       GOT_CONFIG_VALUE if the parameter was present.
 *    c) Add an if test piece of code to the "Missing Parameter Detection"
 *       area to detect if the parameter was not provided in the
 *       JDBC$Config file.  This code decides if this is an error or
 *       whether the products default value can be used. See other
 *       configuration parameter's code for examples on what to code.
 *       Set the got_xxxx to HAVE_CONFIG_DEFAULT if a default had to
 *       be assigned.
 *    d) Add an if test piece of code to the "Semantic Checking" area to
 *       validate the configuration parameter value.  This code will
 *       use one of the validate_config_yyyy routines ( where yyyy is
 *       currently either int, string, file, or userid. Other routines
 *       can be written to handle new data types).  The call to that
 *       routine must pass the sgd value being tested as an argument,
 *       since its the functions argument value that being validated
 *       against the other configuration parameters in the sgd.
 *
 *       Remember to pass the other appropriate defines and values so the
 *       correct validation operation occurs. Notice that there is
 *       the ability to use both constant value (e.g. config defines) or
 *       actual values to drive the validation.  This allows parameters
 *       to be constrained within current operating values not just static
 *       product defaults.
 *
 *       If all goes well, the configuration value in the sgd or wde will
 *       acceptable and the next parameter is validated. If not, the
 *       appropriate error status is returned - no further parameters are
 *       validated since the Server will not begin execution. User
 *       debugging of the JDBC$Config will be needed to fix the bad parameter.
 *
 * c) Add code to all of the set_config_yyyy routines ( where
 *    yyyy is currently either int, string, file, userid, or an
 *    other routine written to handle new data types):
 *    1) In the set_config_yyyy routine that is supposed to set this
 *       parameter,add a case statement into the switch command storing
 *       the CONFIG_xxxx parameter into the sgd or wde. ( Note: this
 *       routine can set values in a JDBC Clients wde of the current
 *       Server Worker). For example CONFIG_FREE_SERVERWORKERS is set
 *       would be set in set_config_int().
 *    2) In those set_config_yyyy routines not processing the parameter,
 *       add the case statement to the error return area.
 *
 *
 * Note that the validate_config_yyyy and set_config_yyyy routines
 * can be called at JDBC Server intial configuration set up time,
 * during an execution JDBC Server, or during the execution of a JDBC
 * Client to:
 *     a) Validate and set configuration values in the sgd for usage by
 *        JDBC Server to controlits overall operation.
 *     b) Validate and set client configuration values in a wde for
 *        by a Server Worker activity to manage a JDBC Client. Client
 *        configuration values are validated against the JDBC Server's
 *        operating configuration to insure proper client operation.
 *
 * It is to accomodate the runtime validation and setting that has the
 * processConfigFile() function using the sgd as a "staging" area for
 * potentially acceptable configuration parameters.  Should this approach
 * not work for some configuration parameter (e.g. we can't validate it
 * using the sgd context alone), then processConfigFile() would have to be
 * modified to provide a more complete "staging" environment to validate
 * the initial configuration as provided in JDBC$Config.
 *
 */

/*
 * Function set_config_int
 *
 * This sets the specified configuration parameter in the
 * sgd to the argument value specified.
 *
 * Parameters:
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are setting.
 *
 * value           - The integer value to use.
 *
 * err_num         - Error number to return if configparam is not a
 *                   valid.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value
 *
 * error_status    - 0 if configuration value was set without error, otherwise
 *                   the err_num value is returned.
 */
int set_config_int(int configparam, int value, int err_num,
                   workerDescriptionEntry *wdePtr){

    int eventStatus;

    /* Do a switch statement on the configuration parameter being set */
    switch(configparam){

        case CONFIG_MAX_ACTIVITIES:{
            sgd.maxServerWorkers=value;
            break;
        }
        case CONFIG_MAX_QUEUED_COMAPI:{
            sgd.maxCOMAPIQueueSize=value;
            break;
        }
        case CONFIG_HOSTPORTNUMBER:{
            sgd.localHostPortNumber=value;
            break;
        }
        case CONFIG_SERVER_RECEIVE_TIMEOUT:{
            /*
             * Call Pass_Event() to have the value set.
             * If there are no active Server Sockets,
             * then just set the new socket receive
             * timeout value directly. This allows the COMAPI
             * system to be called (via Set_Socket_Options)
             * with the new value when the socket is obtained.
             */

             sgd.postedServerReceiveTimeout = value;
#ifndef XABUILD /* JDBC Server */
             eventStatus = Pass_Event();
#endif          /* JDBC Server */

#ifdef XABUILD  /* XA JDBC Server */
             eventStatus = NO_SERVER_SOCKETS_ACTIVE;
#endif          /* XA JDBC Server */
             if (eventStatus == NO_SERVER_SOCKETS_ACTIVE){
                sgd.server_receive_timeout = value;
                sgd.postedServerReceiveTimeout = 0; /* no need to post it. */
             }
            break;
        }
        case CONFIG_SERVER_SEND_TIMEOUT:{
            /*
             * Call Pass_Event() to have the value set.
             * If there are no active Server Sockets,
             * then just set the new socket send
             * timeout value directly. This allows the COMAPI
             * system to be called (via Set_Socket_Options)
             * with the new value when the socket is obtained.
             */

             sgd.postedServerSendTimeout = value;
#ifndef XABUILD /* JDBC Server */
             eventStatus = Pass_Event();
#endif          /* JDBC Server */

#ifdef XABUILD  /* XA JDBC Server */
             eventStatus = NO_SERVER_SOCKETS_ACTIVE;
#endif          /* XA JDBC Server */
             if (eventStatus == NO_SERVER_SOCKETS_ACTIVE){
                sgd.server_send_timeout = value;
                sgd.postedServerSendTimeout  = 0; /* no need to post it. */
             }
            break;
        }
        case CONFIG_CLIENT_RECEIVE_TIMEOUT:{
            sgd.server_activity_receive_timeout=value;
            break;
        }
        case CONFIG_SW_CLIENT_RECEIVE_TIMEOUT:{
            wdePtr->client_receive_timeout=value;
            break;
        }

        /*
         * if configparam is one of these values, or outside
         * the legal range, return the error number in err_num
         */
        case CONFIG_FETCHBLOCK_SIZE:
        case CONFIG_SW_FETCHBLOCK_SIZE: /* this value is not saved in wde */
        case CONFIG_SERVERNAME:
        case CONFIG_APPGROUPNAME:
        case CONFIG_SERVERLOGFILENAME:
        case CONFIG_SERVERTRACEFILENAME:
        case CONFIG_SW_CLIENT_TRACEFILENAME:
        case CONFIG_CLIENT_DEFAULT_TRACEFILE_QUAL:
        case CONFIG_SERVER_LOCALE:
        default:{
            /* bad configurparam value, return error */
            return err_num;
        }
    }
    return 0; /* configuration parameter set */
}


/*
 * Function validate_config_int
 *
 * This function validates an integer configuration parameter and
 * determines if it is an acceptable JDBC Server configuration value.
 * The value is tested to be within minimum and maximum values passed
 * as arguments to the function.  If a product default value is
 * available, it be provided.  Then upon option, the value passed can
 * be set as the new configuration value or the default used, all
 * based on a mode parameter to the function.
 *
 * Parameters:
 *
 * testvalue        - The test integer value to validate.
 *
 * mode             - The validation mode the function is in
 *                    ( as provided by a #define value):
 *                    VALIDATE_ONLY - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server.  No change to
 *                                    the JDBC Servers operating
 *                                    configuration (e.g. sgd )is
 *                                    made by this function.
 *                    VALIDATE_AND_UPDATE - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ).
 *                    VALIDATE_AND_UPDATE_ALLOW_DEFAULT - Check if the
 *                                    argument is acceptable with the
 *                                    other operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ). If not,
 *                                    use the current aceptable value, i.e
 *                                    the products default value or a
 *                                    value dictated by the current operating
 *                                    configuration.
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are working on.
 *
 * minvalue        - The minimum allowed value that this configuration
 *                   parameter can be set to.
 *
 * maxvalue        - The maximum allowed value that this configuration
 *                   parameter can be set to.
 *
 *
 * def_available   - Indicates if a default value is available and provided
 *                   via the defvalue parameter. 0 indicates no default value
 *                   is supplied, non-0 means that defvalue is provided. Note:
 *                   the allowed values for def_available allow use of BOOL_FALSE
 *                   and BOOL_TRUE, or TRUE and FALSE defined tags to be used to
 *                   indicate this arguments value.
 *
 * defvalue        - The default value ( either a product default or the
 *                   allowable default based on current runtime configuration)
 *                   that can be used if the test value does not satisfy the
 *                   test criteria.
 *
 * err_num         - Error number to return if the validation fails in error.
 *                   This number must be greater than 0.
 *
 * warn_num        - Warning number to return if the validation fails in error.
 *                   This number must be less than 0.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value:
 *
 * status          - The returned JDBC Server error message indicator
 *                   that tells how the functions operation went. The
 *                   returned values are:
 *                   0 = parameter was acceptable and the desired action
 *                       as indicated by the mode was done.
 *                   <0 = the parameter was not acceptable, but there
 *                        is a product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The err_num value is whats returned.
 *                   >0 = the parameter was not acceptable, and there
 *                        is no product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The warn_num value is whats returned.
 *
 */
 int validate_config_int(int testvalue, int mode, int configparam,
                         int minvalue, int maxvalue,int def_available,
                         int defvalue, int err_num, int warn_num,
                         workerDescriptionEntry *wdePtr){

    int status;

    /* test if test value is valid */
    if ((testvalue >= minvalue) &&
        (testvalue <= maxvalue) ) {
            /*
             * The test value is ok. What does the
             * caller want to do with it.
             */
            if (mode == VALIDATE_ONLY){
                return 0;
            }
            else if (mode == VALIDATE_AND_UPDATE ||
                     mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
                /* Update the sgd with the acceptable value */
                return set_config_int(configparam, testvalue, err_num, wdePtr);
                }
                else{
                     /* bad mode value, just return validation error */
                     return err_num;
                    }
    }
    else {
        /* printf("in validate_int - value not acceptable %i\n",testvalue); */
        /* value was not acceptable, do they want the default? */
        if (mode == VALIDATE_ONLY ||
            mode == VALIDATE_AND_UPDATE){
            /* validate failed, so indicate failure */
            return err_num;
        }
        else if (mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
               /* Update the sgd with the acceptable default value */
               status=set_config_int(configparam, defvalue, err_num, wdePtr);
               /* return warning if set went ok, else return error */
               return status ? err_num : warn_num ;
             }
             else {
                 /* bad mode value, just return validation error */
                 /* printf("validate_config_int() passed bad values: %i, %i\n",
                            mode,testvalue); */
                 return err_num;
             }
    }
}


/*
 * Function set_config_string
 *
 * This sets the specified configuration parameter in the
 * sgd to the argument value specified.
 *
 * Parameters:
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are setting.
 *
 * valuePtr        - Pointer to the character string value to use.
 *
 * err_num         - Error number to return if configparam is not a
 *                   valid.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value:
 *
 * error_status    - 0 if configuration value was set without error, otherwise
 *                   the err_num value is returned.
 */
int set_config_string(int configparam, char *valuePtr, int err_num,
                      workerDescriptionEntry *wdePtr){

    char * ptr1;
    int errorCode;
    FILE * localMsgFile;
    char localMsgFileName[MAX_FILE_NAME_LEN];

    /* Do a switch statement on the configuration parameter being set */
    switch(configparam){
        case CONFIG_SERVERNAME:{
            strcpy(sgd.serverName,valuePtr);
            break;
        }
        case CONFIG_APPGROUPNAME:{
            strcpy(sgd.appGroupName,valuePtr);
            break;
        }
        case CONFIG_CLIENT_DEFAULT_TRACEFILE_QUAL:{
            strcpy(sgd.defaultClientTraceFileQualifier,valuePtr);
            break;
        }
        case CONFIG_SERVER_LOCALE:{
            strcpy(sgd.serverLocale,valuePtr);

            /* Open the message file, and change the
               server message file pointer and the server message file name,
               based on the locale.
               Also change the activity WDE's message and locale fields.
               Note: This is currently only called from config processing
               in the main program.
               If we ever allow the Console Handler to set the locale,
               then in the code below we need to change the four
               activity WDE fields for not only the
               main program / console activity,
               but also the ICL activity.
               */
            ptr1 = localMsgFileName;

            errorCode = openMessageFile(sgd.serverLocale, &localMsgFile, &ptr1);

            if (errorCode == 0) {
                sgd.serverMessageFile = localMsgFile;
                strcpy(sgd.serverMessageFileName, localMsgFileName);

                act_wdePtr->serverMessageFile = sgd.serverMessageFile;
                    /* Use the Server's locale message file */
                act_wdePtr->workerMessageFile = sgd.serverMessageFile;
                    /* Use the Server's locale message file */
                strcpy(act_wdePtr->workerMessageFileName,
                    sgd.serverMessageFileName);
                    /* the Server's locale message filename */
                strcpy(act_wdePtr->clientLocale, sgd.serverLocale);
                    /* Use the server's locale */
            }

            break;
        }

        /*
         * if configparam is one of these values, or outside
         * the legal range, return the error number in err_num
         */
        case CONFIG_MAX_ACTIVITIES:
        case CONFIG_MAX_QUEUED_COMAPI:
        case CONFIG_HOSTPORTNUMBER:
        case CONFIG_SERVER_RECEIVE_TIMEOUT:
        case CONFIG_SERVER_SEND_TIMEOUT:
        case CONFIG_CLIENT_RECEIVE_TIMEOUT:
        case CONFIG_SERVERLOGFILENAME:
        case CONFIG_SERVERTRACEFILENAME:
        case CONFIG_SW_CLIENT_RECEIVE_TIMEOUT:
        case CONFIG_FETCHBLOCK_SIZE:
        case CONFIG_SW_FETCHBLOCK_SIZE:
        case CONFIG_SW_CLIENT_TRACEFILENAME:
        default:{
            /* bad configurparam value, return error */
            return err_num;
        }
    }
    return 0; /* configuration parameter set */
}


/*
 * Function validate_config_string
 *
 * This function validates an integer configuration parameter and
 * determines if it is an acceptable JDBC Server configuration value.
 * The value is tested to be within minimum and maximum values passed
 * as arguments to the function.  If a product default value is
 * available, it be provided.  Then upon option, the value passed can
 * be set as the new configuration value or the defualt used, all
 * based on a mode parameter to the function.
 *
 * Parameters:
 *
 * testvaluePtr     - Pointer to the test string value to validate.
 *
 * mode             - The validation mode the function is in
 *                    ( as provided by a #define value):
 *                    VALIDATE_ONLY - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server.  No change to
 *                                    the JDBC Servers operating
 *                                    configuration (e.g. sgd )is
 *                                    made by this function.
 *                    VALIDATE_AND_UPDATE - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ).
 *                    VALIDATE_AND_UPDATE_ALLOW_DEFAULT - Check if the
 *                                    argument is acceptable with the
 *                                    other operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ). If not,
 *                                    use the current aceptable value, i.e
 *                                    the products default value or a
 *                                    value dictated by the current operating
 *                                    configuration.
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are working on.
 *
 * minvaluelen     - The minimum allowed length (in chars) that this
 *                   configuration parameter can be set to ( does not
 *                   include trailing null character).
 *
 * maxvaluelen     - The maximum allowed length (in chars) that this
 *                   configuration parameter can be set to ( does not
 *                   include trailing null character).
 *
 * def_available   - Indicates if a default value is available and provided
 *                   via the defvalue parameter. 0 indicates no default value
 *                   is supplied, non-0 means that defvalue is provided. Note:
 *                   the allowed values for def_available allow use of BOOL_FALSE
 *                   and BOOL_TRUE, or TRUE and FALSE defined tags to be used to
 *                   indicate this arguments value.
 *
 * defvaluePtr      - The default value ( either a product default or the
 *                   allowable default based on current runtime configuration)
 *                   that can be used if the test value does not satisfy the
 *                   test criteria.
 *
 * err_num         - Error number to return if the validation fails in error.
 *                   This number must be greater than 0.
 *
 * warn_num        - Warning number to return if the validation fails in error.
 *                   This number must be less than 0.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value:
 *
 * status          - The returned JDBC Server error message indicator
 *                   that tells how the functions operation went. The
 *                   returned values are:
 *                   0 = parameter was acceptable and the desired action
 *                       as indicated by the mode was done.
 *                   <0 = the parameter was not acceptable, but there
 *                        is a product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The err_num value is whats returned.
 *                   >0 = the parameter was not acceptable, and there
 *                        is no product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The warn_num value is whats returned.
 *
 */
 int validate_config_string(char * testvaluePtr, int mode, int configparam,
                         int minvaluelen, int maxvaluelen,int def_available,
                         char * defvaluePtr, int err_num, int warn_num,
                         workerDescriptionEntry *wdePtr){

    int status;
    int testvaluelen;

    /* test if test value is valid */
    testvaluelen=strlen(testvaluePtr);
    if ((testvaluelen >= minvaluelen) &&
        (testvaluelen <= maxvaluelen) ) {
            /*
             * The test value is ok. What does the
             * caller want to do with it.
             */
            if (mode == VALIDATE_ONLY){
                return 0;
            }
            else if (mode == VALIDATE_AND_UPDATE ||
                     mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
                /* Update the sgd with the acceptable value */
                return set_config_string(configparam, testvaluePtr, err_num, wdePtr);
                }
                else{
                     /* bad mode value, just return validation error */
                     return err_num;
                    }
    }
    else {
        /* value was not acceptable, do they want the default? */
        if (mode == VALIDATE_ONLY ||
            mode == VALIDATE_AND_UPDATE){
            /* validate failed, so indicate failure */
            return err_num;
        }
        else if (mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
               /* Update the sgd with the acceptable default value */
               status=set_config_string(configparam, defvaluePtr, err_num, wdePtr);
               /* return warning if set went ok, else return error */
               return status ? err_num : warn_num ;
             }
             else {
                 /* bad mode value, just return validation error */
                 return err_num;
             }
    }
}

/*
 * Function set_config_filename
 *
 * This sets the specified configuration parameter in the
 * sgd to the argument value specified.
 *
 * Since these are files that are used by the JDBC Server
 * or its Server Workers, the attempt to set these file
 * parameters includes opening the files and placing the
 * appropriate @use names on them. In the case of some files
 * e.g. Server Log or Trace file, an initial set of output
 * images are also written to the file.
 *
 * Parameters:
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are setting.
 *
 * valuePtr        - Pointer to the character string value to use.
 *
 * err_num         - Error number to return if configparam is not a
 *                   valid.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value:
 *
 * error_status    - 0 if configuration value was set without error, otherwise
 *                   the err_num value is returned.
 */
int set_config_filename(int configparam, char *valuePtr, int err_num,
                        workerDescriptionEntry *wdePtr){


    int useErr;
    int closeErr;
    FILE *newfile;
    FILE *oldfile;
    char oldfileName[MAX_FILE_NAME_LEN+1];
    char textMessage[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char digits[ITOA_BUFFERSIZE];

    /* a buffer for a generated message */
    /* Do a switch statement on the configuration parameter being set */
    switch(configparam){
        case CONFIG_SERVERLOGFILENAME:{
            /* Save file name and handle to current file */
            oldfile=sgd.serverLogFile;
            strcpy(oldfileName,sgd.serverLogFileName);


            /* Try to fopen the new file */
            newfile=fopenWithErase(valuePtr);
            if (newfile == NULL){
                /* fopen() failed, so do not switch to the
                 * new log file and return an error.
                 */
                return err_num;
                }
            else {
                /*
                 * Switch to the new Log file. This must be done
                 * under T/S control since another activity might
                 * try to write to the existing Log file at this time.
                 *
                 * Put the new name and handle in the sgd.
                 */
                test_set(&(sgd.serverLogFileTS));  /* Set the Log file's T/S cell */
                sgd.serverLogFile=newfile;
                strcpy(sgd.serverLogFileName,valuePtr);
                ts_clear_act(&(sgd.serverLogFileTS)); /* Clear T/S cell */

                /*
                 * Do an @USE for the new log file.  If it fails or gets
                 * warnings, then add a Server log entry, but do not
                 * stop setting up the new Log file since it did open
                 * correctly.
                 */
                sprintf(textMessage, "@USE %s,%s . ", LOG_FILE_USE_NAME,
                        sgd.serverLogFileName);
                useErr = doACSF(textMessage);
                if (useErr < 0) {
                    getLocalizedMessageServer(SERVER_LOG_FILE_USE_FAILED,
                        itoa(useErr, digits, 8), NULL, SERVER_LOGS, textMessage);
                            /* 5332 @USE on server log file name failed;
                               octal status {0} */
                }

                /*
                 * Now close the old Server Log file, if one is present. If there
                 * is a problem closing the file, write a Log record into the new
                 * Log file.
                 *
                 */
               if (oldfile != NULL){
                 closeErr=fclose(oldfile);
                 if (closeErr){
                    /* File did not fclose without error */
                    getLocalizedMessageServer(SERVER_LOG_FILE_CLOSE_FAILED,
                        oldfileName, itoa(closeErr, digits, 8), SERVER_LOGS, textMessage);
                            /* 5334 The fclose() on Server Log file {0} failed;
                               octal status {1} */
                 }
               }

               /*
                * Get the log file name for use in Console messages when server is shutting down in error.
                * Store this value in sgd for use by other modules.
                *
                */
               getFullFileName(LOG_FILE_USE_NAME, sgd.fullLogFileName, TRUE);

            }
            break;
        }

        case CONFIG_SERVERTRACEFILENAME:{
            /* Save file name and handle to current file */
            oldfile=sgd.serverTraceFile;
            strcpy(oldfileName,sgd.serverTraceFileName);

            /* Is new trace file PRINT$? */
            if (strcmp(valuePtr, PRINT$_FILE) == 0){
            	/* Print$ is always open. */
            	newfile = stdout;
            } else {
            	/* Try to fopen the new file. */
                newfile=fopenWithErase(valuePtr);
            }

            if (newfile == NULL){
                /* fopen() failed, so do not switch to the
                 * new Trace file and return an error.
                 */
                return err_num;
                }
            else {
                /*
                 * Switch to the new Trace file. This must be done
                 * under T/S control since another activity might
                 * try to write to the existing Trace file at this time.
                 *
                 * Put the new name and handle in the sgd.
                 */
                test_set(&(sgd.serverTraceFileTS));  /* Set the Trace file's T/S cell */
                sgd.serverTraceFile=newfile;
                strcpy(sgd.serverTraceFileName,valuePtr);
                ts_clear_act(&(sgd.serverTraceFileTS)); /* Clear T/S cell */
                /*
                 * If the trace file is not PRINT$, we need to do a @USE
                 * on the file name. If it fails or gets
                 * warnings, then add a Server log entry, but do not
                 * stop setting up the new Trace file since it did open
                 * correctly.
                 */
                if (strcmp(sgd.serverTraceFileName, PRINT$_FILE) != 0){
                    sprintf(textMessage, "@USE %s,%s . ", TRACE_FILE_USE_NAME,
                            sgd.serverTraceFileName);
                    useErr = doACSF(textMessage);
                    if (useErr < 0) {
                        getLocalizedMessageServer(SERVER_TRACE_FILE_USE_FAILED,
                              itoa(useErr, digits, 8), NULL, SERVER_TRACES, textMessage);
                              /* 5333 @USE on server trace file name failed;
                                      octal status {0} */
                    }
                }

                /*
                 * Now close the old Server Trace file if one is present. If there
                 * is a problem closing the file, write a Trace record into the
                 * existing Log and new Trace file. Do not try to close PRINT$ if
                 * it was the old file.
                 */
                if ((strcmp(oldfileName, PRINT$_FILE) != 0) && oldfile != NULL){
                    closeErr=fclose(oldfile);
                    if (closeErr){
                        /* File did not fclose without error */
                        getLocalizedMessageServer(
                            SERVER_TRACE_FILE_CLOSE_FAILED, oldfileName,
                            itoa(closeErr, digits, 8), SERVER_TRACES, textMessage);
                                /* 5335 The fclose() on Server Trace file
                                   {0} failed; octal status {1} */
                    }
                }

            }
            break;
        }

        case CONFIG_SW_CLIENT_TRACEFILENAME:{
            /* not implemented yet xxxxx, return error */
            return err_num; /* no break; needed */
        }

        /*
         * if configparam is one of these values, or outside
         * the legal range, return the error number in err_num
         */
        case CONFIG_MAX_ACTIVITIES:
        case CONFIG_MAX_QUEUED_COMAPI:
        case CONFIG_HOSTPORTNUMBER:
        case CONFIG_SERVER_RECEIVE_TIMEOUT:
        case CONFIG_SERVER_SEND_TIMEOUT:
        case CONFIG_CLIENT_RECEIVE_TIMEOUT:
        case CONFIG_SERVERNAME:
        case CONFIG_APPGROUPNAME:
        case CONFIG_SW_CLIENT_RECEIVE_TIMEOUT:
        case CONFIG_FETCHBLOCK_SIZE:
        case CONFIG_SW_FETCHBLOCK_SIZE:
        case CONFIG_CLIENT_DEFAULT_TRACEFILE_QUAL:
        case CONFIG_SERVER_LOCALE:
        default:{
            /* bad configurparam value, return error */
            return err_num;
        }
    }
    return 0; /* configuration parameter set */
}


/*
 * Function validate_config_filename
 *
 * This function validates an integer configuration parameter and
 * determines if it is an acceptable JDBC Server configuration value.
 * The value is tested to be within minimum and maximum values passed
 * as arguments to the function.  If a product default value is
 * available, it be provided.  Then upon option, the value passed can
 * be set as the new configuration value or the defualt used, all
 * based on a mode parameter to the function.
 *
 * Parameters:
 *
 * testvaluePtr     - Pointer to the test string value to validate.
 *
 * mode             - The validation mode the function is in
 *                    ( as provided by a #define value):
 *                    VALIDATE_ONLY - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server.  No change to
 *                                    the JDBC Servers operating
 *                                    configuration (e.g. sgd )is
 *                                    made by this function.
 *                    VALIDATE_AND_UPDATE - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ).
 *                    VALIDATE_AND_UPDATE_ALLOW_DEFAULT - Check if the
 *                                    argument is acceptable with the
 *                                    other operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ). If not,
 *                                    use the current aceptable value, i.e
 *                                    the products default value or a
 *                                    value dictated by the current operating
 *                                    configuration.
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are working on.
 *
 * minvaluelen     - The minimum allowed length (in chars) that this
 *                   configuration parameter can be set to ( does not
 *                   include trailing null character).
 *
 * maxvaluelen     - The maximum allowed length (in chars) that this
 *                   configuration parameter can be set to ( does not
 *                   include trailing null character).
 *
 * def_available   - Indicates if a default value is available and provided
 *                   via the defvalue parameter. 0 indicates no default value
 *                   is supplied, non-0 means that defvalue is provided. Note:
 *                   the allowed values for def_available allow use of BOOL_FALSE
 *                   and BOOL_TRUE, or TRUE and FALSE defined tags to be used to
 *                   indicate this arguments value.
 *
 * defvaluePtr      - The default value ( either a product default or the
 *                   allowable default based on current runtime configuration)
 *                   that can be used if the test value does not satisfy the
 *                   test criteria.
 *
 * err_num         - Error number to return if the validation fails in error.
 *                   This number must be greater than 0.
 *
 * warn_num        - Warning number to return if the validation fails in error.
 *                   This number must be less than 0.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value:
 *
 * status          - The returned JDBC Server error message indicator
 *                   that tells how the functions operation went. The
 *                   returned values are:
 *                   0 = parameter was acceptable and the desired action
 *                       as indicated by the mode was done.
 *                   <0 = the parameter was not acceptable, but there
 *                        is a product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The err_num value is whats returned.
 *                   >0 = the parameter was not acceptable, and there
 *                        is no product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The warn_num value is whats returned.
 *
 */
 int validate_config_filename(char * testvaluePtr, int mode, int configparam,
                         int minvaluelen, int maxvaluelen,int def_available,
                         char * defvaluePtr, int err_num, int warn_num,
                         workerDescriptionEntry *wdePtr){

    int status;
    int testvaluelen;

    /* test if test value is valid */
    testvaluelen=strlen(testvaluePtr);
    if ((testvaluelen >= minvaluelen) &&
        (testvaluelen <= maxvaluelen) ) {
            /*
             * The test value is ok. What does the
             * caller want to do with it.
             */
            if (mode == VALIDATE_ONLY){
                return 0;
            }
            else if (mode == VALIDATE_AND_UPDATE ||
                     mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
                /* Update the sgd with the acceptable value */
                return set_config_filename(configparam, testvaluePtr, err_num, wdePtr);
                }
                else{
                     /* bad mode value, just return validation error */
                     /* printf("validate_config_filename() passed bad values: %i, %s\n",
                               mode,testvaluePtr); */
                     return err_num;
                    }
    }
    else {
        /* value was not acceptable, do they want the default? */
        if (mode == VALIDATE_ONLY ||
            mode == VALIDATE_AND_UPDATE){
            /* validate failed, so indicate failure */
            return err_num;
        }
        else if (mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
               /* Update the sgd with the acceptable default value */
               status=set_config_filename(configparam, defvaluePtr, err_num, wdePtr);
               /* return warning if set went ok, else return error */
               return status ? err_num : warn_num ;
             }
             else {
                 /* bad mode value, just return validation error */
                 /* printf("validate_config_filename() passed bad values: %i, %s\n",
                            mode,testvalue); */
                 return err_num;
             }
    }
}

/*
 * Function set_config_userid
 *
 * This sets the specified configuration parameter in the
 * sgd to the argument value specified. Since these
 * parameters are user id's, they need to also be validated for validity
 * by some 2200 operating system code. Currently this is not done and
 * the userid is accepted as. ( xxxxxxx change this here)
 *
 * Parameters:
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are setting.
 *
 * valuePtr        - Pointer to the character string value to use.
 *
 * err_num         - Error number to return if configparam is not a
 *                   valid.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value:
 *
 * error_status    - 0 if configuration value was set without error, otherwise
 *                   the err_num value is returned.
 */
int set_config_userid(int configparam, char *valuePtr, int err_num,
                      workerDescriptionEntry *wdePtr){


    /* Do a switch statement on the configuration parameter being set */
    switch(configparam){
        /*
         * if configparam is one of these values, or outside
         * the legal range, return the error number in err_num
         */
        case CONFIG_MAX_ACTIVITIES:
        case CONFIG_MAX_QUEUED_COMAPI:
        case CONFIG_HOSTPORTNUMBER:
        case CONFIG_APPGROUPNAME:
        case CONFIG_SERVERNAME:
        case CONFIG_SERVER_RECEIVE_TIMEOUT:
        case CONFIG_SERVER_SEND_TIMEOUT:
        case CONFIG_CLIENT_RECEIVE_TIMEOUT:
        case CONFIG_SERVERLOGFILENAME:
        case CONFIG_SERVERTRACEFILENAME:
        case CONFIG_SW_CLIENT_RECEIVE_TIMEOUT:
        case CONFIG_FETCHBLOCK_SIZE:
        case CONFIG_SW_FETCHBLOCK_SIZE:
        case CONFIG_SW_CLIENT_TRACEFILENAME:
        case CONFIG_CLIENT_DEFAULT_TRACEFILE_QUAL:
        case CONFIG_SERVER_LOCALE:
        default:{
            /* bad configurparam value, return error */
            return err_num;
        }
    }
    /* No actual sets of userid occurring  return 0; */ /* configuration parameter set */
}


/*
 * Function validate_config_userid
 *
 * This function validates an integer configuration parameter and
 * determines if it is an acceptable JDBC Server configuration value.
 * The value is tested to be within minimum and maximum values passed
 * as arguments to the function.  If a product default value is
 * available, it be provided.  Then upon option, the value passed can
 * be set as the new configuration value or the defualt used, all
 * based on a mode parameter to the function. Since these parameters
 * are user id's, they need to also be validated for validity
 * by some 2200 operating system code. Currently this is not done and
 * the userid is accepted as. ( xxxxxxx change this here)
 *
 * Parameters:
 *
 * testvaluePtr     - Pointer to the test string value to validate.
 *
 * mode             - The validation mode the function is in
 *                    ( as provided by a #define value):
 *                    VALIDATE_ONLY - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server.  No change to
 *                                    the JDBC Servers operating
 *                                    configuration (e.g. sgd )is
 *                                    made by this function.
 *                    VALIDATE_AND_UPDATE - Check if the argument is
 *                                    acceptable with the other
 *                                    operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ).
 *                    VALIDATE_AND_UPDATE_ALLOW_DEFAULT - Check if the
 *                                    argument is acceptable with the
 *                                    other operational values in the
 *                                    JDBC Server. If so, update the
 *                                    JDBC Server's operating
 *                                    configuration (e.g. sgd ). If not,
 *                                    use the current aceptable value, i.e
 *                                    the products default value or a
 *                                    value dictated by the current operating
 *                                    configuration.
 *
 * configparam     - An integer value indicating which configuration
 *                   parameter we are working on.
 *
 * minvaluelen     - The minimum allowed length (in chars) that this
 *                   configuration parameter can be set to ( does not
 *                   include trailing null character).
 *
 * maxvaluelen     - The maximum allowed length (in chars) that this
 *                   configuration parameter can be set to ( does not
 *                   include trailing null character).
 *
 * def_available   - Indicates if a default value is available and provided
 *                   via the defvalue parameter. 0 indicates no default value
 *                   is supplied, non-0 means that defvalue is provided. Note:
 *                   the allowed values for def_available allow use of BOOL_FALSE
 *                   and BOOL_TRUE, or TRUE and FALSE defined tags to be used to
 *                   indicate this arguments value.
 *
 * defvaluePtr      - The default value ( either a product default or the
 *                   allowable default based on current runtime configuration)
 *                   that can be used if the test value does not satisfy the
 *                   test criteria.
 *
 * err_num         - Error number to return if the validation fails in error.
 *                   This number must be greater than 0.
 *
 * warn_num        - Warning number to return if the validation fails in error.
 *                   This number must be less than 0.
 *
 * wdePtr          - If the configparam indicated a JDBC Client configuration
 *                   parameter is being set, then this is the pointer to the
 *                   Server Worker wde that is being modified. If the
 *                   configparam is for a JDBCServer configuration parameter,
 *                   then this parameter is not used and should be set to NULL.
 *
 * Returned value:
 *
 * status          - The returned JDBC Server error message indicator
 *                   that tells how the functions operation went. The
 *                   returned values are:
 *                   0 = parameter was acceptable and the desired action
 *                       as indicated by the mode was done.
 *                   <0 = the parameter was not acceptable, but there
 *                        is a product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The err_num value is whats returned.
 *                   >0 = the parameter was not acceptable, and there
 *                        is no product default value that can be used.
 *                        The desired action as indicated by the mode
 *                        was done. The warn_num value is whats returned.
 *
 */
 int validate_config_userid(char * testvaluePtr, int mode, int configparam,
                         int minvaluelen, int maxvaluelen,int def_available,
                         char * defvaluePtr, int err_num, int warn_num,
                         workerDescriptionEntry *wdePtr){

    int status;
    int testvaluelen;

    /* test if test value is valid */
    testvaluelen=strlen(testvaluePtr);
    if ((testvaluelen >= minvaluelen) &&
        (testvaluelen <= maxvaluelen) ) {
            /*
             * The test value is ok. What does the
             * caller want to do with it.
             */
            if (mode == VALIDATE_ONLY){
                return 0;
            }
            else if (mode == VALIDATE_AND_UPDATE ||
                     mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
                /* Update the sgd with the acceptable value */
                return set_config_userid(configparam, testvaluePtr, err_num, wdePtr);
                }
                else{
                     /* bad mode value, just return validation error */
                     /* printf("validate_config_userid() passed bad values: %i, %s\n",
                               mode,testvaluePtr); */
                     return err_num;
                    }
    }
    else {
        /* value was not acceptable, do they want the default? */
        if (mode == VALIDATE_ONLY ||
            mode == VALIDATE_AND_UPDATE){
            /* validate failed, so indicate failure */
            return err_num;
        }
        else if (mode == VALIDATE_AND_UPDATE_ALLOW_DEFAULT){
               /* Update the sgd with the acceptable default value */
               status=set_config_userid(configparam, defvaluePtr, err_num, wdePtr);
               /* return warning if set went ok, else return error */
               return status ? err_num : warn_num ;
             }
             else {
                 /* bad mode value, just return validation error */
                 /* printf("validate_config_userid() passed bad values: %i, %s\n",
                            mode,testvalue); */
                 return err_num;
             }
    }
}

/*
 * Function determine_listen_host_format
 *
 * This function takes a listen host name string and determines in what
 * format the host name is provided.  Depending on the format, the function
 * returns either an explicit IP address, or an indicator value (-1) that
 * indicates that the host name must be looked up via a DNS service to get
 * its IP address value.
 *
 * An error status of 0 is returned if no error is detected in the listen
 * host name string , otherwise a non-zero error indicator value is
 * returned indicating the error detected.
 */
int determine_listen_host_format(char * listen_host_ptr, v4v6addr_type * explicit_ip_address){
    int ret;
    int status = 0; /* initially assume no error */
    int ip_int_part = 0; /* used with sscanf to determine if host name starts with an integer */
    int address_family = 0; /* Used to specify IPv4 or IPv6 format */

    /*
     * Determine if host name is an empty string, the string "0", an explicit
     * IP address, or a host name that has to be looked up at server socket
     * listen time.
     */
    if (strlen(listen_host_ptr) == 0 || strlen(listen_host_ptr) == 1 && listen_host_ptr[0] == '0') {
        /* Empty string or the string "0", use 0 as the listen IP address */
        (*explicit_ip_address).v4v6.v4 = DEFAULT_LISTEN_IP_ADDRESS;
        /* The family field must be AF_UNSPEC to allow listening on both an IPv4 and IPv6 address */
        (*explicit_ip_address).family = AF_UNSPEC;
    } else { /* Not 0, might be an explicit IP address or a host name */
        if (strchr(listen_host_ptr, ':') != NULL) {
            /* host name contains a ':', assume an explicit IPv6 address */
            address_family = AF_INET6;
        } else if (sscanf(listen_host_ptr,"%i", &ip_int_part) == 1) {
            /* host name starts with an integer, assume an explicit IPv4 address */
            address_family = AF_INET;
        }
        if (address_family != 0) { /* An IPv4 or IPv6 explicit IP address */
            (*explicit_ip_address).family = address_family;
            /* Convert the IPv4 or IPv6 string to the numeric IP address */
            ret = inet_pton(address_family, (const unsigned char *)listen_host_ptr,
                            (char *)&(*explicit_ip_address).v4v6);
            if (ret != 1) {
                /* Bad IP address format, signal error */
                status = JDBCSERVER_CONFIG_BAD_LISTEN_HOSTNAME_FORMAT;
            }
        } else { /* Not an IPv4 or IPv6 explicit IP address */
            /*
             * Assume a host name. Wait until server socket connect time to
             * find out if "host name" can be found. An indicator value (-1)
             * in the family field is used to indicate host name to IP address
             * lookup must be performed.
             */
            (*explicit_ip_address).family = EXPLICIT_IP_ADDRESS_HOST_IP_LOOKUP;
        }
    }
    return status;
}

/*
 * Function processConfigFile
 *
 * This function processes the initial JDBC Server
 * configuration as supplied in the JDBC$Config. file.
 *
 * Since this sets up the initial values in the Server
 * Global Data (sgd) structure, the actual sgd can be
 * used to hold the individual configuration parameters
 * as they are processed.  This is possible because the
 * JDBC Server is not actually running.  Validation of
 * the configuration values can use the same routines
 * as used by the administation tool and the Console
 * Handler.
 *
 * This routine will handle the sending of error or
 * warning messages to the STDOUT file.
 *
 * Parameters:
 *
 * configurationFileName  - File name of the configuration
 *                          file to process.
 *
 * Returned value:
 *
 * error_status  - If there are any problems with the
 *                 configuration, an appropriate error
 *                 status will be returned:
 *                 0  - all configuration parameters
 *                      were acceptable. This means that
 *                      the JDBC Server will begin
 *                      operation using the specified
 *                      configuration parameters.
 *
 *                 < 0  - one or more of the configuration
 *                        parameters was not acceptable and
 *                        a product default value was used in
 *                        its place. This warning means that
 *                        the JDBC Server will begin operation
 *                        with the adjusted configuration values.
 *                        The status value is the negative of the
 *                        warning message last generated, if more
 *                        than one warning was found.
 *
 *                 > 0  - one or more of the configuration
 *                        parameters was not acceptable and no
 *                        product default value can be used.
 *                        This error indicates the configuration
 *                        is not valid so the JDBC Server will
 *                        terminate its attempt to operate.
 *                        The status value is the error number of
 *                        the warning message last generated, if more
 *                        than one error was found.
 *
 */
int processConfigFile(char * configurationFileName){
    int error_status;
    int error_status1;
    int abs_error_status;
    int mode;
    int tempval;
    int i;
    char insert1[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
#ifndef XABUILD /* JDBC Server */
    char insert2[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    int comapi_mode_letter_present[MAX_COMAPI_MODES]; /* Used to check the comapi mode letters */
    char a_mode_letter[2];
    int  server_listens_on_count;
    int  bad_hostname_num;
#endif /* JDBC Server */

    char * temp_buffPtr;
    char temp_buff[IMAGE_MAX_CHARS];
    char temp_buff1[IMAGE_MAX_CHARS];
    char * paramPtr;
    char * valuePtr;
    char * trailingPtr;
    char *imagePtr;
    char levelVal[IMAGE_MAX_CHARS];
    char octalVal[IMAGE_MAX_CHARS];
    char image[IMAGE_MAX_CHARS];
    char trailingstuff[IMAGE_MAX_CHARS];
    char default_client_tracefile_qual[MAX_QUALIFIER_LEN+1];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* initialize the flags to indicate parameter was not specified */
    int gotKnownParam;
    int got_server_name=GOT_NO_CONFIG_VALUE;
    int got_max_activities=GOT_NO_CONFIG_VALUE;
    int got_max_queued_comapi=GOT_NO_CONFIG_VALUE;
    int got_app_group_name=GOT_NO_CONFIG_VALUE;
    int got_host_port=GOT_NO_CONFIG_VALUE;
    int got_server_receive_timeout=GOT_NO_CONFIG_VALUE;
    int got_server_send_timeout=GOT_NO_CONFIG_VALUE;
    int got_server_log_file=GOT_NO_CONFIG_VALUE;
    int got_server_trace_file=GOT_NO_CONFIG_VALUE;
    int got_client_receive_timeout=GOT_NO_CONFIG_VALUE;
    int got_fetch_block_size=GOT_NO_CONFIG_VALUE;
    int got_client_default_tracefile_qualifier=GOT_NO_CONFIG_VALUE;
    int got_client_tracefile_max_trks=GOT_NO_CONFIG_VALUE;
    int got_client_tracefile_max_cycles=GOT_NO_CONFIG_VALUE;
    int got_server_locale=GOT_NO_CONFIG_VALUE;
    int got_keyin_Id=GOT_NO_CONFIG_VALUE;
    int got_user_access_control=GOT_NO_CONFIG_VALUE;
    int got_comapi_modes=GOT_NO_CONFIG_VALUE;
    int got_log_console_output=GOT_NO_CONFIG_VALUE;
    int got_app_group_number=GOT_NO_CONFIG_VALUE;
    int got_uds_icr_bdi=GOT_NO_CONFIG_VALUE;
    int got_rsa_bdi=GOT_NO_CONFIG_VALUE;
    int got_server_priority=GOT_NO_CONFIG_VALUE;
    int got_rdms_threadname_prefix=GOT_NO_CONFIG_VALUE;
    int got_server_listens_on=GOT_NO_CONFIG_VALUE;
    int got_client_keep_alive=GOT_NO_CONFIG_VALUE;
#ifdef XABUILD /* JDBC XA Server */
    int got_xa_thread_reuse=GOT_NO_CONFIG_VALUE;
#endif /* JDBC XA Server */

    /* Open the Configuration file */
    configFile=fopen(configurationFileName,"r");
    if (configFile == NULL){
        /* Bad Configuration file, can't open it */
        getLocalizedMessage(JDBCSERVER_CONFIG_CANT_OPEN_CONFIG_FILE,
                configurationFileName,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
        return JDBCSERVER_CONFIG_CANT_OPEN_CONFIG_FILE;
    }

    /*
     * Display the input configuration file into STDOUT - later we'll
     * figure out where it really goes. If N-option is set, don't display lines.
     */
    if (!testOptionBit('N')){
        /* Display the configuration file image into STDOUT. */
        getMessageWithoutNumber(SERVER_CONFIG_MESSAGE_LINE_1, NULL, NULL,
            LOG_TO_SERVER_STDOUT);
                /* 5336 First a syntactic scan on the configuration file. */
        getMessageWithoutNumber(SERVER_CONFIG_MESSAGE_LINE_2, NULL, NULL,
            LOG_TO_SERVER_STDOUT);
                /* 5337 The file will be displayed, with syntactic error messages */
        getMessageWithoutNumber(SERVER_CONFIG_MESSAGE_LINE_3, NULL, NULL,
            LOG_TO_SERVER_STDOUT);
                /* 5338 (lines prefixed by \"**ERROR \") displayed within the listing: */
        printf("\n");
    }

    /* while processing config parameters, assume there will be no errors */
    error_status=0;

    /*
     * This loop goes over the images in the Configuration file
     * and does the following steps:
     * 1) Tests if the image is a blank line or comment. These
     *    images are ignored.
     * 2) Looks for a configuration parameter in the following
     *    format:   keyword=value
     *    Current assumptions are that the keyword begins the image,
     *    there are no apostrophes  and no embedded spaces in the
     *    value, there is no space around the equal sign, and that
     *    the value ends in a semicolon;
     * 3) Tests the keyword= part of the image for a match with the
     *    known set of configuration keywords.  If not found, a
     *    error code is returned. If found, then set the tentative
     *    configuration value in the sgd.
     *
     * At the end of the loop we have an sgd that has tentative values
     * that need to be semantically verified before the configuration
     * is deemed acceptable.
     *
     */

    /* Loop until we get a NULL pointer - indicates Configuration file EOF */
    while ( fgets(image,IMAGE_MAX_CHARS,configFile) != NULL){

     /*
      * Display the image into STDOUT, if N-option is not set.
      */
     if (!testOptionBit('N'))printf("%s",image);

    /*
     * Get a pointer to the image area. Use this pointer to traverse
     * the image.
     */
    imagePtr=&image[0];

    /* Strip off leading blanks; this modifies imagePtr. */
    moveToNextNonblank(&imagePtr);

    /* Was the line all blanks - is just the \n left? */
    if (strlen(imagePtr)== 1){
        continue; /* skip it */
    }
    /* Now look for a comment line. */
    if (strncmp(imagePtr,"//",2) == 0 ){
        continue; /* skip it */
    }

    /*
     * Assume this image is in parameter=value format.
     * Retrieve both parts, strip off leading/trailing blanks and set
     * the sgd value. If either part is not found return a config
     * format error.
     */
    paramPtr=strtok(imagePtr,"="); /* find the separating equal sign */
    if (paramPtr == NULL){
        /* improper parameter=value format, indicate error and continue while loop */
        printf("** ");
        getLocalizedMessage(JDBCSERVER_CONFIG_INVALID_PARAMETER_FORMAT,
                                 NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
        error_status=JDBCSERVER_CONFIG_INVALID_PARAMETER_FORMAT;
        continue;
        }
    /* trim off any trailing blanks on parameter*/
    trimTrailingBlanks(paramPtr);

    /* Now find the value string by looking for the ending semicolon */
    valuePtr=strtok(NULL,";");

    /* printf("Value of param: %s\n",paramPtr); */
    /* printf("Value of value: %s\n",valuePtr); */
    if (valuePtr == NULL){
         /* improper parameter=value format, indicate error and continue while loop */
        printf("** ");
        getLocalizedMessage(JDBCSERVER_CONFIG_INVALID_PARAMETER_FORMAT,
                                 NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
        error_status=JDBCSERVER_CONFIG_INVALID_PARAMETER_FORMAT;
        continue;
        }
    /* trim off any leading and trailing blanks on value */
    moveToNextNonblank(&valuePtr);
    trimTrailingBlanks(valuePtr);

    /*
     * Now look if there is trailing stuff after the keyword=value;
     * by looking for the end of line and testing if there is
     * a trailing comment, or just blanks. If so ignore it, else we
     * have trailing trash so output an error
     */
    trailingPtr=strtok(NULL,"\n"); /* Now find the end of the image */
    if (trailingPtr !=NULL){
        /* test for additional non-blank characters */
        if ((sscanf(trailingPtr,"%s\0",trailingstuff)== 1)){
            /* yes, test if is a comment */
             if (strncmp(trailingstuff,"//",2) == 0 ){
                /* a comment, ignore it */
                }
             else {
                   /* improper parameter=value format, indicate error and continue while loop */
                   printf("** ");
                   getLocalizedMessage(JDBCSERVER_CONFIG_INVALID_PARAMETER_FORMAT,
                                            NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
                   error_status=JDBCSERVER_CONFIG_INVALID_PARAMETER_FORMAT;
                   continue;
             }
        }
    }

    /*
     * First, put the keyword into lowercase for the comparisons.
     */
    toLowerCase(paramPtr);

 /* printf("paramPtr's value is %s; its length is %i;\n",paramPtr,strlen(paramPtr)); */
 /* printf("valuePtr's value is %s; its length is %i;\n",valuePtr,strlen(valuePtr)); */
    /*
     * Initial Syntactic Check
     *
     * Now determine which parameter is being supplied and do initial
     * syntactic check and if ok transfer it to sgd.
     *
     * Assume the parameter we have is not known (i.e. bad parameter name). As
     * we check, we will set gotKnownParam to BOOL_TRUE if it is found
     * will be one we know.
     */
     gotKnownParam=BOOL_FALSE;
    if (strcmp(paramPtr,"server_name") == 0 ){
        /*
         * Found the server_name keyword, check its length now
         * since there is no validation routine.
        */
        gotKnownParam=BOOL_TRUE;
        if (strlen(valuePtr) <= MAX_SERVERNAME_LEN){
           strcpy(sgd.serverName,valuePtr);
           got_server_name=GOT_CONFIG_VALUE;
        }
        else {
            /* name was too long, indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_SERVERNAME_LENGTH_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_SERVERNAME_LENGTH_ERROR;
            continue;
        }
    }
    else if (strcmp(paramPtr,"max_activities") == 0 ){
        /* Found the max_activities keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */
        if (sscanf(valuePtr,"%i%s\0",&sgd.maxServerWorkers,trailingstuff)== 1){
            got_max_activities=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_MAX_ACTIVITIES_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_MAX_ACTIVITIES_ERROR;
            continue;
            }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to 1
         * as a default value.
         */
        sgd.maxServerWorkers = 1;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"max_queued_comapi") == 0 ){
        /* Found the max_queued_comapi keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */
        if (sscanf(valuePtr,"%i%s\0",&sgd.maxCOMAPIQueueSize,trailingstuff)== 1){
            got_max_queued_comapi=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_MAX_QUEUED_COMAPI_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_MAX_QUEUED_COMAPI_ERROR;
            continue;
            }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to 0
         * as a default value.
         */
        sgd.maxCOMAPIQueueSize = 0;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"app_group_name") == 0 ){
        /* Found the app_group_name keyword */
        gotKnownParam=BOOL_TRUE;
        if (strlen(valuePtr) <= MAX_APPGROUP_NAMELEN){
           strcpy(sgd.appGroupName,valuePtr);
           got_app_group_name=GOT_CONFIG_VALUE;
        }
        else {
            /* name was too long - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_APPGROUPNAME_LENGTH_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_APPGROUPNAME_LENGTH_ERROR;
            continue;
        }
    }
    else if (strcmp(paramPtr,"host_port") == 0 ){
        /* Found the host_port keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */
        if (sscanf(valuePtr,"%i%s\0",&sgd.localHostPortNumber,trailingstuff)== 1){
             /*
              * Test host_port - this psuedo syntax test is minimal since the customer
              *                  can choose any legal port number for their
              *                  host system.
              */
             if (sgd.localHostPortNumber < 0){
                 /* Negative values are illegal  - indicate error and continue while loop */
                 printf("** ");
                 getLocalizedMessage(JDBCSERVER_CONFIG_HOSTPORTNUMBER_ERROR,
                                          NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
                 error_status=JDBCSERVER_CONFIG_HOSTPORTNUMBER_ERROR;
                 continue;
             }
             got_host_port=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_HOSTPORTNUMBER_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_HOSTPORTNUMBER_ERROR;
            continue;
            }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to 0
         * as a default value.
         */
        sgd.localHostPortNumber = 0;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"server_listens_on") == 0 ){
        /* Found the server_listens_on keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */

        /*
         * Get up to two host names, separated by a comma.  The sscanf will
         * place the host names in the sgd.listen_host_name[] array entries, with
         * the separating comma in a temp buffer which is not further needed. It
         * will also detect the presence of a third, or more, comma separated
         * hostnames, but will not retrieve them.
         */
        server_listens_on_count = sscanf(valuePtr,"%[^,^ ]%[ ,]%[^,^ ]%s",sgd.listen_host_names[0],
                                    temp_buff,sgd.listen_host_names[2],temp_buff1);

        /* Now determine next action based on the results of the sscanf parse. If the
         * number of host name's retrieved is good, further check the syntax and
         * store them in the SGD.
         * Note: semantic checking of a host name (i.e. DNS lookup) will be done later.
         *
         * Before switch, test if case 3 and if so check if separator was a single comma
         * following a first parameter value (a null string for first host name is not allowed).
         * If it was not treat it like case -1 (bad separator usage/syntax)
         */
        if (server_listens_on_count == 3) {
           /* trim off any leading and trailing blanks on
            * temp_buff and test for a single comma. */
           temp_buffPtr=&temp_buff[0];
           moveToNextNonblank(&temp_buffPtr);
           trimTrailingBlanks(temp_buffPtr);
           if ((strcmp(temp_buffPtr,",") !=0)){
              /* bad seperator syntax, signal error */
              server_listens_on_count = -1;
           } else if (strlen(sgd.listen_host_names[0]) == 0){
              /* separator is good, but first host is not there. */
              server_listens_on_count = -1;
           }
        }
        switch (server_listens_on_count){
            case 1:{
                /*
                 * Only one host name present. Check if the host name is an IP
                 * address in correct format.
                 */
               error_status = determine_listen_host_format(sgd.listen_host_names[0],&sgd.explicit_ip_address[0][0]);
               if (error_status == 0) {
                 /*
                  *  Format is OK so far. Now save the first host name in
                  * the second listen_host_name entry also (for CMS server
                  * socket). Clear the unused server socket names to an
                  * empty string (not used).  The listen ip sddresses
                  * will be set up later when the ICL activities are set up.
                  *
                  * Notice, the explicit_ip_address array has only its first
                  * row (i.e. sgd.explicit_ip_address[0]) initialized.  The
                  * other rows will be initialized when JDBCServer sets up
                  * the ICL information and starts the ICL's.
                  */
                 strcpy(sgd.listen_host_names[1], sgd.listen_host_names[0]);
                 sgd.listen_host_names[2][0]='\0';  /* use empty string */
                 sgd.listen_host_names[3][0]='\0';  /* use empty string */
                 sgd.explicit_ip_address[0][1] = sgd.explicit_ip_address[0][0];
                 sgd.explicit_ip_address[0][2].v4v6.v4 = 0;
                 sgd.explicit_ip_address[0][2].family = AF_UNSPEC;
                 sgd.explicit_ip_address[0][3].v4v6.v4 = 0;
                 sgd.explicit_ip_address[0][3].family = AF_UNSPEC;
                 sgd.num_server_sockets = 2; /* we will have two server sockets total; */
                 strcpy(sgd.config_host_names,valuePtr); /* save actual config parameter string */
                 got_server_listens_on=GOT_CONFIG_VALUE;
               }else {
                  /* error detected, display error message and insert, and continue config processing */
                  printf("** ");
                  getLocalizedMessage(error_status,
                            sgd.listen_host_names[0],NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
                  continue;
               }
               break;
            }
            case 3: {
               /*
                * Two host names present. Check if the first host name is an IP
                * address in correct format.
                */
               bad_hostname_num=0; /* in case there is an error status on first host name */
               error_status = determine_listen_host_format(sgd.listen_host_names[0],&sgd.explicit_ip_address[0][0]);
               if (error_status == 0){
                   /* First host name is OK so far, now check the second host name */
                   bad_hostname_num=2; /* in case there is an error status on second host name*/
                   error_status = determine_listen_host_format(sgd.listen_host_names[2],&sgd.explicit_ip_address[0][2]);
                   if (error_status == 0){
                        /*
                         * Both host names are acceptable so far. Save the first host name
                         * in the second listen_host_name entry for its CMS server socket,
                         * and do the same for the second host name for its CMS socket.
                         * The listen ip addresses for all the sockets will be set
                         * up later when the ICL activities are set up.
                         *
                         * Notice, the explicit_ip_address array has only its first
                         * row (i.e. sgd.explicit_ip_address[0]) initialized.  The
                         * other rows will be initialized when JDBCServer sets up
                         * the ICL information and starts the ICL's.
                         */
                        strcpy(sgd.listen_host_names[1], sgd.listen_host_names[0]);
                        strcpy(sgd.listen_host_names[3], sgd.listen_host_names[2]);
                        sgd.explicit_ip_address[0][1] = sgd.explicit_ip_address[0][0];
                        sgd.explicit_ip_address[0][3] = sgd.explicit_ip_address[0][2];
                        sgd.num_server_sockets = 4; /* we will have four server sockets total; */
                        strcpy(sgd.config_host_names,valuePtr); /* save actual config parameter string */
                        got_server_listens_on=GOT_CONFIG_VALUE;
                        break;
                   }
               }

               /* error was detected, display error message and insert, and continue config processing */
               printf("** ");
               getLocalizedMessage(error_status,
                         sgd.listen_host_names[bad_hostname_num],NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
               continue;
            }

            /*
             * The bad cases: -1  - indicates an empty string (no host names)
             *                 2 - indicates just a blank string (no host names)
             *                 4 - indicates unallowed additional hostnames (no more than 2 allowed )
             *                 default - means a bad sscanf value (no usable host names )
             */
            case -1:
            case  2:
            case  4:
            default: {
               /* Indicate configuration parameter error. */
             /* Did not find a possible legal value(s) - indicate error and continue while loop */
                printf("** ");
                getLocalizedMessage(JDBCSERVER_CONFIG_LISTENHOSTS_ERROR,
                                        NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
                error_status=JDBCSERVER_CONFIG_LISTENHOSTS_ERROR;
                continue;
           }
        }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set its sgd num_server_sockets to 0.
         */
        sgd.num_server_sockets = 0;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"rdms_threadname_prefix") == 0 ){
        /* Found the rdms_threadname_prefix keyword */
        gotKnownParam=BOOL_TRUE;

#ifndef XABUILD /* JDBC Server */

        /* Test if the prefix provided is one or two alphanumeric characters */
        i=1;  /* Assume prefix will be BAD */
        switch (strlen(valuePtr)){
            case 1: if (isalnum(valuePtr[0])){
                      i=0; /* prefix is OK, save the UCASE version in sgd. */
                      toUpperCase(valuePtr);
                      strcpy(sgd.rdms_threadname_prefix,valuePtr);
                      sgd.rdms_threadname_prefix_len = 1;
                      got_rdms_threadname_prefix=GOT_CONFIG_VALUE;
                      break;
                      }
            case 2: if (isalnum(valuePtr[0]) && isalnum(valuePtr[1])){
                      i=0; /* prefix is OK, save the UCASE version in sgd. */
                      toUpperCase(valuePtr);
                      strcpy(sgd.rdms_threadname_prefix,valuePtr);
                      sgd.rdms_threadname_prefix_len = 2;
                      got_rdms_threadname_prefix=GOT_CONFIG_VALUE;
                      break;
                      }
            }

        /* Test if we failed syntax check, if so issue error */
        if (i != 0){
          /* Indicate error and continue while loop */
          printf("** ");
          getLocalizedMessage(JDBCSERVER_CONFIG_RDMS_THREADNAME_PREFIX_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
          error_status=JDBCSERVER_CONFIG_RDMS_THREADNAME_PREFIX_ERROR;
          continue;
        }

#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to empty string
         * as a default value.
         */
        sgd.rdms_threadname_prefix[0] = '\0';  /* The XA Server uses the generated runid for the threadname */
        sgd.rdms_threadname_prefix_len = 0;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"client_keep_alive") == 0 ){
        /* Found the client_keep_alive keyword */
        gotKnownParam=BOOL_TRUE;

#ifndef XABUILD /* JDBC Server */
        /*
         * Convert the value string to uppercase before testing.
         */
        toUpperCase(valuePtr);

        /*
         * Now test the client keep alive value.
         */
        /* Now set the appropriate value into its sgd variable. */
        if ((strcmp(valuePtr,CONFIG_CLIENT_KEEPALIVE_ALWAYS_OFF) == 0)){
            /* Set sgd variable to ALWAYS OFF and set flag that we got a valid value */
            sgd.config_client_keepAlive = CLIENT_KEEPALIVE_ALWAYS_OFF;
            got_client_keep_alive=GOT_CONFIG_VALUE;

        } else if ((strcmp(valuePtr,CONFIG_CLIENT_KEEPALIVE_ALWAYS_ON) == 0)){
            /* Set sgd variable to ALWAYS ON and set flag that we got a valid value */
            sgd.config_client_keepAlive = CLIENT_KEEPALIVE_ALWAYS_ON;
            got_client_keep_alive=GOT_CONFIG_VALUE;

        } else if ((strcmp(valuePtr,CONFIG_CLIENT_KEEPALIVE_OFF) == 0)){
            /* Set sgd variable to OFF and set flag that we got a valid value */
            sgd.config_client_keepAlive = CLIENT_KEEPALIVE_OFF;
            got_client_keep_alive=GOT_CONFIG_VALUE;

        } else if ((strcmp(valuePtr,CONFIG_CLIENT_KEEPALIVE_ON) == 0)){
            /* Set sgd variable to ON and set flag that we got a valid value */
            sgd.config_client_keepAlive = CLIENT_KEEPALIVE_ON;
            got_client_keep_alive=GOT_CONFIG_VALUE;

        } else {
            /* Invalid client keep alive value detected, indicate error and continue loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_CLIENT_KEEP_ALIVE_ERROR,
                  valuePtr,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_CLIENT_KEEP_ALIVE_ERROR;
            continue;
        }

#else  /* XA JDBC Server */
        /*
         * Parameter client_keep_alive is not used by XA JDBC Server, so it
         * always tells client that keep alive is turned off (uses RMI-IIOP).
         */
        sgd.config_client_keepAlive = CLIENT_KEEPALIVE_ALWAYS_OFF;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"server_receive_timeout") == 0 ){
        /* Found the server_receive_timeout keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */
        if (sscanf(valuePtr,"%i%s\0",
                   &sgd.server_receive_timeout,trailingstuff)== 1){
            got_server_receive_timeout=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_SERVER_RECEIVE_TIMEOUT_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_SERVER_RECEIVE_TIMEOUT_ERROR;
            continue;
            }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to 0
         * as a default value.
         */
        sgd.server_receive_timeout = 0;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"server_send_timeout") == 0 ){
        /* Found the server_send_timeout keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */
        if (sscanf(valuePtr,"%i%s\0",
                   &sgd.server_send_timeout,trailingstuff)== 1){
            got_server_send_timeout=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_SERVER_SEND_TIMEOUT_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_SERVER_SEND_TIMEOUT_ERROR;
            continue;
            }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to 0
         * as a default value.
         */
        sgd.server_send_timeout = 0;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"server_activity_receive_timeout") == 0 ){
        /* Found the server_activity_receive_timeout keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */
        if (sscanf(valuePtr,"%i%s\0",
                   &sgd.server_activity_receive_timeout,trailingstuff)== 1){
            got_client_receive_timeout=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_CLIENT_RECEIVE_TIMEOUT_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_CLIENT_RECEIVE_TIMEOUT_ERROR;
            continue;
            }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to 0
         * as a default value.
         */
        sgd.server_activity_receive_timeout = 0;
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"server_log_file") == 0 ){
        /* Found the server_log_file keyword */
        gotKnownParam=BOOL_TRUE;

        /* Indicate we are not using the configuration parameter provided but the configuration default name */
        strcpy(insert1,CONFIG_DEFAULT_SERVER_LOGFILENAME);
        strcat(insert1," used for Server log file");
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_LOGFILENAME_TRACEFILENAME_NOT_USED,
                                "server_log_file", insert1, LOG_TO_SERVER_STDOUT);
        /*
         * Use the Configuration default file name "JDBC$LOG" - it is the
         * standard use name for the Server Log file.
         */
        strcpy(sgd.serverLogFileName,CONFIG_DEFAULT_SERVER_LOGFILENAME);
    }
    else if (strcmp(paramPtr,"server_trace_file") == 0 ){
        /* Found the server_trace_file keyword */
        gotKnownParam=BOOL_TRUE;

        /* Indicate we are not using the configuration parameter provided but the configuration default name */
        strcpy(insert1,CONFIG_DEFAULT_SERVER_TRACEFILENAME);
        strcat(insert1," used for Server trace file");
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_LOGFILENAME_TRACEFILENAME_NOT_USED,
                                "server_trace_file", insert1, LOG_TO_SERVER_STDOUT);
        /*
         * Use the Configuration default file name "JDBC$TRACE" - it is the
         * standard use name for the Server Trace file.
         */
        strcpy(sgd.serverTraceFileName,CONFIG_DEFAULT_SERVER_TRACEFILENAME);
    }
    else if (strcmp(paramPtr,"client_default_tracefile_qualifier") == 0 ){
        /* Found the client_default_tracefile_qualifier keyword */
        gotKnownParam=BOOL_TRUE;
        if (strlen(valuePtr) <= MAX_QUALIFIER_LEN){
           strcpy(sgd.defaultClientTraceFileQualifier,valuePtr);
           got_client_default_tracefile_qualifier=GOT_CONFIG_VALUE;
        }
        else {
            /* name was too long - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_TRACEFILEQUAL_LENGTH_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_TRACEFILEQUAL_LENGTH_ERROR;
            continue;
        }
    }
    else if (strcmp(paramPtr,"client_tracefile_max_trks") == 0 ){
        /* Found the client_tracefile_max_trks keyword */
        gotKnownParam=BOOL_TRUE;
        if (sscanf(valuePtr,"%i%s\0",
                   &sgd.clientTraceFileTracks,trailingstuff)== 1){
            /* Adjust value to within the limits */
            if (sgd.clientTraceFileTracks < CLIENT_TRACEFILE_TRACKS_MIN) {
                sgd.clientTraceFileTracks = CLIENT_TRACEFILE_TRACKS_MIN;
            }
            if (sgd.clientTraceFileTracks > CLIENT_TRACEFILE_TRACKS_MAX) {
                sgd.clientTraceFileTracks = CLIENT_TRACEFILE_TRACKS_MAX;
            }
            got_client_tracefile_max_trks=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_TRACEFILE_TRACKS_ERROR,
                                      valuePtr, NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_TRACEFILE_TRACKS_ERROR;
            continue;
        }
    }
    else if (strcmp(paramPtr,"client_tracefile_max_cycles") == 0 ){
        /* Found the client_tracefile_max_cycles keyword */
        gotKnownParam=BOOL_TRUE;
        if (sscanf(valuePtr,"%i%s\0",
                   &sgd.clientTraceFileCycles,trailingstuff)== 1){
            /* Adjust value to within the limits */
            if (sgd.clientTraceFileCycles < CLIENT_TRACEFILE_CYCLES_MIN) {
                sgd.clientTraceFileCycles = CLIENT_TRACEFILE_CYCLES_MIN;
            }
            if (sgd.clientTraceFileCycles > CLIENT_TRACEFILE_CYCLES_MAX) {
                sgd.clientTraceFileCycles = CLIENT_TRACEFILE_CYCLES_MAX;
            }
            got_client_tracefile_max_cycles=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_TRACEFILE_CYCLES_ERROR,
                                      valuePtr, NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_TRACEFILE_CYCLES_ERROR;
            continue;
        }
    }
    else if (strcmp(paramPtr,"server_locale") == 0 ){
        /* Found the server_locale keyword */
        gotKnownParam=BOOL_TRUE;
        if (strlen(valuePtr) <= MAX_LOCALE_LEN){
           strcpy(sgd.serverLocale,valuePtr);
           got_server_locale=GOT_CONFIG_VALUE;
        }
        else {
            /* name was too long - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_SERVER_LOCALE_LENGTH_ERROR,
                                     NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_SERVER_LOCALE_LENGTH_ERROR;
            continue;
        }
    }
    else if (strcmp(paramPtr,"keyin_id") == 0 ){
        /* Found the keyin_id keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */

        /*
         * Convert the value string to uppercase before testing.
         */
        toUpperCase(valuePtr);

        /*
         * Now test the value string - It must pass the
         * tests of the isValidKeyinId function. Use the
         * returned value to determine correwctness.
         */
        if (isValidKeyinId(valuePtr)){
           /* got a legal keyin_id, store it in sgd */
           strcpy(sgd.configKeyinID,valuePtr);
           got_keyin_Id=GOT_CONFIG_VALUE;
        }
        else {
              /* keyin_id was not valid - indicate error and continue while loop */
              printf("** ");
              getLocalizedMessage(JDBCSERVER_VALIDATE_KEYIN_ID_ERROR,
                                       valuePtr,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
              error_status=JDBCSERVER_VALIDATE_KEYIN_ID_ERROR;
              continue;
        }
#else  /* XA JDBC Server */
        /* Always use the generated runid in an XA JDBC Server */
        strcpy(sgd.configKeyinID,sgd.generatedRunID);
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"user_access_control") == 0 ){
        /* Found the user_access_control keyword */
        gotKnownParam=BOOL_TRUE;

        /*
         * Convert the value string to uppercase before testing.
         */
        toUpperCase(valuePtr);

        /*
         * Now test the value string - It must be either "OFF", "JDBC",
         * "JDBC_SECOPT1", "FUND", or "JDBC_FUNDAMENTAL".
         */
        if ((strcmp(valuePtr,CONFIG_JDBC_USER_ACCESS_CONTROL_OFF) == 0)){
            /* We got a valid value, set flag and save value. */
            got_user_access_control=GOT_CONFIG_VALUE;
            sgd.user_access_control= USER_ACCESS_CONTROL_OFF;
        } else if ((strcmp(valuePtr,CONFIG_JDBC_USER_ACCESS_CONTROL_JDBC) == 0)||
                   (strcmp(valuePtr,CONFIG_JDBC_USER_ACCESS_CONTROL_JDBC_SECOPT1) == 0)){
            /* We got a valid value: JDBC SECOPT1 user access security, set flag and save value. */
            got_user_access_control=GOT_CONFIG_VALUE;
            sgd.user_access_control= USER_ACCESS_CONTROL_JDBC;
        } else if ((strcmp(valuePtr,CONFIG_JDBC_USER_ACCESS_CONTROL_FUND) == 0)||
                   (strcmp(valuePtr,CONFIG_JDBC_USER_ACCESS_CONTROL_FUNDAMENTAL) == 0)){
            /* We got a valid value: JDBC Fundamental user access security, set flag and save value. */
            got_user_access_control=GOT_CONFIG_VALUE;
            sgd.user_access_control= USER_ACCESS_CONTROL_FUND;
        } else {
            /* Invalid user access control value detected, indicate error and continue loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_VALIDATE_USER_ACCESS_CONTROL_ERROR,
                  valuePtr,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_VALIDATE_USER_ACCESS_CONTROL_ERROR;
            continue;
        }
    }
    else if (strcmp(paramPtr,"server_priority") == 0 ){
        /* Found the server_priority keyword */
        gotKnownParam=BOOL_TRUE;

        /*
         * Convert the value string to uppercase before testing.
         */
        toUpperCase(valuePtr);

        /*
         * Now test the value string and if legal, set the appropriate value
         * into its sgd variable. If the value is not legal, an error message
         * will be produced and the default (TXN) will be used.
         */
        if ((strcmp(valuePtr,CONFIG_JDBC_SERVER_PRIORITY_TXN) == 0)){
           /* Use transaction switching priority */
           strcpy(sgd.server_priority,CONFIG_JDBC_SERVER_PRIORITY_TXN);
           sgd.er_level_value = CONFIG_JDBC_ER_LEVEL_ARG_TXN;
           got_server_priority=GOT_CONFIG_VALUE; /* Set flag - we got a valid value */
        }
        else if ((strcmp(valuePtr,CONFIG_JDBC_SERVER_PRIORITY_USER) == 0)){
                /* Use user switching priority */
                strcpy(sgd.server_priority,CONFIG_JDBC_SERVER_PRIORITY_USER);
                sgd.er_level_value = CONFIG_JDBC_ER_LEVEL_ARG_USER;
                got_server_priority=GOT_CONFIG_VALUE; /* Set flag - we got a valid value */
             }
        else if ((strcmp(valuePtr,CONFIG_JDBC_SERVER_PRIORITY_BATCH) == 0)){
                /* Use batch switching priority */
                strcpy(sgd.server_priority,CONFIG_JDBC_SERVER_PRIORITY_BATCH);
                sgd.er_level_value = CONFIG_JDBC_ER_LEVEL_ARG_BATCH;
                got_server_priority=GOT_CONFIG_VALUE; /* Set flag - we got a valid value */
             }
        else if ((strcmp(valuePtr,CONFIG_JDBC_SERVER_PRIORITY_DEMAND) == 0)){
                /* Use demand switching priority */
                strcpy(sgd.server_priority,CONFIG_JDBC_SERVER_PRIORITY_DEMAND);
                sgd.er_level_value = CONFIG_JDBC_ER_LEVEL_ARG_DEMAND;
                got_server_priority=GOT_CONFIG_VALUE; /* Set flag - we got a valid value */
             }
        else if ((strncmp(valuePtr, CONFIG_JDBC_SERVER_PRIORITY_LEVEL$,
                          (sizeof(CONFIG_JDBC_SERVER_PRIORITY_LEVEL$)-1)) == 0)){
          /*
           * We got a LEVEL$ specification, so test for the proper
           * "LEVEL$,nnnnnnnnnnnn"  specification.
           *
           * Get up to two values, separated by a comma. Hopefully this will be
           * the key value "LEVEL$" and an octal value to set. The sscanf will
           * place the "LEVEL$" and the octal value in the tempVal1 and tempVal2
           * arrays, with the separating comma in a temp buffer which is not further
           * needed. It will also detect the presence of a third, or more, comma
           * separated values, but will not retrieve them.
           */
          tempval = sscanf(valuePtr,"%[^,^ ]%[ ,]%[^,^ ]%s",levelVal,
                                      temp_buff,octalVal,temp_buff1);
          /* Now determine next action based on the results of the sscanf parse. If the
           * number of parts retrieved is good, further check the syntax and store the
           * desired server's starting LEVEL$ switching priority in the SGD.
           *
           * Test if tempval was 3, if so check if separator following the first
           * parameter value was a single comma (a null string for the key value
           * is not allowed). If it was not set tempval to -1 to indicate bad separator
           * usage/syntax. All other tempval values indicate bad parameter specification syntax.
           */
          if (tempval == 3) {
             /* trim off any leading and trailing blanks on
              * temp_buff and test for a single comma. */
             temp_buffPtr=&temp_buff[0];
             moveToNextNonblank(&temp_buffPtr);
             trimTrailingBlanks(temp_buffPtr);
             if ((strcmp(temp_buffPtr,",") !=0) || strlen(temp_buffPtr)!=1){
                /* bad separator syntax, signal error */
                tempval = -1;
             } else if ((strlen(levelVal) != sizeof(CONFIG_JDBC_SERVER_PRIORITY_LEVEL$)-1)){
                /* separator is good, but there is stuff after "LEVEL$" (e.g. "LEVEL$$".) */
                tempval = -1;
             } else{
                 /*
                  * Two values are present. We know the first one is "LEVEL$" and the
                  * separator was just a comma, so check that the value is a number in
                  * octal format.
                  */
                 tempval=sscanf(octalVal,"%o%s\0",&sgd.er_level_value,trailingstuff);
                 if (tempval !=1){
                    tempval = -1; /* Not an octal number, indicate an error */
                 }
                 else {
                    /*
                     * Found a valid LEVEL$=nnnnnnnnnnnn parameter value. We have the
                     * priority value to use in the SGD, so indicate we have a config
                     * value and continue configuration processing.
                     */
                    got_server_priority=GOT_CONFIG_VALUE; /* Set flag - we got a valid value */
                    continue;
                 }
             }
          }
          /* Was bad parameter syntax detected? If so, issue Invalid switching priority value detected. */
          if (tempval !=3){
              /*
               * The tempval value indicates that sscanf failed - Indicate configuration parameter error.
               * The bad tempval cases:
               *                -1 -  indicates an empty string (no host names), or bad syntax was detected
               *                      on the above check above (when tempval == 3 was detected and checked)
               *                 1 -  indicates just a key value is present (we didn't even need to
               *                       check if it was "LEVEL$" since we know it is.)
               *                 2 -  indicates just a blank string (no host names)
               *                 4 -  indicates unallowed additional values (no more than 2 allowed )
               *        (all other values) - means a bad sscanf value (no usable LEVEL$ parameter )
               */
              printf("** ");
              getLocalizedMessage(JDBCSERVER_VALIDATE_SERVER_PRIORITY_ERROR,
                    valuePtr, NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
              error_status=JDBCSERVER_VALIDATE_SERVER_PRIORITY_ERROR;
              continue;
          }
        }
    }
    else if (strcmp(paramPtr,"comapi_modes") == 0 ){
        /* Found the comapi_modes keyword */
        gotKnownParam=BOOL_TRUE;
#ifndef XABUILD /* JDBC Server */
        /*
         * Convert the value string to uppercase before testing.
         */
        toUpperCase(valuePtr);

        /* Test if its an alphabetic string, and check for duplicate mode letters. */
        memset(&comapi_mode_letter_present, 0, MAX_COMAPI_MODES*4);  /* first clear letter_present to 0's */
        for (i= 0; i< strlen(valuePtr); i++){
            if (isalpha(valuePtr[i])){
                /*
                 * A letter is found. Check if a duplicate,
                 * if not add it to comapi_modes value. If a
                 * duplicate letter, then issue a warning and just skip it.
                 */
                if (comapi_mode_letter_present[valuePtr[i] - 'A'] == 0){
                    /* A new mode letter. Add it to comapi_modes */
                    strncat(sgd.comapi_modes,valuePtr+i,1);
                   comapi_mode_letter_present[valuePtr[i] - 'A'] = 1; /* indicate the letter was found */
                }
                else {
                    /* Issue warning that duplicate mode letter is ignored */
                    strncat(a_mode_letter,valuePtr+i,1);
                    a_mode_letter[1]='\0';
                    printf("** ");
                    getLocalizedMessage(JDBCSERVER_VALIDATE_COMAPI_MODES_DUP_MODE_VALUE_IGNORED,
                          a_mode_letter,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
                }
            }
            else {
                /* Did not find an alphabetic mode character - indicate error and continue while loop */
                printf("** ");
                getLocalizedMessage(JDBCSERVER_VALIDATE_COMAPI_MODES_ERROR,
                      valuePtr,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
                error_status=JDBCSERVER_VALIDATE_COMAPI_MODES_ERROR;
                break;  /* stop the for loop */
            }
        }

        if (error_status != JDBCSERVER_VALIDATE_COMAPI_MODES_ERROR){
            /* We did not error the comapi_modes specification, so indicate we got it */
            got_comapi_modes=GOT_CONFIG_VALUE;   /* indicate we got a comapi_modes config parameter */
        }
#else  /* XA JDBC Server */
        /*
         * Parameter is not used by XA JDBC Server, skip its
         * processing and set it in the sgd to '\0'
         * as a default value.
         */
        sgd.comapi_modes[0] = '\0';
#endif /* JDBC and XA JDBC Servers */
    }
    else if (strcmp(paramPtr,"app_group_number") == 0 ){
        /* Found the app_group_number keyword */
        gotKnownParam=BOOL_TRUE;
        if (sscanf(valuePtr,"%i%s\0",
                   &sgd.appGroupNumber,trailingstuff)== 1){
            got_app_group_number=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_APPGROUPNUMBER_ERROR,
                                     valuePtr,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_APPGROUPNUMBER_ERROR;
            continue;
            }
    }
    else if (strcmp(paramPtr,"xa_thread_reuse") == 0 ){
        /* Found the xa_thread_reuse */

#ifndef XABUILD /* JDBC Server */
        /*
         * Parameter xa_thread_reuse is a "hidden" JDBC XA Server
         * parameter only and is not used by JDBC Server - in a
         * JDBC Server there is no SGD value for it. So acknowledge
         * that we found it but skip its processing entirely.
         */
        gotKnownParam=BOOL_TRUE;
    }
#else  /* JDBC XA Server */
    /*
     * Parameter is used by XA JDBC Server, if specified,
     * so process its value.
     */
    gotKnownParam=BOOL_TRUE;
    i = sscanf(valuePtr,"%i%s\0", &sgd.XA_thread_reuse_limit,trailingstuff);
    /* Was a legal positive integer value ( > 0) specified? */
    if (i == 1 && sgd.XA_thread_reuse_limit > 0){
        /* Yes, the value is legal. */
        got_xa_thread_reuse=GOT_CONFIG_VALUE;
    }
    else {
        /* Did not find a positive integer value - indicate error and continue while loop */
        printf("** ");
        getLocalizedMessage(JDBCSERVER_CONFIG_XA_THREAD_REUSE_LIMIT_ERROR,
                                 valuePtr, NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
        error_status=JDBCSERVER_CONFIG_XA_THREAD_REUSE_LIMIT_ERROR;
        continue;
        }
    }
#endif /* JDBC and XA JDBC Servers */
    else if (strcmp(paramPtr,"uds_icr_bdi") == 0 ){
        /* Found the uds_icr_bdi keyword */
        gotKnownParam=BOOL_TRUE;

#ifndef XABUILD /* JDBC Server */
        /*
         * Parameter is not used by JDBC Server, skip its
         * processing and put 0 in the sgd for it.
         */
        sgd.uds_icr_bdi = 0;
    }
#else
        /*
         * Parameter is used by XA JDBC Server, process its value.
         */
        if (sscanf(valuePtr,"%o%s\0",
                   &sgd.uds_icr_bdi,trailingstuff)== 1){
            got_uds_icr_bdi=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_UDS_ICR_BDI_ERROR,
                                     valuePtr, NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_UDS_ICR_BDI_ERROR;
            continue;
            }
    }
#endif /* JDBC and XA JDBC Servers */

    else if (strcmp(paramPtr,"rsa_bdi") == 0 ){
        /* Found the rsa_bdi keyword */
        gotKnownParam=BOOL_TRUE;
        if (sscanf(valuePtr,"%o%s\0",
                   &sgd.rsaBdi,trailingstuff)== 1){
            got_rsa_bdi=GOT_CONFIG_VALUE;
        }
        else {
            /* Did not find a integer value - indicate error and continue while loop */
            printf("** ");
            getLocalizedMessage(JDBCSERVER_CONFIG_RSA_BDI_ERROR,
                                      valuePtr, NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status=JDBCSERVER_CONFIG_RSA_BDI_ERROR;
            continue;
            }
    }
    else if (strcmp(paramPtr, "log_console_output") == 0 ){
        /* Found the log_console_output keyword */
        gotKnownParam = BOOL_TRUE;

        /*
         * Convert the value string to uppercase before testing.
         */
        toUpperCase(valuePtr);

        /*
         * Now test the value string - It must be either "OFF" or "ON".
         */
        if ((strcmp(valuePtr, CONFIG_JDBC_LOG_CONSOLE_OUTPUT_OFF) == 0) ||
            (strcmp(valuePtr, CONFIG_JDBC_LOG_CONSOLE_OUTPUT_ON) == 0) ){
           /* Set flag, we got a valid value */
           got_log_console_output = GOT_CONFIG_VALUE;

           /* Now set the appropriate value into its sgd variable. */
           if ((strcmp(valuePtr, CONFIG_JDBC_LOG_CONSOLE_OUTPUT_OFF) == 0)){
              sgd.logConsoleOutput = LOG_CONSOLE_OUTPUT_OFF;
           }
           if ((strcmp(valuePtr, CONFIG_JDBC_LOG_CONSOLE_OUTPUT_ON) == 0)){
              sgd.logConsoleOutput = LOG_CONSOLE_OUTPUT_ON;
           }
        }
        else {
            /* Invalid log console output value detected, indicate warning
               and continue loop */
            sgd.logConsoleOutput = CONFIG_DEFAULT_LOG_CONSOLE_OUTPUT_ON;
            printf("** ");
            getLocalizedMessage(
                abs(JDBCSERVER_VALIDATE_LOG_CONSOLE_OUTPUT_WARNING), valuePtr,
                CONFIG_DEFAULT_LOG_CONSOLE_OUTPUT_ON_STRING,
                LOG_TO_SERVER_STDOUT, msgBuffer);
            error_status = JDBCSERVER_VALIDATE_LOG_CONSOLE_OUTPUT_WARNING;
            continue;
        }
    }
    else if ((strcmp(paramPtr,"admin_user_id") == 0 ) ||
             (strcmp(paramPtr,"host_name") == 0 )     ||
             (strcmp(paramPtr,"master_user_id") == 0 )||
             (strcmp(paramPtr,"comapi_server_socket_tries") == 0 )||
             (strcmp(paramPtr,"comapi_server_socket_retry_wait") == 0 )){
        /*
         * Found an obsolete configuration parameter
         * that is no longer used. Signal the user
         * with a warning message and ignore the configuration
         * parameter and continue with config parameter processing.
         */
        printf("** ");
        getLocalizedMessage(JDBCSERVER_OBSOLETE_CONFIG_PARAMETER,
                                 paramPtr,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
        continue;
    }
    /* Test if we found a known configuration parameter. If not, error. */
    if (gotKnownParam ==BOOL_FALSE){
       printf("** ");
       getLocalizedMessage(JDBCSERVER_UNKNOWN_CONFIG_PARAMETER,
                                paramPtr,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
       error_status=JDBCSERVER_UNKNOWN_CONFIG_PARAMETER;
    }

   } /* end of while */

   /*
    * If there was one or more improper parameter specifications,
    * terminate configuration processing.
    */
   if (error_status !=0){
       return error_status;     /* stop processing due to error */

   }

   /*
    * Missing Parameter Detection
    *
    * Place some space after the JDBC$Config. echo. Then indicate that we
    * are now doing a check for missing configuration parameters.
    */
   if (!testOptionBit('N')){
       /* Display into STDOUT status on configuration processing. */
       printf("\n\nNow analyzing the configuration looking for missing parameters, ");
       printf("defaults will be applied if necessary.\n\n");
   }

   /*
    * Now make sure all the configuration parameters have a value.
    * If not, use the product default value for that parameter if
    * available. If not, signal error and return.
    */
    if (got_server_name == GOT_NO_CONFIG_VALUE ){
        /* Use product default server name */
        strcpy(sgd.serverName,CONFIG_DEFAULT_SERVERNAME);
        got_server_name=HAVE_CONFIG_DEFAULT;
        printf("** ");
        sprintf(temp_buff,"\"%s\"", sgd.serverName);
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "server_name", temp_buff, LOG_TO_SERVER_STDOUT);
        }

#ifndef XABUILD  /* JDBC Server */
    if (got_max_activities == GOT_NO_CONFIG_VALUE ){
        /* Use product default for Max. Server Workers */
        sgd.maxServerWorkers=CONFIG_DEFAULT_MAXSERVERWORKERS;
        got_max_activities=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "max_activities", itoa(sgd.maxServerWorkers, insert1, 10), LOG_TO_SERVER_STDOUT);
        }

    if (got_max_queued_comapi == GOT_NO_CONFIG_VALUE ){
        /* Use product default for max. queued on COMAPI */
        sgd.maxCOMAPIQueueSize=CONFIG_DEFAULT_COMAPI_QUEUESIZE;
        got_max_queued_comapi=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "max_queued_comapi", itoa(sgd.maxCOMAPIQueueSize, insert1, 10), LOG_TO_SERVER_STDOUT);
        }
#endif          /* JDBC Server */

    if (got_app_group_name == GOT_NO_CONFIG_VALUE ){
        /* Use product default for application group */
        strcpy(sgd.appGroupName,CONFIG_DEFAULT_APPGROUPNAME);
        got_app_group_name=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "app_group_name", sgd.appGroupName, LOG_TO_SERVER_STDOUT);
        }

#ifndef XABUILD  /* JDBC Server */
    if (got_host_port == GOT_NO_CONFIG_VALUE ){
        /* None specified, and there is no product default */
        getLocalizedMessage(JDBCSERVER_CONFIG_MISSING_HOSTPORT_ERROR,NULL,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
        error_status=JDBCSERVER_CONFIG_MISSING_HOSTPORT_ERROR;
        }

    if (got_server_listens_on == GOT_NO_CONFIG_VALUE ){
        /*
         * Use product default listen IP address (0) for two server sockets
         * to CPCOMM and CMS access. This will cause the JDBC Server to
         * listen on the network defined default IP addresses for this host.
         *
         * The listen ip addresses for all the sockets will be
         * set up later when the ICL activities are set up.
         */
        strcpy(sgd.listen_host_names[0],DEFAULT_LISTEN_IP_ADDRESS_STR);
        strcpy(sgd.listen_host_names[1],DEFAULT_LISTEN_IP_ADDRESS_STR);
        sgd.listen_host_names[2][0]='\0';
        sgd.listen_host_names[3][0]='\0';
        sgd.explicit_ip_address[0][0].v4v6.v4 = DEFAULT_LISTEN_IP_ADDRESS;
        sgd.explicit_ip_address[0][0].family = AF_UNSPEC;
        sgd.explicit_ip_address[0][1].v4v6.v4 = DEFAULT_LISTEN_IP_ADDRESS;
        sgd.explicit_ip_address[0][1].family = AF_UNSPEC;
        sgd.num_server_sockets = 2;
        strcpy(sgd.config_host_names,DEFAULT_LISTEN_IP_ADDRESS_STR); /* save as config parameter string */
        got_server_listens_on=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "server_listens_on", sgd.config_host_names, LOG_TO_SERVER_STDOUT);
        }

    if (got_rdms_threadname_prefix == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        strcpy(sgd.rdms_threadname_prefix,CONFIG_DEFAULT_RDMS_THREADNAME_PREFIX);
        sgd.rdms_threadname_prefix_len = CONFIG_DEFAULT_RDMS_THREADNAME_PREFIX_LEN;
        got_rdms_threadname_prefix=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "rdms_threadname_prefix", sgd.rdms_threadname_prefix, LOG_TO_SERVER_STDOUT);
        }

    if (got_client_keep_alive == GOT_NO_CONFIG_VALUE ){
        /* Use product default for JDBC Server: client keep alive always on*/
        sgd.config_client_keepAlive = CLIENT_KEEPALIVE_ALWAYS_ON;
        got_client_keep_alive=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "client_keep_alive", CONFIG_CLIENT_KEEPALIVE_ALWAYS_ON, LOG_TO_SERVER_STDOUT);
        }

    if (got_server_receive_timeout == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        sgd.server_receive_timeout=CONFIG_DEFAULT_SERVER_RECEIVE_TIMEOUT;
        got_server_receive_timeout=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "server_receive_timeout", itoa(sgd.server_receive_timeout, insert1, 10), LOG_TO_SERVER_STDOUT);
        }

    if (got_server_send_timeout == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        sgd.server_send_timeout=CONFIG_DEFAULT_SERVER_SEND_TIMEOUT;
        got_server_send_timeout=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "server_send_timeout", itoa(sgd.server_send_timeout, insert1, 10), LOG_TO_SERVER_STDOUT);
        }
    if (got_client_receive_timeout == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        sgd.server_activity_receive_timeout=CONFIG_DEFAULT_CLIENT_RECEIVE_TIMEOUT;
        got_client_receive_timeout=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "client_receive_timeout", itoa(sgd.server_activity_receive_timeout, insert1, 10), LOG_TO_SERVER_STDOUT);
        }
#endif          /* JDBC Server */

    if (got_server_log_file == GOT_NO_CONFIG_VALUE ){
        /* Use product default, do not display the missing parameter default used message */
        strcpy(sgd.serverLogFileName,CONFIG_DEFAULT_SERVER_LOGFILENAME);
        got_server_log_file=HAVE_CONFIG_DEFAULT;
        }

    if (got_server_trace_file == GOT_NO_CONFIG_VALUE ){
        /* Use product default, do not display the missing parameter default used message */
        strcpy(sgd.serverTraceFileName,CONFIG_DEFAULT_SERVER_TRACEFILENAME);
        got_server_trace_file=HAVE_CONFIG_DEFAULT;
        }

    if (got_client_default_tracefile_qualifier == GOT_NO_CONFIG_VALUE ){
        /* Use product default qual prefix and the current provided app group name */
        strcpy(sgd.defaultClientTraceFileQualifier,CONFIG_DEFAULT_CLIENT_DEFAULT_TRACEFILE_QUAL_P1);
        strcat(sgd.defaultClientTraceFileQualifier,sgd.appGroupName);
        got_client_default_tracefile_qualifier=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "client_default_tracefile_qualifier", sgd.defaultClientTraceFileQualifier, LOG_TO_SERVER_STDOUT);
        }

    if (got_client_tracefile_max_trks == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        sgd.clientTraceFileTracks=CLIENT_TRACEFILE_TRACKS_DEFAULT;
        got_client_tracefile_max_trks=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "client_tracefile_max_trks", itoa(sgd.clientTraceFileTracks, insert1, 10), LOG_TO_SERVER_STDOUT);
        }

    /* The client_tracefile_max_cycles parameter was never released, so this check should not be make. */
    /* if (got_client_tracefile_max_cycles == GOT_NO_CONFIG_VALUE ){ */
        /* Use product default */
        /* sgd.clientTraceFileCycles=CLIENT_TRACEFILE_CYCLES_DEFAULT; */
        /* got_client_tracefile_max_cycles=HAVE_CONFIG_DEFAULT; */
        /* printf("** "); */
        /* getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED, */
        /*                        "client_tracefile_max_cycles", itoa(sgd.clientTraceFileCycles, insert1, 10), LOG_TO_SERVER_STDOUT); */
        /* } */

    if (got_server_locale == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        strcpy(sgd.serverLocale,CONFIG_DEFAULT_SERVER_LOCALE);
        got_server_locale=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "server_locale", sgd.serverLocale, LOG_TO_SERVER_STDOUT);
        }

    if (got_keyin_Id == GOT_NO_CONFIG_VALUE ){

#ifndef XABUILD  /* JDBC Server */

        /* Use product default */
        strcpy(sgd.configKeyinID,CONFIG_DEFAULT_KEYIN_ID);

        got_keyin_Id=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "keyin_id", sgd.configKeyinID, LOG_TO_SERVER_STDOUT);
        }

#else  /* XA JDBC Server */

        /* Always use the generated runid in an XA JDBC Server */
        strcpy(sgd.configKeyinID,sgd.generatedRunID);

        got_keyin_Id=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_KEYIN_ID_XA_GENERATED_VALUE,
                                "keyin_id", sgd.configKeyinID, LOG_TO_SERVER_STDOUT);
        }

#endif /* JDBC and XA JDBC Servers */

    if (got_user_access_control == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        sgd.user_access_control= USER_ACCESS_CONTROL_OFF;
        got_user_access_control=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "user_access_control", CONFIG_JDBC_USER_ACCESS_CONTROL_OFF, LOG_TO_SERVER_STDOUT);
        }

    if (got_server_priority == GOT_NO_CONFIG_VALUE ){
        /* Use the default switching priority, and Indicate it with a message */
        strcpy(sgd.server_priority, DEFAULT_SERVER_PRIORITY);
        sgd.er_level_value = DEFAULT_ER_LEVEL_ARG;
        got_server_priority = HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "server_priority", DEFAULT_SERVER_PRIORITY, LOG_TO_SERVER_STDOUT);
        }

#ifndef XABUILD  /* JDBC Server */
    if (got_comapi_modes == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        strcpy(sgd.comapi_modes,CONFIG_DEFAULT_COMAPI_MODE);
        got_comapi_modes=HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "comapi_modes", sgd.comapi_modes, LOG_TO_SERVER_STDOUT);
        }
#endif          /* JDBC Server */

    if (got_app_group_number == GOT_NO_CONFIG_VALUE ){
        /* Get default value - use App Group Name to look it up */
        error_status1 = get_application_group_number();

        /* Test status, did we get the app group number? */
        if (error_status1 == 0){
            /* We got it */
            got_app_group_number=HAVE_CONFIG_DEFAULT;
        } else {
            /* Error detected, set error_status to stop config processing. */
            error_status = error_status1;
        }
    }

#ifdef XABUILD  /* XA JDBC Server */
    /* Test for the "hidden" JDBC XA Server only configuration parameter xa_thread_reuse. */
    if (got_xa_thread_reuse == GOT_NO_CONFIG_VALUE ){
        /*
         * Use the configuration default value of 100; this allows an XA database
         * thread to used up to 100 times and then that database thread is ended
         * and a new XA database thread is requested which again may be reused
         * up to 100 times, etc.
         *
         * Note: Since this is a "hidden" configuration parameter, no message will
         * generated indicating that it's default configuration value is being used.
         */
        sgd.XA_thread_reuse_limit = CONFIG_DEFAULT_XA_THREAD_REUSE_LIMIT;
        got_xa_thread_reuse = HAVE_CONFIG_DEFAULT;
    }

    if (got_uds_icr_bdi == GOT_NO_CONFIG_VALUE ){
        /*
         * Use the configuration default value of 0200000; this forces the uds icr bdi to be
         * looked up after the semantic checking on the app group name.
         */
        sgd.uds_icr_bdi = CONFIG_DEFAULT_UDS_ICR_BDI;
        got_uds_icr_bdi = HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "uds_icr_bdi", CONFIG_DEFAULT_UDS_ICR_BDI_STR, LOG_TO_SERVER_STDOUT);
        }
#endif          /* XA JDBC Server */

    if (got_rsa_bdi == GOT_NO_CONFIG_VALUE ){
        /*
         * Use the configuration default value of 0200000; this forces the rsa bdi to be
         * looked up after the semantic checking on the app group name.
         */
        sgd.rsaBdi = CONFIG_DEFAULT_RSA_BDI;
        got_rsa_bdi = HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "rsa_bdi", CONFIG_DEFAULT_RSA_BDI_STR, LOG_TO_SERVER_STDOUT);
    }

    if (got_log_console_output == GOT_NO_CONFIG_VALUE ){
        /* Use product default */
        sgd.logConsoleOutput = CONFIG_DEFAULT_LOG_CONSOLE_OUTPUT_ON;
        got_log_console_output =HAVE_CONFIG_DEFAULT;
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_PARAMETER_MISSING_DEFAULT_USED,
                                "log_console_output", CONFIG_DEFAULT_LOG_CONSOLE_OUTPUT_ON_STRING, LOG_TO_SERVER_STDOUT);
        }

   /*
    * Test if there was a syntactic or missing required parameter
    * problem with one of the configuration file images.  If so,
    * then we can't process the configuration file.  Return error
    * status to caller.
    */
   if (error_status !=0){
    return error_status; /* stop processing due to error */
   }

   /*
    * Semantic Checking
    *
    * Place some space after the missing parameter check. Then indicate that we
    * are now doing a semantic check on the set of configuration parameters.
    */
   if (!testOptionBit('N')){
       /* Display into STDOUT status on configuration processing. */
       printf("\nNow performing a semantic check on the set of configuration parameters.\n\n");
   }

   /*
    * We have a syntactically correct set of parameters. Now we
    * must apply the semantic checks to the configuration parameters.
    * This is done by calling a series of functions that each test
    * one configuration parameter.  When all the semantic checks are
    * completed, we have an acceptable initial JDBC Server configuration
    * or we have encountered a error and configuration processing is halted.
     * Indicate that we want to validate the configuration parameter and
     * update the sgd, allowing program defaults to be used if needed.
     *
     * Notice that it is not necessary to actually move the tested
     * value into the sgd, since in this case the tested values are actually
     * coming from the sgd as set above.
    */
    mode=VALIDATE_AND_UPDATE_ALLOW_DEFAULT;

   /* A 0 value is the normal return from one of the sematic check functions
    * when the parmater is acceptable. If the value returned is < 0, it is the
    * negative value of the warning code message number that indicates the
    * semantic check failed and a product defined default value was applied
    * to the sgd. If the status is > 0 then the configuration parameter is not
    * acceptable and no product defined value was applied.
    */

#ifndef XABUILD  /* JDBC Server */
    /* Test max_activities parameter */
    tempval=sgd.maxServerWorkers;
    error_status = validate_config_int(tempval,mode,CONFIG_MAX_ACTIVITIES,
                             CONFIG_LIMIT_MINSERVERWORKERS,CONFIG_LIMIT_MAXSERVERWORKERS,
                             BOOL_TRUE,CONFIG_DEFAULT_MAXSERVERWORKERS,
                             JDBCSERVER_VALIDATE_MAXACTIVITIES_ERROR,
                             JDBCSERVER_VALIDATE_MAXACTIVITIES_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         sprintf(insert1,"%d",tempval);
         sprintf(insert2,"%d",CONFIG_DEFAULT_MAXSERVERWORKERS);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,insert2,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test max_queued_comapi parameter */
    tempval=sgd.maxCOMAPIQueueSize;
    error_status = validate_config_int(tempval,mode,CONFIG_MAX_QUEUED_COMAPI,
                                       CONFIG_LIMIT_MINQUEUEDCLIENTS,sgd.maxServerWorkers,
                                       BOOL_TRUE,CONFIG_DEFAULT_COMAPI_QUEUESIZE,
                                       JDBCSERVER_VALIDATE_MAXQUEUEDCOMAPI_ERROR,
                                       JDBCSERVER_VALIDATE_MAXQUEUEDCOMAPI_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         sprintf(insert1,"%d",tempval);
         sprintf(insert2,"%d",CONFIG_DEFAULT_COMAPI_QUEUESIZE);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,insert2,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test server_receive_timeout  */
    tempval=sgd.server_receive_timeout;
    error_status = validate_config_int(tempval,mode,
                                       CONFIG_SERVER_RECEIVE_TIMEOUT,
                                       CONFIG_LIMIT_MINSERVER_RECEIVE_TIMEOUT,
                                       CONFIG_LIMIT_MAXSERVER_RECEIVE_TIMEOUT,
                                       BOOL_TRUE,CONFIG_DEFAULT_SERVER_RECEIVE_TIMEOUT,
                                       JDBCSERVER_VALIDATE_SERVER_RECEIVE_TIMEOUT_ERROR,
                                       JDBCSERVER_VALIDATE_SERVER_RECEIVE_TIMEOUT_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         sprintf(insert1,"%d",tempval);
         sprintf(insert2,"%d",CONFIG_DEFAULT_SERVER_RECEIVE_TIMEOUT);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,insert2,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test server_send_timeout  */
    tempval=sgd.server_send_timeout;
    error_status = validate_config_int(tempval,mode,
                                       CONFIG_SERVER_SEND_TIMEOUT,
                                       CONFIG_LIMIT_MINSERVER_SEND_TIMEOUT,
                                       CONFIG_LIMIT_MAXSERVER_SEND_TIMEOUT,
                                       BOOL_TRUE,CONFIG_DEFAULT_SERVER_SEND_TIMEOUT,
                                       JDBCSERVER_VALIDATE_SERVER_SEND_TIMEOUT_ERROR,
                                       JDBCSERVER_VALIDATE_SERVER_SEND_TIMEOUT_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         sprintf(insert1,"%d",tempval);
         sprintf(insert2,"%d",CONFIG_DEFAULT_SERVER_SEND_TIMEOUT);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,insert2,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test client_receive_timeout  */
    tempval=sgd.server_activity_receive_timeout;
    error_status = validate_config_int(tempval,mode,
                                       CONFIG_CLIENT_RECEIVE_TIMEOUT,
                                       CONFIG_LIMIT_MINCLIENT_RECEIVE_TIMEOUT,
                                       CONFIG_LIMIT_MAXCLIENT_RECEIVE_TIMEOUT,
                                       BOOL_TRUE,CONFIG_DEFAULT_CLIENT_RECEIVE_TIMEOUT,
                                       JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_ERROR,
                                       JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_WARNING,
                                       NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         sprintf(insert1,"%d",tempval);
         sprintf(insert2,"%d",CONFIG_DEFAULT_CLIENT_RECEIVE_TIMEOUT);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,insert2,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }
#endif          /* JDBC Server */

    /* Test server_log_file parameter */
    strcpy(insert1,sgd.serverLogFileName);
    error_status = validate_config_filename(sgd.serverLogFileName,mode,CONFIG_SERVERLOGFILENAME,
                                       CONFIG_LIMIT_MINFILENAMELENGTH,CONFIG_LIMIT_MAXFILENAMELENGTH,
                                       BOOL_TRUE,CONFIG_DEFAULT_SERVER_LOGFILENAME,
                                       JDBCSERVER_VALIDATE_LOGFILENAME_ERROR,
                                       JDBCSERVER_VALIDATE_LOGFILENAME_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,CONFIG_DEFAULT_SERVER_LOGFILENAME,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test server_trace_file parameter */
    strcpy(insert1,sgd.serverTraceFileName);
    error_status = validate_config_filename(sgd.serverTraceFileName,mode,CONFIG_SERVERTRACEFILENAME,
                                       CONFIG_LIMIT_MINFILENAMELENGTH,CONFIG_LIMIT_MAXFILENAMELENGTH,
                                       BOOL_TRUE,CONFIG_DEFAULT_SERVER_TRACEFILENAME,
                                       JDBCSERVER_VALIDATE_TRACEFILENAME_ERROR,
                                       JDBCSERVER_VALIDATE_TRACEFILENAME_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,CONFIG_DEFAULT_SERVER_LOGFILENAME,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test client_default_tracefile_qualifier  */
    strcpy(default_client_tracefile_qual,CONFIG_DEFAULT_CLIENT_DEFAULT_TRACEFILE_QUAL_P1);
    strcat(default_client_tracefile_qual,sgd.appGroupName);

    strcpy(insert1,sgd.defaultClientTraceFileQualifier);
    error_status = validate_config_string(sgd.defaultClientTraceFileQualifier,mode,
                                       CONFIG_CLIENT_DEFAULT_TRACEFILE_QUAL,
                                       CONFIG_LIMIT_MINCLIENT_DEFAULT_TRACEFILE_QUAL,
                                       CONFIG_LIMIT_MAXCLIENT_DEFAULT_TRACEFILE_QUAL,
                                       BOOL_TRUE,default_client_tracefile_qual,
                                       JDBCSERVER_VALIDATE_TRACEFILEQUAL_LENGTH_ERROR,
                                       JDBCSERVER_VALIDATE_TRACEFILEQUAL_LENGTH_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,default_client_tracefile_qual,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test server_locale  */
    strcpy(insert1,sgd.serverLocale);
    error_status = validate_config_string(sgd.serverLocale,mode,
                                       CONFIG_SERVER_LOCALE,
                                       CONFIG_LIMIT_MINSERVER_LOCALE,
                                       CONFIG_LIMIT_MAXSERVER_LOCALE,
                                       BOOL_TRUE,CONFIG_DEFAULT_SERVER_LOCALE,
                                       JDBCSERVER_VALIDATE_SERVER_LOCALE_ERROR,
                                       JDBCSERVER_VALIDATE_SERVER_LOCALE_WARNING, NULL);
    if (error_status != 0 ){
         /* is there a warning or an error? */
         abs_error_status=abs(error_status);
         if (error_status < 0){
            /* Warning, insert bad value and default value used */
            getLocalizedMessage(abs(error_status),insert1,CONFIG_DEFAULT_SERVER_LOCALE,LOG_TO_SERVER_STDOUT, msgBuffer);
         }
         else {
            /* Error, insert bad value used */
            getLocalizedMessage(abs(error_status),insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return error_status;
         }
    }

    /* Test app_group_number */
    if (sgd.appGroupNumber < CONFIG_LIMIT_MIN_APPGROUPNUMBER ||
        sgd.appGroupNumber > CONFIG_LIMIT_MAX_APPGROUPNUMBER    ){
            /* Value is outside allowable range */
            sprintf(insert1,"%d",sgd.appGroupNumber);
            getLocalizedMessage(JDBCSERVER_CONFIG_APPGROUPNUMBER_ERROR,
                                     insert1,NULL,LOG_TO_SERVER_STDOUT, msgBuffer);
            return JDBCSERVER_CONFIG_APPGROUPNUMBER_ERROR;
    }

#ifdef XABUILD  /* XA JDBC Server */
    /*
     * Test xa_thread_reuse
     *    There is no additional semantic processing required for the
     *    "hidden" JDBC XA Server only xa_thread_reuse configuration
     *    parameter.
     *
     *
     * Test uds_icr_bdi
     *
     * (This processing is only done in an JDBC XA Server.)
     *
     * First test if the user is requesting that we look up the UDS ICR bdi.
     * (This uses the app group name provided in the configuration).
     */
    if (sgd.rsaBdi == CONFIG_DEFAULT_UDS_ICR_BDI){
        /* We need to look up the UDS ICR bdi value. It will later still
         * have to go through the valid range check as a second check.
         */
        error_status = get_UDS_ICR_bdi();

        /* Test status, did we get the UDS ICR bdi value? */
        if (error_status != 0){
            /* Error detected, stop config processing. */
            return error_status;
        }
    }

    /* Test if the Bdi value is within legal range. */
    if (sgd.uds_icr_bdi < CONFIG_LIMIT_MIN_BDI_VALUE ||
        sgd.uds_icr_bdi > CONFIG_LIMIT_MAX_BDI_VALUE){
            /* Value is outside allowable range */
            sprintf(insert1,"0%o",sgd.uds_icr_bdi);
            getLocalizedMessage(JDBCSERVER_CONFIG_UDS_ICR_BDI_ERROR,
                                     insert1,NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
            return JDBCSERVER_CONFIG_UDS_ICR_BDI_ERROR;
    }
#endif          /* XA JDBC Server */


    /*
     * Test rsa_bdi.
     *
     * First test if the user is requesting that we look up the RSA bdi (this
     * uses the app group name provided in the configuration.)
     */
    if (sgd.rsaBdi == CONFIG_DEFAULT_RSA_BDI){
        /* We need to look up the RSA bdi value. It will later still
         * have to go through the valid range check as a second check.
         */
        error_status = get_RSA_bdi();

        /* Test status, did we get the RSA bdi value? */
        if (error_status != 0){
            /* Error detected, stop config processing. */
            return error_status;
        }
    }

    /* Test if the Bdi value is within legal range. */
    if (sgd.rsaBdi < CONFIG_LIMIT_MIN_BDI_VALUE ||
        sgd.rsaBdi > CONFIG_LIMIT_MAX_BDI_VALUE){
            /* Value is outside allowable range */
            sprintf(insert1,"0%o",sgd.rsaBdi);
            getLocalizedMessage(JDBCSERVER_CONFIG_RSA_BDI_ERROR,
                                     insert1, NULL, LOG_TO_SERVER_STDOUT, msgBuffer);
            return JDBCSERVER_CONFIG_RSA_BDI_ERROR;
    }

    /* Store the RSA BDI for the app group for later RSA calls. This works for
     * both the XA Server and JDBC Server since the activity that has called for
     * the configuration processing is the activity that will need to access RSA.
     *
     * Do this via an RSA call.
     */
    RSA$SETBDI2(sgd.rsaBdi);

#ifndef XABUILD  /* JDBC Server */
    /* Test comapi_modes - We already have a string
     * of the unique COMAPI modes requested. The rest
     * will be done at JDBC Server start up time.
     */
#endif          /* JDBC Server */

    /*
     * Note: the following configuration parameters did
     *       not need semantic testing, or will have that testing
     *       done at a later/earlier time:
     *           keyin_id - testing is done by the CH at its ER KEYIN$ time
     *           user_access_control - validation was done at configuration
     *                                 parameter syntactic check since there
     *                                 is no interaction with other configuration
     *                                 parameters at this time. The remaining testing
     *                                 is done at JDBC Server start up time when the
     *                                 application group number and the JDBC user
     *                                 access security file name is set up.
     *           log_console_output -  validation was done at configuration
     *                                 parameter syntactic check since there
     *                                 is no interaction with other configuration
     *                                 parameters at this time.
     *                                 Legal values are ON (default) and OFF.
     *           server_priority -     validation was done at configuration
     *                                 parameter syntactic check since there
     *                                 is no interaction with other configuration
     *                                 parameters at this time.
     *                                 Legal values are TXN, USER, BATCH, and DEMAND.
     *          server_listens_on -    The host names are set up or looked up when
     *                                 the ICL's are set up for each COMAPI mode.
     *      rdms_threadname_prefix -   No additional semantic testing was needed.
     *           client_keep_alive -   No additional semantic testing was needed.
     */

    /* all done, return the result of this process to the caller. */
    return error_status;
}

/**
 * Function isValidKeyinId
 *
 * Checks if the string passed to it is in valid Keyin Id format:
 *    1) It must be all alphanumeric,
 *    2) It must start with a letter,
 *    3) It must be from 1 to characters in length (as determined by
 *       MIN_KEYIN_CHAR_SIZE and KEYIN_CHAR_SIZE)
 *
 * @param string
 *   The string to verify. The string coming is assumed to be already
 *   uppercased (this is not checked here).
 *
 * Returns:
 *    BOOL_FALSE (0) - False, the string is not valid
 *    BOOL_TRUE  (1) - True,  the string is in valid format.
 */

int isValidKeyinId(char * string) {

    int len;
    int i1;

    /* Get length of string and test it */
    len = strlen(string);
    if (len < MIN_KEYIN_CHAR_SIZE || len > KEYIN_CHAR_SIZE ){
        /* Wrong length, return error indicator */
        return BOOL_FALSE;
    }

    /* test first character for an alphabetic */
    if (isalpha(string[0])== 0){
        /* Not an alphabetic character, return error indicator */
        return BOOL_FALSE;
    }

    /* test the rest of the string for alphanumerics */
    for (i1 = 1; i1<len; i1++) {
        if (isalnum(string[i1])== 0){
            /* Not an alphanumeric character, return error indicator */
            return BOOL_FALSE;
        }
    }

    /* The string passed, return true */
    return BOOL_TRUE;
}

/*
 * Function get_RSA_bdi
 * This function retrieves the RSA bdi for the associated App Group Name
 * and places it into the sgd.rsaBdi variable.
 *
 * If an error occurs, or the App Group Name is not found, then sgd.rsaBdi
 * is set to 0 and an error message is produced.
 *
 * Returned status:
 *      0  - Normal return, RSA bdi looked up and saved.
 *
 *    !=0  - Error detected, sgd.rsaBdi set to 0, an error message is
 *           produced, and an error status (!=0) is returned.
 */
int get_RSA_bdi(){

    int rsaStatus;
    int appGroupNameLen;
    char digits[ITOA_BUFFERSIZE];
    char localAppGroupName[MAX_RSA_BDI_LEN+1];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Get the RSA BDI in the sgd (via an RSA call, passing the app group name).
       This is needed for every worker activity.
       If the RSA call does not return 0, err off the server.
       */
    appGroupNameLen = strlen(sgd.appGroupName);
    strcpy(localAppGroupName, sgd.appGroupName);
    strncat(localAppGroupName, BLANKS_12, MAX_RSA_BDI_LEN-appGroupNameLen);
    toUpperCase(localAppGroupName);

    sgd.rsaBdi = RSA$SETBDI(localAppGroupName, &rsaStatus);

    if (rsaStatus == 0) {
        /* Found the bdi, output a warning message and continue. */
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_RSA_BDI_DEFAULT_RETRIEVED,
            itoa(sgd.rsaBdi, digits, 8), NULL, LOG_TO_SERVER_STDOUT);
                /* 5167 Configuration parameter rsa_bdi has BDI lookup
                        requested; RSA BDI value 0{0} will be used */
    } else {
        /* Bad lookup, can't find RSA bdi so clear sgd.rsaBdi and signal error  */
        sgd.rsaBdi=0;
        getLocalizedMessage(SERVER_BAD_RSA_BDI_CALL,
            itoa(rsaStatus, digits, 10), sgd.appGroupName, LOG_TO_SERVER_STDOUT, msgBuffer);
                /* 5311 The RSA BDI could not be obtained - error status {0},
                        check if app_group_name parameter {1} is correct */
    }
    return rsaStatus;
}

#define UREPBDI_FILE_IMAGE_MAX_CHARS IMAGE_MAX_CHARS
#define UREPBDI_FILE_IMAGE_OFFSET_APP_QUAL 20 /* offset in the UREP$BDI file image to the app groups app qual */
#define UREPBDI_FILE_IMAGE_OFFSET_APP_NUM 36  /* offset in the UREP$BDI file image to the app groups app number */
#define NUMBER_OF_CHARS_IN_APP_NUM 2          /* Currently app group numbers are of form nn */
#define APPGROUP_NAME_PREFIX "APP "           /* Prefix string before appgroup name in UDS$$SRC*UREP$BDI images */
#define UDS_APP_GROUP_INFO_FILENAME "UDS$$SRC*UREP$BDI"        /* Where the app names, BDI's and app number info is found */

/*
 * Function get_application_group_number
 *
 * This function looks into the file UDS$$SRC*UREP$BDI. ( see #define
 * UDS_APP_GROUP_INFO_FILENAME ) and retrieves the application group
 * number that is associated with the application group name provided
 * in the JDBC Server configuration file.
 *
 * This function assumes it can assign the file for read access. If not,
 * the function will return a error value of -1.  It also assumes that
 * the images in the file are no more than 256 characters in length; the
 * actual image length of images in file UDS$$SRC*UREP$BDI. is greatly less
 * than 256 (typically about 80 characters).
 *
 * Input Parameters:
 *      appname_ptr   - a character pointer to the six character
 *                      application group name.
 *
 * Returned Function Value:
 *
 *      0             - indicates the application group name was found and
 *                      sgd.appGroupNumber has been set.
 *
 *      != 0          - indicates the the application group name was not
 *                      found. An error message is produced, sgd.appGroupNumber
 *                      is set to 0 and the returned status value is an error
 *                      indicator (!=0). Types of errors include:
 *
 *                             UDS$$SRC*UREP$BDI. file could not be assigned
 *                             (status value is -1), or closed(status value
 *                             is -2), returned value is an error message number.
 *
 *                             App group name was not found in the UDS$$SRC*UREP$BDI.
 *                             file (returned value is an error message number).
 */
int get_application_group_number(){

    int error_status;
    int appname_len;
    int appnum=0;
    char appnum_char[3]; /* room for 2 characters and a null */
    char appqual_char[MAX_QUALIFIER_LEN+1]; /* room for 12 characters and a null */
    char digits[ITOA_BUFFERSIZE];
    FILE *urepBdiFile;
    char *imagePtr;
    char testAppGroupNameStr[sizeof(APPGROUP_NAME_PREFIX)+MAX_APPGROUP_NAMELEN]; /* string will be of form "APP xxxxxxxxxxxx" (17 characters with null) */
    int testAppGroupNameStr_test_len = sizeof(testAppGroupNameStr)-1; /* test string length without the ending null */
    char image[UREPBDI_FILE_IMAGE_MAX_CHARS];  /* assume a maximum image length of 256 characters */
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /*
     * Get length of App group name, and check if its legal
     * length (0 , App group name length < MAX_APPGROUP_NAMELEN).
     * If not, error (name not found) since there cannot be an
     * app group by that "name".
     */
    appname_len=strlen(sgd.appGroupName);
    if ((appname_len < 1) || (appname_len > MAX_APPGROUP_NAMELEN) ){
            /* Can't find the Application Group name. */
            getLocalizedMessageServer(JDBCSERVER_CONFIG_APP_GROUP_NAME_RSA_BDI_ERROR,
                   sgd.appGroupName, UDS_APP_GROUP_INFO_FILENAME, LOG_TO_SERVER_STDOUT, msgBuffer);
                   /* 5168 Configuration parameter app_group_name {0} is not allowed; could
                           not retrieve the associated RSA bdi value */
            return JDBCSERVER_CONFIG_APP_GROUP_NAME_RSA_BDI_ERROR;
    }

    /* Open the file UDS$$SRC*UREP$BDI. for read access only */
    urepBdiFile=fopen(UDS_APP_GROUP_INFO_FILENAME,"r");
    if (urepBdiFile == NULL){
        /* Can't open it, generate error message (with a status -1) and return */
        getLocalizedMessageServer(SERVER_CANT_OPENCLOSEFILE_UREPBDI_ERROR,
              UDS_APP_GROUP_INFO_FILENAME, itoa(-1, digits, 10), LOG_TO_SERVER_STDOUT, msgBuffer);
              /* 5385 Cannot look up appropriate default app_group_number parameter
                      value because file {0} can't be opened/closed,  status = {1} */
        return SERVER_CANT_OPENCLOSEFILE_UREPBDI_ERROR;
    }

    /*
     * Get the test app group name string in the form
     * "APP xxxxxx" where xxxxxx is the app group name
     * passed into the function. The app group name
     * is always 6 characters, and should be in upper case.
     */
    strcpy(testAppGroupNameStr, APPGROUP_NAME_PREFIX);
    strcat(testAppGroupNameStr,sgd.appGroupName);
    toUpperCase(testAppGroupNameStr);   /* make sure test app name string is in upper case */

    /* If test app group name is less than max. allowed app group name length, add padding blanks. */
    if (appname_len < MAX_APPGROUP_NAMELEN){
        strncat(testAppGroupNameStr,"            ",MAX_APPGROUP_NAMELEN-appname_len);
    }

    /*
     * Now loop over all the records looking for the
     * desired Application group name.
     *
     * Loop until we get a NULL pointer - indicates end of file (EOF).
     */
    while ( fgets(image,UREPBDI_FILE_IMAGE_MAX_CHARS,urepBdiFile) != NULL){

    /*
     * Get a pointer to the image area. Use this pointer to traverse
     * the image.
     */
    imagePtr=&image[0];

    /* test if we have found the desired entry */
    if (strncmp(imagePtr,testAppGroupNameStr,testAppGroupNameStr_test_len) == 0 ){
        /* Yes, get the two digit app group number (as a string)*/
        appnum_char[0]='\0';
        strncat(appnum_char,imagePtr+UREPBDI_FILE_IMAGE_OFFSET_APP_NUM,NUMBER_OF_CHARS_IN_APP_NUM);
        appnum=atoi(appnum_char); /* get app number as a number */

        /* Since we are here, get the application group qualifier also. */
        strncpy(appqual_char, imagePtr + UREPBDI_FILE_IMAGE_OFFSET_APP_QUAL, MAX_QUALIFIER_LEN);
        appqual_char[MAX_QUALIFIER_LEN] = '\0';
        sscanf(appqual_char,"%s", &sgd.appGroupQualifier); /*  get application group qualifier */

        /* Done with UREP$BDI. file, so close it. */
        error_status=fclose(urepBdiFile);
        if (error_status){
          /* Can't close UDS$$SRC*UREP$BDI., generate error message (with a status -2) and return */
          getLocalizedMessageServer(SERVER_CANT_OPENCLOSEFILE_UREPBDI_ERROR,
              UDS_APP_GROUP_INFO_FILENAME, itoa(-2, digits, 10), LOG_TO_SERVER_STDOUT, msgBuffer);
              /* 5385 Cannot look up appropriate default app_group_number parameter
                      value because file {0} can't be opened/closed,  status = {1} */
          return SERVER_CANT_OPENCLOSEFILE_UREPBDI_ERROR;
        }

        /* Normal return, we found application group number, and have application group qualifier. */
        sgd.appGroupNumber=appnum;
        /* printf("Got the application group number = %d\n",sgd.appGroupNumber);  */
        printf("** ");
        getMessageWithoutNumber(JDBCSERVER_CONFIG_APP_GROUP_NUMBER_DEFAULT_RETRIEVED,
            itoa(sgd.appGroupNumber, digits, 10), NULL, LOG_TO_SERVER_STDOUT);
            /* 5169 Configuration parameter app_group_number {0} was retrieved from a UDS/RDMS system file. */
        return 0;
    }

    /* App group name not found yet, try next image */
    } /* end of while */

    /*
     * All images checked and Application group name
     * was not found, issue error message, and return
     * error code.
     */
    getLocalizedMessageServer(JDBCSERVER_CONFIG_APP_GROUP_NAME_RSA_BDI_ERROR,
          sgd.appGroupName, UDS_APP_GROUP_INFO_FILENAME, LOG_TO_SERVER_STDOUT, msgBuffer);
          /* 5168 Configuration parameter app_group_name {0} is not allowed; could
                  not retrieve the associated RSA bdi value */
    return JDBCSERVER_CONFIG_APP_GROUP_NAME_RSA_BDI_ERROR;
}

#ifdef XABUILD  /* XA JDBC Server */
/*
 * Function get_UDS_ICR_bdi
 * This function is only used by the JDBX XA Server configuration processing
 * to retrieve the UDS ICR bdi for the associated App Group Name and places
 * it into the sgd.uds_icr_bdi variable.
 *
 * If an error occurs, or the App Group Name is not found, then sgd.uds_icr_bdi
 * is set to 0 and an error message is produced.
 *
 * Returned status:
 *      0  - Normal return, UDS ICR bdi looked up and saved.
 *
 *    !=0  - Error detected, sgd.uds_icr_bdi set to 0, an error message is
 *           produced, and an error status (!=0) is returned.
 */
int get_UDS_ICR_bdi(){

    int current_appGroupNumber;
    int apQualLen;
    FILE *BdiFile;
    char file_spec[IMAGE_MAX_CHARS];
    char digits[ITOA_BUFFERSIZE];
    char insert1[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char insert2[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /*
     * Check if we have an Application group's qualifier.  We will need it.
     */
    if (sgd.appGroupQualifier[0] == '\0'){
        /* We have to look the qualifier up. Since the look up of the application
         *  group qualifier involves almost the identical work, just use
         * get_application_group_number() to get the information. While we are at
         * it, we can re-verify the application group number provided by the
         * configuration is correct.
         */
        current_appGroupNumber = sgd.appGroupNumber; /* save current value for later test. */
        get_application_group_number(); /* get the app group qualifer and number. */

        /* Test if the retrieved app group number is the same as the SGD value.  If the
         * app group number was not provided in the config file (default wanted), then it
         * was looked up using get_application_group_number(). So if the app group number
         * found now is not the same as the current value was, then it must have been
         * specified explicitly in the config file and it was specified incorrectly. So
         * issue an error message in that case.
         */
        if (current_appGroupNumber != sgd.appGroupNumber){
            /* Indicate app group number difference. */
            sprintf(insert1,"%d", current_appGroupNumber);
            sprintf(insert2,"%d", sgd.appGroupNumber);
            getLocalizedMessage(JDBCSERVER_CONFIG_INCORRECT_APPGROUPNUMBER_ERROR,
                                     insert1, insert2, LOG_TO_SERVER_STDOUT, msgBuffer);
            return JDBCSERVER_CONFIG_INCORRECT_APPGROUPNUMBER_ERROR;
        }
    }

    /*
     * Now obtain a list of the installed application group qualifiers and UDSICR bdi's.
     * The UDSICR information was produced by an SSG skeleton just before the
     * execution of the JDBC Server, with the results left in the file
     * TPF$.UDSICR/BDI.
     *
     * Open the file UDSICR_QUAL_BDI_FILENAME (TPF$.UDSICR/BDI.)  for
     * read access only.
     */
    sgd.uds_icr_bdi = 0;
    BdiFile = fopen(UDSICR_QUAL_BDI_FILENAME,"r");
    if (BdiFile == NULL){
        /* Can't open file TPF$.UDSICR/BDI.  Without it, the JDBC
         * XA Server cannot operate correctly, so signal the error
         * and stop the server.
         */
        getLocalizedMessageServer(XASERVER_CANT_OPEN_UDSICRBDI_FILE,
              UDSICR_QUAL_BDI_FILENAME, NULL, SERVER_LOGS, msgBuffer);
              /* 5415 Cannot open file {0} to obtain UDSICR bdi information,
                      XA JDBC Server shutting down */
        return XASERVER_CANT_OPEN_UDSICRBDI_FILE;
    }
    /* Need the length of the qualifier since it can be from 1 to 12 chars */
    apQualLen = strlen(sgd.appGroupQualifier);
    /* Now loop over the UDSICR entries */
    while (fgets(file_spec, IMAGE_MAX_CHARS, BdiFile) != NULL) {
       if (memcmp(sgd.appGroupQualifier, &file_spec[1], apQualLen) == 0) {
          if (file_spec[0] == '-' && isalpha(file_spec[1]) && file_spec[apQualLen+1] == ':' &&
              memcmp(&file_spec[apQualLen+2], "04", 2) == 0 && isoctal(file_spec[apQualLen+4]) &&
              isoctal(file_spec[apQualLen+5]) && isoctal(file_spec[apQualLen+6]) &&
              isoctal(file_spec[apQualLen+7]) &&isoctal(file_spec[apQualLen+8]) &&
              file_spec[apQualLen+9] == ' '){
                  /* We got an entry, get the bdi value */
                  sgd.uds_icr_bdi = (file_spec[apQualLen+4] << 12) + (file_spec[apQualLen+5] << 9) +
                                    (file_spec[apQualLen+6] << 6)  + (file_spec[apQualLen+7] << 3) +
                                     file_spec[apQualLen+8] - OCT_CONVERT + 0200000;
                  break;
          }
       }
    }

    /* Did we find the correct UDS ICR bdi value? */
    if (sgd.uds_icr_bdi == 0) {
        /* Retrieved information is not in the correct format to
         * provide the UDSICR bdi qualifier information.  Without it,
         * the JDBC Server cannot operate correctly, so signal the
         * error and stop the server.  */
        getLocalizedMessageServer(XASERVER_BAD_UDSICR_BDI_INFO,
              file_spec, NULL, SERVER_LOGS, msgBuffer);
              /* 5416 Incorrect UDSICR BDI specification: {0}, XA JDBC Server shutting down*/
        return XASERVER_BAD_UDSICR_BDI_INFO;
    }

    /* Found the UDS ICR bdi, so output a warning message and continue. */
    printf("** ");
    getMessageWithoutNumber(JDBCSERVER_CONFIG_UDS_ICR_BDI_DEFAULT_RETRIEVED,
                            itoa(sgd.uds_icr_bdi, digits, 8), NULL, LOG_TO_SERVER_STDOUT);
          /* 5180 Configuration parameter uds_icr_bdi has BDI lookup requested; UDS ICR BDI value 0{0} will be used */

    /* Now close the UDSICR bdi file. */
    if (fclose(BdiFile)){
        /* Can't close file TPF$.UDSICR/BDI.  This is not considered
         * fatal, just log the error and continue.
         */
        getMessageWithoutNumber(SERVER_CANT_CLOSE_BDI_FILE, UDSICR_QUAL_BDI_FILENAME, \
                                "XA ", SERVER_LOGS);
           /* 5390 Cannot close file {0}, {1}JDBC Server will continue operation */
    }

    /* UDS ICR value set, so return. */
    return 0;
}
#endif          /* XA JDBC Server */
