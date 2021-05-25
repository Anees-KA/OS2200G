/**
 * File: Suval.c.
 *
 * Procedures that comprise the JDBC Server's support for
 * JDBC user access security.  The primary functionality of
 * the code within is to call SUVAL$ to perform a userid
 * permission check against the JDBC user access security
 * file onto which the customers security officer has
 * associated an ACR with userid's granted appropriate
 * permissions.  This information is used to determine if
 * a userid can open an RDMS thread of a particular access
 * type (read, update, access).

 *
 * This code may utilize other code in the C-Interface library to accomplish
 * its required tasks.
 *
 */
/* Standard C header files and OS2200 System Library files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ertran.h>
#include <time.h>
#include <ctype.h>
#include <tsqueue.h>
#include <task.h>
#include <sysutil.h>
#include <universal.h>

/* JDBC Project Files */
#include "packet.h" /* Include crosses the c-interface/server boundary */
#include "server-msg.h" /* Include crosses the c-interface/server boundary */
#include "ProcessTask.h"
#include "Server.h"
#include "ServerConsol.h"
#include "ServerLog.h"

extern serverGlobalData sgd;                /* The Server Global Data (SGD),*/
                                            /* visible to all Server activities. */

/*
 * Function validate_userid_access_rights
 *
 * This function retrieves the JDBC access rights information for a given
 * userid with respect to the RDMS application group for which access is
 * being attempted using SUVAL$. It tests if the desired access right is
 * available for that userid on the JDBC user access security file. If
 * the desired access permission is available for the tested userid, then a
 * error status of 0 (OK) is returned. If the desired access permission is
 * not available for the tested userid or the userid is not found, then
 * a error status of 1 is returned, indicating no access rights are allowed.
 *
 * This procedure assumes that the JDBC user access security file has already
 * been processed by get_JDBC_Security_File_Info() to obtain its owner userid
 * and compartment information and that that information is available in the
 * mfd lead item packet passed in as an argument to this function.
 *
 * Input Parameters:
 *
 *       test_userid_ptr    - The userid attempting to access RDMS through JDBC.
 *                            This must point to a 12 character ASCII string,
 *                            blank padded if needed. If the userid string is not
 *                            12 characters in length, the SUVAL$ request will
 *                            fail with an error status of 2 and a function
 *                            return status of 041 (packet error).
 *
 *       rdms_access_type_wanted - a value of 1 (bit 0) means READ or ACCESS access (read) requested,
 *                                 a value of 3 (bits 1 and 2) means UPDATE access (read and write) requested,
 *                                 all other values are considered an error, and
 *                                 error status of 1 will be returned.
 *
 *      lead_item_ptr       - a pointer to the lead item packet for the JDBC
 *                            user access security file.  This information is
 *                            obtained using the MSCON$ EXIST$ function.  The
 *                            owner userid and compartment number will be
 *                            extracted fromthis packet and used by the SUVAL$.
 *
 *       project            - userid's project id to use for SUVAL$ validation test.
 *
 *       account            - userid's account number to use for SUVAL$ validation test.
 *
 * Returned Function Value:
 *
 *       0     - The access requested is allowed (JDBC_USER_ACCESS_ALLOWED),
 *       1     - The access is not allowed (JDBC_USER_ACCESS_DENIED)
 *               This usually indicates either the user does not have desired
 *               access permissions, or the user id was not found, or another
 *               SUVAL$ error occurred.  If needed (e.g. SUVAL$ calling error
 *               by the JDBC Server code), this access attempt is logged
 *               in the JDBC Log file in addition to the system log.
 *
 */
int validate_userid_access_rights(char * test_userid_ptr, int rdms_access_type_wanted,
                                  mfd_lead_item_struct * li_ptr,
                                  int * project, int * account){
    suval_pkt_struct suval_pkt;
    csr_buf_struct csr_buf_pkt;
    int em_va;                            /* will hold EM VA to be converted to BM VA*/
    int error_status;
    int bad_error_statuses;               /* will hold both error and function return statuses */
    int inregs[1];  /* input register values */
    int outregs[1]; /* output register values */
    int ercode=ER_SUVAL$;     /* the ER to do is SUVAL$*/
    int inputval=0400000000000;   /* input is expected in A0  */
    int outputval=0;              /* no output is expected, returned results are in packet */
    int scratch;                  /* dummy item so ucsgeneral can obtain right L,BDI */

    /* used to dump SUVAL Pkt */
    int * dump;
    char digits[ITOA_BUFFERSIZE];
    char message[SERVER_ERROR_MESSAGE_BUFFER_LENGTH+1];

    if (DEBUG){
       printf("in validate_userid_access_rights(), test_userid is %s, rdms_access_type_wanted is %d\n",
       test_userid_ptr, rdms_access_type_wanted);
    }

    /*
     * Perform a SUVAL security check to determine if the desired user
     * has the appropriate user access permission to Begin a Thread for
     * the desired access (READ or UPDATE).
     *
     * This operation consists of four steps:
     *
     * Step 1) Check the type of access required. If not allowed, return
     *         JDBC_USER_ACCESS_DENIED to indicate access is denied.
     *
     * Step 2) Build a SUVAL$ packet, placing the JDBC access security
     *         file name, its owner id, its compartment info, the ACR
     *         name from the MSCON$ packet, and the userid for which
     *         access rights are to be obtained. This information comes
     *         from the input parameters.
     *
     * Step 3) Do an ER SUVAL$ to retrieve the allowed access rights for
     *         the tested userid. If the SUVAL$ returns in error, the error
     *         is logged, and JDBC_USER_ACCESS_DENIED is returned since we
     *         cannot determine if user-id has adequate access permissions
     *         (safe thing to do because of error).
     *
     * Step 4) Check the returned status.  If access is allowed, return 0 else
     *         return an error status of 1. If the test userid is not found
     *         attached to the file and ACR, then an error status of 1 is
     *         returned. So either the caller gets an JDBC_USER_ACCESS_ALLOWED
     *         or JDBC_USER_ACCESS_DENIED back.
     */

    /* test if legal rdms_access_type_wanted values */
    if ((rdms_access_type_wanted != JDBC_READ_OR_ACCESS_ACCESS_REQUESTED ) &&
        (rdms_access_type_wanted != JDBC_UPDATE_ACCESS_REQUESTED )){
       /* illegal value, return 1 - access denied */
       return JDBC_USER_ACCESS_DENIED;
    }

    /* Build the SUVAL$ packet */
    memset(&suval_pkt, 0, sizeof(suval_pkt));  /* first clear packet to 0's */

    suval_pkt.subj = SUBJ_TYPE_USERID;
    suval_pkt.subj_validation_type = PROJ_AND_ACCOUNT_SUPPLIED;
    suval_pkt.obj_type = FILE_OBJECT;
    suval_pkt.obj_validation_flags = OBJ_SEC_INFO_SUPPLIED_PKT_FORMAT_2;
    suval_pkt.num_func_wrds = 1;
    suval_pkt.err_status = 0;
    ascfd(&suval_pkt.subj_user_id, test_userid_ptr, 12);
    suval_pkt.a_zero = 0;
    suval_pkt.file_flag_bits = li_ptr->file_flag_bits;
    suval_pkt.acc_types_for_acr = li_ptr->access_type;
    suval_pkt.b_zero = 0;
    suval_pkt.clearance_level = li_ptr->clearance_level;
    suval_pkt.acr_address = li_ptr->acr_address;
    suval_pkt.acr_name = li_ptr->acr_name;
    suval_pkt.c_zero = 0;
    suval_pkt.compartment_set_info = (unsigned short) &csr_buf_pkt;
    suval_pkt.d_zero = 0;
    suval_pkt.owner[0] = li_ptr->owner[0];
    suval_pkt.owner[1] = li_ptr->owner[1];
    suval_pkt.func_code = ACCESS_VALIDATION;
    suval_pkt.func_flags = LOG_FAILURES_IN_SYSTEM_LOG;
    suval_pkt.func_return_status = 0;
    suval_pkt.account[0] = account[0];
    suval_pkt.account[1] = account[1];
    suval_pkt.project[0] = project[0];
    suval_pkt.project[1] = project[1];
    suval_pkt.sub_func = rdms_access_type_wanted; /* place the access desired by the test userid */

    /* place compartment info into a substructure for the SUVAL$ to access */
    memset(&csr_buf_pkt, 0, sizeof csr_buf_pkt);
    csr_buf_pkt.compartment_bit_mask = li_ptr->compartment_set_info;
    csr_buf_pkt.compartment_ver_num = li_ptr->compartment_ver_num;

    /* form the (pktlength,pktaddr) value to be in A0 */
    em_va = (int) &suval_pkt;
    ucsemtobm(&em_va, &inregs[0], &error_status);
    if (error_status !=0){
        /* Internal error, this should not happen. */
        printf("EM to BM in SUVAL set up failed, error_status is %d\n",error_status);
        return JDBC_INTERNAL_CODE_ERROR_ACCESS_DENIED;
    }
    inregs[0] &= 0777777;  /* keep only address offset in H2 */
    inregs[0] |= (sizeof(suval_pkt) / BYTES_PER_WORD) << 18; /* mask in SUVAL$ packet size in H1 */

   /*
    * Test if we are in SGD debug mode, specifically tracing the SUVAL$ calls. If
    * so, trace out the SUVAL$ packet before the ER SUVAL$.  Note: this trace is
    * done to the server log file since this is where the messages are sent
    * regarding user access (as determined by the SUVAL$ information).
    */
   if (TEST_SGD_DEBUG_TRACE_SUVAL){
        /* dump SUVAL$ packet (15 words ) in octal in the JDBC Server Trace file. */
        dump = (int *)&suval_pkt;
        sprintf(message,"SUVAL$ test being performed for userid %s, with packet :\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o\n",
                test_userid_ptr, dump[0], dump[1], dump[2], dump[3], dump[4], dump[5], dump[6],
                dump[7], dump[8], dump[9], dump[10], dump[11], dump[12], dump[13], dump[14]);
        addServerLogEntry(SERVER_TRACES, message);
    }

    /* Do the SUVAL$ to retrieve test userid access rights */
    ucsgeneral(&ercode, &inputval, &outputval, inregs, outregs, &scratch, &scratch); /* do the SUVAL$ */

    /*
     * Save the new bad error and function
     * status pair for later checking (place error status in H1 and
     * function return status in H2).
     */
    bad_error_statuses = (suval_pkt.err_status << 18) | suval_pkt.func_return_status;

   /* Test if we are in SGD debug mode, specifically tracing the SUVAL$ calls */
   if (TEST_SGD_DEBUG_TRACE_SUVAL){
        getMessageWithoutNumber(SERVER_SUVAL_ERROR_DETECTED, itoa(bad_error_statuses, digits, 8),
                                test_userid_ptr, SERVER_LOGTRACES);
              /* 5386 JDBC Server SUVAL$ access now receives octal status codes = {0},  */
              /* tested user id was {1} */

        /* dump SUVAL$ packet (15 words ) in octal in the JDBC Server Trace file. */
        dump = (int *)&suval_pkt;
        sprintf(message,"SUVAL$ err and func return status codes: %03o,%05o, and packet :\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o\n",
                suval_pkt.err_status, suval_pkt.func_return_status, dump[0], dump[1], dump[2], dump[3], dump[4], dump[5], dump[6],
                dump[7], dump[8], dump[9], dump[10], dump[11], dump[12], dump[13], dump[14]);
        addServerLogEntry(SERVER_TRACES, message);
    }

    /*
     * Test if there was an error executing SUVAL$.  Some of the error combinations
     * indicate a problem with the userid or the JDBC access security file
     * (SYS$LIB$*JDBC$RDMSnn.) information.
     *
     * First check for the common cases which we handle with a simple access
     * accepted or denied, which we return to caller.
     *
     * In all other cases, log the SUVAL$ error since it should not occur and
     * is likely either an internal problem with our code, or the Security
     * officer has caused the problem in the way they manipulated the JDBC
     * user access security file or the ACR while the JDBC Server is running
     * (i.e., we are looking at timing problem while the security officer
     * changes access permissions, etc.
     *
     * Common cases checked first. If its one of these cases, each case must
     * finish by returning the appropriate status to calling procedure.
     */
    if (suval_pkt.err_status == SUVAL_PACKET_VALID){
        /* test function return status and return access accepted/denied to caller*/
        switch (suval_pkt.func_return_status){
            case VALIDATION_SUCCEEDED:{
                /* Both error_status and func_return_status were 0, so
                 * let the caller know they have access permission.
                 */
                return JDBC_USER_ACCESS_ALLOWED;
            }
            case ACC_READ_FAILED:
            case ACC_WRITE_FAILED:
            case ACC_READ_WRITE_FAILED:{
                /*
                 * One of the needed access permissions was denied, so
                 * let the caller know they do not have access permission.
                 */
                return JDBC_USER_ACCESS_DENIED;
            }
         }
    }
    /* The last simple case is where the userid does not exist */
    if ((suval_pkt.err_status == SUVAL_SUBJECT_NOT_FOUND) &&
        (suval_pkt.func_return_status == VALIDATION_NOT_PERFORMED_PACKET_ERROR)){
            /*
             * The user id was not found, so let the caller know
             * they do not have any access permission by default.
             */
            return JDBC_USER_ACCESS_DENIED;
    }

    /*
     * Now come all the rest of the cases.  These should not occur in normal
     * operation.  Log them (in the manner that follows) and return an access
     * denied status to the caller.
     *
     * If we are in debugging mode, trace the error and the SUVAL$ packet into
     * the JDBC Server's trace file.
     *
     * For the JDBC Server Log file, we will log the error the first time it
     * occurs, but not on subsequent repeated occurances until the error codes
     * once again change. This records the type of event happening, and it
     * also handles the cases where the security officer is changing the ACR
     * values and that work is not fully completed.
     *
     */
    /* printf("We got here in Suval/c :  suval_pkt.err_status=%d, suval_pkt.func_return_status =%d, bad_error_statuses=%d, suval_pkt is %d words long\n",
              suval_pkt.err_status, suval_pkt.func_return_status , bad_error_statuses,sizeof(suval_pkt)); */

    /*
     * Log only the occurrances when the error
     * conditions change.
     */
    if (bad_error_statuses != sgd.SUVAL_bad_error_statuses){
        /* are we debugging Server? If so, trace all SUVAL$ failures to Trace file */
        if (sgd.debug==BOOL_TRUE){
            getMessageWithoutNumber(SERVER_SUVAL_ERROR_DETECTED, itoa(bad_error_statuses, digits, 8),
                                    test_userid_ptr, SERVER_LOGTRACES);
                  /* 5386 JDBC Server SUVAL$ access now receives octal status codes = {0},  */
                  /* tested user id was {1} */

            /* dump SUVAL$ packet (15 words ) in octal in the JDBC Server Trace file. */
            dump = (int *)&suval_pkt;
        sprintf(message,"SUVAL$ err and func return status codes: %03o,%05o, and packet :\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o %012o %012o %012o\n  %012o %012o %012o\n",
                suval_pkt.err_status, suval_pkt.func_return_status, dump[0], dump[1], dump[2], dump[3], dump[4], dump[5], dump[6],
                    dump[7], dump[8], dump[9], dump[10], dump[11], dump[12], dump[13], dump[14]);
            addServerLogEntry(SERVER_TRACES, message);
        }
        else {
            /* Just place a Log file entry */
            getMessageWithoutNumber(SERVER_SUVAL_ERROR_DETECTED, itoa(bad_error_statuses, digits, 8),
                                    test_userid_ptr, SERVER_LOGS);
                  /* 5386 JDBC Server SUVAL$ access now receives SUVAL$ octal status codes = {0}, */
                  /* tested user id was {2} */
        }
        /* Now set new current bad status. */
        sgd.SUVAL_bad_error_statuses = bad_error_statuses;
    }

    /* Now return an access denied indicator if we reach this point */
    return JDBC_USER_ACCESS_DENIED;
}
