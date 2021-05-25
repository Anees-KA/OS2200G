#ifndef _SERVER_H_
#define _SERVER_H_

/*
 * File: Server.h
 *
 * Header file used to provide access to the Defines, Functions,
 * and Procedures in the JDBC Server code that are necessary for
 * proper operation of the C-Interface Library code. This allows
 * proper isolation of the JDBC Server and Server Worker activity
 * code from the C-Interface Library code.
 *
 * This header file either references header and code files in the
 * JDBC Server source files (as #includes, or as specific header
 * information directly, as determined by the nature of the
 * information being shared (e.g. function prototypes, define constants)
 * with the C-Interface Library code.
 *
 * To reference the #include files, the C-Interface Library
 * compilations must establish a US$PFn use name on the JDBC
 * Server Source file directory.
 *
 * Note: Server.h needs defines from packet.h, so the coder should
 * have a #include "packet.h" preceding the #include "Server.h"
 * in their code.
 */

/*
 * Provide access to the JDBC Server defines needed for this interfacing
 * code (e.g. Allocate_Response_Packet routine, debug prefix length, etc.
 */
/* Standard C header files and OS2200 System Library files */
#include <errno.h>
#include "reguds.h"
#include "rsa.h"
#include "tx.h"

/* JDBC Project Files */
#include "statement.h" /* Include crosses the c-interface/server boundary */
#include "AllocRespPkt.h"

/* Needed for use with the DWTIME$ date-time */
void UCSDWTIME$(long long *);
void UCSCNVTIM$CC(long long *, char *); /* Takes arg 1, a DWTIME$ value, and returns as arg2, a formatted date-time string. */
#pragma interface UCSDWTIME$
#pragma interface UCSCNVTIM$CC

/*
 * Define intercept macro's for the various RSA packet entry
 * points that allows for debugging their SQL operations by
 * a JDBC client operating in the C-Interface code. The support
 * for these macro's is in the Server.c code
 * provided by the JDBC Server or JDBC XA Server
 * form of the RDMS JDBC product.
 *
 * Note: There are macros for all of the RDMS/RSA packet
 * interfaces. However, only those packet interfaces that are
 * actually used have the corresponding tracef_XXXX_PacketSQL
 * routines defined (e.g., currently there are no set_output_id_code()
 * function calls, hence there is no tracef_Set_Output_Id_Code_PacketSQL()
 * routine defined at this time). When the need arises, a C compilation
 * error will indicate the need to implement the appropriate
 * tracef_XXXX_PacketSQL routine.
 */

#define call_rsa(theSQLimage,theErrorCode,theAuxInfo)                                       \
           /* First, perform the SQL operation via the rsa() interface. */                  \
             rsa(theSQLimage,theErrorCode,theAuxInfo);                                      \
           /* Tracef the SQL command operation if we are debugging SQL's */                 \
             if (debugInternal || debugSQL){                                                \
             /* Yes, trace the SQL operation and any EXPLAIN in the client's trace file. */ \
             tracef_RsaSQL(0, theSQLimage, theErrorCode, theAuxInfo, NULL, NULL);           \
             } /* End of call_rsa() macro */

#define call_rsa_p1(theSQLimage,theErrorCode,theAuxInfo,p1)                                 \
           /* First, perform the SQL operation via the rsa() interface. */                  \
             rsa(theSQLimage,theErrorCode,theAuxInfo,p1);                                   \
           /* Tracef the SQL command operation if we are debugging SQL's */                 \
             if (debugInternal || debugSQL){                                                \
             /* Yes, trace the SQL operation and any EXPLAIN in the client's trace file. */ \
             tracef_RsaSQL(0, theSQLimage, theErrorCode, theAuxInfo, NULL, NULL);           \
             } /* End of call_rsa() macro */

#define call_rsa_rse(theSQLimage,theErrorCode,theAuxInfo,theP1Ptr,theP1NullPtr,theP2Ptr,theP2NullPtr,theP3Ptr,theP3NullPtr,theP4Ptr,theP4NullPtr,theP5Ptr,theP5NullPtr)  \
           /* First, perform the SQL FETCH operation via the rsa() interface. Remember that */ \
           /* we have to provide parameters for null indicators because of begin_thread()   */ \
           /* option for null indication, even though the result cursor entries do not have */ \
           /* null values.                                                                  */ \
           rsa(theSQLimage,theErrorCode,theAuxInfo,theP1Ptr,theP1NullPtr,theP2Ptr,theP2NullPtr,theP3Ptr,theP3NullPtr,theP4Ptr,theP4NullPtr,theP5Ptr,theP5NullPtr); \
           /* Tracef the SQL command operation if we are debugging SQL's */                    \
             if (debugInternal || debugSQL){                                                   \
             /* Yes, trace the SQL operation and any EXPLAIN in the client's trace file. */    \
             tracef_RsaSQL(0, theSQLimage, theErrorCode, theAuxInfo, NULL, NULL);              \
             } /* End of call_rsa_rse() macro */

#define call_rsa_pv(theSQLimage,theErrorCode,theAuxInfo,pvcount,pv_array)                   \
           /* First, perform the SQL operation via the rsa() interface. */                  \
             rsa(theSQLimage,theErrorCode,theAuxInfo);                                      \
           /* Tracef the SQL command operation if we are debugging SQL's */                 \
             if (debugInternal || debugSQL){                                                \
             /* Yes, trace the SQL operation, set_param variables, and */                   \
             /* any EXPLAIN in the client's trace file.                */                   \
             tracef_RsaSQL(0, theSQLimage, theErrorCode, theAuxInfo, pvcount, pv_array);    \
             } /* End of call_rsa_pv() macro */

#define call_rsa_pv_h(theSQLimage,theErrorCode,theAuxInfo,pvcount,pv_array,handle)          \
             /* First, perform the SQL operation via the rsa() interface. */                \
             rsa(theSQLimage,theErrorCode,theAuxInfo);                                      \
             /* Test for error 2226 (a stored procedure execution without preceding Set Stmt Handle) */   \
             if (atoi(theErrorCode) == 2226){                                               \
                /* Set the handle, set_params (if any), and re-execute the command. */      \
                call_set_stmt_handle(handle);                                               \
                if (pvcount != NULL){                                                       \
                    /* PV count present, so re-set the SQL command parameters */            \
                    set_params(pvcount, pv_array);                                          \
                }                                                                           \
                rsa(theSQLimage,theErrorCode,theAuxInfo);                                   \
             }                                                                              \
             /* Tracef the SQL command operation if we are debugging SQL's */               \
             if (debugInternal || debugSQL){                                                \
             /* Yes, trace the SQL operation, set_param variables, and */                   \
             /* any EXPLAIN in the client's trace file.                */                   \
             tracef_RsaSQL(0, theSQLimage, theErrorCode, theAuxInfo, pvcount, pv_array);    \
             } /* End of call_rsa_pv_h() macro */

#define call_begin_thread(thread_name,application_name,userid,data_conversion,indicators_not_supplied,access_type,rec_option,suppress_msg,error_code,aux_info)\
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = begin_thread(thread_name,application_name,userid,data_conversion,             \
                        indicators_not_supplied,access_type,rec_option,suppress_msg,aux_info);        \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Begin_Thread_PacketSQL(thread_name,application_name,userid,data_conversion,       \
                    indicators_not_supplied,access_type,rec_option,suppress_msg,error_code,aux_info); \
           }  /* end call_begin_thread macro */

#define call_reg_uds_resource(reg_uds_resource_packet,x_status)                                       \
           /* First, perform the 2PC Begin Thread) via the ODTP packet interface. */                  \
           x_status = _reg_uds_resource(&reg_uds_resource_packet);                                    \
           /* Tracef the _reg_uds_resource operation if we are debugging internal */                  \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the _reg_uds_resource operation in the client's trace file. */             \
             tracef("Executed ODTP(int=%d): _reg_uds_resource(%p);\n", x_status, &reg_uds_resource_packet); \
           }  /* end call_reg_uds_resource */

#define call_end_thread(step_control_option,error_code,aux_info)                                      \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = end_thread(step_control_option,aux_info);                                     \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_End_Thread_PacketSQL(step_control_option,error_code,aux_info);                    \
           } /* end call_end_thread macro */

#define call_commit_thread(step_control_option,error_code,aux_info)                                   \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = commit_thread(step_control_option,aux_info);                                  \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Commit_Thread_PacketSQL(step_control_option,error_code,aux_info);                 \
           } /* end call_commit_thread macro */

#define call_rollback_thread(rb_step_control_option,error_code,aux_info)                              \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = rollback_thread(rb_step_control_option,aux_info);                             \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Rollback_Thread_PacketSQL(rb_step_control_option,error_code,aux_info);            \
           } /* end call_Rollback_thread macro */

#define call_get_description(qualifier_name,table_name,version_name,column_name,column_data_type,data_type_info1,data_type_info2,nulls_allowed,in_primary_code,aux_info)\
           /* First, perform the SQL operation via the packet interface. */                           \
           get_description(qualifier_name,table_name,version_name,column_name,                        \
                           column_data_type,data_type_info1,data_type_info2,                          \
                           nulls_allowed,in_primary_code,aux_info);                                   \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /* tracef_Get_Description_PacketSQL(qualifier_name,table_name,version_name,column_name,  \
                                      column_data_type,data_type_info1,data_type_info2,               \
                                      nulls_allowed,in_primary_code,aux_info); */                     \
         /*   } end call_get_description macro */

#define call_get_cursor_description(cursor_name,qualifier_name,table_name,version_name,column_name,column_data_type,data_type_info1,data_type_info2,nulls_allowed,in_primary_code,aux_info)\
           /* First, perform the SQL operation via the packet interface. */                           \
           get_cursor_description(cursor_name,qualifier_name,table_name,version_name,column_name,     \
                           column_data_type,data_type_info1,data_type_info2,                          \
                           nulls_allowed,in_primary_code,aux_info);                                   \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /* tracef_Get_Description_PacketSQL(qualifier_name,table_name,version_name,column_name,  \
                                      column_data_type,data_type_info1,data_type_info2,               \
                                      nulls_allowed,in_primary_code,aux_info); */                     \
         /*   } end call_get_cursor_description macro */

#define call_get_formatted_cursor_description(cursor_name,pca_ptr,pca_size,error_code,aux_info)       \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = get_formatted_cursor_description(cursor_name,pca_ptr,pca_size,aux_info);      \
           /* Tracef the operation if we are debugging Internal or SQL's */                           \
           if (debugInternal){                                                                        \
               tracef("Executed RSA_PKT(err=%d, aux=%d): get_formatted_cursor_description (\"%s\", %p, %d);\n",\
                      atoi(error_code),*aux_info,cursor_name, pca_ptr,pca_size);                            \
            /*   tracef_get_formatted_cursor_descriptionSQL(cursor_name,pca_ptr,pca_size,error_code,aux_info); */\
           } /* end call_get_formatted_cursor_description macro */

#define call_set_pca(pca_ptr,pca_size,error_code,auxInfo)                                             \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = set_pca(pca_ptr,pca_size,auxInfo);                                            \
           /* Tracef the operation if we are debugging Internal or SQL's */                           \
           if (debugInternal){                                                                        \
             tracef("Executed RSA_PKT(err=%d,aux=%d): set_pca(%p, %d);\n",atoi(error_code), *auxInfo, pca_ptr, pca_size);\
           } /* end call_set_pca macro */

#define call_use_default(name_type,default_name,error_code,aux_info)                                  \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = use_default(name_type,default_name,aux_info);                                 \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Use_Default_PacketSQL(name_type,default_name,error_code, aux_info);               \
           } /* end call_use_default macro */

#define call_get_parameter_info(qualifier_name,routine_name,param_buffer,num_params,error_code,aux_info)\
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = get_parameter_info(qualifier_name,routine_name,param_buffer,num_params,aux_info);\
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Get_Parameter_Info_PacketSQL(qualifier_name,routine_name,param_buffer,            \
                                                 num_params,error_code,aux_info);                     \
           }  /* end call_get_parameter_info macro */

#define call_set_output_id_code(kanji_code,aux_info)                                                  \
           /* First, perform the SQL operation via the packet interface. */                           \
           set_output_id_code(kanji_code,aux_info);                                                   \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
            /* tracef_Set_Output_Id_code_PacketSQL(kanji_code,aux_info); */                           \
           /* } end call_set_output_id_code macro */

#define call_set_statement(set_command,set_value,error_code,aux_info)                                 \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = set_statement(set_command,set_value,aux_info);                                \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Set_Statement_PacketSQL(set_command,set_value,error_code,aux_info);               \
           } /* end call_set_statement macro */

#define call_set_stmt_handle(handle)                                                                  \
           /* First, perform the SQL operation via the packet interface. */                           \
           set_stmt_handle(handle);                                                                   \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugSQL){                                                                             \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef("Executed SQL(err=0,aux=0): RSA_PKT set_stmt_handle(%d)\n",handle);               \
           } /* end call_set_stmt_handle macro */

#define call_get_blob(blob_id,start_byte,number_of_bytes,blob_data,error_code,aux_info)               \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = get_blob(blob_id,start_byte,number_of_bytes,blob_data,aux_info);              \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /*tracef_get_blob(blob_id,start_byte,number_of_bytes,blob_data, */                       \
             /*                error_code,aux_info);*/                                                \
          /*  } end call_get_blob macro */

#define call_get_blob_as_bytes(handle,start_byte,max_number_of_bytes,number_of_bytes_returned,        \
                               byte_data,error_code,aux_info)                                         \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = get_blob_as_bytes(handle,start_byte,max_number_of_bytes,                      \
                                          number_of_bytes_returned,byte_data,aux_info);               \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /*tracef_get_blob_as_bytes(handle,start_byte,max_number_of_bytes, */                     \
             /*                         number_of_bytes_returned,byte_data,aux_info); */              \
          /*  } end get_blob_as_bytes macro */


#define call_put_blob(data_to_stage,staged_start_byte,number_of_bytes,maximum_byte_count,             \
                      handle,error_code,aux_info)                                                     \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = put_blob(data_to_stage, staged_start_byte,number_of_bytes,                    \
                                 maximum_byte_count,handle,aux_info);                                 \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /*tracef_put_blob(data_to_stage, staged_start_byte,number_of_bytes,maximum_byte_count,*/ \
             /*                handle,aux_info);*/                                                    \
          /*  } end call_put_blob macro */

#define call_truncate_blob(truncated_lgth, handle, error_code,aux_info)                               \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = truncate_blob(truncated_lgth, handle, aux_info);                              \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /*tracef_truncate_blob(truncated_lgth, handle, error_code,aux_info);*/                   \
           /*  } end call_truncate_blob macro */

#define call_get_lob_length(blob_id, lob_length)                                                      \
           /* First, perform the SQL operation via the packet interface. */                           \
           lob_length = get_lob_length(blob_id);                                                      \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /*tracef_get_lob_length(blob_id, lob_length);*/                                          \
           /*  } end call_get_lob_length macro */

#define call_get_lob_handle(lob_id, handle)                                                           \
           /* First, perform the SQL operation via the packet interface. */                           \
           handle = get_lob_handle(lob_id);                                                           \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           /* if (debugInternal){ */                                                                  \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /* tracef("Executed RSA_PKT(int=%d): get_lob_handle();\n",handle); */                    \
           /* } end call_get_lob_handle macro */

#define call_get_blob_handle_length(handle, size_in_bytes, error_code, aux_info)                      \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = get_blob_handle_length(handle, size_in_bytes, aux_info);                      \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
          /* if (debugSQL){ */                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             /*tracef_get_blob_handle_length(handle, size_in_bytes, error_code, aux_info);*/          \
           /*  } end call_get_blob_handle_length macro */

#define call_get_results_cursor_name(cursor_name,error_code,aux_info)                                 \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = get_results_cursor_name(cursor_name,aux_info);                                \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Get_Results_Cursor_Name_PacketSQL(cursor_name,error_code,aux_info);               \
           } /* end call_get_results_cursor_name macro */

#define call_execute_prepared(jdbcSection_ptr,pvcount,pv_array,error_code,aux_info)                   \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = execute_prepared((int *)jdbcSection_ptr+JDBC_SECTION_HDR_SIZE_IN_WORDS,pvcount,pv_array,aux_info);\
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Execute_Prepared_PacketSQL(jdbcSection_ptr,pvcount,pv_array,error_code,aux_info); \
           } /* end call_execute_prepared macro */

#define call_execute_declared(jdbcSection_ptr,error_code,aux_info,cursor_name)                        \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = execute_declared((int *)jdbcSection_ptr+JDBC_SECTION_HDR_SIZE_IN_WORDS,aux_info,cursor_name);\
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Execute_Declared_PacketSQL(jdbcSection_ptr,error_code,aux_info,cursor_name);      \
           } /* end call_execute_declared macro */

#define call_drop_cursor(cursor_name,error_code,aux_info)                                             \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = drop_cursor(cursor_name,aux_info);                                            \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Drop_Cursor_PacketSQL(cursor_name,error_code,aux_info);                           \
           } /* end call_drop_cursor macro */

#define call_open_cursor(cursor_name,pvcount,pv_array,error_code,aux_info)                            \
           /* First, perform the SQL operation via the packet interface. */                           \
           error_code = open_cursor(cursor_name,pvcount,pv_array,aux_info);                           \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal || debugSQL){                                                            \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef_Open_Cursor_PacketSQL(cursor_name,pvcount,pv_array,error_code,aux_info);          \
           } /* end call_open_cursor macro */

#define call_rsa_init(size)                                                                           \
           /* First, perform the SQL operation via the packet interface. */                           \
           rsa_init(size);                                                                            \
           /* Tracef the SQL command operation if we are debugging SQL's */                           \
           if (debugInternal){                                                                        \
             /* Yes, trace the SQL operation in the client's trace file. */                           \
             tracef("Executed RSA_PKT(): rsa_init(%d);\n",*size);                                     \
           } /* end call_set_statement macro */

#define call_fetch_to_buffer(cursor_name,fetchbuffptr,fetchbuffsize,expsize,irb,norecs,error_code,aux_info) \
           error_code = fetch_to_buffer(cursor_name,fetchbuffptr,fetchbuffsize,expsize,irb,norecs, aux_info); \
           if (debugInternal || debugSQL) {                                                           \
               /* Yes, trace the SQL operation in the client's trace file. */                         \
               tracef_Fetch_To_BufferSQL(cursor_name,fetchbuffptr,fetchbuffsize,expsize,irb,norecs,error_code,aux_info); \
           } /* end call_fetch_to_buffer macro */

/* Return set_statement main and second argument values as a strings */
#define setStatementTypeString(type) \
        type  == statistics ? "STATISTICS" :  \
        (type == output_id_code ? "OUTPUT ID CODE" : \
        (type == auxinfo ? "AUXINFO" : \
        (type == varchar ? "VARCHAR" : \
        (type == storage_area ? "STORAGE AREA" : \
        (type == character_set ? "CHARACTER SET" : \
        (type == eliminate_null ? "ELIMINATE NULL" : \
        (type == result_set_cursor ? "RESULT SET CURSOR" : \
        (type == update_count ? "UPDATE COUNT" : \
        (type == stmt_handle ? "STMT HANDLE" : \
        (type == set_null_string_0 ? "SET NULL STRING" : \
        (type == skipgenerated ? "SKIP GENERATED" : \
        (type == autocommit ? "AUTOCOMMIT" : \
        (type == caller ? "CALLER" : \
        (type == holdable_cursor ? "HOLDABLE CURSOR" : \
        (type == read_only ? "READ ONLY" : \
        (type == fetch_first ? "FETCH FIRST" : \
        (type == optimize_for ? "OPTIMIZE FOR" : \
        (type == send_section ? "SEND SECTION" : \
        (type == pv_indicators ? "PV INDICATORS" : \
        "UNKNOWN_SET_TYPE")))))))))))))))))))

#define setStatementValueString(type) \
        type  == off ? "OFF" :  \
        (type == on ? "ON" : \
        (type == lets_j ? "LETS-J" : \
        (type == shift_jis ? "SHIFT-JIS" : \
        (type == record_count ? "RECORD COUNT" : \
        (type == page_count ? "PAGE COUNT" : \
        (type == off_count ? "OFF COUNT" : \
        (type == varchar_fn ? "VARCHAR FN" : \
        (type == varchar_col ? "VARCHAR COL" : \
        (type == return_error ? "RETURN ERROR" : \
        (type == null_value ? "NULL VALUE" : \
        (type == single_space ? "SINGLE SPACE" : \
        (type == jdbc ? "JDBC" : \
        (type == default_caller ? "DEFAULT CALLER" : \
        (type == ret_error ? "RETURN ERROR" : \
        (type == int_passed ? "INT PASSED" : \
        "UNKNOWN_SET_VALUE")))))))))))))))

void tracef_RsaSQL(int rsaCallType, char * theSQLimagePtr, rsa_error_code_type theErrorCodePtr, int * theAuxInfoPtr,
                   int *p_count, pv_array_type *p_ArrayPtr);
void tracef_Begin_Thread_PacketSQL(char  *thread_name, char *application_name, char *userid, boolean data_conversion,
                                   boolean indicators_not_supplied, access_type_code access_type,
                                   rec_option_code rec_option, boolean suppress_msg,
                                   rsa_err_code_type error_code,int *aux_info);
void tracef_reg_uds_resource(_uds_resource * reg_uds_resource_packetPtr, int x_status);
void tracef_Set_Statement_PacketSQL(set_statement_type set_command, set_value_type set_value,
                                    rsa_err_code_type error_code, int *aux_info);
void tracef_End_Thread_PacketSQL(step_control_code step_control_option, rsa_err_code_type error_code, int *aux_info);
void tracef_Commit_Thread_PacketSQL(step_control_code step_control_option, rsa_err_code_type error_code, int *aux_info);
void tracef_Rollback_Thread_PacketSQL(rb_step_control_code rb_step_control_option, rsa_err_code_type error_code, int *aux_info);
void tracef_Use_Default_PacketSQL(name_type_code name_type, char *default_name,
                                  rsa_err_code_type error_code, int *aux_info);
void tracef_Get_Results_Cursor_Name_PacketSQL(char *cursor_name, rsa_err_code_type error_code, int *aux_info);
void tracef_Execute_Prepared_PacketSQL(jdbc_section_hdr * jdbcSectionHdrPtr, int *p_count, pv_array_type *p_ArrayPtr,
                                       rsa_err_code_type error_code, int *aux_info);
void tracef_Execute_Declared_PacketSQL(jdbc_section_hdr * jdbcSectionHdrPtr, rsa_err_code_type error_code, int *aux_info, char *cursor_name);
void tracef_Drop_Cursor_PacketSQL(char *cursor_name, rsa_err_code_type error_code, int *aux_info);
void tracef_Open_Cursor_PacketSQL(char *cursor_name, int *p_count, pv_array_type *p_ArrayPtr,
                                  rsa_err_code_type error_code, int *aux_info);
void tracef_get_formatted_cursor_descriptionSQL(char *cursor_name, PCA *pca_ptr, int pca_size, rsa_err_code_type error_code, int *aux_info);
void tracef_Get_Parameter_Info_PacketSQL(char * qualifier_name, char * routine_name, param_buf_type * param_buffer,
                                         int * num_params, rsa_err_code_type error_code, int *aux_info);
void tracef_Fetch_To_BufferSQL(char * cursor_name, int * fetchbuffptr, int fetchbuffsize, int expsize,
                               int irb, int norecs, rsa_err_code_type error_code, int *aux_info);

/* Provide activity level access to the activities debugging/trace variables
 * which are declared in ServerActWDE.c.
 */
extern int debugInternal;       /* indicates internal level of debug info is desired */
extern int debugDetail;         /* indicates detailed level of debug info is desired */
extern int debugBrief;          /* indicates brief level of debug info is desired */
extern int debugTstampFlag;     /* indicates whether we need to add the timestamp prefix during tracef processing. */
extern int debugPrefixFlag;     /* indicates whether we need to add the debugPrefix during tracef processing. */
extern int debugPrefixOrTstampFlag; /* indicates whether we need to add the debugPrefix or Timestamp during tracef processing. */
extern int debugSQL;            /* indicates whether SQL debug info is desired */
extern int debugSQLE;           /* indicates whether SQL debug info, if produced, should include EXPLAIN text */
extern int debugSQLP;           /* indicates whether SQL debug info, if produced, should include $Pi parameter info */
extern char debugPrefix[REQ_DEBUG_PREFIX_STRING_LENGTH+1]; /* trace object and & parent's object instance id's */

/*
 * These defines are a subset of those in JDBCServer.h, with a
 * leading prefix of SERVER_ added to the #define name.
 */
#define SERVER_JDBC_SERVER_LEVEL_SIZE  JDBC_SERVER_LEVEL_SIZE
#define SERVER_BUILD_LEVEL_MAX_SIZE  JDBC_SERVER_BUILD_LEVEL_MAX_SIZE
#define SERVER_MAX_APPGROUP_NAME_LEN  MAX_APPGROUP_NAMELEN
#define SERVER_MAX_FILENAME_LEN   100   /* MAX_FILENAME_LEN */
#define SERVER_MAX_RDMS_LEVEL_LEN  36   /* MAX_RDMS_LEVEL_LEN */
#define SERVER_USER_ACCESS_TYPE_ALLOWED 0   /* same as JDBC_USER_ACCESS_ALLOWED */
#define SERVER_USER_ACCESS_TYPE_DENIED  1   /* same as JDBC_USER_ACCESS_DENIED */
#define SERVER_CHARS_IN_USER_ID  CHARS_IN_USER_ID /* same as CHARS_IN_USER_ID in suval.h */
#define CLIENT_SERVER_COMPATIBILITY_ALLOWED 0 /* same as JDBC_USER_ACCESS_ALLOWED */
#define CLIENT_SERVER_COMPATIBILITY_DENIED 1 /* same as JDBC_USER_ACCESS_DENIED */
/*
 * This define is a subset of that in ServerLog.h, with a leading prefix
 * of SERVER_ added to the #define name and the value explicitly specified.
 */
#define SERVER_TO_SERVER_STDOUT (0x00FD & 0x00EF) /* Must be same value as TO_SERVER_STDOUT in ServerLog.h */

/* Max usable response packet (max size minus debug area) */
#define SERVER_MAX_ALLOWED_RESPONSE_PACKET MAX_ALLOWED_RESPONSE_PACKET - RES_DEBUG_INFO_MAX_SIZE

/* Size of the timestamp string added in front of debugging output */
#define TIMESTAMP_PREFIX_STRING_LENGTH 13  /* time string is of form "HH:MM:SS.SSS " */

/* Now comes some RSA oriented stuff regarding the RSA BDI */
#define MAX_RSA_BDI_LEN 12
#define BLANKS_12 "            "
#define UDSSRC_RSA_BDI 0201515     /* RSA bdi for Application Group 3 (UDSSRC) */
#define APPSVN_RSA_BDI 0202621     /* RSA bdi for Application Group 7 (APPSVN) */
int RSA$SETBDI(char * appGroupName, int * status);

/* Create a set of defines for the Feature Flags that call functions which     */
/* return the corresponding feature flag from the SGD for the Server.          */
#define FF_SupportsSQLSection               getFF_SupportsSQLSection()
#define FF_SupportsFetchToBuffer            getFF_SupportsFetchToBuffer()

/* Bit Mask defines for the Feature Flags in the Begin Thread response packet. */
/* Byte 0: */
#define FF_NOT_UNSED_07_BIT                     7   /* set bit 7 - old 15R1RsaLibrary flag */
#define FF_NOT_UNSED_06_BIT                     6   /* set bit 6 - old EnhancedBlobRetrieval flag */
#define FF_NOT_UNSED_05_BIT                     5   /* set bit 5 - old SupportsBlobWrites flag */
#define FF_NOT_UNSED_04_BIT                     4   /* set bit 4 - old SupportsSetReadOnly flag */
#define FF_NOT_UNSED_03_BIT                     3   /* set bit 3 - old SupportsSetCallerJDBC flag */
#define FF_NOT_UNSED_02_BIT                     2   /* set bit 2 - old SupportsSetAutoCommit flag */
#define FF_NOT_UNSED_01_BIT                     1   /* set bit 1 - old SupportsSetPassData  flag */
#define FF_NOT_UNSED_00_BIT                     0   /* set bit 0 - old SupportsDropCloseCursor flag */
/* Byte 1: */
#define FF_NOT_UNSED_17_BIT                     7   /* set bit 7 - old SupportsGetFormattedDescription flag */
#define FF_NOT_UNSED_16_BIT                     6   /* set bit 6 - old DbmdCatalogChanges flag */
#define FF_NOT_UNSED_15_BIT                     5   /* set bit 5 - old SupportsUpdatableCursor flag */
#define FF_NOT_UNSED_14_BIT                     4   /* set bit 4 - old SetHoldableCursor flag */
#define FF_SUPPORTSSQLSECTION_BIT               3   /* set bit 3 */
#define FF_SUPPORTSFETCHTOBUFFER_BIT            2   /* set bit 2 */
#define FF_NOT_UNSED_11_BIT                     1   /* set bit 1 */
#define FF_NOT_UNSED_10_BIT                     0   /* set bit 0 */

/* Prototypes for the functions which return the specific Feature Flag value.  */
int getFF_SupportsSQLSection();
int getFF_SupportsFetchToBuffer();

/* Other function/procedure prototypes */
void getServerAppGroup(char * appNamePtr);

void getRdmsLevelID(char * rdmsLevelIDPtr);

void addServerInstanceInfoArgument(char * buffer, int * bufferIndex);

void setServerUsageType(char serverUsageType);

void setServerInstanceDateandTime();

void displayableServerInstanceIdent(char * displayString, char * serverInstanceInfoPtr, int withBytes);

int getServerInstanceInfoArgSize();

int testServerInstanceInfo(char * instanceInfoPtr);

int getWorkerActivityID();

long long getWorkerUniqueActivityID();

void getRunID(char * runIDPtr);

void getGeneratedRunID(char * rdmsLevelIDPtr);

void getRunIDs();

void getServerUserID();

#ifdef XABUILD  /* XA JDBC Server */

int getDebugXA();

int get_tx_info(int * transaction_state);

void reset_xa_reuse_counter();

int is_xa_reuse_counter_at_limit();

void bumpXAclientCount(int taskCode, int reuse_xa_thread);

void clearSavedXA_threadInfo();

void saveXA_threadInfo(int beginThreadTaskCode, char * userNamePtr, char * schemaPtr, char * versionPtr, int access_type,
                       int rec_option, int varcharInt, int emptyStringProp, int skipGeneratedType,
                       char * storageAreaPtr, char * charSetPtr);

int compareXA_savedThreadInfo(char * userNamePtr, int access_type, int rec_option);

int compareXA_savedCharSetInfo(char * charSetPtr);

int compareXA_savedStorageAreaInfo(char * storageAreaPtr);

void dumpSavedBTinfo_AndTrxState();

int getSaved_beginThreadTaskCode();

void processedUseCommand(int flag);

int getSaved_processedUseCommandFlag();

void processedSetCommand(int flag);

int getSaved_processedSetCommandFlag();

int getSaved_rsaDump();

int getSaved_debugSQLE();

char * getSaved_schemaName();

char * getSaved_versionName();

int getSaved_varchar();

int getSaved_emptyStringProp();

int getSaved_skipGeneratedType();

int getClientCount();

int getXAreuseCount();

int getTrx_mode();

int getTransaction_state();

    /* Return tx_info() Code as a string */
#define tx_info_String(trx_mode) \
        trx_mode == TXINFO_NOT_TRX_MODE ? "TXINFO_NOT_TRX_MODE" : \
        (trx_mode == TXINFO_TRX_MODE ? "TXINFO_TRX_MODE"        : \
        "tx_info() value not recognized by tx_info_String()")

/* Return transaction_state as a string */
#define transaction_state_String(transaction_state) \
        transaction_state == TX_ACTIVE ? "TX_ACTIVE"                                : \
        (transaction_state == TX_TIMEOUT_ROLLBACK_ONLY ? "TX_TIMEOUT_ROLLBACK_ONLY" : \
        (transaction_state == TX_ROLLBACK_ONLY ? "TX_ROLLBACK_ONLY"                 : \
        "transaction_state value not recognized by transaction_state_String()"))

#endif          /* XA JDBC Server */
long long NanoTime();

int getRdmsBaseLevel();

int getRdmsFeatureLevel();

int setClientReceiveTimeout(int * requested_Client_Receive_TimeoutValue, int * actual_Client_Receive_TimeoutValue);

int setClientFetchBlockSize(int * requested_Client_Fetch_Block_Size, int * actual_Client_Fetch_Block_Size);

int setClientTraceFile(char * requested_ClientTraceFileName, char * actual_ClientTraceFileName);

void setClientLocale(char * locale);

void setClientUserid(char * userid);

int getMaxConnections();

int getClientReceiveTimeout();

void get_txnFlag_ODTPtokenValue(int * txn_flag, int * odtpTokenValue);

int getServerTypeAndStartDate();

int getServerStartMillisecond();

int tracef(const char *cs, ...);

void Tcheck(int traceid, char* fileName, char * tracetext, void * pointer);

void * Tmalloc(int traceid, char* fileName, int size);

void * Tcalloc(int traceid, char* fileName, int num, int size);

void Tfree(int traceid, char* fileName, void * p);

#define DWTIME_STRING_LEN 22 /* Size of DWTimeString() returned string, including trailing null */
char * DWTimeString(long long dwtime, char * DWTimeStringPtr);

void TSOnMsgFile();

void clearTSOnMsgFile();

void setClientDriverLevel(char * level, char * driverBuildLevel);

void getServerLevel(char * level, char * serverBuildLevel);

void bumpClientCount();

int validate_client_driver_level_compatibility(char * clientLevel, char * serverLevel);

int validate_user_thread_access_permission(char * userid_ptr, int access_type,
                                           int * project, int * account);

int validate_user_thread_access_permission_fundamental(int access_type);

void add_a_ServerLogEntry(int logIndicator, char *message);

int getClientSocket();

int getSessionID();

void getThreadName(char * threadNamePtr);

void setThreadName(char * threadNamePrefix, int connectionID);

int get_appGroupNumber();

int get_uds_icr_bdi();

int do_RSA_DebugDumpAll();

int retrieve_RDMS_level_info();

void addServerLogMessage(char * msgPtr);

int setServer_Approved_KeepAliveState(int client_Requested_keepAliveState);

int getClient_Assigned_KeepAliveState();

void now_calling_RDMS();

void nolonger_calling_RDMS();

int getMax_Size_Res_Pkt();

char * itoa(int value, char * buffer, int base);

char * generate_ID_level_mismatch_response_packet(int reqPktId);

void hexdumpWithHeading(char * ptr, int count, int num_per_line, char * labelStr, char *arg1, char *arg2);

void hexdump(char * ptr, int count, int num_per_line);

void tracefDumpWords(void * ptr, int count, int num_per_line, char *labelStr, char *arg1, char *arg2);

void tracefDumpBytes(void * ptr, int count, int num_per_line, char *labelStr, char *arg1, char *arg2);

void dumpClientInfo();

void stop_all_tasks(int messageNumber, char* insert1, char * insert2);

void closeRdmsConnection(int cleanup);

char * getAsciiRsaErrorMessage(char * msgBuffer);

void addFeatureFlagsArgument(char * buffer, int * bufferIndex);

void addDateTime(char *hdrPtr);


#include <ertran.h>
#define ER_TIME  023                      /* ER code for TIME$  */
#define ER_TDATE 054                      /* ER code for TDATE$ */

/* The MAX_JDBC_TRACE_BUFFER_SIZE replaces the constant value 32767 that originally was in
   element server.c, function tracef, for the declaration of the printf buffer (array work).
   The JDBC tracef function was taken from the UC URTS library and modified slightly for JDBC.
   UC has a documented limit of 32767 for the max string size that can be output using fprintf.
   If you try to output a string beyond the limit, you IGDM with UC.  For JDBC tracing,
   we use this define constant for declaring our max trace buffer size.  You should never
   call tracef with a string larger than this value or an IGDM can occur.  Note that tracefu
   and tracefuu will truncate a string > MAX_JDBC_TRACE_BUFFER_SIZE before calling tracef.
*/
#define MAX_JDBC_TRACE_BUFFER_SIZE      60000

/* Defines to handle logging date/time info */
#define MILLI_SECONDS_PER_DAY 86400000  /* milliseconds in one day, used when we cross midnight */
#define MILLI_SECONDS_PER_HOUR       3600000
#define MILLI_SECONDS_PER_MINUTE     60000
#define MILLI_SECONDS_PER_SECOND     1000

#endif /* _SERVER_H_ */

