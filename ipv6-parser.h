#ifndef _IPV6PARSER_H_
#define _IPV6PARSER_H_
/*
 * Both ipv6-parser.c and ipv6-parser.h are originally from the COMAPI
 * product.  They should be reviewed and updated periodically based on
 * any changes that the COMAPI group makes to them.
 *
 */
/*  *  *  *  *  *  U  *  N  *  I  *  S  *  Y  *  S  *  *  *  *  *  *
 *                                                                 *
 *              Copyright (c) 2019 Unisys Corporation              *
 *                      All Rights Reserved                        *
 *                      UNISYS CONFIDENTIAL                        *
 *                                                                 *
 *  C  *  O  *  R  *  P  *  O  *  R  *  A  *  T  *  I  *  O  *  N  */

/* -------------- Local defines, structures, unions and enums --------------- */

#define IPV4_FULL              4
#define IPV6_FULL             16
#define IPV6_HALF              8
#define SHORT_SZ               2
#define MAX_IPV6_ADDRESS_LGTH 45
#define MAX_IPV4_ADDRESS_LGTH 15
#define IPV6_SIZE             256

#define INET_PTON6_64(src,dst)       inet_pton6(src,dst,0)
#define INET_PTON6_128(src,dst)      inet_pton6(src,dst,1)
#define INET_NTOP6_64(src,dst,size)  inet_ntop6(src,dst,size,0)
#define INET_NTOP6_128(src,dst,size) inet_ntop6(src,dst,size,1)

/*******************************************************************************
 *
 *  INET_NTOP
 *      This will convert a network format IP address to printable format.
 *      The IP address can be either an IPv4 or IPv6 address.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    af is the address family for the IP address.
 *    src is a pointer to the binary IP Address.
 *    dst is a pointer to the ASCII form of the IP Address passed.
 *    size is the size of dst.
 *
 *  RETURNS:  NULL if fail, pointer to dst if conversion worked.
 *
 ******************************************************************************/

const char *
inet_ntop (
            int                  af,
            const unsigned char *src,
            char                *dst,
            unsigned int         size
          );

/*******************************************************************************
 *
 *  INET_NTOP4
 *      This will convert a network format IPV4 address to printable format.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    src is a pointer to the binary IPV4 Address.
 *    dst is a pointer to the ASCII form of the IPV4 Address passed.
 *    size is the size of dst.
 *
 *  RETURNS:  NULL if fail, pointer to dst if conversion worked.
 *
 ******************************************************************************/

const char *
inet_ntop4(
            const unsigned char *src,
            char                *dst,
            unsigned int        size
          );

/*******************************************************************************
 *
 *  INET_NTOP6
 *      This will convert a network format IPV6 address to printable format.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    src is a pointer to the binary IPV6 Address.
 *    dst is a pointer to the ASCII form of the IPV6 Address passed.
 *    size is the size of dst.
 *    mode is a integer indicating if it's a 64 or 128 bit IPV6 Address.
 *         1 = 128, 0 = 64.
 *
 *  RETURNS:  NULL if fail, pointer to dst if conversion worked.
 *
 ******************************************************************************/

const char *
inet_ntop6(
            const unsigned char *src,
            char                *dst,
            unsigned int        size,
            int                 mode
          );

/*******************************************************************************
 *
 *  INET_PTON
 *      This will convert an ASCII string of an IP address to binary format.
 *      The IP address can be either an IPv4 or IPv6 address.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    af is the address family for the IP address.
 *    src is a pointer to the ASCII string to be parsed to binary.
 *    dst is a pointer to the binary form of the IP Address passed.
 *
 *  RETURNS:  1  if address was valid for the address family.
 *            0  if address was not valid (dst is unchanged).
 *           -1  all other error conditions (dst is unchanged).
 *
 ******************************************************************************/

int
inet_pton (
            int                  af,
            const unsigned char *src,
            char                *dst
          );

/*******************************************************************************
 *
 *  INET_PTON4
 *      This will convert an IPV4 address to binary.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    src is a pointer to the ASCII string to be parsed to binary.
 *    dst is a pointer to the binary form of the IPV4 Address passed.
 *
 *  RETURNS:  an int, 0 if parsing failed due to string not being a valid
 *            IPV4 Address, 1 if valid IPV4 Address.
 *
 ******************************************************************************/

int
inet_pton4(
            const char    *src,
            unsigned char *dst
          );

/*******************************************************************************
 *
 *  INET_PTON6
 *      This will convert an IPV6 address to binary.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    src is a pointer to the ASCII string to be parsed to binary.
 *    dst is a pointer to the binary form of the IPV6 Address passed.
 *    mode is a integer indicating if it's a 64 or 128 bit IPV6 Address.
 *         1 = 128, 0 = 64.
 *
 *  RETURNS:  an int, 0 if parsing failed due to string not being a valid
 *            IPV6 Address, 1 if valid IPV6 Address.
 *
 ******************************************************************************/

int
inet_pton6(
            const char    *src,
            unsigned char *dst,
            int            mode
          );

/*******************************************************************************
 *
 *  STRIP
 *      This will strip off a prefix or zone off an address.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    src is a pointer to the input ASCII string to be parsed.
 *    dst is a pointer to the returned ASCII string for the zone.
 *    size is the size of dst.
 *
 *  RETURNS:  NULL if fail, string of tokenizied string if necessary.
 *
 ******************************************************************************/

char*
strip(
       char *src,
       char *dst,
       int  size
     );

/*******************************************************************************
 *
 *  ZONE_PARSE
 *      This will give the zone from an IPV6 address.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    src is a pointer to the input ASCII string to be parsed.
 *    dst is a pointer to the returned ASCII string for the zone.  This must be
 *        at least 12 characters in length
 *    size is the size of dst.
 *
 *  RETURNS:  NULL if fail, pointer to dst if success.
 *
 ******************************************************************************/

char*
zone_parse(
            char *src,
            char *dst,
            int  size
          );

/*******************************************************************************
 *
 *  PREFIX_PARSE
 *      This will give the prefix from an IPV6 address.
 *
 *    Wait                : None.
 *    Caller requirements : None.
 *    Path                : Not main-path.
 *    Activity            : Any.
 *    Locks               : None.
 *
 *  PARAMETERS:
 *    src is a pointer to the ASCII string to be parsed.
 *
 *  RETURNS:  -1 if fail, the integer prefix value if success.
 *
 ******************************************************************************/

int
prefix_parse(
              char *src
            );

#endif /* _IPV6PARSER_H_ */
