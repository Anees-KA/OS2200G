#ifndef _SERVER_UASFM_H_
#define _SERVER_UASFM_H_
/*
 * File: ServerUASFM.h
 *
 * Header file used to provide access to the Functions and
 * Procedures related to using and accessing the ER MSCON$ Exec
 * service.  These services are used as part of the JDBC user access
 * security capability.
 *
 * Header file corresponding to code file ServerUASFM.c.
 */

/* JDBC Project Files */
#include "mscon.h"

void user_Access_Security_File_MSCON();

/* status values for function get_MSCON_exist_lead_item() */
#define JDBC_MSCON_EXIST_WAS_SUCCESSFUL 0
#define JDBC_INTERNAL_CODE_ERROR_EMTOBM1_MSCON_EXIST_FAILED -1
#define JDBC_INTERNAL_CODE_ERROR_EMTOBM2_MSCON_EXIST_FAILED -2

int get_MSCON_exist_lead_item(char * filename_ptr, mscon_exist_pkt_struct * mscon_pkt_ptr,
                              mfd_lead_item_struct * li_ptr);

void set_sgd_MSCON_information();

#endif /* _SERVER_UASFM_H_ */
