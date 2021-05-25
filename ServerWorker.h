#ifndef _SERVER_WORKER_H_
#define _SERVER_WORKER_H_
/**
 * File: ServerWorker.h.
 *
 * Header file for the code that establishes and maintains a JDBC Server's
 * Server Worker activity.
 *
 * Header file corresponding to code file ServerWorker.c.
 *
 */

/* JDBC Project Files */
#include "JDBCServer.h"

/* Function prototypes */

#ifndef XABUILD  /* JDBC Server */

static workerDescriptionEntry *createServerWorkerWDE( int new_client_socket
                                                     , int new_client_COMAPI_bdi
                                                     , int new_client_COMAPI_IPv6
                                                     , int new_client_ICL_num
                                                     , v4v6addr_type new_client_IP_addr
                                                     , int new_client_network_interface);

void assignServerWorkerToClient(workerDescriptionEntry *wdePtr
                                , int new_client_socket
                                , int new_client_COMAPI_bdi
                                , int new_client_COMAPI_IPv6
                                , int new_client_ICL_num
                                , v4v6addr_type new_client_IP_addr
                                , int new_client_network_interface);

void serverWorkerShutDown(workerDescriptionEntry *wdePtr
                          , enum serverWorkerShutdownPoints serverWorkerShutdownPoint
                          , int client_socket);

void serverWorker(int new_client_socket, int new_client_COMAPI_bdi, int new_client_COMAPI_IPv6,
                  int new_client_ICL_num, int new_client_IP_addr1, long long new_client_IP_addr2,
                  long long new_client_IP_addr3, int new_client_network_interface);

int moveByteArrayArg_1(char * retBuffer, char * buffer, int * bufferIndex, int retBufSize);
int moveUnicodeStringArg_1(char * retBuffer, char * buffer, int * bufferIndex, int retBufSize);
int moveStringArg_1(char * retBuffer, char * buffer, int * bufferIndex, int bufSize);
int Validate_RequestPacket(char *requestPacketPtr, int task_code);

#endif          /*  JDBC Server */

void initializeServerWorkerWDE(workerDescriptionEntry *wdePtr);

#endif /* _SERVER_WORKER_H_ */
