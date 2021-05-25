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

/* The following below copyright applies to inet_pton4,inet_pton6,
 * inet_ntop4, and inet_ntop6. */

/* Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* This element includes all of the functions that are needed to
   parse an IPV6 address to binary network order, and to ASCII. */

/* --------------------------- Header inclusions ---------------------------- */

/*  Standard #includes  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <tsqueue.h>
#include <limits.h>

/*  Product specific #includes  */

#include "ipv6-parser.h"
#include "apidef.h"

/* -------------- Local defines, structures, unions and enums --------------- */

/* ----------------------- Compilation unit functions ----------------------- */

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
inet_ntop (                                                          /* _FUNC */
            int                  af,
            const unsigned char *src,
            char                *dst,
            unsigned int         size
          )
  {
    switch (af)
      {
        case AF_INET:
          {
            return (inet_ntop4(src, dst, size));
          }
        case AF_INET6:
          {
            return (inet_ntop6(src, dst, size, 1));
          }
        default:
          {
            return (NULL);
          }
      }
  }

#pragma eject

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
inet_ntop4 (                                                         /* _FUNC */
             const unsigned char *src,
             char                *dst,
             unsigned int         size
           )
  {
    static const char fmt[] = "%u.%u.%u.%u";
    char              tmp[sizeof "255.255.255.255"];

    if ((sprintf(tmp, fmt, src[0], src[1], src[2], src[3])) > size)
      {
        return NULL;
      }
    return strcpy(dst, tmp);
  }

#pragma eject

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
inet_ntop6 (                                                         /* _FUNC */
             const unsigned char *src,
             char                *dst,
             unsigned int         size,
             int                  mode
           )
  {
    char          tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
    char         *tp;
    struct        { int base, len; } best, cur;
    unsigned int  words[IPV6_FULL / SHORT_SZ];
    int           i;
    int           full_half = mode ? IPV6_FULL : IPV6_HALF;

    memset(words, '\0', sizeof words);
    for (i = 0; i < full_half; i += 2)
      {
        words[i / 2] = (src[i] << 8) | src[i + 1];
      }
    best.base = -1;
    cur.base = -1;
    for (i = 0; i < (full_half / SHORT_SZ); i++)
      {
        if (words[i] == 0)
          {
            if (cur.base == -1)
              {
                cur.base = i;
                cur.len = 1;
              }
            else
              {
                cur.len++;
              }
          }
        else
          {
            if (cur.base != -1)
              {
                if (best.base == -1 || cur.len > best.len)
                  {
                    best = cur;
                  }
                cur.base = -1;
              }
          }
      }
    if (cur.base != -1)
      {
        if (best.base == -1 || cur.len > best.len)
          {
            best = cur;
          }
      }
    if (best.base != -1 && best.len < 2)
      {
        best.base = -1;
      }

    tp = tmp;
    for (i = 0; i < (full_half / SHORT_SZ); i++)
      {

        /* Are we inside the best run of 0x00's? */

        if (best.base != -1 && i >= best.base &&
            i < (best.base + best.len))
          {
            if (i == best.base)
              {
               *tp++ = ':';
              }
            continue;
          }

        /* Are we following an initial run of 0x00s or any real hex? */

        if (i != 0)
          {
            *tp++ = ':';
          }

        /* Is this address an encapsulated IPv4? */

        if (i == 6 && best.base == 0 &&
           (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
          {
            if (!inet_ntop4(src+12,tp,sizeof tmp - (tp - tmp)))
              {
                return NULL;
              }
            tp += strlen(tp);
            break;
          }
        tp += sprintf(tp,"%x",words[i]);
      }

    /* Was it a trailing run of 0x00's? */

    if (best.base != -1 && (best.base + best.len) == (full_half / SHORT_SZ))
      {
        *tp++ = ':';
      }
    *tp++ = '\0';

    if ((unsigned int)(tp - tmp) > size)
      {
        return NULL;
      }
    return strcpy(dst, tmp);
  }

#pragma eject

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
inet_pton (                                                          /* _FUNC */
            int                  af,
            const unsigned char *src,
            char                *dst
          )
  {
    switch (af)
      {
        case AF_INET:
          {
            return (inet_pton4(src, dst));
          }
        case AF_INET6:
          {
            return (inet_pton6(src, dst, 1));
          }
        default:
          {
            return (-1);
          }
      }
  }

#pragma eject

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
inet_pton4(                                                          /* _FUNC */
            const char    *src,
            unsigned char *dst
          )
  {
    int            saw_digit;
    int            octets;
    int            ch;
    unsigned int   new;
    unsigned char  tmp[IPV4_FULL];
    unsigned char *tp;

    saw_digit = 0;
    octets = 0;
    *(tp = tmp) = 0;
    while ((ch = *src++) != '\0')
      {
        if (ch >= '0' && ch <= '9')
          {
            new = *tp * 10 + (ch - '0');

            if (new > 255)
              {
                return 0;
              }
            *tp = new;
            if (! saw_digit)
              {
                 if (++octets > 4)
                  {
                    return 0;
                  }
                 saw_digit = 1;
              }
          }
        else if (ch == '.' && saw_digit)
          {
            if (octets == 4)
              {
                return 0;
              }
            *++tp = 0;
            saw_digit = 0;
          }
        else
          {
            return 0;
          }
      }
    if (octets < 4)
      {
        return 0;
      }
    memcpy(dst, tmp, IPV4_FULL);
    return 1;
  }

#pragma eject

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
inet_pton6(                                                          /* _FUNC */
            const char    *src,
            unsigned char *dst,
            int            mode
          )
  {
    static const char  xdigits[] = "0123456789abcdef";
    unsigned char      tmp[IPV6_FULL];
    unsigned char     *tp;
    unsigned char     *endp;
    unsigned char     *colonp;
    const char        *curtok;
    int                ch;
    int                saw_xdigit;
    unsigned int       val;
    int                full_half = mode ? IPV6_FULL : IPV6_HALF;
    int                n;
    int                i;

    tp = memset(tmp, '\0', full_half);
    endp = tp + full_half;
    colonp = NULL;

    /* Leading :: requires some special handling. */

    if (*src == ':')
      {
        if (*++src != ':')
          {
            return 0;
          }
      }
    curtok = src;
    saw_xdigit = 0;
    val = 0;
    while (((ch = tolower (*src++)) != '\0') &&
           (ch != '%'))
      {
        const char *pch;
        pch = strchr(xdigits, ch);
        if (pch != NULL)
          {
            val <<= 4;
            val |= (pch - xdigits);
            if (val > 0xffff)
              {
                return 0;
              }
            saw_xdigit = 1;
            continue;
          }
        if (ch == ':')
          {
            curtok = src;
            if (!saw_xdigit)
              {
                if (colonp)
                  {
                    return 0;
                  }
                colonp = tp;
                continue;
              }
            else if (*src == '\0')
              {
                return 0;
              }
            if (tp + SHORT_SZ > endp)
              {
                return 0;
              }
            *tp++ = (unsigned char) (val >> 8) & 0xff;
            *tp++ = (unsigned char) val & 0xff;
            saw_xdigit = 0;
            val = 0;
            continue;
          }
        if (ch == '.' && ((tp + IPV4_FULL) <= endp) &&
            inet_pton4(curtok, tp) > 0)
          {

            /* '\0' was seen by inet_pton4(). */

            tp += IPV4_FULL;
            saw_xdigit = 0;
            break;
          }
        return 0;
      }
    if (saw_xdigit)
      {
        if (tp + SHORT_SZ > endp)
          {
            return 0;
          }
        *tp++ = (unsigned char) (val >> 8) & 0xff;
        *tp++ = (unsigned char) val & 0xff;
      }
    if (colonp != NULL)
      {
        n = tp - colonp;
        if (tp == endp)
          {
            return 0;
          }
        for (i = 1; i <= n; i++)
          {
            endp[- i] = colonp[n - i];
            colonp[n - i] = 0;
          }
          tp = endp;
      }
    if (tp != endp)
      {
        return 0;
      }
    memcpy(dst, tmp, full_half);
    return 1;
  }

#pragma eject

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

char *
strip(                                                               /* _FUNC */
       char *src,
       char *dst,
       int   size
     )
  {
    char temp[IPV6_SIZE];
    int  i = 0;
    int  address_string_length;

    strncpy(temp,src,IPV6_SIZE);
    address_string_length = strlen(temp);
    while (temp[i] != '%' && temp[i] != '/' && temp[i] != '\0')
      {
        if (i == address_string_length)
          {
            return NULL;
          }
        i++;
      }
    if (temp[i] == '\0')
      {
        i++; /* advance past null */
      }
    else
      {
        temp[i] = '\0';
        i++;
      }
    if (i < size && strncpy(dst,temp,i))
      {
        return dst;
      }
    return NULL;
  }

#pragma eject

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

char *
zone_parse(                                                          /* _FUNC */
            char *src,
            char *dst,
            int   size
          )
  {
    char temp[IPV6_SIZE];
    char value_chr[13];
    int  i = 0;
    int  j = 0;
    int  address_string_length;

    strncpy(temp,src,IPV6_SIZE);
    address_string_length = strlen(temp);

    /* Advance through the IP address until we hit the zone delimiter,
       make sure we don't exceed our buffer, if we are going to, there is
       no prefix and return NULL. */

    while (temp[i] != '%')
      {
        if (i == address_string_length)
          {
            return NULL;
          }
        i++;
      }
    i++; /* need to advance past '%' delimiter */

    /* Found the zone delimiter lets grab the characters for the length.
       If we exceed our value chr buffer then its a malformed prefix and
       we need to return a fail (NULL). */

    while (temp[i] != '/' && temp[i] != '\0')
      {
        if (j == 12)
          {
            return NULL;
          }
        value_chr[j++] = temp[i++];
      }

    /* If we didn't fill our value chr buffer with ascii digits,
       we need to null terminate it, if its 0 then its malformed
       and return failure. */

    if (j == 0)
      {
        return NULL;
      }
    value_chr[j++] = '\0';

    if (j < size && strncpy(dst,value_chr,j))
      {
        return dst;
      }
    return NULL;
  }

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
prefix_parse(                                                        /* _FUNC */
              char *src
            )
  {
    char temp[IPV6_SIZE];
    char value_chr[3];
    int  i = 0;
    int  j = 0;
    int  value_int;
    int  address_string_length;

    strncpy(temp,src,IPV6_SIZE);
    address_string_length = strlen(temp);

    /* Advance through the IP address until we hit the prefix delimiter,
       make sure we don't exceed our buffer, if we are going to, there is
       no prefix and return -1. */

    while (temp[i] != '/')
      {
        if (i == address_string_length)
          {
            return 0;
          }
        i++;
      }

    i++; /* need to advance past '/' delimiter */

    /* Found the prefix delimiter lets grab the characters for the length.
       If we exceed our value chr buffer then its a malformed prefix and
       we need to return a fail (-1). */

    while (temp[i] != '%' && temp[i] != '\0')
      {
        if (j == 3)
          {
            return -1;
          }
        value_chr[j++] = temp[i++];
      }

    /* If we didn't fill our value chr buffer with ascii digits,
       we need to null terminate it, if its 0 then its malformed
       and return failure. */

    if (j == 0)
      {
        return -1;
      }
    if (j < 3)
      {
        value_chr[j] = '\0';
      }

    value_int = atoi(value_chr);
    if (value_int < 0 || value_int > 128)
      {
        return -1;
      }
    return value_int;
  }
