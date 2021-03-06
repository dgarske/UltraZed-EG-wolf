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
#include "sleep.h" /* for usleep() used for network startup wait */

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/ssl.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfcrypt/test/test.h"
#include "wolfcrypt/benchmark/benchmark.h"

#include "wolftpm/tpm2_wrap.h"

#include "tpm_io.h"
#include "tpm_csr.h"
#include "tpm_timestamp.h"
#include "tpm_timeset.h"
#include "tls_client.h"
#include "tls_server.h"
#include "cert_verify.h"
int echo_application(void);
int network_ready; /* global variable in networking.c */

#define THREAD_STACKSIZE (32*1024)


WOLFTPM2_DEV dev;

typedef struct func_args {
	int argc;
	char** argv;
	int return_code;
} func_args;

static const char menu1[] = "\r\n"
		"\tt. wolfCrypt Test\r\n"
		"\tb. wolfCrypt Benchmark\r\n"
		"\ts. wolfSSL TLS Server\r\n"
		"\tc. wolfSSL TLS Client\r\n"
		"\te. Xilinx TCP Echo Server\r\n"
		"\tr. TPM Generate Certificate Signing Request (CSR)\r\n"
		"\tg. TPM Get/Set Time\r\n"
		"\tp. TPM Signed Timestamp\r\n"
		"\tv. Certification Chain Validate Test\r\n"
		"\tl. TPM Clear (reset TPM)\r\n";

static char get_stdin_char(void)
{
	 return XUartPs_RecvByte(STDIN_BASEADDRESS);
}

void wolfmenu_thread(void* p)
{
	int rc;
	uint8_t cmd;
	func_args args;
	WOLFTPM2_CAPS caps;

	(void)p;

#ifdef DEBUG_WOLFSSL
	wolfSSL_Debugging_ON();
#endif
	wolfSSL_Init();

#ifdef WOLFSSL_XILINX_CRYPT
	xil_printf("Demonstrating Xilinx hardened crypto\r\n");

	/* Change from SYSOSC to PLL using the CSU_CTRL register */
	u32 value = Xil_In32(XSECURE_CSU_CTRL_REG);
	Xil_Out32(XSECURE_CSU_CTRL_REG, value | 0x1);

	/* Change the CSU PLL divisor using the CSU_PLL_CTRL register */
	Xil_Out32(0xFF5E00A0, 0x1000400);

#elif defined(WOLFSSL_ARMASM)
	xil_printf("Demonstrating ARMv8 hardware acceleration\r\n");
#else
	xil_printf("Demonstrating wolfSSL software implementation\r\n");
#endif

    rc = wolfTPM2_Init(&dev, TPM2_IoCb, NULL);
	if (rc != 0) {
		xil_printf("TPM Startup Error %d\r\n", rc);
	}

    rc = wolfTPM2_GetCapabilities(&dev, &caps);
    xil_printf("TPM Mfg %s (%d), Vendor %s, Fw %u.%u (%u), "
        "FIPS 140-2 %d, CC-EAL4 %d\r\n",
        caps.mfgStr, caps.mfg, caps.vendorStr, caps.fwVerMajor,
        caps.fwVerMinor, caps.fwVerVendor, caps.fips140_2, caps.cc_eal4);


	xil_printf("Waiting for network to start\r\n");
	while (!network_ready) {
		usleep(1000);
	}

	while (1) {
        memset(&args, 0, sizeof(args));
        rc = NOT_COMPILED_IN; /* default */

		xil_printf("\n\r\t\t\t\tMENU\r\n");
		xil_printf(menu1);
		xil_printf("Please select one of the above options:\r\n");

        do {
        	cmd = get_stdin_char();
        } while (cmd == '\n' || cmd == '\r');

		switch (cmd) {
		case 't':
			xil_printf("Running wolfCrypt Tests...\r\n");
        #ifndef NO_CRYPT_TEST
			args.return_code = 0;
			wolfcrypt_test(&args);
			rc = args.return_code;
        #endif
			break;

		case 'b':
			xil_printf("Running wolfCrypt Benchmarks...\r\n");
        #ifndef NO_CRYPT_BENCHMARK
			args.return_code = 0;
			benchmark_test(&args);
			rc = args.return_code;
        #endif
			break;

		case 's':
			rc = TPM2_TLS_Server(NULL);
			break;
		case 'c':
			rc = TPM2_TLS_Client(NULL);
			break;
		case 'e':
			rc = echo_application();
			break;
		case 'r':
			rc = TPM2_CSR_Example(NULL);
			break;
		case 'g':
		{
			UINT64 clockOut = 0;
			rc = TPM2_ClockGet_Example(NULL, &clockOut);
			if (rc == 0) {
				clockOut += (50 * 1000); /* advance 50 seconds */
				rc = TPM2_ClockSet_Example(NULL, &clockOut);
			}
			break;
		}
		case 'p':
			rc = TPM2_Timestamp_Test(NULL);
			break;
		case 'v':
			rc = VerifyCert_Test();
			break;
		case 'l':
			rc = wolfTPM2_Clear(&dev);
			break;
		default:
			xil_printf("\n\rSelection out of range\r\n");
			break;
		}

		xil_printf("Return code %d\r\n", rc);
	}

    wolfSSL_Cleanup();
	wolfTPM2_Cleanup(&dev);

	vTaskDelete(NULL);
    return;
}
