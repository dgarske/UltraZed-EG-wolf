/* wolf_menu.c
 *
 * Copyright (C) 2006-2020 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xrtcpsu.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xuartps_hw.h"

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/ssl.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfcrypt/test/test.h"
#include "wolfcrypt/benchmark/benchmark.h"

#include "tpm_csr.h"
#include "tpm_timestamp.h"
#include "tls_client.h"
#include "tls_server.h"
int echo_application(void);
int network_ready; /* global variable in networking.c */

#define THREAD_STACKSIZE (32*1024)


typedef struct func_args {
	int argc;
	char** argv;
	int return_code;
} func_args;

static const char menu1[] = "\n"
		"\tt. wolfCrypt Test\n"
		"\tb. wolfCrypt Benchmark\n"
		"\ts. wolfSSL TLS Server\n"
		"\tc. wolfSSL TLS Client\n"
		"\te. Xilinx TCP Echo Server\n"
		"\tr. TPM Generate Certificate Signing Request (CSR)\n"
		"\tp. TPM Signed Timestamp\n";

static char get_stdin_char(void)
{
	 return XUartPs_RecvByte(STDIN_BASEADDRESS);
}

void wolfmenu_thread(void* p)
{
	uint8_t cmd;
	func_args args;
	(void)p;

#ifdef DEBUG_WOLFSSL
	wolfSSL_Debugging_ON();
#endif
	wolfSSL_Init();

#ifdef WOLFSSL_XILINX_CRYPT
	xil_printf("Demonstrating Xilinx hardened crypto\n\r");

	/* Change from SYSOSC to PLL using the CSU_CTRL register */
	u32 value = Xil_In32(XSECURE_CSU_CTRL_REG);
	Xil_Out32(XSECURE_CSU_CTRL_REG, value | 0x1);

	/* Change the CSU PLL divisor using the CSU_PLL_CTRL register */
	Xil_Out32(0xFF5E00A0, 0x1000400);

#elif defined(WOLFSSL_ARMASM)
	xil_printf("Demonstrating ARMv8 hardware acceleration\n\r");
#else
	xil_printf("Demonstrating wolfSSL software implementation\n\r");
#endif

	while (1) {
        memset(&args, 0, sizeof(args));
        args.return_code = NOT_COMPILED_IN; /* default */

		xil_printf("\n\t\t\t\tMENU\n");
		xil_printf(menu1);
		xil_printf("Please select one of the above options:\n");

        do {
        	cmd = get_stdin_char();
        } while (cmd == '\n' || cmd == '\r');

		switch (cmd) {
		case 't':
			xil_printf("Running wolfCrypt Tests...\n");
        #ifndef NO_CRYPT_TEST
			args.return_code = 0;
			wolfcrypt_test(&args);
        #endif
			xil_printf("Crypt Test: Return code %d\n", args.return_code);
			break;

		case 'b':
			xil_printf("Running wolfCrypt Benchmarks...\n");
        #ifndef NO_CRYPT_BENCHMARK
			args.return_code = 0;
			benchmark_test(&args);
        #endif
			break;

		case 's':
			if (!network_ready) {
				xil_printf("Networking not started\n\r");
				break;
			}
			xil_printf("Starting TLS Server\n\r");
			args.return_code = TPM2_TLS_Server(NULL);
			break;
		case 'c':
			if (!network_ready) {
				xil_printf("Networking not started\n\r");
				break;
			}
			xil_printf("Starting TLS Client\n\r");
			args.return_code = TPM2_TLS_Client(NULL);
			break;
		case 'e':
			if (!network_ready) {
				xil_printf("Networking not started\n\r");
				break;
			}
			xil_printf("Xilinx Echo Server Example\n\r");
			args.return_code = echo_application();
			break;
		case 'r':
			xil_printf("TPM CSR Example\n\r");
			args.return_code = TPM2_CSR_Example(NULL);
			break;
		case 'p':
			xil_printf("TPM Signed Timestamp Example\n\r");
			args.return_code = TPM2_Timestamp_Test(NULL);
			break;
		default:
			xil_printf("\nSelection out of range\n");
			break;
		}

		xil_printf("Return code %d\n", args.return_code);
	}

    wolfSSL_Cleanup();

	vTaskDelete(NULL);
    return;
}
