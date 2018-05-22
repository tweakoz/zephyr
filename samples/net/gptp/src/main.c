/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if 1
#define SYS_LOG_DOMAIN "gptp-app"
#define NET_SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#define NET_LOG_ENABLED 1
#endif

#include <zephyr.h>
#include <errno.h>

#include <net/net_core.h>
#include <net/net_l2.h>
#include <net/net_if.h>
#include <net/ethernet.h>
#include <net/gptp.h>

struct func_stat {
	u32_t ptr;
	s64_t times_executed;
	s64_t last_start;
	s64_t total_time;
};

#define NUMBER_OF_PROFILED_CB 200
struct func_stat func_stats[NUMBER_OF_PROFILED_CB];
unsigned int funcs_n = 0;

inline int add_func(void *func)
{
	if (funcs_n >= NUMBER_OF_PROFILED_CB)
		return -1;
	func_stats[funcs_n].ptr = func;
	func_stats[funcs_n].times_executed = 0;
	func_stats[funcs_n].last_start = 0;
	func_stats[funcs_n].total_time = 0;
	return funcs_n++;
}

inline int get_func_num(void *func)
{
	int i;
	for (i = 0; i < funcs_n; ++i) {
		if (func_stats[i].ptr == (u32_t)func)
			return i;
	}
	return -1;
}

void print_func_stats()
{
	int i;
	for (i = 0; i < funcs_n; ++i) {
		printk("%08x%10lld%10lld\n",
		       func_stats[i].ptr - 1,
		       func_stats[i].total_time,
		       func_stats[i].times_executed);
	}
}

void __cyg_profile_func_enter (void *func,  void *caller)
{
	int n = 0;
	struct func_stat *f;
	n = get_func_num(func);

	if (n < 0) {
		if (funcs_n >= NUMBER_OF_PROFILED_CB)
			return;
		n = add_func(func);
	}
	f = &func_stats[n];
	f->times_executed++;
	f->last_start = k_uptime_get();
}

void __cyg_profile_func_exit (void *func, void *caller)
{
	int n = -1;
	struct func_stat *f;
	n = get_func_num(func);
	if (n < 0)
		return;
	f = &func_stats[n];
	f->total_time += (k_uptime_get() - f->last_start);
}

static struct gptp_phase_dis_cb phase_dis;

/* Enable following if you want to run gPTP over VLAN with this application */
#define GPTP_OVER_VLAN 0
#define GPTP_VLAN_TAG 42

#if defined(CONFIG_NET_VLAN)
/* User data for the interface callback */
struct ud {
	struct net_if *first;
	struct net_if *second;
	struct net_if *third;
};

static void iface_cb(struct net_if *iface, void *user_data)
{
	struct ud *ud = user_data;

	if (net_if_l2(iface) != &NET_L2_GET_NAME(ETHERNET)) {
		return;
	}

	if (!ud->first) {
		ud->first = iface;
		return;
	}

	if (!ud->second) {
		ud->second = iface;
		return;
	}

	if (!ud->third) {
		ud->third = iface;
		return;
	}
}

static int setup_iface(struct net_if *iface, const char *ipv6_addr,
		       const char *ipv4_addr, u16_t vlan_tag)
{
	struct net_if_addr *ifaddr;
	struct in_addr addr4;
	struct in6_addr addr6;
	int ret;

	ret = net_eth_vlan_enable(iface, vlan_tag);
	if (ret < 0) {
		NET_ERR("Cannot enable VLAN for tag %d (%d)", vlan_tag, ret);
	}

	if (net_addr_pton(AF_INET6, ipv6_addr, &addr6)) {
		NET_ERR("Invalid address: %s", ipv6_addr);
		return -EINVAL;
	}

	ifaddr = net_if_ipv6_addr_add(iface, &addr6, NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		NET_ERR("Cannot add %s to interface %p", ipv6_addr, iface);
		return -EINVAL;
	}

	if (net_addr_pton(AF_INET, ipv4_addr, &addr4)) {
		NET_ERR("Invalid address: %s", ipv6_addr);
		return -EINVAL;
	}

	ifaddr = net_if_ipv4_addr_add(iface, &addr4, NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		NET_ERR("Cannot add %s to interface %p", ipv4_addr, iface);
		return -EINVAL;
	}

	NET_DBG("Interface %p VLAN tag %d setup done.", iface, vlan_tag);

	return 0;
}

static int init_vlan(void)
{
	struct ud ud;
	int ret;

	memset(&ud, 0, sizeof(ud));

	net_if_foreach(iface_cb, &ud);

#if GPTP_OVER_VLAN
	ret = net_eth_vlan_enable(ud.first, GPTP_VLAN_TAG);
	if (ret < 0) {
		NET_ERR("Cannot enable VLAN for tag %d (%d)", GPTP_VLAN_TAG,
			ret);
	}
#endif

	/* This sample has two VLANs. For the second one we need to manually
	 * create IP address for this test. But first the VLAN needs to be
	 * added to the interface so that IPv6 DAD can work properly.
	 */
	ret = setup_iface(ud.second,
			  CONFIG_NET_SAMPLE_IFACE2_MY_IPV6_ADDR,
			  CONFIG_NET_SAMPLE_IFACE2_MY_IPV4_ADDR,
			  CONFIG_NET_SAMPLE_IFACE2_VLAN_TAG);
	if (ret < 0) {
		return ret;
	}

	ret = setup_iface(ud.third,
			  CONFIG_NET_SAMPLE_IFACE3_MY_IPV6_ADDR,
			  CONFIG_NET_SAMPLE_IFACE3_MY_IPV4_ADDR,
			  CONFIG_NET_SAMPLE_IFACE3_VLAN_TAG);
	if (ret < 0) {
		return ret;
	}

	return 0;
}
#endif /* CONFIG_NET_VLAN */

static void gptp_phase_dis_cb(u8_t *gm_identity,
			      u16_t *time_base,
			      struct gptp_scaled_ns *last_gm_ph_change,
			      double *last_gm_freq_change)
{
	char output[sizeof("xx:xx:xx:xx:xx:xx:xx:xx")];
	static u8_t id[8];

	if (memcmp(id, gm_identity, sizeof(id))) {
		memcpy(id, gm_identity, sizeof(id));

		NET_DBG("GM %s last phase %d.%lld",
			gptp_sprint_clock_id(gm_identity, output,
					     sizeof(output)),
			last_gm_ph_change->high,
			last_gm_ph_change->low);
	}
}

static int init_app(void)
{
#if defined(CONFIG_NET_VLAN)
	if (init_vlan() < 0) {
		NET_ERR("Cannot setup VLAN");
	}
#endif

	gptp_register_phase_dis_cb(&phase_dis, gptp_phase_dis_cb);

	return 0;
}

void main(void)
{
	init_app();
	while (1) {
		k_sleep(K_SECONDS(10));
		print_func_stats();
		printk("---\n");
	}
}
