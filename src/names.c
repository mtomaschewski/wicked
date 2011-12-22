/*
 * Routines for mapping constants to names and vice versa
 *
 * Copyright (C) 2009-2011 Olaf Kirch <okir@suse.de>
 */
#include <stdlib.h>
#include <string.h>
#include <net/if_arp.h>

#include <wicked/netinfo.h>
#include <wicked/addrconf.h>

/*
 * Map interface link layer types to strings and vice versa
 */
static ni_intmap_t __linktype_names[] = {
	{ "unknown",		NI_IFTYPE_UNKNOWN },
	{ "loopback",		NI_IFTYPE_LOOPBACK },
	{ "ethernet",		NI_IFTYPE_ETHERNET },
	{ "bridge",		NI_IFTYPE_BRIDGE },
	{ "bond",		NI_IFTYPE_BOND },
	{ "vlan",		NI_IFTYPE_VLAN },
	{ "wireless",		NI_IFTYPE_WIRELESS },
	{ "infiniband",		NI_IFTYPE_INFINIBAND },
	{ "ppp",		NI_IFTYPE_PPP },
	{ "slip",		NI_IFTYPE_SLIP },
	{ "sit",		NI_IFTYPE_SIT },
	{ "gre",		NI_IFTYPE_GRE },
	{ "isdn",		NI_IFTYPE_ISDN },
	{ "tunnel",		NI_IFTYPE_TUNNEL },
	{ "tunnel6",		NI_IFTYPE_TUNNEL6 },
	{ "virtual-tunnel",	NI_IFTYPE_TUN },
	{ "virtual-tap",	NI_IFTYPE_TAP },
	{ "dummy",		NI_IFTYPE_DUMMY },

	{ NULL }
};

int
ni_linktype_name_to_type(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __linktype_names, &value) < 0)
		return -1;
	return value;
}

const char *
ni_linktype_type_to_name(unsigned int type)
{
	return ni_format_int_mapped(type, __linktype_names);
}

/*
 * Map addrconf name to type constant and vice versa
 */
static ni_intmap_t __addrconf_names[] = {
	{ "dhcp",	NI_ADDRCONF_DHCP	},
	{ "static",	NI_ADDRCONF_STATIC	},
	{ "auto",	NI_ADDRCONF_AUTOCONF	},
	{ "ibft",	NI_ADDRCONF_IBFT	},

	{ NULL }
};

int
ni_addrconf_name_to_type(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __addrconf_names, &value) < 0)
		return -1;
	return value;
}

const char *
ni_addrconf_type_to_name(unsigned int type)
{
	return ni_format_int_mapped(type, __addrconf_names);
}

/*
 * Map addrconf states to strings and vice versa
 */
static ni_intmap_t __addrconf_states[] = {
	{ "none",		NI_ADDRCONF_STATE_NONE },
	{ "requesting",		NI_ADDRCONF_STATE_REQUESTING },
	{ "granted",		NI_ADDRCONF_STATE_GRANTED },
	{ "releasing",		NI_ADDRCONF_STATE_RELEASING },
	{ "released",		NI_ADDRCONF_STATE_RELEASED },
	{ "failed",		NI_ADDRCONF_STATE_FAILED },

	{ NULL }
};

int
ni_addrconf_name_to_state(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __addrconf_states, &value) < 0)
		return -1;
	return value;
}

const char *
ni_addrconf_state_to_name(unsigned int type)
{
	return ni_format_int_mapped(type, __addrconf_states);
}

/*
 * Map addrconf update values to strings and vice versa
 */
static ni_intmap_t __addrconf_updates[] = {
	{ "default-route",	NI_ADDRCONF_UPDATE_DEFAULT_ROUTE },
	{ "hostname",		NI_ADDRCONF_UPDATE_HOSTNAME },
	{ "hosts-file",		NI_ADDRCONF_UPDATE_HOSTSFILE },
	{ "syslog",		NI_ADDRCONF_UPDATE_SYSLOG },
	{ "resolver",		NI_ADDRCONF_UPDATE_RESOLVER },
	{ "nis",		NI_ADDRCONF_UPDATE_NIS },
	{ "ntp",		NI_ADDRCONF_UPDATE_NTP },
	{ "smb",		NI_ADDRCONF_UPDATE_NETBIOS },
	{ "slp",		NI_ADDRCONF_UPDATE_SLP },

	{ NULL }
};

int
ni_addrconf_name_to_update_target(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __addrconf_updates, &value) < 0)
		return -1;
	return value;
}

const char *
ni_addrconf_update_target_to_name(unsigned int type)
{
	return ni_format_int_mapped(type, __addrconf_updates);
}

/*
 * Map address family names to type constants and vice versa
 */
static ni_intmap_t __addrfamily_names[] = {
	{ "ipv4",	AF_INET		},
	{ "ipv6",	AF_INET6	},

	{ NULL }
};

int
ni_addrfamily_name_to_type(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __addrfamily_names, &value) < 0)
		return -1;
	return value;
}

const char *
ni_addrfamily_type_to_name(unsigned int type)
{
	return ni_format_int_mapped(type, __addrfamily_names);
}

/*
 * Map ARPHRD_* constants to string
 */
#define __ARPMAP(token, name) { #name, ARPHRD_##token }

static ni_intmap_t __arphrd_names[] = {
 __ARPMAP(NETROM,		netrom),
 __ARPMAP(ETHER,		ether),
 __ARPMAP(EETHER,		eether),
 __ARPMAP(AX25,			ax25),
 __ARPMAP(PRONET,		pronet),
 __ARPMAP(CHAOS,		chaos),
 __ARPMAP(IEEE802,		ieee802),
 __ARPMAP(ARCNET,		arcnet),
 __ARPMAP(APPLETLK,		appletlk),
 __ARPMAP(DLCI,			dlci),
 __ARPMAP(ATM,			atm),
 __ARPMAP(METRICOM,		metricom),
 __ARPMAP(IEEE1394,		ieee1394),
 __ARPMAP(EUI64,		eui64),
 __ARPMAP(INFINIBAND,		infiniband),
 __ARPMAP(SLIP,			slip),
 __ARPMAP(CSLIP,		cslip),
 __ARPMAP(SLIP6,		slip6),
 __ARPMAP(CSLIP6,		cslip6),
 __ARPMAP(RSRVD,		rsrvd),
 __ARPMAP(ADAPT,		adapt),
 __ARPMAP(ROSE,			rose),
 __ARPMAP(X25,			x25),
 __ARPMAP(HWX25,		hwx25),
 __ARPMAP(PPP,			ppp),
 __ARPMAP(HDLC,			hdlc),
 __ARPMAP(LAPB,			lapb),
 __ARPMAP(DDCMP,		ddcmp),
 __ARPMAP(RAWHDLC,		rawhdlc),
 __ARPMAP(TUNNEL,		tunnel),
 __ARPMAP(TUNNEL6,		tunnel6),
 __ARPMAP(FRAD,			frad),
 __ARPMAP(SKIP,			skip),
 __ARPMAP(LOOPBACK,		loopback),
 __ARPMAP(LOCALTLK,		localtlk),
 __ARPMAP(FDDI,			fddi),
 __ARPMAP(BIF,			bif),
 __ARPMAP(SIT,			sit),
 __ARPMAP(IPDDP,		ipddp),
 __ARPMAP(IPGRE,		ipgre),
 __ARPMAP(PIMREG,		pimreg),
 __ARPMAP(HIPPI,		hippi),
 __ARPMAP(ASH,			ash),
 __ARPMAP(ECONET,		econet),
 __ARPMAP(IRDA,			irda),
 __ARPMAP(FCPP,			fcpp),
 __ARPMAP(FCAL,			fcal),
 __ARPMAP(FCPL,			fcpl),
 __ARPMAP(FCFABRIC,		fcfabric),
 __ARPMAP(IEEE802_TR,		IEEE802_tr),
 __ARPMAP(IEEE80211,		ieee80211),
 __ARPMAP(IEEE80211_PRISM,	IEEE80211_prism),
 __ARPMAP(IEEE80211_RADIOTAP,	IEEE80211_radiotap),
 __ARPMAP(VOID,			void),
 __ARPMAP(NONE,			none),
 /* 65534 tun */

 { 0 }
};

int
ni_arphrd_name_to_type(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __arphrd_names, &value) < 0)
		return -1;
	return value;
}

const char *
ni_arphrd_type_to_name(unsigned int type)
{
	return ni_format_int_mapped(type, __arphrd_names);
}

/*
 * Map event names to type constants and vice versa
 */
static ni_intmap_t __event_names[] = {
	{ "link-create",	NI_EVENT_LINK_CREATE },
	{ "link-delete",	NI_EVENT_LINK_DELETE },
	{ "link-up",		NI_EVENT_LINK_UP },
	{ "link-down",		NI_EVENT_LINK_DOWN },
	{ "network-up",		NI_EVENT_NETWORK_UP },
	{ "network-down",	NI_EVENT_NETWORK_DOWN },

	{ NULL }
};

ni_event_t
ni_event_name_to_type(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __event_names, &value) < 0)
		return -1;
	return value;
}

const char *
ni_event_type_to_name(ni_event_t type)
{
	return ni_format_int_mapped(type, __event_names);
}

static ni_intmap_t __ifaction_names[] = {
	{ "boot",		NI_IFACTION_BOOT },
	{ "shutdown",		NI_IFACTION_SHUTDOWN },
	{ "manual-up",		NI_IFACTION_MANUAL_UP },
	{ "manual-down",	NI_IFACTION_MANUAL_DOWN },
	{ "link-up",		NI_IFACTION_LINK_UP },
	{ "link-down",		NI_IFACTION_LINK_DOWN },

	{ NULL }
};

int
ni_ifaction_name_to_type(const char *name)
{
	unsigned int value;

	if (ni_parse_int_mapped(name, __ifaction_names, &value) < 0)
		return -1;
	return value;
}

const char *
ni_ifaction_type_to_name(unsigned int type)
{
	return ni_format_int_mapped(type, __ifaction_names);
}

/*
 * Map netinfo interface types to ARPHRD_ and vice versa
 */
static struct __ni_arptype_iftype_map {
	int		type;
	unsigned int	arp_type;
} __ni_arptype_iftype_map[] = {
      {	NI_IFTYPE_LOOPBACK,	ARPHRD_LOOPBACK	},
      {	NI_IFTYPE_ETHERNET,	ARPHRD_ETHER	},
      {	NI_IFTYPE_BRIDGE,	ARPHRD_ETHER	},
      {	NI_IFTYPE_BOND,		ARPHRD_ETHER	},
      {	NI_IFTYPE_VLAN,		ARPHRD_ETHER	},
      {	NI_IFTYPE_WIRELESS,	ARPHRD_ETHER	},
      {	NI_IFTYPE_INFINIBAND,	ARPHRD_INFINIBAND },
      {	NI_IFTYPE_PPP,		ARPHRD_PPP	},
      {	NI_IFTYPE_SLIP,		ARPHRD_SLIP	},
      {	NI_IFTYPE_SLIP,		ARPHRD_CSLIP	},
      {	NI_IFTYPE_SIT,		ARPHRD_SIT	},
      {	NI_IFTYPE_GRE,		ARPHRD_IPGRE	},
      {	NI_IFTYPE_TUNNEL,	ARPHRD_TUNNEL	},
      {	NI_IFTYPE_TUNNEL6,	ARPHRD_TUNNEL6	},
      {	NI_IFTYPE_TUN,		ARPHRD_ETHER	},
      {	NI_IFTYPE_TAP,		ARPHRD_ETHER	},
      {	NI_IFTYPE_DUMMY,	ARPHRD_LOOPBACK	},

      {	NI_IFTYPE_UNKNOWN, ARPHRD_NONE }
};

unsigned int
ni_arphrd_type_to_iftype(int arp_type)
{
	struct __ni_arptype_iftype_map *map;

	for (map = __ni_arptype_iftype_map; map->arp_type != ARPHRD_NONE; ++map)
		if (map->arp_type == arp_type)
			break;
	return map->type;
}

int
ni_iftype_to_arphrd_type(unsigned int iftype)
{
	struct __ni_arptype_iftype_map *map;

	for (map = __ni_arptype_iftype_map; map->arp_type != ARPHRD_NONE; ++map)
		if (map->type == iftype)
			break;
	return map->arp_type;
}
