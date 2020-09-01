/* wolf_port.c
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

#include "compile_time.h"

/* TIME CODE */
/* TODO: Implement TPM based time */
static int hw_get_time_sec(void)
{
    time_t sec = 0;
    XRtcPsu_Config* con;
    XRtcPsu         rtc;

    con = XRtcPsu_LookupConfig(XPAR_XRTCPSU_0_DEVICE_ID);
    if (con != NULL) {
        if (XRtcPsu_CfgInitialize(&rtc, con, con->BaseAddr) == XST_SUCCESS) {
            sec = (time_t)XRtcPsu_GetCurrentTime(&rtc);
        }
        else {
            xil_printf("Unable to initialize RTC\n\r");
        }
    }

    return sec;
}

/* This is used by wolfCrypt asn.c for cert time checking */
unsigned long my_time(unsigned long* timer)
{
	unsigned long sec = hw_get_time_sec();
	if (timer != NULL)
		*timer = sec;
    return sec;
}

#ifndef WOLFCRYPT_ONLY
/* This is used by TLS only */
unsigned int LowResTimer(void)
{
    return hw_get_time_sec();
}
#endif

#ifndef NO_CRYPT_BENCHMARK
/* This is used by wolfCrypt benchmark tool only */
double current_time(int reset)
{
    double time;
	int timeMs = hw_get_time_sec();
    (void)reset;
    time = (timeMs / 1000); // sec
    time += (double)(timeMs % 1000) / 1000; // ms
    return time;
}
#endif

/* Test RNG Seed Function */
/* TODO: Must provide real seed to RNG */
unsigned char my_rng_seed_gen(void)
{
	static unsigned int kTestSeed = 1;
	#warning Must implement your own RNG source
	return kTestSeed++;
}
