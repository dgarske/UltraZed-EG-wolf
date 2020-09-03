/*
 * Copyright (C) 2016 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include "xparameters.h"
#include "netif/xadapter.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "networking.h"

#if LWIP_IPV6==1
#include "lwip/ip.h"
#else
#if LWIP_DHCP==1
#include "lwip/dhcp.h"
#endif
#endif
#include "lwip/netdb.h"

#ifdef XPS_BOARD_ZCU102
#ifdef XPAR_XIICPS_0_DEVICE_ID
int IicPhyReset(void);
#endif
#endif

void lwip_init();

#if LWIP_IPV6==0
#if LWIP_DHCP==1
extern volatile int dhcp_timoutcntr;
err_t dhcp_start(struct netif *netif);
#endif
#endif

#define THREAD_STACKSIZE ((8*1024)/sizeof(size_t)) /* 8KB */

static struct netif server_netif;
struct netif *echo_netif;
int network_ready = 0;

#if LWIP_IPV6==1
void print_ip6(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf(" %x:%x:%x:%x:%x:%x:%x:%x\r\n",
			IP6_ADDR_BLOCK1(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK2(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK3(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK4(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK5(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK6(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK7(&ip->u_addr.ip6),
			IP6_ADDR_BLOCK8(&ip->u_addr.ip6));
}

#else
void print_ip(char *msg, ip_addr_t *ip)
{
	xil_printf(msg);
	xil_printf("%d.%d.%d.%d\r\n", ip4_addr1(ip), ip4_addr2(ip),
			ip4_addr3(ip), ip4_addr4(ip));
}

void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}
#endif

void network_thread(void *p)
{
    struct netif *netif;
    /* the mac address of the board. this should be unique per board */
    unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };
#if LWIP_IPV6==0
    ip_addr_t ipaddr, netmask, gw;
#if LWIP_DHCP==1
    int mscnt = 0;
#endif
#endif

    netif = &server_netif;

#if LWIP_IPV6==0
#if LWIP_DHCP==0
    /* initialize IP addresses to be used */
    IP4_ADDR(&ipaddr,  192, 168, 0, 20);
    IP4_ADDR(&netmask, 255, 255, 255,  0);
    IP4_ADDR(&gw,      192, 168, 0, 1);
#endif

    /* print out IP settings of the board */

#if LWIP_DHCP==0
    print_ip_settings(&ipaddr, &netmask, &gw);
    /* print all application headers */
#endif

#if LWIP_DHCP==1
	ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;
#endif
#endif

#if LWIP_IPV6==0
    /* Add network interface to the netif_list, and set it as default */
    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, PLATFORM_EMAC_BASEADDR)) {
        xil_printf("Error adding N/W interface\r\n");
        return;
    }
#else
    /* Add network interface to the netif_list, and set it as default */
    if (!xemac_add(netif, NULL, NULL, NULL, mac_ethernet_address, PLATFORM_EMAC_BASEADDR)) {
        xil_printf("Error adding N/W interface\r\n");
        return;
    }

    netif->ip6_autoconfig_enabled = 1;

    netif_create_ip6_linklocal_address(netif, 1);
    netif_ip6_addr_set_state(netif, 0, IP6_ADDR_VALID);

    print_ip6("\n\rBoard IPv6 address ", &netif->ip6_addr[0].u_addr.ip6);
#endif

    netif_set_default(netif);

    /* specify that the network if is up */
    netif_set_up(netif);

    /* start packet receive thread - required for lwIP operation */
    sys_thread_new("xemacif_input_thread", 
            (void(*)(void*))xemacif_input_thread, 
            netif,
            THREAD_STACKSIZE,
            DEFAULT_THREAD_PRIO);

#if LWIP_IPV6==0
#if LWIP_DHCP==1
    dhcp_start(netif);
    while (1) {
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		dhcp_fine_tmr();
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_COARSE_TIMER_SECS*1000) {
			dhcp_coarse_tmr();
			mscnt = 0;
		}
	}
#endif
#endif
    network_ready = 1;
    vTaskDelete(NULL);
    return;
}

void networking_start(void* p)
{
#if LWIP_DHCP==1
	int mscnt = 0;
#endif
    (void)p;

#ifdef XPS_BOARD_ZCU102
	IicPhyReset();
#endif

	/* initialize lwIP before calling sys_thread_new */
    lwip_init();

    /* any thread using lwIP should be created using sys_thread_new */
    sys_thread_new("NW_THRD", network_thread, NULL,
		THREAD_STACKSIZE,
            DEFAULT_THREAD_PRIO);

#if LWIP_IPV6==0
#if LWIP_DHCP==1
    while (1) {
	    vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		if (server_netif.ip_addr.addr) {
			xil_printf("DHCP request success\r\n");
			print_ip_settings(&(server_netif.ip_addr), &(server_netif.netmask), &(server_netif.gw));
			break;
		}
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_COARSE_TIMER_SECS * 2000) {
			xil_printf("ERROR: DHCP request timed out\r\n");
			xil_printf("Configuring default IP of 192.168.0.20\r\n");
			IP4_ADDR(&(server_netif.ip_addr),  192, 168, 0, 20);
			IP4_ADDR(&(server_netif.netmask), 255, 255, 255,  0);
			IP4_ADDR(&(server_netif.gw),  192, 168, 0, 1);
			print_ip_settings(&(server_netif.ip_addr), &(server_netif.netmask), &(server_netif.gw));
			break;
		}
	}
#endif
#endif
    network_ready = 1;
    vTaskDelete(NULL);
}


/* LWIP Socket Handling for wolf */
int SockIORecv(WOLFSSL* ssl, char* buff, int sz, void* ctx)
{
    SockIoCbCtx* sockCtx = (SockIoCbCtx*)ctx;
    int recvd;

    (void)ssl;

    /* Receive message from socket */
    if ((recvd = (int)recv(sockCtx->fd, buff, sz, 0)) == -1) {
        /* error encountered. Be responsible and report it in wolfSSL terms */

        xil_printf("IO RECEIVE ERROR: ");
        switch (errno) {
    #if EAGAIN != EWOULDBLOCK
        case EAGAIN: /* EAGAIN == EWOULDBLOCK on some systems, but not others */
    #endif
        case EWOULDBLOCK:
            if (wolfSSL_get_using_nonblock(ssl)) {
                xil_printf("would block\r\n");
                return WOLFSSL_CBIO_ERR_WANT_READ;
            }
            else {
                xil_printf("socket timeout\r\n");
                return WOLFSSL_CBIO_ERR_TIMEOUT;
            }
        case ECONNRESET:
            xil_printf("connection reset\r\n");
            return WOLFSSL_CBIO_ERR_CONN_RST;
        case EINTR:
            xil_printf("socket interrupted\r\n");
            return WOLFSSL_CBIO_ERR_ISR;
        case ECONNREFUSED:
            xil_printf("connection refused\r\n");
            return WOLFSSL_CBIO_ERR_WANT_READ;
        case ECONNABORTED:
            xil_printf("connection aborted\r\n");
            return WOLFSSL_CBIO_ERR_CONN_CLOSE;
        default:
            xil_printf("general error\r\n");
            return WOLFSSL_CBIO_ERR_GENERAL;
        }
    }
    else if (recvd == 0) {
        xil_printf("Connection closed\r\n");
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }

#ifdef TLS_BENCH_MODE
    {
        const double zeroVal = 0.0;
        if (XMEMCMP(&benchStart, &zeroVal, sizeof(double)) == 0) {
            benchStart = gettime_secs(1);
        }
    }
#endif

#ifdef DEBUG_WOLFTPM
    /* successful receive */
    xil_printf("SockIORecv: received %d bytes from %d\r\n", sz, sockCtx->fd);
#endif

    return recvd;
}

int SockIOSend(WOLFSSL* ssl, char* buff, int sz, void* ctx)
{
    SockIoCbCtx* sockCtx = (SockIoCbCtx*)ctx;
    int sent;

    (void)ssl;

    /* Receive message from socket */
    if ((sent = (int)send(sockCtx->fd, buff, sz, 0)) == -1) {
        /* error encountered. Be responsible and report it in wolfSSL terms */

        xil_printf("IO SEND ERROR: ");
        switch (errno) {
    #if EAGAIN != EWOULDBLOCK
        case EAGAIN: /* EAGAIN == EWOULDBLOCK on some systems, but not others */
    #endif
        case EWOULDBLOCK:
            xil_printf("would block\r\n");
            return WOLFSSL_CBIO_ERR_WANT_READ;
        case ECONNRESET:
            xil_printf("connection reset\r\n");
            return WOLFSSL_CBIO_ERR_CONN_RST;
        case EINTR:
            xil_printf("socket interrupted\r\n");
            return WOLFSSL_CBIO_ERR_ISR;
        case EPIPE:
            xil_printf("socket EPIPE\r\n");
            return WOLFSSL_CBIO_ERR_CONN_CLOSE;
        default:
            xil_printf("general error\r\n");
            return WOLFSSL_CBIO_ERR_GENERAL;
        }
    }
    else if (sent == 0) {
        xil_printf("Connection closed\r\n");
        return 0;
    }

#ifdef DEBUG_WOLFTPM
    /* successful send */
    xil_printf("SockIOSend: sent %d bytes to %d\r\n", sz, sockCtx->fd);
#endif

    return sent;
}

int SetupSocketAndListen(SockIoCbCtx* sockIoCtx, word32 port)
{
    struct sockaddr_in servAddr;
    int optval  = 1;

    /* Setup server address */
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    /* Create a socket that uses an Internet IPv4 address,
     * Sets the socket to be stream based (TCP),
     * 0 means choose the default protocol. */
    if ((sockIoCtx->listenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        xil_printf("ERROR: failed to create the socket\r\n");
        return -1;
    }

    /* allow reuse */
    if (setsockopt(sockIoCtx->listenFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        xil_printf("setsockopt SO_REUSEADDR failed\r\n");
        return -1;
    }

    /* Connect to the server */
    if (bind(sockIoCtx->listenFd, (struct sockaddr*)&servAddr,
                                                    sizeof(servAddr)) == -1) {
        xil_printf("ERROR: failed to bind\r\n");
        return -1;
    }

    if (listen(sockIoCtx->listenFd, 5) != 0) {
        xil_printf("ERROR: failed to listen\r\n");
        return -1;
    }

    return 0;
}

int SocketWaitClient(SockIoCbCtx* sockIoCtx)
{
    int connd;
    struct sockaddr_in clientAddr;
    socklen_t          size = sizeof(clientAddr);

    if ((connd = accept(sockIoCtx->listenFd, (struct sockaddr*)&clientAddr, &size)) == -1) {
        xil_printf("ERROR: failed to accept the connection\n\r\n");
        return -1;
    }
    sockIoCtx->fd = connd;
    return 0;
}

int SetupSocketAndConnect(SockIoCbCtx* sockIoCtx, const char* host,
    word32 port)
{
    struct sockaddr_in servAddr;
#if defined(LWIP_DNS) && LWIP_DNS == 1
    struct hostent* entry;
#endif

    /* Setup server address */
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);

#if defined(LWIP_DNS) && LWIP_DNS == 1
    /* Resolve host */
    entry = gethostbyname(host);
    if (entry) {
        XMEMCPY(&servAddr.sin_addr.s_addr, entry->h_addr_list[0],
            entry->h_length);
    }
    else
#endif
    {
        servAddr.sin_addr.s_addr = inet_addr(host);
    }

    /* Create a socket that uses an Internet IPv4 address,
     * Sets the socket to be stream based (TCP),
     * 0 means choose the default protocol. */
    if ((sockIoCtx->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        xil_printf("ERROR: failed to create the socket\r\n");
        return -1;
    }

    /* Connect to the server */
    if (connect(sockIoCtx->fd, (struct sockaddr*)&servAddr,
                                                    sizeof(servAddr)) == -1) {
        xil_printf("ERROR: failed to connect\r\n");
        return -1;
    }

    return 0;
}

void CloseAndCleanupSocket(SockIoCbCtx* sockIoCtx)
{
    if (sockIoCtx->fd != -1) {
        close(sockIoCtx->fd);
        sockIoCtx->fd = -1;
    }
    if (sockIoCtx->listenFd != -1) {
        close(sockIoCtx->listenFd);
        sockIoCtx->listenFd = -1;
    }
}
