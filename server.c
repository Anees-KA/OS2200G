/**
 * File: Server.c.
 *
 * Procedures that comprise the JDBC Server's callable interfaces
 * that are necessary for the proper operation of the C-Interface
 * Library code while maintaining proper isolation of the JDBC
 * Server and Server Worker activity code and environment from the
 * C-Interface Library code.

 *
 * This code may utilize other code in the C-Interface library to accomplish
 * its required tasks.
 *
 */

/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <tsqueue.h>
#include <task.h>
#include <sysutil.h>
#include <ctype.h>
#include <universal.h>

/* JDBC Project Files */
#include "cintstruct.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "taskcodes.h" /* Include crosses the c-interface/server boundary */
#include "utilities.h" /* Include crosses the c-interface/server boundary */
#include "DisplayCmds.h"
#include "ProcessTask.h"
#include "ServerConfig.h"
#include "ServerConsol.h"
#include "ServerLog.h"

extern serverGlobalData sgd;        /* The Server Global Data (SGD),*/
                                    /* visible to all Server activities. */

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

/* Code to provide access to the Allocate_Response_Packet routine
 * is currently in AllocRespPkt.c, but this code will be moved here
 * in the near future < xxxxxxx >.
 */

/*
 * These routines use defines that are coming from JDBCServer.h. Since
 * these routines are also used by the C-Interface library routines,
 * those defines should be redefined with new defines that can be utilized
 * in the C-Interface library directly without the need to reference and
 * JDBC Server source .h files.
 */

/*
 * Function getClientSocket
 *
 * This function returns the lower 32 bits of the clients socket handle.
 *
 */
int getClientSocket(){

    return ((act_wdePtr->client_socket) & 0037777777777); /* mask off the top 4 bits */
}

/*
 * Function getSessionID()
 *
 * This function returns the full 36 bits of the clients socket handle.
 *
 */
int getSessionID(){

    return (act_wdePtr->client_socket);
}

/*
 * Function getThreadName
 *
 * This function returns the thread name used, or to be used,
 * for the RDMS thread.
 *
 */
void getThreadName(char * threadNamePtr){

    strcpy(threadNamePtr,act_wdePtr->threadName);
    return;
}

/*
 * Function setThreadName
 *
 * This function sets the RDMS Thread name. It used the passed prefix
 * and connection ID to build the threadname. The threadname is 8 characters
 * with the last 2 characters always blank, since UDSMON only shows 6 characters.
 * The connectionID is one to two characters and is module 1296 (base 36). The
 * passed prefix can be 0 to 5 characters long. If it is zero, then the default
 * prefix string found in the SGD is used. If needed, the prefix is truncated
 * on the right to make it all fit in 6 characters.
 * Note: We could use RDMS_THREADNAME_LEN, that really complicates the code
 *       below, so we assume the length is 8.
 * Default Prefix values:
 *   Server - from jdbc$config, which defaults to "JS"
 *   XAServer - the generated runId string
 *
 * @param threadNamePrefix
 *   The optional RDMS threadname prefix
 * @param connectionID
 *   The connection ID for this connection
 */
void setThreadName(char * threadNamePrefix, int connectionID){

#ifdef XABUILD  /* XA JDBC Server */

    /* Get the generated runid and space fill it */
    strcpy(act_wdePtr->threadName,sgd.generatedRunID);
    strncat(act_wdePtr->threadName,"        ",8-strlen(act_wdePtr->threadName)); /* pad to 8 chars */

#else           /* JDBC Server */

    char tempThreadname[9];
    int tempThreadnamePrefixLen;
    char DChrs[36];
    char tempChar;

    strncpy(DChrs, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", 36);
    /* Prefill the threadname string */
    memcpy(tempThreadname, "000000  ", 8);
    tempThreadname[8] = '\0';
    /* Limit the prefix to 5 characters */
    tempThreadnamePrefixLen = (strlen(threadNamePrefix) > 5 ? 5 : strlen(threadNamePrefix));
    if(tempThreadnamePrefixLen > 0) {
        /* Use the passed prefix */
        strncpy(tempThreadname, threadNamePrefix, tempThreadnamePrefixLen);
    } else {
        /* Use the default prefix from the SGD */
        tempThreadnamePrefixLen = sgd.rdms_threadname_prefix_len;
        strncpy(tempThreadname, sgd.rdms_threadname_prefix, tempThreadnamePrefixLen);
    }
    /* Generate the 1-2 character connection ID string using module 1296 (base-36)  */
    /* This should be the same algorithm used on the Java side in RdmsOject.        */
    tempChar = DChrs[(connectionID % 1296) / 36];
    if(tempChar != '0') {  /* Overlay the 5th character only if the ID is large enough */
        tempThreadname[4] = tempChar;
    }
    tempThreadname[5] = DChrs[connectionID % 36];

    /* Now we can save the final copy of the threadname */
    strncpy(act_wdePtr->threadName, tempThreadname, 8);
    act_wdePtr->threadName[8] = '\0';     /* NULL terminator */
#endif
    if (debugDetail) {
        tracef("** In setThreadName(%s, %i) - Resulting threadname is '%s'\n",
                threadNamePrefix, connectionID, act_wdePtr->threadName);
    }

    return;
}

/*
 * Function getMax_Size_Res_Pkt
 *
 * This function returns the maximum size in bytes for a single response packet.
 *
 */
int getMax_Size_Res_Pkt() {

    return SERVER_MAX_ALLOWED_RESPONSE_PACKET;
}

/*
 * Function getServerAppGroup
 *
 * This function copies the name of the UDS Application group that
 * this JDBC Server uses.
 *
 * Parameter:
 *
 * appNamePtr   -  pointer to a character array large enough
 *                 to hold the app group name followed by a
 *                 trailing . Character array must be
 *                 MAX_APPGROUP_NAMELEN+1 in size.
 *
 */
void getServerAppGroup(char * appNamePtr){

    strcpy(appNamePtr,sgd.appGroupName);
    return;
}

/*
 * Procedure getRdmsLevelID
 *
 * This function copies the RDMS level string retrieved from RDMS
 * when this JDBC Server started.
 *
 * Parameter:
 *
 * rdmsLevelIDPtr -   pointer to a character array large enough
 *                    to hold the RDMS Level string followed by a
 *                    trailing null. Character array must be
 *                    MAX_RDMS_LEVEL_LEN+1 in size.
 *
 */
void getRdmsLevelID(char * rdmsLevelIDPtr){
    strcpy(rdmsLevelIDPtr,sgd.rdmsLevelID);
    return;
}

/*
 * Procedure getRdmsBaseLevel
 *
 * This function returns the RDMS base level number from the
 * string retrieved from RDMS when this JDBC Server started.
 * This value is the first (from the left) of the three or four
 * numbers that are "-" separated in the RDMS Level ID string.
 */
int getRdmsBaseLevel(){
    return sgd.rdmsBaseLevel;
}

/*
 * Procedure getRdmsFeatureLevel
 *
 * This function returns the RDMS feature level number from the
 * string retrieved from RDMS when this JDBC Server started.
 * This value is the second (from the left) of the three or four
 * numbers that are "-" separated in the RDMS Level ID string.
 */
int getRdmsFeatureLevel(){
    return sgd.rdmsFeatureLevel;
}

/*
 * Function setClientLocale
 *
 * This function sets the client's locale string.
 *
 * @param locale
 *   The locale string from the connection properties.
 */

void setClientLocale(char * locale) {

    act_wdePtr->clientLocale[0]='\0';
    strncpy(act_wdePtr->clientLocale, locale, MAX_LOCALE_LEN);

    /* If this client's locale is the same as the server locale,
       or if there is no locale (i.e., empty string),
       we can use the server's message file.
       So set the client's message file pointer and message file name.
       */
    if ((strcmp(locale, sgd.serverLocale) == 0)
            || (strlen(locale) == 0)) {
        act_wdePtr->workerMessageFile = sgd.serverMessageFile;
        strcpy(act_wdePtr->workerMessageFileName, sgd.serverMessageFileName);

    /* If the client's locale (passed in as an argument)
       is not the same as server's locale,
       the client's message file will not be opened until the first message
       is generated via a call to getLocalizedMessage.
       In that case, getLocalizedMessage calls openMessageFile to set the fields
       workerMessageFile and workerMessageFileName in the WDE.
       */
    } else {
        act_wdePtr->workerMessageFile = NULL;
        act_wdePtr->workerMessageFileName[0]='\0';
    }
}

/*
 * Function setClientUserid
 *
 * This function sets the client's user id.
 *
 * @param userid
 *   The userid from the connection properties.
 */
void setClientUserid(char * userid){
    act_wdePtr->client_Userid[0]='\0';
    strncpy(act_wdePtr->client_Userid, userid, CHARS_IN_USER_ID);
}

/*
 * Function setClientReceiveTimeout
 *
 * This function sets the clients receive timeout value to be used for
 * a given JDBC Client's client socket in the Server Worker code handling
 * the particular client. This value is validated against the associated
 * JDBC Server's configuration parameter client_receive_timeout,
 * and if there is a conflict the JDBC Server's configuration parameter
 * will be used.  The value actually used is returned to the caller.
 *
 * Parameter:
 *
 * requested_Client_Receive_TimeoutValue -  pointer to a the client
 *                                          receive timeout requested.
 *                                          If the timeout value provided is 0,
 *                                          then the JDBC Server's value for the
 *                                          client_recieve_timeout configuration
 *                                          parameter will be used.
 * actual_Client_Receive_TimeoutValue    -  pointer to the client receive
 *                                          timeout value actually assigned.
 *
 * Returned value:
 *
 * status           -  a value of 0 is returned if the value provided was acceptedable.
 *                  -  a value < 0 is returned if the value was not acceptable
 *                     and the configuration parameter value for the receive timeout
 *                     was used.  The status value returned is the negative of the
 *                     JDBC Server warning message for this situation.
 *                  -  a value > 0 indicates the value was not acceptable. No change
 *                     to the current WDE's operational value was applied.
 *
 */
int setClientReceiveTimeout(int * requested_Client_Receive_TimeoutValue, int * actual_Client_Receive_TimeoutValue){

    int error_status;
    int timeout_value;

/* printf("timeout (sgd.server_activity_receive_timeout) is %d\n",sgd.server_activity_receive_timeout); */

    /*
     * Check if the caller has passed a timeout value of 0.  If so, use
     * the JDBC Server's client_recieve_timeout configuration parameter value,
     * otherwise use the passed in timeout value for the client's recieve timeout.
     */
    timeout_value=*requested_Client_Receive_TimeoutValue;
    if (timeout_value == 0){
       timeout_value=sgd.server_activity_receive_timeout;
    }

    /*
     * Since this is a Server Worker based value, use this workers
     * wdePtr. Have the validation and assignment operation done
     * by the configuration validation routine.
     */
    error_status = validate_config_int(timeout_value,
                                       VALIDATE_AND_UPDATE_ALLOW_DEFAULT,
                                       CONFIG_SW_CLIENT_RECEIVE_TIMEOUT,
                                       CONFIG_LIMIT_MINCLIENT_RECEIVE_TIMEOUT,
                                       CONFIG_LIMIT_MAXCLIENT_RECEIVE_TIMEOUT,
                                       BOOL_TRUE,sgd.server_activity_receive_timeout,
                                       JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_ERROR,
                                       JDBCSERVER_VALIDATE_CLIENT_RECEIVE_TIMEOUT_WARNING,
                                       act_wdePtr);

    /* return actual value currently set */
    *actual_Client_Receive_TimeoutValue=act_wdePtr->client_receive_timeout;
    return error_status;
}

/*
 * Function setClientFetchBlockSize
 *
 * This function sets the clients fetch block size value. This
 * value is validated against its associated configuration parameter
 * and if there is a conflict, the configuration parameter will
 * be used.  The value actually used is returned to the caller.
 *
 * Parameter:
 *
 * requested_Client_Fetch_Block_Size     -  pointer to a the client
 *                                          fetch block size requested.
 * actual_Client_Fetch_Block_Size        -  pointer to the client fetch
 *                                          block size actually assigned.
 *
 * Returned value:
 *
 * status           -  a value of 0 is returned if the value provided was acceptedable.
 *                  -  a value < 0 is returned if the value was not acceptable
 *                     and the configuration parameter value for the receive timeout
 *                     was used.  The status value returned is the negative of the
 *                     JDBC Server warning message for this situation.
 *                  -  a value > 0 indicates the value was not acceptable. No change
 *                     to the current operational value was applied.
 *
 */
int setClientFetchBlockSize(int * requested_Client_Fetch_Block_Size, int * actual_Client_Fetch_Block_Size){
    int error_status;

    /*
     * Since this is a Server Worker based value, use this workers
     * wdePtr. Have the validation and assignment operation done
     * by the configuration validation routine.
     */

    error_status = validate_config_int(*requested_Client_Fetch_Block_Size,
                                       VALIDATE_AND_UPDATE_ALLOW_DEFAULT,
                                       CONFIG_SW_FETCHBLOCK_SIZE,
                                       CONFIG_LIMIT_MINFETCHBLOCK_SIZE,
                                       CONFIG_LIMIT_MAXFETCHBLOCK_SIZE,
                                       BOOL_TRUE,sgd.fetch_block_size,
                                       JDBCSERVER_VALIDATE_SW_FETCHBLOCK_SIZE_ERROR,
                                       JDBCSERVER_VALIDATE_SW_FETCHBLOCK_SIZE_WARNING,
                                       act_wdePtr);

    /* return actual value currently set */
    *actual_Client_Fetch_Block_Size=act_wdePtr->fetch_block_size;
    return error_status;
}

/*
 * Function setClientTraceFile
 *
 * This function sets the clients trace file value. This
 * value is validated against its associated configuration parameter
 * and if there is a conflict, the configuration parameter will
 * be used.  The value actually used is returned to the caller.
 * The client trace file that is used will be fopened as part of
 * this operation.
 *
 * Parameter:
 *
 * requested_ClientTraceFileName     - pointer to the client trace file
 *                                     name requested.
 * actual_ClientTraceFileName        -  pointer to the client trace file
 *                                      name actually assigned.
 *
 * Returned value:
 *
 * status           -  a value of 0 is returned if the value provided was acceptedable.
 *                  -  a value < 0 is returned if the value was not acceptable
 *                     and the configuration parameter value for the receive timeout
 *                     was used.  The status value returned is the negative of the
 *                     JDBC Server warning message for this situation.
 *                  -  a value > 0 indicates the value was not acceptable. No change
 *                     to the current operational value was applied.
 *
 */int setClientTraceFile(char * requested_ClientTraceFileName, char * actual_ClientTraceFileName){
    /* int error_status; */

    /*
     * Since this is a Server Worker based value, use this workers
     * wdePtr. Have the validation and assignment operation done
     * by the configuration validation routine.
     */
/* Uncomment out this code when the details of where and what the default
 * client trace file name is
 */

  /*
    error_status = validate_config_int(requested_ClientTraceFileName,
                                       VALIDATE_AND_UPDATE_ALLOW_DEFAULT,
                                       CONFIG_CLIENT_TRACEFILENAME,
                                       CONFIG_LIMIT_MINFILENAMELENGTH,
                                       CONFIG_LIMIT_MAXFILENAMELENGTH,
                                       BOOL_TRUE,<to be supplied>,
                                       JDBCSERVER_VALIDATE_CLIENT_TRACEFILENAME_ERROR,
                                       JDBCSERVER_VALIDATE_CLIENT_TRACEFILENAME_WARNING,
                                       act_wdePtr);
  */

    /* return actual value currently set */
 /* strcpy(actual_ClientTraceFileName,act_wdePtr->clientTraceFileName); */
 /* return error_status; */

 /* temp code */
 strcpy(actual_ClientTraceFileName,"");
 return 0;
}

/*
 * Function getMaxConnections
 *
 * This function returns the maximum number of connections
 * supported by this JDBC Server.  Currently this value is
 * defined by the Server's max_activities configuration
 * parameter.
 *
 *
 */
int getMaxConnections(){
    /*
     * Return sgd.maxServerWorkers which contains the
     * max_activities configuration parameter value. This
     * is the maximum number of concurrent connections that
     * could be supported.
     *
     */
    return sgd.maxServerWorkers;
}

/*
 * Function getClientReceiveTimeout
 *
 * This function returns this JDBC Client's socket recieve timeout
 * value.  Currently this value is defined as either the Server's
 * server_activity_recieve_timeout configuration parameter or it
 * can be provided by the timeout connection property and set
 * using function setClientReceiveTimeout().
 *
 *
 */
int getClientReceiveTimeout(){

    /* printf("act_wdePtr->client_receive_timeout = %d\n",act_wdePtr->client_receive_timeout); */

    /*
     * Return this JDBC Client's socket recieve timeout
     * value from its WDE.
     */
    return act_wdePtr->client_receive_timeout;
}

#ifdef XABUILD  /* XA JDBC Server */
/*
 * Procedure getDebugXA
 *
 * This function returns the value of sgd.debugXA which
 * is used to debug the XA code only.
 */
int getDebugXA(){
    return sgd.debugXA;
}

/*
 * Function get_tx_info
 *
 * This function returns the XA TX statuses for the XA JDBC Client.
 *
 *  Parameter:
 *    transaction_state        - pointer to an int location that is
 *                               set by tx_info() to the previous
 *                               transactions state (e.g., TX_ROLLBACK).
 *
 * Function returned value:
 *    0 (TXINFO_NOT_TRX_MODE)  - Indicates we are not in a global
 *                               transactional state. This value
 *                               is returned when we are not yet
 *                               a branch (enlisted) of the global
 *                               transaction, i.e., at the time of
 *                               the XA_BEGIN_THREAD_TASK request
 *                               packet.
 *    1 (TXINFO_TRX_MODE)      - Indicates we are already in a
 *                               global transactional state. This
 *                               value is returned on the subsequent
 *                               request packets while in a transactional
 *                               branch.
 */
int get_tx_info(int * transaction_state){

    TXINFO txinfo;
    int trx_mode;

    /*
     * XA JDBC Server returns this JDBC Client's global
     * transactional state value as retrieved from OTM.
     */
    trx_mode = tx_info(&txinfo);
    *transaction_state = txinfo.transaction_state;
    return trx_mode;
}

/*
 * Function get_txnFlag_ODTPtokenValue
 *
 * This function returns the saved ODTP txn_flag and
 * odtpTokenValue values saved in the client's WDE.
 *
 * Returns, via the two pointer arguments:
 * txn_flag  - This JDBC Client's transaction state flag (int)
 *             as derived by ODTP from the current CORBA
 *             information associated to the currently
 *             being processed Request packet. A value of
 *             0 indicates we are not in a transaction. A
 *             value of 1 indicates we are in a transaction.
 *
 * odtpTokenValue - This JDBC Client's odtpTokenValue (int, as
 *             4-bytes) taken out of the request header of the
 *             currently being processed Request packet. A non-0
 *             value is both a flag and token value indicating
 *             we are in a non-transactional dialogue with a
 *             specific copy of the JDBC XA Server (as
 *             indicated by the specific token value). A
 *             value of 0 indicates we are NOT in a
 *             non-transactional stateful dialogue with
 *             a JDBC XA Server, but in a global transactional
 *             dialogue with this copy of the JDBC XA Server.
 */
void get_txnFlag_ODTPtokenValue(int * txn_flag, int * odtpTokenValue){

    /*
     * Return the txn_flag and odtpTokenValue values
     * from the current client's WDE.
     */
    *txn_flag = act_wdePtr->txn_flag;
    *odtpTokenValue = act_wdePtr->odtpTokenValue;
    return;
}

/*
* Function clearSavedXA_threadInfo clears/resets the saved User id, database
* thread access type and recovery type fields in the JDBC XA Server's SGD.
*/
void clearSavedXA_threadInfo(){

    /*
     * We are clearing/resetting the saved database thread connection information.
     *
     * We don't use this information with stateful database threads (non-XA,
     * BEGIN_THREAD_TASK), so we clear the fields and set saved_beginThreadTaskCode
     * to BEGIN_THREAD_TASK so later XA usage (XA_BEGIN_THREAD_TASK) will detect the
     * JDBC XA Server's transition to XA usage. Therefore, as an optimization, we can
     * check if we have already cleared these fields by checking if saved_beginThreadTaskCode
     *  is already BEGIN_THREAD_TASK and if so we can skip resetting the saved fields
     * since they're still reset from the last time.
     */
    if (sgd.saved_beginThreadTaskCode != BEGIN_THREAD_TASK){
        /* We need to clear/reset the various save databse thread infomation fields in the SGD. */
        sgd.saved_beginThreadTaskCode = BEGIN_THREAD_TASK;  /* Force new database thread the next time in all cases (see chart in connection.c for more details).                */
        sgd.XA_thread_reuses_so_far = 0;                    /* Reset the number of XA Server's XA database thread reuses in a row left so far. Also reset by reset_xa_reuse_counter(),saveXA_threadInfo(). */
        sgd.saved_XAaccessType = 0;                         /* Set by saveXA_threadInfo(). The saved access type for XA Server's database thread (for XA reuse use only)         */
        sgd.saved_XArecovery = 0;                           /* Set by saveXA_threadInfo(). The saved recovery mode for XA Server's database thread (for XA reuse use only)       */
        sgd.saved_rsaDump = 0;                              /* Set by saveXA_threadInfo(). The saved rdmstrace rsadump flag for XA Server's database thread (for XA reuse use only) */
        sgd.saved_debugSQLE = 0;                            /* Set by saveXA_threadInfo(). The saved debugSQLE flag for XA Server's database thread (for XA reuse use only)      */
        sgd.saved_processedUseCommand = 0;                  /* Set by processedUseCommand(). The saved processed USE command indicator. Its set to 1 if a USE command is processed.*/
        sgd.saved_processedSetCommand = 0;                  /* Set by processedSetCommand(). The saved processed SET command indicator. Its set to 1 if a SET command is processed.*/
        sgd.saved_varcharInt = 0;                           /* Set by saveXA_threadInfo(). The saved varchar setting for XA Server's database thread (for XA reuse use only). Set to 1 if property varchar=varchar is set. */
        sgd.saved_emptyStringProp = 0;                      /* Set by saveXA_threadInfo(). The saved emptyStringProp setting for XA Server's database thread (for XA reuse use only). */
        sgd.saved_skipGeneratedType = 1;                    /* Set by saveXA_threadInfo(). Reset to default 1 (on) value. The saved skipGeneratedType setting for XA Server's database thread. */
        sgd.saved_XAappGroupName[0] = '\0';                 /* Set by saveXA_threadInfo(). The saved appgroup name for XA Server's database thread (for XA reuse use only)       */
        sgd.saved_client_Userid[0] = '\0';                  /* Force new database thread the first time in all cases (cannot be empty in normal usage, see connection.c).        */
                                                            /* Set by saveXA_threadInfo(). The saved userid of the XA Server's JDBC client.                                      */
        sgd.saved_schemaName[0] = '\0';                     /* Set by saveXA_threadInfo(). The saved connection property schema name (Unicode) of the XA Server's JDBC client.   */
        sgd.saved_schemaName[1] = '\0';                     /* Set by saveXA_threadInfo(). The saved connection property schema name (Unicode) of the XA Server's JDBC client.   */
        sgd.saved_versionName[0] = '\0';                    /* Set by saveXA_threadInfo(). The saved connection property version name of the XA Server's JDBC client.            */
        sgd.saved_charSet[0] = '\0';                        /* Set by saveXA_threadInfo(). The saved connection property charSet of the XA Server's JDBC client.                 */
        sgd.saved_storageAreaName[0] = '\0';                /* Set by saveXA_threadInfo(). The saved connection property storageArea name of the XA Server's JDBC client.        */
    }
}

/*
* Function saveXA_threadInfo saves the User id, database thread access type and
* recovery type used by the current XA Server's UDS/RDMS 2PC database thread so
* they can be checked when the next client attempts to reuse the current 2PC
* database thread.
*/
void saveXA_threadInfo(int beginThreadTaskCode, char * userNamePtr, char * schemaPtr, char * versionPtr, int access_type,
                       int rec_option, int varcharInt, int emptyStringProp, int skipGeneratedType,
                       char * storageAreaPtr, char * charSetPtr){
    sgd.XA_thread_reuses_so_far = 0;                                    /* Reset the number of XA Server's XA database thread reuses in a row left so far. Also reset by reset_xa_reuse_counter(). */
    sgd.saved_beginThreadTaskCode = beginThreadTaskCode;                /* Save BT task code that tells current type of XA Server's opened database thread (non-XA, XA). */
    sgd.saved_XAaccessType = access_type;                               /* Save current access type for XA Server's database thread (for XA reuse use only)   */
    sgd.saved_XArecovery = rec_option;                                  /* Save current recovery mode for XA Server's database thread (for XA reuse use only) */
    sgd.saved_rsaDump = do_RSA_DebugDumpAll();                          /* Save current rdmstrace rsadump flag for XA Server's database thread (for XA reuse use only) */
    sgd.saved_debugSQLE = debugSQLE;                                    /* Save current debugSQLE flag for XA Server's database thread (for XA reuse use only) */
    sgd.saved_client_Userid[0] = '\0';                                  /* Clear old saved userid value. */
    strncpy(sgd.saved_client_Userid, userNamePtr, CHARS_IN_USER_ID);    /* Save current userid of the XA Server's JDBC client.   */
    sgd.saved_processedUseCommand = 0;                                  /* Clear the processed USE command indicator. Its set to 1 if a USE command is processed. */
    sgd.saved_processedSetCommand = 0;                                  /* Clear the processed SET command indicator. Its set to 1 if a SET command is processed. */
    sgd.saved_varcharInt = varcharInt;                                  /* Save current varchar setting. Its set to 1 if connection property varchar=varchar is set. */
    sgd.saved_emptyStringProp = emptyStringProp;                        /* Save current emptyStringProp setting for XA Server's database thread.                      */
    sgd.saved_skipGeneratedType = skipGeneratedType;                    /* Save current skipGeneratedType setting for XA Server's database thread.                */
    sgd.saved_schemaName[0] = '\0';                                     /* Clear old saved schema name (Unicode) value. */
    sgd.saved_schemaName[1] = '\0';                                     /* Clear old saved schema name value. */
    strcpyuu(sgd.saved_schemaName, schemaPtr);                          /* Save connection property schema name (Unicode) of the XA Server's JDBC client.         */
    sgd.saved_versionName[0] = '\0';                                    /* Clear old saved version name value. */
    strncpy(sgd.saved_versionName, versionPtr, MAX_SCHEMA_NAME_LEN);    /* Save connection property version name of the XA Server's JDBC client.                  */
    sgd.saved_charSet[0] = '\0';                                        /* Clear old saved charSet value. */
    strncpy(sgd.saved_charSet, charSetPtr, MAX_CHARSET_LEN);            /* Save connection property charSet of the XA Server's JDBC client.                       */
    sgd.saved_storageAreaName[0] = '\0';                                /* Clear old saved storageArea name value. */
    strncpy(sgd.saved_storageAreaName, storageAreaPtr, MAX_STORAGEAREA_NAME_LEN); /* Save connection property storageArea name of the XA Server's JDBC client.    */

#if XA_THREAD_REUSE_ENABLED && XA_THREAD_REUSE_TESTING
    /*
     * Check if we are saving state for the 11th client (test 9) or  the 12th client (test 10). If so,
     * we are testing XA reuse handling of the rsadump debug trace so save a value of "TRUE".
     *
     * Remember, this call is made after the client number is bumped after the thread is "opened",
     * so we do not need to adjust the clientCount value before we make our decision.
     */
    if (getClientCount() == 11 || getClientCount() == 12){
        sgd.saved_rsaDump =  BOOL_TRUE;
        if (debugInternal){
            tracef("In saveXA_threadInfo, doing thread reuse test for client %d, sgd.saved_rsaDump now %d\n", getClientCount()+1, sgd.saved_rsaDump);
        }
    }
#endif  /* XA_THREAD_REUSE_TESTING */
}

/*
 * Function compareXA_savedThreadInfo
 *
 * Tests if the input parameters match the saved User id, database thread
 * access type and recovery type from the last client user.
 *
 * First check is that the last database thread we opened was a
 * transactional one (XA_BEGIN_THREAD_TASK) because if it was a stateful
 * non-XA database thread (BEGIN_THREAD_TASK) we must always start a new
 * XA database thread so just let this method return a 0 to force
 * that operation to happen.
 * Second check is if the user name, access type, and recovery type are
 * the same. If not, then we will return 0 to force a new XA database thread,
 * otherwise we return 1 indicating we can utilize an existing open XA
 * database thread if its available.
 *
 * Note: We do not check the character set, storage area, schema name, or
 * version name here. They are checked in the conditioning portion of the
 * database thread establishment process.
 */
int compareXA_savedThreadInfo(char * userNamePtr, int access_type, int rec_option){

#if XA_THREAD_REUSE_TESTING
    if ((getClientCount()+1) == 6){
        /* Test 4, change the recovery option value */
        rec_option = 7777;
        if (debugInternal){
            tracef("In compareXA_savedThreadInfo(), doing thread reuse test for client 6, rec_option now = %d\n", rec_option);
        }
    } else if ((getClientCount()+1) == 7){
        /* Test 5, change the access mode option value */
        access_type = 7777;
        if (debugInternal){
            tracef("In compareXA_savedThreadInfo(), doing thread reuse test for client 7, access_type now = %d\n", access_type);
        }
    }
#endif

    if (sgd.saved_beginThreadTaskCode == XA_BEGIN_THREAD_TASK && access_type == sgd.saved_XAaccessType &&
        rec_option == sgd.saved_XArecovery && strcmp(userNamePtr, sgd.saved_client_Userid) == 0){
        return 1; /* True, compatible, we could re-use an XA database connection. */
    } else {
        return 0; /* False, not compatible, couldn't reuse an XA database connection. */
    }
}

/*
 * Function compareXA_savedCharSetInfo
 *
 * Tests if the input parameter matches the saved charSet value
 * from the last client user. Returns 1 if they match and 0 if not.
 */
int compareXA_savedCharSetInfo(char * charSetPtr){
    return (strcmp(charSetPtr, sgd.saved_charSet) == 0) ? 1 : 0;
}

/*
 * Function compareXA_savedStorageAreaInfo
 *
 * Tests if the input parameter matches the saved storage area value
 * from the last client user. Returns 1 if they match and 0 if not.
 */
int compareXA_savedStorageAreaInfo(char * storageAreaPtr){
    return (strcmp(storageAreaPtr, sgd.saved_storageAreaName) == 0) ? 1 : 0;
}

/* Return the task code from the last begin thread task. */
int getSaved_beginThreadTaskCode(){
    return sgd.saved_beginThreadTaskCode;
}

/* Return the USE command flag from the saved thread usage. */
int getSaved_processedUseCommandFlag(){
    return sgd.saved_processedUseCommand;
}

/* Return the SET command flag from the saved thread usage. */
int getSaved_processedSetCommandFlag(){
    return sgd.saved_processedSetCommand;
}

/* Return the rsaDump flag from the saved thread usage. */
int getSaved_rsaDump(){
    return sgd.saved_rsaDump;
}

/* Return the debugSQLE flag from the saved thread usage. */
int getSaved_debugSQLE(){
    return sgd.saved_debugSQLE;
}

/* Return a reference to the schema name from the saved thread usage. */
char * getSaved_schemaName(){
    return sgd.saved_schemaName;
}

/* Return a reference to the version name from the saved thread usage. */
char * getSaved_versionName(){
    return sgd.saved_versionName;
}

/* Return the varchar flag from the saved thread usage. */
int getSaved_varchar(){
    return sgd.saved_varcharInt;
}

/* Return the emptyStringProp flag from the saved thread usage. */
int getSaved_emptyStringProp(){
    return sgd.saved_emptyStringProp;
}

/* Return the skipGeneratedType value from the saved thread usage. */
int getSaved_skipGeneratedType(){
    return sgd.saved_skipGeneratedType;
}

/* Return the clientCount from the SGD. */
int getClientCount(){
    return sgd.clientCount;
}

/* Return the XA reuse counter from the SGD. */
int getXAreuseCount(){
    return sgd.XA_thread_reuses_so_far;
}

/* Return the tx_info()'s trx_mode from the SGD. */
int getTrx_mode(){
    return sgd.trx_mode;
}

/* Return the tx_info()'s transaction_state from the SGD. */
int getTransaction_state(){
    return sgd.transaction_state;
}

/*
 * Sets or clears the the saved thread's processed a USE command flag when we are in XA Server.
 *
 * Parameter:
 *  flag - A value of 0 indicates we are resetting the flag (done during doBeginThread()
 *         processing). A value of 1 indicates we processed a USE command during this
 *         database thread usage.
 */
void processedUseCommand(int flag){
    sgd.saved_processedUseCommand = flag;
}

/*
 * Sets or clears the the saved thread's processed a SET command flag when we are in XA Server.
 *
 * Parameter:
 *  flag - A value of 0 indicates we are resetting the flag (done during doBeginThread()
 *         processing). A value of 1 indicates we processed a SET command during this
 *         database thread usage.
 */
void processedSetCommand(int flag){
    sgd.saved_processedSetCommand = flag;
}

void dumpSavedBTinfo_AndTrxState(){

    int transaction_state;
    int trx_mode;

    /* Get current tx_info() values */
    trx_mode = get_tx_info(&transaction_state);

    /*
     * Trace the saved Begin Thread information from previous database connection
     * and the current relevant tx_info() values.
     */
    tracef("Saved begin thread arguments from last database thread usage, and current tx_info values:\n");
    if (strlen(sgd.saved_client_Userid) == 0) {
        /* No userid means we haven't actually got a database thread yet; what we have is the dummy initialization value. */
        tracef("sgd.saved_beginThreadTaskCode = %s (%d) <- initialization value, no previous database threads were opened\n",
               taskCodeString(sgd.saved_beginThreadTaskCode), sgd.saved_beginThreadTaskCode);
    } else {
        /* Userid is present, so it is a real previous saved set of values */
        tracef("sgd.saved_beginThreadTaskCode = %s (%d)\n", taskCodeString(sgd.saved_beginThreadTaskCode), sgd.saved_beginThreadTaskCode);
    }
    tracef("sgd.saved_XAaccessType        = %d\n", sgd.saved_XAaccessType);
    tracef("sgd.saved_XArecovery          = %d\n", sgd.saved_XArecovery);
    tracef("sgd.saved_rsaDump             = %d\n", sgd.saved_rsaDump);
    tracef("sgd.saved_debugSQLE           = %d\n", sgd.saved_debugSQLE);
    tracef("sgd.saved_client_Userid       = %s\n", sgd.saved_client_Userid);
    tracef("sgd.saved_processedUseCommand = %d\n", sgd.saved_processedUseCommand);
    tracef("sgd.saved_processedSetCommand = %d\n", sgd.saved_processedSetCommand);
    tracef("sgd.saved_varcharInt          = %d\n", sgd.saved_varcharInt);
    tracef("sgd.saved_emptyStringProp     = %d\n", sgd.saved_emptyStringProp);
    tracef("sgd.saved_skipGeneratedType   = %d\n", sgd.saved_skipGeneratedType);
    tracefuu("sgd.saved_schemaName          = %s\n", sgd.saved_schemaName);
    tracef("sgd.saved_versionName         = %s\n", sgd.saved_versionName);
    tracef("sgd.saved_charSet             = %s\n", sgd.saved_charSet);
    tracef("sgd.saved_storageAreaName     = %s\n", sgd.saved_storageAreaName);
    tracef("sgd.XA_thread_reuses_so_far   = %d\n", sgd.XA_thread_reuses_so_far);
    tracef("trx_mode                      = %d\n", trx_mode);
    tracef("transaction_state             = %d\n\n", transaction_state);

}

/**
 * bumpXAclientCount
 *
 * This function increments sgd.clientCount by 1, and updates
 * the count of non-XA (stateful) or XA database threads opened,
 * and XA database thread reuses, where appropriate.
 */
void bumpXAclientCount(int taskCode, int reuse_xa_thread) {

    /* Bump total client count. */
    sgd.clientCount ++;

    /* Bump count of non-XA/XA thread usage and reuse count if necessary. */
    if (taskCode == XA_BEGIN_THREAD_TASK){
        /* An XA database thread usage. */
        sgd.totalXAthreads++;
        /* Are we reusing an already opened XA database thread? */
        if (reuse_xa_thread){
            sgd.totalXAReuses++;
        }
    } else {
        /* Must be the non-XA (stateful) BEGIN_THREAD_TASK usage. */
        sgd.totalStatefulThreads++;
    }
} /* bumpXAclientCount */

/*
 * Function reset_xa_reuse_counter
 *
 * Resets the SGD's XA_thread_reuses_so_far field back to 0.
 */
void reset_xa_reuse_counter(){
    sgd.XA_thread_reuses_so_far = 0;
}

/*
 * Function is_xa_reuse_counter_at_limit
 *
 * This function is called by doBeginThread() routine to determine if
 * there are any reuses of the existing XA database thread left based
 * on the JDBC XA Server's configuration maximum XA thread reuse limit.
 */
int is_xa_reuse_counter_at_limit(){
    return (sgd.XA_thread_reuses_so_far >= sgd.XA_thread_reuse_limit);
}
#endif          /* XA JDBC Server */

/*
 * Function addServerInstanceInfoArgument()
 *
 * This function adds the Server Instance Information as a 14-byte
 * array argument to the response packet at the current offset.
 *
 * The three SGD fields usageTypeOfServer_serverStartDate,
 * serverStartMilliseconds, and generatedRunID (minus any extra
 * 7th and 8th trailing null bytes) comprising a 14-byte Server's
 * instance identification.
 *
 * No conversions of the values in the byte array will be done on
 * the server (i.e., the bytes are treated as sequences of bits,
 * not as integral values). Don't assume the C input value has a
 * NUL terminator.
 *
 * @param buffer
 *   A pointer to the byte array where the value is inserted.
 *
 * @param bufferIndex
 *   A pointer to the offset in the byte array where the value is
 *   inserted. The offset is incremented so that it indexes past
 *   the inserted descriptor and value.

 */
void addServerInstanceInfoArgument(char * buffer, int * bufferIndex){

    /* Add the Server Instance identification bytes to buffer. */
    addArgDescriptor(BYTE_ARRAY_TYPE, REQ_SERVERINSTANCE_IDENT_LENGTH, buffer, bufferIndex);
    putByteArrayIntoBytes(sgd.usageTypeOfServer_serverStartDate, REQ_SERVERINSTANCE_IDENT_LENGTH, buffer, bufferIndex);
}

/*
 * Procedure setServerUsageType
 *
 * This function sets the Server instances usage type field.
 */
void setServerUsageType(char serverUsageType){

    sgd.usageTypeOfServer_serverStartDate[0] = serverUsageType;
}

/**
 * Function getServerInstanceInfoArgSize
 *
 * Returns the size in bytes of a ServerInstanceInfo
 * argument added by the ServerInstanceInfoArg()
 * procedure. Used primarily in precomputing the
 * needed size of a response packet.
 */
int getServerInstanceInfoArgSize(){
    return DESCRIPTOR_LEN + REQ_SERVERINSTANCE_IDENT_LENGTH;
}

/**
 * Function testServerInstanceInfo
 *
 * Tests the current Server's instance information
 * against the instance information pointed to by
 * the parameter argument to see if they are compatible
 * (i.e., the client request can be processed by the
 * current JDBC Server or JDBC XA Server.
 *
 * Parameters:
 *   instanceInfoPtr - Pointer to a server instance
 *                     information that's to be compared.
 *                     This usually points to the information
 *                     from the client's request packet. Note:
 *                     only its first 14 bytes are referenced
 *                     (two trailing null bytes are added
 *                     after the grunid field before testing
 *                     against Server's instance info).
 *
 * Returns:
 *   0 (false)       - If the instance information is compatible.
 *   1 (true)        - If the instance information is not compatible.
 */
int testServerInstanceInfo(char * instanceInfoPtr){

    serverInstanceIdent_struct testInstanceIdent;
    serverInstanceIdent_struct * serverInstanceIdent;

    /*
     * Copy the test server instance information into a
     * word-aligned structure.  Clear to 0 (nulls) the last
     * (13th-16th) bytes of the testInstanceIdent structure
     * before the copy so that a six character generated
     * runid value will always have a trailing null and
     * that testInstanceIdent (16 bytes) can be tested as
     * 4 word-aligned ints.
     */
    testInstanceIdent.identInt[3] = 0;
    memcpy(&testInstanceIdent, instanceInfoPtr, REQ_SERVERINSTANCE_IDENT_LENGTH);

    /* For easy testing, use a pointer to SGD's server instance info. */
    serverInstanceIdent = (serverInstanceIdent_struct *)&(sgd.usageTypeOfServer_serverStartDate);

    /* OK, are the test and Server's instance info entries the same? */
    if (testInstanceIdent.identInt[0] == serverInstanceIdent->identInt[0] &&
        testInstanceIdent.identInt[1] == serverInstanceIdent->identInt[1] &&
        testInstanceIdent.identInt[2] == serverInstanceIdent->identInt[2] &&
        testInstanceIdent.identInt[3] == serverInstanceIdent->identInt[3]){
            /* Yes, they match, we are back to the same server as before. */
            return 0; /* false */
    }

    /* No, they didn't match, we are not back to the same server as before. */
    return 1; /* true */
}

/**
 * Function setServerInstanceDateandTime
 *
 * This function sets the Server instance's date and time information.
 */
void setServerInstanceDateandTime(){

    int startTimeMilliseconds;
    int milliseconds;
    int hours;
    int minutes;
    int seconds;
    int year;
    int month;
    int day;
    int index = 0;
    char dateTimeString[20]; /* Enough space for the 17 chars returned by UCSCNVTIM$CC() */

    /*
     * Get start date/time in calendar and milliseconds since midnight
     * formats and place it into the Server's indentification information.
     */
    UCSDWTIME$(&sgd.serverStartTimeValue);

    /* Have UCSCNVTIM$CC() convert server start time into format: YYYYMMDDHHMMSSmmm */
    UCSCNVTIM$CC(&sgd.serverStartTimeValue, dateTimeString);

    /* Now extract the date-time information from the UCSDWTIME$() call. */
    sscanf(dateTimeString, "%4d%2d%2d%2d%2d%2d%3d", &year, &month, &day, &hours, &minutes, &seconds, &milliseconds);

    /* Now compute start time in milliseconds from midnight. */
    startTimeMilliseconds = (((((hours * 60) + minutes) * 60) + seconds) *1000) + milliseconds;

    /* Place date and time fields into the SGD and Server's indentification information. */
    putIntIntoBytes(startTimeMilliseconds, &sgd.serverStartMilliseconds[0], &index);
    sgd.usageTypeOfServer_serverStartDate[1] = year - 2000; /* keep only the 2-digit year (YY) */
    sgd.usageTypeOfServer_serverStartDate[2] = month;
    sgd.usageTypeOfServer_serverStartDate[3] = day;
}

/*
 * Function displayableServerInstanceIdent
 *
 * Function returns a displayable version of the Server Instance
 * Identification information.
 *
 * Parameter:
 *   displayString:      A pointer to a provided space allocate
 *                       large enough space to hold the constructed
 *                       Server Instance Identification information
 *                       and anything else the caller may want to
 *                       add after it - SERVER_ERROR_MESSAGE_BUFFER_LENGTH
 *                       is the char size used by the calling routines.
 *   instanceInfoPtr:    A pointer to the server instance
 *                       information to format. If not null,
 *                       this would likely point to the client
 *                       provided (in a request packet) server
 *                       instance information. If null, the
 *                       actual server instance information of
 *                       the current server is returned.
 *   withBytes:          Indicates whether the display string
 *                       should include a byte by byte display.
 *                       If 1 (true) the byte display will be
 *                       added. If 0 (false) the byte display
 *                       will not be added.
 * @return
 *   A pointer to the char array where the display string is placed.
 *
 * Note: The caller will need to free the malloc'ed space pointed
 *       to by the returned pointer value.
 *
 */
void displayableServerInstanceIdent(char * displayString, char * instanceInfoPtr, int withBytes){

    int milliseconds;
    int millisecs;
    int itsecs;
    int seconds;
    int minutes;
    int hours;
    int index = 0;
    serverInstanceIdent_struct * infoPtr;

    /*
    * For convenience, reference the desired server instance info
    * using structured references. If the instanceInfoPtr is null,
    * then point to the Server's own server instance information.
    */
    if (instanceInfoPtr != NULL){
        infoPtr = (serverInstanceIdent_struct *)instanceInfoPtr;
    } else {
        infoPtr = (serverInstanceIdent_struct *)&sgd.usageTypeOfServer_serverStartDate;
    }

    /* Convert the server start time (in milliseconds) into the HH:MM:SS.sss pieces */
    milliseconds = getIntFromBytes(&infoPtr->ident.serverStartMilliseconds[0], &index);
    millisecs = milliseconds % 1000;   /* The given millisecs in the given second */
    itsecs = milliseconds / 1000;      /* The given time, in whole seconds past midnight */
    seconds = itsecs%60;               /* The number of seconds in the given minute */
    minutes = (itsecs/60)%60;          /* The number of minutes in the given hour */
    hours =(itsecs/3600);              /* The given hour (usually < 24 since it's current day) */

    /*
     * Form the output display string, e.g., "YYYYMMDD HH:MM:SS.sss".
     *
     * Should it also include the byte display?
     */
    if (withBytes){
        /* Yes, display the values and display their byte values also. */
        sprintf(displayString,"%s, 20%02d%02d%02d %02d%:%02d:%02d.%03d, %s  (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
                usageTypeOfServerString(infoPtr->identChar[0]), infoPtr->ident.usageTypeOfServer_serverStartDate[1],
                infoPtr->ident.usageTypeOfServer_serverStartDate[2], infoPtr->ident.usageTypeOfServer_serverStartDate[3],
                hours, minutes, seconds, millisecs, infoPtr->ident.generatedRunID,
                infoPtr->identChar[0], infoPtr->identChar[1], infoPtr->identChar[2],
                infoPtr->identChar[3], infoPtr->identChar[4], infoPtr->identChar[5],
                infoPtr->identChar[6], infoPtr->identChar[7], infoPtr->identChar[8],
                infoPtr->identChar[9], infoPtr->identChar[10], infoPtr->identChar[11],
                infoPtr->identChar[12], infoPtr->identChar[13]);
    } else {
        /* No, just display the values without their byte display. */
        sprintf(displayString,"%s, 20%02d%02d%02d %02d%:%02d:%02d.%03d, %s",
                usageTypeOfServerString(infoPtr->identChar[0]), infoPtr->ident.usageTypeOfServer_serverStartDate[1],
                infoPtr->ident.usageTypeOfServer_serverStartDate[2], infoPtr->ident.usageTypeOfServer_serverStartDate[3],
                hours, minutes, seconds, millisecs, infoPtr->ident.generatedRunID);
    }
}

/*
 * Function getWorkerActivityID
 *
 * This function returns the current worker's 2200
 * activity id, obtained from the WDE.
 */
int getWorkerActivityID(){
    return act_wdePtr->serverWorkerActivityID;
}

/*
 * Function getWorkerUniqueActivityID
 *
 * This function returns the current worker's 2200
 * unique activity id, obtained from the WDE.
 */
long long getWorkerUniqueActivityID(){
    return act_wdePtr->serverWorkerUniqueActivityID;
}

/*
 * Procedure getRunID
 *
 * This function copies the current run's 2200 run id
 * string, obtained from the SGD.
 *
 * Parameter:
 *
 * runIDPtr - pointer to a character array large enough
 *            to hold the run id string followed by a
 *            trailing null. Character array must be
 *            RUNID_CHAR_SIZE+1 in size.
 *
 */
void getRunID(char * runIDPtr){

    /* If we don't already have it, go get it. */
    if (strlen(sgd.originalRunID) == 0){
        getRunIDs();
    }
    strcpy(runIDPtr, sgd.originalRunID);
    return;
}

/*
 * Procedure getGeneratedRunID
 *
 * This function copies the current run's 2200 generated
 * run id string, obtained from the SGD.
 *
 * Parameter:
 *
 * generatedRunIDPtr - pointer to a character array large enough
 *                    to hold the generated run id string followed
 *                    by a trailing null. Character array must be
 *                    RUNID_CHAR_SIZE+1 in size.
 *
 */
void getGeneratedRunID(char * generatedRunIDPtr){

    /* If we don't already have it, go get it. */
    if (strlen(sgd.generatedRunID) == 0){
        getRunIDs();
    }
    strcpy(generatedRunIDPtr, sgd.generatedRunID);
    return;
}

/**
 * getRunIDs
 *
 * Set sgd.originalRunID with the run's original run ID.
 * The fieldata value is in word 0 (i.e., the first word)
 * of the PCT.
 *
 * Set sgd.generatedRunID with the run's generated run ID.
 * The fieldata value is in word 1 (i.e., the second word)
 * of the PCT.
 */

#define PCT_ORIG_RUN_ID_POS 0
#define PCT_GEN_RUN_ID_POS 1

void getRunIDs() {

 int nbrWords = PCT_GEN_RUN_ID_POS + 1;
    int start = 0;
    int buffer[PCT_GEN_RUN_ID_POS+1];
    int fieldataOrigRunID;
    int fieldataGenRunID;
    char * ptr1;

    /* If we already have them no need to retrieve them again. */
    if (strlen(sgd.originalRunID) != 0){
        return;
    }

    /* ER PCT$ to get the first two words of the PCT */
    ucspct(&nbrWords, &start, buffer);

    /* Original run ID */

    /* Get the original run ID in fieldata (6 chars) */
    fieldataOrigRunID = buffer[PCT_ORIG_RUN_ID_POS];

    /* printf("fd orig run id = %12o\n", fieldataOrigRunID); */

    /* Convert the ID from fieldata to ASCII */
    fdasc(sgd.originalRunID, &fieldataOrigRunID, RUNID_CHAR_SIZE);

    /* If the ID is less than 6 chars, find the first space */
    ptr1 = (char*) memchr(sgd.originalRunID, ' ', RUNID_CHAR_SIZE);

    /* NUL-terminate the string */
    if (ptr1 == NULL) {
        sgd.originalRunID[RUNID_CHAR_SIZE] = '\0';
    } else {
        *ptr1 = '\0';
    }

#if DEBUG
    printf("ascii original run id = '%s'\n", sgd.originalRunID);
#endif

    /* Generated run ID */

    /* Get the generated run ID in fieldata (6 chars) */
    fieldataGenRunID = buffer[PCT_GEN_RUN_ID_POS];

    /* printf("fd generated run id = %12o\n", fieldataGenRunID); */

    /* Convert the ID from fieldata to ASCII */
    fdasc(sgd.generatedRunID, &fieldataGenRunID, RUNID_CHAR_SIZE);

    /* If the ID is less than 6 chars, find the first space */
    ptr1 = (char*) memchr(sgd.generatedRunID, ' ', RUNID_CHAR_SIZE);

    /* NUL-terminate the string */
    if (ptr1 == NULL) {
        sgd.generatedRunID[RUNID_CHAR_SIZE] = '\0';
    } else {
        *ptr1 = '\0';
    }

#if DEBUG
    printf("ascii generated run id = '%s'\n", sgd.generatedRunID);
#endif

} /* getServerUserID */

/**
 * getServerUserID
 *
 * Set sgd.serverUserID with the userID used to start the Server.
 * This value is retrieved using the ucsinfo() function, which
 * uses the EXEC INFO$ ER.  The user-id is retrieved in argument
 * group 1.
 */

void getServerUserID() {

    int function = 1;   /* Set the argument group function code for user-id retrieval */
    int status;
    char * ptr1;

    /* If we already have it no need to retrieve it again. */
    if (strlen(sgd.serverUserID) != 0){
        return;
    }

    /* INFO$ ER to get the userid used to start the Server. */
    ucsinfo(&function, &status, sgd.serverUserID);

    /* If the ID is less than 12 chars, find the first space */
    ptr1 = (char*) memchr(sgd.serverUserID, ' ', MAX_USERID_LEN);

    /* NUL-terminate the string */
    if (ptr1 == NULL) {
        sgd.serverUserID[MAX_USERID_LEN] = '\0';
    } else {
        *ptr1 = '\0';
    }

    /* Check for possible error in ER call and dump status message, but continue. */
    if (status != 0) {
        printf("In getServerUserID(), sgd.serverUserID = '%s', status = %i, \n", sgd.serverUserID, status);
    }

} /* getServerUserID */

/*
 * Function get_appGroupNumber
 *
 * This function returns the JDBC Server's application group number.
 */
int get_appGroupNumber(){

    /* printf("sgd.appGroupNumber = %d\n", sgd.appGroupNumber); */

    /*
     * Return this JDBC Client's global transactional
     * state value from its WDE.
     */
    return sgd.appGroupNumber;
}

/*
 * Function get_uds_icr_bdi
 *
 * This function returns the UDS application group's ICR bdi.
 */
int get_uds_icr_bdi(){

    /* printf("sgd.appGroupNumber = %d\n", sgd.appGroupNumber); */

    /*
     * Return this JDBC Client's global transactional
     * state value from its WDE.
     */
    return sgd.uds_icr_bdi;
}

/*
 * Function do_RSA_DebugDumpAll()
 *
 * This function returns either a BOOL_FALSE (0) or BOOL_TRUE (1)
 * depending on whether the clients debugFlags value indicates an
 * RSA DEBUG DUMP ALL command should be issued.
 */
int do_RSA_DebugDumpAll(){


    return ((act_wdePtr->debugFlags & DEBUG_RSA_DUMPALLMASK)>0)?BOOL_TRUE:BOOL_FALSE;
}

/*
 * Function retrieve_RDMS_level_info
 *
 * This function sets the sgd with the RDMS level information.  It is assumed
 * that an open RDMS Thread is available and on this thread a LEVEL RDMS
 * command can be issued. If the RDMS level has already been set, this function
 * just returns (i.e., we only need to issue the LEVEL RDMS command only
 * once per server execution).
 *
 * As part of this function, the retrieved RDMS level is checked to determine if
 * it is adequate for the JDBC Server or XA JDBC Server usage.  If not, then the
 * JDBC error message number is returned. The caller must take any needed actions.
 *
 * This routine also sets the "Feature Flags" which define if selected features
 * are available or not.  This is used for the multi-RDMS level support.  The
 * Feature Flags (FF_xxxx) are defined in the 'sgd' data structure.
 *
 * Returned value:
 *
 *    0 - LEVEL RDMS command was successfully executed and an acceptable RDMS has
 *        its level info set in the SGD.
 *
 *  !=0 - Error occurred retrieving the RDMS level information or the RDMS level
 *        is not adequate for the JDBC Server or XA JDBC Server. The error situation
 *        is logged in the Server's log file, and the error message code and error
 *        status is returned to the caller so they can issue any appropriate error
 *        message (e.g. in an error response packet back to a JDBC client, logging the
 *        event, or terminating the server.)
 *        The returned value is the JDBC error message number.
 */
int retrieve_RDMS_level_info() {

    int error_status;
    rsa_error_code_type errorCode;
    int auxinfo;
    int x;
    int len;
    int majorlevel;
    int featurelevel;
    int level3;
    int level4;
    int level5;
    char p[MAX_RDMS_LEVEL_LEN+1];
    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */

    /* Test if we already have successfully retrieved the RDMS level, if so just return. */
    if (sgd.rdmsBaseLevel != 0) {
         return 0;
    }

    /* Now get the RDMS level */
    act_wdePtr->in_RDMS = CALLING_RDMS;
    rsa("LEVEL RDMS INTO $P1;", errorCode,&auxinfo,p,&x);
    act_wdePtr->in_RDMS = NOT_CALLING_RDMS;                          \
    error_status = atoi(errorCode);

    /* Test the error status - did we get the RDMS level or did command fail? */
    if (error_status != 0) {
        /* Error encountered attempting to retrieve RDMS level, clear sgd's RDMS level info and log the error. */
        sgd.rdmsLevelID[0]='\0'; /* clear it out just in case */
        /* 5374 {0} received RDMS error code {1} attempting to retrieve RDMS Level. */
        getLocalizedMessageServer(SERVER_CANT_RETRIEVE_RDMS_LEVEL, SERVERTYPENAME, errorCode, SERVER_LOGS, msgBuffer);
        return SERVER_CANT_RETRIEVE_RDMS_LEVEL;
    }

    /*
     * Verify the RDMS level is adequate. It must be RDMS 17R1 or
     * higher. Note that the returned level string is in the form:
     * bb-ff-ss-xxxx-yyyy , where bb is the base level, ff is the feature level,
     * and xxxx and yyyy could either be a mainline number or a mainline-build number.
     * We only need to test the base level and feature level numbers - the
     * mainline and build numbers if present will be ignored.
     *
     * Note: If this is an RDMS development level, RDMS sets the feature level
     * field to 0 and specifes the feature level in the third field (i.e.,
     * the returned string is of the form: bb-0-ff-yyyy).
     *
     * First parse the RSA/RDMS level retrieved.  Remember
     * that the level id starts after RDMS_LEVEL_INDENT characters since the
     * string is preceded by "RDMS LEVEL ".
     */
    sscanf(p,"RDMS LEVEL %d%*[-]%d%*[-]%d%*[-]%d%*[-]%d ", &majorlevel,
           &featurelevel, &level3, &level4, &level5);
    if (debugInternal) {
        tracef("RDMS Level: %s\n", p);
    }

    /* Save the RDMS level id string in the SGD, copying only up to
     * the MAX_RDMS_LEVEL_LEN number of characters.
     */
    len=strcspn(p+RDMS_LEVEL_INDENT," ");
    strncpy(sgd.rdmsLevelID, p+RDMS_LEVEL_INDENT, MIN(len,MAX_RDMS_LEVEL_LEN));

    /*
     * If the RDMS feature level is zero, then this is a RDMS
     * development level and level3 indicates the true feature level.
     */
    if (featurelevel == 0){
         /* Set feature level to level 3 value */
         featurelevel = level3;
    }

    /* Now test if it is adequate for our JDBC Server usage. */
    if ( (majorlevel < MINIMAL_RDMS_BASE_LEVEL) ||
         ((majorlevel == MINIMAL_RDMS_BASE_LEVEL)
          &&(featurelevel < MINIMAL_RDMS_FEATURE_LEVEL))){
        /* RDMS level is not adequate for JDBC Server to run, generate a Server log message. */
        /* 5375 {0} cannot operate with {1}, RDMS Level 17R1 or higher is needed */
        getLocalizedMessageServer(SERVER_DOESNT_HAVE_ADEQUATE_RDMS_LEVEL, SERVERTYPENAME, sgd.rdmsLevelID, SERVER_LOGS, msgBuffer);
        return SERVER_DOESNT_HAVE_ADEQUATE_RDMS_LEVEL; /* Return error message code */
    }

    /*
     * RDMS level is adequate. Save the major level (base)
     * and minor level (feature).
     */
    sgd.rdmsBaseLevel = majorlevel;
    sgd.rdmsFeatureLevel = featurelevel;

    /* Now that we have the RDMS level and it meets the minimum supported level, we need to set
     * the Feature Flags in the SGD.  Each of the flags needs to be set, since they are
     * not initialized. Set the base level for each flag assuming the current level, then set exceptions.
     */
    sgd.FFlag_SupportsSQLSection = 1;  /* supported, get/use RDMS SQL sections if available */
    sgd.FFlag_SupportsFetchToBuffer = 1;  /* supported, used the multi-record fetch api */

    /* Must be RDMS 17R2 or higher to support SQL Sections */
    if ((majorlevel < RDMS_BASE_LEVEL_17) ||
        (majorlevel == RDMS_BASE_LEVEL_17 && featurelevel < RDMS_FEATURE_LEVEL_2)){
        sgd.FFlag_SupportsSQLSection = 0;  /* not supported, use actual SQL command */
    }

    /* Must be RDMS 19R1 or higher to support Fetch to Buffer */
    if ((majorlevel < RDMS_BASE_LEVEL_19)){
        sgd.FFlag_SupportsFetchToBuffer = 0;  /* not supported, use FETCH commands */
    }

    return 0; /* We got acceptable RDMS Level data in the SGD. */
}

/*
 * Function addServerLogMessage
 *
 * Adds a message to the log file. The caller
 * passes a character string, which is sent to
 * the Server Log (TO_SERVER_LOGFILE). Upon
 * return, the caller must free any storage
 * associate to the character string if
 * necessary.
 */
 void addServerLogMessage(char * msgPtr){

    /* add Server log entry */
    addServerLogEntry(TO_SERVER_LOGFILE, msgPtr);
    return;
 }

/*
 * Function tracef
 *
 * Function tracef produces formatted write to the client's trace file.
 * The debugPrefix indentation string is placed before each new line,
 * before the output characters so that the trace file has the same
 * appearance style as that generated on the JDBC client side for this
 * connection. This is possible since this routine is each activity has
 * its own trace file and there is no concurrent usage of this routine
 * and the debugPrefix information.
 *
 * Function tracef can handle a string up to MAX_JDBC_TRACE_BUFFER_SIZE
 * in length.  This function does not check if the string is too large.
 * A string that is too large can result in an IGDM as the buffer that
 * is used (array work) is on the ALS and no checks are done to guard
 * against overflow.  This is the same as the UC version of this routine.
 *
 * This code is based on the code in the UC compiler for the printf
 * function ( see UC source file UCSxxxx*SRVCBASE$$.UC-PRINTF, where
 * xxxx is the compiler base.  For this code we used the 12R1 base.)
 *
 * Although the code was based on printf, this code will actually use
 * fprintf to do the output. The caller uses a function call that
 * was converted from a printf format into the tracef format, which
 * will direct the output to the client trace file automatically without
 * the coder having to indicate the file handle, hence this code is
 * based on the printf parameter sequence.
 *
 * Note: Obviously the function name * was changed from "printf" to
 * "tracef". All other changes/extensions made to that code are
 * preceded/ended by a comment with the text "Addition for JDBC Server:"
 * and "End change for JDBC Server".
 *
 * Parameters:
 *     *cs - pointer to the format string. Same format as that used in
 *           the printf() or fprinf() functions.
 *     ... - represents the parameter arguments needed for producing
 *           the output. Same format as printf() or fprintf(), its a
 *           variable number of parameters.
 *
 */
int tracef(const char *cs, ...)
{
int i, c, n;
int num_skipped_parms = 1;
char *p, *out, *_pfmt();
char format_type;
int  format_len;
va_list ap;
/* Addition for JDBC Server: Replace UC URTS constant 32767 with define name MAX_JDBC_TRACE_BUFFER_SIZE.
   Beware function _pfmt (called by this function) moves strings to this buffer (work) and does not check
   for buffer overflow so IGDM can occur for buffer overflow.
   Note tracefu and tracefuu truncate strings > MAX_JDBC_TRACE_BUFFER_SIZE before calling tracef.
*/
int using_clientFile_or_serverFile; /* 0 means trace to client trace file or stdout, 1 means trace to Server's JDBC$TRACE file. */
long long timeValue;
char timeBuffer[30];
char timeString[30];
char work[MAX_JDBC_TRACE_BUFFER_SIZE];
/* End change for JDBC Server */
/* Addition for JDBC Server: Use clientTraceFile as output file. */
FILE *so;
so = act_wdePtr->clientTraceFile;
using_clientFile_or_serverFile = 0; /* Assume we'll use a client's trace file. */
/* End change for JDBC Server */
/**  **/
va_start(ap, cs);
n =  0;
/*
 * Addition for JDBC Server and JDBC XA Server:
 *
 * This code will output the debugPrefix before the rest of the
 * trace image is sent based on the debugPrefixFlag. If flag is
 * set, debugPrefix is output first.  If flag is not set, then
 * it is assumed we are within a trace image and no debugPrefix
 * will be added until a "\n" is detected. If the debugTstamp is
 * set, then a timestamp value will be computed and printed before
 * the debugPrefix.
 *
 * If the client trace file is not available, i.e. clientTraceFile
 * is equal to NULL, then output will be sent to the JDBC Server or
 * JDBC XA Server's JDBC$TRACE file and if that file is not available
 * then regular STDOUT will be used. Note: This should only occur when
 * tracing is produced while a JDBC XA Server is cleaning up between
 * clients and the previous client had tracing turned on, or when
 * the output was sent to tracef() by a compile time
 * activated statement (i.e. if (DEBUG) {tracef("xxx xx x");})
 * since only under these conditions could the trace file not be
 * already open when using a debugXXXXX indicator.
 *
 * The decision to use STDOUT is done so trace output is not lost,
 * since the client receives an SQLException if the trace file could
 * not be opened ( and hence the use of STDOUT here).
 *
 */
if ((so == NULL) && (sgd.serverTraceFile != NULL)){
    /*
     * No client trace file handle and JDBC$TRACE is
     * available, so send the output to the JDBC$TRACE file.
     */
    so = sgd.serverTraceFile;
    using_clientFile_or_serverFile = 1; /* Using Server's JDBC$TRACE file. */
} else if ((so == NULL) || (so == stdout)){
    /*
     * No client trace file handle (and possibly JDBC$TRACE was
     * not available), so use standard out (print$). Of course, if
     * the client trace file handle may already be set to stdout.
     * Either way, we will use stdout. We will use the Server's Client
     * Trace File T/S to synchronize this entire tracef with other
     * activities that may be doing tracing to either the potentially
     * the same client trace file or to stdout.
     */
    so=stdout;
    using_clientFile_or_serverFile = 0; /* Using a client's trace file or stdout. */
}

/* So which T/S cell do we need to use to control file write access? */
if (using_clientFile_or_serverFile == 0){
    /*
     * It's going to a client's trace file or standard out.
     * To avoid possible concurrency issues with the client's trace file or when
     * when we need to use standard out, we will use the Server's Client Trace
     * File T/S to synchronize this entire tracef with other activities that may
     * be doing the same. The T/S will be released before returning from tracef().
     */
    test_set(&(sgd.clientTraceFileTS));
} else {
    /*
     * It's going to the Server's JDBC$TRACE file.
     * To avoid possible concurrency issues with JDBC$TRACE, we will use the
     * Server's JDBC$TRACE T/S to synchronize output since there may be other
     * Server activities (not just ServerWorker activities) producing Server
     * related tracing.
     */
    test_set(&(sgd.serverTraceFileTS));
}
/* End change for JDBC Server */

/* Test if we need the current timestamp value */
if (debugTstampFlag == BOOL_TRUE){
  /*
   * UCSCNVTIM$CC() returns date time in form : YYYYMMDDHHMMSSmmm . So,
   * Form time string of format: HH:MM:SS.SSS
   */
  UCSDWTIME$(&timeValue);
  UCSCNVTIM$CC(&timeValue, timeBuffer);
  timeBuffer[17]='\0';
  timeString[0]='\0';
  strncat(timeString, &timeBuffer[8],2); /* copy the HH */
  strcat(timeString,":");
  strncat(timeString, &timeBuffer[10],2); /* copy the MM */
  strcat(timeString,":");
  strncat(timeString, &timeBuffer[12],2); /* copy the SS */
  strcat(timeString,".");
  strncat(timeString, &timeBuffer[14],3); /* copy the mmm */
  strcat(timeString," ");
}

/* Addition for JDBC Server: Test if we need to output timestamp or debugPrefix now */
if (debugPrefixOrTstampFlag == BOOL_TRUE){
 if (debugTstampFlag == BOOL_TRUE){
  if (fprintf(so,timeString)< 0) {
        /* Addition for JDBC Server: Release T/S and return EOF on error. */
        if (using_clientFile_or_serverFile == 0){
            ts_clear_act(&(sgd.clientTraceFileTS));
        } else {
            ts_clear_act(&(sgd.serverTraceFileTS));
        }
        return(EOF); /* add timestamp prefix right now, return EOF on error*/
  }
  n=n+TIMESTAMP_PREFIX_STRING_LENGTH;
  debugPrefixOrTstampFlag=BOOL_FALSE;
 }
 if (debugPrefixFlag == BOOL_TRUE){
  if (fprintf(so,debugPrefix)< 0){
        /* Addition for JDBC Server: Release T/S and return EOF on error. */
        if (using_clientFile_or_serverFile == 0){
            ts_clear_act(&(sgd.clientTraceFileTS));
        } else {
            ts_clear_act(&(sgd.serverTraceFileTS));
        }
        return(EOF); /* add debug prefix right now, return EOF on error*/
  }
  n=n+1+REQ_DEBUG_PREFIX_STRING_LENGTH;
  debugPrefixOrTstampFlag=BOOL_FALSE;
 }
}
/* End change for JDBC Server */

while (*cs != '\0')
   {
   c = *cs++;
   if (c == '%')
           {
           p = _pfmt (cs, work, &ap, &n, &format_type, &format_len,
                                                       num_skipped_parms   );
           if (p != NULL)
               {
               cs = p;
               if (format_type == 'c')
                   {
                   out = work;
                   for (i = 1; i <= format_len; i++){
                     /* Addition for JDBC Server: Test if we need to output timestamp or debugPrefix now */
                     if (debugPrefixOrTstampFlag == BOOL_TRUE){
                      if (debugTstampFlag == BOOL_TRUE){
                       if (fprintf(so,timeString)< 0){
                          /* Addition for JDBC Server: Release T/S and return EOF on error. */
                           if (using_clientFile_or_serverFile == 0){
                            ts_clear_act(&(sgd.clientTraceFileTS));
                           } else {
                               ts_clear_act(&(sgd.serverTraceFileTS));
                           }
                          return(EOF); /* add timestamp prefix right now, return EOF on error*/
                       }

                       n=n+TIMESTAMP_PREFIX_STRING_LENGTH;
                       debugPrefixOrTstampFlag=BOOL_FALSE;
                       }
                      if (debugPrefixFlag == BOOL_TRUE){
                       if (fprintf(so,debugPrefix)< 0){
                          /* Addition for JDBC Server: Release T/S and return EOF on error. */
                           if (using_clientFile_or_serverFile == 0){
                            ts_clear_act(&(sgd.clientTraceFileTS));
                           } else {
                               ts_clear_act(&(sgd.serverTraceFileTS));
                           }
                          return(EOF); /* add debug prefix right now, return EOF on error*/
                       }
                       n=n+1+REQ_DEBUG_PREFIX_STRING_LENGTH;
                       debugPrefixOrTstampFlag=BOOL_FALSE;
                      }
                     }
                     c=*out;
                     /* End change for JDBC Server */

                     if (putc (*out++,so) == EOF){
                          /* Addition for JDBC Server: Release T/S and return EOF on error. */
                         if (using_clientFile_or_serverFile == 0){
                            ts_clear_act(&(sgd.clientTraceFileTS));
                         } else {
                             ts_clear_act(&(sgd.serverTraceFileTS));
                         }
                          return(EOF);
                     }

                     /* Addition for JDBC Server: Test if we added a \n, if so set debugPrefixFlag */
                     /* It is OK to test this here since an EOF condition above means we won't print any more */
                     if (c =='\n') debugPrefixOrTstampFlag=BOOL_TRUE;
                     /* End change for JDBC Server */
                     }
                   }
               else
                   {
                    for (out = work; *out != 0; ){

                      /* Addition for JDBC Server: Test if we need to output timestamp or debugPrefix now */
                      if (debugPrefixOrTstampFlag == BOOL_TRUE){
                       if (debugTstampFlag == BOOL_TRUE){
                        if (fprintf(so,timeString)< 0){
                          /* Addition for JDBC Server: Release T/S and return EOF on error. */
                            if (using_clientFile_or_serverFile == 0){
                                ts_clear_act(&(sgd.clientTraceFileTS));
                            } else {
                                ts_clear_act(&(sgd.serverTraceFileTS));
                            }
                          return(EOF);
                        }
                        n=n+TIMESTAMP_PREFIX_STRING_LENGTH;
                        debugPrefixOrTstampFlag=BOOL_FALSE;
                        }
                       if (debugPrefixFlag == BOOL_TRUE){
                        if (fprintf(so,debugPrefix)< 0){
                          /* Addition for JDBC Server: Release T/S and return EOF on error. */
                            if (using_clientFile_or_serverFile == 0){
                                ts_clear_act(&(sgd.clientTraceFileTS));
                            } else {
                                ts_clear_act(&(sgd.serverTraceFileTS));
                            }
                          return(EOF);
                        }
                        n=n+1+REQ_DEBUG_PREFIX_STRING_LENGTH;
                        debugPrefixOrTstampFlag=BOOL_FALSE;
                       }
                      }
                      c=*out;
                      /* End change for JDBC Server */

                      if(putc(*out++,so) == EOF){
                          /* Addition for JDBC Server: Release T/S and return EOF on error. */
                          if (using_clientFile_or_serverFile == 0){
                            ts_clear_act(&(sgd.clientTraceFileTS));
                          } else {
                              ts_clear_act(&(sgd.serverTraceFileTS));
                          }
                          return(EOF);
                      }

                      /* Addition for JDBC Server: Test if we added a \n, if so set debugPrefixFlag */
                      /* It is OK to test this here since an EOF condition above means we won't print any more */
                      if (c =='\n') debugPrefixOrTstampFlag=BOOL_TRUE;
                      /* End change for JDBC Server */
                      }
                    }
               }
           }
   else
       {
        /* Addition for JDBC Server: Test if we need to output timestamp or debugPrefix now */
        if (debugPrefixOrTstampFlag == BOOL_TRUE){
          if (debugTstampFlag == BOOL_TRUE){
            if (fprintf(so,timeString)< 0){
                /* Addition for JDBC Server: Release T/S and return EOF on error. */
                if (using_clientFile_or_serverFile == 0){
                    ts_clear_act(&(sgd.clientTraceFileTS));
                } else {
                    ts_clear_act(&(sgd.serverTraceFileTS));
                }
                return(EOF);
            }
            n=n+TIMESTAMP_PREFIX_STRING_LENGTH;
            debugPrefixOrTstampFlag=BOOL_FALSE;
          }
          if (debugPrefixFlag == BOOL_TRUE){
            if (fprintf(so,debugPrefix)< 0){
                /* Addition for JDBC Server: Release T/S and return EOF on error. */
                if (using_clientFile_or_serverFile == 0){
                    ts_clear_act(&(sgd.clientTraceFileTS));
                } else {
                    ts_clear_act(&(sgd.serverTraceFileTS));
                }
                return(EOF);
            }
            n=n+1+REQ_DEBUG_PREFIX_STRING_LENGTH;
            debugPrefixOrTstampFlag=BOOL_FALSE;
          }
        }
        /* End change for JDBC Server */

        if(putc(c,so) == EOF){
                /* Addition for JDBC Server: Release T/S and return EOF on error. */
                if (using_clientFile_or_serverFile == 0){
                    ts_clear_act(&(sgd.clientTraceFileTS));
                } else {
                    ts_clear_act(&(sgd.serverTraceFileTS));
                }
                return(EOF);
        }
           /* Addition for JDBC Server: Test if we added a \n, if so set debugPrefixFlag */
           /* It is OK to test this here since an EOF condition above means we won't print any more */
           if (c =='\n') debugPrefixOrTstampFlag=BOOL_TRUE;
           /* End change for JDBC Server */
           n ++;
       }
   }
/* Addition for JDBC Server: Release T/S and return. */
if (using_clientFile_or_serverFile == 0){
    ts_clear_act(&(sgd.clientTraceFileTS));
} else {
    ts_clear_act(&(sgd.serverTraceFileTS));
}
/* End change for JDBC Server */
return(n);
}

/*
 * Function Tcheck
 *
 * Does a free() using a dummy malloc pointer that points to the 8th word of the
 * first malloc bank used. This acts like a malloc/free "NOP" since it has no
 * affect on the actual malloc environment. If the malloc chain checking code
 * has been turned on, the call to Tcheck() will get the malloc chain checking
 * code to perform its malloc/free chain check.
 *
 * Parameters:
 *
 * id        - a int number identifying the dummy free().
 * filename  - a string literal containing the name of the code file that the
 *               dummy free() invocation is being done.
 * tracetext - Pointer to a character string. If debugInternal is set, this string
 *             will be tracef()'ed just before the free() call is performed.
 *
 * pointer   - A pointer value to be displayed as a pointer after the trace text.
 *
 *
 */
void Tcheck(int traceid, char* fileName, char * tracetext, void * pointer){
    /* structure for dummy malloc pointer  */
    union {
        void * ptr;
        int words[2];
    }  dummyFreePointer;
    /* Construct the dummy free pointer to use in free() calls that do
     * nothing. Construct the pointer to the 8th word of the malloc bank
     * that the ServerWorker's standing request packet is in, since this
     * malloc buffer is always present and we just need the malloc bank's
     * bdi value to construct the dummy free pointer.
     */
    dummyFreePointer.ptr = (void *)act_wdePtr->standingRequestPacketPtr;
    dummyFreePointer.words[1] = 7; /* throw away original offset and use 7 instead. */

    /* Generate a trace image if desired. */
    if (debugInternal){
        tracef("Tcheck(): file= %s id= %d %s %p\n", fileName, traceid, tracetext, pointer);
    }

    /* Generate a trace image if desired. */
/*    if (debugInternal){ */
/*        tracef("Tcheck(): file= %s id= %d %s %p\n", fileName, traceid, tracetext, pointer); */
/*    } */

    /* Now do the dummy free() and return. */
    free(dummyFreePointer.ptr);
    return;
}

/*
 * Functions Tmalloc(), Tcalloc() and Tfree()
 *
 * Functions and procedure for allocating and freeing malloc'ed buffer space.
 * These functions allow us to gather some information on the amount of memory
 * being used and to turn on a memory allocation/free debug trace, if desired.
 *
 * Note: If the trace is used, the algorithm used to compute the number of words
 * that are actually allocated must be changed if this code is placed into code
 * running in TIP mode.
 *
 * Function Tmalloc() and Tcalloc() are used in place of malloc()and calloc()
 * respectively. Their usage is:
 *    Tmalloc(traceid, filename, allocSize)
 *
 *    Tcalloc(traceid, filename, num, allocSize)
 *
 * where the arguments have the following meaning:
 *
 *    traceid =  an integer value used to identify the particular usage instance
 *               of the function.  This number should be unique across all the
 *               RDMS JDBC code files to aid in locating the code source line
 *               when a trace is produced.
 *               NOTE:  The traceid numbers are assigned in order from 1 to
 *               MAX_TMALLOC_TRACEID_MAX. When new Tmalloc(), Tcalloc() calls
 *               are added, they get the next set of traceids numbers and
 *               MAX_TMALLOC_TRACEID_MAX must be updated.
 *
 *    filename = a string literal containing the name of the code file that the
 *               Tmalloc() or Tcalloc() invocation is being done.
 *
 *    num      = number of objects to allocate space for (calloc() only)
 *
 *    allocSize = the size in bytes of the requested allocation.
 *
 *
 * Procedure Tfree() is used in place of free(). Its usage is:
 *    Tfree(traceid, filename, allocPtr)
 *
 * where the arguments have the following meaning:
 *
 *    traceid =  an integer value used to identify the particular usage instance
 *               of the Tfree procedure.  This number should be unique across all
 *               the RDMS JDBC code files to aid in locating the code source line
 *               when a trace is produced. There is no correlation between this
 *               id number and the id number associated to the Tmalloc that
 *               originally allocated the space.
 *               NOTE:  The traceid numbers are assigned in order from 1 to
 *               MAX_TMALLOC_TRACEID_MAX. When new Tfree() calls are added,
 *               they get the next set of traceids numbers and
 *               MAX_TMALLOC_TRACEID_MAX must be updated.
 *
 *    filename = a string literal containing the name of the code file that the
 *               Tfree() invocation is being done.
 *
 *    allocPtr = pointer to the alloc'ed memory being free'd.
 *
 */
void * Tmalloc(int traceid, char* fileName, int size) {

    void * p;
    int * pp;
    int wordSize;

#if MONITOR_TMALLOC_USAGE
    tmalloc_entry       * tmalloc_entryPtr;
    tmalloc_table_entry * tmallocTable_entryPtr;
    tmalloc_table_entry * tmallocTable_global_entryPtr;
#else
    char pbuff[200];
    int rounded_size;
#endif

#if !MONITOR_TMALLOC_USAGE
    /* Allocate the memory requested and record amount in the activities wde entry*/
    errno = 0;
    rounded_size = ((size + 7)/8)*8; /* Round request size up to an even number of double words. */
    p = malloc(rounded_size);

    /* Generate a trace image if desired. */
/*    if (debugInternal){ */
/*        pp=((int *)p)-2; *//* get an int ptr to header with alloc'ed size in words */
/*        wordSize= *pp & 0777777777; *//* only need Q2-Q4 */
/*        tracef("Tmalloc(): file= %s id= %d ActID= %d size= %d rounded_size= %d p= %p pp= %p words=  %d errno=%d\n", */
/*               fileName, traceid, act_wdePtr->serverWorkerActivityID, size, rounded_size, p, pp, wordSize, errno);  */
/*    } */

    /*
     * Get the size in words of the actual allocation and
     * update (increment) the total allocated so far by this activity.
     *
     * In non-TIP mode, back the pointer up two words and take
     * Q2-Q4's value (lower 27 bits) to get the allocated word size.
     *
     * The computation of pp and wordSize must be modified if this code
     * is moved into code running under TIP since a different storage
     * structure layout is used.  Without changes, this code will get bad
     * allocation lengths if run in TIP.
     *
     * First, test if we got a null pointer - handle this special.
     */
    if (p != NULL){
        pp=((int *)p)-2; /* get an int ptr to header with alloc'ed size in words */
        wordSize= *pp & 0777777777; /* only need Q2-Q4 */
        act_wdePtr->totalAllocSpace += wordSize;
#if SIMPLE_MALLOC_TRACING == 2
        test_set(&(sgd.serverLogFileTS));  /* Get control of the Log file's T/S cell */
        printf("In Tmalloc(%3d), words = %d, newTotal = %d, p = %p, errno = %d\n", traceid, wordSize, act_wdePtr->totalAllocSpace, p, errno);
        ts_clear_act(&(sgd.serverLogFileTS));
#endif
#if SIMPLE_MALLOC_TRACING > 0
        if (debugInternal){
            tracef("In Tmalloc(%3d), words = %d, newTotal = %d, p = %p, errno = %d\n", traceid, wordSize, act_wdePtr->totalAllocSpace, p, errno);
        }
#endif
    } else {
        /*
         * pointer is null, so no space was allocated - indicate it with
         * empty allocation, a trace will show the NULL pointer.
         */
        wordSize=0;
    }

    /* Test if we are in debugging mode and want to trace malloc's */
    if (sgd.debug == SGD_DEBUG_MALLOCS){
        /* Yes, send a trace image to the JDBC Server's trace file */
        sprintf(pbuff,
               "Tmalloc() file= %s id= %d ActID= %d alloc_size= %d ptr=%p alloc_wordSize= %d total_words= %d",
               fileName, traceid, act_wdePtr->serverWorkerActivityID, size, p, wordSize, act_wdePtr->totalAllocSpace);
        addServerLogEntry(TO_SERVER_TRACEFILE,pbuff);
    }

    /* return pointer to malloc'ed space */
    return p;

#else        /* Monitor Tmalloc, etc. usage */
    /*
     * Allocate the requested space plus a hidden tmalloc entry header.
     * After allocation, obtain a pointer to the malloc_area to return
     * to caller so the tmalloc header area remains "hidden", and the
     * caller has a pointer to the space they can utilize.
     */
    tmalloc_entryPtr = malloc(size + TMALLOC_ENTRY_HEADER_SIZE);

    /* Set up the initial tmalloc_entry information. */
    tmalloc_entryPtr->dLink.priorDLinkListPtr = NULL;
    tmalloc_entryPtr->dLink.nextDLinkListPtr = NULL;
    tmalloc_entryPtr->traceid = traceid;
    tmalloc_entryPtr->malloc_size = size;
    UCSDWTIME$(&tmalloc_entryPtr->requestDWTime);
    tmalloc_entryPtr->tagValue = TMALLOC_ENTRY_TAGVALUE;

    /* Obtain pointer to the tmalloc_table's entry for this traceid. */
    tmallocTable_entryPtr = (tmalloc_table_entry *)&(act_wdePtr->tmalloc_table[traceid]);

    /*
     * Update this tmalloc_table's entry information.
     */
    tmallocTable_entryPtr->tmalloc.type_of_entry = TMALLOC_TABLE_ENTRY_TYPE_TMALLOC;
    tmallocTable_entryPtr->tmalloc.sum_tmalloc_calls++;
    strncpy(tmallocTable_entryPtr->tmalloc.eltName, fileName, TMALLOC_TABLE_ENTRY_ELTNAME_SIZE);
    tmallocTable_entryPtr->tmalloc.sum_malloced_memory += size;

    /* Add this tmalloc_entry to the tmalloc_table's dLinkList. */
    addDLinkListEntry(&tmallocTable_entryPtr->tmalloc.firstdLink, &tmalloc_entryPtr->dLink);

    /* Display information about the Tcalloc() call. */
    if (MONITOR_TMALLOC_USAGE_DISPLAY_CALLS){

        /* Use Log file's T/S cell so all the output is together.*/
        test_set(&(sgd.serverLogFileTS));
        printf("Act. Id %12d called Tmalloc(%d, %s, %d), now has %d dlink_entries\n",
               act_wdePtr->serverWorkerActivityID, traceid, fileName, size, tmallocTable_entryPtr->tmalloc.firstdLink.num_entries);
        ts_clear_act(&(sgd.serverLogFileTS));
    }

    /*
     * Update the tmalloc_table's global entry information.
     */
    tmallocTable_global_entryPtr = (tmalloc_table_entry *)&(act_wdePtr->tmalloc_table[0]);
    tmallocTable_global_entryPtr->total.type_of_entry = TMALLOC_TABLE_ENTRY_TYPE_TOTAL;
    tmallocTable_global_entryPtr->total.total_tmalloc_calls++;
    tmallocTable_global_entryPtr->total.total_malloced_memory += size;
    tmallocTable_global_entryPtr->total.total_dlink_entries++;

    /* Return pointer to caller's "visible" malloc'ed space */
    p = &(tmalloc_entryPtr->malloc_area);
    return p;
#endif       /* Monitor Tmalloc, etc. usage */
}

void * Tcalloc(int traceid, char* fileName, int num, int size) {

    void * p;
    int * pp;
    int wordSize;

#if MONITOR_TMALLOC_USAGE
    int calloc_size;
    tmalloc_entry * tmalloc_entryPtr;
    tmalloc_table_entry * tmallocTable_entryPtr;
    tmalloc_table_entry * tmallocTable_global_entryPtr;
#else
    char pbuff[200];
    int rounded_size;
#endif
#if !MONITOR_TMALLOC_USAGE
    /* Allocate the memory requested and record amount in the activities wde entry*/
    errno = 0;
    rounded_size = ((size + 7)/8)*8; /* Round request size up to an even number of double words. */
    p = calloc(num, rounded_size); /* like malloc() but memory returned will be 0'ed */


    /* Generate a trace image if desired. */
/*    if (debugInternal){ */
/*        pp=((int *)p)-2; *//* get an int ptr to header with alloc'ed size in words */
/*        wordSize= *pp & 0777777777; *//* only need Q2-Q4 */
/*        tracef("Tcalloc(): file= %s id= %d ActID= %d size= %d rounded_size= %d p= %p pp= %p wordSize=  %d errno=%d num = %d\n\n", */
/*               fileName, traceid, act_wdePtr->serverWorkerActivityID, size, rounded_size, p, pp, wordSize, errno, num); */
/*    } */

    /*
     * Get the size in words of the actual allocation and
     * update (increment) the total allocated so far by this activity.
     *
     * In non-TIP mode, back the pointer up two words and take
     * Q2-Q4's value (lower 27 bits) to get the allocated word size.
     *
     * The computation of pp and wordSize must be modified if this code
     * is moved into code running under TIP since a different storage
     * structure layout is used.  Without changes, this code will get bad
     * allocation lengths if run in TIP.
     *
     * First, test if we got a null pointer - handle this special.
     */
    if (p != NULL){
        pp=((int *)p)-2; /* get an int ptr to header with alloc'ed size in words */
        wordSize= *pp & 0777777777; /* only need Q2-Q4 */
        act_wdePtr->totalAllocSpace += wordSize;
#if SIMPLE_MALLOC_TRACING == 2
        test_set(&(sgd.serverLogFileTS));  /* Get control of the Log file's T/S cell */
        printf("In Tcalloc(%3d), words = %d, newTotal = %d, p = %p, errno = %d\n", traceid, wordSize, act_wdePtr->totalAllocSpace, p, errno);
        ts_clear_act(&(sgd.serverLogFileTS));
#endif
#if SIMPLE_MALLOC_TRACING > 0
        if (debugInternal){
            tracef("In Tcalloc(%3d), words = %d, newTotal = %d, p = %p, errno = %d\n", traceid, wordSize, act_wdePtr->totalAllocSpace, p, errno);
        }
#endif
    } else {
        /*
         * pointer is null, so no space was allocated - indicate it with
         * empty allocation, a trace will show the NULL pointer.
         */
        wordSize=0;
    }

    /* Test if we are in debugging mode and want to trace calloc's */
    if (sgd.debug == SGD_DEBUG_MALLOCS){
        /* Yes, send a trace image to the JDBC Server's trace file */
        sprintf(pbuff,
               "Tcalloc() file= %s id= %d ActID= %d alloc_size= %d ptr=%p alloc_words= %d total_words= %d",
               fileName, traceid, act_wdePtr->serverWorkerActivityID, num*size, p, wordSize, act_wdePtr->totalAllocSpace);
        addServerLogEntry(TO_SERVER_TRACEFILE,pbuff);
    }

    /* return pointer to calloc'ed space */
    return p;
#else        /* Monitor Tcalloc, etc. usage */
    /*
     * Like malloc() but memory returned will be 0'ed.
     *
     * Allocate the requested space plus a hidden tmalloc entry header.
     * After allocation, obtain a pointer to the malloc_area to return
     * to caller so the tmalloc header area remains "hidden", and the
     * caller has a pointer to the space they can utilize.
     *
     * For calloc() purposes, compute full size needed (i.e., num * size)
     * and add the tmalloc header size to that, calling calloc() to
     * request just one allocation of that full size needed.
     */
    calloc_size = num * size;
    tmalloc_entryPtr = calloc(1, calloc_size + TMALLOC_ENTRY_HEADER_SIZE);

    /* Set up the initial tmalloc_entry information. */
    tmalloc_entryPtr->dLink.priorDLinkListPtr = NULL;
    tmalloc_entryPtr->dLink.nextDLinkListPtr = NULL;
    tmalloc_entryPtr->traceid = traceid;
    tmalloc_entryPtr->malloc_size = calloc_size;
    UCSDWTIME$(&tmalloc_entryPtr->requestDWTime);
    tmalloc_entryPtr->tagValue = TMALLOC_ENTRY_TAGVALUE;

    /* Obtain pointer to the tmalloc_table's entry for this traceid. */
    tmallocTable_entryPtr = (tmalloc_table_entry *)&(act_wdePtr->tmalloc_table[traceid]);

    /*
     * Update this tmalloc_table's entry information.
     */
    tmallocTable_entryPtr->tmalloc.type_of_entry = TMALLOC_TABLE_ENTRY_TYPE_TMALLOC;
    tmallocTable_entryPtr->tmalloc.sum_tmalloc_calls++;
    strncpy(tmallocTable_entryPtr->tmalloc.eltName, fileName, TMALLOC_TABLE_ENTRY_ELTNAME_SIZE);
    tmallocTable_entryPtr->tmalloc.sum_malloced_memory += calloc_size;

    /* Add this tmalloc_entry to the tmalloc_table's dLinkList. */
    addDLinkListEntry(&tmallocTable_entryPtr->tmalloc.firstdLink, &tmalloc_entryPtr->dLink);

    /* Display information about the Tcalloc() call. */
    if (MONITOR_TMALLOC_USAGE_DISPLAY_CALLS){
        /* Use Log file's T/S cell so all the output is together.*/
        test_set(&(sgd.serverLogFileTS));

        printf("Act. Id %12d called Tcalloc(%d, %s, %d), now has %d dlink_entries\n",
               act_wdePtr->serverWorkerActivityID, traceid, fileName, calloc_size, tmallocTable_entryPtr->tmalloc.firstdLink.num_entries);
        ts_clear_act(&(sgd.serverLogFileTS));
    }

    /*
     * Update the tmalloc_table's global entry information.
     */
    tmallocTable_global_entryPtr = (tmalloc_table_entry *)&(act_wdePtr->tmalloc_table[0]);
    tmallocTable_global_entryPtr->total.total_tmalloc_calls++;
    tmallocTable_global_entryPtr->total.type_of_entry = TMALLOC_TABLE_ENTRY_TYPE_TOTAL;
    tmallocTable_global_entryPtr->total.total_malloced_memory += calloc_size;
    tmallocTable_global_entryPtr->total.total_dlink_entries++;

    /* Return pointer to caller's "visible" calloc'ed space */
    p = &(tmalloc_entryPtr->malloc_area);
    return p;
#endif       /* Monitor Tcalloc, etc. usage */
}

/*
 * Procedure Tfree
 *
 * This procedure returns allocated memory back to available space.
 * For procedure details see the comments in front of function Tmalloc().
 *
 */
 void Tfree(int traceid, char* fileName, void * p) {

     int * pp;
     int wordSize;

#if !MONITOR_TMALLOC_USAGE
    char pbuff[200];

    /* Test if the pointer was NULL. */
    if (p != NULL){
       /*
        * Get the size in words of the allocation and
        * update (decrement) the total allocated so far by this activity.
        *
        * In non-TIP mode, back the pointer up two words and take
        * Q2-Q4's value (lower 27 bits) to get the allocated word size.
        *
        * First,
        */
       pp=((int *)p)-2; /* get an int ptr to header with alloc'ed size in words */
       wordSize= *pp & 0777777777; /* only need Q2-Q4 */
       act_wdePtr->totalAllocSpace -= wordSize;
    } else {
        /*
         * pointer is null, so no space was allocated - indicate it with
         * empty allocation, the trace will show the NULL pointer.
         */
        wordSize=0;
    }

    /* Test if we are in debugging mode and want to trace free's */
    if (sgd.debug == SGD_DEBUG_MALLOCS){
       /* Yes, generate and output a trace image to the JDBC Server's trace file. */
       sprintf(pbuff,
               "Tfree() file= %s id= %d ActID= %d ....... ....... ptr=%p alloc_words= %d total_words= %d",
               fileName, traceid, act_wdePtr->serverWorkerActivityID, p, wordSize, act_wdePtr->totalAllocSpace);
       addServerLogEntry(TO_SERVER_TRACEFILE,pbuff);
    }


      /* Generate a trace image if desired. */
/*    if (debugInternal){ */
/*        pp=((int *)p)-2; *//* get an int ptr to header with alloc'ed size in words */
/*        wordSize= *pp & 0777777777; *//* only need Q2-Q4 */
/*        tracef("Tfree(): file= %s id= %d ActID= %d ... ... p= %p pp= %p wordSize=  %d errno= %d\n", */
/*               fileName, traceid, act_wdePtr->serverWorkerActivityID, p, pp, wordSize);             */
/*    } */

    /* Return allocated space back to free space heap*/
    errno = 0;
    free(p);
#if SIMPLE_MALLOC_TRACING == 2
    test_set(&(sgd.serverLogFileTS));  /* Get control of the Log file's T/S cell */
    printf("In Tfree(%3d),   words = %d, newTotal = %d, p = %p, errno = %d\n", traceid, wordSize, act_wdePtr->totalAllocSpace, p, errno);
    ts_clear_act(&(sgd.serverLogFileTS));
#endif
#if SIMPLE_MALLOC_TRACING > 0
        if (debugInternal){
            tracef("In Tfree(%3d),   words = %d, newTotal = %d, p = %p, errno = %d\n", traceid, wordSize, act_wdePtr->totalAllocSpace, p, errno);
        }
#endif
    return;
#else        /* Monitor Tfree, etc. usage */

    int * tmalloc_entry_tagvalue;
    tmalloc_entry * tmalloc_entryPtr;
    tmalloc_table_entry * tmallocTable_Tfree_entryPtr;
    tmalloc_table_entry * tmallocTable_Tmalloc_entryPtr;
    tmalloc_table_entry * tmallocTable_global_entryPtr;
    int tmalloc_traceid;
    int tmalloc_size;

    /*
     * First, test if we actually have a tmalloc'ed malloc entry.
     * If it does not have the correct tag value, it's either not
     * one of our Tmalloc's (somewhere we did a regular malloc())
     * or its a corrupted tmalloc entry in which case we can't
     * do anything with it, so just do a free() and return.
     */
    tmalloc_entry_tagvalue = (int *) p - WORD_OFFSET_TO_TMALLOC_ENTRY_HEADER_TAG_VALUE;
    if (*tmalloc_entry_tagvalue != TMALLOC_ENTRY_TAGVALUE){
        /*
         * Doesn't look like a tmalloc entry. Tracef this
         * fact - we should not be calling Tfree or we have
         * an error. Then free space and return.
         */
        free(p);
        return;
    }
    /*
     * Pointer does looks like a tmalloc entry - process it.
     * Using the pointer provided by the caller, obtain a
     * pointer to the start of the allocation's "hidden"
     * tmalloc header so we can free the actual malloc'ed
     * space. Get the tmalloc table entries for this Tfree
     * call and its associated Tmalloc entry.
     */
    tmalloc_entryPtr = (tmalloc_entry *)((int *) p - TMALLOC_ENTRY_HEADER_SIZE_WORDS);
    tmalloc_traceid = tmalloc_entryPtr->traceid;
    tmalloc_size = tmalloc_entryPtr->malloc_size;
    tmallocTable_Tmalloc_entryPtr = (tmalloc_table_entry *)&(act_wdePtr->tmalloc_table[tmalloc_traceid]);
    tmallocTable_Tfree_entryPtr = (tmalloc_table_entry *)&(act_wdePtr->tmalloc_table[traceid]);

    /*
     * Update the associated tmalloc_table's Tmalloc entry information.
     */
    tmallocTable_Tmalloc_entryPtr->tmalloc.sum_tfree_calls++;
    tmallocTable_Tmalloc_entryPtr->tmalloc.sum_freed_memory += tmalloc_size;

    /*
     * Update this tmalloc_table's Tfree entry information.
     */
    tmallocTable_Tfree_entryPtr->tfree.type_of_entry = TMALLOC_TABLE_ENTRY_TYPE_TFREE;
    strncpy(tmallocTable_Tfree_entryPtr->tfree.eltName, fileName, TMALLOC_TABLE_ENTRY_ELTNAME_SIZE);
    tmallocTable_Tfree_entryPtr->tfree.sum_tfree_calls++;
    tmallocTable_Tfree_entryPtr->tfree.sum_freed_memory += tmalloc_size;

    /* Also, accumulate freed space information based on the trace id. Keep info for
     * up to three distinct tmalloc traceid's, with all the rest just lumped together.
     */
    if (tmallocTable_Tfree_entryPtr->tfree.tmalloc_traceid1 == tmalloc_traceid ||
        tmallocTable_Tfree_entryPtr->tfree.tmalloc_traceid1 == 0 ){
            /* Accumulate traced1 information here. */
            tmallocTable_Tfree_entryPtr->tfree.tmalloc_traceid1 = tmalloc_traceid; /* Easier to always save it, even the first time. */
            tmallocTable_Tfree_entryPtr->tfree.traceid1_tfree_calls++;
            tmallocTable_Tfree_entryPtr->tfree.sum_traceid1_freed_memory += tmalloc_size;
    } else if (tmallocTable_Tfree_entryPtr->tfree.tmalloc_traceid2 == tmalloc_traceid ||
               tmallocTable_Tfree_entryPtr->tfree.tmalloc_traceid2 == 0 ){
            /* Accumulate traceid2 information here. */
            tmallocTable_Tfree_entryPtr->tfree.tmalloc_traceid2 = tmalloc_traceid; /* Easier to always save it, even the first time. */
            tmallocTable_Tfree_entryPtr->tfree.traceid2_tfree_calls++;
            tmallocTable_Tfree_entryPtr->tfree.sum_traceid2_freed_memory += tmalloc_size;
    } else {
            /* Accumulate all the rest of the traceid's information here. */
            tmallocTable_Tfree_entryPtr->tfree.untraceid_tfree_calls++;
            tmallocTable_Tfree_entryPtr->tfree.sum_untraceid_freed_memory += tmalloc_size;
    }

    /*
     * Remove the associated tmalloc_entry from the traceid's tmalloc_table_entry
     * table's dLinkList.
     */
    removeDLinkListEntry(&tmallocTable_Tmalloc_entryPtr->tmalloc.firstdLink, &tmalloc_entryPtr->dLink);

    /* Display information about the Tfree() call. */
    if (MONITOR_TMALLOC_USAGE_DISPLAY_CALLS){
        /* Use Log file's T/S cell so all the output is together.*/
        test_set(&(sgd.serverLogFileTS));
        printf("Act. Id %12d called Tfree(%d, %s): %d bytes freed, Tmalloc %d now has %d dlink_entries\n",
               act_wdePtr->serverWorkerActivityID, traceid, fileName, tmalloc_size, tmalloc_traceid, tmallocTable_Tmalloc_entryPtr->tmalloc.firstdLink.num_entries);
        ts_clear_act(&(sgd.serverLogFileTS));
    }

    /*
     * Update the tmalloc_table's global entry information.
     */
    tmallocTable_global_entryPtr = (tmalloc_table_entry *)&(act_wdePtr->tmalloc_table[0]);
    tmallocTable_global_entryPtr->total.type_of_entry = TMALLOC_TABLE_ENTRY_TYPE_TOTAL;
    tmallocTable_global_entryPtr->total.total_tfree_calls++;
    tmallocTable_global_entryPtr->total.total_freed_memory += tmalloc_size;
    tmallocTable_global_entryPtr->total.total_dlink_entries--;


    /*
     * Remove the tmalloc'ed malloc entry. First clear the entries tag value
     * so it is not reprocessed angain if Tfree() is called on it a second
     * time. Then return actual allocated space back to free space heap
     */
    *tmalloc_entry_tagvalue = 0;
    free(tmalloc_entryPtr);
    return;
#endif       /* Monitor Tfree, etc. usage */
}

#if MONITOR_TMALLOC_USAGE /* Tmalloc/Tcalloc/Tfree tracking */
/*
 * Function output_malloc_tracking_summary
 *
 * When tracking malloc storage usage (using TMalloc, Tcalloc, and Tfree), this
 * procedure produces a trace file listing of the current state of this WDE's
 * tmalloc_table.
 */
void output_malloc_tracking_summary(){

    int i;
    dLinkListHeader * dListHeaderPtr;
    tmalloc_table_entry * tmallocTable_entryPtr;

    /* Is reporting turned on? If not, return. */
    if (MONITOR_TMALLOC_USAGE_REPORT == 0){
        return;
    }

    /* Get control of the Log file's T/S cell so all the output is together.*/
    test_set(&(sgd.serverLogFileTS));

    /* Loop over all the Tmalloc table entries. */
    for (i=0; i <= TMALLOC_TRACEID_MAX; i++){
         /*
         * Determine how to report the Tmalloc tables entry's
         * information based on what type of entry it is. Use
         * the sub-structs "total" to do the test, since all
         * the sub-structs have the type_of_entry field first.
         *
         * If the type_of_entry is not set, then the Tmalloc,
         * Tcalloc, or Tfree associated to this traceid was not
         * used and there will be no output displayed for it.
         */
        tmallocTable_entryPtr = &(act_wdePtr->tmalloc_table[i]);
        switch (tmallocTable_entryPtr->total.type_of_entry) {
            case TMALLOC_TABLE_ENTRY_TYPE_TOTAL: {
                /*
                 * Display the totals for all the table's traceid's Tmalloc, Tcalloc,
                 * and Tfree entries.
                 */
                printf("\nTotal Summary:        total_tmallocs total_tfrees total_malloced_memory total_freed_memory total_dlink_entries\n");
                printf("Act.Id %12d   %14d %12d %21d %18d %19d\n",
                       act_wdePtr->serverWorkerActivityID, tmallocTable_entryPtr->total.total_tmalloc_calls, tmallocTable_entryPtr->total.total_tfree_calls,
                       tmallocTable_entryPtr->total.total_malloced_memory, tmallocTable_entryPtr->total.total_freed_memory,
                       tmallocTable_entryPtr->total.total_dlink_entries);
            break;
            }
            case TMALLOC_TABLE_ENTRY_TYPE_TMALLOC: {
                if (MONITOR_TMALLOC_USAGE_REPORT > 1){
                    /* Display the individual sums for the table's traceid's Tmalloc entry. */
                    dListHeaderPtr = &(tmallocTable_entryPtr->tmalloc.firstdLink);
                    if (dListHeaderPtr->num_entries == 0){
                        /* No dList entries are present. Reduce output amount accordingly. */
                        printf("Tmalloc:%4d %15s, calls: %7d, malloced: %10d, tfrees: %7d, freed: %10d\n",
                               i, tmallocTable_entryPtr->tmalloc.eltName, tmallocTable_entryPtr->tmalloc.sum_tmalloc_calls,
                               tmallocTable_entryPtr->tmalloc.sum_malloced_memory, tmallocTable_entryPtr->tmalloc.sum_tfree_calls,
                               tmallocTable_entryPtr->tmalloc.sum_freed_memory);
                    } else {
                        /* dList entries are still present. Output all information. */
                        printf("Tmalloc:%4d %15s, calls: %7d, malloced: %10d, tfrees: %7d, freed: %10d, dlink_entries: %6d\n",
                               i, tmallocTable_entryPtr->tmalloc.eltName, tmallocTable_entryPtr->tmalloc.sum_tmalloc_calls,
                               tmallocTable_entryPtr->tmalloc.sum_malloced_memory, tmallocTable_entryPtr->tmalloc.sum_tfree_calls,
                               tmallocTable_entryPtr->tmalloc.sum_freed_memory, dListHeaderPtr->num_entries);
                    }

                    /* Do we want a detailed display of the tmalloc entries on the tmalloc entry list? */
                    if (MONITOR_TMALLOC_USAGE_REPORT == 3){
                        dumpAllDlinkedEntries(dListHeaderPtr);
                    }
                }
                break;
            }
            case TMALLOC_TABLE_ENTRY_TYPE_TFREE: {
                if (MONITOR_TMALLOC_USAGE_REPORT > 1){
                    /* Display the individual sums for the table's traceid's Tfree entry. */
                    if (tmallocTable_entryPtr->tfree.untraceid_tfree_calls > 0){
                        /* All Tfrees's were for more than two Tmalloc traceid's. Output all information. */
                        printf("Tfree:  %4d %15s, calls: %7d, freed:    %10d, Tmalloc%4d (%d/%d), Tmalloc%4d (%d/%d), Others (%d/%d)\n",
                               i, tmallocTable_entryPtr->tfree.eltName, tmallocTable_entryPtr->tfree.sum_tfree_calls, tmallocTable_entryPtr->tfree.sum_freed_memory,
                               tmallocTable_entryPtr->tfree.tmalloc_traceid1, tmallocTable_entryPtr->tfree.traceid1_tfree_calls, tmallocTable_entryPtr->tfree.sum_traceid1_freed_memory,
                               tmallocTable_entryPtr->tfree.tmalloc_traceid2, tmallocTable_entryPtr->tfree.traceid2_tfree_calls, tmallocTable_entryPtr->tfree.sum_traceid2_freed_memory,
                               tmallocTable_entryPtr->tfree.untraceid_tfree_calls, tmallocTable_entryPtr->tfree.sum_untraceid_freed_memory);
                    } else if (tmallocTable_entryPtr->tfree.traceid2_tfree_calls > 0){
                        /* All Tfrees's were for just two different Tmalloc traceid's. Reduce output amount accordingly */
                        printf("Tfree:  %4d %15s, calls: %7d, freed:    %10d, Tmalloc%4d (%d/%d), Tmalloc%4d (%d/%d)\n",
                               i, tmallocTable_entryPtr->tfree.eltName, tmallocTable_entryPtr->tfree.sum_tfree_calls, tmallocTable_entryPtr->tfree.sum_freed_memory,
                               tmallocTable_entryPtr->tfree.tmalloc_traceid1, tmallocTable_entryPtr->tfree.traceid1_tfree_calls, tmallocTable_entryPtr->tfree.sum_traceid1_freed_memory,
                               tmallocTable_entryPtr->tfree.tmalloc_traceid2, tmallocTable_entryPtr->tfree.traceid2_tfree_calls, tmallocTable_entryPtr->tfree.sum_traceid2_freed_memory);
                    } else {
                        /* All Tfrees's were for just one Tmalloc traceid. Reduce output amount accordingly */
                        printf("Tfree:  %4d %15s, calls: %7d, freed:    %10d, Tmalloc%4d (%d/%d)\n",
                               i, tmallocTable_entryPtr->tfree.eltName, tmallocTable_entryPtr->tfree.sum_tfree_calls, tmallocTable_entryPtr->tfree.sum_freed_memory,
                               tmallocTable_entryPtr->tfree.tmalloc_traceid1, tmallocTable_entryPtr->tfree.traceid1_tfree_calls, tmallocTable_entryPtr->tfree.sum_traceid1_freed_memory);
                    }
                }
                break;
            }
            case TMALLOC_TABLE_ENTRY_TYPE_UNUSED: {
                /* The table entry for this traceid was not used. Nothing to display. */
                break;
            }
        }
    }

    /* All done, release the T/S and return. */
    ts_clear_act(&(sgd.serverLogFileTS));
}

/*
 * Function addDLinkListEntry
 * Adds a new entry onto the doubly linked list.
 *
 * Parameters:
 *   dListHeaderPtr    - Pointer to the dlist header that points to the
 *                       first tmalloc_entry on the doubly linked list.
 * Return:
 *   dLinkListEntryPtr - Pointer to the link field (next, prior) of
 *                       the entry being added to the list.
 */
void addDLinkListEntry(dLinkListHeader * dListHeaderPtr, dLinkListEntry * dLinkListEntryPtr){

    /* Is this new entry the first one to be on the list? */
    if (dListHeaderPtr->firstDLinkListPtr == NULL){
        /* Yes, initialize the new entry's linked pointers. */
        dLinkListEntryPtr->nextDLinkListPtr = NULL;
        dLinkListEntryPtr->priorDLinkListPtr = NULL;
    } else {
        /*
         * Link the new entry before the current first
         * entry on the doubly linked list.
         */
        dLinkListEntryPtr->nextDLinkListPtr = dListHeaderPtr->firstDLinkListPtr;
        dLinkListEntryPtr->priorDLinkListPtr = NULL;
        (dListHeaderPtr->firstDLinkListPtr)->priorDLinkListPtr = dLinkListEntryPtr;
    }

    /*  Now make the new entry the first entry, bump the count, and we're done. */
    dListHeaderPtr->firstDLinkListPtr = dLinkListEntryPtr;
    dListHeaderPtr->num_entries++;
    return;
}

/*
 * Function removeDLinkListEntry
 * Removes the specified entry from the doubly linked list.
 *
 * Parameters:
 *   dListHeaderPtr    - Pointer to the dlist header that points to the
 *                       first tmalloc_entry on the doubly linked list.
 * Return:
 *   dLinkListEntryPtr - Pointer to the link field (next, prior) of
 *                       the entry being removed from the list.
 */
void removeDLinkListEntry(dLinkListHeader * dListHeaderPtr, dLinkListEntry * dLinkListEntryPtr){
    dLinkListEntry * nextDLinkListPtr;
    dLinkListEntry * priorDLinkListPtr;

    /* Get the specified entry's link to the next entry in forward direction. */
    nextDLinkListPtr = dLinkListEntryPtr->nextDLinkListPtr;
    priorDLinkListPtr = dLinkListEntryPtr->priorDLinkListPtr;

    /* Test if prior link indicates we are at the top of the list. */
    if (priorDLinkListPtr == NULL){
        /*
         * Yes we are. Point firstDLinkListPtr pointed to entry to next
         * linked entry forward, and if there is an entry there, set that
         * entry's prior pointer to NULL since it is now the first
         * entry and has no prior entry in backward direction.
         */
        dListHeaderPtr->firstDLinkListPtr = nextDLinkListPtr;
        if (nextDLinkListPtr != NULL){
            nextDLinkListPtr->priorDLinkListPtr = NULL;
        }
    } else {
        /* No, just link this entry's prior entry to this entry's next entry (forward direction). */
        priorDLinkListPtr->nextDLinkListPtr = nextDLinkListPtr;

        /*
         * Now link this entry's next entry to this entry's prior entry in prior
         * direction. Check that we are not at the end of the linked list - If we
         * are at the end of the list there is no next entry to deal with.
         */
        if (nextDLinkListPtr != NULL){
            /*
             * Not at the end of the list. Set the next entry's prior entry to
             * this entry's prior entry (backward direction).
             */
            nextDLinkListPtr->priorDLinkListPtr = priorDLinkListPtr;
        }
    }

    /* Clear the removed entry's link pointers, decrement the count, and then we are done. */
    dLinkListEntryPtr->nextDLinkListPtr = NULL;
    dLinkListEntryPtr->priorDLinkListPtr = NULL;
    dListHeaderPtr->num_entries--;
    return;
}

/*
 * Function dumpAllDlinkedEntries
 *
 * Debug trace of all the doubly linked tmalloc
 * entries on the provided list. The caller insures
 * that all output images appear together (e.g., they
 * do a test_set(&(sgd.serverLogFileTS)) and a
 * ts_clear_act(&(sgd.serverLogFileTS)) at the end).
 *
 * Parameters:
 *   dListHeaderPtr    - Pointer to the dlist header that points to the
 *                       first tmalloc_entry on the doubly linked list.
 * Return:
 *
 */
void dumpAllDlinkedEntries(dLinkListHeader * dListHeaderPtr) {
    dLinkListEntry * nextDLinkListPtr;
    dLinkListEntry * currentDLinkListPtr;
    char DWTimeStr[DWTIME_STRING_LEN];
    int count = 0;

    /* Get the link to the first entry in forward direction. */
    currentDLinkListPtr = dListHeaderPtr->firstDLinkListPtr;

    /* Dump entries if any exist. */
    if (currentDLinkListPtr == NULL){
        /* No entries are present, so just return. */
        return;
    } else {
        /* Display a heading for the displayed entries. */
        printf("   Entry no. entry location           prior entry location     next entry location      malloc_size  DWTimestamp\n");

        /*
         * Traverse the list of doubleLinked entries in the forward direction
         * until done (NULL). Limit while loop to actual number of entries.
         */
        while(currentDLinkListPtr != NULL && count < dListHeaderPtr->num_entries ){
            /*
             * Bump the count of the number of entries we're displaying, and
             * get the link to the next dLink entry in the forward direction,
             * the link to the prior entry in the backward direction, and which
             * entry we're dumping.
             */
            count++;
            nextDLinkListPtr = currentDLinkListPtr->nextDLinkListPtr;

            /* Now dump the current tmalloc entry on the doubly linked list. */
            printf("    %8d %p %p %p %11d  %s\n", count, currentDLinkListPtr, currentDLinkListPtr->priorDLinkListPtr,
                   nextDLinkListPtr, ((tmalloc_entry *)currentDLinkListPtr)->malloc_size,
                   DWTimeString(((tmalloc_entry *)currentDLinkListPtr)->requestDWTime, &DWTimeStr[0]));

            /* Move to the next entry. */
            currentDLinkListPtr = nextDLinkListPtr;
        }
    }
    /* All done. */
    return;
}
#endif                       /* Tmalloc/Tcalloc/Tfree tracking */

/*
* Function NanoTime time retrieves the current nanosecond time (nanoseconds since 00:00, December 31, 1899)
*/
long long NanoTime(){

    long long nanoSeconds;
    /* Get the current DWTIME$ value to use and return it. */
    UCSDWTIME$(&nanoSeconds);
    return nanoSeconds;
}

/*
 * Function DWTimeString
 *
 * Returns the DWTIME$ value as a null terminated character
 * string in the format: "YYYY-MM-DD HH:MM:SS.sss"
 *
 * Parameters
 *    dwtime          - The DWTIME$ value to be converted.
 *    DWTimeStringPtr - Pointer to a char array at least
 *                      22 characters long (DWTIME_STRING_LEN).
 *
 *
 * Returns:
 *    DWTimeStringPtr - Returns the pointer to the second
 *                      parameter, which is where the DWTime
 *                      value string was created.
 */
char * DWTimeString(long long dwtime, char * DWTimeStringPtr){

    char timeBuffer[30];

    /*
     * UCSCNVTIM$CC() returns date time in format : "YYYYMMDDHHMMSSmmm"  So,
     * use it to form the date-time string format: "YYYYMMDD HH:MM:SS.SSS"
     */
    UCSCNVTIM$CC(&dwtime, timeBuffer);
    timeBuffer[17] ='\0';
    *DWTimeStringPtr ='\0';
    strncat(DWTimeStringPtr, &timeBuffer[0],8);   /* copy the YYYYMMDD */
    strcat(DWTimeStringPtr," ");
    strncat(DWTimeStringPtr, &timeBuffer[8],2); /* copy the HH */
    strcat(DWTimeStringPtr,":");
    strncat(DWTimeStringPtr, &timeBuffer[10],2); /* copy the MM */
    strcat(DWTimeStringPtr,":");
    strncat(DWTimeStringPtr, &timeBuffer[12],2); /* copy the SS */
    strcat(DWTimeStringPtr,".");
    strncat(DWTimeStringPtr, &timeBuffer[14],3); /* copy the mmm (SSS) */

    /* Return reference to converted DWTime string. */
    return DWTimeStringPtr;
}

/**
 * TSOnMsgFile
 *
 * This function performs a test and set on the message file T/S cell
 * in the sgd.
 *
 * Function clearTSOnMsgFile should later be called to clear the T/S that is
 * set by this function.
 */

void TSOnMsgFile() {

    test_set(&(sgd.msgFileTS));

 } /* TSOnMsgFile */

/**
 * clearTSOnMsgFile
 *
 * This function clears the message file test and set cell in the sgd.
 *
 * Function TSOnMsgFile was previously called to set the T/S cell.
 */

void clearTSOnMsgFile() {

    ts_clear_act(&(sgd.msgFileTS));

 } /* clearTSOnMsgFile */

/**
 * setClientDriverLevel
 *
 * This function sets the WDE variable clientDriverLevel.
 * For the level parameter:
 * This variable is a four byte entity
 * that contains the client's driver level
 * as four unsigned integral values:
 *   * major level
 *   * minor level
 *   * stability level
 *   * release level
 *
 * For the driver build level parameter:
 * contains the build id string of the JDBC Driver
 * as a string value.
 *
 * @param level
 *   A char array of size 4.
 */

void setClientDriverLevel(char * level,char * driverBuildLevel) {

    memcpy(act_wdePtr->clientDriverLevel, level, JDBC_DRIVER_LEVEL_SIZE);
    strncpy(act_wdePtr->clientDriverBuildLevel, driverBuildLevel,JDBC_SERVER_BUILD_LEVEL_MAX_SIZE);
} /* setClientDriverLevel */

/**
 * getClientDriverLevel
 *
 * This function gets the WDE variable clientDriverLevel.
 * For the level parameter:
 * This variable is a four byte entity
 * that contains the client's driver level
 * as four unsigned integral values:
 *   * major level
 *   * minor level
 *   * stability level
 *   * release level
 *
 * For the driver build level parameter:
 * contains the build id string of the JDBC Driver
 * as a string value.
 *
 * @param level
 *   A char pointer to an array of size 4.
 */

void getClientDriverLevel(char * level, char * driverBuildLevel) {

    memcpy(level, act_wdePtr->clientDriverLevel, JDBC_DRIVER_LEVEL_SIZE);
    strcpy(driverBuildLevel, act_wdePtr->clientDriverBuildLevel);


} /* getClientDriverLevel */

/**
 * getServerLevel
 *
 * This function gets the SGD variable serverLevel,
 * and the Server build level id string.
 *
 * For the Server Level parameter:
 *   This variable is a four byte entity
 *   that contains the server's level
 *   as four unsigned integral values:
 *   * major level
 *   * minor level
 *   * feature level
 *   * platform level
 *
 * For the Server Build Level id string parameter:
 *   This variable contains a build id string
 *   of up to SERVER_JDBC_SERVER_BUILD_LEVEL_MAX_SIZE
 *   characters.
 *
 * @param level
 *   A char pointer to an array of size JDBC_SERVER_LEVEL_SIZE (=4).
 */

void getServerLevel(char * level, char * serverBuildLevel) {

    memcpy(level, sgd.server_LevelID, JDBC_SERVER_LEVEL_SIZE);
    strncpy(serverBuildLevel, JDBCSERVER_BUILD_LEVEL_ID, JDBC_SERVER_BUILD_LEVEL_MAX_SIZE);

} /* getServerLevel */

/**
 * bumpClientCount
 *
 * This function increments sgd.clientCount by 1.
 */
void bumpClientCount() {
    sgd.clientCount ++;

} /* bumpClientCount */

/**
 * getClient_Assigned_KeepAliveState
 *
 * This function gets the Server approved keepAlive
 * state for a given JDBC Client from its Server Worker's
 * WDE entry. The value is returned as the function value.
 * The value indicates whether to have a keep alive thread
 * (CLIENT_KEEPALIVE_ON) or not to (CLIENT_KEEPALIVE_OFF).
 */
int getClient_Assigned_KeepAliveState(){
   return act_wdePtr->client_keepAlive;
}

/**
 * setServer_Approved_KeepAliveState
 *
 * This function sets the Server approved keepAlive
 * state for a given JDBC Client based on the clients
 * requested keepAlive state and the JDBC Server's
 * keepAlive configuration parameter setting. The value
 * that was set is returned as the function value so it
 * can be returned to the JDBC Client code for its usage.
 *
 * @param client_Requested_keepAliveState
 *   An integer value indicating whether the client
 *   wants a JDBC Driver connection keep alive thread
 *  (CLIENT_KEEPALIVE_ON), doesn't want a keep alive thread
 *   CLIENT_KEEPALIVE_OFF), or will accept the keep alive
 *   default (CLIENT_KEEPALIVE_DEFAULT).
 * @return
 *   An integer value that is passed back to the JDBC
 *   Driver connection that tells it whether to start
 *   a keep alive thread (CLIENT_KEEPALIVE_ON) or not
 *   to (CLIENT_KEEPALIVE_OFF).
 */
int setServer_Approved_KeepAliveState(int client_Requested_keepAliveState){

   switch (sgd.config_client_keepAlive){
      case CLIENT_KEEPALIVE_ALWAYS_ON:
         /*
          * JDBC Client connection will always use a keep alive
          * thread regardless of clients connection property value.
          */
         act_wdePtr->client_keepAlive = CLIENT_KEEPALIVE_ON;
         return CLIENT_KEEPALIVE_ON;

      case CLIENT_KEEPALIVE_ALWAYS_OFF:
         /*
          * JDBC Client connection will always not use a keep alive
          * thread regardless of clients connection property value.
          */
         act_wdePtr->client_keepAlive = CLIENT_KEEPALIVE_OFF;
         return CLIENT_KEEPALIVE_OFF;

      case CLIENT_KEEPALIVE_ON:
      case CLIENT_KEEPALIVE_OFF:
         /* Does the client want the Server's default keep alive thread action? */
         if (client_Requested_keepAliveState == CLIENT_KEEPALIVE_DEFAULT){
            /* Use the Server's configuration keep alive default value. */
            act_wdePtr->client_keepAlive = sgd.config_client_keepAlive;
            return sgd.config_client_keepAlive;
         }
         else {
            /* Let the client have the keep alive thread value they requested. */
            act_wdePtr->client_keepAlive = client_Requested_keepAliveState;
            return client_Requested_keepAliveState;
         }
      default:
            /*
             * SGD keep alive value not processed by switch
             * command - use CLIENT_KEEPALIVE_ON as default.
             */
            act_wdePtr->client_keepAlive = CLIENT_KEEPALIVE_ON;
            return CLIENT_KEEPALIVE_ON;
   }
}

/*
 * Function now_calling_RDMS
 *
 * This function sets the in_RDMS flag in the activites
 * wde entry to CALLING_RDMS.
 *
 * This function is necessary since we cannot place the code
 * to set the wde entry directly into the C-Interface code
 * without including additional JDBC Server .h files.
 *
 */
void now_calling_RDMS(){
   act_wdePtr->in_RDMS = CALLING_RDMS;                          \
}

/*
 * Function nolonger_calling_RDMS
 *
 * This function clears the in_RDMS flag in the activites
 * wde entry to NOT_CALLING_RDMS.
 *
 * This function is necessary since we cannot place the code
 * to set the wde entry directly into the C-Interface code
 * without including additional JDBC Server .h files.
 *
 */
void nolonger_calling_RDMS(){
   act_wdePtr->in_RDMS = NOT_CALLING_RDMS;                          \
}


/*
 * Function validate_client_driver_level_compatibility
 *
 * This function operates validates whether the client driver's level is
 * compatible with the server.
 *
 *
 * Note:   The call to validate_client_driver_level_compatibility() is
 *         made by doBeginThread() BEFORE the actual RDMS database
 *         thread is opened. The database thread is opened only
 *         after doBeginThread() recieves an acceptable result
 *         from the validate_client_driver_level_compatibility() call.
 *
 * Input parameters:
 *
 *     clientLevel  - pointer to the client level.  This is a char array of length 4.
 *                    Each char corresponds to the respective numeric value in (major, minor, feature, platform).
 *                    The char is completely numeric, not alphabetical, so it is a nine byte number rather than
 *                    a nine byte character representation of the numeric value value.
 *
 *     serverLevel  - pointer to the server level.  This is used to determine the maximum value of the range
 *                    of levels of clients that the server accepts.  Again, it is a char array of length 4
 *                    of the same significance as the client level.
 *
 * Returned value:
 *
 *       0     - The access requested is allowed (CLIENT_SERVER_COMPATIBILITY_ALLOWED).
 *               The JDBC Client's level falls within the range that the Server allows.
 *
 *       1     - The access is not allowed (CLIENT_SERVER_COMPATIBILITY_DENIED). The
 *               JDBC Client must have its connection request disallowed.
 *
 */
int validate_client_driver_level_compatibility(char *clientLevel, char *serverLevel) {
    /* We allow the client's level if and only if it falls within the server's range of accepted levels:
     * 1. Minimum level - one level minor level back, currently
     * 2. Maximum level - the server's level
     */
    union productLevel {
        struct {
            char major;
            char minor;
            char feature;
            char platform;
        } levelComponents;

        int levelAsInt;
    };

    union productLevel serverLevelAllowedMinimum_u;
    union productLevel serverLevelAllowedMaximum_u;
    union productLevel clientLevel_u;

    int accessAllowed = CLIENT_SERVER_COMPATIBILITY_ALLOWED; /* Assume compatibility allowed */

    if (debugInternal) {
        tracef("** Entered validate_client_driver_level_compatibility()\n");
    }

    /* Create an integer representation of the min server level for the range check, via union.
     *
     * The char representations of the 4 aspects of the server level overlay exactly onto an int:
     * char requires 9 bits on the OS2200, int requires 36 bits on the OS2200, and 4 * (numBitsInChar) = 36 = numBitsInInt
     *
     * Therefore, we overlay the chars on the int as follows, where bracketed numbers represent the range
     * of bits with 0 being the least significant bit:
     * [major]      - [27-35] (most significant bits)
     * [minor]      - [18-26]
     * [feature]    - [9-17]
     * [platform]   - [0-8] (least significant bits)
     */
    serverLevelAllowedMinimum_u.levelComponents.major = JDBCSERVER_MINIMUM_MAJOR_LEVEL_ID;         /* [major] */
    serverLevelAllowedMinimum_u.levelComponents.minor = JDBCSERVER_MINIMUM_MINOR_LEVEL_ID;         /* [minor] */
    serverLevelAllowedMinimum_u.levelComponents.feature = JDBCSERVER_MINIMUM_FEATURE_LEVEL_ID;     /* [feature] */
    serverLevelAllowedMinimum_u.levelComponents.platform = JDBCSERVER_MINIMUM_PLATFORM_LEVEL_ID;   /* [platform] */

    /* Create an integer representation of the max server level for the range check. */
    serverLevelAllowedMaximum_u.levelComponents.major = serverLevel[0];       /* [major] */
    serverLevelAllowedMaximum_u.levelComponents.minor = serverLevel[1];       /* [minor] */
    serverLevelAllowedMaximum_u.levelComponents.feature = serverLevel[2];     /* [feature] */
    serverLevelAllowedMaximum_u.levelComponents.platform = JDBCSERVER_MAXIMUM_PLATFORM_LEVEL_ID;    /* [platform] */

    /* Create an integer representation of the client level for the range check. */
    clientLevel_u.levelComponents.major = clientLevel[0];       /* [major] */
    clientLevel_u.levelComponents.minor = clientLevel[1];       /* [minor] */
    clientLevel_u.levelComponents.feature = clientLevel[2];     /* [feature] */
    clientLevel_u.levelComponents.platform = clientLevel[3];    /* [platform] */

    /* Check if the client level falls within the server's accepted range */
    if (clientLevel_u.levelAsInt < serverLevelAllowedMinimum_u.levelAsInt
            || clientLevel_u.levelAsInt > serverLevelAllowedMaximum_u.levelAsInt) {
        /* The client level is not allowed */
        accessAllowed = CLIENT_SERVER_COMPATIBILITY_DENIED;
    }

    if (debugInternal) {
        tracef("Inside validate_client_driver_level_compatibility: \n");
        tracef("serverMinimum: (%d, %d, %d, %d) %d\n",
                serverLevelAllowedMinimum_u.levelComponents.major,
                serverLevelAllowedMinimum_u.levelComponents.minor,
                serverLevelAllowedMinimum_u.levelComponents.feature,
                serverLevelAllowedMinimum_u.levelComponents.platform,
                serverLevelAllowedMinimum_u.levelAsInt);
        tracef("serverMaximum: (%d, %d, %d, %d) %d\n",
                    serverLevelAllowedMaximum_u.levelComponents.major,
                    serverLevelAllowedMaximum_u.levelComponents.minor,
                    serverLevelAllowedMaximum_u.levelComponents.feature,
                    serverLevelAllowedMaximum_u.levelComponents.platform,
                    serverLevelAllowedMaximum_u.levelAsInt);
        tracef("clientLevel: (%d, %d, %d, %d) %d\n",
                    clientLevel_u.levelComponents.major,
                    clientLevel_u.levelComponents.minor,
                    clientLevel_u.levelComponents.feature,
                    clientLevel_u.levelComponents.platform,
                    clientLevel_u.levelAsInt);
        tracef("** Exiting validate_client_driver_level_compatibility(): accessAllowed = %d\n", accessAllowed);
    }

    return accessAllowed;
}

/*
 * Function validate_user_thread_access_permission
 *
 * This function operates in a SECOPT-1 environment and
 * validates whether the specified user id has the appropriate
 * user access permissions on the JDBC user access security
 * file to allow that user id to Begin a thread to RDMS of
 * the thread access type desired.
 *
 * This routine calls function validate_userid_access_rights
 * which performs a SUVAL$ opeartion to determine the user id's
 * access rights on the JDBC user access security file.
 *
 * If there was an error obtaining the MSCON$ information on
 * the JDBC user access control security file, then we can't do
 * a SUVAL$ so just return an access denied status.
 *
 * Note:   The call to validate_user_thread_access_permission() is
 *         made by doBeginThread() BEFORE the actual RDMS database
 *         thread is opened. The database thread is opened only
 *         after doBeginThread() recieves an acceptable result
 *         from the validate_user_thread_access_permission() call.
 *
 * Input parameters:
 *
 *     userid_ptr   - pointer to the user id whose
 *                    permissions will be verified.
 *
 *     access_type  - type of RDMS thread requested. This
 *                    value is the integer that results from
 *                    the cast as an integer of a value of the
 *                    enum type access_type_code (in rsacli.h).
 *                    ( So 0=read, 1=update, 2=access).
 *
 *     project      - project id as 12 Fieldata characters.
 *
 *     account      - account number as 12 Fieldata characters.
 *
 * Returned value:
 *
 *       0     - The access requested is allowed (SERVER_USER_ACCESS_TYPE_ALLOWED).
 *               The JDBC Client can have an RDMS thread of the desired access type.
 *
 *       1     - The access is not allowed (SERVER_USER_ACCESS_TYPE_DENIED). The
 *               JDBC Client must have its connection request disallowed. ( This
 *               usually indicates either the user does not have desired access
 *               permissions, or the user id was not found, or another
 *               SUVAL$ error occurred. In all cases, the underlying JDBC
 *               Server code will have handled any security logging or
 *               JDBC Server log entries. )
 *
 *
 */
int validate_user_thread_access_permission(char * userid_ptr, int access_type,
                                           int * project, int * account){

    int userid_len;
    char userid[CHARS_IN_USER_ID+1]; /* room for a 12 char userid */
    int requested_access;

    /* test if we need to trace the procedure entry */
    if (debugInternal) {
        tracef("** Entered validate_user_thread_access_permission()\n");
    }

    /*
     * Check the SGD to see if JDBC user access control is set
     * to OFF or FUND (i.e., not JDBC). If so, just return
     * SERVER_USER_ACCESS_TYPE_ALLOWED since there is no user
     * access controls being imposed on the user-id at this time
     * or point in the RDMS database thread establishment process.
     * (Note: JDBC user access control for fundamental security will
     * be performed just after RDMS database thread acquisition.)
     */
    if (sgd.user_access_control != USER_ACCESS_CONTROL_JDBC){
        return SERVER_USER_ACCESS_TYPE_ALLOWED;
    }

    /*
     * Test if the MSCON$ information is valid ( test if the current MSCON$
     * packet has a MSCON$ status of MSCON_GOT_LEAD_ITEM). If not,
     * can't do SUVAL$ so just deny access.
     */
    /* printf("In server/c (sgd.MSCON_li_status[sgd.which_MSCON_li_pkt] & MSCON_ERROR_STATUS_MASK) = %012o\n",
           (sgd.MSCON_li_status[sgd.which_MSCON_li_pkt] & MSCON_ERROR_STATUS_MASK)); */

    if ((sgd.MSCON_li_status[sgd.which_MSCON_li_pkt] & MSCON_ERROR_STATUS_MASK)!= MSCON_GOT_LEAD_ITEM){
        return SERVER_USER_ACCESS_TYPE_DENIED;
    }

    /* transfer callers test userid into user id array */
    userid[0]='\0'; /* add null to start of string */
    strncat(userid,userid_ptr,CHARS_IN_USER_ID);
    userid_len=strlen(userid);
    if (userid_len < CHARS_IN_USER_ID){
        /* pad out to 12 characters with blanks */
        strncat(userid,"            ",CHARS_IN_USER_ID-userid_len);
    }

    if (DEBUG){
    printf("In validate_user_thread_access_permission(), userid= %s, userid_len=%d\n",userid, userid_len);
    }

    /*
     * Set requested access according to the accessType property value.
     */
    switch (access_type){
        case 0: {
            /* READ access desired */
            requested_access=ACC_READ;
            break;
        }
        case 1: {
            /* UPDATE access desired */
            requested_access=ACC_READ_WRITE;
            break;
        }
        case 2: {
            /* ACCESS access desired */
            requested_access=ACC_READ;
            break;
        }
        default: {
                /* bad access type - returned access denied */
                return SERVER_USER_ACCESS_TYPE_DENIED;
        }
    }

    /* Now call validate_userid_access_rights to validate
     * permissions and return. Use the current MSCON lead item packet for
     * JDBC user access security file.
     */
     return validate_userid_access_rights(userid, requested_access, &sgd.MSCON_li_pkt[sgd.which_MSCON_li_pkt],
                                          project, account);
}

/*
 * Function validate_user_thread_access_permission_fundamental
 *
 * This function operates in a fundamental environment and
 * validates whether the just opened RDMS database thread for
 * the specified user id has the appropriate user access
 * permissions to allow that user id to utilize the RDMS
 * database thread of the thread access type desired.
 *
 * Note:   In the fundamental security approach, the user access
 *         security check is made by doBeginThread() immediately
 *         AFTER the RDMS database thread is opened, with that
 *         database thread being immediately closed if the
 *         doBeginThread() code recieves an access denied status
 *         response.
 *
 * Input parameters:
 *
 *  access_type  - type of RDMS thread requested. This
 *                 value is the integer that results from
 *                 the cast as an integer of a value of the
 *                 enum type access_type_code (in rsacli.h).
 *                 ( So 0=read, 1=update, 2=access).
 *
 * Returned value:
 *
 *       0     - The access requested is allowed (SERVER_USER_ACCESS_TYPE_ALLOWED).
 *               The JDBC Client can have an RDMS thread of the desired access type.
 *
 *       1     - The access is not allowed (SERVER_USER_ACCESS_TYPE_DENIED). The
 *               JDBC Client must have its connection request disallowed. ( This
 *               usually indicates either the user does not have desired access
 *               permissions, or the user id was not found, or another
 *               SUVAL$ error occurred. In all cases, the underlying JDBC
 *               Server code will have handled any security logging or
 *               JDBC Server log entries. )
 *
 *
 */
int validate_user_thread_access_permission_fundamental(int access_type){
    int access_allowed = SERVER_USER_ACCESS_TYPE_ALLOWED; /* Assume RDMS access allowed */
    int rdms_status;
    int p1;
    rsa_error_code_type errorCode;
    int auxinfo;
    /* test if we need to trace the procedure entry */
    if (debugInternal) {
        tracef("** Entered validate_user_thread_access_permission_fundamental(): access_type = %d\n", access_type);
    }
    /*
     * Check the SGD to see if JDBC user access control is set
     * to OFF or JDBC (i.e., not FUND). If so, just return
     * SERVER_USER_ACCESS_TYPE_ALLOWED since there is no user
     * access controls being imposed on the user-id at this time
     * or point in the RDMS database thread establishment process.
     * (Note: JDBC user access control for SECOPT-1 security was
     * already performed just before RDMS database thread acquisition.)
     */
    if (sgd.user_access_control == USER_ACCESS_CONTROL_FUND){
        /*
         * JDBC User Access Control for Fundamental Security (FUND) is requested.
         * Based on the requested access according to the accessType property
         * value, perform the RDMS table selection request to determine if the
         * user has the necessary permissions and return.
         */
        switch (access_type){
            case 0:   /* READ access requested.   */
            case 2: { /* ACCESS access requested. */
                /*
                 * READ or ACCESS access to RDMS through JDBC is requested. This type
                 * of JDBC access to RDMS is allowed if the DBA has granted FETCH access
                 * for this userid on table JDBC$CATALOG2.JDBC$ACCESS. In that case,
                 * RDMS will return a no-find (error 6001) to the command. If the DBA did
                 * not grant the FETCH access, RDMS will return a error 6002 indicating
                 * the command is rejected.
                 */
                if (debugInternal) {
                    tracef("Issuing: RSA SQL to determine if Read access is allowed.\n");
                }
                call_rsa_p1("SELECT u FROM \"JDBC$CATALOG2\".\"JDBC$ACCESS\" INTO $P1;", errorCode, &auxinfo, &p1);
                if ((rdms_status = atoi(errorCode)) != 6001){
                    /* Didn't get a 6001 status, so can't have a READ or ACCESS database thread. */
                    access_allowed = SERVER_USER_ACCESS_TYPE_DENIED;
                }
                break;
            }
            case 1: { /* UPDATE access requested. */
                /*
                 * UPDATE access to RDMS through JDBC is requested. This type of JDBC access
                 * to RDMS is allowed if the DBA has granted UPDATE access for this userid on
                 * table JDBC$CATALOG2.JDBC$ACCESS. In that case, RDMS will return a 0 status
                 * to the command (and no actual record updates happened)). If the DBA did not
                 * grant the UPDATE access, RDMS will return a error 6002 indicating the command
                 * is rejected.
                 */
                if (debugInternal) {
                    tracef("Issuing: RSA SQL to determine if Update access is allowed.\n");
                }
                call_rsa("UPDATE \"JDBC$CATALOG2\".\"JDBC$ACCESS\" SET U=U+1 WHERE PK=-1;", errorCode,&auxinfo);
                if ((rdms_status = atoi(errorCode)) != 0){
                    /* Didn't get a 0 status, so can't have an UPDATE database thread. */
                    access_allowed = SERVER_USER_ACCESS_TYPE_DENIED;
                }
                break;
            }
            default: {
                    /* Bad access type requested - returned access denied */
                    access_allowed = SERVER_USER_ACCESS_TYPE_DENIED;
            }
        }
    }
    /* Test if we need to trace the procedure's results. */
    if (debugInternal){
        tracef("** Exiting validate_user_thread_access_permission_fundamental(): access_allowed = %d\n", access_allowed);
    }
    /* Return the result of the access check. */
    return access_allowed;
}
/*
 * Function add_a_ServerLogEntry
 *
 * This function provides access to the Server's
 * addServerLogEntry() procedure. It uses the same
 * parameter definitions.
 *
 * Parameters:
 *    logIndicator  - an integer value containing a set of indicator bits
 *                    that control the location and content of the log/trace
 *                    file message that's added. If LOG_NOTHING (0), no
 *                    logging is done.
 *    message       - character buffer containing the message (\0 terminated)
 */
void add_a_ServerLogEntry(int logIndicator, char *message){

    /* Check if logging needs to be done. */
    if (logIndicator != LOG_NOTHING){
        /* Just call the JDBC Server or JDBC XA Server's addServerLogEntry() procedure. */
        addServerLogEntry(logIndicator, message);
    }
}

/*
 * Function itoa
 *
 * This function converts an integer value into the character
 * array provided.
 *
 * Parameters:
 *
 * value    - integer value to convert to base 10 or base 16, or base 8
 * buffer   - a character buffer sufficently large to hold the
 *            converted 2200 integer value into characters plus
 *            sign and null terminator.
 * base     - base to convert to: either 8, 10, or 16
 *
 * Warning:   A buffer of 20 characters is sufficent and REQUIRED (
 *            always nice to have a bit of extra room).
 */
char * itoa(int value, char * buffer, int base){

    int num_chars;

    /* In first implementation, just use sprintf to do the
     * dirty work.  Later we may want to place a small conversion
     * routine here.
     */
    if (base == 10){
    num_chars=sprintf(buffer,"%i",value);
    return buffer;
    }
    else if (base == 16){
    num_chars=sprintf(buffer,"%x",value);
    return buffer;

    }
    else if (base == 8){
    num_chars=sprintf(buffer,"%o",value);
    return buffer;
    }
}

/*
 * Function generate_ID_level_mismatch_response_packet
 *
 * This function generates an error response packet in
 * the case when there is a mismatch in the request packet
 * id.  When this occurs, it is not clear if we have recieved
 * a request packet from a valid JDBC Driver of the correct
 * level or not.
 *
 * The Server Worker or XA Server Worker will not service the
 * calling client.
 *
 * However, just in case the request packet is from a JDBC
 * Driver of a different level, an error response packet
 * is generated to be sent back to notify the JDBC client
 * of the level mismatch. the error response contains
 * only server-side information that is placed by the
 * JDBC driver into an SQLException message that should
 * be helpfull for the JDBC Client to determine and resolve
 * the JDBC driver/server mismatch.
 *
 * Parameter:
 *  reqPktId  - The packet id value found in the request
 *              packet.
 *
 *
 * Returns:   A pointer to a response packet containing the
 *            desired error response information.
 */

char * generate_ID_level_mismatch_response_packet(int reqPktId){

    char * responsePktPtr;
    int messageLen;
    int responsePktLen;
    int bufferIndex;
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

#ifndef XABUILD /* Local Server */
#define MISMATCH_SERVER_INSERT1 "JDBC Server"
#else /* XA Server */
#define MISMATCH_SERVER_INSERT1 "JDBC XA Server"
#endif /* XA Server */

    /*
     * Construct the special response packet using common code
     * for either the JDBC Server or JDBC XA Server:
     *
     * 1) For the JDBC Server and JDBC XA Server, the actual
     *    response packet is allocated in the normal manner.
     *
     * 2) For forming and filling the special response
     *    packet, the SGD and WDE are not used. The response
     *    packet will contain the server id, build level
     *    string, and packet ids.
     */
    if (JDBCSERVER_PLATFORM_LEVEL_ID !=0){
      /* the platform level is not zero, display it also */
      sprintf(message,
          "%s level(build): %d.%d.%d.%d(%s), packet-ids are (%d/%d)",
          MISMATCH_SERVER_INSERT1,
          JDBCSERVER_MAJOR_LEVEL_ID, JDBCSERVER_MINOR_LEVEL_ID,
          JDBCSERVER_FEATURE_LEVEL_ID, JDBCSERVER_PLATFORM_LEVEL_ID,
          JDBCSERVER_BUILD_LEVEL_ID,
          reqPktId, REQUEST_VALIDATION);
    }
    else {
      /* the platform level is zero, no need to display it */
      sprintf(message,
          "%s level(build): %d.%d.%d(%s), packet-ids are (%d/%d)",
          MISMATCH_SERVER_INSERT1,
          JDBCSERVER_MAJOR_LEVEL_ID, JDBCSERVER_MINOR_LEVEL_ID,
          JDBCSERVER_FEATURE_LEVEL_ID, JDBCSERVER_BUILD_LEVEL_ID,
          reqPktId, REQUEST_VALIDATION);
     }

    /* Allocate the response packet buffer */
    messageLen = strlen(message);
    responsePktLen = MISMATCH_VAL_ID_RESPONSE_PKT_HDR_SIZE + messageLen;
    responsePktPtr = allocateResponsePacket(responsePktLen);

    /*
     * Fill in the Special Response Packet:
     *
     * Set the special packet validation/identification id's in the
     * the response packet header area so the JDBC Driver knows its
     * a mismatched  client/server response packet.
     */
    bufferIndex = 0;
    putIntIntoBytes(responsePktLen, responsePktPtr, &bufferIndex);
    responsePktPtr[4] = MISMATCH_CLIENT_SERVER_VALIDATION;           /* set special response packet validation id */
    responsePktPtr[5] = MISMATCH_CLIENT_SERVER_TASK_IDENTIFIER1;     /* set special response packet identification1 id */
    responsePktPtr[6] = MISMATCH_CLIENT_SERVER_TASK_IDENTIFIER2;     /* set special response packet identification2 id */
    responsePktPtr[7] = MISMATCH_CLIENT_SERVER_RESPONSE_PKT_FORMAT1; /* Currently format 1 (only one message string is sent back). */
    bufferIndex = 8;
    putIntIntoBytes(12, responsePktPtr, &bufferIndex);          /* offset to message length field */
    bufferIndex = 12;
    putIntIntoBytes(messageLen, responsePktPtr, &bufferIndex); /* length of message text */
    bufferIndex = 16;
    putASCIIStringIntoBytes(message, messageLen, responsePktPtr, &bufferIndex);

    /* test if we want an internal trace */
#if DEBUG
    /* Do a hexdump of the special response packet */
    dumpRawResponsePkt(responsePktPtr, responsePktLen);
#endif

    /* Return the response packet so it can be sent back. */
    return responsePktPtr;
}

/*
 * Function tracef_RsaSQL
 *
 * This function is called when the debugSQL flag is set on.  Otherwise,
 * normal C-Interface library handling of the RSA returned status will occur.
 *
 * This procedure places the executed SQL command into the Client's trace
 * file. Optionally, it will also places the SQL access path information that
 * is available when an EXPLAIN ALL ON has been done, and/or the error message
 * text associated with the SQL command execution.
 *
 * This function is called by the RSA() macro that replaces the regular
 * RSA() procedure call.
 *
 * Note 1: If error 7041 was returned, then there is explanation information available.
 * If debugSQLE is set, then the explanation code is traced and the error code and
 * auxinfo values are set to 0 and 0 respectively.  This is done since receiving a
 * 7041 indicates that the SQL command succeeded correctly and that explain
 * information is available. Since the explain information will be consumed for the
 * debug trace, the calling code will receive the correct indication that the SQL
 * command did, in fact, execute correctly. If it had  not, another error code would
 * have been returned.
 *
 * Note 2: If an error other than 7041, 6022, or 6023 is returned, then up to the first
 * RSA_MSG_VARS number of GET ERROR message lines will be saved in the C_int_structure in
 * the getError_msgLine[] array for later retrieval by the getCompressedRsaMessage() function.
 * This allows us to retrieve the current SQL command's error message lines now and have
 * the first RSA_MSG_VARS lines of them available to getCompressedRsaMessage() to return
 * as part of the response packet error message to be returned to the JDBC Client. This
 * mechanism is used only in debugSQL mode. In non-debugSQL mode, the getCompressedRsaMessage()
 * routine will actually perform the GET ERROR commands to retrieve the message lines (since
 * debugSQL was not set and tracef_RsaSQL was not called).
 *
 * Note 3: If an error 6022 or 6023 has occurred, this is not an error situation, but a
 * warning by RDMS that a stored procedure has been executed that returned result sets.
 * We cannot retrieve the actual RDMS provided warning message (6022 or 6023), because this
 * would cause the internally generated GET RESULTS CURSOR NAME request to fail.  So in these
 * cases, the appropriate warning message text is generated within this procedure and the 6022
 * and 6023 will be treated as good status values (like 0 and 7041 are treated).
 *
 * This code uses its own variables for its geterror call so that the calling
 * code does not have its original RSA() call affected by the SQL trace or the
 * Explanation retrieval.
 *
 * Input parameters:
 *     theSQLimagePtr    - a pointer to the SQL command string from the callers RSA() call
 *     theErrorCodePtr   - a pointer to the AuxInfo value from the callers RSA() call
 *     theAuxInfoPtr     - a pointer to the AuxInfo value from the callers RSA() call
 *
 * Returned Value:
 * None
 *
 */
#define RESULT_SET_CURSOR_WARNING_CODE 6022         /* The RDMS warning code indicating stored procedure results cursor */
#define RDMS_6022_MSG "*WARNING 6022: An invoked Stored Procedure returned a Result Set.\n"
#define RESULT_SET_CURSOR_BOUNDED_WARNING_CODE 6023 /* The RDMS warning code indicating stored procedure results cursor */
#define RDMS_6023_MSG "*WARNING 6023: An invoked Stored Procedure attempted to return too many Result Sets. (Only the suggested maximum remain available.)\n"
#define EXPLAIN_RSA_ERROR_CODE 7041                 /* The RDMS error code indicating EXPLAIN text is available */
#define MAX_NUMBER_OF_CALLS_TO_GETERROR_FOR_LINES 20 /* Each GetERROR call can return 6 lines    */
                                                     /* of information. Hence the max. number of */
                                                     /* to retrieve for trace purposes is 120.   */
#define SQLBUFF_LEN  (MAX_RSA_COMMAND_LEN+100) + \
                     ((MAX_RESULTSET_COLUMNS + (MAX_NUMBER_OF_CALLS_TO_GETERROR_FOR_LINES * RSA_MSG_VARS)) \
                     * (((MAX_RSA_MSG_LEN + 2 +3)/4)*4))
#define RDMS_6001_MSG "*ERROR 6001: A no find or end-of-cursor was detected by RDMS.\n"
#define LEN_BETWEEN_NL 132  /* maximum number of characters before a newline character is added */
void tracef_RsaSQL(int rsaCallType, char * theSQLimagePtr, rsa_error_code_type theErrorCodePtr, int * theAuxInfoPtr,
                   int *p_count, pv_array_type *p_ArrayPtr){

    int     n;
    int     i;
    int     nowDone;
    int     pvCount;
    char    digits[ITOA_BUFFERSIZE];
    int     eos;
    int     eosP;
    int     next;
    int     last_nl;
    char    *inPtr;
    char    *sqlBuffPtr;
    char    byte1;
    char    byte2;
    int     pv_traceImage_len;
    int     pv_traceImage_header_len;
    char    *pv_traceImagePtr;
    char    pv_traceImage[LEN_BETWEEN_NL+50];  /* plenty of extra room for tracing in octal format a $Pi containing Unicode characters. */
    char    sqlBuff[SQLBUFF_LEN];
    pv_array_type pvArray[RSA_MSG_VARS];
    char    msgLine[RSA_MSG_VARS][((MAX_RSA_MSG_LEN + 2 +3)/4)*4]; /* for messages - make sure each array entry is word aligned */
    rsa_error_code_type tempErrorCode;
    int     tempAuxInfo;
    char    firstChar;
    int     charMode = 0; /* 1 = ASCII                     */
                          /* 2 = Unicode ASCII             */
                          /* 0 = Assume Non-ASCII Unicode  */
    int outErrorCode;
    int sqlErrorCode;
    c_intDataStructure * pDS;


    /* get the actual error code as an integer */
    sqlErrorCode = atoi(theErrorCodePtr);
    outErrorCode = sqlErrorCode; /* Assume we will display actual error code. */

    /*
     * Now, test if debugSQLE has been set and if there is EXPLAIN information
     * available (indicated by an error code of EXPLAIN_RSA_ERROR_CODE (7041)).
     *
     * If so, trace the SQL showing an error of 0 since the SQL command must
     * have been legal (or another error code value would have been returned).
     * The Explain information will be retrieved by GET ERROR calls later.
     * Also clear the actual error code field, since an error code of 7041
     * means the actual command would have got an error code of 0 had not
     * EXPLAIN ALL ON not been set. We can leave auxinfo alone since it is
     * defined to be 0 in both cases and RDMS should have returned 0 (if not
     * leave the value they returned.)
     */
    if ((debugSQLE == BOOL_TRUE) && (sqlErrorCode == EXPLAIN_RSA_ERROR_CODE)){
      /* Adjust SQL command error info as if worked correctly and didn't have Explain information */
      outErrorCode = 0;
      strcpy(theErrorCodePtr,"0000");
    }

    /*
     * Test whether the SQL command was in ASCII or UNICODE.  Do this
     * by checking the first character of the SQL command.  This assumes
     * the first character of the SQL command is not '\0' since all SQL
     * commands submitted to RSA by RDMS JDBC will be ended with a ';'
     * character (hence the trailing '\0' will not be the first byte in
     * the command string).
     */
    firstChar = *theSQLimagePtr;    /* get the first character of SQL command */
    if (firstChar == 0) {
        /* Unicode ASCII    */
        charMode = 2;
    } else if ((firstChar >=32) && (firstChar<127)) {
        /* ASCII            */
        charMode = 1;
    }

    /*
     * Next, add the SQL command or RSA packet call image to the sqlBuff as either ASCII
     * or UNICODE, as we just determined. Block the images to about a maximum of 132
     * characters per line so the output trace images are somewhat constrained.  If there
     * are embedded '\n' in the SQL command, then they will be respected in the blocking
     * of the trace images.
     *
     * NOTICE: Since all the images in sqlBuff will be traced as a block, all the
     * images added to sqlBuff must end with a '\n'. During construction, a '\0'
     * is added after each added image in case its the last image added to sqlBuff.
     *
     * First add the command header description information based on the type of RSA
     * call: 0 == rsa() SQL command execution, not 0 == RSA PKT interface execution.
     */
    if (rsaCallType == 0){
        /* Call is one of those that uses rsa() to execute an SQL command. */
        strcpy(sqlBuff,"Executed SQL");
    } else {
        /* Call is one of those that uses an RSA packet interface routine. */
        strcpy(sqlBuff,"Executed RSA_PKT");
    }
    /*
     * Now add the rest of the command header description information. Do it quicker
     * if normal SQL/PKT execution (i.e., error code = 0, aux info = 0).
     */
    if (outErrorCode == 0 && *theAuxInfoPtr == 0){
        /* both are 0's, so add header in one strcpy() call */
        strcat(sqlBuff,"(err=0,aux=0): ");
    } else {
        /* Not 0's, so add the various pieces, converting the error code/aux info values. */
        strcat(sqlBuff,"(err=");
        strcat(sqlBuff,itoa(outErrorCode, digits, 10));
        strcat(sqlBuff,",aux=");
        strcat(sqlBuff,itoa(*theAuxInfoPtr, digits, 10));
        strcat(sqlBuff,"): ");
    }

    /*
     * Set up the sqlBuff transfer pointers and the length variables
     */
    next = strlen(sqlBuff);         /* Length of the header text */
    last_nl = 0;                    /* Counts number chars since last \n */
    sqlBuffPtr = &sqlBuff[0] + next; /* Place for adding next character to sqlBuff */
    inPtr = theSQLimagePtr;          /* pointer to SQL command string. */
    eos = 0;                         /* indicates we have'nt seen null terminator yet. Set to 1 when its found */

    /*
     * Now transfer the SQL command image over, converting the SQL
     * command based on length and character set (ASCII or UNICODE).
     * Remember the command will end with a \0.
     */
    if (charMode != 1){
       /* Assume Unicode Ascii or mixed Unicode. Remember command string is null terminated. */
       /* Loop until we find the null terminating character, or fill the sqlBuff array */
       while ((eos == 0) && (next < SQLBUFF_LEN - 9)){

           /* Test the left-side byte of the character to determine type of transfer */
           if (*inPtr == 0){
                /* Move to second byte and test if its in printable Ascii subset */
                inPtr++;

                if((*inPtr>=040) && (*inPtr<0177)){
                    /* Printable Ascii subset of Unicode, transfer the Ascii character. */
                    *sqlBuffPtr = *inPtr;
                    sqlBuffPtr++;
                    next++;
                    last_nl++;

                    /* Test if we need to add a '\n' now */
                    if (last_nl >= LEN_BETWEEN_NL){
                       *sqlBuffPtr = '\n';
                       sqlBuffPtr++;
                       next++;
                       last_nl = 0;
                     }

                } else if (*inPtr == '\0') {
                     /*
                      * Next byte is 0, so a trailing null located. Add a null,
                      * but do not bump the counters since we may be adding
                      * more characters later on. Also, if this is the last
                      * character added, then variable next will still have the
                      * proper string length.
                      * Indicate we have reached the end of the input string (eos = 1).
                      */
                      *sqlBuffPtr = '\0';
                      eos = 1;            /*   End of SQL command string found */

                } else if (*inPtr == '\n') {
                      /*
                       * Next byte is a newline character. Add a newline and
                       * bump the counters and update the count since last newline.
                       */
                      *sqlBuffPtr = '\n';
                      sqlBuffPtr++;
                      next++;
                      last_nl = 0;

                }else {
                    /* An unprintable character. Display it in printable octal format.*/
                    /* A format of %6.6o on byte2 handles display of both bytes, since byte1 is 0. */
                    sprintf(sqlBuffPtr,"\\%6.6o", *inPtr);
                    sqlBuffPtr = sqlBuffPtr + 7;
                    next=next+7;
                    last_nl = last_nl + 7;

                    /* Test if we need to add a '\n' now */
                    if (last_nl >= LEN_BETWEEN_NL){
                       *sqlBuffPtr = '\n';
                       sqlBuffPtr++;
                       next++;
                       last_nl = 0;
                     }
                }
           } else {
               /*
                * A Unicode character with a non-zero upper (first)
                * byte. Display in printable octal format.
                */
               byte1=*inPtr;
               inPtr++; /* Position to second byte of the Unicode character */
               byte2=*inPtr;
               sprintf(sqlBuffPtr,"\\%3.3o%3.3o", byte1, byte2); /* %6.6o handles display of both bytes */
               sqlBuffPtr = sqlBuffPtr + 7;
               next=next+7;
               last_nl = last_nl + 7;

               /* Test if we need to add a '\n' now */
               if (last_nl >= LEN_BETWEEN_NL){
                  *sqlBuffPtr = '\n';
                  sqlBuffPtr++;
                  next++;
                  last_nl = 0;
               }
           }

           /* Bump input character pointer the rest of the way to the next Unicode character. */
           inPtr++;

       } /* end Unicode character transfer while loop */
    } else {
       /* Assume Ascii. Remember command string is null terminated. */
       /* Loop until we find the null terminating character, or fill the sqlBuff array */
       while ((eos == 0) && (next < SQLBUFF_LEN - 9)){
          /* Test what type of Ascii character it is */
          if((*inPtr>=040) && (*inPtr<0177)){
              /* Printable Ascii, transfer the Ascii character. */
              *sqlBuffPtr = *inPtr;
              sqlBuffPtr++;
              next++;
              last_nl++;

              /* Test if we need to add a '\n' now */
              if (last_nl >= LEN_BETWEEN_NL){
                 *sqlBuffPtr = '\n';
                 sqlBuffPtr++;
                 next++;
                 last_nl = 0;
               }

          } else if (*inPtr == '\0') {
               /*
                * Next byte is '\0'; trailing null located. Add a null,
                * but do not bump the counters since we may be adding
                * more characters later on, and the null will be covered up.
                * Indicate we have reached the end of the input string (eos = 1).
                */
               *sqlBuffPtr = '\0';
               eos = 1;            /*   End of SQL command string found */

          } else if (*inPtr == '\n') {
               /*
                * Next byte is a newline character. Add a newline and
                * bump the counters and reset the count since last newline.
                */
               *sqlBuffPtr = '\n';
               sqlBuffPtr++;
               next++;
               last_nl = 0;

          }else {
                /* An unprintable character. Display it in printable octal format.*/
                sprintf(sqlBuffPtr,"\\%3.3o",*inPtr); /* %3.3o handles display of the byte */
                sqlBuffPtr = sqlBuffPtr + 4;
                next=next+4;
                last_nl = last_nl + 4;

                /* Test if we need to add a '\n' now */
                if (last_nl >= LEN_BETWEEN_NL){
                   *sqlBuffPtr = '\n';
                   sqlBuffPtr++;
                   next++;
                   last_nl = 0;
                 }
          }

          /* Now move to the next input string character. */
          inPtr++;

       } /* end Ascii character transfer while loop */
     }

     /*
      * Check if exited the conversion loops without adding a null,
      * or if we need to add a trailing '\n'.
      */
     if (eos == 0) {
       /*
        * SQL command image was just too big.  Trace it and reset to get
        * the error message text.  Too bad if they get separated in the
        * trace file by other concurrent trace. Good news is all trace will
        * be present in the trace file. Also, any GET ERROR messages are
        * up to 132 characters only in length, so we should have trace buffer
        * space to hold them once the SQL command has been traced.
        */
       *sqlBuffPtr = '\0';       /* Add a trailing null */
       tracef("%s\n", sqlBuff);  /* Need the trailing \n in the tracef template */

       /* Reset the sqlBuff transfer pointers and the length variables */
       eos = 0;
       sqlBuffPtr = &sqlBuff[0];
       *sqlBuffPtr = '\0';
       next = 0;
       last_nl = 0;

     } else if (last_nl != 0){
       /*
        * Need to add a trailing '\n' to end of the SQL command that was
        * transfered since it does not end in a '\n' (which it would if
        * last_nl == 0).
        */
       *sqlBuffPtr = '\n';
       sqlBuffPtr++;
       next++;
       last_nl = 0;
       *sqlBuffPtr = '\0';  /* place a '\0' at the end, just in case. */
     }

    /* Now determine if there are parameter values to display, and the output error code is not a 6001 (EOF). */
    if ((debugSQLP) && (outErrorCode != 6001) && (p_count != NULL) && (p_ArrayPtr != NULL)){
        /*
         * A trace of the $P (or ?) parameter values is desired, and the
         * p_count and array pointers are not null, so check if there are
         * parameters to trace.
         */
         if (*p_count != 0){
             /*
              * There are parameters. Loop over them, adding them to sqlBuff
              * for tracing. For Character parameters, include as much of its
              * characters as will fit into a 132 character trace line.
              */
             i=0;
             eos = 0;
             while ((eos == 0) && (i < *p_count)){
                /* test image length so far. if too large, display what we got and reset. */
                if (next >= (SQLBUFF_LEN - 9)){
                    /* Image length is too long. Output what we have and reset for the rest. */
                    tracef("%s\n", sqlBuff);  /* Need the trailing \n in the tracef template just in case */

                   /* Reset the sqlBuff transfer pointers and the length variables */
                   sqlBuffPtr = &sqlBuff[0];
                   *sqlBuffPtr = '\0';
                   next = 0;
                   last_nl = 0;
                }
                /* Add a trace line for the parameter. */
                pv_traceImage[0]='\0';

                switch (p_ArrayPtr[i].pv_type)
                {
                    case ucs_char:
                         if (i % 2){
                           /* This parameter might be an indicator variable, align the "i" accordingly */
                           if (i < 20){
                                sprintf(pv_traceImage,"    i/$P%d: char(%d) = \"",
                                        i+1, strlen(p_ArrayPtr[i].pv_addr));
                           } else if (i < 200){
                                sprintf(pv_traceImage,"     i/$P%d: char(%d) = \"",
                                        i+1, strlen(p_ArrayPtr[i].pv_addr));
                           } else {
                                sprintf(pv_traceImage,"      i/$P%d: char(%d) = \"",
                                        i+1, strlen(p_ArrayPtr[i].pv_addr));
                           }
                         } else {
                                sprintf(pv_traceImage,"  $P%d/$P%d: char(%d) = \"",
                                       (i/2)+1, i+1, strlen(p_ArrayPtr[i].pv_addr));
                         }
                        pv_traceImage_len = strlen(pv_traceImage); /* length so far */
                        pv_traceImage_header_len = pv_traceImage_len; /* save it for later testing on char length displayed */
                        pv_traceImagePtr = &pv_traceImage[pv_traceImage_len];
                        inPtr = p_ArrayPtr[i].pv_addr;
                        eosP = 0;
                        /* Copy value or up to a filled line's worth. assume next character might be displayed in octal (4 chars)*/
                         while ((eosP == 0) && (pv_traceImage_len < (LEN_BETWEEN_NL - 4))){
                            /* Test what type of Ascii character it is */
                            if((*inPtr>=040) && (*inPtr<0177)){
                                /* Printable Ascii, transfer the Ascii character. */
                                *pv_traceImagePtr = *inPtr;
                                pv_traceImagePtr++;
                                pv_traceImage_len++;
                                *pv_traceImagePtr = '\0';  /* always make sure there is a trailing '\0' */

                            } else if (*inPtr == '\0') {
                                 /*
                                  * Next byte is '\0'; trailing null located. Add a null,
                                  * but do not bump the counters since we may be adding
                                  * more characters later on, and the null will be covered up.
                                  * Indicate we have reached the end of the input string (eos = 1).
                                  */
                                 *pv_traceImagePtr = '\0';
                                 eosP = 1;            /*  End of parameter string found */

                            }else {
                                  /* An unprintable character. Display it in printable octal format.*/
                                  sprintf(pv_traceImagePtr,"\\%3.3o",*inPtr); /* %3.3o handles display of the byte */
                                  pv_traceImagePtr = pv_traceImagePtr + 4;
                                  pv_traceImage_len=pv_traceImage_len + 4;
                            }

                            /* Now move to the next input string character. */
                            inPtr++;

                         } /* end Ascii character transfer while loop */

                        /*
                         * Test if we need to add a ... at end of truncated char display. Note: We
                         * have allowed extra space for the "" and ... characters (i.e., the trace
                         * image can actually be a few characters longer than 132). Test if the
                         * ending '\0' was detected. If not, then we did not process the full
                         * Ascii character string.
                         */
                        if (eosP == 0 ){
                           /* Yes, add the ... ending */
                           strcat(pv_traceImage," ...\"\n");

                        } else {
                           /* No, just close off string. */
                           strcat(pv_traceImage,"\"\n");
                        }
                        break;
                    case ucs_unicode:
                         if (i % 2){
                           /* This parameter might be an indicator variable, align the "i" accordingly */
                           if (i < 20){
                                sprintf(pv_traceImage,"    i/$P%d: uchar(%d) = \"",
                                        i+1, strlenu(p_ArrayPtr[i].pv_addr));
                           } else if (i < 200){
                                sprintf(pv_traceImage,"     i/$P%d: uchar(%d) = \"",
                                        i+1, strlenu(p_ArrayPtr[i].pv_addr));
                           } else {
                                sprintf(pv_traceImage,"      i/$P%d: uchar(%d) = \"",
                                        i+1, strlenu(p_ArrayPtr[i].pv_addr));
                           }
                         } else {
                                sprintf(pv_traceImage,"  $P%d/$P%d: uchar(%d) = \"",
                                       (i/2)+1, i+1, strlenu(p_ArrayPtr[i].pv_addr));
                         }
                        pv_traceImage_len = strlen(pv_traceImage); /* length so far */
                        pv_traceImage_header_len = pv_traceImage_len; /* save it for later testing on char length displayed */
                        pv_traceImagePtr = &pv_traceImage[pv_traceImage_len];
                        inPtr = p_ArrayPtr[i].pv_addr;
                        eosP = 0;
                        /* Copy value or up to a filled line's worth. assume next character might be displayed in octal (7 chars)*/
                         while ((eosP == 0) && (pv_traceImage_len < (LEN_BETWEEN_NL - 7))){

                             /* Test the left-side byte of the character to determine type of transfer */
                             if (*inPtr == 0){
                                  /* Move to second byte and test if its in printable Ascii subset */
                                  inPtr++;

                                  if ((*inPtr >= 040) && (*inPtr < 0177)){
                                      /* Printable Ascii subset of Unicode, transfer the Ascii character. */
                                      *pv_traceImagePtr = *inPtr;
                                      pv_traceImagePtr++;
                                      pv_traceImage_len++;
                                      *pv_traceImagePtr = '\0';  /* always make sure there is a trailing '\0' */

                                  } else if (*inPtr == '\0') {
                                       /*
                                        * Next byte is 0, so a trailing null located. Add a null,
                                        * but do not bump the counters since we may be adding
                                        * more characters later on. Also, if this is the last
                                        * character added, then variable pv_traceImage_len will still
                                        * have the proper string length.
                                        * Indicate we have reached the end of the input string (eos = 1).
                                        */
                                        *pv_traceImagePtr = '\0';
                                        eosP = 1;            /*  End of parameter string found */

                                  }else {
                                      /* An unprintable character. Display it in printable octal format.*/
                                      /* A format of %6.6o on byte2 handles display of both bytes, since byte1 is 0. */
                                      sprintf(pv_traceImagePtr,"\\%6.6o",*inPtr);
                                      pv_traceImagePtr = pv_traceImagePtr + 7;
                                      pv_traceImage_len = pv_traceImage_len + 7;
                                  }
                             } else {
                                 /*
                                  * A Unicode character with a non-zero upper (first)
                                  * byte. Display in printable octal format.
                                  */
                                 byte1=*inPtr;
                                 inPtr++; /* Position to second byte of the Unicode character */
                                 byte2=*inPtr;
                                 sprintf(pv_traceImagePtr,"\\%3.3o%3.3o", byte1, byte2); /* %6.6o handles display of both bytes */
                                 pv_traceImagePtr = pv_traceImagePtr + 7;
                                 pv_traceImage_len = pv_traceImage_len+7;
                             }

                             /* Bump input character pointer the rest of the way to the next Unicode character. */
                             inPtr++;

                         } /* end Unicode character transfer while loop */

                        /*
                         * Test if we need to add a ... at end of truncated char display. Note: We
                         * have allowed extra space for the "" and ... characters (i.e., the trace
                         * image can actually be a few characters longer than 132). Test if the
                         * ending '\0' was detected. If not, then we did not process the full
                         * unicode character string.
                         */
                        if (eosP == 0 ){
                           /* Yes, add the ... ending */
                           strcat(pv_traceImage," ...\"\n");

                        } else {
                           /* No, just close off string. */
                           strcat(pv_traceImage,"\"\n");
                        }
                        break;
                    case ucs_int:
                        switch (p_ArrayPtr[i].pv_length)
                        {
                            case HALF_WORD:
                                if (i % 2){
                                  /* This parameter might be an indicator variable, align the "i" accordingly */
                                  if (i < 20){
                                       sprintf(pv_traceImage,"    i/$P%d: int(%d,%d) = %d\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((short *) p_ArrayPtr[i].pv_addr));
                                  } else if (i < 200){
                                       sprintf(pv_traceImage,"     i/$P%d: int(%d,%d) = %d\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((short *) p_ArrayPtr[i].pv_addr));
                                  } else {
                                       sprintf(pv_traceImage,"      i/$P%d: int(%d,%d) = %d\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((short *) p_ArrayPtr[i].pv_addr));
                                  }
                                } else {
                                       sprintf(pv_traceImage,"  $P%d/$P%d: int(%d,%d) = %d\n",
                                              (i/2)+1, i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((short *) p_ArrayPtr[i].pv_addr));
                                }
                                break;
                            case FULL_WORD:
                                if (i % 2){
                                  /* This parameter might be an indicator variable, align the "i" accordingly */
                                  if (i < 20){
                                       sprintf(pv_traceImage,"    i/$P%d: int(%d,%d) = %d\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                                  } else if (i < 200){
                                       sprintf(pv_traceImage,"     i/$P%d: int(%d,%d) = %d\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                                  } else {
                                       sprintf(pv_traceImage,"      i/$P%d: int(%d,%d) = %d\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                                  }
                                } else {
                                       sprintf(pv_traceImage,"  $P%d/$P%d: int(%d,%d) = %d\n",
                                              (i/2)+1, i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                                }
                                break;
                            case DOUBLE_WORD:
                                if (i % 2){
                                  /* This parameter might be an indicator variable, align the "i" accordingly */
                                  if (i < 20){
                                       sprintf(pv_traceImage,"    i/$P%d: int(%d,%d) = %-lld\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((long long *) p_ArrayPtr[i].pv_addr));
                                  } else if (i < 200){
                                       sprintf(pv_traceImage,"     i/$P%d: int(%d,%d) = %-lld\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((long long *) p_ArrayPtr[i].pv_addr));
                                  } else {
                                       sprintf(pv_traceImage,"      i/$P%d: int(%d,%d) = %-lld\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((long long *) p_ArrayPtr[i].pv_addr));
                                  }
                                } else {
                                       sprintf(pv_traceImage,"  $P%d/$P%d: int(%d,%d) = %-lld\n",
                                              (i/2)+1, i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((long long *) p_ArrayPtr[i].pv_addr));
                                }
                                break;
                        }
                        break;
                    case ucs_float:
                        switch (p_ArrayPtr[i].pv_length)
                        {
                            case FULL_WORD:
                                if (i % 2){
                                  /* This parameter might be an indicator variable, align the "i" accordingly */
                                  if (i < 20){
                                       sprintf(pv_traceImage,"    i/$P%d: float(%d,%d) = %-12.9G\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((float *) p_ArrayPtr[i].pv_addr));
                                  } else if (i < 200){
                                       sprintf(pv_traceImage,"     i/$P%d: float(%d,%d) = %-12.9G\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((float *) p_ArrayPtr[i].pv_addr));
                                  } else {
                                       sprintf(pv_traceImage,"      i/$P%d: float(%d,%d) = %-12.9G\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((float *) p_ArrayPtr[i].pv_addr));
                                  }
                                } else {
                                       sprintf(pv_traceImage,"  $P%d/$P%d: float(%d,%d) = %-12.9G\n",
                                              (i/2)+1, i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((float *) p_ArrayPtr[i].pv_addr));
                                }
                                break;
                            case DOUBLE_WORD:
                                if (i % 2){
                                  /* This parameter might be an indicator variable, align the "i" accordingly */
                                  if (i < 20){
                                       sprintf(pv_traceImage,"    i/$P%d: double(%d,%d) = %-21.18G\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((double *) p_ArrayPtr[i].pv_addr));
                                  } else if (i < 200){
                                       sprintf(pv_traceImage,"     i/$P%d: double(%d,%d) = %-21.18G\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((double *) p_ArrayPtr[i].pv_addr));
                                  } else {
                                       sprintf(pv_traceImage,"      i/$P%d: double(%d,%d) = %-21.18G\n",
                                               i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((double *) p_ArrayPtr[i].pv_addr));
                                  }
                                } else {
                                       sprintf(pv_traceImage,"  $P%d/$P%d: double(%d,%d) = %-21.18G\n",
                                              (i/2)+1, i+1, p_ArrayPtr[i].pv_precision,
                                               p_ArrayPtr[i].pv_scale, *((double *) p_ArrayPtr[i].pv_addr));
                                }
                                break;
                        }
                        break;
                    case ucs_blob_locator:
                        if (i % 2){
                          /* This parameter might be an indicator variable, align the "i" accordingly */
                          if (i < 20){
                               sprintf(pv_traceImage,"    i/$P%d: lob_id = %d\n",
                                       i+1, p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                          } else if (i < 200){
                               sprintf(pv_traceImage,"     i/$P%d: lob_id = %d\n",
                                       i+1, p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                          } else {
                               sprintf(pv_traceImage,"      i/$P%d: lob_id = %d\n",
                                       i+1, p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                          }
                        } else {
                               sprintf(pv_traceImage,"  $P%d/$P%d: lob_id = %d\n",
                                      (i/2)+1, i+1, p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, *((int *) p_ArrayPtr[i].pv_addr));
                        }
                        break;
                    default :
                        if (i % 2){
                          /* This parameter might be an indicator variable, align the "i" accordingly */
                          if (i < 20){
                               sprintf(pv_traceImage,"    i/$P%d: unsupported type(%d) %s, precision = %d, scale = %d, length = %d\n",
                                       i+1, p_ArrayPtr[i].pv_type,
                                       pvTypeString(p_ArrayPtr[i].pv_type), p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, p_ArrayPtr[i].pv_length);
                          } else if (i < 200){
                               sprintf(pv_traceImage,"     i/$P%d: unsupported type(%d) %s, precision = %d, scale = %d, length = %d\n",
                                       i+1, p_ArrayPtr[i].pv_type,
                                       pvTypeString(p_ArrayPtr[i].pv_type), p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, p_ArrayPtr[i].pv_length);
                          } else {
                               sprintf(pv_traceImage,"      i/$P%d: unsupported type(%d) %s, precision = %d, scale = %d, length = %d\n",
                                       i+1, p_ArrayPtr[i].pv_type,
                                       pvTypeString(p_ArrayPtr[i].pv_type), p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, p_ArrayPtr[i].pv_length);
                          }
                        } else {
                               sprintf(pv_traceImage,"  $P%d/$P%d: unsupported type(%d) %s, precision = %d, scale = %d, length = %d\n",
                                      (i/2)+1, i+1, p_ArrayPtr[i].pv_type,
                                       pvTypeString(p_ArrayPtr[i].pv_type), p_ArrayPtr[i].pv_precision,
                                       p_ArrayPtr[i].pv_scale, p_ArrayPtr[i].pv_length);
                        }
                        /* No break; needed here - end of main switch statement */
                } /* end of switch */

                /* Now add the pv trace image to the sqlBuff if there is space. Bump up the other counters */
                pv_traceImage_len = strlen(pv_traceImage);
                if ((next + pv_traceImage_len) >= SQLBUFF_LEN - 9){
                    /* Image length is too long. Output what we have and reset for the rest. */
                    tracef("%s\n", sqlBuff);  /* Need the trailing \n in the tracef template just in case */

                   /* Reset the sqlBuff transfer pointers and the length variables */
                   eos = 0;
                   sqlBuffPtr = &sqlBuff[0];
                   *sqlBuffPtr = '\0';
                   next = 0;
                   last_nl = 0;
                }

                /* Now add the pv trace line to sqlBuff and bump counters, etc. */
                strcat(sqlBuffPtr, pv_traceImage);
                next = next + pv_traceImage_len;
                sqlBuffPtr = sqlBuffPtr + pv_traceImage_len;
                *sqlBuffPtr = '\0';                          /* it should be there, but just in case */
                last_nl = 0;                                 /* pv trace image ended in a newline */

                /* bump i to next pv entry */
                i++;

             } /* end of while loop */

         } /* end of *p_count > 0 test */
    }  /* end of debugSQLP test */


    /*
     * Now, determine if we need to issue SQL GET ERROR commands to
     * retreive either error or explain information.  Once any additional
     * GET ERROR text is added, we can then perform the tracefdu() call
     * to send out the trace output.
     *
     * If the errorcode was 0, 6001, 6022, or 6023 we can handle this
     * quickly since no actual GET ERROR call need be made and there is
     * no additional message text to add. In the case of error 6001, we
     * will directly add the appropriate "6001 EOF" text. In the case
     * of warning 6022 or 6023, we will directly add the appropriate
     * warning message text.
     *
     * For all other errors (i.e., not 0, 6001, 6022, or 6023), we will
     * need to perform Get Error commands to retrieve either the error
     * messages or the Explain text (when the error code is 7041). In that
     * case, always copy/convert it to Unicode since we will  be retrieving
     * the error messages (or Explain text) using ucs-unicode as the pv_type.
     *
     * Also, the values of debugSQLE and debugSQLP will be tested to
     * determine if the additional Explain text or $Pi variable values
     * should be displayed.
     */

     /* So, first check for the simple error 0 case. If so, skip the GET ERROR stuff*/
    if (sqlErrorCode != 0){
      /*
       * The error code was not 0, so check if it was a 6001 error.
       * Handle error code 6001 specially. For all others we will
       * need to do GET ERROR calls.
       */
      if (sqlErrorCode == 6001){
          /*
           * Add Ascii form of the 6001 EOF message. Bump
           * sqlBuffPtr and length counter by size of message.
           */
          strcat(sqlBuffPtr, RDMS_6001_MSG);
          sqlBuffPtr = sqlBuffPtr + sizeof(RDMS_6001_MSG);
          next = next + sizeof(RDMS_6001_MSG);
          last_nl = 0; /* RDMS_6001_MSG ends in a '\n' */

      } else if (sqlErrorCode == 6022){
          /*
           * Add Ascii form of the 6022 EOF message. Bump
           * sqlBuffPtr and length counter by size of message.
           */
          strcat(sqlBuffPtr, RDMS_6022_MSG);
          sqlBuffPtr = sqlBuffPtr + sizeof(RDMS_6022_MSG);
          next = next + sizeof(RDMS_6022_MSG);
          last_nl = 0; /* RDMS_6022_MSG ends in a '\n' */
      } else if (sqlErrorCode == 6023){
          /*
           * Add Ascii form of the 6023 EOF message. Bump
           * sqlBuffPtr and length counter by size of message.
           */
          strcat(sqlBuffPtr, RDMS_6023_MSG);
          sqlBuffPtr = sqlBuffPtr + sizeof(RDMS_6023_MSG);
          next = next + sizeof(RDMS_6023_MSG);
          last_nl = 0; /* RDMS_6023_MSG ends in a '\n' */
      /* Now we have an error code other than 0, 6001, 6022, or 6023.  */
      } else {
          /*
           * We have an error number other than than 0, 6001, 6022,
           * or 6023. We need to call GET ERROR and append that
           * information to the SQL command being traced so it all
           * appears together in the trace file.
           *
           * If the error code is 7041 (Explain), there will be a
           * remark header line in the mesage lines retrieved. For
           * all other error codes, add a error message header image
           * to separate error message from the SQL command text.
           * Remember, it must end in a '\n' so image traces correctly.
           */
          if (sqlErrorCode != EXPLAIN_RSA_ERROR_CODE){
              /* Add Ascii form of the RDMS error message title line. */
             /* sprintf(sqlBuffPtr,"*ERROR  %4.4d:RDMS error message follows:\n", sqlErrorCode); */
              strcat(sqlBuffPtr,"*ERROR  ");
              sqlBuffPtr = sqlBuffPtr + 8;
              strcat(sqlBuffPtr,theErrorCodePtr); /* Use the actual error code characters - its faster than an int to char */
              sqlBuffPtr = sqlBuffPtr + 4;
              strcat(sqlBuffPtr,":RDMS error message follows:\n");
              sqlBuffPtr = sqlBuffPtr + 29; /* bump pointer by total length of message text just added */
              next = next + 8 + 4 + 29;     /* add total length of message text just added */
              last_nl = 0;                  /* message text just added ends in a '\n' */
          }

          /* Define RSA_MSG_VARS number of pv_array entries. */
          pvCount = RSA_MSG_VARS;

          /*
           * We need the c_int_structure pointer since we will have the
           * GET ERROR messages to place in the getError_msgLine[] array.
           */
          pDS = get_C_IntPtr();  /* get pointer to c_int_structure */

          /*
           * Loop until all GET ERROR lines are retrieved, or the loop has executed
           * MAX_NUMBER_OF_CALLS_TO_GETERROR_FOR_LINES number of times.
           */
          nowDone = BOOL_FALSE;
          for (n = 0; n < MAX_NUMBER_OF_CALLS_TO_GETERROR_FOR_LINES; n++){
             /* Fill in the pv_array to request the messages */
             for (i=0; i < pvCount; i++){
                 /* Set up a $Pi to get one Explain message line */
                 if (n == 0){
                   /*
                    * We need only initialize the descriptor parts on the first
                    * pass through the outer loop - these values will still be
                    * there on subsequent passes through the outer loop.
                    */
                   pvArray[i].pv_addr = msgLine[i];
                   pvArray[i].pv_type = ucs_unicode;
                   pvArray[i].pv_precision = 18;
                   pvArray[i].pv_length = MAX_RSA_MSG_LEN + 2; /* Allow for nulls */
                   pvArray[i].pv_scale = 0;
                   pvArray[i].pv_flags = 0040;

                 /*
                  * Also clear out the getError_msgLine[] array now, in case less
                  * than RSA_MSG_VARS lines are retrieved from GET ERROR
                  */
                 pDS->getError_msgLine[i][0] = '\0';
                 pDS->getError_msgLine[i][1] = '\0';
                 }

                 /* Always clear out the message area on each pass through the loop */
                 msgLine[i][0] = '\0';
                 msgLine[i][1] = '\0';

             } /* end for to set up pvArray */

             /*
              * Now get some Explain lines using "GETERROR INTO $P1,$P2,$P3,$P4,$P5,$P6" ,
              * where $pi are passed via set_params
              */
             set_params(&pvCount, pvArray);
             rsa(GET_ERROR, tempErrorCode, &tempAuxInfo);

             /* Now process the returned Explain lines */
             for (i=0; i < pvCount; i++){

                /* Check if there are no more lines, first UNICODE character is double '\0' */
                if (msgLine[i][0] == '\0' && msgLine[i][1] == '\0'){
                      /* All done, indicate and stop the nested loops */
                      nowDone = BOOL_TRUE;
                      break;
                }

                /*
                 * If we are processing the first RSA_MSG_VARS worth of messages (i.e., n ==0),
                 * then save the msgLine array messages in the getError_msgLine[] array.
                 */
                if (n == 0){
                  strcpyuu(pDS->getError_msgLine[i],msgLine[i]);
                }

                /*
                 * A error status message is present, so concatenate the
                 * message line into the trace message array.
                 */
                eos = 0;  /* indicates we have'nt seen null terminator yet. Set to 1 when its found */
                inPtr = msgLine[i];
                while ((eos==0) && (next < SQLBUFF_LEN - 9)){
                    /* Test the left-side byte of the character to determine type of transfer */
                    if (*inPtr == 0){
                         /* Move to second byte and test if its in printable Ascii subset */
                         inPtr++;

                         if((*inPtr>=040) && (*inPtr<0177)){
                             /* Printable Ascii subset of Unicode, transfer the Ascii character. */
                             *sqlBuffPtr = *inPtr;
                             sqlBuffPtr++;
                             next++;
                             last_nl++;

                             /* Test if we need to add a '\n' now */
                             if (last_nl >= LEN_BETWEEN_NL){
                                *sqlBuffPtr = '\n';
                                sqlBuffPtr++;
                                next++;
                                last_nl = 0;
                              }
                         } else if (*inPtr == '\0') {
                              /*
                               * Next byte is '\0'; trailing null located. Add a null,
                               * but do not bump the counters since we may be adding
                               * more characters later on, and the null will be covered up.
                               * Indicate we have reached the end of the input string (eos = 1).
                               */
                              *sqlBuffPtr = '\0';
                              eos = 1;            /*   End of SQL command string found */

                         } else if (*inPtr == '\n') {
                              /*
                               * Next byte is a newline character. Add a newline and
                               * bump the counters and reset the count since last newline.
                               */
                              *sqlBuffPtr = '\n';
                              sqlBuffPtr++;
                              next++;
                              last_nl = 0;

                         }else {
                             /* An unprintable character. Display it in printable octal format.*/
                             sprintf(sqlBuffPtr,"\\%6.6o",*inPtr); /* %6.6o handles display of both bytes */
                             sqlBuffPtr = sqlBuffPtr + 7;
                             next=next+7;
                             last_nl = last_nl + 7;

                             /* Test if we need to add a '\n' now */
                             if (last_nl >= LEN_BETWEEN_NL){
                                *sqlBuffPtr = '\n';
                                sqlBuffPtr++;
                                next++;
                                last_nl = 0;
                              }
                         }
                    } else {
                        /* A Unicode character with a non-zero upper byte. Display in printable octal format */
                        byte1=*inPtr;
                        inPtr++; /* get to second byte of Unicode character */
                        byte2=*inPtr;
                        sprintf(sqlBuffPtr,"\\%3.3o%3.3o", byte1, byte2); /* %6.6o handles display of both bytes */
                        sqlBuffPtr = sqlBuffPtr + 7;
                        next=next+7;
                        last_nl = last_nl + 7;

                        /* Test if we need to add a '\n' now */
                        if (last_nl >= LEN_BETWEEN_NL){
                           *sqlBuffPtr = '\n';
                           sqlBuffPtr++;
                           next++;
                           last_nl = 0;
                        }
                    }

                    /* Bump input character pointer the rest of the way to the next Unicode character. */
                    inPtr++;

                } /* end Unicode character transfer while loop */

                /* Check if exited the conversion loop without adding a null. */
                if (eos == 0) {
                  /*
                   * SQL command image was just too big.  Trace it and reset to get
                   * the error message text.  Too bad if they get separated in the
                   * trace file by other concurrent traces. Good news is all trace will
                   * be present in the trace file. Also, any GET ERROR messages are
                   * up to 132 characters only in length, so we should have trace buffer
                   * space to hold them once the SQL command has been traced.
                   */
                  *sqlBuffPtr = '\0';        /* Add a trailing null */
                  tracef("%s\n", sqlBuff);   /* Need the trailing \n in the tracef template */

                  /* After the tracef(), clear and reset sqlBuff and the pointer/counters */
                  sqlBuffPtr = &sqlBuff[0];
                  *sqlBuffPtr = '\0';
                  next =  0;
                  last_nl  = 0;

                } else if (last_nl != 0){
                  /*
                   * Need to add a trailing '\n' to end of each msgLine that was
                   * transfered since it does not end in a '\n' (which it would if
                   * last_nl == 0).
                   */
                  *sqlBuffPtr = '\n';
                  sqlBuffPtr++;
                  next++;
                  last_nl = 0;
                  *sqlBuffPtr = '\0';  /* place a '\0' at the end, just in case. */
                }

                /*
                 * Are we done? (Did we find the empty image after the Explain lines),
                 * or was there an error code (don't need to check it, its probably a 6001)
                 */
                if (nowDone || (atoi(tempErrorCode) != 0)){
                   break; /* Yes, exit the main For loop */
                }
             } /* end of For loop processing the pvCount number of retrieved msgLines */

             /* Are we done retrieving GET ERROR message lines? Test nowDone and final error code */
             if (nowDone || (atoi(tempErrorCode) != 0)){
                /* Yes, break outer For loop - no more GET ERROR calls needed. */
                   break; /* Yes, exit the outer For loop */
             }

          } /* end of For Loop, looking for all the Explain lines in groups of pvCount */
      }  /* end of checking for !=0 error codes and inserting GET ERROR text */
    } /* All done adding any GET ERROR messages */

    /*
     * We are done composing the trace lines into sqlBuff. Now trace the
     * SQL command and any additional explain or error message text and
     * return to the calling procedure. Notice: the sqlBuff always ends
     * in a '\n' followed by a '\0' .
     */
    tracef("%s", sqlBuff);   /* Don't need the trailing \n in this tracef template */
    return;
}

/*
 * Function tracef_Begin_Thread_PacketSQL
 *
 * This function is called when the debugSQL flag is set on.  Otherwise,
 * normal C-Interface library handling of the begin_thread() procedure
 * returned status will occur.
 *
 * This procedure places a copy of the the executed begin_thread() procedures
 * parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_begin_thread macro that replaces the
 * regular set_statement procedure call.
 */
void tracef_Begin_Thread_PacketSQL(char  *thread_name, char *application_name, char *userid, boolean data_conversion,
                  boolean indicators_not_supplied, access_type_code access_type, rec_option_code rec_option,
                  boolean suppress_msg, rsa_err_code_type error_code,int *aux_info){

    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"begin_thread(\"%12s\", \"%12s\", \"%12s\", %d, %d, %d, %d, %d);  [activity ids = %012o (%024llo), runid/grunid = %s/%s]",
            thread_name,application_name,userid,data_conversion,
            indicators_not_supplied,access_type,rec_option,suppress_msg,
            act_wdePtr->serverWorkerActivityID, act_wdePtr->serverWorkerUniqueActivityID,
            sgd.originalRunID, sgd.generatedRunID);

    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

#ifdef XABUILD  /* XA JDBC Server */
/*
 * Function tracef_reg_uds_resource
 *
 * This function is called when the debugSQL flag is set on.
 * Otherwise, normal C-Interface library handling of the
 * reg_uds_resource() procedure's returned status will occur.
 *
 * This procedure places a copy of the executed
 * reg_uds_resource() procedure's parameters and returned
 * error code into the Client's trace file.
 */
void tracef_reg_uds_resource(_uds_resource * reg_uds_resource_packetPtr, int x_status){

    int begin_thread_status;
    char error_code[5];
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    /* Construct a dummy "SQL" command string for tracing */
    sprintf(error_code,"%4d", x_status);
    begin_thread_status = reg_uds_resource_packetPtr->begin_thread_status;
    sprintf(psuedoCommand,"_reg_uds_resource(\"%12s\", \"%12s\", \"%12s\", %d, %d, %d, %d);  [activity ids = %012o (%024llo), runid/grunid = %s/%s]",
            reg_uds_resource_packetPtr->user_name, reg_uds_resource_packetPtr->thread_name,
            reg_uds_resource_packetPtr->app_name,  reg_uds_resource_packetPtr->recovery,
            reg_uds_resource_packetPtr->access, reg_uds_resource_packetPtr->appnum,
            reg_uds_resource_packetPtr->bdi, act_wdePtr->serverWorkerActivityID,
            act_wdePtr->serverWorkerUniqueActivityID, sgd.originalRunID,
            sgd.generatedRunID);

    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, (rsa_err_code_type)&error_code, &begin_thread_status, NULL, NULL);
}
#endif          /* XA JDBC Server */

/*
 * Function tracef_End_Thread_PacketSQL
 *
 * This function is called when the debugSQL or debugInternal flag is set on.
 * Otherwise, normal C-Interface library handling of the end_thread() procedures
 * returned status will occur.
 *
 * This procedure places a copy of the the executed end_thread() procedures
 * parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_end_thread macro that replaces the
 * regular end_thread procedure call.
 */
void tracef_End_Thread_PacketSQL(step_control_code step_control_option, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"end_thread(%d);", step_control_option);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Commit_Thread_PacketSQL
 *
 * This function is called when the debugSQL or debugInternal flag is set on.
 * Otherwise, normal C-Interface library handling of the commit_thread() procedures
 * returned status will occur.
 *
 * This procedure places a copy of the the executed commit_thread() procedures
 * parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_commit_thread macro that replaces the
 * regular commit_thread procedure call.
 */
void tracef_Commit_Thread_PacketSQL(step_control_code step_control_option, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"commit_thread(%d);", step_control_option);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Rollback_Thread_PacketSQL
 *
 * This function is called when the debugSQL or debugInternal flag is set on.
 * Otherwise, normal C-Interface library handling of the rollback_thread() procedures
 * returned status will occur.
 *
 * This procedure places a copy of the the executed rollback_thread() procedures
 * parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_rollback_thread macro that replaces the
 * regular rollback_thread procedure call.
 */
void tracef_Rollback_Thread_PacketSQL(rb_step_control_code rb_step_control_option, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"rollback_thread(%d);", rb_step_control_option);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Use_Default_PacketSQL
 *
 * This function is called when the debugSQL or debugInternal flag is set on.
 * Otherwise, normal C-Interface library handling of the use_default() procedures
 * returned status will occur.
 *
 * This procedure places a copy of the the executed use_default() procedures
 * parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_use_default macro that replaces the
 * regular use_default procedure call.
 */
void tracef_Use_Default_PacketSQL(name_type_code name_type, char *default_name,
                                  rsa_err_code_type error_code, int *aux_info){

    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"use_default(%d, \"%s\");", name_type, default_name);

    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Set_Statement_PacketSQL
 *
 * This function is called when the debugSQL or debugInternal flag is set on.
 * Otherwise, normal C-Interface library handling of the set_statement() procedures
 * returned status will occur.
 *
 * This procedure places a copy of the the executed set_statement() procedures
 * parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_set_statement macro that replaces the
 * regular set_statement procedure call.
 */
void tracef_Set_Statement_PacketSQL(set_statement_type set_command, set_value_type set_value,
                                    rsa_err_code_type error_code, int *aux_info){

    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"set_statement(%d, %d);", set_command, set_value);

    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Get_Results_Cursor_Name_PacketSQL
 *
 * This function is called when the debugSQL flag is set on.  Otherwise,
 * normal C-Interface library handling of the get_results_cursor_name()
 * procedure's returned status will occur.
 *
 * This procedure places a copy of the the executed get_results_cursor_name()
 * procedures parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_get_results_cursor_name macro that
 * replaces the regular get_results_cursor_name procedure call.
 */
void tracef_Get_Results_Cursor_Name_PacketSQL(char *cursor_name, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"get_results_cursor_name(); returned \"%s\"", cursor_name);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Execute_Prepared_PacketSQL
 *
 * This function is called when the debugSQL flag is set on.  Otherwise,
 * normal C-Interface library handling of the execute_prepared()
 * procedure's returned status will occur.
 *
 * This procedure places a copy of the the executed execute_prepared()
 * procedures parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_execute_prepared macro that
 * replaces the regular execute_prepared procedure call.
 */
void tracef_Execute_Prepared_PacketSQL(jdbc_section_hdr * jdbcSectionHdrPtr, int *p_count, pv_array_type *p_ArrayPtr,
                                       rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"execute_prepared(id=%d);", jdbcSectionHdrPtr->sectionVerifyId);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, p_count, p_ArrayPtr);
}

/*
 * Function tracef_Execute_Declared_PacketSQL
 *
 * This function is called when the debugSQL flag is set on.  Otherwise,
 * normal C-Interface library handling of the execute_declared()
 * procedure's returned status will occur.
 *
 * This procedure places a copy of the the executed execute_declared()
 * procedures parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_execute_declared macro that
 * replaces the regular execute_declared procedure call.
 */
void tracef_Execute_Declared_PacketSQL(jdbc_section_hdr * jdbcSectionHdrPtr, rsa_err_code_type error_code, int *aux_info, char *cursor_name){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"execute_declared(id=%d,\"%s\");", jdbcSectionHdrPtr->sectionVerifyId, cursor_name);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Drop_Cursor_PacketSQL
 *
 * This function is called when the debugSQL flag is set on.  Otherwise,
 * normal C-Interface library handling of the drop_cursor()
 * procedure's returned status will occur.
 *
 * This procedure places a copy of the the executed drop_cursor()
 * procedures parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_drop_cursor macro that
 * replaces the regular drop_cursor procedure call.
 */
void tracef_Drop_Cursor_PacketSQL(char *cursor_name, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"drop_cursor(\"%s\");", cursor_name);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Open_Cursor_PacketSQL
 *
 * This function is called when the debugSQL flag is set on.  Otherwise,
 * normal C-Interface library handling of the open_cursor()
 * procedure's returned status will occur.
 *
 * This procedure places a copy of the the executed open_cursor()
 * procedures parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_open_cursor macro that
 * replaces the regular open_cursor procedure call.
 */
void tracef_Open_Cursor_PacketSQL(char *cursor_name, int *p_count, pv_array_type *p_ArrayPtr,
                                       rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"open_cursor(\"%s\");", cursor_name);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, p_count, p_ArrayPtr);
}

/*
 * Function tracef_get_formatted_cursor_descriptionSQL
 *
 * This function is called when the debugInternal or debugSQL flag
 * is set on.  Otherwise, normal C-Interface library handling of
 * the get_formatted_cursor_description() procedure's returned
 * status will occur.
 *
 * This procedure places a copy of the the executed get_formatted_cursor_description()
 * procedure's parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_get_formatted_cursor_description macro
 * that replaces the regular get_formatted_cursor_description procedure call.
 */
void tracef_get_formatted_cursor_descriptionSQL(char *cursor_name, PCA *pca_ptr,
                int pca_size, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"get_formatted_cursor_description(\"%s\", %p, %d);",
                cursor_name, pca_ptr, pca_size);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Get_Parameter_Info_PacketSQL
 *
 * This function is called when the debugInternal or debugSQL flag
 * is set on.  Otherwise, normal C-Interface library handling of
 * the get_parameter_info() procedure's returned status will occur.
 *
 * This procedure places a copy of the the executed get_parameter_info()
 * procedure's parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_get_parameter_info macro
 * that replaces the regular get_parameter_info procedure call.
 */
void tracef_Get_Parameter_Info_PacketSQL(char * qualifier_name, char * routine_name, param_buf_type * param_buffer,
                                         int * num_params, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"get_parameter_info(\"%s\", \"%s\", %p, %d);",
                qualifier_name, routine_name, param_buffer, *num_params);
    /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 * Function tracef_Fetch_To_BufferSQL
 *
 * This function is called when the debugInternal or debugSQL flag
 * is set on.  Otherwise, normal C-Interface library handling of
 * the get_parameter_info() procedure's returned status will occur.
 *
 * This procedure places a copy of the the executed fetch_to_buffer()
 * procedure's parameters and returned error code into the Client's trace file.
 *
 * This function is called by the call_fetch_to_buffer macro
 * that replaces the regular fetch_to_buffer procedure call.
 */
void tracef_Fetch_To_BufferSQL(char * cursor_name, int * fetchbuffptr, int fetchbuffsize, int expsize,
                               int irb, int norecs, rsa_err_code_type error_code, int *aux_info){
    char psuedoCommand[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];
    /* Construct a dummy SQL command string for tracing */
    sprintf(psuedoCommand,"fetch_to_buffer(\"%s\", %p, %d, %d, %d, %d);",
            cursor_name, fetchbuffptr, fetchbuffsize, expsize, irb, norecs);

            /* Call tracef_RsaSQL() to do the actual tracing */
    tracef_RsaSQL(1, psuedoCommand, error_code, aux_info, NULL, NULL);
}

/*
 *  Function c_intDataStructure
 *
 *  Returns the pointer to the C_INT structure for this client.
 */

c_intDataStructure * get_C_IntPtr(){
   return (c_intDataStructure *)(act_wdePtr->c_int_pkt_ptr);
}

/*
 * Procedure initializeC_INT
 *
 * This routine initializes the C_INT of this Server Worker to initial
 * default values. This routine is always called by the Server Worker as part
 * of its initial JDBC Client handling process.
 *
 * Parameters:
 *
 * c_intPktPtr          - pointer to the C_INT Packet
 *
 */
void initializeC_INT(c_intDataStructure *c_intPktPtr){
    /* printf("Begin: Initializing c_int\n"); */
    c_intPktPtr->threadIsOpen = 0;
    c_intPktPtr->autoCommit = BOOL_TRUE; /* Initially assume auto-commit on, it may be set off later */
    c_intPktPtr->numberOfErrors = 0;
    c_intPktPtr->numberOfWarnings = 0;
    c_intPktPtr->messageAreaOffset = 0;
    c_intPktPtr->messageAreaOffsetSave = 0;
    c_intPktPtr->taskCode = 22;
    c_intPktPtr->firstStatement = NULL;
    c_intPktPtr->resultSetCurrentCursor = NULL;
    c_intPktPtr->firstMetaDataListPtr = NULL;
    c_intPktPtr->resultsetMetadataPtr = NULL;
    c_intPktPtr->sectionIdCounter = 0;
    c_intPktPtr->sectionVerifyId = 0;
    c_intPktPtr->metadataIdCounter = 0;
    c_intPktPtr->metadataVerifyId = 0;
    c_intPktPtr->sqlSectionPtr = NULL;
    c_intPktPtr->resultsetResponsePtr = NULL;
    c_intPktPtr->dbmdhandlerResponsePtr = NULL;
    c_intPktPtr->reqStatementId = 0 ;
    c_intPktPtr->ptr_show_column_size = NULL ;
    c_intPktPtr->ptr_show_decimal_digits = NULL ;
    c_intPktPtr->ptr_show_char_octet_length = NULL ;
    c_intPktPtr->dbmdhandlerIndex = 0 ;
    c_intPktPtr->rdmsFetchBufferPtr = NULL ;
    c_intPktPtr->trimmedBufferPtr = NULL ;
    c_intPktPtr->charset_ccs = 0;
    c_intPktPtr->charset_name[0] = '\0';
    c_intPktPtr->getError_msgLine[0][0]='\0';
    c_intPktPtr->getError_msgLine[0][1]='\0';

    /* printf("End: Initializing c_int\n"); */
    return;
}

/* Now define some of the utility routines */

/*
 * Dump "count" number of bytes in Hex. Precede the dump with an
 * optional header line. Remember to Put a "\n" in labelStr to
 * have heading on a separate line.
 */
void hexdumpWithHeading(char * ptr, int count, int num_per_line, char *labelStr, char *arg1, char *arg2){
   int i;
   int num_per_line_p1 = num_per_line + 1;

   /* Is there a heading line to display first? */
   if (labelStr != NULL){
      printf(labelStr, arg1, arg2);
   }

   for (i = 0; i < count; i++) {
      printf("%x ", ptr[i]);
      if ((i%(num_per_line_p1) == num_per_line) || (i == count-1)) {
          printf("\n");
      }
   }
}

/*
 * Dump "count" number of bytes in Hex. Precede the dump with a
 * header line.
 */
void hexdump(char * ptr, int count, int num_per_line)
{
   int i;
   int num_per_line_p1 = num_per_line + 1;

   printf("Hex dump: \n");
   for (i = 0; i < count; i++) {
      printf("%x ", ptr[i]);
      if ((i%(num_per_line_p1) == num_per_line) || (i == count-1)) {
          printf("\n");
      }
   }
}

/*
 * Dump "count" number of words in octal. Precede the dump with an
 * optional header line.
 */
void tracefDumpWords(void * ptr, int count, int num_per_line, char *labelStr, char *arg1, char *arg2) {
    int i;
    int num_per_line_p1 = num_per_line + 1;

    /* Is there is a trace image header to display first? */
    if (labelStr != NULL){
        tracef(labelStr, arg1, arg2); /* labelStr has any formatting constructs desired. */
    }

    /* Now dump the words in octal, num_perline words per line. */
    for (i = 0; i < count; i++) {
        tracef("%012o ", ((int *)ptr)[i]);
        if ((i%(num_per_line_p1) == num_per_line) || (i == count-1)) {
            tracef("\n");
        }
    }
}

/*
 * Dump "count" number of bytes in octal. Precede the dump with an
 * optional header line.
 */
void tracefDumpBytes(void * ptr, int count, int num_per_line, char *labelStr, char *arg1, char *arg2) {
    int i;
    int num_per_line_p1 = num_per_line + 1;

    /* Is there is a trace image header to display first? */
    if (labelStr != NULL){
        tracef(labelStr, arg1, arg2); /* labelStr has any formatting constructs desired. */
    }

    /* Now dump the bytes in octal, num_perline words per line. */
    for (i = 0; i < count; i++) {
        tracef("%03o ", ((char *)ptr)[i]);
        if ((i%(num_per_line_p1) == num_per_line) || (i == count-1)) {
            tracef("\n");
        }
    }
}

/*
 * Function dumpClientInfo
 *
 * This routine will dump to trace files the current Server Worker
 * data area, aka, the WDE.
 */
void dumpClientInfo() {

    char messageBuffer[5000];  /* Get sufficent space; getClientInfoTraceImages() can use about 1200 chars. */

    /* Get the state of the client's WDE, in the JDBC Server or XA JDBC Server, as one long set of trace lines. */
    getClientInfoTraceImages(messageBuffer, act_wdePtr);

    /* Now output all the information to the client trace file as one trace "image". */
    tracef("%s", messageBuffer);
}

/*
 * Procedure stop_all_tasks.
 *
 * This procedure calls tsk_stopall() to terminate JDBC Server now.  If a messageNumber
 * is specified, then a console message is sent before the JDMS Server tasks are
 * terminated.
 *
 * @param messageNumber
 *   A int specifying the message to send to the console.  If 0, then no console
 *   message in sent.  Otherwise, call getLocalizedMessageServerNoErrorNumber() to
 *   retrieve the message text and call sendcns() to send the message to the
 *   console.
 *
 */
void stop_all_tasks(int messageNumber, char* insert1, char * insert2) {

    char msgBuffer[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1]; /* Maximal room for an error message. */
    char* console_msg;

    if (messageNumber != 0) {

        console_msg = getLocalizedMessageServerNoErrorNumber(messageNumber, insert1 , insert2, 0, msgBuffer);

        sendcns(console_msg, strlen(console_msg));
    }

#if MONITOR_TMALLOC_USAGE /* Tmalloc/Tcalloc/Tfree tracking */
    /*
     * Before stopping all tasks, produce a malloc'ed memory tracking
     * summary for the activity local WDE used by the JDBC Server.
     */
    output_malloc_tracking_summary();
#endif                       /* Tmalloc/Tcalloc/Tfree tracking */

    tsk_stopall(); /* stop all remaining JDBC Server tasks. */
}

/*
 * closeRdmsConnection
 *
 * This routine will do a Rollback and End Thread command to RDMS.
 * The function is used when the client connection is gone but we still
 * have a database connection that must be cleaned up.  This routine
 * will simulate a client doing a ROLLBACK_THREAD_TASK and END_THREAD_TASK.
 *
 * cleanup - This flag is used to determine if additional cleanup
 *           of the activity is needed.
 *           0 - no clean up needed (the activity is going away)
 *           1 - clean up needed (the activity will be re-used)
 */
void closeRdmsConnection(int cleanup) {

    int auxinfo;
    rsa_err_code_type errCode;  /* pointer to char */

    c_intDataStructure * pDS ;
    pDS = get_C_IntPtr();

    if (debugInternal) {
        tracef("In closeRdmsConnection(): cleanup = %i\n", cleanup);
    }

    /* Do extra clean up of malloc'd memory if requested by calling argument. */
    if (cleanup != 0) {
        /*
         * Make sure that all the data structures maintained for this client
         * database connection are now freed. This has the potential to access RDMS,
         * so do it before we actually close the database thread. It is not necessary
         * to DROP any result set cursors that are still present since we are
         * going to end the database thread and they will go away then anyway.
         */
        freeAllStructures(0);   /* with dropCursor flag set to false */

        /* Release this user's PCA space. Let next user allocate their own PCA. */
        if (pDS->pca_ptr != NULL){
            Tfree(1, "server.c", pDS->pca_ptr);
            pDS->pca_ptr = NULL;
            pDS->pca_size = 0;
        }
    }

    /* Do a Rollback and an End Thread.  No need to check the status
     * at this time since we have to do both commands regardless of
     * the returned statuses. Status will be traced if debugInternal
     * or debugSQL is turned on.
     */
    now_calling_RDMS(); /* Set the in_RDMS flag */
    call_rollback_thread(rbdefault, errCode, &auxinfo); /* rollback with tracing */
    call_end_thread(nop, errCode, &auxinfo);             /* end thread with tracing */
    nolonger_calling_RDMS(); /* Clear the in_RDMS flag */

    /*
     * Insure RSA has free'd its URTS$TABLE space - we really
     * should not have to do this.
     */
    if (debugInternal) {
        tracef("Issuing: rsa$free()\n");
    }
    rsa$free();

    /* Indicate the database thread is no longer there (closed) */
    pDS->threadIsOpen = 0;
    act_wdePtr->openRdmsThread = BOOL_FALSE;

    return;
} /* closeRdmsConnection */

/*
 * getAsciiRsaErrorMessage
 *
 * Routine to get the detailed error message (ASCII) from RSA/RDMS for the last error encountered.
 * This routine is similar to getCompressedRsaMessage(), but returns a ASCII message and does not
 * check if tracing is enabled.  The error message retrieval is intended for local consumption on
 * the server and not for returning to the Java side.
 *
 * @param   msgBuffer         A char pointer to a message buffer large enough
 *                            to hold a message of RSA_MSG_VARS * (MAX_RSA_MSG_LEN + 2)/2
 *                            characters in length.
 *
 * @return  char *            The char pointer to the to a message buffer (msgBuffer)
 *                            with the compressed null-terminated error message.
 *
 */
char * getAsciiRsaErrorMessage(char * msgBuffer) {
    rsa_error_code_type tempErrorCode;
    int     tempAuxInfo;
    int i;
    int pvCount;
    char *errorMessage;
    char msgLine[RSA_MSG_VARS][(((MAX_RSA_MSG_LEN +2)/2 +3)/4)*4]; /* for messages - make sure each array entry is word aligned */
    pv_array_type pvArray[RSA_MSG_VARS];

    /* Get pointer to space for error message.  */
    errorMessage = msgBuffer;

    /* Null terminate the error message. */
    *errorMessage = '\0';
    /* Build the PV_Array */
    pvCount = RSA_MSG_VARS;     /*  12 lines */

    /* Fill in the pv_array to request the messages */
    for (i=0; i < pvCount; i++)
    {
        /* Request a message line */
        pvArray[i].pv_addr = msgLine[i];
        msgLine[i][0] = '\0';
        pvArray[i].pv_type = ucs_char;                  /* Retrieve as ASCII */
        pvArray[i].pv_precision = 9;
        pvArray[i].pv_length = (MAX_RSA_MSG_LEN + 2)/2; /* 132 + 1 for null terminator */
        pvArray[i].pv_scale = 0;
        pvArray[i].pv_flags = 0040;
    }

    /* Call set_params for the pv_array information */
    set_params(&pvCount, pvArray);

    /* Call rsa to obtain the error message one line at a time. */
    rsa(GET_ERROR, tempErrorCode, &tempAuxInfo);
    dumpPvArray(AFTER, "GET ERROR", "getRsaErrorMessage", pvCount, pvArray);

    /* We have the error messages in the msgLine[] array */
    for (i = 0; i < RSA_MSG_VARS; i++) {
        /* Check if there are no more lines */
        if (msgLine[i][0] == '\0') {
            break;
        }

        /* Copy message line into message array */
        strcat(errorMessage,&msgLine[i][0]);
        strcat(errorMessage,"\n");
    } /* End of for loop using msgLine[] array */


    return(errorMessage);
}

/*
 * Function addFeatureFlagsArgument()
 *
 * This function adds the Feature Flags as a 64-bit (8 byte) value.
 * Each feature flag is assigned 1 bit for the response packet.
 * This should allow plenty of space for future flags.
 *
 * The Feature Flags are defined on the 2200 side and passed to the
 * Java-side in the BeginThread response packet.
 *
 * @param buffer
 *   A pointer to the byte array where the value is inserted.
 *
 * @param bufferIndex
 *   A pointer to the offset in the byte array where the value is
 *   inserted. The offset is incremented so that it indexes past
 *   the inserted descriptor and value.

 */
void addFeatureFlagsArgument(char * buffer, int * bufferIndex){

    char ffArg[8];

    /* Build the Feature Flag Argument value */
    /* While, many of these values are not used anymore, but they are set to support older levels of drivers. */
    ffArg[0] = 0174;    /* 0111 1100 */
                                            /* bit 7: 0 = FFlag_15R1RsaLibrary */
                                            /* bit 6: 1 = FFlag_EnhancedBlobRetrieval */
                                            /* bit 5: 1 = FFlag_SupportsBlobWrites */
                                            /* bit 4: 1 = FFlag_SupportsSetReadOnly */
                                            /* bit 3: 1 = FFlag_SupportsSetCallerJDBC */
                                            /* bit 2: 1 = FFlag_SupportsSetAutoCommit */
                                            /* bit 1: 0 = FFlag_SupportsSetPassData */
                                            /* bit 0: 0 = FFlag_SupportsDropCloseCursor */

    ffArg[1] = 0360 +   /* 1111 0000 */
                                            /* bit 7: 1 = FFlag_SupportsGetFormattedDescription */
                                            /* bit 6: 1 = FFlag_DbmdCatalogChanges */
                                            /* bit 5: 1 = FFlag_SupportsUpdatableCursor */
                                            /* bit 4: 1 = FFlag_SetHoldableCursor */
                (sgd.FFlag_SupportsSQLSection << FF_SUPPORTSSQLSECTION_BIT) +                      /* set bit 3 */
                (sgd.FFlag_SupportsFetchToBuffer << FF_SUPPORTSFETCHTOBUFFER_BIT);                 /* set bit 2 */

    ffArg[2] =  0;
    ffArg[3] =  0;
    ffArg[4] =  0;
    ffArg[5] =  0;
    ffArg[6] =  0;
    ffArg[7] =  0;

    /* Add the Feature Flag bytes to buffer. */
    addArgDescriptor(BYTE_ARRAY_TYPE, 8, buffer, bufferIndex);
    putByteArrayIntoBytes(ffArg, 8, buffer, bufferIndex);
    if (debugInternal) {
        /* Display the Feature Flags in the response packet */
        tracef("Feature Flags settings: ffArg = 0%o,0%o,0%o,0%o,0%o,0%o,0%o,0%o\n",
                ffArg[0],ffArg[1],ffArg[2],ffArg[3],ffArg[4],ffArg[5],ffArg[6],ffArg[7]);
        tracef("    FF_SupportsSQLSection = %i\n", sgd.FFlag_SupportsSQLSection);
        tracef("    FF_SupportsFetchToBuffer = %i\n", sgd.FFlag_SupportsFetchToBuffer);
    }
}

/*
 * Feature Flag retrieval functions.
 *
 * These functions return the Feature Flag value, as an int,
 * from the SGD field that corresponds to the function's name.
 */
int getFF_SupportsSQLSection(){
    return sgd.FFlag_SupportsSQLSection;
}
int getFF_SupportsFetchToBuffer(){
    return sgd.FFlag_SupportsFetchToBuffer;
}

/**
 * addDateTime
 *
 * Adds a date/time stamp to the end of the character string.
 * Uses the getDateTime() routine which uses the DWTIME$ routine.
 * The format of the timestamp is (including the trailing space).
 * "YYYYMMDD HH:MM:SS.SSS "
 *
 * Parameter:
 * hdr - pointer to a character string ending in '\0' that
 *       will have the date/time appended.
 */

void addDateTime(char *hdrPtr) {

    char tempDateTimeStr[GETDATETIME_STRINGLEN];    /* 22 */

    /* Append the current timestamp to the passed char string. */
    strcat(hdrPtr,getDateTime(NULL, tempDateTimeStr));
    strcat(hdrPtr," ");     /* Add a traling space. */
    return;

 } /* addDateTime */
