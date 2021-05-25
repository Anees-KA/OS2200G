#ifndef _MSCON_H_
#define _MSCON_H_
/*
 * File: mscon.h
 *
 * Header file used to provide access to the Defines, Functions,
 * and Procedures related to using and accessing the ER MSCON$ Exec
 * service.  These services are used as part of the JDBC user access
 * security capability.
 */

/* defines used for MSCON$ and the EXIST$ function */
#define MSCON_EXIST_FUNC     017
#define MSCON_EXIST_PKT_LEN  8     /* If this define is changed, dump code in ServerUASFM must be updated */
#define MFD_LEAD_ITEM_LEN    28    /* If this define is changed, dump code in ServerUASFM must be updated */
#define MFD_MAIN_ITEM_LEN    28
#define START_WITH_LEAD_ITEM 0
#define START_WITH_MAIN_ITEM 1
#define MSCON_BUFFER_LENGTH 034   /* buffer length of one 28 word sector */
#define MSCON_FILE_CYCLE 0        /* file cycle 0 (current cycle)*/
#define MSCON_START_SECTOR  0     /* start sector 0 */
#define MSCON_SECTOR_COUNT 1      /* sector count 1 */
#define MSCON_START_ITEM 0        /* start item 0 (lead item) */
#define ER_MSCON$ 0125
#define MSCON_FULL_FILENAME_LEN 24  /* 24 chars (12 char. qualifier and 12 char filename) */

/* ER MSCON$ status codes:                                                   */

#define NOT_EXIST      043      /* File does not exist in the MFD.           */
#define NO_SHARED_DIR  045      /* Shared MFD is configured but unavailable. */
#define INVALID_DIR_ID 046      /* Directory id is invalid.                  */
#define MSCON_GOT_LEAD_ITEM  01000000 /* OK status - got Lead item as desired (also indicates more items available if desired). */
#define MSCON_ERROR_STATUS_MASK 0777777000000 /* used to mask off H2 (the MSCON$ packet address) during error detection. */
#define MSCON_PACKET_ADDRESS_MASK 0000000777777 /* used to mask off H1 (the MSCON$ error status) leaving packet address. */


/* Structure definitions */

typedef unsigned int uuint;

typedef struct
{
   int         a_zero                 : 12;
   int         dir_index              : 6;
   int         func_code              : 18;
   uuint       qualifier[2];
   uuint       filename[2];
   int         buffer_length          : 12;
   int         unused                 : 12;
   int         f_cycle                : 12;
   /* int         unused_1               : 18; */
   /* int         buffer_address         : 18; */
   int         buffer_address;                    /* use a full word instead */
   int         start_sector           : 12;
   int         sector_count           : 12;
   int         start_item             : 6;
   int         unused_2               : 6;
   int         sectors[3][28];
}mscon_exist_pkt_struct;

typedef struct
{
   int            one_lead_item_sector : 1;
   int            indicator            : 3;
   int            unreferenced_bits    : 32;
   uuint          qual[2];
   uuint          file[2];
   uuint          proj_id[2];
   uuint          read_key;
   uuint          write_key;
   int            file_type            : 6;
   int            count                : 6;
   int            max_range            : 6;
   int            cur_range            : 6;
   int            highest_f_cycle      : 12;
   int            status_bits          : 12;
   int            shrd_file_index      : 6;
   int            num_security_wrds    : 6;
   int            access_type          : 12;
   int            flags                : 12;
   int            clearance_level      : 6;
   int            acr_address          : 18;
   uuint          acr_name;
   int            one                  : 6;
   int            compartment_set_info : 30;
   int            compartment_ver_num;
   uuint          owner[2];
   int            file_flag_bits       : 12;
   int            b_unreferenced       : 24;
   int            main_item_link[10];
}mfd_lead_item_struct;

#define LEAD_ITEM                           2
#define LEAD_ITEM_EXT                       0
#define MAIN_ITEM                           4

#endif /* _MSCON_H_ */
