#include <arpa/inet.h>
#include <dev/board.h>
#include <dev/nvmem.h>
#include <netinet/if_ether.h>
#include <sys/confnet.h>

#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gorp/edline.h>

#include "conf.h"

#define	DEFAULT_MAC		"\x9e\x79\xcf\x6f\xd6\xc9"
#define DEFAULT_IP		"10.32.96.50"
#define DEFAULT_NETMASK		"255.255.255.0"
#define DEFAULT_GATEWAY		"0.0.0.0"

#define REMOTE_IP		"10.32.96.32"
#define REMOTE_PORT		5025

#define CONFIG_OFFSET		256
#define MAGIC_COOKIE		0xdeadbeaf

#define MAC_LEN			18
#define IP_LEN			16
#define PORT_LEN		5

static void conf_set_local_defaults(void);
static void conf_edit_local(void);
static void conf_set_remote_defaults(void);
static void conf_edit_remote(void);

static void
conf_set_local_defaults(void)
{
	printf("Setting default network configuration\n");

	NutNetLoadConfig(DEV_ETHER_NAME);

	memcpy(confnet.cd_name, DEV_ETHER_NAME, sizeof(confnet.cd_name));

	memcpy(confnet.cdn_mac, DEFAULT_MAC, sizeof(confnet.cdn_mac));
	confnet.cdn_cip_addr = inet_addr(DEFAULT_IP);
	confnet.cdn_ip_mask = inet_addr(DEFAULT_NETMASK);
	confnet.cdn_gateway = inet_addr(DEFAULT_GATEWAY);

	if (!NutNetSaveConfig()) {
		printf("Saved network configuration\n");
	} else {
		printf("Failed to save network configuration\n");
	}
}

static void
conf_edit_local(void)
{
	EDLINE *el;
	char buf[64];
	uint8_t *cp;
	uint32_t addr;

	if (NutNetLoadConfig(DEV_ETHER_NAME)) {
		printf("No configuration available, starting with defaults\n");
		conf_set_local_defaults();
	}

	el = EdLineOpen(EDIT_MODE_ECHO);

	do {
		strncpy(buf, ether_ntoa(confnet.cdn_mac), sizeof(buf) - 1);
		printf("MAC Address: ");
		EdLineRead(el, buf, MAC_LEN);
		cp = ether_aton(buf);
	} while (cp == NULL);
	memcpy(confnet.cdn_mac, cp, 6);

	do {
		strncpy(buf, inet_ntoa(confnet.cdn_cip_addr), sizeof(buf) - 1);
		printf("IP address: ");
		EdLineRead(el, buf, IP_LEN);
		addr = inet_addr(buf);
	} while (addr == -1);
	confnet.cdn_cip_addr = addr;

	do {
		strncpy(buf, inet_ntoa(confnet.cdn_ip_mask), sizeof(buf) - 1);
		printf("IP mask: ");
		EdLineRead(el, buf, IP_LEN);
		addr = inet_addr(buf);
	} while (addr == -1);
	confnet.cdn_ip_mask = addr;

	do {
		strncpy(buf, inet_ntoa(confnet.cdn_gateway), sizeof(buf) - 1);
		printf("IP gate: ");
		EdLineRead(el, buf, IP_LEN);
		addr = inet_addr(buf);
	} while (addr == -1);
	confnet.cdn_gateway = addr;

	if (!NutNetSaveConfig()) {
		printf("Saved network configuration\n");
	} else {
		printf("Failed to save network configuration\n");
	}
}

static void
conf_set_remote_defaults(void)
{
	struct remote_conf conf;

	printf("Setting default remote address\n");

	conf.magic = MAGIC_COOKIE;

	conf.addr = inet_addr(REMOTE_IP);
	conf.port = REMOTE_PORT;

	if (!NutNvMemSave(CONFIG_OFFSET, &conf, sizeof(conf))) {
		printf("Saved remote address\n");
	} else {
		printf("Failed to save remote address\n");
	}
}

static void
conf_edit_remote(void)
{
	EDLINE *el;
	char buf[64];
	uint32_t addr;
	struct remote_conf conf;

	if (NutNvMemLoad(CONFIG_OFFSET, &conf, sizeof(conf))) {
		printf("Failed to read from non-volatile memory\n");
	}

	if (conf.magic != MAGIC_COOKIE) {
		printf("No remote address available, starting with defaults\n");
		conf_set_remote_defaults();

		if (NutNvMemLoad(CONFIG_OFFSET, &conf, sizeof(conf))) {
			printf("Failed to read from non-volatile memory\n");
		}
	}

	el = EdLineOpen(EDIT_MODE_ECHO);

	do {
		strncpy(buf, inet_ntoa(conf.addr), sizeof(buf) - 1);
		printf("Remote IP address: ");
		EdLineRead(el, buf, IP_LEN);
		addr = inet_addr(buf);
	} while (addr == -1);
	conf.addr = addr;

	sprintf(buf, "%d", conf.port);
	printf("Remote port: ");
	EdLineRead(el, buf, PORT_LEN);
	conf.port = atoi(buf);

	if (!NutNvMemSave(CONFIG_OFFSET, &conf, sizeof(conf))) {
		printf("Saved remote address\n");
	} else {
		printf("Failed to save remote address\n");
	}
}

void
conf_edit(void)
{
	conf_edit_local();
	conf_edit_remote();
}

void
conf_set_defaults(void)
{
	conf_set_local_defaults();
	conf_set_remote_defaults();
}

struct remote_conf
conf_get_remote(void)
{
	struct remote_conf conf;

	if (NutNvMemLoad(CONFIG_OFFSET, &conf, sizeof(conf))) {
		printf("Failed to read from non-volatile memory\n");
	}

	if (conf.magic != MAGIC_COOKIE) {
		printf("No remote address available, starting with defaults\n");
		conf_set_remote_defaults();

		if (NutNvMemLoad(CONFIG_OFFSET, &conf, sizeof(conf))) {
			printf("Failed to read from non-volatile memory\n");
		}
	}

	return conf;
}
