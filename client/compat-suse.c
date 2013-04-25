/*
 * Compat functions for SUSE ifcfg style files
 * This support is not complete yet.
 *
 * Copyright (C) 2010-2012 Olaf Kirch <okir@suse.de>
 */

#include <limits.h>
#include <ctype.h>
#include <errno.h>

#include <wicked/address.h>
#include <wicked/util.h>
#include <wicked/logging.h>
#include <wicked/sysconfig.h>
#include <wicked/netinfo.h>
#include <wicked/route.h>
#include <wicked/xml.h>
#include <wicked/ethernet.h>
#include <wicked/bonding.h>
#include <wicked/bridge.h>
#include <wicked/vlan.h>
#include <wicked/fsm.h>

#include <wicked/objectmodel.h>
#include <wicked/dbus.h>
#include "wicked-client.h"

static ni_compat_netdev_t *__ni_suse_read_interface(const char *, const char *);
static ni_bool_t	__ni_suse_read_globals(const char *path);
static void		__ni_suse_free_globals(void);
static ni_bool_t	__ni_suse_sysconfig_read(ni_sysconfig_t *, ni_compat_netdev_t *);
static ni_bool_t	__process_indexed_variables(const ni_sysconfig_t *, ni_netdev_t *, const char *,
				ni_bool_t (*)(const ni_sysconfig_t *, ni_netdev_t *, const char *));
static ni_var_t *	__find_indexed_variable(const ni_sysconfig_t *, const char *, const char *);
static ni_route_t *	__ni_suse_read_routes(const char *);

static ni_sysconfig_t *	__ni_suse_config_defaults = NULL;
static ni_sysconfig_t *	__ni_suse_dhcp_defaults   = NULL;
static ni_route_t *	__ni_suse_global_routes   = NULL;


#define __NI_SUSE_SYSCONFIG_NETWORK_DIR		"/etc/sysconfig/network"
#define __NI_SUSE_CONFIG_IFPREFIX		"ifcfg-"
#define __NI_SUSE_CONFIG_GLOBAL			"config"
#define __NI_SUSE_CONFIG_DHCP			"dhcp"
#define __NI_SUSE_ROUTES_IFPREFIX		"ifroute-"
#define __NI_SUSE_ROUTES_GLOBAL			"routes"

#define __NI_VLAN_TAG_MAX			4094

static ni_bool_t
__ni_suse_ifcfg_valid_suffix(const char *name, size_t pfxlen)
{
	const char *blacklist[] = {
		"~", ".old", ".bak", ".orig", ".scpmbackup",
		".rpmnew", ".rpmsave", ".rpmorig",
	};
	size_t nlen, slen, i;

	nlen = ni_string_len(name);
	if (nlen <= pfxlen)
		return FALSE;

	for (i = 0; i < sizeof(blacklist)/sizeof(blacklist[0]); ++i) {
		const char *suffix = blacklist[i];

		slen = ni_string_len(suffix);
		if (nlen < slen)
			continue;

		if (ni_string_eq(suffix, name + (nlen - slen)))
			return FALSE;
	}
	return TRUE;
}

static ni_bool_t
__ni_suse_ifcfg_valid_prefix(const char *basename, const char *prefix)
{
	size_t pfxlen;

	if (!basename || !prefix)
		return FALSE;

	pfxlen = strlen(prefix);
	if (strncmp(basename, prefix, pfxlen))
		return FALSE;

	return TRUE;
}

static int
__ni_suse_valid_ifname(const char *ifname)
{
	size_t i, len = ni_string_len(ifname);

	if (!len || len >= IFNAMSIZ)
		return FALSE;

	if (!isalnum((unsigned char)ifname[0]))
		return FALSE;

	for(i = 1; i < len; ++i) {
		if(isalnum((unsigned char)ifname[i]) ||
			ifname[i] == '-' ||
			ifname[i] == '_' ||
			ifname[i] == '.')
			continue;
		return FALSE;
	}
	return TRUE;
}

static int
__ni_suse_ifcfg_scan_files(const char *dirname, ni_string_array_t *res)
{
	ni_string_array_t files = NI_STRING_ARRAY_INIT;
	const char *pattern = __NI_SUSE_CONFIG_IFPREFIX"*";
	size_t pfxlen = sizeof(__NI_SUSE_CONFIG_IFPREFIX)-1;
	unsigned int i, count = res->count;

	if( !ni_scandir(dirname, pattern, &files))
		return 0;

	for(i = 0; i < files.count; ++i) {
		const char *file = files.data[i];

		if (!__ni_suse_ifcfg_valid_suffix(file, pfxlen)) {
			ni_debug_readwrite("Ignoring blacklisted %sfile: %s",
					__NI_SUSE_CONFIG_IFPREFIX, file);
			continue;
		}

		ni_string_array_append(res, file);
	}
	ni_string_array_destroy(&files);

	return res->count - count;
}

ni_bool_t
__ni_suse_get_interfaces(const char *path, ni_compat_netdev_array_t *result)
{
	ni_string_array_t files = NI_STRING_ARRAY_INIT;
	ni_bool_t success = FALSE;
	int i;

	if (ni_string_len(path) == 0)
		path = __NI_SUSE_SYSCONFIG_NETWORK_DIR;

	if (ni_isdir(path)) {
		if (!__ni_suse_read_globals(path))
			goto done;

		if (!__ni_suse_ifcfg_scan_files(path, &files)) {
			ni_error("No ifcfg files found");
			goto done;
		}

		for (i = 0; i < files.count; ++i) {
			const char *filename = files.data[i];
			const char *ifname = filename + (sizeof(__NI_SUSE_CONFIG_IFPREFIX)-1);
			char pathbuf[PATH_MAX];
			ni_compat_netdev_t *compat;

			snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, filename);
			if (!(compat = __ni_suse_read_interface(pathbuf, ifname)))
				goto done;
			ni_compat_netdev_array_append(result, compat);
		}
	} else {
		char *basedir = NULL;
		ni_compat_netdev_t *compat;

		ni_string_dup(&basedir, ni_dirname(path));
		if (!__ni_suse_read_globals(basedir)) {
			ni_string_free(&basedir);
			goto done;
		}
		ni_string_free(&basedir);

		if (!(compat = __ni_suse_read_interface(path, NULL)))
			goto done;
		ni_compat_netdev_array_append(result, compat);
	}

	success = TRUE;

done:
	__ni_suse_free_globals();
	ni_string_array_destroy(&files);
	return success;
}

/*
 * Read global ifconfig files like ifcfg-routes and dhcp
 */
static ni_bool_t
__ni_suse_read_globals(const char *path)
{
	char pathbuf[PATH_MAX];

	if (path == NULL) {
		ni_error("%s: path is NULL", __func__);
		return FALSE;
	}

	__ni_suse_free_globals();

	snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, __NI_SUSE_CONFIG_GLOBAL);
	if (ni_file_exists(pathbuf)) {
		__ni_suse_config_defaults = ni_sysconfig_read(pathbuf);
		if (__ni_suse_config_defaults == NULL) {
			ni_error("unable to parse %s", pathbuf);
			return FALSE;
		}
	}

	snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, __NI_SUSE_CONFIG_DHCP);
	if (ni_file_exists(pathbuf)) {
		__ni_suse_dhcp_defaults = ni_sysconfig_read(pathbuf);
		if (__ni_suse_dhcp_defaults == NULL) {
			ni_error("unable to parse %s", pathbuf);
			return FALSE;
		}
	}

	snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, __NI_SUSE_ROUTES_GLOBAL);
	if (ni_file_exists(pathbuf)) {
		if ((__ni_suse_global_routes = __ni_suse_read_routes(pathbuf)) == NULL)
			return FALSE;
	}

	return TRUE;
}

static void
__ni_suse_free_globals(void)
{
	if (__ni_suse_config_defaults)
		ni_sysconfig_destroy(__ni_suse_config_defaults);

	if (__ni_suse_dhcp_defaults)
		ni_sysconfig_destroy(__ni_suse_dhcp_defaults);

	ni_route_list_destroy(&__ni_suse_global_routes);
}

/*
 * Read the routing information from sysconfig/network/routes.
 */
ni_route_t *
__ni_suse_read_routes(const char *filename)
{
	ni_route_t *route_list = NULL;
	char buffer[512];
	FILE *fp;

	if ((fp = fopen(filename, "r")) == NULL) {
		ni_error("unable to open %s: %m", filename);
		return NULL;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		char *dest, *gw, *mask = NULL, *ifname = NULL, *type = NULL;
		ni_sockaddr_t dest_addr, gw_addr, mask_addr;
		unsigned int prefixlen = 255;
		ni_route_t *rp;

		buffer[strcspn(buffer, "#\r\n")] = '\0';
		
		if (!(dest = strtok(buffer, " \t")))
			continue;

		gw = strtok(NULL, " \t");
		if (gw)
			mask = strtok(NULL, " \t");
		if (mask)
			ifname = strtok(NULL, " \t");
		if (ifname)
			type = strtok(NULL, " \t");

		if (gw == NULL || !strcmp(gw, "-")) {
			/* This is a local interface route.
			 * Some SLES versions have an ifcfg-route with
			 * "127/8" in it. */
			memset(&gw_addr, 0, sizeof(gw_addr));
		} else
		if (ni_sockaddr_parse(&gw_addr, gw, AF_UNSPEC) < 0) {
			ni_error("%s: cannot parse gw addr \"%s\"",
					filename, gw);
			goto error;
		}

		if (!strcmp(dest, "default")) {
			memset(&dest_addr, 0, sizeof(dest_addr));
			dest_addr.ss_family = gw_addr.ss_family;
			prefixlen = 0;
		} else {
			char *sp;

			if ((sp = strchr(dest, '/')) != NULL) {
				*sp++ = '\0';
				prefixlen = strtoul(sp, NULL, 10);
			}
			if (ni_sockaddr_parse(&dest_addr, dest, AF_UNSPEC) < 0) {
				ni_error("%s: cannot parse dest addr \"%s\"",
						filename, dest);
				goto error;
			}
			if (prefixlen == 255) {
				if (!mask || !strcmp(mask, "-")) {
					/* No prefix and no mask given - assume the destination
					   is a single address. Use the full address length
					   as prefix. */
					prefixlen = ni_af_address_length(dest_addr.ss_family) * 8;
				} else {
					/* We have a mask. Try to parse it and count the bits. */
					if (ni_sockaddr_parse(&mask_addr, mask, AF_UNSPEC) < 0) {
						ni_error("%s: cannot parse mask addr \"%s\"",
								filename, mask);
						goto error;
					}
					prefixlen = ni_sockaddr_netmask_bits(&mask_addr);
				}
			}
		}

		rp = ni_route_new(prefixlen, &dest_addr, &gw_addr, &route_list);
		if (rp == NULL) {
			ni_error("Unable to add route %s %s %s", dest, gw, mask?: "-");
			goto error;
		}

		if (ifname && strcmp(ifname, "-"))
			ni_string_dup(&rp->nh.device, ifname);

		(void) type; /* currently ignored */
	}

	fclose(fp);
	return route_list;

error:
	ni_route_list_destroy(&route_list);
	fclose(fp);
	return NULL;
}

/*
 * Read the configuration of a single interface from a sysconfig file
 */
static ni_compat_netdev_t *
__ni_suse_read_interface(const char *filename, const char *ifname)
{
	const char *basename = ni_basename(filename);
	size_t pfxlen = sizeof(__NI_SUSE_CONFIG_IFPREFIX)-1;
	ni_compat_netdev_t *compat = NULL;
	ni_sysconfig_t *sc;

	if (ni_string_len(ifname) == 0) {
		if (!__ni_suse_ifcfg_valid_prefix(basename, __NI_SUSE_CONFIG_IFPREFIX)) {
			ni_error("Rejecting file without '%s' prefix: %s",
				__NI_SUSE_CONFIG_IFPREFIX, filename);
			return NULL;
		}
		if (!__ni_suse_ifcfg_valid_suffix(basename, pfxlen)) {
			ni_error("Rejecting blacklisted %sfile: %s",
				__NI_SUSE_CONFIG_IFPREFIX, filename);
			return NULL;
		}
		ifname = basename + pfxlen;
	}

	if (!__ni_suse_valid_ifname(ifname)) {
		ni_error("Rejecting suspect interface name: %s", ifname);
		return NULL;
	}

	if (!(sc = ni_sysconfig_read(filename))) {
		ni_error("unable to parse %s", filename);
		goto error;
	}

	compat = ni_compat_netdev_new(ifname);
	if (__ni_suse_sysconfig_read(sc, compat) < 0)
		goto error;

	ni_sysconfig_destroy(sc);
	return compat;

error:
	if (sc)
		ni_sysconfig_destroy(sc);
	if (compat)
		ni_compat_netdev_free(compat);
	return NULL;
}

ni_compat_netdev_t *
ni_compat_netdev_new(const char *ifname)
{
	ni_compat_netdev_t *compat;

	compat = calloc(1, sizeof(*compat));
	compat->dev = ni_netdev_new(ifname, 0);

	return compat;
}

/*
 * Translate the SUSE startmodes to <control> element
 */
static const ni_ifworker_control_t *
__ni_suse_startmode(const char *mode)
{
	static const struct __ni_control_params {
		const char *		name;
		ni_ifworker_control_t	control;
	} __ni_suse_control_params[] = {
		/* manual is the default in ifcfg */
		{ "manual",	{ NULL,		NULL,		TRUE,	FALSE,	30	} },

		{ "auto",	{ "boot",	NULL,		FALSE,	TRUE,	30	} },
		{ "boot",	{ "boot",	NULL,		FALSE,	TRUE,	30	} },
		{ "onboot",	{ "boot",	NULL,		FALSE,	TRUE,	30	} },
		{ "on",		{ "boot",	NULL,		FALSE,	TRUE,	30	} },

		{ "hotplug",	{ "boot",	NULL,		FALSE,	FALSE,	30	} },
		{ "ifplugd",	{ "ignore",	NULL,		FALSE,	FALSE,	30	} },

		{ "nfsroot",	{ "boot",	"localfs",	TRUE,	TRUE,	NI_IFWORKER_INFINITE_TIMEOUT	} },
		{ "off",	{ "off",	NULL,		FALSE,	FALSE,	0	} },

		{ NULL }
	};
	const struct __ni_control_params *p, *params = NULL;

	if (mode != NULL) {
		for (p = __ni_suse_control_params; p->name; ++p) {
			if (ni_string_eq(p->name, mode)) {
				params = p;
				break;
			}
		}
	}

	if (!params)
		params = &__ni_suse_control_params[0];

	return &params->control;
}

/*
 * Try loopback interface
 */
static ni_bool_t
try_loopback(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;

	/* Consider "lo" as a reserved name for loopback. */
	if (strcmp(dev->name, "lo"))
		return FALSE;

	dev->link.type = NI_IFTYPE_LOOPBACK;
	return TRUE;
}

/*
 * Handle Ethernet devices
 */
static ni_bool_t
try_ethernet(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	ni_ethernet_t *eth;
	const char *value;

	/* FIXME: this is an array ETHTOOL_OPTIONS[SUFFIX] */
	if ((value = ni_sysconfig_get_value(sc, "ETHTOOL_OPTIONS")) != NULL) {
		/* ETHTOOL_OPTIONS comes in two flavors
		 *   - starting with a dash: this is "-$option ifname $stuff"
		 *   - otherwise: this is a paramater to be passed to "-s ifname"
		 */
		/* FIXME: parse and translate to xml */
		dev->link.type = NI_IFTYPE_ETHERNET;
		eth = ni_netdev_get_ethernet(dev);
		(void) eth;
	}

	return TRUE;
}

/*
 * Handle bonding devices
 *
 * Bonding interfaces have variables BONDIG_SLAVE_0, BONDIG_SLAVE_1, ... that
 * describe the slave devices.
 *
 * Global bonding configuration is contained in BONDING_MODULE_OPTS
 */
static ni_bool_t
try_add_bonding_slave(const ni_sysconfig_t *sc, ni_netdev_t *dev, const char *suffix)
{
	ni_bonding_t *bond;
	ni_var_t *var;

	var = __find_indexed_variable(sc, "BONDING_SLAVE", suffix);
	if (!var || !var->value)
		return FALSE;

	dev->link.type = NI_IFTYPE_BOND;

	if ((bond = ni_netdev_get_bonding(dev)) == NULL)
		return FALSE;

	return ni_bonding_add_slave(bond, var->value);
}

static ni_bool_t
try_set_bonding_options(ni_netdev_t *dev, const char *options)
{
	ni_string_array_t temp;
	ni_bonding_t * bond;
	unsigned int i;
	ni_bool_t ret = TRUE;

	if ((bond = ni_netdev_get_bonding(dev)) == NULL)
		return FALSE;

	ni_string_array_init(&temp);
	ni_string_split(&temp, options, " \t", 0);
	for (i = 0; i < temp.count; ++i) {
		char *key = temp.data[i];
		char *val = strchr(key, '=');

		if (val != NULL)
			*val++ = '\0';

		if (!ni_string_len(key) || !ni_string_len(val)) {
			ni_error("ifcfg-%s: Unable to parse bonding options '%s'",
				dev->name, options);
			ret = FALSE;
			break;
		}
		if (!ni_bonding_set_option(bond, key, val)) {
			ni_error("ifcfg-%s: Unable to parse bonding option: %s=%s",
				dev->name, key, val);
			ret = FALSE;
			break;
		}
	}
	ni_string_array_destroy(&temp);

	return ret;
}

static ni_bool_t
try_bonding(ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	const char *module_opts, *err;
	ni_bool_t enabled;

	if (!ni_sysconfig_get_boolean(sc, "BONDING_MASTER", &enabled) || !enabled)
		return FALSE;

	if (!__process_indexed_variables(sc, dev, "BONDING_SLAVE", try_add_bonding_slave))
		return FALSE;

	if ((module_opts = ni_sysconfig_get_value(sc, "BONDING_MODULE_OPTS")) != NULL) {
		if (!try_set_bonding_options(dev, module_opts))
			return FALSE;
	}

	if ((err = ni_bonding_validate(ni_netdev_get_bonding(dev))) != NULL) {
		ni_error("ifcfg-%s: bonding validation: %s",
			dev->name, err);
		return FALSE;
	}

	return TRUE;
}

/*
 * Bridge devices are recognized by BRIDGE=yes
 */
static ni_bool_t
try_bridge(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	ni_bridge_t *bridge;
	ni_bool_t enabled;
	const char *value;

	if (!ni_sysconfig_get_boolean(sc, "BRIDGE", &enabled) || !enabled)
		return FALSE;

	dev->link.type = NI_IFTYPE_BRIDGE;
	bridge = ni_netdev_get_bridge(dev);

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_STP")) != NULL) {
		if (!strcasecmp(value, "off") || !strcasecmp(value, "no")) {
			bridge->stp = TRUE;
		} else
		if (!strcasecmp(value, "on") || !strcasecmp(value, "yes")) {
			bridge->stp = FALSE;
		} else {
			ni_error("ifcfg-%s: Cannot parse BRIDGE_STP='%s'",
				dev->name, value);
			return FALSE;
		}
	}

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_PRIORITY")) != NULL) {
		if (ni_parse_int(value, &bridge->priority, 0) < 0) {
			ni_error("ifcfg-%s: Cannot parse BRIDGE_PRIORITY='%s'",
				dev->name, value);
			return FALSE;
		}
	}

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_AGEINGTIME")) != NULL) {
		if (ni_parse_double(value, &bridge->ageing_time) < 0) {
			ni_error("ifcfg-%s: Cannot parse BRIDGE_AGEINGTIME='%s'",
				dev->name, value);
			return FALSE;
		}
	}

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_FORWARDDELAY")) != NULL) {
		if (ni_parse_double(value, &bridge->forward_delay) < 0) {
			ni_error("ifcfg-%s: Cannot parse BRIDGE_FORWARDDELAY='%s'",
				dev->name, value);
			return FALSE;
		}
	}
	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_HELLOTIME")) != NULL) {
		if (ni_parse_double(value, &bridge->hello_time) < 0) {
			ni_error("ifcfg-%s: Cannot parse BRIDGE_HELLOTIME='%s'",
				dev->name, value);
			return FALSE;
		}
	}

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_MAXAGE")) != NULL) {
		if (ni_parse_double(value, &bridge->max_age) < 0) {
			ni_error("ifcfg-%s: Cannot parse BRIDGE_MAXAGE='%s'",
				dev->name, value);
			return FALSE;
		}
	}

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_PORTS")) != NULL) {
		char *portnames = NULL, *name_pos = NULL, *name = NULL;

		ni_string_dup(&portnames, value);
		for (name = strtok_r(portnames, " \t", &name_pos);
		     name != NULL;
		     name = strtok_r(NULL, " \t", &name_pos)) {

			if (!__ni_suse_valid_ifname(name)) {
				ni_error("ifcfg-%s: BRIDGE_PORTS='%s' "
					 "rejecting suspect port name '%s'",
					 dev->name, value, name);
				free(portnames);
				return FALSE;
			}

			ni_bridge_port_new(bridge, name, 0);
		}
		ni_string_free(&portnames);
	}

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_PORTPRIORITIES")) != NULL) {
		char *portprios = NULL, *prio_pos = NULL, *prio = NULL;
		unsigned int tmp, i = 0;

		ni_string_dup(&portprios, value);
		for (prio = strtok_r(portprios, " \t", &prio_pos);
		     prio != NULL && i < bridge->ports.count;
		     prio = strtok_r(NULL, " \t", &prio_pos), ++i) {
			ni_bridge_port_t *port = bridge->ports.data[i];

			if (!strcmp("-", prio))
				continue;

			if (ni_parse_int(prio, &tmp, 0) < 0) {
				ni_error("ifcfg-%s: BRIDGE_PORTPRIORITIES='%s' "
					 "unable to parse port '%s' priority '%s'",
					 dev->name, value, port->ifname, prio);
				free(portprios);
				return FALSE;
			}
			port->priority = tmp;
		}
		ni_string_free(&portprios);
	}

	if ((value = ni_sysconfig_get_value(sc, "BRIDGE_PATHCOSTS")) != NULL) {
		char *portcosts = NULL, *cost_pos = NULL, *cost = NULL;
		unsigned int tmp, i = 0;

		ni_string_dup(&portcosts, value);
		for (cost = strtok_r(portcosts, " \t", &cost_pos);
		     cost != NULL && i < bridge->ports.count;
		     cost = strtok_r(NULL, " \t", &cost_pos), ++i) {
			ni_bridge_port_t *port = bridge->ports.data[i++];

			if (!strcmp("-", cost))
				continue;

			if (ni_parse_int(cost, &tmp, 0) < 0) {
				ni_error("ifcfg-%s: BRIDGE_PATHCOSTS='%s' "
					 "unable to parse port '%s' costs '%s'",
					 dev->name, value, port->ifname, cost);
				free(portcosts);
				return FALSE;
			}
			port->path_cost = tmp;
		}
		ni_string_free(&portcosts);
	}

	if ((value = ni_bridge_validate(bridge)) != NULL) {
		ni_error("ifcfg-%s: bridge validation: %s", dev->name, value);
		return FALSE;
	}
	return TRUE;
}

static ni_bool_t
__try_vlan_tag_parse(const char *str, unsigned int *tag)
{
	char *end = NULL;

	if (!ni_string_len(str) || !isdigit((unsigned char)str[0]))
		return FALSE;

	*tag = strtoul(str, &end, 10);
	if (!end || *end != '\0')
		return FALSE;

	return TRUE;
}

/*
 * VLAN interfaces are recognized by their name (vlan<N>)
 */
static ni_bool_t
try_vlan(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	ni_vlan_t *vlan;
	const char *etherdev = NULL;
	const char *vlantag = NULL;
	unsigned int tag;
	size_t len;

	if ((etherdev = ni_sysconfig_get_value(sc, "ETHERDEVICE")) == NULL)
		return FALSE;

	if (!strcmp(dev->name, etherdev)) {
		ni_error("ifcfg-%s: ETHERDEVICE=\"%s\" self-reference",
			dev->name, etherdev);
		return FALSE;
	}

	if ((vlantag = ni_sysconfig_get_value(sc, "VLAN_ID")) != NULL) {
		if (!__try_vlan_tag_parse(vlantag, &tag)) {
			ni_error("ifcfg-%s: Cannot parse VLAN_ID=\"%s\"",
				dev->name, vlantag);
			return FALSE;
		}
	} else {
		if ((vlantag = strrchr(dev->name, '.')) != NULL) {
			/* name.<TAG> */
			++vlantag;
		} else {
			/* name<TAG> */
			len = strlen(dev->name);
			vlantag = &dev->name[len];
			while(len > 0 && isdigit((unsigned char)vlantag[-1]))
				vlantag--;
		}
		if (!__try_vlan_tag_parse(vlantag, &tag)) {
			ni_error("ifcfg-%s: Cannot parse vlan-tag from interface name",
				dev->name);
			return FALSE;
		}
	}
	if (tag > __NI_VLAN_TAG_MAX) {
		ni_error("ifcfg-%s: VLAN tag %u is out of numerical range",
			dev->name, tag);
		return FALSE;
#if 0
	} else if (tag == 0) {
		ni_warn("%s: VLAN tag 0 disables VLAN filter and is probably not what you want",
			dev->name, tag);
#endif
	}

	dev->link.type = NI_IFTYPE_VLAN;
	vlan = ni_netdev_get_vlan(dev);
	ni_string_dup(&vlan->physdev_name, etherdev);
	vlan->tag = tag;

	return TRUE;
}

/*
 * Handle Wireless devices
 * Not yet implemented
 */
static ni_bool_t
try_wireless(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;

	if (ni_sysconfig_get(sc, "WIRELESS_ESSID") == NULL)
		return FALSE;

	dev->link.type = NI_IFTYPE_WIRELESS;
	ni_warn("ifcfg-%s: conversion of wireless interfaces not yet supported", dev->name);

	return TRUE;
}

/*
 * Handle Tunnel interfaces
 */
static ni_bool_t
try_tunnel(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	const char *value;
	static const ni_intmap_t __tunnel_types[] = {
		{ "tun",	NI_IFTYPE_TUN		},
		{ "tap",	NI_IFTYPE_TAP		},
		{ "sit",	NI_IFTYPE_SIT		},
		{ "gre",	NI_IFTYPE_GRE		},
		{ "ipip",	NI_IFTYPE_TUNNEL	},
		{ "ip6tnl",	NI_IFTYPE_TUNNEL6	},
		{ NULL,		NI_IFTYPE_UNKNOWN	},
	};

	/* FIXME: this are just the types... */
	if ((value = ni_sysconfig_get_value(sc, "TUNNEL")) != NULL) {
		const ni_intmap_t *map;

		for (map = __tunnel_types; map->name; ++map) {
			if (!strcmp(map->name, value)) {
				dev->link.type = (ni_iftype_t)value;
				return TRUE;
			}
		}
	}
	return TRUE;
}

/*
 * Static addrconf:
 *
 * Given a suffix like "" or "_1", try to get the IP address and related information.
 * This will evaluate
 *   IPADDR_x
 *   PREFIXLEN_x if needed
 *   BROADCAST_x
 *   REMOTE_IPADDR_x
 */
static ni_bool_t
__get_ipaddr(const ni_sysconfig_t *sc, const char *suffix, ni_address_t **list)
{
	ni_var_t *var;
	ni_sockaddr_t local_addr;
	unsigned int prefixlen;
	ni_address_t *ap;

	var = __find_indexed_variable(sc, "IPADDR", suffix);
	if (!var || !var->value || !var->value[0])
		return TRUE;

	if (!ni_sockaddr_prefix_parse(var->value, &local_addr, &prefixlen)) {
cannot_parse:
		ni_error("Unable to parse %s=\"%s\"", var->name, var->value);
		return FALSE;
	}

	/* If the address wasn't in addr/prefix format, go look elsewhere */
	if (prefixlen == ~0U) {
		ni_sockaddr_t netmask;

		/* Try PREFIXLEN variable */
		var = __find_indexed_variable(sc, "PREFIXLEN", suffix);
		if (var && var->value) {
			prefixlen = strtoul(var->value, NULL, 0);
		} else
		if (local_addr.ss_family == AF_INET
		 && (var = __find_indexed_variable(sc, "NETMASK", suffix)) != NULL
		 && ni_sockaddr_parse(&netmask, var->value, AF_INET) >= 0) {
			prefixlen = ni_sockaddr_netmask_bits(&netmask);
		} else {
			unsigned int dummy, len;

			if (!ni_af_sockaddr_info(local_addr.ss_family, &dummy, &len))
				goto cannot_parse;
			prefixlen = len * 8;
		}
	}

	ap = ni_address_new(local_addr.ss_family, prefixlen, &local_addr, list);
	if (ap->family == AF_INET) {
		var = __find_indexed_variable(sc, "BROADCAST", suffix);
		if (var) {
			ni_sockaddr_parse(&ap->bcast_addr, var->value, AF_INET);
			if (ap->bcast_addr.ss_family != ap->family) {
				ni_error("%s: ignoring BROADCAST%s=%s (wrong address family)",
						sc->pathname, suffix, var->value);
				ap->bcast_addr.ss_family = AF_UNSPEC;
			}
		} else {
			/* Clear the default, it's useless */
			memset(&ap->bcast_addr, 0, sizeof(ap->bcast_addr));
		}
	}

	var = __find_indexed_variable(sc, "REMOTE_IPADDR", suffix);
	if (var) {
		ni_sockaddr_parse(&ap->peer_addr, var->value, AF_UNSPEC);
		if (ap->peer_addr.ss_family != ap->family) {
			ni_error("%s: ignoring REMOTE_IPADDR%s=%s (wrong address family)",
					sc->pathname, suffix, var->value);
			ap->peer_addr.ss_family = AF_UNSPEC;
		}
	}

	return TRUE;
}

/*
 * Process static addrconf
 */
static ni_bool_t
__ni_suse_addrconf_static(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	const char *routespath;

	/* Loop over all IPADDR* variables and get the addresses */
	{
		ni_string_array_t names = NI_STRING_ARRAY_INIT;
		unsigned int i;

		if (!ni_sysconfig_find_matching(sc, "IPADDR", &names))
			return FALSE;

		for (i = 0; i < names.count; ++i) {
			if (!__get_ipaddr(sc, names.data[i] + 6, &dev->addrs))
				return FALSE;
		}
		ni_string_array_destroy(&names);
	}

	/* Hack up the loopback interface */
	if (!strcmp(dev->name, "lo")) {
		ni_sockaddr_t local_addr;

		ni_sockaddr_parse(&local_addr, "127.0.0.1", AF_INET);
		if (ni_address_list_find(dev->addrs, &local_addr) == NULL)
			ni_address_new(AF_INET, 8, &local_addr, &dev->addrs);

		ni_sockaddr_parse(&local_addr, "::1", AF_INET6);
		if (ni_address_list_find(dev->addrs, &local_addr) == NULL)
			ni_address_new(AF_INET6, 128, &local_addr, &dev->addrs);
	}

	routespath = ni_sibling_path_printf(sc->pathname, "ifroute-%s", dev->name);
	if (routespath && ni_file_exists(routespath)) {
		dev->routes = __ni_suse_read_routes(routespath);
		if (dev->routes == NULL)
			ni_warn("unable to parse %s", routespath);
	}

	if (__ni_suse_global_routes) {
		ni_route_t *rp;

		for (rp = __ni_suse_global_routes; rp; rp = rp->next) {
			ni_address_t *ap;

			switch (rp->family) {
			case AF_INET:
				if (rp->nh.device && !ni_string_eq(rp->nh.device, dev->name))
					continue;

				for (ap = dev->addrs; ap; ap = ap->next) {
					if (ap->family == AF_INET
					 && rp->nh.gateway.ss_family == AF_INET
					 && ni_address_can_reach(ap, &rp->nh.gateway)) {
						ni_route_list_append(&dev->routes, ni_route_clone(rp));
						break;
					}
				}
				break;

			case AF_INET6:
				/* For IPv6, we add the route as long as the interface name matches */
				if (!rp->nh.device || !ni_string_eq(rp->nh.device, dev->name))
					continue;

				ni_route_list_append(&dev->routes, ni_route_clone(rp));
				break;

			default: ;
			}
		}
	}

	ni_address_list_dedup(&dev->addrs);
	return TRUE;
}

/*
 * Process DHCPv4 addrconf
 */
static ni_bool_t
__ni_suse_addrconf_dhcp4_options(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	const char *string;
	unsigned int uint;

	if ((string = ni_sysconfig_get_value(sc, "DHCLIENT_HOSTNAME_OPTION")) && strcasecmp(string, "auto"))
		ni_string_dup(&compat->dhcp4.hostname, string);
	if ((string = ni_sysconfig_get_value(sc, "DHCLIENT_CLIENT_ID")) != NULL)
		ni_string_dup(&compat->dhcp4.client_id, string);
	if ((string = ni_sysconfig_get_value(sc, "DHCLIENT_VENDOR_CLASS_ID")) != NULL)
		ni_string_dup(&compat->dhcp4.vendor_class, string);

	if (ni_sysconfig_get_integer(sc, "DHCLIENT_WAIT_AT_BOOT", &uint))
		compat->dhcp4.acquire_timeout = uint? uint : NI_IFWORKER_INFINITE_TIMEOUT;
	if (ni_sysconfig_get_integer(sc, "DHCLIENT_LEASE_TIME", &uint))
		compat->dhcp4.lease_time = ((int) uint >= 0)? uint : NI_IFWORKER_INFINITE_TIMEOUT;

	/* Ignored for now:
	   DHCLIENT_USE_LAST_LEASE
	   WRITE_HOSTNAME_TO_HOSTS
	   DHCLIENT_MODIFY_SMB_CONF
	   DHCLIENT_SET_HOSTNAME
	   DHCLIENT_SET_DEFAULT_ROUTE
	 */

	return TRUE;
}

/*
 * Process DHCPv6 addrconf
 */
static ni_bool_t
__ni_suse_addrconf_dhcp6_options(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	(void)sc;
	(void)compat;

#if 0	/* FIXME: Use defaults for now */
	const char *string;
	unsigned int uint;

	if ((string = ni_sysconfig_get_value(sc, "DHCLIENT6_HOSTNAME_OPTION")) && strcasecmp(string, "auto"))
		ni_string_dup(&compat->dhcp4.hostname, string);
	if ((string = ni_sysconfig_get_value(sc, "DHCLIENT6_CLIENT_ID")) != NULL)
		ni_string_dup(&compat->dhcp4.client_id, string);
	if ((string = ni_sysconfig_get_value(sc, "DHCLIENT6_VENDOR_CLASS_ID")) != NULL)
		ni_string_dup(&compat->dhcp4.vendor_class, string);

	if (ni_sysconfig_get_integer(sc, "DHCLIENT6_WAIT_AT_BOOT", &uint))
		compat->dhcp4.acquire_timeout = uint? uint : NI_IFWORKER_INFINITE_TIMEOUT;
	if (ni_sysconfig_get_integer(sc, "DHCLIENT6_LEASE_TIME", &uint))
		compat->dhcp4.lease_time = ((int) uint >= 0)? uint : NI_IFWORKER_INFINITE_TIMEOUT;

	/* Ignored for now:
	   DHCLIENT_USE_LAST_LEASE
	   WRITE_HOSTNAME_TO_HOSTS
	   DHCLIENT_MODIFY_SMB_CONF
	   DHCLIENT_SET_HOSTNAME
	   DHCLIENT_SET_DEFAULT_ROUTE
	 */
#endif
	return TRUE;
}

static ni_bool_t
__ni_suse_addrconf_dhcp4(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	if (compat->dhcp4.enabled)
		return TRUE;

	if (__ni_suse_dhcp_defaults)
		__ni_suse_addrconf_dhcp4_options(__ni_suse_dhcp_defaults, compat);

	/* overwrite DHCP defaults with parameters from this ifcfg file */
	__ni_suse_addrconf_dhcp4_options(sc, compat);

	compat->dhcp4.enabled = TRUE;
	return TRUE;
}

static ni_bool_t
__ni_suse_addrconf_dhcp6(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	if (compat->dhcp6.enabled)
		return TRUE;

	if (__ni_suse_dhcp_defaults)
		__ni_suse_addrconf_dhcp6_options(__ni_suse_dhcp_defaults, compat);

	/* overwrite DHCP defaults with parameters from this ifcfg file */
	__ni_suse_addrconf_dhcp6_options(sc, compat);

	compat->dhcp6.enabled = TRUE;
	return TRUE;
}

static ni_bool_t
__ni_suse_addrconf_autoip4(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	(void)sc;
	(void)compat;

	/* TODO */
	return TRUE;
}

static ni_bool_t
__ni_suse_bootproto(const ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	const char *value;
	char *bp, *s, *p;

	if ((value = ni_sysconfig_get_value(sc, "BOOTPROTO")) == NULL)
		value = "static";
	else if (!value[0] || ni_string_eq(dev->name, "lo"))
		value = "static";

	/* Hmm... bonding slave -- set ethtool, but no link up */
	if (ni_string_eq_nocase(value, "none")) {
		return TRUE;
	}

	/* Hmm... ignore this config completely -> ibft firmware */
	if (ni_string_eq_nocase(value, "ibft")) {
		return TRUE;
	}

	if (ni_string_eq_nocase(value, "6to4")) {
		__ni_suse_addrconf_static(sc, compat);
		return TRUE;
	}

	if (ni_string_eq_nocase(value, "static")) {
		__ni_suse_addrconf_static(sc, compat);
		return TRUE;
	}

	bp = p = NULL;
	ni_string_dup(&bp, value);
	for (s = strtok_r(bp, "+", &p); s; s = strtok_r(NULL, "+", &p)) {
		if(!strcasecmp(s, "dhcp")) {
			__ni_suse_addrconf_dhcp4(sc, compat);
			__ni_suse_addrconf_dhcp6(sc, compat);
		}
		else if (ni_string_eq(value, "dhcp4")) {
			__ni_suse_addrconf_dhcp4(sc, compat);
		}
		else if (ni_string_eq(value, "dhcp6")) {
			__ni_suse_addrconf_dhcp6(sc, compat);
		}
		else if (ni_string_eq(value, "autoip")) {
			__ni_suse_addrconf_autoip4(sc, compat);
		}
		else {
			ni_warn("ifcfg-%s: Unknown BOOTPROTO value \"%s\"",
				dev->name, s);
		}
	}
	ni_string_free(&bp);

	/* static is included in the "+" variants */
	__ni_suse_addrconf_static(sc, compat);
	return TRUE;
}

/*
 * Read an ifcfg file
 */
ni_bool_t
__ni_suse_sysconfig_read(ni_sysconfig_t *sc, ni_compat_netdev_t *compat)
{
	ni_netdev_t *dev = compat->dev;
	const char *value;

	if ((value = ni_sysconfig_get_value(sc, "STARTMODE")) != NULL)
		compat->control = __ni_suse_startmode(value);
	else
		compat->control = __ni_suse_startmode(NULL);

	ni_sysconfig_get_integer(sc, "MTU", &dev->link.mtu);

	if ((value = ni_sysconfig_get_value(sc, "LLADDR")) != NULL
	 && ni_link_address_parse(&dev->link.hwaddr, NI_IFTYPE_ETHERNET, value) < 0) {
		ni_warn("ifcfg-%s: Cannot parse LLADDR=\"%s\"",
				dev->name, value);
	}

	if (!try_loopback(sc, compat)
	 && !try_bonding(sc, compat)
	 && !try_bridge(sc, compat)
	 && !try_vlan(sc, compat)
	 && !try_wireless(sc, compat)
	 && !try_tunnel(sc, compat)
	 && !try_ethernet(sc, compat)
	 )
		;

	__ni_suse_bootproto(sc, compat);
	/* FIXME: What to do with these:
		NAME
		USERCONTROL
	 */

	return 0;
}

/*
 * Given a basename like "IPADDR", try to find all variables with this
 * prefix (eg "IPADDR", "IPADDR_0", "IPADDR_1", ...) and invoke the provided function
 * for each. Note, this passes the variable suffix ("", "_0", "_1") rather than
 * the full variable name into the called function.
 */
static ni_bool_t
__process_indexed_variables(const ni_sysconfig_t *sc, ni_netdev_t *dev,
				const char *basename,
				ni_bool_t (*func)(const ni_sysconfig_t *, ni_netdev_t *, const char *))
{
	ni_string_array_t names = NI_STRING_ARRAY_INIT;
	unsigned int i, pfxlen;

	if (!ni_sysconfig_find_matching(sc, basename, &names))
		return FALSE;

	pfxlen = strlen(basename);
	for (i = 0; i < names.count; ++i) {
		if (!func(sc, dev, names.data[i] + pfxlen)) {
			ni_string_array_destroy(&names);
			return FALSE;
		}
	}
	ni_string_array_destroy(&names);
	return TRUE;
}

/*
 * Given a base name and a suffix (eg "IPADDR" and "_1"), build a variable name
 * and look it up.
 */
static ni_var_t *
__find_indexed_variable(const ni_sysconfig_t *sc, const char *basename, const char *suffix)
{
	ni_var_t *res;
	char namebuf[64];

	snprintf(namebuf, sizeof(namebuf), "%s%s", basename, suffix);
	res = ni_sysconfig_get(sc, namebuf);
	if (res && (res->value == NULL || res->value[0] == '\0'))
		res = NULL;
	return res;
}
