#ifndef _ALLOC_RESP_PKT_H_
#define _ALLOC_RESP_PKT_H_
/*
 * File: AllocRespPkt.h
 * Header file used for the Allocate_Respose_Packet procedure
 * which returns a buffer in which a Response Packet is constructed.
 * The procedure supports both JNI and network access to the JDBC
 * Server supporting both local and remote JDBC Clients, respectively.
 *
 * Header file corresponding to code file AllocRespPkt.c
 *
 */

/* MAX_ALLOWED_RESPONSE_PACKET has an upper limit based on the size of a
 * Q-bank.  Our JDBC Driver uses COMAPI and Q-banks for the response packet.
 * The maximum addressable size of a Q-bank is decimal 262,143 words (address
 * range (0 - 0777776).  The first 01000 (decimal 512) words are not
 * available for user space.  Therefore, the upper limit JDBC
 * can ever define for MAX_ALLOWED_RESPONSE_PACKET is
 * (262,143 - 512) * 4 = 1,046,524 bytes.  We have chosen 1,000,000 bytes
 * as an arbitrary value.
 */
#define MAX_ALLOWED_RESPONSE_PACKET 1000000

char* allocateResponsePacket(int response_packet_size);

void releaseResPktForReallocation(char * resPkt);
#endif /* _ALLOC_RESP_PKT_H_ */
