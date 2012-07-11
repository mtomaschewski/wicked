/*
 *	DHCP Unique Identifier (DUID)
 *
 *	Copyright (C) 2012 Marius Tomaschewski <mt@suse.de>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, see <http://www.gnu.org/licenses/> or write
 *	to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *	Boston, MA 02110-1301 USA.
 */
#ifndef __WICKED_DHCP6_DUID_H__
#define __WICKED_DHCP6_DUID_H__

#include <wicked/types.h>
#include <wicked/util.h>


/*
 * DUID can be not more than 128 octets long,
 *      not including the type code.
 *
 * http://tools.ietf.org/html/rfc3315#section-9.1
 */
#define NI_DUID_TYPE_LEN	sizeof(uint16_t)
#define NI_DUID_DATA_LEN	128
#define	NI_DUID_MAX_SIZE	(NI_DUID_TYPE_LEN + NI_DUID_DATA_LEN)

/*
 * DUID (LLT type 1) generation time is in seconds since
 * midnight (UTC), January 1, 2000, modulo 2^32. This is
 * the offset in seconds since POSIX.1 time() 1970 epoch:
 *
 * http://tools.ietf.org/html/rfc3315#section-9.2
 */
#define NI_DUID_TIME_EPOCH	946684800

/*
 * Known DUID types
 *
 * http://tools.ietf.org/html/rfc3315#section-9.1
 * http://tools.ietf.org/html/rfc6355#section-6
 */
#define NI_DUID_TYPE_LLT	1
#define NI_DUID_TYPE_EN		2
#define NI_DUID_TYPE_LL		3
#define NI_DUID_TYPE_UUID	4

/* TODO: MAC based uuid duid, see http://tools.ietf.org/html/rfc4122#section-4.1.6  */

/*
 * DUID len is not more than 128 octets + type code.
 */
typedef ni_opaque_t		ni_duid_t;

#if 0
extern ni_duid_t *		ni_duid_new_llt (unsigned int hwtype, const void *hwaddr, size_t len);
extern ni_duid_t *		ni_duid_new_ll  (unsigned int hwtype, const void *hwaddr, size_t len);
extern ni_duid_t *		ni_duid_new_en  (unsigned int enumber, const void *identifier, size_t len);

extern ni_duid_t *		ni_duid_clone(const ni_duid_t * duid);
#endif

extern void			ni_duid_free (ni_duid_t * duid);
extern void			ni_duid_clear(ni_duid_t * duid);

extern ni_bool_t		ni_duid_init_llt(ni_duid_t *duid, unsigned int hwtype, const void *hwaddr, size_t len);
extern ni_bool_t		ni_duid_init_ll (ni_duid_t *duid, unsigned int hwtype, const void *hwaddr, size_t len);
extern ni_bool_t		ni_duid_init_en (ni_duid_t *duid, unsigned int enumber, const void *identifier, size_t len);
extern ni_bool_t		ni_duid_init_uuid(ni_duid_t *duid, const ni_uuid_t *uuid);

extern ni_bool_t		ni_duid_parse_hex(ni_duid_t *duid, const char *str);

extern ni_bool_t		ni_duid_eq(const ni_duid_t *duid1, const ni_duid_t *duid2);

#endif /* __WICKED_DHCP6_DUID_H__ */
