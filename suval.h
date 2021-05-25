#ifndef _SUVAL_H_
#define _SUVAL_H_

/*
 * File: suval.h
 *
 * Header file used to provide access to the Defines, Functions,
 * and Procedures related to using and accessing the ER SUVAL$ Exec
 * service.  These services are used as part of the JDBC user access
 * security capability.
 */

/* JDBC Project Files */
#include "mscon.h"

/*
 *  The stuff for the SUVAL$ packet
 */

#define ER_SUVAL$ 0214

/* values for obj_type */

#define OBJ_TYPE_USERID 04
#define OBJ_TYPE_INTER  012
#define OBJ_TYPE_PRIV   013
#define OBJ_TYPE_FILE   014

/* values for obj_validation_flags */

#define OBJ_VAL_NAME_SUPPLIED_V1 0
#define OBJ_VAL_INFO_SUPPLIED_V1 1
#define OBJ_VAL_NAME_SUPPLIED_V2 2
#define OBJ_VAL_INFO_SUPPLIED_V2 3

/* values for func_code */

#define VALIDATE_CL     1
#define VALIDATE_CS     2
#define VALIDATE_ACCESS 3
#define VALIDATE_INTER  4
#define VALIDATE_PRIV   5

/* values for func_flags */
#define LOG_FAILURES_IN_SYSTEM_LOG 1

/* values for function validate_userid_access_rights() */
#define JDBC_READ_OR_ACCESS_ACCESS_REQUESTED   1    /* Remember access type of ACCESS is same as READ */
#define JDBC_UPDATE_ACCESS_REQUESTED           3
#define JDBC_USER_ACCESS_ALLOWED 0
#define JDBC_USER_ACCESS_DENIED  1
#define JDBC_INTERNAL_CODE_ERROR_ACCESS_DENIED -1


/* Security access bits in SUVAL$ subfunction. */
#define ACC_READ         001
#define ACC_WRITE        002
#define ACC_READ_WRITE   003

/* Error status values */
#define SUVAL_PACKET_VALID                    000
#define SUVAL_INVALID_PACKET_PARAMETERS       001
#define SUVAL_SUBJECT_NOT_FOUND               002
#define SUVAL_OBJECT_NOT_FOUND                003

/* Function return status values */
#define VALIDATION_SUCCEEDED                  000
#define ACC_READ_FAILED                       001
#define ACC_WRITE_FAILED                      002
#define ACC_READ_WRITE_FAILED                 003
#define VALIDATION_FAILED                     040
#define VALIDATION_NOT_PERFORMED_PACKET_ERROR 041

#define SUBJ_VAL_AGAINST_PACKET_USERID      0  /* values for subj_validation_type  */
#define SUBJ_TYPE_USERID                    4  /* value for subj_type  */
#define USER_ID                             4
#define FILE_OBJECT                         014
#define PROJ_AND_ACCOUNT_SUPPLIED           4
#define OBJ_SEC_INFO_SUPPLIED_PKT_FORMAT_2  3
#define ACCESS_VALIDATION                   3
#define LOG_FAILURES_IN_SYSTEM_LOG          1
#define MIN_SECURITY_WORDS                  7
#define OWNED_MASK                          04000
#define SECTOR_ADDRESSABLE_MASS_STORAGE     036
#define MODIFY_ACR                          000004
#define MODIFY_OWNER                        001000

#define START_WITH_LEAD     0
#define START_WITH_MAIN     1
#define CHARS_IN_USER_ID    12

#define READ_VALUE 020
#define MEMORY_COPY 1

#define FDSPACES  0050505050505
#define FDSLASHES 0747474747474

/* Structure definitions */

typedef struct
{
   int       subj                 : 6;
   int       subj_validation_type : 6;
   int       obj_type             : 6;
   int       obj_validation_flags : 6;
   int       num_func_wrds        : 6;
   int       err_status           : 6;
   uuint     subj_user_id[2];
   int       a_zero               : 12;
   int       file_flag_bits       : 12;
   int       acc_types_for_acr    : 12;
   int       b_zero               : 12;
   int       clearance_level      : 6;
   int       acr_address          : 18;
   int       acr_name;
   int       c_zero               : 18;
   int       compartment_set_info : 18;
   int       d_zero;
   uuint     owner[2];
   int       func_code            : 6;
   int       func_flags           : 6;
   int       func_return_status   : 12;
   int       sub_func             : 12;
   uuint     project[2];
   uuint     account[2];
}suval_pkt_struct;

typedef struct
{
   int        unused0[2];
   int        compartment_ver_num;
   int        compartment_bit_mask;
   int        unused1[24];
}csr_buf_struct;

int validate_userid_access_rights(char * test_userid_ptr, int rdms_access_type_wanted,
                                  mfd_lead_item_struct * li_ptr,
                                  int * project, int * account);

#endif /* _SUVAL_H_ */
