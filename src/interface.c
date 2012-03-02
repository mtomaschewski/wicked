/*
 * Handle network interface objects
 *
 * Copyright (C) 2009-2012 Olaf Kirch <okir@suse.de>
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <net/if_arp.h>
#include <signal.h>
#include <time.h>

#include <wicked/netinfo.h>
#include <wicked/addrconf.h>
#include <wicked/bridge.h>
#include <wicked/bonding.h>
#include <wicked/ethernet.h>
#include <wicked/wireless.h>
#include <wicked/vlan.h>
#include <wicked/socket.h>
#include <wicked/resolver.h>
#include <wicked/nis.h>
#include "netinfo_priv.h"
#include "config.h"

/*
 * Constructor for network interface.
 * Takes interface name and ifindex.
 */
ni_netdev_t *
__ni_interface_new(const char *name, unsigned int index)
{
	ni_netdev_t *ifp;

	ifp = calloc(1, sizeof(*ifp) * 2);
	if (!ifp)
		return NULL;

	ifp->users = 1;
	ifp->link.type = NI_IFTYPE_UNKNOWN;
	ifp->link.arp_type = ARPHRD_NONE;
	ifp->link.hwaddr.type = ARPHRD_NONE;
	ifp->link.ifindex = index;

	if (name)
		ifp->name = xstrdup(name);

	/* Initialize address family specific info */
	__ni_afinfo_init(&ifp->ipv4, AF_INET);
	__ni_afinfo_init(&ifp->ipv6, AF_INET6);

	return ifp;
}

ni_netdev_t *
ni_interface_new(ni_netconfig_t *nc, const char *name, unsigned int index)
{
	ni_netdev_t *ifp;

	ifp = __ni_interface_new(name, index);
	if (nc && ifp)
		__ni_interface_list_append(&nc->interfaces, ifp);
	
	return ifp;
}

/*
 * Destructor function (and assorted helpers)
 */
void
ni_interface_clear_addresses(ni_netdev_t *ifp)
{
	ni_address_list_destroy(&ifp->addrs);
}

void
ni_interface_clear_routes(ni_netdev_t *ifp)
{
	ni_route_list_destroy(&ifp->routes);
}

static void
ni_interface_free(ni_netdev_t *ifp)
{
	ni_string_free(&ifp->name);
	ni_string_free(&ifp->link.qdisc);
	ni_string_free(&ifp->link.kind);
	ni_string_free(&ifp->link.alias);

	/* Clear out addresses, stats */
	ni_interface_clear_addresses(ifp);
	ni_interface_clear_routes(ifp);
	ni_interface_set_link_stats(ifp, NULL);
	ni_interface_set_ethernet(ifp, NULL);
	ni_interface_set_bonding(ifp, NULL);
	ni_interface_set_bridge(ifp, NULL);
	ni_interface_set_vlan(ifp, NULL);
	ni_interface_set_wireless(ifp, NULL);

	ni_addrconf_lease_list_destroy(&ifp->leases);

	free(ifp);
}

/*
 * Reference counting of interface objects
 */
ni_netdev_t *
ni_interface_get(ni_netdev_t *ifp)
{
	if (!ifp->users)
		return NULL;
	ifp->users++;
	return ifp;
}

int
ni_interface_put(ni_netdev_t *ifp)
{
	if (!ifp->users) {
		ni_error("ni_interface_put: bad mojo");
		return 0;
	}
	ifp->users--;
	if (ifp->users == 0) {
		ni_interface_free(ifp);
		return 0;
	}
	return ifp->users;
}

/*
 * This is a convenience function for adding routes to an interface.
 */
ni_route_t *
ni_interface_add_route(ni_netdev_t *ifp,
				unsigned int prefix_len,
				const ni_sockaddr_t *dest,
				const ni_sockaddr_t *gw)
{
	return __ni_route_new(&ifp->routes, prefix_len, dest, gw);
}

/*
 * Get the interface's VLAN information
 */
ni_vlan_t *
ni_interface_get_vlan(ni_netdev_t *ifp)
{
	if (!ifp->link.vlan)
		ifp->link.vlan = __ni_vlan_new();
	return ifp->link.vlan;
}

void
ni_interface_set_vlan(ni_netdev_t *ifp, ni_vlan_t *vlan)
{
	if (ifp->link.vlan)
		ni_vlan_free(ifp->link.vlan);
	ifp->link.vlan = vlan;
}

/*
 * Get the interface's bridge information
 */
ni_bridge_t *
ni_interface_get_bridge(ni_netdev_t *ifp)
{
	if (ifp->link.type != NI_IFTYPE_BRIDGE)
		return NULL;
	if (!ifp->bridge)
		ifp->bridge = ni_bridge_new();
	return ifp->bridge;
}

void
ni_interface_set_bridge(ni_netdev_t *ifp, ni_bridge_t *bridge)
{
	if (ifp->bridge)
		ni_bridge_free(ifp->bridge);
	ifp->bridge = bridge;
}

/*
 * Get the interface's bonding information
 */
ni_bonding_t *
ni_interface_get_bonding(ni_netdev_t *ifp)
{
	if (ifp->link.type != NI_IFTYPE_BOND)
		return NULL;
	if (!ifp->bonding)
		ifp->bonding = ni_bonding_new();
	return ifp->bonding;
}

void
ni_interface_set_bonding(ni_netdev_t *ifp, ni_bonding_t *bonding)
{
	if (ifp->bonding)
		ni_bonding_free(ifp->bonding);
	ifp->bonding = bonding;
}

/*
 * Get the interface's ethernet information
 */
ni_ethernet_t *
ni_interface_get_ethernet(ni_netdev_t *ifp)
{
	if (ifp->link.type != NI_IFTYPE_ETHERNET)
		return NULL;
	if (!ifp->ethernet)
		ifp->ethernet = calloc(1, sizeof(ni_ethernet_t));
	return ifp->ethernet;
}

void
ni_interface_set_ethernet(ni_netdev_t *ifp, ni_ethernet_t *ethernet)
{
	if (ifp->ethernet)
		ni_ethernet_free(ifp->ethernet);
	ifp->ethernet = ethernet;
}

/*
 * Set the interface's wireless info
 */
ni_wireless_t *
ni_interface_get_wireless(ni_netdev_t *dev)
{
	if (dev->link.type != NI_IFTYPE_WIRELESS)
		return NULL;
	if (!dev->wireless)
		dev->wireless = ni_wireless_new(dev);
	return dev->wireless;
}

void
ni_interface_set_wireless(ni_netdev_t *ifp, ni_wireless_t *wireless)
{
	if (ifp->wireless)
		ni_wireless_free(ifp->wireless);
	ifp->wireless = wireless;
}

/*
 * Set the interface's link stats
 */
void
ni_interface_set_link_stats(ni_netdev_t *ifp, ni_link_stats_t *stats)
{
	if (ifp->link.stats)
		free(ifp->link.stats);
	ifp->link.stats = stats;
}

/*
 * Locate any lease for the same addrconf mechanism
 */
ni_addrconf_lease_t *
__ni_interface_find_lease(ni_netdev_t *ifp, int family, ni_addrconf_mode_t type, int remove)
{
	ni_addrconf_lease_t *lease, **pos;

	for (pos = &ifp->leases; (lease = *pos) != NULL; pos = &lease->next) {
		if (lease->type == type && lease->family == family) {
			if (remove) {
				*pos = lease->next;
				lease->next = NULL;
			}
			return lease;
		}
	}

	return NULL;
}

/*
 * We received an updated lease from an addrconf agent.
 */
int
ni_interface_set_lease(ni_netdev_t *ifp, ni_addrconf_lease_t *lease)
{
	ni_addrconf_lease_t **pos;

	ni_interface_unset_lease(ifp, lease->family, lease->type);
	for (pos = &ifp->leases; *pos != NULL; pos = &(*pos)->next)
		;

	*pos = lease;
	return 0;
}

int
ni_interface_unset_lease(ni_netdev_t *ifp, int family, ni_addrconf_mode_t type)
{
	ni_addrconf_lease_t *lease;

	if ((lease = __ni_interface_find_lease(ifp, family, type, 1)) != NULL)
		ni_addrconf_lease_free(lease);
	return 0;
}

ni_addrconf_lease_t *
ni_interface_get_lease(ni_netdev_t *dev, int family, ni_addrconf_mode_t type)
{
	return __ni_interface_find_lease(dev, family, type, 0);
}

ni_addrconf_lease_t *
ni_interface_get_lease_by_owner(ni_netdev_t *dev, const char *owner)
{
	ni_addrconf_lease_t *lease;

	for (lease = dev->leases; lease; lease = lease->next) {
		if (ni_string_eq(lease->owner, owner))
			return lease;
	}

	return NULL;
}

/*
 * Given an address, look up the lease owning it
 */
ni_addrconf_lease_t *
__ni_interface_address_to_lease(ni_netdev_t *ifp, const ni_address_t *ap)
{
	ni_addrconf_lease_t *lease;

	for (lease = ifp->leases; lease; lease = lease->next) {
		if (__ni_lease_owns_address(lease, ap))
			return lease;
	}

	return NULL;
}

int
__ni_lease_owns_address(const ni_addrconf_lease_t *lease, const ni_address_t *match)
{
	time_t now = time(NULL);
	ni_address_t *ap;

	if (!lease || lease->family != match->family)
		return 0;

	/* IPv6 autoconf is special; we record the IPv6 address prefixes in the
	 * lease. */
	if (lease->family == AF_INET6 && lease->type == NI_ADDRCONF_AUTOCONF) {
		ni_route_t *rp;

		for (rp = lease->routes; rp; rp = rp->next) {
			if (rp->prefixlen != match->prefixlen)
				continue;
			if (rp->expires && rp->expires <= now)
				continue;
			if (ni_address_prefix_match(rp->prefixlen, &rp->destination, &match->local_addr))
				return 1;
		}
	}

	for (ap = lease->addrs; ap; ap = ap->next) {
		if (ap->prefixlen != match->prefixlen)
			continue;
		if (ap->expires && ap->expires <= now)
			continue;

		/* Note: for IPv6 autoconf, we will usually have recorded the
		 * address prefix only; the address that will eventually be picked
		 * by the autoconf logic will be different */
		if (lease->family == AF_INET6 && lease->type == NI_ADDRCONF_AUTOCONF) {
			if (!ni_address_prefix_match(match->prefixlen, &ap->local_addr, &match->local_addr))
				continue;
		} else {
			if (ni_address_equal(&ap->local_addr, &match->local_addr))
				continue;
		}

		if (ni_address_equal(&ap->peer_addr, &match->peer_addr)
		 && ni_address_equal(&ap->anycast_addr, &match->anycast_addr))
			return 1;
	}
	return 0;
}

/*
 * Given a route, look up the lease owning it
 */
ni_addrconf_lease_t *
__ni_interface_route_to_lease(ni_netdev_t *ifp, const ni_route_t *rp)
{
	ni_addrconf_lease_t *lease;
	ni_address_t *ap;

	if (!ifp || !rp)
		return NULL;

	for (lease = ifp->leases; lease; lease = lease->next) {
		/* First, check if this is an interface route */
		for (ap = lease->addrs; ap; ap = ap->next) {
			if (rp->prefixlen == ap->prefixlen
			 && ni_address_prefix_match(ap->prefixlen, &rp->destination, &ap->local_addr))
				return lease;
		}

		if (__ni_lease_owns_route(lease, rp))
			return lease;
	}

	return NULL;
}

ni_route_t *
__ni_lease_owns_route(const ni_addrconf_lease_t *lease, const ni_route_t *rp)
{
	ni_route_t *own;

	if (!lease)
		return 0;

	for (own = lease->routes; own; own = own->next) {
		if (ni_route_equal(own, rp))
			return own;
	}
	return NULL;
}

/*
 * Guess the interface type based on its name and characteristics
 * We should really make this configurable!
 */
static ni_intmap_t __ifname_types[] = {
	{ "ib",		NI_IFTYPE_INFINIBAND	},
	{ "ip6tunl",	NI_IFTYPE_TUNNEL6	},
	{ "ipip",	NI_IFTYPE_TUNNEL	},
	{ "sit",	NI_IFTYPE_SIT		},
	{ "tun",	NI_IFTYPE_TUN		},

	{ NULL }
};
int
ni_interface_guess_type(ni_netdev_t *ifp)
{
	if (ifp->link.type != NI_IFTYPE_UNKNOWN)
		return ifp->link.type;

	if (ifp->name == NULL)
		return ifp->link.type;

	ifp->link.type = NI_IFTYPE_ETHERNET;
	if (!strcmp(ifp->name, "lo")) {
		ifp->link.type = NI_IFTYPE_LOOPBACK;
	} else {
		ni_intmap_t *map;

		for (map = __ifname_types; map->name; ++map) {
			unsigned int len = strlen(map->name);

			if (!strncmp(ifp->name, map->name, len)
			 && isdigit(ifp->name[len])) {
				ifp->link.type = map->value;
				break;
			}
		}
	}

	return ifp->link.type;
}

/*
 * Functions for handling lists of interfaces
 */
void
__ni_interface_list_destroy(ni_netdev_t **list)
{
	ni_netdev_t *ifp;

	while ((ifp = *list) != NULL) {
		*list = ifp->next;
		ni_interface_put(ifp);
	}
}

void
__ni_interface_list_append(ni_netdev_t **list, ni_netdev_t *new_ifp)
{
	ni_netdev_t *ifp;

	while ((ifp = *list) != NULL)
		list = &ifp->next;

	new_ifp->next = NULL;
	*list = new_ifp;
}

