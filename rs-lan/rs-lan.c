/*
 * Copyright 2012 Piotr Domagalski <piotr@domagalski.com>
 *
 * Released under MIT license.
 */

#include <arpa/inet.h>
#include <dev/board.h>
#include <dev/reset.h>
#include <dev/watchdog.h>
#include <netinet/if_ether.h>
#include <pro/dhcp.h>
#include <sys/confnet.h>
#include <sys/event.h>
#include <sys/heap.h>
#include <sys/socket.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/version.h>

#ifdef NUTDEBUG
#  include <sys/osdebug.h>
#  include <net/netdebug.h>
#endif

#include <compiler.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "conf.h"

#define UART_DEV		DEV_UART0
#define UART_NAME		DEV_UART0_NAME
#define UART_SPEED		38400

#define CONSOLE_DEV		DEV_UART1
#define CONSOLE_NAME		DEV_UART1_NAME
#define CONSOLE_SPEED		115200

#define NETWORK_TIMEOUT		100
#define MSG_CONNECTED		"CONNECTED\n"
#define MSG_NOTCONNECTED	"NOTCONNECTED\n"
#define MSG_TIMEOUT		"TIMEOUT\n"

#define DHCP_TIMEOUT		5000
#define RESET_ON_CONNECTION_ERR	1
#define RECONNECTION_DELAY	100

#define BUF_SIZE		256

#define log(...)						\
	do { 							\
		if (!configuration_mode) { 			\
			printf("%04d ", (int) time(NULL));	\
			printf(__VA_ARGS__);			\
		}						\
	} while (0)

static void init_console(void);
static void init_network(void);
static void init_uart(void);
static void reset(void);
static const char *start_reason(void);

static int network_timeout_needed(const char *request);
static void start_network_timeout(void);
static void stop_network_timeout(void);
static void network_timeout_callback(HANDLE timer, void *arg);

static void open_connection(void);
static int is_connection_active(void);
static void close_connection(void);

static void led1_on(void);
static void led1_off(void);
static void led2_on(void);
static void led2_off(void);
static void led2_toggle(void);

static FILE * volatile uart_stream;
static FILE * volatile network_stream;
static TCPSOCKET * volatile network_socket;

static volatile HANDLE network_timer;
static volatile HANDLE network_timeout_event;

static volatile int resetting;
static volatile int configuration_mode;
static volatile int log_msgs;

THREAD(uart_thread, arg)
{
	char buf[BUF_SIZE];

	for (;;) {
		if (resetting) {
			NutThreadExit();
		}

		if (!fgets(buf, BUF_SIZE, uart_stream)) {
			log("Failed to read from UART\n");
			reset();
		}

		if (log_msgs) {
			log("UART recvd (%d): %s", strlen(buf), buf);
		}

		if (!is_connection_active()) {
			if (fputs(MSG_NOTCONNECTED, uart_stream) == EOF) {
				log("Failed to send over uart\n");
				reset();
			}

			fflush(uart_stream);
			continue;
		}

		if (fputs(buf, network_stream) == EOF) {
			int err = NutTcpError(network_socket);
			log("Failed to send over socket: %d %s\n", err, strerror(err));
		} else {
			fflush(network_stream);

			if (network_timeout_needed(buf)) {
				start_network_timeout();
			}
		}
	}
}

THREAD(console_thread, arg)
{
	for (;;) {
		if (resetting) {
			NutThreadExit();
		}

		switch (getchar()) {
			case 'c':
				configuration_mode = 1;
				puts("");
				puts("--------- CONFIGURATION ---------");
				conf_edit();
				puts("---------------------------------");
				puts("");
				configuration_mode = 0;
				break;

			case 'C':
				conf_set_defaults();
				break;

			case 'l':
				log_msgs = !log_msgs;
				log("Log msgs: %d\n", log_msgs);
				break;

			case 'u':
				log("Running for %ld seconds\n", (long) time(NULL));
				break;

			case 'h':
				log("%u bytes RAM free\n", (unsigned int) NutHeapAvailable());
				break;

#ifdef NUTDEBUG
			case 'd':
				NutDumpThreadList(stdout);
				puts("");
				NutDumpTimerList(stdout);
				break;
#endif

			case 'r':
				log("Reset\n");
				reset();
				break;

			case 'w':
				log("Watchdog test\n");
				for (;;) {
					continue;
				}
				break;

		}
	}
}

THREAD(network_thread, arg)
{
	char buf[BUF_SIZE];

	for (;;) {
		if (resetting) {
			NutThreadExit();
		}

		if (!is_connection_active()) {
			open_connection();
		}

		if (fgets(buf, BUF_SIZE, network_stream)) {
			stop_network_timeout();
			led2_toggle();

			if (log_msgs) {
				log("Netw recvd (%d): %s", strlen(buf), buf);
			}

			if (fputs(buf, uart_stream) == EOF) {
				log("Failed to send over uart\n");
				reset();
			}

			fflush(uart_stream);
		} else {
			int err = NutTcpError(network_socket);
			log("Failed to read from socket: %d %s\n", err, strerror(err));
			close_connection();
		}
	}
}

THREAD(network_timeout_thread, arg)
{
	NutThreadSetPriority(32);

	for (;;) {
		NutEventWait(&network_timeout_event, NUT_WAIT_INFINITE);

		if (fputs(MSG_TIMEOUT, uart_stream) == EOF) {
			log("Failed to send over uart\n");
			reset();
		}

		fflush(uart_stream);
	}
}

static int
network_timeout_needed(const char *request)
{
	/**
	 * These commands are checked for response and a timeout message is sent if no response is
	 * received after NETWORK_TIMEOUT miliseconds.
	 */
	const char *commands_with_response[] = {
		"READ",
		"MEAS",
	};
	int i;

	for (i = 0; i < sizeof(commands_with_response) / sizeof(commands_with_response[0]); i++) {
		if (strstr(request, commands_with_response[i]) == request)
			return 1;
	}

	return 0;
}

static void
start_network_timeout(void)
{
	if (network_timer) {
		NutTimerStop(network_timer);
	}
	network_timer = NutTimerStart(NETWORK_TIMEOUT, network_timeout_callback, NULL, TM_ONESHOT);
}

static void
stop_network_timeout(void)
{
	if (network_timer) {
		NutTimerStop(network_timer);
		network_timer = 0;
	}
}

static void
network_timeout_callback(HANDLE timer, void *arg)
{
	network_timer = 0;
	NutEventPostAsync(&network_timeout_event);
}

static void
open_connection(void)
{
	struct remote_conf conf;

	conf = conf_get_remote();

	log("Opening connection to %s:%d\n", inet_ntoa(conf.addr), conf.port);

	if (!(network_socket = NutTcpCreateSocket())) {
		log("Failed to create socket\n");
		reset();
	}

	while (NutTcpConnect(network_socket, conf.addr, conf.port)) {
		int err = NutTcpError(network_socket);
		log("Failed to connect: %d %s\n", err, strerror(err));
#if RESET_ON_CONNECTION_ERR == 1
		reset();
#else
		NutDelay(RECONNECTION_DELAY);
#endif
	}

	network_stream = _fdopen((int) network_socket, "r+b");
	if (!network_stream) {
		log("Failed to open socket socket as stream\n");
		reset();
	}

	log("Connected\n");

	led1_on();

	if (fputs(MSG_CONNECTED, uart_stream) == EOF) {
		log("Failed to send over uart\n");
		reset();
	}

	fflush(uart_stream);
}

static int
is_connection_active(void)
{
	return network_socket != NULL && network_stream != NULL;
}

static void
close_connection(void)
{
	led1_off();

	fclose(network_stream);
	network_stream = NULL;

	NutTcpCloseSocket(network_socket);
	network_socket = NULL;
}

static void
init_console(void)
{
	u_long baud = CONSOLE_SPEED;

	NutRegisterDevice(&CONSOLE_DEV, 0, 0);

	freopen(CONSOLE_NAME, "w", stdout);
	freopen(CONSOLE_NAME, "w", stderr);
	freopen(CONSOLE_NAME, "r", stdin);
	_ioctl(_fileno(stdout), UART_SETSPEED, &baud);

	if (!NutThreadCreate("console", console_thread, NULL, 1024)) {
		log("Failed to create a console thread\n");
		reset();
	}
}

static void
init_network(void)
{
	log("Registering %s...\n", DEV_ETHER_NAME);

	if (NutRegisterDevice(&DEV_ETHER, 0, 0)) {
		log("Failed!\n");
		reset();
	}

	log("Done!\n");

	if (NutDhcpIfConfig(DEV_ETHER_NAME, NULL, DHCP_TIMEOUT)) {
		log("Configuring " DEV_ETHER_NAME " failed\n");
		return;
	}

	log("Using MAC %s\n", ether_ntoa(confnet.cdn_mac));
	log("Using IP %s\n", inet_ntoa(confnet.cdn_ip_addr));

	if (!NutThreadCreate("network", network_thread, NULL, 1024)) {
		log("Failed to create a network thread\n");
		reset();
	}

	if (!NutThreadCreate("network-to", network_timeout_thread, NULL, 1024)) {
		log("Failed to create a network-to thread\n");
		reset();
	}
}

static void
init_uart(void)
{
	u_long buad = UART_SPEED;

	NutRegisterDevice(&UART_DEV, 0, 0);

	uart_stream = fopen(UART_NAME, "r+b");
	_ioctl(_fileno(uart_stream), UART_SETSPEED, &buad);

	if (!NutThreadCreate("uart", uart_thread, NULL, 1024)) {
		log("Failed to create uart thread\n");
		reset();
	}
}

static void
reset(void)
{
	if (configuration_mode) {
		return;
	}

	resetting = 1;

	if (network_socket) {
		/*
		 * XXX: This sometimes causes a Data Abort exception, probably because of ongoing
		 * transmission in other threads. But it is better to have DA exception and reset
		 * (since we're resetting anyway), than to leave the socket open, because Agilent
		 * will run out of socket resources and will start rejecting connections.
		 */
		NutTcpCloseSocket(network_socket);
	}

	/* Give some time to send out UART buffers */
	NutSleep(250);

	NutReset();

	log("Reset failed\n");
	for (;;)
		continue;
}

static const char *
start_reason(void)
{
	const char *cause;

	switch (NutResetCause()) {
		case NUT_RSTTYP_POWERUP:
			cause = "Powerup";
			break;
		case NUT_RSTTYP_WATCHDOG:
			cause = "Watchdog";
			break;
		case NUT_RSTTYP_EXTERNAL:
			cause = "External";
			break;
		case NUT_RSTTYP_SOFTWARE:
			cause = "Software";
			break;
		case NUT_RSTTYP_BROWNOUT:
			cause = "Brownout";
			break;
		default:
			cause = "Unknown";
			break;
	}

	return cause;
}

static void
led1_on(void)
{
	/* Led1 - PA10 */
	outr(PIOA_OER, _BV(10));
	outr(PIOA_SODR, _BV(10));
}

static void
led1_off(void)
{
	/* Led1 - PA10 */
	outr(PIOA_OER, _BV(10));
	outr(PIOA_CODR, _BV(10));
}

static void
led2_on(void)
{
	/* Led2 - PA11 */
	outr(PIOA_OER, _BV(11));
	outr(PIOA_SODR, _BV(11));
}

static void
led2_off(void)
{
	/* Led2 - PA11 */
	outr(PIOA_OER, _BV(11));
	outr(PIOA_CODR, _BV(11));
}

static void
led2_toggle(void)
{
	static int led_state = 0;

	if (!led_state) {
		led2_on();
	} else {
		led2_off();
	}

	led_state = !led_state;
}

int main(void)
{
	led1_off();
	led2_off();

	init_console();

	puts("");
	log("Agilent 34410A RS-LAN translator\n");
	log("Compilation " __DATE__ " " __TIME__ "\n");
	log("Start reason: %s\n", start_reason());
       	log("Nut/OS %s\n", NutVersionString());

	init_uart();
	init_network();

	NutWatchDogStart(600, 0);

	for (;;) {
		NutWatchDogRestart();
		NutSleep(500);
	}

	return 0;
}
