/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */


#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#include <string.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>

#ifndef G_OS_WIN32
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <glib-object.h>

#include "gdm-address.h"

struct _GdmAddress
{
        struct sockaddr_storage *ss;
};

/* Register GdmAddress in the glib type system */
GType
gdm_address_get_type (void)
{
        static GType addr_type = 0;

        if (addr_type == 0) {
                addr_type = g_boxed_type_register_static ("GdmAddress",
                                                          (GBoxedCopyFunc) gdm_address_copy,
                                                          (GBoxedFreeFunc) gdm_address_free);
        }

        return addr_type;
}

/**
 * gdm_address_get_family_type:
 * @address: A pointer to a #GdmAddress
 *
 * Use this function to retrive the address family of @address.
 *
 * Return value: The address family of @address.
 **/
int
gdm_address_get_family_type (GdmAddress *address)
{
        g_return_val_if_fail (address != NULL, -1);

        return address->ss->ss_family;
}


/**
 * gdm_address_new_from_sockaddr:
 * @sa: A pointer to a sockaddr.
 * @size: size of sockaddr in bytes.
 *
 * Creates a new #GdmAddress from @sa.
 *
 * Return value: The new #GdmAddress
 * or %NULL if @sa was invalid or the address family isn't supported.
 **/
GdmAddress *
gdm_address_new_from_sockaddr (struct sockaddr *sa,
                               size_t           size)
{
        GdmAddress *addr;

        g_return_val_if_fail (sa != NULL, NULL);
        g_return_val_if_fail (size >= sizeof (struct sockaddr), NULL);
        g_return_val_if_fail (size <= sizeof (struct sockaddr_storage), NULL);

        addr = g_new0 (GdmAddress, 1);
        addr->ss = g_new0 (struct sockaddr_storage, 1);
        memcpy (addr->ss, sa, size);

        return addr;
}

/**
 * gdm_address_get_sockaddr_storage:
 * @address: A #GdmAddress
 *
 * This function tanslates @address into a equivalent
 * sockaddr_storage
 *
 * Return value: A newly allocated sockaddr_storage structure the caller must free
 * or %NULL if @address did not point to a valid #GdmAddress.
 **/
struct sockaddr_storage *
gdm_address_get_sockaddr_storage (GdmAddress *address)
{
        struct sockaddr_storage *ss;

        g_return_val_if_fail (address != NULL, NULL);
        g_return_val_if_fail (address->ss != NULL, NULL);

        ss = g_memdup (address->ss, sizeof (struct sockaddr_storage));

        return ss;
}

struct sockaddr_storage *
gdm_address_peek_sockaddr_storage (GdmAddress *address)
{
        g_return_val_if_fail (address != NULL, NULL);

        return address->ss;
}

static gboolean
v4_v4_equal (const struct sockaddr_in *a,
             const struct sockaddr_in *b)
{
        return a->sin_addr.s_addr == b->sin_addr.s_addr;
}

#ifdef ENABLE_IPV6
static gboolean
v6_v6_equal (struct sockaddr_in6 *a,
             struct sockaddr_in6 *b)
{
        return IN6_ARE_ADDR_EQUAL (&a->sin6_addr, &b->sin6_addr);
}
#endif

#define SA(__s)    ((struct sockaddr *) __s)
#define SIN(__s)   ((struct sockaddr_in *) __s)
#define SIN6(__s)  ((struct sockaddr_in6 *) __s)

gboolean
gdm_address_equal (GdmAddress *a,
                   GdmAddress *b)
{
        guint8 fam_a;
        guint8 fam_b;

        g_return_val_if_fail (a != NULL, FALSE);
        g_return_val_if_fail (a->ss != NULL, FALSE);
        g_return_val_if_fail (b != NULL, FALSE);
        g_return_val_if_fail (b->ss != NULL, FALSE);

        fam_a = a->ss->ss_family;
        fam_b = b->ss->ss_family;

        if (fam_a == AF_INET && fam_b == AF_INET) {
                return v4_v4_equal (SIN (a->ss), SIN (b->ss));
        }
#ifdef ENABLE_IPV6
        else if (fam_a == AF_INET6 && fam_b == AF_INET6) {
                return v6_v6_equal (SIN6 (a->ss), SIN6 (b->ss));
        }
#endif
        return FALSE;
}

/* for debugging */
static const char *
address_family_str (GdmAddress *address)
{
        const char *str;
        switch (address->ss->ss_family) {
        case AF_INET:
                str = "inet";
                break;
        case AF_INET6:
                str = "inet6";
                break;
        case AF_UNIX:
                str = "unix";
                break;
        case AF_UNSPEC:
                str = "unspecified";
                break;
        default:
                str = "unknown";
                break;
        }
        return str;
}

static void
_gdm_address_debug (GdmAddress *address, char *hostname, char *host, char *port)
{
        g_return_if_fail (address != NULL);

        hostname = NULL;
        host = NULL;
        port = NULL;


        g_debug ("Address family:%d (%s) hostname:%s host:%s port:%s local:%d loopback:%d",
                 address->ss->ss_family,
                 address_family_str (address) ? address_family_str (address) : "(null)",
                 hostname ? hostname : "(null)",
                 host ? host : "(null)",
                 port ? port : "(null)",
                 gdm_address_is_local (address),
                 gdm_address_is_loopback (address));

        g_free (hostname);
        g_free (host);
        g_free (port);
}

void
gdm_address_debug (GdmAddress *address)
{
        char *hostname;
        char *host;
        char *port;

        gdm_address_get_hostname (address, &hostname);
        gdm_address_get_numeric_info (address, &host, &port);

        _gdm_address_debug (address, hostname, host, port);
}

gboolean
gdm_address_get_hostname (GdmAddress *address,
                          char      **hostnamep)
{
        char     host [NI_MAXHOST];
        int      res;
        gboolean ret;

        g_return_val_if_fail (address != NULL, FALSE);
        g_return_val_if_fail (address->ss != NULL, FALSE);

        ret = FALSE;

        host [0] = '\0';
        res = getnameinfo ((const struct sockaddr *)address->ss,
                           (int) gdm_sockaddr_len (address->ss),
                           host, sizeof (host),
                           NULL, 0,
                           0);
        if (res == 0) {
                ret = TRUE;
                goto done;
        } else {
                const char *err_msg;

                err_msg = gai_strerror (res);
                g_warning ("Unable to lookup hostname: %s",
                        err_msg ? err_msg : "(null)");
                _gdm_address_debug (address, NULL, NULL, NULL);
        }

        /* try numeric? */

 done:
        if (hostnamep != NULL) {
                *hostnamep = g_strdup (host);
        }

        return ret;
}

gboolean
gdm_address_get_numeric_info (GdmAddress *address,
                              char      **hostp,
                              char      **servp)
{
        char     host [NI_MAXHOST];
        char     serv [NI_MAXSERV];
        int      res;
        gboolean ret;

        g_return_val_if_fail (address != NULL, FALSE);
        g_return_val_if_fail (address->ss != NULL, FALSE);

        ret = FALSE;

        host [0] = '\0';
        serv [0] = '\0';
        res = getnameinfo ((const struct sockaddr *)address->ss,
                           (int) gdm_sockaddr_len (address->ss),
                           host, sizeof (host),
                           serv, sizeof (serv),
                           NI_NUMERICHOST | NI_NUMERICSERV);
        if (res != 0) {
                const char *err_msg;

                err_msg = gai_strerror (res);
                g_warning ("Unable to lookup numeric info: %s",
                        err_msg ? err_msg : "(null)");
                _gdm_address_debug (address, NULL, NULL, NULL);
        } else {
                ret = TRUE;
        }

        if (servp != NULL) {
                *servp = g_strdup (serv);
        }
        if (hostp != NULL) {
                *hostp = g_strdup (host);
        }

        return ret;
}

gboolean
gdm_address_is_loopback (GdmAddress *address)
{
        g_return_val_if_fail (address != NULL, FALSE);
        g_return_val_if_fail (address->ss != NULL, FALSE);

        switch (address->ss->ss_family){
#ifdef  AF_INET6
        case AF_INET6:
                return IN6_IS_ADDR_LOOPBACK (&((struct sockaddr_in6 *)address->ss)->sin6_addr);
                break;
#endif
        case AF_INET:
                return (INADDR_LOOPBACK == htonl (((struct sockaddr_in *)address->ss)->sin_addr.s_addr));
                break;
        default:
                break;
        }

        return FALSE;
}

static void
add_local_siocgifconf (GList **list)
{
        struct ifconf ifc;
        struct ifreq  ifreq;
        struct ifreq *ifr;
        struct ifreq *the_end;
        int           sock;
        char          buf[BUFSIZ];

        if ((sock = socket (PF_INET, SOCK_DGRAM, 0)) < 0) {
                perror ("socket");
                return;
        }

        ifc.ifc_len = sizeof (buf);
        ifc.ifc_buf = buf;
        if (ioctl (sock, SIOCGIFCONF, (char *) &ifc) < 0) {
                perror ("SIOCGIFCONF");
                close (sock);
                return;
        }

        /* Get IP address of each active IP network interface. */
        the_end = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);

        for (ifr = ifc.ifc_req; ifr < the_end; ifr++) {
                if (ifr->ifr_addr.sa_family == AF_INET) {
                        /* IP net interface */
                        ifreq = *ifr;

                        if (ioctl (sock, SIOCGIFFLAGS, (char *) &ifreq) < 0) {
                                perror("SIOCGIFFLAGS");
                        } else if (ifreq.ifr_flags & IFF_UP) {  /* active interface */
                                if (ioctl (sock, SIOCGIFADDR, (char *) &ifreq) < 0) {
                                        perror("SIOCGIFADDR");
                                } else {
                                        GdmAddress *address;
                                        address = gdm_address_new_from_sockaddr ((struct sockaddr *)&ifreq.ifr_addr,
                                                                                 sizeof (struct sockaddr));

                                        gdm_address_debug (address);

                                        *list = g_list_append (*list, address);
                                }
                        }
                }

                /* Support for variable-length addresses. */
#ifdef HAS_SA_LEN
                ifr = (struct ifreq *) ((caddr_t) ifr
                                        + ifr->ifr_addr.sa_len - sizeof(struct sockaddr));
#endif
        }

        close (sock);
}

static void
add_local_addrinfo (GList **list)
{
        char             hostbuf[BUFSIZ];
        struct addrinfo *result;
        struct addrinfo *res;
        struct addrinfo  hints;

        hostbuf[BUFSIZ-1] = '\0';
        if (gethostname (hostbuf, BUFSIZ-1) != 0) {
                g_debug ("%s: Could not get server hostname, using localhost", "gdm_peek_local_address_list");
                snprintf (hostbuf, BUFSIZ-1, "localhost");
        }

        memset (&hints, 0, sizeof (hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_flags = AI_CANONNAME;

        g_debug ("GdmAddress: looking up hostname: %s", hostbuf);
        result = NULL;
        if (getaddrinfo (hostbuf, NULL, &hints, &result) != 0) {
                g_debug ("%s: Could not get address from hostname!", "gdm_peek_local_address_list");

                return;
        }

        for (res = result; res != NULL; res = res->ai_next) {
                GdmAddress *address;

                g_debug ("family=%d sock_type=%d protocol=%d flags=0x%x canonname=%s\n",
                         res->ai_family,
                         res->ai_socktype,
                         res->ai_protocol,
                         res->ai_flags,
                         res->ai_canonname ? res->ai_canonname : "(null)");
                address = gdm_address_new_from_sockaddr (res->ai_addr, res->ai_addrlen);
                *list = g_list_append (*list, address);
        }

        if (result != NULL) {
                freeaddrinfo (result);
                result = NULL;
        }
}

const GList *
gdm_address_peek_local_list (void)
{
        static GList  *list = NULL;
        static time_t  last_time = 0;

        /* Don't check more then every 5 seconds */
        if (last_time + 5 > time (NULL)) {
                return list;
        }

        g_list_foreach (list, (GFunc)gdm_address_free, NULL);
        g_list_free (list);
        list = NULL;

        last_time = time (NULL);

        add_local_siocgifconf (&list);
        add_local_addrinfo (&list);

        return list;
}

gboolean
gdm_address_is_local (GdmAddress *address)
{
        const GList *list;

        if (gdm_address_is_loopback (address)) {
                return TRUE;
        }

        list = gdm_address_peek_local_list ();

        while (list != NULL) {
                GdmAddress *addr = list->data;

                if (gdm_address_equal (address, addr)) {
                        return TRUE;
                }

                list = list->next;
        }

        return FALSE;
}

/**
 * gdm_address_copy:
 * @address: A #GdmAddress.
 *
 * Duplicates @address.
 *
 * Return value: Duplicated @address or %NULL if @address was not valid.
 **/
GdmAddress *
gdm_address_copy (GdmAddress *address)
{
        GdmAddress *addr;

        g_return_val_if_fail (address != NULL, NULL);

        addr = g_new0 (GdmAddress, 1);
        addr->ss = g_memdup (address->ss, sizeof (struct sockaddr_storage));

        return addr;
}

/**
 * gdm_address_free:
 * @address: A #GdmAddress.
 *
 * Frees the memory allocated for @address.
 **/
void
gdm_address_free (GdmAddress *address)
{
        g_return_if_fail (address != NULL);

        g_free (address->ss);
        g_free (address);
}

