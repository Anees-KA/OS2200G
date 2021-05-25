/**********************************************************************
 *  *  *  *  *  U  *  N  *  I  *  S  *  Y  *  S  *  *  *  *  *  *  *  *
 *                                                                    *
 *               Copyright (c) 2019 Unisys Corporation.               *
 *               All rights reserved.                                 *
 *               UNISYS CONFIDENTIAL                                  *
 *                                                                    *
 *  C  *  O  *  R  *  P  *  O  *  R  *  A  *  T  *  I  *  O  *  N  *  *
 **********************************************************************/
#ifndef __JDMS_SEC_IDENT_H_
#define __JDMS_SEC_IDENT_H_

/* JDBC Project Files */
#include "secuseridef.h" /* Include crosses the c-interface/server boundary */

/******************************************************************************

 Include File Name:

   "sec$ident$.h"

 Description:

   This include file is contains definitions of structs and constants
   used by the interface SEC$IDENT$.

 Requires:

   None.

 ******************************************************************************/

#pragma standard SEC$IDENT$
#pragma interface SEC$IDENT$

/*************************************************************************

 Structure Name:

   sec_ident_packet

 Purpose:

   This structure describes the structure for the call packet for SEC$IDENT$.

 Field Descriptions:

   version:           version of the call packet
   request_type:      indicate the call to retrieve or replace

 *************************************************************************/
enum {
   not_initialized = 0,
   activity_ident = 1,
   run_ident = 2,
   ss_caller_ident = 3,
   step_ident = 4
} ;

enum {
   illegal_ident_version = 0,
   ident_version_1 = 1,
   ident_version_2 = 2
} ;

typedef struct {
   int version;
   int request_type;
} sec_ident_packet;

typedef struct
{
   int        version;
   char       ident_userid[12];
   char       *authenticate_handle;
   int        reserverd[3];
   char       ident_project[12];
   char       ident_account[12];
   int        reserved017 :5;
   int        commpressed_bit :1;
   int        commpressed_mask :30;
   int        ident_clearance_level :6;
   int        reservedi020 :30;
   int        ident_dloc_privilrge :1;
   int        ident_impersonate_in_cgy :1;
   int        ident_impersonate_in_sst :1;
   int        reserved021 :33;
} sec_ident_buffer;

exec_status_t SEC$IDENT$(sec_ident_packet *, sec_ident_buffer *);

#endif /* ifndef __JDMS_SEC_IDENT_H_ */

