#ifndef _NETWORK_API_H_
#define _NETWORK_API_H_
/*
 * File: NetworkAPI.h
 * Header file used for the JDBC Server's network API.  The
 * routines provided here utilize COMAPI and JNI to support
 * the remote and local JDBC Clients, respectively.
 *
 * Header file corresponding to code file NetworkAPI.c
 *
 */

/* JDBC Project Files */
#include "JDBCServer.h"

/* COMAPI SETOPTS values */
#define COMAPI_LINGER_OPT       32  /* Linger option */
#define COMAPI_BLOCKING_OPT  66048  /* Blocking option */
#define COMAPI_MULTIPLE_OPTS 65535  /* Multiple options option */
#define COMAPI_DEBUG_OPT         2  /* COMAPI debug option */
#define COMAPI_RECEIVE_OPT     256  /* Receive timeout option */
#define COMAPI_SEND_OPT       4096  /* Send timeout option */
#define COMAPI_INTERFACE_OPT     3  /* COMAPI interface option */
#define COMAPI_IP_IPV4V6    131075  /* COMAPI IP address option */

#define NO_SERVER_SOCKETS_ACTIVE -1  /* Used as a COMAPI-like error status by Pass_Event() to indicate no server sockets were found */

/* Data structures used with COMAPI */
typedef struct {
    int *COMAPI_option_name_ptr;
    int *COMAPI_option_value_ptr;
    int *COMAPI_option_length_ptr;
} COMAPI_option_entry;

/* Function prototypes */

int Register_Application();

void Deregister_Application();

void deregComapiAndStopTasks(int messageNumber, char* insert1, char* insert2);

int Start_Socket(int *socket_ptr);

int Bequeath_Socket(int socket);

int Inherit_Socket(int socket);

int Listen_Port(int socket, int max_queue_size, int interface,
                v4v6addr_type listen_ip_address, int listen_port);

int Close_Socket(int socket);

int Inherit_Close_Socket(int socket);

int Set_Socket_Options(int socket, int *option_name_ptr, int *option_value_ptr
                       , int *option_length_ptr);

int Pass_Event();

void Pass_Event_to_SW(int socket, int comapi_bdi);

int Accept_Client(int server_socket, int * new_client_socket_ptr,
                  v4v6addr_type * remote_ip_address, int * network_interface);

int Select_Socket(int * socket_list);

void Rescind_Close_Socket(int socket);

int Receive_Data(int socket, char * buffer, int buffer_length, int *buffer_count);

int Borrow_Qbank(char **borrowed_qbankPtrPtr, int response_packet_size);

int Return_Qbank(char * qbankPtr);

int Send_DataQB(int socket, char * qbankPtr, int actual_response_packet_size);

int Get_Hostname(char * remote_namePtr, v4v6addr_type remote_ip_address,
                 workerDescriptionEntry *wdePtr);

int Get_Remote_IP_Address(int socket, char * remote_namePtr, v4v6addr_type * remote_ip_address,
                          int interface);

void IP_to_Char(char * charPtr, v4v6addr_type remote_ip_address);

static void log_COMAPI_error(char * func_name, int socket, int error_status, int aux_status);

static void check_COMAPI_status(char * func_name, int socket, int * status, int * aux_status);
#endif /* _NETWORK_API_H_ */
