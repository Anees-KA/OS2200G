#ifndef _SERVER_ICL_H_
#define _SERVER_ICL_H_
/**
 * File: ServerICL.h.
 *
 * Header file for the code that establishes and maintains a Initial Connection Listener
 * for the JDBC Server.
 *
 * Header file corresponding to the code file ServerICL.c.
 *
 */

/* JDBC Project Files */
#include "apidef.h"

/* Function prototypes */

int startInitialConnectionListener();

void initialConnectionListener(int ICL_num, int ICL_comapi_bdi, int ICL_comapi_IPv6);

static void initialConnectionListenerShutDown(enum ICLShutdownPoints ICLShutDownPoint, int socket);

int Initialize_ServerSockets();

int Init_COMAPI(int * socket_ptr, int interface, v4v6addr_type listen_ip_address, int port);

int Get_Client_From_ServerSockets(int * new_client_socket_ptr, v4v6addr_type * remote_ip_addr_ptr,
                                  int * network_interface_ptr, int network_order);

void Terminate_Server_Sockets();
#endif /* _SERVER_ICL_H_ */
